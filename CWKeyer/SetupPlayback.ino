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

