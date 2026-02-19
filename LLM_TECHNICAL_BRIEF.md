# ESP32-C6 + PN532 Integration: LLM Technical Brief

## 1. Context & Objective
**Goal:** Develop a production-ready IoT firmware for the **ESP32-C6 (RISC-V)** to interface with an **Adafruit PN532 NFC/RFID Controller** via **I2C**.
**Key Requirements:**
*   Read ISO14443A (Mifare) UIDs.
*   Connect to Wi-Fi (2.4GHz) with robust reconnection logic.
*   Log data to a Supabase PostgreSQL database via REST API.
*   Implement Offline Caching (LittleFS) for reliability.
*   Provide real-time LED status feedback.

## 2. Hardware Architecture & Pinout
**Platform:** ESP32-C6 DevKitC-1 (RISC-V Architecture)
**Sensor:** Adafruit PN532 Breakout Board (v1.3/v1.6)

### 2.1 Wiring Configuration (I2C)
The ESP32-C6 has flexible GPIO matrixing, but we selected "Safe" pins to avoid boot strapping conflicts and USB/UART interference.

| PN532 Pin | ESP32-C6 Pin | Function | Critical Note |
| :--- | :--- | :--- | :--- |
| **3.3V** | **3V3** | Power | **DO NOT use 5V.** The PN532 breakout has a regulator that drops out if fed 3.3V into the 5V pin. Feed 3.3V directly to the 3.3V pin to bypass the regulator. |
| **GND** | **GND** | Ground | Common ground is essential. |
| **SDA** | **GPIO 4** | I2C Data | Standard I2C pin for C6. |
| **SCL** | **GPIO 5** | I2C Clock | Standard I2C pin for C6. |
| **IRQ** | **GPIO 0** | Interrupt | Used for non-blocking checks (future proofing). |
| **RST** | **GPIO 1** | Reset | Hardware reset for the PN532. |

### 2.2 Board Configuration (PN532)
*   **SEL0:** ON (High)
*   **SEL1:** OFF (Low)
*   *Why:* This hardwires the PN532 chip into **I2C Mode**. SPI/UART modes require different jumper settings.

## 3. Software Stack & Frameworks
**Build System:** PlatformIO Core
**Framework:** Arduino (via `pioarduino` fork)

### 3.1 `platformio.ini` Configuration
The standard `espressif32` platform often lags for newer RISC-V chips like the C6. We utilized the community fork for stability.

```ini
[env:esp32-c6-devkitc-1]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32-c6-devkitc-1
framework = arduino
board_build.filesystem = littlefs  ; Enable LittleFS for offline caching
monitor_speed = 115200

# USB / Serial Configuration (Critical for C6)
build_flags =
    -D ARDUINO_USB_CDC_ON_BOOT=0  ; Use Hardware UART (CP210x), not Native USB
    -D ARDUINO_USB_MODE=1

lib_deps =
    adafruit/Adafruit PN532 @ ^1.3.3
    adafruit/Adafruit NeoPixel @ ^1.12.0
    bblanchon/ArduinoJson @ ^7.0.3
```

## 4. Key Implementation Challenges & Solutions

### 4.1 USB/Serial Port Conflict (The "Busy" Error)
**Issue:** `[Errno 35] Resource temporarily unavailable` during upload.
**Cause:** The ESP32-C6 (especially on macOS) handles USB enumeration differently. Opening the Serial Monitor locks the port, preventing `esptool` from resetting the board for upload.
**Solution:**
1.  **Golden Rule:** Close ALL serial monitors before hitting Upload.
2.  **Hardware Reset:** If stuck, unplug/replug the device to clear the USB stack state.

### 4.2 I2C Initialization
**Issue:** Default `Wire.begin()` might not map correctly on C6 variants.
**Solution:** Explicitly define pins in code:
```cpp
Wire.begin(PN532_SDA_PIN, PN532_SCL_PIN); // GPIO 4, 5
```

### 4.3 Offline Caching Logic (LittleFS)
**Requirement:** Data must not be lost if Wi-Fi drops.
**Implementation:**
1.  **Queue System:** We append failed payloads to `/queue.txt` in LittleFS.
2.  **Sync Mechanism:** A non-blocking timer in `loop()` checks for connection + file existence every 30s.
3.  **Atomic Processing:**
    *   Rename `/queue.txt` -> `/sending.txt` (Prevents writing to the file while reading it).
    *   Read line-by-line -> Try Upload -> If Success, continue.
    *   If Fail, write back to `/queue.txt`.
    *   Delete `/sending.txt` when done.

### 4.4 Wi-Fi Stability on ESP32-C6
**Issue:** Connection timeouts or refusal to connect to certain routers.
**Solution:**
1.  **Explicit Mode:** Call `WiFi.mode(WIFI_STA)` to disable the default AP mode (saves power/interference).
2.  **Clean Slate:** Call `WiFi.disconnect()` before `WiFi.begin()` to clear stale configs.
3.  **Extended Timeout:** Increased connection wait loop from 10s to **60s**. The C6 radio stack can take longer to negotiate WPA3/WPA2 handshakes.

### 4.5 The "Ghost" Cart Add (Amazon Integration)
**Discovery:** Amazon has no public API for "Add to Cart."
**Workaround:** We utilize **Voice Monkey** (a 3rd party Alexa Skill API).
**Flow:**
*   ESP32 -> Supabase -> n8n Webhook -> Voice Monkey API -> Virtual Doorbell -> Alexa Routine -> "Alexa, Reorder Red Bull".

## 5. Code Structure (Best Practices)
*   **`secrets.h`:** Separation of concerns. Credentials (SSID, API Keys) are strictly excluded from Git via `.gitignore`.
*   **Non-Blocking Delay:** Used `millis()` timers for the sync loop to keep the NFC reader responsive.
*   **Visual Feedback:**
    *   🟠 **Orange:** Boot
    *   🔵 **Blue:** Wi-Fi Connected
    *   🔴 **Red:** Offline / Error
    *   🟢 **Green Flash:** Transaction Success
    *   🟡 **Yellow Flash:** Saved to Offline Cache

## 6. Future Considerations
*   **Deep Sleep:** The PN532 consumes ~100mA. For battery use, implement `esp_deep_sleep_start()` and use the PN532 `IRQ` pin (GPIO 0) to wake the ESP32 only when a card is present.
*   **Security:** Currently uses `HTTP` (or `HTTPS` without strict cert pinning). For enterprise, implement mTLS or verify Supabase SSL fingerprints.
