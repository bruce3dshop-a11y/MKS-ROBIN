#pragma once
/* ============================================================
 * User-configurable settings (persisted to flash via storage)
 * ============================================================ */

#include <stdint.h>
#include <stdbool.h>
#include "pid/pid.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t  magic;              // FLASH_STORAGE_MAGIC
    uint16_t  last_target_c;      // Last set temperature
    uint16_t  max_temp_limit;     // User-defined maximum (default 300)
    bool      beeper_enabled;
    uint8_t   brightness;         // 0-255 (reserved for future HW)
    PidCoeffs pid;
    uint32_t  heating_cycles;
    float     recorded_max_temp;
    float     avg_pwm;
    uint32_t  total_heat_time_s;
    uint32_t  runtime_s;
    uint8_t   _pad[8];            // alignment padding
    uint32_t  crc;
} Settings;

void      settings_init(void);        // load from flash (or defaults)
Settings *settings_get(void);
void      settings_save(void);        // write to flash
void      settings_factory_reset(void);

#ifdef __cplusplus
}
#endif
