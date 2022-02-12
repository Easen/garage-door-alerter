#include "PagerDuty.h"

bool readHTTPAnswer(Client *client, String &body, String &headers)
{
    int ch_count = 0;
    unsigned long now = millis();
    bool finishedHeaders = false;
    bool currentLineIsBlank = true;
    bool responseReceived = false;
    int longPoll = 0;
    unsigned int waitForResponse = 1500;
    int maxMessageLength = 15000;

    while (millis() - now < longPoll * 1000 + waitForResponse)
    {
        while (client->available())
        {
            char c = client->read();
            responseReceived = true;

            if (!finishedHeaders)
            {
                if (currentLineIsBlank && c == '\n')
                {
                    finishedHeaders = true;
                }
                else
                {
                    headers += c;
                }
            }
            else
            {
                if (ch_count < maxMessageLength)
                {
                    body += c;
                    ch_count++;
                }
            }

            if (c == '\n')
                currentLineIsBlank = true;
            else if (c != '\r')
                currentLineIsBlank = false;
        }

        if (responseReceived)
        {
#ifdef PAGER_DUTY_DEBUG
            Serial.println();
            Serial.println(body);
            Serial.println();
#endif
            break;
        }
    }
    return responseReceived;
}

String sendPostToPagerDuty(Client *client, const String &url, JsonObject payload)
{
    String body;
    String headers;

    // Connect with api.telegram.org if not already connected
    if (!client->connected())
    {
#ifdef PAGER_DUTY_DEBUG
        Serial.println(F("Connecting to PagerDuty"));
#endif
        if (!client->connect(PAGER_DUTY_HOST, PAGER_DUTY_PORT))
        {
#ifdef PAGER_DUTY_DEBUG
            Serial.println(F("Connection error"));
#endif
        }
    }
    if (client->connected())
    {
        // POST URI
        client->print(F("POST /"));
        client->print(url);
        client->println(F(" HTTP/1.1"));
        // Host header
        client->println(F("Host:" PAGER_DUTY_HOST));
        // JSON content type
        client->println(F("Content-Type: application/json"));

        // Content length
        int length = measureJson(payload);
        client->print(F("Content-Length:"));
        client->println(length);
        // End of headers
        client->println();
        // POST message body
        String out;
        serializeJson(payload, out);

        client->println(out);
#ifdef PAGER_DUTY_DEBUG
        Serial.println(String("Posting:") + out);
#endif

        readHTTPAnswer(client, body, headers);
    }

    return body;
}

void closeClient(Client *client)
{
    if (client->connected())
    {
#ifdef TELEGRAM_DEBUG
        Serial.println(F("Closing client"));
#endif
        client->stop();
    }
}

String pd_severity_to_string(pd_severity severity)
{
    switch (severity)
    {
    case CRITICAL:
        return "critical";
    case ERROR:
        return "error";
    case WARNING:
        return "warning";
    case INFO:
    default:
        return "info";
    }
}

PagerDuty::PagerDuty(const String &routing_key, Client &client)
{
#ifdef PAGER_DUTY_DEBUG
    Serial.println("PagerDuty::PagerDuty()");
#endif
    this->routing_key = routing_key;
    this->client = &client;
}

std::shared_ptr<PagerDutyEvent> PagerDuty::create_event(const pd_severity severity, const String &summary, const String &source)
{
#ifdef PAGER_DUTY_DEBUG
    Serial.println("PagerDuty::create_event()");
#endif

    DynamicJsonDocument payload(1024);
    payload["routing_key"] = this->routing_key;
    payload["event_action"] = "trigger";
    payload["payload"]["summary"] = summary;
    payload["payload"]["source"] = source;
    payload["payload"]["severity"] = pd_severity_to_string(severity);

    String response_body = sendPostToPagerDuty(this->client, "v2/enqueue", payload.as<JsonObject>());
    closeClient(this->client);
    DynamicJsonDocument response_json(1024);
    deserializeJson(response_json, response_body);

    String dedup_key = response_json["dedup_key"];
    std::shared_ptr<PagerDutyEvent> event = std::make_shared<PagerDutyEvent>(PagerDutyEvent(this->routing_key, dedup_key, severity, summary, source, this->client));
    return event;
}

PagerDutyEvent::PagerDutyEvent(const String &routing_key,
                               const String &dedup_key,
                               const pd_severity &severity,
                               const String &payload_summary,
                               const String &payload_source,
                               Client *client)
{
#ifdef PAGER_DUTY_DEBUG
    Serial.println("PagerDutyEvent::PagerDutyEvent()");
#endif
    this->routing_key = routing_key;
    this->dedup_key = dedup_key;
    this->severity = severity;
    this->payload_summary = payload_summary;
    this->payload_source = payload_source;
    this->client = client;
}

bool PagerDutyEvent::acknowledge()
{
#ifdef PAGER_DUTY_DEBUG
    Serial.println("PagerDutyEvent::acknowledge()");
#endif

    DynamicJsonDocument payload(1024);
    payload["routing_key"] = this->routing_key;
    payload["dedup_key"] = this->dedup_key;
    payload["event_action"] = "acknowledge";
    payload["payload"]["summary"] = this->payload_summary;
    payload["payload"]["source"] = this->payload_source;
    payload["payload"]["severity"] = pd_severity_to_string(severity);

    String response_body = sendPostToPagerDuty(this->client, "v2/enqueue", payload.as<JsonObject>());
    closeClient(this->client);
    DynamicJsonDocument response_json(1024);
    deserializeJson(response_json, response_body);

    String status = response_json["status"];
    if (status.equals("OK"))
    {
        return true;
    }
    return false;
}

bool PagerDutyEvent::resolve()
{
#ifdef PAGER_DUTY_DEBUG
    Serial.println("PagerDutyEvent::resolve()");
#endif
    DynamicJsonDocument payload(1024);
    payload["routing_key"] = this->routing_key;
    payload["dedup_key"] = this->dedup_key;
    payload["event_action"] = "resolve";
    payload["payload"]["summary"] = this->payload_summary;
    payload["payload"]["source"] = this->payload_source;
    payload["payload"]["severity"] = pd_severity_to_string(severity);

    String response_body = sendPostToPagerDuty(this->client, "v2/enqueue", payload.as<JsonObject>());
    closeClient(this->client);
    DynamicJsonDocument response_json(1024);
    deserializeJson(response_json, response_body);

    String status = response_json["status"];
    if (status.equals("OK"))
    {
        return true;
    }
    return false;
}
