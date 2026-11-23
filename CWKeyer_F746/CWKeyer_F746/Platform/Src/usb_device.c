/**
 * @file    usb_device.c
 */
#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_msc.h"

extern USBD_StorageTypeDef USBD_SD_fops;   /* usbd_storage.c */

USBD_HandleTypeDef hUsbDeviceFS;
static bool s_ok;
static volatile bool s_on;

void usb_device_clock_config(void)
{
    /* USB FS needs exactly 48 MHz. PLLQ gives 50 MHz at 200 MHz SYSCLK,
       therefore CK48 is taken from PLLSAI-P: 1 MHz * 192 / 4 = 48 MHz.
       PLLSAI-R (LCD pixel clock) is kept. SDMMC1 also uses CK48. */
    RCC_PeriphCLKInitTypeDef clk;
    HAL_RCCEx_GetPeriphCLKConfig(&clk);
    clk.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
    clk.Clk48ClockSelection  = RCC_CLK48SOURCE_PLLSAIP;
    clk.PLLSAI.PLLSAIN       = 192;
    clk.PLLSAI.PLLSAIP       = RCC_PLLSAIP_DIV4;
    HAL_RCCEx_PeriphCLKConfig(&clk);
}

bool usb_device_init(void)
{
    if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK)
        return false;
    if (USBD_RegisterClass(&hUsbDeviceFS, USBD_MSC_CLASS) != USBD_OK)
        return false;
    if (USBD_MSC_RegisterStorage(&hUsbDeviceFS, &USBD_SD_fops) != USBD_OK)
        return false;
    s_ok = true;                 /* not started: no pull-up, PC sees only a power sink */
    return s_ok;
}

void usb_device_enable(bool on)
{
    if (!s_ok || on == s_on)
        return;
    if (on)
    {
        s_on = true;             /* app stops using the card before the PC can access it */
        USBD_Start(&hUsbDeviceFS);
    }
    else
    {
        USBD_Stop(&hUsbDeviceFS);   /* removes the pull-up: PC sees the drive unplugged */
        s_on = false;
    }
}

bool usb_device_active(void)
{
    return s_on;
}

bool usb_device_configured(void)
{
    return s_on && hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED;
}
