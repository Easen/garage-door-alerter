#ifndef CONFIG_H
#define CONFIG_H

#include <vector>

#define SECOND 1000L

// WiFi
#define WIFI_SSID "..."
#define WIFI_PASSWORD "..."

const unsigned int WIFI_CONNECT_TIMEOUT = 30 * SECOND;
const unsigned int WIFI_CONNECT_CHECK_INTERVAL = SECOND;

// PagerDuty
#define PD_ENABLED
#define PD_ROUTING_KEY "..."
#define PD_SOURCE "Garage Door"

// Telegram
#define TG_ENABLED
#define TG_BOT_TOKEN "..."
#define TG_OWNER_CHAT_ID "..."
const unsigned long TG_BOT_INTERVAL = 5 * SECOND;

// BLE Key Fobs
#define BLE_SCAN_DURATION 5
#define BLE_MAX_RSSI -80
std::vector<String> keyFobs = {"..."};

// Device
#define DEVICE_NAME "garage-door-alerter"

#define DOOR_SENSOR_PIN 13
#define DOOR_OPENED_LED 25
#define DOOR_CLOSED_LED 26
const unsigned long DOOR_CHECK_INTERVAL = 2 * SECOND;
const unsigned long long DEVICE_TTL = 30LL * 24LL * 60LL * 60LL * SECOND;
const bool STEALTH_MODE = true;

// Enable debug?
// #define DEBUG

#endif
