/**
 * vibration_adc_uart.c
 * STM32L476RG — Vibration ADC + UART using LL (Low-Layer) drivers
 *
 * Reads vibration sensor on PA0 (ADC1_IN5) at 100 Hz.
 * Sends JSON lines over USART1 (PA9 TX) to ESP32 at 115200 baud.
 *
 * LL drivers are lighter than HAL: no OS overhead, no callbacks,
 * direct peripheral register access through thin inline wrappers.
 *
 * ─────────────────────────────────────────────────────────────────
 * CUBEIDE SETUP (.ioc)
 * ─────────────────────────────────────────────────────────────────
 * Project Manager → Advanced Settings:
 *   ADC1   → LL
 *   USART1 → LL
 *   GPIO   → LL
 *   (RCC and SYS can remain HAL — only used for clock init)
 *
 * Pinout:
 *   PA0  → ADC1_IN5      (CN7-28 / Arduino A0)  ← sensor OUT
 *   PA9  → USART1_TX     (CN10-21)               → ESP32 GPIO16
 *   PA10 → USART1_RX     (CN10-33)               ← ESP32 GPIO17
 *
 * ADC1 settings in .ioc:
 *   Resolution      12 bit
 *   Channel         IN5 (PA0)
 *   Conversion mode Single
 *   Sampling time   47.5 cycles
 *   External trigger Software start
 *
 * USART1 settings in .ioc:
 *   Baud            115200
 *   Word length     8 bit
 *   Parity          None
 *   Stop bits       1
 *   Direction       TX/RX
 *
 * ─────────────────────────────────────────────────────────────────
 * HOW TO INTEGRATE
 * ─────────────────────────────────────────────────────────────────
 * After CubeIDE generates the project (with LL selected):
 *
 *   Option A — let CubeIDE generate the init, add only the app:
 *     In main.c, inside  USER CODE BEGIN 2 :
 *         vibration_ll_run();   // never returns
 *
 *   Option B — use this file standalone (includes full init):
 *     Call  vibration_ll_init()  then  vibration_ll_run()
 *     from main(). Remove the generated MX_ADC1_Init /
 *     MX_USART1_UART_Init calls to avoid conflicts.
 * ─────────────────────────────────────────────────────────────────
 */

#include "stm32l4xx_ll_adc.h"
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_utils.h"   /* LL_mDelay */
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ── User config ────────────────────────────────────────────────── */
#define SENSOR_ID        "sensor_1"
#define VREF_MV          3300UL      /* mV — adjust if using ext. VREF */
#define ADC_FULL_SCALE   4095UL      /* 12-bit */
#define SAMPLE_PERIOD_MS 10          /* 10 ms = 100 Hz                 */

/*
 * USART1 peripheral clock (Hz).
 * By default USART1 is clocked from PCLK2.
 * On Nucleo-L476RG at 80 MHz PLL: PCLK2 = 80 MHz.
 * Run  LL_RCC_GetUSARTClockFreq(LL_RCC_USART1_CLKSOURCE)
 * if you are unsure, or just use SystemCoreClock when PCLK2 = HCLK.
 */
#define USART1_CLK_HZ    80000000UL

/* ────────────────────────────────────────────────────────────────
 * GPIO INIT
 * PA0  → Analog  (ADC input, no pull, no speed needed)
 * PA9  → AF7     (USART1_TX, push-pull, high speed)
 * PA10 → AF7     (USART1_RX, push-pull, high speed)
 * ──────────────────────────────────────────────────────────────── */
static void gpio_init(void)
{
    /* Enable GPIOA clock on AHB2 */
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    /* PA0 — analog input for ADC */
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_0, LL_GPIO_MODE_ANALOG);
    LL_GPIO_EnablePinAnalogControl(GPIOA, LL_GPIO_PIN_0); /* ASCR bit — required on L4 */

    /* PA9 — USART1 TX */
    LL_GPIO_SetPinMode       (GPIOA, LL_GPIO_PIN_9,  LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_8_15    (GPIOA, LL_GPIO_PIN_9,  LL_GPIO_AF_7);
    LL_GPIO_SetPinSpeed      (GPIOA, LL_GPIO_PIN_9,  LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinOutputType (GPIOA, LL_GPIO_PIN_9,  LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinPull       (GPIOA, LL_GPIO_PIN_9,  LL_GPIO_PULL_NO);

    /* PA10 — USART1 RX */
    LL_GPIO_SetPinMode       (GPIOA, LL_GPIO_PIN_10, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_8_15    (GPIOA, LL_GPIO_PIN_10, LL_GPIO_AF_7);
    LL_GPIO_SetPinSpeed      (GPIOA, LL_GPIO_PIN_10, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinOutputType (GPIOA, LL_GPIO_PIN_10, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinPull       (GPIOA, LL_GPIO_PIN_10, LL_GPIO_PULL_NO);
}

/* ────────────────────────────────────────────────────────────────
 * ADC INIT  (ADC1, channel IN5, 12-bit, software trigger, polling)
 *
 * L476 ADC startup sequence (RM0351 §16):
 *   1. Exit deep power-down
 *   2. Enable internal voltage regulator, wait ≥ 20 µs
 *   3. Run calibration (single-ended)
 *   4. Wait for ADVREGEN stable, then enable ADC
 *   5. Wait for ADRDY flag
 * ──────────────────────────────────────────────────────────────── */
static void adc_init(void)
{
    /* 1. Enable ADC1 clock on AHB2 */
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_ADC);

    /* 2. Set ADC common clock: synchronous, PCLK/4
     *    (safe choice — avoids exceeding 80 MHz ADC input clock on L4) */
    LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(ADC1),
                          LL_ADC_CLOCK_SYNC_PCLK_DIV4);

    /* 3. Exit deep power-down mode */
    LL_ADC_DisableDeepPowerDown(ADC1);

    /* 4. Enable internal voltage regulator */
    LL_ADC_EnableInternalRegulator(ADC1);
    LL_mDelay(1); /* Wait > 20 µs for ADVREGEN stable */

    /* 5. Calibrate for single-ended inputs */
    LL_ADC_StartCalibration(ADC1, LL_ADC_SINGLE_ENDED);
    while (LL_ADC_IsCalibrationOnGoing(ADC1));

    /* 6. Configure ADC resolution and alignment */
    LL_ADC_SetResolution   (ADC1, LL_ADC_RESOLUTION_12B);
    LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_RIGHT);
    LL_ADC_SetLowPowerMode (ADC1, LL_ADC_LP_MODE_NONE);

    /* 7. Configure regular conversion sequence (single channel) */
    LL_ADC_REG_SetTriggerSource  (ADC1, LL_ADC_REG_TRIG_SOFTWARE);
    LL_ADC_REG_SetContinuousMode (ADC1, LL_ADC_REG_CONV_SINGLE);
    LL_ADC_REG_SetOverrun        (ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
    LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
    LL_ADC_REG_SetSequencerRanks (ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_5);

    /* 8. Sampling time for channel 5 (47.5 ADC clock cycles) */
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_5,
                                  LL_ADC_SAMPLINGTIME_47CYCLES_5);
    LL_ADC_SetChannelSingleDiff  (ADC1, LL_ADC_CHANNEL_5,
                                  LL_ADC_SINGLE_ENDED);

    /* 9. Enable ADC, wait for ADRDY */
    LL_ADC_Enable(ADC1);
    while (!LL_ADC_IsActiveFlag_ADRDY(ADC1));
}

/* ────────────────────────────────────────────────────────────────
 * USART1 INIT  (115200 8N1, TX+RX, no hardware flow control)
 * ──────────────────────────────────────────────────────────────── */
static void usart_init(void)
{
    /* Enable USART1 clock on APB2 */
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);

    /* Select PCLK2 as USART1 clock source */
    LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK2);

    /* Baud rate, word length, stop bits, parity */
    LL_USART_SetBaudRate(USART1,
                         USART1_CLK_HZ,
                         LL_USART_PRESCALER_DIV1,
                         LL_USART_OVERSAMPLING_16,
                         115200);
    LL_USART_SetDataWidth          (USART1, LL_USART_DATAWIDTH_8B);
    LL_USART_SetStopBitsLength     (USART1, LL_USART_STOPBITS_1);
    LL_USART_SetParity             (USART1, LL_USART_PARITY_NONE);
    LL_USART_SetTransferDirection  (USART1, LL_USART_DIRECTION_TX_RX);
    LL_USART_SetHWFlowCtrl         (USART1, LL_USART_HWCONTROL_NONE);
    LL_USART_SetOverSampling       (USART1, LL_USART_OVERSAMPLING_16);

    /* Enable USART, wait for TX and RX to be ready */
    LL_USART_Enable(USART1);
    while (!LL_USART_IsActiveFlag_TEACK(USART1));
    while (!LL_USART_IsActiveFlag_REACK(USART1));
}

/* ────────────────────────────────────────────────────────────────
 * APPLICATION HELPERS
 * ──────────────────────────────────────────────────────────────── */

/**
 * Trigger one ADC conversion and return the 12-bit result.
 * Blocking — waits for the EOC (End of Conversion) flag.
 */
static uint16_t adc_read(void)
{
    LL_ADC_REG_StartConversion(ADC1);
    while (!LL_ADC_IsActiveFlag_EOC(ADC1));
    uint16_t val = (uint16_t)LL_ADC_REG_ReadConversionData12(ADC1);
    LL_ADC_ClearFlag_EOC(ADC1);
    return val;
}

/**
 * Send a single byte over USART1.
 * Waits for the transmit data register to be empty (TXE).
 */
static void usart_send_byte(uint8_t byte)
{
    while (!LL_USART_IsActiveFlag_TXE(USART1));
    LL_USART_TransmitData8(USART1, byte);
}

/**
 * Send a null-terminated string over USART1, byte by byte.
 */
static void usart_send_string(const char *s)
{
    while (*s)
    {
        usart_send_byte((uint8_t)*s);
        s++;
    }
    /* Wait for the last byte to finish transmitting before returning */
    while (!LL_USART_IsActiveFlag_TC(USART1));
}

/* ────────────────────────────────────────────────────────────────
 * PUBLIC API
 * ──────────────────────────────────────────────────────────────── */

/**
 * Initialize GPIO, ADC1, and USART1 using LL drivers.
 *
 * Call this from main() before vibration_ll_run().
 * Skip this if you let CubeIDE generate the MX_ init functions
 * (select LL in Advanced Settings — CubeIDE generates equivalent code).
 */
void vibration_ll_init(void)
{
    gpio_init();
    adc_init();
    usart_init();
}

/**
 * Main sampling loop — runs forever at 100 Hz.
 *
 * Each iteration:
 *   1. Read ADC (PA0 / IN5)
 *   2. Convert raw value → voltage in mV
 *   3. Send JSON line over USART1
 *   4. Wait 10 ms
 *
 * JSON format (newline delimited):
 *   {"sensor_id":"sensor_1","raw":2048,"voltage":1.654}\n
 */
void vibration_ll_run(void)
{
    char     buf[80];
    uint16_t raw;
    uint32_t voltage_mv;

    for (;;)
    {
        /* --- Sample ------------------------------------------------ */
        raw        = adc_read();
        voltage_mv = (raw * VREF_MV) / ADC_FULL_SCALE;

        /* --- Format JSON ------------------------------------------- */
        /*
         * voltage_mv is in millivolts (e.g. 1654 for 1.654 V).
         * Split into integer and fractional parts to avoid printf float,
         * which pulls in heavy newlib soft-float code.
         * Using %lu / %lu keeps binary small and execution fast.
         */
        snprintf(buf, sizeof(buf),
                 "{\"sensor_id\":\"%s\","
                 "\"raw\":%u,"
                 "\"voltage\":%lu.%03lu}\n",
                 SENSOR_ID,
                 (unsigned)raw,
                 voltage_mv / 1000UL,
                 voltage_mv % 1000UL);

        /* --- Transmit ---------------------------------------------- */
        usart_send_string(buf);

        /* --- Wait for next sample (10 ms = 100 Hz) ----------------- */
        LL_mDelay(SAMPLE_PERIOD_MS);
    }
}
