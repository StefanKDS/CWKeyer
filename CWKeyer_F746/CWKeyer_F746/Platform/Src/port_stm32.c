/**
 * @file    port_stm32.c
 * @brief   Implementation of App/Inc/port.h for the STM32F746G-DISCO.
 */
#include "port.h"
#include "audio_codec.h"
#include "board.h"
#include "flash_store.h"
#include "stm32746g_discovery_sd.h"
#include "usb_device.h"

/* ---- time / system ----------------------------------------------------- */

uint32_t port_millis(void)
{
    return HAL_GetTick();
}

uint32_t port_random(void)
{
    /* xorshift32, seeded with timing entropy (touch/keying moments) */
    static uint32_t x = 0x1234567u;
    x ^= SysTick->VAL ^ (HAL_GetTick() << 7);
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static uint32_t s_lock_depth;
static uint32_t s_primask;

void port_irq_lock(void)
{
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    if (s_lock_depth++ == 0)
        s_primask = pm;
}

void port_irq_unlock(void)
{
    if (s_lock_depth && --s_lock_depth == 0 && s_primask == 0)
        __enable_irq();
}

/* ---- keyer I/O ------------------------------------------------------------ */

bool port_paddle_dit(void)
{
    return HAL_GPIO_ReadPin(PADDLE_DIT_PORT, PADDLE_DIT_PIN) == GPIO_PIN_RESET;
}

bool port_paddle_dah(void)
{
    return HAL_GPIO_ReadPin(PADDLE_DAH_PORT, PADDLE_DAH_PIN) == GPIO_PIN_RESET;
}

bool port_straight_key(void)
{
    return HAL_GPIO_ReadPin(STRAIGHT_KEY_PORT, STRAIGHT_KEY_PIN) == GPIO_PIN_RESET;
}

void port_key_out(bool on)
{
    HAL_GPIO_WritePin(KEY_OUT_PORT, KEY_OUT_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ---- audio ----------------------------------------------------------------- */

void port_audio_set_input_gain(int8_t db)
{
    audio_codec_set_input_gain(db);
}

/* ---- settings -------------------------------------------------------------- */

bool port_settings_load(void *buf, size_t len)
{
    return flash_store_load(buf, len);
}

bool port_settings_save(const void *buf, size_t len)
{
    return flash_store_save(buf, len);
}

/* ---- SD card ----------------------------------------------------------------
 * The card is shared with the USB mass storage class, which accesses it from
 * the USB interrupt. Accesses from the main loop therefore mask that interrupt. */

#define SD_XFER_TIMEOUT 1000u

static volatile bool s_sd_ok;

bool sd_card_ready(void)
{
    return s_sd_ok;
}

bool sd_card_init(void)
{
    if (s_sd_ok)
        BSP_SD_DeInit();          /* card may have been changed */
    s_sd_ok = (BSP_SD_Init() == MSD_OK);
    return s_sd_ok;
}

bool port_sd_present(void)
{
    return BSP_SD_IsDetected() == SD_PRESENT;
}

bool port_sd_init(void)
{
    HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    bool ok = sd_card_init();
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    return ok;
}

/**
 * Block transfer SD <-> memory, used by the application and by USB MSC.
 *
 * The BSP reads/writes in polling mode: the CPU empties the 32 word SDMMC
 * FIFO. At 24 MHz / 4 bit the FIFO fills within ~10 us, so any interrupt
 * during the transfer (audio DMA, SysTick) causes an RX overrun / TX
 * underrun and the transfer fails. Therefore interrupts are blocked during
 * the data phase only (8 blocks = approx. 0.4 ms, the audio buffer lasts
 * 4 ms, so nothing is lost). Waiting for the card to finish programming
 * after a write happens with interrupts enabled.
 */
bool sd_transfer(bool write, uint8_t *buf, uint32_t lba, uint32_t count)
{
    if (!s_sd_ok)
        return false;

    for (int attempt = 0; attempt < 3; attempt++)
    {
        uint32_t pm = __get_PRIMASK();
        __disable_irq();
        uint8_t r = write ? BSP_SD_WriteBlocks((uint32_t *)(void *)buf, lba, count, SD_XFER_TIMEOUT)
                          : BSP_SD_ReadBlocks((uint32_t *)(void *)buf, lba, count, SD_XFER_TIMEOUT);
        if (pm == 0)
            __enable_irq();

        uint32_t t0 = HAL_GetTick();
        bool ok = (r == MSD_OK);
        while (BSP_SD_GetCardState() != SD_TRANSFER_OK)
        {
            if (HAL_GetTick() - t0 > 2000u)
            {
                ok = false;
                break;
            }
        }
        if (ok)
            return true;
    }
    return false;
}

bool port_sd_read(uint32_t lba, uint8_t *buf, uint32_t count)
{
    if (usb_device_active())
        return false;
    HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    bool ok = sd_transfer(false, buf, lba, count);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    return ok;
}

/* ---- USB ------------------------------------------------------------------- */

void port_usb_drive(bool on)
{
    usb_device_enable(on);
}

bool port_usb_active(void)
{
    return usb_device_active();
}

bool port_usb_host_connected(void)
{
    return usb_device_configured();
}

extern volatile uint32_t g_msc_reads, g_msc_writes, g_msc_errors;

void port_usb_stats(uint32_t *reads, uint32_t *writes, uint32_t *errors)
{
    *reads  = g_msc_reads;
    *writes = g_msc_writes;
    *errors = g_msc_errors;
}
