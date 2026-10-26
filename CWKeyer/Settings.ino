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
