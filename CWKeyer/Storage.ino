/////////////////////////////////////////////////////////////////
/// WriteTextToEEPROM() - Save Text String to EEPROM
// Writes a string starting at specified address to EEPROM
// String is null-terminated to mark end (standard C convention)
// Remaining memory is filled with '@' as padding marker
/////////////////////////////////////////////////////////////////
void WriteTextToEEPROM(byte addr, String text)
{
  // Write each character of the string
  for (int i = 0; i < text.length(); i++)
  {
    EEPROM.write(addr, text[i]);  // Write character
    addr += 1;                      // Next EEPROM address
  }
  EEPROM.write(addr, 0);           // Write null terminator (string end)
  addr += 1;

  // Optional padding: Fill rest with '@' (recognition marker)
  // Prevents old data from being appended when reading
  for (int i = addr; i < 128; i++)
  {
    EEPROM.write(i, '@');
  }
  EEPROM.commit();  // Write all changes to EEPROM
}

/////////////////////////////////////////////////////////////////
/// ReadTextFromEEPROM() - Read Text String from EEPROM
// Reads from specified EEPROM address until null terminator (0x00)
// or padding character ('@') is found
// Returns the read string
/////////////////////////////////////////////////////////////////
String ReadTextFromEEPROM(byte addr)
{
  String retVal;  // Result string

  for (int i = addr; i < 128; i++)   // Read until EEPROM boundary
  {
    byte readValue = EEPROM.read(i);  // Read byte from EEPROM
    if (readValue == 0)               // Null terminator = string end
      break;
    char readValueChar = char(readValue);
    if(readValueChar != '@')          // Ignore padding characters
      retVal += readValueChar;        // Append character to string
  }
  DEBUG_PRINTLN(retVal);  // Debug output
  return retVal;
}
