"""
Memory Bottle - Python Flask Server
Receives audio and color files from ESP32 and triggers UNO R4 playback
"""

from flask import Flask, request, jsonify
import os
import serial
import serial.tools.list_ports
import time
from pathlib import Path
import pygame

app = Flask(__name__)

# Configuration
UPLOAD_FOLDER = Path('uploads')
UPLOAD_FOLDER.mkdir(exist_ok=True)

# Serial port configuration
SERIAL_PORT = None  # Will be auto-detected
BAUD_RATE = 115200
arduino_serial = None


def find_arduino_port():
    """Auto-detect Arduino UNO R4 Minima serial port"""
    ports = serial.tools.list_ports.comports()

    for port in ports:
        # Look for Arduino in port description
        if 'Arduino' in port.description or 'USB' in port.description:
            print(f"Found potential Arduino on: {port.device} - {port.description}")
            return port.device

    # If no Arduino found, list all available ports
    print("Available serial ports:")
    for port in ports:
        print(f"  {port.device} - {port.description}")

    return None


def connect_to_arduino():
    """Connect to Arduino UNO R4 Minima"""
    global arduino_serial, SERIAL_PORT

    if SERIAL_PORT is None:
        SERIAL_PORT = find_arduino_port()

    if SERIAL_PORT is None:
        print("ERROR: Could not find Arduino. Please specify port manually.")
        print("Update SERIAL_PORT variable in this script.")
        return False

    try:
        arduino_serial = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)  # Wait for Arduino to reset
        print(f"Connected to Arduino on {SERIAL_PORT}")
        return True
    except serial.SerialException as e:
        print(f"Failed to connect to Arduino: {e}")
        return False


def send_command_to_arduino(command):
    """Send command to Arduino via serial"""
    global arduino_serial

    if arduino_serial is None or not arduino_serial.is_open:
        if not connect_to_arduino():
            return False

    try:
        arduino_serial.write(f"{command}\n".encode())
        print(f"Sent command to Arduino: {command}")
        return True
    except Exception as e:
        print(f"Error sending command to Arduino: {e}")
        return False


def send_color_to_arduino(color_data):
    """Send color data to Arduino"""
    global arduino_serial

    # Wait for Arduino to request color
    timeout = time.time() + 5
    while time.time() < timeout:
        if arduino_serial and arduino_serial.in_waiting > 0:
            request = arduino_serial.readline().decode().strip()
            if request == "REQ:COLOR":
                arduino_serial.write(f"{color_data}\n".encode())
                print(f"Sent color data: {color_data}")
                return True
        time.sleep(0.1)

    print("Timeout waiting for color request")
    return False


def play_audio_on_laptop(audio_path):
    """Play audio through laptop speakers using pygame"""
    try:
        # Initialize pygame mixer if not already initialized
        if not pygame.mixer.get_init():
            pygame.mixer.init(frequency=16000, size=-16, channels=1)

        print(f"Playing audio on laptop speakers: {audio_path}")
        pygame.mixer.music.load(str(audio_path))
        pygame.mixer.music.play()

        # Wait for audio to finish playing
        while pygame.mixer.music.get_busy():
            time.sleep(0.1)

        print("Audio playback finished")
        return True

    except Exception as e:
        print(f"Error playing audio: {e}")
        return False


@app.route('/upload', methods=['POST'])
def upload_files():
    """
    Endpoint to receive audio.wav and color.dat from ESP32
    Then triggers playback on UNO R4
    """
    try:
        # Check if files are in request
        if 'audio' not in request.files or 'color' not in request.files:
            return jsonify({
                'status': 'error',
                'message': 'Missing audio or color file'
            }), 400

        audio_file = request.files['audio']
        color_file = request.files['color']

        # Validate filenames
        if audio_file.filename == '' or color_file.filename == '':
            return jsonify({
                'status': 'error',
                'message': 'Empty filename'
            }), 400

        # Save files to disk
        audio_path = UPLOAD_FOLDER / 'audio.wav'
        color_path = UPLOAD_FOLDER / 'color.dat'

        audio_file.save(audio_path)
        color_file.save(color_path)

        print(f"Received files:")
        print(f"  Audio: {audio_path} ({os.path.getsize(audio_path)} bytes)")
        print(f"  Color: {color_path} ({os.path.getsize(color_path)} bytes)")

        # Read color data for logging
        with open(color_path, 'r') as f:
            color_data = f.read().strip()
            print(f"  Color RGB: {color_data}")

        # Send command to Arduino to display color
        if send_command_to_arduino("PLAY:START"):
            # Wait a moment for Arduino to acknowledge
            time.sleep(0.5)

            # Read response from Arduino if available
            if arduino_serial and arduino_serial.in_waiting > 0:
                response = arduino_serial.readline().decode().strip()
                print(f"Arduino response: {response}")

            # Send color data when Arduino requests it
            send_color_to_arduino(color_data)

            # Play audio through laptop speakers
            play_audio_on_laptop(audio_path)

            return jsonify({
                'status': 'success',
                'message': 'Files received, audio played on laptop, color displayed on Arduino',
                'audio_size': os.path.getsize(audio_path),
                'color': color_data
            }), 200
        else:
            # Even if Arduino isn't connected, play audio
            print("Arduino not connected, playing audio anyway")
            play_audio_on_laptop(audio_path)

            return jsonify({
                'status': 'warning',
                'message': 'Files saved and audio played, but could not connect to Arduino for color display'
            }), 200

    except Exception as e:
        print(f"Error processing upload: {e}")
        return jsonify({
            'status': 'error',
            'message': str(e)
        }), 500


@app.route('/status', methods=['GET'])
def status():
    """Check server and Arduino connection status"""
    arduino_connected = arduino_serial is not None and arduino_serial.is_open

    return jsonify({
        'server': 'running',
        'arduino_connected': arduino_connected,
        'serial_port': SERIAL_PORT,
        'upload_folder': str(UPLOAD_FOLDER)
    })


@app.route('/test', methods=['GET'])
def test_playback():
    """Test endpoint to trigger playback manually"""
    if send_command_to_arduino("PLAY:START"):
        return jsonify({'status': 'success', 'message': 'Playback command sent'})
    else:
        return jsonify({'status': 'error', 'message': 'Failed to send command'}), 500


def cleanup():
    """Cleanup function to close serial connection"""
    global arduino_serial
    if arduino_serial and arduino_serial.is_open:
        arduino_serial.close()
        print("Closed Arduino serial connection")


if __name__ == '__main__':
    import atexit
    atexit.register(cleanup)

    print("=" * 60)
    print("Memory Bottle Server Starting")
    print("=" * 60)

    # Initialize pygame mixer for audio playback
    pygame.mixer.init(frequency=16000, size=-16, channels=1)
    print("Audio playback initialized (laptop speakers)")

    # Try to connect to Arduino
    connect_to_arduino()

    # Get local IP address
    import socket
    hostname = socket.gethostname()
    local_ip = socket.gethostbyname(hostname)

    print(f"\nServer Information:")
    print(f"  Local IP: {local_ip}")
    print(f"  Port: 5000")
    print(f"  Upload endpoint: http://{local_ip}:5000/upload")
    print(f"  Status endpoint: http://{local_ip}:5000/status")
    print(f"\nUpdate your ESP32 sketch with:")
    print(f"  serverURL = \"http://{local_ip}:5000/upload\";")
    print("\n" + "=" * 60)

    # Run Flask server
    # Use 0.0.0.0 to allow connections from other devices on network
    app.run(host='0.0.0.0', port=8080, debug=True, use_reloader=False)
