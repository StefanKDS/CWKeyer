/////////////////////////////////////////////////////////////////
/// notFound() - HTTP 404 Handler
// Sends a 404 error page when client requests non-existent path
/////////////////////////////////////////////////////////////////
void notFound()
{
  server.send(404, "text/html", wrapInPage("<p>Page not found</p>"));
}

/////////////////////////////////////////////////////////////////
/// handleRoot() - HTTP GET / (Main Page)
// Serves the HTML page when a client calls the root URL
// Loads stored texts and checks which letters are selected for training
/////////////////////////////////////////////////////////////////
void handleRoot()
{
  String page = index_html;  // Load HTML template

  // Read stored texts from EEPROM
  String text1 = ReadTextFromEEPROM(EEPROM_MEM1_ADDR);
  String text2 = ReadTextFromEEPROM(EEPROM_MEM2_ADDR);

  // Replace placeholders in HTML template with current values
  page.replace("%TEXT1%", text1);
  page.replace("%TEXT2%", text2);
  page.replace("%TRAINER_CHARACTERS%", generateTrainerCharacterCheckboxes());

  // Send cache control header (prevents old versions on Android)
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");

  // Send HTML page to client
  server.send(200, "text/html", page);
}

/////////////////////////////////////////////////////////////////
/// handleSaveLetters() - HTTP POST /save_letters
// Processes trainer form: saves selected letters to EEPROM
// and updates global variable
/////////////////////////////////////////////////////////////////
void handleSaveLetters()
{
  String selected = "";
  int args = server.args();  // Number of form parameters

  // Collect all selected letters (parameter name: "letters")
  for (int i = 0; i < args; i++)
  {
    if (server.argName(i) == "characters" || server.argName(i) == "letters")
    {
      String value = server.arg(i);
      for (unsigned int j = 0; j < value.length(); j++)
      {
        char character = value[j];
        if (character != '\0' &&
            strchr(TRAINER_CHARACTERS, character) != NULL &&
            selected.indexOf(character) == -1)
          selected += character;
      }
    }
  }

  if (selected.length() == 0)
    selected = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  // Save selection to EEPROM and global variable
  WriteTextToEEPROM(0x40, selected);
  selectedLetters = selected;  // Update global variable for trainer

  // Send confirmation response to client
  String content = "<p>Saved: " + selected + "</p>";
  server.send(200, "text/html", wrapInPage(content));
}

/////////////////////////////////////////////////////////////////
/// handleGet() - HTTP GET /get?input1=... or /get?input2=...
// Saves Morse text from web form to EEPROM
// Parameter input1 -> EEPROM_MEM1_ADDR
// Parameter input2 -> EEPROM_MEM2_ADDR
/////////////////////////////////////////////////////////////////
void handleGet()
{
  String inputParam = "none";  // Which parameter was received

  if (server.hasArg(TEXT_1))    // Check for input1 parameter
  {
    WriteTextToEEPROM(EEPROM_MEM1_ADDR, server.arg(TEXT_1));
    inputParam = TEXT_1;
  }
  else if (server.hasArg(TEXT_2))  // Check for input2 parameter
  {
    WriteTextToEEPROM(EEPROM_MEM2_ADDR, server.arg(TEXT_2));
    inputParam = TEXT_2;
  }

  String content = "<p>Saved: " + inputParam + "</p>";
  server.send(200, "text/html", wrapInPage(content));
}

/////////////////////////////////////////////////////////////////
/// generateTrainerCharacterCheckboxes() - Generate Trainer Character Options
// Generates HTML for letters, numbers, and supported punctuation
/////////////////////////////////////////////////////////////////
String generateTrainerCharacterCheckboxes()
{
  String html = "";

  for (unsigned int i = 0; TRAINER_CHARACTERS[i] != '\0'; i++)
  {
    char character = TRAINER_CHARACTERS[i];
    String value = character == '&' ? String("&amp;") : String(character);
    bool checked = selectedLetters.indexOf(character) != -1;

    html += "<input type='checkbox' name='characters' value='" + value + "' id='c" + String(i) + "'";
    if (checked) html += " checked";
    html += ">";
    html += "<label for='c" + String(i) + "'>" + value + "</label> ";

    if ((i + 1) % 10 == 0)
      html += "<br>";
  }
  return html;
}
