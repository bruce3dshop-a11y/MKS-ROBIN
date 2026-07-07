/* ============================================================
 * ST7920 128×64 Graphic LCD — Software SPI Driver
 * ============================================================ */

#include "display/display.h"
#include <string.h>

// ── Framebuffer ──────────────────────────────────────────────
uint8_t display_fb[LCD_BUF_BYTES];
static uint8_t _hw_buf[LCD_BUF_BYTES];   // shadow of what LCD currently shows

// ── GPIO helpers ─────────────────────────────────────────────
static inline void cs_high(void)   { LCD_CS_PORT->BSRR  = LCD_CS_PIN; }
static inline void cs_low(void)    { LCD_CS_PORT->BSRR  = (LCD_CS_PIN << 16); }
static inline void sck_high(void)  { LCD_SCK_PORT->BSRR = LCD_SCK_PIN; }
static inline void sck_low(void)   { LCD_SCK_PORT->BSRR = (LCD_SCK_PIN << 16); }
static inline void sid_high(void)  { LCD_MOSI_PORT->BSRR = LCD_MOSI_PIN; }
static inline void sid_low(void)   { LCD_MOSI_PORT->BSRR = (LCD_MOSI_PIN << 16); }

// ── ST7920 serial protocol timing ────────────────────────────
// At 168 MHz one NOP ≈ 6 ns; ST7920 min SCLK cycle ≈ 1000 ns at 5V/400ns at 3.3V
// We insert a few NOPs for safety.
static inline void delay_ns(void) {
    __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP();
}

/* Send one byte MSB-first over the ST7920 3-wire serial interface.
 * CS must already be HIGH before calling (ST7920 CS is active-high). */
static void spi_byte(uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        sck_low();
        if (b & (1 << i)) sid_high(); else sid_low();
        delay_ns();
        sck_high();
        delay_ns();
    }
    sck_low();
}

/* ST7920 serial command format:
 *  CS is ACTIVE HIGH — raise before frame, lower after.
 *  Start byte : 1111 1RWx  (RW=0 write, x=0 cmd / x=1 data… actually:
 *               0xF8 = 11111000 = write command
 *               0xFA = 11111010 = write data)
 *  Data byte  : upper nibble | 0000
 *  Data byte  : lower nibble | 0000 */
static void st7920_cmd(uint8_t cmd) {
    cs_high();               // ST7920 CS is active-HIGH
    delay_ns();
    spi_byte(0xF8);          // start: write instruction
    spi_byte(cmd & 0xF0);    // high nibble
    spi_byte(cmd << 4);      // low  nibble
    delay_ns();
    cs_low();                // deselect
}

static void st7920_data(uint8_t data) {
    cs_high();
    delay_ns();
    spi_byte(0xFA);          // start: write data
    spi_byte(data & 0xF0);
    spi_byte(data << 4);
    delay_ns();
    cs_low();
}

// ── Public API ───────────────────────────────────────────────

void display_init(void) {
    GPIO_InitTypeDef g = {};

    // Enable clocks for all ports used by LCD pins
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();   // PD1=SCK, PD3=MOSI

    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Pull  = GPIO_NOPULL;

    g.Pin = LCD_CS_PIN;
    HAL_GPIO_Init(LCD_CS_PORT, &g);

    g.Pin = LCD_SCK_PIN;
    HAL_GPIO_Init(LCD_SCK_PORT, &g);

    g.Pin = LCD_MOSI_PIN;
    HAL_GPIO_Init(LCD_MOSI_PORT, &g);

    cs_low();    // CS idle = LOW (ST7920 CS is active-high)
    sck_low();

    HAL_Delay(50);   // Wait for LCD power up

    // ST7920 initialization sequence
    st7920_cmd(0x30);   // Function set: 8-bit, basic instruction
    HAL_Delay(2);
    st7920_cmd(0x30);   // Repeat
    HAL_Delay(2);
    st7920_cmd(0x0C);   // Display ON, cursor OFF, blink OFF
    HAL_Delay(2);
    st7920_cmd(0x01);   // Clear display
    HAL_Delay(10);
    st7920_cmd(0x06);   // Entry mode: increment, no shift
    HAL_Delay(2);
    st7920_cmd(0x34);   // Extended instructions
    HAL_Delay(2);
    st7920_cmd(0x36);   // Extended + graphic display ON
    HAL_Delay(2);

    memset(display_fb, 0, sizeof(display_fb));
    memset(_hw_buf, 0xFF, sizeof(_hw_buf));  // force full refresh on first flush
}

/* Push framebuffer to GDRAM.
 * ST7920 GDRAM layout: 16 rows of 2 half-planes (64 rows × 16 bytes each).
 * Upper half:  rows  0-31 → GDRAM Y = 0-31,  X = 0-15
 * Lower half:  rows 32-63 → GDRAM Y = 0-31,  X = 16-31  (X wraps around)
 * Each row is 128 pixels = 16 bytes.
 */
void display_flush(void) {
    for (int row = 0; row < 32; row++) {
        const uint8_t *upper = &display_fb[row * 16];
        const uint8_t *lower = &display_fb[(row + 32) * 16];

        st7920_cmd(0x80 | row);     // Set GDRAM vertical address
        st7920_cmd(0x80 | 0);       // Set GDRAM horizontal address = 0

        for (int x = 0; x < 16; x++) st7920_data(upper[x]);
        for (int x = 0; x < 16; x++) st7920_data(lower[x]);
    }
    memcpy(_hw_buf, display_fb, LCD_BUF_BYTES);
}

void display_clear(void) {
    memset(display_fb, 0, LCD_BUF_BYTES);
}

void display_set_contrast(uint8_t /*level*/) {
    // ST7920 does not have a contrast command over SPI.
    // Contrast is hardware-adjusted via VO pin (trim pot on LCD module).
}
