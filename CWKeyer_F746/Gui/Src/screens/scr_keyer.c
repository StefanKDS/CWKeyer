/**
 * @file    scr_keyer.c
 * @brief   Keyer: paddle text, memories M1..M3, speed +/-.
 */
#include "app.h"
#include "keyer.h"
#include "morse.h"
#include "player.h"
#include "port.h"
#include "screens.h"
#include "settings.h"
#include "ui.h"
#include <stdio.h>

static void enter(void)
{
    app_set_mode(APP_MODE_KEYER);
}

static void frame(void)
{
    if (ui_statusbar("Keyer", true))
        screen_back();

    const int right_w = 116;
    const int left_w  = LCD_W - 2 * PAD - GAP - right_w;
    const int btn_h   = 46;
    const int text_h  = CONTENT_H - 2 * PAD - GAP - btn_h;

    /* ---- text of the paddle input ---- */
    textlog_t *log = app_keyer_log();
    ui_textbox(R(PAD, CONTENT_Y + PAD, left_w, text_h), log->buf, log->len, COL_RX, -1, true);

    /* ---- memory buttons ---- */
    int y = CONTENT_Y + PAD + text_h + GAP;
    int small = 46;
    int mw = (left_w - 2 * small - 4 * 6) / 3;
    int x = PAD;
    for (int i = 0; i < MEM_COUNT; i++)
    {
        char lbl[4] = { 'M', (char)('1' + i), 0, 0 };
        if (ui_button(R(x, y, mw, btn_h), ICON_PLAY, lbl, g_settings.mem[i][0] ? 0 : UI_DISABLED))
            app_play_memory(i);
        x += mw + 6;
    }
    if (ui_button(R(x, y, small, btn_h), ICON_STOP, NULL, player_busy() ? UI_ACCENT : 0))
        app_stop_playback();
    x += small + 6;
    if (ui_button(R(x, y, small, btn_h), ICON_ERASER, NULL, 0))
    {
        textlog_clear(log);
        keyer_clear();
    }

    /* ---- speed ---- */
    int rx = LCD_W - PAD - right_w;
    int ry = CONTENT_Y + PAD;
    ui_label(R(rx, ry, right_w, 18), &font_small, "Tempo", COL_MUTED, ALIGN_CENTER);
    ry += 20;
    int v = g_settings.wpm;
    if (ui_button(R(rx, ry, right_w, 46), ICON_PLUS, NULL, UI_REPEAT | UI_ICON_M) && v < 50) v++;
    ry += 46 + 4;
    char buf[8];
    snprintf(buf, sizeof buf, "%d", v);
    int tw = gfx_text_width(&font_large, buf);
    gfx_text(&font_large, rx + (right_w - tw) / 2 - 16, ry + 2, buf, COL_TEXT);
    gfx_text(&font_small, rx + (right_w + tw) / 2 - 12, ry + 12, "WPM", COL_MUTED);
    ry += 36;
    if (ui_button(R(rx, ry, right_w, 46), ICON_MINUS, NULL, UI_REPEAT | UI_ICON_M) && v > 5) v--;
    ry += 46 + 10;

    if (v != g_settings.wpm)
    {
        /* keep Farnsworth distance like the ESP version (fwpm follows wpm) */
        int diff = g_settings.wpm - g_settings.fwpm;
        g_settings.wpm  = (uint8_t)v;
        g_settings.fwpm = (uint8_t)(v - diff < 5 ? 5 : v - diff);
        app_apply_settings();
        settings_mark_dirty();
    }

    /* ---- paddle indicators ---- */
    bool dit = port_paddle_dit(), dah = port_paddle_dah();
    if (g_settings.paddle_swap) { bool t = dit; dit = dah; dah = t; }
    gfx_circle(rx + 20, ry + 8, 7, dit ? COL_ACCENT : COL_BORDER);
    gfx_text(&font_small, rx + 32, ry, "Dit", COL_MUTED);
    gfx_circle(rx + 70, ry + 8, 7, dah ? COL_ACCENT : COL_BORDER);
    gfx_text(&font_small, rx + 82, ry, "Dah", COL_MUTED);
    ry += 22;

    char pat[MORSE_MAX_ELEMENTS + 1];
    keyer_get_pattern(pat, sizeof pat);
    ui_label(R(rx, ry, right_w, 22), &font_mono, pat, COL_ACCENT, ALIGN_CENTER);
}

const screen_t scr_keyer = { enter, frame, NULL };
