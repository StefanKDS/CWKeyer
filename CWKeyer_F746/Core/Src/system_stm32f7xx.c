/**
 * @file    system_stm32f7xx.c
 * @brief   CMSIS system file (minimal): FPU enable, SystemCoreClock handling.
 *          The real clock setup is done in SystemClock_Config() (main.c).
 */
#include "stm32f7xx.h"

#if !defined(HSE_VALUE)
#define HSE_VALUE 25000000U
#endif
#if !defined(HSI_VALUE)
#define HSI_VALUE 16000000U
#endif

uint32_t SystemCoreClock = 16000000U;
const uint8_t AHBPrescTable[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9 };
const uint8_t APBPrescTable[8]  = { 0, 0, 0, 0, 1, 2, 3, 4 };

void SystemInit(void)
{
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));   /* CP10, CP11 full access */
#endif
}

void SystemCoreClockUpdate(void)
{
    uint32_t pllm, pllvco, pllp, pllsource;

    switch (RCC->CFGR & RCC_CFGR_SWS)
    {
    case 0x00:  SystemCoreClock = HSI_VALUE; break;
    case 0x04:  SystemCoreClock = HSE_VALUE; break;
    case 0x08:
        pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) >> 22;
        pllm = RCC->PLLCFGR & RCC_PLLCFGR_PLLM;
        pllvco = ((pllsource ? HSE_VALUE : HSI_VALUE) / pllm) *
                 ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6);
        pllp = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLP) >> 16) + 1) * 2;
        SystemCoreClock = pllvco / pllp;
        break;
    default:    SystemCoreClock = HSI_VALUE; break;
    }
    SystemCoreClock >>= AHBPrescTable[(RCC->CFGR & RCC_CFGR_HPRE) >> 4];
}
