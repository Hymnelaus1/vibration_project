# Vibration Anomaly Detection System

Real-time vibration monitoring and anomaly detection using an ESP32 sensor node, a Python server, and a One-Class SVM machine learning model.

## System Architecture

```
Vibration Sensor (ADC)
        ↓
     ESP32
  (WiFi HTTP POST)
        ↓
  Flask Server (PC)
  - Feature extraction on 64-sample windows
  - One-Class SVM anomaly detection
  - SQLite data logging
        ↓
  Live Web Dashboard (WebSocket)
```

## Features Extracted per Window (64 samples)
| Feature | What it detects |
|---------|----------------|
| RMS | Average energy level |
| Peak-to-Peak | Mechanical looseness |
| Kurtosis | Impact / shock events (bearing damage) |
| Crest Factor | Crack / impulse faults |
| FFT Energy | Dominant frequency energy |

## Project Structure

```
vibration_project/
├── server/
│   ├── sunucu.py        ← Flask API + WebSocket server
│   ├── features.py      ← Signal feature extraction
│   └── dashboard.html   ← Real-time web dashboard
├── ml/
│   ├── train_model.py   ← One-Class SVM training script
│   └── model.pkl        ← Trained model (generated, not committed)
├── esp32/
│   └── README.md        ← ESP32 firmware guide
└── stm32/
    └── README.md        ← STM32 embedded inference plan (emlearn)
```

## Quickstart

### 1. Install dependencies
```bash
pip install flask flask-sock scikit-learn numpy joblib
```

### 2. Start the server
```bash
python server/sunucu.py
```
Dashboard available at `http://localhost:5000`

### 3. Collect normal data
Run your ESP32 under **normal** operating conditions and let the server log data. You need at least 50 windows (64 samples each) stored in SQLite.

### 4. Train the model
```bash
python ml/train_model.py
```
This trains a One-Class SVM on the collected normal data and saves `ml/model.pkl`.

### 5. Restart the server
The server auto-loads `ml/model.pkl` on startup. Anomalies will now be flagged in real time.

## API

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/data` | Receive sensor reading from ESP32 |
| GET | `/readings` | Last 60 readings as JSON |
| GET | `/` | Live dashboard |
| WS | `/ws` | WebSocket for real-time updates |

### POST `/data` payload
```json
{
  "sensor_id": "sensor_1",
  "raw": 2048,
  "voltage": 1.654
}
```

## Roadmap

- [x] ESP32 WiFi data collection
- [x] Flask server with SQLite logging
- [x] Feature extraction (RMS, Kurtosis, Crest Factor, FFT)
- [x] One-Class SVM anomaly detection
- [x] Real-time WebSocket dashboard
- [ ] ESP32 Arduino firmware (see `esp32/`)
- [ ] STM32 embedded inference with emlearn (see `stm32/`)
- [ ] Alert system (email / buzzer on anomaly)
