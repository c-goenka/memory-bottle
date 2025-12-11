# Memory Bottle

Audio and color recording system with WiFi playback.

## Quick Start

```bash
git clone https://github.com/YOUR_USERNAME/memory-bottle.git
cd memory-bottle
python -m venv venv
source venv/bin/activate  # Windows: venv\Scripts\activate
pip install -r requirements.txt
```

Configure WiFi credentials and upload sketches (see Setup below).

## System Components

1. **ESP32 Nano** (in bottle) - Records audio/color to SD card
2. **Flask Server** (laptop) - Receives files via WiFi, plays audio
3. **UNO R4 Minima** (laptop) - Displays color on LED ring

## Hardware Wiring

### ESP32 Nano (Recorder)
- **A0** → MAX9814 Microphone
- **A2** → 10kΩ Potentiometer (sensor selector)
- **D7** → Capacitive Touch Button (cap sensor, INPUT_PULLUP)
- **D2** → SW-520D Tilt Sensor (INPUT_PULLUP, inverted)
- **D5** → NeoPixel LED Ring (7 LEDs)
- **D10** → SD Card CS
- **SDA/SCL** → TCS34725 Color Sensor (with 3.3V level shifter)

### UNO R4 Minima (Display)
- **D5** → NeoPixel LED Ring (85 LEDs)
- **USB** → Serial to laptop (115200 baud)

## LED Status

| Color/Pattern | State | Meaning |
|---------------|-------|---------|
| Dim white | IDLE | Ready to record |
| Blue solid | SELECTING | Microphone selected |
| Red solid | SELECTING | Color sensor selected |
| Blue brightening | RECORDING | Recording audio (15s) |
| Red brightening | RECORDING | Recording color (8s) |
| Yellow pulsing | INCOMPLETE | 1 of 2 recordings complete |
| Green solid | READY | Both recordings done, ready to pour |
| Cyan pulsing | TRANSFERRING | Uploading via WiFi |
| Red blinking | ERROR | Hardware failure |

## Setup

### 1. Install Arduino Libraries

Via Arduino IDE Library Manager:
- Adafruit_NeoPixel
- Adafruit_TCS34725

### 2. Configure WiFi

Edit [bottle-recorder.ino:40-42](bottle-recorder/bottle-recorder.ino#L40-L42):

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverURL = "http://YOUR_LAPTOP_IP:8080/upload";
```

**Find your laptop IP:**
- macOS/Linux: `ifconfig | grep inet`
- Windows: `ipconfig`

### 3. Upload Sketches

1. Upload `bottle-recorder.ino` to **ESP32 Nano**
2. Upload `bottle-playback.ino` to **UNO R4 Minima**

### 4. Start Server

```bash
source venv/bin/activate  # Windows: venv\Scripts\activate
python bottle-server.py
```

Server runs on port 8080 and auto-detects the UNO R4.

## Usage

### Recording

1. **Rotate potentiometer** → LED turns blue (mic) or red (color sensor)
2. **Open cap** → Recording starts (forced duration: 15s audio, 8s color)
3. Recording completes automatically:
   - Yellow pulse = need to record with other sensor
   - Green solid = ready to pour
4. **Repeat** to record with the other sensor

### Playback

**Open cap + tilt bottle** (when LED is green) → WiFi transfer, audio plays on laptop, color displays on LED ring

After playback, memory clears and returns to IDLE.

## State Machine

```
IDLE (dim white)
  ↓ rotate pot
SELECTING (blue/red)
  ↓ open cap
RECORDING (brightening, forced duration)
  ↓ auto-complete
INCOMPLETE (yellow) or READY (green)
  ↓ open + tilt (when ready)
TRANSFERRING (cyan)
  ↓ success
IDLE (cleared)
```

## Error Handling

**3 yellow blinks** - Attempted pour with incomplete recordings
**3 red blinks** - WiFi transfer failed (auto-retry, 3 max attempts)
**5 red blinks** - Hardware initialization failure (SD/color sensor)
**Red blinking** - Persistent error state (requires power cycle)

## Technical Specs

**Audio:** 16kHz, 16-bit PCM mono WAV, 15 seconds (forced duration)
**Color:** RGB format `R,G,B` (0-255), captured after 8 second recording period
**WiFi:** HTTP POST with `X-Color-Data` header, 30s timeout
**Timing:** 100ms debounce, 100ms tilt stabilization, 150 ADC threshold

## Configuration Constants

All timing and threshold values are defined as constants at the top of each file:

**bottle-recorder.ino:**
- `RECORDING_DURATION` (15000ms)
- `COLOR_RECORDING_DURATION` (8000ms)
- `POT_THRESHOLD` (150)
- `POT_MIDPOINT` (2048)
- `SELECTING_TIMEOUT` (5000ms)

**bottle-server.py:**
- `SAMPLE_RATE` (16000)
- `COLOR_REQUEST_TIMEOUT` (5s)
- `DEFAULT_COLOR` ("128,128,128")

**bottle-playback.ino:**
- `WAVE_SPEED` (50ms)
- `WAVE_LENGTH` (15 LEDs)
- `AUTO_RESET_DELAY` (60s)

## Debugging

### Quick Test
```bash
python debug-system.py
```

### Test Mode
Set `TEST_MODE` to `true` in [bottle-recorder.ino:37](bottle-recorder/bottle-recorder.ino#L37) to simulate sensors via Serial commands.

### Debug Sketches
- `debug-sketches/debug-nano-esp32.ino` - Test ESP32 sensors
- `debug-sketches/debug-uno-r4.ino` - Test UNO R4 LEDs

### Common Issues

**WiFi won't connect** - Use 2.4GHz network, verify credentials
**SD card error** - Format as FAT32, check wiring
**No audio** - Check laptop volume, verify pygame installed
**Arduino not found** - Check USB connection, verify port
**Color sensor fails** - Check I2C connections and level shifter

## Sensor Selection

Potentiometer uses ESP32's 12-bit ADC (0-4095):
- **< 2048** = Microphone (blue LED)
- **≥ 2048** = Color sensor (red LED)

Threshold of 150 ADC units prevents false triggers from noise.

## Project Structure

```
memory-bottle/
├── bottle-recorder/
│   └── bottle-recorder.ino          # ESP32 firmware (~970 lines)
├── bottle-playback/
│   └── bottle-playback.ino          # UNO R4 firmware (~380 lines)
├── bottle-server.py                 # Flask server (~218 lines)
├── debug-sketches/                  # Hardware testing tools
├── requirements.txt
└── README.md
```
