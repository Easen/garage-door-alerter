#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#endif

#ifdef TG_ENABLED
#include <UniversalTelegramBot.h>
#endif

#ifdef PD_ENABLED
#include <ArxSmartPtr.h>
#include "PagerDuty.h"
#endif

Preferences preferences;
#define PREFERENCE_NS "garage-door"
#define PREFERENCE_RESTART_REASON_KEY "restart_reason"

unsigned long door_check_lasttime;
unsigned long startup_time;

#ifdef TG_ENABLED
unsigned long tg_bot_lasttime;
WiFiClientSecure tg_secured_client;
UniversalTelegramBot bot(TG_BOT_TOKEN, tg_secured_client);
#endif

#ifdef PD_ENABLED
WiFiClientSecure pd_secured_client;
PagerDuty pg(PD_ROUTING_KEY, pd_secured_client);
std::shared_ptr<PagerDutyEvent> current_pg_event;
#endif

bool wifi_connected;
int current_door_state;
int last_door_state;
bool restart_flag;
const String DOOR_OPENING_MSG = "The garage door has been OPENED.";
const String DOOR_OPEN_MSG = "The garage door is currently OPEN.";
const String DOOR_CLOSING_MSG = "The garage door has been CLOSED.";
const String DOOR_CLOSED_MSG = "The garage door is currently CLOSED.";

void wifi_connect()
{
  WiFi.setHostname(DEVICE_NAME);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int duration = 0;
  while (WiFi.status() != WL_CONNECTED && duration < WIFI_CONNECT_TIMEOUT)
  {
    DEBUG_PRINT("Connecting to WiFi...");

    delay(WIFI_CONNECT_CHECK_INTERVAL);
    duration += WIFI_CONNECT_CHECK_INTERVAL;
    digitalWrite(DOOR_CLOSED_LED, HIGH);
    delay(500);
    duration += 500;
    digitalWrite(DOOR_CLOSED_LED, LOW);
    digitalWrite(DOOR_OPENED_LED, HIGH);
    delay(500);
    duration += 500;
    digitalWrite(DOOR_OPENED_LED, LOW);
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    DEBUG_PRINT("Failed to connect, restarting!");
    preferences.getString(PREFERENCE_RESTART_REASON_KEY, "Failed to connect to WiFi");
    preferences.end();
    delay(1000);
    ESP.restart();
  }
  DEBUG_PRINT("Connected to WiFi!");
  wifi_connected = true;
}

void arduino_ota_setup()
{
  ArduinoOTA.setHostname(DEVICE_NAME);
  ArduinoOTA
      .onStart([]()
               {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      DEBUG_PRINT("Start updating " + type); })
      .onEnd([]()
             { DEBUG_PRINT("\nEnd"); })
      .onError([](ota_error_t error)
               {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) DEBUG_PRINT("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) DEBUG_PRINT("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) DEBUG_PRINT("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINT("Receive Failed");
      else if (error == OTA_END_ERROR) DEBUG_PRINT("End Failed"); });

  ArduinoOTA.begin();
}

void update_door_status_led(bool door_closed)
{
  digitalWrite(DOOR_OPENED_LED, LOW);
  digitalWrite(DOOR_CLOSED_LED, LOW);

  if (door_closed)
  {
    digitalWrite(DOOR_CLOSED_LED, HIGH);
  }
  else if (!STEALTH_MODE)
  {
    digitalWrite(DOOR_OPENED_LED, HIGH);
  }
}

void door_opened_event()
{
  update_door_status_led(false);

  DEBUG_PRINT(DOOR_OPENING_MSG);

#ifdef TG_ENABLED
  bot.sendMessage(TG_OWNER_CHAT_ID, DOOR_OPENING_MSG);
#endif

#ifdef PD_ENABLED
  current_pg_event = pg.create_event(CRITICAL, "Garage Door Opened", PD_SOURCE);
#endif
}

void door_closed_event()
{
  update_door_status_led(true);

  DEBUG_PRINT(DOOR_CLOSING_MSG);

#ifdef TG_ENABLED
  bot.sendMessage(TG_OWNER_CHAT_ID, DOOR_CLOSING_MSG);
#endif

#ifdef PD_ENABLED
  if (current_pg_event.get() != NULL)
  {
    current_pg_event.get()->resolve();
    current_pg_event.reset();
  }
#endif
}

#ifdef TG_ENABLED
void handleNewMessages(int numNewMessages)
{
  DEBUG_PRINT("handleNewMessages");
  DEBUG_PRINT(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    if (!chat_id.equals(TG_OWNER_CHAT_ID))
    {
      bot.sendMessage(chat_id, "403 FORBIDDEN");
      continue;
    }

    static bool confirm_restart = false;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
    {
      from_name = "Guest";
    }

    if (confirm_restart)
    {
      if (text.equalsIgnoreCase("yes"))
      {
        restart_flag = true;
        bot.sendMessage(chat_id, "Restarting...");
        preferences.putString(PREFERENCE_RESTART_REASON_KEY, "/restart command was issued");
        preferences.end();
        continue;
      }
      confirm_restart = false;
    }

    if (text == "/status")
    {
      if (current_door_state == LOW)
      {
        bot.sendMessage(chat_id, DOOR_CLOSED_MSG);
      }
      else
      {
        bot.sendMessage(chat_id, DOOR_OPEN_MSG);
      }
    }

    if (text == "/test")
    {
      bot.sendMessage(chat_id, "Starting test in 3 seconds...");
      delay(3000);
      door_opened_event();
      bot.sendMessage(chat_id, "Awaiting 10 seconds before triggering a close event...");
      delay(10000);
      door_closed_event();
      bot.sendMessage(chat_id, "Test complete! Re-arming the device in 3 seconds...");
      delay(3000);
      current_door_state = -1;
    }

    if (text == "/restart")
    {
      confirm_restart = true;
      bot.sendMessage(chat_id, "Please confirm you wish to restart the device (yes/no)?");
    }

    if (text == "/uptime")
    {
      long m_milliseconds = (millis() - startup_time);
      long m_seconds = (m_milliseconds / 1000);
      long m_minutes = (m_seconds / 60);
      long m_hours = (m_minutes / 60);
      long m_days = (m_hours / 24);

      long m_mod_milliseconds = m_milliseconds % 1000;
      long m_mod_seconds = m_seconds % 60;
      long m_mod_minutes = m_minutes % 60;
      long m_mod_hours = m_hours % 24;
      bot.sendMessage(chat_id, (String)(m_mod_hours) + " hours, " +
                                   (String)(m_mod_minutes) + " minutes, " +
                                   (String)(m_mod_seconds) + " seconds");
    }
  }
}
#endif

void monitor_door()
{
  if (millis() - door_check_lasttime > DOOR_CHECK_INTERVAL)
  {
    DEBUG_PRINT("Checking door");
    last_door_state = current_door_state;
    current_door_state = digitalRead(DOOR_SENSOR_PIN);

    if ((last_door_state == LOW || last_door_state == -1) && current_door_state == HIGH)
    {
      door_opened_event();
    }
    else if ((last_door_state == HIGH || last_door_state == -1) && current_door_state == LOW)
    {
      door_closed_event();
    }
    door_check_lasttime = millis();
  }
}

#ifdef TG_ENABLED
void monitor_telegram_bot()
{
  if (millis() - tg_bot_lasttime > TG_BOT_INTERVAL)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    if (numNewMessages > 0)
    {
      DEBUG_PRINT("got response");
      handleNewMessages(numNewMessages);
    }
    tg_bot_lasttime = millis();
  }
}
#endif

void setup()
{
  Serial.begin(9600);

  preferences.begin(PREFERENCE_NS, false);

  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);

  pinMode(DOOR_CLOSED_LED, OUTPUT);
  pinMode(DOOR_OPENED_LED, OUTPUT);

  wifi_connect();

  arduino_ota_setup();

  String restart_reason = preferences.getString(PREFERENCE_RESTART_REASON_KEY, "");
  DEBUG_PRINT("Reboot reason: " + restart_reason);

  current_door_state = digitalRead(DOOR_SENSOR_PIN);
#ifdef TG_ENABLED
  tg_secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  String current_door_msg = DOOR_OPEN_MSG;
  if (current_door_state == LOW)
  {
    current_door_msg = DOOR_CLOSED_MSG;
  }
  if (restart_reason.length() > 0)
  {
    preferences.putString(PREFERENCE_RESTART_REASON_KEY, "");
    bot.sendMessage(TG_OWNER_CHAT_ID, "Device is online. Reason for restart: \n" + restart_reason + "\n\n" + current_door_msg);
  }
  else
  {
    bot.sendMessage(TG_OWNER_CHAT_ID, "Device is online.\n\n" + current_door_msg);
  }
#endif

#ifdef PD_ENABLED
  pd_secured_client.setCACert(PAGER_DUTY_CERTIFICATE_ROOT);
#endif

  startup_time = millis();
}

void loop()
{
  ArduinoOTA.handle();

  if (restart_flag)
  {
    DEBUG_PRINT("restart_flag=true");
    delay(1000);
    ESP.restart();
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    // was previously connected to wifi
    DEBUG_PRINT("Wifi connection lost");
    preferences.putString(PREFERENCE_RESTART_REASON_KEY, "Wifi connection lost");
    preferences.end();
    restart_flag = true;
    return;
  }

  if (millis() - startup_time > DEVICE_TTL)
  {
    DEBUG_PRINT("Reached device TTL - Restarting...");
#ifdef TG_ENABLED
    if (wifi_connected)
    {
      bot.sendMessage(TG_OWNER_CHAT_ID, "Device is restarting as it has reached its TTL");
    }
#endif
    preferences.putString(PREFERENCE_RESTART_REASON_KEY, "Device reached TTL");
    preferences.end();
    restart_flag = true;
    return;
  }

  if (!wifi_connected)
  {
    return;
  }

  monitor_door();

#ifdef TG_ENABLED
  monitor_telegram_bot();
#endif
}