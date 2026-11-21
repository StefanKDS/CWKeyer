/**
 * @file    morse.h
 * @brief   Morse code table: character <-> dot/dash pattern.
 *
 * Hardware independent. Patterns use '.' for dit and '-' for dah.
 */
#ifndef MORSE_H
#define MORSE_H

#include <stdbool.h>

#define MORSE_DOT            '.'
#define MORSE_DASH           '-'
#define MORSE_MAX_ELEMENTS   8      /**< longest pattern incl. reserve */
#define MORSE_UNKNOWN        '*'    /**< returned for unknown patterns  */

/** Returns the pattern for c (case-insensitive) or NULL if unsupported. */
const char *morse_encode(char c);

/** Returns the character for a pattern, MORSE_UNKNOWN if no match, 0 if empty. */
char morse_decode(const char *pattern);

/** True if c can be sent as Morse (space counts as supported). */
bool morse_is_supported(char c);

#endif /* MORSE_H */
