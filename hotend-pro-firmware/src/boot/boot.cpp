/* ============================================================
 * Boot screen
 * Fade-in logo → hold 2 s → fade-out
 * "Fade" is simulated on the monochrome ST7920 by alternating
 * pixel rows on/off in a curtain pattern (32 levels).
 * ============================================================ */

#include "boot/boot.h"
#include "display/display.h"
#include "graphics/graphics.h"
#include "graphics/bitmaps.h"
#include <string.h>

// Draw the static boot frame into the framebuffer
static void _draw_frame(void) {
    display_clear();

    // Logo bitmap (64×16) centered at top
    gfx_bitmap(32, 4, 64, 16, BMP_LOGO);

    // Title text
    gfx_str5x7_center(26, "HOTEND PRO");

    // Subtitle
    gfx_str5x7_center(36, "Smart Controller");

    // Version
    gfx_str5x7_center(50, "Version 1.0");

    // Decorative top/bottom lines
    gfx_hline(0, 0,  128, true);
    gfx_hline(0, 63, 128, true);
    gfx_hline(0, 1,  128, true);
    gfx_hline(0, 62, 128, true);
}

/* Apply a stipple-pattern fade over the base framebuffer.
 * Uses a Bayer-style ordered dither: at each intensity level (0–64)
 * a different fraction of pixels are cleared, producing a smooth fade
 * on a monochrome display without screen-tearing artefacts.
 *
 * level 0  = all pixels off  (fully black)
 * level 64 = all pixels on   (original image fully visible)
 */
static void _apply_fade(const uint8_t *base, uint8_t level) {
    // Bayer 8×8 dither matrix, values 0-63
    static const uint8_t bayer8[8][8] = {
        { 0,32, 8,40, 2,34,10,42},
        {48,16,56,24,50,18,58,26},
        {12,44, 4,36,14,46, 6,38},
        {60,28,52,20,62,30,54,22},
        { 3,35,11,43, 1,33, 9,41},
        {51,19,59,27,49,17,57,25},
        {15,47, 7,39,13,45, 5,37},
        {63,31,55,23,61,29,53,21},
    };

    for (int row = 0; row < LCD_HEIGHT; row++) {
        int byte_off = row * (LCD_WIDTH / 8);
        for (int bx = 0; bx < LCD_WIDTH / 8; bx++) {
            uint8_t out = 0;
            for (int bit = 0; bit < 8; bit++) {
                int col = bx * 8 + bit;
                bool pixel_on = (base[byte_off + bx] >> (7 - bit)) & 1;
                uint8_t threshold = bayer8[row & 7][col & 7];
                if (pixel_on && level > threshold) {
                    out |= (1 << (7 - bit));
                }
            }
            display_fb[byte_off + bx] = out;
        }
    }
}

void boot_screen_run(void) {
    _draw_frame();

    // Snapshot the fully-rendered frame
    uint8_t base_fb[LCD_BUF_BYTES];
    memcpy(base_fb, display_fb, LCD_BUF_BYTES);

    // ── Fade in (0 → 64 in 16 steps) ────────────────────────
    for (int step = 0; step <= 16; step++) {
        uint8_t level = (uint8_t)(step * 4);   // 0, 4, 8 … 64
        _apply_fade(base_fb, level);
        display_flush();
        HAL_Delay(60);
    }

    // ── Hold 2 seconds ───────────────────────────────────────
    HAL_Delay(2000);

    // ── Fade out (64 → 0 in 16 steps) ───────────────────────
    for (int step = 16; step >= 0; step--) {
        uint8_t level = (uint8_t)(step * 4);
        _apply_fade(base_fb, level);
        display_flush();
        HAL_Delay(60);
    }

    display_clear();
    display_flush();
}
