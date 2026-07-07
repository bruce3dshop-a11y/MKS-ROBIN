/* ============================================================
 * Professional PID controller
 * - Incremental (velocity) form with anti-windup
 * - Derivative on measurement (not error) to avoid derivative kick
 * - Soft start: ramp output cap during cold start
 * - Thermal runaway detection
 * - Ziegler-Nichols relay-feedback autotune
 * ============================================================ */

#include "pid/pid.h"
#include "temperature/temperature.h"
#include "heater/heater.h"
#include <math.h>
#include <string.h>

// Provided by main.cpp — kicks the IWDG during the blocking autotune loop
extern "C" void autotune_watchdog_kick(void);

// ── State ─────────────────────────────────────────────────────
static PidCoeffs _coeffs = {
    PID_KP_DEFAULT, PID_KI_DEFAULT, PID_KD_DEFAULT
};
static float     _target   = 0.0f;
static float     _output   = 0.0f;     // 0–1000
static PidState  _state    = PID_IDLE;
static bool      _faulted  = false;

// Derivative filter state
static float _prev_temp    = 0.0f;
static float _integral     = 0.0f;

// Soft start
static float _softstart_cap = 0.0f;
#define SOFTSTART_RAMP_RATE  50.0f   // permille per PID tick

// Thermal runaway
static uint32_t _heat_start_ms  = 0;
static float    _heat_start_temp = 0.0f;
static bool     _heating_active = false;
static bool     _was_ready      = false;
static float    _ready_temp     = 0.0f;

// Statistics
static uint32_t _heating_cycles   = 0;
static float    _max_temp         = 0.0f;
static double   _sum_pwm          = 0.0;
static uint32_t _pwm_samples      = 0;
static uint32_t _heat_time_start  = 0;
static uint32_t _total_heat_s     = 0;
static bool     _heat_timer_run   = false;

// ─────────────────────────────────────────────────────────────
static void _check_thermal_runaway(float temp, uint32_t now_ms) {
    if (!_heating_active) return;

    // Runaway check 1: not rising while heating
    if ((now_ms - _heat_start_ms) > THERMAL_RUNAWAY_PERIOD_MS) {
        if ((temp - _heat_start_temp) < THERMAL_RUNAWAY_MIN_DELTA) {
            pid_fault_shutdown();
            return;
        }
        _heat_start_ms   = now_ms;
        _heat_start_temp = temp;
    }

    // Runaway check 2: way above target while heating is off
    if (_output < 50.0f && (temp - _target) > THERMAL_PROTECTION_HYSTERESIS) {
        pid_fault_shutdown();
    }
}

// ─────────────────────────────────────────────────────────────
void pid_init(void) {
    _target   = 0.0f;
    _output   = 0.0f;
    _integral = 0.0f;
    _state    = PID_IDLE;
    _faulted  = false;
    _softstart_cap = 1000.0f;
}

void pid_set_target(float target_c) {
    if (_faulted) return;
    float prev_target = _target;
    _target = target_c;
    if (target_c < 1.0f) {
        _state  = PID_IDLE;
        _output = 0.0f;
        _heating_active = false;
        if (_heat_timer_run) {
            // stop heat timer
            _heat_timer_run = false;
        }
    } else if (target_c > prev_target + 5.0f) {
        // New higher target — re-apply soft start
        _softstart_cap = 0.0f;
        if (!_heating_active) {
            _heating_cycles++;
            _heating_active = true;
        }
    }
}

float pid_get_target(void) { return _target; }

void pid_update(float temp, uint32_t now_ms) {
    if (_faulted || _state == PID_IDLE) { _output = 0.0f; return; }

    // Check thermistor fault
    ThermStatus ts = temperature_get_status();
    if (ts == THERM_OPEN || ts == THERM_SHORT) {
        pid_fault_shutdown();
        return;
    }
    if (ts == THERM_OVERTEMP) {
        // Hard over-temperature: cut power immediately
        _output = 0.0f;
        return;
    }

    // Thermal runaway
    _check_thermal_runaway(temp, now_ms);
    if (_faulted) return;

    // Statistics
    if (temp > _max_temp) _max_temp = temp;

    // Determine state
    float error = _target - temp;
    if (fabsf(error) < 2.0f)      _state = PID_READY;
    else if (error >  0.0f)       _state = PID_HEATING;
    else                          _state = PID_COOLING;

    // Track heating cycles and heat time
    if (_state == PID_HEATING) {
        if (!_heat_timer_run) {
            _heat_timer_run  = true;
            _heat_time_start = now_ms;
        }
    } else {
        if (_heat_timer_run) {
            _total_heat_s   += (now_ms - _heat_time_start) / 1000;
            _heat_timer_run  = false;
        }
    }

    // PID computation (position form)
    float dt = PID_SAMPLE_MS / 1000.0f;

    // Proportional
    float p_term = _coeffs.kp * error;

    // Integral with anti-windup clamp
    _integral += error * dt;
    if (_integral >  PID_I_CLAMP / _coeffs.ki) _integral =  PID_I_CLAMP / _coeffs.ki;
    if (_integral < -PID_I_CLAMP / _coeffs.ki) _integral = -PID_I_CLAMP / _coeffs.ki;
    float i_term = _coeffs.ki * _integral;

    // Derivative on measurement (not error) — avoids derivative kick on setpoint change
    float d_term = -_coeffs.kd * (temp - _prev_temp) / dt;
    _prev_temp   = temp;

    float raw_out = p_term + i_term + d_term;

    // Soft start — ramp up cap
    _softstart_cap += SOFTSTART_RAMP_RATE;
    if (_softstart_cap > PID_OUTPUT_MAX) _softstart_cap = PID_OUTPUT_MAX;

    // Clamp output
    if (raw_out > _softstart_cap) raw_out = _softstart_cap;
    if (raw_out < PID_OUTPUT_MIN) raw_out = PID_OUTPUT_MIN;

    _output = raw_out;

    // Statistics accumulation
    _sum_pwm += _output;
    _pwm_samples++;
}

float    pid_get_output(void)  { return _faulted ? 0.0f : _output; }
PidState pid_get_state(void)   { return _faulted ? PID_FAULT : _state; }
PidCoeffs pid_get_coeffs(void) { return _coeffs; }
void pid_set_coeffs(PidCoeffs c) { _coeffs = c; }

void pid_fault_shutdown(void) {
    _faulted  = true;
    _output   = 0.0f;
    _state    = PID_FAULT;
    _heating_active = false;
}
bool pid_is_faulted(void) { return _faulted; }
void pid_clear_fault(void) {
    _faulted  = false;
    _state    = PID_IDLE;
    _output   = 0.0f;
    _integral = 0.0f;
}

uint32_t pid_get_heating_cycles(void)  { return _heating_cycles; }
float    pid_get_max_temp(void)        { return _max_temp; }
float    pid_get_avg_pwm(void) {
    if (_pwm_samples == 0) return 0.0f;
    return (float)(_sum_pwm / _pwm_samples) / 10.0f;  // → percent
}
uint32_t pid_get_total_heat_time_s(void) { return _total_heat_s; }

// ── Autotune — relay feedback (Åström-Hägglund) ──────────────
bool pid_autotune(float target_c,
                  void (*progress_cb)(float temp, int cycle))
{
    const float relay_d    = 300.0f;   // relay output amplitude (permille)
    const int   max_cycles = 8;

    float max_t = -1e6f, min_t = 1e6f;
    uint32_t period_ms = 0;
    uint32_t t_last_cross = 0;
    int      cross_count  = 0;
    bool     above        = false;
    bool     valid        = false;
    float    relay_out    = relay_d;

    // Blocking autotune — we must directly drive the heater here since the
    // main loop is suspended for the duration.
    // Include heater header at top of file; heater_set_output is called directly.
    uint32_t start = HAL_GetTick();
    uint32_t timeout = 300000UL;   // 5 min max

    while ((HAL_GetTick() - start) < timeout) {
        // Update temperature reading
        temperature_update();
        float temp = temperature_get();

        // Detect thermistor fault — immediately cut heater and abort
        if (temperature_get_status() != THERM_OK) {
            heater_set_output(0.0f);
            _faulted = true;
            _output  = 0.0f;
            return false;
        }

        // Overtemp safety
        if (temp > (target_c + 20.0f)) {
            relay_out = 0.0f;
        }

        // Apply relay output directly to heater hardware AND track in _output
        _output = relay_out;
        heater_set_output(relay_out);

        // Track extremes
        if (temp > max_t) max_t = temp;
        if (temp < min_t) min_t = temp;

        // Detect zero crossings around target
        bool now_above = (temp > target_c);
        if (now_above != above) {
            above = now_above;
            relay_out = above ? 0.0f : relay_d;
            if (t_last_cross > 0) {
                uint32_t half_period = HAL_GetTick() - t_last_cross;
                period_ms = half_period * 2;
                cross_count++;
            }
            t_last_cross = HAL_GetTick();

            if (progress_cb) progress_cb(temp, cross_count);

            if (cross_count >= max_cycles) { valid = true; break; }
        }

        // Kick watchdog so the autotune session doesn't cause a reset.
        autotune_watchdog_kick();

        HAL_Delay(100);
    }

    // Ensure heater is off when we exit
    heater_set_output(0.0f);
    _output = 0.0f;
    if (!valid || period_ms < 100) return false;

    // Ziegler–Nichols from relay method:
    // Ku (ultimate gain) = 4d / (π × amplitude)
    float amplitude = (max_t - min_t) / 2.0f;
    if (amplitude < 1.0f) return false;

    float ku = (4.0f * relay_d) / (3.14159f * amplitude);
    float pu = period_ms / 1000.0f;  // seconds

    // ZN PID rules
    PidCoeffs c;
    c.kp = 0.6f   * ku;
    c.ki = 2.0f   * c.kp / pu;
    c.kd = 0.125f * c.kp * pu;

    _coeffs = c;
    return true;
}
