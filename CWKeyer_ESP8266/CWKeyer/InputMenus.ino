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

