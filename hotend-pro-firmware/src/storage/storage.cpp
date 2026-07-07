/* ============================================================
 * Flash storage — STM32F407 sector 11 (0x080E0000, 128 KB)
 * Simple single-sector write with CRC32 verification.
 * ============================================================ */

#include "storage/storage.h"
#include <string.h>

// Simple CRC32 (no hardware CRC used to keep dependencies minimal)
static uint32_t _crc32(const uint8_t *buf, uint32_t len) {
    uint32_t crc = 0xFFFFFFFF;
    while (len--) {
        crc ^= *buf++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

void storage_init(void) { /* Nothing — flash always accessible */ }

bool storage_load(void *dst, uint32_t size) {
    // Read magic
    uint32_t magic = *(volatile uint32_t *)FLASH_STORAGE_ADDR;
    if (magic != FLASH_STORAGE_MAGIC) return false;

    // Read entire struct
    memcpy(dst, (const void *)FLASH_STORAGE_ADDR, size);

    // Verify CRC (CRC stored in last 4 bytes of struct)
    uint32_t *p_crc = (uint32_t *)((uint8_t *)dst + size - 4);
    uint32_t calc   = _crc32((const uint8_t *)dst, size - 4);
    if (calc != *p_crc) return false;

    return true;
}

void storage_erase(void) {
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {};
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector       = FLASH_STORAGE_SECTOR;
    erase.NbSectors    = 1;

    uint32_t sector_error = 0;
    HAL_FLASHEx_Erase(&erase, &sector_error);

    HAL_FLASH_Lock();
}

// Maximum settings struct size we support (must be >= sizeof(Settings))
#define STORAGE_MAX_SIZE 256

void storage_save(const void *src, uint32_t size) {
    if (size > STORAGE_MAX_SIZE) return;

    // Copy into a mutable staging buffer so we can append the CRC
    uint8_t buf[STORAGE_MAX_SIZE];
    memcpy(buf, src, size);

    // Compute CRC over all bytes except the last 4 (CRC field) and write it in
    uint32_t crc = _crc32(buf, size - 4);
    memcpy(&buf[size - 4], &crc, sizeof(crc));

    storage_erase();

    HAL_FLASH_Unlock();

    const uint32_t *words = (const uint32_t *)buf;
    uint32_t        n     = (size + 3) / 4;
    uint32_t        addr  = FLASH_STORAGE_ADDR;

    for (uint32_t i = 0; i < n; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, words[i]);
        addr += 4;
    }

    HAL_FLASH_Lock();
}
