/**
 * @file    scr_letters.c
 * @brief   Character selection for the trainer (was the web page
 *          "Trainer Letters" of the ESP version) incl. Koch lessons.
 */
#include "app.h"
#include "screens.h"
#include "settings.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

static const char s_all[]  = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,/?=";
static const char s_koch[] = "KMRSUAPTLOWI.NJEF0Y,VG5/Q9ZH38B?427C1D6X";   /* LCWO order */

static char s_sel[LETTERS_LEN + 1];
static int  s_koch_lesson;

static void enter(void)
{
    strcpy(s_sel, g_settings.letters);
    s_koch_lesson = 0;
}

static void leave(void)
{
    /* store in canonical order */
    char out[LETTERS_LEN + 1];
    size_t n = 0;
    for (const char *p = s_all; *p && n < LETTERS_LEN; p++)
        if (strchr(s_sel, *p)) out[n++] = *p;
    out[n] = 0;
    if (strcmp(out, g_settings.letters) != 0)
    {
        strcpy(g_settings.letters, out);
        settings_save_now();
    }
}

static void toggle(char c)
{
    char *p = strchr(s_sel, c);
    size_t n = strlen(s_sel);
    if (p)
        memmove(p, p + 1, n - (size_t)(p - s_sel));
    else if (n < LETTERS_LEN)
    {
        s_sel[n] = c;
        s_sel[n + 1] = 0;
    }
}

static void frame(void)
{
    if (ui_statusbar("Zeichenauswahl Trainer", true))
        screen_back();

    const int per_row = 14, kg = 3;
    const int kw = (LCD_W - 2 * PAD - (per_row - 1) * kg) / per_row;
    const int kh = 38;
    int n = (int)strlen(s_all);
    for (int i = 0; i < n; i++)
    {
        char lbl[2] = { s_all[i], 0 };
        rect_t r = R(PAD + (i % per_row) * (kw + kg), CONTENT_Y + PAD + (i / per_row) * (kh + kg), kw, kh);
        if (ui_key(r, lbl, strchr(s_sel, s_all[i]) != NULL))
            toggle(s_all[i]);
    }

    int y = CONTENT_Y + PAD + 3 * (kh + kg) + 6;
    char buf[48];
    snprintf(buf, sizeof buf, "%u Zeichen gewählt", (unsigned)strlen(s_sel));
    gfx_text(&font_small, PAD, y, buf, COL_MUTED);
    y += 22;

    const int bh = 48;
    int bw = (LCD_W - 2 * PAD - 4 * 6) / 5;
    int x = PAD;
    if (ui_button(R(x, y, bw, bh), NO_ICON, "Alle", 0))
        strcpy(s_sel, s_all);
    x += bw + 6;
    if (ui_button(R(x, y, bw, bh), NO_ICON, "Keine", 0))
        s_sel[0] = 0;
    x += bw + 6;

    /* Koch lesson: [-] Lektion n [+] */
    int kx = x, kwid = 2 * bw + 6;
    gfx_rrect(R(kx, y, kwid, bh), 6, COL_PANEL, COL_BORDER, 1);
    bool koch_changed = false;
    if (ui_button(R(kx, y, 44, bh), ICON_MINUS, NULL, UI_NOFRAME) && s_koch_lesson > 1)
    { s_koch_lesson--; koch_changed = true; }
    if (ui_button(R(kx + kwid - 44, y, 44, bh), ICON_PLUS, NULL, UI_NOFRAME) && s_koch_lesson < (int)strlen(s_koch) - 1)
    { s_koch_lesson = s_koch_lesson < 1 ? 1 : s_koch_lesson + 1; koch_changed = true; }
    snprintf(buf, sizeof buf, s_koch_lesson ? "Koch %d" : "Koch", s_koch_lesson);
    ui_label(R(kx + 44, y, kwid - 88, bh), &font_normal, buf, COL_TEXT, ALIGN_CENTER);
    if (koch_changed)
    {
        /* lesson n = first n+1 characters */
        int cnt = s_koch_lesson + 1;
        memcpy(s_sel, s_koch, (size_t)cnt);
        s_sel[cnt] = 0;
    }
    x += kwid + 6;
    if (ui_button(R(x, y, LCD_W - PAD - x, bh), ICON_CHECK, "OK", UI_FILLED))
        screen_back();
}

const screen_t scr_letters = { enter, frame, leave };
