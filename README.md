# CWKeyer

A simple CWKeyer for CW-Paddles based on a ESP8266 with WebInterface.  

SSID:     CWKeyer  
PW:       123456789  
Address:  192.168.4.2  


# Features

- Adjustable speed in WPM and Farnsworth
- Internal buzzer for sound output ( can be deactivated in software )
- LineOut
- 2 Memories for 128 char's each
- WebInterface for setting the memory and training data
- CW Audio decoder ( Under 'Docs' you can find a 'How it works' )

# Source layout

The Arduino sketch is split into tabs by responsibility:

- `CWKeyer.ino` contains initialization and the main loop.
- `MenuController.ino` handles paddle input, menu actions, and rotary navigation.
- `AudioDecoder.ino` contains the Goertzel-based audio decoder.
- `MorsePlayback.ino` and `MorseCode.ino` handle Morse output and conversion.
- `MenuScreens.ino` contains OLED screens.
- `WebInterface.ino` contains web request handlers.
- `Settings.ino` and `Storage.ino` handle configuration and EEPROM access.

The tabs are compiled together as one Arduino sketch and share its existing state.

# Morse training

- **Listen Char** plays a hidden character for copy practice.
- **Copy Word/Call** plays a short CW abbreviation, word, or callsign and checks the complete response. If the selected character set is too narrow for those prompts, it uses a four-character group instead.
- **Give Random** and **Give A-Z** show a character to send.
- The web settings page lets you choose letters, digits, and supported punctuation for random and listening practice.
- Missed characters are weighted toward appearing again. The trainer shows session accuracy and skipped questions. Training starts at 10 WPM; turn the encoder in the trainer menu to set a saved start speed from 10 to 60 WPM in 2 WPM steps. After every 10 consecutive correct answers, training speed increases by 2 WPM up to 60 WPM. The regular keyer speed is restored when leaving the trainer.
- In a training question, a short mode-button press replays the prompt, encoder rotation skips it, and holding the mode button returns to the trainer menu.

# Images

![plot](https://github.com/StefanKDS/CWKeyer/blob/ESP8266/Pictures/Keyer.jpg)


# Circuit

![plot](https://github.com/StefanKDS/CWKeyer/blob/ESP8266/Circuit/Circuit.jpg?raw=true)
