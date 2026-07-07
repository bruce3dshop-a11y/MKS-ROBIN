#pragma once
/* ============================================================
 * Graphics primitives — draw into display_fb back-buffer.
 * All coordinates: x=0 left, y=0 top, width=128, height=64.
 * ============================================================ */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Pixel ─────────────────────────────────────────────────────
void gfx_pixel(int x, int y, bool set);
bool gfx_get_pixel(int x, int y);

// ── Primitives ────────────────────────────────────────────────
void gfx_hline(int x, int y, int w, bool set);
void gfx_vline(int x, int y, int h, bool set);
void gfx_rect(int x, int y, int w, int h, bool set);
void gfx_fill_rect(int x, int y, int w, int h, bool set);
void gfx_line(int x0, int y0, int x1, int y1, bool set);
void gfx_circle(int cx, int cy, int r, bool set);
void gfx_fill_circle(int cx, int cy, int r, bool set);

// ── Text: 5×7 font ────────────────────────────────────────────
void gfx_char5x7(int x, int y, char c);
int  gfx_str5x7(int x, int y, const char *s);   // returns end x
void gfx_str5x7_center(int y, const char *s);    // centered on screen

// ── Text: 16×24 large font (digits + °C) ─────────────────────
void gfx_digit16x24(int x, int y, int digit);    // 0-9
void gfx_number16x24(int x, int y, int val);     // up to 3 digits
void gfx_degC16x24(int x, int y);                // '°C' symbol

// ── Bitmap ────────────────────────────────────────────────────
// w must be multiple of 8; bmp is row-major, MSB=leftmost pixel
void gfx_bitmap(int x, int y, int w, int h, const uint8_t *bmp);

// ── Progress bar ─────────────────────────────────────────────
// value 0–1000 (permille)
void gfx_progress_bar(int x, int y, int w, int h, int permille);

// ── Scrolling temperature graph ───────────────────────────────
// samples: array of GRAPH_SAMPLES floats (°C), max_temp for scaling
void gfx_temp_graph(int x, int y, int w, int h,
                    const float *samples, int count, float max_temp);

// ── Invert a rectangular region ──────────────────────────────
void gfx_invert_rect(int x, int y, int w, int h);

#ifdef __cplusplus
}
#endif
