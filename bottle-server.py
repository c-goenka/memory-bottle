"""
Memory Bottle - Master Server
1. Receives Audio/Color from ESP32
2. Plays Audio on Laptop
3. Sends Color to UNO R4 via Serial
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
SERIAL_PORT = None  # Auto-detected
BAUD_RATE = 115200
arduino_serial = None


# --- ARDUINO CONNECTION LOGIC ---
def find_arduino_port():
    """Auto-detect Arduino UNO R4"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if 'Arduino' in port.description or 'USB' in port.description or 'usbmodem' in port.device:
            return port.device
    return None


def connect_to_arduino():
    """Connect to Arduino UNO R4"""
    global arduino_serial, SERIAL_PORT
    if SERIAL_PORT is None:
        SERIAL_PORT = find_arduino_port()

    if SERIAL_PORT:
        try:
            arduino_serial = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            time.sleep(2)  # Wait for reset
            print(f"✓ Connected to UNO R4 on {SERIAL_PORT}")
            return True
        except Exception as e:
            print(f"✗ Failed to connect to UNO R4: {e}")
    else:
        print("⚠ No UNO R4 found (LEDs will not work)")
    return False


def send_command_to_arduino(command):
    global arduino_serial
    if not arduino_serial or not arduino_serial.is_open:
        if not connect_to_arduino(): return False

    try:
        arduino_serial.write(f"{command}\n".encode())
        return True
    except:
        return False


def send_color_handshake(color_data):
    """Wait for UNO R4 to ask for color, then send it"""
    global arduino_serial
    if not arduino_serial: return


    print("Waiting for UNO R4 to request color...")
    start = time.time()
    while time.time() - start < 5:
        if arduino_serial.in_waiting:
            line = arduino_serial.readline().decode().strip()
            if line == "REQ:COLOR":
                arduino_serial.write(f"{color_data}\n".encode())
                print(f"✓ Sent color data: {color_data}")
                return
        time.sleep(0.05)
    print("✗ Timeout: UNO R4 did not request color")


# --- AUDIO PLAYER ---
def play_audio_on_laptop(audio_path):
    try:
        if not pygame.mixer.get_init():
            pygame.mixer.init(frequency=16000, size=-16, channels=1)

        print(f"♫ Playing audio: {audio_path}")
        pygame.mixer.music.load(str(audio_path))
        pygame.mixer.music.play()

        while pygame.mixer.music.get_busy():
            time.sleep(0.1)
        print("✓ Audio finished")
    except Exception as e:
        print(f"Error playing audio: {e}")


# --- SERVER ENDPOINTS ---
@app.route('/upload', methods=['POST'])
def upload_files():
    try:
        print("\n" + "="*40)
        print("RECEIVING MEMORY")
        print("="*40)


        # 1. Get Data
        color_data = request.headers.get('X-Color-Data', "128,128,128")
        audio_data = request.get_data()

        if not audio_data:
            return jsonify({'status': 'error', 'message': 'No audio data'}), 400


        # 2. Save Files
        with open(UPLOAD_FOLDER / 'audio.wav', 'wb') as f:
            f.write(audio_data)
        with open(UPLOAD_FOLDER / 'color.dat', 'w') as f:
            f.write(color_data)

        print(f"Files Saved | Audio: {len(audio_data)} bytes | Color: {color_data}")


        # 3. Trigger UNO R4 (LEDs)
        if send_command_to_arduino("PLAY:START"):
            send_color_handshake(color_data)

        # 4. Play Audio (Laptop)
        play_audio_on_laptop(UPLOAD_FOLDER / 'audio.wav')


        return jsonify({'status': 'success'}), 200


    except Exception as e:
        print(f"Error: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500


@app.route('/status', methods=['GET'])
def status():
    return jsonify({
        'server': 'running',
        'arduino': arduino_serial is not None and arduino_serial.is_open
    })


if __name__ == '__main__':
    print("STARTING SERVER...")
    pygame.mixer.init(frequency=16000, size=-16, channels=1)
    connect_to_arduino()

    # Run on Port 8080 (Matches your logs)
    app.run(host='0.0.0.0', port=8080, debug=True, use_reloader=False)



