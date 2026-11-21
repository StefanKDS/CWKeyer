/**
 * @file    app.h
 * @brief   Application core: connects keyer, player, decoder, trainer,
 *          audio and settings. Hardware independent.
 *
 * Execution contexts
 *   app_tick_1ms()     SysTick interrupt, 1 kHz  (paddles, keyer, player)
 *   app_audio_block()  audio DMA interrupt       (decoder, sidetone synth)
 *   app_poll()         main loop                 (events, trainer, settings)
 */
#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stdint.h>

#define APP_VERSION       "1.0"
#define AUDIO_FS          16000u      /**< codec sample rate [Hz]           */

#define TEXTLOG_SIZE      1024u

typedef enum
{
    APP_MODE_IDLE = 0,
    APP_MODE_KEYER,
    APP_MODE_MONITOR,
    APP_MODE_TRAINER,
    APP_MODE_SDTEXT,
} app_mode_t;

typedef struct
{
    char     buf[TEXTLOG_SIZE];
    uint16_t len;
    uint16_t version;          /**< incremented on every change */
} textlog_t;

void app_init(void);
void app_tick_1ms(void);
void app_audio_block(const int16_t *in, int16_t *out, uint32_t frames);
void app_poll(void);

void app_set_mode(app_mode_t mode);
app_mode_t app_mode(void);
void app_apply_settings(void);       /**< after changing g_settings */

void app_play_text(const char *text, uint32_t len, bool key_tx);
void app_play_memory(int idx);
void app_stop_playback(void);
bool app_tone_active(void);

textlog_t *app_keyer_log(void);
textlog_t *app_monitor_log(void);
void textlog_clear(textlog_t *l);

#endif
