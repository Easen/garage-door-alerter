#define WIFI_SSID "..."
#define WIFI_PASSWORD "..."

#define PD_ENABLED 1
#define PD_ROUTING_KEY "..."
#define PD_SOURCE "..."

#define TG_ENABLED 1
#define TG_BOT_TOKEN "..."
#define TG_OWNER_CHAT_ID "..."

// #define DEBUG 1
#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#endif

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#ifdef TG_ENABLED
#include <UniversalTelegramBot.h>
#endif

#ifdef PD_ENABLED
#include <ArxSmartPtr.h>
#include "PagerDuty.h"
#endif

#define SECOND 1000
const unsigned int WIFI_CONNECT_TIMEOUT = 30 * SECOND;
const unsigned int WIFI_CONNECT_CHECK_INTERVAL = SECOND;

const unsigned long TG_BOT_INTERVAL = 5 * SECOND;
unsigned long tg_bot_lasttime;

const unsigned long DOOR_CHECK_INTERVAL = 2 * SECOND;
unsigned long door_check_lasttime;

const unsigned long RESET_INTERVAL = 24 * 60 * 60 * SECOND;
unsigned long startup_time;

const int DOOR_SENSOR_PIN = 13;
const int DOOR_OPENED_LED = 25;
const int DOOR_CLOSED_LED = 26;

#ifdef TG_ENABLED
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

void (*resetFunc)(void) = 0;

void wifi_connect()
{
  WiFi.setHostname("GarageDoorAlerter");
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
    resetFunc();
  }
  ` DEBUG_PRINT("Connected to WiFi!");
  wifi_connected = true;
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

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/status")
    {
      if (current_door_state == LOW)
      {
        bot.sendMessage(chat_id, "Garage door is currently closed");
      }
      else
      {
        bot.sendMessage(chat_id, "Garage door is currently open");
      }
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

void setup()
{
  Serial.begin(9600);

  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);

  pinMode(DOOR_CLOSED_LED, OUTPUT);
  pinMode(DOOR_OPENED_LED, OUTPUT);

  wifi_connect();

#ifdef TG_ENABLED
  tg_secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
#endif

#ifdef PD_ENABLED
  pd_secured_client.setCACert(PAGER_DUTY_CERTIFICATE_ROOT);
#endif
  current_door_state = digitalRead(DOOR_SENSOR_PIN);
  if (current_door_state == LOW)
  {
    digitalWrite(DOOR_CLOSED_LED, HIGH);
  }
  else
  {
    digitalWrite(DOOR_OPENED_LED, HIGH);
  }

  startup_time = millis();
}

void monitor_door()
{
  if (millis() - door_check_lasttime > DOOR_CHECK_INTERVAL)
  {
    DEBUG_PRINT("Checking door");
    last_door_state = current_door_state;
    current_door_state = digitalRead(DOOR_SENSOR_PIN);

    if (last_door_state == LOW && current_door_state == HIGH)
    {
      digitalWrite(DOOR_CLOSED_LED, LOW);
      digitalWrite(DOOR_OPENED_LED, HIGH);

      DEBUG_PRINT("The door-opening event is detected");

#ifdef TG_ENABLED
      bot.sendMessage(TG_OWNER_CHAT_ID, "The door-opening event is detected");
#endif

#ifdef PD_ENABLED
      current_pg_event = pg.create_event(CRITICAL, "Garage Door Opened", PD_SOURCE);
#endif
    }
    else if (last_door_state == HIGH && current_door_state == LOW)
    {
      digitalWrite(DOOR_CLOSED_LED, HIGH);
      digitalWrite(DOOR_OPENED_LED, LOW);

      DEBUG_PRINT("The door-closing event is detected");

#ifdef TG_ENABLED
      bot.sendMessage(TG_OWNER_CHAT_ID, "The door-closing event is detected");
#endif

#ifdef PD_ENABLED
      if (current_pg_event.get() != NULL)
      {
        current_pg_event.get()->resolve();
        current_pg_event.reset();
      }
#endif
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

void loop()
{
  if (millis() - startup_time > RESET_INTERVAL)
  {
    DEBUG_PRINT("Reached reset interval - Restarting...");
    resetFunc();
    return;
  }

  if (!wifi_connected)
  {
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    // was previously connected to wifi
    DEBUG_PRINT("Wifi connection lost");
    wifi_connect();
  }

  monitor_door();

#ifdef TG_ENABLED
  monitor_telegram_bot();
#endif
}