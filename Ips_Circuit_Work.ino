/*-------Author by--------- 
------Md. Rasel Ahmed------*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

// WiFi credentials
const char* ssid = "RASEL HOME";
const char* password = "Home@2004";

// Server settings
ESP8266WebServer server(80);
const char* serverURL = "http://103.115.255.11:8080/charging_data.php";

// Pin definitions
#define ANALOG_IN_PIN A0
#define RELAY_PIN D5  // GPIO2 on Wemos D1 Mini
#define BUZZER D7

// Voltage thresholds
const float REF_VOLTAGE = 3.3;  // ESP8266 ADC reference voltage
const float FULL_CUTOFF_VOLTAGE = 14.6;
const float LOW_CUTOFF_VOLTAGE = 13;

// Resistor values for voltage divider (in ohms)
const float R1 = 46000.0;  // 47KOhm
const float R2 = 10000.0;  // 10KOhm

// LCD configuration
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Timing variables
unsigned long previousMillis = 0;
unsigned long previousMillis2 = 0;
unsigned long ota_progress_millis = 0;
const long interval = 5000;  // 5 seconds
//data store every 2min
unsigned long previousMillis_dataStore = 0;
const unsigned long duration = 120000;  // 2 minutes in milliseconds


// Voltage variables
float adc_voltage = 0.0;
float in_voltage = 0.0;
float averageVoltage = 0.0;
int adc_value = 0;

// Debug mode
#define DEBUG_MODE true
#if DEBUG_MODE
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

void setup(void) {
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Initialize Serial communication
  Serial.begin(115200);
  DEBUG_PRINTLN("Initializing...");

  // Pin configuration
  pinMode(BUZZER, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Initially turn off relay

  // Connect to WiFi
  WiFi.begin(ssid, password);
  DEBUG_PRINT("Connecting to WiFi: ");
  DEBUG_PRINTLN(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    DEBUG_PRINT(".");
  }
  DEBUG_PRINTLN("\nConnected!");
  DEBUG_PRINT("IP Address: ");
  DEBUG_PRINTLN(WiFi.localIP());

  // Setup OTA
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/plain", "Hello, this is the root page!");
  });
  ElegantOTA.begin(&server);
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
  server.begin();

  DEBUG_PRINTLN("HTTP server started");

  lcd.setCursor(0, 0);
  lcd.print("System Initialized");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");
}

void loop() {
  server.handleClient();
  ElegantOTA.loop();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis2 <= 5000) {
    displayWelcomeMessage();
    // Check if 2 minutes have passed
    if (millis() - previousMillis_dataStore >= duration) {
      previousMillis_dataStore = millis();  // Update timer

      // Send data to the server
      data_store();
      
    }
  } else if (currentMillis - previousMillis2 <= 15000) {
    Voltage();
  } else if (currentMillis - previousMillis2 <= 30000) {
    displayVoltageStatus();
  } else if (currentMillis - previousMillis2 >= 32000) {
    if (digitalRead(RELAY_PIN) == HIGH) {
      Beep();
    }
    previousMillis2 = currentMillis;
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    DEBUG_PRINTLN("WiFi reconnecting...");
  }
}

void displayWelcomeMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("360 TECH KNOWLEDGE");
  lcd.setCursor(0, 1);
  lcd.print("DSP Solar IPS");
}

void Voltage() {
  const int numReadings = 50;
  float totalVoltage = 0.0;

  for (int i = 0; i < numReadings; i++) {
    adc_value = analogRead(ANALOG_IN_PIN);
    adc_voltage = (adc_value * REF_VOLTAGE) / 1023.0;
    in_voltage = adc_voltage * (R1 + R2) / R2;
    totalVoltage += in_voltage;
    delay(50);
  }

  averageVoltage = (totalVoltage / numReadings) - 0.25;  // Calibration adjustment
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Input Voltage");
  lcd.setCursor(0, 1);
  lcd.print("DC ");
  lcd.print(averageVoltage, 2);
  lcd.print("V");

  if (averageVoltage < LOW_CUTOFF_VOLTAGE) {
    DEBUG_PRINTLN("Low voltage detected. Charging ON.");
    digitalWrite(RELAY_PIN, HIGH);
  } else if (averageVoltage > FULL_CUTOFF_VOLTAGE) {
    DEBUG_PRINTLN("Full voltage detected. Charging OFF.");
    digitalWrite(RELAY_PIN, LOW);
  }
}

void displayVoltageStatus() {
  lcd.clear();
  if (averageVoltage >= 14.2) {
    lcd.setCursor(0, 0);
    lcd.print("Battery Full");
  } else if (averageVoltage >= 13.7) {
    lcd.setCursor(0, 0);
    lcd.print("Standard Charged");
  } else if (averageVoltage <= 11.0) {
    lcd.setCursor(0, 0);
    lcd.print("Battery LOW!");
    lcd.setCursor(0, 1);
    lcd.print("Connect Charger");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Charging...");
  }
  lcd.setCursor(0, 1);
  lcd.print("DC ");
  lcd.print(averageVoltage, 2);
  lcd.print("V");
}

void Beep() {
  digitalWrite(BUZZER, HIGH);
  delay(50);
  digitalWrite(BUZZER, LOW);
  delay(50);
}

void onOTAStart() {
  DEBUG_PRINTLN("OTA update started!");
}

void onOTAProgress(size_t current, size_t final) {
  int progressPercentage = (current * 100) / final;
  DEBUG_PRINT("OTA Progress: ");
  DEBUG_PRINT(progressPercentage);
  DEBUG_PRINTLN("%");
}

void onOTAEnd(bool success) {
  if (success) {
    DEBUG_PRINTLN("OTA update finished successfully!");
  } else {
    DEBUG_PRINTLN("OTA update failed!");
  }
}

void data_store() {
  if (WiFi.status() == WL_CONNECTED) {
    DynamicJsonDocument jsonDoc(512);
    jsonDoc["device_id"] = WiFi.macAddress();
    jsonDoc["battery_voltage"] = averageVoltage;
    jsonDoc["charging_status"] = digitalRead(RELAY_PIN);
    jsonDoc["ssid"] = WiFi.SSID();
    jsonDoc["ip"] = WiFi.localIP().toString();  // Convert IPAddress to string

    String jsonPayload;
    serializeJson(jsonDoc, jsonPayload);

    sendDataToServer(jsonPayload);
  } else {
    DEBUG_PRINTLN("WiFi not connected!");
  }
}

void sendDataToServer(String payload) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;              // Create a WiFiClient instance
    HTTPClient http;                // Create an HTTPClient instance
    http.begin(client, serverURL);  // Use the new API with WiFiClient

    http.addHeader("Content-Type", "application/json");  // Set content type
    int httpResponseCode = http.POST(payload);           // Send POST request

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Response: " + response);
    } else {
      Serial.println("Error in sending POST request: " + String(httpResponseCode));
    }

    http.end();  // End the HTTP request
  } else {
    Serial.println("WiFi not connected!");
  }
}
