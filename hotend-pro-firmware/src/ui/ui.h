#pragma once
/* ============================================================
 * UI screen manager — handles all screens and input routing
 * ============================================================ */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCREEN_HOME      = 0,
    UI_SCREEN_SETTINGS  = 1,
    UI_SCREEN_AUTOTUNE  = 2,
    UI_SCREEN_STATISTICS = 3,
    UI_SCREEN_LOCK      = 4,
} UiScreen;

void ui_init(void);
void ui_tick(uint32_t now_ms);    // call every UI_REFRESH_MS
UiScreen ui_get_screen(void);

#ifdef __cplusplus
}
#endif
