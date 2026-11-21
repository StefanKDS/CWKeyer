/**
 * @file    cw_decoder.c
 * @brief   Goertzel based CW decoder.
 *
 * Processing chain (per hop of 64 samples):
 *   1. Goertzel magnitude over the last 128 samples
 *   2. adaptive threshold: the ESP version let one limit follow the
 *      magnitude (factor 1/6). That limit also follows the noise floor
 *      in pauses and causes false triggers with noisy receiver audio.
 *      Here a peak tracker (fast attack, slow release) and a noise
 *      floor tracker (fast down, slow up) are used; the threshold lies
 *      between both. Detection requires peak > squelch and SNR > 4.
 *   3. hysteresis + glitch filter (state must be stable for 2 hops)
 *   4. timing: dit/dah separation with running averages of dit and
 *      dah length (ESP version: dit average only), letter gap > 2 dits, word gap
 *      > max(5 dits, 1.6 x average letter gap) so Farnsworth works.
 */
#include "cw_decoder.h"
#include "morse.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static event_queue_t *s_q;
static uint32_t       s_fs = 16000;
static volatile bool  s_enabled;

/* Goertzel */
static float    s_coeff;
static int16_t  s_win[DEC_WINDOW];
static uint32_t s_win_pos;
static uint32_t s_hop_cnt;

/* level tracking */
static float    s_mag;
static float    s_peak;
static float    s_floor;
static float    s_thr_on, s_thr_off;
static bool     s_valid;             /* enough signal / SNR               */
static float    s_squelch = 0.001f;

/* state + timing (in ms, derived from sample count) */
static uint32_t s_samples;           /* running sample counter          */
static bool     s_raw, s_filtered;
static uint8_t  s_stable_cnt;
static float    s_t_high_start, s_t_low_start;
static float    s_dit_avg = 60.0f;   /* ~20 WPM                         */
static float    s_dah_avg = 180.0f;
static float    s_lgap_avg = 180.0f; /* average letter gap               */
static char     s_code[MORSE_MAX_ELEMENTS + 1];
static uint8_t  s_clen;
static bool     s_char_done = true;
static bool     s_space_done = true;

static float now_ms(void)
{
    return (float)s_samples * 1000.0f / (float)s_fs;
}

void cw_decoder_init(event_queue_t *q, uint32_t sample_rate)
{
    s_q  = q;
    s_fs = sample_rate;
    cw_decoder_configure(700, 20);
    cw_decoder_reset();
}

void cw_decoder_configure(uint16_t target_hz, uint8_t squelch)
{
    /* bin centred on the target frequency */
    float k = floorf(0.5f + ((float)DEC_WINDOW * target_hz) / (float)s_fs);
    float w = (2.0f * (float)M_PI * k) / (float)DEC_WINDOW;
    s_coeff = 2.0f * cosf(w);

    /* squelch 0..100 -> -70 dBFS .. -20 dBFS */
    float db = -70.0f + 0.5f * (float)squelch;
    s_squelch = powf(10.0f, db / 20.0f);
}

void cw_decoder_enable(bool on)
{
    if (on && !s_enabled)
        cw_decoder_reset();
    s_enabled = on;
}

void cw_decoder_reset(void)
{
    memset(s_win, 0, sizeof(s_win));
    s_win_pos = s_hop_cnt = 0;
    s_peak    = 0.0f;
    s_floor   = 0.0f;
    s_valid   = false;
    s_dit_avg = 60.0f;
    s_dah_avg = 180.0f;
    s_lgap_avg = 180.0f;
    s_t_low_start = s_t_high_start = 0.0f;
    s_raw = s_filtered = false;
    s_stable_cnt = 0;
    s_clen = 0;
    s_code[0] = '\0';
    s_char_done = s_space_done = true;
}

static void emit_char(void)
{
    if (s_clen == 0)
        return;
    char c = morse_decode(s_code);
    evq_push(s_q, EV_DEC_CHAR, c);
    s_clen    = 0;
    s_code[0] = '\0';
}

static void add_element(char e)
{
    if (s_clen < MORSE_MAX_ELEMENTS)
    {
        s_code[s_clen++] = e;
        s_code[s_clen]   = '\0';
    }
}

static void goertzel(void)
{
    float q1 = 0.0f, q2 = 0.0f;
    uint32_t idx = s_win_pos;                 /* oldest sample */
    for (uint32_t i = 0; i < DEC_WINDOW; i++)
    {
        float q0 = s_coeff * q1 - q2 + (float)s_win[idx];
        q2 = q1;
        q1 = q0;
        idx = (idx + 1u) & (DEC_WINDOW - 1u);
    }
    float m2 = q1 * q1 + q2 * q2 - q1 * q2 * s_coeff;
    /* normalise to sine amplitude relative to full scale */
    s_mag = (2.0f * sqrtf(m2 > 0 ? m2 : 0)) / ((float)DEC_WINDOW * 32768.0f);
}

static void update_threshold(void)
{
    /* peak: instant attack, release ~2 s (hop = 4 ms) */
    if (s_mag > s_peak) s_peak = s_mag;
    else                s_peak -= (s_peak - s_mag) * 0.002f;

    /* noise floor: fast down, slow up (~1 s), frozen while a tone is on */
    if (s_mag < s_floor)  s_floor += (s_mag - s_floor) * 0.3f;
    else if (!s_filtered) s_floor += (s_mag - s_floor) * 0.004f;

    float span = s_peak - s_floor;
    s_thr_on  = s_floor + span * 0.50f;
    s_thr_off = s_floor + span * 0.35f;
    s_valid   = (s_peak > s_squelch) && (s_peak > 4.0f * s_floor);
}

static float word_gap(void)
{
    if (s_lgap_avg < s_dit_avg * 2.5f)
        s_lgap_avg = s_dit_avg * 2.5f;
    float a = s_dit_avg * 5.0f;
    float b = s_lgap_avg * 1.6f;
    return a > b ? a : b;
}

static void detect(void)
{
    const float t = now_ms();

    bool raw = s_valid && (s_filtered ? (s_mag > s_thr_off) : (s_mag > s_thr_on));
    if (raw != s_raw)
    {
        s_raw = raw;
        s_stable_cnt = 0;
    }
    else if (s_stable_cnt < 255)
    {
        s_stable_cnt++;
    }

    if (s_raw != s_filtered && s_stable_cnt >= 1)   /* stable for 2 hops */
    {
        s_filtered = s_raw;
        const float hop_ms = (float)DEC_HOP * 1000.0f / (float)s_fs;
        const float edge   = t - hop_ms;            /* compensate filter delay */

        if (!s_filtered)
        {
            /* tone ended -> dit or dah */
            float high = edge - s_t_high_start;
            s_t_low_start = edge;

            /* two cluster tracking (dit / dah). A clearly longer or
               shorter element than expected re-synchronises both,
               so speed changes are followed quickly. */
            if (high > s_dah_avg * 1.5f)
            {
                s_dah_avg  = high;
                s_dit_avg  = high / 3.0f;
                s_lgap_avg = high;
            }
            else if (high < s_dit_avg * 0.5f)
            {
                s_dit_avg  = high;
                s_dah_avg  = high * 3.0f;
                s_lgap_avg = high * 3.0f;
            }

            float mid = (s_dit_avg + s_dah_avg) * 0.5f;
            if (mid > s_dit_avg * 2.0f)
                mid = s_dit_avg * 2.0f;                 /* ESP rule: dah > 2 dits */
            if (high < mid)
            {
                s_dit_avg = (high + 2.0f * s_dit_avg) / 3.0f;
                add_element(MORSE_DOT);
            }
            else
            {
                s_dah_avg = (high + 2.0f * s_dah_avg) / 3.0f;
                add_element(MORSE_DASH);
            }
            /* keep the ratio plausible (dah = 2.5 .. 3.5 dit) */
            if (s_dah_avg < s_dit_avg * 2.5f) s_dah_avg = s_dit_avg * 2.5f;
            if (s_dah_avg > s_dit_avg * 3.5f) s_dit_avg = s_dah_avg / 3.5f;
            if (s_dit_avg < 20.0f)  { s_dit_avg = 20.0f;  s_dah_avg = 60.0f;  }  /* 60 WPM */
            if (s_dit_avg > 240.0f) { s_dit_avg = 240.0f; s_dah_avg = 720.0f; }  /*  5 WPM */
            s_char_done = false;
            s_space_done = false;
        }
        else
        {
            /* tone started: learn letter gap (Farnsworth) */
            float low = edge - s_t_low_start;
            if (low > s_dit_avg * 2.0f && low < s_lgap_avg * 2.5f)
                s_lgap_avg = (low + 3.0f * s_lgap_avg) / 4.0f;
            s_t_high_start = edge;
        }
    }

    /* gaps while silent */
    if (!s_filtered)
    {
        float low = t - s_t_low_start;
        if (!s_char_done && low > s_dit_avg * 2.0f)
        {
            emit_char();
            s_char_done = true;
        }
        if (!s_space_done && low > word_gap())
        {
            evq_push(s_q, EV_DEC_SPACE, ' ');
            s_space_done = true;
        }
    }
}

void cw_decoder_process(const int16_t *mono, uint32_t n)
{
    if (!s_enabled)
        return;

    for (uint32_t i = 0; i < n; i++)
    {
        s_win[s_win_pos] = mono[i];
        s_win_pos = (s_win_pos + 1u) & (DEC_WINDOW - 1u);
        s_samples++;
        if (++s_hop_cnt >= DEC_HOP)
        {
            s_hop_cnt = 0;
            goertzel();
            update_threshold();
            detect();
        }
    }
}

void cw_decoder_get_status(cw_decoder_status_t *st)
{
    st->level     = s_mag;
    st->threshold = s_thr_on;
    st->tone      = s_filtered;
    st->wpm       = (uint8_t)(1200.0f / s_dit_avg + 0.5f);
}
