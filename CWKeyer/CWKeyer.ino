#include <Wire.h>
#include "Rotary.h"
#include "ESPOLED.h"
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

#define CLICKS_PER_STEP   4 
#define START_POS 14
#define STEP_SIZE 1

#define SERIAL_SPEED    115200

#define EEPROM_SIZE     257
#define EEPROM_WPM_ADDR 0   // 1 byte
#define EEPROM_MEM1_ADDR 1  // 128 byte
#define EEPROM_MEM2_ADDR 130   // 128 byte

#define SETUP_MEM_1 1 
#define SETUP_MEM_2 1 

/////////////////////////////////////////////////////////////////
// Objects
/////////////////////////////////////////////////////////////////

Rotary r;
OLED display(4, 5,0x3c,10);

/////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////

char beepLong;
char beepShort;
char beepPause;
char wpm = START_POS;
bool setupMode = false;

byte mem_selection;

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

  // Keyer +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println("- KEYER INIT -");
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
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
    Serial.println("SetupMode Start");
    display.clear();
    display.print("Setup", 0,0);

    delay(250);
  }

  if( digitalRead(MODE_BUTTON_PIN) == LOW && setupMode == true )
  {
    setupMode = false;
    display.clear();
    ShowStdScreen();

    delay(250);
  }
  
  // Keyer +++++++++++++++++++++++++++++++++++++++
  if( digitalRead(KEYER_SHORT_PIN) == LOW )
  {
    if(setupMode == false)
    {
      digitalWrite(BUZZER_PIN,1);
      delay(beepShort);
      digitalWrite(BUZZER_PIN,0);
      delay(beepPause);
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
      digitalWrite(BUZZER_PIN,1);
      delay(beepLong);
      digitalWrite(BUZZER_PIN,0);
      delay(beepPause);
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

 
  // +++++++++++++++++++++++++++++++++++++++++++++
}

void HandleSetupKeys(int key)
{
  if(key == KEYER_SHORT_PIN)
  {
    Serial.println("KEYER_SHORT_PIN");
  }

  if(key == KEYER_LONG_PIN)
  {
    Serial.println("KEYER_LONG_PIN");
  }
}

void ShowStdScreen()
{
  display.print("CWKeyer v0.1", 0,0);
  char string[128];
  sprintf(string, "Speed: %i WPM", wpm);
  display.print(string, 4,0);
}

/////////////////////////////////////////////////////////////////
/// Rotary 'onChange'
/////////////////////////////////////////////////////////////////
void rotate(Rotary& r) {
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

/////////////////////////////////////////////////////////////////
/// CalculateTimes
/////////////////////////////////////////////////////////////////
void CalculateTimes(char wpm)
{
    char w = 1200 / wpm;
    beepPause = w; 
    beepShort = w; 
    beepLong = 3 * w;
}
