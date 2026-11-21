/**
 * @file    ui.h
 * @brief   Small immediate-mode widget toolkit for the touch screen.
 *
 * Every frame the active screen calls the widget functions; each call
 * draws the widget and returns the user interaction (e.g. "clicked").
 * No widget objects or callbacks are needed, which keeps the screen
 * code short and easy to follow.
 */
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>
#include "gfx.h"
#include "theme.h"

typedef struct
{
    bool    down;
    int16_t x, y;
} touch_t;

/* button flags */
#define UI_ACCENT     0x01u   /**< accent border (selected)            */
#define UI_FILLED     0x02u   /**< accent background (primary/on)      */
#define UI_DISABLED   0x04u
#define UI_ICON_M     0x08u   /**< medium icon instead of small        */
#define UI_ICON_L     0x10u
#define UI_REPEAT     0x20u   /**< auto repeat while held (+/- keys)   */
#define UI_NOFRAME    0x40u

#define NO_ICON       ICON_COUNT

void ui_frame_begin(const touch_t *t, uint32_t now_ms);
void ui_frame_end(void);

bool ui_button(rect_t r, icon_id_t icon, const char *label, uint32_t flags);
bool ui_tile(rect_t r, icon_id_t icon, const char *label);
bool ui_tab(rect_t r, icon_id_t icon, const char *label, bool selected);
bool ui_list_item(rect_t r, icon_id_t icon, const char *label, const char *right, bool selected);
bool ui_slider(rect_t r, const char *label, int *value, int min, int max, int step, const char *unit);
bool ui_toggle(rect_t r, const char *label, bool *value);
bool ui_segment(rect_t r, const char *const *options, int count, int *selected);
bool ui_key(rect_t r, const char *label, bool on);     /**< toggle key (letters) */

void ui_panel(rect_t r, color_t fill);
void ui_label(rect_t r, const font_t *f, const char *text, color_t c, align_t a);
void ui_meter(rect_t r, float level, float threshold, bool on);
void ui_progress(rect_t r, float fraction);

/** word-wrapped text area using the mono font.
 *  highlight >= 0: that character is highlighted and kept visible,
 *  otherwise the end of the text is shown. */
void ui_textbox(rect_t r, const char *text, int len, color_t c, int highlight, bool cursor);

/** status bar, returns true when the back arrow was tapped */
bool ui_statusbar(const char *title, bool back);

#endif
