#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif


// --- CONFIGURATION ---
#define PIN        D5     // Arduino pin connected to NeoPixels
#define NUMPIXELS  85     // How many NeoPixels are changing


Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Wave effect variables
uint32_t currentColor = pixels.Color(0, 255, 255); // Default cyan
int wavePosition = 0;
unsigned long lastUpdate = 0;
int waveSpeed = 50; // milliseconds between updates (lower = faster)
int waveLength = 15; // how many LEDs in the wave trail

// Fade-in effect variables
float fadeInMultiplier = 0.0; // Start at 0 (off)
bool fadingIn = true;
float fadeInSpeed = 0.01; // How fast to fade in (0.01 = 1% per frame)

// Auto-reset to white variables
unsigned long lastKeypressTime = 0;
unsigned long resetDelay = 60000; // 60 seconds in milliseconds
uint32_t defaultColor = pixels.Color(255, 255, 255); // White

void setup() {
 #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
   clock_prescale_set(clock_div_1);
 #endif


 pixels.begin(); // Initialize NeoPixel strip
 pixels.show();  // Initialize all pixels to 'off'
  // Start the Serial Monitor so we can read keypresses
 Serial.begin(9600);
 Serial.println("System Ready: Enter numbers 1-9 or 0 to change colors.");
}


void loop() {
 // Check if data is coming from the Serial Monitor
 if (Serial.available() > 0) {

   // Read the key pressed
   char key = Serial.read();

   // Create a variable to hold the chosen color
   uint32_t selectedColor = 0;
   bool validKey = true; // Flag to check if a valid number was pressed


   // Assign color based on keypress
   switch (key) {
     case '1': selectedColor = pixels.Color(255, 0, 0); break;   // Red
     case '2': selectedColor = pixels.Color(255, 128, 0); break; // Orange
     case '3': selectedColor = pixels.Color(255, 255, 0); break; // Yellow
     case '4': selectedColor = pixels.Color(0, 255, 0); break;   // Green
     case '5': selectedColor = pixels.Color(0, 255, 255); break; // Cyan
     case '6': selectedColor = pixels.Color(0, 0, 255); break;   // Blue
     case '7': selectedColor = pixels.Color(128, 0, 255); break; // Purple
     case '8': selectedColor = pixels.Color(255, 0, 128); break; // Pink
     case '9': selectedColor = pixels.Color(255, 255, 255); break; // White (Bright)
     case '0': selectedColor = pixels.Color(50, 50, 50); break;  // Dim White (Option 10)
     default:
       // If they press a letter or space, ignore it
       validKey = false;
       break;
   }


   // Only update the color if a valid number key was pressed
   if (validKey) {
     currentColor = selectedColor;
     // Reset fade-in effect on every keypress
     fadeInMultiplier = 0.0;
     fadingIn = true;
     // Record the time of this keypress
     lastKeypressTime = millis();
     Serial.print("Changed color to mode: ");
     Serial.println(key);
   }
 }

 // Check if 60 seconds have passed since last keypress
 if (lastKeypressTime > 0 && (millis() - lastKeypressTime >= resetDelay)) {
   // Reset to white
   currentColor = defaultColor;
   fadeInMultiplier = 0.0;
   fadingIn = true;
   lastKeypressTime = 0; // Reset timer to prevent repeated triggers
   Serial.println("Auto-reset to white after 60 seconds");
 }

 // Wave effect animation (runs continuously)
 unsigned long currentTime = millis();
 if (currentTime - lastUpdate >= waveSpeed) {
   lastUpdate = currentTime;

   // Gradually fade in the lights on startup
   if (fadingIn) {
     fadeInMultiplier += fadeInSpeed;
     if (fadeInMultiplier >= 1.0) {
       fadeInMultiplier = 1.0;
       fadingIn = false;
     }
   }

   // Extract RGB components from currentColor
   uint8_t r = (uint8_t)((currentColor >> 16) & 0xFF);
   uint8_t g = (uint8_t)((currentColor >> 8) & 0xFF);
   uint8_t b = (uint8_t)(currentColor & 0xFF);

   // Set brightness for all LEDs using a sine wave pattern
   for (int i = 0; i < NUMPIXELS; i++) {
     // Calculate brightness using sine wave
     // The wave position shifts the sine wave along the strip
     float angle = (i + wavePosition) * 2.0 * PI / waveLength;
     float brightness = (sin(angle) + 1.0) / 2.0; // Normalize to 0-1 range

     // Apply minimum brightness so lights don't go completely off
     brightness = 0.2 + (brightness * 0.8); // Range from 20% to 100%

     // Apply fade-in multiplier
     brightness *= fadeInMultiplier;

     // Set pixel color with calculated brightness
     pixels.setPixelColor(i, pixels.Color(r * brightness, g * brightness, b * brightness));
   }

   pixels.show();

   // Move wave position forward
   wavePosition = (wavePosition + 1) % waveLength;
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
