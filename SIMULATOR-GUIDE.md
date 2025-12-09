# Memory Bottle Simulator Guide

A Python-based simulator that replicates the Arduino ESP32 state machine without requiring any physical hardware.

## Quick Start

```bash
# Install dependencies (if not already installed)
pip install colorama

# Run the simulator
python bottle-simulator.py
```

The simulator starts in IDLE state and accepts commands via console input.

## Available Commands

### Sensor Commands
| Command | Description |
|---------|-------------|
| `cap open` | Simulate removing the cap (button released) |
| `cap close` | Simulate putting cap on (button pressed) |
| `tilt` | Simulate tilting the bottle |
| `upright` | Simulate bottle upright position |
| `pot <value>` | Set potentiometer value (0-4095) |
| `pot mic` | Shortcut to select microphone (pot=0) |
| `pot color` | Shortcut to select color sensor (pot=3000) |
| `color <r> <g> <b>` | Set color sensor RGB values (0-255 each) |

### Control Commands
| Command | Description |
|---------|-------------|
| `status` | Show current state, sensor values, and recordings |
| `reset` | Clear all recordings and return to IDLE |
| `files` | List files in sim_storage/ directory |
| `transfer` | Force WiFi transfer (only works in READY state) |
| `help` | Show command reference |
| `quit` / `exit` | Exit the simulator |

## LED Visualization

The simulator displays LED status at the bottom of the screen using colored dots:

- **Dim white** (●●●●●●●) - IDLE state
- **Blue** - Microphone selected or recording audio
- **Red** - Color sensor selected or recording color
- **Yellow pulsing** - INCOMPLETE (1 of 2 recordings)
- **Green** - READY (both recordings complete)
- **Cyan pulsing** - TRANSFERRING (WiFi upload)
- **Red blinking** - ERROR state

## Example Workflows

### Workflow 1: Complete Recording Cycle

```
> status                    # Check initial state
> pot mic                   # Select microphone
> cap open                  # Start audio recording
[Wait ~15 seconds or let it auto-complete]
> cap close                 # Stop recording (if before 15s)
> status                    # Should show Has Audio: YES, state INCOMPLETE

> pot color                 # Select color sensor
> cap open                  # Record color (instant)
> cap close                 # Complete color recording
> status                    # Should show state READY, both recordings YES

> cap open                  # Open cap
> tilt                      # Tilt bottle to trigger pour
[WiFi transfer happens automatically]
> status                    # Should be back to IDLE, recordings cleared
```

### Workflow 2: Test Incomplete Pour Warning

```
> pot mic
> cap open
> cap close                 # Only audio recorded
> cap open
> tilt                      # Try to pour with incomplete recordings
[Should show warning: need both recordings]
```

### Workflow 3: Test State Transitions

```
> pot mic                   # Enter SELECTING state
[Wait 5 seconds without input]
                            # Should timeout back to IDLE

> pot 1000                  # Set pot to AUDIO range
> pot 3000                  # Change to COLOR range
> status                    # Check selected sensor
```

### Workflow 4: Test Recording Overwrite

```
> pot mic
> cap open
> cap close                 # Audio recording #1

> pot mic                   # Select audio again
> cap open
> cap close                 # Audio recording #2 (overwrites #1)

> status                    # Still only Has Audio: YES (not 2x)
```

## State Machine Flow

```
IDLE
  ↓ pot rotation (>200 change)
SELECTING
  ↓ cap open
RECORDING (15s or until cap closed)
  ↓ cap close
INCOMPLETE (1/2 recordings) OR READY (2/2 recordings)
  ↓ cap open + tilt (if ready)
TRANSFERRING
  ↓ success
IDLE (memory cleared)
```

## File Storage

The simulator creates files in `sim_storage/` directory:

- **audio.wav** - 16kHz, 16-bit PCM mono WAV file (dummy/silent)
- **color.dat** - Text file with "R,G,B" format
- **recordings.txt** - Status file tracking which recordings exist

Use `files` command to see what's in storage.

## Sensor Value Details

### Cap Button
- `0` = cap closed (pressed)
- `1` = cap open (released)
- Uses edge detection (capJustOpened, capJustClosed)

### Tilt Sensor
- `0` = tilted
- `1` = upright

### Potentiometer
- `0-2047` = Microphone selected
- `2048-4095` = Color sensor selected
- Change >200 units triggers SELECTING state

### Color Sensor
- Default: (128, 64, 200)
- Format: RGB tuple (0-255 each)
- Set with: `color 255 128 64`

## Tips

1. **Check status often**: Use `status` to see current state, sensor values, and recordings
2. **Auto-complete recording**: Audio recording auto-completes after 15 seconds
3. **Edge detection**: The cap button uses edge detection, so you need to actually "open" and "close" it with separate commands
4. **Reset anytime**: Use `reset` to clear everything and start fresh
5. **Files persist**: Files in sim_storage/ persist between runs unless you `reset` or delete them

## Troubleshooting

**Simulator won't start:**
- Make sure colorama is installed: `pip install colorama`
- Check Python version: Requires Python 3.6+

**Commands not working:**
- Type `help` to see full command list
- Commands are case-insensitive
- Make sure to press Enter after each command

**State machine stuck:**
- Use `status` to check current state
- Use `reset` to return to IDLE
- Press Ctrl+C to exit and restart

**LED colors not showing:**
- Colorama might not be installed
- Try: `pip install colorama`
- Simulator still works without colors

## Differences from Real Hardware

**What's Simulated:**
- Complete state machine logic
- Edge detection for cap button
- Pot value selection
- Recording timing (15 seconds)
- File creation (dummy WAV files)
- State transitions

**What's Not Simulated:**
- Actual microphone sampling (creates silent WAV)
- Actual color sensor reading (uses preset values)
- Real WiFi transfer (just prints "transferring")
- Debouncing delays
- LED brightness variations

## Running Tests

A test script is included to verify the simulator:

```bash
python test_simulator.py
```

This runs automated tests on all core functionality.

## Integration with Real System

The simulator can be used alongside the real system:

1. **Test logic** in simulator without hardware
2. **Develop features** and verify state transitions
3. **Upload to ESP32** when confident
4. **Use TEST_MODE** in Arduino for similar testing on hardware

The state machine logic matches the Arduino code exactly!
