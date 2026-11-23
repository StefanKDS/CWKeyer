/**
 * @file    usbd_storage.c
 * @brief   MSC storage interface: SD card via the BSP (polling).
 *
 * Called from the USB interrupt. The application only reads the card from
 * the main loop with the USB interrupt masked (port_stm32.c), so both never
 * access the SD controller at the same time.
 */
#include <stdbool.h>
#include "usbd_msc.h"
#include "stm32746g_discovery_sd.h"

#define LUN_COUNT  1
#define SD_TIMEOUT 2000U

extern bool sd_card_ready(void);     /* port_stm32.c */
extern bool sd_card_init(void);

static int8_t sto_init(uint8_t lun)
{
    (void)lun;
    return sd_card_ready() || sd_card_init() ? 0 : -1;
}

static int8_t sto_capacity(uint8_t lun, uint32_t *block_num, uint16_t *block_size)
{
    (void)lun;
    BSP_SD_CardInfo info;
    if (!sd_card_ready())
        return -1;
    BSP_SD_GetCardInfo(&info);
    *block_num  = info.LogBlockNbr - 1U;
    *block_size = (uint16_t)info.LogBlockSize;
    return 0;
}

static int8_t sto_ready(uint8_t lun)
{
    (void)lun;
    if (BSP_SD_IsDetected() != SD_PRESENT)
        return -1;
    return (sd_card_ready() || sd_card_init()) ? 0 : -1;
}

static int8_t sto_wp(uint8_t lun)
{
    (void)lun;
    return 0;
}

static int8_t wait_ready(void)
{
    uint32_t t0 = HAL_GetTick();
    while (BSP_SD_GetCardState() != SD_TRANSFER_OK)
    {
        if (HAL_GetTick() - t0 > SD_TIMEOUT)
            return -1;
    }
    return 0;
}

static int8_t sto_read(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
    (void)lun;
    if (BSP_SD_ReadBlocks((uint32_t *)(void *)buf, blk_addr, blk_len, SD_TIMEOUT) != MSD_OK)
        return -1;
    return wait_ready();
}

static int8_t sto_write(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
    (void)lun;
    if (BSP_SD_WriteBlocks((uint32_t *)(void *)buf, blk_addr, blk_len, SD_TIMEOUT) != MSD_OK)
        return -1;
    return wait_ready();
}

static int8_t sto_max_lun(void)
{
    return LUN_COUNT - 1;
}

/* SCSI inquiry: 36 bytes */
static int8_t s_inquiry[] =
{
    0x00, 0x80, 0x02, 0x02, 0x1F, 0x00, 0x00, 0x00,
    'C', 'W', 'K', 'e', 'y', 'e', 'r', ' ',                                  /* vendor   */
    'S', 'D', ' ', 'c', 'a', 'r', 'd', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', /* product */
    '1', '.', '0', '0',                                                      /* revision */
};

USBD_StorageTypeDef USBD_SD_fops =
{
    sto_init, sto_capacity, sto_ready, sto_wp, sto_read, sto_write, sto_max_lun, s_inquiry,
};
