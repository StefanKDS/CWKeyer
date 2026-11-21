/**
 * @file    screens.c
 * @brief   Screen navigation with a small back stack.
 */
#include "screens.h"
#include "gfx.h"
#include "theme.h"

static const screen_t *const s_table[SCR_COUNT] =
{
    [SCR_HOME]     = &scr_home,
    [SCR_KEYER]    = &scr_keyer,
    [SCR_MONITOR]  = &scr_monitor,
    [SCR_TRAINER]  = &scr_trainer,
    [SCR_DRILL]    = &scr_drill,
    [SCR_SDTEXT]   = &scr_sdtext,
    [SCR_MEMORY]   = &scr_memory,
    [SCR_SETTINGS] = &scr_settings,
    [SCR_LETTERS]  = &scr_letters,
};

#define STACK_DEPTH 6
static screen_id_t s_stack[STACK_DEPTH];
static int         s_sp;
static screen_id_t s_cur = SCR_HOME;
static screen_id_t s_pending = SCR_COUNT;
static bool        s_pending_back;

static void do_switch(screen_id_t to)
{
    if (s_table[s_cur]->leave) s_table[s_cur]->leave();
    s_cur = to;
    if (s_table[s_cur]->enter) s_table[s_cur]->enter();
}

void screens_init(void)
{
    s_sp  = 0;
    s_cur = SCR_HOME;
    if (s_table[s_cur]->enter) s_table[s_cur]->enter();
}

/* navigation is deferred to the end of the frame so a screen never
   changes while its frame() is still drawing */
void screen_goto(screen_id_t id)
{
    s_pending      = id;
    s_pending_back = false;
}

void screen_back(void)
{
    s_pending_back = true;
    s_pending      = SCR_COUNT;
}

screen_id_t screen_current(void)
{
    return s_cur;
}

void screens_frame(void)
{
    gfx_fill(R(0, 0, LCD_W, LCD_H), COL_BG);
    s_table[s_cur]->frame();

    if (s_pending_back)
    {
        s_pending_back = false;
        do_switch(s_sp > 0 ? s_stack[--s_sp] : SCR_HOME);
    }
    else if (s_pending < SCR_COUNT)
    {
        screen_id_t to = s_pending;
        s_pending = SCR_COUNT;
        if (to == SCR_HOME)
            s_sp = 0;
        else if (s_sp < STACK_DEPTH)
            s_stack[s_sp++] = s_cur;
        do_switch(to);
    }
}
