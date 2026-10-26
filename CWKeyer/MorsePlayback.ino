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
