/**
 * @file    keyer.h
 * @brief   Iambic paddle keyer + straight key (port of the ESP8266 loop logic).
 *
 * keyer_tick() must be called every 1 ms (SysTick context).
 * Keyed elements / characters are reported through an event queue.
 */
#ifndef KEYER_H
#define KEYER_H

#include <stdbool.h>
#include <stdint.h>
#include "event_queue.h"
#include "morse_timing.h"

void keyer_init(event_queue_t *q);
void keyer_configure(const morse_timing_t *t, uint8_t mode, bool swap, uint8_t debounce_ms);
void keyer_tick(void);

bool keyer_tone(void);          /**< true while the keyer wants the tone on       */
bool keyer_busy(void);          /**< element/gap running or paddle pressed        */
void keyer_get_pattern(char *out, uint32_t size); /**< current, unfinished pattern */
void keyer_clear(void);         /**< discard the current pattern                  */

#endif
