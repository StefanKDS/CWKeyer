/**
 * @file    text_source.h
 * @brief   Practice texts from SD card (*.TXT in root or in folder /CW).
 */
#ifndef TEXT_SOURCE_H
#define TEXT_SOURCE_H

#include <stdbool.h>
#include <stdint.h>

#define TEXT_MAX_FILES   40
#define TEXT_BUF_SIZE    16384u

typedef enum
{
    TS_NO_CARD = 0,
    TS_ERROR,
    TS_READY,
} text_source_status_t;

text_source_status_t text_source_scan(void);       /**< (re)mount + list files */
text_source_status_t text_source_status(void);
int         text_source_count(void);
const char *text_source_name(int idx);
uint32_t    text_source_size(int idx);
bool        text_source_load(int idx);              /**< load + normalise       */
const char *text_source_text(void);
uint32_t    text_source_len(void);
int         text_source_loaded_index(void);

/** Normalise raw text for CW: upper case, umlauts -> AE/OE/UE/SS,
 *  whitespace collapsed, unsupported characters removed.
 *  Returns the number of characters written to out (no terminator). */
uint32_t    text_normalize(const char *in, uint32_t len, char *out, uint32_t max);

#endif
