/*
 * Memory Bottle - UNO R4 Minima Color Display
 * Receives color commands from laptop server and displays on LED ring
 * (Audio plays through laptop speakers)
 */

#include <Adafruit_NeoPixel.h>

// Pin Definitions
#define LED_PIN 6

// Configuration
#define NUM_LEDS 15

// NeoPixel strip
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// State variables
bool isDisplaying = false;
uint8_t displayRed = 0;
uint8_t displayGreen = 0;
uint8_t displayBlue = 0;

void setup() {
  Serial.begin(115200);

  // Initialize LED strip
  strip.begin();
  strip.setBrightness(50);
  strip.clear();
  strip.show();

  Serial.println("UNO R4 Minima Color Display Ready");
  Serial.println("Waiting for PLAY:START command...");
}

void loop() {
  // Check for serial commands
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "PLAY:START") {
      Serial.println("ACK:PLAY_START");
      startColorDisplay();
    }
  }
}

void startColorDisplay() {
  Serial.println("Starting color display...");

  // Request color data from server
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
    Serial.println("Color data timeout, using default white");
    displayRed = 255;
    displayGreen = 255;
    displayBlue = 255;
  }

  isDisplaying = true;

  // Display color on LEDs
  strip.fill(strip.Color(displayRed, displayGreen, displayBlue));
  strip.show();

  Serial.println("Color displayed. Waiting for audio to finish...");

  // Keep display on for 15 seconds (typical audio length)
  // Server controls timing - audio plays on laptop
  delay(15000);

  // After display period
  isDisplaying = false;

  // Fade out LEDs
  fadeOutLEDs();

  Serial.println("Display complete");
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
