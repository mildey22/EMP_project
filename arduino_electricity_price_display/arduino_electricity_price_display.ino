#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ezTime.h>
#include <LiquidCrystal_I2C.h>

// ---------- WIFI ----------
const char* ssid = "";
const char* password = "";

// ---------- API ----------
const char* apiUrl = "https://api.porssisahko.net/v2/latest-prices.json";

// ---------- LCD ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- TIME ----------
Timezone myTZ;

// ---------- DATA ----------
float prices[24];

// ---------- FUNCTIONS ----------

void connectWiFi() {
  Serial.print("Connecting to WiFi");

  WiFi.begin(ssid, password);

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

  if (https.begin(client, apiUrl)) {

    int httpCode = https.GET();

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

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Hour: ");
  lcd.print(currentHour);

  lcd.setCursor(0, 1);
  lcd.print(currentPrice);
  lcd.print(" c/kWh");

  Serial.print("Hour ");
  Serial.print(currentHour);
  Serial.print(": ");
  Serial.println(currentPrice);
}

// ---------- SETUP ----------

void setup() {

  Serial.begin(115200);
  delay(1000);

  lcd.init();
  lcd.backlight();

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
    displayCurrentPrice();
    lastUpdate = millis();
  }
}
