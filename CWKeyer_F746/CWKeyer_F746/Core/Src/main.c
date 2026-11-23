/**
 * @file    main.c
 * @brief   CW Keyer for STM32F746G-DISCO - start-up and main loop.
 *
 * Architecture (see README.md):
 *
 *   Core/      start-up, clocks, MPU/cache, interrupt vectors
 *   Platform/  board specific drivers (audio codec, display, touch,
 *              GPIO, flash storage, USB mass storage) and port.h
 *   App/       hardware independent logic (keyer, player, decoder,
 *              trainer, settings, SD texts)          <- ported ESP code
 *   Gui/       touch UI: renderer, widgets, screens, fonts, icons
 *   Lib/       FAT16/32 reader
 *
 * Contexts:  SysTick 1 kHz -> app_tick_1ms()   (paddles, keyer, player)
 *            SAI DMA       -> app_audio_block() (decoder, sidetone)
 *            USB OTG FS    -> mass storage access to the SD card
 *            main loop     -> app_poll(), GUI at ~30 fps
 */
#include "main.h"
#include "app.h"
#include "audio_codec.h"
#include "board.h"
#include "display.h"
#include "gui.h"
#include "stm32746g_discovery_sd.h"
#include "text_source.h"
#include "touch.h"
#include "settings.h"
#include "usb_device.h"

#define FRAME_MS        33u     /* ~30 fps */
#define SD_POLL_MS      1000u

static void MPU_Config(void);
static void SystemClock_Config(void);

int main(void)
{
    MPU_Config();
    SCB_EnableICache();
    SCB_EnableDCache();

    HAL_Init();
    SystemClock_Config();
    HAL_NVIC_SetPriority(SysTick_IRQn, IRQ_PRIO_SYSTICK, 0);

    board_gpio_init();
    if (!display_init())
        Error_Handler();
    usb_device_clock_config();        /* 48 MHz for USB FS + SDMMC */
    touch_init();

    app_init();                       /* loads settings from flash */
    audio_codec_init(AUDIO_FS);
    app_apply_settings();             /* now also reaches the codec */

    text_source_scan();
    usb_device_init();                /* SD card as USB drive on CN13, switched on in the GUI */
    gui_init();
    board_tick_enable();

    uint32_t last_frame = 0;
    uint32_t last_sd = 0;
    bool sd_present = BSP_SD_IsDetected() == SD_PRESENT;

    for (;;)
    {
        app_poll();

        uint32_t now = HAL_GetTick();
        if (now - last_frame >= FRAME_MS)
        {
            last_frame = now;
            bool rot = g_settings.rotate180 != 0;
            touch_t t;
            touch_read(&t, rot);
            gui_render(display_canvas(rot), &t, now);
            display_swap(rot);
        }

        /* SD card inserted / removed -> rescan (USB drive on/off is handled by the GUI) */
        if (now - last_sd >= SD_POLL_MS)
        {
            last_sd = now;
            bool p = BSP_SD_IsDetected() == SD_PRESENT;
            if (p != sd_present)
            {
                sd_present = p;
                text_source_scan();
            }
        }
    }
}

/**
 * System clock: HSE 25 MHz -> PLL -> 200 MHz SYSCLK (over-drive),
 * AHB 200 MHz, APB1 50 MHz, APB2 100 MHz, PLLQ 50 MHz (SDMMC).
 * Same as the ST BSP examples (SDRAM timing of the BSP assumes 200 MHz).
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = { 0 };
    RCC_ClkInitTypeDef clk = { 0 };

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 25;
    osc.PLL.PLLN       = 400;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = 8;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    if (HAL_PWREx_EnableOverDrive() != HAL_OK)
        Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_6) != HAL_OK)
        Error_Handler();
}

/**
 * MPU:
 *  region 0: SDRAM (frame buffers) 8 MiB  -> write-through, so the LTDC
 *            always sees the CPU writes without cache maintenance
 *  region 1: SRAM2 16 KiB (.dma_buffers) -> not cacheable (audio DMA)
 */
static void MPU_Config(void)
{
    MPU_Region_InitTypeDef r = { 0 };

    HAL_MPU_Disable();

    r.Enable           = MPU_REGION_ENABLE;
    r.Number           = MPU_REGION_NUMBER0;
    r.BaseAddress      = 0xC0000000u;
    r.Size             = MPU_REGION_SIZE_8MB;
    r.AccessPermission = MPU_REGION_FULL_ACCESS;
    r.TypeExtField     = MPU_TEX_LEVEL0;
    r.IsCacheable      = MPU_ACCESS_CACHEABLE;
    r.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    r.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    r.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
    r.SubRegionDisable = 0x00;
    HAL_MPU_ConfigRegion(&r);

    r.Number           = MPU_REGION_NUMBER1;
    r.BaseAddress      = 0x2004C000u;
    r.Size             = MPU_REGION_SIZE_16KB;
    r.TypeExtField     = MPU_TEX_LEVEL1;
    r.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    r.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    r.IsShareable      = MPU_ACCESS_SHAREABLE;
    HAL_MPU_ConfigRegion(&r);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void Error_Handler(void)
{
    __disable_irq();
    for (;;)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
