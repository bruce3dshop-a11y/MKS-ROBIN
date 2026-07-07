#pragma once
/* ============================================================
 * HOTEND PRO — Hardware Configuration
 * MKS Robin Nano V3.1 / STM32F407VET6
 *
 * Verify pin assignments against your actual hardware schematic.
 * All assignments documented with connector reference.
 * ============================================================ */

#include "stm32f4xx_hal.h"

// ── System ──────────────────────────────────────────────────
#define CPU_FREQ_HZ         168000000UL
#define HSE_FREQ_HZ         8000000UL

// ── ST7920 Display (Software SPI — EXP2 connector) ──────────
// EXP2: CS=PB6, SCK=PA5, MOSI=PA7
// MKS Robin Nano V3.1 + RepRap Full Graphic Smart Controller (ST7920 SPI mode)
// EXP1/EXP2 connector pins (verified against Marlin pins_MKS_ROBIN_NANO_V3.h):
//   CS   = LCD_PINS_RS   = PA8
//   MOSI = LCD_PINS_ENABLE = PD3  (ST7920 SID)
//   SCK  = LCD_PINS_D4   = PD1   (ST7920 E/CLK)
#define LCD_CS_PORT         GPIOA
#define LCD_CS_PIN          GPIO_PIN_8

#define LCD_SCK_PORT        GPIOD
#define LCD_SCK_PIN         GPIO_PIN_1

#define LCD_MOSI_PORT       GPIOD
#define LCD_MOSI_PIN        GPIO_PIN_3

#define LCD_WIDTH           128
#define LCD_HEIGHT          64
#define LCD_BUF_BYTES       (LCD_WIDTH * LCD_HEIGHT / 8)   // 1024

// ── Rotary Encoder (EXP2 + EXP1) ────────────────────────────
// ENC_A = BTN_EN1 = PC6, ENC_B = BTN_EN2 = PC7, BTN = BTN_ENC = PB1
#define ENC_A_PORT          GPIOC
#define ENC_A_PIN           GPIO_PIN_6

#define ENC_B_PORT          GPIOC
#define ENC_B_PIN           GPIO_PIN_7

#define ENC_BTN_PORT        GPIOB
#define ENC_BTN_PIN         GPIO_PIN_1

// ── Beeper (EXP1 pin 1 = PC5) ───────────────────────────────
#define BEEPER_PORT         GPIOC
#define BEEPER_PIN          GPIO_PIN_5

// ── Heater HE0 (PE5 → TIM9_CH1 AF3) ────────────────────────
#define HEATER_PORT         GPIOE
#define HEATER_PIN          GPIO_PIN_5
#define HEATER_TIM          TIM9
#define HEATER_TIM_CH       TIM_CHANNEL_1
#define HEATER_TIM_AF       GPIO_AF3_TIM9
#define HEATER_PWM_FREQ_HZ  1000          // 1 kHz PWM
#define HEATER_PWM_PERIOD   999           // ARR = 999 → 1 kHz at 1 MHz TIM clock

// ── Thermistor TH0 (PC1 → ADC1_IN11) ───────────────────────
#define THERM_PORT          GPIOC
#define THERM_PIN           GPIO_PIN_1
#define THERM_ADC           ADC1
#define THERM_ADC_CHANNEL   ADC_CHANNEL_11

// ── Temperature limits ───────────────────────────────────────
#define TEMP_MAX_C          300
#define TEMP_MIN_C          0
#define TEMP_DEFAULT_C      200
#define TEMP_AMBIENT_C      25

// Thermistor: 100K NTC (Marlin Type 1 / EPCOS B57560G104F)
#define THERMISTOR_NOMINAL  100000        // Resistance at 25°C
#define THERMISTOR_B_COEFF  4092          // Beta coefficient (K)
#define THERMISTOR_SERIES_R 4700          // Series resistor (Ω)
#define ADC_MAX             4095          // 12-bit ADC
#define ADC_REF_V           3.3f

// ── Safety ───────────────────────────────────────────────────
#define THERMAL_RUNAWAY_PERIOD_MS    30000  // 30 s window
#define THERMAL_RUNAWAY_MIN_DELTA    2.0f   // Min °C rise during heating
#define THERMAL_PROTECTION_HYSTERESIS 5.0f  // °C above target triggers fault
#define OPEN_THERM_ADC_THRESHOLD     100    // ADC < 100 → open thermistor
#define SHORT_THERM_ADC_THRESHOLD    4090   // ADC > 4090 → shorted thermistor
#define WATCHDOG_TIMEOUT_MS          2000

// ── PID defaults ─────────────────────────────────────────────
#define PID_KP_DEFAULT      22.2f
#define PID_KI_DEFAULT      1.08f
#define PID_KD_DEFAULT      114.0f
#define PID_OUTPUT_MAX      1000.0f
#define PID_OUTPUT_MIN      0.0f
#define PID_I_CLAMP         200.0f
#define PID_SAMPLE_MS       100           // 10 Hz PID loop

// ── Flash storage page (sector 11, last 128 KB sector) ──────
// STM32F407VET6 flash: sectors 0-11
// Sector 11: 0x080E0000 → 128 KB — used for non-volatile storage
#define FLASH_STORAGE_SECTOR    FLASH_SECTOR_11
#define FLASH_STORAGE_ADDR      0x080E0000UL
#define FLASH_STORAGE_MAGIC     0xDEADBEEFUL

// ── UI ───────────────────────────────────────────────────────
#define UI_REFRESH_MS       50            // 20 FPS
#define GRAPH_SAMPLES       60            // 60-second history
#define LOCK_HOLD_MS        3000          // 3 s to lock/unlock
#define ENCODER_TEMP_STEP   1             // °C per click
#define ENCODER_FAST_STEP   5             // °C per click when spinning fast

// ── Sounds ───────────────────────────────────────────────────
#define BEEP_FREQ_HZ        2700
#define BEEP_SHORT_MS       50
#define BEEP_DOUBLE_GAP_MS  80
#define BEEP_LONG_MS        500
