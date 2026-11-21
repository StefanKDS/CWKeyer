/**
 * @file    board.c
 * @brief   GPIO for paddles / key output and the 1 ms application tick.
 */
#include "board.h"
#include "app.h"

static volatile bool s_tick_on;

void board_gpio_init(void)
{
    GPIO_InitTypeDef g = { 0 };

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;

    g.Pin = PADDLE_DIT_PIN | STRAIGHT_KEY_PIN;
    HAL_GPIO_Init(GPIOG, &g);
    g.Pin = PADDLE_DAH_PIN;
    HAL_GPIO_Init(PADDLE_DAH_PORT, &g);

    HAL_GPIO_WritePin(KEY_OUT_PORT, KEY_OUT_PIN, GPIO_PIN_RESET);
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Pin  = KEY_OUT_PIN;
    HAL_GPIO_Init(KEY_OUT_PORT, &g);
}

void board_tick_enable(void)
{
    s_tick_on = true;
}

void board_tick_1ms(void)
{
    if (s_tick_on)
        app_tick_1ms();
}
