/**
 * @file    scr_settings.c
 * @brief   All settings (ESP: encoder setup menu + web page) on one screen
 *          with a tab bar on the left.
 */
#include "app.h"
#include "player.h"
#include "screens.h"
#include "settings.h"
#include "text_source.h"
#include "ui.h"
#include <stdio.h>

static settings_tab_t s_tab = SETTINGS_TAB_SPEED;
static bool           s_changed;

void scr_settings_set_tab(settings_tab_t tab)
{
    s_tab = tab;
}

static void enter(void)
{
    app_set_mode(APP_MODE_IDLE);
    s_changed = false;
}

static void leave(void)
{
    if (s_changed)
        settings_save_now();
}

/* helpers that write back into the settings struct */
static void slider_u8(rect_t r, const char *label, uint8_t *v, int min, int max, int step, const char *unit)
{
    int t = *v;
    if (ui_slider(r, label, &t, min, max, step, unit))
    {
        *v = (uint8_t)t;
        s_changed = true;
        app_apply_settings();
    }
}

static void slider_u16(rect_t r, const char *label, uint16_t *v, int min, int max, int step, const char *unit)
{
    int t = *v;
    if (ui_slider(r, label, &t, min, max, step, unit))
    {
        *v = (uint16_t)t;
        s_changed = true;
        app_apply_settings();
    }
}

static void toggle_u8(rect_t r, const char *label, uint8_t *v)
{
    bool b = *v != 0;
    if (ui_toggle(r, label, &b))
    {
        *v = b ? 1 : 0;
        s_changed = true;
        app_apply_settings();
    }
}

static void tab_speed(rect_t c)
{
    const int h = 54;
    settings_t *s = &g_settings;
    slider_u8(R(c.x, c.y, c.w, h), "Character speed", &s->wpm, 5, 50, 1, "WPM");
    if (s->fwpm > s->wpm) s->fwpm = s->wpm;
    slider_u8(R(c.x, c.y + h + 6, c.w, h), "Farnsworth (spacing)", &s->fwpm, 5, s->wpm, 1, "WPM");
    slider_u8(R(c.x, c.y + 2 * (h + 6), c.w, h), "Trainer group length", &s->group_len, 1, 10, 1, "chars");

    char buf[64];
    snprintf(buf, sizeof buf, "Dit %u ms  ·  letter gap %u ms",
             1200u / s->wpm, 3u * 1200u / s->fwpm);
    gfx_text(&font_small, c.x, c.y + 3 * (h + 6) + 6, buf, COL_MUTED);
}

static void tab_audio(rect_t c)
{
    const int h = 54;
    settings_t *s = &g_settings;
    slider_u16(R(c.x, c.y, c.w, h), "Sidetone pitch", &s->tone_hz, 400, 1000, 10, "Hz");
    slider_u8(R(c.x, c.y + h + 4, c.w, h), "Volume", &s->volume, 0, 100, 5, "%");
    toggle_u8(R(c.x, c.y + 2 * (h + 4), c.w, 40), "Sidetone on", &s->sidetone_on);
    toggle_u8(R(c.x, c.y + 2 * (h + 4) + 44, c.w, 40), "Hear line in on monitor", &s->monitor_passthru);
}

static void tab_keyer(rect_t c)
{
    settings_t *s = &g_settings;
    static const char *const modes[2] = { "Iambic + memory", "Iambic" };
    int m = s->keyer_mode;
    gfx_text(&font_small, c.x, c.y, "Paddle mode", COL_MUTED);
    if (ui_segment(R(c.x, c.y + 18, c.w, 38), modes, 2, &m))
    {
        s->keyer_mode = (uint8_t)m;
        s_changed = true;
        app_apply_settings();
    }
    int y = c.y + 18 + 38 + 6;
    toggle_u8(R(c.x, y, c.w, 40), "Swap paddles (dit ↔ dah)", &s->paddle_swap);
    y += 44;
    toggle_u8(R(c.x, y, c.w, 40), "Key TX output D5", &s->key_out_on);
    y += 46;
    slider_u8(R(c.x, y, c.w, 54), "Debounce", &s->debounce_ms, 1, 30, 1, "ms");
}

static void tab_decoder(rect_t c)
{
    const int h = 54;
    settings_t *s = &g_settings;
    slider_u16(R(c.x, c.y, c.w, h), "Decoder tone", &s->dec_freq_hz, 400, 1000, 10, "Hz");
    slider_u8(R(c.x, c.y + h + 6, c.w, h), "Squelch", &s->dec_squelch, 0, 100, 5, "%");
    int g = s->line_gain_db;
    if (ui_slider(R(c.x, c.y + 2 * (h + 6), c.w, h), "Input gain", &g, -16, 30, 1, "dB"))
    {
        s->line_gain_db = (int8_t)g;
        s_changed = true;
        app_apply_settings();
    }
    gfx_text(&font_small, c.x, c.y + 3 * (h + 6) + 4, "Line in (blue jack) · 16 kHz · Goertzel", COL_MUTED);
}

static void tab_letters(rect_t c)
{
    ui_label(R(c.x, c.y, c.w, 24), &font_normal, "Trainer characters:", COL_MUTED, ALIGN_LEFT);
    gfx_set_clip(R(c.x, c.y + 28, c.w, 60));
    ui_label(R(c.x, c.y + 28, c.w, 30), &font_mono, g_settings.letters, COL_RX, ALIGN_LEFT);
    gfx_reset_clip();
    if (ui_button(R(c.x, c.y + 80, c.w, 48), ICON_ABC, "Select characters", UI_ICON_M))
        screen_goto(SCR_LETTERS);
    if (ui_button(R(c.x, c.y + 136, c.w, 48), ICON_FLOPPY, "Memory texts M1–M3", UI_ICON_M))
        screen_goto(SCR_MEMORY);
}

static void tab_info(rect_t c)
{
    char buf[64];
    int y = c.y;
    const int lh = 20;
    snprintf(buf, sizeof buf, "CW Keyer %s", APP_VERSION);
    int ex = gfx_text(&font_bold, c.x, y, buf, COL_TEXT);
    gfx_text(&font_small, ex + 10, y + 2, "STM32F746G-DISCO", COL_MUTED);
    y += lh + 6;
    gfx_text(&font_small, c.x, y, "Paddle: D2 = dit, D3 = dah, GND", COL_MUTED); y += lh;
    gfx_text(&font_small, c.x, y, "Straight key: D4   ·   TX out: D5", COL_MUTED); y += lh;
    gfx_text(&font_small, c.x, y, "Audio: line in (blue) / headphones (green)", COL_MUTED); y += lh;
    text_source_status_t ts = text_source_status();
    if (ts == TS_USB)
        snprintf(buf, sizeof buf, "SD card: USB drive on (SD texts screen)");
    else
        snprintf(buf, sizeof buf, "SD card: %s, %d texts  ·  power: USB FS",
                 ts == TS_READY ? "ready" : ts == TS_NO_CARD ? "no card" : "error", text_source_count());
    gfx_text(&font_small, c.x, y, buf, COL_MUTED); y += lh + 2;

    toggle_u8(R(c.x, y, c.w, 38), "Rotate display 180°", &g_settings.rotate180);
    y += 42;

    if (ui_button(R(c.x, y, (c.w - 6) / 2, 42), ICON_REFRESH, "Rescan SD", ts == TS_USB ? UI_DISABLED : 0))
        text_source_scan();
    if (ui_button(R(c.x + (c.w + 6) / 2, y, (c.w - 6) / 2, 42), ICON_X, "Defaults", 0))
    {
        settings_defaults(&g_settings);
        app_apply_settings();
        s_changed = true;
    }
}

static void frame(void)
{
    if (ui_statusbar("Settings", true))
        screen_back();

    static const struct { icon_id_t icon; const char *label; } tabs[SETTINGS_TAB_COUNT] =
    {
        { ICON_GAUGE,  "Speed"   },
        { ICON_VOLUME, "Audio"   },
        { ICON_KEY,    "Keyer"   },
        { ICON_WAVE,   "Decoder" },
        { ICON_ABC,    "Texts"   },
        { ICON_INFO,   "Info"    },
    };
    const int side_w = 132;
    const int tab_h = CONTENT_H / SETTINGS_TAB_COUNT;
    gfx_fill(R(0, CONTENT_Y, side_w, CONTENT_H), COL_SIDEBAR);
    for (int i = 0; i < SETTINGS_TAB_COUNT; i++)
        if (ui_tab(R(0, CONTENT_Y + i * tab_h, side_w, tab_h), tabs[i].icon, tabs[i].label, (int)s_tab == i))
            s_tab = (settings_tab_t)i;

    rect_t c = R(side_w + 14, CONTENT_Y + 10, LCD_W - side_w - 28, CONTENT_H - 18);
    switch (s_tab)
    {
    case SETTINGS_TAB_SPEED:   tab_speed(c);   break;
    case SETTINGS_TAB_AUDIO:   tab_audio(c);   break;
    case SETTINGS_TAB_KEYER:   tab_keyer(c);   break;
    case SETTINGS_TAB_DECODER: tab_decoder(c); break;
    case SETTINGS_TAB_LETTERS: tab_letters(c); break;
    case SETTINGS_TAB_INFO:    tab_info(c);    break;
    default: break;
    }
}

const screen_t scr_settings = { enter, frame, leave };
