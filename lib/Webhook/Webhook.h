#ifndef WEBHOOK_H
#define WEBHOOK_H

#include <Arduino.h>
#include <Client.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "../HttpUtils/HttpUtils.h"

typedef enum
{
    SUCCESS = 1,
    UNABLE_CONNECT = -1,
} trigger_webhook_status;

class Webhook
{
public:
    Webhook(const bool ssl, const char *hostname, const short unsigned int port, const char *path);
    trigger_webhook_status trigger_webhook(String &body);

private:
    const char *hostname;
    short unsigned int port = 80;
    const char *path;
    WiFiClient *client;
};
#endif