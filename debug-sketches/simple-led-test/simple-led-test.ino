/*
 * Simple LED Test - Just turn LEDs white
 * Upload this to test if your LED ring is working
 */

#include <Adafruit_NeoPixel.h>

// Pin and LED configuration
#define LED_PIN D5        // LED data pin
#define NUM_LEDS 7        // Number of LEDs in ring

// Create LED strip object
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  Serial.println("LED Test - Turning on white LEDs");

  // Initialize LED strip
  strip.begin();
  strip.setBrightness(200);  // Set brightness (0-255)

  // Turn all LEDs white
  strip.fill(strip.Color(255, 255, 255));  // RGB: full white
  strip.show();

  Serial.println("LEDs should now be white");
}

void loop() {
  // Nothing - just keep LEDs on
}
