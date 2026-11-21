/**
 * @file    morse_timing.c
 */
#include "morse_timing.h"

static uint8_t clamp_wpm(uint8_t v)
{
    if (v < WPM_MIN) return WPM_MIN;
    if (v > WPM_MAX) return WPM_MAX;
    return v;
}

void morse_timing_calc(morse_timing_t *t, uint8_t wpm, uint8_t fwpm)
{
    wpm  = clamp_wpm(wpm);
    fwpm = clamp_wpm(fwpm);
    if (fwpm > wpm)
        fwpm = wpm;                       /* Farnsworth never faster than wpm */

    const uint16_t T   = (uint16_t)(1200u / wpm);
    const uint16_t Tfw = (uint16_t)(1200u / fwpm);

    t->dit          = T;
    t->dah          = (uint16_t)(3u * T);
    t->elem_gap     = T;
    t->letter_extra = (uint16_t)(3u * Tfw - T);  /* total letter gap = 3 Tfw */
    t->word_extra   = (uint16_t)(4u * Tfw);      /* total word gap   = 7 Tfw */
}
