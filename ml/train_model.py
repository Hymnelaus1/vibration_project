"""
Run: python ml/train_model.py  (from project root)

Trains a One-Class SVM on normal motor vibration data collected by the server.
Saves the trained model to ml/model.pkl, which the server loads on startup.

One-Class SVM vs Isolation Forest:
  - More sensitive to the exact boundary of "normal" behaviour
  - Better for low-dimensional feature spaces (we use 5 features)
  - nu=0.05 means ~5% of training samples may be treated as outliers
"""
import os
import sqlite3
import numpy as np
import joblib
from sklearn.svm import OneClassSVM
from sklearn.preprocessing import StandardScaler
from sklearn.pipeline import make_pipeline

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.abspath(os.path.join(BASE_DIR, ".."))
DB_PATH  = os.path.join(ROOT_DIR, "sensor_data.db")

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
    print(f"[TRAIN] {len(X)} windows found")

    if len(X) < 50:
        print(f"Not enough data — need at least 50 windows, have {len(X)}")
        return

    # StandardScaler is critical for SVM — features have very different scales
    model = make_pipeline(
        StandardScaler(),
        OneClassSVM(kernel="rbf", nu=0.05, gamma="scale")
    )
    model.fit(X)

    model_path = os.path.join(BASE_DIR, "model.pkl")
    joblib.dump(model, model_path)
    print(f"[TRAIN] Saved to {model_path}")
    print(f"  Features : RMS, Peak-Peak, Kurtosis, Crest Factor, FFT Energy")
    print(f"  Samples  : {X.shape[0]}")

if __name__ == "__main__":
    train()
