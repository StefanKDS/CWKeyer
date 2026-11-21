/**
 * @file    flash_store.h
 * @brief   Wear levelled record storage in internal flash sector 7
 *          (0x080C0000, 256 KiB, excluded from the linker FLASH region).
 */
#ifndef FLASH_STORE_H
#define FLASH_STORE_H

#include <stdbool.h>
#include <stddef.h>

bool flash_store_load(void *buf, size_t len);
bool flash_store_save(const void *buf, size_t len);

#endif
