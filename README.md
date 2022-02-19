# garage-door-alerter
EPS32 Garage Door Alerter

## Features

* Automatic WiFi reconnect
  * Will check for WiFi and if it can't get establish an connection it will restart (soft reset) and try again.
* Automatic restart (soft reset), every 24 hours
* Telegram integration (using witnessmenow' [UniversalTelegramBot](https://registry.platformio.org/libraries/witnessmenow/UniversalTelegramBot))
  * Requires a BOT_ID (speak to the _Botfather_) & your CHAT_ID (see _get id bot_)
  * `/uptime` - reports the devices uptime
  * `/status` - reports on door status
* PagerDuty integration (I recommend creating a free account)
  * Will trigger an critical incident when the door has been opened
  * Will automatically resolve the incident when door has been closed.
  
## Requirements

* Door senor (reed switch magnet)
* 2 LEDs 

## Configuration

See `src/config.h`.
