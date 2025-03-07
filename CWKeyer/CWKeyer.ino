#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

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

/////////////////////////////////////////////////////////////////
// Objects
/////////////////////////////////////////////////////////////////
Rotary r;
OLED display(4, 5,0x3c,0);
AsyncWebServer server(80);

/////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////

short beepLong;
short beepShort;
short beepPause;
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

int State = STATE_IDLE;

/////////////////////////////////////////////////////////////////
// HTML Page
/////////////////////////////////////////////////////////////////
// HTML web page to handle 3 input fields (input1, input2, input3)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>CWKeyer v0.1</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head>
  <body>
  <h1>CWKeyer v0.1</h1>
  <form action="/get">
    Text 1: <input type="text" name="input1" maxlength="128">
    <input type="submit" value="Submit">
  </form><br>
  <form action="/get">
    Text 2: <input type="text" name="input2" maxlength="128">
    <input type="submit" value="Submit">
  </form><br>
</body></html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}


/////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(SERIAL_SPEED);
  delay(1000);

   // EEPROM ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println("- EEPROM INIT -");
  EEPROM.begin(EEPROM_SIZE);
  
  byte value;
  value = EEPROM.read(EEPROM_WPM_ADDR);
  Serial.print("Read wpm: ");
  Serial.println(value, DEC);

  if(value > 255 || value < 0)
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
  Serial.print("Read Speaker setup: ");
  Serial.println(value, DEC);

  SwitchSpeaker((byte)value);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // WEB SERVER ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  WiFi.softAP(ssid, password);

  IPAddress Ip(192, 168, 4, 2);    //setto IP Access Point same as gateway
  IPAddress NMask(255, 255, 255, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);

  // Print ESP8266 Local IP Address
  Serial.println(WiFi.localIP());

  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(TEXT_1)) {
      // Write TEXT_1 to EEPROM
      WriteTextToEEPROM(EEPROM_MEM1_ADDR, request->getParam(TEXT_1)->value());
      
      //inputMessage = request->getParam(TEXT_1)->value();
      inputParam = TEXT_1;
    }
    // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
    else if (request->hasParam(TEXT_2)) {
      // Write TEXT_2 to EEPROM
      WriteTextToEEPROM(EEPROM_MEM2_ADDR, request->getParam(TEXT_2)->value());
      
      //inputMessage = request->getParam(TEXT_2)->value();
      inputParam = TEXT_2;
    }
    else {
      //inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
    request->send(200, "text/html", inputParam + " is saved." +
                                     "<br><a href=\"/\">Back</a>");
  });
  server.onNotFound(notFound);

  server.begin();
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // OLED Init +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println("- BUZZER INIT -");
  // Initialize display
  display.begin();

  ShowMainScreen();
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // Rotary Init +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  r.begin(ROTARY_PIN1, ROTARY_PIN2, CLICKS_PER_STEP);
  r.setChangedHandler(rotate);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // Beep Init +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println("- BUZZER INIT -");
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN,0);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // Keyer +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println("- KEYER INIT -");
  pinMode(KEYER_SHORT_PIN, INPUT_PULLUP);
  pinMode(KEYER_LONG_PIN, INPUT_PULLUP);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // MODE BTN ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println("- BUTTON INIT -");
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
    Serial.println("MAIN_MENU");
    
    if(selected_menu_item == CW_KEYER)
    {
      Serial.println("ShowKeyerScreen");
      ShowKeyerScreen();
      return;
    }

    if(selected_menu_item == SETUP)
    {
      Serial.println("ShowSetupScreen");
      ShowSetupScreen();
      return;
    }

    if(selected_menu_item == MONITOR)
    {
      Serial.println("ShowMonitorScreen");
      ShowMonitorScreen();
      return;
    }

    if(selected_menu_item == TRAINER)
    {
      Serial.println("ShowTimerScreen");
      ShowTrainerScreen();
      return;
    }
  }

  // CW_KEYER
  if(actual_menu == CW_KEYER)
  {
     Serial.println("KEYER");
     
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
      Serial.println("Play Mem 1");
      return;
    }

     if(selected_menu_item == MEM_2)
     {
      PlayMemory(EEPROM_MEM2_ADDR);
      Serial.println("Play Mem 2");
      return;
    }

    if(selected_menu_item == CW_KEYER_BACK)
    {
      ShowMainScreen();
      Serial.println("Keyer Back");
      return;
    }
  }

  // TRAINER
  if(actual_menu == TRAINER)
  {
     Serial.println("TRAINER");
     
    if(selected_menu_item == TRAINER_BACK)
    {
      Serial.println("Trainer Back");
      ShowMainScreen();
      return;
    }
  }

  // MONITOR
  if(actual_menu == MONITOR)
  {
    Serial.println("MONITOR");
    ShowMainScreen();
    return;
  }

  // SETUP
  if(actual_menu == SETUP)
  {
     Serial.println("SETUP");
     
    if(selected_menu_item == SETUP_SPEAKER)
    {
      if(speakerOn == true)
      {
        SwitchSpeaker(false);
        display.print("Speaker ON ", 2,3);
        Serial.println("Speaker ON");
      }
      else
      {
        SwitchSpeaker(true);
        display.print("Speaker OFF", 2,3);
        Serial.println("Speaker OFF");
      }

      return;
    }

    if(selected_menu_item == SETUP_BACK)
    {
      Serial.println("Setup Back");
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
  Serial.print(outputChar);
}

/////////////////////////////////////////////////////////////////
/// PlayMemory
/////////////////////////////////////////////////////////////////
void PlayMemory(byte addr)
{
  String text = ReadTextFromEEPROM(addr);

  if(text.length() <= 0)
    return;
    
  for (int i=0; i < text.length(); i++)
  {
    String encoded =  EncodeChar(text[i]);

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
        else if(encoded[i] == ' ')
        {
           delay(beepPause*2);
        }
      }
    }
  }
}

/////////////////////////////////////////////////////////////////
/// EncodeChar
/////////////////////////////////////////////////////////////////
String EncodeChar(char sign)
{
    if (sign >= 'a' && sign <= 'z') 
    {
      return MORSE_LETTERS[sign - 'a'];
    }
    else if (sign >= 'A' && sign <= 'Z') 
    {
      return MORSE_LETTERS[sign - 'A'];
    }
    else if (sign >= '0' && sign <= '9') 
    {
      return MORSE_NUMBERS[sign - '0'];
    }
    else if (sign == ' ') 
    {
      return " ";
    }

    return "";
}

/////////////////////////////////////////////////////////////////
/// DecodeMorseCode
///////////////////////////////////////////////////////////////// 
String DecodeMorseCode(String code)
{
  for (int i=0; i<26; i++)
  {
    if (strcmp( MORSE_LETTERS[i],code.c_str() ) == 0)
    {
      Serial.print(LETTERS[i]);
      return LETTERS[i];
    }
  }

  for (int i=0; i<10; i++)
  {
    if (strcmp( MORSE_NUMBERS[i], code.c_str()) == 0)
    {
      Serial.print(i);
      return String(i);
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
  display.print("CWKeyer v0.3", 0,1);

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
  display.print(string, 6,1);
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

  display.print("Back", 2,4);

  display.print(">", 2,1);
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

  display.print("192.168.4.2", 5,1);

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

    Serial.print("Pause:");
    Serial.println(beepPause);

    Serial.print("Short:");
    Serial.println(beepShort);

    Serial.print("Long:");
    Serial.println(beepLong);
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

  for (int i = addr; i < 128 - text.length(); i++)
  {
        EEPROM.write(addr, '@');
        addr += 1;
  }
  
  EEPROM.commit();
}

/////////////////////////////////////////////////////////////////
/// Read text from EEPROM
/////////////////////////////////////////////////////////////////
String ReadTextFromEEPROM(byte addr)
{
  String retVal;
  
  // reading byte-by-byte from EEPROM
    for (int i = addr; i < 128; i++) {
        byte readValue = EEPROM.read(i);

        if (readValue == 0) {
            break;
        }

        char readValueChar = char(readValue);
        if(readValueChar != '@')
          retVal += readValueChar;
    }

    Serial.println(retVal);

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
    display.print(string, 6,1);
  
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
