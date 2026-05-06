/**
 * vibration_wifi.ino
 * ESP32 — UART Bridge to WiFi
 *
 * Receives JSON lines from STM32L476RG over UART,
 * then HTTP POSTs them to the Flask server.
 *
 * WIRING
 * ──────
 *  STM32 PA9  (USART1 TX)  →  ESP32 GPIO16 (RX2)
 *  STM32 PA10 (USART1 RX)  ←  ESP32 GPIO17 (TX2)
 *  STM32 GND               →  ESP32 GND
 *  (Both boards run at 3.3 V — no level shifter needed)
 *
 * LIBRARIES REQUIRED
 * ──────────────────
 *  - ArduinoJson   (install via Arduino Library Manager)
 *  - HTTPClient    (built-in with ESP32 Arduino core)
 *  - WiFi          (built-in with ESP32 Arduino core)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/* ── User configuration ──────────────────────────────────────── */
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL    = "http://YOUR_PC_IP:5000/data";
/*
 * Replace YOUR_PC_IP with the local IP of the PC running sunucu.py.
 * Find it with:  ipconfig (Windows)  /  ip a (Linux)
 * Example: "http://192.168.1.42:5000/data"
 */

/* ── UART2 pins ──────────────────────────────────────────────── */
#define UART_RX_PIN  16   /* GPIO16 ← STM32 TX (PA9)  */
#define UART_TX_PIN  17   /* GPIO17 → STM32 RX (PA10) */
#define UART_BAUD    115200

/* ── Internal state ──────────────────────────────────────────── */
HardwareSerial stm32Serial(2);   /* UART2 */

/* ── Helpers ─────────────────────────────────────────────────── */
void connect_wifi()
{
    Serial.print("[WiFi] Connecting to ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("[WiFi] Connected — IP: ");
    Serial.println(WiFi.localIP());
}

/**
 * Send a JSON payload to the Flask server via HTTP POST.
 * Returns the HTTP response code (200 = OK, -1 = connection error).
 */
int post_to_server(const char* json_payload)
{
    if (WiFi.status() != WL_CONNECTED) {
        connect_wifi();
    }

    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");

    int response_code = http.POST(json_payload);

    if (response_code < 0) {
        Serial.print("[HTTP] Error: ");
        Serial.println(http.errorToString(response_code));
    }

    http.end();
    return response_code;
}

/* ── Arduino setup ───────────────────────────────────────────── */
void setup()
{
    /* Debug serial (USB) */
    Serial.begin(115200);
    Serial.println("[BOOT] ESP32 Vibration Bridge");

    /* UART to STM32 */
    stm32Serial.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    Serial.println("[UART] Listening on GPIO16/17 at 115200");

    /* WiFi */
    connect_wifi();
}

/* ── Arduino loop ────────────────────────────────────────────── */
void loop()
{
    /*
     * STM32 sends one JSON line per sample, ending with '\n'.
     * readStringUntil('\n') blocks until a full line arrives.
     *
     * Expected format from STM32:
     *   {"sensor_id":"sensor_1","raw":2048,"voltage":1.654}\n
     */
    if (!stm32Serial.available()) return;

    String line = stm32Serial.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) return;

    /* Validate JSON before forwarding */
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, line);

    if (err) {
        Serial.print("[JSON] Parse error: ");
        Serial.println(err.c_str());
        return;
    }

    /* Log to USB serial */
    uint16_t raw     = doc["raw"];
    float    voltage = doc["voltage"];
    Serial.printf("[DATA] raw=%u  voltage=%.3f V\n", raw, voltage);

    /* Forward to Flask server */
    int code = post_to_server(line.c_str());
    Serial.printf("[HTTP] Response: %d\n", code);
}
