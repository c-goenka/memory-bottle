# Memory Bottle - Implementation Guide

A multi-device system for recording audio and color, then playing them back through WiFi transfer.

## System Overview

The Memory Bottle system consists of three main components:

1. **Nano ESP32 (Recording)** - Inside the bottle, records audio/color and stores to SD card
2. **Python Flask Server** - Runs on laptop, receives files via WiFi and coordinates playback
3. **UNO R4 Minima (Playback)** - Connected to laptop, plays audio and displays color

## File Structure

```
memory-bottle/
├── bottle-recorder/
│   └── bottle-recorder.ino          # Nano ESP32 recording system
├── bottle-playback/
│   └── bottle-playback.ino          # UNO R4 playback system
├── bottle-server.py                 # Flask server
├── requirements.txt                 # Python dependencies
└── README.md                        # This file
```

## Hardware Setup

### Nano ESP32 (Bottle - Recording System)

**Pin Connections:**
- A0 → MAX9814 Microphone
- A1 → 10kΩ Potentiometer (sensor selector)
- D2 → Push Button (cap sensor, with pullup)
- D3 → SW-520D Tilt Sensor (with pullup)
- D6 → NeoPixel LED Strip (15 LEDs arranged in a ring inside the bottle)
- D10 → SD Card CS pin
- SDA/SCL → TCS34725 Color Sensor (with level converter)

**Required Libraries:**
- WiFi (built-in ESP32)
- HTTPClient (built-in ESP32)
- SD (built-in)
- SPI (built-in)
- Adafruit_NeoPixel
- Adafruit_TCS34725

### UNO R4 Minima (Playback System)

**Pin Connections:**
- D6 → NeoPixel LED Strip (arranged in a circle for output display)
- D9 → MAX98357 BCLK (I2S Bit Clock)
- D10 → MAX98357 LRC (I2S Word Select)
- D11 → MAX98357 DIN (I2S Data)

**Required Libraries:**
- Adafruit_NeoPixel
- I2S library (for audio output)

### Python Server (Laptop)

**Requirements:**
```bash
pip install flask pyserial
```

## Configuration Steps

### 1. Configure Nano ESP32

Open [bottle-recorder.ino](bottle-recorder/bottle-recorder.ino) and update:

```cpp
// Line 32-34: Update WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverURL = "http://LAPTOP_IP:5000/upload";
```

**To find your laptop IP:**
- **macOS/Linux**: Run `ifconfig` and look for your WiFi adapter's inet address
- **Windows**: Run `ipconfig` and look for IPv4 Address

### 2. Upload Arduino Sketches

1. Open Arduino IDE
2. Select board: **Arduino Nano ESP32** for recording system
3. Upload [bottle-recorder.ino](bottle-recorder/bottle-recorder.ino)
4. Select board: **Arduino UNO R4 Minima** for playback system
5. Upload [bottle-playback.ino](bottle-playback/bottle-playback.ino)

### 3. Start Python Server

```bash
cd memory-bottle
python bottle-server.py
```

The server will:
- Auto-detect the UNO R4 serial port
- Display your local IP address
- Listen on port 5000 for uploads

**Note:** Copy the server URL shown in the console and update it in the ESP32 sketch if different.

## How to Use

### Recording Process

1. **Select Sensor** - Rotate potentiometer:
   - LED turns **blue** = Microphone selected
   - LED turns **red** = Color sensor selected

2. **Start Recording** - Open the bottle cap:
   - LED ring gradually increases in brightness over 15 seconds
   - For audio: records for up to 15 seconds
   - For color: captures instantly

3. **Stop Recording** - Close the bottle cap:
   - If first recording: LED shows **yellow pulse** (INCOMPLETE)
   - If second recording: LED shows **solid green** (READY)

4. **Record Second Sensor** - Rotate potentiometer and repeat steps 1-3

### Playback Process

1. **Ready to Pour** - When LED is solid green (2 recordings complete)
2. **Pour Gesture** - Open cap AND tilt the bottle:
   - LED pulses **cyan** during WiFi transfer
   - Files upload to laptop server
   - UNO R4 plays audio through speaker
   - UNO R4 displays color on LED strip

3. **After Playback** - Memory clears, LED returns to dim white, ready for new recordings

## LED Status Indicators

| LED Pattern | State | Meaning |
|------------|-------|---------|
| Dim white | IDLE | Ready to start, no recordings |
| Blue | SELECTING | Microphone selected |
| Red | SELECTING | Color sensor selected |
| Brightening blue/red ring | RECORDING | Recording in progress (increases brightness over 15s) |
| Yellow pulse | INCOMPLETE | Need one more recording |
| Solid green | READY | Both recordings complete, ready to pour |
| Pulsing cyan | TRANSFERRING | Uploading to server |
| Red blink | ERROR | SD card or WiFi failure |

## State Machine

```
IDLE → SELECTING (rotate potentiometer)
     → RECORDING (open cap)

SELECTING → RECORDING (open cap)
          → IDLE (timeout)

RECORDING → INCOMPLETE (close cap, 1 recording)
          → READY (close cap, 2 recordings)

INCOMPLETE → SELECTING (rotate potentiometer)
           → RECORDING (open cap)

READY → TRANSFERRING (open cap + tilt)

TRANSFERRING → IDLE (success)
             → READY (failure, can retry)
             → ERROR (3 failures)
```

## Troubleshooting

### ESP32 Won't Connect to WiFi
- Check SSID and password in the sketch
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Move closer to WiFi router

### SD Card Error
- Ensure SD card is formatted as FAT32
- Check all SPI wiring connections
- Try a different SD card (some cards are incompatible)

### Server Can't Find Arduino
- Check USB connection to UNO R4
- On macOS: Port usually `/dev/cu.usbmodem*`
- On Windows: Port usually `COM3` or higher
- On Linux: Port usually `/dev/ttyACM0`
- Manually set `SERIAL_PORT` in the Python script if auto-detection fails

### Transfer Fails
- Verify laptop IP address hasn't changed
- Check firewall isn't blocking port 5000
- Ensure Python server is running
- Try pinging the server from another device

### No Audio Output
- Verify MAX98357 wiring (BCLK, LRC, DIN)
- Check speaker connection (3W 4Ω)
- Ensure UNO R4 is receiving serial data
- Check Serial Monitor for "REQ:AUDIO" messages

### Color Sensor Not Working
- Verify I2C wiring (SDA/SCL)
- Ensure level converter is properly connected (TCS34725 is 3.3V)
- Check sensor has clear view of object
- Test with brightly colored objects first

## Technical Details

### Audio Format
- Sample Rate: 16kHz
- Bit Depth: 16-bit PCM
- Channels: Mono
- Max Duration: 15 seconds
- File Format: WAV

### Color Format
- Plain text file: `R,G,B`
- Values: 0-255 per channel
- Example: `255,128,0` (orange)

### WiFi Transfer
- Protocol: HTTP POST multipart/form-data
- Timeout: 30 seconds
- Retry: Up to 3 attempts

### Timing Parameters
- Button debounce: 50ms
- Tilt stabilization: 100ms
- Potentiometer read: 100ms
- Recording duration: 15 seconds max
- LED brightness ramp: 0-255 over 15 seconds during recording

## System Behavior Rules

### Valid Actions
- Open cap when idle/incomplete → starts recording
- Close cap during recording → saves recording
- Rotate potentiometer when cap closed → changes sensor
- Open + tilt when ready (2 recordings) → transfer and playback

### Ignored Actions
- Tilting during recording (recording continues)
- Opening + tilting with < 2 recordings (shows yellow warning)
- Rotating potentiometer during recording
- Recording same sensor twice (overwrites previous)

## Error Handling

### SD Card Failure
- Shows 5 rapid red blinks
- Disables recording
- Requires reset to recover

### WiFi Failure (3 attempts)
- Shows 3 red blinks per failure
- After 3 failures: enters ERROR state
- Stays in READY if < 3 failures (can retry)

### Incomplete Pour (< 2 recordings)
- Shows 3 yellow blinks
- Stays in current state
- User must complete recordings first

## Development Notes

### Potentiometer Calibration
The ESP32 has a 12-bit ADC (0-4095). The threshold is set at 2048:
- 0-2047 = Microphone
- 2048-4095 = Color sensor

Adjust threshold in `readSensorSelection()` if needed.

### Audio Sampling
Real-time audio recording at 16kHz is timing-critical. The current implementation uses:
```cpp
unsigned long sampleInterval = 1000000 / SAMPLE_RATE;  // 62.5 microseconds
```

### I2S Audio Output
The UNO R4 playback sketch uses I2S for audio. You'll need an I2S library compatible with the R4's Renesas RA4M1 processor. The MAX98357 is an I2S DAC that outputs to a speaker.

## Future Enhancements

Possible improvements:
- Add audio compression to store more recordings
- Implement audio effects (reverb, echo)
- Add visual feedback during playback (VU meter)
- Support multiple bottles with unique IDs
- Add battery power option for portability
- Implement OTA updates for ESP32

## License

This project is provided as-is for educational and personal use.

## Support

For issues or questions:
1. Check the Serial Monitor output for error messages
2. Verify all hardware connections
3. Ensure all required libraries are installed
4. Test each component separately before integration
