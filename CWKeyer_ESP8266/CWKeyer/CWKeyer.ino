#include <ESP8266WiFi.h> 
#include <ESP8266WebServer.h>
#include "WebPage.h"
#include <Wire.h>
#include "Rotary.h"
#include "ESPOLED.h"
#include "MorseCode.h"  
#include <EEPROM.h>
#include "Defines.h"

/////////////////////////////////////////////////////////////////
// DECLARATIONS - Function Prototypes
// These prototypes allow functions to be called before their
// definition in the code
/////////////////////////////////////////////////////////////////

void CalculateTimes(char wpm, char fwpm);  // Calculate timing values based on WPM
String ReadTextFromEEPROM(byte addr);      // Read string from EEPROM at specified address
void SwitchSpeaker(byte value);            // Enable/disable buzzer output
void WriteTextToEEPROM(byte addr, String text);  // Write string to EEPROM at specified address
void ShowMainScreen();                     // Display main menu screen
void ShowSetupScreen();                    // Display setup/configuration screen
void ShowMonitorScreen();                  // Display Morse monitor (decoder) screen
void ShowTrainerScreen();                  // Display trainer mode menu
void ShowKeyerScreen();                    // Display CW keyer menu
void rotate(Rotary& r);                    // Handle rotary encoder rotation
void ReactOnButtonClick();                 // Handle mode button click
void ProcessBeep(short beepoLength, char outputChar);  // Start a beep tone
void ProcessSign(char sign);               // Play a single character as Morse code
void startBeep(unsigned int length, char symbol);     // Start non-blocking beep
void updateBeep();                         // Update beep state (non-blocking)
void startPlayback(String text);           // Start playback of stored text
void updatePlayback();                     // Update playback state (non-blocking)

/////////////////////////////////////////////////////////////////
// GLOBAL OBJECTS - Hardware Interfaces
/////////////////////////////////////////////////////////////////
Rotary r;                               // Rotary encoder for menu navigation and speed adjustment
OLED display(SDA, SCL, 0x3c, 2);       // OLED display (I2C address 0x3c)
ESP8266WebServer server(80);            // Web server for WiFi configuration (Port 80)

/////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES - State and Timing
/////////////////////////////////////////////////////////////////

// ===== Non-blocking Beep / Playback State =====
// Audio tone playback is non-blocking; these variables manage its state
bool beeping = false;           // true while tone is currently active
unsigned long beepEnd = 0;      // Timestamp when current tone ends (milliseconds)

bool inPause = false;           // true while a pause between elements is running
unsigned long pauseEnd = 0;     // Timestamp when current pause ends (milliseconds)

// ===== Timing Control for Character Recognition =====
unsigned long lastKeyTime = 0;  // Timestamp of last key input (for character end detection)
unsigned int letterGap = 3;     // Pause gap between characters (3T units)

// ===== Playback State - For Stored Text Playback =====
bool playing = false;           // true while text is being played back
String playbackText = "";       // Text to be played
int playbackIndex = 0;          // Current position in playbackText
String currentMorse = "";       // Morse code of current character (e.g. ".-")
int morseIndex = 0;             // Current position in Morse code (which dot/dash)
unsigned long nextActionTime = 0; // Timestamp of next action during playback

// ===== Morse Code Timing (T-based) =====
// T is the basic time unit: T = 1200ms / WPM
// With Farnsworth, pauses are extended, but dots/dashes remain the same
unsigned int T_unit = 0;        // Basic time unit in milliseconds
unsigned int dit_len = 0;       // Duration of a dot (1T)
unsigned int dah_len = 0;       // Duration of a dash (3T)
unsigned int elementGap = 0;    // Pause between dot/dash (1T - Inter-Element Gap)
unsigned int letterExtra = 0;   // Extra pause after letter (Farnsworth adjustment)
unsigned int wordExtra = 0;     // Extra pause after word (Farnsworth adjustment)

// ===== User Configuration and Modes =====
char wpm = START_POS;           // Speed in words per minute (WPM)
char fwpm = wpm;                // Farnsworth speed (for extended pauses)
bool speed_mode = false;        // true = Encoder for changing WPM speed
bool fspeed_mode = false;       // true = Encoder for changing Farnsworth speed
bool speakerOn = true;          // true = Buzzer is enabled
bool settingsOn = false;        // true = WiFi/Web-Server is enabled
double key_activated;           // Timestamp of last key press
short char_on_screen = -1;      // Number of characters displayed on screen

// ===== Menu Navigation =====
byte selected_menu_item = SETUP_SPEAKER;  // Currently selected menu item
byte actual_menu = MAIN_MENU;             // Current screen/menu being displayed

// ===== WiFi and Web Server =====
const char* ssid     = "CWKeyer";        // WiFi network name of the access point
const char* password = "123456789";      // WiFi password
const char* TEXT_1 = "input1";           // Parameter name for web form (memory 1)
const char* TEXT_2 = "input2";           // Parameter name for web form (memory 2)

// ===== Morse Code Decoding and Trainer =====
String decoderString;                    // Buffers received dots/dashes (e.g. ".-")
String selectedLetters = "";             // Letters for trainer (e.g. "ABC...XYZ")
char currentTrainerLetter;                // Current letter to train
short currentAZPosition = -1;             // Position in alphabet for sequential training

// ===== State Management =====
int State = STATE_IDLE;                  // Main application state

/////////////////////////////////////////////////////////////////
// Audio Decoder - Goertzel Algorithm for Morse Audio Input
/////////////////////////////////////////////////////////////////
// ===== Goertzel Variables =====
const int audioInPin = A0;                // Analog input pin for audio decoder
const float sampling_freq = 8928.0;       // Sampling frequency for Goertzel algorithm
const float target_freq = 700.0;          // Target frequency (optimized for CW audio, minimum 700Hz required)
const int decoder_n = 24;                 // Number of samples per Goertzel iteration

float decoder_coeff;                      // Goertzel coefficient (precomputed)
float decoder_Q1 = 0;                     // Goertzel Q1 state variable
float decoder_Q2 = 0;                     // Goertzel Q2 state variable
int decoder_testData[24];                 // Buffer for audio samples

// ===== State Tracking =====
int decoder_realstate = LOW;              // Current audio state (raw)
int decoder_realstatebefore = LOW;        // Previous raw state
int decoder_filteredstate = LOW;          // Debounced audio state
int decoder_filteredstatebefore = LOW;    // Previous debounced state

// ===== Magnitude Tracking =====
float decoder_magnitude;                  // Magnitude of Goertzel output
int decoder_magnitudelimit = 100;         // Adaptive threshold for signal detection
const int decoder_magnitudelimit_low = 100;  // Minimum threshold

// ===== Timing Variables =====
const int decoder_nbtime = 200;           // Debounce time in microseconds
unsigned long decoder_starttimehigh;      // When HIGH state started
unsigned long decoder_highduration;       // Duration of HIGH state (tone)
unsigned long decoder_hightimesavg = 100000;  // Average HIGH duration (~60ms for 20 WPM)
unsigned long decoder_startttimelow;      // When LOW state started
unsigned long decoder_lowduration;        // Duration of LOW state (silence)
unsigned long decoder_laststarttime = 0;  // Last state change time

// ===== Morse Code Buffer =====
char decoder_code[20] = "";               // Accumulated Morse code string (e.g. ".-")
int decoder_stop = LOW;                   // Flag to prevent multiple detections

// ===== Sampling State Machine =====
int decoder_sampleIndex = 0;              // Current sample index in buffer
bool decoder_readyToProcess = false;      // Flag indicating buffer is full and ready to process

/////////////////////////////////////////////////////////////////
/// DecoderInit() - Initialize decoder at setup
/////////////////////////////////////////////////////////////////
