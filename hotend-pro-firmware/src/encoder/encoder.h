#pragma once
/* ============================================================
 * Rotary encoder with push button
 * ============================================================ */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ENC_EVENT_NONE      = 0,
    ENC_EVENT_CW        = 1,   // clockwise step
    ENC_EVENT_CCW       = 2,   // counter-clockwise step
    ENC_EVENT_CLICK     = 3,   // short press + release
    ENC_EVENT_HOLD      = 4,   // held ≥ 3 s
} EncoderEvent;

void         encoder_init(void);
void         encoder_update(uint32_t now_ms);   // call every 1–5 ms
EncoderEvent encoder_pop_event(void);           // returns and clears next event
bool         encoder_any_event(void);

#ifdef __cplusplus
}
#endif
