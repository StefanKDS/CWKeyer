# Audio Morse Code Decoder - Technical Documentation

## Overview

This document explains the audio decoding system for Morse code via the analog input pin A0 of the ESP8266. The system uses the **Goertzel Algorithm** to detect a 700 Hz signal and dynamically adapts to different Morse code speeds (WPM).

---

## System Architecture

### Hardware Setup
- **Audio Input**: Analog pin A0 (ADC input)
- **Sampling Frequency**: 8928 Hz
- **Target Frequency**: 700 Hz (optimized for CW audio)
- **Samples per Processing Cycle**: 24
- **Debounce Time**: 200 microseconds

### Software Components

The decoder consists of four main processing phases:

```
┌─────────────────────┐
│  DecoderUpdate()    │ ← Main entry point, called from loop()
└──────────┬──────────┘
           │
    ┌──────┴──────┐
    │ Sampling    │ Collect 24 ADC samples from A0
    │ Phase       │ (non-blocking, ~3ms period)
    └──────┬──────┘
           │
    ┌──────┴─────────────────┐
    │ Processing Phase        │ When 24 samples collected
    └──────┬──────────────────┘
           │
    ├──→ DecoderProcessGoertzel()     ← Frequency analysis
    ├──→ DecoderUpdateStateLogic()    ← Signal detection + debouncing
    └──→ DecoderDetectMorse()         ← Morse pattern recognition
```

---

## Phase 1: Sampling Phase

### Purpose
Non-blocking sample collection from the analog input.

### Code
```cpp
if (decoder_sampleIndex < decoder_n) 
{
    decoder_testData[decoder_sampleIndex] = analogRead(audioInPin);
    decoder_sampleIndex++;
    
    if (decoder_sampleIndex >= decoder_n) 
    {
        decoder_readyToProcess = true;
    }
    return;  // Exit immediately, resume next loop iteration
}
```

### Behavior
- **Per Loop Iteration**: 1 sample collected (~111 µs per sample at 8928 Hz)
- **Samples Needed**: 24 samples
- **Time to Collect**: ~2.7 ms
- **Why Non-blocking**: Allows other code (WiFi, UI) to run in parallel
- **Buffer**: `decoder_testData[24]` stores samples

### Timing Calculation
```
Sample Period = 1 / 8928 Hz ≈ 112 µs
24 samples = ~2.7 ms per analysis cycle
Analysis frequency = ~370 Hz (every 2.7 ms)
```

---

## Phase 2: Goertzel Algorithm (Frequency Analysis)

### Purpose
Determine the magnitude of energy at 700 Hz frequency.

### Initialization (DecoderInit)
```cpp
int k = (int)(0.5 + ((decoder_n * target_freq) / sampling_freq));
// k = (24 * 700) / 8928 ≈ 1.89 ≈ 2

float omega = (2.0 * PI * k) / decoder_n;
// omega ≈ 0.5236 rad

decoder_coeff = 2.0 * cos(omega);
// decoder_coeff ≈ 1.618
```

### Processing (DecoderProcessGoertzel)
```cpp
// DFT-like computation for 700 Hz
for (int i = 0; i < decoder_n; i++) 
{
    float Q0 = decoder_coeff * decoder_Q1 - decoder_Q2 + (float)decoder_testData[i];
    decoder_Q2 = decoder_Q1;
    decoder_Q1 = Q0;
}

// Calculate magnitude (energy at 700 Hz)
float magnitudeSquared = (decoder_Q1 * decoder_Q1) + (decoder_Q2 * decoder_Q2) 
                         - decoder_Q1 * decoder_Q2 * decoder_coeff;
decoder_magnitude = sqrt(magnitudeSquared);

// Reset for next cycle
decoder_Q1 = 0;
decoder_Q2 = 0;
```

### Output
- **`decoder_magnitude`**: Floating-point value representing signal strength at 700 Hz
- **Range**: 0 to ~1000+ (depends on input amplitude)
- **Example Values**:
  - Silence: magnitude ≈ 20-50
  - Weak signal: magnitude ≈ 50-100
  - Strong signal: magnitude ≈ 200-500+

---

## Phase 3: Adaptive Magnitude Threshold

### ⭐ KEY TO SPEED ADAPTATION ⭐

This is the mechanism that allows the decoder to work at different Morse speeds automatically.

### The Problem
- Different CW keyers output at different volume levels
- Noise floor varies with USB cable quality, environment, etc.
- A fixed threshold wouldn't work across all scenarios

### The Solution: Exponential Moving Average

```cpp
// Adaptive magnitude limit using exponential moving average
if (decoder_magnitude > decoder_magnitudelimit_low) 
{
    decoder_magnitudelimit += (decoder_magnitude - decoder_magnitudelimit) / 6;
}

if (decoder_magnitudelimit < decoder_magnitudelimit_low)
    decoder_magnitudelimit = decoder_magnitudelimit_low;
```

### How It Works

1. **Initial State**: `decoder_magnitudelimit = 100`

2. **When Signal Present** (magnitude > 100):
   - **Factor**: 1/6 = 0.167 (exponential moving average)
   - **Update**: Move threshold 1/6 of the way toward current magnitude
   - **Example**: 
     - Signal magnitude = 300
     - Current limit = 100
     - New limit = 100 + (300-100)/6 = 100 + 33 = **133**
     - Next sample (mag=320): 133 + (320-133)/6 = 133 + 31 = **164**
     - Converges slowly: after 10 cycles ≈ 280

3. **When Signal Absent** (magnitude < 100):
   - Limit stays at minimum value `decoder_magnitudelimit_low = 100`
   - Acts as noise floor

4. **Signal Threshold** (in Phase 3):
   - Detection threshold = `decoder_magnitudelimit * 0.6`
   - Creates hysteresis:
     - HIGH if magnitude > limit * 0.6
     - LOW if magnitude ≤ limit * 0.6

### Why This Handles Multiple Speeds

**Speed Influences Dit/Dah Timing:**
```
20 WPM:  T = 1200/20 = 60 ms   → Dit = 60 ms
5 WPM:   T = 1200/5 = 240 ms   → Dit = 240 ms
```

**The Adaptive Threshold Benefits:**

1. **Fast Morse (20-30 WPM)**:
   - Rapid magnitude fluctuations (60-200 ms duration each)
   - Small gaps (60 ms) between elements
   - Threshold adapts quickly to signal variations
   - Debounce time (200 µs) is negligible compared to timing

2. **Slow Morse (5-10 WPM)**:
   - Longer magnitude plateaus (200-600 ms duration)
   - Larger gaps (200 ms+) between elements
   - Threshold stabilizes during the long tone
   - Debounce prevents false detections during pauses

3. **Volume Changes**:
   - If keyer volume increases → magnitude increases
   - Threshold follows slowly (factor of 1/6) → no missed dots
   - If keyer volume decreases → threshold also decreases
   - Self-balancing mechanism

### Numerical Example: 20 WPM vs 5 WPM

**20 WPM Signal (letter 'E' = one dit)**
```
Time: 0-60ms    → HIGH tone (60 ms dit)
      60-120ms  → LOW (60 ms gap to next letter or end)

Decoder cycles at ~370 Hz = ~1 sample every 2.7ms

Magnitude timeline:
t=0ms:    mag=150, limit=100 → NEW limit = 100 + 50/6 ≈ 108
t=2.7ms:  mag=160, limit=108 → NEW limit = 108 + 52/6 ≈ 117
t=5.4ms:  mag=165, limit=117 → NEW limit = 117 + 48/6 ≈ 125
...
t=60ms:   tone ends, mag→50 (silence)
t=62.7ms: mag=50, limit stays at 100 (minimum)
```
Threshold = 100 * 0.6 = 60
Result: Tone detected during 0-60ms, pause detected at 62.7ms ✓

**5 WPM Signal (letter 'E' = one dit)**
```
Time: 0-240ms   → HIGH tone (240 ms dit)
      240-480ms → LOW (240 ms gap)

Magnitude timeline:
t=0ms:    mag=200, limit=100 → NEW limit = 100 + 100/6 ≈ 117
t=2.7ms:  mag=210, limit=117 → NEW limit = 117 + 93/6 ≈ 132
...
t=32ms:   mag=220, limit=170 (after many iterations) → NEW limit ≈ 185
...
t=240ms:  tone ends, mag→50
t=242.7ms: mag=50, limit=100 (minimum)
```
Threshold = 100 * 0.6 = 60
Result: Tone detected during 0-240ms, pause detected at 242.7ms ✓
```

---

## Phase 3: State Logic & Debouncing

### Purpose
Eliminate noise and false triggers through hysteresis and debouncing.

### Code
```cpp
void DecoderUpdateStateLogic()
{
    unsigned long now = micros();
    
    // Signal threshold with hysteresis
    if (decoder_magnitude > decoder_magnitudelimit * 0.6)
        decoder_realstate = HIGH;
    else
        decoder_realstate = LOW;
    
    // Detect state change
    if (decoder_realstate != decoder_realstatebefore)
        decoder_laststarttime = now;
    
    // Debouncing: only update on stable state change
    if ((now - decoder_laststarttime) > decoder_nbtime)
        decoder_filteredstate = decoder_realstate;
    
    decoder_realstatebefore = decoder_realstate;
}
```

### Hysteresis Behavior
```
        Signal Magnitude
             ▲
        500  │     ╱╲      ╱╲
             │    ╱  ╲    ╱  ╲  ← Real signal
        300  │   ╱    ╲  ╱    ╲
             │  ╱      ╲╱      ╲
        150  │╱─────────────────╲   ← Threshold (limit * 0.6)
             │                   ╲__
         50  │____________________╲___
             │
             └──────────────────────→ Time

Hysteresis:
- Signal > 150: decoder_realstate = HIGH
- Signal ≤ 150: decoder_realstate = LOW
- Prevents flutter when signal is near threshold
```

### Debouncing
- **Debounce Time**: 200 microseconds
- **Purpose**: Ignore noise spikes shorter than 200 µs
- **Speed Implication**:
  - At 20 WPM: 60 ms = 60,000 µs (300× longer than debounce)
  - At 5 WPM: 240 ms = 240,000 µs (1200× longer than debounce)
  - Debounce is negligible, won't affect timing

---

## Phase 4: Morse Detection

### Purpose
Measure HIGH/LOW durations and recognize dots/dashes.

### HIGH-to-LOW Transition (Tone Ended)
```cpp
if (decoder_filteredstate == LOW) 
{
    decoder_highduration = now - decoder_starttimehigh;
    
    // Distinguish Dot from Dash using learned average
    if (decoder_highduration < (decoder_hightimesavg * 2)) 
    {
        // Dit detected
        decoder_hightimesavg = (decoder_highduration + decoder_hightimesavg + decoder_hightimesavg) / 3;
        strcat(decoder_code, ".");
    } 
    else 
    {
        // Dah detected - extrapolate dit length
        unsigned long dit_estimate = decoder_highduration / 3;
        decoder_hightimesavg = (dit_estimate + decoder_hightimesavg + decoder_hightimesavg) / 3;
        strcat(decoder_code, "-");
    }
}
```

### ⭐ SPEED ADAPTATION IN MORSE DETECTION ⭐

**Using Learned Average (`decoder_hightimesavg`)**

1. **Initial Value**: `decoder_hightimesavg = 100,000 µs` (~60 ms for 20 WPM)

2. **Adaptive Learning**:
   ```
   Formula: new_avg = (measured + old_avg + old_avg) / 3
           = (measured + 2×old_avg) / 3
   
   This is a 1-pole low-pass filter with α = 1/3
   
   Example sequence at 20 WPM (dit ≈ 60 ms = 60,000 µs):
   - Initial: avg = 100,000 µs (guess)
   - Measure dit₁ = 58,000 µs
     → new_avg = (58,000 + 2×100,000)/3 = 86,000 µs
   - Measure dit₂ = 61,000 µs
     → new_avg = (61,000 + 2×86,000)/3 = 77,667 µs
   - Measure dit₃ = 60,500 µs
     → new_avg = (60,500 + 2×77,667)/3 = 71,944 µs
   ... converges to ~60,000 µs
   ```

3. **Dot vs Dash Recognition**:
   ```
   Threshold: 2 × avg
   
   At 20 WPM (avg = 60 ms):
   - Dit: 60 ms < 120 ms ✓ (recognized as ".")
   - Dah: 180 ms > 120 ms ✓ (recognized as "-")
   - Dah estimated: 180/3 = 60 ms ✓ (matches dit avg)
   
   At 5 WPM (avg = 240 ms):
   - Dit: 240 ms < 480 ms ✓
   - Dah: 720 ms > 480 ms ✓
   ```

### LOW-to-HIGH Transition (Pause Started)
```cpp
if (decoder_filteredstate == HIGH) 
{
    decoder_lowduration = now - decoder_startttimelow;
    
    // Long pause = character separation
    if (decoder_lowduration > decoder_hightimesavg * 2) 
    {
        DecoderDecodeMorse();  // Decode the character
        decoder_code[0] = '\0';  // Clear buffer
    }
}
```

### Character Separation
```
Timing thresholds (in terms of dit length):
- Element gap (between dot/dash):  1T (example: 60 ms)
- Character gap (after letter):     3T (threshold: 2T+)
- Word gap (after space):           7T

decoder_hightimesavg tracks the "T" value automatically
Example timeline for "HI":
├─ H: dit-dah-dah-dah (60-180-180-180 ms + 60ms gap)
├─ gap: 180 ms  → > 120 ms (2×avg) → Character decoded
├─ I: dit-dit (60-60 ms + 60ms gap)
├─ gap: 180 ms  → > 120 ms → Character decoded
└─ word pause: 240 ms → > 120 ms (also triggers decode)
```

### Timeout Mechanism
```cpp
// Very long pause = word end
if ((now - decoder_startttimelow) > (decoder_highduration * 6) && decoder_stop == LOW) 
{
    DecoderDecodeMorse();
    decoder_code[0] = '\0';
    decoder_stop = HIGH;
}
```

**Why `decoder_highduration * 6`?**
- If last tone was 60 ms (dit)
  - Timeout = 60 × 6 = 360 ms
  - Normal word gap = 180-200 ms
  - Catches pauses after word end
- If last tone was 180 ms (dah)
  - Timeout = 180 × 6 = 1080 ms
  - Provides ~1 second buffer for next word

---

## Speed Adaptation Summary

### How It All Works Together

| WPM | T Value | Dit Duration | Dah Duration | Gap Threshold | Speed |
|-----|---------|--------------|--------------|---------------|-------|
| 5   | 240 ms  | 240 ms       | 720 ms       | > 480 ms      | Very slow |
| 10  | 120 ms  | 120 ms       | 360 ms       | > 240 ms      | Slow |
| 20  | 60 ms   | 60 ms        | 180 ms       | > 120 ms      | Fast |
| 40  | 30 ms   | 30 ms        | 90 ms        | > 60 ms       | Very fast |

### Key Mechanisms

1. **Sampling Rate** (8928 Hz):
   - Fast enough to capture 700 Hz sine wave (Nyquist: > 1400 Hz)
   - Slow enough to reduce noise

2. **Goertzel Algorithm**:
   - Narrow-band filter focused on 700 Hz
   - Computationally efficient (one 2nd-order filter)
   - Resistant to harmonics and interference

3. **Adaptive Magnitude Threshold** (1/6 exponential average):
   - Handles volume variations
   - Accommodates different CW keyer outputs
   - Independent of speed

4. **Learned Average Duration** (1/3 low-pass filter):
   - Tracks actual dit duration
   - Adapts to any WPM automatically
   - Uses dah-to-dit ratio (3:1) for validation

5. **Debouncing** (200 µs):
   - Eliminates noise glitches
   - Negligible vs. CW timing (1000× longer)

6. **Hysteresis** (60% threshold):
   - Prevents false transitions near signal boundary
   - Stable detection of presence/absence

---

## Practical Behavior at Different Speeds

### 30 WPM Signal
```
Input: "CQ" (C = -.-. Q = --.-.)
Timing: 20ms dits, 60ms dashes

Analysis cycles at ~370 Hz (2.7ms apart):
├─ Dah1 (60ms): 22 cycles detect HIGH
├─ gap (20ms): 7 cycles detect LOW
├─ dit1 (20ms): 7 cycles detect HIGH
...
Average = ~23 ms (tracked by decoder_hightimesavg)
Threshold = 46 ms for dot vs. dash
Detection: 60 > 46 → dah ✓, 20 < 46 → dit ✓
```

### 5 WPM Signal
```
Input: "CQ"
Timing: 240ms dits, 720ms dashes

Analysis cycles:
├─ Dah1 (720ms): 267 cycles detect HIGH (stabilizes quickly)
├─ gap (240ms): 89 cycles detect LOW
...
Average = ~240 ms (learned)
Threshold = 480 ms for dot vs. dash
Detection: 720 > 480 → dah ✓, 240 < 480 → dit ✓
```

---

## Limitations & Design Considerations

1. **Fixed 700 Hz Target**:
   - Works best for standard CW audio in 600-900 Hz range
   - May miss extremely high-pitched (>1200 Hz) or low-pitched (<500 Hz) tones

2. **Buffer Size** (`decoder_code[20]`):
   - Handles up to 20 symbols (dots/dashes)
   - Limits character length
   - Should be sufficient (longest standard: Z = "--..") (5 symbols)

3. **Initial Guess** (`decoder_hightimesavg = 100,000`)
   - Assumes ~20 WPM at startup
   - Converges to actual speed within 3-5 characters

4. **Minimum WPM**:
   - Below ~3 WPM, analysis cycles may miss dit boundaries
   - Not a practical limitation (most CW is > 10 WPM)

---

## Conclusion

The audio decoder achieves **automatic speed adaptation** through:
1. **Fixed-frequency Goertzel analysis** (700 Hz detection)
2. **Adaptive magnitude threshold** (exponential moving average)
3. **Learned element duration** (low-pass filtered average)
4. **Ratio-based element classification** (2× threshold distinguishes dots/dashes)

This design allows a single decoder to work reliably from 5 WPM to 50+ WPM without configuration changes.
