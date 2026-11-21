/**
 * @file    settings.h
 * @brief   Persistent user settings (replaces EEPROM + web page of the ESP version).
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#define SETTINGS_MAGIC     0x43574B59u   /* "CWKY" */
#define SETTINGS_VERSION   1u

#define MEM_COUNT          3             /**< number of text memories */
#define MEM_TEXT_LEN       128           /**< max chars per memory    */
#define LETTERS_LEN        48            /**< trainer character set   */

typedef enum
{
    KEYER_IAMBIC_MEMORY = 0,   /**< iambic with dot/dash memory (ESP behaviour) */
    KEYER_IAMBIC_PLAIN  = 1,   /**< iambic without memory                       */
} keyer_mode_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;

    /* speed */
    uint8_t  wpm;                 /**< character speed            */
    uint8_t  fwpm;                /**< Farnsworth spacing speed   */

    /* audio */
    uint16_t tone_hz;             /**< sidetone frequency          */
    uint8_t  volume;              /**< 0..100 %                    */
    uint8_t  sidetone_on;         /**< speaker / headphone on      */
    uint8_t  monitor_passthru;    /**< line in -> headphone in monitor */
    int8_t   line_gain_db;        /**< -16..+30 dB                 */

    /* keyer */
    uint8_t  keyer_mode;          /**< keyer_mode_t                */
    uint8_t  paddle_swap;
    uint8_t  key_out_on;          /**< drive TX keying output      */
    uint8_t  debounce_ms;

    /* decoder */
    uint16_t dec_freq_hz;         /**< Goertzel target frequency   */
    uint8_t  dec_squelch;         /**< 0..100, min. signal level   */

    /* trainer */
    uint8_t  group_len;           /**< letters per group (hear)    */
    char     letters[LETTERS_LEN + 1];

    /* memories */
    char     mem[MEM_COUNT][MEM_TEXT_LEN + 1];

    uint8_t  reserved[16];
    uint32_t crc;                 /**< CRC32 over all bytes before crc */
} settings_t;

extern settings_t g_settings;

void settings_defaults(settings_t *s);
void settings_sanitize(settings_t *s);
void settings_load(void);          /**< load from storage or defaults        */
void settings_mark_dirty(void);    /**< request save (delayed)               */
void settings_poll(void);          /**< call from main loop, saves if dirty  */
void settings_save_now(void);
uint32_t settings_crc32(const void *data, uint32_t len);

#endif
