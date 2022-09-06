#include "Webhook.h"

Webhook::Webhook(const bool ssl, const char *hostname, const uint16_t port, const char *path)
{
    WiFiClient *client = (ssl) ? new WiFiClientSecure() : new WiFiClient();
    this->client = client;
    this->hostname = hostname;
    this->port = port;
    this->path = path;
}

trigger_webhook_status Webhook::trigger_webhook(String &body)
{
    if (!this->client->connected())
    {
        if (!client->connect(this->hostname, this->port))
        {
            return UNABLE_CONNECT;
        }
    }

    this->client->print(F("GET "));
    this->client->print(this->path);
    this->client->println(F(" HTTP/1.1"));

    this->client->print(F("Host: "));
    this->client->println(F(this->hostname));

    this->client->println(F("User-Agent: garage-door-alerter"));
    this->client->println(F("Accept: */*"));
    this->client->println();

    String headers;
    readHTTPAnswer(this->client, body, headers);

    return SUCCESS;
}