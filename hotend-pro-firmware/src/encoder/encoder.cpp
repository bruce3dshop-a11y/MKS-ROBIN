/* ============================================================
 * Rotary encoder — debounced quadrature decode + button events
 * ============================================================ */

#include "encoder/encoder.h"
#include "config.h"

#define EVENT_QUEUE_SIZE 8

static EncoderEvent _queue[EVENT_QUEUE_SIZE];
static int _q_head = 0, _q_tail = 0;

static inline void _enqueue(EncoderEvent e) {
    int next = (_q_tail + 1) % EVENT_QUEUE_SIZE;
    if (next != _q_head) { _queue[_q_tail] = e; _q_tail = next; }
}

// Encoder state
static uint8_t _enc_state   = 0;
static int8_t  _enc_accel   = 0;

// Button state
static bool    _btn_prev    = false;
static bool    _btn_down    = false;
static uint32_t _btn_down_ms = 0;
static bool    _hold_fired  = false;

// Debounce
#define DEBOUNCE_TICKS 3
static uint8_t _deb_a = 0, _deb_b = 0, _deb_btn = 0;
static bool    _a_stable = false, _b_stable = false, _btn_stable = false;

static bool _read_pin(GPIO_TypeDef *port, uint16_t pin) {
    return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET;
}

void encoder_init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef g = {};
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;

    g.Pin = ENC_A_PIN;   HAL_GPIO_Init(ENC_A_PORT, &g);
    g.Pin = ENC_B_PIN;   HAL_GPIO_Init(ENC_B_PORT, &g);
    g.Pin = ENC_BTN_PIN; HAL_GPIO_Init(ENC_BTN_PORT, &g);

    _enc_state = (_read_pin(ENC_A_PORT, ENC_A_PIN) ? 2 : 0)
               | (_read_pin(ENC_B_PORT, ENC_B_PIN) ? 1 : 0);
}

/* Full quadrature decode table (4-state → CW/CCW) */
static const int8_t _qem[16] = {
    0, -1,  1,  0,
    1,  0,  0, -1,
   -1,  0,  0,  1,
    0,  1, -1,  0
};

void encoder_update(uint32_t now_ms) {
    // ── Debounce A, B ────────────────────────────────────────
    bool raw_a   = _read_pin(ENC_A_PORT, ENC_A_PIN);
    bool raw_b   = _read_pin(ENC_B_PORT, ENC_B_PIN);
    bool raw_btn = !_read_pin(ENC_BTN_PORT, ENC_BTN_PIN); // active-low

    _deb_a   = ((_deb_a   << 1) | raw_a)   & 0x07;
    _deb_b   = ((_deb_b   << 1) | raw_b)   & 0x07;
    _deb_btn = ((_deb_btn << 1) | raw_btn)  & 0x07;

    bool a_now   = (_deb_a   == 0x07) ? true  : (_deb_a   == 0x00) ? false : _a_stable;
    bool b_now   = (_deb_b   == 0x07) ? true  : (_deb_b   == 0x00) ? false : _b_stable;
    bool btn_now = (_deb_btn == 0x07) ? true  : (_deb_btn == 0x00) ? false : _btn_stable;

    // ── Quadrature decode ────────────────────────────────────
    if (a_now != _a_stable || b_now != _b_stable) {
        uint8_t new_state = (a_now ? 2 : 0) | (b_now ? 1 : 0);
        int8_t  dir = _qem[(_enc_state << 2) | new_state];
        _enc_state = new_state;
        _a_stable = a_now;
        _b_stable = b_now;

        if (dir != 0) {
            _enqueue(dir > 0 ? ENC_EVENT_CW : ENC_EVENT_CCW);
        }
    }

    // ── Button ───────────────────────────────────────────────
    if (btn_now != _btn_stable) {
        _btn_stable = btn_now;
        if (btn_now) {
            // Button just pressed
            _btn_down    = true;
            _btn_down_ms = now_ms;
            _hold_fired  = false;
        } else {
            // Button released
            if (_btn_down && !_hold_fired) {
                _enqueue(ENC_EVENT_CLICK);
            }
            _btn_down = false;
        }
    }

    // Hold detection
    if (_btn_down && !_hold_fired) {
        if ((now_ms - _btn_down_ms) >= LOCK_HOLD_MS) {
            _hold_fired = true;
            _enqueue(ENC_EVENT_HOLD);
        }
    }

    _btn_prev = btn_now;
}

EncoderEvent encoder_pop_event(void) {
    if (_q_head == _q_tail) return ENC_EVENT_NONE;
    EncoderEvent e = _queue[_q_head];
    _q_head = (_q_head + 1) % EVENT_QUEUE_SIZE;
    return e;
}

bool encoder_any_event(void) { return _q_head != _q_tail; }
