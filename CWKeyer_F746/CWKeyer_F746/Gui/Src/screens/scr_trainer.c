/**
 * @file    scr_trainer.c
 * @brief   Trainer menu (tiles) and the exercise screen ("drill").
 */
#include "app.h"
#include "keyer.h"
#include "morse.h"
#include "player.h"
#include "screens.h"
#include "settings.h"
#include "trainer.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

/* ===================================================================== */
/*  Menu                                                                 */
/* ===================================================================== */

static void menu_enter(void)
{
    app_set_mode(APP_MODE_IDLE);
}

static void menu_frame(void)
{
    if (ui_statusbar("Trainer", true))
        screen_back();

    const int cols = 3, rows = 2;
    const int w = (LCD_W - 2 * PAD - (cols - 1) * GAP) / cols;
    const int h = (CONTENT_H - 2 * PAD - (rows - 1) * GAP) / rows;

    static const struct { icon_id_t icon; const char *label; int mode; } tiles[6] =
    {
        { ICON_EAR,     "Listen (groups)", TRAINER_HEAR          },
        { ICON_SHUFFLE, "Send random",    TRAINER_GIVE_RANDOM   },
        { ICON_SORT_AZ, "Send A–Z",        TRAINER_GIVE_AZ       },
        { ICON_REPEAT,  "Listen + send",   TRAINER_LISTEN_REPEAT },
        { ICON_FILE,    "Text from SD",    -1                    },
        { ICON_ABC,     "Characters",      -2                    },
    };

    for (int i = 0; i < 6; i++)
    {
        rect_t r = R(PAD + (i % cols) * (w + GAP), CONTENT_Y + PAD + (i / cols) * (h + GAP), w, h);
        if (ui_tile(r, tiles[i].icon, tiles[i].label))
        {
            if (tiles[i].mode >= 0)
            {
                scr_drill_set_mode(tiles[i].mode);
                screen_goto(SCR_DRILL);
            }
            else if (tiles[i].mode == -1)
                screen_goto(SCR_SDTEXT);
            else
                screen_goto(SCR_LETTERS);
        }
    }
}

const screen_t scr_trainer = { menu_enter, menu_frame, NULL };

/* ===================================================================== */
/*  Exercise                                                             */
/* ===================================================================== */

static trainer_mode_t s_mode;

void scr_drill_set_mode(int m)
{
    s_mode = (trainer_mode_t)m;
}

static void drill_enter(void)
{
    app_set_mode(APP_MODE_TRAINER);
    trainer_start(s_mode);
}

static void drill_leave(void)
{
    trainer_stop();
}

/* "-.-" -> "– · –" for display */
static void pattern_pretty(const char *pat, char *out, size_t size)
{
    size_t n = 0;
    for (; *pat && n + 6 < size; pat++)
    {
        const char *s = (*pat == '.') ? "·" : "–";
        if (n) out[n++] = ' ';
        size_t l = strlen(s);
        memcpy(&out[n], s, l);
        n += l;
    }
    out[n] = '\0';
}

static void draw_big_letter(rect_t r, const trainer_state_t *st)
{
    ui_panel(r, COL_PANEL);

    if (st->mode == TRAINER_HEAR)
    {
        /* group: shown after playback or on request */
        const char *txt = (st->reveal) ? st->target : "?????";
        char hidden[12];
        if (!st->reveal)
        {
            size_t n = strlen(st->target);
            memset(hidden, '?', n);
            hidden[n] = 0;
            txt = hidden;
        }
        ui_label(R(r.x, r.y + 30, r.w, 60), &font_large, txt, st->reveal ? COL_TEXT : COL_MUTED, ALIGN_CENTER);
        gfx_text_in(&font_mono, R(r.x, r.y + 90, r.w, 30), st->reveal ? st->target : "", COL_MUTED, ALIGN_CENTER);
        ui_label(R(r.x, r.y + r.h - 50, r.w, 40), &font_small,
                 player_busy() ? "playing ..." : "Send E or T for the next group", COL_MUTED, ALIGN_CENTER);
        return;
    }

    char ch[2] = { st->target[0], 0 };
    bool show = st->reveal || st->mode == TRAINER_GIVE_RANDOM || st->mode == TRAINER_GIVE_AZ;
    if (show)
    {
        ui_label(R(r.x, r.y + 4, r.w, r.h - 40), &font_huge, ch, COL_TEXT, ALIGN_CENTER);
        char pp[32];
        const char *pat = morse_encode(st->target[0]);
        pattern_pretty(pat ? pat : "", pp, sizeof pp);
        if (st->feedback != FB_NONE || st->mode == TRAINER_LISTEN_REPEAT)
            ui_label(R(r.x, r.y + r.h - 40, r.w, 30), &font_large, pp, COL_MUTED, ALIGN_CENTER);
    }
    else
    {
        ui_label(R(r.x, r.y + 4, r.w, r.h - 40), &font_huge, "?", COL_MUTED, ALIGN_CENTER);
        ui_label(R(r.x, r.y + r.h - 40, r.w, 30), &font_small, "listen and send it back", COL_MUTED, ALIGN_CENTER);
    }
}

static void drill_frame(void)
{
    const trainer_state_t *st = trainer_state();
    if (ui_statusbar(trainer_mode_name(st->mode), true))
        screen_back();

    const int side_w = 156;
    rect_t big = R(PAD, CONTENT_Y + PAD, LCD_W - 2 * PAD - GAP - side_w, CONTENT_H - 2 * PAD);
    draw_big_letter(big, st);

    int x = big.x + big.w + GAP;
    int y = CONTENT_Y + PAD;

    /* feedback */
    rect_t fb = R(x, y, side_w, 58);
    if (st->feedback == FB_CORRECT)
    {
        gfx_rrect(fb, RADIUS, COL_OK_BG, COL_OK_BORDER, 1);
        gfx_icon(ICON_CHECK, ICON_M, fb.x + 12, fb.y + 15, COL_OK_TEXT);
        gfx_text(&font_bold, fb.x + 48, fb.y + 20, "Correct", COL_OK_TEXT);
    }
    else if (st->feedback == FB_WRONG)
    {
        char buf[24];
        gfx_rrect(fb, RADIUS, COL_ERR_BG, COL_ERR_BORDER, 1);
        gfx_icon(ICON_X, ICON_M, fb.x + 12, fb.y + 15, COL_ERR_TEXT);
        snprintf(buf, sizeof buf, "Wrong: %c", st->answer ? st->answer : '?');
        gfx_text(&font_bold, fb.x + 48, fb.y + 20, buf, COL_ERR_TEXT);
    }
    else
    {
        gfx_rrect(fb, RADIUS, COL_BG, COL_BORDER, 1);
        ui_label(fb, &font_small, st->mode == TRAINER_HEAR ? "listen" : "use the paddle", COL_MUTED, ALIGN_CENTER);
    }
    y += 58 + 8;

    /* current input */
    char pat[MORSE_MAX_ELEMENTS + 1];
    keyer_get_pattern(pat, sizeof pat);
    gfx_text(&font_small, x, y, "Input", COL_MUTED);
    y += 18;
    gfx_rrect(R(x, y, side_w, 32), 6, COL_TEXTAREA, COL_BORDER, 1);
    gfx_text_in(&font_mono, R(x + 8, y, side_w - 16, 32), pat, COL_RX, ALIGN_LEFT);
    y += 32 + 8;

    /* score */
    char buf[32];
    gfx_text(&font_small, x, y, "Score", COL_MUTED);
    y += 16;
    snprintf(buf, sizeof buf, "%u", st->correct);
    int ex = gfx_text(&font_large, x, y, buf, COL_TEXT);
    unsigned pct = st->total ? (100u * st->correct) / st->total : 0;
    snprintf(buf, sizeof buf, "/ %u  ·  %u %%", st->total, pct);
    gfx_text(&font_small, ex + 6, y + 10, buf, COL_MUTED);

    /* buttons */
    int bh = 44, bw = (side_w - 2 * 6) / 3;
    int by = CONTENT_Y + CONTENT_H - PAD - bh;
    if (ui_button(R(x, by, bw, bh), st->reveal ? ICON_EYE_OFF : ICON_EYE, NULL, 0))
        trainer_toggle_reveal();
    if (ui_button(R(x + bw + 6, by, bw, bh), ICON_REPEAT, NULL, 0))
        trainer_repeat();
    if (ui_button(R(x + 2 * (bw + 6), by, bw, bh), ICON_SKIP_FWD, NULL, 0))
        trainer_next();
}

const screen_t scr_drill = { drill_enter, drill_frame, drill_leave };
