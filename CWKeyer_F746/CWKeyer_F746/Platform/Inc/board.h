/**
 * @file    board.h
 * @brief   Pin assignment STM32F746G-DISCO (Arduino header CN4/CN7).
 *
 *   Signal        Arduino  MCU    Function
 *   ------------  -------  -----  --------------------------------------
 *   PADDLE_DIT    D2       PG6    input, pull-up, paddle contact to GND
 *   PADDLE_DAH    D3       PB4    input, pull-up, paddle contact to GND
 *   STRAIGHT_KEY  D4       PG7    input, pull-up, straight key to GND
 *   KEY_OUT       D5       PI0    output, high = key down (use a transistor
 *                                 or opto coupler to key the transmitter)
 *   GND           GND      -      common paddle ground
 *
 *   Audio: line in (blue jack CN11) -> WM8994 IN1 -> SAI2 block B
 *          headphone (green jack CN10) <- WM8994 HPOUT1 <- SAI2 block A
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f7xx_hal.h"

#define PADDLE_DIT_PORT     GPIOG
#define PADDLE_DIT_PIN      GPIO_PIN_6
#define PADDLE_DAH_PORT     GPIOB
#define PADDLE_DAH_PIN      GPIO_PIN_4
#define STRAIGHT_KEY_PORT   GPIOG
#define STRAIGHT_KEY_PIN    GPIO_PIN_7
#define KEY_OUT_PORT        GPIOI
#define KEY_OUT_PIN         GPIO_PIN_0

/* interrupt priorities (lower = more urgent) */
#define IRQ_PRIO_SYSTICK    1u   /* keyer / player time base 1 kHz */
#define IRQ_PRIO_AUDIO      2u   /* SAI DMA: decoder + sidetone    */
#define IRQ_PRIO_SD         5u
#define IRQ_PRIO_USB        6u   /* MSC: SD access inside the USB interrupt */

void board_gpio_init(void);
void board_tick_enable(void);   /* start calling app_tick_1ms()    */
void board_tick_1ms(void);      /* called from SysTick_Handler      */

#endif
