/**
 * @file    ui.c
 */
#include "ui.h"
#include "app.h"
#include "settings.h"
#include "text_source.h"
#include <stdio.h>
#include <string.h>

/* ---- input state ---------------------------------------------------- */
static touch_t  s_t, s_prev;
static bool     s_pressed;      /* down edge this frame   */
static bool     s_released;     /* up edge this frame     */
static int16_t  s_down_x, s_down_y;
static uint32_t s_now, s_down_time, s_last_repeat;
static int      s_id;           /* widget counter         */
static int      s_active = -1;  /* widget under the finger */

void ui_frame_begin(const touch_t *t, uint32_t now_ms)
{
    s_prev     = s_t;
    s_t        = *t;
    s_now      = now_ms;
    s_pressed  = s_t.down && !s_prev.down;
    s_released = !s_t.down && s_prev.down;
    if (s_pressed)
    {
        s_down_x    = s_t.x;
        s_down_y    = s_t.y;
        s_down_time = now_ms;
        s_last_repeat = now_ms;
        s_active    = -1;
    }
    if (!s_t.down && !s_released)
        s_active = -1;
    s_id = 0;
}

void ui_frame_end(void)
{
    if (s_released)
        s_active = -1;
}

/* Generic interaction: returns true when the widget was clicked.
 * *held reports if the widget is currently pressed. */
static bool interact(rect_t r, bool *held, bool repeat)
{
    int id = ++s_id;
    if (s_pressed && rect_contains(r, s_down_x, s_down_y))
        s_active = id;

    bool is_active = (s_active == id);
    bool inside    = rect_contains(r, s_t.down ? s_t.x : s_prev.x, s_t.down ? s_t.y : s_prev.y);
    if (held) *held = is_active && s_t.down && inside;

    if (!is_active)
        return false;

    if (repeat && s_t.down)
    {
        if (s_pressed) return true;
        if (s_now - s_down_time > 500u && s_now - s_last_repeat > 120u)
        {
            s_last_repeat = s_now;
            return true;
        }
        return false;
    }
    if (repeat)
        return false;
    return s_released && inside;
}

static icon_size_t icon_size(uint32_t flags)
{
    return (flags & UI_ICON_L) ? ICON_L : (flags & UI_ICON_M) ? ICON_M : ICON_S;
}

bool ui_button(rect_t r, icon_id_t icon, const char *label, uint32_t flags)
{
    bool held = false;
    bool clicked = interact(r, &held, (flags & UI_REPEAT) != 0);
    if (flags & UI_DISABLED) { held = false; clicked = false; }

    color_t bg = (flags & UI_FILLED) ? COL_ACCENT : (held ? COL_PANEL_DOWN : COL_PANEL);
    color_t bd = (flags & (UI_ACCENT | UI_FILLED)) || held ? COL_ACCENT : COL_BORDER;
    color_t fg = (flags & UI_FILLED) ? COL_ACCENT_DARK : COL_TEXT;
    color_t ic = (flags & UI_FILLED) ? COL_ACCENT_DARK : COL_ACCENT;
    if (flags & UI_DISABLED) { fg = COL_DISABLED; ic = COL_DISABLED; }

    if (!(flags & UI_NOFRAME))
        gfx_rrect(r, 6, bg, bd, 1);
    else if (held)
        gfx_fill_rrect(r, 6, COL_PANEL_DOWN);

    icon_size_t is = icon_size(flags);
    int iw = (icon != NO_ICON) ? gfx_icon_px(is) : 0;
    int tw = (label && *label) ? gfx_text_width(&font_normal, label) : 0;
    int gap = (iw && tw) ? 6 : 0;
    int x = r.x + (r.w - iw - gap - tw) / 2;

    if (iw)
        gfx_icon(icon, is, x, r.y + (r.h - iw) / 2, ic);
    if (tw)
        gfx_text(&font_normal, x + iw + gap, r.y + (r.h - font_normal.height) / 2, label, fg);
    return clicked;
}

bool ui_tile(rect_t r, icon_id_t icon, const char *label)
{
    bool held = false;
    bool clicked = interact(r, &held, false);
    gfx_rrect(r, RADIUS, held ? COL_PANEL_DOWN : COL_PANEL, held ? COL_ACCENT : COL_BORDER, 1);
    int is = gfx_icon_px(ICON_L);
    int total = is + 8 + font_normal.height;
    int y = r.y + (r.h - total) / 2;
    gfx_icon(icon, ICON_L, r.x + (r.w - is) / 2, y, COL_ACCENT);
    gfx_text_in(&font_normal, R(r.x, y + is + 8, r.w, font_normal.height), label, COL_TEXT, ALIGN_CENTER);
    return clicked;
}

bool ui_tab(rect_t r, icon_id_t icon, const char *label, bool selected)
{
    bool held = false;
    bool clicked = interact(r, &held, false);
    if (selected || held)
    {
        gfx_fill(r, selected ? COL_PANEL : COL_PANEL_DOWN);
        if (selected)
            gfx_fill(R(r.x, r.y, 3, r.h), COL_ACCENT);
    }
    int is = gfx_icon_px(ICON_S);
    gfx_icon(icon, ICON_S, r.x + 12, r.y + (r.h - is) / 2, selected ? COL_ACCENT : COL_MUTED);
    gfx_text(&font_normal, r.x + 12 + is + 8, r.y + (r.h - font_normal.height) / 2, label,
             selected ? COL_TEXT : COL_MUTED);
    return clicked;
}

bool ui_list_item(rect_t r, icon_id_t icon, const char *label, const char *right, bool selected)
{
    bool held = false;
    bool clicked = interact(r, &held, false);
    if (selected || held)
        gfx_fill_rrect(r, 4, selected ? COL_PANEL : COL_PANEL_DOWN);
    if (selected)
        gfx_fill(R(r.x, r.y + 4, 3, r.h - 8), COL_ACCENT);
    int x = r.x + 8;
    if (icon != NO_ICON)
    {
        gfx_icon(icon, ICON_S, x, r.y + (r.h - ICON_PX_S) / 2, selected ? COL_ACCENT : COL_MUTED);
        x += ICON_PX_S + 6;
    }
    int rw = right ? gfx_text_width(&font_small, right) + 8 : 0;
    gfx_set_clip(R(x, r.y, r.x + r.w - x - rw, r.h));
    gfx_text(&font_normal, x, r.y + (r.h - font_normal.height) / 2, label, selected ? COL_TEXT : COL_MUTED);
    gfx_reset_clip();
    if (right)
        gfx_text_in(&font_small, R(r.x, r.y, r.w - 6, r.h), right, COL_MUTED, ALIGN_RIGHT);
    return clicked;
}

bool ui_slider(rect_t r, const char *label, int *value, int min, int max, int step, const char *unit)
{
    bool held = false;
    rect_t hit = R(r.x, r.y, r.w, r.h);
    (void)interact(hit, &held, false);
    bool changed = false;

    int track_y = r.y + r.h - 14;
    int tx0 = r.x + 10, tx1 = r.x + r.w - 10;

    if (s_active == s_id && s_t.down)
    {
        int x = s_t.x < tx0 ? tx0 : (s_t.x > tx1 ? tx1 : s_t.x);
        int v = min + (int)(((long)(x - tx0) * (max - min) + (tx1 - tx0) / 2) / (tx1 - tx0));
        v = min + ((v - min + step / 2) / step) * step;
        if (v > max) v = max;
        if (v != *value) { *value = v; changed = true; }
    }

    char buf[24];
    snprintf(buf, sizeof buf, "%d %s", *value, unit ? unit : "");
    gfx_text(&font_normal, r.x, r.y, label, COL_TEXT);
    gfx_text_in(&font_bold, R(r.x, r.y, r.w, font_normal.height), buf, COL_ACCENT, ALIGN_RIGHT);

    int pos = tx0 + (int)((long)(*value - min) * (tx1 - tx0) / (max > min ? max - min : 1));
    gfx_fill_rrect(R(tx0, track_y - 3, tx1 - tx0, 6), 3, COL_BORDER);
    gfx_fill_rrect(R(tx0, track_y - 3, pos - tx0 + 3, 6), 3, COL_ACCENT);
    gfx_circle(pos, track_y, held ? 11 : 9, COL_TEXT);
    return changed;
}

bool ui_toggle(rect_t r, const char *label, bool *value)
{
    bool held = false;
    bool clicked = interact(r, &held, false);
    if (clicked) *value = !*value;
    if (held) gfx_fill_rrect(r, 6, COL_PANEL_DOWN);

    gfx_text(&font_normal, r.x + 4, r.y + (r.h - font_normal.height) / 2, label, COL_TEXT);
    rect_t sw = R(r.x + r.w - 52, r.y + (r.h - 26) / 2, 48, 26);
    gfx_fill_rrect(sw, 13, *value ? COL_ACCENT : COL_BORDER);
    int kx = *value ? sw.x + sw.w - 13 : sw.x + 13;
    gfx_circle(kx, sw.y + 13, 10, *value ? COL_TEXT : COL_MUTED);
    return clicked;
}

bool ui_segment(rect_t r, const char *const *opt, int n, int *sel)
{
    bool changed = false;
    int w = r.w / n;
    gfx_rrect(r, 6, COL_PANEL, COL_BORDER, 1);
    for (int i = 0; i < n; i++)
    {
        rect_t c = R(r.x + i * w, r.y, (i == n - 1) ? r.w - i * w : w, r.h);
        bool held = false;
        if (interact(c, &held, false) && *sel != i) { *sel = i; changed = true; }
        if (*sel == i)
            gfx_fill_rrect(rect_inset(c, 2), 5, COL_ACCENT);
        else if (held)
            gfx_fill_rrect(rect_inset(c, 2), 5, COL_PANEL_DOWN);
        gfx_text_in(&font_normal, c, opt[i], *sel == i ? COL_ACCENT_DARK : COL_TEXT, ALIGN_CENTER);
    }
    return changed;
}

bool ui_key(rect_t r, const char *label, bool on)
{
    bool held = false;
    bool clicked = interact(r, &held, false);
    gfx_rrect(r, 5, on ? COL_ACCENT : (held ? COL_PANEL_DOWN : COL_BG), on ? COL_ACCENT : COL_BORDER, 1);
    gfx_text_in(&font_bold, r, label, on ? COL_ACCENT_DARK : COL_TEXT, ALIGN_CENTER);
    return clicked;
}

void ui_panel(rect_t r, color_t fill)
{
    gfx_fill_rrect(r, RADIUS, fill);
}

void ui_label(rect_t r, const font_t *f, const char *text, color_t c, align_t a)
{
    gfx_text_in(f, r, text, c, a);
}

void ui_meter(rect_t r, float level, float thr, bool on)
{
    if (level > 1.0f) level = 1.0f;
    if (thr > 1.0f) thr = 1.0f;
    gfx_fill_rrect(r, 3, COL_BORDER);
    int w = (int)(level * (float)r.w);
    if (w > 0)
        gfx_fill_rrect(R(r.x, r.y, w < 6 ? 6 : w, r.h), 3, on ? COL_GREEN : COL_MUTED);
    int tx = r.x + (int)(thr * (float)r.w);
    gfx_fill(R(tx - 1, r.y - 3, 2, r.h + 6), COL_ACCENT);
}

void ui_progress(rect_t r, float f)
{
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    gfx_fill_rrect(r, r.h / 2, COL_BORDER);
    int w = (int)(f * (float)r.w);
    if (w >= r.h)
        gfx_fill_rrect(R(r.x, r.y, w, r.h), r.h / 2, COL_ACCENT);
}

/* ---- word wrapped text ------------------------------------------- */

#define MAX_LINES 1200
static uint16_t s_line_start[MAX_LINES];

static int wrap_lines(const char *t, int len, int cols)
{
    int n = 0, pos = 0;
    while (pos < len && n < MAX_LINES)
    {
        s_line_start[n++] = (uint16_t)pos;
        if (len - pos <= cols) break;
        int brk = pos + cols;                   /* break at last space */
        int sp = brk;
        while (sp > pos && t[sp] != ' ') sp--;
        pos = (sp > pos) ? sp + 1 : brk;
    }
    if (n == 0) s_line_start[n++] = 0;
    return n;
}

void ui_textbox(rect_t r, const char *text, int len, color_t c, int hl, bool cursor)
{
    gfx_rrect(r, 6, COL_TEXTAREA, COL_BORDER, 1);
    const font_t *f = &font_mono;
    int cw = f->glyphs[0].adv ? f->glyphs[0].adv : 12;  /* mono: all equal (space) */
    int lh = f->height + 2;
    rect_t in = rect_inset(r, 8);
    int cols = in.w / cw;
    int rows = in.h / lh;
    if (cols < 1 || rows < 1) return;

    int n = wrap_lines(text, len, cols);
    int first;
    if (hl >= 0)
    {
        int l = 0;
        while (l + 1 < n && s_line_start[l + 1] <= hl) l++;
        first = l - 1;                               /* one line of context */
        if (first > n - rows) first = n - rows;
    }
    else
    {
        first = n - rows;
    }
    if (first < 0) first = 0;

    gfx_set_clip(in);
    for (int i = 0; i < rows && first + i < n; i++)
    {
        int s = s_line_start[first + i];
        int e = (first + i + 1 < n) ? s_line_start[first + i + 1] : len;
        int y = in.y + i * lh;
        if (hl >= s && hl < e)
        {
            int x = in.x + (hl - s) * cw;
            gfx_fill_rrect(R(x - 1, y, cw + 2, lh), 3, COL_ACCENT);
            gfx_text_n(f, in.x, y, &text[s], hl - s, c);
            char ch[2] = { text[hl], 0 };
            gfx_text(f, x, y, ch, COL_ACCENT_DARK);
            gfx_text_n(f, x + cw, y, &text[hl + 1], e - hl - 1, hl >= 0 ? COL_MUTED : c);
        }
        else
        {
            gfx_text_n(f, in.x, y, &text[s], e - s, (hl >= 0 && s > hl) ? COL_MUTED : c);
        }
        if (cursor && first + i == n - 1 && ((s_now / 500u) & 1u))
        {
            int x = in.x + (len - s) * cw;
            if (len - s >= cols) x = in.x + cols * cw - 2;
            gfx_fill(R(x + 1, y + 2, 2, f->height - 2), COL_ACCENT);
        }
    }
    gfx_reset_clip();
}

/* ---- status bar --------------------------------------------------- */

bool ui_statusbar(const char *title, bool back)
{
    bool pressed = false;
    gfx_fill(R(0, 0, LCD_W, BAR_H), COL_BAR);
    int x = 10;
    if (back)
    {
        rect_t br = R(0, 0, 56, BAR_H);
        bool held = false;
        pressed = interact(br, &held, false);
        if (held) gfx_fill(br, COL_PANEL_DOWN);
        gfx_icon(ICON_BACK, ICON_S, 10, (BAR_H - ICON_PX_S) / 2, COL_TEXT);
        x = 40;
    }
    else
    {
        gfx_icon(ICON_ANTENNA, ICON_S, 8, (BAR_H - ICON_PX_S) / 2, COL_ACCENT);
        x = 34;
    }
    gfx_text(&font_bold, x, (BAR_H - font_bold.height) / 2, title, COL_TEXT);

    /* right side, from right to left */
    int rx = LCD_W - 8;
    int cy = (BAR_H - ICON_PX_S) / 2;
    bool sd = text_source_status() == TS_READY;
    rx -= ICON_PX_S;
    gfx_icon(ICON_SD, ICON_S, rx, cy, sd ? COL_GREEN : COL_DISABLED);
    rx -= ICON_PX_S + 8;
    gfx_icon(g_settings.sidetone_on ? ICON_VOLUME : ICON_VOLUME_OFF, ICON_S, rx, cy, COL_MUTED);
    if (g_settings.key_out_on)
    {
        rx -= ICON_PX_S + 8;
        gfx_icon(ICON_ANTENNA, ICON_S, rx, cy, app_tone_active() ? COL_ERR_BORDER : COL_MUTED);
    }
    rx -= 16;
    gfx_circle(rx + 5, BAR_H / 2, 5, app_tone_active() ? COL_ACCENT : COL_BORDER);

    char buf[16];
    int ty = (BAR_H - font_small.height) / 2;
    if (g_settings.fwpm < g_settings.wpm)
    {
        snprintf(buf, sizeof buf, "%u", g_settings.fwpm);
        rx -= 10 + gfx_text_width(&font_bold, buf);
        gfx_text(&font_bold, rx, ty - 1, buf, COL_ACCENT);
        rx -= 4 + gfx_text_width(&font_small, "FW");
        gfx_text(&font_small, rx, ty, "FW", COL_MUTED);
    }
    snprintf(buf, sizeof buf, "%u", g_settings.wpm);
    rx -= 10 + gfx_text_width(&font_bold, buf);
    gfx_text(&font_bold, rx, ty - 1, buf, COL_ACCENT);
    rx -= 4 + gfx_text_width(&font_small, "WPM");
    gfx_text(&font_small, rx, ty, "WPM", COL_MUTED);
    return pressed;
}
