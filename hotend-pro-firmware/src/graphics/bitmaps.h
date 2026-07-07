#pragma once
/* ============================================================
 * Icon and animation bitmaps for HOTEND PRO
 * All bitmaps are raw 1bpp, row-major, MSB=leftmost pixel.
 * ============================================================ */

#include <stdint.h>

// ── Flame animation — 3 frames, 16×16 pixels ────────────────
// 16 rows × 2 bytes per row = 32 bytes per frame
extern const uint8_t BMP_FLAME[3][32];

// ── Thermometer icon — 16×16 pixels (32 bytes) ──────────────
extern const uint8_t BMP_THERM[32];

// ── Lock icon — 12×16 pixels (2 bytes per row, 16 rows) ─────
extern const uint8_t BMP_LOCK[32];

// ── Checkmark icon — 12×12 pixels ───────────────────────────
extern const uint8_t BMP_CHECK[24];

// ── Warning / Error icon — 16×16 pixels ─────────────────────
extern const uint8_t BMP_WARN[32];

// ── Boot logo bitmap — 64×16 pixels (8 bytes per row × 16) ──
extern const uint8_t BMP_LOGO[128];
