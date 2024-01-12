#ifndef DEFINES_H
#define DEFINES_H

#define ROTARY_PIN1 0
#define ROTARY_PIN2 2
#define BUZZER_PIN 15
#define KEYER_SHORT_PIN 12
#define KEYER_LONG_PIN 13
#define MODE_BUTTON_PIN 16
#define SPEAKER_PIN 14
#define NO_KEY 999

#define CLICKS_PER_STEP   4 
#define START_POS 14
#define STEP_SIZE 1

#define SERIAL_SPEED    115200

// EEPROM //////////////
#define EEPROM_SIZE     260
#define EEPROM_WPM_ADDR 0   // 1 byte
#define EEPROM_MEM1_ADDR 1  // 128 byte
#define EEPROM_MEM2_ADDR 129   // 128 byte
#define EEPROM_SPEAKER_ADDR 258
///////////////////////

// STATES ///////////////
#define STATE_IDLE 1000
#define STATE_MEM 1001
#define STATE_TRAINER 1002
///////////////////////

// MENU ////////////////
#define MAIN_MENU_COUNT 3
#define MAIN_MENU 9
#define CW_KEYER 1
#define TRAINER 2
#define SETUP 3
///////////////////////

// SETUP MENU /////////
#define SETUP_MENU_COUNT 2
#define SETUP_SPEAKER 1
#define SETUP_BACK 2
///////////////////////

// SETUP CW_KEYER /////////
#define KEYER_MENU_COUNT 4
#define MEM_1 1
#define MEM_2 2
#define SPEED 3
#define CW_KEYER_BACK 4
///////////////////////

// TRAINER MENU /////////
#define TRAINER_MENU_COUNT 1
#define TRAINER_BACK 1
///////////////////////

// All morse characters
#define MORSE_DOT '.'
#define MORSE_DASH '-'

#endif
