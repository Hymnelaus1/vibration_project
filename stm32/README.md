# STM32L476RG — Vibration ADC + UART

Reads the vibration sensor via ADC and sends readings to the ESP32 over UART.

## Pinout (Nucleo-L476RG)

| STM32 Pin | Function     | Connects To         |
|-----------|-------------|---------------------|
| PA0       | ADC1_IN5    | Vibration sensor OUT |
| PA9       | USART1 TX   | ESP32 GPIO16 (RX2)  |
| PA10      | USART1 RX   | ESP32 GPIO17 (TX2)  |
| GND       | Ground      | ESP32 GND           |

> Both boards operate at **3.3 V** — no level shifter required.

## CubeIDE Peripheral Configuration

**ADC1**
- Channel: IN5 (PA0)
- Resolution: 12 bits
- Conversion mode: Single (software triggered)
- Sampling time: 47.5 cycles

**USART1**
- Baud rate: 115200
- Word length: 8 bits, No parity, 1 stop bit
- Mode: Polling (no DMA/IRQ needed)

## How to Integrate

1. Open your `.ioc` and configure ADC1 + USART1 as above.
2. Click **Generate Code**.
3. Add `vibration_adc_uart.c` to your project's `Core/Src/` folder.
4. In `main.c`, call `vibration_run()` inside `USER CODE BEGIN 2`:

```c
/* USER CODE BEGIN 2 */
vibration_run();   // loops forever at 100 Hz
/* USER CODE END 2 */
```

## Data Format Sent Over UART

One JSON line per sample, terminated with `\n`:

```json
{"sensor_id":"sensor_1","raw":2048,"voltage":1.654}
```

- `raw` → 12-bit ADC value (0–4095)
- `voltage` → converted voltage (0.000–3.300 V)
- Sampling rate: **100 Hz** (10 ms per sample)

## Future: Embedded ML Inference

Later phase — run the One-Class SVM directly on STM32 using [`emlearn`](https://emlearn.readthedocs.io):

```python
import emlearn
c_model = emlearn.convert(trained_sklearn_model, method='inline')
c_model.save(file='stm32/model.h', name='vibration_model')
```

STM32 will then output `NORMAL` / `ANOMALY` instead of raw ADC, removing the need for server-side ML.
