#include "Arduino_BMI270_BMM150.h"

// --- Hardware Setup ---
const int FSR_PIN_B = A0; // Sensor for tone 'B'
const int FSR_PIN_D = A1; // Sensor for tone 'D'
const int FSR_PIN_G = A2; // Sensor for tone 'G'

const int CAP_BUTTON_PIN = 2; 

// --- Thresholds (Adjust these!) ---
const int FSR_THRESHOLD = 300;     
const float SHAKE_THRESHOLD = 2; 
const float TURN_THRESHOLD = -0.6; 

// --- IMU Variables ---
float x, y, z;

// --- Status Variables ---
bool cap = false;             
bool wasShaken = false;       
bool isUpsideDown = false;    
bool justTurned = false;      

// --- Tone Storage (Sequence) ---
char toneSequence[50];        
int sequenceLength = 0;       

// --- Helper variables (for clean key presses) ---
bool b_wasPressed = false;
bool d_wasPressed = false;
bool g_wasPressed = false;

// --- Debounce Variables for Cap Button ---
int capButtonState = HIGH;        
int lastCapButtonState = HIGH;    
unsigned long lastCapDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY_MS = 50;


void setup() {
  Serial.begin(9600);

  if (!IMU.begin()) {
    Serial.println("Error: Failed to initialize IMU!");
    while (1);
  }

  pinMode(CAP_BUTTON_PIN, INPUT_PULLUP);
  
  Serial.println("Bottle Layering Sequencer Ready.");
  Serial.println("Press 'r' on keyboard to reset all sounds."); // HINWEIS
}

void loop() {
  
  // 1. Read Inputs
  // --------------------------------
  
  // NEU: Prüft auf 'r' für Reset
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'r' || command == 'R') {
      Serial.println("CMD:RESET"); // Sendet Reset-Befehl an Laptop
      clearSequence(); // Löscht die Arduino-Sequenz
    }
  }

  // Read the physical cap button (with debounce)
  int reading = digitalRead(CAP_BUTTON_PIN);

  if (reading != lastCapButtonState) {
    lastCapDebounceTime = millis();
  }

  if ((millis() - lastCapDebounceTime) > DEBOUNCE_DELAY_MS) {
    if (reading != capButtonState) {
      capButtonState = reading;

      if (capButtonState == LOW) {
        cap = !cap; 
        Serial.print(">>> Cap is now: ");
        Serial.println(cap ? "OPEN" : "CLOSED");
        
        if (cap == false) {
          wasShaken = false;
          Serial.println("Shake status reset.");
        }
      }
    }
  }
  lastCapButtonState = reading;
  
  // Read IMU data
  float totalAccel = 0;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    totalAccel = sqrt(x * x + y * y + z * z);
  }

  // Read FSR sensors
  int fsr_b_val = analogRead(FSR_PIN_B);
  int fsr_d_val = analogRead(FSR_PIN_D);
  int fsr_g_val = analogRead(FSR_PIN_G);

  bool b_isPressed = fsr_b_val > FSR_THRESHOLD;
  bool d_isPressed = fsr_d_val > FSR_THRESHOLD;
  bool g_isPressed = fsr_g_val > FSR_THRESHOLD;
  bool anyPressed = b_isPressed || d_isPressed || g_isPressed;

  
  // 2. Logic: Process states
  // --------------------------------

  // --- LOGIC: CAP CLOSED (Detect shake) ---
  if (cap == false) {
    if (totalAccel > SHAKE_THRESHOLD) {
      wasShaken = true;
      Serial.println("!!! SHAKEN !!!");
      delay(200); 
    }
  }
  
  // --- LOGIC: CAP OPEN (Record & Playback) ---
  if (cap == true) {
    
    // A) RECORDING: If one of the FSRs is PRESSED
    if (b_isPressed && !b_wasPressed) {
      recordTone('B');
    }
    if (d_isPressed && !d_wasPressed) {
      recordTone('D');
    }
    if (g_isPressed && !g_wasPressed) {
      recordTone('G');
    }

    // B) PLAYBACK: If NOTHING is pressed AND bottle is turned upside down
    justTurned = false;
    if (z < TURN_THRESHOLD && !isUpsideDown) {
      isUpsideDown = true;
      justTurned = true; 
      Serial.println("--- Bottle turned upside down ---");
    } else if (z > abs(TURN_THRESHOLD)) {
      isUpsideDown = false; 
    }

    // Trigger playback
    if (justTurned && !anyPressed) {
      if (wasShaken) {
        // Case 3: Shaken -> Play Chord
        playChord();
        wasShaken = false; 
      } else {
        // Case 2: Not shaken -> Play Sequence
        playSequence();
      }
      // ❗️❗️❗️
      // clearSequence(); // <--- ENTFERNT!
      // Die Sequenz baut sich jetzt immer weiter auf.
      // ❗️❗️❗️
    }
  }

  // 3. Save sensor states for next loop
  b_wasPressed = b_isPressed;
  d_wasPressed = d_isPressed;
  g_wasPressed = g_isPressed;

  delay(20); 
}


// --- HELPER FUNCTIONS ---

/**
 * Adds a tone to the sequence.
 * SENDS AN IMMEDIATE PLAY COMMAND.
 */
void recordTone(char tone) {
  // Sendet den Befehl zum sofortigen Abspielen (einzeln)
  Serial.print("PLAY:"); 
  Serial.println(tone);

  // Fügt den Ton zur Sequenz für die spätere Wiedergabe hinzu
  if (sequenceLength < 50) { 
    toneSequence[sequenceLength] = tone;
    sequenceLength++;
    Serial.print("Recorded for sequence: "); Serial.println(tone);
  } else {
    Serial.println("Sequence memory full!");
  }
}

/**
 * Sends the "Play this Sequence" command to the laptop.
 * (Sends all recorded tones)
 */
void playSequence() {
  if (sequenceLength == 0) {
    Serial.println("PLAYBACK: Sequence is empty.");
    return;
  }

  Serial.print("PLAYBACK (Sequence): ");
  for (int i = 0; i < sequenceLength; i++) {
    char tone = toneSequence[i];

    Serial.print(" "); Serial.print(tone);
  }
  Serial.println("\n(Sequence End)");
}

/**
 * Sends the "Play this Chord" command to the laptop.
 * (Sends all unique tones)
 */
void playChord() {
  if (sequenceLength == 0) {
    Serial.println("PLAYBACK: (Chord) Sequence is empty.");
    return;
  }

  bool hasB = false;
  bool hasD = false;
  bool hasG = false;

  for (int i = 0; i < sequenceLength; i++) {
    if (toneSequence[i] == 'B') hasB = true;
    if (toneSequence[i] == 'D') hasD = true;
    if (toneSequence[i] == 'G') hasG = true;
  }

  Serial.print("CMD:CHORD:"); // Command for "Chord"
  if (hasB) Serial.print("B,");
  if (hasD) Serial.print("D,");
  if (hasG) Serial.print("G,");
  Serial.println(); // Finish command
}

/**
 * Clears the sequence.
 */
void clearSequence() {
  sequenceLength = 0;
  Serial.println(">>> Sequence cleared.");
}