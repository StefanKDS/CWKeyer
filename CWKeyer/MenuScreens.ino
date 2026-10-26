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
  display.print("Listen Char", 2,4);
  display.print("Give Random", 3,4);
  display.print("Give A-Z", 4,4);
  display.print("Copy Word", 5,4);

  display.print("Back", 6,4);

  display.print(">", 2,1);  // Selection arrow
  char speed[17];
  snprintf(speed, sizeof(speed), "Start%d Now%d", trainerStartWpm, wpm);
  display.print(speed, 1, 0);
  ShowTrainerProgress();
}

/////////////////////////////////////////////////////////////////
/// ShowTrainerHearScreen() - Display Hidden-Character Listening Practice
/////////////////////////////////////////////////////////////////
void ShowTrainerHearScreen()
{
  actual_menu = TRAINER_HEAR_SCREEN;
  selected_menu_item = 1;
  display.clear();
  display.print("Listen Char", 0, 2);
  display.print("Tap=replay", 1, 3);
  display.print("Listen & key", 2, 2);
  display.print("Turn=skip", 3, 3);
  display.print("?", 4,7);
  display.print("Hold=menu", 6, 3);
  ShowTrainerProgress();

  trainerPrompt = String(SelectTrainerCharacter());
  trainerResponse = "";
  startPlayback(trainerPrompt);
  decoderString = "";
}

/////////////////////////////////////////////////////////////////
/// ShowTrainerListenRepeatScreen() - Display Word and Callsign Copy Practice
/////////////////////////////////////////////////////////////////
void ShowTrainerListenRepeatScreen()
{
  actual_menu = TRAINER_LISTEN_REPEAT_SCREEN;
  selected_menu_item = 1;
  display.clear();
  trainerPrompt = SelectTrainerWord();
  display.print(trainerUsingGroup ? "Copy 4-Ch Group" : "Copy Word/Call", 0, 1);
  display.print("Tap=replay", 1, 3);
  display.print("Listen, then key", 2, 0);
  display.print("Turn=skip", 3, 3);
  display.print("Hold=menu", 6, 3);
  ShowTrainerProgress();

  trainerResponse = "";
  startPlayback(trainerPrompt);
  decoderString = "";
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

  currentTrainerLetter = SelectTrainerCharacter();
  trainerPrompt = String(currentTrainerLetter);
  trainerResponse = "";

  display.print("Give Random", 0, 2);
  display.print("Tap=again", 1, 3);
  display.printChar16(currentTrainerLetter, 3, 7);
  display.print("Turn=skip", 5, 3);
  display.print("Hold=menu", 6, 3);
  ShowTrainerProgress();
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

  currentTrainerLetter = 'A' + currentAZPosition;
  trainerPrompt = String(currentTrainerLetter);
  trainerResponse = "";

  display.print("Give A-Z", 0, 4);
  display.print("Tap=again", 1, 3);
  display.printChar16(currentTrainerLetter, 3, 7);
  display.print("Turn=skip", 5, 3);
  display.print("Hold=menu", 6, 3);
  ShowTrainerProgress();
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
