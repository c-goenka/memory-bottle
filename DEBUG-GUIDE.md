# Debug Guide - Memory Bottle

Quick reference for debugging hardware and software issues.

## Debug Workflow

```
1. Run debug-system.py          → Test Python/server setup
2. Upload debug-nano-esp32.ino  → Test all ESP32 sensors
3. Upload debug-uno-r4.ino      → Test UNO R4 playback
4. Upload actual sketches       → Run full system
```

## Debug Tools

### 1. System Debug (Python)

**Run:** `python debug-system.py`

**Tests:**
- Python version and dependencies
- Network configuration and port availability
- Serial port detection
- Arduino connection
- File structure
- WiFi configuration in sketch
- Optional: Start test server

**Output:** Pass/fail for each component + summary

### 2. Nano ESP32 Debug (Arduino)

**Upload:** `debug-sketches/debug-nano-esp32.ino`
**Serial Monitor:** 115200 baud

**Tests:**
1. **LED Strip** - Color patterns, individual LEDs, brightness ramp
2. **Button (Cap Sensor)** - Open/close detection (HIGH/LOW)
3. **Tilt Sensor** - Upright/tilted detection (HIGH/LOW)
4. **Potentiometer** - ADC values 0-4095, threshold at 2048
5. **Microphone** - Audio level range, sensitivity check
6. **Color Sensor** - RGB detection, I2C communication
7. **SD Card** - Read/write/delete test files
8. **WiFi** - Connect to network, get IP address

**Menu:** Type number (1-8) to run specific test, 9 for all tests

### 3. UNO R4 Debug (Arduino)

**Upload:** `debug-sketches/debug-uno-r4.ino`
**Serial Monitor:** 115200 baud

**Tests:**
1. **LED Strip** - Color patterns, individual LEDs, fade effect
2. **Serial Communication** - Verify serial connection
3. **I2S Pins** - Toggle BCLK/LRC/DIN (use multimeter)
4. **Playback Protocol** - Simulate server commands
5. **All LED Patterns** - Rainbow, wipe, chase, pulse

**Custom Color:** Type `COLOR:255,0,0` for red (or any R,G,B values)

## LED State Reference

Use LEDs to identify current system state:

| Color | State | What to Check |
|-------|-------|---------------|
| Dim white | IDLE | Normal - ready to start |
| Blue solid | SELECTING | Pot selects mic |
| Red solid | SELECTING | Pot selects color |
| Blue brightening | RECORDING | Audio recording |
| Red brightening | RECORDING | Color sampling |
| Yellow pulse | INCOMPLETE | Need 2nd recording |
| Green solid | READY | Ready to pour |
| Cyan pulse | TRANSFERRING | WiFi active |
| Red blink | ERROR | Check Serial Monitor |

## Component Testing Checklist

### ESP32 - Before Assembly

- [ ] LED strip lights up (all colors)
- [ ] Button reads HIGH when not pressed
- [ ] Tilt sensor reads HIGH when upright
- [ ] Potentiometer ranges 0-4095
- [ ] Microphone shows range > 200
- [ ] Color sensor detects RGB
- [ ] SD card reads/writes
- [ ] WiFi connects and gets IP

### UNO R4 - Before Assembly

- [ ] LED strip lights up (all colors)
- [ ] Serial communication works
- [ ] I2S pins toggle (if testing with scope)
- [ ] Receives PLAY:START command
- [ ] Displays color from COLOR:R,G,B

### Server - Before Integration

- [ ] Python 3.7+ installed
- [ ] Flask and pyserial installed
- [ ] Port 5000 available
- [ ] Serial port detected
- [ ] Can connect to Arduino
- [ ] uploads/ directory writable

## Common Debug Patterns

### LEDs Don't Work

1. Run LED test in debug sketch
2. Check power supply (5V, sufficient current)
3. Verify D6 connection
4. Test with simple strip.fill(Color(255,0,0))
5. Try different brightness levels

### Button Stuck HIGH or LOW

1. Run button test in debug sketch
2. Check INPUT_PULLUP is set
3. Verify wiring to D2
4. Test continuity with multimeter
5. Try reversing button polarity

### Microphone Silent

1. Run microphone test in debug sketch
2. Check expected range (should be > 200)
3. Verify A0 connection
4. Test MAX9814 power (VCC/GND)
5. Check gain setting on MAX9814

### Color Sensor Not Found

1. Run color sensor test
2. Verify I2C wiring (SDA/SCL)
3. Check level shifter (TCS34725 needs 3.3V)
4. Test with i2c_scanner sketch
5. Verify TCS34725 address (0x29)

### SD Card Fails

1. Run SD card test
2. Format as FAT32
3. Try different card (some incompatible)
4. Check SPI wiring (MOSI/MISO/SCK/CS)
5. Verify CS on pin 10
6. Test with CardInfo example sketch

### WiFi Won't Connect

1. Run WiFi test in debug sketch
2. Confirm 2.4GHz network (not 5GHz)
3. Check SSID/password spelling
4. Move closer to router
5. Check router allows new devices
6. Verify ESP32 WiFi antenna

### Serial Port Not Found

1. Check USB cable (data, not charge-only)
2. Install USB drivers if needed
3. Try different USB port
4. Check device manager (Windows) or ls /dev (Mac/Linux)
5. Verify Arduino IDE can see port

### Server Can't Talk to Arduino

1. Check Arduino is connected
2. Verify correct port in script
3. Check baud rate (115200)
4. Test with debug sketch first
5. Look for Serial Monitor conflicts

## Debugging Commands

### Python Serial Test

```python
import serial
ser = serial.Serial('/dev/cu.usbmodem14101', 115200, timeout=1)
ser.write(b'PLAY:START\n')
print(ser.readline())
ser.close()
```

### Arduino I2C Scanner

```cpp
#include <Wire.h>
void setup() {
  Wire.begin();
  Serial.begin(115200);
  Serial.println("I2C Scanner");
}
void loop() {
  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found: 0x");
      Serial.println(i, HEX);
    }
  }
  delay(5000);
}
```

### Test WiFi Connection

```cpp
WiFi.begin("SSID", "PASSWORD");
while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
}
Serial.println(WiFi.localIP());
```

## Serial Monitor Output

### Normal Startup (ESP32)

```
SD card initialized
Color sensor initialized
WiFi connected
IP: 192.168.1.100
State: IDLE
```

### Normal Startup (UNO R4)

```
UNO R4 Minima Playback System Ready
Waiting for PLAY:START command...
```

### Error Examples

```
SD card initialization failed!  → Check card/wiring
Color sensor not found!         → Check I2C/level shifter
WiFi connection failed          → Check SSID/password
```

## Integration Testing

After all components pass individual tests:

1. **Upload main sketches**
2. **Start server:** `python bottle-server.py`
3. **Test recording:** Rotate pot, open cap, close cap
4. **Check Serial Monitor** for state changes
5. **Test playback:** Open + tilt when green
6. **Monitor server output** for file upload
7. **Verify UNO R4** receives and plays

## Quick Fixes

**Reset everything:**
1. Unplug both Arduinos
2. Stop Python server (Ctrl+C)
3. Wait 5 seconds
4. Start server first
5. Plug in UNO R4 (playback)
6. Plug in ESP32 (recorder)
7. Check Serial Monitors

**Clear SD card:**
1. Remove from ESP32
2. Delete audio.wav, color.dat, recordings.txt
3. Reinsert
4. Reset ESP32

**WiFi issues:**
1. Restart router
2. Forget network on ESP32 (WiFi.disconnect())
3. Check router firewall
4. Use static IP if needed

## Contact Serial Monitors

Watch real-time state changes:

**ESP32:** Shows current state, sensor readings, WiFi status
**UNO R4:** Shows playback commands, color data, audio streaming
**Server:** Shows file uploads, serial commands sent

Run all three simultaneously for full system visibility.
