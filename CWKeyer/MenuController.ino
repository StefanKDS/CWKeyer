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
      StartTrainerSession();
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
      EndTrainerSession();
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
    StopTrainerQuestion();
    ShowTrainerScreen();
    return;
  }

  if(actual_menu == TRAINER_LISTEN_REPEAT_SCREEN)
  {
    StopTrainerQuestion();
    ShowTrainerScreen();
    return;
  }

  if(actual_menu == TRAINER_HEAR_SCREEN)
  {
    StopTrainerQuestion();
    ShowTrainerScreen();
    return;
  }

  if(actual_menu == TRAINER_GIVE_AZ_SCREEN)
  {
    StopTrainerQuestion();
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
  if(IsTrainerQuestionActive())
  {
    SkipTrainerQuestion();
    return;
  }

  if(actual_menu == TRAINER)
  {
    int adjustedStartWpm = trainerStartWpm;
    if(r.getDirection() == RE_RIGHT && adjustedStartWpm < TRAINER_MAX_WPM)
      adjustedStartWpm += TRAINER_WPM_STEP;
    else if(r.getDirection() != RE_RIGHT && adjustedStartWpm > TRAINER_MIN_WPM)
      adjustedStartWpm -= TRAINER_WPM_STEP;

    if(adjustedStartWpm != trainerStartWpm)
    {
      trainerStartWpm = adjustedStartWpm;
      EEPROM.write(EEPROM_TRAINER_START_WPM_ADDR, trainerStartWpm);
      EEPROM.commit();

      if(trainerSessionActive)
      {
        wpm = trainerStartWpm;
        fwpm = trainerStartWpm;
        trainerCorrectStreak = 0;
        CalculateTimes(wpm, fwpm);
      }
      ShowTrainerScreen();
    }
    return;
  }

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
