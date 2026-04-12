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
void DecoderInit()
{
  // Calculate Goertzel coefficient for target frequency
  int k = (int)(0.5 + ((decoder_n * target_freq) / sampling_freq));
  float omega = (2.0 * PI * k) / decoder_n;
  decoder_coeff = 2.0 * cos(omega);

  DEBUG_PRINTLN("Audio Decoder initialized");
}

/////////////////////////////////////////////////////////////////
/// DecoderUpdate() - Non-blocking decoder update
// Call this in every loop when actual_menu == MONITOR
// Implements a two-phase approach: sampling and processing
/////////////////////////////////////////////////////////////////
void DecoderUpdate()
{
  // ===== Sampling Phase: Collect samples non-blockingly =====
  if (decoder_sampleIndex < decoder_n) 
  {
    decoder_testData[decoder_sampleIndex] = analogRead(audioInPin);
    decoder_sampleIndex++;

    if (decoder_sampleIndex >= decoder_n) 
    {
      decoder_readyToProcess = true;
    }
    return;  // Exit this iteration briefly
  }

  // ===== Processing Phase: Process collected samples =====
  if (decoder_readyToProcess)
  {
    DecoderProcessGoertzel();       // Run Goertzel algorithm on samples
    DecoderUpdateStateLogic();      // Apply hysteresis and debouncing
    DecoderDetectMorse();           // Detect and decode Morse characters

    decoder_sampleIndex = 0;
    decoder_readyToProcess = false;
  }
}

/////////////////////////////////////////////////////////////////
/// DecoderProcessGoertzel() - Goertzel Algorithm
// Implements the Goertzel DFT algorithm to detect target frequency
// in audio samples
/////////////////////////////////////////////////////////////////
void DecoderProcessGoertzel()
{
  // ===== Goertzel DFT Calculation =====
  for (int i = 0; i < decoder_n; i++) 
  {
    float Q0 = decoder_coeff * decoder_Q1 - decoder_Q2 + (float)decoder_testData[i];
    decoder_Q2 = decoder_Q1;
    decoder_Q1 = Q0;
  }

  // ===== Calculate Magnitude =====
  float magnitudeSquared = (decoder_Q1 * decoder_Q1) + (decoder_Q2 * decoder_Q2) - decoder_Q1 * decoder_Q2 * decoder_coeff;
  decoder_magnitude = sqrt(magnitudeSquared);

  // Reset for next iteration
  decoder_Q1 = 0;
  decoder_Q2 = 0;

  // ===== Adaptive Magnitude Limit (IMPORTANT for different speeds) =====
  // Continuously adapt threshold to compensate for varying input levels
  if (decoder_magnitude > decoder_magnitudelimit_low) 
  {
    decoder_magnitudelimit += (decoder_magnitude - decoder_magnitudelimit) / 6;
  }

  if (decoder_magnitudelimit < decoder_magnitudelimit_low)
    decoder_magnitudelimit = decoder_magnitudelimit_low;
}

/////////////////////////////////////////////////////////////////
/// DecoderUpdateStateLogic() - Hysteresis and Debouncing
// Applies threshold with hysteresis and debouncing to stabilize
// state transitions
/////////////////////////////////////////////////////////////////
void DecoderUpdateStateLogic()
{
  unsigned long now = micros();

  // ===== Signal threshold with hysteresis =====
  if (decoder_magnitude > decoder_magnitudelimit * 0.6)
    decoder_realstate = HIGH;
  else
    decoder_realstate = LOW;

  // ===== Detect state change =====
  if (decoder_realstate != decoder_realstatebefore)
    decoder_laststarttime = now;

  // ===== Debouncing: Only update on stable state change =====
  if ((now - decoder_laststarttime) > decoder_nbtime)
    decoder_filteredstate = decoder_realstate;

  decoder_realstatebefore = decoder_realstate;
}

/////////////////////////////////////////////////////////////////
/// DecoderDetectMorse() - Morse Detection
// Measures tone and silence durations to detect dots and dashes
// Recognizes character boundaries and word boundaries by timeout
/////////////////////////////////////////////////////////////////
void DecoderDetectMorse()
{
  unsigned long now = micros();

  if (decoder_filteredstate != decoder_filteredstatebefore) 
  {
    decoder_stop = LOW;

    // ===== Transition from HIGH to LOW: Tone ended =====
    if (decoder_filteredstate == LOW) 
    {
      decoder_startttimelow = now;
      decoder_highduration = now - decoder_starttimehigh;

      // ===== Dot or dash? =====
      if (decoder_highduration < (decoder_hightimesavg * 2)) 
      {
        // Dot detected - learn from the dot duration
        decoder_hightimesavg = (decoder_highduration + decoder_hightimesavg + decoder_hightimesavg) / 3;
        if (strlen(decoder_code) < sizeof(decoder_code) - 1) 
          strcat(decoder_code, ".");
      } 
      else 
      {
        // Dash detected - extrapolate dot length from dash (Dash = 3×Dot)
        unsigned long dit_estimate = decoder_highduration / 3;
        decoder_hightimesavg = (dit_estimate + decoder_hightimesavg + decoder_hightimesavg) / 3;
        if (strlen(decoder_code) < sizeof(decoder_code) - 1) 
          strcat(decoder_code, "-");
      }
    }

    // ===== Transition from LOW to HIGH: Pause after tone =====
    if (decoder_filteredstate == HIGH) 
    {
      decoder_starttimehigh = now;
      decoder_lowduration = now - decoder_startttimelow;

      // ===== Long pause = character separation =====
      if (decoder_lowduration > decoder_hightimesavg * 2) 
      {
        DecoderDecodeMorse();  // Decode accumulated Morse code
        decoder_code[0] = '\0';  // Clear buffer
      }
    }
  }

  // ===== Timeout: Very long pause = word end =====
  if ((now - decoder_startttimelow) > (decoder_highduration * 6) && decoder_stop == LOW) 
  {
    DecoderDecodeMorse();
    decoder_code[0] = '\0';
    decoder_stop = HIGH;
  }

  decoder_filteredstatebefore = decoder_filteredstate;
}

/////////////////////////////////////////////////////////////////
/// DecoderDecodeMorse() - Morse Code to ASCII Conversion
// Decodes accumulated Morse code string to ASCII character
// Supports letters (A-Z), numbers (0-9), and special characters
/////////////////////////////////////////////////////////////////
void DecoderDecodeMorse()
{
  if (strlen(decoder_code) == 0) return;

  char c = ' ';

  // ===== Letters (A-Z) =====
  if (strcmp(decoder_code, ".-") == 0) c = 'A';
  else if (strcmp(decoder_code, "-...") == 0) c = 'B';
  else if (strcmp(decoder_code, "-.-.") == 0) c = 'C';
  else if (strcmp(decoder_code, "-..") == 0) c = 'D';
  else if (strcmp(decoder_code, ".") == 0) c = 'E';
  else if (strcmp(decoder_code, "..-.") == 0) c = 'F';
  else if (strcmp(decoder_code, "--.") == 0) c = 'G';
  else if (strcmp(decoder_code, "....") == 0) c = 'H';
  else if (strcmp(decoder_code, "..") == 0) c = 'I';
  else if (strcmp(decoder_code, ".---") == 0) c = 'J';
  else if (strcmp(decoder_code, "-.-") == 0) c = 'K';
  else if (strcmp(decoder_code, ".-..") == 0) c = 'L';
  else if (strcmp(decoder_code, "--") == 0) c = 'M';
  else if (strcmp(decoder_code, "-.") == 0) c = 'N';
  else if (strcmp(decoder_code, "---") == 0) c = 'O';
  else if (strcmp(decoder_code, ".--.") == 0) c = 'P';
  else if (strcmp(decoder_code, "--.-") == 0) c = 'Q';
  else if (strcmp(decoder_code, ".-.") == 0) c = 'R';
  else if (strcmp(decoder_code, "...") == 0) c = 'S';
  else if (strcmp(decoder_code, "-") == 0) c = 'T';
  else if (strcmp(decoder_code, "..-") == 0) c = 'U';
  else if (strcmp(decoder_code, "...-") == 0) c = 'V';
  else if (strcmp(decoder_code, ".--") == 0) c = 'W';
  else if (strcmp(decoder_code, "-..-") == 0) c = 'X';
  else if (strcmp(decoder_code, "-.--") == 0) c = 'Y';
  else if (strcmp(decoder_code, "--..") == 0) c = 'Z';
  // ===== Numbers (0-9) =====
  else if (strcmp(decoder_code, "-----") == 0) c = '0';
  else if (strcmp(decoder_code, ".----") == 0) c = '1';
  else if (strcmp(decoder_code, "..---") == 0) c = '2';
  else if (strcmp(decoder_code, "...--") == 0) c = '3';
  else if (strcmp(decoder_code, "....-") == 0) c = '4';
  else if (strcmp(decoder_code, ".....") == 0) c = '5';
  else if (strcmp(decoder_code, "-....") == 0) c = '6';
  else if (strcmp(decoder_code, "--...") == 0) c = '7';
  else if (strcmp(decoder_code, "---..") == 0) c = '8';
  else if (strcmp(decoder_code, "----.") == 0) c = '9';
  // ===== Special Characters =====
  else if (strcmp(decoder_code, "..--..") == 0) c = '?';
  else if (strcmp(decoder_code, ".-.-.-") == 0) c = '.';
  else if (strcmp(decoder_code, "--..--") == 0) c = ',';
  else if (strcmp(decoder_code, "-.-.--") == 0) c = '!';
  else if (strcmp(decoder_code, ".--.-.") == 0) c = '@';
  else if (strcmp(decoder_code, "---...") == 0) c = ':';
  else if (strcmp(decoder_code, "-....-") == 0) c = '-';
  else if (strcmp(decoder_code, "-..-.") == 0) c = '/';
  else if (strcmp(decoder_code, "-.--.") == 0) c = '(';
  else if (strcmp(decoder_code, "-.--.-") == 0) c = ')';
  else if (strcmp(decoder_code, ".-...") == 0) c = '&';
  else if (strcmp(decoder_code, "...-..-") == 0) c = '$';
  else if (strcmp(decoder_code, ".-.-.") == 0) c = '+';
  else if (strcmp(decoder_code, "-...-") == 0) c = '=';

  // ===== Display decoded character =====
  if (c != ' ') 
  {
    Serial.print(c);  // Debug output

    // Display character on monitor display
    if (actual_menu == MONITOR)
    {
      if(char_on_screen >= 90)
      {
        char_on_screen = 0;
        display.clear();
      }
      else char_on_screen++;

      int r, c_col;
      CalcDisplayPosition(char_on_screen, &r, &c_col);
      char buf[2] = {c, '\0'};
      display.print(buf, r, c_col);
    }
  }
}

/////////////////////////////////////////////////////////////////
/// notFound() - HTTP 404 Handler
// Sends a 404 error page when client requests non-existent path
/////////////////////////////////////////////////////////////////
void notFound() 
{
  server.send(404, "text/html", wrapInPage("<p>Page not found</p>"));
}

/////////////////////////////////////////////////////////////////
/// handleRoot() - HTTP GET / (Main Page)
// Serves the HTML page when a client calls the root URL
// Loads stored texts and checks which letters are selected for training
/////////////////////////////////////////////////////////////////
void handleRoot() 
{
  String page = index_html;  // Load HTML template
  
  // Read stored texts from EEPROM
  String text1 = ReadTextFromEEPROM(EEPROM_MEM1_ADDR);
  String text2 = ReadTextFromEEPROM(EEPROM_MEM2_ADDR);

  // Replace placeholders in HTML template with current values
  page.replace("%TEXT1%", text1);
  page.replace("%TEXT2%", text2);
  page.replace("%LETTERS_CHECKBOXES%", generateLetterCheckboxes());

  // Send cache control header (prevents old versions on Android)
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");

  // Send HTML page to client
  server.send(200, "text/html", page);
}

/////////////////////////////////////////////////////////////////
/// handleSaveLetters() - HTTP POST /save_letters
// Processes trainer form: saves selected letters to EEPROM
// and updates global variable
/////////////////////////////////////////////////////////////////
void handleSaveLetters() 
{
  String selected = "";
  int args = server.args();  // Number of form parameters

  // Collect all selected letters (parameter name: "letters")
  for (int i = 0; i < args; i++) 
  {
    if (server.argName(i) == "letters") 
    {
      selected += server.arg(i);  // Append letter
    }
  }

  // Save selection to EEPROM and global variable
  WriteTextToEEPROM(0x40, selected);
  selectedLetters = selected;  // Update global variable for trainer

  // Send confirmation response to client
  String content = "<p>Saved: " + selected + "</p>";
  server.send(200, "text/html", wrapInPage(content));
}

/////////////////////////////////////////////////////////////////
/// handleGet() - HTTP GET /get?input1=... or /get?input2=...
// Saves Morse text from web form to EEPROM
// Parameter input1 -> EEPROM_MEM1_ADDR
// Parameter input2 -> EEPROM_MEM2_ADDR
/////////////////////////////////////////////////////////////////
void handleGet() 
{
  String inputParam = "none";  // Which parameter was received

  if (server.hasArg(TEXT_1))    // Check for input1 parameter
  {
    WriteTextToEEPROM(EEPROM_MEM1_ADDR, server.arg(TEXT_1));
    inputParam = TEXT_1;
  } 
  else if (server.hasArg(TEXT_2))  // Check for input2 parameter
  {
    WriteTextToEEPROM(EEPROM_MEM2_ADDR, server.arg(TEXT_2));
    inputParam = TEXT_2;
  }

  String content = "<p>Saved: " + inputParam + "</p>";
  server.send(200, "text/html", wrapInPage(content));
}

/////////////////////////////////////////////////////////////////
/// generateLetterCheckboxes() - Generate HTML for Letter Checkboxes
// Generates HTML form elements for all 26 letters
// Recognizes previously selected letters and marks them as "checked"
/////////////////////////////////////////////////////////////////
String generateLetterCheckboxes() 
{
  String html = "";
  
  // Loop through all 26 letters
  for (int i = 0; i < 26; i++) 
  {
    char letterChar = pgm_read_byte(&LETTERS[i]);  // Read letter from PROGMEM
    String letter = String(letterChar);             // Convert to String
    bool checked = selectedLetters.indexOf(letter) != -1;  // Was this letter selected?

    // Generate checkbox HTML
    html += "<input type='checkbox' name='letters' value='" + letter + "' id='l" + String(i) + "'";
    if (checked) 
      html += " checked";  // Mark as already selected
    html += ">";
    html += "<label for='l" + String(i) + "'>" + letter + "</label> ";

    // New line after 7 letters for better formatting
    if ((i+1) % 7 == 0) 
      html += "<br>";
  }
  return html;
}

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
/// startBeep() - Start a Tone Non-blockingly
// Starts a beep tone with specified length
// Tone is played non-blockingly - function returns immediately
// and updateBeep() manages the tone ending
/////////////////////////////////////////////////////////////////
void startBeep(unsigned int length, char symbol)
{
  noTone(SPEAKER_PIN);        // Ensure no old tone is running
  tone(SPEAKER_PIN, 1000);    // Start 1000 Hz tone on speaker

  if (speakerOn)              // If buzzer is enabled
    digitalWrite(BUZZER_PIN, HIGH);  // Turn on buzzer too

  beeping = true;             // Mark tone as running
  beepEnd = millis() + length;  // Calculate end time
  DEBUG_PRINT(symbol);        // Debug output of symbol
}

/////////////////////////////////////////////////////////////////
/// updateBeep() - Manage Beep Timing (Non-blocking)
// Called in every loop iteration
// Checks if beep should end or pause should start
// Prevents main loop from being blocked
/////////////////////////////////////////////////////////////////
void updateBeep()
{
  unsigned long now = millis();

  // If beep reached its planned end time
  if(beeping && now >= beepEnd)
  {
    noTone(SPEAKER_PIN);      // Turn off speaker tone
    digitalWrite(BUZZER_PIN, LOW);  // Turn off buzzer
    beeping = false;

    // Automatically start pause after tone (1T)
    inPause = true;
    pauseEnd = now + elementGap;  // Pause lasts 1T
  }

  // If pause is finished
  if(inPause && now >= pauseEnd)
  {
    inPause = false;      // Pause ended - next tone can begin
    lastKeyTime = now;    // IMPORTANT: Update lastKeyTime for character recognition
  }
}

/////////////////////////////////////////////////////////////////
/// startPlayback() - Start Text Playback
// Initializes playback of stored text as Morse code
/////////////////////////////////////////////////////////////////
void startPlayback(String text)
{
  playbackText = text;
  playbackIndex = 0;
  currentMorse = "";
  morseIndex = 0;
  playing = true;
  nextActionTime = millis(); // Start immediately
}

/////////////////////////////////////////////////////////////////
/// updatePlayback() - Update Playback of Stored Text (Non-blocking)
// Controls Morse code playback
// 1. Converts characters to Morse code
// 2. Plays dots (dit) and dashes (dah) with correct timing
// 3. Creates correct pauses between elements, letters, and words
/////////////////////////////////////////////////////////////////
void updatePlayback()
{
  if (!playing)           // If no playback is running, do nothing
    return;

  unsigned long now = millis();
  if (now < nextActionTime)  // Wait until time for next action
    return;

  // ===== Load next character when current Morse code is done =====
  if (currentMorse.length() == 0)
  {
    // Check if we've played all characters
    if (playbackIndex >= playbackText.length())
    {
      playing = false;  // Playback finished
      return;
    }

    char c = playbackText[playbackIndex++];  // Next character from text
    if (c == ' ')
    {
      // Word pause: 7T total, but 1T (elementGap) already counted between letters
      // So only add wordExtra (6T extra)
      nextActionTime = now + wordExtra;
      return;
    }
    else
    {
      // Convert letter to Morse code (e.g. 'A' -> ".-")
      currentMorse = EncodeChar(c);
      morseIndex = 0;  // Start from first symbol of Morse code
    }
  }

  // ===== Next symbol (dot or dash) of current letter =====
  if (morseIndex < currentMorse.length())
  {
    char sym = currentMorse[morseIndex++];  // Current symbol ('.' or '-')
    unsigned int len = (sym == '.') ? dit_len : dah_len;  // Length based on symbol
    startBeep(len, sym);  // Start beep
    // After beep: wait for beep length + 1T (elementGap) for next symbol
    nextActionTime = now + len + elementGap;
  }
  else
  {
    // All symbols of letter played
    // Wait for letter pause minus already consumed 1T (elementGap)
    currentMorse = "";  // Clear buffer for next letter
    nextActionTime = now + letterExtra;  // Extra pause before next letter
  }
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

    // ===== Mode Button (Menu Navigation) =====
    if(digitalRead(MODE_BUTTON_PIN) == LOW)
    {
        StateMachine(MODE_BUTTON_PIN);
        delay(250);  // Debounce delay
    }

    // ===== Keyer Input (Paddles) =====
    bool dit = digitalRead(KEYER_SHORT_PIN) == LOW;  // Dot paddle pressed
    bool dah = digitalRead(KEYER_LONG_PIN) == LOW;   // Dash paddle pressed

    static bool lastWasDit = false;
    static bool iambicModeActive = false;

    // Only process if no beep or pause is running
    if(!beeping && !inPause)
    {
        // ===== IAMBIC Mode A: Both paddles pressed =====
        if(dit && dah)
        {
            iambicModeActive = true;

            if(lastWasDit)
            {
                StateMachine(KEYER_LONG_PIN);
                lastWasDit = false;
            }
            else
            {
                StateMachine(KEYER_SHORT_PIN);
                lastWasDit = true;
            }

            lastKeyTime = millis();
            letterProcessed = false;
        }

        // ===== Single Dot =====
        else if(dit && !iambicModeActive)
        {
            StateMachine(KEYER_SHORT_PIN);
            lastWasDit = true;

            lastKeyTime = millis();
            letterProcessed = false;
        }

        // ===== Single Dash =====
        else if(dah && !iambicModeActive)
        {
            StateMachine(KEYER_LONG_PIN);
            lastWasDit = false;

            lastKeyTime = millis();
            letterProcessed = false;
        }

        // ===== IAMBIC Mode B: Last impulse =====
        else if(iambicModeActive)
        {
            iambicModeActive = false;

            // Last opposite tone
            if(lastWasDit)
            {
                StateMachine(KEYER_LONG_PIN);
                lastWasDit = false;
            }
            else
            {
                StateMachine(KEYER_SHORT_PIN);
                lastWasDit = true;
            }

            lastKeyTime = millis();
            letterProcessed = false;
        }
    }
    
    updateBeep();      // Update beep state
    updatePlayback();  // Update playback state

    // ===== Character Recognition for Monitor and Trainer Modes =====
    // Monitor Mode: Display each recognized letter
    // Trainer Mode: Compare input with expected letter
    if(actual_menu == MONITOR || 
       actual_menu == TRAINER_GIVE_RANDOM_SCREEN || 
       actual_menu == TRAINER_GIVE_AZ_SCREEN ||
       actual_menu == TRAINER_LISTEN_REPEAT_SCREEN)
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
            else if(actual_menu == TRAINER_GIVE_RANDOM_SCREEN || 
                    actual_menu == TRAINER_GIVE_AZ_SCREEN ||
                    actual_menu == TRAINER_LISTEN_REPEAT_SCREEN)
            {
                // Check trainer answer
                if(buf[0] == currentTrainerLetter)
                {
                    display.clear();
                    display.print("Correct !", 4, 4);
                }
                else
                {
                    display.clear();
                    display.print("Not Correct !", 4, 2);
                    ProcessSign(currentTrainerLetter);  // Play correct letter
                }

                delay(1000);

                // Load next trainer question
                if(actual_menu == TRAINER_GIVE_RANDOM_SCREEN)
                    ShowTrainerGiveRandomScreen();
                else if(actual_menu == TRAINER_GIVE_AZ_SCREEN)
                    ShowTrainerGiveAZScreen();
                else if(actual_menu == TRAINER_LISTEN_REPEAT_SCREEN)
                    ShowTrainerListenRepeatScreen();
            }
            else if(actual_menu == TRAINER_HEAR_SCREEN)
            {
                if(buf[0] == 'E' || buf[0] == 'T')
                {
                    ShowTrainerHearScreen();
                }
            }

            decoderString = "";  // Clear buffer for next character
        }
    }

    StateMachine(NO_KEY);  // Process state machine with no key
}

/////////////////////////////////////////////////////////////////
/// StateMachine() - Central Input Processing
// Manages interpretation of user inputs:
// - KEYER_SHORT_PIN: Enter dot (dit)
// - KEYER_LONG_PIN: Enter dash (dah)
// - MODE_BUTTON_PIN: Menu navigation and functions
/////////////////////////////////////////////////////////////////
void StateMachine(int key)
{
  // Currently only one state (STATE_IDLE)
  // Could be extended in future (e.g. for menu editing)
  if(State == STATE_IDLE)
  {
    // ===== Short Paddle Button (Dot) =====
    if(key == KEYER_SHORT_PIN)
    {
      // Only process if no tone is running (prevents overlap)
      if(!beeping && !inPause)
      {
        ProcessBeep(dit_len, '.');  // Dot beep with correct length
        decoderString += ".";       // Add dot to decoding buffer
        key_activated = millis();   // Store timestamp
      }
    }

    // ===== Long Paddle Button (Dash) =====
    if(key == KEYER_LONG_PIN)
    {
      // Only process if no tone is running
      if(!beeping && !inPause)
      {
        ProcessBeep(dah_len, '-');  // Dash beep with correct length
        decoderString += "-";       // Add dash to decoding buffer
        key_activated = millis();   // Store timestamp
      }
    }

    // ===== Mode Button (Menu Navigation and Functions) =====
    if(key == MODE_BUTTON_PIN)
    {
      ReactOnButtonClick();  // Call menu handler
    }
  }
}

/////////////////////////////////////////////////////////////////
/// ReactOnButtonClick() - Handle Mode Button Clicks
// Routes button clicks to appropriate menu or action handlers
// Behavior depends on current menu (actual_menu)
/////////////////////////////////////////////////////////////////
void ReactOnButtonClick()
{
  // ===== MAIN_MENU =====
  if(actual_menu == MAIN_MENU)
  {
    DEBUG_PRINTLN("MAIN_MENU");
    
    if(selected_menu_item == CW_KEYER)
    {
      DEBUG_PRINTLN("ShowKeyerScreen");
      ShowKeyerScreen();
      return;
    }

    if(selected_menu_item == SETUP)
    {
      DEBUG_PRINTLN("ShowSetupScreen");
      ShowSetupScreen();
      return;
    }

    if(selected_menu_item == MONITOR)
    {
      DEBUG_PRINTLN("ShowMonitorScreen");
      ShowMonitorScreen();
      return;
    }

    if(selected_menu_item == TRAINER)
    {
      DEBUG_PRINTLN("ShowTrainerScreen");
      ShowTrainerScreen();
      return;
    }
  }

  // ===== CW_KEYER =====
  if(actual_menu == CW_KEYER)
  {
     DEBUG_PRINTLN("KEYER");
     
    if(selected_menu_item == SPEED && speed_mode == false)
    {
      speed_mode = true;  // Enable speed adjustment mode
    }
    else if(selected_menu_item == SPEED && speed_mode == true)
    {
      speed_mode = false;  // Disable speed adjustment mode
    }

    if(selected_menu_item == MEM_1)
    {
      PlayMemory(EEPROM_MEM1_ADDR);
      DEBUG_PRINTLN("Play Mem 1");
      return;
    }

    if(selected_menu_item == MEM_2)
    {
      PlayMemory(EEPROM_MEM2_ADDR);
      DEBUG_PRINTLN("Play Mem 2");
      return;
    }

    if(selected_menu_item == CW_KEYER_BACK)
    {
      ShowMainScreen();
      DEBUG_PRINTLN("Keyer Back");
      return;
    }
  }

  // ===== TRAINER =====
  if(actual_menu == TRAINER)
  {
     DEBUG_PRINTLN("TRAINER");
     
    if (selected_menu_item == TRAINER_HEAR)
    {
      DEBUG_PRINTLN("Trainer Hear");
      ShowTrainerHearScreen();
      return;
    }

    if (selected_menu_item == TRAINER_GIVE_RANDOM)
    {
      DEBUG_PRINTLN("Trainer Give Random");
      ShowTrainerGiveRandomScreen();
      return;
    }

    if (selected_menu_item == TRAINER_GIVE_AZ)
    {
      DEBUG_PRINTLN("Trainer Give A-Z");
      selectedLetters ="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
      currentAZPosition = -1;
      ShowTrainerGiveAZScreen();
      return;
    }

    if (selected_menu_item == TRAINER_LISTEN_REPEAT)
    {
      DEBUG_PRINTLN("Trainer Listen & Repeat");
      ShowTrainerListenRepeatScreen();
      return;
    }

    if(selected_menu_item == TRAINER_BACK)
    {
      DEBUG_PRINTLN("Trainer Back");
      ShowMainScreen();
      return;
    }
  }

  // ===== MONITOR =====
  if(actual_menu == MONITOR)
  {
    ShowMainScreen();
    return;
  }

  // ===== TRAINER Screen Return Handlers =====
  if(actual_menu == TRAINER_GIVE_RANDOM_SCREEN)
  {
    ShowTrainerScreen();
    return;
  }

  if(actual_menu == TRAINER_LISTEN_REPEAT_SCREEN)
  {
    ShowTrainerScreen();
    return;
  }

  if(actual_menu == TRAINER_HEAR_SCREEN)
  {
    ShowTrainerScreen();
    return;
  }

  if(actual_menu == TRAINER_GIVE_AZ_SCREEN)
  {
    selectedLetters = ReadTextFromEEPROM(0x40);  // Restore saved letter selection
    ShowTrainerScreen();
    return;
  }

  // ===== SETUP =====
  if(actual_menu == SETUP)
  {
     DEBUG_PRINTLN("SETUP");
     
    if(selected_menu_item == SETUP_SPEAKER)
    {
      // Toggle speaker on/off
      if(speakerOn == true)
      {
        SwitchSpeaker(false);
        display.print("Speaker OFF", 2,4);
        DEBUG_PRINTLN("Speaker OFF");
      }
      else
      {
        SwitchSpeaker(true);
        display.print("Speaker ON ", 2,4);
        DEBUG_PRINTLN("Speaker ON ");
      }

      return;
    }

    if(selected_menu_item == SETUP_SETTINGS)
    {
      // Toggle WiFi settings on/off
      if(settingsOn == true)
      {
        SwitchSettings(false);
        display.print("Settings OFF", 3,4);
        DEBUG_PRINTLN("Settings OFF");
      }
      else
      {
        SwitchSettings(true);
        display.print("Settings ON ", 3,4);
        DEBUG_PRINTLN("Settings ON ");
      }

      return;
    }

    if(selected_menu_item == SETUP_FARNSWORTH && fspeed_mode == false)
    {
      fspeed_mode = true;  // Enable Farnsworth speed adjustment
    }
    else if(selected_menu_item == SETUP_FARNSWORTH && fspeed_mode == true)
    {
      fspeed_mode = false;  // Disable Farnsworth speed adjustment
    }

    if(selected_menu_item == SETUP_BACK)
    {
      DEBUG_PRINTLN("Setup Back");
      ShowMainScreen();
      return;
    }
  }  
}

/////////////////////////////////////////////////////////////////
/// SwitchSpeaker() - Enable or Disable Buzzer
// Toggles buzzer output on/off and saves status to EEPROM
// Status is restored on next startup
/////////////////////////////////////////////////////////////////
void SwitchSpeaker(byte value)
{
  // Convert byte value to boolean
  if(value == 0)
    speakerOn = false;
  else
    speakerOn = true;

  // Save status to EEPROM
  EEPROM.write(EEPROM_SPEAKER_ADDR, speakerOn);
  EEPROM.commit();  // Write to EEPROM
}

/////////////////////////////////////////////////////////////////
/// SwitchSettings() - Enable or Disable WiFi and Web Server
// Enables WiFi connection and starts access point for
// browser access to change settings (stored texts, trainer letters)
/////////////////////////////////////////////////////////////////
void SwitchSettings(byte value)
{
  if(value == 0) 
  {
    // ===== Turn WiFi and Server OFF =====
    settingsOn = false;

    // Shut down all WiFi components
    server.stop();                  // Stop web server
    WiFi.disconnect(true);          // Disconnect WiFi and disable AP
    WiFi.mode(WIFI_OFF);            // Turn WLAN completely off
    DEBUG_PRINTLN("WiFi/Server OFF");
  }
  else 
  {
    // ===== Turn WiFi and Server ON =====
    settingsOn = true;

    // ===== Start Access Point =====
    WiFi.mode(WIFI_AP);            // Switch to Access Point mode
    WiFi.softAP(ssid, password);    // Create AP with SSID and password

    // Set fixed IP address for the access point
    IPAddress Ip(192, 168, 4, 2);           // AP IP address
    IPAddress NMask(255, 255, 255, 0);     // Network mask
    WiFi.softAPConfig(Ip, Ip, NMask);

    DEBUG_PRINTLN(WiFi.localIP());

    // ===== Register Web Handlers (Routes) =====
    server.on("/", handleRoot);             // GET / -> Show HTML
    server.on("/save_letters", handleSaveLetters);  // POST /save_letters -> Save letters
    server.on("/get", handleGet);           // GET /get?input1=... -> Save Morse text
    server.onNotFound(notFound);            // 404 Handler
    server.begin();                         // Start web server

    DEBUG_PRINTLN("WiFi/Server ON");
  }
}

/////////////////////////////////////////////////////////////////
/// ProcessBeep() - Process Beep Non-blockingly
// Called by StateMachine when paddle button is pressed
// Starts beep tone and stores symbol for decoding
/////////////////////////////////////////////////////////////////
void ProcessBeep(short beepoLength, char outputChar)
{
    tone(SPEAKER_PIN, 1000);   // Start 1000 Hz tone

    // Turn on buzzer if enabled
    if(speakerOn) 
    {
        digitalWrite(BUZZER_PIN, HIGH);
    }

    beeping = true;                      // Mark as active
    beepEnd = millis() + beepoLength;   // Calculate end time
    // Pause after beep is automatically started in updateBeep()
    DEBUG_PRINT(outputChar);  // Debug output
}

/////////////////////////////////////////////////////////////////
/// ProcessSign() - Play Single Character as Morse Code
// Used by trainer to play correct character when user makes mistake
/////////////////////////////////////////////////////////////////
void ProcessSign(char sign)
{
  // Start playback of single character (e.g. 'A' -> ".-")
  startPlayback(String(sign));
}

/////////////////////////////////////////////////////////////////
/// PlayMemory() - Play Stored Text from EEPROM
// Called by CW Keyer menu when user wants to play one of the
// two stored text memories
/////////////////////////////////////////////////////////////////
void PlayMemory(byte addr)
{
  // Read text from EEPROM
  String text = ReadTextFromEEPROM(addr);
  DEBUG_PRINTLN(text);

  // Safety check: only play if text is not empty
  if(text.length() <= 0)
    return;

  // Start non-blocking playback
  startPlayback(text);
}

/////////////////////////////////////////////////////////////////
/// EncodeChar() - Convert Character to Morse Code
// Takes ASCII letter or digit and returns corresponding Morse code
// (e.g. 'A' -> ".-")
// Morse code is stored in PROGMEM (Flash memory)
/////////////////////////////////////////////////////////////////
String EncodeChar(char sign)
{
  char buffer[6]; // Max Morse characters + null terminator (e.g. "-----")

  // ===== Upper and lower case letters =====
  if (sign >= 'a' && sign <= 'z') 
  {
    strcpy_P(buffer, MORSE_LETTERS[sign - 'a']);  // Read from PROGMEM
    return String(buffer);
  } 
  else if (sign >= 'A' && sign <= 'Z') 
  {
    strcpy_P(buffer, MORSE_LETTERS[sign - 'A']);
    return String(buffer);
  } 
  // ===== Digits =====
  else if (sign >= '0' && sign <= '9') 
  {
    strcpy_P(buffer, MORSE_NUMBERS[sign - '0']);
    return String(buffer);
  }

  return "";  // Unknown character
}

/////////////////////////////////////////////////////////////////
/// DecodeMorseCode() - Convert Morse Code to Character
// Takes Morse code string (e.g. ".-") and returns corresponding
// character (e.g. 'A')
// Used by monitor and trainer
/////////////////////////////////////////////////////////////////
String DecodeMorseCode(String code) 
{
  // ===== Search all 26 letters =====
  for (int i = 0; i < 26; i++) 
  {
    char buffer[6];
    strcpy_P(buffer, MORSE_LETTERS[i]);  // Read Morse code from PROGMEM
    if (strcmp(buffer, code.c_str()) == 0)  // Compare with input
    {
      char letterChar = pgm_read_byte(&LETTERS[i]);  // Get letter
      return String(letterChar);
    }
  }

  // ===== Search all 10 digits =====
  for (int i = 0; i < 10; i++) 
  {
    char buffer[6];
    strcpy_P(buffer, MORSE_NUMBERS[i]);
    if (strcmp(buffer, code.c_str()) == 0)
    {
      return String(char('0' + i));  // Convert index to digit
    }
  }

  return "*";  // No match found
}

/////////////////////////////////////////////////////////////////
/// ShowMainScreen() - Display Main Menu Screen
/////////////////////////////////////////////////////////////////
void ShowMainScreen()
{
  actual_menu = MAIN_MENU;
  selected_menu_item = 1;
  display.clear();

  char str[20];
  snprintf(str, sizeof(str), "CWKeyer v%s", VERSION);
  display.print(str, 0, 1);

  display.print("CW-Keyer", 2,4);
  display.print("Monitor", 3,4);
  display.print("Trainer", 4,4);
  display.print("Setup", 5,4);

  // Show WiFi status if enabled
  if(settingsOn == true)
        display.print("-192.168.4.2-", 6,2);
      else
        display.print("             ", 6,2);

   display.print(">", 2,1);  // Selection arrow
}

/////////////////////////////////////////////////////////////////
/// ShowKeyerScreen() - Display CW Keyer Menu Screen
/////////////////////////////////////////////////////////////////
void ShowKeyerScreen()
{
  actual_menu = CW_KEYER;
  selected_menu_item = 1;
  
  display.clear();
  display.print("CW-Keyer", 0,1);

  display.print("Mem 1", 2,4);
  display.print("Mem 2", 3,4);
  display.print("Speed", 4,4);
  display.print("Back", 5,4);

  display.print(">", 2,1);  // Selection arrow

  char string[20];
  snprintf(string, sizeof(string), "Speed: %i WPM", wpm);
  display.print(string, 6,2);
}

/////////////////////////////////////////////////////////////////
/// ShowTrainerScreen() - Display Trainer Mode Menu Screen
/////////////////////////////////////////////////////////////////
void ShowTrainerScreen()
{
  actual_menu = TRAINER;
  selected_menu_item = 1;
  
  display.clear();
  display.print("Trainer", 0,1);
  display.print("Hear", 2,4);
  display.print("Give Random", 3,4);
  display.print("Give A-Z", 4,4);
  display.print("Repeat", 5,4);

  display.print("Back", 6,4);

  display.print(">", 2,1);  // Selection arrow
}

/////////////////////////////////////////////////////////////////
/// ShowTrainerHearScreen() - Display Trainer Hear Mode
// Shows random letters and plays them for user to listen
/////////////////////////////////////////////////////////////////
void ShowTrainerHearScreen()
{
  actual_menu = TRAINER_HEAR_SCREEN;
  selected_menu_item = 1;
  display.clear();

  // Select random letter from active list
  if (selectedLetters.length() == 0) 
  {
    // Fallback: all letters
    selectedLetters ="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  } 

  char buf[6];

  // Generate 5 random letters
  for(int i=0; i<5; i++)
  {
    int idx = random(0, selectedLetters.length());
    char letter = selectedLetters[idx];
    buf[i] = letter;
  }

  buf[5] = '\0';
  startPlayback(String(buf));
  display.print(buf, 4, 5);

  decoderString = "";  // Clear decoder buffer
}

/////////////////////////////////////////////////////////////////
/// ShowTrainerListenRepeatScreen() - Display Trainer Listen & Repeat Mode
// Shows a single letter, plays it, and user must repeat by keying
/////////////////////////////////////////////////////////////////
void ShowTrainerListenRepeatScreen()
{
  actual_menu = TRAINER_LISTEN_REPEAT_SCREEN;
  selected_menu_item = 1;
  display.clear();

  // Select random letter from active list
  if (selectedLetters.length() == 0) 
  {
    // Fallback: all letters
    selectedLetters ="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  }
  
  int idx = random(0, selectedLetters.length());
  char letter = selectedLetters[idx];
  currentTrainerLetter = letter;

  char buf[2] = { currentTrainerLetter, '\0' };
  display.print(buf, 4, 7);

  startPlayback(String(buf));  // Play the letter

  DEBUG_PRINT("SelectedLetters: ");
  DEBUG_PRINTLN(selectedLetters);
  DEBUG_PRINT("CurrentTrainerLetter: ");
  DEBUG_PRINTLN(currentTrainerLetter);

  decoderString = "";  // Clear decoder buffer
}

/////////////////////////////////////////////////////////////////
/// ShowTrainerGiveRandomScreen() - Display Trainer Give Random Mode
// Shows random letter and user must key the Morse code for it
/////////////////////////////////////////////////////////////////
void ShowTrainerGiveRandomScreen()
{
  actual_menu = TRAINER_GIVE_RANDOM_SCREEN;
  selected_menu_item = 1;
  display.clear();

  // Select random letter from active list
  if (selectedLetters.length() == 0) 
  {
    // Fallback: all letters
    selectedLetters ="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  }
  
  int idx = random(0, selectedLetters.length());
  char letter = selectedLetters[idx];
  currentTrainerLetter = letter;

  char buf[2] = { currentTrainerLetter, '\0' };
  display.print(buf, 4, 7);  // Display target letter

  DEBUG_PRINT("SelectedLetters: ");
  DEBUG_PRINTLN(selectedLetters);
  DEBUG_PRINT("CurrentTrainerLetter: ");
  DEBUG_PRINTLN(currentTrainerLetter);

  decoderString = "";  // Clear decoder buffer
}

/////////////////////////////////////////////////////////////////
/// ShowTrainerGiveAZScreen() - Display Trainer Give A-Z Mode
// Shows letters sequentially A-Z and user must key the Morse code
/////////////////////////////////////////////////////////////////
void ShowTrainerGiveAZScreen()
{
  actual_menu = TRAINER_GIVE_AZ_SCREEN;
  selected_menu_item = 1;
  display.clear();

  currentAZPosition++;

  if(currentAZPosition > 25)
    currentAZPosition = 0;  // Loop back to 'A'

  char letter = selectedLetters[currentAZPosition];
  currentTrainerLetter = letter;

  char buf[2] = { currentTrainerLetter, '\0' };
  display.print(buf, 4, 7);  // Display target letter

  DEBUG_PRINT("CurrentTrainerLetter: ");
  DEBUG_PRINTLN(currentTrainerLetter);

  decoderString = "";  // Clear decoder buffer
}

/////////////////////////////////////////////////////////////////
/// ShowSetupScreen() - Display Setup/Configuration Screen
/////////////////////////////////////////////////////////////////
void ShowSetupScreen()
{
  actual_menu = SETUP;
  selected_menu_item = 1;
  
  display.clear();
    
  // Display speaker status
  if(speakerOn == true)
    display.print("Speaker ON  ", 2,4);
  else
    display.print("Speaker OFF", 2,4);

  // Display settings status
  if(settingsOn == true)
    display.print("Settings ON  ", 3,4);
  else
    display.print("Settings OFF", 3,4);
    
  display.print("Farnsworth", 4,4);

  char string[20];
  snprintf(string, sizeof(string), "FW: %i WPM", fwpm);
  display.print(string, 6,2);

  display.print("Back", 5,4);
  display.print(">", 2,1);  // Selection arrow
}

/////////////////////////////////////////////////////////////////
/// ShowMonitorScreen() - Display Morse Monitor (Decoder) Screen
/////////////////////////////////////////////////////////////////
void ShowMonitorScreen()
{
  actual_menu = MONITOR;
  selected_menu_item = 1;
  char_on_screen = 0;
  display.clear();

  // IMPORTANT: Disable web server for optimal timing
  if(settingsOn == true)
  {
    DEBUG_PRINTLN("Disabling WiFi for Monitor Mode");
    SwitchSettings(0);
  }
}

/////////////////////////////////////////////////////////////////
/// CalculateTimes() - Calculate Morse Timing Constants
// Calculates timing based on WPM (words per minute)
// Supports Farnsworth timing for slow-speed training
/////////////////////////////////////////////////////////////////
void CalculateTimes(char wpm, char fwpm)
{
  if (wpm <= 0) 
    wpm = 1;
  if (fwpm > wpm) 
    fwpm = wpm;  // Farnsworth must not be faster than regular WPM

  unsigned int T = 1200 / wpm;    // Basic time unit (element length)
  unsigned int Tfw = 1200 / fwpm; // Farnsworth basis (extended pauses)

  // ===== Calculate global timing constants =====
  T_unit    = T;                // Store basic time for reference
  dit_len   = T;                // Dot = 1T
  dah_len   = 3 * T;            // Dash = 3T
  elementGap = T;               // Pause between dot/dash = 1T

  // ===== Farnsworth Pauses (extended) =====
  // Letter pause: Total 3T in Farnsworth
  // Since 1T (elementGap) after last element is already counted,
  // we only add the difference (2T extra)
  letterExtra = (3 * Tfw) - elementGap;  // Extra pause after letter

  // Word pause: Total 7T in Farnsworth
  // Since 1T (elementGap) is already counted, add difference (6T extra)
  wordExtra = (7 * Tfw) - elementGap;    // Extra pause after word

  DEBUG_PRINT("T: "); DEBUG_PRINTLN(T);
  DEBUG_PRINT("Tfw: "); DEBUG_PRINTLN(Tfw);
  DEBUG_PRINT("dit: "); DEBUG_PRINTLN(dit_len);
  DEBUG_PRINT("dah: "); DEBUG_PRINTLN(dah_len);
  DEBUG_PRINT("elementGap: "); DEBUG_PRINTLN(elementGap);
  DEBUG_PRINT("letterExtra: "); DEBUG_PRINTLN(letterExtra);
  DEBUG_PRINT("wordExtra: "); DEBUG_PRINTLN(wordExtra);
}

/////////////////////////////////////////////////////////////////
/// WriteTextToEEPROM() - Save Text String to EEPROM
// Writes a string starting at specified address to EEPROM
// String is null-terminated to mark end (standard C convention)
// Remaining memory is filled with '@' as padding marker
/////////////////////////////////////////////////////////////////
void WriteTextToEEPROM(byte addr, String text)
{
  // Write each character of the string
  for (int i = 0; i < text.length(); i++)
  {
    EEPROM.write(addr, text[i]);  // Write character
    addr += 1;                      // Next EEPROM address
  }
  EEPROM.write(addr, 0);           // Write null terminator (string end)
  addr += 1;

  // Optional padding: Fill rest with '@' (recognition marker)
  // Prevents old data from being appended when reading
  for (int i = addr; i < 128; i++)
  {
    EEPROM.write(i, '@');
  }
  EEPROM.commit();  // Write all changes to EEPROM
}

/////////////////////////////////////////////////////////////////
/// ReadTextFromEEPROM() - Read Text String from EEPROM
// Reads from specified EEPROM address until null terminator (0x00)
// or padding character ('@') is found
// Returns the read string
/////////////////////////////////////////////////////////////////
String ReadTextFromEEPROM(byte addr)
{
  String retVal;  // Result string
  
  for (int i = addr; i < 128; i++)   // Read until EEPROM boundary
  {
    byte readValue = EEPROM.read(i);  // Read byte from EEPROM
    if (readValue == 0)               // Null terminator = string end
      break;
    char readValueChar = char(readValue);
    if(readValueChar != '@')          // Ignore padding characters
      retVal += readValueChar;        // Append character to string
  }
  DEBUG_PRINTLN(retVal);  // Debug output
  return retVal;
}

/////////////////////////////////////////////////////////////////
/// DisplaySelectionArrow() - Display Menu Selection Arrow
// Removes old arrow and places it at position of current menu item
// Enables visual navigation through menu options
/////////////////////////////////////////////////////////////////
void DisplaySelectionArrow()
{
  // Remove arrow from all possible positions (rows 2-6)
  display.print(" ", 2,1);
  display.print(" ", 3,1);
  display.print(" ", 4,1);
  display.print(" ", 5,1);
  display.print(" ", 6,1);

  // Place arrow at current menu position
  // (selected_menu_item + 1, since menu items start at 1 but display rows at 0)
  display.print(">", selected_menu_item+1, 1);
}

/////////////////////////////////////////////////////////////////
/// rotate() - Handle Rotary Encoder Rotation
// Called each time encoder is rotated
// Behavior depends on current menu:
// - CW_KEYER menu: Changes speed (WPM) if speed_mode=true
// - SETUP menu: Changes Farnsworth speed if fspeed_mode=true
// - Other menus: Navigate through menu options
/////////////////////////////////////////////////////////////////
void rotate(Rotary& r) 
{
  // ===== Set Speed in Keyer Menu =====
  if(actual_menu == CW_KEYER && speed_mode == true)
  {
    // Increase or decrease WPM based on encoder direction
    if(r.getDirection() == 1)      // Turn right = higher speed
    {
         wpm += STEP_SIZE;  // Increase by STEP_SIZE (e.g. 5 WPM)
    }
    else                             // Turn left = lower speed
    {
      if(wpm > 0)
      {
        wpm -= STEP_SIZE;   // Decrease by STEP_SIZE
      }
    }

    fwpm = wpm;  // Keep Farnsworth speed in sync

    // Update display with new WPM
    char string[20];
    snprintf(string, sizeof(string), "Speed: %i WPM", wpm);
    display.print(string, 6,2);

    CalculateTimes(wpm, fwpm);  // Recalculate all timing
    EEPROM.write(EEPROM_WPM_ADDR, wpm);  // Save to EEPROM
    EEPROM.commit();
    return;
  }

  // ===== Set Farnsworth Speed in Setup Menu =====
  if(actual_menu == SETUP && fspeed_mode == true)
  {
    if(r.getDirection() == 1)       // Turn right
    {
         fwpm += STEP_SIZE;
    }
    else                             // Turn left
    {
      if(wpm > 0)  // Check regular WPM, not fwpm (possible bug?)
      {
        fwpm -= STEP_SIZE;
      }
    }

    char string[20];
    snprintf(string, sizeof(string), "FW: %i WPM", fwpm);
    display.print(string, 6,2);

    CalculateTimes(wpm, fwpm);  // Recalculate all timing
    EEPROM.write(EEPROM_FWPM_ADDR, fwpm);  // Save to EEPROM
    EEPROM.commit();
    return;
  }

  // ===== Menu Navigation =====
  // Navigate in Keyer menu up/down
  if(actual_menu == CW_KEYER)
  {
    if(r.getDirection() == 1)  // Turn right = menu down
    {
      selected_menu_item++;  // Move to next menu item

      if(selected_menu_item > KEYER_MENU_COUNT)  // Wrap: if at end, go to start
        selected_menu_item = 1;
    }
    else                         // Turn left = menu up
    {
      selected_menu_item--;
      if(selected_menu_item < 1)
        selected_menu_item = KEYER_MENU_COUNT;  // Wrap
    }

    DisplaySelectionArrow();  // Show new position
    return;
  }

  // Navigate in main menu (wraps in both directions)
  if(actual_menu == MAIN_MENU)
  {
    if(r.getDirection() == 1)  // Turn right = menu down
    {
      selected_menu_item++;

      if(selected_menu_item > MAIN_MENU_COUNT)
        selected_menu_item = 1;  // Wrap
    }
    else                         // Turn left = menu up
    {
      selected_menu_item--;
      if(selected_menu_item < 1)
        selected_menu_item = MAIN_MENU_COUNT;  // Wrap
    }

    DisplaySelectionArrow();
    return;
  }

  // Navigate in Setup menu
  if(actual_menu == SETUP)
  {
    if(r.getDirection() == 1)  // Turn right = menu down
    {
      selected_menu_item++;

      if(selected_menu_item > SETUP_MENU_COUNT)
        selected_menu_item = 1;
    }
    else                         // Turn left = menu up
    {
      selected_menu_item--;
      if(selected_menu_item < 1)
        selected_menu_item = SETUP_MENU_COUNT;  // Wrap
    }

    DisplaySelectionArrow();
    return;
  }

  // Navigate in Trainer menu
  if(actual_menu == TRAINER)
  {
    if(r.getDirection() == 1)  // Turn right = menu down
    {
      selected_menu_item++;

      if(selected_menu_item > TRAINER_MENU_COUNT)
        selected_menu_item = 1;
    }
    else                         // Turn left = menu up
    {
      selected_menu_item--;
      if(selected_menu_item < 1)
        selected_menu_item = TRAINER_MENU_COUNT;  // Wrap
    }

    DisplaySelectionArrow();
    return;
  }
}
