from flask import Flask, request, jsonify, render_template_string
from flask_sock import Sock
import sqlite3
from datetime import datetime
import json
import threading
from collections import defaultdict, deque
import joblib
import os
import numpy as np
from features import extract_features

app = Flask(__name__)
sock = Sock(app)
DB_PATH = "sensor_data.db"

clients = set()
clients_lock = threading.Lock()

WINDOW_SIZE = 64
windows = defaultdict(lambda: deque(maxlen=WINDOW_SIZE))

model = None
if os.path.exists("model.pkl"):
    model = joblib.load("model.pkl")
    print("[MODEL] model.pkl yuklendi")

def init_db():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS readings (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            sensor_id       TEXT    NOT NULL,
            raw             INTEGER NOT NULL,
            voltage         REAL    NOT NULL,
            timestamp       TEXT    NOT NULL,
            rms             REAL,
            peak_to_peak    REAL,
            kurtosis        REAL,
            crest_factor    REAL,
            fft_energy      REAL,
            anomaly_score   REAL,
            is_anomaly      INTEGER DEFAULT 0
        )
    """)
    conn.commit()
    conn.close()

def get_last_readings(limit=60):
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("""
        SELECT sensor_id, raw, voltage, timestamp,
               anomaly_score, is_anomaly
        FROM readings ORDER BY timestamp DESC LIMIT ?
    """, (limit,))
    rows = cursor.fetchall()
    conn.close()
    return list(reversed(rows))

def broadcast(data: dict):
    message = json.dumps(data)
    dead = set()
    with clients_lock:
        for ws in clients:
            try:
                ws.send(message)
            except Exception:
                dead.add(ws)
        for ws in dead:
            clients.discard(ws)

@app.route("/data", methods=["POST"])
def receive_data():
    data      = request.get_json()
    sensor_id = data.get("sensor_id", "sensor_1")
    raw       = data["raw"]
    voltage   = data["voltage"]
    ts        = datetime.now().isoformat()

    windows[sensor_id].append(voltage)

    features = None
    anomaly  = 0
    score    = None

    if len(windows[sensor_id]) == WINDOW_SIZE:
        features = extract_features(list(windows[sensor_id]))

        if model is not None:
            feat_vec = np.array([[
                features["rms"],
                features["peak_to_peak"],
                features["kurtosis"],
                features["crest_factor"],
                features["fft_energy"]
            ]])
            score   = float(model.decision_function(feat_vec)[0])
            anomaly = int(model.predict(feat_vec)[0] == -1)

    conn   = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("""
        INSERT INTO readings
        (sensor_id, raw, voltage, timestamp,
         rms, peak_to_peak, kurtosis, crest_factor,
         fft_energy, anomaly_score, is_anomaly)
        VALUES (?,?,?,?,?,?,?,?,?,?,?)
    """, (
        sensor_id, raw, voltage, ts,
        features["rms"]          if features else None,
        features["peak_to_peak"] if features else None,
        features["kurtosis"]     if features else None,
        features["crest_factor"] if features else None,
        features["fft_energy"]   if features else None,
        score, anomaly
    ))
    conn.commit()
    conn.close()

    broadcast({
        "type":      "new_reading",
        "sensor_id": sensor_id,
        "raw":       raw,
        "voltage":   round(voltage, 3),
        "timestamp": ts,
        "anomaly":   anomaly,
        "score":     round(score, 4) if score is not None else None,
        "features":  features
    })

    print(f"[{ts[11:19]}] {sensor_id} V={voltage:.3f} {'ANOMALI!' if anomaly else 'OK'}")
    return jsonify({"status": "ok", "anomaly": anomaly}), 200

@app.route("/readings", methods=["GET"])
def get_readings():
    rows = get_last_readings(60)
    results = [
        {
            "sensor_id":     r[0],
            "raw":           r[1],
            "voltage":       r[2],
            "timestamp":     r[3],
            "anomaly_score": r[4],
            "is_anomaly":    r[5]
        }
        for r in rows
    ]
    return jsonify(results), 200

@sock.route("/ws")
def websocket(ws):
    with clients_lock:
        clients.add(ws)
    rows = get_last_readings(60)
    history = [
        {
            "sensor_id": r[0],
            "raw":       r[1],
            "voltage":   round(r[2], 3),
            "timestamp": r[3],
            "anomaly":   r[5] if r[5] else 0
        }
        for r in rows
    ]
    ws.send(json.dumps({"type": "history", "data": history}))
    try:
        while True:
            ws.receive()
    except Exception:
        pass
    finally:
        with clients_lock:
            clients.discard(ws)

@app.route("/")
def dashboard():
    return render_template_string(open("dashboard.html").read())

if __name__ == "__main__":
    init_db()
    app.run(host="0.0.0.0", port=5000, debug=False)
