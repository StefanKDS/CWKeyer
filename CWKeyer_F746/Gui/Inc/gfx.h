/**
 * @file    gfx.h
 * @brief   Software renderer for a 480x272 RGB565 frame buffer.
 *
 * Anti-aliased text (4 bpp fonts), icons, rounded rectangles.
 * Hardware independent: renders into any uint16_t buffer.
 */
#ifndef GFX_H
#define GFX_H

#include <stdbool.h>
#include <stdint.h>
#include "fonts.h"
#include "icons.h"

#define LCD_W  480
#define LCD_H  272

typedef uint16_t color_t;

#define RGB565(r, g, b)  ((color_t)((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((b) >> 3)))

typedef struct
{
    int16_t x, y, w, h;
} rect_t;

typedef enum
{
    ALIGN_LEFT   = 0,
    ALIGN_CENTER = 1,
    ALIGN_RIGHT  = 2,
} align_t;

static inline rect_t R(int x, int y, int w, int h)
{
    rect_t r = { (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h };
    return r;
}

static inline bool rect_contains(rect_t r, int x, int y)
{
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

rect_t   rect_inset(rect_t r, int d);

void     gfx_begin(uint16_t *fb);
void     gfx_set_clip(rect_t r);
void     gfx_reset_clip(void);
color_t  gfx_blend(color_t fg, color_t bg, uint8_t alpha);   /* alpha 0..255 */

void     gfx_fill(rect_t r, color_t c);
void     gfx_fill_rrect(rect_t r, int radius, color_t c);
void     gfx_rrect(rect_t r, int radius, color_t fill, color_t border, int border_w);
void     gfx_circle(int cx, int cy, int r, color_t c);
void     gfx_hline(int x, int y, int w, color_t c);
void     gfx_vline(int x, int y, int h, color_t c);

int      gfx_text_width(const font_t *f, const char *s);
int      gfx_text_width_n(const font_t *f, const char *s, int n);
/** draws text, (x,y) = top-left of the line box, returns end x */
int      gfx_text(const font_t *f, int x, int y, const char *s, color_t c);
int      gfx_text_n(const font_t *f, int x, int y, const char *s, int n, color_t c);
void     gfx_text_in(const font_t *f, rect_t r, const char *s, color_t c, align_t a);

void     gfx_icon(icon_id_t id, icon_size_t sz, int x, int y, color_t c);
void     gfx_icon_center(icon_id_t id, icon_size_t sz, rect_t r, color_t c);
int      gfx_icon_px(icon_size_t sz);

/** decodes one UTF-8 code point, advances *s */
uint32_t utf8_next(const char **s);

#endif
