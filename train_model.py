"""
Çalıştır: python train_model.py
Motorun NORMAL çalışmasından toplanmış verilerle modeli eğitir.
"""
import sqlite3
import numpy as np
import joblib
from sklearn.ensemble import IsolationForest

DB_PATH = "sensor_data.db"

def load_features():
    conn   = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("""
        SELECT rms, peak_to_peak, kurtosis, crest_factor, fft_energy
        FROM readings
        WHERE rms IS NOT NULL
        ORDER BY timestamp ASC
    """)
    rows = cursor.fetchall()
    conn.close()
    return np.array(rows, dtype=np.float32)

def train():
    X = load_features()
    print(f"[EĞİTİM] {len(X)} pencere bulundu")

    if len(X) < 50:
        print("⚠ Yeterli veri yok! En az 50 pencere gerekli.")
        print(f"  Şu an: {len(X)} | Eksik: {50 - len(X)}")
        return

    # contamination=0.05 → verinin %5'i anomali sayılır
    # n_estimators=200  → daha kararlı sonuç
    model = IsolationForest(
        n_estimators=200,
        contamination=0.05,
        random_state=42
    )
    model.fit(X)
    joblib.dump(model, "model.pkl")
    print("[EĞİTİM] model.pkl kaydedildi ✓")
    print(f"  Özellikler: RMS, Peak-Peak, Kurtosis, Crest, FFT energy")
    print(f"  Eğitim boyutu: {X.shape}")

if __name__ == "__main__":
    train()