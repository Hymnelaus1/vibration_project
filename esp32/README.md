# ESP32 Firmware

This folder will contain the Arduino sketch for the ESP32 sensor node.

## What the ESP32 does
- Reads raw ADC values from the vibration sensor (e.g. ADXL335, SW-420, or piezo)
- Converts ADC → voltage
- Sends a JSON POST request to the Flask server over WiFi

## Expected POST payload

```json
{
  "sensor_id": "sensor_1",
  "raw": 2048,
  "voltage": 1.654
}
```

## Endpoint
```
POST http://<server-ip>:5000/data
Content-Type: application/json
```

## Wiring
| ESP32 Pin | Sensor |
|-----------|--------|
| 3.3V      | VCC    |
| GND       | GND    |
| GPIO34    | OUT    |

> GPIO34 is input-only and ADC1 — safe for analog reads while WiFi is active.
