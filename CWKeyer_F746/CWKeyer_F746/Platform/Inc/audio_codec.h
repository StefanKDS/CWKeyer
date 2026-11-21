/**
 * @file    audio_codec.h
 * @brief   WM8994 codec via SAI2: line in (block B, RX) and headphone
 *          (block A, TX), 16 bit stereo, circular DMA.
 *
 * Every AUDIO_BLOCK_FRAMES frames app_audio_block() is called from the
 * DMA interrupt with the received samples and the output buffer.
 */
#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H

#include <stdbool.h>
#include <stdint.h>

#define AUDIO_BLOCK_FRAMES  64u   /* 4 ms @ 16 kHz */

bool audio_codec_init(uint32_t sample_rate);
void audio_codec_set_input_gain(int8_t db);
bool audio_codec_ok(void);

#endif
