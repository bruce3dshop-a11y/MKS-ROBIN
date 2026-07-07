/* ============================================================
 * Graphics primitives implementation
 * ============================================================ */

#include "graphics/graphics.h"
#include "graphics/fonts.h"
#include "display/display.h"
#include <stdlib.h>
#include <string.h>

// ── Internal ──────────────────────────────────────────────────
static inline void _clip(int *v, int lo, int hi) {
    if (*v < lo) *v = lo;
    if (*v > hi) *v = hi;
}

void gfx_pixel(int x, int y, bool set) {
    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) return;
    int idx = y * (LCD_WIDTH / 8) + (x / 8);
    uint8_t mask = 0x80 >> (x & 7);
    if (set) display_fb[idx] |=  mask;
    else     display_fb[idx] &= ~mask;
}

bool gfx_get_pixel(int x, int y) {
    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) return false;
    return (display_fb[y * (LCD_WIDTH / 8) + (x / 8)] >> (7 - (x & 7))) & 1;
}

void gfx_hline(int x, int y, int w, bool set) {
    for (int i = x; i < x + w; i++) gfx_pixel(i, y, set);
}
void gfx_vline(int x, int y, int h, bool set) {
    for (int i = y; i < y + h; i++) gfx_pixel(x, i, set);
}
void gfx_rect(int x, int y, int w, int h, bool set) {
    gfx_hline(x, y, w, set);
    gfx_hline(x, y + h - 1, w, set);
    gfx_vline(x, y, h, set);
    gfx_vline(x + w - 1, y, h, set);
}
void gfx_fill_rect(int x, int y, int w, int h, bool set) {
    for (int row = y; row < y + h; row++) gfx_hline(x, row, w, set);
}
void gfx_line(int x0, int y0, int x1, int y1, bool set) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    while (true) {
        gfx_pixel(x0, y0, set);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}
void gfx_circle(int cx, int cy, int r, bool set) {
    int x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
        gfx_pixel(cx+x, cy+y, set); gfx_pixel(cx-x, cy+y, set);
        gfx_pixel(cx+x, cy-y, set); gfx_pixel(cx-x, cy-y, set);
        gfx_pixel(cx+y, cy+x, set); gfx_pixel(cx-y, cy+x, set);
        gfx_pixel(cx+y, cy-x, set); gfx_pixel(cx-y, cy-x, set);
        if (d <= 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}
void gfx_fill_circle(int cx, int cy, int r, bool set) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x*x + y*y <= r*r) gfx_pixel(cx+x, cy+y, set);
}
void gfx_invert_rect(int x, int y, int w, int h) {
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            gfx_pixel(col, row, !gfx_get_pixel(col, row));
}

// ── 5×7 font ─────────────────────────────────────────────────
void gfx_char5x7(int x, int y, char c) {
    if (c < 32 || c > 127) c = '?';
    const uint8_t *col = FONT5x7[(uint8_t)(c - 32)];
    for (int cx = 0; cx < 5; cx++) {
        uint8_t bits = col[cx];
        for (int row = 0; row < 7; row++)
            gfx_pixel(x + cx, y + row, (bits >> row) & 1);
    }
}
int gfx_str5x7(int x, int y, const char *s) {
    while (*s) { gfx_char5x7(x, y, *s++); x += 6; }
    return x;
}
void gfx_str5x7_center(int y, const char *s) {
    int len = 0;
    for (const char *p = s; *p; p++) len++;
    int x = (LCD_WIDTH - len * 6) / 2;
    gfx_str5x7(x, y, s);
}

// ── 16×24 large font ─────────────────────────────────────────
#include "graphics/fonts.h"

// Draw one 16×24 glyph from FONT16x24 (2 bytes × 24 rows = 48 bytes)
static void _draw_glyph16x24(int x, int y, int idx) {
    const uint8_t *data = FONT16x24[idx];
    for (int row = 0; row < 24; row++) {
        uint8_t hi = data[row * 2];
        uint8_t lo = data[row * 2 + 1];
        for (int b = 0; b < 8; b++) gfx_pixel(x + b,     y + row, (hi >> (7 - b)) & 1);
        for (int b = 0; b < 8; b++) gfx_pixel(x + 8 + b, y + row, (lo >> (7 - b)) & 1);
    }
}

void gfx_digit16x24(int x, int y, int digit) {
    if (digit < 0 || digit > 9) return;
    _draw_glyph16x24(x, y, digit);
}

void gfx_number16x24(int x, int y, int val) {
    if (val < 0) val = 0;
    if (val > 999) val = 999;
    int hundreds = val / 100;
    int tens     = (val % 100) / 10;
    int ones     = val % 10;
    int xoff = x;
    if (val >= 100) { _draw_glyph16x24(xoff, y, hundreds); xoff += 17; }
    if (val >= 10)  { _draw_glyph16x24(xoff, y, tens);     xoff += 17; }
    _draw_glyph16x24(xoff, y, ones);
}

void gfx_degC16x24(int x, int y) {
    _draw_glyph16x24(x,      y, F16_DEG);
    _draw_glyph16x24(x + 16, y, F16_C);
}

// ── Bitmap ────────────────────────────────────────────────────
void gfx_bitmap(int x, int y, int w, int h, const uint8_t *bmp) {
    int bytes_per_row = w / 8;
    for (int row = 0; row < h; row++) {
        for (int byte_idx = 0; byte_idx < bytes_per_row; byte_idx++) {
            uint8_t b = bmp[row * bytes_per_row + byte_idx];
            for (int bit = 0; bit < 8; bit++)
                gfx_pixel(x + byte_idx * 8 + bit, y + row, (b >> (7 - bit)) & 1);
        }
    }
}

// ── Progress bar ─────────────────────────────────────────────
void gfx_progress_bar(int x, int y, int w, int h, int permille) {
    gfx_rect(x, y, w, h, true);
    int fill = (int)((long)(w - 2) * permille / 1000);
    if (fill > 0) gfx_fill_rect(x + 1, y + 1, fill, h - 2, true);
}

// ── Temperature graph ─────────────────────────────────────────
void gfx_temp_graph(int x, int y, int w, int h,
                    const float *samples, int count, float max_temp)
{
    // Draw frame
    gfx_rect(x, y, w, h, true);
    if (count < 2) return;

    float scale = (float)(h - 4) / max_temp;
    int prev_px = -1, prev_py = -1;

    for (int i = 0; i < count && i < w - 2; i++) {
        int si = (count >= (w - 2)) ? (count - (w - 2) + i) : i;
        if (si < 0 || si >= count) continue;
        float t = samples[si];
        if (t < 0) t = 0;
        int px = x + 1 + i;
        int py = y + h - 2 - (int)(t * scale);
        if (py < y + 1)     py = y + 1;
        if (py > y + h - 2) py = y + h - 2;
        if (prev_px >= 0)
            gfx_line(prev_px, prev_py, px, py, true);
        prev_px = px; prev_py = py;
    }
}
