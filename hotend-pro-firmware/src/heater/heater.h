#pragma once
/* ============================================================
 * Heater PWM output — TIM9_CH1 on PE5
 * ============================================================ */

#include <stdint.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

void    heater_init(void);
void    heater_set_output(float permille);   // 0.0–1000.0
void    heater_off(void);
float   heater_get_output(void);
uint8_t heater_get_percent(void);            // 0–100

#ifdef __cplusplus
}
#endif
