/* ============================================================
 * Settings — load from / save to flash via storage module
 * ============================================================ */

#include "settings/settings.h"
#include "storage/storage.h"
#include <string.h>

static Settings _s;

static void _apply_defaults(void) {
    memset(&_s, 0, sizeof(_s));
    _s.magic           = FLASH_STORAGE_MAGIC;
    _s.last_target_c   = TEMP_DEFAULT_C;
    _s.max_temp_limit  = TEMP_MAX_C;
    _s.beeper_enabled  = true;
    _s.brightness      = 255;
    _s.pid.kp          = PID_KP_DEFAULT;
    _s.pid.ki          = PID_KI_DEFAULT;
    _s.pid.kd          = PID_KD_DEFAULT;
    _s.heating_cycles  = 0;
    _s.recorded_max_temp = 0.0f;
    _s.avg_pwm         = 0.0f;
    _s.total_heat_time_s = 0;
    _s.runtime_s       = 0;
}

void settings_init(void) {
    storage_init();
    if (!storage_load(&_s, sizeof(_s))) {
        _apply_defaults();
    }
}

Settings *settings_get(void) { return &_s; }

void settings_save(void) {
    storage_save(&_s, sizeof(_s));
}

void settings_factory_reset(void) {
    _apply_defaults();
    settings_save();
}
