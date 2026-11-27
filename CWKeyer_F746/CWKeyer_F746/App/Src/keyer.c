/**
 * @file    keyer.c
 * @brief   Iambic keyer state machine, 1 ms time base.
 *
 * Behaviour of the ESP8266 version is kept:
 *  - paddle inputs are debounced
 *  - a short tap is stored ("pending") and sent after the running element
 *    (mode KEYER_IAMBIC_MEMORY)
 *  - squeezing both paddles alternates dit/dah
 *  - a character is complete after letter_extra ms of silence,
 *    a word gap is reported after a further word_extra ms.
 */
#include "keyer.h"
#include "morse.h"
#include "port.h"
#include "settings.h"
#include <string.h>

typedef enum { KS_IDLE = 0, KS_ELEMENT, KS_GAP } keyer_state_t;

typedef struct
{
    bool     raw, stable;
    uint8_t  cnt;           /* ms the raw level differs from stable */
} debounce_t;

static event_queue_t   *s_q;
static morse_timing_t   s_t;
static uint8_t          s_mode;
static bool             s_swap;
static uint8_t          s_debounce = 10;

static volatile keyer_state_t s_state;
static uint16_t         s_timer;         /* remaining ms in ELEMENT/GAP     */
static uint32_t         s_idle;          /* ms since last gap ended         */
static bool             s_pend_dit, s_pend_dah;
static bool             s_last_was_dit;
static bool             s_space_sent = true;
static volatile bool    s_tone;

static debounce_t       s_dit, s_dah, s_sk;
static bool             s_sk_down;
static uint32_t         s_sk_time;

static char             s_pattern[MORSE_MAX_ELEMENTS + 1];
static uint8_t          s_plen;

/* Returns true on a new stable "pressed" edge. */
static bool debounce(debounce_t *d, bool raw)
{
    if (raw == d->stable)
    {
        d->cnt = 0;
        return false;
    }
    if (++d->cnt >= s_debounce)
    {
        d->stable = raw;
        d->cnt    = 0;
        return raw;           /* edge only when pressed */
    }
    return false;
}

static void add_element(char e)
{
    if (s_plen < MORSE_MAX_ELEMENTS)
    {
        s_pattern[s_plen++] = e;
        s_pattern[s_plen]   = '\0';
    }
    evq_push(s_q, EV_KEY_ELEMENT, e);
}

static void start_element(bool dit)
{
    s_tone         = true;
    s_timer        = dit ? s_t.dit : s_t.dah;
    s_state        = KS_ELEMENT;
    s_last_was_dit = dit;
    s_space_sent   = false;
    add_element(dit ? MORSE_DOT : MORSE_DASH);
}

static void finish_char(void)
{
    if (s_plen == 0)
        return;
    char c = morse_decode(s_pattern);
    evq_push(s_q, EV_KEY_CHAR, c);
    s_plen       = 0;
    s_pattern[0] = '\0';
}

void keyer_init(event_queue_t *q)
{
    s_q = q;
    morse_timing_calc(&s_t, 14, 14);
    keyer_clear();
}

void keyer_configure(const morse_timing_t *t, uint8_t mode, bool swap, uint8_t debounce_ms)
{
    port_irq_lock();
    s_t        = *t;
    s_mode     = mode;
    s_swap     = swap;
    s_debounce = debounce_ms ? debounce_ms : 1;
    port_irq_unlock();
}

void keyer_clear(void)
{
    port_irq_lock();
    s_plen       = 0;
    s_pattern[0] = '\0';
    s_pend_dit   = s_pend_dah = false;
    s_space_sent = true;
    port_irq_unlock();
}

void keyer_tick(void)
{
    /* ---- inputs ------------------------------------------------------ */
    bool raw_dit = port_paddle_dit();
    bool raw_dah = port_paddle_dah();
    if (s_swap) { bool t = raw_dit; raw_dit = raw_dah; raw_dah = t; }

    bool new_dit = debounce(&s_dit, raw_dit);
    bool new_dah = debounce(&s_dah, raw_dah);

    if (s_mode == KEYER_IAMBIC_MEMORY)
    {
        if (new_dit) s_pend_dit = true;
        if (new_dah) s_pend_dah = true;
    }
    else if (s_state == KS_IDLE)
    {
        /* plain iambic: taps only count while idle */
        if (new_dit) s_pend_dit = true;
        if (new_dah) s_pend_dah = true;
    }

    /* ---- straight key (D4): direct keying + duration based decoding --- */
    debounce(&s_sk, port_straight_key());
    /* Level based start: a press that arrives while a gap is still running
       (human gaps are often shorter than one dit) must not be lost, else
       elements are missing and wrong characters / '*' are decoded. */
    if (s_sk.stable && !s_sk_down && s_state != KS_ELEMENT)
    {
        s_sk_down    = true;
        s_sk_time    = 0;
        s_tone       = true;
        s_space_sent = false;
        s_state      = KS_IDLE;
        s_pend_dit   = s_pend_dah = false;
    }
    if (s_sk_down)
    {
        s_sk_time++;
        if (!s_sk.stable)
        {
            s_sk_down = false;
            s_tone    = false;
            add_element(s_sk_time < 2u * s_t.dit ? MORSE_DOT : MORSE_DASH);
            s_state   = KS_IDLE;      /* letter / word gap is timed from now */
            s_idle    = 0;
        }
        return;
    }

    /* ---- element state machine -------------------------------------- */
    switch (s_state)
    {
    case KS_ELEMENT:
        if (s_timer) s_timer--;
        if (s_timer == 0)
        {
            s_tone  = false;
            s_state = KS_GAP;
            s_timer = s_t.elem_gap;
        }
        break;

    case KS_GAP:
        if (s_timer) s_timer--;
        if (s_timer == 0)
        {
            s_state = KS_IDLE;
            s_idle  = 0;
        }
        break;

    case KS_IDLE:
    default:
    {
        bool want_dit = s_pend_dit || s_dit.stable;
        bool want_dah = s_pend_dah || s_dah.stable;

        if (want_dit && want_dah)
        {
            /* squeeze: alternate */
            if (s_last_was_dit) { s_pend_dah = false; start_element(false); }
            else                { s_pend_dit = false; start_element(true);  }
        }
        else if (want_dit)
        {
            s_pend_dit = false;
            start_element(true);
        }
        else if (want_dah)
        {
            s_pend_dah = false;
            start_element(false);
        }
        else
        {
            s_idle++;
            if (s_plen > 0 && s_idle >= s_t.letter_extra)
                finish_char();
            if (!s_space_sent && s_idle >= (uint32_t)s_t.letter_extra + s_t.word_extra)
            {
                s_space_sent = true;
                evq_push(s_q, EV_KEY_SPACE, ' ');
            }
        }
        break;
    }
    }
}

bool keyer_tone(void)
{
    return s_tone;
}

bool keyer_busy(void)
{
    return s_state != KS_IDLE || s_sk_down || s_dit.stable || s_dah.stable;
}

void keyer_get_pattern(char *out, uint32_t size)
{
    if (size == 0) return;
    port_irq_lock();
    strncpy(out, s_pattern, size - 1);
    out[size - 1] = '\0';
    port_irq_unlock();
}
