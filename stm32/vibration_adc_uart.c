/**
 * vibration_adc_uart.c
 * STM32L476RG — Vibration ADC + PIR EXTI + UART (LL drivers)
 *
 * ─────────────────────────────────────────────────────────────────
 * BAĞLANTI (WIRING)
 * ─────────────────────────────────────────────────────────────────
 *
 *  Titreşim Sensörü:
 *    SENSOR OUT  →  PA0  (CN8  pin 28)   ADC1_IN5
 *    SENSOR VCC  →  3.3V (CN8  pin 15)
 *    SENSOR GND  →  GND  (CN8  pin 20)
 *
 *  PIR Hareket Sensörü (BISS0001):
 *    PIR OUT  →  PB4  (CN10 pin 27)   EXTI4
 *    PIR VCC  →  5V   (CN10 pin 18)   ← PIR 5V ister!
 *    PIR GND  →  GND  (CN10 pin 20)
 *
 *  ESP32 UART:
 *    ESP32 GPIO16 (RX)  ←  PA9  (CN10 pin 21)  USART1_TX
 *    ESP32 GPIO17 (TX)  →  PA10 (CN10 pin 33)  USART1_RX
 *    ESP32 GND          →  GND
 *
 * ─────────────────────────────────────────────────────────────────
 * CubeIDE .ioc → Project Manager → Advanced Settings:
 *   ADC1   → LL
 *   USART1 → LL
 *   GPIO   → LL
 *
 * CubeIDE .ioc → Pinout:
 *   PB4 → GPIO_EXTIx  (rising edge, pull-down)
 *   NVIC → EXTI4 interrupt → Enabled
 * ─────────────────────────────────────────────────────────────────
 *
 * JSON format:
 *   {"sensor_id":"sensor_1","raw":2048,"voltage":1.654,"motion":0}\n
 *   motion = 1 → hareket algılandı (bir sonraki gönderimlde sıfırlanır)
 */

#include "stm32l4xx_ll_adc.h"
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_utils.h"
#include "stm32l4xx_ll_exti.h"
#include "stm32l4xx_ll_system.h"   /* SYSCFG */
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ── Config ─────────────────────────────────────────────────────── */
#define SENSOR_ID        "sensor_1"
#define VREF_MV          3300UL
#define ADC_FULL_SCALE   4095UL
#define SAMPLE_PERIOD_MS 10          /* 100 Hz */
#define USART1_CLK_HZ    80000000UL

/* ── PIR motion flag (set by EXTI IRQ) ──────────────────────────── */
volatile uint8_t motion_detected = 0;

/* ────────────────────────────────────────────────────────────────
 * EXTI4 IRQ HANDLER — PIR hareketi burada yakalanır
 * Bu fonksiyon stm32l4xx_it.c'ye de taşınabilir.
 * ──────────────────────────────────────────────────────────────── */
void EXTI4_IRQHandler(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(EXTI, LL_EXTI_LINE_4))
    {
        LL_EXTI_ClearFlag_0_31(EXTI, LL_EXTI_LINE_4);
        motion_detected = 1;   /* Ana döngü okur ve sıfırlar */
    }
}

/* ── GPIO Init ───────────────────────────────────────────────────── */
static void gpio_init(void)
{
    /* GPIOA — PA0 (ADC), PA9/PA10 (UART) */
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    /* PA0 — titreşim sensörü analog giriş */
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_0, LL_GPIO_MODE_ANALOG);
    LL_GPIO_EnablePinAnalogControl(GPIOA, LL_GPIO_PIN_0);

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

    /* GPIOB — PB4 (PIR EXTI4) */
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_4, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinPull(GPIOB, LL_GPIO_PIN_4, LL_GPIO_PULL_DOWN);
    /* Pull-down: PIR çıkışı HIGH olduğunda yükselen kenar tetikler */
}

/* ── EXTI Init — PIR hareket kesmesi ────────────────────────────── */
static void pir_exti_init(void)
{
    /* SYSCFG clock — EXTI kaynak seçimi için gerekli */
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);

    /* EXTI4 kaynağını GPIOB'ye bağla (PB4) */
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTB, LL_SYSCFG_EXTI_LINE4);

    /* Yükselen kenar tetikleme (PIR: LOW→HIGH = hareket başladı) */
    LL_EXTI_EnableIT_0_31        (EXTI, LL_EXTI_LINE_4);
    LL_EXTI_EnableRisingTrig_0_31(EXTI, LL_EXTI_LINE_4);
    LL_EXTI_DisableFallingTrig_0_31(EXTI, LL_EXTI_LINE_4);

    /* NVIC — EXTI4 kesmesini etkinleştir */
    NVIC_SetPriority(EXTI4_IRQn, 1);
    NVIC_EnableIRQ  (EXTI4_IRQn);
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
    LL_ADC_REG_SetSequencerRanks (ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_5);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_5,
                                  LL_ADC_SAMPLINGTIME_47CYCLES_5);
    LL_ADC_SetChannelSingleDiff  (ADC1, LL_ADC_CHANNEL_5, LL_ADC_SINGLE_ENDED);

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

/* ── Helpers ─────────────────────────────────────────────────────── */
static uint16_t adc_read(void)
{
    LL_ADC_REG_StartConversion(ADC1);
    while (!LL_ADC_IsActiveFlag_EOC(ADC1));
    uint16_t val = (uint16_t)LL_ADC_REG_ReadConversionData12(ADC1);
    LL_ADC_ClearFlag_EOC(ADC1);
    return val;
}

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
    pir_exti_init();
    adc_init();
    usart_init();
}

void vibration_ll_run(void)
{
    char     buf[100];
    uint16_t raw;
    uint32_t voltage_mv;
    uint8_t  motion;

    for (;;)
    {
        /* 1. ADC — titreşim */
        raw        = adc_read();
        voltage_mv = (raw * VREF_MV) / ADC_FULL_SCALE;

        /* 2. PIR — hareket durumunu oku ve bayrağı sıfırla */
        motion = motion_detected;
        motion_detected = 0;   /* bir sonraki periyoda kadar sıfırla */

        /* 3. JSON gönder */
        snprintf(buf, sizeof(buf),
                 "{\"sensor_id\":\"%s\","
                 "\"raw\":%u,"
                 "\"voltage\":%lu.%03lu,"
                 "\"motion\":%u}\n",
                 SENSOR_ID,
                 (unsigned)raw,
                 voltage_mv / 1000UL, voltage_mv % 1000UL,
                 (unsigned)motion);

        usart_send_string(buf);
        LL_mDelay(SAMPLE_PERIOD_MS);
    }
}
