#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

// Configuration
#define PIN        D5
#define NUMPIXELS  85

// Wave effect settings
#define WAVE_SPEED 50       // milliseconds between updates
#define WAVE_LENGTH 15      // LEDs in wave trail
#define FADE_SPEED 0.01     // Fade-in speed (1% per frame)
#define MIN_BRIGHTNESS 0.2  // Minimum brightness (20%)
#define MAX_BRIGHTNESS 1.0  // Maximum brightness (100%)
#define AUTO_RESET_DELAY 60000  // Auto-reset to white after 60s

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// State variables
uint32_t currentColor = pixels.Color(0, 255, 255);  // Default cyan
uint32_t defaultColor = pixels.Color(255, 255, 255);  // White
int wavePosition = 0;
unsigned long lastUpdate = 0;
unsigned long lastKeypressTime = 0;
float fadeInMultiplier = 0.0;
bool fadingIn = true;

void setup() {
  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
  #endif

  pixels.begin();
  pixels.show();
  Serial.begin(9600);
  Serial.println("Ready: Enter 1-9 or 0 to change colors");
}

void loop() {
  // Handle serial color selection
  if (Serial.available() > 0) {
    char key = Serial.read();
    uint32_t selectedColor = 0;
    bool validKey = true;

    switch (key) {
      case '1': selectedColor = pixels.Color(255, 0, 0); break;     // Red
      case '2': selectedColor = pixels.Color(255, 128, 0); break;   // Orange
      case '3': selectedColor = pixels.Color(255, 255, 0); break;   // Yellow
      case '4': selectedColor = pixels.Color(0, 255, 0); break;     // Green
      case '5': selectedColor = pixels.Color(0, 255, 255); break;   // Cyan
      case '6': selectedColor = pixels.Color(0, 0, 255); break;     // Blue
      case '7': selectedColor = pixels.Color(128, 0, 255); break;   // Purple
      case '8': selectedColor = pixels.Color(255, 0, 128); break;   // Pink
      case '9': selectedColor = pixels.Color(255, 255, 255); break; // White
      case '0': selectedColor = pixels.Color(50, 50, 50); break;    // Dim White
      default: validKey = false; break;
    }

    if (validKey) {
      currentColor = selectedColor;
      fadeInMultiplier = 0.0;
      fadingIn = true;
      lastKeypressTime = millis();
      Serial.print("Color: ");
      Serial.println(key);
    }
  }

  // Auto-reset to white after timeout
  if (lastKeypressTime > 0 && (millis() - lastKeypressTime >= AUTO_RESET_DELAY)) {
    currentColor = defaultColor;
    fadeInMultiplier = 0.0;
    fadingIn = true;
    lastKeypressTime = 0;
    Serial.println("Auto-reset to white");
  }

  // Wave effect animation
  unsigned long currentTime = millis();
  if (currentTime - lastUpdate >= WAVE_SPEED) {
    lastUpdate = currentTime;

    // Fade in on startup
    if (fadingIn) {
      fadeInMultiplier += FADE_SPEED;
      if (fadeInMultiplier >= MAX_BRIGHTNESS) {
        fadeInMultiplier = MAX_BRIGHTNESS;
        fadingIn = false;
      }
    }

    // Extract RGB components
    uint8_t r = (currentColor >> 16) & 0xFF;
    uint8_t g = (currentColor >> 8) & 0xFF;
    uint8_t b = currentColor & 0xFF;

    // Apply wave pattern to all LEDs
    for (int i = 0; i < NUMPIXELS; i++) {
      float angle = (i + wavePosition) * 2.0 * PI / WAVE_LENGTH;
      float brightness = (sin(angle) + 1.0) / 2.0;
      brightness = MIN_BRIGHTNESS + (brightness * (MAX_BRIGHTNESS - MIN_BRIGHTNESS));
      brightness *= fadeInMultiplier;

      pixels.setPixelColor(i, pixels.Color(r * brightness, g * brightness, b * brightness));
    }

    pixels.show();
    wavePosition = (wavePosition + 1) % WAVE_LENGTH;
  }
}

// End

// /*
//  * Memory Bottle - UNO R4 Minima Color Display
//  * Receives color commands from laptop server and displays on LED ring
//  * (Audio plays through laptop speakers)
//  */

// #include <Adafruit_NeoPixel.h>

// // Pin Definitions
// #define LED_PIN D5

// // Configuration
// #define NUM_LEDS 85

// // Display Effects - Choose one:
// // 1 = Smooth Pulse (breathing effect)
// // 2 = Fast Pulse (heartbeat effect)
// // 3 = Wave (color flows across LEDs)
// // 4 = Sparkle (random twinkling)
// // 5 = Solid (static color, no animation)
// #define DISPLAY_EFFECT 3

// // NeoPixel strip
// Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// // State variables
// bool isDisplaying = false;
// uint8_t displayRed = 0;
// uint8_t displayGreen = 0;
// uint8_t displayBlue = 0;

// void setup() {
//   Serial.begin(115200);

//   // Initialize LED strip
//   strip.begin();
//   strip.setBrightness(50);
//   strip.clear();
//   strip.show();

//   Serial.println("UNO R4 Minima Color Display Ready");
//   Serial.println("Waiting for PLAY:START command...");
// }

// void loop() {
//   // Check for serial commands
//   if (Serial.available() > 0) {
//     String command = Serial.readStringUntil('\n');
//     command.trim();

//     if (command == "PLAY:START") {
//       Serial.println("ACK:PLAY_START");
//       startColorDisplay();
//     }
//   }
// }

// void startColorDisplay() {
//   Serial.println("Starting color display...");

//   // Request color data from server
//   Serial.println("REQ:COLOR");

//   // Wait for color data (format: R,G,B)
//   unsigned long timeout = millis() + 2000;
//   while (Serial.available() < 5 && millis() < timeout) {
//     delay(10);
//   }

//   if (Serial.available() > 0) {
//     String colorData = Serial.readStringUntil('\n');
//     parseColorData(colorData);
//   } else {
//     // Use default color if not received
//     Serial.println("Color data timeout, using default white");
//     displayRed = 255;
//     displayGreen = 255;
//     displayBlue = 255;
//   }

//   isDisplaying = true;

//   Serial.println("Color displayed. Starting visual effect...");

//   // Display effect for 15 seconds (synced with audio duration)
//   unsigned long displayDuration = 15000;  // 15 seconds

//   #if DISPLAY_EFFECT == 1
//     effectSmoothPulse(displayDuration);
//   #elif DISPLAY_EFFECT == 2
//     effectFastPulse(displayDuration);
//   #elif DISPLAY_EFFECT == 3
//     effectWave(displayDuration);
//   #elif DISPLAY_EFFECT == 4
//     effectSparkle(displayDuration);
//   #elif DISPLAY_EFFECT == 5
//     effectSolid(displayDuration);
//   #else
//     effectSmoothPulse(displayDuration);  // Default
//   #endif

//   // After display period
//   isDisplaying = false;

//   // Fade out LEDs
//   fadeOutLEDs();

//   Serial.println("Display complete");
// }

// void parseColorData(String data) {
//   // Format: R,G,B
//   int firstComma = data.indexOf(',');
//   int secondComma = data.indexOf(',', firstComma + 1);

//   if (firstComma > 0 && secondComma > firstComma) {
//     displayRed = data.substring(0, firstComma).toInt();
//     displayGreen = data.substring(firstComma + 1, secondComma).toInt();
//     displayBlue = data.substring(secondComma + 1).toInt();

//     Serial.print("Color set to: ");
//     Serial.print(displayRed);
//     Serial.print(",");
//     Serial.print(displayGreen);
//     Serial.print(",");
//     Serial.println(displayBlue);
//   }
// }

// // ============================================================================
// // VISUAL EFFECTS
// // ============================================================================

// void effectSmoothPulse(unsigned long duration) {
//   // Smooth breathing effect
//   unsigned long startTime = millis();
//   while (millis() - startTime < duration) {
//     float pulseSpeed = 0.003;  // Slow, calm breathing
//     float brightness = (sin(millis() * pulseSpeed) + 1.0) / 2.0;
//     brightness = 0.3 + (brightness * 0.7);  // 30% to 100%

//     uint8_t r = displayRed * brightness;
//     uint8_t g = displayGreen * brightness;
//     uint8_t b = displayBlue * brightness;

//     strip.fill(strip.Color(r, g, b));
//     strip.show();
//     delay(20);
//   }
// }

// void effectFastPulse(unsigned long duration) {
//   // Fast heartbeat effect
//   unsigned long startTime = millis();
//   while (millis() - startTime < duration) {
//     float pulseSpeed = 0.008;  // Faster pulse
//     float brightness = (sin(millis() * pulseSpeed) + 1.0) / 2.0;
//     brightness = 0.2 + (brightness * 0.8);  // 20% to 100%

//     uint8_t r = displayRed * brightness;
//     uint8_t g = displayGreen * brightness;
//     uint8_t b = displayBlue * brightness;

//     strip.fill(strip.Color(r, g, b));
//     strip.show();
//     delay(15);
//   }
// }

// void effectWave(unsigned long duration) {
//   // Color wave flowing across the LED strip
//   unsigned long startTime = millis();
//   while (millis() - startTime < duration) {
//     for (int i = 0; i < NUM_LEDS; i++) {
//       // Create wave pattern
//       float wave = sin((millis() * 0.005) + (i * 0.3));
//       float brightness = (wave + 1.0) / 2.0;
//       brightness = 0.1 + (brightness * 0.9);  // 10% to 90% for better wave visibility

//       uint8_t r = displayRed * brightness;
//       uint8_t g = displayGreen * brightness;
//       uint8_t b = displayBlue * brightness;

//       strip.setPixelColor(i, strip.Color(r, g, b));
//     }
//     strip.show();
//     delay(30);
//   }
// }

// void effectSparkle(unsigned long duration) {
//   // Random twinkling effect
//   unsigned long startTime = millis();

//   // Start with base color at 40% brightness
//   uint8_t baseR = displayRed * 0.4;
//   uint8_t baseG = displayGreen * 0.4;
//   uint8_t baseB = displayBlue * 0.4;

//   while (millis() - startTime < duration) {
//     // Fill with base color
//     strip.fill(strip.Color(baseR, baseG, baseB));

//     // Add random sparkles
//     int numSparkles = random(3, 8);
//     for (int i = 0; i < numSparkles; i++) {
//       int pixel = random(NUM_LEDS);
//       strip.setPixelColor(pixel, strip.Color(displayRed, displayGreen, displayBlue));
//     }

//     strip.show();
//     delay(100);
//   }
// }

// void effectSolid(unsigned long duration) {
//   // Static solid color (no animation)
//   strip.fill(strip.Color(displayRed, displayGreen, displayBlue));
//   strip.show();
//   delay(duration);
// }

// void fadeOutLEDs() {
//   // Gradually fade LEDs to black
//   for (int brightness = 100; brightness >= 0; brightness -= 5) {
//     int r = (displayRed * brightness) / 100;
//     int g = (displayGreen * brightness) / 100;
//     int b = (displayBlue * brightness) / 100;

//     strip.fill(strip.Color(r, g, b));
//     strip.show();
//     delay(50);
//   }

//   strip.clear();
//   strip.show();
// }
