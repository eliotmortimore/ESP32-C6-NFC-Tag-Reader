# ESP32-C6 NFC/RFID Scanner (Supabase Logger)

An IoT project that turns an **ESP32-C6** into a networked NFC scanner. It reads ISO14443A tags (Mifare, etc.) using an **Adafruit PN532** breakout board and instantly logs the Unique ID (UID) to a **Supabase** PostgreSQL database via WiFi.

## 🚀 Features
*   **WiFi Connected:** Connects to 2.4GHz networks (supports open and password-protected).
*   **Database Logging:** Sends JSON POST requests to Supabase REST API.
*   **Visual Feedback:** On-board RGB LED indicates status (Boot, WiFi, Scan, Success, Error).
*   **Modern Core:** Built for the RISC-V ESP32-C6 using the `pioarduino` platform.

## 🛠 Hardware Required
*   **ESP32-C6 DevKitC-1** (or similar C6 board).
*   **Adafruit PN532 Breakout Board** (v1.3 or v1.6).
*   Jumper wires.

## 🔌 Wiring & Configuration

### 1. PN532 Jumpers (Critical)
Before wiring, set the selection jumpers/switches on the PN532 board for **I2C Mode**:
*   **SEL0:** ON (High)
*   **SEL1:** OFF (Low)

### 2. Pin Connections
| PN532 Pin | ESP32-C6 Pin | Function | Note |
| :--- | :--- | :--- | :--- |
| **3.3V** | **3V3** | Power | **Connect 3.3V to 3.3V** (Bypass PN532 regulator) |
| **GND** | **GND** | Ground | Common Ground |
| **SDA** | **GPIO 4** | I2C Data | |
| **SCL** | **GPIO 5** | I2C Clock | |
| **IRQ** | **GPIO 0** | Interrupt | |
| **RST** | **GPIO 1** | Reset | |

## 💻 Software Setup

### 1. Prerequisites
*   [VS Code](https://code.visualstudio.com/) with **PlatformIO** extension installed.

### 2. Configuration
1.  Clone this repository.
2.  Navigate to `include/`.
3.  Rename `secrets_example.h` to `secrets.h` (this file is ignored by git).
4.  Enter your credentials:

```cpp
// include/secrets.h
#define WIFI_SSID       "Your_Network_Name"
#define WIFI_PASSWORD   "Your_Password"      // Leave empty "" for open networks
#define SUPABASE_URL    "https://your-project.supabase.co"
#define SUPABASE_KEY    "your-anon-key"
```

### 3. Database Setup (Supabase)
1.  Create a new Supabase project.
2.  Go to the **Table Editor** and create a table named `scans`.
3.  Add the following columns:
    *   `id` (int8, primary key)
    *   `created_at` (timestamptz, default: now())
    *   `uid` (text) - *This stores the NFC Tag ID*
4.  Ensure your API Key has permission to insert into this table.

### 4. Build & Upload
Connect your ESP32-C6 via USB and run:
```bash
platformio run --target upload
```

## 💡 LED Status Codes
The on-board RGB LED (GPIO 8) provides real-time feedback:

| Color | Pattern | Status |
| :--- | :--- | :--- |
| 🟠 **Orange** | Solid | Booting / Initializing Hardware |
| 🔵 **Blue** | Blinking | Connecting to WiFi |
| 🔵 **Blue** | Solid | **Ready to Scan** (Connected to WiFi) |
| 🟡 **Yellow** | Solid | Processing Scan / Uploading to API |
| 🟢 **Green** | Flash | **Success!** Data saved to database |
| 🔴 **Red** | Flash/Solid | **Error** (WiFi failed, API Error, or PN532 missing) |

## 📄 License
Open Source. Feel free to modify and use.
