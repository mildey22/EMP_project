#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ezTime.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "driver/i2s.h"
#include "secrets.h"
#include <math.h>

// ---------- DISPLAY ----------
#define TFT_CS 5
#define TFT_DC 16
#define TFT_RST 4
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ---------- AUDIO ----------
#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 22

// ---------- TIME ----------
Timezone myTZ;

// ---------- JSON ----------
DynamicJsonDocument priceDoc(20000);

float currentTemp = -1000; // default “invalid”

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
  client.setInsecure();

  HTTPClient https;

  Serial.println("Fetching prices...");

  if (https.begin(client, PRICE_API_URL)) {

    int httpCode = https.GET();

    Serial.print("HTTP response: ");
    Serial.println(httpCode);

    if (httpCode > 0) {

      String payload = https.getString();

      DeserializationError error = deserializeJson(priceDoc, payload);

      if (error) {
        Serial.println("JSON parse failed");
        return;
      }

      Serial.println("Prices updated");

    } else {
      Serial.println("HTTP request failed");
    }

    https.end();
  }
}

void fetchTemperature() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;

  if (https.begin(client, TEMP_API_URL)) {
    int httpCode = https.GET();
    if (httpCode > 0) {
      String payload = https.getString();
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        currentTemp = doc["current_weather"]["temperature"].as<float>();
        Serial.print("Current temp: "); Serial.println(currentTemp);
      } else {
        Serial.println("Temp JSON parse failed");
      }
    } else {
      Serial.println("Temp HTTP request failed");
    }
    https.end();
  }
}

float getCurrentPrice() {
  if (!priceDoc.containsKey("prices")) return -1.0;

  JsonArray prices = priceDoc["prices"].as<JsonArray>();

  // get current UTC time as epoch
  time_t nowUtc = now(); // ezTime gives UTC epoch

  for (JsonObject priceEntry : prices) {
    const char* startStr = priceEntry["startDate"];
    const char* endStr   = priceEntry["endDate"];

    struct tm tmStart, tmEnd;
    strptime(startStr, "%Y-%m-%dT%H:%M:%S", &tmStart);
    strptime(endStr,   "%Y-%m-%dT%H:%M:%S", &tmEnd);

    time_t startEpoch = mktime(&tmStart);
    time_t endEpoch   = mktime(&tmEnd);

    if (nowUtc >= startEpoch && nowUtc < endEpoch) {
      return priceEntry["price"].as<float>();
    }
  }

  return -1.0; // no matching price
}

void displayCurrentPrice() {
  float price = getCurrentPrice();

  tft.fillScreen(ST77XX_BLACK);

  // --- Time display ---
  tft.setCursor(5, 5);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(myTZ.dateTime("H:i"));

    // --- Temperature display ---
  tft.setCursor(85, 5);       // adjust X/Y to top-right
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
    if (currentTemp > -100) {
      tft.print(currentTemp, 1);
      tft.print(" C");
    } else {
      tft.print("--C");
  }

  // --- Price display ---
  tft.setCursor(10, 53);
  tft.setTextSize(3);

  if (price >= 0.0) {
    // --- Color switch based on price ---
    if (price < 5.0) {
      tft.setTextColor(ST77XX_GREEN);
    } else if (price < 15.0) {
      tft.setTextColor(ST77XX_YELLOW);
    } else {
      tft.setTextColor(ST77XX_RED);
    }

    tft.print(price, 2);  // big number
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("c/kWh");  // smaller unit

  } else {
    tft.setTextColor(ST77XX_RED);
    tft.println("No data");
  }

  // --- Serial log ---
  Serial.print("Device time: "); Serial.println(myTZ.dateTime());
  Serial.print("Current price: "); Serial.println(price);
}

void stopAudio() {
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void setupI2S() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void playTone(int freq, int durationMs) {
  const int sampleRate = 44100;
  const int totalSamples = sampleRate * durationMs / 1000;

  int16_t buffer[512];

  int bufferIndex = 0;

  for (int i = 0; i < totalSamples; i++) {
    buffer[bufferIndex++] = 8000 * sin(2 * PI * freq * i / sampleRate);

    if (bufferIndex == 512) {
      size_t bytesWritten;
      i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &bytesWritten, portMAX_DELAY);
      bufferIndex = 0;
    }
  }

  // 🔥 flush remaining samples (THIS is what you're missing)
  if (bufferIndex > 0) {
    size_t bytesWritten;
    i2s_write(I2S_NUM_0, buffer, bufferIndex * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
  }

  // optional: tiny silence gap so it doesn’t smear into next sound
  int16_t silence[256] = {0};
  size_t bw;
  i2s_write(I2S_NUM_0, silence, sizeof(silence), &bw, portMAX_DELAY);
}

// ---------- SETUP ----------

void setup() {
  Serial.begin(115200);

  delay(300);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  connectWiFi();

  setupI2S();

  stopAudio();
  delay(100);

  playTone(1000, 200);
  stopAudio();
  delay(50);

  playTone(1500, 200);
  stopAudio();

  waitForSync();
  myTZ.setLocation("Europe/Helsinki");

  fetchPrices();           // get the latest prices from API
  displayCurrentPrice();   // show them on the OLED right away

}

// ---------- LOOP ----------

void loop() {

  static unsigned long lastDisplayUpdate = 0;
  static unsigned long lastFetch = 0;

  if (millis() - lastDisplayUpdate > 60000) {

    displayCurrentPrice();
    lastDisplayUpdate = millis();

  }

  if (myTZ.hour() == 14 && myTZ.minute() == 0) {
    fetchPrices();
  }

  static int lastTempHour = -1;
  int hour = myTZ.hour();

    if (hour != lastTempHour) {
      fetchTemperature();
      lastTempHour = hour;
  }
}
