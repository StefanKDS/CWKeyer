/**
 * @file    settings.c
 * @brief   Default values, validation and delayed saving of settings.
 */
#include "settings.h"
#include "port.h"
#include "morse_timing.h"
#include <stddef.h>
#include <string.h>

settings_t g_settings;

static bool     s_dirty;
static uint32_t s_dirty_since;

#define SAVE_DELAY_MS  3000u     /* flash is written 3 s after last change */

uint32_t settings_crc32(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    while (len--)
    {
        crc ^= *p++;
        for (int k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

void settings_defaults(settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->magic            = SETTINGS_MAGIC;
    s->version          = SETTINGS_VERSION;
    s->size             = (uint16_t)sizeof(*s);
    s->wpm              = 14;          /* START_POS of the ESP version */
    s->fwpm             = 14;
    s->tone_hz          = 700;
    s->volume           = 60;
    s->sidetone_on      = 1;
    s->monitor_passthru = 1;
    s->line_gain_db     = 0;
    s->keyer_mode       = KEYER_IAMBIC_MEMORY;
    s->paddle_swap      = 0;
    s->key_out_on       = 0;
    s->debounce_ms      = 10;
    s->dec_freq_hz      = 700;
    s->dec_squelch      = 20;
    s->group_len        = 5;
    strcpy(s->letters, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    strcpy(s->mem[0], "CQ CQ CQ DE DL0ABC DL0ABC K");
    strcpy(s->mem[1], "TNX FER QSO 73 ES GL");
    strcpy(s->mem[2], "UR RST 599 5NN");
}

static uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void settings_sanitize(settings_t *s)
{
    s->wpm          = clamp_u8(s->wpm, WPM_MIN, WPM_MAX);
    s->fwpm         = clamp_u8(s->fwpm, WPM_MIN, s->wpm);
    if (s->tone_hz < 300 || s->tone_hz > 1200)      s->tone_hz = 700;
    if (s->dec_freq_hz < 300 || s->dec_freq_hz > 1200) s->dec_freq_hz = 700;
    s->volume       = clamp_u8(s->volume, 0, 100);
    s->dec_squelch  = clamp_u8(s->dec_squelch, 0, 100);
    s->debounce_ms  = clamp_u8(s->debounce_ms, 1, 30);
    s->group_len    = clamp_u8(s->group_len, 1, 10);
    if (s->line_gain_db < -16) s->line_gain_db = -16;
    if (s->line_gain_db > 30)  s->line_gain_db = 30;
    if (s->keyer_mode > KEYER_IAMBIC_PLAIN) s->keyer_mode = KEYER_IAMBIC_MEMORY;

    s->letters[LETTERS_LEN] = '\0';
    for (int i = 0; i < MEM_COUNT; i++)
        s->mem[i][MEM_TEXT_LEN] = '\0';
}

void settings_load(void)
{
    settings_t tmp;
    if (port_settings_load(&tmp, sizeof(tmp)) &&
        tmp.magic   == SETTINGS_MAGIC &&
        tmp.version == SETTINGS_VERSION &&
        tmp.size    == sizeof(tmp) &&
        tmp.crc     == settings_crc32(&tmp, offsetof(settings_t, crc)))
    {
        g_settings = tmp;
    }
    else
    {
        settings_defaults(&g_settings);
    }
    settings_sanitize(&g_settings);
    s_dirty = false;
}

void settings_mark_dirty(void)
{
    s_dirty       = true;
    s_dirty_since = port_millis();
}

void settings_save_now(void)
{
    settings_sanitize(&g_settings);
    g_settings.magic   = SETTINGS_MAGIC;
    g_settings.version = SETTINGS_VERSION;
    g_settings.size    = (uint16_t)sizeof(settings_t);
    g_settings.crc     = settings_crc32(&g_settings, offsetof(settings_t, crc));
    (void)port_settings_save(&g_settings, sizeof(g_settings));
    s_dirty = false;
}

void settings_poll(void)
{
    if (s_dirty && (uint32_t)(port_millis() - s_dirty_since) >= SAVE_DELAY_MS)
        settings_save_now();
}
