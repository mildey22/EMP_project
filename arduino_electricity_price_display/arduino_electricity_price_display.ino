#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Set the LCD address to 0x27 for a 20x4 display
LiquidCrystal_I2C lcd(0x27, 20, 4);
const char* ssid = "";
const char* password = "";

void setup() {
  lcd.init();          // Initialize the LCD
  lcd.backlight();     // Turn on backlight

  Serial.begin(115200);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    lcd.setCursor(0, 0);
    lcd.print("Connecting to WiFi...");
  }
  lcd.setCursor(0, 0);
  lcd.print("Connected to WiFi");
}

void loop() {
  // Nothing here. Just vibing.
}
