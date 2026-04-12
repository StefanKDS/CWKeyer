#ifndef DEFINES_H
#define DEFINES_H

#define VERSION "0.65"

#define ROTARY_PIN1 2 //D4
#define ROTARY_PIN2 0 //D3
#define BUZZER_PIN 15 //D8
#define KEYER_SHORT_PIN 12 //D6
#define KEYER_LONG_PIN 13 //D7
#define MODE_BUTTON_PIN 16 //D0
#define SPEAKER_PIN 14 //D5
#define SCL 5 //D1
#define SDA 4 //D2
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
#define EEPROM_SPEAKER_ADDR 258 // 1 byte
#define EEPROM_FWPM_ADDR 259 // 1 byte
///////////////////////

// STATES ///////////////
#define STATE_IDLE 1000
#define STATE_MEM 1001
#define STATE_TRAINER 1002
///////////////////////

// MENU ////////////////
#define MAIN_MENU_COUNT 4
#define MAIN_MENU 9
#define CW_KEYER 1
#define MONITOR 2
#define TRAINER 3
#define SETUP 4

#define TRAINER_GIVE_RANDOM_SCREEN 10
#define TRAINER_GIVE_AZ_SCREEN 11
#define TRAINER_LISTEN_REPEAT_SCREEN 12
#define TRAINER_HEAR_SCREEN 20
///////////////////////

// SETUP MENU /////////
#define SETUP_MENU_COUNT 4
#define SETUP_SPEAKER 1
#define SETUP_SETTINGS 2
#define SETUP_FARNSWORTH 3
#define SETUP_BACK 4
///////////////////////

// SETUP CW_KEYER /////////
#define KEYER_MENU_COUNT 4
#define MEM_1 1
#define MEM_2 2
#define SPEED 3
#define CW_KEYER_BACK 4
///////////////////////

// TRAINER MENU /////////
#define TRAINER_MENU_COUNT 5
#define TRAINER_HEAR 1
#define TRAINER_GIVE_RANDOM 2
#define TRAINER_GIVE_AZ 3
#define TRAINER_LISTEN_REPEAT 4
#define TRAINER_BACK 5
///////////////////////

// All morse characters
#define MORSE_DOT '.'
#define MORSE_DASH '-'

// #define DEBUG 1

#ifdef DEBUG
  #define DEBUG_BEGIN(...) { Serial.begin(__VA_ARGS__); }
  #define DEBUG_PRINT(...) { Serial.print(__VA_ARGS__); }
  #define DEBUG_PRINTLN(...) { Serial.println(__VA_ARGS__); }
#else
  #define DEBUG_BEGIN(...) {}
  #define DEBUG_PRINT(...) {}
  #define DEBUG_PRINTLN(...) {}
#endif

#endif
