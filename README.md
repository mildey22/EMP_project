# The EMP - Electricity Price Alert Device

- [Description](#description)
- [Features](#features)
- [Requirements](#requirements)
- [Configuration](#configuration)

## Description

The EMP, or the Energy Monitoring Panel is an experimental IoT student project that displays real-time electricity prices and alerts the user when prices rise above configurable thresholds. 
The device uses visual indicators and sound notifications, and may optionally integrate with a Telegram bot for remote alerts.

The hardware will be housed in a 3D-printed lightning bolt-shaped casing with an integrated display.

This project is built as a learning exercise in:

- IoT systems design
- API integration
- Embedded programming
- UI design
- Responsive user notification systems
- 3D printing for hardware enclosure design

## Requirements

Hardware Requirements

- ESP32
- Display module
- Amplifier
- Small speaker
- 3D printed enclosure

This project depends on the following 3rd party libraries:

- ArduinoJson
- ezTime
- Adafruit GFX
- Adafruit ST7735

## Configuration

Create a `secrets.h` file:

```const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";
const char* PRICE_API_URL = "https://api.porssisahko.net/v2/latest-prices.json";
const char* TEMP_API_URL = "https://api.open-meteo.com/v1/forecast?latitude=60.17&longitude=24.94&current_weather=true";
```

Add `secrets.h` to .gitignore
