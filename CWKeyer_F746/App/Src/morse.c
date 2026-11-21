/**
 * @file    morse.c
 * @brief   Morse code table (ITU) incl. digits and common punctuation.
 */
#include "morse.h"
#include <string.h>

typedef struct
{
    char        ch;
    const char *code;
} morse_entry_t;

static const morse_entry_t s_table[] =
{
    {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},
    {'E', "."},     {'F', "..-."},  {'G', "--."},   {'H', "...."},
    {'I', ".."},    {'J', ".---"},  {'K', "-.-"},   {'L', ".-.."},
    {'M', "--"},    {'N', "-."},    {'O', "---"},   {'P', ".--."},
    {'Q', "--.-"},  {'R', ".-."},   {'S', "..."},   {'T', "-"},
    {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},
    {'Y', "-.--"},  {'Z', "--.."},
    {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."},
    {'8', "---.."}, {'9', "----."},
    {'?', "..--.."},{'.', ".-.-.-"},{',', "--..--"},{'!', "-.-.--"},
    {'@', ".--.-."},{':', "---..."},{'-', "-....-"},{'/', "-..-."},
    {'(', "-.--."}, {')', "-.--.-"},{'&', ".-..."}, {'$', "...-..-"},
    {'+', ".-.-."}, {'=', "-...-"}, {'\'', ".----."},{'"', ".-..-."},
    {';', "-.-.-."},{'_', "..--.-"},
};

#define TABLE_SIZE (sizeof(s_table) / sizeof(s_table[0]))

static char to_upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

const char *morse_encode(char c)
{
    c = to_upper(c);
    for (unsigned i = 0; i < TABLE_SIZE; i++)
    {
        if (s_table[i].ch == c)
            return s_table[i].code;
    }
    return NULL;
}

char morse_decode(const char *pattern)
{
    if (pattern == NULL || pattern[0] == '\0')
        return 0;

    for (unsigned i = 0; i < TABLE_SIZE; i++)
    {
        if (strcmp(s_table[i].code, pattern) == 0)
            return s_table[i].ch;
    }
    return MORSE_UNKNOWN;
}

bool morse_is_supported(char c)
{
    return (c == ' ') || (morse_encode(c) != NULL);
}
