/////////////////////////////////////////////////////////////////
/// EncodeChar() - Convert Character to Morse Code
// Takes ASCII letter or digit and returns corresponding Morse code
// (e.g. 'A' -> ".-")
// Morse code is stored in PROGMEM (Flash memory)
/////////////////////////////////////////////////////////////////
String EncodeChar(char sign)
{
  char buffer[6]; // Max Morse characters + null terminator (e.g. "-----")

  // ===== Upper and lower case letters =====
  if (sign >= 'a' && sign <= 'z')
  {
    strcpy_P(buffer, MORSE_LETTERS[sign - 'a']);  // Read from PROGMEM
    return String(buffer);
  }
  else if (sign >= 'A' && sign <= 'Z')
  {
    strcpy_P(buffer, MORSE_LETTERS[sign - 'A']);
    return String(buffer);
  }
  // ===== Digits =====
  else if (sign >= '0' && sign <= '9')
  {
    strcpy_P(buffer, MORSE_NUMBERS[sign - '0']);
    return String(buffer);
  }
  else if (sign == '?') return "..--..";
  else if (sign == '.') return ".-.-.-";
  else if (sign == ',') return "--..--";
  else if (sign == '!') return "-.-.--";
  else if (sign == '@') return ".--.-.";
  else if (sign == ':') return "---...";
  else if (sign == '-') return "-....-";
  else if (sign == '/') return "-..-.";
  else if (sign == '(') return "-.--.";
  else if (sign == ')') return "-.--.-";
  else if (sign == '&') return ".-...";
  else if (sign == '$') return "...-..-";
  else if (sign == '+') return ".-.-.";
  else if (sign == '=') return "-...-";

  return "";  // Unknown character
}

/////////////////////////////////////////////////////////////////
/// DecodeMorseCode() - Convert Morse Code to Character
// Takes Morse code string (e.g. ".-") and returns corresponding
// character (e.g. 'A')
// Used by monitor and trainer
/////////////////////////////////////////////////////////////////
String DecodeMorseCode(String code)
{
  // ===== Search all 26 letters =====
  for (int i = 0; i < 26; i++)
  {
    char buffer[6];
    strcpy_P(buffer, MORSE_LETTERS[i]);  // Read Morse code from PROGMEM
    if (strcmp(buffer, code.c_str()) == 0)  // Compare with input
    {
      char letterChar = pgm_read_byte(&LETTERS[i]);  // Get letter
      return String(letterChar);
    }
  }

  // ===== Search all 10 digits =====
  for (int i = 0; i < 10; i++)
  {
    char buffer[6];
    strcpy_P(buffer, MORSE_NUMBERS[i]);
    if (strcmp(buffer, code.c_str()) == 0)
    {
      return String(char('0' + i));  // Convert index to digit
    }
  }

  if (code == "..--..") return "?";
  if (code == ".-.-.-") return ".";
  if (code == "--..--") return ",";
  if (code == "-.-.--") return "!";
  if (code == ".--.-.") return "@";
  if (code == "---...") return ":";
  if (code == "-....-") return "-";
  if (code == "-..-.") return "/";
  if (code == "-.--.") return "(";
  if (code == "-.--.-") return ")";
  if (code == ".-...") return "&";
  if (code == "...-..-") return "$";
  if (code == ".-.-.") return "+";
  if (code == "-...-") return "=";

  return "*";  // No match found
}
