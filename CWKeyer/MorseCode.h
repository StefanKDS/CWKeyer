#ifndef MORSECODE_H
#define MORSECODE_H

#include <pgmspace.h>

const char MORSE_LETTERS[][6] PROGMEM = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....",
  "..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.",
  "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."
};

const char LETTERS[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

const char MORSE_NUMBERS[][6] PROGMEM = {
  "-----", ".----", "..---", "...--", "....-", ".....",
  "-....", "--...", "---..", "----."
};

#endif
