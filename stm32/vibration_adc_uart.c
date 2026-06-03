/**
 * vibration_adc_uart.c
 * STM32L476RG — Dual ADC (Vibration + LM35 Temperature) + UART LL
 *
 * Sensor 1 — Vibration : PA0  → ADC1_IN5   (CN8 pin 28)
 * Sensor 2 — LM35 Temp : PA1  → ADC1_IN6   (CN8 pin 30)
 * UART TX               : PA9  → USART1_TX  (CN10 pin 21) → ESP32 GPIO16
 * UART RX               : PA10 → USART1_RX  (CN10 pin 33) ← ESP32 GPIO17
 *
 * LM35 output: 10 mV / °C
 *   temp_c = voltage_mv / 10
 *
 * JSON sent per sample (100 Hz):
 *   {"sensor_id":"sensor_1","raw":2048,"voltage":1.654,"temp_c":24.9}\n
 *
 * CubeIDE .ioc → Project Manager → Advanced Settings:
 *   ADC1   → LL   (add IN5 + IN6)
 *   USART1 → LL
 */

#include "stm32l4xx_ll_adc.h"
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_utils.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ── Config ─────────────────────────────────────────────────────── */
#define SENSOR_ID        "sensor_1"
#define VREF_MV          3300UL
#define ADC_FULL_SCALE   4095UL
#define SAMPLE_PERIOD_MS 10          /* 100 Hz */
#define USART1_CLK_HZ    80000000UL

/* ADC channels */
#define VIB_CHANNEL      LL_ADC_CHANNEL_5   /* PA0 — vibration sensor */
#define TEMP_CHANNEL     LL_ADC_CHANNEL_6   /* PA1 — LM35             */

/* ── GPIO Init ───────────────────────────────────────────────────── */
static void gpio_init(void)
{
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    /* PA0 — vibration sensor analog input */
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_0, LL_GPIO_MODE_ANALOG);
    LL_GPIO_EnablePinAnalogControl(GPIOA, LL_GPIO_PIN_0);

    /* PA1 — LM35 analog input */
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_1, LL_GPIO_MODE_ANALOG);
    LL_GPIO_EnablePinAnalogControl(GPIOA, LL_GPIO_PIN_1);

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

/* ── ADC Init ────────────────────────────────────────────────────── */
static void adc_init(void)
{
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_ADC);

    LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(ADC1),
                          LL_ADC_CLOCK_SYNC_PCLK_DIV4);

    LL_ADC_DisableDeepPowerDown(ADC1);
    LL_ADC_EnableInternalRegulator(ADC1);
    LL_mDelay(1);

    LL_ADC_StartCalibration(ADC1, LL_ADC_SINGLE_ENDED);
    while (LL_ADC_IsCalibrationOnGoing(ADC1));

    LL_ADC_SetResolution   (ADC1, LL_ADC_RESOLUTION_12B);
    LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_RIGHT);
    LL_ADC_SetLowPowerMode (ADC1, LL_ADC_LP_MODE_NONE);

    LL_ADC_REG_SetTriggerSource  (ADC1, LL_ADC_REG_TRIG_SOFTWARE);
    LL_ADC_REG_SetContinuousMode (ADC1, LL_ADC_REG_CONV_SINGLE);
    LL_ADC_REG_SetOverrun        (ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
    LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);

    /* Pre-configure sampling time for both channels */
    LL_ADC_SetChannelSamplingTime(ADC1, VIB_CHANNEL,
                                  LL_ADC_SAMPLINGTIME_47CYCLES_5);
    LL_ADC_SetChannelSingleDiff  (ADC1, VIB_CHANNEL, LL_ADC_SINGLE_ENDED);

    LL_ADC_SetChannelSamplingTime(ADC1, TEMP_CHANNEL,
                                  LL_ADC_SAMPLINGTIME_247CYCLES_5); /* LM35 needs longer */
    LL_ADC_SetChannelSingleDiff  (ADC1, TEMP_CHANNEL, LL_ADC_SINGLE_ENDED);

    /* Start with vibration channel selected */
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, VIB_CHANNEL);

    LL_ADC_Enable(ADC1);
    while (!LL_ADC_IsActiveFlag_ADRDY(ADC1));
}

/* ── USART Init ──────────────────────────────────────────────────── */
static void usart_init(void)
{
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);
    LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK2);

    LL_USART_SetBaudRate       (USART1, USART1_CLK_HZ,
                                LL_USART_OVERSAMPLING_16, 115200);
    LL_USART_SetDataWidth      (USART1, LL_USART_DATAWIDTH_8B);
    LL_USART_SetStopBitsLength (USART1, LL_USART_STOPBITS_1);
    LL_USART_SetParity         (USART1, LL_USART_PARITY_NONE);
    LL_USART_SetTransferDirection(USART1, LL_USART_DIRECTION_TX_RX);
    LL_USART_SetHWFlowCtrl     (USART1, LL_USART_HWCONTROL_NONE);
    LL_USART_SetOverSampling   (USART1, LL_USART_OVERSAMPLING_16);

    LL_USART_Enable(USART1);
    while (!LL_USART_IsActiveFlag_TEACK(USART1));
    while (!LL_USART_IsActiveFlag_REACK(USART1));
}

/* ── ADC: channel-switching single read ─────────────────────────── */
static uint16_t adc_read_channel(uint32_t channel)
{
    /* Point sequencer to requested channel */
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, channel);

    /* Trigger and wait */
    LL_ADC_REG_StartConversion(ADC1);
    while (!LL_ADC_IsActiveFlag_EOC(ADC1));
    uint16_t val = (uint16_t)LL_ADC_REG_ReadConversionData12(ADC1);
    LL_ADC_ClearFlag_EOC(ADC1);
    return val;
}

/* ── UART helpers ────────────────────────────────────────────────── */
static void usart_send_byte(uint8_t byte)
{
    while (!LL_USART_IsActiveFlag_TXE(USART1));
    LL_USART_TransmitData8(USART1, byte);
}

static void usart_send_string(const char *s)
{
    while (*s) { usart_send_byte((uint8_t)*s++); }
    while (!LL_USART_IsActiveFlag_TC(USART1));
}

/* ── Public API ──────────────────────────────────────────────────── */
void vibration_ll_init(void)
{
    gpio_init();
    adc_init();
    usart_init();
}

/**
 * Main loop — 100 Hz
 *
 * Each iteration reads:
 *   1. Vibration sensor  (PA0 / ADC1_IN5)
 *   2. LM35 temperature  (PA1 / ADC1_IN6)
 *
 * Sends one JSON line over UART:
 *   {"sensor_id":"sensor_1","raw":2048,"voltage":1.654,"temp_c":24.9}\n
 */
void vibration_ll_run(void)
{
    char     buf[120];
    uint16_t vib_raw, temp_raw;
    uint32_t voltage_mv, temp_mv;
    uint32_t temp_int, temp_frac;

    for (;;)
    {
        /* 1. Read vibration sensor (IN5 / PA0) */
        vib_raw    = adc_read_channel(VIB_CHANNEL);
        voltage_mv = (vib_raw * VREF_MV) / ADC_FULL_SCALE;

        /* 2. Read LM35 (IN6 / PA1)
         *    LM35 output: 10 mV per °C
         *    temp_c = voltage_mv / 10
         *    e.g. 249 mV → 24.9 °C
         */
        temp_raw  = adc_read_channel(TEMP_CHANNEL);
        temp_mv   = (temp_raw * VREF_MV) / ADC_FULL_SCALE;
        temp_int  = temp_mv / 10;        /* whole degrees  */
        temp_frac = temp_mv % 10;        /* tenths         */

        /* 3. Build JSON — no float printf */
        snprintf(buf, sizeof(buf),
                 "{\"sensor_id\":\"%s\","
                 "\"raw\":%u,"
                 "\"voltage\":%lu.%03lu,"
                 "\"temp_c\":%lu.%lu}\n",
                 SENSOR_ID,
                 (unsigned)vib_raw,
                 voltage_mv / 1000UL, voltage_mv % 1000UL,
                 temp_int, temp_frac);

        /* 4. Send */
        usart_send_string(buf);

        /* 5. Wait 10 ms → 100 Hz */
        LL_mDelay(SAMPLE_PERIOD_MS);
    }
}
