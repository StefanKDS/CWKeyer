/////////////////////////////////////////////////////////////////
// Audio Decoder - Goertzel Algorithm for Morse Audio Input
/////////////////////////////////////////////////////////////////
// ===== Goertzel Variables =====
const int audioInPin = A0;                // Analog input pin for audio decoder
const float sampling_freq = 8928.0;       // Sampling frequency for Goertzel algorithm
const float target_freq = 700.0;          // Target frequency (optimized for CW audio, minimum 700Hz required)
const int decoder_n = 24;                 // Number of samples per Goertzel iteration

float decoder_coeff;                      // Goertzel coefficient (precomputed)
float decoder_Q1 = 0;                     // Goertzel Q1 state variable
float decoder_Q2 = 0;                     // Goertzel Q2 state variable
int decoder_testData[24];                 // Buffer for audio samples

// ===== State Tracking =====
int decoder_realstate = LOW;              // Current audio state (raw)
int decoder_realstatebefore = LOW;        // Previous raw state
int decoder_filteredstate = LOW;          // Debounced audio state
int decoder_filteredstatebefore = LOW;    // Previous debounced state

// ===== Magnitude Tracking =====
float decoder_magnitude;                  // Magnitude of Goertzel output
int decoder_magnitudelimit = 100;         // Adaptive threshold for signal detection
const int decoder_magnitudelimit_low = 100;  // Minimum threshold

// ===== Timing Variables =====
const int decoder_nbtime = 200;           // Debounce time in microseconds
unsigned long decoder_starttimehigh;      // When HIGH state started
unsigned long decoder_highduration;       // Duration of HIGH state (tone)
unsigned long decoder_hightimesavg = 100000;  // Average HIGH duration (~60ms for 20 WPM)
unsigned long decoder_startttimelow;      // When LOW state started
unsigned long decoder_lowduration;        // Duration of LOW state (silence)
unsigned long decoder_laststarttime = 0;  // Last state change time

// ===== Morse Code Buffer =====
char decoder_code[20] = "";               // Accumulated Morse code string (e.g. ".-")
int decoder_stop = LOW;                   // Flag to prevent multiple detections

// ===== Sampling State Machine =====
int decoder_sampleIndex = 0;              // Current sample index in buffer
bool decoder_readyToProcess = false;      // Flag indicating buffer is full and ready to process

/////////////////////////////////////////////////////////////////
/// DecoderInit() - Initialize decoder at setup
/////////////////////////////////////////////////////////////////
void DecoderInit()
{
  // Calculate Goertzel coefficient for target frequency
  int k = (int)(0.5 + ((decoder_n * target_freq) / sampling_freq));
  float omega = (2.0 * PI * k) / decoder_n;
  decoder_coeff = 2.0 * cos(omega);

  DEBUG_PRINTLN("Audio Decoder initialized");
}

/////////////////////////////////////////////////////////////////
/// DecoderUpdate() - Non-blocking decoder update
// Call this in every loop when actual_menu == MONITOR
// Implements a two-phase approach: sampling and processing
/////////////////////////////////////////////////////////////////
void DecoderUpdate()
{
  // ===== Sampling Phase: Collect samples non-blockingly =====
  if (decoder_sampleIndex < decoder_n)
  {
    decoder_testData[decoder_sampleIndex] = analogRead(audioInPin);
    decoder_sampleIndex++;

    if (decoder_sampleIndex >= decoder_n)
    {
      decoder_readyToProcess = true;
    }
    return;  // Exit this iteration briefly
  }

  // ===== Processing Phase: Process collected samples =====
  if (decoder_readyToProcess)
  {
    DecoderProcessGoertzel();       // Run Goertzel algorithm on samples
    DecoderUpdateStateLogic();      // Apply hysteresis and debouncing
    DecoderDetectMorse();           // Detect and decode Morse characters

    decoder_sampleIndex = 0;
    decoder_readyToProcess = false;
  }
}

/////////////////////////////////////////////////////////////////
/// DecoderProcessGoertzel() - Goertzel Algorithm
// Implements the Goertzel DFT algorithm to detect target frequency
// in audio samples
/////////////////////////////////////////////////////////////////
void DecoderProcessGoertzel()
{
  // ===== Goertzel DFT Calculation =====
  for (int i = 0; i < decoder_n; i++)
  {
    float Q0 = decoder_coeff * decoder_Q1 - decoder_Q2 + (float)decoder_testData[i];
    decoder_Q2 = decoder_Q1;
    decoder_Q1 = Q0;
  }

  // ===== Calculate Magnitude =====
  float magnitudeSquared = (decoder_Q1 * decoder_Q1) + (decoder_Q2 * decoder_Q2) - decoder_Q1 * decoder_Q2 * decoder_coeff;
  decoder_magnitude = sqrt(magnitudeSquared);

  // Reset for next iteration
  decoder_Q1 = 0;
  decoder_Q2 = 0;

  // ===== Adaptive Magnitude Limit (IMPORTANT for different speeds) =====
  // Continuously adapt threshold to compensate for varying input levels
  if (decoder_magnitude > decoder_magnitudelimit_low)
  {
    decoder_magnitudelimit += (decoder_magnitude - decoder_magnitudelimit) / 6;
  }

  if (decoder_magnitudelimit < decoder_magnitudelimit_low)
    decoder_magnitudelimit = decoder_magnitudelimit_low;
}

/////////////////////////////////////////////////////////////////
/// DecoderUpdateStateLogic() - Hysteresis and Debouncing
// Applies threshold with hysteresis and debouncing to stabilize
// state transitions
/////////////////////////////////////////////////////////////////
void DecoderUpdateStateLogic()
{
  unsigned long now = micros();

  // ===== Signal threshold with hysteresis =====
  if (decoder_magnitude > decoder_magnitudelimit * 0.6)
    decoder_realstate = HIGH;
  else
    decoder_realstate = LOW;

  // ===== Detect state change =====
  if (decoder_realstate != decoder_realstatebefore)
    decoder_laststarttime = now;

  // ===== Debouncing: Only update on stable state change =====
  if ((now - decoder_laststarttime) > decoder_nbtime)
    decoder_filteredstate = decoder_realstate;

  decoder_realstatebefore = decoder_realstate;
}

/////////////////////////////////////////////////////////////////
/// DecoderDetectMorse() - Morse Detection
// Measures tone and silence durations to detect dots and dashes
// Recognizes character boundaries and word boundaries by timeout
/////////////////////////////////////////////////////////////////
void DecoderDetectMorse()
{
  unsigned long now = micros();

  if (decoder_filteredstate != decoder_filteredstatebefore)
  {
    decoder_stop = LOW;

    // ===== Transition from HIGH to LOW: Tone ended =====
    if (decoder_filteredstate == LOW)
    {
      decoder_startttimelow = now;
      decoder_highduration = now - decoder_starttimehigh;

      // ===== Dot or dash? =====
      if (decoder_highduration < (decoder_hightimesavg * 2))
      {
        // Dot detected - learn from the dot duration
        decoder_hightimesavg = (decoder_highduration + decoder_hightimesavg + decoder_hightimesavg) / 3;
        if (strlen(decoder_code) < sizeof(decoder_code) - 1)
          strcat(decoder_code, ".");
      }
      else
      {
        // Dash detected - extrapolate dot length from dash (Dash = 3×Dot)
        unsigned long dit_estimate = decoder_highduration / 3;
        decoder_hightimesavg = (dit_estimate + decoder_hightimesavg + decoder_hightimesavg) / 3;
        if (strlen(decoder_code) < sizeof(decoder_code) - 1)
          strcat(decoder_code, "-");
      }
    }

    // ===== Transition from LOW to HIGH: Pause after tone =====
    if (decoder_filteredstate == HIGH)
    {
      decoder_starttimehigh = now;
      decoder_lowduration = now - decoder_startttimelow;

      // ===== Long pause = character separation =====
      if (decoder_lowduration > decoder_hightimesavg * 2)
      {
        DecoderDecodeMorse();  // Decode accumulated Morse code
        decoder_code[0] = '\0';  // Clear buffer
      }
    }
  }

  // ===== Timeout: Very long pause = word end =====
  if ((now - decoder_startttimelow) > (decoder_highduration * 6) && decoder_stop == LOW)
  {
    DecoderDecodeMorse();
    decoder_code[0] = '\0';
    decoder_stop = HIGH;
  }

  decoder_filteredstatebefore = decoder_filteredstate;
}

/////////////////////////////////////////////////////////////////
/// DecoderDecodeMorse() - Morse Code to ASCII Conversion
// Decodes accumulated Morse code string to ASCII character
// Supports letters (A-Z), numbers (0-9), and special characters
/////////////////////////////////////////////////////////////////
void DecoderDecodeMorse()
{
  if (strlen(decoder_code) == 0) return;

  char c = ' ';

  // ===== Letters (A-Z) and numbers (0-9) from PROGMEM tables =====
  for (byte i = 0; i < 26; i++)
    if (strcmp_P(decoder_code, MORSE_LETTERS[i]) == 0) c = pgm_read_byte(&LETTERS[i]);
  for (byte i = 0; i < 10; i++)
    if (strcmp_P(decoder_code, MORSE_NUMBERS[i]) == 0) c = '0' + i;
  if (c == ' ')
  {
    // ===== Special Characters =====
    if (strcmp(decoder_code, "..--..") == 0) c = '?';
    else if (strcmp(decoder_code, ".-.-.-") == 0) c = '.';
    else if (strcmp(decoder_code, "--..--") == 0) c = ',';
    else if (strcmp(decoder_code, "-.-.--") == 0) c = '!';
    else if (strcmp(decoder_code, ".--.-.") == 0) c = '@';
    else if (strcmp(decoder_code, "---...") == 0) c = ':';
    else if (strcmp(decoder_code, "-....-") == 0) c = '-';
    else if (strcmp(decoder_code, "-..-.") == 0) c = '/';
    else if (strcmp(decoder_code, "-.--.") == 0) c = '(';
    else if (strcmp(decoder_code, "-.--.-") == 0) c = ')';
    else if (strcmp(decoder_code, ".-...") == 0) c = '&';
    else if (strcmp(decoder_code, "...-..-") == 0) c = '$';
    else if (strcmp(decoder_code, ".-.-.") == 0) c = '+';
    else if (strcmp(decoder_code, "-...-") == 0) c = '=';
  }

  // ===== Display decoded character =====
  if (c != ' ')
  {
    Serial.print(c);  // Debug output

    // Display character on monitor display
    if (actual_menu == MONITOR)
    {
      if(char_on_screen >= 90)
      {
        char_on_screen = 0;
        display.clear();
      }
      else char_on_screen++;

      int r, c_col;
      CalcDisplayPosition(char_on_screen, &r, &c_col);
      char buf[2] = {c, '\0'};
      display.print(buf, r, c_col);
    }
  }
}
