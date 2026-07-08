/* ============================================================
 * ST7920 128×64 Graphic LCD — Software SPI Driver
 * ============================================================
 * MKS Robin Nano V3.1 + RepRap Full Graphic Smart Controller
 *
 * ST7920 serial interface constraints (VDD = 5 V):
 *   Max clock frequency : 1 MHz  → half-period ≥ 500 ns
 *   CS is ACTIVE HIGH   : raise before frame, lower after
 *
 * Pin mapping (EXP1/EXP2 connector, Marlin-compatible):
 *   CS   = PA8  (LCD_PINS_RS   in Marlin)
 *   SCK  = PD1  (LCD_PINS_D4   in Marlin)
 *   MOSI = PD3  (LCD_PINS_ENABLE in Marlin)
 * ============================================================ */

#include "display/display.h"
#include <string.h>

// ── Framebuffer ──────────────────────────────────────────────
uint8_t display_fb[LCD_BUF_BYTES];

// ── GPIO helpers (direct register for speed) ─────────────────
static inline void cs_high(void)  { LCD_CS_PORT->BSRR  = LCD_CS_PIN; }
static inline void cs_low(void)   { LCD_CS_PORT->BSRR  = (uint32_t)LCD_CS_PIN << 16; }
static inline void sck_high(void) { LCD_SCK_PORT->BSRR = LCD_SCK_PIN; }
static inline void sck_low(void)  { LCD_SCK_PORT->BSRR = (uint32_t)LCD_SCK_PIN << 16; }
static inline void sid_high(void) { LCD_MOSI_PORT->BSRR = LCD_MOSI_PIN; }
static inline void sid_low(void)  { LCD_MOSI_PORT->BSRR = (uint32_t)LCD_MOSI_PIN << 16; }

/* ~500 ns delay at 168 MHz — used for SPI bit-bang timing only.
 * NOT sufficient between ST7920 commands (need ≥72 µs). */
static inline void half_period(void) {
    for (volatile uint8_t i = 0; i < 14; i++) __NOP();
}

/* Busy-wait for at least `us` microseconds.
 *
 * ST7920 command execution time = 72 µs (datasheet).
 * Arduino/Marlin hides this latency inside slow digitalWrite() calls
 * (~5 µs × 24 transitions = ~120 µs per command naturally).
 * With direct BSRR writes (~3 ns each) we must delay explicitly.
 *
 * Calibration: 168 MHz core, ~5 cycles per loop iteration (NOP + decrement
 * + compare + branch on Cortex-M4 with I-cache).  34 iterations ≈ 1 µs.
 */
static void delay_us(uint32_t us) {
    volatile uint32_t n = us * 34u;
    while (n--) { __NOP(); }
}

/* Send one byte MSB-first. CS must already be HIGH. */
static void spi_byte(uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        sck_low();
        if (b & (1 << i)) sid_high(); else sid_low();
        half_period();
        sck_high();
        half_period();
    }
    sck_low();
}

/* ST7920 serial frame:
 *   Sync  : 0xF8 = 11111000  → write instruction
 *           0xFA = 11111010  → write data
 *   Byte 1: D7-D4 | 0000    (high nibble)
 *   Byte 2: D3-D0 | 0000    (low  nibble)
 * CS stays HIGH for the entire 3-byte frame, then goes LOW. */
static void st7920_cmd(uint8_t cmd) {
    cs_high();
    half_period();
    spi_byte(0xF8);
    spi_byte(cmd & 0xF0);
    spi_byte((uint8_t)(cmd << 4));
    half_period();
    cs_low();
    half_period();
}

static void st7920_data(uint8_t d) {
    cs_high();
    half_period();
    spi_byte(0xFA);
    spi_byte(d & 0xF0);
    spi_byte((uint8_t)(d << 4));
    half_period();
    cs_low();
    half_period();
}

/* Write multiple GDRAM bytes with CS held HIGH throughout.
 *
 * ST7920 serial protocol requires a sync byte BEFORE EACH data byte:
 *   [CS HIGH]
 *     0xFA + high_nibble + low_nibble   ← byte 0
 *     0xFA + high_nibble + low_nibble   ← byte 1
 *     ...
 *   [CS LOW]
 *
 * Sending one sync byte for multiple data bytes is WRONG and results
 * in a blank display even though commands are delivered correctly.
 */
static void st7920_data_stream(const uint8_t *buf, uint8_t len) {
    cs_high();
    half_period();
    for (uint8_t i = 0; i < len; i++) {
        spi_byte(0xFA);                        // sync byte for THIS byte
        spi_byte(buf[i] & 0xF0);              // high nibble
        spi_byte((uint8_t)(buf[i] << 4));     // low  nibble
    }
    half_period();
    cs_low();
    half_period();
}

// ── Public API ───────────────────────────────────────────────

void display_init(void) {
    GPIO_InitTypeDef g = {};

    /* Enable GPIO clocks for all three LCD pins */
    __HAL_RCC_GPIOA_CLK_ENABLE();   // PA8  = CS
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();   // PD1  = SCK, PD3 = MOSI

    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_MEDIUM;   // no need for VERY_HIGH at 1 MHz
    g.Pull  = GPIO_NOPULL;

    g.Pin = LCD_CS_PIN;   HAL_GPIO_Init(LCD_CS_PORT,   &g);
    g.Pin = LCD_SCK_PIN;  HAL_GPIO_Init(LCD_SCK_PORT,  &g);
    g.Pin = LCD_MOSI_PIN; HAL_GPIO_Init(LCD_MOSI_PORT, &g);

    cs_low();    // idle = deselected (CS active-high, so LOW = idle)
    sck_low();
    sid_low();

    HAL_Delay(100);   // ST7920 power-on stabilisation ≥ 40 ms

    /* ── Initialisation sequence (from ST7920 datasheet + U8glib) ── */
    st7920_cmd(0x30);  HAL_Delay(5);   // Function set: 8-bit, basic
    st7920_cmd(0x30);  HAL_Delay(5);   // Repeat (datasheet requirement)
    st7920_cmd(0x30);  HAL_Delay(5);   // Repeat
    st7920_cmd(0x08);  HAL_Delay(5);   // Display OFF
    st7920_cmd(0x01);  HAL_Delay(15);  // Clear display (needs ≥10 ms)
    st7920_cmd(0x06);  HAL_Delay(5);   // Entry mode: auto-increment
    st7920_cmd(0x0C);  HAL_Delay(5);   // Display ON, no cursor
    st7920_cmd(0x34);  HAL_Delay(5);   // Extended instructions
    st7920_cmd(0x36);  HAL_Delay(5);   // Graphic display ON

    /* ── Connection diagnostic ────────────────────────────────
     * Fill entire GDRAM with 1s (all pixels ON → solid black screen)
     * for 400 ms.  If the display goes black: ST7920 is alive, SPI
     * pins are correct.  If still solid blue: pins are wrong.
     * Remove this block once the connection is confirmed working. */
    memset(display_fb, 0xFF, sizeof(display_fb));
    display_flush();
    HAL_Delay(400);

    memset(display_fb, 0, sizeof(display_fb));
    display_flush();   // push blank frame to clear any GDRAM noise
}

/* Push framebuffer to ST7920 GDRAM.
 *
 * GDRAM layout (128 × 64 display):
 *   The 64 rows are split into two 32-row halves:
 *     Upper half: FB rows  0-31  → Y = 0-31,  X-start = 0x00
 *     Lower half: FB rows 32-63  → Y = 0-31,  X-start = 0x08
 *   Each row = 16 bytes (128 pixels / 8).
 *
 * CRITICAL timing note:
 *   ST7920 command execution time = 72 µs (datasheet).
 *   We MUST wait ≥ 72 µs after setting the Y address before sending
 *   the X address, and again before writing data.  Without this,
 *   GDRAM writes silently fail and the display stays blank/blue.
 */
void display_flush(void) {
    for (uint8_t row = 0; row < 32; row++) {
        /* Upper half */
        st7920_cmd(0x80 | row);   // Y address
        delay_us(80);             // ≥ 72 µs execution time
        st7920_cmd(0x80);         // X address = 0
        delay_us(80);
        st7920_data_stream(&display_fb[row * 16], 16);

        /* Lower half (same Y range, X offset = 8 words = 0x88) */
        st7920_cmd(0x80 | row);
        delay_us(80);
        st7920_cmd(0x88);         // X address = 8
        delay_us(80);
        st7920_data_stream(&display_fb[(row + 32) * 16], 16);
    }
}

void display_clear(void) {
    memset(display_fb, 0, LCD_BUF_BYTES);
}

void display_set_contrast(uint8_t /*level*/) {
    // ST7920 contrast is hardware-set via VO trim-pot on the LCD board.
}
