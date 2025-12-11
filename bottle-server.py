"""
Memory Bottle Server
Receives audio/color from ESP32, plays audio on laptop, displays color on UNO R4
"""

from flask import Flask, request, jsonify
import serial
import serial.tools.list_ports
import time
from pathlib import Path
import pygame

app = Flask(__name__)

# Configuration
UPLOAD_FOLDER = Path('uploads')
UPLOAD_FOLDER.mkdir(exist_ok=True)
BAUD_RATE = 115200
COLOR_REQUEST_TIMEOUT = 5  # seconds
ARDUINO_RESET_DELAY = 2  # seconds
DEFAULT_COLOR = "128,128,128"  # gray
SAMPLE_RATE = 16000

arduino_serial = None


def find_arduino_port():
    """Auto-detect Arduino UNO R4"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if 'Arduino' in port.description or 'USB' in port.description or 'usbmodem' in port.device:
            return port.device
    return None


def connect_to_arduino():
    """Connect to Arduino UNO R4"""
    global arduino_serial
    port = find_arduino_port()

    if port:
        try:
            arduino_serial = serial.Serial(port, BAUD_RATE, timeout=1)
            time.sleep(ARDUINO_RESET_DELAY)
            print(f"✓ Connected to UNO R4 on {port}")
            return True
        except Exception as e:
            print(f"✗ Failed to connect: {e}")
    else:
        print("⚠ No UNO R4 found (LEDs disabled)")
    return False


def send_command_to_arduino(command):
    """Send command to Arduino, reconnecting if needed"""
    global arduino_serial
    if not arduino_serial or not arduino_serial.is_open:
        if not connect_to_arduino():
            return False

    try:
        arduino_serial.write(f"{command}\n".encode())
        return True
    except:
        return False


def send_color_handshake(color_data):
    """Wait for UNO R4 to request color, then send it"""
    global arduino_serial
    if not arduino_serial:
        print("✗ No Arduino connection")
        return

    print("Waiting for color request...")
    start = time.time()
    while time.time() - start < COLOR_REQUEST_TIMEOUT:
        if arduino_serial.in_waiting:
            line = arduino_serial.readline().decode().strip()
            print(f"[UNO R4]: {line}")
            if line == "REQ:COLOR":
                arduino_serial.write(f"{color_data}\n".encode())
                print(f"✓ Sent color: {color_data}")
                return
        time.sleep(0.05)
    print("✗ Color request timeout")


def play_audio_on_laptop(audio_path):
    """Play audio file through laptop speakers"""
    try:
        if not pygame.mixer.get_init():
            pygame.mixer.init(frequency=SAMPLE_RATE, size=-16, channels=1)

        print(f"♫ Playing: {audio_path}")
        pygame.mixer.music.load(str(audio_path))
        pygame.mixer.music.play()

        while pygame.mixer.music.get_busy():
            time.sleep(0.1)
        print("✓ Playback complete")
    except Exception as e:
        print(f"✗ Audio error: {e}")


@app.route('/upload', methods=['POST'])
def upload_files():
    """Receive memory from ESP32 and play it back"""
    try:
        print("\n" + "="*40)
        print("RECEIVING MEMORY")
        print("="*40)

        # Get data from ESP32
        color_data = request.headers.get('X-Color-Data', DEFAULT_COLOR)
        audio_data = request.get_data()

        if not audio_data:
            return jsonify({'status': 'error', 'message': 'No audio data'}), 400

        # Save files
        with open(UPLOAD_FOLDER / 'audio.wav', 'wb') as f:
            f.write(audio_data)
        with open(UPLOAD_FOLDER / 'color.dat', 'w') as f:
            f.write(color_data)

        print(f"Saved {len(audio_data)} bytes | Color: {color_data}")

        # Trigger LED display
        if send_command_to_arduino("PLAY:START"):
            send_color_handshake(color_data)

        # Play audio
        play_audio_on_laptop(UPLOAD_FOLDER / 'audio.wav')

        return jsonify({'status': 'success'}), 200

    except Exception as e:
        print(f"✗ Error: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500


@app.route('/status', methods=['GET'])
def status():
    return jsonify({
        'server': 'running',
        'arduino': arduino_serial is not None and arduino_serial.is_open
    })


@app.route('/test-playback', methods=['GET'])
def test_playback():
    """Test playback using saved files from uploads/ folder"""
    try:
        print("\n" + "="*40)
        print("TEST PLAYBACK")
        print("="*40)

        # Load color data
        color_file = UPLOAD_FOLDER / 'color.dat'
        if color_file.exists():
            with open(color_file, 'r') as f:
                test_color = f.read().strip()
            print(f"✓ Color: {test_color}")
        else:
            test_color = "255,0,255"  # Purple
            print(f"⚠ No color.dat, using purple: {test_color}")

        # Load audio file
        test_audio = UPLOAD_FOLDER / 'audio.wav'
        if not test_audio.exists():
            print("⚠ No audio file, creating test audio...")
            create_test_audio(test_audio)
        else:
            print(f"✓ Audio: {test_audio}")

        # Trigger LED display
        if send_command_to_arduino("PLAY:START"):
            send_color_handshake(test_color)

        # Play audio
        play_audio_on_laptop(test_audio)

        return jsonify({
            'status': 'success',
            'message': 'Test complete',
            'color': test_color,
            'audio_file': str(test_audio)
        }), 200

    except Exception as e:
        print(f"✗ Test error: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500


def create_test_audio(filepath):
    """Create 1 second of silent test audio"""
    import wave
    import struct

    duration = 1  # seconds
    num_samples = SAMPLE_RATE * duration

    with wave.open(str(filepath), 'w') as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(SAMPLE_RATE)
        for _ in range(num_samples):
            wav_file.writeframes(struct.pack('h', 0))

    print(f"✓ Created test audio: {filepath}")


if __name__ == '__main__':
    print("Starting Memory Bottle Server...")
    pygame.mixer.init(frequency=SAMPLE_RATE, size=-16, channels=1)
    connect_to_arduino()
    app.run(host='0.0.0.0', port=8080, debug=True, use_reloader=False)



