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
void StartTrainerSession();
void EndTrainerSession();
char SelectTrainerCharacter();
String SelectTrainerWord();
void SubmitTrainerCharacter(char answer);
void ShowTrainerProgress();
void StopTrainerQuestion();
void StartNextTrainerQuestion();
void ReplayTrainerQuestion();
void SkipTrainerQuestion();
bool IsTrainerQuestionActive();

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
const char TRAINER_CHARACTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789?.,!@:-/()&$+=";
String trainerPrompt = "";
String trainerResponse = "";
String trainerRetryQueue = "";
bool trainerUsingGroup = false;
unsigned long trainerSessionCorrect = 0;
unsigned long trainerSessionAttempts = 0;
unsigned long trainerSessionSkipped = 0;
unsigned int trainerCorrectStreak = 0;
bool trainerAdvancePending = false;
bool trainerSpeedIncreased = false;
bool modeButtonDown = false;
bool modeButtonHandled = false;
unsigned long modeButtonPressedAt = 0;
unsigned long trainerAdvanceAt = 0;
byte trainerStartWpm = TRAINER_MIN_WPM;
char trainerPreviousWpm = START_POS;
char trainerPreviousFwpm = START_POS;
bool trainerSessionActive = false;

// ===== State Management =====
int State = STATE_IDLE;                  // Main application state

/////////////////////////////////////////////////////////////////
/// setup() - Arduino Initialization
// Called once at startup
// Initializes all hardware components and variables:
// - EEPROM (load stored WPM, texts, letters)
// - Display (OLED)
// - Input pins (Keyer, Mode button)
// - Audio outputs (Buzzer, Speaker)
// - WiFi/WebServer (optional)
/////////////////////////////////////////////////////////////////
void setup() 
{
  DEBUG_BEGIN(SERIAL_SPEED);  // Serial debug output (if DEBUG_PRINT is defined)

  // ===== WiFi and Server initially OFF =====
  SwitchSettings(0);

  // ===== EEPROM Initialization =====
  DEBUG_PRINTLN("- EEPROM INIT -");
  EEPROM.begin(EEPROM_SIZE);

  byte savedTrainerStartWpm = EEPROM.read(EEPROM_TRAINER_START_WPM_ADDR);
  if(savedTrainerStartWpm >= TRAINER_MIN_WPM &&
     savedTrainerStartWpm <= TRAINER_MAX_WPM &&
     (savedTrainerStartWpm - TRAINER_MIN_WPM) % TRAINER_WPM_STEP == 0)
  {
    trainerStartWpm = savedTrainerStartWpm;
  }

  // ===== Load WPM from EEPROM =====
  byte value;
  value = EEPROM.read(EEPROM_WPM_ADDR);
  DEBUG_PRINT("Read wpm: ");
  DEBUG_PRINTLN(value, DEC);

  // Validate WPM: Must be between 0 and 99
  if(value >= 100 || value < 0)
  {
    wpm = START_POS;  // Use default value if invalid
  }
  else
  {
    wpm = value;  // Use stored value
  }

  // ===== Load Farnsworth WPM from EEPROM =====
  value = EEPROM.read(EEPROM_FWPM_ADDR);
  DEBUG_PRINT("Read fwpm: ");
  DEBUG_PRINTLN(value, DEC);
  if(value >= 100 || value < 0)
  {
    fwpm = START_POS;
  }
  else
  {
    fwpm = value;
  }

  // Calculate all timing constants based on WPM
  CalculateTimes(wpm, fwpm);

  // Load stored texts from EEPROM (for memories 1 and 2)
  ReadTextFromEEPROM(EEPROM_MEM1_ADDR);
  ReadTextFromEEPROM(EEPROM_MEM2_ADDR);

  // ===== Load Speaker Status from EEPROM =====
  value = EEPROM.read(EEPROM_SPEAKER_ADDR);
  DEBUG_PRINT("Read Speaker setup: ");
  DEBUG_PRINTLN(value, DEC);
  SwitchSpeaker((byte)value);

  // Load letter selection for trainer
  selectedLetters = ReadTextFromEEPROM(0x40);
  if (selectedLetters.length() == 0)
    selectedLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  // ===== OLED Display Initialization =====
  DEBUG_PRINTLN("- DISPLAY INIT -");
  display.begin();   // Start OLED display
  ShowMainScreen();  // Show startup screen

  // ===== Rotary Encoder Initialization =====
  r.begin(ROTARY_PIN2, ROTARY_PIN1, CLICKS_PER_STEP);
  r.setChangedHandler(rotate);  // Set callback for rotation

  // ===== Buzzer Pin Initialization =====
  DEBUG_PRINTLN("- BUZZER INIT -");
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, 0);  // Buzzer start disabled

  // ===== Keyer Buttons Initialization =====
  DEBUG_PRINTLN("- KEYER INIT -");
  pinMode(KEYER_SHORT_PIN, INPUT_PULLUP);  // Dot paddle
  pinMode(KEYER_LONG_PIN, INPUT_PULLUP);   // Dash paddle

  // ===== Mode Button Initialization =====
  DEBUG_PRINTLN("- BUTTON INIT -");
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

  // ===== Speaker Pin Initialization =====
  pinMode(SPEAKER_PIN, OUTPUT);  // For tone output (frequency via tone())

  // ===== Audio Decoder Initialization =====
  DEBUG_PRINTLN("- AUDIO DECODER INIT -");
  DecoderInit();
}

/////////////////////////////////////////////////////////////////
/// CalcDisplayPosition() - Calculate Display Position from Character Count
// Converts linear character index to row and column on OLED display
// Assumes 15 characters per row (0-14)
/////////////////////////////////////////////////////////////////
void CalcDisplayPosition( short chars_on_display, int* r, int* c )
{
  *r = chars_on_display / 15;  // Row (0-7)
  *c = chars_on_display % 15;  // Column (0-14)
}

/////////////////////////////////////////////////////////////////
/// loop() - Main Loop
// Runs continuously
// 1. Processes user inputs (buttons, encoder, mode button)
// 2. Updates beep and playback timing
// 3. Decodes Morse code and displays results
// 4. Manages web interface (if enabled)
/////////////////////////////////////////////////////////////////
void loop() 
{
    // ===== State Flag for Character Recognition =====
    // Prevents character from being processed multiple times when
    // pause after character has elapsed
    static bool letterProcessed = false;

    // ===== Audio Decoder - TIME CRITICAL =====
    // Called with HIGH PRIORITY before server/UI processing
    if (actual_menu == MONITOR)
    {
      DecoderUpdate();  // Non-blocking audio decoding
    }

    // Handle web server requests (only when not in monitor mode)
    if (settingsOn && actual_menu != MONITOR) 
    {
        server.handleClient();
    }

    r.loop();  // Update rotary encoder state

    // Short press replays a trainer prompt; a long press returns to the trainer menu.
    bool modeButtonPressed = digitalRead(MODE_BUTTON_PIN) == LOW;
    if(modeButtonPressed)
    {
        if(!modeButtonDown)
        {
            modeButtonDown = true;
            modeButtonHandled = false;
            modeButtonPressedAt = millis();
        }

        if(IsTrainerQuestionActive() &&
           !modeButtonHandled &&
           millis() - modeButtonPressedAt >= 1000)
        {
            StopTrainerQuestion();
            ShowTrainerScreen();
            modeButtonHandled = true;
        }
    }
    else if(modeButtonDown)
    {
        unsigned long pressDuration = millis() - modeButtonPressedAt;
        if(!modeButtonHandled && pressDuration >= 30)
        {
            if(IsTrainerQuestionActive())
              ReplayTrainerQuestion();
            else
              StateMachine(MODE_BUTTON_PIN);
        }
        modeButtonDown = false;
    }

    // ===== Keyer Input (Paddles) =====
    // Paddle-Eingaenge entprellen: Rohpegel muss 20 ms stabil sein,
    // bevor eine neue Betätigung erkannt wird. Ohne Entprellung kann
    // Kontaktprellen mehrere Dit/Dah-Ereignisse aus einem Tipp erzeugen.
    bool rawDit = (digitalRead(KEYER_SHORT_PIN) == LOW);
    bool rawDah = (digitalRead(KEYER_LONG_PIN) == LOW);

    static bool stableDit = false;
    static bool stableDah = false;
    static bool previousRawDit = false;
    static bool previousRawDah = false;
    static unsigned long ditChangeTime = 0;
    static unsigned long dahChangeTime = 0;
    static bool lastWasDit = false;
    static bool pendingDit = false;
    static bool pendingDah = false;

    unsigned long inputNow = millis();
    if (rawDit != previousRawDit) {
        previousRawDit = rawDit;
        ditChangeTime = inputNow;
    }
    if (rawDah != previousRawDah) {
        previousRawDah = rawDah;
        dahChangeTime = inputNow;
    }

    bool newDitPress = false;
    bool newDahPress = false;
    if (rawDit != stableDit && (unsigned long)(inputNow - ditChangeTime) >= 20) {
        stableDit = rawDit;
        newDitPress = stableDit;  // nur stabile Druck-Flanke, nicht Loslassen
    }
    if (rawDah != stableDah && (unsigned long)(inputNow - dahChangeTime) >= 20) {
        stableDah = rawDah;
        newDahPress = stableDah;  // nur stabile Druck-Flanke, nicht Loslassen
    }

    bool waitingForTrainerAudio =
      playing && (actual_menu == TRAINER_HEAR_SCREEN ||
                  actual_menu == TRAINER_LISTEN_REPEAT_SCREEN);
    if(newDitPress && !waitingForTrainerAudio && !trainerAdvancePending)
      pendingDit = true;
    if(newDahPress && !waitingForTrainerAudio && !trainerAdvancePending)
      pendingDah = true;

    // Gespeicherte kurze Tastendruecke erst nach Ton UND normaler
    // Elementpause senden. Ein gehaltenes Paddle (stableDit/stableDah)
    // wird ebenfalls beruecksichtigt und dadurch nach jeder Elementpause
    // wiederholt. Kurze Taps bleiben ueber pendingDit/pendingDah gespeichert.
    bool wantDit = pendingDit || stableDit;
    bool wantDah = pendingDah || stableDah;

    if(!beeping && !inPause && !waitingForTrainerAudio && !trainerAdvancePending)
    {
        if(wantDit && wantDah)
        {
            // Bei beiden aktiven Paddles abwechseln.
            if(lastWasDit)
            {
                StateMachine(KEYER_LONG_PIN);
                pendingDah = false;
                lastWasDit = false;
            }
            else
            {
                StateMachine(KEYER_SHORT_PIN);
                pendingDit = false;
                lastWasDit = true;
            }
            lastKeyTime = millis();
            letterProcessed = false;
        }
        else if(wantDit)
        {
            StateMachine(KEYER_SHORT_PIN);
            pendingDit = false;
            lastWasDit = true;
            lastKeyTime = millis();
            letterProcessed = false;
        }
        else if(wantDah)
        {
            StateMachine(KEYER_LONG_PIN);
            pendingDah = false;
            lastWasDit = false;
            lastKeyTime = millis();
            letterProcessed = false;
        }
    }
    
    updateBeep();      // Update beep state
    updatePlayback();  // Update playback state

    if(trainerAdvancePending &&
       millis() >= trainerAdvanceAt &&
       !playing && !beeping && !inPause)
    {
        StartNextTrainerQuestion();
    }

    // ===== Character Recognition for Monitor and Trainer Modes =====
    if(actual_menu == MONITOR || 
       actual_menu == TRAINER_GIVE_RANDOM_SCREEN || 
       actual_menu == TRAINER_GIVE_AZ_SCREEN ||
       actual_menu == TRAINER_LISTEN_REPEAT_SCREEN ||
       actual_menu == TRAINER_HEAR_SCREEN)
    {
        unsigned long now = millis();

        // ===== Trigger: Character was fully entered =====
        // Checks:
        // 1. decoderString has data (at least one dot/dash)
        // 2. No beep or pause is running
        // 3. Enough time has passed since last input (letterExtra)
        // 4. Character has not been processed yet (letterProcessed = false)
        if(decoderString.length() > 0 && 
           !beeping && 
           !inPause && 
           (now - lastKeyTime) >= letterExtra &&
           !letterProcessed)
        {
            letterProcessed = true;  // Prevent processing until next character

            String decodedLetter = DecodeMorseCode(decoderString);
            char buf[2] = { decodedLetter[0], '\0' };

            if(actual_menu == MONITOR)
            {
                // Display decoded character on monitor
                if(char_on_screen >= 90)
                {
                    char_on_screen = 0;
                    display.clear();
                }
                else char_on_screen++;

                int r, c;
                CalcDisplayPosition(char_on_screen, &r, &c);
                display.print(buf, r, c);
            }
            else
            {
                SubmitTrainerCharacter(buf[0]);
            }

            decoderString = "";  // Clear buffer for next character
        }
    }

    StateMachine(NO_KEY);  // Process state machine with no key
}
