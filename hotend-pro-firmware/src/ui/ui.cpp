/* ============================================================
 * UI screen manager — Home, Settings, AutoTune, Statistics, Lock
 * ============================================================ */

#include "ui/ui.h"
#include "display/display.h"
#include "graphics/graphics.h"
#include "graphics/bitmaps.h"
#include "encoder/encoder.h"
#include "pid/pid.h"
#include "temperature/temperature.h"
#include "heater/heater.h"
#include "settings/settings.h"
#include "storage/storage.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ── Graph buffer ─────────────────────────────────────────────
static float _graph[GRAPH_SAMPLES];
static int   _graph_idx   = 0;
static int   _graph_count = 0;
static uint32_t _last_graph_ms = 0;

// ── Flame animation ───────────────────────────────────────────
static uint8_t _flame_frame   = 0;
static uint32_t _last_anim_ms  = 0;

// ── UI state ──────────────────────────────────────────────────
static UiScreen _screen        = UI_SCREEN_HOME;
static bool     _locked        = false;
static int      _settings_item = 0;
static bool     _in_edit       = false;
static uint32_t _last_input_ms = 0;

// Autotune progress
static bool     _autotune_running  = false;
static int      _autotune_cycle    = 0;
static float    _autotune_temp     = 0.0f;
static bool     _autotune_done     = false;
static bool     _autotune_ok       = false;

// Runtime counter
static uint32_t _start_ms = 0;

#define SETTINGS_ITEMS 5
static const char *SETTINGS_LABELS[SETTINGS_ITEMS] = {
    "Max Temp", "Beeper", "Factory Rst", "Save", "Back"
};

// ─────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────

static void _beep_short(void) {
    Settings *s = settings_get();
    if (!s->beeper_enabled) return;
    // Toggle beeper GPIO at ~2.7 kHz for BEEP_SHORT_MS
    uint32_t t = HAL_GetTick();
    uint32_t half_period_us = 1000000 / (BEEP_FREQ_HZ * 2);
    while (HAL_GetTick() - t < BEEP_SHORT_MS) {
        BEEPER_PORT->BSRR = BEEPER_PIN;
        // busy-loop for half period (very approximate)
        for (volatile uint32_t d = 0; d < (half_period_us * 14 / 10); d++) __NOP();
        BEEPER_PORT->BSRR = (BEEPER_PIN << 16);
        for (volatile uint32_t d = 0; d < (half_period_us * 14 / 10); d++) __NOP();
    }
    BEEPER_PORT->BSRR = (BEEPER_PIN << 16);
}

static void _beep_double(void) {
    _beep_short();
    HAL_Delay(BEEP_DOUBLE_GAP_MS);
    _beep_short();
}

static void _beep_long(void) {
    Settings *s = settings_get();
    if (!s->beeper_enabled) return;
    uint32_t t = HAL_GetTick();
    uint32_t half_period_us = 1000000 / (BEEP_FREQ_HZ * 2);
    while (HAL_GetTick() - t < BEEP_LONG_MS) {
        BEEPER_PORT->BSRR = BEEPER_PIN;
        for (volatile uint32_t d = 0; d < (half_period_us * 14 / 10); d++) __NOP();
        BEEPER_PORT->BSRR = (BEEPER_PIN << 16);
        for (volatile uint32_t d = 0; d < (half_period_us * 14 / 10); d++) __NOP();
    }
    BEEPER_PORT->BSRR = (BEEPER_PIN << 16);
}

// ─────────────────────────────────────────────────────────────
// HOME screen
// ─────────────────────────────────────────────────────────────

static void _draw_home(uint32_t now_ms) {
    display_clear();

    float temp   = temperature_get();
    float target = pid_get_target();
    float output = pid_get_output();
    PidState state = pid_get_state();

    // ── Top status bar ────────────────────────────────────────
    const char *state_str = "IDLE";
    switch (state) {
        case PID_HEATING: state_str = "HEATING"; break;
        case PID_READY:   state_str = "READY";   break;
        case PID_COOLING: state_str = "COOLING"; break;
        case PID_FAULT:   state_str = "  ERROR"; break;
        default: break;
    }
    // Status bar background
    gfx_fill_rect(0, 0, 128, 9, true);
    // Status text inverted
    int sx = (128 - (int)strlen(state_str) * 6) / 2;
    for (const char *p = state_str; *p; p++) {
        gfx_char5x7(sx, 1, *p);
        sx += 6;
    }
    // Invert to white-on-black
    gfx_invert_rect(0, 0, 128, 9);

    // ── Flame / thermometer icon (16×16) left side ────────────
    if (state == PID_HEATING || state == PID_READY) {
        if (now_ms - _last_anim_ms >= 200) {
            _flame_frame = (_flame_frame + 1) % 3;
            _last_anim_ms = now_ms;
        }
        gfx_bitmap(1, 12, 16, 16, BMP_FLAME[_flame_frame]);
    } else if (state == PID_FAULT) {
        gfx_bitmap(1, 12, 16, 16, BMP_WARN);
    } else {
        gfx_bitmap(1, 12, 16, 16, BMP_THERM);
    }

    // ── Large current temperature ─────────────────────────────
    int temp_int = (int)(temp + 0.5f);
    gfx_number16x24(19, 10, temp_int);
    gfx_degC16x24(19 + (temp_int >= 100 ? 51 : temp_int >= 10 ? 34 : 17), 10);

    // ── SET temperature (small) ───────────────────────────────
    char buf[24];
    snprintf(buf, sizeof(buf), "SET %d\xB0" "C", (int)target);
    gfx_str5x7(1, 30, buf);

    // ── Power bar & percentage ────────────────────────────────
    snprintf(buf, sizeof(buf), "PWR %d%%", (int)(output / 10.0f));
    gfx_str5x7(1, 39, buf);
    gfx_progress_bar(42, 39, 85, 7, (int)output);

    // ── ETA ──────────────────────────────────────────────────
    if (state == PID_HEATING && output > 100.0f) {
        float delta = target - temp;
        float rate  = output / 1000.0f * 15.0f;  // rough estimate °C/s
        if (rate > 0.1f) {
            int eta_s = (int)(delta / rate);
            snprintf(buf, sizeof(buf), "ETA %02d:%02d", eta_s / 60, eta_s % 60);
            gfx_str5x7(80, 30, buf);
        }
    } else if (state == PID_READY) {
        gfx_str5x7(80, 30, "READY");
    } else if (state == PID_FAULT) {
        gfx_str5x7(80, 30, "FAULT!");
    }

    // ── Temperature graph (last 60 s) ─────────────────────────
    gfx_temp_graph(0, 48, 128, 16, _graph, _graph_count,
                   (float)(settings_get()->max_temp_limit));

    // ── Lock indicator ────────────────────────────────────────
    if (_locked) {
        gfx_bitmap(112, 0, 16, 16, BMP_LOCK);
    }
}

static void _handle_home_input(EncoderEvent ev, uint32_t now_ms) {
    float target = pid_get_target();

    switch (ev) {
    case ENC_EVENT_CW:
        if (!_locked) {
            target += ENCODER_TEMP_STEP;
            if (target > settings_get()->max_temp_limit)
                target = settings_get()->max_temp_limit;
            pid_set_target(target);
            _beep_short();
        }
        break;
    case ENC_EVENT_CCW:
        if (!_locked) {
            target -= ENCODER_TEMP_STEP;
            if (target < 0) target = 0;
            pid_set_target(target);
            _beep_short();
        }
        break;
    case ENC_EVENT_CLICK:
        if (!_locked) {
            // Confirm target — save to settings
            settings_get()->last_target_c = (uint16_t)pid_get_target();
            settings_save();
            _beep_double();
        } else {
            _beep_long();  // beep to signal locked
        }
        break;
    case ENC_EVENT_HOLD:
        _locked = !_locked;
        _beep_long();
        if (!_locked) {
            // Navigate to settings on long hold while unlocked? No — per spec: hold = lock/unlock
        }
        break;
    default:
        break;
    }

    // Long hold while unlocked → could also navigate to settings menu
    // Double-click nav: single click on home enters settings menu
    // (We treat double-click as setpoint confirm + settings enter on 2nd click)
    (void)now_ms;
}

// ─────────────────────────────────────────────────────────────
// LOCK screen
// ─────────────────────────────────────────────────────────────

static void _draw_lock(void) {
    display_clear();
    gfx_bitmap(56, 14, 16, 16, BMP_LOCK);
    gfx_str5x7_center(34, "  LOCKED  ");
    gfx_str5x7_center(46, "Hold 3s to unlock");
    gfx_rect(0, 0, 128, 64, true);
}

// ─────────────────────────────────────────────────────────────
// SETTINGS screen
// ─────────────────────────────────────────────────────────────

static void _draw_settings(void) {
    display_clear();

    // Title bar
    gfx_fill_rect(0, 0, 128, 9, true);
    gfx_invert_rect(0, 0, 128, 9);
    gfx_str5x7_center(1, "SETTINGS");
    gfx_invert_rect(0, 0, 128, 9);

    Settings *s = settings_get();
    char buf[32];

    for (int i = 0; i < SETTINGS_ITEMS; i++) {
        int y = 12 + i * 10;
        bool sel = (i == _settings_item);

        if (sel) gfx_fill_rect(0, y - 1, 128, 9, true);

        gfx_str5x7(3, y, SETTINGS_LABELS[i]);

        // Value display
        switch (i) {
        case 0: snprintf(buf, sizeof(buf), "%d\xB0" "C", s->max_temp_limit); break;
        case 1: snprintf(buf, sizeof(buf), "%s", s->beeper_enabled ? "ON" : "OFF"); break;
        default: buf[0] = 0; break;
        }
        if (buf[0]) gfx_str5x7(90, y, buf);

        if (sel) gfx_invert_rect(0, y - 1, 128, 9);
    }
}

static void _handle_settings_input(EncoderEvent ev) {
    Settings *s = settings_get();
    switch (ev) {
    case ENC_EVENT_CW:
        if (!_in_edit) {
            _settings_item = (_settings_item + 1) % SETTINGS_ITEMS;
        } else {
            // Edit current value
            if (_settings_item == 0) {
                if (s->max_temp_limit < 300) s->max_temp_limit++;
            } else if (_settings_item == 1) {
                s->beeper_enabled = !s->beeper_enabled;
                _in_edit = false;
            }
        }
        _beep_short();
        break;
    case ENC_EVENT_CCW:
        if (!_in_edit) {
            _settings_item = (_settings_item + SETTINGS_ITEMS - 1) % SETTINGS_ITEMS;
        } else {
            if (_settings_item == 0) {
                if (s->max_temp_limit > 50) s->max_temp_limit--;
            } else if (_settings_item == 1) {
                s->beeper_enabled = !s->beeper_enabled;
                _in_edit = false;
            }
        }
        _beep_short();
        break;
    case ENC_EVENT_CLICK:
        _beep_short();
        if (!_in_edit) {
            switch (_settings_item) {
            case 0: _in_edit = true; break;
            case 1: _in_edit = true; break;
            case 2:
                settings_factory_reset();
                _beep_long();
                _screen = UI_SCREEN_HOME;
                break;
            case 3:
                settings_save();
                _beep_double();
                _screen = UI_SCREEN_HOME;
                break;
            case 4:
                _screen = UI_SCREEN_HOME;
                break;
            }
        } else {
            _in_edit = false;
        }
        break;
    case ENC_EVENT_HOLD:
        _screen = UI_SCREEN_HOME;
        _in_edit = false;
        _beep_long();
        break;
    default: break;
    }
}

// ─────────────────────────────────────────────────────────────
// AUTOTUNE screen
// ─────────────────────────────────────────────────────────────

static void _autotune_progress_cb(float temp, int cycle) {
    _autotune_temp  = temp;
    _autotune_cycle = cycle;

    // Refresh display during autotune
    display_clear();
    char buf[32];

    gfx_fill_rect(0, 0, 128, 9, true);
    gfx_invert_rect(0, 0, 128, 9);
    gfx_str5x7_center(1, "PID AUTOTUNE");
    gfx_invert_rect(0, 0, 128, 9);

    snprintf(buf, sizeof(buf), "Cycle %d / 8", cycle);
    gfx_str5x7_center(16, buf);

    snprintf(buf, sizeof(buf), "Temp: %.1f\xB0" "C", (double)temp);
    gfx_str5x7_center(28, buf);

    gfx_str5x7_center(40, "Please wait...");

    gfx_progress_bar(4, 52, 120, 8, (int)((float)cycle / 8.0f * 1000.0f));

    display_flush();
}

static void _draw_autotune(void) {
    display_clear();
    char buf[32];

    gfx_fill_rect(0, 0, 128, 9, true);
    gfx_invert_rect(0, 0, 128, 9);
    gfx_str5x7_center(1, "PID AUTOTUNE");
    gfx_invert_rect(0, 0, 128, 9);

    if (!_autotune_running && !_autotune_done) {
        gfx_str5x7_center(20, "Press to start");
        snprintf(buf, sizeof(buf), "Target: %d\xB0" "C",
                 (int)pid_get_target());
        gfx_str5x7_center(32, buf);
        gfx_str5x7_center(44, "Hold to cancel");
    } else if (_autotune_done) {
        if (_autotune_ok) {
            PidCoeffs c = pid_get_coeffs();
            snprintf(buf, sizeof(buf), "Kp=%.1f", (double)c.kp);
            gfx_str5x7_center(16, buf);
            snprintf(buf, sizeof(buf), "Ki=%.3f", (double)c.ki);
            gfx_str5x7_center(26, buf);
            snprintf(buf, sizeof(buf), "Kd=%.1f", (double)c.kd);
            gfx_str5x7_center(36, buf);
            gfx_str5x7_center(50, "Saved! Click=Back");
        } else {
            gfx_bitmap(56, 20, 16, 16, BMP_WARN);
            gfx_str5x7_center(40, "AUTOTUNE FAILED");
            gfx_str5x7_center(50, "Click to go back");
        }
    }
}

static void _handle_autotune_input(EncoderEvent ev) {
    switch (ev) {
    case ENC_EVENT_CLICK:
        _beep_short();
        if (!_autotune_running && !_autotune_done) {
            // Start autotune
            _autotune_running = true;
            float t = pid_get_target();
            if (t < 150.0f) t = 200.0f;
            _autotune_ok = pid_autotune(t, _autotune_progress_cb);
            if (_autotune_ok) {
                // Save coefficients
                settings_get()->pid = pid_get_coeffs();
                settings_save();
                _beep_double();
            } else {
                _beep_long();
            }
            _autotune_running = false;
            _autotune_done    = true;
        } else if (_autotune_done) {
            _autotune_done    = false;
            _screen           = UI_SCREEN_HOME;
        }
        break;
    case ENC_EVENT_HOLD:
        _autotune_running = false;
        _autotune_done    = false;
        _screen = UI_SCREEN_HOME;
        _beep_long();
        break;
    default: break;
    }
}

// ─────────────────────────────────────────────────────────────
// STATISTICS screen
// ─────────────────────────────────────────────────────────────

static void _draw_statistics(uint32_t now_ms) {
    display_clear();

    gfx_fill_rect(0, 0, 128, 9, true);
    gfx_invert_rect(0, 0, 128, 9);
    gfx_str5x7_center(1, "STATISTICS");
    gfx_invert_rect(0, 0, 128, 9);

    uint32_t runtime_s = (now_ms - _start_ms) / 1000 + settings_get()->runtime_s;
    char buf[32];

    snprintf(buf, sizeof(buf), "Runtime %02lu:%02lu:%02lu",
             runtime_s / 3600, (runtime_s % 3600) / 60, runtime_s % 60);
    gfx_str5x7(2, 12, buf);

    snprintf(buf, sizeof(buf), "Cycles  %lu", pid_get_heating_cycles());
    gfx_str5x7(2, 22, buf);

    snprintf(buf, sizeof(buf), "MaxTemp %.0f\xB0" "C", (double)pid_get_max_temp());
    gfx_str5x7(2, 32, buf);

    snprintf(buf, sizeof(buf), "AvgPWR  %.0f%%", (double)pid_get_avg_pwm());
    gfx_str5x7(2, 42, buf);

    uint32_t ht = pid_get_total_heat_time_s() + settings_get()->total_heat_time_s;
    snprintf(buf, sizeof(buf), "HeatTime %02lu:%02lu", ht / 3600, (ht % 3600) / 60);
    gfx_str5x7(2, 52, buf);
}

static void _handle_statistics_input(EncoderEvent ev) {
    if (ev == ENC_EVENT_CLICK || ev == ENC_EVENT_HOLD) {
        _screen = UI_SCREEN_HOME;
        _beep_short();
    }
}

// ─────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────

void ui_init(void) {
    memset(_graph, 0, sizeof(_graph));
    _start_ms = HAL_GetTick();

    // Restore last target
    Settings *s = settings_get();
    pid_set_target((float)s->last_target_c);
    pid_set_coeffs(s->pid);
}

void ui_tick(uint32_t now_ms) {
    // ── Update graph every second ────────────────────────────
    if (now_ms - _last_graph_ms >= 1000) {
        _last_graph_ms = now_ms;
        float t = temperature_get();
        _graph[_graph_idx] = t;
        _graph_idx = (_graph_idx + 1) % GRAPH_SAMPLES;
        if (_graph_count < GRAPH_SAMPLES) _graph_count++;
    }

    // ── Process encoder events ───────────────────────────────
    EncoderEvent ev = encoder_pop_event();

    // Global: hold while on home screen → lock/unlock
    // (handled inside _handle_home_input)

    if (_locked && _screen == UI_SCREEN_HOME) {
        if (ev == ENC_EVENT_HOLD) {
            _locked = false;
            _beep_long();
        }
        // Don't process other events while locked
    } else if (!_locked) {
        switch (_screen) {
        case UI_SCREEN_HOME:
            // Long hold enters settings
            if (ev == ENC_EVENT_HOLD) {
                // If already at idle (target=0): go to settings
                // Otherwise: lock
                if (pid_get_target() < 1.0f) {
                    _screen = UI_SCREEN_SETTINGS;
                    _beep_long();
                } else {
                    _handle_home_input(ev, now_ms);
                }
            } else {
                _handle_home_input(ev, now_ms);
            }
            break;
        case UI_SCREEN_SETTINGS:
            _handle_settings_input(ev);
            break;
        case UI_SCREEN_AUTOTUNE:
            _handle_autotune_input(ev);
            break;
        case UI_SCREEN_STATISTICS:
            _handle_statistics_input(ev);
            break;
        case UI_SCREEN_LOCK:
            if (ev == ENC_EVENT_HOLD) {
                _locked = false;
                _screen = UI_SCREEN_HOME;
                _beep_long();
            }
            break;
        }
    }

    // ── Draw active screen ───────────────────────────────────
    if (_locked) {
        _draw_lock();
    } else {
        switch (_screen) {
        case UI_SCREEN_HOME:        _draw_home(now_ms);       break;
        case UI_SCREEN_SETTINGS:    _draw_settings();         break;
        case UI_SCREEN_AUTOTUNE:    _draw_autotune();         break;
        case UI_SCREEN_STATISTICS:  _draw_statistics(now_ms); break;
        case UI_SCREEN_LOCK:        _draw_lock();             break;
        }
    }

    display_flush();
}

UiScreen ui_get_screen(void) { return _screen; }
