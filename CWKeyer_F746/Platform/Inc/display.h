/**
 * @file    display.h
 * @brief   LCD 480x272 RGB565 with double buffering in SDRAM.
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

bool      display_init(void);
uint16_t *display_backbuffer(void);
void      display_swap(void);          /**< show back buffer at next VSYNC */

#endif
