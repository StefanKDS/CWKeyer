/**
 * @file    flash_store.c
 * @brief   Records of 1 KiB are appended to the sector; the newest valid
 *          record wins. The sector is only erased when it is full
 *          (every 256 saves), which keeps flash wear and the erase stall
 *          (~1-2 s, CPU waits for flash) rare.
 */
#include "flash_store.h"
#include "stm32f7xx_hal.h"
#include <string.h>

#define STORE_ADDR    0x080C0000u
#define STORE_SIZE    0x00040000u
#define STORE_SECTOR  FLASH_SECTOR_7
#define SLOT_SIZE     1024u
#define SLOT_COUNT    (STORE_SIZE / SLOT_SIZE)
#define REC_MAGIC     0x52454331u     /* "REC1" */

typedef struct
{
    uint32_t magic;
    uint32_t len;
} rec_hdr_t;

static const rec_hdr_t *slot(uint32_t i)
{
    return (const rec_hdr_t *)(STORE_ADDR + i * SLOT_SIZE);
}

static int find_last(void)
{
    int last = -1;
    for (uint32_t i = 0; i < SLOT_COUNT; i++)
    {
        if (slot(i)->magic == 0xFFFFFFFFu)
            break;
        if (slot(i)->magic == REC_MAGIC)
            last = (int)i;
    }
    return last;
}

bool flash_store_load(void *buf, size_t len)
{
    int i = find_last();
    if (i < 0 || slot((uint32_t)i)->len != len)
        return false;
    memcpy(buf, (const uint8_t *)slot((uint32_t)i) + sizeof(rec_hdr_t), len);
    return true;
}

bool flash_store_save(const void *buf, size_t len)
{
    if (len + sizeof(rec_hdr_t) > SLOT_SIZE)
        return false;

    int next = find_last() + 1;
    /* skip slots that are not erased (e.g. interrupted write) */
    while (next < (int)SLOT_COUNT && slot((uint32_t)next)->magic != 0xFFFFFFFFu)
        next++;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_ERSERR);

    if (next >= (int)SLOT_COUNT)
    {
        FLASH_EraseInitTypeDef e = { 0 };
        uint32_t err = 0;
        e.TypeErase    = FLASH_TYPEERASE_SECTORS;
        e.Sector       = STORE_SECTOR;
        e.NbSectors    = 1;
        e.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        if (HAL_FLASHEx_Erase(&e, &err) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
        next = 0;
    }

    uint32_t addr = STORE_ADDR + (uint32_t)next * SLOT_SIZE;
    bool ok = true;

    /* data first, header last: a record only becomes valid when complete */
    const uint8_t *p = (const uint8_t *)buf;
    for (size_t o = 0; o < len && ok; o += 4)
    {
        uint32_t w = 0xFFFFFFFFu;
        memcpy(&w, p + o, (len - o) >= 4 ? 4 : (len - o));
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + sizeof(rec_hdr_t) + o, w) == HAL_OK;
    }
    if (ok)
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 4u, (uint32_t)len) == HAL_OK;
    if (ok)
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, REC_MAGIC) == HAL_OK;

    HAL_FLASH_Lock();

    /* flash is read through the (cached) AXI bus */
    SCB_InvalidateDCache_by_Addr((uint32_t *)STORE_ADDR, (int32_t)STORE_SIZE);
    return ok;
}
