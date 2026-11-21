/**
 * @file    morse_timing.h
 * @brief   PARIS based timing incl. Farnsworth spacing.
 *
 *  T   = 1200 / wpm   (element unit)
 *  Tfw = 1200 / fwpm  (spacing unit, fwpm <= wpm)
 *
 *  dit = 1T, dah = 3T, gap between elements = 1T
 *  gap between letters = 3Tfw, gap between words = 7Tfw
 */
#ifndef MORSE_TIMING_H
#define MORSE_TIMING_H

#include <stdint.h>

#define WPM_MIN  5
#define WPM_MAX  50

typedef struct
{
    uint16_t dit;          /**< dit length [ms]                                 */
    uint16_t dah;          /**< dah length [ms]                                 */
    uint16_t elem_gap;     /**< gap after each element [ms]                     */
    uint16_t letter_extra; /**< additional gap after letter (3Tfw - elem_gap)   */
    uint16_t word_extra;   /**< additional gap after letter gap for word (4Tfw) */
} morse_timing_t;

/** Calculates the timing. Values are clamped to WPM_MIN..WPM_MAX, fwpm <= wpm. */
void morse_timing_calc(morse_timing_t *t, uint8_t wpm, uint8_t fwpm);

#endif
