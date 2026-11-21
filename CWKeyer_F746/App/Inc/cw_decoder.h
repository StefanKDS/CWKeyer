/**
 * @file    cw_decoder.h
 * @brief   Audio CW decoder: Goertzel tone detection + adaptive timing.
 *
 * Port of Decoder.ino. Samples come from the codec line input
 * (cw_decoder_process() is called from the audio DMA interrupt).
 * The time base is derived from the sample counter, therefore the
 * decoder is independent of loop timing (unlike the ESP version).
 */
#ifndef CW_DECODER_H
#define CW_DECODER_H

#include <stdbool.h>
#include <stdint.h>
#include "event_queue.h"

#define DEC_WINDOW   128u   /**< Goertzel window (8 ms @ 16 kHz)          */
#define DEC_HOP       64u   /**< new result every 64 samples (4 ms)       */

typedef struct
{
    float    level;        /**< tone magnitude, 0..1 full scale            */
    float    threshold;    /**< current detection threshold                */
    bool     tone;         /**< filtered tone state                        */
    uint8_t  wpm;          /**< estimated speed                            */
} cw_decoder_status_t;

void cw_decoder_init(event_queue_t *q, uint32_t sample_rate);
void cw_decoder_configure(uint16_t target_hz, uint8_t squelch);
void cw_decoder_enable(bool on);
void cw_decoder_process(const int16_t *mono, uint32_t n);
void cw_decoder_get_status(cw_decoder_status_t *st);
void cw_decoder_reset(void);

#endif
