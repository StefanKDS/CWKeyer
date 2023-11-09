#include <ezBuzzer.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <ESPRotary.h>
#include <EEPROM.h>

/////////////////////////////////////////////////////////////////
// DEFINE
/////////////////////////////////////////////////////////////////

#define ROTARY_PIN1 D3
#define ROTARY_PIN2 D4
#define BUZZER_PIN D8
#define KEYER_SHORT_PIN D6
#define KEYER_LONG_PIN D7
#define LED_PIN D5

#define CLICKS_PER_STEP 4   // this number depends on your rotary encoder
#define MIN_POS         0
#define MAX_POS         2000
#define START_POS       250
#define INCREMENT       50   // this number is the counter increment on each step

#define BASE_BEEP_TIME 250
#define BASE_BEEP_PAUSE 250

#define EEPROM_MEM1 1
#define EEPROM_MEM2 2
#define EEPROM_SPEED 3

#define SERIAL_SPEED    115200

/////////////////////////////////////////////////////////////////
// Objects
/////////////////////////////////////////////////////////////////

ESPRotary rotary;
LiquidCrystal_I2C lcd(0x3F, 16, 2);
ezBuzzer buzzer(BUZZER_PIN);

/////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////

int speedBeepAddOn = START_POS;
int beepPause = BASE_BEEP_PAUSE;
short cwSpeed;
int addr = 0;


/////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(SERIAL_SPEED);
  delay(50);

  // EEPROM +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println("- EEPROM INIT -");
  EEPROM.begin(512);  //Initialize EEPROM

  // EEPROM AdressTable
  //
  // 0-128 Memory 1
  // 129 - 256 Memory 2
  // 257 - 258 cwSpeed
  
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // Rotary Encoder Init +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  rotary.begin(ROTARY_PIN1, ROTARY_PIN2, CLICKS_PER_STEP, MIN_POS, MAX_POS, START_POS, INCREMENT);
  rotary.setChangedHandler(rotate);
  rotary.setLeftRotationHandler(showDirection);
  rotary.setRightRotationHandler(showDirection);
  rotary.setLowerOverflowHandler(lower);
  rotary.setUpperOverflowHandler(upper);

  Serial.println("- ROTARY INIT -");
  Serial.println("\n\nRanged Counter");
  Serial.println("You can only set values between " + String(MIN_POS) + " and " + String(MAX_POS) +".");
  Serial.print("Current position: ");
  Serial.println(rotary.getPosition());
  Serial.println();
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // LCD I2C Init ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println("- LCD INIT -");
  /*Serial.println ("I2C scanner. Scanning ...");
  byte count = 0;

  Wire.begin();
  for (byte i = 8; i < 120; i++)
  {
    Wire.beginTransmission (i);
    if (Wire.endTransmission () == 0)
      {
      Serial.print ("Found address: ");
      Serial.print (i, DEC);
      Serial.print (" (0x");
      Serial.print (i, HEX);
      Serial.println (")");
      count++;
      delay (1);  // maybe unneeded?
      } // end of good response
  } // end of for loop
  Serial.println ("Done.");
  Serial.print ("Found ");
  Serial.print (count, DEC);
  Serial.println (" device(s).");*/

  lcd.init();
  lcd.clear();         
  lcd.backlight();

  lcd.setCursor(0,0);   //Set cursor to character 2 on line 0
  lcd.print("CW Keyer v0.1");
  
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // Beep Init +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println("- BUZZER INIT -");
  buzzer.beep(BASE_BEEP_TIME);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // Keyer +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  Serial.println("- KEYER INIT -");
  pinMode(KEYER_SHORT_PIN, INPUT);
  pinMode(KEYER_LONG_PIN, INPUT);
  // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
}

/////////////////////////////////////////////////////////////////
// Loop
/////////////////////////////////////////////////////////////////
void loop() {
  // Rotary ++++++++++++++++++++++++++++++++++++++
  rotary.loop();
  // +++++++++++++++++++++++++++++++++++++++++++++

  // Keyer +++++++++++++++++++++++++++++++++++++++
  if( digitalRead(KEYER_SHORT_PIN) )
  {
    buzzer.beep(BASE_BEEP_TIME + speedBeepAddOn);
    delay(beepPause + speedBeepAddOn);
  }

  if( digitalRead(KEYER_LONG_PIN) )
  {
    buzzer.beep((BASE_BEEP_TIME * 2) + speedBeepAddOn);
    delay(beepPause + speedBeepAddOn);
  }
  // +++++++++++++++++++++++++++++++++++++++++++++
}


/////////////////////////////////////////////////////////////////
// Rotary 'OnChange'
/////////////////////////////////////////////////////////////////
void rotate(ESPRotary& rotary) {
   Serial.print("Rotary pos = ");
   Serial.println(rotary.getPosition());
   speedBeepAddOn = rotary.getPosition();
}

/////////////////////////////////////////////////////////////////
// Rotary 'out of bounds event UPPER'
/////////////////////////////////////////////////////////////////
void upper(ESPRotary& rotary) {
   Serial.println("Rotary upper bound hit");
}

/////////////////////////////////////////////////////////////////
// Rotary 'out of bounds event LOWER'
/////////////////////////////////////////////////////////////////
void lower(ESPRotary& rotary) {
   Serial.println("Rotary lower bound hit");
}

/////////////////////////////////////////////////////////////////
// Rotary 'on left or right rotation'
/////////////////////////////////////////////////////////////////
void showDirection(ESPRotary& rotary) {
  Serial.print("Rotary direction: ");
  Serial.println(rotary.directionToString(rotary.getDirection()));
}

/////////////////////////////////////////////////////////////////
// Write to EEPROM
//
// Beispiel: 
// short speed;
// writeToEEPROM(EEPROM_SPEED, reinterpret_cast<char*>(&speed), sizeof(speed));
/////////////////////////////////////////////////////////////////
void writeToEEPROM (char type, char* value, int valueSize)
{
  int startPos;
  
  if(type == 1)  //MEM1
  {
    startPos = 0;
  }
  else if (type ==2) //MEM2
  {
    startPos = 129;
  }
  else if(type == 3) //CW_SPEED
  {
    startPos = 257;
  }

  for (int i = 0; i < valueSize; i++)
  {
    EEPROM.write(startPos + i, *value);
    value++;
  }

  EEPROM.commit();
}

/////////////////////////////////////////////////////////////////
// Read from EEPROM
//
// Beispiel: 
// short speed;
// readFromEEPROM(EEPROM_SPEED, reinterpret_cast<char*>(&speed), sizeof(speed));
/////////////////////////////////////////////////////////////////
void readFromEEPROM (char type, char* value, int valueSize)
{
  int startPos;
  
  if(type == 1)  //MEM1
  {
    startPos = 0;
  }
  else if (type ==2) //MEM2
  {
    startPos = 129;
  }
  else if(type == 3) //CW_SPEED
  {
    startPos = 257;
  }

 for (int i = 0; i < valueSize; i++)
  {
    *value = EEPROM.read(startPos + i);
    value++;
  }
}
