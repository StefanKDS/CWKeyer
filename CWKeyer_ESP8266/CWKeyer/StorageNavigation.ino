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

