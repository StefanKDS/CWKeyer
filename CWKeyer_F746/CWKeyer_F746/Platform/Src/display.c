/**
 * @file    display.c
 * @brief   Uses the ST BSP for LTDC/SDRAM initialisation, then swaps the
 *          layer frame buffer address directly at vertical blanking.
 */
#include "display.h"
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_sdram.h"
#include <string.h>

#define FB_SIZE   (480u * 272u * 2u)
#define FB0       (LCD_FB_START_ADDRESS)
#define FB1       (LCD_FB_START_ADDRESS + FB_SIZE)

static uint32_t s_front = FB0;

bool display_init(void)
{
    if (BSP_SDRAM_Init() != SDRAM_OK)
        return false;
    if (BSP_LCD_Init() != LCD_OK)
        return false;

    memset((void *)FB0, 0, FB_SIZE);
    memset((void *)FB1, 0, FB_SIZE);

    BSP_LCD_LayerRgb565Init(0, FB0);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_DisplayOn();
    s_front = FB0;
    return true;
}

uint16_t *display_backbuffer(void)
{
    return (uint16_t *)((s_front == FB0) ? FB1 : FB0);
}

static void rotate_180(uint16_t *fb)
{
    uint32_t count = 480u * 272u;
    for (uint32_t i = 0u, j = count - 1u; i < j; ++i, --j)
    {
        uint16_t tmp = fb[i];
        fb[i] = fb[j];
        fb[j] = tmp;
    }
}

void display_swap(void)
{
    uint32_t back = (s_front == FB0) ? FB1 : FB0;

    rotate_180((uint16_t *)back);

    /* frame buffer region is write-through (MPU), no cache clean needed */
    __DSB();
    LTDC_Layer1->CFBAR = back;
    LTDC->SRCR = LTDC_SRCR_VBR;                 /* reload at vertical blanking */
    uint32_t t0 = HAL_GetTick();
    while ((LTDC->SRCR & LTDC_SRCR_VBR) && (HAL_GetTick() - t0) < 40u)
    {
        /* wait until the new buffer is displayed */
    }
    s_front = back;
}
