#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArxSmartPtr.h>
#include "PagerDuty.h"

#define WIFI_SSID "..."
#define WIFI_PASSWORD "..."

#define PG_ENABLED 0
#define PD_ROUTING_KEY "..."
#define PD_SOURCE "garage_door_alerter"

#define BOT_TOKEN "..."

#define OWNER_CHAT_ID "..."

const unsigned int WIFI_CONNECT_TIMEOUT = 10 * 1000;
const unsigned int WIFI_CONNECT_CHECK_INTERVAL = 1000;

const unsigned long BOT_INTERVAL = 3000;
const unsigned long CHECK_DOOR_INTERVAL = 2000;

const int DOOR_SENSOR_PIN = 13;
const int DOOR_OPENED_LED = 25;
const int DOOR_CLOSED_LED = 26;

WiFiClientSecure tg_secured_client;
UniversalTelegramBot bot(BOT_TOKEN, tg_secured_client);

WiFiClientSecure pd_secured_client;
PagerDuty pg(PD_ROUTING_KEY, pd_secured_client);

std::shared_ptr<PagerDutyEvent> current_pg_event;

unsigned long bot_lasttime;
unsigned long door_check_lasttime;

bool wifi_connected;
int current_door_state;
int last_door_state;
bool initial_state = false;

void (*resetFunc)(void) = 0;

static void wifi_connect()
{
  WiFi.setHostname("GarageDoorAlerter");
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int duration = 0;
  while (WiFi.status() != WL_CONNECTED && duration < WIFI_CONNECT_TIMEOUT)
  {
    delay(WIFI_CONNECT_CHECK_INTERVAL);
    duration += WIFI_CONNECT_CHECK_INTERVAL;
    Serial.println("Connecting to WiFi...");
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
    Serial.println("Failed to connect, restarting!");
    resetFunc();
  }

  Serial.println("Connected to WiFi!");
  wifi_connected = true;
  initial_state = true;
}

void handleNewMessages(int numNewMessages)
{
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    if (!chat_id.equals(OWNER_CHAT_ID))
    {
      bot.sendMessage(chat_id, "403 FORBIDDEN");
      continue;
    }

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/reboot")
    {
      Serial.println("Rebooting!");
      bot.sendMessage(chat_id, "Rebooting!");
      // delay(5000);
      // resetFunc();
    }

    if (text == "/start")
    {
      String welcome = "Welcome to Universal Arduino Telegram Bot library, " + from_name + ".\n";
      welcome += "This is Chat Action Bot example.\n\n";
      welcome += "/send_test_action : to send test chat action message\n";
      bot.sendMessage(chat_id, welcome);
    }
  }
}

void setup()
{
  Serial.begin(9600);

  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);

  pinMode(DOOR_CLOSED_LED, OUTPUT);
  pinMode(DOOR_OPENED_LED, OUTPUT);

  wifi_connect();

  tg_secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  pd_secured_client.setCACert(PAGER_DUTY_CERTIFICATE_ROOT);

  current_door_state = digitalRead(DOOR_SENSOR_PIN);
  if (current_door_state == LOW)
  {
    digitalWrite(DOOR_CLOSED_LED, HIGH);
  }
  else
  {
    digitalWrite(DOOR_OPENED_LED, HIGH);
  }
}

void monitor_door()
{
  if (millis() - door_check_lasttime > CHECK_DOOR_INTERVAL)
  {
    Serial.println("Checking door");
    last_door_state = current_door_state;
    current_door_state = digitalRead(DOOR_SENSOR_PIN);

    if (last_door_state == LOW && current_door_state == HIGH)
    {
      digitalWrite(DOOR_CLOSED_LED, LOW);
      digitalWrite(DOOR_OPENED_LED, HIGH);

      Serial.println("The door-opening event is detected");
      bot.sendMessage(OWNER_CHAT_ID, "The door-opening event is detected");
#ifdef PG_ENABLED
      current_pg_event = pg.create_event(CRITICAL, "Garage Door Opened", PD_SOURCE);
#endif
    }
    else if (last_door_state == HIGH && current_door_state == LOW)
    {
      digitalWrite(DOOR_CLOSED_LED, HIGH);
      digitalWrite(DOOR_OPENED_LED, LOW);

      Serial.println("The door-closing event is detected");
      bot.sendMessage(OWNER_CHAT_ID, "The door-closing event is detected");
      if (current_pg_event.get() != NULL)
      {
        current_pg_event.get()->resolve();
        current_pg_event.reset();
      }
    }
    door_check_lasttime = millis();
  }
}

void monitor_telegram_bot()
{
  if (millis() - bot_lasttime > BOT_INTERVAL)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    if (numNewMessages > 0)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
    }
    bot_lasttime = millis();
  }
}
void loop()
{

  if (!wifi_connected)
  {
    return;
  }

  if (wifi_connected && WiFi.status() != WL_CONNECTED)
  {
    // was previously connected to wifi
    Serial.println("Wifi connection lost");
    wifi_connect();
  }

  monitor_door();

  monitor_telegram_bot();
}