# Wiring Diagram: ESP32-C6 <-> Adafruit PN532 (I2C)

## 1. Hardware Prep (CRITICAL)
Before wiring, check the **SEL0** and **SEL1** jumpers/switches on your PN532 breakout board. 
For **I2C mode**, they must be set to:
*   **SEL0**: ON (High)
*   **SEL1**: OFF (Low)

## 2. Connections

| PN532 Pin | ESP32-C6 Pin | Function        | Note                                      |
| :-------- | :----------- | :-------------- | :---------------------------------------- |
| 3.3V      | 3V3          | Power           | **Connect 3.3V to 3.3V** (Bypassing PN532 regulator) |
| GND       | GND          | Ground          | Common Ground                             |
| SDA       | GPIO 4       | I2C Data        |                                           |
| SCL       | GPIO 5       | I2C Clock       |                                           |
| IRQ       | GPIO 0       | Interrupt       |                                           |
| RST       | GPIO 1       | Reset           |                                           |

*Note: Disconnect any previous circuits (LEDs/Buttons) from these pins.*

## 3. Status LED (On-board GPIO 8)
*   **Orange**: Initializing
*   **Blue**: Ready / Waiting for Card
*   **Green (Flash)**: Card Detected
*   **Red**: Error (PN532 not found)
