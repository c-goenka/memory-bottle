/*
 * Memory Bottle - UNO R4 Minima Hardware Debug
 * Tests LED strip and serial communication
 * (Audio plays through laptop speakers, not UNO R4)
 *
 * Upload to UNO R4 Minima and open Serial Monitor at 115200 baud
 */

#include <Adafruit_NeoPixel.h>

// Pin Definitions
#define LED_PIN 6

// Configuration
#define NUM_LEDS 15

// NeoPixel strip
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n================================");
  Serial.println("  UNO R4 MINIMA HARDWARE DEBUG");
  Serial.println("  (Color display only)");
  Serial.println("================================\n");

  printMenu();
}

void loop() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    Serial.println();

    if (cmd == "1") {
      testLEDs();
    } else if (cmd == "2") {
      testSerial();
    } else if (cmd == "3") {
      testPlaybackProtocol();
    } else if (cmd == "4") {
      testAllPatterns();
    } else if (cmd.startsWith("COLOR:")) {
      // Manual color test: COLOR:255,128,0
      testCustomColor(cmd.substring(6));
    } else if (cmd == "0" || cmd == "m") {
      printMenu();
    } else {
      Serial.println("Invalid option. Press 0 for menu.");
    }
  }
}

void printMenu() {
  Serial.println("\n================================");
  Serial.println("  SELECT TEST:");
  Serial.println("================================");
  Serial.println("1 - LED Strip Test");
  Serial.println("2 - Serial Communication Test");
  Serial.println("3 - Color Display Protocol Test");
  Serial.println("4 - All LED Patterns");
  Serial.println("COLOR:R,G,B - Custom Color (e.g., COLOR:255,0,0)");
  Serial.println("0 - Show Menu");
  Serial.println("================================\n");
}

void testLEDs() {
  Serial.println(">>> LED STRIP TEST");
  Serial.println("Initializing LED strip...");

  strip.begin();
  strip.setBrightness(50);
  strip.clear();
  strip.show();

  Serial.println("✓ LED strip initialized");
  Serial.println("\nRunning LED patterns...");

  // Test 1: Red
  Serial.println("  - Red");
  strip.fill(strip.Color(255, 0, 0));
  strip.show();
  delay(1000);

  // Test 2: Green
  Serial.println("  - Green");
  strip.fill(strip.Color(0, 255, 0));
  strip.show();
  delay(1000);

  // Test 3: Blue
  Serial.println("  - Blue");
  strip.fill(strip.Color(0, 0, 255));
  strip.show();
  delay(1000);

  // Test 4: White
  Serial.println("  - White");
  strip.fill(strip.Color(255, 255, 255));
  strip.show();
  delay(1000);

  // Test 5: Individual LEDs
  Serial.println("  - Individual LED sweep");
  strip.clear();
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(255, 255, 255));
    strip.show();
    delay(100);
  }

  // Test 6: Fade out (like playback ending)
  Serial.println("  - Fade out effect");
  for (int brightness = 100; brightness >= 0; brightness -= 5) {
    int val = (255 * brightness) / 100;
    strip.fill(strip.Color(0, val, val));
    strip.show();
    delay(50);
  }

  strip.clear();
  strip.show();

  Serial.println("✓ LED test complete\n");
  printMenu();
}

void testSerial() {
  Serial.println(">>> SERIAL COMMUNICATION TEST");
  Serial.println("UNO R4 is ready to receive commands.");
  Serial.println("This test verifies serial connection.\n");

  Serial.println("✓ If you can read this, serial is working!");
  Serial.println("Baud rate: 115200");
  Serial.print("Available serial ports: ");
  Serial.println(Serial ? "Serial0" : "None");

  Serial.println("\nSending test message to verify output...");
  for (int i = 1; i <= 5; i++) {
    Serial.print("  Test message ");
    Serial.print(i);
    Serial.println("/5");
    delay(200);
  }

  Serial.println("✓ Serial communication working\n");
  printMenu();
}

void testPlaybackProtocol() {
  Serial.println(">>> COLOR DISPLAY PROTOCOL TEST");
  Serial.println("This simulates the server communication protocol.");
  Serial.println("(Audio plays on laptop speakers, not UNO R4)");
  Serial.println();

  // Step 1: Wait for PLAY:START
  Serial.println("Waiting for PLAY:START command...");
  Serial.println("(Type 'PLAY:START' and press Enter)");

  unsigned long timeout = millis() + 10000;
  bool received = false;

  while (millis() < timeout) {
    if (Serial.available() > 0) {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();

      if (cmd == "PLAY:START") {
        Serial.println("✓ Received PLAY:START");
        received = true;
        break;
      } else {
        Serial.print("✗ Unexpected command: ");
        Serial.println(cmd);
      }
    }
  }

  if (!received) {
    Serial.println("✗ Timeout waiting for PLAY:START\n");
    printMenu();
    return;
  }

  delay(500);

  // Step 2: Request color
  Serial.println("\nACK:PLAY_START");
  Serial.println("REQ:COLOR");
  Serial.println("Waiting for color data (format: R,G,B)...");

  timeout = millis() + 5000;
  received = false;
  String colorData = "";

  while (millis() < timeout) {
    if (Serial.available() > 0) {
      colorData = Serial.readStringUntil('\n');
      colorData.trim();
      received = true;
      break;
    }
  }

  if (!received) {
    Serial.println("✗ Timeout waiting for color data\n");
    printMenu();
    return;
  }

  Serial.print("✓ Received color: ");
  Serial.println(colorData);

  // Parse and display color
  int r, g, b;
  if (parseColor(colorData, r, g, b)) {
    Serial.print("  Parsed RGB: ");
    Serial.print(r);
    Serial.print(", ");
    Serial.print(g);
    Serial.print(", ");
    Serial.println(b);

    strip.begin();
    strip.setBrightness(50);
    strip.fill(strip.Color(r, g, b));
    strip.show();
    Serial.println("  LED showing color");
  } else {
    Serial.println("✗ Invalid color format");
  }

  Serial.println("\nKeeping color displayed for 15 seconds...");
  Serial.println("(Audio would play on laptop during this time)");

  // Keep display on for 15 seconds (simulating audio duration)
  for (int i = 15; i > 0; i--) {
    Serial.print("  ");
    Serial.print(i);
    Serial.println(" seconds remaining...");
    delay(1000);
  }

  // Fade out
  Serial.println("\nFading out LEDs...");
  for (int brightness = 100; brightness >= 0; brightness -= 5) {
    int rVal = (r * brightness) / 100;
    int gVal = (g * brightness) / 100;
    int bVal = (b * brightness) / 100;
    strip.fill(strip.Color(rVal, gVal, bVal));
    strip.show();
    delay(50);
  }

  strip.clear();
  strip.show();

  Serial.println("✓ Color display protocol test complete\n");
  printMenu();
}

void testAllPatterns() {
  Serial.println(">>> ALL LED PATTERNS TEST");

  strip.begin();
  strip.setBrightness(50);

  Serial.println("\n1. Rainbow cycle");
  rainbowCycle(2);

  Serial.println("2. Color wipe - Red");
  colorWipe(strip.Color(255, 0, 0), 50);

  Serial.println("3. Color wipe - Green");
  colorWipe(strip.Color(0, 255, 0), 50);

  Serial.println("4. Color wipe - Blue");
  colorWipe(strip.Color(0, 0, 255), 50);

  Serial.println("5. Theater chase - White");
  theaterChase(strip.Color(255, 255, 255), 50);

  Serial.println("6. Pulse effect");
  pulseEffect(strip.Color(0, 255, 255), 20);

  strip.clear();
  strip.show();

  Serial.println("✓ All patterns complete\n");
  printMenu();
}

void testCustomColor(String colorStr) {
  Serial.print(">>> CUSTOM COLOR: ");
  Serial.println(colorStr);

  int r, g, b;
  if (parseColor(colorStr, r, g, b)) {
    Serial.print("Setting RGB: ");
    Serial.print(r);
    Serial.print(", ");
    Serial.print(g);
    Serial.print(", ");
    Serial.println(b);

    strip.begin();
    strip.setBrightness(50);
    strip.fill(strip.Color(r, g, b));
    strip.show();

    Serial.println("✓ Color displayed\n");
  } else {
    Serial.println("✗ Invalid format. Use: COLOR:R,G,B (e.g., COLOR:255,128,0)\n");
  }

  printMenu();
}

bool parseColor(String data, int &r, int &g, int &b) {
  int firstComma = data.indexOf(',');
  int secondComma = data.indexOf(',', firstComma + 1);

  if (firstComma > 0 && secondComma > firstComma) {
    r = data.substring(0, firstComma).toInt();
    g = data.substring(firstComma + 1, secondComma).toInt();
    b = data.substring(secondComma + 1).toInt();
    return true;
  }
  return false;
}

// LED Pattern Functions

void rainbowCycle(int wait) {
  for (int j = 0; j < 256; j++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / NUM_LEDS) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, color);
    strip.show();
    delay(wait);
  }
}

void theaterChase(uint32_t color, int wait) {
  for (int a = 0; a < 10; a++) {
    for (int b = 0; b < 3; b++) {
      strip.clear();
      for (int c = b; c < NUM_LEDS; c += 3) {
        strip.setPixelColor(c, color);
      }
      strip.show();
      delay(wait);
    }
  }
}

void pulseEffect(uint32_t color, int wait) {
  for (int brightness = 0; brightness <= 255; brightness += 5) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    r = (r * brightness) / 255;
    g = (g * brightness) / 255;
    b = (b * brightness) / 255;

    strip.fill(strip.Color(r, g, b));
    strip.show();
    delay(wait);
  }
  for (int brightness = 255; brightness >= 0; brightness -= 5) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    r = (r * brightness) / 255;
    g = (g * brightness) / 255;
    b = (b * brightness) / 255;

    strip.fill(strip.Color(r, g, b));
    strip.show();
    delay(wait);
  }
}

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
