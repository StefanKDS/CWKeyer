/**
 * @file    gui.c
 */
#include "gui.h"
#include "gfx.h"
#include "screens.h"

void gui_init(void)
{
    screens_init();
}

void gui_render(uint16_t *fb, const touch_t *touch, uint32_t now_ms)
{
    gfx_begin(fb);
    ui_frame_begin(touch, now_ms);
    screens_frame();
    ui_frame_end();
}
