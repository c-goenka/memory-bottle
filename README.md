# Memory Bottle

Audio and color recording system with WiFi playback.

## System Components

1. **Nano ESP32** (in bottle) - Records audio/color to SD card
2. **Python Server** (laptop) - Receives files via WiFi
3. **UNO R4 Minima** (laptop) - Plays audio and displays color

## Files

```
memory-bottle/
├── bottle-recorder/bottle-recorder.ino    # Nano ESP32
├── bottle-playback/bottle-playback.ino    # UNO R4
├── bottle-server.py                       # Flask server
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

### UNO R4 (Playback)
- **D6** → NeoPixel LED Ring
- **D9** → MAX98357 BCLK
- **D10** → MAX98357 LRC
- **D11** → MAX98357 DIN

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

### 1. Install Dependencies

**Python:**
```bash
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -r requirements.txt
```

**Arduino libraries** (via Library Manager):
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

### 4. Run Server

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

**Open cap + tilt bottle** (when green) → Transfers via WiFi, plays on UNO R4

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

**Audio:** 16kHz, 16-bit PCM, mono WAV (15s max)
**Color:** RGB text file, format `R,G,B` (0-255)
**WiFi:** HTTP POST multipart/form-data, 30s timeout
**Timing:** 50ms debounce, 100ms tilt stabilization

## Troubleshooting

**WiFi won't connect** - Use 2.4GHz network, check credentials
**SD card error** - Format as FAT32, check SPI wiring
**No audio playback** - Verify I2S wiring, check Serial Monitor
**Server can't find Arduino** - Check USB connection, set `SERIAL_PORT` manually in script
**Color sensor fails** - Verify I2C + level shifter, test with bright colors

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
