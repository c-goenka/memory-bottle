/*
 * Memory Bottle - Nano ESP32 Hardware Debug
 * Tests each sensor and component individually
 *
 * Upload to Nano ESP32 and open Serial Monitor at 115200 baud
 */

#include <SD.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_TCS34725.h>
#include <Wire.h>
#include <WiFi.h>

// Pin Definitions
#define MIC_PIN A0
#define POT_PIN A2
#define BUTTON_PIN D7      // Cap sensor (LOW=open, HIGH=closed)
#define TILT_PIN D2        // Tilt sensor (LOW=tilted, HIGH=upright)
#define LED_PIN D5
#define SD_CS_PIN D10

// Configuration
#define NUM_LEDS 7

// WiFi Configuration (edit these for your network)
const char* WIFI_SSID = "chetan";
const char* WIFI_PASSWORD = "5123632290";
const char* SERVER_IP = "192.168.0.191";

// Objects
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_TCS34725 colorSensor = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

int testNumber = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n================================");
  Serial.println("  NANO ESP32 HARDWARE DEBUG");
  Serial.println("================================\n");

  // Initialize pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TILT_PIN, INPUT_PULLUP);
  pinMode(MIC_PIN, INPUT);
  pinMode(POT_PIN, INPUT);

  printMenu();
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();

    // Clear any remaining characters
    while (Serial.available() > 0) {
      Serial.read();
    }

    Serial.println();

    switch (cmd) {
      case '1':
        testLEDs();
        break;
      case '2':
        testButton();
        break;
      case '3':
        testTilt();
        break;
      case '4':
        testPotentiometer();
        break;
      case '5':
        testMicrophone();
        break;
      case '6':
        testColorSensor();
        break;
      case '7':
        testSDCard();
        break;
      case '8':
        testWiFi();
        break;
      case '9':
        testAll();
        break;
      case '0':
      case 'm':
        printMenu();
        break;
      default:
        Serial.println("Invalid option. Press 0 for menu.");
    }
  }
}

void printMenu() {
  Serial.println("\n================================");
  Serial.println("  SELECT TEST:");
  Serial.println("================================");
  Serial.println("1 - LED Strip Test");
  Serial.println("2 - Button (Cap Sensor) Test");
  Serial.println("3 - Tilt Sensor Test");
  Serial.println("4 - Potentiometer Test");
  Serial.println("5 - Microphone Test");
  Serial.println("6 - Color Sensor Test");
  Serial.println("7 - SD Card Test");
  Serial.println("8 - WiFi Test");
  Serial.println("9 - Run All Tests");
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

  // // Test 1: Red1
  // Serial.println("  - Red");
  // strip.fill(strip.Color(255, 0, 0));
  // strip.show();
  // delay(1000);

  // // Test 2: Green
  // Serial.println("  - Green");
  // strip.fill(strip.Color(0, 255, 0));
  // strip.show();
  // delay(1000);

  // // Test 3: Blue
  // Serial.println("  - Blue");
  // strip.fill(strip.Color(0, 0, 255));
  // strip.show();
  // delay(1000);

  // // Test 4: Individual LEDs
  // Serial.println("  - Individual LED sweep");
  // strip.clear();
  // for (int i = 0; i < NUM_LEDS; i++) {
  //   strip.setPixelColor(i, strip.Color(255, 255, 255));
  //   strip.show();
  //   delay(100);
  // }

  // Test 5: Brightness ramp
  Serial.println("  - Brightness ramp (like recording)");
  for (int brightness = 0; brightness <= 255; brightness += 1) {
    strip.fill(strip.Color(brightness, brightness, brightness));
    strip.show();
    delay(30);
  }

  strip.clear();
  strip.show();

  Serial.println("✓ LED test complete");
  Serial.println("All LEDs should have lit up in patterns.\n");
  printMenu();
}

void testButton() {
  Serial.println(">>> BUTTON (CAP SENSOR) TEST");
  Serial.println("Reading button state for 10 seconds...");
  Serial.println("Open and close cap to test.");
  Serial.println("Expected: HIGH when closed, LOW when open\n");

  unsigned long startTime = millis();
  int lastState = -1;

  while (millis() - startTime < 10000) {
    int buttonState = digitalRead(BUTTON_PIN);

    if (buttonState != lastState) {
      lastState = buttonState;
      if (buttonState == HIGH) {
        Serial.println("  Cap: CLOSED (HIGH)");
      } else {
        Serial.println("  Cap: OPEN (LOW)");
      }
    }

    delay(50);
  }

  Serial.println("\n✓ Button test complete");
  Serial.println("If states didn't change, check wiring.\n");
  printMenu();
}

void testTilt() {
  Serial.println(">>> TILT SENSOR TEST");
  Serial.println("Reading tilt state for 10 seconds...");
  Serial.println("Tilt the bottle to test.");
  Serial.println("Expected: HIGH when upright, LOW when tilted\n");

  unsigned long startTime = millis();
  int lastState = -1;

  while (millis() - startTime < 10000) {
    int tiltState = digitalRead(TILT_PIN);

    if (tiltState != lastState) {
      lastState = tiltState;
      if (tiltState == HIGH) {
        Serial.println("  Bottle: UPRIGHT (HIGH)");
      } else {
        Serial.println("  Bottle: TILTED (LOW)");
      }
    }

    delay(50);
  }

  Serial.println("\n✓ Tilt test complete");
  Serial.println("If states didn't change, check wiring.\n");
  printMenu();
}

void testPotentiometer() {
  Serial.println(">>> POTENTIOMETER TEST");
  Serial.println("Reading potentiometer for 10 seconds...");
  Serial.println("Rotate potentiometer to test.");
  Serial.println("Values: 0-2047 = MIC (blue), 2048-4095 = COLOR (red)\n");

  strip.begin();
  strip.setBrightness(50);

  unsigned long startTime = millis();
  int lastValue = -1;

  while (millis() - startTime < 10000) {
    int potValue = analogRead(POT_PIN);

    if (abs(potValue - lastValue) > 100) {
      lastValue = potValue;

      Serial.print("  Value: ");
      Serial.print(potValue);
      Serial.print(" / 4095");

      if (potValue < 2048) {
        Serial.println(" → MICROPHONE (blue LED)");
        strip.fill(strip.Color(0, 0, 255));
      } else {
        Serial.println(" → COLOR SENSOR (red LED)");
        strip.fill(strip.Color(255, 0, 0));
      }
      strip.show();
    }

    delay(100);
  }

  strip.clear();
  strip.show();

  Serial.println("\n✓ Potentiometer test complete");
  Serial.println("Values should change from 0 to 4095 as you rotate.\n");
  printMenu();
}

void testMicrophone() {
  Serial.println(">>> MICROPHONE TEST");
  Serial.println("Reading microphone for 10 seconds...");
  Serial.println("Make sounds to test (talk, clap, etc.)\n");

  unsigned long startTime = millis();
  int minVal = 4095;
  int maxVal = 0;

  while (millis() - startTime < 10000) {
    int micValue = analogRead(MIC_PIN);

    if (micValue < minVal) minVal = micValue;
    if (micValue > maxVal) maxVal = micValue;

    // Print value every 200ms
    if (millis() % 200 < 10) {
      Serial.print("  Mic: ");
      Serial.print(micValue);
      Serial.print(" | Min: ");
      Serial.print(minVal);
      Serial.print(" | Max: ");
      Serial.println(maxVal);
    }

    delay(10);
  }

  int range = maxVal - minVal;

  Serial.println("\n✓ Microphone test complete");
  Serial.print("Range: ");
  Serial.print(range);

  if (range < 50) {
    Serial.println(" - ✗ TOO LOW! Check wiring or microphone.");
  } else if (range < 200) {
    Serial.println(" - ⚠ Low sensitivity, but may work.");
  } else {
    Serial.println(" - ✓ Good range!");
  }
  Serial.println();
  printMenu();
}

void testColorSensor() {
  Serial.println(">>> COLOR SENSOR TEST");
  Serial.println("Initializing TCS34725...");

  if (!colorSensor.begin()) {
    Serial.println("✗ TCS34725 NOT FOUND!");
    Serial.println("Check:");
    Serial.println("  - I2C wiring (SDA/SCL)");
    Serial.println("  - Level shifter (3.3V required)");
    Serial.println("  - Power connections\n");
    printMenu();
    return;
  }

  Serial.println("✓ TCS34725 found");
  Serial.println("\nReading colors for 10 seconds...");
  Serial.println("Place colored objects near sensor.\n");

  strip.begin();
  strip.setBrightness(50);

  unsigned long startTime = millis();

  while (millis() - startTime < 10000) {
    uint16_t r, g, b, c;
    colorSensor.getRawData(&r, &g, &b, &c);

    // Normalize to 0-255
    uint8_t red = map(r, 0, 65535, 0, 255);
    uint8_t green = map(g, 0, 65535, 0, 255);
    uint8_t blue = map(b, 0, 65535, 0, 255);

    Serial.print("  RGB: ");
    Serial.print(red);
    Serial.print(", ");
    Serial.print(green);
    Serial.print(", ");
    Serial.println(blue);

    // Display color on LEDs
    strip.fill(strip.Color(red, green, blue));
    strip.show();

    delay(500);
  }

  strip.clear();
  strip.show();

  Serial.println("\n✓ Color sensor test complete");
  Serial.println("LEDs should have shown detected colors.\n");
  printMenu();
}

void testSDCard() {
  Serial.println(">>> SD CARD TEST");
  Serial.println("Initializing SD card...");

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("✗ SD CARD INITIALIZATION FAILED!");
    Serial.println("Check:");
    Serial.println("  - SD card is inserted");
    Serial.println("  - Card is formatted as FAT32");
    Serial.println("  - SPI wiring (CS on pin 10)");
    Serial.println("  - Card is not corrupted\n");
    printMenu();
    return;
  }

  Serial.println("✓ SD card initialized");

  // Test write
  Serial.println("\nWriting test file...");
  File testFile = SD.open("/test.txt", FILE_WRITE);
  if (!testFile) {
    Serial.println("✗ Cannot create test file!");
    printMenu();
    return;
  }

  testFile.println("Memory Bottle SD Test");
  testFile.close();
  Serial.println("✓ Write successful");

  // Test read
  Serial.println("Reading test file...");
  testFile = SD.open("/test.txt", FILE_READ);
  if (!testFile) {
    Serial.println("✗ Cannot read test file!");
    printMenu();
    return;
  }

  String content = "";
  while (testFile.available()) {
    content += (char)testFile.read();
  }
  testFile.close();

  Serial.print("Content: ");
  Serial.println(content);
  Serial.println("✓ Read successful");

  // Test delete
  Serial.println("Deleting test file...");
  SD.remove("/test.txt");
  Serial.println("✓ Delete successful");

  Serial.println("\n✓ SD card test complete");
  Serial.println("All operations successful!\n");
  printMenu();
}

void testWiFi() {
  Serial.println(">>> WIFI TEST");
  Serial.println("\nUsing WiFi credentials from top of sketch:");
  Serial.print("  SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("  Server IP: ");
  Serial.println(SERVER_IP);
  Serial.println("\n(Edit WIFI_SSID, WIFI_PASSWORD, and SERVER_IP at the top of the sketch to change)\n");

  String ssid = WIFI_SSID;
  String password = WIFI_PASSWORD;

  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("✗ WiFi connection failed!");
    Serial.println("Check SSID and password.");
  }

  Serial.println();
  printMenu();
}

void testAll() {
  Serial.println("\n>>> RUNNING ALL TESTS\n");

  delay(500);
  testLEDs();
  delay(1000);

  Serial.println("Press Enter to continue to Button test...");
  while (!Serial.available()) {}
  while (Serial.available()) Serial.read();
  testButton();
  delay(1000);

  Serial.println("Press Enter to continue to Tilt test...");
  while (!Serial.available()) {}
  while (Serial.available()) Serial.read();
  testTilt();
  delay(1000);

  Serial.println("Press Enter to continue to Potentiometer test...");
  while (!Serial.available()) {}
  while (Serial.available()) Serial.read();
  testPotentiometer();
  delay(1000);

  Serial.println("Press Enter to continue to Microphone test...");
  while (!Serial.available()) {}
  while (Serial.available()) Serial.read();
  testMicrophone();
  delay(1000);

  Serial.println("Press Enter to continue to Color Sensor test...");
  while (!Serial.available()) {}
  while (Serial.available()) Serial.read();
  testColorSensor();
  delay(1000);

  Serial.println("Press Enter to continue to SD Card test...");
  while (!Serial.available()) {}
  while (Serial.available()) Serial.read();
  testSDCard();
  delay(1000);

  Serial.println("\n================================");
  Serial.println("  ALL TESTS COMPLETE");
  Serial.println("================================\n");

  printMenu();
}
