/**
 * @file    scr_monitor.c
 * @brief   Monitor: decodes CW from the codec line input.
 */
#include "app.h"
#include "cw_decoder.h"
#include "screens.h"
#include "settings.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>

static bool s_frozen;

static void enter(void)
{
    s_frozen = false;
    app_set_mode(APP_MODE_MONITOR);
}

/* level 0..1 full scale -> 0..1 on a -70..0 dB scale */
static float db_scale(float v)
{
    if (v < 1e-5f) return 0.0f;
    float db = 20.0f * log10f(v);
    float f = (db + 70.0f) / 70.0f;
    return f < 0 ? 0 : (f > 1 ? 1 : f);
}

static void frame(void)
{
    if (ui_statusbar("Monitor", true))
        screen_back();

    cw_decoder_status_t st;
    cw_decoder_get_status(&st);
    textlog_t *log = app_monitor_log();

    const int bottom_h = 44;
    const int meter_h  = 22;
    int text_h = CONTENT_H - 2 * PAD - 2 * GAP - bottom_h - meter_h;
    ui_textbox(R(PAD, CONTENT_Y + PAD, LCD_W - 2 * PAD, text_h), log->buf, log->len, COL_RX, -1, !s_frozen);

    /* ---- signal meter ---- */
    int y = CONTENT_Y + PAD + text_h + GAP;
    gfx_text(&font_small, PAD, y + 3, "Level", COL_MUTED);
    ui_meter(R(PAD + 48, y + 6, LCD_W - 2 * PAD - 48 - 30, 12), db_scale(st.level), db_scale(st.threshold), st.tone);
    gfx_circle(LCD_W - PAD - 10, y + 12, 8, st.tone ? COL_GREEN : COL_BORDER);

    /* ---- info + buttons ---- */
    y += meter_h + GAP;
    char buf[24];
    int x = PAD;
    snprintf(buf, sizeof buf, "%u Hz", g_settings.dec_freq_hz);
    gfx_rrect(R(x, y, 120, bottom_h), 6, COL_PANEL, COL_BORDER, 1);
    gfx_text(&font_small, x + 10, y + 13, "Tone", COL_MUTED);
    gfx_text(&font_bold, x + 42, y + 12, buf, COL_TEXT);
    x += 120 + 6;
    snprintf(buf, sizeof buf, "%u WPM", st.wpm);
    gfx_rrect(R(x, y, 136, bottom_h), 6, COL_PANEL, COL_BORDER, 1);
    gfx_text(&font_small, x + 10, y + 13, "Decoded", COL_MUTED);
    gfx_text(&font_bold, x + 66, y + 12, buf, COL_TEXT);
    x += 136 + 6;

    int bw = (LCD_W - PAD - x - 2 * 6) / 3;
    if (ui_button(R(x, y, bw, bottom_h), ICON_ADJUST, NULL, 0))
    {
        scr_settings_set_tab(SETTINGS_TAB_DECODER);
        screen_goto(SCR_SETTINGS);
    }
    x += bw + 6;
    if (ui_button(R(x, y, bw, bottom_h), ICON_ERASER, NULL, 0))
        textlog_clear(log);
    x += bw + 6;
    if (ui_button(R(x, y, bw, bottom_h), s_frozen ? ICON_PLAY : ICON_PAUSE, NULL, s_frozen ? UI_ACCENT : 0))
    {
        s_frozen = !s_frozen;
        cw_decoder_enable(!s_frozen);
    }
}

const screen_t scr_monitor = { enter, frame, NULL };
