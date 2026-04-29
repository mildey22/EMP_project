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

int lastPriceZone = -1;   // FIXED (was float)

// ---------- TIME ----------
Timezone myTZ;

// ---------- JSON ----------
DynamicJsonDocument priceDoc(20000);

float currentTemp = -1000;

// ---------- FORWARD DECLARATIONS ----------
void playPriceSound(float price);

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
      }
    }

    https.end();
  }
}

//test mode, turn off for accurate results
#define TEST_MODE 1

 float getCurrentPrice() {

  #if TEST_MODE
  static int testState = 0;

  float price;

  if (testState == 0) price = 3.0;
  else if (testState == 1) price = 10.0;
  else price = 20.0;

  testState = (testState + 1) % 3;

  return price;
#endif
  
  if (!priceDoc.containsKey("prices")) return -1.0;

  JsonArray prices = priceDoc["prices"].as<JsonArray>();
  time_t nowUtc = now();

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

  return -1.0;
}

void displayCurrentPrice() {
  float price = getCurrentPrice();

  int zone;
  if (price < 5.0) zone = 0;
  else if (price < 15.0) zone = 1;
  else zone = 2;

  // ONLY trigger sound on change
  if (zone != lastPriceZone && price >= 0) {
    playPriceSound(price);
    lastPriceZone = zone;
  }

  tft.fillScreen(ST77XX_BLACK);

  // Time
  tft.setCursor(5, 5);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(myTZ.dateTime("H:i"));

  // Temperature
  tft.setCursor(85, 5);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);

  if (currentTemp > -100) {
    tft.print(currentTemp, 1);
    tft.print(" C");
  } else {
    tft.print("--C");
  }

  // Price
  tft.setCursor(10, 53);
  tft.setTextSize(3);

  if (price >= 0.0) {
    if (price < 5.0) tft.setTextColor(ST77XX_GREEN);
    else if (price < 15.0) tft.setTextColor(ST77XX_YELLOW);
    else tft.setTextColor(ST77XX_RED);

    tft.print(price, 2);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("c/kWh");
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.println("No data");
  }
}

// ---------- AUDIO ----------

void stopAudio() {
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void beep(int freq, int duration) {
  playTone(freq, duration);
  stopAudio();
}

void playPriceSound(float price) {

  if (price < 5.0) {
    beep(600, 120);
    delay(80);
    beep(800, 120);
  }

  else if (price < 15.0) {
    beep(1000, 100);
    delay(120);
    beep(1000, 100);
  }

  else {
    beep(1200, 150);
    delay(100);
    beep(900, 150);
    delay(100);
    beep(600, 250);
  }
}

// ---------- I2S ----------

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

  if (bufferIndex > 0) {
    size_t bytesWritten;
    i2s_write(I2S_NUM_0, buffer, bufferIndex * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
  }

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

  fetchPrices();
  displayCurrentPrice();
}

// ---------- LOOP ----------

void loop() {
  static unsigned long lastDisplayUpdate = 0;

//change to one minute when not testing
  if (millis() - lastDisplayUpdate > 20000) {
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
