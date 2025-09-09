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

/////////////////////////////////////////////////////////////////
// Objects
/////////////////////////////////////////////////////////////////
Rotary r;
OLED display(4, 5,0x3c,0);
ESP8266WebServer server(80);

/////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////

short beepLong;
short beepShort;
short beepPause;
short letterPause;
short wordPause;
char wpm = START_POS;
bool speed_mode = false;
bool speakerOn = true;
double key_activated;
short char_on_screen = -1;

bool pause = false;

byte selected_menu_item = SETUP_SPEAKER;
byte actual_menu = MAIN_MENU;

const char* ssid     = "CWKeyer";
const char* password = "123456789";
const char* TEXT_1 = "input1";
const char* TEXT_2 = "input2";

String decoderString;
String selectedLetters = ""; // Globale Variable für die Auswahl
bool showTrainerFeedback;
char currentTrainerLetter;

int State = STATE_IDLE;

/////////////////////////////////////////////////////////////////
// notFound
/////////////////////////////////////////////////////////////////
void notFound() {
  server.send(404, "text/html", wrapInPage("<p>Seite nicht gefunden</p>"));
}

/////////////////////////////////////////////////////////////////
// handleRoot
/////////////////////////////////////////////////////////////////
void handleRoot() {
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
void handleSaveLetters() {
  String selected = "";
  int args = server.args();
  for (int i = 0; i < args; i++) {
    if (server.argName(i) == "letters") {
      selected += server.arg(i);
    }
  }
  WriteTextToEEPROM(0x40, selected);
  selectedLetters = selected;

  String content = "<p>Gespeichert: " + selected + "</p>";
  server.send(200, "text/html", wrapInPage(content));
}

/////////////////////////////////////////////////////////////////
// handleGet
/////////////////////////////////////////////////////////////////
void handleGet() {
  String inputParam = "none";
  if (server.hasArg(TEXT_1)) {
    WriteTextToEEPROM(EEPROM_MEM1_ADDR, server.arg(TEXT_1));
    inputParam = TEXT_1;
  } else if (server.hasArg(TEXT_2)) {
    WriteTextToEEPROM(EEPROM_MEM2_ADDR, server.arg(TEXT_2));
    inputParam = TEXT_2;
  }
  String content = "<p>Gespeichert: " + inputParam + "</p>";
  server.send(200, "text/html", wrapInPage(content));
}

/////////////////////////////////////////////////////////////////
// generateLetterCheckboxes
/////////////////////////////////////////////////////////////////
String generateLetterCheckboxes() {
  String html = "";
  for (int i = 0; i < 26; i++) {
    char letterChar = pgm_read_byte(&LETTERS[i]); // korrekt aus PROGMEM lesen
    String letter = String(letterChar);          // in String umwandeln
    bool checked = selectedLetters.indexOf(letter) != -1;
    html += "<input type='checkbox' name='letters' value='" + letter + "' id='l" + String(i) + "'";
    if (checked) html += " checked";
    html += ">";
    html += "<label for='l" + String(i) + "'>" + letter + "</label> ";
    if ((i+1) % 7 == 0) html += "<br>";
  }
  return html;
}

/////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////
void setup() 
{
  DEBUG_BEGIN(SERIAL_SPEED);
  //delay(1000);

   // EEPROM ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  DEBUG_PRINTLN("- EEPROM INIT -");
  EEPROM.begin(EEPROM_SIZE);
  
  byte value;
  value = EEPROM.read(EEPROM_WPM_ADDR);
  DEBUG_PRINT("Read wpm: ");
  DEBUG_PRINTLN(value, DEC);

  if(value >= 100 || value < 0)
  {
    wpm = 14;
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
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // WEB SERVER ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  WiFi.softAP(ssid, password);

  IPAddress Ip(192, 168, 4, 2);    //setto IP Access Point same as gateway
  IPAddress NMask(255, 255, 255, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);

  // Print ESP8266 Local IP Address
  DEBUG_PRINTLN(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/save_letters", handleSaveLetters);
  server.on("/get", handleGet);
  server.onNotFound(notFound);
  server.begin();
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // OLED Init +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  DEBUG_PRINTLN("- BUZZER INIT -");
  // Initialize display
  display.begin();

  ShowMainScreen();
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // Rotary Init +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  r.begin(ROTARY_PIN1, ROTARY_PIN2, CLICKS_PER_STEP);
  r.setChangedHandler(rotate);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // Beep Init +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  DEBUG_PRINTLN("- BUZZER INIT -");
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN,0);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // Keyer +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  DEBUG_PRINTLN("- KEYER INIT -");
  pinMode(KEYER_SHORT_PIN, INPUT_PULLUP);
  pinMode(KEYER_LONG_PIN, INPUT_PULLUP);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // MODE BTN ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  DEBUG_PRINTLN("- BUTTON INIT -");
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  pinMode(SPEAKER_PIN, OUTPUT);
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
void loop() {
  server.handleClient();

  // Encoder +++++++++++++++++++++++++++++++++++++
  r.loop();
  // +++++++++++++++++++++++++++++++++++++++++++++

  // Encoder Button
  if( digitalRead(MODE_BUTTON_PIN) == LOW )
  {
    StateMachine(MODE_BUTTON_PIN);
    delay(250);
  }
  
  // Keyer +++++++++++++++++++++++++++++++++++++++
  if( digitalRead(KEYER_SHORT_PIN) == LOW )
  {
    StateMachine(KEYER_SHORT_PIN);
  }
  else
  {
     digitalWrite(BUZZER_PIN,0);
  }

  if( digitalRead(KEYER_LONG_PIN) == LOW )
  {
     StateMachine(KEYER_LONG_PIN);
  }
  else
  {
    digitalWrite(BUZZER_PIN,0);
  }

  if(actual_menu == MONITOR)
  {
    if((millis() - key_activated) > (beepPause*2) && decoderString != "")
    {
      char buffer[5];  // Annahme: Platz für die Zeichenfolge
      String morseString = DecodeMorseCode(decoderString);
      morseString.toCharArray(buffer, sizeof(buffer));
      // 6 x 15 Zeichen sind möglich = 90
      
      if(char_on_screen == 90)
      {
        char_on_screen = 0;
        display.clear();
      }
      else
      {
        char_on_screen++;
      }

      int r,c;
      CalcDisplayPosition(char_on_screen, &r, &c);

      display.print(buffer, r,c);
      decoderString = "";
    }
  }

  if(actual_menu == TRAINER_GIVE_SCREEN)
  {
    if((millis() - key_activated) > (beepPause*2) && decoderString != "")
    {
      char buffer[5];  // Annahme: Platz für die Zeichenfolge
      String morseString = DecodeMorseCode(decoderString);
      morseString.toCharArray(buffer, sizeof(buffer));
      // 6 x 15 Zeichen sind möglich = 90
      
      if(buffer[0] == currentTrainerLetter)
      {
         display.clear();
         display.print("Correct !",4,4);
      }
      else
      {
         display.clear();
         display.print("Not Correct !",4,2);
         ProcessSign(currentTrainerLetter);
      }
       delay(1000);
       ShowTrainerGiveScreen();
    }
  }

  StateMachine(NO_KEY);
  // +++++++++++++++++++++++++++++++++++++++++++++
}

/////////////////////////////////////////////////////////////////
/// StateMachine
/////////////////////////////////////////////////////////////////
void StateMachine(int key)
{
  if(State == STATE_IDLE)
  {
    if(key == KEYER_SHORT_PIN)
    {
      ProcessBeep(beepShort, '.');
      decoderString += ".";
      key_activated = millis();
    }

    if(key == KEYER_LONG_PIN)
    {
       ProcessBeep(beepLong, '-');
       decoderString += "-";
       key_activated = millis();
    }

    if(key == MODE_BUTTON_PIN)
    {
       ReactOnButtonClick();
    }
  }
  else if(State == STATE_MEM)
  {
    if(key == NO_KEY)
    {
      
    }
  }
  else if(State == STATE_TRAINER)
  {
    
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

    if (selected_menu_item == TRAINER_GIVE)
    {
      DEBUG_PRINTLN("Trainer Give");
      ShowTrainerGiveScreen();
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

  if(actual_menu == TRAINER_GIVE_SCREEN)
  {
    ShowMainScreen();
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
/// ProcessBeep
/////////////////////////////////////////////////////////////////
void ProcessBeep(short beepoLength, char outputChar)
{
  tone(SPEAKER_PIN, 1000, beepoLength); 
  if(speakerOn == true)
  {
    digitalWrite(BUZZER_PIN,1);
    delay(beepoLength);
    digitalWrite(BUZZER_PIN,0);
    delay(beepPause);
  }
  else
  {
    delay(beepPause+beepoLength);
  }
  DEBUG_PRINT(outputChar);
}

/////////////////////////////////////////////////////////////////
/// ProcessSign
/////////////////////////////////////////////////////////////////
void ProcessSign(char sign)
{
  String encoded =  EncodeChar(sign);
  if(encoded.length() > 0)
    {
      for (int i=0; i < encoded.length(); i++)
      {
        if(encoded[i] == '.')
        {
          ProcessBeep(beepShort, encoded[i]);
        }
        else if(encoded[i] == '-')
        {
          ProcessBeep(beepLong, encoded[i]);
        }
      }
    }
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
    
  for (int i=0; i < text.length(); i++)
  {
    if(text[i] != ' ')
    {
      // Ein Buchstabe oder eine Zahl
      ProcessSign(text[i]);
      // Pause zwischen den Buchstaben
      delay(letterPause);
    }
    else
    {
      // Ein Leerzeichen
      delay(wordPause);
    }
  }
}

/////////////////////////////////////////////////////////////////
/// EncodeChar
/////////////////////////////////////////////////////////////////
String EncodeChar(char sign)
{
  char buffer[6]; // max Morse-Zeichen + null-Terminator

  if (sign >= 'a' && sign <= 'z') {
    strcpy_P(buffer, MORSE_LETTERS[sign - 'a']);
    return String(buffer);
  } 
  else if (sign >= 'A' && sign <= 'Z') {
    strcpy_P(buffer, MORSE_LETTERS[sign - 'A']);
    return String(buffer);
  } 
  else if (sign >= '0' && sign <= '9') {
    strcpy_P(buffer, MORSE_NUMBERS[sign - '0']);
    return String(buffer);
  }

  return "";
}

/////////////////////////////////////////////////////////////////
/// DecodeMorseCode
///////////////////////////////////////////////////////////////// 
String DecodeMorseCode(String code) {
  for (int i = 0; i < 26; i++) {
    char buffer[6];
    strcpy_P(buffer, MORSE_LETTERS[i]);
    if (strcmp(buffer, code.c_str()) == 0) {
      char letterChar = pgm_read_byte(&LETTERS[i]);
      return String(letterChar);
    }
  }

  for (int i = 0; i < 10; i++) {
    char buffer[6];
    strcpy_P(buffer, MORSE_NUMBERS[i]);
    if (strcmp(buffer, code.c_str()) == 0) {
      return String(char('0' + i));
    }
  }

  return "*";
}

/////////////////////////////////////////////////////////////////
/// ShowMainScreen
/////////////////////////////////////////////////////////////////
void ShowMainScreen()
{
  actual_menu = MAIN_MENU;
  selected_menu_item = 1;
  display.clear();
  display.print("CWKeyer v0.4", 0,1);

  display.print("CW-Keyer", 2,4);
  display.print("Monitor", 3,4);
  display.print("Trainer", 4,4);
  display.print("Setup", 5,4);

   display.print(">", 2,1);
}

/////////////////////////////////////////////////////////////////
/// ShowKeyerScreen
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
/// ShowTrainerScreen
/////////////////////////////////////////////////////////////////
void ShowTrainerScreen()
{
  actual_menu = TRAINER;
  selected_menu_item = 1;
  
  display.clear();
  display.print("Trainer", 0,1);
  display.print("Hear", 2,4);
  display.print("Give", 3,4);

  display.print("Back", 4,4);

  display.print(">", 2,1);
}

/////////////////////////////////////////////////////////////////
/// ShowTrainerHearScreen
/////////////////////////////////////////////////////////////////
void ShowTrainerHearScreen()
{

}

/////////////////////////////////////////////////////////////////
/// ShowTrainerGiveScreen
/////////////////////////////////////////////////////////////////
void ShowTrainerGiveScreen()
{
  actual_menu = TRAINER_GIVE_SCREEN;
  selected_menu_item = 1;
  display.clear();

  // Zufälligen Buchstaben aus aktiver Liste wählen
  int idx = random(0, selectedLetters.length());
  char letter = pgm_read_byte(&LETTERS[idx]);
  currentTrainerLetter = letter;

  char buf[2] = { currentTrainerLetter, '\0' };
  display.print(buf, 4, 7);

  DEBUG_PRINTLN(selectedLetters);
  DEBUG_PRINTLN(currentTrainerLetter);

  decoderString = "";
  showTrainerFeedback = false;
}

/////////////////////////////////////////////////////////////////
/// ShowSetupScreen
/////////////////////////////////////////////////////////////////
void ShowSetupScreen()
{
  actual_menu = SETUP;
  selected_menu_item = 1;
  
  display.clear();
  display.print("Setup", 0,1);
    
  if(speakerOn == true)
    display.print("Speaker ON  ", 2,4);
  else
    display.print("Speaker OFF", 2,4);

  display.print("Back", 3,4);

  display.print("192.168.4.2", 5,4);

  display.print(">", 2,1);
}

/////////////////////////////////////////////////////////////////
/// ShowSetupScreen
/////////////////////////////////////////////////////////////////
void ShowMonitorScreen()
{
  actual_menu = MONITOR;
  selected_menu_item = 1;
  char_on_screen = 0;
  display.clear();

  // 

}

/////////////////////////////////////////////////////////////////
/// CalculateTimes
/////////////////////////////////////////////////////////////////
void CalculateTimes(char wpm)
{
    short w = 1200 / wpm;
    beepPause = w; 
    beepShort = w; 
    beepLong = 3 * w;
    letterPause = 3 * w;
    wordPause   = 7 * w;

    DEBUG_PRINT("Pause:");
    DEBUG_PRINTLN(beepPause);

    DEBUG_PRINT("Short:");
    DEBUG_PRINTLN(beepShort);

    DEBUG_PRINT("Long:");
    DEBUG_PRINTLN(beepLong);
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
  for (int i = addr; i < 128; i++) {
    byte readValue = EEPROM.read(i);
    if (readValue == 0) break; // Nullterminator = Ende
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
/// Rotary 'onChange'
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
