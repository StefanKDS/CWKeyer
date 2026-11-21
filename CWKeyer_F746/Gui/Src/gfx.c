/**
 * @file    gfx.c
 */
#include "gfx.h"
#include <string.h>

static uint16_t *s_fb;
static rect_t    s_clip = { 0, 0, LCD_W, LCD_H };

/* ------------------------------------------------------------------ */

rect_t rect_inset(rect_t r, int d)
{
    return R(r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d);
}

void gfx_begin(uint16_t *fb)
{
    s_fb = fb;
    gfx_reset_clip();
}

void gfx_set_clip(rect_t r)
{
    int x0 = r.x < 0 ? 0 : r.x, y0 = r.y < 0 ? 0 : r.y;
    int x1 = r.x + r.w > LCD_W ? LCD_W : r.x + r.w;
    int y1 = r.y + r.h > LCD_H ? LCD_H : r.y + r.h;
    s_clip = R(x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
}

void gfx_reset_clip(void)
{
    s_clip = R(0, 0, LCD_W, LCD_H);
}

color_t gfx_blend(color_t fg, color_t bg, uint8_t a)
{
    if (a >= 250) return fg;
    if (a <= 5)   return bg;
    /* blend in 565 with 5 bit alpha (classic trick) */
    uint32_t a5 = ((uint32_t)a + 4u) >> 3;
    uint32_t f = ((uint32_t)fg | ((uint32_t)fg << 16)) & 0x07E0F81Fu;
    uint32_t b = ((uint32_t)bg | ((uint32_t)bg << 16)) & 0x07E0F81Fu;
    uint32_t r = ((((f - b) * a5) >> 5) + b) & 0x07E0F81Fu;
    return (color_t)(r | (r >> 16));
}

static inline void px(int x, int y, color_t c)
{
    if (x >= s_clip.x && y >= s_clip.y && x < s_clip.x + s_clip.w && y < s_clip.y + s_clip.h)
        s_fb[y * LCD_W + x] = c;
}

static inline void px_a(int x, int y, color_t c, uint8_t a)
{
    if (x >= s_clip.x && y >= s_clip.y && x < s_clip.x + s_clip.w && y < s_clip.y + s_clip.h)
    {
        uint16_t *p = &s_fb[y * LCD_W + x];
        *p = gfx_blend(c, *p, a);
    }
}

void gfx_fill(rect_t r, color_t c)
{
    int x0 = r.x > s_clip.x ? r.x : s_clip.x;
    int y0 = r.y > s_clip.y ? r.y : s_clip.y;
    int x1 = r.x + r.w < s_clip.x + s_clip.w ? r.x + r.w : s_clip.x + s_clip.w;
    int y1 = r.y + r.h < s_clip.y + s_clip.h ? r.y + r.h : s_clip.y + s_clip.h;
    if (x1 <= x0 || y1 <= y0) return;

    uint32_t c2 = (uint32_t)c | ((uint32_t)c << 16);
    for (int y = y0; y < y1; y++)
    {
        uint16_t *p = &s_fb[y * LCD_W + x0];
        int n = x1 - x0;
        if (((uintptr_t)p & 2u) && n) { *p++ = c; n--; }
        uint32_t *p32 = (uint32_t *)(void *)p;
        for (; n >= 2; n -= 2) *p32++ = c2;
        if (n) *(uint16_t *)(void *)p32 = c;
    }
}

void gfx_hline(int x, int y, int w, color_t c) { gfx_fill(R(x, y, w, 1), c); }
void gfx_vline(int x, int y, int h, color_t c) { gfx_fill(R(x, y, 1, h), c); }

/* coverage of a pixel (px,py) for a circle quadrant, 0..255 (4x4 supersampling) */
static uint8_t circle_cov(float dx, float dy, float r)
{
    int n = 0;
    float r2 = r * r;
    for (int sy = 0; sy < 4; sy++)
        for (int sx = 0; sx < 4; sx++)
        {
            float x = dx + (sx + 0.5f) / 4.0f, y = dy + (sy + 0.5f) / 4.0f;
            if (x * x + y * y <= r2) n++;
        }
    return (uint8_t)(n * 255 / 16);
}

void gfx_fill_rrect(rect_t r, int rad, color_t c)
{
    if (rad * 2 > r.w) rad = r.w / 2;
    if (rad * 2 > r.h) rad = r.h / 2;
    if (rad <= 0) { gfx_fill(r, c); return; }

    gfx_fill(R(r.x, r.y + rad, r.w, r.h - 2 * rad), c);
    gfx_fill(R(r.x + rad, r.y, r.w - 2 * rad, rad), c);
    gfx_fill(R(r.x + rad, r.y + r.h - rad, r.w - 2 * rad, rad), c);

    /* corners */
    for (int y = 0; y < rad; y++)
        for (int x = 0; x < rad; x++)
        {
            /* distance from corner centre, pixel (x,y) in the top-left corner */
            uint8_t a = circle_cov((float)(rad - x - 1), (float)(rad - y - 1), (float)rad);
            if (!a) continue;
            px_a(r.x + x,               r.y + y,               c, a);
            px_a(r.x + r.w - 1 - x,     r.y + y,               c, a);
            px_a(r.x + x,               r.y + r.h - 1 - y,     c, a);
            px_a(r.x + r.w - 1 - x,     r.y + r.h - 1 - y,     c, a);
        }
}

void gfx_rrect(rect_t r, int rad, color_t fill, color_t border, int bw)
{
    if (bw > 0)
    {
        gfx_fill_rrect(r, rad, border);
        gfx_fill_rrect(rect_inset(r, bw), rad > bw ? rad - bw : 0, fill);
    }
    else
    {
        gfx_fill_rrect(r, rad, fill);
    }
}

void gfx_circle(int cx, int cy, int r, color_t c)
{
    gfx_fill_rrect(R(cx - r, cy - r, 2 * r, 2 * r), r, c);
}

/* ------------------------------------------------------------------ */
/*  Text                                                              */
/* ------------------------------------------------------------------ */

uint32_t utf8_next(const char **ps)
{
    const uint8_t *s = (const uint8_t *)*ps;
    uint32_t c = *s++;
    if (c >= 0xC0 && c < 0xE0 && (*s & 0xC0) == 0x80)
    {
        c = ((c & 0x1Fu) << 6) | (*s++ & 0x3Fu);
    }
    else if (c >= 0xE0 && c < 0xF0 && (s[0] & 0xC0) == 0x80 && (s[1] & 0xC0) == 0x80)
    {
        c = ((c & 0x0Fu) << 12) | ((uint32_t)(s[0] & 0x3Fu) << 6) | (s[1] & 0x3Fu);
        s += 2;
    }
    *ps = (const char *)s;
    return c;
}

static const glyph_t *find_glyph(const font_t *f, uint32_t cp)
{
    int lo = 0, hi = f->count - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        if (f->glyphs[mid].cp == cp) return &f->glyphs[mid];
        if (f->glyphs[mid].cp < cp) lo = mid + 1; else hi = mid - 1;
    }
    return (cp != '?') ? find_glyph(f, '?') : NULL;
}

int gfx_text_width_n(const font_t *f, const char *s, int n)
{
    int w = 0;
    const char *end = s + n;
    while (*s && s < end)
    {
        const glyph_t *g = find_glyph(f, utf8_next(&s));
        if (g) w += g->adv;
    }
    return w;
}

int gfx_text_width(const font_t *f, const char *s)
{
    return gfx_text_width_n(f, s, 0x7FFF);
}

static void draw_glyph(const font_t *f, const glyph_t *g, int x, int y, color_t c)
{
    int stride = (g->w + 1) / 2;
    const uint8_t *bm = &f->bitmap[g->off];
    int gx = x + g->xo, gy = y + f->ascent + g->yo;

    /* trivial reject */
    if (gx >= s_clip.x + s_clip.w || gy >= s_clip.y + s_clip.h ||
        gx + g->w <= s_clip.x || gy + g->h <= s_clip.y)
        return;

    for (int row = 0; row < g->h; row++)
    {
        const uint8_t *line = &bm[row * stride];
        for (int col = 0; col < g->w; col++)
        {
            uint8_t v = (col & 1) ? (line[col >> 1] & 0x0F) : (line[col >> 1] >> 4);
            if (v)
                px_a(gx + col, gy + row, c, (uint8_t)(v * 17));
        }
    }
}

int gfx_text_n(const font_t *f, int x, int y, const char *s, int n, color_t c)
{
    const char *end = s + n;
    while (*s && s < end)
    {
        const glyph_t *g = find_glyph(f, utf8_next(&s));
        if (!g) continue;
        if (g->w) draw_glyph(f, g, x, y, c);
        x += g->adv;
    }
    return x;
}

int gfx_text(const font_t *f, int x, int y, const char *s, color_t c)
{
    return gfx_text_n(f, x, y, s, 0x7FFF, c);
}

void gfx_text_in(const font_t *f, rect_t r, const char *s, color_t c, align_t a)
{
    int w = gfx_text_width(f, s);
    int x = r.x;
    if (a == ALIGN_CENTER) x = r.x + (r.w - w) / 2;
    else if (a == ALIGN_RIGHT) x = r.x + r.w - w;
    int y = r.y + (r.h - f->height) / 2;
    gfx_text(f, x, y, s, c);
}

/* ------------------------------------------------------------------ */
/*  Icons                                                             */
/* ------------------------------------------------------------------ */

int gfx_icon_px(icon_size_t sz)
{
    return g_icons[0][sz].size;
}

void gfx_icon(icon_id_t id, icon_size_t sz, int x, int y, color_t c)
{
    if (id >= ICON_COUNT || sz >= ICON_SIZES) return;
    const icon_bitmap_t *ic = &g_icons[id][sz];
    int stride = (ic->size + 1) / 2;
    for (int row = 0; row < ic->size; row++)
        for (int col = 0; col < ic->size; col++)
        {
            uint8_t b = ic->data[row * stride + (col >> 1)];
            uint8_t v = (col & 1) ? (b & 0x0F) : (b >> 4);
            if (v)
                px_a(x + col, y + row, c, (uint8_t)(v * 17));
        }
}

void gfx_icon_center(icon_id_t id, icon_size_t sz, rect_t r, color_t c)
{
    int s = gfx_icon_px(sz);
    gfx_icon(id, sz, r.x + (r.w - s) / 2, r.y + (r.h - s) / 2, c);
}
