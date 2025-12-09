# Memory Bottle

Audio and color recording system with WiFi playback.

## Quick Start

```bash
git clone https://github.com/YOUR_USERNAME/memory-bottle.git
cd memory-bottle
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -r requirements.txt
```

Then configure WiFi and upload sketches (see Setup below).

## System Components

1. **Nano ESP32** (in bottle) - Records audio/color to SD card
2. **Python Server** (laptop) - Receives files via WiFi, plays audio
3. **UNO R4 Minima** (laptop) - Displays color on LED ring

## Files

```
memory-bottle/
├── bottle-recorder/bottle-recorder.ino    # Nano ESP32 main
├── bottle-playback/bottle-playback.ino    # UNO R4 main
├── bottle-server.py                       # Flask server
├── debug-sketches/                        # Hardware testing
│   ├── debug-nano-esp32.ino              #   Test ESP32 sensors
│   └── debug-uno-r4.ino                  #   Test UNO R4
├── debug-system.py                        # System testing
├── DEBUG-GUIDE.md                         # Full debug reference
├── requirements.txt
└── README.md
```

## Hardware Wiring

### Nano ESP32 (Recorder)
- **A0** → MAX9814 Microphone
- **A1** → 10kΩ Potentiometer (sensor selector)
- **D2** → Push Button (cap sensor, INPUT_PULLUP)
- **D3** → SW-520D Tilt Sensor (INPUT_PULLUP)
- **D6** → NeoPixel LED Ring (15 LEDs)
- **D10** → SD Card CS
- **SDA/SCL** → TCS34725 Color Sensor (with 3.3V level shifter)

### UNO R4 Minima (Color Display)
- **D6** → NeoPixel LED Ring (15 LEDs)
- **USB** → Serial connection to laptop

## LED Status (State Debugging)

| LED Color/Pattern | State | Meaning |
|-------------------|-------|---------|
| Dim white | IDLE | No recordings, ready to start |
| **Blue** solid | SELECTING | Microphone selected |
| **Red** solid | SELECTING | Color sensor selected |
| **Blue** brightening | RECORDING | Recording audio (0-255 over 15s) |
| **Red** brightening | RECORDING | Sampling color |
| **Yellow** pulsing | INCOMPLETE | 1 of 2 recordings done |
| **Green** solid | READY | 2 recordings done, ready to pour |
| **Cyan** pulsing | TRANSFERRING | Uploading to WiFi |
| **Red** blinking | ERROR | SD/WiFi failure |

## Setup

### 1. Install Arduino Libraries

Via Arduino IDE Library Manager:
- Adafruit_NeoPixel
- Adafruit_TCS34725

### 2. Configure WiFi

Edit `bottle-recorder/bottle-recorder.ino` lines 32-34:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverURL = "http://YOUR_LAPTOP_IP:5000/upload";
```

**Find your laptop IP:**
- macOS/Linux: `ifconfig | grep inet`
- Windows: `ipconfig`

### 3. Upload Sketches

1. Upload `bottle-recorder.ino` to **Nano ESP32**
2. Upload `bottle-playback.ino` to **UNO R4 Minima**

### 4. Start Server

```bash
source venv/bin/activate  # On Windows: venv\Scripts\activate
python bottle-server.py
```

Server displays local IP - update ESP32 sketch if needed.

## Usage

### Recording

1. **Rotate potentiometer** → Blue (mic) or Red (color sensor)
2. **Open cap** → Recording starts, LED brightness increases
3. **Close cap** → Recording saves
   - Yellow pulse = need other sensor
   - Green solid = ready to pour
4. **Repeat** for second sensor

### Playback

**Open cap + tilt bottle** (when green) → Transfers via WiFi, plays audio on laptop, displays color on UNO R4

After playback, memory clears and returns to IDLE (dim white).

## State Machine

```
IDLE (dim white)
  ↓ rotate pot
SELECTING (blue/red)
  ↓ open cap
RECORDING (brightening)
  ↓ close cap
INCOMPLETE (yellow pulse) or READY (green)
  ↓ open + tilt (if ready)
TRANSFERRING (cyan pulse)
  ↓ success
IDLE (memory cleared)
```

## Error States

**3 yellow blinks** - Tried to pour with < 2 recordings
**3 red blinks** - WiFi transfer failed (can retry)
**5 red blinks** - SD card failure (requires reset)
**Red blinking** - Persistent ERROR state

## Technical Specs

**Audio:** 16kHz, 16-bit PCM, mono WAV (15s max) - plays through laptop speakers via pygame
**Color:** RGB text file, format `R,G,B` (0-255) - displays on UNO R4 LED ring
**WiFi:** HTTP POST multipart/form-data, 30s timeout
**Timing:** 50ms debounce, 100ms tilt stabilization

## Debugging

**Quick Start:** Run `python debug-system.py` to test everything

**Test Mode:** See [TEST-MODE-GUIDE.md](TEST-MODE-GUIDE.md) to test the full system without sensors (using Serial commands)

**Full Guide:** See [DEBUG-GUIDE.md](DEBUG-GUIDE.md) for detailed troubleshooting

### Debug Scripts

**1. System Check:** `python debug-system.py`
- Tests Python, network, serial, files, WiFi config

**2. ESP32 Hardware:** Upload `debug-sketches/debug-nano-esp32.ino` (Serial: 115200)
- Tests all sensors: LEDs, button, tilt, pot, mic, color, SD, WiFi

**3. UNO R4 Hardware:** Upload `debug-sketches/debug-uno-r4.ino` (Serial: 115200)
- Tests LEDs, serial, color display protocol

### Common Issues

**WiFi won't connect** - Use 2.4GHz network, check credentials
**SD card error** - Format as FAT32, check SPI wiring
**No audio playback** - Check laptop speakers, verify pygame installed (`pip install pygame`)
**Server can't find Arduino** - Check USB connection, set `SERIAL_PORT` manually in script
**Color sensor fails** - Verify I2C + level shifter, test with bright colors
**LEDs not working** - Check power, verify D6 connection, test with debug sketch

## Sensor Selection

Potentiometer reads ESP32's 12-bit ADC (0-4095):
- **0-2047** = Microphone (blue LED)
- **2048-4095** = Color sensor (red LED)

## Valid Actions

- Open cap (IDLE/INCOMPLETE) → start recording
- Close cap (RECORDING) → save recording
- Rotate pot (cap closed) → change sensor
- Open + tilt (READY, 2 recordings) → transfer/play

## Ignored Actions

- Tilt during recording
- Open + tilt with < 2 recordings (shows warning)
- Rotate pot during recording
- Record same sensor twice (overwrites previous)
