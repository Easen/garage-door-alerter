#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include <Arduino.h>

bool readHTTPAnswer(Client *client, String &body, String &headers);

#endif