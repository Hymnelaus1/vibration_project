# ESP32 — UART Bridge to WiFi

Receives JSON lines from the STM32 over UART and forwards them to the Flask server via HTTP POST.

## Wiring

| STM32 Pin     | ESP32 Pin       | Description       |
|---------------|-----------------|-------------------|
| PA9 (USART1 TX) | GPIO16 (RX2)  | STM32 → ESP32     |
| PA10 (USART1 RX)| GPIO17 (TX2)  | ESP32 → STM32     |
| GND           | GND             | Common ground     |

> Both boards run at **3.3 V** — direct connection, no level shifter.

## Required Libraries

Install via **Arduino Library Manager**:
- `ArduinoJson` by Benoit Blanchon (version 6.x)

Built-in with ESP32 Arduino core (no install needed):
- `WiFi`
- `HTTPClient`

## Setup

1. Open `vibration_wifi.ino` in Arduino IDE.
2. Fill in your credentials at the top:

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL    = "http://192.168.1.XX:5000/data";
```

3. Select board: **ESP32 Dev Module** (or your specific variant).
4. Flash and open Serial Monitor at 115200 baud to see live output.

## What It Does

```
STM32 sends (UART 115200):
  {"sensor_id":"sensor_1","raw":2048,"voltage":1.654}\n

ESP32 receives → validates JSON → HTTP POST to Flask server
```

- Validates JSON before forwarding (drops malformed lines)
- Auto-reconnects to WiFi if connection drops
- Logs each reading to USB Serial Monitor for debugging

## Full System Architecture

```
Vibration Sensor
      ↓ analog
  STM32L476RG
  - ADC read (100 Hz)
  - UART TX (115200)
      ↓ UART
   ESP32
  - UART RX
  - HTTP POST (JSON)
      ↓ WiFi
  Flask Server (PC)
  - Feature extraction
  - One-Class SVM
  - SQLite logging
      ↓ WebSocket
  Web Dashboard
```
