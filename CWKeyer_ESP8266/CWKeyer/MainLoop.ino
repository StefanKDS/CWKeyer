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
    // Paddle-Eingaenge entprellen: Rohpegel muss 20 ms stabil sein,
    // bevor eine neue Betätigung erkannt wird. Ohne Entprellung kann
    // Kontaktprellen mehrere Dit/Dah-Ereignisse aus einem Tipp erzeugen.
    bool rawDit = (digitalRead(KEYER_SHORT_PIN) == LOW);
    bool rawDah = (digitalRead(KEYER_LONG_PIN) == LOW);

    static bool stableDit = false;
    static bool stableDah = false;
    static bool previousRawDit = false;
    static bool previousRawDah = false;
    static unsigned long ditChangeTime = 0;
    static unsigned long dahChangeTime = 0;
    static bool lastWasDit = false;
    static bool pendingDit = false;
    static bool pendingDah = false;

    unsigned long inputNow = millis();
    if (rawDit != previousRawDit) {
        previousRawDit = rawDit;
        ditChangeTime = inputNow;
    }
    if (rawDah != previousRawDah) {
        previousRawDah = rawDah;
        dahChangeTime = inputNow;
    }

    bool newDitPress = false;
    bool newDahPress = false;
    if (rawDit != stableDit && (unsigned long)(inputNow - ditChangeTime) >= 20) {
        stableDit = rawDit;
        newDitPress = stableDit;  // nur stabile Druck-Flanke, nicht Loslassen
    }
    if (rawDah != stableDah && (unsigned long)(inputNow - dahChangeTime) >= 20) {
        stableDah = rawDah;
        newDahPress = stableDah;  // nur stabile Druck-Flanke, nicht Loslassen
    }

    if(newDitPress) pendingDit = true;
    if(newDahPress) pendingDah = true;

    // Gespeicherte kurze Tastendruecke erst nach Ton UND normaler
    // Elementpause senden. Ein gehaltenes Paddle (stableDit/stableDah)
    // wird ebenfalls beruecksichtigt und dadurch nach jeder Elementpause
    // wiederholt. Kurze Taps bleiben ueber pendingDit/pendingDah gespeichert.
    bool wantDit = pendingDit || stableDit;
    bool wantDah = pendingDah || stableDah;

    if(!beeping && !inPause)
    {
        if(wantDit && wantDah)
        {
            // Bei beiden aktiven Paddles abwechseln.
            if(lastWasDit)
            {
                StateMachine(KEYER_LONG_PIN);
                pendingDah = false;
                lastWasDit = false;
            }
            else
            {
                StateMachine(KEYER_SHORT_PIN);
                pendingDit = false;
                lastWasDit = true;
            }
            lastKeyTime = millis();
            letterProcessed = false;
        }
        else if(wantDit)
        {
            StateMachine(KEYER_SHORT_PIN);
            pendingDit = false;
            lastWasDit = true;
            lastKeyTime = millis();
            letterProcessed = false;
        }
        else if(wantDah)
        {
            StateMachine(KEYER_LONG_PIN);
            pendingDah = false;
            lastWasDit = false;
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

