/**
 * @file    scr_memory.c
 * @brief   Edit the text memories M1..M3 with an on-screen keyboard
 *          (was the web page "Memory" of the ESP version).
 */
#include "app.h"
#include "player.h"
#include "screens.h"
#include "settings.h"
#include "ui.h"
#include <string.h>

static int  s_mem;
static char s_edit[MEM_TEXT_LEN + 1];

static void load(int idx)
{
    s_mem = idx;
    strcpy(s_edit, g_settings.mem[idx]);
}

static void enter(void)
{
    app_set_mode(APP_MODE_IDLE);
    load(s_mem);
}

static void save(void)
{
    if (strcmp(g_settings.mem[s_mem], s_edit) != 0)
    {
        strcpy(g_settings.mem[s_mem], s_edit);
        settings_mark_dirty();
    }
}

static void leave(void)
{
    save();
    app_stop_playback();
}

static void frame(void)
{
    if (ui_statusbar("Edit memories", true))
        screen_back();

    int y = CONTENT_Y + 4;
    int x = PAD;
    for (int i = 0; i < MEM_COUNT; i++)
    {
        char lbl[3] = { 'M', (char)('1' + i), 0 };
        if (ui_button(R(x, y, 48, 42), NO_ICON, lbl, s_mem == i ? UI_FILLED : 0) && s_mem != i)
        {
            save();
            load(i);
        }
        x += 54;
    }
    ui_textbox(R(x, y, LCD_W - PAD - x, 42), s_edit, (int)strlen(s_edit), COL_RX, -1, true);
    y += 42 + 4;

    /* ---- keyboard ---- */
    static const char *const rows[4] =
    {
        "1234567890",
        "QWERTYUIOP",
        "ASDFGHJKL/",
        "ZXCVBNM?.,",
    };
    const int kh = 34, kg = 4;
    const int kw = (LCD_W - 2 * PAD - 9 * kg) / 10;
    size_t len = strlen(s_edit);
    for (int r = 0; r < 4; r++)
    {
        for (int c = 0; c < 10; c++)
        {
            char lbl[2] = { rows[r][c], 0 };
            if (ui_button(R(PAD + c * (kw + kg), y, kw, kh), NO_ICON, lbl, 0) && len < MEM_TEXT_LEN)
            {
                s_edit[len++] = rows[r][c];
                s_edit[len] = 0;
            }
        }
        y += kh + kg;
    }

    /* ---- bottom row ---- */
    int bw = kw + kg;
    x = PAD;
    if (ui_button(R(x, y, 2 * bw - kg, kh), ICON_BACKSPACE, NULL, UI_REPEAT) && len > 0)
        s_edit[--len] = 0;
    x += 2 * bw;
    if (ui_button(R(x, y, 4 * bw - kg, kh), NO_ICON, "Space", 0) && len < MEM_TEXT_LEN && len > 0 && s_edit[len - 1] != ' ')
    {
        s_edit[len++] = ' ';
        s_edit[len] = 0;
    }
    x += 4 * bw;
    if (ui_button(R(x, y, bw - kg, kh), ICON_ERASER, NULL, 0))
        s_edit[0] = 0;
    x += bw;
    if (ui_button(R(x, y, bw - kg, kh), player_busy() ? ICON_STOP : ICON_PLAY, NULL, 0))
    {
        if (player_busy())
            app_stop_playback();
        else
            app_play_text(s_edit, (uint32_t)strlen(s_edit), false);
    }
    x += bw;
    if (ui_button(R(x, y, LCD_W - PAD - x, kh), ICON_CHECK, "OK", UI_FILLED))
    {
        save();
        screen_back();
    }
}

const screen_t scr_memory = { enter, frame, leave };
