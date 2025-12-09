#!/usr/bin/env python3
"""
Memory Bottle System Debug Script
Tests Python server setup, network, and Arduino communication
"""

import sys
import os
import time
import socket
from pathlib import Path

def print_header(text):
    print("\n" + "=" * 60)
    print(f"  {text}")
    print("=" * 60)

def print_success(text):
    print(f"✓ {text}")

def print_error(text):
    print(f"✗ {text}")

def print_info(text):
    print(f"→ {text}")

def print_warning(text):
    print(f"⚠ {text}")

def test_python_version():
    print_header("1. Python Version")
    version = sys.version_info
    print(f"Python {version.major}.{version.minor}.{version.micro}")

    if version.major >= 3 and version.minor >= 7:
        print_success("Compatible (requires 3.7+)")
        return True
    else:
        print_error("Python 3.7+ required")
        return False

def test_dependencies():
    print_header("2. Python Dependencies")

    required = ['flask', 'serial', 'pygame']
    packages = ['Flask', 'pyserial', 'pygame']

    missing = []
    for module, package in zip(required, packages):
        try:
            __import__(module)
            print_success(f"{package}")
        except ImportError:
            print_error(f"{package} - pip install {package}")
            missing.append(package)

    return len(missing) == 0

def test_network():
    print_header("3. Network Configuration")

    try:
        hostname = socket.gethostname()
        local_ip = socket.gethostbyname(hostname)

        print_success(f"Hostname: {hostname}")
        print_success(f"Local IP: {local_ip}")
        print_info(f"ESP32 URL: http://{local_ip}:5000/upload")

        # Test internet connectivity (for WiFi)
        try:
            socket.create_connection(("8.8.8.8", 53), timeout=3)
            print_success("Internet connected")
        except OSError:
            print_warning("No internet - WiFi may not work")

        return True
    except Exception as e:
        print_error(f"Network error: {e}")
        return False

def test_port():
    print_header("4. Port 5000 Availability")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        result = sock.connect_ex(('127.0.0.1', 5000))
        sock.close()

        if result == 0:
            print_error("Port 5000 in use")
            print_info("Stop other servers or change port")
            return False
        else:
            print_success("Port 5000 available")
            return True
    except Exception as e:
        print_error(f"Port check failed: {e}")
        return False

def test_serial_ports():
    print_header("5. Serial Ports (Arduino)")

    try:
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()

        if not ports:
            print_error("No serial ports found")
            print_info("Connect UNO R4 via USB")
            return False

        print_success(f"Found {len(ports)} port(s):")
        for port in ports:
            arduino = "👍" if 'Arduino' in port.description else ""
            print(f"  • {port.device} - {port.description} {arduino}")

        return True
    except Exception as e:
        print_error(f"Serial check failed: {e}")
        return False

def test_serial_connection():
    print_header("6. Arduino Connection")

    try:
        import serial
        import serial.tools.list_ports

        ports = serial.tools.list_ports.comports()
        arduino_port = None

        for port in ports:
            if 'Arduino' in port.description:
                arduino_port = port.device
                break

        if not arduino_port:
            print_info("No auto-detected Arduino")
            arduino_port = input("Enter port (e.g., /dev/cu.usbmodem14101): ").strip()

        if not arduino_port:
            print_error("No port specified")
            return False

        print_info(f"Testing {arduino_port}...")

        try:
            ser = serial.Serial(arduino_port, 115200, timeout=2)
            time.sleep(2)

            print_success(f"Connected to {arduino_port}")

            if ser.in_waiting > 0:
                data = ser.readline().decode().strip()
                print_info(f"Arduino: {data}")

            ser.close()
            return True

        except serial.SerialException as e:
            print_error(f"Connection failed: {e}")
            return False

    except Exception as e:
        print_error(f"Serial test failed: {e}")
        return False

def test_file_structure():
    print_header("7. Project Files")

    base = Path(__file__).parent
    files = {
        'bottle-recorder/bottle-recorder.ino': 'ESP32 recorder',
        'bottle-playback/bottle-playback.ino': 'UNO R4 playback',
        'bottle-server.py': 'Flask server',
        'requirements.txt': 'Dependencies'
    }

    all_ok = True
    for file, desc in files.items():
        path = base / file
        if path.exists():
            print_success(f"{desc:20} ({file})")
        else:
            print_error(f"{desc:20} ({file}) NOT FOUND")
            all_ok = False

    return all_ok

def test_uploads_dir():
    print_header("8. Uploads Directory")

    uploads = Path(__file__).parent / 'uploads'

    if not uploads.exists():
        print_info("Creating uploads/...")
        try:
            uploads.mkdir(exist_ok=True)
            print_success("Created uploads/")
        except Exception as e:
            print_error(f"Cannot create: {e}")
            return False
    else:
        print_success("uploads/ exists")

    # Test write
    test_file = uploads / '.test'
    try:
        test_file.write_text('test')
        test_file.unlink()
        print_success("uploads/ writable")
        return True
    except Exception as e:
        print_error(f"uploads/ not writable: {e}")
        return False

def test_wifi_config():
    print_header("9. ESP32 WiFi Config")

    sketch = Path(__file__).parent / 'bottle-recorder' / 'bottle-recorder.ino'

    if not sketch.exists():
        print_error("bottle-recorder.ino not found")
        return False

    content = sketch.read_text()

    issues = []
    if 'YOUR_WIFI_SSID' in content:
        issues.append("SSID not set")
    if 'YOUR_WIFI_PASSWORD' in content:
        issues.append("Password not set")
    if 'LAPTOP_IP' in content:
        issues.append("Server IP not set")

    if issues:
        print_error("Config issues:")
        for issue in issues:
            print(f"  • {issue}")
        print_info("Edit bottle-recorder.ino lines 32-34")
        return False
    else:
        print_success("WiFi credentials configured")
        print_warning("Verify they're correct")
        return True

def interactive_server_test():
    print_header("10. Server Test (Optional)")

    response = input("\nStart Flask server? (y/n): ").lower().strip()

    if response != 'y':
        print_info("Skipped")
        return True

    print_info("Starting server on http://0.0.0.0:5000")
    print_info("Press Ctrl+C to stop")
    print_info("Test: http://localhost:5000/status\n")

    try:
        import subprocess
        subprocess.run([sys.executable, 'bottle-server.py'])
    except KeyboardInterrupt:
        print("\n")
        print_success("Server stopped")
        return True
    except Exception as e:
        print_error(f"Server error: {e}")
        return False

def summary(results):
    print_header("SUMMARY")

    passed = sum(1 for r in results if r[1])
    total = len(results)

    print(f"\nPassed: {passed}/{total}\n")

    for name, result in results:
        icon = "✓" if result else "✗"
        print(f"  {icon} {name}")

    if passed == total:
        print("\n" + "=" * 60)
        print_success("All tests passed! System ready.")
        print("=" * 60)
        print("\nNext steps:")
        print("1. Upload debug-nano-esp32.ino to ESP32 (test sensors)")
        print("2. Upload debug-uno-r4.ino to UNO R4 (test playback)")
        print("3. Upload actual sketches")
        print("4. Run: python bottle-server.py")
        return True
    else:
        print("\n" + "=" * 60)
        print_error("Fix issues above before continuing")
        print("=" * 60)
        return False

def main():
    print("\n" + "=" * 60)
    print("  MEMORY BOTTLE SYSTEM DEBUG")
    print("=" * 60)

    tests = [
        ("Python Version", test_python_version),
        ("Dependencies", test_dependencies),
        ("Network", test_network),
        ("Port 5000", test_port),
        ("Serial Ports", test_serial_ports),
        ("Serial Connection", test_serial_connection),
        ("File Structure", test_file_structure),
        ("Uploads Dir", test_uploads_dir),
        ("WiFi Config", test_wifi_config),
    ]

    results = []

    for name, test_func in tests:
        try:
            result = test_func()
            results.append((name, result))
        except KeyboardInterrupt:
            print("\n\nInterrupted")
            sys.exit(1)
        except Exception as e:
            print_error(f"Unexpected error: {e}")
            results.append((name, False))

    all_passed = summary(results)

    if all_passed:
        interactive_server_test()
    else:
        sys.exit(1)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nTerminated")
        sys.exit(0)
