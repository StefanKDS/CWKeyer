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
  char letter;

  // Generate 5 random letters
  for(int i=0; i<5; i++)
  {
    int idx = random(0, selectedLetters.length());
    letter = selectedLetters[idx];
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
  display.printChar16(currentTrainerLetter, 3, 7);

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

  display.printChar16(currentTrainerLetter, 3, 7);

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

  display.printChar16(currentTrainerLetter, 3, 7);

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
