/**
 * @file    screens.h
 * @brief   Screen manager and the individual screens.
 *
 * Each screen has an optional enter()/leave() and a frame() function,
 * frame() is called ~30 times per second and draws the whole screen.
 */
#ifndef SCREENS_H
#define SCREENS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum
{
    SCR_HOME = 0,
    SCR_KEYER,
    SCR_MONITOR,
    SCR_TRAINER,
    SCR_DRILL,
    SCR_SDTEXT,
    SCR_MEMORY,
    SCR_SETTINGS,
    SCR_LETTERS,
    SCR_COUNT
} screen_id_t;

typedef struct
{
    void (*enter)(void);
    void (*frame)(void);
    void (*leave)(void);
} screen_t;

void        screens_init(void);
void        screens_frame(void);
void        screen_goto(screen_id_t id);
void        screen_back(void);
screen_id_t screen_current(void);

/* screen implementations (screens/scr_*.c) */
extern const screen_t scr_home, scr_keyer, scr_monitor, scr_trainer, scr_drill,
                      scr_sdtext, scr_memory, scr_settings, scr_letters;

/* shared helpers */
typedef enum
{
    SETTINGS_TAB_SPEED = 0,
    SETTINGS_TAB_AUDIO,
    SETTINGS_TAB_KEYER,
    SETTINGS_TAB_DECODER,
    SETTINGS_TAB_LETTERS,
    SETTINGS_TAB_INFO,
    SETTINGS_TAB_COUNT
} settings_tab_t;

void scr_drill_set_mode(int trainer_mode);
void scr_settings_set_tab(settings_tab_t tab);

#endif
