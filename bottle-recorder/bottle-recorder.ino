/*
 * Memory Bottle - Nano ESP32 Recording System
 * Records audio and color, transfers via WiFi to laptop
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_TCS34725.h>
#include <Wire.h>

// Pin Definitions
#define MIC_PIN A0
#define POT_PIN A2
#define BUTTON_PIN D7      // Cap sensor (HIGH=open/removed, LOW=closed/on)
#define TILT_PIN D2        // Tilt sensor (LOW=tilted, HIGH=upright)
#define LED_PIN D5
#define SD_CS_PIN D10

// Configuration
#define NUM_LEDS 7
#define RECORDING_DURATION 15000  // 15 seconds in ms
#define SAMPLE_RATE 16000
#define DEBOUNCE_TIME 100
#define TILT_STABLE_TIME 100
#define WIFI_TIMEOUT 30000
#define POT_READ_INTERVAL 100

// Test Mode - Set to true to enable serial command simulation
#define TEST_MODE false  // Set to false to use real sensors

// TODO: WiFi credentials
const char* ssid = "jannes";
const char* password = "dipdip35";
const char* serverURL = "http://10.238.45.140:5000/upload";

// System States
enum State {
  IDLE,
  SELECTING,
  RECORDING,
  INCOMPLETE,
  READY,
  TRANSFERRING,
  ERROR_STATE
};

// Sensor Types
enum SensorType {
  AUDIO,
  COLOR
};

// Global Objects
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_TCS34725 colorSensor = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// State Variables
State currentState = IDLE;
SensorType selectedSensor = AUDIO;
bool hasAudio = false;
bool hasColor = false;
int wifiFailCount = 0;

// Timing Variables
unsigned long lastPotRead = 0;
unsigned long lastDebounceTime = 0;
unsigned long tiltStableTime = 0;
unsigned long recordingStartTime = 0;
unsigned long selectingStartTime = 0;

// Button States
bool lastButtonState = HIGH;
bool buttonState = HIGH;
bool previousButtonState = HIGH;  // For edge detection
bool lastTiltState = HIGH;
bool tiltState = HIGH;
bool stableTilt = false;

void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TILT_PIN, INPUT_PULLUP);
  pinMode(MIC_PIN, INPUT);
  pinMode(POT_PIN, INPUT);

  // Initialize LED strip
  strip.begin();
  strip.setBrightness(50);
  strip.show();

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card initialization failed!");
    enterErrorState();
    return;
  }
  Serial.println("SD card initialized");

  // Initialize color sensor
  if (!colorSensor.begin()) {
    Serial.println("Color sensor not found!");
    enterErrorState();
    return;
  }
  Serial.println("Color sensor initialized");

  // Connect to WiFi (commented out for faster testing)
  // WiFi.begin(ssid, password);
  // Serial.print("Connecting to WiFi");
  // int attempts = 0;
  // while (WiFi.status() != WL_CONNECTED && attempts < 20) {
  //   delay(500);
  //   Serial.print(".");
  //   attempts++;
  // }

  // if (WiFi.status() == WL_CONNECTED) {
  //   Serial.println("\nWiFi connected");
  //   Serial.print("IP: ");
  //   Serial.println(WiFi.localIP());
  // } else {
  //   Serial.println("\nWiFi connection failed - will retry during transfer");
  // }
  Serial.println("WiFi connection skipped (enable in code when ready to test transfer)");

  // Clear old recordings on startup (for testing)
  Serial.println("Clearing old recordings...");
  SD.remove("/audio.wav");
  SD.remove("/color.dat");
  SD.remove("/recordings.txt");
  hasAudio = false;
  hasColor = false;
  Serial.println("Memory cleared - starting fresh");

  // Set initial state
  currentState = IDLE;
  updateLEDs(currentState, 0);

  #if TEST_MODE
    Serial.println("\n*** TEST MODE ENABLED ***");
    Serial.println("Use serial commands to simulate sensors");
    printTestMenu();
  #endif
}

void loop() {
  #if TEST_MODE
    // In test mode, check for serial commands to simulate sensors
    handleTestCommands();
  #else
    // Read inputs with debouncing
    readButtonState();
    readTiltState();  // Using physical tilt sensor

    // Read potentiometer periodically
    if (millis() - lastPotRead > POT_READ_INTERVAL) {
      readSensorSelection();
      lastPotRead = millis();
    }

    // Debug: Print tilt sensor status periodically
    static unsigned long lastTiltPrint = 0;
    if (millis() - lastTiltPrint > 1000) {  // Print every 1 second
      Serial.print("[TILT] State: ");
      Serial.print(tiltState == LOW ? "TILTED" : "UPRIGHT");
      Serial.print(" | Stable: ");
      Serial.println(stableTilt ? "YES" : "NO");
      lastTiltPrint = millis();
    }

    // Manual tilt control for testing (type TILT or UPRIGHT in Serial Monitor to override)
    if (Serial.available() > 0) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      command.toUpperCase();

      if (command == "TILT") {
        tiltState = LOW;
        stableTilt = true;
        Serial.println("[MANUAL OVERRIDE] Tilt sensor → TILTED");
      } else if (command == "UPRIGHT") {
        tiltState = HIGH;
        stableTilt = true;
        Serial.println("[MANUAL OVERRIDE] Tilt sensor → UPRIGHT");
      }
    }
  #endif

  // State machine
  switch (currentState) {
    case IDLE:
      handleIdleState();
      break;

    case SELECTING:
      handleSelectingState();
      break;

    case RECORDING:
      handleRecordingState();
      break;

    case INCOMPLETE:
      handleIncompleteState();
      break;

    case READY:
      handleReadyState();
      break;

    case TRANSFERRING:
      handleTransferringState();
      break;

    case ERROR_STATE:
      // Stay in error state until reset
      break;
  }

  // Clear edge detection after state processing
  // This prevents detecting the same edge multiple times
  previousButtonState = buttonState;
}

void readButtonState() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_TIME) {
    if (reading != buttonState) {
      previousButtonState = buttonState;  // Save previous state before updating
      buttonState = reading;
    }
  }

  lastButtonState = reading;
}

void readTiltState() {
  int reading = digitalRead(TILT_PIN);

  if (reading != lastTiltState) {
    tiltStableTime = millis();
    stableTilt = false;
  } else if (!stableTilt && (millis() - tiltStableTime) > TILT_STABLE_TIME) {
    stableTilt = true;
    tiltState = !reading;  // Inverted because sensor is mounted upside down
  }

  lastTiltState = reading;
}

SensorType readSensorSelection() {
  int potValue = analogRead(POT_PIN);

  // ESP32 has 12-bit ADC (0-4095), so we scale the thresholds
  if (potValue < 2048) {
    selectedSensor = AUDIO;
  } else {
    selectedSensor = COLOR;
  }

  return selectedSensor;
}

int getRecordingCount() {
  int count = 0;
  if (hasAudio) count++;
  if (hasColor) count++;
  return count;
}

bool isCapOpen() {
  return buttonState == HIGH;  // Cap off/removed = button not pressed = HIGH
}

bool capJustOpened() {
  // Detect rising edge: cap was on (LOW) and is now removed (HIGH)
  return (previousButtonState == LOW && buttonState == HIGH);
}

bool capJustClosed() {
  // Detect falling edge: cap was off (HIGH) and is now on (LOW)
  return (previousButtonState == HIGH && buttonState == LOW);
}

bool isTilted() {
  return stableTilt && tiltState == LOW;
}

bool canPour() {
  return hasAudio && hasColor;
}

void handleIdleState() {
  updateLEDs(IDLE, 0);

  // Check for potentiometer rotation to enter SELECTING
  static int lastPotValue = 0;
  int potValue = analogRead(POT_PIN);

  if (abs(potValue - lastPotValue) > 50) {  // Threshold for movement detection
    currentState = SELECTING;
    selectingStartTime = millis();  // Reset the timer when entering SELECTING
    Serial.println("State: SELECTING");
    readSensorSelection();  // Update selected sensor
    Serial.print("Selected: ");
    Serial.print(selectedSensor == AUDIO ? "MICROPHONE" : "COLOR SENSOR");
    Serial.print(" (pot value: ");
    Serial.print(potValue);
    Serial.println(")");
  }
  lastPotValue = potValue;

  // Check for cap being removed to start recording
  if (capJustOpened()) {
    startRecording();
  }
}

void handleSelectingState() {
  updateLEDs(SELECTING, 0);

  // Check for cap being removed to start recording
  if (capJustOpened()) {
    startRecording();
  }

  // Return to IDLE if no activity for a while
  if (millis() - selectingStartTime > 5000) {
    currentState = IDLE;
    Serial.println("State: IDLE (timeout from selecting)");
  }

  // Reset timer if potentiometer is being adjusted
  static int lastPotValue = analogRead(POT_PIN);
  static SensorType lastSelectedSensor = selectedSensor;
  int potValue = analogRead(POT_PIN);
  if (abs(potValue - lastPotValue) > 100) {
    selectingStartTime = millis();

    // Check if sensor selection changed
    readSensorSelection();  // Update selected sensor
    if (selectedSensor != lastSelectedSensor) {
      Serial.print("Selected: ");
      Serial.print(selectedSensor == AUDIO ? "MICROPHONE" : "COLOR SENSOR");
      Serial.print(" (pot value: ");
      Serial.print(potValue);
      Serial.println(")");
      lastSelectedSensor = selectedSensor;
    }
  }
  lastPotValue = potValue;
}

void handleRecordingState() {
  unsigned long elapsed = millis() - recordingStartTime;
  int progress = (elapsed * NUM_LEDS) / RECORDING_DURATION;
  progress = constrain(progress, 0, NUM_LEDS);

  updateLEDs(RECORDING, progress);

  // Check if cap is put back on (end recording)
  if (capJustClosed()) {
    stopRecording();
    return;
  }

  // Auto-stop after 15 seconds
  if (elapsed >= RECORDING_DURATION) {
    stopRecording();
  }
}

void handleIncompleteState() {
  updateLEDs(INCOMPLETE, 0);

  // Check for potentiometer rotation to enter SELECTING
  static int lastPotValue = 0;
  int potValue = analogRead(POT_PIN);
  if (abs(potValue - lastPotValue) > 50) {  // Match IDLE threshold
    currentState = SELECTING;
    selectingStartTime = millis();  // Reset the timer when entering SELECTING
    Serial.println("State: SELECTING");
    readSensorSelection();  // Update selected sensor
    Serial.print("Selected: ");
    Serial.print(selectedSensor == AUDIO ? "MICROPHONE" : "COLOR SENSOR");
    Serial.print(" (pot value: ");
    Serial.print(potValue);
    Serial.println(")");
  }
  lastPotValue = potValue;

  // Check for cap being removed to start recording
  if (capJustOpened()) {
    startRecording();
  }
}

void handleReadyState() {
  updateLEDs(READY, 0);

  // Check for pour gesture (open cap AND tilt)
  if (isCapOpen() && isTilted()) {
    currentState = TRANSFERRING;
    Serial.println("State: TRANSFERRING");
    transferData();
  }
}

void handleTransferringState() {
  updateLEDs(TRANSFERRING, 0);
}

void startRecording() {
  currentState = RECORDING;
  recordingStartTime = millis();
  Serial.print("Starting recording: ");
  Serial.println(selectedSensor == AUDIO ? "AUDIO" : "COLOR");

  if (selectedSensor == COLOR) {
    captureColor();
  }
}

void stopRecording() {
  Serial.println("Stopping recording");

  if (selectedSensor == AUDIO) {
    recordAudio();  // Finalize the audio recording
  }

  // Determine next state based on recording count
  int count = getRecordingCount();
  if (count == 1) {
    currentState = INCOMPLETE;
    Serial.println("State: INCOMPLETE");
  } else if (count == 2) {
    currentState = READY;
    Serial.println("State: READY");
  } else {
    currentState = IDLE;
    Serial.println("State: IDLE");
  }

  saveRecordingStatus();
}

void recordAudio() {
  File audioFile = SD.open("/audio.wav", FILE_WRITE);
  if (!audioFile) {
    Serial.println("Failed to open audio.wav for writing");
    enterErrorState();
    return;
  }

  // Write WAV header (44 bytes)
  unsigned long dataSize = SAMPLE_RATE * 2 * (RECORDING_DURATION / 1000);  // 16-bit samples
  writeWAVHeader(audioFile, dataSize);

  #if TEST_MODE
    // In test mode, write dummy audio data (silence)
    Serial.println("Creating dummy audio file (no microphone required)");
    for (int i = 0; i < 1000; i++) {
      int16_t sample16 = 0;
      audioFile.write((byte*)&sample16, 2);
    }
  #else
    // Record audio samples
    unsigned long startTime = millis();
    unsigned long sampleInterval = 1000000 / SAMPLE_RATE;  // microseconds
    unsigned long nextSample = micros();

    while (millis() - startTime < RECORDING_DURATION) {
      // Update button state to detect cap closure
      readButtonState();

      // Check if cap was closed during recording
      if (capJustClosed()) {
        Serial.println("Cap closed during audio recording");
        break;
      }

      if (micros() >= nextSample) {
        int sample = analogRead(MIC_PIN);
        // Convert 12-bit ADC (0-4095) to 16-bit signed (-32768 to 32767)
        int16_t sample16 = (sample - 2048) * 16;
        audioFile.write((byte*)&sample16, 2);
        nextSample += sampleInterval;
      }
    }
  #endif

  audioFile.close();
  hasAudio = true;

  #if TEST_MODE
    Serial.println("Audio recorded successfully (TEST MODE - dummy file)");
  #else
    Serial.println("Audio recorded successfully");
  #endif
}

void captureColor() {
  uint8_t red, green, blue;

  #if TEST_MODE
    // In test mode, use dummy color values
    red = 128;
    green = 64;
    blue = 200;
    Serial.println("Using test color values (no sensor required)");
  #else
    // Real mode - read from color sensor
    uint16_t r, g, b, c;
    colorSensor.getRawData(&r, &g, &b, &c);

    // Normalize to 0-255 range
    red = map(r, 0, 65535, 0, 255);
    green = map(g, 0, 65535, 0, 255);
    blue = map(b, 0, 65535, 0, 255);
  #endif

  // Save to SD card
  File colorFile = SD.open("/color.dat", FILE_WRITE);
  if (!colorFile) {
    Serial.println("Failed to open color.dat for writing");
    enterErrorState();
    return;
  }

  colorFile.print(red);
  colorFile.print(",");
  colorFile.print(green);
  colorFile.print(",");
  colorFile.println(blue);
  colorFile.close();

  hasColor = true;
  Serial.print("Color captured: ");
  Serial.print(red);
  Serial.print(",");
  Serial.print(green);
  Serial.print(",");
  Serial.println(blue);
}

void transferData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting reconnection...");
    WiFi.begin(ssid, password);
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
      delay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
      handleTransferFailure();
      return;
    }
  }

  HTTPClient http;
  http.begin(serverURL);
  http.setTimeout(WIFI_TIMEOUT);

  // Create multipart form data
  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

  // Read files from SD
  File audioFile = SD.open("/audio.wav", FILE_READ);
  File colorFile = SD.open("/color.dat", FILE_READ);

  if (!audioFile || !colorFile) {
    Serial.println("Failed to open files for transfer");
    if (audioFile) audioFile.close();
    if (colorFile) colorFile.close();
    handleTransferFailure();
    return;
  }

  // Build multipart body
  String body = "";

  // Audio file part
  body += "--" + boundary + "\r\n";
  body += "Content-Disposition: form-data; name=\"audio\"; filename=\"audio.wav\"\r\n";
  body += "Content-Type: audio/wav\r\n\r\n";

  String audioData = "";
  while (audioFile.available()) {
    audioData += (char)audioFile.read();
  }
  body += audioData + "\r\n";

  // Color file part
  body += "--" + boundary + "\r\n";
  body += "Content-Disposition: form-data; name=\"color\"; filename=\"color.dat\"\r\n";
  body += "Content-Type: text/plain\r\n\r\n";

  String colorData = "";
  while (colorFile.available()) {
    colorData += (char)colorFile.read();
  }
  body += colorData + "\r\n";
  body += "--" + boundary + "--\r\n";

  audioFile.close();
  colorFile.close();

  // Send POST request
  int httpCode = http.POST(body);

  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Transfer successful!");
    wifiFailCount = 0;
    clearMemory();
    currentState = IDLE;
    Serial.println("State: IDLE");
  } else {
    Serial.print("Transfer failed, code: ");
    Serial.println(httpCode);
    handleTransferFailure();
  }

  http.end();
}

void handleTransferFailure() {
  wifiFailCount++;

  // Show 3 red blinks
  for (int i = 0; i < 3; i++) {
    strip.fill(strip.Color(255, 0, 0));
    strip.show();
    delay(300);
    strip.clear();
    strip.show();
    delay(200);
  }

  if (wifiFailCount >= 3) {
    Serial.println("3 transfer failures, entering ERROR state");
    enterErrorState();
  } else {
    Serial.println("Transfer failed, returning to READY state");
    currentState = READY;
  }
}

void clearMemory() {
  SD.remove("/audio.wav");
  SD.remove("/color.dat");
  hasAudio = false;
  hasColor = false;
  saveRecordingStatus();
  Serial.println("Memory cleared");
}

void loadRecordingStatus() {
  File statusFile = SD.open("/recordings.txt", FILE_READ);
  if (statusFile) {
    String line = statusFile.readStringUntil('\n');
    // Format: audio:1,color:1
    int audioIdx = line.indexOf("audio:");
    int colorIdx = line.indexOf("color:");

    if (audioIdx >= 0) {
      hasAudio = line.charAt(audioIdx + 6) == '1';
    }
    if (colorIdx >= 0) {
      hasColor = line.charAt(colorIdx + 6) == '1';
    }

    statusFile.close();
    Serial.print("Loaded status - Audio: ");
    Serial.print(hasAudio);
    Serial.print(", Color: ");
    Serial.println(hasColor);
  } else {
    hasAudio = false;
    hasColor = false;
  }
}

void saveRecordingStatus() {
  SD.remove("/recordings.txt");
  File statusFile = SD.open("/recordings.txt", FILE_WRITE);
  if (statusFile) {
    statusFile.print("audio:");
    statusFile.print(hasAudio ? "1" : "0");
    statusFile.print(",color:");
    statusFile.println(hasColor ? "1" : "0");
    statusFile.close();
  }
}

void updateLEDs(State state, int progress) {
  strip.clear();

  switch (state) {
    case IDLE:
      // Dim white
      strip.fill(strip.Color(10, 10, 10));
      break;

    case SELECTING:
      // Blue for mic, red for color
      if (selectedSensor == AUDIO) {
        strip.fill(strip.Color(0, 0, 255));
      } else {
        strip.fill(strip.Color(255, 0, 0));
      }
      break;

    case RECORDING:
      // All LEDs in ring increase in brightness based on recording progress
      // Map progress (0-15) to brightness (0-255)
      {
        int brightness = map(progress, 0, NUM_LEDS, 0, 255);
        if (selectedSensor == AUDIO) {
          strip.fill(strip.Color(0, 0, brightness));  // Blue for audio, increasing brightness
        } else {
          strip.fill(strip.Color(brightness, 0, 0));  // Red for color, increasing brightness
        }
      }
      break;

    case INCOMPLETE:
      // Yellow pulse
      {
        int brightness = (sin(millis() / 200.0) + 1) * 127;
        strip.fill(strip.Color(brightness, brightness, 0));
      }
      break;

    case READY:
      // Solid green
      strip.fill(strip.Color(0, 255, 0));
      break;

    case TRANSFERRING:
      // Pulsing cyan
      {
        int brightness = (sin(millis() / 300.0) + 1) * 127;
        strip.fill(strip.Color(0, brightness, brightness));
      }
      break;

    case ERROR_STATE:
      // Red blink
      if ((millis() / 500) % 2 == 0) {
        strip.fill(strip.Color(255, 0, 0));
      }
      break;
  }

  strip.show();
}

void showIncompletePourWarning() {
  // Show 3 yellow blinks
  for (int i = 0; i < 3; i++) {
    strip.fill(strip.Color(255, 255, 0));
    strip.show();
    delay(300);
    strip.clear();
    strip.show();
    delay(200);
  }
  Serial.println("Incomplete pour attempt - need both recordings");
}

void enterErrorState() {
  currentState = ERROR_STATE;
  Serial.println("State: ERROR");

  // Show 5 rapid red blinks
  for (int i = 0; i < 5; i++) {
    strip.fill(strip.Color(255, 0, 0));
    strip.show();
    delay(150);
    strip.clear();
    strip.show();
    delay(150);
  }
}

void writeWAVHeader(File &file, unsigned long dataSize) {
  // RIFF header
  file.print("RIFF");
  unsigned long chunkSize = dataSize + 36;
  file.write((byte*)&chunkSize, 4);
  file.print("WAVE");

  // fmt subchunk
  file.print("fmt ");
  unsigned long subchunk1Size = 16;
  file.write((byte*)&subchunk1Size, 4);
  unsigned short audioFormat = 1;  // PCM
  file.write((byte*)&audioFormat, 2);
  unsigned short numChannels = 1;  // Mono
  file.write((byte*)&numChannels, 2);
  unsigned long sampleRate = SAMPLE_RATE;
  file.write((byte*)&sampleRate, 4);
  unsigned long byteRate = SAMPLE_RATE * 2;  // 16-bit samples
  file.write((byte*)&byteRate, 4);
  unsigned short blockAlign = 2;
  file.write((byte*)&blockAlign, 2);
  unsigned short bitsPerSample = 16;
  file.write((byte*)&bitsPerSample, 2);

  // data subchunk
  file.print("data");
  file.write((byte*)&dataSize, 4);
}

// ============================================================================
// TEST MODE FUNCTIONS
// ============================================================================

#if TEST_MODE

void printTestMenu() {
  Serial.println("\n========================================");
  Serial.println("  TEST MODE COMMANDS:");
  Serial.println("========================================");
  Serial.println("CAP_OPEN     - Simulate cap removed");
  Serial.println("CAP_CLOSE    - Simulate cap put on");
  Serial.println("TILT         - Simulate bottle tilted");
  Serial.println("UPRIGHT      - Simulate bottle upright");
  Serial.println("POT_MIC      - Select microphone");
  Serial.println("POT_COLOR    - Select color sensor");
  Serial.println("STATUS       - Show current state");
  Serial.println("MENU         - Show this menu");
  Serial.println("========================================\n");
}

void handleTestCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toUpperCase();

    if (command == "CAP_OPEN") {
      Serial.println("\n> Simulating: Cap removed");
      previousButtonState = buttonState;
      buttonState = HIGH;  // Cap off = HIGH

    } else if (command == "CAP_CLOSE") {
      Serial.println("\n> Simulating: Cap put on");
      previousButtonState = buttonState;
      buttonState = LOW;  // Cap on = LOW

    } else if (command == "TILT") {
      Serial.println("\n> Simulating: Bottle tilted");
      tiltState = LOW;  // Tilted = LOW
      stableTilt = true;

    } else if (command == "UPRIGHT") {
      Serial.println("\n> Simulating: Bottle upright");
      tiltState = HIGH;  // Upright = HIGH
      stableTilt = true;

    } else if (command == "POT_MIC") {
      Serial.println("\n> Simulating: Potentiometer → Microphone");
      selectedSensor = AUDIO;

    } else if (command == "POT_COLOR") {
      Serial.println("\n> Simulating: Potentiometer → Color Sensor");
      selectedSensor = COLOR;

    } else if (command == "STATUS") {
      printStatus();

    } else if (command == "MENU") {
      printTestMenu();

    } else if (command.length() > 0) {
      Serial.println("\n> Unknown command: " + command);
      Serial.println("Type MENU for help");
    }
  }
}

void printStatus() {
  Serial.println("\n========================================");
  Serial.println("  CURRENT STATUS:");
  Serial.println("========================================");

  Serial.print("State: ");
  switch (currentState) {
    case IDLE: Serial.println("IDLE"); break;
    case SELECTING: Serial.println("SELECTING"); break;
    case RECORDING: Serial.println("RECORDING"); break;
    case INCOMPLETE: Serial.println("INCOMPLETE"); break;
    case READY: Serial.println("READY"); break;
    case TRANSFERRING: Serial.println("TRANSFERRING"); break;
    case ERROR_STATE: Serial.println("ERROR"); break;
  }

  Serial.print("Selected Sensor: ");
  Serial.println(selectedSensor == AUDIO ? "AUDIO" : "COLOR");

  Serial.print("Cap State: ");
  Serial.println(buttonState == HIGH ? "OPEN" : "CLOSED");

  Serial.print("Tilt State: ");
  Serial.println(tiltState == LOW ? "TILTED" : "UPRIGHT");

  Serial.print("Has Audio: ");
  Serial.println(hasAudio ? "YES" : "NO");

  Serial.print("Has Color: ");
  Serial.println(hasColor ? "YES" : "NO");

  Serial.print("Can Pour: ");
  Serial.println(canPour() ? "YES" : "NO");

  Serial.println("========================================\n");
}

#endif  // TEST_MODE
