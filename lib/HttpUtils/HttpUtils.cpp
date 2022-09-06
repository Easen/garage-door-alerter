#include "HttpUtils.h"

// #define DEBUG 1

bool readHTTPAnswer(Client *client, String &body, String &headers)
{
    int ch_count = 0;
    unsigned long now = millis();
    bool finishedHeaders = false;
    bool currentLineIsBlank = true;
    bool responseReceived = false;
    int longPoll = 0;
    unsigned int waitForResponse = 10 * 1000;
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
#ifdef DEBUG
            Serial.println("HEADERS:");
            Serial.println(headers);
            Serial.println();
            Serial.println("BODY:");
            Serial.println(body);
            Serial.println();
#endif
            break;
        }
    }
    return responseReceived;
}