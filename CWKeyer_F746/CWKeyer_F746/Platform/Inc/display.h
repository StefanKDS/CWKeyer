/**
 * @file    display.h
 * @brief   LCD 480x272 RGB565 with double buffering in SDRAM.
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

bool      display_init(void);
/** buffer the GUI renders into. With rotate180 a separate canvas is used
 *  that display_swap() copies turned by 180 degrees into the back buffer. */
uint16_t *display_canvas(bool rotate180);
void      display_swap(bool rotate180);   /**< show the new frame at next VSYNC */

#endif
