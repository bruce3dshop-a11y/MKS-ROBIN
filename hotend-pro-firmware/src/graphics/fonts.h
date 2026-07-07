#pragma once
/* ============================================================
 * Bitmap fonts for HOTEND PRO
 *   FONT5x7   — 5×7 small font  (ASCII 32-127)
 *   FONT8x13  — 8×13 medium font (digits + limited chars)
 *   FONT16x24 — 16×24 large font (digits 0-9 + °C)
 * ============================================================ */

#include <stdint.h>

// ─── 5×7 font — each char is 5 bytes (columns), 7 pixels tall ──
// Characters 32 (space) through 127 are included.
extern const uint8_t FONT5x7[96][5];

// ─── 8×13 medium font — each char is 13 bytes (rows), 8 pixels wide
// Characters '0'-'9', '%', '°', 'A'-'Z', ':', '/', ' '
extern const uint8_t FONT8x13_chars[];          // character map
extern const uint8_t FONT8x13_data[][13];        // bitmap data (8 wide × 13 tall)

// ─── 16×24 large font — digits 0-9 + ':' + '°' + 'C'
// Each char = 2 bytes × 24 rows = 48 bytes (16 pixels wide)
extern const uint8_t FONT16x24[13][48];

// Helper: index for FONT16x24 (0-9=digits, 10=':', 11='°', 12='C')
#define F16_COLON   10
#define F16_DEG     11
#define F16_C       12
