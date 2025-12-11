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
#define COLOR_RECORDING_DURATION 8000  // 8 seconds for color capture
#define SAMPLE_RATE 16000
#define DEBOUNCE_TIME 100
#define TILT_STABLE_TIME 100
#define WIFI_TIMEOUT 30000
#define POT_READ_INTERVAL 100
#define POT_THRESHOLD 150  // Minimum change to trigger sensor selection
#define POT_MIDPOINT 2048  // ESP32 12-bit ADC midpoint (0-4095)
#define SELECTING_TIMEOUT 5000  // Time before returning to IDLE
#define LED_UPDATE_INTERVAL 100  // LED update rate during recording

// Test Mode - Set to true to enable serial command simulation
#define TEST_MODE false  // Set to false to use real sensors

// TODO: WiFi credentials
const char* ssid = "jannes";
const char* password = "dipdip35";
const char* serverURL = "http://10.238.45.55:8080/upload";

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

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed - will retry during transfer");
  }

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
    readTiltState();

    // Read potentiometer periodically
    if (millis() - lastPotRead > POT_READ_INTERVAL) {
      readSensorSelection();
      lastPotRead = millis();
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

  if (potValue < POT_MIDPOINT) {
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
  static int lastPotValue = -1;
  int potValue = analogRead(POT_PIN);

  if (lastPotValue == -1) {
    lastPotValue = potValue;
  }

  if (abs(potValue - lastPotValue) > POT_THRESHOLD) {
    lastPotValue = potValue;
    currentState = SELECTING;
    selectingStartTime = millis();
    readSensorSelection();
    Serial.print("State: SELECTING (");
    Serial.print(selectedSensor == AUDIO ? "MICROPHONE" : "COLOR SENSOR");
    Serial.println(")");
  }

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
    return;
  }

  // Return to IDLE if no activity for a while
  if (millis() - selectingStartTime > SELECTING_TIMEOUT) {
    currentState = IDLE;
    Serial.println("State: IDLE (timeout)");
    return;
  }

  // Reset timer if potentiometer is being adjusted
  static int lastPotValue = -1;
  static SensorType lastSelectedSensor = selectedSensor;
  int potValue = analogRead(POT_PIN);

  if (lastPotValue == -1) {
    lastPotValue = potValue;
  }

  if (abs(potValue - lastPotValue) > POT_THRESHOLD) {
    lastPotValue = potValue;
    selectingStartTime = millis();
    readSensorSelection();

    if (selectedSensor != lastSelectedSensor) {
      Serial.print("Selected: ");
      Serial.println(selectedSensor == AUDIO ? "MICROPHONE" : "COLOR SENSOR");
      lastSelectedSensor = selectedSensor;
    }
  }
}

void handleRecordingState() {
  // NOTE: This handler is not typically reached during recording because
  // recordAudio() and captureColor() are blocking functions.
  // State transitions happen in startRecording() after recording completes.
  updateLEDs(RECORDING, 0);
}

void handleIncompleteState() {
  updateLEDs(INCOMPLETE, 0);

  // Check for potentiometer rotation to enter SELECTING
  static int lastPotValue = -1;
  int potValue = analogRead(POT_PIN);

  if (lastPotValue == -1) {
    lastPotValue = potValue;
  }

  if (abs(potValue - lastPotValue) > POT_THRESHOLD) {
    lastPotValue = potValue;
    currentState = SELECTING;
    selectingStartTime = millis();
    readSensorSelection();
    Serial.print("State: SELECTING (");
    Serial.print(selectedSensor == AUDIO ? "MICROPHONE" : "COLOR SENSOR");
    Serial.println(")");
  }

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

  Serial.print("Recording ");
  Serial.println(selectedSensor == AUDIO ? "AUDIO" : "COLOR");

  if (selectedSensor == AUDIO) {
    recordAudio();
  } else {
    captureColor();
  }

  // Determine next state based on recording count
  int count = getRecordingCount();

  if (count == 1) {
    currentState = INCOMPLETE;
    Serial.println("State: INCOMPLETE (need 1 more)");
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
  SD.remove("/audio.wav");
  File audioFile = SD.open("/audio.wav", FILE_WRITE);

  if (!audioFile) {
    enterErrorState();
    return;
  }

  writeWAVHeader(audioFile, 0);  // Placeholder header
  Serial.println("Recording audio (15s)...");

  const int bufferSize = 4096;
  static uint8_t buffer[bufferSize];
  int bufferIndex = 0;

  unsigned long startTime = millis();
  unsigned long sampleInterval = 1000000 / SAMPLE_RATE;
  unsigned long nextSample = micros();
  unsigned long totalBytesWritten = 0;
  unsigned long lastLEDUpdate = 0;

  // Record for full duration
  while (millis() - startTime < RECORDING_DURATION) {
    // Update LEDs to show progress
    unsigned long currentTime = millis();
    if (currentTime - lastLEDUpdate >= LED_UPDATE_INTERVAL) {
      unsigned long elapsed = currentTime - startTime;
      int progress = (elapsed * NUM_LEDS) / RECORDING_DURATION;
      progress = constrain(progress, 0, NUM_LEDS);
      updateLEDs(RECORDING, progress);
      lastLEDUpdate = currentTime;
    }

    // Sample audio
    if (micros() >= nextSample) {
      int sample = analogRead(MIC_PIN);
      int16_t sample16 = (sample - 2048) * 16;

      buffer[bufferIndex] = sample16 & 0xFF;
      buffer[bufferIndex + 1] = (sample16 >> 8);
      bufferIndex += 2;

      nextSample += sampleInterval;
      if (micros() - nextSample > 2000) nextSample = micros();

      if (bufferIndex >= bufferSize) {
        audioFile.write(buffer, bufferSize);
        totalBytesWritten += bufferSize;
        bufferIndex = 0;
      }
    }
  }

  // Write remaining buffer data
  if (bufferIndex > 0) {
    audioFile.write(buffer, bufferIndex);
    totalBytesWritten += bufferIndex;
  }

  Serial.print("Audio recorded: ");
  Serial.print(totalBytesWritten);
  Serial.println(" bytes");

  // Completion blink effect
  strip.clear();
  strip.show();
  delay(200);
  strip.fill(strip.Color(0, 0, 255));
  strip.show();
  delay(500);

  // Update WAV header with actual size
  audioFile.seek(0);
  writeWAVHeader(audioFile, totalBytesWritten);
  audioFile.close();

  hasAudio = true;
}


void captureColor() {
  Serial.println("Recording color (8s)...");

  unsigned long startTime = millis();
  unsigned long lastLEDUpdate = 0;

  // Show LEDs gradually brightening during recording period
  while (millis() - startTime < COLOR_RECORDING_DURATION) {
    unsigned long currentTime = millis();
    if (currentTime - lastLEDUpdate >= LED_UPDATE_INTERVAL) {
      unsigned long elapsed = currentTime - startTime;
      int progress = (elapsed * NUM_LEDS) / COLOR_RECORDING_DURATION;
      progress = constrain(progress, 0, NUM_LEDS);
      updateLEDs(RECORDING, progress);
      lastLEDUpdate = currentTime;
    }
  }

  // Capture color at end of recording period
  uint8_t red, green, blue;

  #if TEST_MODE
    red = 128;
    green = 64;
    blue = 200;
    Serial.println("Using test color values");
  #else
    uint16_t r, g, b, c;
    colorSensor.getRawData(&r, &g, &b, &c);
    red = map(r, 0, 65535, 0, 255);
    green = map(g, 0, 65535, 0, 255);
    blue = map(b, 0, 65535, 0, 255);
  #endif

  Serial.print("Color captured: ");
  Serial.print(red);
  Serial.print(",");
  Serial.print(green);
  Serial.print(",");
  Serial.println(blue);

  // Completion blink effect
  strip.clear();
  strip.show();
  delay(200);
  strip.fill(strip.Color(red, green, blue));
  strip.show();
  delay(500);

  // Save to SD card
  File colorFile = SD.open("/color.dat", FILE_WRITE);
  if (!colorFile) {
    Serial.println("Failed to save color data");
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
}

void transferData() {
  // Ensure WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting WiFi...");
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

  // Read color data
  String colorString = "128,128,128";  // Default gray
  File colorFile = SD.open("/color.dat", FILE_READ);
  if (colorFile) {
    if (colorFile.available()) {
      colorString = colorFile.readString();
      colorString.trim();
    }
    colorFile.close();
  }

  if (colorString.length() == 0) {
    colorString = "128,128,128";
  }

  // Open audio file
  File audioFile = SD.open("/audio.wav", FILE_READ);
  if (!audioFile) {
    Serial.println("Failed to open audio file");
    handleTransferFailure();
    return;
  }

  // Send HTTP POST request
  HTTPClient http;
  http.begin(serverURL);
  http.setTimeout(WIFI_TIMEOUT);
  http.addHeader("Content-Type", "audio/wav");
  http.addHeader("X-Color-Data", colorString);

  Serial.print("Uploading ");
  Serial.print(audioFile.size());
  Serial.print(" bytes with color ");
  Serial.println(colorString);

  int httpCode = http.sendRequest("POST", &audioFile, audioFile.size());
  audioFile.close();

  // Handle response
  if (httpCode == 200) {
    Serial.println("Transfer successful!");
    wifiFailCount = 0;
    clearMemory();
    currentState = IDLE;
    Serial.println("State: IDLE");
  } else {
    Serial.print("Transfer failed, code: ");
    Serial.println(httpCode);
    if (httpCode > 0) {
      Serial.println(http.getString());
    }
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
    Serial.println("3 transfer failures, clearing memory and resetting to IDLE");
    wifiFailCount = 0;  // Reset the failure count
    clearMemory();
    currentState = IDLE;
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
      // Use gradual brightness curve starting from very dim (5) to full (255)
      {
        // Calculate normalized progress (0.0 to 1.0)
        float normalizedProgress = (float)progress / NUM_LEDS;

        // Apply quadratic curve for smoother, more gradual increase
        float curve = normalizedProgress * normalizedProgress;

        // Map to brightness range: 5 (very dim start) to 255 (full brightness)
        int brightness = 5 + (curve * 250);

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
