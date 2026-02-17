#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// --- Pin Definitions ---
// I2C Interface
#define PN532_SDA_PIN   4  // SDA
#define PN532_SCL_PIN   5  // SCL
// Control Pins
#define PN532_IRQ_PIN   0  // Interrupt Request
#define PN532_RST_PIN   1  // Reset

// On-board LED
#define RGB_BUILTIN 8

// --- Objects ---
Adafruit_PN532 nfc(PN532_IRQ_PIN, PN532_RST_PIN);
Adafruit_NeoPixel pixel(1, RGB_BUILTIN, NEO_GRB + NEO_KHZ800);

// --- Functions ---
void connectToWiFi();
void sendToSupabase(String uid);

void updateStatusLED();

void setup(void) {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nESP32-C6 NFC Scanner Initializing...");

  // Initialize LED
  pixel.begin();
  pixel.setBrightness(20);
  pixel.setPixelColor(0, pixel.Color(255, 165, 0)); // Orange (Booting)
  pixel.show();

  // Initialize I2C
  Wire.begin(PN532_SDA_PIN, PN532_SCL_PIN);

  // Initialize PN532
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN53x board");
    pixel.setPixelColor(0, pixel.Color(255, 0, 0)); // Red (Error)
    pixel.show();
    while (1) delay(10);
  }
  
  Serial.print("Found chip PN5"); 
  Serial.println((versiondata>>24) & 0xFF, HEX);
  nfc.SAMConfig();

  // Connect to Wi-Fi
  connectToWiFi();

  Serial.println("Waiting for an ISO14443A Card ...");
  updateStatusLED(); // Set LED based on connection status
}

void loop(void) {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100);

  if (success) {
    // Flash Yellow (Processing)
    pixel.setPixelColor(0, pixel.Color(255, 255, 0)); 
    pixel.show();

    Serial.println("Found an ISO14443A card");
    
    // Convert UID to String
    String uidString = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) uidString += "0";
      uidString += String(uid[i], HEX);
    }
    uidString.toUpperCase();
    
    Serial.print("  UID Value: ");
    Serial.println(uidString);

    // Send to Database
    sendToSupabase(uidString);

    // Debounce
    delay(2000); 
    updateStatusLED(); // Restore LED status (Blue or Red/Off)
  }
}

void updateStatusLED() {
  if (WiFi.status() == WL_CONNECTED) {
    pixel.setPixelColor(0, pixel.Color(0, 0, 255)); // Blue (Online)
  } else {
    // Offline Mode: Solid Red to indicate "Not Connected"
    pixel.setPixelColor(0, pixel.Color(255, 0, 0)); 
  }
  pixel.show();
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  
  // Stability improvements
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  if (String(WIFI_PASSWORD) == "") {
    WiFi.begin(WIFI_SSID);
  } else {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (attempts > 120) { // 60 seconds timeout (120 * 500ms)
      Serial.println("\nWiFi Connect Failed! Starting in Offline Mode.");
      // Indicate Error: Flash Red 3 times
      for(int i=0; i<3; i++) {
        pixel.setPixelColor(0, pixel.Color(255, 0, 0));
        pixel.show();
        delay(200);
        pixel.setPixelColor(0, 0);
        pixel.show();
        delay(200);
      }
      return; // Exit function, allowing loop() to run without WiFi
    }
    delay(500);
    Serial.print(".");
    // Blink Blue/Off
    if (attempts % 2 == 0) pixel.setPixelColor(0, pixel.Color(0, 0, 255));
    else pixel.setPixelColor(0, 0); 
    pixel.show();
    attempts++;
  }
  Serial.println("\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void sendToSupabase(String uid) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Construct the endpoint URL: https://[project].supabase.co/rest/v1/scans
    String url = String(SUPABASE_URL) + "/rest/v1/scans";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
    http.addHeader("Prefer", "return=representation"); // Ask for the inserted row back

    // Create JSON Payload
    JsonDocument doc;
    doc["uid"] = uid;
    // Note: Supabase adds 'created_at' automatically if configured in table
    
    String requestBody;
    serializeJson(doc, requestBody);
    
    int httpResponseCode = http.POST(requestBody);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.println(response);

      if (httpResponseCode == 201) {
        // Success: Green Flash
        pixel.setPixelColor(0, pixel.Color(0, 255, 0)); 
        pixel.show();
        delay(500);
      } else {
        // API Error: Red Flash
        pixel.setPixelColor(0, pixel.Color(255, 0, 0)); 
        pixel.show();
        delay(500);
      }
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
      // Connection Error: Red Flash
      pixel.setPixelColor(0, pixel.Color(255, 0, 0)); 
      pixel.show();
      delay(500);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
    pixel.setPixelColor(0, pixel.Color(255, 0, 0)); 
    pixel.show();
  }
}
