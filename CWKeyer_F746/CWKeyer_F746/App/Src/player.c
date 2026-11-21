/**
 * @file    player.c
 * @brief   Port of startPlayback()/updatePlayback() with a 1 ms tick.
 */
#include "player.h"
#include "morse.h"
#include "port.h"

static event_queue_t     *s_q;
static morse_timing_t     s_t;

static const char        *s_text;
static uint32_t           s_len;
static volatile uint32_t  s_idx;        /* next character to load         */
static volatile uint32_t  s_cur;        /* character currently sent       */
static const char        *s_code;       /* pattern of current character   */
static uint8_t            s_code_idx;
static uint32_t           s_wait;       /* ms until next action           */
static uint32_t           s_tone_left;  /* ms of tone remaining           */
static volatile bool      s_playing;
static volatile bool      s_paused;
static volatile bool      s_tone;

void player_init(event_queue_t *q)
{
    s_q = q;
    morse_timing_calc(&s_t, 14, 14);
}

void player_set_timing(const morse_timing_t *t)
{
    port_irq_lock();
    s_t = *t;
    port_irq_unlock();
}

void player_start(const char *text, uint32_t len)
{
    port_irq_lock();
    s_text      = text;
    s_len       = len;
    s_idx       = 0;
    s_cur       = 0;
    s_code      = NULL;
    s_code_idx  = 0;
    s_wait      = 0;
    s_tone_left = 0;
    s_tone      = false;
    s_paused    = false;
    s_playing   = (text != NULL && len > 0);
    port_irq_unlock();
}

void player_stop(void)
{
    port_irq_lock();
    s_playing = false;
    s_tone    = false;
    port_irq_unlock();
}

void player_pause(bool pause) { s_paused = pause; }
bool player_busy(void)        { return s_playing; }
bool player_paused(void)      { return s_paused; }
uint32_t player_pos(void)     { return s_cur; }
bool player_tone(void)        { return s_tone; }

void player_tick(void)
{
    if (!s_playing)
        return;

    /* tone running? */
    if (s_tone_left)
    {
        if (--s_tone_left == 0)
            s_tone = false;
    }

    if (s_wait)
    {
        s_wait--;
        return;
    }
    if (s_paused)
        return;

    /* load next character */
    if (s_code == NULL)
    {
        while (s_idx < s_len)
        {
            char c = s_text[s_idx];
            s_cur  = s_idx++;
            if (c == ' ')
            {
                s_wait = s_t.word_extra;     /* letter gap already elapsed */
                return;
            }
            s_code = morse_encode(c);
            if (s_code != NULL)
                break;                       /* skip unsupported chars */
        }
        if (s_code == NULL)
        {
            s_playing = false;
            evq_push(s_q, EV_PLAY_DONE, 0);
            return;
        }
        s_code_idx = 0;
    }

    /* next element of the current character */
    char sym = s_code[s_code_idx];
    if (sym != '\0')
    {
        s_code_idx++;
        uint32_t len = (sym == MORSE_DOT) ? s_t.dit : s_t.dah;
        s_tone       = true;
        s_tone_left  = len;
        s_wait       = len + s_t.elem_gap - 1u;
    }
    else
    {
        s_code = NULL;                       /* character finished */
        s_wait = s_t.letter_extra - 1u;
    }
}
