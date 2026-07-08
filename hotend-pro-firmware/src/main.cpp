/* ============================================================
 * HOTEND PRO — Main Entry Point
 * STM32F407VET6 / MKS Robin Nano V3.1
 *
 * No Arduino framework — pure STM32 HAL + custom drivers.
 * ============================================================ */

#include "stm32f4xx_hal.h"
#include "config.h"

#include "display/display.h"
#include "encoder/encoder.h"
#include "temperature/temperature.h"
#include "heater/heater.h"
#include "pid/pid.h"
#include "settings/settings.h"
#include "boot/boot.h"
#include "ui/ui.h"

// ── Forward declarations ──────────────────────────────────────
static void SystemClock_Config(void);
static void GPIO_Init(void);
static void IWDG_Init(void);
static void Beeper_Init(void);
static void Error_Handler(void);

// ── Watchdog handle ───────────────────────────────────────────
static IWDG_HandleTypeDef hiwdg;

// ─────────────────────────────────────────────────────────────
int main(void) {
    // HAL init (SysTick, NVIC grouping)
    HAL_Init();

    // Configure system clock to 168 MHz
    SystemClock_Config();

    // GPIO common init (beeper, etc.)
    GPIO_Init();
    Beeper_Init();

    // Peripheral init
    display_init();
    encoder_init();
    temperature_init();
    heater_init();
    pid_init();

    // Load settings from flash
    settings_init();

    // ── Boot screen (before watchdog so the animation doesn't trip it) ──
    boot_screen_run();

    // Watchdog (reset if main loop stalls > 2 s) — started AFTER boot
    IWDG_Init();

    // ── UI init (restores last target, applies saved PID) ─────
    ui_init();

    // ── Main loop ─────────────────────────────────────────────
    uint32_t last_pid_ms  = 0;
    uint32_t last_ui_ms   = 0;
    uint32_t last_enc_ms  = 0;
    uint32_t last_therm_ms = 0;
    uint32_t last_save_ms = 0;

    // Check if we recovered from fault — clear on power cycle
    if (pid_is_faulted()) {
        pid_clear_fault();
    }

    while (1) {
        uint32_t now = HAL_GetTick();

        // ── Encoder polling (every 2 ms for responsiveness) ───
        if (now - last_enc_ms >= 2) {
            last_enc_ms = now;
            encoder_update(now);
        }

        // ── Temperature ADC (every 100 ms) ────────────────────
        if (now - last_therm_ms >= 100) {
            last_therm_ms = now;
            temperature_update();

            // Immediate fault check
            ThermStatus ts = temperature_get_status();
            if (ts == THERM_OPEN || ts == THERM_SHORT) {
                heater_off();
                pid_fault_shutdown();
            }
        }

        // ── PID update (every PID_SAMPLE_MS = 100 ms) ────────
        if (now - last_pid_ms >= PID_SAMPLE_MS) {
            last_pid_ms = now;
            float temp = temperature_get();
            pid_update(temp, now);

            // Apply PID output to heater (or 0 if faulted)
            float out = pid_get_output();
            heater_set_output(out);
        }

        // ── UI tick (every 50 ms = 20 FPS) ───────────────────
        if (now - last_ui_ms >= UI_REFRESH_MS) {
            last_ui_ms = now;
            ui_tick(now);
        }

        // ── Periodic settings autosave (every 60 s) ──────────
        if (now - last_save_ms >= 60000UL) {
            last_save_ms = now;
            Settings *s = settings_get();
            s->heating_cycles     = pid_get_heating_cycles();
            s->recorded_max_temp  = pid_get_max_temp();
            s->avg_pwm            = pid_get_avg_pwm();
            s->total_heat_time_s  = pid_get_total_heat_time_s();
            s->runtime_s         += 60;
            s->last_target_c      = (uint16_t)pid_get_target();
            settings_save();
        }

        // ── Kick watchdog ─────────────────────────────────────
        HAL_IWDG_Refresh(&hiwdg);
    }
}

// ─────────────────────────────────────────────────────────────
// System clock: HSE 8 MHz → PLL → 168 MHz (AHB=168, APB1=42, APB2=84)
// ─────────────────────────────────────────────────────────────
static void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 8;    // f_VCO_in = 8/8 = 1 MHz
    osc.PLL.PLLN       = 336;  // f_VCO    = 336 MHz
    osc.PLL.PLLP       = RCC_PLLP_DIV2;  // SYSCLK = 168 MHz
    osc.PLL.PLLQ       = 7;    // USB = 48 MHz (not used but must be ≤ 48)
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    RCC_ClkInitTypeDef clk = {};
    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   // AHB  = 168 MHz
    clk.APB1CLKDivider = RCC_HCLK_DIV4;     // APB1 = 42 MHz (max 42)
    clk.APB2CLKDivider = RCC_HCLK_DIV2;     // APB2 = 84 MHz (max 84)
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) Error_Handler();

    // Enable peripheral clocks used throughout
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
}

// ─────────────────────────────────────────────────────────────
// GPIO init — any pins not owned by a driver module
// ─────────────────────────────────────────────────────────────
static void GPIO_Init(void) {
    // Nothing additional — all pins initialised in their driver modules
}

// ─────────────────────────────────────────────────────────────
// Beeper pin
// ─────────────────────────────────────────────────────────────
static void Beeper_Init(void) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef g = {};
    g.Pin   = BEEPER_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BEEPER_PORT, &g);
    HAL_GPIO_WritePin(BEEPER_PORT, BEEPER_PIN, GPIO_PIN_RESET);

    /* ── Startup confirmation: 3 × 200 ms beeps ─────────────────
     * If you hear this triple-beep pattern, our firmware is running.
     * This is DISTINCT from the MKS bootloader single-beep.
     * Remove once LCD is confirmed working. */
    for (int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(BEEPER_PORT, BEEPER_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(BEEPER_PORT, BEEPER_PIN, GPIO_PIN_RESET);
        HAL_Delay(150);
    }
}

// ─────────────────────────────────────────────────────────────
// Independent watchdog: LSI ~32 kHz, prescaler /32 → ~1 kHz
// Reload = 2000 → ~2 s timeout
// ─────────────────────────────────────────────────────────────
static void IWDG_Init(void) {
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
    hiwdg.Init.Reload    = 2000;
    HAL_IWDG_Init(&hiwdg);
}

// ─────────────────────────────────────────────────────────────
// HAL callbacks
// ─────────────────────────────────────────────────────────────

// Required by HAL — called when HAL_Init sets up SysTick
extern "C" void SysTick_Handler(void) {
    HAL_IncTick();
}

// ── Watchdog kick called from pid_autotune during its blocking loop ──
// Defined here (in main.cpp) so it has access to hiwdg without a
// circular dependency between pid and heater/watchdog modules.
extern "C" void autotune_watchdog_kick(void) {
    HAL_IWDG_Refresh(&hiwdg);
}

// Error handler — safety: shut off heater, flash display
static void Error_Handler(void) {
    // Ensure heater is off
    HEATER_PORT->BSRR = (HEATER_PIN << 16);  // direct GPIO low
    while (1) {
        // Toggle beeper in error pattern
        BEEPER_PORT->BSRR = BEEPER_PIN;
        HAL_Delay(100);
        BEEPER_PORT->BSRR = (BEEPER_PIN << 16);
        HAL_Delay(100);
    }
}

// Required by HAL — called for hard faults; ensure heater off
extern "C" void HardFault_Handler(void) {
    HEATER_PORT->BSRR = (HEATER_PIN << 16);
    while (1) {}
}

extern "C" void NMI_Handler(void)             {}
extern "C" void MemManage_Handler(void)        { HardFault_Handler(); }
extern "C" void BusFault_Handler(void)         { HardFault_Handler(); }
extern "C" void UsageFault_Handler(void)       { HardFault_Handler(); }
extern "C" void DebugMon_Handler(void)         {}
extern "C" void PendSV_Handler(void)           {}
