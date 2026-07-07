/* ============================================================
 * Heater PWM — TIM9 CH1 on PE5 (AF3)
 * Timer clock = APB2 = 84 MHz
 * Prescaler = 84-1 → TIM clock = 1 MHz
 * ARR = HEATER_PWM_PERIOD (999) → f = 1 kHz
 * ============================================================ */

#include "heater/heater.h"

static TIM_HandleTypeDef _htim;
static float _current_output = 0.0f;

void heater_init(void) {
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_TIM9_CLK_ENABLE();

    // PE5 → TIM9_CH1 (AF3)
    GPIO_InitTypeDef g = {};
    g.Pin       = HEATER_PIN;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = HEATER_TIM_AF;
    HAL_GPIO_Init(HEATER_PORT, &g);

    // TIM9 base config
    _htim.Instance               = HEATER_TIM;
    _htim.Init.Prescaler         = (84 - 1);        // APB2 = 84 MHz → 1 MHz
    _htim.Init.CounterMode       = TIM_COUNTERMODE_UP;
    _htim.Init.Period            = HEATER_PWM_PERIOD; // ARR=999 → 1 kHz
    _htim.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    _htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&_htim);

    TIM_OC_InitTypeDef oc = {};
    oc.OCMode       = TIM_OCMODE_PWM1;
    oc.Pulse        = 0;
    oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode   = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&_htim, &oc, HEATER_TIM_CH);

    HAL_TIM_PWM_Start(&_htim, HEATER_TIM_CH);
}

void heater_set_output(float permille) {
    if (permille < 0.0f)         permille = 0.0f;
    if (permille > 1000.0f)      permille = 1000.0f;
    _current_output = permille;
    uint32_t ccr = (uint32_t)(permille * (float)(HEATER_PWM_PERIOD + 1) / 1000.0f);
    if (ccr > (HEATER_PWM_PERIOD + 1)) ccr = HEATER_PWM_PERIOD + 1;
    __HAL_TIM_SET_COMPARE(&_htim, HEATER_TIM_CH, ccr);
}

void heater_off(void) { heater_set_output(0.0f); }

float   heater_get_output(void)  { return _current_output; }
uint8_t heater_get_percent(void) { return (uint8_t)(_current_output / 10.0f); }
