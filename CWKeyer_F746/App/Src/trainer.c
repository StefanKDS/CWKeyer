/**
 * @file    trainer.c
 * @brief   Trainer logic. The ESP version blocked for 1 s after an answer
 *          (delay(1000)); here a non-blocking feedback timer is used.
 */
#include "trainer.h"
#include "player.h"
#include "port.h"
#include "settings.h"
#include <string.h>

#define FEEDBACK_MS  1000u

static trainer_state_t s_st;
static bool            s_active;
static int8_t          s_az_pos = -1;
static uint32_t        s_fb_time;
static char            s_play_buf[16];

static const char *letters(void)
{
    if (g_settings.letters[0] == '\0')
        return "ABCDEFGHIJKLMNOPQRSTUVWXYZ";      /* fallback as in ESP version */
    return g_settings.letters;
}

static char random_letter(void)
{
    const char *l = letters();
    uint32_t n = (uint32_t)strlen(l);
    return l[port_random() % n];
}

static void play(const char *txt)
{
    strncpy(s_play_buf, txt, sizeof(s_play_buf) - 1);
    s_play_buf[sizeof(s_play_buf) - 1] = '\0';
    player_start(s_play_buf, (uint32_t)strlen(s_play_buf));
}

static void new_question(void)
{
    s_st.feedback = FB_NONE;
    s_st.answer   = 0;

    switch (s_st.mode)
    {
    case TRAINER_HEAR:
    {
        uint8_t n = g_settings.group_len;
        if (n > sizeof(s_st.target) - 1) n = sizeof(s_st.target) - 1;
        for (uint8_t i = 0; i < n; i++)
            s_st.target[i] = random_letter();
        s_st.target[n] = '\0';
        s_st.reveal    = false;
        play(s_st.target);
        break;
    }
    case TRAINER_GIVE_RANDOM:
        s_st.target[0] = random_letter();
        s_st.target[1] = '\0';
        s_st.reveal    = true;
        break;

    case TRAINER_GIVE_AZ:
        if (++s_az_pos > 25) s_az_pos = 0;
        s_st.target[0] = (char)('A' + s_az_pos);
        s_st.target[1] = '\0';
        s_st.reveal    = true;
        break;

    case TRAINER_LISTEN_REPEAT:
        s_st.target[0] = random_letter();
        s_st.target[1] = '\0';
        s_st.reveal    = false;
        play(s_st.target);
        break;

    default:
        break;
    }
}

void trainer_start(trainer_mode_t mode)
{
    memset(&s_st, 0, sizeof(s_st));
    s_st.mode = mode;
    s_az_pos  = -1;
    s_active  = true;
    new_question();
}

void trainer_stop(void)
{
    s_active = false;
    player_stop();
}

void trainer_next(void)
{
    if (s_active)
        new_question();
}

void trainer_repeat(void)
{
    if (s_active)
        play(s_st.target);
}

void trainer_toggle_reveal(void)
{
    s_st.reveal = !s_st.reveal;
}

void trainer_on_keyed_char(char c)
{
    if (!s_active || s_st.feedback != FB_NONE)
        return;

    if (s_st.mode == TRAINER_HEAR)
    {
        /* as in the ESP version: E or T continues with the next group */
        if (c == 'E' || c == 'T')
            new_question();
        return;
    }

    s_st.answer = c;
    s_st.total++;
    if (c == s_st.target[0])
    {
        s_st.correct++;
        s_st.feedback = FB_CORRECT;
    }
    else
    {
        s_st.feedback = FB_WRONG;
        play(s_st.target);                    /* play correct letter */
    }
    s_st.reveal = true;
    s_fb_time   = port_millis();
}

void trainer_poll(void)
{
    if (!s_active || s_st.feedback == FB_NONE)
        return;
    if (player_busy())
        s_fb_time = port_millis();            /* wait until correction is played */
    if ((uint32_t)(port_millis() - s_fb_time) >= FEEDBACK_MS)
        new_question();
}

const trainer_state_t *trainer_state(void)
{
    return &s_st;
}

const char *trainer_mode_name(trainer_mode_t m)
{
    static const char *const names[TRAINER_MODE_COUNT] =
    {
        "Hören", "Geben zufällig", "Geben A–Z", "Hören + Geben"
    };
    return (m < TRAINER_MODE_COUNT) ? names[m] : "";
}
