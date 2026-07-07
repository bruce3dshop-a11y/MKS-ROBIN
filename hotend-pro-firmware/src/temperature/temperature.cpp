/* ============================================================
 * Temperature sensing — STM32F407 ADC + NTC thermistor
 * Steinhart-Hart / Beta model for 100K NTC (Marlin Type 1)
 * ============================================================ */

#include "temperature/temperature.h"
#include <math.h>

static ADC_HandleTypeDef hadc;
static float    _temp_c  = 25.0f;
static uint16_t _adc_raw = 0;
static ThermStatus _status = THERM_OK;

// Simple EMA filter (alpha ≈ 0.2)
static float _ema = 25.0f;
#define EMA_ALPHA 0.2f

static float adc_to_celsius(uint16_t adc) {
    if (adc < OPEN_THERM_ADC_THRESHOLD)  return -999.0f;  // open
    if (adc > SHORT_THERM_ADC_THRESHOLD) return  999.0f;  // short

    // Voltage divider: Vout = Vcc * R_NTC / (R_series + R_NTC)
    // R_NTC = R_series * adc / (ADC_MAX - adc)
    float r_ntc = (float)THERMISTOR_SERIES_R * (float)adc
                  / ((float)ADC_MAX - (float)adc);

    // Steinhart-Hart simplified (Beta equation):
    // 1/T = 1/T0 + (1/B) * ln(R/R0)
    // T in Kelvin
    float temp_k = 1.0f /
        (1.0f / (TEMP_AMBIENT_C + 273.15f) +
         logf(r_ntc / (float)THERMISTOR_NOMINAL) / (float)THERMISTOR_B_COEFF);

    return temp_k - 273.15f;
}

void temperature_init(void) {
    // Enable clocks
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    // Configure PC1 as analog input
    GPIO_InitTypeDef g = {};
    g.Pin  = THERM_PIN;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(THERM_PORT, &g);

    // ADC1 configuration
    hadc.Instance                   = THERM_ADC;
    hadc.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc.Init.ScanConvMode          = DISABLE;
    hadc.Init.ContinuousConvMode    = DISABLE;
    hadc.Init.DiscontinuousConvMode = DISABLE;
    hadc.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc.Init.NbrOfConversion       = 1;
    hadc.Init.DMAContinuousRequests = DISABLE;
    hadc.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc);

    ADC_ChannelConfTypeDef cc = {};
    cc.Channel      = THERM_ADC_CHANNEL;
    cc.Rank         = 1;
    cc.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    HAL_ADC_ConfigChannel(&hadc, &cc);
}

void temperature_update(void) {
    HAL_ADC_Start(&hadc);
    if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK) {
        _adc_raw = HAL_ADC_GetValue(&hadc);
    }
    HAL_ADC_Stop(&hadc);

    float raw_c = adc_to_celsius(_adc_raw);

    if (raw_c < -100.0f) {
        _status = THERM_OPEN;
        return;
    }
    if (raw_c > 500.0f) {
        _status = THERM_SHORT;
        return;
    }
    if (raw_c > TEMP_MAX_C) {
        _status = THERM_OVERTEMP;
    } else {
        _status = THERM_OK;
    }

    // EMA filter
    _ema  = EMA_ALPHA * raw_c + (1.0f - EMA_ALPHA) * _ema;
    _temp_c = _ema;
}

float        temperature_get(void)         { return _temp_c;  }
uint16_t     temperature_get_adc_raw(void) { return _adc_raw; }
ThermStatus  temperature_get_status(void)  { return _status;  }
