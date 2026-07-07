#pragma once
/* ============================================================
 * Flash storage — read/write to STM32F407 sector 11
 * ============================================================ */

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

void storage_init(void);
bool storage_load(void *dst, uint32_t size);
void storage_save(const void *src, uint32_t size);
void storage_erase(void);

#ifdef __cplusplus
}
#endif
