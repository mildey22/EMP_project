#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ezTime.h>
#include <SPI.h> 
#include <Adafruit_GFX.h> 
#include <Adafruit_ST7735.h>
#include "secrets.h"

// ---------- OLED ----------
#define TFT_CS     5
#define TFT_DC     16
#define TFT_RST    4
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ---------- TIME ----------
Timezone myTZ;

// ---------- DATA ----------
float prices[24];

// ---------- FUNCTIONS ----------

void connectWiFi() {
  Serial.print("Connecting to WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
}

void fetchPrices() {
  WiFiClientSecure client;
  client.setInsecure();  // skip certificate validation (fine for school project)

  HTTPClient https;

  Serial.println("Fetching prices...");

  if (https.begin(client, API_KEY)) {
    int httpCode = https.GET();

    Serial.print("HTTP response code: ");
    Serial.println(httpCode);  // <-- helpful for debugging

    if (httpCode > 0) {
      String payload = https.getString();
      Serial.println("Received data");
      parsePrices(payload);
    } else {
      Serial.println("HTTP request failed");
    }

    https.end();
  } else {
    Serial.println("Connection failed");
  }
}

void parsePrices(String payload) {
  DynamicJsonDocument doc(20000);
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.println("JSON parse failed");
    return;
  }

  JsonArray arr = doc["prices"];  // adjust if your JSON structure differs

  for (int i = 0; i < 24; i++) {
    prices[i] = arr[i]["price"];
  }

  Serial.println("Prices stored.");
}

void displayCurrentPrice() {
  int currentHour = myTZ.hour();
  float currentPrice = prices[currentHour];

  // ---------- OLED DISPLAY ----------
  tft.fillScreen(ST77XX_BLACK);  // clear screen

  tft.setCursor(0, 10);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.print("Hour: ");
  tft.print(currentHour);

  tft.setCursor(0, 40);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(3);
  tft.print(currentPrice, 2);  // two decimals
  tft.print(" c/kWh");

  // ---------- SERIAL ----------
  Serial.print("Hour ");
  Serial.print(currentHour);
  Serial.print(": ");
  Serial.println(currentPrice);
}

// ---------- SETUP ----------

void setup() {
  Serial.begin(115200);
  delay(1000);

  tft.initR(INITR_BLACKTAB); // initialize the display
  tft.fillScreen(ST77XX_BLACK);

  connectWiFi();

  waitForSync();
  myTZ.setLocation("Europe/Helsinki");

  fetchPrices();
  displayCurrentPrice();
}

// ---------- LOOP ----------

void loop() {
  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate > 60000) {   // update every 60 sec
    fetchPrices();        // <-- fetch fresh prices each minute
    displayCurrentPrice();
    lastUpdate = millis();
  }
}
