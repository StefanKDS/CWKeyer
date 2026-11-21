/**
 * @file    player.h
 * @brief   Non-blocking text -> Morse playback (memories, trainer, SD texts).
 *
 * player_tick() is called every 1 ms (SysTick context).
 * The text buffer passed to player_start() must stay valid until playback ends.
 */
#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <stdint.h>
#include "event_queue.h"
#include "morse_timing.h"

void     player_init(event_queue_t *q);
void     player_set_timing(const morse_timing_t *t);
void     player_start(const char *text, uint32_t len);
void     player_stop(void);
void     player_pause(bool pause);
bool     player_busy(void);
bool     player_paused(void);
uint32_t player_pos(void);          /**< index of the character being sent */
void     player_tick(void);
bool     player_tone(void);

#endif
