/**
 * @file    gui.h
 * @brief   GUI entry points used by main().
 */
#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include "ui.h"

void gui_init(void);
/** renders one frame into fb using the given touch state */
void gui_render(uint16_t *fb, const touch_t *touch, uint32_t now_ms);

#endif
