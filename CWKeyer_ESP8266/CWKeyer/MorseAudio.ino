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

