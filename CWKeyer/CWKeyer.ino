#include <ESP8266WiFi.h> 
#include <ESP8266WebServer.h>
#include "WebPage.h"
#include <Wire.h>
#include "Rotary.h"
#include "ESPOLED.h"
#include "MorseCode.h"
#include <EEPROM.h>
#include "Defines.h"

/////////////////////////////////////////////////////////////////
// DEKLARATIONEN
/////////////////////////////////////////////////////////////////

void CalculateTimes(char wpm);
String ReadTextFromEEPROM(byte addr);
void SwitchSpeaker(byte value);
void WriteTextToEEPROM(byte addr, String text);
void ShowMainScreen();
void ShowSetupScreen();
void ShowMonitorScreen();
void ShowTrainerScreen();
void ShowKeyerScreen();
void rotate(Rotary& r);
void ReactOnButtonClick();
void ProcessBeep(short beepoLength, char outputChar);
void ProcessSign(char sign);
void startBeep(unsigned int length, char symbol);
void updateBeep();
void startPlayback(String text);
void updatePlayback();

/////////////////////////////////////////////////////////////////
// Objects
/////////////////////////////////////////////////////////////////
Rotary r;
OLED display(SDA, SCL, 0x3c, 0);
ESP8266WebServer server(80);

/////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////

// Non-blocking Beep / Playback state
bool beeping = false;
unsigned long beepEnd = 0;

bool inPause = false;
unsigned long pauseEnd = 0;

unsigned long lastKeyTime = 0; // Zeit der letzten Eingabe
unsigned int letterGap = 3;    // 3T Pause = Buchstabenende


bool playing = false;
String playbackText = "";
int playbackIndex = 0;
String currentMorse = "";
int morseIndex = 0;
unsigned long nextActionTime = 0;

// Timing (T-basierte)
unsigned int T_unit = 0;
unsigned int dit_len = 0;
unsigned int dah_len = 0;
unsigned int elementGap = 0; // entspricht inter-element gap (1T)
unsigned int letterExtra = 0;
unsigned int wordExtra = 0;

char wpm = START_POS;
bool speed_mode = false;
bool speakerOn = true;
bool settingsOn = false;
double key_activated;
short char_on_screen = -1;

byte selected_menu_item = SETUP_SPEAKER;
byte actual_menu = MAIN_MENU;

const char* ssid     = "CWKeyer";
const char* password = "123456789";
const char* TEXT_1 = "input1";
const char* TEXT_2 = "input2";

String decoderString;
String selectedLetters = ""; // Globale Variable für die Auswahl
char currentTrainerLetter;
short currentAZPosition = -1;

int State = STATE_IDLE;

/////////////////////////////////////////////////////////////////
// notFound
/////////////////////////////////////////////////////////////////
void notFound() 
{
  server.send(404, "text/html", wrapInPage("<p>Seite nicht gefunden</p>"));
}

/////////////////////////////////////////////////////////////////
// handleRoot
/////////////////////////////////////////////////////////////////
void handleRoot() 
{
  String page = index_html;
  String text1 = ReadTextFromEEPROM(EEPROM_MEM1_ADDR);
  String text2 = ReadTextFromEEPROM(EEPROM_MEM2_ADDR);

  page.replace("%TEXT1%", text1);
  page.replace("%TEXT2%", text2);
  page.replace("%LETTERS_CHECKBOXES%", generateLetterCheckboxes());

  // Cache-Header hinzufügen, damit Android nicht alte Versionen anzeigt
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");

  server.send(200, "text/html", page);
}

/////////////////////////////////////////////////////////////////
// handleSaveLetters
/////////////////////////////////////////////////////////////////
void handleSaveLetters() 
{
  String selected = "";
  int args = server.args();
  for (int i = 0; i < args; i++) 
  {
    if (server.argName(i) == "letters") 
    {
      selected += server.arg(i);
    }
  }
  WriteTextToEEPROM(0x40, selected);
  selectedLetters = selected;

  String content = "<p>Saved: " + selected + "</p>";
  server.send(200, "text/html", wrapInPage(content));
}

/////////////////////////////////////////////////////////////////
// handleGet
/////////////////////////////////////////////////////////////////
void handleGet() 
{
  String inputParam = "none";
  if (server.hasArg(TEXT_1)) 
  {
    WriteTextToEEPROM(EEPROM_MEM1_ADDR, server.arg(TEXT_1));
    inputParam = TEXT_1;
  } 
  else if (server.hasArg(TEXT_2)) 
  {
    WriteTextToEEPROM(EEPROM_MEM2_ADDR, server.arg(TEXT_2));
    inputParam = TEXT_2;
  }
  String content = "<p>Saved: " + inputParam + "</p>";
  server.send(200, "text/html", wrapInPage(content));
}

/////////////////////////////////////////////////////////////////
// generateLetterCheckboxes
/////////////////////////////////////////////////////////////////
String generateLetterCheckboxes() 
{
  String html = "";
  for (int i = 0; i < 26; i++) 
  {
    char letterChar = pgm_read_byte(&LETTERS[i]); // korrekt aus PROGMEM lesen
    String letter = String(letterChar);          // in String umwandeln
    bool checked = selectedLetters.indexOf(letter) != -1;
    html += "<input type='checkbox' name='letters' value='" + letter + "' id='l" + String(i) + "'";
    if (checked) 
      html += " checked";
    html += ">";
    html += "<label for='l" + String(i) + "'>" + letter + "</label> ";
    if ((i+1) % 7 == 0) 
      html += "<br>";
  }
  return html;
}

/////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////
void setup() 
{
  DEBUG_BEGIN(SERIAL_SPEED);

  // Wifi und server off
  SwitchSettings(0);

  // EEPROM
  DEBUG_PRINTLN("- EEPROM INIT -");
  EEPROM.begin(EEPROM_SIZE);
  
  byte value;
  value = EEPROM.read(EEPROM_WPM_ADDR);
  DEBUG_PRINT("Read wpm: ");
  DEBUG_PRINTLN(value, DEC);

  if(value >= 100 || value < 0)
  {
    wpm = START_POS;
  }
  else
  {
    wpm = value;
  }
  CalculateTimes(wpm);

  ReadTextFromEEPROM(EEPROM_MEM1_ADDR);
  ReadTextFromEEPROM(EEPROM_MEM2_ADDR);

  value = EEPROM.read(EEPROM_SPEAKER_ADDR);
  DEBUG_PRINT("Read Speaker setup: ");
  DEBUG_PRINTLN(value, DEC);

  SwitchSpeaker((byte)value);

  selectedLetters = ReadTextFromEEPROM(0x40); // Auswahl laden

  // OLED Init 
  DEBUG_PRINTLN("- BUZZER INIT -");
  // Initialize display
  display.begin();

  ShowMainScreen();

  // Rotary Init
  r.begin(ROTARY_PIN1, ROTARY_PIN2, CLICKS_PER_STEP);
  r.setChangedHandler(rotate);

  // Beep Init 
  DEBUG_PRINTLN("- BUZZER INIT -");
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN,0);

  // Keyer
  DEBUG_PRINTLN("- KEYER INIT -");
  pinMode(KEYER_SHORT_PIN, INPUT_PULLUP);
  pinMode(KEYER_LONG_PIN, INPUT_PULLUP);

  // MODE BTN
  DEBUG_PRINTLN("- BUTTON INIT -");
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

  pinMode(SPEAKER_PIN, OUTPUT);
}

/////////////////////////////////////////////////////////////////
// startBeep 
/////////////////////////////////////////////////////////////////
void startBeep(unsigned int length, char symbol)
{
  noTone(SPEAKER_PIN);
  tone(SPEAKER_PIN, 1000);
  if (speakerOn) 
    digitalWrite(BUZZER_PIN, HIGH);

  beeping = true;
  beepEnd = millis() + length;
  DEBUG_PRINT(symbol);
}

/////////////////////////////////////////////////////////////////
// updateBeep
/////////////////////////////////////////////////////////////////
void updateBeep()
{
 unsigned long now = millis();

    if(beeping && now >= beepEnd)
    {
      noTone(SPEAKER_PIN);
      digitalWrite(BUZZER_PIN, LOW);
      beeping = false;

      // Starte Pause nach Ton
      inPause = true;
      pauseEnd = now + elementGap;
    }

    if(inPause && now >= pauseEnd)
    {
      inPause = false; // nächste Eingabe möglich
      lastKeyTime = now; // WICHTIG: update lastKeyTime hier
    }
}

/////////////////////////////////////////////////////////////////
// startPlayback
/////////////////////////////////////////////////////////////////
void startPlayback(String text)
{
  playbackText = text;
  playbackIndex = 0;
  currentMorse = "";
  morseIndex = 0;
  playing = true;
  nextActionTime = millis(); // sofort loslegen
}

/////////////////////////////////////////////////////////////////
// updatePlayback
/////////////////////////////////////////////////////////////////
void updatePlayback()
{
  if (!playing) 
    return;

  unsigned long now = millis();
  if (now < nextActionTime) 
    return;

  // Wenn wir gerade kein Morse für das aktuelle Zeichen haben -> nächstes Zeichen holen
  if (currentMorse.length() == 0)
  {
    if (playbackIndex >= playbackText.length())
    {
      playing = false;
      return;
    }

    char c = playbackText[playbackIndex++];
    if (c == ' ')
    {
      // Wortpause: bereits elementGap (1T) vor dem Aufruf berücksichtigt -> zusätzlich wordExtra
      nextActionTime = now + wordExtra;
      return;
    }
    else
    {
      currentMorse = EncodeChar(c);
      morseIndex = 0;
    }
  }

  // Nächstes Symbol dieser Buchstabe
  if (morseIndex < currentMorse.length())
  {
    char sym = currentMorse[morseIndex++];
    unsigned int len = (sym == '.') ? dit_len : dah_len;
    startBeep(len, sym);
    // nach Beep wollen wir (len + elementGap) warten (Beep + 1T)
    nextActionTime = now + len + elementGap;
  }
  else
  {
    // Ganze Buchstabe fertig: zusätzlich letterExtra warten (wir hatten schon elementGap)
    currentMorse = "";
    nextActionTime = now + letterExtra;
  }
}

/////////////////////////////////////////////////////////////////
// CalcDisplayPosition
/////////////////////////////////////////////////////////////////
void CalcDisplayPosition( short chars_on_display, int* r, int* c )
{
  *r = chars_on_display / 15;
  *c = chars_on_display % 15;
}

/////////////////////////////////////////////////////////////////
// Loop
/////////////////////////////////////////////////////////////////
void loop() 
{
    if (settingsOn) 
    {
      server.handleClient();
    }
    
    r.loop();

    // Encoder Button
    if(digitalRead(MODE_BUTTON_PIN) == LOW)
    {
        StateMachine(MODE_BUTTON_PIN);
        delay(250);
    }

    // Keyer
    bool shortPressed = digitalRead(KEYER_SHORT_PIN) == LOW;
    bool longPressed  = digitalRead(KEYER_LONG_PIN) == LOW;

    if(shortPressed && !beeping && !inPause) 
    {
        StateMachine(KEYER_SHORT_PIN);
        lastKeyTime = millis(); // letzte Eingabe merken
    }

    if(longPressed && !beeping && !inPause) 
    {
        StateMachine(KEYER_LONG_PIN);
        lastKeyTime = millis(); // letzte Eingabe merken
    }

    updateBeep();
    updatePlayback();

    if(actual_menu == TRAINER_HEAR_SCREEN)
    {
      unsigned long now = millis();

      if(decoderString.length() > 0 && !beeping && !inPause && (now - lastKeyTime) >= letterExtra)
      {
        String decodedLetter = DecodeMorseCode(decoderString);

        char buf[2] = { decodedLetter[0], '\0' }; // char-Array für Display

        DEBUG_PRINTLN(buf[0]);

        if(buf[0] == 'E' ||  buf[0] == 'T')
        {
          decoderString = "";
          ShowTrainerHearScreen();
        }
      }
    }

    // MONITOR oder TRAINER
    if(actual_menu == MONITOR || actual_menu == TRAINER_GIVE_RANDOM_SCREEN || actual_menu == TRAINER_GIVE_AZ_SCREEN)
    {
        unsigned long now = millis();

      // Prüfen, ob ein Buchstabe fertig ist: kein Beep mehr und
      // seit letztem Element mindestens letterExtra Zeit vergangen
      if(decoderString.length() > 0 && !beeping && !inPause && (now - lastKeyTime) >= letterExtra)
      {
        String decodedLetter = DecodeMorseCode(decoderString);

        char buf[2] = { decodedLetter[0], '\0' }; // char-Array für Display

        if(actual_menu == MONITOR)
        {
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
        else if(actual_menu == TRAINER_GIVE_RANDOM_SCREEN || actual_menu == TRAINER_GIVE_AZ_SCREEN)
        {
            if(buf[0] == currentTrainerLetter)
            {
                display.clear();
                display.print("Correct !", 4,4);
            }
            else
            {
                display.clear();
                display.print("Not Correct !",4,2);
                ProcessSign(currentTrainerLetter);
            }
            delay(1000);
            if(actual_menu == TRAINER_GIVE_RANDOM_SCREEN)
              ShowTrainerGiveRandomScreen();
            else
              ShowTrainerGiveAZScreen();
        }

        decoderString = ""; // Buffer leeren für das nächste Zeichen
      }
    }

    StateMachine(NO_KEY);
}

/////////////////////////////////////////////////////////////////
/// StateMachine
/////////////////////////////////////////////////////////////////
void StateMachine(int key)
{
  if(State == STATE_IDLE)
  {
    // SHORT KEY
    if(key == KEYER_SHORT_PIN)
    {
      // Nur starten, wenn aktuell kein Beep oder Pause läuft
      if(!beeping && !inPause)
      {
        ProcessBeep(dit_len, '.');
        decoderString += ".";
        key_activated = millis();
      }
    }

    // LONG KEY
    if(key == KEYER_LONG_PIN)
    {
      if(!beeping && !inPause)
      {
        ProcessBeep(dah_len, '-');
        decoderString += "-";
        key_activated = millis();
      }
    }

    // MODE BUTTON
    if(key == MODE_BUTTON_PIN)
    {
      ReactOnButtonClick();
    }
  }
}

/////////////////////////////////////////////////////////////////
/// ReactOnButtonClick
/////////////////////////////////////////////////////////////////
void ReactOnButtonClick()
{
  // MAIN_MENU
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
      DEBUG_PRINTLN("ShowTimerScreen");
      ShowTrainerScreen();
      return;
    }
  }

  // CW_KEYER
  if(actual_menu == CW_KEYER)
  {
     DEBUG_PRINTLN("KEYER");
     
    if(selected_menu_item == SPEED && speed_mode == false)
    {
      speed_mode = true;
    }
    else if(selected_menu_item == SPEED && speed_mode == true)
    {
      speed_mode = false;
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

  // TRAINER
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
      DEBUG_PRINTLN("Trainer Give AZ");
      selectedLetters ="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
      currentAZPosition = -1;
      ShowTrainerGiveAZScreen();
      return;
    }

    if(selected_menu_item == TRAINER_BACK)
    {
      DEBUG_PRINTLN("Trainer Back");
      ShowMainScreen();
      return;
    }
  }
  // MONITOR
  if(actual_menu == MONITOR)
  {
    ShowMainScreen();
    return;
  }

  if(actual_menu == TRAINER_GIVE_RANDOM_SCREEN)
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
    selectedLetters = ReadTextFromEEPROM(0x40); 
    ShowTrainerScreen();
    return;
  }

  // SETUP
  if(actual_menu == SETUP)
  {
     DEBUG_PRINTLN("SETUP");
     
    if(selected_menu_item == SETUP_SPEAKER)
    {
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

    if(selected_menu_item == SETUP_BACK)
    {
      DEBUG_PRINTLN("Setup Back");
      ShowMainScreen();
      return;
    }
  }  
}

/////////////////////////////////////////////////////////////////
/// SwitchSpeaker
/////////////////////////////////////////////////////////////////
void SwitchSpeaker(byte value)
{
  if(value == 0)
    speakerOn = false;
  else
    speakerOn = true;

  EEPROM.write(EEPROM_SPEAKER_ADDR, speakerOn);
  EEPROM.commit();
}

/////////////////////////////////////////////////////////////////
/// SwitchSettings
/////////////////////////////////////////////////////////////////
void SwitchSettings(byte value)
{
  if(value == 0) 
  {
    settingsOn = false;

    // WLAN & Server stoppen
    server.stop();
    WiFi.disconnect(true);   // trennt alles
    WiFi.mode(WIFI_OFF);     // schaltet WLAN wirklich aus
    DEBUG_PRINTLN("WiFi/Server OFF");
  }
  else 
  {
    settingsOn = true;

    // WLAN & Server starten
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    IPAddress Ip(192, 168, 4, 2);
    IPAddress NMask(255, 255, 255, 0);
    WiFi.softAPConfig(Ip, Ip, NMask);

    DEBUG_PRINTLN(WiFi.localIP());

    server.on("/", handleRoot);
    server.on("/save_letters", handleSaveLetters);
    server.on("/get", handleGet);
    server.onNotFound(notFound);
    server.begin();

    DEBUG_PRINTLN("WiFi/Server ON");
  }
}

/////////////////////////////////////////////////////////////////
/// ProcessBeep  (non-blocking)
/////////////////////////////////////////////////////////////////
void ProcessBeep(short beepoLength, char outputChar)
{
    tone(SPEAKER_PIN, 1000); 
    // Ton starten
    if(speakerOn) 
    {
        digitalWrite(BUZZER_PIN, HIGH);
    }

    beeping = true;
    beepEnd = millis() + beepoLength;  // wann der Ton endet
    // Pause nach Beep wird in updateBeep() automatisch gestartet
    DEBUG_PRINT(outputChar);
}

/////////////////////////////////////////////////////////////////
/// ProcessSign
/////////////////////////////////////////////////////////////////
void ProcessSign(char sign)
{
  // play single character non-blocking
  startPlayback(String(sign));
}

/////////////////////////////////////////////////////////////////
/// PlayMemory
/////////////////////////////////////////////////////////////////
void PlayMemory(byte addr)
{
  String text = ReadTextFromEEPROM(addr);

  DEBUG_PRINTLN(text);

  if(text.length() <= 0)
    return;

  startPlayback(text);
}

/////////////////////////////////////////////////////////////////
/// EncodeChar
/////////////////////////////////////////////////////////////////
String EncodeChar(char sign)
{
  char buffer[6]; // max Morse-Zeichen + null-Terminator

  if (sign >= 'a' && sign <= 'z') 
  {
    strcpy_P(buffer, MORSE_LETTERS[sign - 'a']);
    return String(buffer);
  } 
  else if (sign >= 'A' && sign <= 'Z') 
  {
    strcpy_P(buffer, MORSE_LETTERS[sign - 'A']);
    return String(buffer);
  } 
  else if (sign >= '0' && sign <= '9') 
  {
    strcpy_P(buffer, MORSE_NUMBERS[sign - '0']);
    return String(buffer);
  }

  return "";
}

/////////////////////////////////////////////////////////////////
/// DecodeMorseCode
///////////////////////////////////////////////////////////////// 
String DecodeMorseCode(String code) 
{
  for (int i = 0; i < 26; i++) 
  {
    char buffer[6];
    strcpy_P(buffer, MORSE_LETTERS[i]);
    if (strcmp(buffer, code.c_str()) == 0) 
    {
      char letterChar = pgm_read_byte(&LETTERS[i]);
      return String(letterChar);
    }
  }

  for (int i = 0; i < 10; i++) 
  {
    char buffer[6];
    strcpy_P(buffer, MORSE_NUMBERS[i]);
    if (strcmp(buffer, code.c_str()) == 0) 
    {
      return String(char('0' + i));
    }
  }

  return "*";
}

/////////////////////////////////////////////////////////////////
// ShowMainScreen
/////////////////////////////////////////////////////////////////
void ShowMainScreen()
{
  actual_menu = MAIN_MENU;
  selected_menu_item = 1;
  display.clear();
  display.print("CWKeyer v0.42", 0,1);

  display.print("CW-Keyer", 2,4);
  display.print("Monitor", 3,4);
  display.print("Trainer", 4,4);
  display.print("Setup", 5,4);

   display.print(">", 2,1);
}

/////////////////////////////////////////////////////////////////
// ShowKeyerScreen
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

  display.print(">", 2,1);

  char string[128];
  sprintf(string, "Speed: %i WPM", wpm);
  display.print(string, 6,2);
}

/////////////////////////////////////////////////////////////////
// ShowTrainerScreen
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

  display.print("Back", 5,4);

  display.print(">", 2,1);
}

/////////////////////////////////////////////////////////////////
// ShowTrainerHearScreen
/////////////////////////////////////////////////////////////////
void ShowTrainerHearScreen()
{
  actual_menu = TRAINER_HEAR_SCREEN;
  selected_menu_item = 1;
  display.clear();

  // Zufälligen Buchstaben aus aktiver Liste wählen
  if (selectedLetters.length() == 0) 
  {
    // Fallback: alle Buchstaben
    selectedLetters ="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  } 

  char buf[6];

  for(int i=0; i<5; i++)
  {
    int idx = random(0, selectedLetters.length());
    char letter = selectedLetters[idx];
    buf[i] = letter;
  }

  buf[5] = '\0';
  startPlayback(String(buf));
  display.print(buf, 4, 5);

  decoderString = "";
}

/////////////////////////////////////////////////////////////////
// ShowTrainerGiveRandomScreen
/////////////////////////////////////////////////////////////////
void ShowTrainerGiveRandomScreen()
{
  actual_menu = TRAINER_GIVE_RANDOM_SCREEN;
  selected_menu_item = 1;
  display.clear();

  // Zufälligen Buchstaben aus aktiver Liste wählen
  if (selectedLetters.length() == 0) 
  {
    // Fallback: alle Buchstaben
    selectedLetters ="ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  }
  int idx = random(0, selectedLetters.length());
  char letter = selectedLetters[idx];
  currentTrainerLetter = letter;

  char buf[2] = { currentTrainerLetter, '\0' };
  display.print(buf, 4, 7);

  DEBUG_PRINT("SelectedLetters: ");
  DEBUG_PRINTLN(selectedLetters);
  DEBUG_PRINT("CurrentTrainerLetter: ");
  DEBUG_PRINTLN(currentTrainerLetter);

  decoderString = "";
}

/////////////////////////////////////////////////////////////////
// ShowTrainerGiveAZScreen
/////////////////////////////////////////////////////////////////
void ShowTrainerGiveAZScreen()
{
  actual_menu = TRAINER_GIVE_AZ_SCREEN;
  selected_menu_item = 1;
  display.clear();

  currentAZPosition++;

  if(currentAZPosition > 25)
    currentAZPosition = 0;

  char letter = selectedLetters[currentAZPosition];
  currentTrainerLetter = letter;

  char buf[2] = { currentTrainerLetter, '\0' };
  display.print(buf, 4, 7);

  DEBUG_PRINT("CurrentTrainerLetter: ");
  DEBUG_PRINTLN(currentTrainerLetter);

  decoderString = "";
}

/////////////////////////////////////////////////////////////////
// ShowSetupScreen
/////////////////////////////////////////////////////////////////
void ShowSetupScreen()
{
  actual_menu = SETUP;
  selected_menu_item = 1;
  
  display.clear();
    
  if(speakerOn == true)
    display.print("Speaker ON  ", 2,4);
  else
    display.print("Speaker OFF", 2,4);

  if(settingsOn == true)
    display.print("Settings ON  ", 3,4);
  else
    display.print("Settings OFF", 3,4);

  display.print("Back", 4,4);
  display.print("192.168.4.2", 6,4);
  display.print(">", 2,1);
}

/////////////////////////////////////////////////////////////////
// ShowMonitorScreen
/////////////////////////////////////////////////////////////////
void ShowMonitorScreen()
{
  actual_menu = MONITOR;
  selected_menu_item = 1;
  char_on_screen = 0;
  display.clear();
}

/////////////////////////////////////////////////////////////////
// CalculateTimes
/////////////////////////////////////////////////////////////////
void CalculateTimes(char wpm)
{
  if (wpm <= 0) wpm = 1;
  unsigned int T = 1200 / wpm; // Grundeinheit in ms

  T_unit    = T;
  dit_len   = T;        // 1T
  dah_len   = 3 * T;    // 3T
  elementGap = T;       // Pause zwischen Elementen (1T)

  // Wenn wir nach jedem Element schon 1T einplanen,
  // brauchen wir für Buchstaben noch zusätzlich 2T (3T total):
  letterExtra = 3 * T - elementGap; // = 2T

  // Für Wortpause: insgesamt 7T => zusätzlich 6T (neben dem 1T)
  wordExtra = 7 * T - elementGap;   // = 6T

  DEBUG_PRINT("T: "); DEBUG_PRINTLN(T);
  DEBUG_PRINT("dit: "); DEBUG_PRINTLN(dit_len);
  DEBUG_PRINT("dah: "); DEBUG_PRINTLN(dah_len);
  DEBUG_PRINT("elementGap: "); DEBUG_PRINTLN(elementGap);
  DEBUG_PRINT("letterExtra: "); DEBUG_PRINTLN(letterExtra);
  DEBUG_PRINT("wordExtra: "); DEBUG_PRINTLN(wordExtra);
}

/////////////////////////////////////////////////////////////////
/// Write text to EEPROM
/////////////////////////////////////////////////////////////////
void WriteTextToEEPROM(byte addr, String text)
{
  for (int i = 0; i < text.length(); i++)
  {
    EEPROM.write(addr, text[i]);
    addr += 1;
  }
  EEPROM.write(addr, 0); // Nullterminator schreiben
  addr += 1;

  // Optional: Rest mit '@' auffüllen (nicht zwingend nötig)
  for (int i = addr; i < 128; i++)
  {
    EEPROM.write(i, '@');
  }
  EEPROM.commit();
}

/////////////////////////////////////////////////////////////////
/// Read text from EEPROM
/////////////////////////////////////////////////////////////////
String ReadTextFromEEPROM(byte addr)
{
  String retVal;
  for (int i = addr; i < 128; i++) 
  {
    byte readValue = EEPROM.read(i);
    if (readValue == 0)
      break; // Nullterminator = Ende
    char readValueChar = char(readValue);
    if(readValueChar != '@')
      retVal += readValueChar;
  }
  DEBUG_PRINTLN(retVal);
  return retVal;
}

void DisplaySelectionArrow()
{
  // Auswahlpfeil überall entfernen
  display.print(" ", 2,1);
  display.print(" ", 3,1);
  display.print(" ", 4,1);
  display.print(" ", 5,1);
  display.print(" ", 6,1);

  // Neuen Auswahlpfeil hinzufügen
  display.print(">", selected_menu_item+1,1 );
}

/////////////////////////////////////////////////////////////////
// Rotary 'onChange'
/////////////////////////////////////////////////////////////////
void rotate(Rotary& r) 
{
  // Speed einstellen
  if(actual_menu == CW_KEYER && speed_mode == true)
  {
    if(r.getDirection() == 1)
    {
         wpm += STEP_SIZE;
    }
    else
    {
      if(wpm > 0)
      {
        wpm -= STEP_SIZE;
      }
    }

    char string[128];
    sprintf(string, "Speed: %i WPM", wpm);
    display.print(string, 6,2);
  
    CalculateTimes(wpm);
    EEPROM.write(EEPROM_WPM_ADDR, wpm);
    EEPROM.commit();
    return;
  }

  // Menüs durchgehen
  if(actual_menu == CW_KEYER)
  {
    selected_menu_item++;
    
    if(selected_menu_item > KEYER_MENU_COUNT)
      selected_menu_item = 1;

    DisplaySelectionArrow();
    return;
  }

  if(actual_menu == MAIN_MENU)
  {
    if(r.getDirection() == 1)
    {
      selected_menu_item++;
    
      if(selected_menu_item > MAIN_MENU_COUNT)
        selected_menu_item = 1;
    }
    else
    {
      selected_menu_item--;
      if(selected_menu_item < 1)
        selected_menu_item = MAIN_MENU_COUNT;
    }

    DisplaySelectionArrow();
    
    return;
  }

  if(actual_menu == SETUP)
  {
    selected_menu_item++;
    
    if(selected_menu_item > SETUP_MENU_COUNT)
      selected_menu_item = 1;

    DisplaySelectionArrow();

    return;
  }
  
  if(actual_menu == TRAINER)
  {
    selected_menu_item++;
    
    if(selected_menu_item > TRAINER_MENU_COUNT)
      selected_menu_item = 1;

    DisplaySelectionArrow();

    return;
  }
}
