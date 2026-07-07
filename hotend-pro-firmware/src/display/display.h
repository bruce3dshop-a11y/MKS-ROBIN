#pragma once
/* ============================================================
 * ST7920 128×64 Graphic LCD Driver (Software SPI)
 * Double-buffered framebuffer — no flicker.
 * ============================================================ */

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Framebuffer: 128×64 pixels = 1024 bytes.
 * Layout: row-major, 8 pixels per byte, MSB = leftmost pixel. */
extern uint8_t display_fb[LCD_BUF_BYTES];   // back-buffer (draw here)

void display_init(void);

/* Push the back-buffer to LCD GDRAM.  Call at end of each frame. */
void display_flush(void);

/* Clear the back-buffer (does NOT flush). */
void display_clear(void);

/* Brightness fade helpers (0–255) — adjusted via contrast command. */
void display_set_contrast(uint8_t level);

#ifdef __cplusplus
}
#endif
