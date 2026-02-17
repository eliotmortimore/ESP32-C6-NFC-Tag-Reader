#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include <LittleFS.h>

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
bool sendToSupabase(String uid); // Returns true on success
void saveOffline(String uid);
void processOfflineQueue();
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

  // Initialize LittleFS
  if(!LittleFS.begin(true)){
    Serial.println("LittleFS Mount Failed");
    return;
  }
  
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

  // 1. Process Offline Queue (if connected)
  // Check every 30 seconds or so, non-blocking check would be better,
  // but for now we can just check if wifi is up.
  static unsigned long lastSyncTime = 0;
  if (millis() - lastSyncTime > 30000 && WiFi.status() == WL_CONNECTED) {
    processOfflineQueue();
    lastSyncTime = millis();
  }

  // 2. Scan for Card
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

    // Try sending to Database
    bool sent = sendToSupabase(uidString);

    if (!sent) {
      // If failed, save offline
      saveOffline(uidString);
      // Flash Yellow (Saved Offline) instead of Green
      pixel.setPixelColor(0, pixel.Color(255, 255, 0)); 
      pixel.show();
      delay(500);
    } 

    // Debounce
    delay(2000); 
    updateStatusLED(); // Restore LED status
  }
}

void updateStatusLED() {
  if (WiFi.status() == WL_CONNECTED) {
    pixel.setPixelColor(0, pixel.Color(0, 0, 255)); // Blue (Online)
  } else {
    // Offline Mode: Solid Red
    pixel.setPixelColor(0, pixel.Color(255, 0, 0)); 
  }
  pixel.show();
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  
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
    if (attempts > 120) { // 60 seconds
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
      return; 
    }
    delay(500);
    Serial.print(".");
    if (attempts % 2 == 0) pixel.setPixelColor(0, pixel.Color(0, 0, 255));
    else pixel.setPixelColor(0, 0); 
    pixel.show();
    attempts++;
  }
  Serial.println("\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

bool sendToSupabase(String uid) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(SUPABASE_URL) + "/rest/v1/scans";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
    http.addHeader("Prefer", "return=representation");

    JsonDocument doc;
    doc["uid"] = uid;
    
    String requestBody;
    serializeJson(doc, requestBody);
    
    int httpResponseCode = http.POST(requestBody);
    http.end();

    if (httpResponseCode == 201) {
      Serial.println("Supabase Upload Success");
      // Green Flash
      pixel.setPixelColor(0, pixel.Color(0, 255, 0)); 
      pixel.show();
      delay(500);
      return true;
    } else {
      Serial.print("Supabase Upload Failed: ");
      Serial.println(httpResponseCode);
      return false;
    }
  } else {
    Serial.println("No WiFi connection for upload.");
    return false;
  }
}

void saveOffline(String uid) {
  Serial.println("Saving offline...");
  File file = LittleFS.open("/queue.txt", "a");
  if(file){
    file.println(uid);
    file.close();
    Serial.println("Saved to /queue.txt");
  } else {
    Serial.println("Failed to open /queue.txt for appending");
  }
}

void processOfflineQueue() {
  if(!LittleFS.exists("/queue.txt")) return;

  Serial.println("Processing Offline Queue...");
  
  // 1. Rename queue to sending
  LittleFS.rename("/queue.txt", "/sending.txt");
  
  File file = LittleFS.open("/sending.txt", "r");
  if(!file){
    Serial.println("Failed to open queue file");
    return;
  }

  // 2. Read line by line
  while(file.available()){
    String uid = file.readStringUntil('\n');
    uid.trim(); // Remove whitespace/newline
    if(uid.length() > 0) {
      Serial.print("Syncing offline UID: ");
      Serial.println(uid);
      bool sent = sendToSupabase(uid);
      if(!sent) {
        // If fail, put back in queue
        saveOffline(uid);
      }
      delay(200); // Small delay between uploads
    }
  }
  file.close();
  
  // 3. Delete processed file
  LittleFS.remove("/sending.txt");
  Serial.println("Offline Queue Processing Complete");
}
