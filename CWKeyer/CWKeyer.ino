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
// DEKLARATIONEN - Funktionsprototypen
// Diese Prototypen ermöglichen, dass Funktionen vor ihrer Definition
// aufgerufen werden können
/////////////////////////////////////////////////////////////////

void CalculateTimes(char wpm, char fwpm);  // Berechnet Timings basierend auf WPM-Wert
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
// Globale Objekte - Hardware-Schnittstellen
/////////////////////////////////////////////////////////////////
Rotary r;                               // Drehregler für Menünavigation und Geschwindigkeitsanpassung
OLED display(SDA, SCL, 0x3c, 2);       // OLED-Display (I2C-Adresse 0x3c)
ESP8266WebServer server(80);            // Web-Server für WiFi-Konfiguration (Port 80)

/////////////////////////////////////////////////////////////////
// Globale Variablen - Zustand und Timing
/////////////////////////////////////////////////////////////////

// ===== Non-blocking Beep / Playback State =====
// Der Ton wird nicht blockierend abgespielt; diese Variablen verwalten seinen Status
bool beeping = false;           // true, während der Ton aktiv ist
unsigned long beepEnd = 0;      // Zeitpunkt, an dem der aktuelle Ton endet

bool inPause = false;           // true, während eine Pause zwischen Elementen läuft
unsigned long pauseEnd = 0;     // Zeitpunkt, an dem die aktuelle Pause endet

// ===== Timing-Steuerung für Zeichenerkennung =====
unsigned long lastKeyTime = 0; // Zeit der letzten Tastatureingabe (zur Erkennung von Zeichenenden)
unsigned int letterGap = 3;    // Pauenabstand zwischen Buchstaben (3T)


// ===== Playback State - für gespeicherte Text-Wiedergabe =====
bool playing = false;           // true, während Text wiedergegeben wird
String playbackText = "";       // Der zu playenden Text
int playbackIndex = 0;          // Aktuelle Position im playbackText
String currentMorse = "";       // Morse-Code des aktuellen Zeichens (z.B. ".-")
int morseIndex = 0;             // Aktuelle Position im Morse-Code (welcher Punkt/Strich)
unsigned long nextActionTime = 0; // Zeitpunkt der nächsten Aktion beim Playback

// ===== Morse-Code Timing (T-basiert) =====
// T ist die Basis-Zeiteinheit: T = 1200ms / WPM
// Mit Farnsworth werden Pausen verlängert, aber Punkt/Strich bleiben gleich
unsigned int T_unit = 0;        // Basis-Zeiteinheit in Millisekunden
unsigned int dit_len = 0;       // Länge eines Punktes (1T)
unsigned int dah_len = 0;       // Länge eines Strichs (3T)
unsigned int elementGap = 0;    // Pause zwischen Punkt/Strich (1T - Inter-Element Gap)
unsigned int letterExtra = 0;   // Zusätzliche Pause nach Buchstabe (Farnsworth-Anpassung)
unsigned int wordExtra = 0;     // Zusätzliche Pause nach Wort (Farnsworth-Anpassung)

// ===== Benutzer-Konfiguration und Modi =====
char wpm = START_POS;           // Geschwindigkeit in Wörter pro Minute (WPM)
char fwpm = wpm;                // Farnsworth-Geschwindigkeit (für längere Pausen)
bool speed_mode = false;        // true = Encoder zum Ändern der WPM-Geschwindigkeit
bool fspeed_mode = false;       // true = Encoder zum Ändern der Farnsworth-Geschwindigkeit
bool speakerOn = true;          // true = Buzzer ist aktiviert
bool settingsOn = false;        // true = WiFi/Web-Server ist aktiviert
double key_activated;           // Zeitstempel der letzten Tastatureingabe
short char_on_screen = -1;      // Nummer des Zeichens auf dem Monitor-Bildschirm

// ===== Menü-Navigation =====
byte selected_menu_item = SETUP_SPEAKER;  // Der aktuell ausgewählte Menüpunkt
byte actual_menu = MAIN_MENU;             // Der aktuelle Bildschirm/Menü

// ===== WiFi und Web-Server =====
const char* ssid     = "CWKeyer";        // WiFi-Netzwerkname des Access Points
const char* password = "123456789";      // WiFi-Passwort
const char* TEXT_1 = "input1";           // Parameter-Namen für Web-Formulare
const char* TEXT_2 = "input2";

// ===== Morse-Code Dekodierung und Trainer =====
String decoderString;                    // Buffert die empfangenen Punkt/Strich (z.B. ".-")
String selectedLetters = "";             // Buchstaben für Trainer (z.B. "ABC...XYZ")
char currentTrainerLetter;                // Der zu trainieren Buchstabe
short currentAZPosition = -1;             // Position in Alphabet für sequenzielles Training

// ===== Zustandsverwaltung =====
int State = STATE_IDLE;                  // Hauptzustand der Anwendung

/////////////////////////////////////////////////////////////////
// notFound
/////////////////////////////////////////////////////////////////
void notFound() 
{
  server.send(404, "text/html", wrapInPage("<p>Seite nicht gefunden</p>"));
}

/////////////////////////////////////////////////////////////////
/// handleRoot() - HTTP GET / (Haupt-Seite)
// Diese Funktion serviert die HTML-Seite, wenn ein Client
// die Root-URL aufruft. Sie lädt gespeicherte Texte und
// Überprüft, welche Buchstaben für das Training ausgewählt sind.
/////////////////////////////////////////////////////////////////
void handleRoot() 
{
  String page = index_html;  // Lade HTML-Template
  // Lese gespeicherte Texte aus EEPROM
  String text1 = ReadTextFromEEPROM(EEPROM_MEM1_ADDR);
  String text2 = ReadTextFromEEPROM(EEPROM_MEM2_ADDR);

  // Ersetze Platzhalter im HTML-Template mit aktuellen Werten
  page.replace("%TEXT1%", text1);
  page.replace("%TEXT2%", text2);
  page.replace("%LETTERS_CHECKBOXES%", generateLetterCheckboxes());

  // Sende Cache-Control Header (verhindert alte Versionen auf Android)
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");

  // Sende HTML-Seite an Client
  server.send(200, "text/html", page);
}

/////////////////////////////////////////////////////////////////
/// handleSaveLetters() - HTTP POST /save_letters
// Diese Funktion verarbeitet das Trainer-Formular: Speichert
// ausgewählte Buchstaben im EEPROM und aktualisiert globale Variable.
/////////////////////////////////////////////////////////////////
void handleSaveLetters() 
{
  String selected = "";
  int args = server.args();  // Anzahl der Form-Parameter

  // Sammle alle ausgewählten Buchstaben (Parameter-Name: "letters")
  for (int i = 0; i < args; i++) 
  {
    if (server.argName(i) == "letters") 
    {
      selected += server.arg(i);  // Hänge Buchstabe an
    }
  }

  // Speichere Auswahl im EEPROM und in globaler Variable
  WriteTextToEEPROM(0x40, selected);
  selectedLetters = selected;  // Update globale Variable für Trainer

  // Bestätigungs-Antwort an Client
  String content = "<p>Saved: " + selected + "</p>";
  server.send(200, "text/html", wrapInPage(content));
}

/////////////////////////////////////////////////////////////////
/// handleGet() - HTTP GET /get?input1=... oder /get?input2=...
// Diese Funktion speichert Morse-Text aus dem Web-Formular.
// Parameter input1 -> EEPROM_MEM1_ADDR
// Parameter input2 -> EEPROM_MEM2_ADDR
/////////////////////////////////////////////////////////////////
void handleGet() 
{
  String inputParam = "none";  // Welcher Parameter empfangen wurde

  if (server.hasArg(TEXT_1))    // Prüfe auf input1-Parameter
  {
    WriteTextToEEPROM(EEPROM_MEM1_ADDR, server.arg(TEXT_1));
    inputParam = TEXT_1;
  } 
  else if (server.hasArg(TEXT_2))  // Prüfe auf input2-Parameter
  {
    WriteTextToEEPROM(EEPROM_MEM2_ADDR, server.arg(TEXT_2));
    inputParam = TEXT_2;
  }

  String content = "<p>Saved: " + inputParam + "</p>";
  server.send(200, "text/html", wrapInPage(content));
}

/////////////////////////////////////////////////////////////////
/// generateLetterCheckboxes() - Erzeugt HTML für Buchstaben-Checkboxen
// Diese Funktion generiert HTML-Formular-Elemente für alle 26 Buchstaben.
// Sie erkennt, welche Buchstaben zuvor ausgewählt wurden, und
// markiert diese als "checked".
/////////////////////////////////////////////////////////////////
String generateLetterCheckboxes() 
{
  String html = "";
  // Durchlaufe alle 26 Buchstaben
  for (int i = 0; i < 26; i++) 
  {
    char letterChar = pgm_read_byte(&LETTERS[i]);  // Lese Buchstabe aus PROGMEM
    String letter = String(letterChar);             // Konvertiere zu String
    bool checked = selectedLetters.indexOf(letter) != -1;  // War dieser Buchstabe gewählt?

    // Erzeuge Checkbox-HTML
    html += "<input type='checkbox' name='letters' value='" + letter + "' id='l" + String(i) + "'";
    if (checked) 
      html += " checked";  // Markiere als bereits gewählt
    html += ">";
    html += "<label for='l" + String(i) + "'>" + letter + "</label> ";

    // Neue Zeile nach 7 Buchstaben für bessere Formatierung
    if ((i+1) % 7 == 0) 
      html += "<br>";
  }
  return html;
}

/////////////////////////////////////////////////////////////////
/// setup() - Arduino Initialisierung
// Diese Funktion wird einmal beim Start aufgerufen und
// initialisiert alle Hardware-Komponenten und Variablen:
// - EEPROM (lade gespeicherte WPM, Texte, Buchstaben)
// - Display (OLED)
// - Eingabe-Pins (Keyer, Mode-Button)
// - Audio-Outputs (Buzzer, Speaker)
// - WiFi/WebServer (optional)
/////////////////////////////////////////////////////////////////
void setup() 
{
  DEBUG_BEGIN(SERIAL_SPEED);  // Serielle Debug-Ausgabe (wenn DEBUG_PRINT definiert)

  // ===== WiFi und Server initial AUS =====
  SwitchSettings(0);

  // ===== EEPROM Initialisierung =====
  DEBUG_PRINTLN("- EEPROM INIT -");
  EEPROM.begin(EEPROM_SIZE);  // EEPROM initialisieren

  // ===== Lade WPM aus EEPROM =====
  byte value;
  value = EEPROM.read(EEPROM_WPM_ADDR);
  DEBUG_PRINT("Read wpm: ");
  DEBUG_PRINTLN(value, DEC);

  // Validiere WPM: Muss zwischen 0 und 99 liegen
  if(value >= 100 || value < 0)
  {
    wpm = START_POS;  // Verwende Standardwert falls ungültig
  }
  else
  {
    wpm = value;  // Verwende gespeicherten Wert
  }

  // ===== Lade Farnsworth-WPM aus EEPROM =====
  value = EEPROM.read(EEPROM_FWPM_ADDR);
  DEBUG_PRINT("Read fwpm: ");
  DEBUG_PRINTLN(value, DEC);
  if(value >= 100 || value < 0)
  {
    fwpm = START_POS;
  }
  else
  {
    fwpm = value;
  }

  // Berechne alle Timing-Konstanten basierend auf WPM
  CalculateTimes(wpm, fwpm);

  // Lade gespeicherte Texte aus EEPROM (für Speicher 1 und 2)
  ReadTextFromEEPROM(EEPROM_MEM1_ADDR);
  ReadTextFromEEPROM(EEPROM_MEM2_ADDR);

  // ===== Lade Speaker-Status aus EEPROM =====
  value = EEPROM.read(EEPROM_SPEAKER_ADDR);
  DEBUG_PRINT("Read Speaker setup: ");
  DEBUG_PRINTLN(value, DEC);
  SwitchSpeaker((byte)value);

  // Lade Buchstaben-Auswahl für Trainer
  selectedLetters = ReadTextFromEEPROM(0x40);

  // ===== OLED Display Initialisierung =====
  DEBUG_PRINTLN("- BUZZER INIT -");
  display.begin();  // Starte OLED-Display
  ShowMainScreen(); // Zeige Start-Bildschirm

  // ===== Rotary Encoder Initialisierung =====
  r.begin(ROTARY_PIN2, ROTARY_PIN1, CLICKS_PER_STEP);
  r.setChangedHandler(rotate);  // Setze Callback für Drehung

  // ===== Buzzer-Pin Initialisierung =====
  DEBUG_PRINTLN("- BUZZER INIT -");
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, 0);  // Buzzer starten deaktiviert

  // ===== Keyer-Tasten Initialisierung =====
  DEBUG_PRINTLN("- KEYER INIT -");
  pinMode(KEYER_SHORT_PIN, INPUT_PULLUP);  // Punkt-Paddel
  pinMode(KEYER_LONG_PIN, INPUT_PULLUP);   // Strich-Paddel

  // ===== Mode-Button Initialisierung =====
  DEBUG_PRINTLN("- BUTTON INIT -");
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

  // ===== Speaker-Pin Initialisierung =====
  pinMode(SPEAKER_PIN, OUTPUT);  // Für Ton-Ausgabe (Frequenz mit tone())
}

/////////////////////////////////////////////////////////////////
/// startBeep() - Startet einen Ton nicht-blockierend
// Diese Funktion startet einen Piep-Ton mit einer bestimmten
// Länge. Der Ton wird nicht blockierend ausgespielt - die Funktion
// kehrt sofort zurück und updateBeep() verwaltet das Beenden.
/////////////////////////////////////////////////////////////////
void startBeep(unsigned int length, char symbol)
{
  noTone(SPEAKER_PIN);        // Stelle sicher, dass kein alter Ton läuft
  tone(SPEAKER_PIN, 1000);    // Starte 1000 Hz Ton auf Speaker

  if (speakerOn)              // Wenn Buzzer aktiviert ist
    digitalWrite(BUZZER_PIN, HIGH);  // Buzzer auch einschalten

  beeping = true;             // Markiere als Ton läuft
  beepEnd = millis() + length;  // Berechne Endzeit
  DEBUG_PRINT(symbol);        // Debug-Ausgabe des Symbols
}

/////////////////////////////////////////////////////////////////
// updateBeep() - Verwaltet Beep-Timing (non-blocking)
// Diese Funktion wird in jedem Loop aufgerufen und prüft, ob ein Beep
// beendet werden soll oder eine Pause starten soll. So wird verhindert,
// dass der Hauptloop blockiert wird.
/////////////////////////////////////////////////////////////////
void updateBeep()
{
 unsigned long now = millis();

    // Wenn der Beep die geplante Endzeit erreicht hat
    if(beeping && now >= beepEnd)
    {
      noTone(SPEAKER_PIN);      // Ton auf Speaker ausschalten
      digitalWrite(BUZZER_PIN, LOW);  // Buzzer ausschalten
      beeping = false;

      // Starte automatisch eine Pause nach dem Ton (1T)
      inPause = true;
      pauseEnd = now + elementGap;  // Pause dauert 1T
    }

    // Wenn die Pause vorbei ist
    if(inPause && now >= pauseEnd)
    {
      inPause = false;      // Pause beendet - nächster Ton kann beginnen
      lastKeyTime = now;    // WICHTIG: Aktualisiere lastKeyTime für Zeichenerkennung
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
// updatePlayback() - Wiedergabe von gespeicherten Texten (non-blocking)
// Diese Funktion kontrolliert die Wiedergabe von Morse-Code. Sie:
// 1. Konvertiert Zeichen zu Morse-Code
// 2. Spielt Punkte (dit) und Striche (dah) mit korrektem Timing ab
// 3. Erzeugt korrekte Pausen zwischen Elementen, Buchstaben und Wörtern
/////////////////////////////////////////////////////////////////
void updatePlayback()
{
  if (!playing)           // Wenn keine Wiedergabe läuft, nichts tun
    return;

  unsigned long now = millis();
  if (now < nextActionTime)  // Warte, bis die Zeit für die nächste Aktion reif ist
    return;

  // ===== Nächstes Zeichen laden, wenn das aktuelle Morse-Code fertig ist =====
  if (currentMorse.length() == 0)
  {
    // Überprüfe, ob wir alle Zeichen des Textes abgespielt haben
    if (playbackIndex >= playbackText.length())
    {
      playing = false;  // Wiedergabe beendet
      return;
    }

    char c = playbackText[playbackIndex++];  // Nächstes Zeichen aus dem Text
    if (c == ' ')
    {
      // Wortpause: 7T total, aber 1T (elementGap) ist schon zwischen Buchstaben
      // Also nur wordExtra (6T extra) hinzufügen
      nextActionTime = now + wordExtra;
      return;
    }
    else
    {
      // Konvertiere Buchstabe zu Morse-Code (z.B. 'A' -> ".-")
      currentMorse = EncodeChar(c);
      morseIndex = 0;  // Starten Sie vom ersten Symbol des Morse-Codes
    }
  }

  // ===== Nächstes Symbol (Punkt oder Strich) des aktuellen Buchstabens =====
  if (morseIndex < currentMorse.length())
  {
    char sym = currentMorse[morseIndex++];  // Aktuelles Symbol ('.' oder '-')
    unsigned int len = (sym == '.') ? dit_len : dah_len;  // Länge basierend auf Symbol
    startBeep(len, sym);  // Beep starten
    // Nach dem Beep: warte auf Beeplänge + 1T (elementGap) für nächstes Symbol
    nextActionTime = now + len + elementGap;
  }
  else
  {
    // Alle Symbole des Buchstabens abgespielt
    // Warte auf Buchstabenpause minus der bereits verbrauchten 1T (elementGap)
    currentMorse = "";  // Buffer für nächsten Buchstaben leeren
    nextActionTime = now + letterExtra;  // Zusätzliche Pause vor nächstem Buchstabe
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
// loop() - Hauptschleife
// Diese Funktion läuft ständig und:
// 1. Verarbeitet Benutzereingaben (Tasten, Encoder, Mode-Button)
// 2. Aktualisiert Beep und Playback-Timing
// 3. Dekodiert Morse-Code und zeigt Ergebnisse an
// 4. Verwaltet das Web-Interface (falls aktiviert)
/////////////////////////////////////////////////////////////////
void loop() 
{
    // ===== Zustands-Flag für Zeichenerkennung =====
    // Verhindert, dass ein Zeichen mehrmals verarbeitet wird, wenn
    // die Pause nach einem Zeichen abgelaufen ist
    static bool letterProcessed = false;

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

    // ===== Keyer-Eingabe (Paddel) =====

    bool dit = digitalRead(KEYER_SHORT_PIN) == LOW;
    bool dah = digitalRead(KEYER_LONG_PIN) == LOW;

    static bool lastWasDit = false;
    static bool iambicModeActive = false;

    // Nur wenn bereit
    if(!beeping && !inPause)
    {
        // ===== IAMBIC START =====
        if(dit && dah)
        {
            iambicModeActive = true;

            if(lastWasDit)
            {
                StateMachine(KEYER_LONG_PIN);
                lastWasDit = false;
            }
            else
            {
                StateMachine(KEYER_SHORT_PIN);
                lastWasDit = true;
            }

            lastKeyTime = millis();
            letterProcessed = false;
        }

        // ===== EINZEL DIT =====
        else if(dit && !iambicModeActive)
        {
            StateMachine(KEYER_SHORT_PIN);
            lastWasDit = true;

            lastKeyTime = millis();
            letterProcessed = false;
        }

        // ===== EINZEL DAH =====
        else if(dah && !iambicModeActive)
        {
            StateMachine(KEYER_LONG_PIN);
            lastWasDit = false;

            lastKeyTime = millis();
            letterProcessed = false;
        }

        // ===== IAMBIC MODE B: letzter Impuls =====
        else if(iambicModeActive)
        {
            iambicModeActive = false;

            // 🔥 letzter Gegenton
            if(lastWasDit)
            {
                StateMachine(KEYER_LONG_PIN);
                lastWasDit = false;
            }
            else
            {
                StateMachine(KEYER_SHORT_PIN);
                lastWasDit = true;
            }

            lastKeyTime = millis();
            letterProcessed = false;
        }
    }
    
    updateBeep();
    updatePlayback();

    // ===== Zeichenerkennung für Monitor und Trainer-Modi =====
    // Im Monitor-Modus: Zeige jeden erkannten Buchstaben an
    // Im Trainer-Modus: Vergleiche Eingabe mit erwartetem Buchstaben
    if(actual_menu == MONITOR || 
       actual_menu == TRAINER_GIVE_RANDOM_SCREEN || 
       actual_menu == TRAINER_GIVE_AZ_SCREEN)
    {
        unsigned long now = millis();

        // ===== Trigger: Zeichen wurde vollständig eingegeben =====
        // Prüfungen:
        // 1. decoderString hat Daten (mindestens ein Punkt/Strich)
        // 2. Kein Beep oder Pause läuft
        // 3. Genug Zeit ist vergangen seit letzter Eingabe (letterExtra)
        // 4. Zeichen wurde noch nicht verarbeitet (letterProcessed = false)
        if(decoderString.length() > 0 && 
           !beeping && 
           !inPause && 
           (now - lastKeyTime) >= letterExtra &&
           !letterProcessed)
        {
            letterProcessed = true;  // Verhindere Verarbeitung bis zum nächsten Zeichen

            String decodedLetter = DecodeMorseCode(decoderString);
            char buf[2] = { decodedLetter[0], '\0' };

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
            else if(actual_menu == TRAINER_GIVE_RANDOM_SCREEN || 
                    actual_menu == TRAINER_GIVE_AZ_SCREEN)
            {
                if(buf[0] == currentTrainerLetter)
                {
                    display.clear();
                    display.print("Correct !", 4, 4);
                }
                else
                {
                    display.clear();
                    display.print("Not Correct !", 4, 2);
                    ProcessSign(currentTrainerLetter);
                }

                delay(1000);

                if(actual_menu == TRAINER_GIVE_RANDOM_SCREEN)
                    ShowTrainerGiveRandomScreen();
                else
                    ShowTrainerGiveAZScreen();
            }
            else if(actual_menu == TRAINER_HEAR_SCREEN)
            {
                if(buf[0] == 'E' || buf[0] == 'T')
                {
                    ShowTrainerHearScreen();
                }
            }

            decoderString = ""; // 🔥 Buffer leeren für nächstes Zeichen
        }
    }

    StateMachine(NO_KEY);
}

/////////////////////////////////////////////////////////////////
/// StateMachine() - Zentrale Eingabeverarbeitung
// Diese Funktion verwaltet die Interpretation von Benutzereingaben:
// - KEYER_SHORT_PIN: Punkt eingeben (dit)
// - KEYER_LONG_PIN: Strich eingeben (dah)
// - MODE_BUTTON_PIN: Menünavigation und Funktionen
/////////////////////////////////////////////////////////////////
void StateMachine(int key)
{
  // Gegenwärtig gibt es nur einen Zustand (STATE_IDLE)
  // Dies könnte in Zukunft erweitert werden (z.B. für Menü-Bearbeitung)
  if(State == STATE_IDLE)
  {
    // ===== Kurze Paddel-Taste (Punkt) =====
    if(key == KEYER_SHORT_PIN)
    {
      // Nur verarbeiten, wenn kein Ton läuft (verhindert Überlagerung)
      if(!beeping && !inPause)
      {
        ProcessBeep(dit_len, '.');  // Punkt-Beep mit korrekter Länge
        decoderString += ".";       // Punkt zum Dekodierungs-Buffer hinzufügen
        key_activated = millis();   // Zeitstempel speichern
      }
    }

    // ===== Lange Paddel-Taste (Strich) =====
    if(key == KEYER_LONG_PIN)
    {
      // Nur verarbeiten, wenn kein Ton läuft
      if(!beeping && !inPause)
      {
        ProcessBeep(dah_len, '-');  // Strich-Beep mit korrekter Länge
        decoderString += "-";       // Strich zum Dekodierungs-Buffer hinzufügen
        key_activated = millis();   // Zeitstempel speichern
      }
    }

    // ===== Mode-Button (Menü-Navigation und Funktionsaufrufe) =====
    if(key == MODE_BUTTON_PIN)
    {
      ReactOnButtonClick();  // Rufe Menü-Handler auf
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

    if(selected_menu_item == SETUP_FARNSWORTH && fspeed_mode == false)
    {
      fspeed_mode = true;
    }
    else if(selected_menu_item == SETUP_FARNSWORTH && fspeed_mode == true)
    {
      fspeed_mode = false;
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
/// SwitchSpeaker() - Aktiviert oder deaktiviert Buzzer
// Diese Funktion schaltet den Buzzer-Output um und speichert
// den Status im EEPROM, damit er beim nächsten Start wieder
// hergestellt wird.
/////////////////////////////////////////////////////////////////
void SwitchSpeaker(byte value)
{
  // Konvertiere Byte-Wert zu Boolean
  if(value == 0)
    speakerOn = false;
  else
    speakerOn = true;

  // Speichere Status im EEPROM
  EEPROM.write(EEPROM_SPEAKER_ADDR, speakerOn);
  EEPROM.commit();  // Schreibe in EEPROM
}

/////////////////////////////////////////////////////////////////
/// SwitchSettings() - Aktiviert oder deaktiviert WiFi und Web-Server
// Diese Funktion ermöglicht die WiFi-Verbindung und startet
// einen Access Point, um sich über Browser zu verbinden und
// Einstellungen zu ändern (Speichertexte, Trainer-Buchstaben).
/////////////////////////////////////////////////////////////////
void SwitchSettings(byte value)
{
  if(value == 0) 
  {
    // ===== WiFi und Server AUSSCHALTEN =====
    settingsOn = false;

    // Herunterfahren aller WiFi-Komponenten
    server.stop();                  // Stoppe Web-Server
    WiFi.disconnect(true);          // Trenne WiFi und deaktiviere AP
    WiFi.mode(WIFI_OFF);            // Schalte WLAN komplett aus
    DEBUG_PRINTLN("WiFi/Server OFF");
  }
  else 
  {
    // ===== WiFi und Server EINSCHALTEN =====
    settingsOn = true;

    // ===== Starte Access Point =====
    WiFi.mode(WIFI_AP);            // Wechsel zu Access Point Modus
    WiFi.softAP(ssid, password);    // Erstelle AP mit SSID und Passwort

    // Setze feste IP-Adresse für den Access Point
    IPAddress Ip(192, 168, 4, 2);           // AP IP-Adresse
    IPAddress NMask(255, 255, 255, 0);     // Netzmaske
    WiFi.softAPConfig(Ip, Ip, NMask);

    DEBUG_PRINTLN(WiFi.localIP());

    // ===== Registriere Web-Handler (Routen) =====
    server.on("/", handleRoot);             // GET / -> Zeige HTML
    server.on("/save_letters", handleSaveLetters);  // POST /save_letters -> Speichere Buchstaben
    server.on("/get", handleGet);           // GET /get?input1=... -> Speichere Morse-Text
    server.onNotFound(notFound);            // 404 Handler
    server.begin();                         // Starte Web-Server

    DEBUG_PRINTLN("WiFi/Server ON");
  }
}

/////////////////////////////////////////////////////////////////
/// ProcessBeep() - Verarbeitet Beep nicht-blockierend
// Diese Funktion wird von StateMachine aufgerufen, wenn eine
// Paddel-Taste gedrückt wird. Sie startet den Beep-Ton
// und speichert das Symbol für die Dekodierung.
/////////////////////////////////////////////////////////////////
void ProcessBeep(short beepoLength, char outputChar)
{
    tone(SPEAKER_PIN, 1000);   // Starte 1000 Hz Ton

    // Schalte Buzzer ein, wenn aktiviert
    if(speakerOn) 
    {
        digitalWrite(BUZZER_PIN, HIGH);
    }

    beeping = true;                      // Markiere als aktiv
    beepEnd = millis() + beepoLength;   // Berechne Endzeit
    // Pause nach Beep wird automatisch in updateBeep() gestartet
    DEBUG_PRINT(outputChar);  // Debug-Ausgabe
}

/////////////////////////////////////////////////////////////////
/// ProcessSign() - Spielt ein einzelnes Zeichen als Morse-Code
// Diese Funktion wird vom Trainer verwendet, um das korrekte
// Zeichen abzuspielen, wenn der Benutzer einen Fehler macht.
/////////////////////////////////////////////////////////////////
void ProcessSign(char sign)
{
  // Starte Playback eines einzelnen Zeichens (z.B. 'A' -> ".-")
  startPlayback(String(sign));
}

/////////////////////////////////////////////////////////////////
/// PlayMemory() - Spielt gespeicherten Text aus EEPROM ab
// Diese Funktion wird vom CW-Keyer-Menü aufgerufen, wenn der
// Benutzer einen der beiden Speicher abspielen möchte.
/////////////////////////////////////////////////////////////////
void PlayMemory(byte addr)
{
  // Lese Text aus EEPROM
  String text = ReadTextFromEEPROM(addr);
  DEBUG_PRINTLN(text);

  // Sicherheitsprüfung: nur abspielen, wenn Text nicht leer ist
  if(text.length() <= 0)
    return;

  // Starte nicht-blockierende Wiedergabe
  startPlayback(text);
}

/////////////////////////////////////////////////////////////////
/// EncodeChar() - Konvertiert Zeichen zu Morse-Code
// Diese Funktion nimmt einen ASCII-Buchstaben oder Ziffer und
// gibt den entsprechenden Morse-Code zurück (z.B. 'A' -> ".-").
// Der Morse-Code ist im PROGMEM gespeichert (Flash-Speicher).
/////////////////////////////////////////////////////////////////
String EncodeChar(char sign)
{
  char buffer[6]; // max Morse-Zeichen + null-Terminator (z.B. "-----")

  // ===== Große und kleine Buchstaben =====
  if (sign >= 'a' && sign <= 'z') 
  {
    strcpy_P(buffer, MORSE_LETTERS[sign - 'a']);  // Lese aus PROGMEM
    return String(buffer);
  } 
  else if (sign >= 'A' && sign <= 'Z') 
  {
    strcpy_P(buffer, MORSE_LETTERS[sign - 'A']);
    return String(buffer);
  } 
  // ===== Ziffern =====
  else if (sign >= '0' && sign <= '9') 
  {
    strcpy_P(buffer, MORSE_NUMBERS[sign - '0']);
    return String(buffer);
  }

  return "";  // Unbekanntes Zeichen
}

/////////////////////////////////////////////////////////////////
/// DecodeMorseCode() - Konvertiert Morse-Code zu Zeichen
// Diese Funktion nimmt einen Morse-Code-String (z.B. ".-")
// und gibt das entsprechende Zeichen zurück (z.B. 'A').
// Wird vom Monitor und Trainer verwendet.
/////////////////////////////////////////////////////////////////
String DecodeMorseCode(String code) 
{
  // ===== Durchsuche alle 26 Buchstaben =====
  for (int i = 0; i < 26; i++) 
  {
    char buffer[6];
    strcpy_P(buffer, MORSE_LETTERS[i]);  // Lese Morse-Code aus PROGMEM
    if (strcmp(buffer, code.c_str()) == 0)  // Vergleiche mit Eingabe
    {
      char letterChar = pgm_read_byte(&LETTERS[i]);  // Hole Buchstaben
      return String(letterChar);
    }
  }

  // ===== Durchsuche alle 10 Ziffern =====
  for (int i = 0; i < 10; i++) 
  {
    char buffer[6];
    strcpy_P(buffer, MORSE_NUMBERS[i]);
    if (strcmp(buffer, code.c_str()) == 0)
    {
      return String(char('0' + i));  // Konvertiere Index zu Ziffer
    }
  }

  return "*";  // Keine Entsprechung gefunden
}

/////////////////////////////////////////////////////////////////
// ShowMainScreen
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

  if(settingsOn == true)
        display.print("-192.168.4.2-", 6,2);
      else
        display.print("             ", 6,2);

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

  char string[20];
  snprintf(string, sizeof(string), "Speed: %i WPM", wpm);
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
  display.print("Farnsworth", 4,4);

  char string[20];
  snprintf(string, sizeof(string), "FW: %i WPM", fwpm);
  display.print(string, 6,2);

  display.print("Back", 5,4);
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
void CalculateTimes(char wpm, char fwpm)
{
  if (wpm <= 0) 
    wpm = 1;
  if (fwpm > wpm) 
    fwpm = wpm;  // Farnsworth darf nicht schneller sein

  unsigned int T = 1200 / wpm;    // Basis-Zeit (Elementlänge)
  unsigned int Tfw = 1200 / fwpm; // Farnsworth-Basis (verlängerte Pausen)

  // ===== Berechne globale Timing-Konstanten =====
  T_unit    = T;                // Speichere Basis-Zeit für Referenz
  dit_len   = T;                // Punkt = 1T
  dah_len   = 3 * T;            // Strich = 3T
  elementGap = T;               // Pause zwischen Punkt/Strich = 1T

  // ===== Farnsworth-Pausen (verlängert) =====
  // Buchstabenpause: Total 3T nach Farnsworth
  // Da 1T (elementGap) nach dem letzten Element bereits gezählt wird,
  // addieren wir nur die Differenz (2T extra)
  letterExtra = (3 * Tfw) - elementGap;  // Zusätzliche Pause nach Buchstabe

  // Wortpause: Total 7T nach Farnsworth
  // Da 1T (elementGap) bereits gezählt wird, addieren wir die Differenz (6T extra)
  wordExtra = (7 * Tfw) - elementGap;    // Zusätzliche Pause nach Wort

  DEBUG_PRINT("T: "); DEBUG_PRINTLN(T);
  DEBUG_PRINT("Tfw: "); DEBUG_PRINTLN(Tfw);
  DEBUG_PRINT("dit: "); DEBUG_PRINTLN(dit_len);
  DEBUG_PRINT("dah: "); DEBUG_PRINTLN(dah_len);
  DEBUG_PRINT("elementGap: "); DEBUG_PRINTLN(elementGap);
  DEBUG_PRINT("letterExtra: "); DEBUG_PRINTLN(letterExtra);
  DEBUG_PRINT("wordExtra: "); DEBUG_PRINTLN(wordExtra);
}

/////////////////////////////////////////////////////////////////
/// WriteTextToEEPROM() - Speichert einen Text im EEPROM
// Diese Funktion schreibt einen String ab einer bestimmten Adresse
// ins EEPROM des ESP8266. Der String wird null-terminiert, um sein
// Ende zu markieren (Standard C-String-Konvention).
// Restlicher Speicher wird mit '@' gefüllt (Padding).
/////////////////////////////////////////////////////////////////
void WriteTextToEEPROM(byte addr, String text)
{
  // Schreibe jeden Buchstaben des Strings
  for (int i = 0; i < text.length(); i++)
  {
    EEPROM.write(addr, text[i]);  // Schreibe Zeichen
    addr += 1;                      // Nächste EEPROM-Adresse
  }
  EEPROM.write(addr, 0);           // Nullterminator schreiben (String-Ende)
  addr += 1;

  // Optionales Padding: Rest mit '@' auffüllen (Erkennungsmarker)
  // Dies verhindert, dass alte Daten beim Lesen angehängt werden
  for (int i = addr; i < 128; i++)
  {
    EEPROM.write(i, '@');
  }
  EEPROM.commit();  // Schreibe alle Änderungen in EEPROM
}

/////////////////////////////////////////////////////////////////
/// ReadTextFromEEPROM() - Liest einen Text aus dem EEPROM
// Diese Funktion liest ab einer EEPROM-Adresse, bis ein
// Nullterminator (0x00) oder ein Padding-Zeichen ('@') gefunden wird.
// Gibt den gelesenen String zurück.
/////////////////////////////////////////////////////////////////
String ReadTextFromEEPROM(byte addr)
{
  String retVal;  // Ergebnis-String
  for (int i = addr; i < 128; i++)   // Lese bis zur EEPROM-Grenze
  {
    byte readValue = EEPROM.read(i);  // Lese Byte aus EEPROM
    if (readValue == 0)               // Nullterminator = String-Ende
      break;
    char readValueChar = char(readValue);
    if(readValueChar != '@')          // Ignoriere Padding-Zeichen
      retVal += readValueChar;        // Hänge Zeichen an String
  }
  DEBUG_PRINTLN(retVal);  // Debug-Ausgabe
  return retVal;
}

/////////////////////////////////////////////////////////////////
/// DisplaySelectionArrow() - Zeigt Menü-Auswahlpfeil an
// Diese Funktion entfernt den alten Pfeil und platziert ihn
// an der Position des aktuellen Menü-Elements. Dies ermöglicht
// eine visuelle Navigation durch die Menü-Optionen.
/////////////////////////////////////////////////////////////////
void DisplaySelectionArrow()
{
  // Entferne Pfeil von allen möglichen Positionen (Zeilen 2-6)
  display.print(" ", 2,1);
  display.print(" ", 3,1);
  display.print(" ", 4,1);
  display.print(" ", 5,1);
  display.print(" ", 6,1);

  // Platziere Pfeil an der aktuellen Menü-Position
  // (selected_menu_item + 1, da Menü-Items bei 1 anfangen, aber Display-Zeilen bei 0)
  display.print(">", selected_menu_item+1, 1);
}

/////////////////////////////////////////////////////////////////
/// rotate() - Rotary Encoder Drehung verarbeiten
// Diese Funktion wird jedes Mal aufgerufen, wenn der Encoder
// gedreht wird. Je nach aktuellem Menü:
// - Im CW_KEYER Menü: Ändert die Geschwindigkeit (WPM) wenn speed_mode=true
// - Im SETUP Menü: Ändert die Farnsworth-Geschwindigkeit wenn fspeed_mode=true
// - In anderen Menüs: Navigiert durch Menü-Optionen
/////////////////////////////////////////////////////////////////
void rotate(Rotary& r) 
{
  // ===== Geschwindigkeit einstellen im Keyer-Menü =====
  if(actual_menu == CW_KEYER && speed_mode == true)
  {
    // Erhöhe oder senke WPM basierend auf Encoder-Richtung
    if(r.getDirection() == 1)      // Drehung rechts = höhere Geschwindigkeit
    {
         wpm += STEP_SIZE;  // Erhöhe um STEP_SIZE (z.B. 5 WPM)
    }
    else                             // Drehung links = niedrigere Geschwindigkeit
    {
      if(wpm > 0)
      {
        wpm -= STEP_SIZE;   // Senke um STEP_SIZE
      }
    }

    // Aktualisiere Display mit neuer WPM
    char string[20];
    snprintf(string, sizeof(string), "Speed: %i WPM", wpm);
    display.print(string, 6,2);

    CalculateTimes(wpm, fwpm);  // Neuberechnung aller Timings
    EEPROM.write(EEPROM_WPM_ADDR, wpm);  // Speichere in EEPROM
    EEPROM.commit();
    return;
  }

  // ===== Farnsworth-Geschwindigkeit im Setup-Menü =====
  if(actual_menu == SETUP && fspeed_mode == true)
  {
    if(r.getDirection() == 1)       // Drehung rechts
    {
         fwpm += STEP_SIZE;
    }
    else                             // Drehung links
    {
      if(wpm > 0)  // Prüfe normale WPM, nicht fwpm (Bug?)
      {
        fwpm -= STEP_SIZE;
      }
    }

    char string[20];
    snprintf(string, sizeof(string), "FW: %i WPM", fwpm);
    display.print(string, 6,2);

    CalculateTimes(wpm, fwpm);  // Neuberechnung aller Timings
    EEPROM.write(EEPROM_FWPM_ADDR, fwpm);  // Speichere in EEPROM
    EEPROM.commit();
    return;
  }  // ===== Menü-Navigation =====
  // Im Keyer-Menü nach oben/unten navigieren

  {
    selected_menu_item++;  // Zum nächsten Menü-Element

    if(selected_menu_item > KEYER_MENU_COUNT)  // Zyklisch: Wenn am Ende, gehe zum Anfang
      selected_menu_item = 1;

    DisplaySelectionArrow();  // Zeige neue Position an
    return;
  }

  // Im Haupt-Menü (bietet Zirkulation in beide Richtungen)
  if(actual_menu == MAIN_MENU)
  {
    if(r.getDirection() == 1)  // Drehung rechts = Menü nach unten
    {
      selected_menu_item++;

      if(selected_menu_item > MAIN_MENU_COUNT)
        selected_menu_item = 1;  // Zyklisch
    }
    else                         // Drehung links = Menü nach oben
    {
      selected_menu_item--;
      if(selected_menu_item < 1)
        selected_menu_item = MAIN_MENU_COUNT;  // Zyklisch
    }

    DisplaySelectionArrow();

    return;
  }

  // Im Setup-Menü navigieren
  if(actual_menu == SETUP)
  {
    selected_menu_item++;

    if(selected_menu_item > SETUP_MENU_COUNT)
      selected_menu_item = 1;

    DisplaySelectionArrow();

    return;
  }

  // Im Trainer-Menü navigieren
  if(actual_menu == TRAINER)
  {
    selected_menu_item++;

    if(selected_menu_item > TRAINER_MENU_COUNT)
      selected_menu_item = 1;

    DisplaySelectionArrow();

    return;
  }
}
