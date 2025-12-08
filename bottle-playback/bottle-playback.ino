/*
 * Memory Bottle - UNO R4 Minima Playback System
 * Receives commands from laptop server, plays audio and displays color
 */

#include <Adafruit_NeoPixel.h>

// Pin Definitions
#define LED_PIN 6
#define I2S_BCLK 9    // Bit Clock
#define I2S_LRC 10    // Left/Right Clock (Word Select)
#define I2S_DIN 11    // Data In

// Configuration
#define NUM_LEDS 15
#define SAMPLE_RATE 16000
#define BUFFER_SIZE 512

// NeoPixel strip
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// State variables
bool isPlaying = false;
uint8_t displayRed = 0;
uint8_t displayGreen = 0;
uint8_t displayBlue = 0;

// Audio buffer
int16_t audioBuffer[BUFFER_SIZE];

void setup() {
  Serial.begin(115200);

  // Initialize LED strip
  strip.begin();
  strip.setBrightness(50);
  strip.clear();
  strip.show();

  // Initialize I2S for audio output
  setupI2S();

  Serial.println("UNO R4 Minima Playback System Ready");
  Serial.println("Waiting for PLAY:START command...");
}

void loop() {
  // Check for serial commands
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "PLAY:START") {
      Serial.println("ACK:PLAY_START");
      startPlayback();
    }
  }

  // Update LED display if playing
  if (isPlaying) {
    updateLEDs();
  }
}

void setupI2S() {
  // Configure I2S pins
  pinMode(I2S_BCLK, OUTPUT);
  pinMode(I2S_LRC, OUTPUT);
  pinMode(I2S_DIN, OUTPUT);

  // TODO: Placeholder
  // Note: Full I2S setup on UNO R4 requires additional configuration
  // This is a placeholder - actual I2S implementation depends on
  // whether you're using the built-in DAC or external MAX98357

  // For MAX98357, you would typically use a library like I2S.h
  // which handles the hardware I2S peripheral on the R4
}

void startPlayback() {
  Serial.println("Starting playback...");

  // Read color data from serial
  // Server should send color data after PLAY:START
  Serial.println("REQ:COLOR");

  // Wait for color data (format: R,G,B)
  unsigned long timeout = millis() + 2000;
  while (Serial.available() < 5 && millis() < timeout) {
    delay(10);
  }

  if (Serial.available() > 0) {
    String colorData = Serial.readStringUntil('\n');
    parseColorData(colorData);
  } else {
    // Use default color if not received
    Serial.println("Color data timeout, using default");
    displayRed = 255;
    displayGreen = 255;
    displayBlue = 255;
  }

  // Request audio data
  Serial.println("REQ:AUDIO");

  isPlaying = true;

  // Display color on LEDs
  strip.fill(strip.Color(displayRed, displayGreen, displayBlue));
  strip.show();

  // Play audio
  playAudio();

  // After playback completes
  isPlaying = false;

  // Fade out LEDs
  fadeOutLEDs();

  Serial.println("Playback complete");
}

void parseColorData(String data) {
  // Format: R,G,B
  int firstComma = data.indexOf(',');
  int secondComma = data.indexOf(',', firstComma + 1);

  if (firstComma > 0 && secondComma > firstComma) {
    displayRed = data.substring(0, firstComma).toInt();
    displayGreen = data.substring(firstComma + 1, secondComma).toInt();
    displayBlue = data.substring(secondComma + 1).toInt();

    Serial.print("Color set to: ");
    Serial.print(displayRed);
    Serial.print(",");
    Serial.print(displayGreen);
    Serial.print(",");
    Serial.println(displayBlue);
  }
}

void playAudio() {
  // Request audio samples from server
  // The server should stream audio data over serial
  // Format: binary 16-bit PCM samples

  Serial.println("Playing audio...");

  unsigned long playbackDuration = 15000;  // 15 seconds max
  unsigned long startTime = millis();
  unsigned long sampleInterval = 1000000 / SAMPLE_RATE;  // microseconds per sample
  unsigned long nextSample = micros();

  int samplesReceived = 0;

  while (millis() - startTime < playbackDuration) {
    // TODO: Placeholder
    // In a real implementation, you would:
    // 1. Receive audio samples from serial
    // 2. Buffer them
    // 3. Output via I2S to MAX98357

    if (micros() >= nextSample) {
      // Read sample from serial if available
      if (Serial.available() >= 2) {
        int16_t sample;
        Serial.readBytes((char*)&sample, 2);

        // Output to I2S
        outputI2SSample(sample);

        samplesReceived++;
        nextSample += sampleInterval;
      } else {
        // No data available, output silence
        outputI2SSample(0);
        nextSample += sampleInterval;
      }
    }

    // Check for end of audio marker
    if (Serial.available() > 0) {
      char c = Serial.peek();
      if (c == 'E') {
        String cmd = Serial.readStringUntil('\n');
        if (cmd == "END:AUDIO") {
          break;
        }
      }
    }
  }

  Serial.print("Audio playback finished. Samples: ");
  Serial.println(samplesReceived);
}

void outputI2SSample(int16_t sample) {
  // TODO: Placeholder
  // This is a simplified placeholder
  // Actual I2S output depends on your hardware setup

  // For MAX98357 using I2S peripheral on UNO R4:
  // You would use the I2S library to send samples to the DAC
  // Example (requires I2S library):
  // I2S.write(sample);

  // For now, this is a placeholder that represents the timing
  delayMicroseconds(1);
}

void updateLEDs() {
  // Keep LEDs displaying the color during playback
  strip.fill(strip.Color(displayRed, displayGreen, displayBlue));
  strip.show();
}

void fadeOutLEDs() {
  // Gradually fade LEDs to black
  for (int brightness = 100; brightness >= 0; brightness -= 5) {
    int r = (displayRed * brightness) / 100;
    int g = (displayGreen * brightness) / 100;
    int b = (displayBlue * brightness) / 100;

    strip.fill(strip.Color(r, g, b));
    strip.show();
    delay(50);
  }

  strip.clear();
  strip.show();
}
