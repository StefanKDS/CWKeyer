/**
 * @file    scr_sdtext.c
 * @brief   Practice texts from SD card: file list, playback with highlight.
 */
#include "app.h"
#include "player.h"
#include "port.h"
#include "screens.h"
#include "text_source.h"
#include "ui.h"
#include <stdio.h>

static int  s_sel   = -1;
static int  s_first = 0;
static bool s_hide;            /* hide text while playing (copy practice) */
static int  s_text_first = -1;

static void enter(void)
{
    app_set_mode(APP_MODE_SDTEXT);
    if (text_source_status() != TS_READY)
        text_source_scan();
    s_sel = text_source_loaded_index();
    s_text_first = -1;
}

static void leave(void)
{
    app_stop_playback();
}

static void select_file(int i)
{
    app_stop_playback();
    if (text_source_load(i))
    {
        s_sel = i;
        s_text_first = -1;
    }
}

static void frame(void)
{
    if (ui_statusbar("SD texts", true))
        screen_back();

    const int list_w = 172;
    const int item_h = 34;
    const int nav_h  = 40;
    int y0 = CONTENT_Y + PAD;
    int list_h = CONTENT_H - 2 * PAD - nav_h - 6;
    int visible = list_h / item_h;
    int count = text_source_count();

    /* ---- file list ---- */
    if (text_source_status() != TS_READY)
    {
        if (text_source_status() == TS_USB)
        {
            bool host = port_usb_host_connected();
            gfx_icon_center(ICON_USB, ICON_L, R(PAD, y0, list_w, 52), host ? COL_ACCENT : COL_MUTED);
            ui_label(R(PAD, y0 + 52, list_w, 22), &font_normal, host ? "Connected to PC" : "Waiting for PC ...",
                     COL_TEXT, ALIGN_CENTER);
            ui_label(R(PAD, y0 + 76, list_w, 18), &font_small, "Eject the drive on the PC,", COL_MUTED, ALIGN_CENTER);
            ui_label(R(PAD, y0 + 93, list_w, 18), &font_small, "then disconnect here", COL_MUTED, ALIGN_CENTER);
            //uint32_t rd, wr, er;
            //char st[48];
            //port_usb_stats(&rd, &wr, &er);
            //snprintf(st, sizeof st, "R %lu  W %lu  Err %lu", (unsigned long)rd, (unsigned long)wr, (unsigned long)er);
            //ui_label(R(PAD, y0 + 166, list_w, 18), &font_small, st, er ? COL_ERR_TEXT : COL_MUTED, ALIGN_CENTER);
            if (ui_button(R(PAD, y0 + 118, list_w, 44), ICON_USB, "Disconnect", UI_FILLED))
            {
                port_usb_drive(false);
                text_source_scan();
                s_sel = -1;
                s_first = 0;
            }
        }
        else
        {
            const char *msg = text_source_status() == TS_NO_CARD ? "No SD card" : "SD error (FAT16/32?)";
            ui_label(R(PAD, y0, list_w, 40), &font_normal, msg, COL_MUTED, ALIGN_CENTER);
        }
    }
    else if (count == 0)
    {
        ui_label(R(PAD, y0, list_w, 40), &font_normal, "No *.txt files", COL_MUTED, ALIGN_CENTER);
    }
    for (int i = 0; i < visible && s_first + i < count && text_source_status() == TS_READY; i++)
    {
        int idx = s_first + i;
        if (ui_list_item(R(PAD, y0 + i * item_h, list_w, item_h - 2), ICON_FILE,
                         text_source_name(idx), NULL, idx == s_sel))
            select_file(idx);
    }
    int ny = y0 + list_h + 6;
    int nb = (list_w - 3 * 5) / 4;
    bool usb = text_source_status() == TS_USB;
    if (ui_button(R(PAD, ny, nb, nav_h), ICON_SKIP_BACK, NULL, s_first > 0 && !usb ? 0 : UI_DISABLED))
        s_first = s_first - visible < 0 ? 0 : s_first - visible;
    if (ui_button(R(PAD + nb + 5, ny, nb, nav_h), ICON_SKIP_FWD, NULL, s_first + visible < count && !usb ? 0 : UI_DISABLED))
        s_first += visible;
    if (ui_button(R(PAD + 2 * (nb + 5), ny, nb, nav_h), ICON_REFRESH, NULL, usb ? UI_DISABLED : 0))
    {
        app_stop_playback();
        text_source_scan();
        s_sel = -1;
        s_first = 0;
    }
    /* USB drive: the PC gets the card, the keyer releases it */
    if (ui_button(R(PAD + 3 * (nb + 5), ny, list_w - 3 * (nb + 5), nav_h), ICON_USB, NULL, usb ? UI_ACCENT : 0) && !usb)
    {
        port_usb_drive(true);
        text_source_scan();      /* -> TS_USB */
    }

    /* ---- text + player ---- */
    int x = PAD + list_w + GAP;
    int w = LCD_W - PAD - x;
    int bh = 44;
    int text_h = CONTENT_H - 2 * PAD - bh - 20;
    bool loaded = s_sel >= 0 && text_source_loaded_index() == s_sel && text_source_len() > 0;
    bool playing = player_busy();
    int pos = playing ? (int)player_pos() : -1;

    if (s_hide && playing)
    {
        gfx_rrect(R(x, y0, w, text_h), 6, COL_TEXTAREA, COL_BORDER, 1);
        gfx_icon_center(ICON_EAR, ICON_L, R(x, y0, w, text_h - 30), COL_BORDER);
        ui_label(R(x, y0 + text_h - 50, w, 30), &font_small, "Text hidden – copy by ear", COL_MUTED, ALIGN_CENTER);
    }
    else if (loaded)
    {
        ui_textbox_scroll(R(x, y0, w, text_h), text_source_text(), (int)text_source_len(), COL_RX,
                          pos < 0 ? 0 : pos, &s_text_first);
    }
    else
    {
        gfx_rrect(R(x, y0, w, text_h), 6, COL_TEXTAREA, COL_BORDER, 1);
        ui_label(R(x, y0, w, text_h), &font_small, "Select a file on the left", COL_MUTED, ALIGN_CENTER);
    }

    int py = y0 + text_h + 8;
    float frac = (loaded && text_source_len()) ? (float)(pos < 0 ? 0 : pos) / (float)text_source_len() : 0.0f;
    ui_progress(R(x, py, w, 6), frac);

    int by = py + 12;
    int b = (w - 3 * 6) / 4;
    uint32_t dis = loaded ? 0 : UI_DISABLED;
    if (ui_button(R(x, by, b, bh), ICON_SKIP_BACK, NULL, dis))
    {
        s_text_first = -1;
        app_play_text(text_source_text(), text_source_len(), false);
    }
    if (ui_button(R(x + (b + 6), by, b, bh), (playing && !player_paused()) ? ICON_PAUSE : ICON_PLAY, NULL,
                  dis | ((playing && !player_paused()) ? UI_ACCENT : 0)))
    {
        if (!playing)
        {
            s_text_first = -1;
            app_play_text(text_source_text(), text_source_len(), false);
        }
        else
            player_pause(!player_paused());
    }
    if (ui_button(R(x + 2 * (b + 6), by, b, bh), ICON_STOP, NULL, dis))
        app_stop_playback();
    if (ui_button(R(x + 3 * (b + 6), by, b, bh), s_hide ? ICON_EYE_OFF : ICON_EYE, NULL, s_hide ? UI_ACCENT : 0))
        s_hide = !s_hide;
}

const screen_t scr_sdtext = { enter, frame, leave };
