/**
 * @file    stm32f7xx_it.c
 * @brief   Interrupt handlers.
 */
#include "main.h"
#include "board.h"
#include "stm32f7xx_it.h"

extern SAI_HandleTypeDef hsai_tx;
extern SAI_HandleTypeDef hsai_rx;
extern SD_HandleTypeDef  uSdHandle;     /* stm32746g_discovery_sd.c */

void NMI_Handler(void)        { }
void HardFault_Handler(void)  { for (;;) { } }
void MemManage_Handler(void)  { for (;;) { } }
void BusFault_Handler(void)   { for (;;) { } }
void UsageFault_Handler(void) { for (;;) { } }
void SVC_Handler(void)        { }
void DebugMon_Handler(void)   { }
void PendSV_Handler(void)     { }

void SysTick_Handler(void)
{
    HAL_IncTick();
    board_tick_1ms();
}

/* audio: SAI2 block A (TX) / block B (RX) */
void DMA2_Stream4_IRQHandler(void) { HAL_DMA_IRQHandler(hsai_tx.hdmatx); }
void DMA2_Stream7_IRQHandler(void) { HAL_DMA_IRQHandler(hsai_rx.hdmarx); }

/* SD card (the BSP enables these interrupts; reads are done in polling mode) */
void DMA2_Stream3_IRQHandler(void) { if (uSdHandle.hdmarx) HAL_DMA_IRQHandler(uSdHandle.hdmarx); }
void DMA2_Stream6_IRQHandler(void) { if (uSdHandle.hdmatx) HAL_DMA_IRQHandler(uSdHandle.hdmatx); }
void SDMMC1_IRQHandler(void)       { HAL_SD_IRQHandler(&uSdHandle); }
