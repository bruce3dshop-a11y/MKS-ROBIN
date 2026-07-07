#pragma once
/* ============================================================
 * STM32F4xx HAL Configuration
 * Enable only the modules we need to keep code size small.
 * ============================================================ */

#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED

/* HSE / LSE oscillator values */
#if !defined(HSE_VALUE)
  #define HSE_VALUE    8000000U
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    100U
#endif
#if !defined(LSE_VALUE)
  #define LSE_VALUE    32768U
#endif
#if !defined(EXTERNAL_CLOCK_VALUE)
  #define EXTERNAL_CLOCK_VALUE    12288000U
#endif

/* System config */
#define VDD_VALUE                  3300U
#define TICK_INT_PRIORITY          0U
#define USE_RTOS                   0U
#define PREFETCH_ENABLE            1U
#define INSTRUCTION_CACHE_ENABLE   1U
#define DATA_CACHE_ENABLE          1U

/* SysTick -> HAL_Delay base */
#define USE_SysTick_CLK_SOURCE     1U

/* Includes for each module */
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "stm32f4xx_hal_pwr.h"
#include "stm32f4xx_hal_iwdg.h"

/* Assert macro — disable in release */
#define assert_param(expr) ((void)0U)
