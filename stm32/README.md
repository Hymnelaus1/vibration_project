# STM32 Embedded Inference (Planned)

This folder will contain C code for running the trained SVM model directly on an STM32.

## Plan

1. Train the One-Class SVM on PC using `ml/train_model.py`
2. Export the model to a C header using the [`emlearn`](https://emlearn.readthedocs.io) library:
   ```python
   import emlearn
   cmodel = emlearn.convert(trained_sklearn_model, method='inline')
   cmodel.save(file='stm32/model.h', name='vibration_model')
   ```
3. Include `model.h` in your STM32 project
4. Feed the 5 feature values into the model and get `NORMAL` / `ANOMALY`
5. Send result to ESP32 via UART

## Architecture

```
Vibration Sensor
      ↓ ADC
   STM32
  - Feature extraction (RMS, Kurtosis, etc.)
  - One-Class SVM inference (emlearn)
  - Output: NORMAL=0 / ANOMALY=1
      ↓ UART
   ESP32
  - Sends result + raw data to server over WiFi
```

## Why STM32 instead of ESP32 for inference?
- ESP32 WiFi + ADC + ML = tight on RAM and CPU cycles
- STM32 handles the heavy signal processing; ESP32 just does networking
- Allows offline anomaly detection without a server connection
