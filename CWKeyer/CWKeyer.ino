#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <Wire.h>
#include "Rotary.h"
#include "ESPOLED.h"
#include "MorseCode.h"
#include <EEPROM.h>

/////////////////////////////////////////////////////////////////
// DEFINE
/////////////////////////////////////////////////////////////////


#define ROTARY_PIN1 0
#define ROTARY_PIN2 2
#define BUZZER_PIN 15
#define KEYER_SHORT_PIN 12
#define KEYER_LONG_PIN 13
#define MODE_BUTTON_PIN 16
#define SPEAKER_PIN 14

#define CLICKS_PER_STEP   4 
#define START_POS 14
#define STEP_SIZE 1

#define SERIAL_SPEED    115200

#define EEPROM_SIZE     260
#define EEPROM_WPM_ADDR 0   // 1 byte
#define EEPROM_MEM1_ADDR 1  // 128 byte
#define EEPROM_MEM2_ADDR 129   // 128 byte
#define EEPROM_SPEAKER_ADDR 258

// MENU ////////////////
#define MAIN_MENU_COUNT 3
#define MAIN_MENU 1
#define SETUP 2
#define CW_KEYER 3
#define TRAINER 4
///////////////////////

// SETUP MENU /////////
#define SETUP_MENU_COUNT 1
#define SETUP_SPEAKER 21
///////////////////////

// SETUP CW_KEYER /////////
#define KEYER_MENU_COUNT 3
#define SPEED 31 
#define MEM_1 32
#define MEM_2 33
///////////////////////

// TRAINER MENU /////////
#define TRAINER_MENU_COUNT 3
#define FIRST 41
///////////////////////

// All morse characters
#define MORSE_DOT '.'
#define MORSE_DASH '-'

/////////////////////////////////////////////////////////////////
// Objects
/////////////////////////////////////////////////////////////////
Rotary r;
OLED display(4, 5,0x3c,10);
AsyncWebServer server(80);

/////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////

short beepLong;
short beepShort;
short beepPause;
char wpm = START_POS;
bool setupMode = false;
bool speakerOn = true;

bool pause = false;

byte selected_menu_item = SETUP_SPEAKER;
byte actual_menu = MAIN_MENU;

const char* ssid     = "CWKeyer";
const char* password = "123456789";
const char* TEXT_1 = "input1";
const char* TEXT_2 = "input2";

String decoderString;

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

  ShowStdScreen();
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
  Serial.println("- KEYER INIT -");
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  pinMode(SPEAKER_PIN, OUTPUT);
}

/////////////////////////////////////////////////////////////////
// Loop
/////////////////////////////////////////////////////////////////
void loop() {
  // Encoder +++++++++++++++++++++++++++++++++++++
  r.loop();
  // +++++++++++++++++++++++++++++++++++++++++++++

  if( digitalRead(MODE_BUTTON_PIN) == LOW && setupMode == false )
  {
    setupMode = true;
    ShowSetupScreen();
    Serial.println("SetupMode Start");

    delay(250);
  }

  if( digitalRead(MODE_BUTTON_PIN) == LOW && setupMode == true )
  {
    setupMode = false;
    ShowStdScreen();

    delay(250);
  }
  
  // Keyer +++++++++++++++++++++++++++++++++++++++
  if( digitalRead(KEYER_SHORT_PIN) == LOW )
  {
    if(setupMode == false)
    {
      ProcessBeep(beepShort, '.');
      decoderString += ".";
    }
    else
    {
      HandleSetupKeys(KEYER_SHORT_PIN);
      delay(250);
    }
  }
  else
  {
     digitalWrite(BUZZER_PIN,0);
  }

  if( digitalRead(KEYER_LONG_PIN) == LOW )
  {
    if(setupMode == false)
    {
      ProcessBeep(beepLong, '-');
      decoderString += "-";
    }
    else
    {
      HandleSetupKeys(KEYER_LONG_PIN);
      delay(250);
    }
  }
  else
  {
    digitalWrite(BUZZER_PIN,0);
  }

  if((decoderString.length() > 0) && (pause == true))
  {
    DecodeMorseCode(decoderString);
  }
 
  // +++++++++++++++++++++++++++++++++++++++++++++
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

  if(setupMode == true)
  {
    display.print(">", 2,0);
    
    if(speakerOn == true)
      display.print("Speaker ON ", 2,3);
    else
      display.print("Speaker OFF", 2,3);
  }

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
/// HandleSetupKeys
/////////////////////////////////////////////////////////////////
void HandleSetupKeys(int key)
{
  if(key == KEYER_SHORT_PIN)
  {
  
  }

  if(key == KEYER_LONG_PIN)
  {
    if(selected_setup_item == 1)
    {
      if(speakerOn == true)
        SwitchSpeaker(false);
      else
        SwitchSpeaker(true);
    }

    if(selected_setup_item == 2)
    {
      PlayMemory(EEPROM_MEM1_ADDR);
    }

    if(selected_setup_item == 3)
    {
      PlayMemory(EEPROM_MEM2_ADDR);
    }

    if(selected_setup_item == 4)
    {
      StartDecoderMode();
    }
  }
}

/////////////////////////////////////////////////////////////////
/// HandleDecoderModeKeys
/////////////////////////////////////////////////////////////////
void HandleDecoderModeKeys(int key)
{
  if(key == KEYER_SHORT_PIN)
  {
  
  }

  if(key == KEYER_LONG_PIN)
  {

  }
}

/////////////////////////////////////////////////////////////////
/// StartDecoderMode
/////////////////////////////////////////////////////////////////
void StartDecoderMode()
{
   display.clear();
   display.print("DecoderMode", 0,0);
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
  Serial.println(code.c_str());
  
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
  
  return "";
}

/////////////////////////////////////////////////////////////////
/// ShowSetupScreen
/////////////////////////////////////////////////////////////////
void ShowSetupScreen()
{
    display.clear();
    display.print("Setup", 0,0);

    display.print(">", 2,0);
    
    if(speakerOn == true)
      display.print("Speaker ON", 2,3);
    else
      display.print("Speaker OFF", 2,3);

    display.print("Play MEM 1", 3,3);
    display.print("Play MEM 2", 4,3);
    display.print("Decoder", 5,3);
}

/////////////////////////////////////////////////////////////////
/// ShowStdScreen
/////////////////////////////////////////////////////////////////
void ShowStdScreen()
{
  display.clear();
  display.print("CWKeyer v0.1", 0,0);
  char string[128];
  sprintf(string, "Speed: %i WPM", wpm);
  display.print(string, 4,0);
}

/////////////////////////////////////////////////////////////////
/// Rotary 'onChange'
/////////////////////////////////////////////////////////////////
void rotate(Rotary& r) {
   if(setupMode == false)
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
    display.print(string, 4,0);
  
    CalculateTimes(wpm);
    EEPROM.write(EEPROM_WPM_ADDR, wpm);
    EEPROM.commit();
   }
   else
   {
    selected_setup_item++;
    if(selected_setup_item > 4)
      selected_setup_item = 1;

    display.print(" ", 2,0);
    display.print(" ", 3,0);
    display.print(" ", 4,0);

    display.print(">", selected_setup_item+1,0);
   }
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
