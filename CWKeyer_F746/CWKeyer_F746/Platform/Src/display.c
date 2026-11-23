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
#define FB_CANVAS (LCD_FB_START_ADDRESS + 2u * FB_SIZE)   /* render target for 180 deg */

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

static uint32_t backbuffer(void)
{
    return (s_front == FB0) ? FB1 : FB0;
}

uint16_t *display_canvas(bool rotate180)
{
    return (uint16_t *)(rotate180 ? FB_CANVAS : backbuffer());
}

/* copy src into dst turned by 180 degrees (= reversed pixel order) */
static void rotate_copy(const uint32_t *src, uint32_t *dst)
{
    const uint32_t n = FB_SIZE / 4u;            /* two pixels per word */
    uint32_t *d = dst + n - 1u;
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t v = src[i];
        *d-- = (v >> 16) | (v << 16);
    }
}

void display_swap(bool rotate180)
{
    uint32_t back = backbuffer();
    if (rotate180)
        rotate_copy((const uint32_t *)FB_CANVAS, (uint32_t *)back);

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
