#pragma once
/* ============================================================
 * Temperature sensing — ADC + Steinhart-Hart thermistor model
 * ============================================================ */

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    THERM_OK      = 0,
    THERM_OPEN    = 1,   // thermistor disconnected (ADC too low)
    THERM_SHORT   = 2,   // thermistor shorted (ADC too high)
    THERM_OVERTEMP = 3,  // above TEMP_MAX_C
} ThermStatus;

void         temperature_init(void);
void         temperature_update(void);         // call periodically (10 Hz)
float        temperature_get(void);            // °C, filtered
uint16_t     temperature_get_adc_raw(void);
ThermStatus  temperature_get_status(void);

#ifdef __cplusplus
}
#endif
