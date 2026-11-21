/**
 * @file    app.c
 */
#include "app.h"
#include "cw_decoder.h"
#include "event_queue.h"
#include "keyer.h"
#include "morse_timing.h"
#include "player.h"
#include "port.h"
#include "settings.h"
#include "trainer.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- event queues: one per producer context ------------------------- */
static event_queue_t s_q_tick;      /* SysTick: keyer + player      */
static event_queue_t s_q_audio;     /* audio DMA: decoder           */

static volatile app_mode_t s_mode = APP_MODE_IDLE;
static volatile bool s_play_keys_tx;
static char          s_play_buf[MEM_TEXT_LEN + 1];

static textlog_t     s_keyer_log;
static textlog_t     s_mon_log;

/* ---- sidetone synth -------------------------------------------------- */
#define SINE_LEN 256u
static int16_t       s_sine[SINE_LEN];
static uint32_t      s_phase, s_phase_inc;
static float         s_env;                 /* 0..1 envelope         */
static float         s_env_step;
static float         s_amp;                 /* 0..1 from volume      */
static volatile bool s_tone_on;
static bool          s_passthru;
static bool          s_sidetone;

/* ===================================================================== */

void textlog_clear(textlog_t *l)
{
    l->len = 0;
    l->buf[0] = '\0';
    l->version++;
}

static void textlog_add(textlog_t *l, char c)
{
    if (c == ' ' && (l->len == 0 || l->buf[l->len - 1] == ' '))
        return;
    if (l->len >= TEXTLOG_SIZE - 1)
    {
        /* drop the oldest half */
        uint16_t keep = TEXTLOG_SIZE / 2;
        memmove(l->buf, &l->buf[l->len - keep], keep);
        l->len = keep;
    }
    l->buf[l->len++] = c;
    l->buf[l->len]   = '\0';
    l->version++;
}

textlog_t *app_keyer_log(void)   { return &s_keyer_log; }
textlog_t *app_monitor_log(void) { return &s_mon_log; }

/* ===================================================================== */

void app_apply_settings(void)
{
    morse_timing_t t;
    settings_t *s = &g_settings;

    settings_sanitize(s);
    morse_timing_calc(&t, s->wpm, s->fwpm);
    keyer_configure(&t, s->keyer_mode, s->paddle_swap != 0, s->debounce_ms);
    player_set_timing(&t);
    cw_decoder_configure(s->dec_freq_hz, s->dec_squelch);
    port_audio_set_input_gain(s->line_gain_db);

    /* volume 0..100 % -> logarithmic amplitude (-40 dB .. 0 dB) */
    s_amp = (s->volume == 0) ? 0.0f : powf(10.0f, (-40.0f + 0.4f * (float)s->volume) / 20.0f);
    s_phase_inc  = (uint32_t)(((uint64_t)s->tone_hz << 32) / AUDIO_FS);
    s_sidetone   = s->sidetone_on != 0;
    s_passthru   = s->monitor_passthru != 0;
}

void app_init(void)
{
    evq_init(&s_q_tick);
    evq_init(&s_q_audio);

    for (uint32_t i = 0; i < SINE_LEN; i++)
        s_sine[i] = (int16_t)(32000.0f * sinf(2.0f * (float)M_PI * (float)i / SINE_LEN));
    s_env_step = 1.0f / (0.005f * (float)AUDIO_FS);          /* 5 ms ramps */

    keyer_init(&s_q_tick);
    player_init(&s_q_tick);
    cw_decoder_init(&s_q_audio, AUDIO_FS);

    settings_load();
    app_apply_settings();
    textlog_clear(&s_keyer_log);
    textlog_clear(&s_mon_log);
}

void app_set_mode(app_mode_t mode)
{
    if (mode == s_mode)
        return;
    if (s_mode == APP_MODE_TRAINER)
        trainer_stop();
    player_stop();
    keyer_clear();
    s_mode = mode;
    cw_decoder_enable(mode == APP_MODE_MONITOR);
}

app_mode_t app_mode(void) { return s_mode; }

void app_play_text(const char *text, uint32_t len, bool key_tx)
{
    s_play_keys_tx = key_tx;
    player_start(text, len);
}

void app_play_memory(int idx)
{
    if (idx < 0 || idx >= MEM_COUNT)
        return;
    strcpy(s_play_buf, g_settings.mem[idx]);
    app_play_text(s_play_buf, (uint32_t)strlen(s_play_buf), true);
}

void app_stop_playback(void)
{
    player_stop();
}

bool app_tone_active(void)
{
    return s_tone_on;
}

/* ===================================================================== */
/*  Interrupt context                                                    */
/* ===================================================================== */

void app_tick_1ms(void)
{
    keyer_tick();

    /* paddle break-in stops a running playback */
    if (keyer_busy() && player_busy() && keyer_tone())
        player_stop();

    player_tick();

    bool k = keyer_tone();
    bool p = player_tone();
    s_tone_on = k || p;

    port_key_out(g_settings.key_out_on && (k || (p && s_play_keys_tx)));
}

void app_audio_block(const int16_t *in, int16_t *out, uint32_t frames)
{
    int16_t mono[64];
    const bool pass = s_passthru && (s_mode == APP_MODE_MONITOR);
    const bool tone = s_tone_on && s_sidetone;

    for (uint32_t done = 0; done < frames; )
    {
        uint32_t n = frames - done;
        if (n > 64u) n = 64u;

        for (uint32_t i = 0; i < n; i++)
        {
            const int16_t *fr = &in[2u * (done + i)];
            int32_t m = ((int32_t)fr[0] + (int32_t)fr[1]) / 2;
            mono[i] = (int16_t)m;

            /* envelope (raised to avoid key clicks) */
            if (tone)
            {
                s_env += s_env_step;
                if (s_env > 1.0f) s_env = 1.0f;
            }
            else
            {
                s_env -= s_env_step;
                if (s_env < 0.0f) s_env = 0.0f;
            }

            float v = 0.0f;
            if (s_env > 0.0f)
            {
                s_phase += s_phase_inc;
                float shaped = s_env * s_env * (3.0f - 2.0f * s_env);   /* smoothstep */
                v = (float)s_sine[s_phase >> 24] * shaped * s_amp;
            }
            if (pass)
                v += (float)m * s_amp;

            if (v > 32767.0f)  v = 32767.0f;
            if (v < -32768.0f) v = -32768.0f;
            int16_t o = (int16_t)v;
            out[2u * (done + i)]      = o;
            out[2u * (done + i) + 1u] = o;
        }
        cw_decoder_process(mono, n);
        done += n;
    }
}

/* ===================================================================== */
/*  Main loop                                                            */
/* ===================================================================== */

void app_poll(void)
{
    event_t ev;

    while (evq_pop(&s_q_tick, &ev))
    {
        switch (ev.type)
        {
        case EV_KEY_CHAR:
            textlog_add(&s_keyer_log, ev.arg);
            if (s_mode == APP_MODE_TRAINER)
                trainer_on_keyed_char(ev.arg);
            break;
        case EV_KEY_SPACE:
            textlog_add(&s_keyer_log, ' ');
            break;
        default:
            break;
        }
    }

    while (evq_pop(&s_q_audio, &ev))
    {
        if (ev.type == EV_DEC_CHAR || ev.type == EV_DEC_SPACE)
            textlog_add(&s_mon_log, ev.arg);
    }

    if (s_mode == APP_MODE_TRAINER)
        trainer_poll();

    settings_poll();
}
