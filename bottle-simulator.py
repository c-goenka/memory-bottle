#!/usr/bin/env python3
"""
Memory Bottle - Python State Machine Simulator
Simulates the Arduino ESP32 state machine without physical hardware
"""

import os
import sys
import time
import math
import struct
import select
from enum import Enum
from pathlib import Path
from typing import Optional

try:
    from colorama import init, Fore, Back, Style
    init(autoreset=True)
    HAS_COLORAMA = True
except ImportError:
    HAS_COLORAMA = False
    print("Warning: colorama not installed. Install with: pip install colorama")
    print("LED colors will not be displayed.\n")

# Constants
RECORDING_DURATION = 15.0  # seconds
SAMPLE_RATE = 16000
DEBOUNCE_TIME = 0.1  # seconds
SELECTING_TIMEOUT = 5.0  # seconds
POT_CHANGE_THRESHOLD = 200
NUM_LEDS = 7

# Storage directory
STORAGE_DIR = Path(__file__).parent / "sim_storage"


class State(Enum):
    """State machine states"""
    IDLE = 0
    SELECTING = 1
    RECORDING = 2
    INCOMPLETE = 3
    READY = 4
    TRANSFERRING = 5
    ERROR_STATE = 6


class SensorType(Enum):
    """Sensor types"""
    AUDIO = 0
    COLOR = 1


class MemoryBottleSimulator:
    """Simulates the Memory Bottle state machine"""

    def __init__(self):
        # State variables
        self.current_state = State.IDLE
        self.selected_sensor = SensorType.AUDIO
        self.has_audio = False
        self.has_color = False
        self.wifi_fail_count = 0

        # Sensor values (persistent)
        self.button_state = 0  # 0=closed, 1=open
        self.previous_button_state = 0
        self.tilt_state = 1  # 0=tilted, 1=upright
        self.pot_value = 0  # 0-4095
        self.color_rgb = (128, 64, 200)  # Default test color

        # Timing
        self.recording_start_time = 0.0
        self.selecting_start_time = 0.0
        self.last_pot_value = 0
        self.last_update_time = time.time()

        # Control flags
        self.running = True
        self.auto_mode = True  # Auto-run by default

        # Initialize storage
        self._init_storage()
        self._load_recording_status()

        # Print welcome message
        self._print_welcome()

    def _init_storage(self):
        """Initialize storage directory"""
        STORAGE_DIR.mkdir(exist_ok=True)
        print(f"Storage directory: {STORAGE_DIR}")

    def _print_welcome(self):
        """Print welcome message"""
        print("\n" + "="*60)
        print("  MEMORY BOTTLE SIMULATOR")
        print("  Python State Machine Tester")
        print("="*60)
        print("\nType 'help' for command list, 'status' for current state")
        print("Press Ctrl+C to exit\n")

    # ========================================================================
    # Sensor Helper Methods
    # ========================================================================

    def is_cap_open(self) -> bool:
        """Check if cap is open"""
        return self.button_state == 1

    def cap_just_opened(self) -> bool:
        """Detect rising edge (cap just opened)"""
        return self.previous_button_state == 0 and self.button_state == 1

    def cap_just_closed(self) -> bool:
        """Detect falling edge (cap just closed)"""
        return self.previous_button_state == 1 and self.button_state == 0

    def is_tilted(self) -> bool:
        """Check if bottle is tilted"""
        return self.tilt_state == 0

    def can_pour(self) -> bool:
        """Check if both recordings are complete"""
        return self.has_audio and self.has_color

    def get_recording_count(self) -> int:
        """Get number of recordings"""
        count = 0
        if self.has_audio:
            count += 1
        if self.has_color:
            count += 1
        return count

    def read_sensor_selection(self) -> SensorType:
        """Determine selected sensor from pot value"""
        if self.pot_value < 2048:
            return SensorType.AUDIO
        else:
            return SensorType.COLOR

    # ========================================================================
    # State Handlers
    # ========================================================================

    def handle_idle_state(self):
        """Handle IDLE state"""
        # Check for potentiometer rotation to enter SELECTING
        if abs(self.pot_value - self.last_pot_value) > POT_CHANGE_THRESHOLD:
            self.current_state = State.SELECTING
            self.selecting_start_time = time.time()
            print("State: SELECTING")

        # Check for cap being removed to start recording
        if self.cap_just_opened():
            self.start_recording()

    def handle_selecting_state(self):
        """Handle SELECTING state"""
        # Check for cap being removed to start recording
        if self.cap_just_opened():
            self.start_recording()
            return

        # Return to IDLE if no activity for timeout period
        elapsed = time.time() - self.selecting_start_time
        if elapsed > SELECTING_TIMEOUT:
            self.current_state = State.IDLE
            print("State: IDLE (timeout from selecting)")
            return

        # Reset timer if potentiometer is being adjusted
        if abs(self.pot_value - self.last_pot_value) > 100:
            self.selecting_start_time = time.time()

    def handle_recording_state(self):
        """Handle RECORDING state"""
        elapsed = time.time() - self.recording_start_time

        # Check if cap is put back on (end recording)
        if self.cap_just_closed():
            self.stop_recording()
            return

        # Auto-stop after 15 seconds
        if elapsed >= RECORDING_DURATION:
            self.stop_recording()

    def handle_incomplete_state(self):
        """Handle INCOMPLETE state"""
        # Check for potentiometer rotation to enter SELECTING
        if abs(self.pot_value - self.last_pot_value) > POT_CHANGE_THRESHOLD:
            self.current_state = State.SELECTING
            self.selecting_start_time = time.time()
            print("State: SELECTING")

        # Check for cap being removed to start recording
        if self.cap_just_opened():
            self.start_recording()

    def handle_ready_state(self):
        """Handle READY state"""
        # Check for pour gesture (open cap AND tilt)
        if self.is_cap_open() and self.is_tilted():
            self.current_state = State.TRANSFERRING
            print("State: TRANSFERRING")
            self.transfer_data()

    def handle_transferring_state(self):
        """Handle TRANSFERRING state - just display, transfer happens in transition"""
        pass

    def handle_error_state(self):
        """Handle ERROR state - stay here until reset"""
        pass

    # ========================================================================
    # Recording Methods
    # ========================================================================

    def start_recording(self):
        """Start recording"""
        # Check for incomplete pour attempt
        if self.is_tilted() and not self.can_pour():
            self.show_incomplete_pour_warning()
            return

        self.current_state = State.RECORDING
        self.recording_start_time = time.time()
        print(f"Starting recording: {self.selected_sensor.name}")

        if self.selected_sensor == SensorType.COLOR:
            self.capture_color()

    def stop_recording(self):
        """Stop recording"""
        print("Stopping recording")

        if self.selected_sensor == SensorType.AUDIO:
            self.record_audio()

        # Determine next state based on recording count
        count = self.get_recording_count()
        if count == 1:
            self.current_state = State.INCOMPLETE
            print("State: INCOMPLETE")
        elif count == 2:
            self.current_state = State.READY
            print("State: READY")
        else:
            self.current_state = State.IDLE
            print("State: IDLE")

        self.save_recording_status()

    def record_audio(self):
        """Record audio to WAV file"""
        audio_path = STORAGE_DIR / "audio.wav"

        try:
            with open(audio_path, 'wb') as f:
                # Write WAV header
                data_size = SAMPLE_RATE * 2 * int(RECORDING_DURATION)  # 16-bit samples
                self._write_wav_header(f, data_size)

                # Write dummy audio data (silence)
                for _ in range(SAMPLE_RATE * int(RECORDING_DURATION)):
                    sample = 0  # Silent audio
                    f.write(struct.pack('<h', sample))

            self.has_audio = True
            print(f"Audio recorded successfully (dummy file): {audio_path}")

        except Exception as e:
            print(f"Error recording audio: {e}")
            self.enter_error_state()

    def capture_color(self):
        """Capture color to file"""
        color_path = STORAGE_DIR / "color.dat"

        try:
            with open(color_path, 'w') as f:
                r, g, b = self.color_rgb
                f.write(f"{r},{g},{b}\n")

            self.has_color = True
            print(f"Color captured: {r},{g},{b}")

        except Exception as e:
            print(f"Error capturing color: {e}")
            self.enter_error_state()

    def _write_wav_header(self, file, data_size):
        """Write WAV file header"""
        # RIFF header
        file.write(b'RIFF')
        file.write(struct.pack('<I', data_size + 36))  # Chunk size
        file.write(b'WAVE')

        # fmt subchunk
        file.write(b'fmt ')
        file.write(struct.pack('<I', 16))  # Subchunk1 size
        file.write(struct.pack('<H', 1))   # Audio format (PCM)
        file.write(struct.pack('<H', 1))   # Num channels (mono)
        file.write(struct.pack('<I', SAMPLE_RATE))
        file.write(struct.pack('<I', SAMPLE_RATE * 2))  # Byte rate
        file.write(struct.pack('<H', 2))   # Block align
        file.write(struct.pack('<H', 16))  # Bits per sample

        # data subchunk
        file.write(b'data')
        file.write(struct.pack('<I', data_size))

    # ========================================================================
    # Transfer Methods
    # ========================================================================

    def transfer_data(self):
        """Transfer data (simulate WiFi transfer)"""
        audio_path = STORAGE_DIR / "audio.wav"
        color_path = STORAGE_DIR / "color.dat"

        if not audio_path.exists() or not color_path.exists():
            print("Error: Missing files for transfer")
            self.handle_transfer_failure()
            return

        print("Simulating WiFi transfer...")
        print(f"  - Uploading {audio_path}")
        print(f"  - Uploading {color_path}")

        # Simulate transfer delay
        time.sleep(1.0)

        # Simulate successful transfer
        print("Transfer successful!")
        self.wifi_fail_count = 0
        self.clear_memory()
        self.current_state = State.IDLE
        print("State: IDLE")

    def handle_transfer_failure(self):
        """Handle transfer failure"""
        self.wifi_fail_count += 1
        print(f"Transfer failed (attempt {self.wifi_fail_count}/3)")

        if self.wifi_fail_count >= 3:
            print("3 transfer failures, entering ERROR state")
            self.enter_error_state()
        else:
            print("Returning to READY state (can retry)")
            self.current_state = State.READY

    def clear_memory(self):
        """Clear all recordings"""
        audio_path = STORAGE_DIR / "audio.wav"
        color_path = STORAGE_DIR / "color.dat"

        if audio_path.exists():
            audio_path.unlink()
        if color_path.exists():
            color_path.unlink()

        self.has_audio = False
        self.has_color = False
        self.save_recording_status()
        print("Memory cleared")

    def show_incomplete_pour_warning(self):
        """Show warning for incomplete pour attempt"""
        print("\n" + "!"*60)
        print("  WARNING: Incomplete pour attempt")
        print("  Need both audio and color recordings!")
        print("!"*60 + "\n")

    def enter_error_state(self):
        """Enter error state"""
        self.current_state = State.ERROR_STATE
        print("\n" + "!"*60)
        print("  ERROR STATE - Reset required")
        print("!"*60 + "\n")

    # ========================================================================
    # File Management
    # ========================================================================

    def _load_recording_status(self):
        """Load recording status from file"""
        status_path = STORAGE_DIR / "recordings.txt"

        if status_path.exists():
            try:
                with open(status_path, 'r') as f:
                    line = f.read().strip()
                    # Format: audio:1,color:1
                    if 'audio:1' in line:
                        self.has_audio = True
                    if 'color:1' in line:
                        self.has_color = True
                    print(f"Loaded status - Audio: {self.has_audio}, Color: {self.has_color}")
            except Exception as e:
                print(f"Error loading status: {e}")
        else:
            self.has_audio = False
            self.has_color = False

    def save_recording_status(self):
        """Save recording status to file"""
        status_path = STORAGE_DIR / "recordings.txt"

        try:
            with open(status_path, 'w') as f:
                audio_val = '1' if self.has_audio else '0'
                color_val = '1' if self.has_color else '0'
                f.write(f"audio:{audio_val},color:{color_val}\n")
        except Exception as e:
            print(f"Error saving status: {e}")

    # ========================================================================
    # LED Visualization
    # ========================================================================

    def update_leds(self):
        """Update LED display"""
        if not HAS_COLORAMA:
            return

        # Calculate current state info
        led_char = "●"
        led_str = led_char * NUM_LEDS

        if self.current_state == State.IDLE:
            color = Style.DIM + Fore.WHITE
            desc = "Dim white (IDLE)"

        elif self.current_state == State.SELECTING:
            if self.selected_sensor == SensorType.AUDIO:
                color = Fore.BLUE
                desc = "Blue (Microphone selected)"
            else:
                color = Fore.RED
                desc = "Red (Color sensor selected)"

        elif self.current_state == State.RECORDING:
            elapsed = time.time() - self.recording_start_time
            progress = min(100, int((elapsed / RECORDING_DURATION) * 100))

            if self.selected_sensor == SensorType.AUDIO:
                color = Fore.BLUE
                desc = f"Blue (Recording audio {progress}%)"
            else:
                color = Fore.RED
                desc = f"Red (Recording color {progress}%)"

        elif self.current_state == State.INCOMPLETE:
            # Yellow pulse
            brightness = int((math.sin(time.time() * 3) + 1) * 50)
            color = Fore.YELLOW
            desc = f"Yellow (Pulsing - 1 of 2 recordings)"

        elif self.current_state == State.READY:
            color = Fore.GREEN
            desc = "Green (Ready to pour)"

        elif self.current_state == State.TRANSFERRING:
            # Cyan pulse
            brightness = int((math.sin(time.time() * 2) + 1) * 50)
            color = Fore.CYAN
            desc = "Cyan (Transferring...)"

        elif self.current_state == State.ERROR_STATE:
            # Red blink
            if int(time.time() * 2) % 2 == 0:
                color = Fore.RED
            else:
                color = Style.RESET_ALL
            desc = "Red (ERROR - blinking)"

        else:
            color = Style.RESET_ALL
            desc = "Unknown state"

        # Print LED visualization
        print(f"\r{color}{led_str}{Style.RESET_ALL}  {desc}", end="", flush=True)

    # ========================================================================
    # Command Processing
    # ========================================================================

    def process_command(self, command: str):
        """Process user command"""
        command = command.strip().lower()
        parts = command.split()

        if not parts:
            return

        cmd = parts[0]

        # Sensor commands
        if cmd == "cap":
            if len(parts) > 1:
                if parts[1] == "open":
                    print("\n> Command: Cap opened")
                    self.previous_button_state = self.button_state
                    self.button_state = 1
                elif parts[1] == "close":
                    print("\n> Command: Cap closed")
                    self.previous_button_state = self.button_state
                    self.button_state = 0
                else:
                    print(f"\n> Unknown cap command: {parts[1]}")
            else:
                print("\n> Usage: cap open | cap close")

        elif cmd == "tilt":
            print("\n> Command: Bottle tilted")
            self.tilt_state = 0

        elif cmd == "upright":
            print("\n> Command: Bottle upright")
            self.tilt_state = 1

        elif cmd == "pot":
            if len(parts) > 1:
                if parts[1] == "mic":
                    self.pot_value = 0
                    self.selected_sensor = SensorType.AUDIO
                    print("\n> Command: Potentiometer → Microphone")
                elif parts[1] == "color":
                    self.pot_value = 3000
                    self.selected_sensor = SensorType.COLOR
                    print("\n> Command: Potentiometer → Color sensor")
                else:
                    try:
                        value = int(parts[1])
                        if 0 <= value <= 4095:
                            self.pot_value = value
                            self.selected_sensor = self.read_sensor_selection()
                            print(f"\n> Command: Potentiometer = {value} ({self.selected_sensor.name})")
                        else:
                            print("\n> Error: Pot value must be 0-4095")
                    except ValueError:
                        print(f"\n> Error: Invalid pot value: {parts[1]}")
            else:
                print("\n> Usage: pot <value> | pot mic | pot color")

        elif cmd == "color":
            if len(parts) >= 4:
                try:
                    r = int(parts[1])
                    g = int(parts[2])
                    b = int(parts[3])
                    if all(0 <= v <= 255 for v in [r, g, b]):
                        self.color_rgb = (r, g, b)
                        print(f"\n> Command: Color sensor = ({r},{g},{b})")
                    else:
                        print("\n> Error: RGB values must be 0-255")
                except ValueError:
                    print("\n> Error: Invalid RGB values")
            else:
                print("\n> Usage: color <r> <g> <b>")

        # Control commands
        elif cmd == "status":
            self.print_status()

        elif cmd == "reset":
            print("\n> Command: Reset system")
            self.clear_memory()
            self.current_state = State.IDLE
            self.wifi_fail_count = 0
            self.button_state = 0
            self.previous_button_state = 0
            self.tilt_state = 1
            self.pot_value = 0
            print("System reset complete")

        elif cmd == "files":
            self.list_files()

        elif cmd == "transfer":
            if self.current_state == State.READY:
                print("\n> Command: Force transfer")
                self.current_state = State.TRANSFERRING
                self.transfer_data()
            else:
                print(f"\n> Error: Cannot transfer in {self.current_state.name} state")
                print("  Must be in READY state with both recordings")

        elif cmd == "help":
            self.print_help()

        elif cmd == "quit" or cmd == "exit":
            print("\n> Exiting simulator...")
            self.running = False

        else:
            print(f"\n> Unknown command: {cmd}")
            print("  Type 'help' for command list")

    def print_status(self):
        """Print current status"""
        print("\n" + "="*60)
        print("  CURRENT STATUS")
        print("="*60)
        print(f"State:           {self.current_state.name}")
        print(f"Selected Sensor: {self.selected_sensor.name}")
        print(f"Cap State:       {'OPEN' if self.button_state == 1 else 'CLOSED'}")
        print(f"Tilt State:      {'TILTED' if self.tilt_state == 0 else 'UPRIGHT'}")
        print(f"Pot Value:       {self.pot_value} / 4095")
        print(f"Color RGB:       {self.color_rgb}")
        print(f"Has Audio:       {'YES' if self.has_audio else 'NO'}")
        print(f"Has Color:       {'YES' if self.has_color else 'NO'}")
        print(f"Can Pour:        {'YES' if self.can_pour() else 'NO'}")
        print(f"WiFi Failures:   {self.wifi_fail_count}")
        print("="*60 + "\n")

    def list_files(self):
        """List files in storage"""
        print("\n" + "="*60)
        print("  FILES IN STORAGE")
        print("="*60)

        files = list(STORAGE_DIR.glob("*"))
        if files:
            for f in sorted(files):
                size = f.stat().st_size
                print(f"  {f.name:20s} {size:>10,} bytes")
        else:
            print("  (no files)")

        print("="*60 + "\n")

    def print_help(self):
        """Print help text"""
        print("\n" + "="*60)
        print("  COMMAND REFERENCE")
        print("="*60)
        print("\nSensor Commands:")
        print("  cap open              - Open the cap")
        print("  cap close             - Close the cap")
        print("  tilt                  - Tilt the bottle")
        print("  upright               - Make bottle upright")
        print("  pot <value>           - Set pot value (0-4095)")
        print("  pot mic               - Select microphone")
        print("  pot color             - Select color sensor")
        print("  color <r> <g> <b>     - Set color sensor RGB values")
        print("\nControl Commands:")
        print("  status                - Show current state and sensors")
        print("  reset                 - Clear memory and return to IDLE")
        print("  files                 - List files in storage")
        print("  transfer              - Force transfer (if in READY state)")
        print("  help                  - Show this help")
        print("  quit / exit           - Exit simulator")
        print("="*60 + "\n")

    # ========================================================================
    # Main Loop
    # ========================================================================

    def run(self):
        """Main simulator loop"""
        print("Simulator running... (auto-mode)")
        print("Type commands and press Enter\n")

        try:
            while self.running:
                # Check for user input (non-blocking)
                if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                    command = sys.stdin.readline()
                    if command:
                        self.process_command(command)

                # Update selected sensor from pot value
                self.selected_sensor = self.read_sensor_selection()

                # Run state machine
                if self.current_state == State.IDLE:
                    self.handle_idle_state()
                elif self.current_state == State.SELECTING:
                    self.handle_selecting_state()
                elif self.current_state == State.RECORDING:
                    self.handle_recording_state()
                elif self.current_state == State.INCOMPLETE:
                    self.handle_incomplete_state()
                elif self.current_state == State.READY:
                    self.handle_ready_state()
                elif self.current_state == State.TRANSFERRING:
                    self.handle_transferring_state()
                elif self.current_state == State.ERROR_STATE:
                    self.handle_error_state()

                # Update LED display
                self.update_leds()

                # Clear edge detection after state processing
                self.previous_button_state = self.button_state
                self.last_pot_value = self.pot_value

                # Small delay for readability
                time.sleep(0.1)

        except KeyboardInterrupt:
            print("\n\nSimulator stopped by user")

        print("\nGoodbye!\n")


def main():
    """Main entry point"""
    simulator = MemoryBottleSimulator()
    simulator.run()


if __name__ == "__main__":
    main()
