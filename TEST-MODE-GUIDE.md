# Test Mode Guide - Memory Bottle

Test the entire Memory Bottle state machine without physical sensors!

## Enabling Test Mode

Test mode is **ENABLED** by default in `bottle-recorder.ino`:

```cpp
#define TEST_MODE true  // Set to false to use real sensors
```

## How to Use

1. **Upload the sketch** to your Nano ESP32
2. **Open Serial Monitor** at 115200 baud
3. You'll see the test mode menu:

```
*** TEST MODE ENABLED ***
Use serial commands to simulate sensors

========================================
  TEST MODE COMMANDS:
========================================
CAP_OPEN     - Simulate cap removed
CAP_CLOSE    - Simulate cap put on
TILT         - Simulate bottle tilted
UPRIGHT      - Simulate bottle upright
POT_MIC      - Select microphone
POT_COLOR    - Select color sensor
STATUS       - Show current state
MENU         - Show this menu
========================================
```

## Testing the Full Workflow

### Test 1: Record Audio
```
1. Type: POT_MIC
   → Selects microphone (blue LED)

2. Type: CAP_OPEN
   → Starts recording (LED brightness increases)
   → Creates dummy audio file
   → After ~15 seconds (or immediately in test mode), recording stops

3. Type: CAP_CLOSE
   → Stops recording
   → State changes to INCOMPLETE (yellow pulse)

4. Type: STATUS
   → Check: Has Audio = YES
```

### Test 2: Record Color
```
1. Type: POT_COLOR
   → Selects color sensor (red LED)

2. Type: CAP_OPEN
   → Starts recording
   → Captures test color: 128,64,200

3. Type: CAP_CLOSE
   → Stops recording
   → State changes to READY (green solid)

4. Type: STATUS
   → Check: Has Color = YES
   → Check: Can Pour = YES
```

### Test 3: Pour/Transfer
```
1. Type: CAP_OPEN
   → Cap opened

2. Type: TILT
   → Bottle tilted
   → Triggers WiFi transfer!
   → State changes to TRANSFERRING (cyan pulse)
   → Files upload to server
   → Memory clears, returns to IDLE
```

## Test Scenarios

### Scenario: Try to pour with incomplete recordings
```
1. POT_MIC
2. CAP_OPEN
3. CAP_CLOSE
   → State: INCOMPLETE (only audio)

4. CAP_OPEN
5. TILT
   → Should show warning (incomplete pour attempt)
```

### Scenario: Record same sensor twice
```
1. POT_MIC
2. CAP_OPEN → CAP_CLOSE  (audio #1)
3. POT_MIC (select again)
4. CAP_OPEN → CAP_CLOSE  (audio #2, overwrites #1)
```

### Scenario: Change sensor while cap is open
```
1. POT_MIC
2. CAP_OPEN (starts audio recording)
3. POT_COLOR (changes sensor - note behavior)
```

## What Gets Simulated

**In TEST MODE:**
- ✅ Cap open/close detection
- ✅ Tilt sensor
- ✅ Sensor selection (pot)
- ✅ Audio recording (creates dummy WAV file)
- ✅ Color capture (uses test values: 128,64,200)
- ✅ State machine transitions
- ✅ LED updates

**Still REAL (not simulated):**
- SD card writing
- WiFi connection
- Server communication
- LED display

## Switching to Real Mode

When you're ready to test with actual sensors:

1. Edit `bottle-recorder.ino`
2. Change: `#define TEST_MODE false`
3. Re-upload to ESP32

## Troubleshooting

**Serial Monitor shows garbled text:**
- Check baud rate is set to 115200

**Commands don't work:**
- Make sure to press Enter after typing
- Commands are case-insensitive (CAP_OPEN or cap_open both work)

**LEDs don't change:**
- Check LED strip wiring (D5 pin)
- Verify power supply

**WiFi transfer fails:**
- Check WiFi credentials in code (lines 35-37)
- Make sure server is running: `python bottle-server.py`
- Verify server IP matches sketch

## Quick Reference

**Complete test flow (copy/paste one by one):**
```
STATUS
POT_MIC
CAP_OPEN
CAP_CLOSE
STATUS
POT_COLOR
CAP_OPEN
CAP_CLOSE
STATUS
CAP_OPEN
TILT
STATUS
```

This simulates:
1. Check initial state
2. Record audio
3. Check after audio
4. Record color
5. Check ready state
6. Pour and transfer
7. Check final state (back to IDLE)
