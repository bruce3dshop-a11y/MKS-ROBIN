#pragma once
/* ============================================================
 * Professional PID controller with soft-start, integral clamp,
 * derivative filtering, and thermal-runaway detection.
 * ============================================================ */

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PID_IDLE      = 0,
    PID_HEATING   = 1,
    PID_READY     = 2,
    PID_COOLING   = 3,
    PID_FAULT     = 4,
    PID_AUTOTUNE  = 5,
} PidState;

typedef struct {
    float kp, ki, kd;
} PidCoeffs;

void      pid_init(void);
void      pid_set_target(float target_c);
float     pid_get_target(void);
void      pid_update(float current_temp, uint32_t now_ms);  // call at PID_SAMPLE_MS
float     pid_get_output(void);    // 0.0–1000.0 (permille of full power)
PidState  pid_get_state(void);
PidCoeffs pid_get_coeffs(void);
void      pid_set_coeffs(PidCoeffs c);

// Autotune — runs blocking relay-feedback test; returns true on success
bool      pid_autotune(float target_c,
                       void (*progress_cb)(float temp, int cycle));

// Force shutdown (fault)
void      pid_fault_shutdown(void);
bool      pid_is_faulted(void);
void      pid_clear_fault(void);

// Statistics
uint32_t  pid_get_heating_cycles(void);
float     pid_get_max_temp(void);
float     pid_get_avg_pwm(void);
uint32_t  pid_get_total_heat_time_s(void);

#ifdef __cplusplus
}
#endif
