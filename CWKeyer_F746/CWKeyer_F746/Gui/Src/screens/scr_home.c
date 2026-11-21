/**
 * @file    scr_home.c
 * @brief   Start screen: six large icon tiles (replaces the ESP main menu).
 */
#include "app.h"
#include "screens.h"
#include "ui.h"

static void enter(void)
{
    app_set_mode(APP_MODE_IDLE);
}

static void frame(void)
{
    ui_statusbar("CW Keyer " APP_VERSION, false);

    const int cols = 3, rows = 2;
    const int w = (LCD_W - 2 * PAD - (cols - 1) * GAP) / cols;
    const int h = (CONTENT_H - 2 * PAD - (rows - 1) * GAP) / rows;

    static const struct { icon_id_t icon; const char *label; screen_id_t to; } tiles[6] =
    {
        { ICON_KEY,    "Keyer",         SCR_KEYER    },
        { ICON_WAVE,   "Monitor",       SCR_MONITOR  },
        { ICON_SCHOOL, "Trainer",       SCR_TRAINER  },
        { ICON_FILE,   "SD Texts",     SCR_SDTEXT   },
        { ICON_FLOPPY, "Memory",       SCR_MEMORY   },
        { ICON_GEAR,   "Settings",     SCR_SETTINGS },
    };

    for (int i = 0; i < 6; i++)
    {
        rect_t r = R(PAD + (i % cols) * (w + GAP), CONTENT_Y + PAD + (i / cols) * (h + GAP), w, h);
        if (ui_tile(r, tiles[i].icon, tiles[i].label))
            screen_goto(tiles[i].to);
    }
}

const screen_t scr_home = { enter, frame, NULL };
