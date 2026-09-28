#include <Adafruit_NeoPixel.h>

// ============================================================================
// Pin & Hardware Configuration
// ============================================================================
#define LED_PIN       6     // Data input pin for the NeoPixel strip
#define NUM_LEDS      60    // Total number of NeoPixel RGB LEDs

// 3-State Knob Pin:
// On Arduino Uno/Nano:
// - If using a potentiometer or 3-position rotary switch with a resistor divider,
//   connect the wiper/output to Analog Pin A3 (analogRead(A3) / channel 3).
// - If you wired a digital 3-way switch (GND / Floating / 5V) to Digital Pin 3,
//   set USE_DIGITAL_TRISTATE_KNOB to true below.
#define KNOB_PIN      A3
#define USE_DIGITAL_TRISTATE_KNOB false

// ============================================================================
// State Machine Enums & Forward Declarations
// ============================================================================
enum StateId {
  STATE_NORMAL,
  STATE_CONCERT,
  STATE_PARTY
};

class LightContext;

class LightState {
public:
  virtual ~LightState() {}
  virtual StateId getId() const = 0;
  virtual const char* getName() const = 0;
  virtual void enter(LightContext* ctx) {}
  virtual void update(LightContext* ctx, unsigned long currentMillis) = 0;
};

// ============================================================================
// Global NeoPixel Object
// ============================================================================
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ============================================================================
// Controller Context
// ============================================================================
class LightContext {
private:
  LightState* currentState;

public:
  LightContext() : currentState(nullptr) {}

  void transitionTo(LightState* newState) {
    if (newState == currentState) return;

    currentState = newState;
    if (currentState != nullptr) {
      Serial.print(F("[State Transition] -> "));
      Serial.println(currentState->getName());
      currentState->enter(this);
    }
  }

  void update(unsigned long currentMillis) {
    if (currentState != nullptr) {
      currentState->update(this, currentMillis);
    }
  }

  StateId getCurrentStateId() const {
    if (currentState != nullptr) {
      return currentState->getId();
    }
    return STATE_NORMAL;
  }
};

// ============================================================================
// State 1: NORMAL
// Warm-white color with subtle low-frequency pulsing (breathing effect)
// ============================================================================
class NormalState : public LightState {
private:
  unsigned long lastUpdateMillis = 0;
  const unsigned long UPDATE_INTERVAL = 25; // 40 FPS smooth breathing
  const unsigned long PULSE_PERIOD = 4500;   // 4.5 seconds full breath cycle

  // Warm-White Base Color (~2700K incandescent glow)
  const uint8_t WARM_R = 255;
  const uint8_t WARM_G = 145;
  const uint8_t WARM_B = 35;

  // Subtle pulsing range: sways between 45% and 85% of full brightness
  const float MIN_FACTOR = 0.45f;
  const float MAX_FACTOR = 0.85f;

public:
  StateId getId() const override { return STATE_NORMAL; }
  const char* getName() const override { return "NORMAL (Warm White Pulse)"; }

  void enter(LightContext* ctx) override {
    strip.setBrightness(255); // Full scale; brightness modulated smoothly in color values
    lastUpdateMillis = 0;
  }

  void update(LightContext* ctx, unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // Calculate low-frequency sine wave modulation (0.0 to 1.0)
    float phase = (float)(currentMillis % PULSE_PERIOD) / (float)PULSE_PERIOD;
    float wave = (sin(phase * 2.0f * PI) + 1.0f) * 0.5f;

    // Scale smoothly within subtle range
    float factor = MIN_FACTOR + wave * (MAX_FACTOR - MIN_FACTOR);

    uint8_t r = (uint8_t)(WARM_R * factor);
    uint8_t g = (uint8_t)(WARM_G * factor);
    uint8_t b = (uint8_t)(WARM_B * factor);
    uint32_t color = strip.Color(r, g, b);

    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, color);
    }
    strip.show();
  }
};

// ============================================================================
// State 2: CONCERT
// Colors smoothly change through rainbow colors, brightness 50%
// ============================================================================
class ConcertState : public LightState {
private:
  unsigned long lastUpdateMillis = 0;
  const unsigned long UPDATE_INTERVAL = 20; // 50 FPS smooth rainbow transition
  uint16_t firstPixelHue = 0;

public:
  StateId getId() const override { return STATE_CONCERT; }
  const char* getName() const override { return "CONCERT (Smooth Rainbow @ 50%)"; }

  void enter(LightContext* ctx) override {
    // Set strip brightness to 50% (128 / 255)
    strip.setBrightness(128);
    lastUpdateMillis = 0;
  }

  void update(LightContext* ctx, unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // Advance rainbow hue counter smoothly
    firstPixelHue += 180; // Full 360-degree rainbow rotation every ~7 seconds

    for (int i = 0; i < NUM_LEDS; i++) {
      // Flow rainbow across all 60 LEDs with gamma-corrected vibrant colors
      uint32_t pixelHue = firstPixelHue + (i * 65536L / NUM_LEDS);
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
  }
};

// ============================================================================
// State 3: PARTY
// Color of each LED is individual and determined by random, brightness 70%
// ============================================================================
class PartyState : public LightState {
private:
  unsigned long lastUpdateMillis = 0;
  const unsigned long UPDATE_INTERVAL = 90; // High-energy party beat interval (90 ms)
  const uint8_t LEDS_PER_BEAT = 15;         // Number of random LEDs refreshed per beat

  // Helper to generate a vibrant random saturated color
  uint32_t getRandomVibrantColor() {
    uint16_t randomHue = random(0, 65536);
    return strip.gamma32(strip.ColorHSV(randomHue, 255, 255));
  }

public:
  StateId getId() const override { return STATE_PARTY; }
  const char* getName() const override { return "PARTY (Random Colors @ 70%)"; }

  void enter(LightContext* ctx) override {
    // Set strip brightness to 70% (178 / 255 ≈ 70%)
    strip.setBrightness(178);

    // Immediately assign every LED its own individual random color
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, getRandomVibrantColor());
    }
    strip.show();
    lastUpdateMillis = millis();
  }

  void update(LightContext* ctx, unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // Pick a burst of random LEDs and assign them new random vivid colors
    for (int k = 0; k < LEDS_PER_BEAT; k++) {
      int randomPixel = random(0, NUM_LEDS);
      strip.setPixelColor(randomPixel, getRandomVibrantColor());
    }
    strip.show();
  }
};

// ============================================================================
// State Instances & Context
// ============================================================================
LightContext systemContext;
NormalState  normalState;
ConcertState concertState;
PartyState   partyState;

// ============================================================================
// 3-State Knob Reader with Debounce & Hysteresis
// ============================================================================
// Thresholds for dividing 0-1023 ADC into 3 sectors:
// - Sector 0 (Normal):  ADC 0   .. ~341  (~0.0V - 1.6V)
// - Sector 1 (Concert): ADC 342 .. ~682  (~1.7V - 3.3V)
// - Sector 2 (Party):   ADC 683 .. 1023  (~3.4V - 5.0V)
const int THRESHOLD_LOW  = 341;
const int THRESHOLD_HIGH = 682;
const int HYSTERESIS     = 35; // Prevents boundary jitter/fluttering

StateId readKnobState(StateId currentState) {
#if USE_DIGITAL_TRISTATE_KNOB
  // Tri-state detection on Digital Pin 3:
  // Position 1: GND | Position 2: Floating/Open | Position 3: 5V
  pinMode(3, INPUT_PULLUP);
  delayMicroseconds(20);
  int pullupVal = digitalRead(3);

  pinMode(3, INPUT);
  delayMicroseconds(20);
  int floatVal = digitalRead(3);

  if (pullupVal == LOW) {
    return STATE_NORMAL;   // Switched to GND
  } else if (floatVal == HIGH) {
    return STATE_PARTY;    // Switched to 5V
  } else {
    return STATE_CONCERT;  // Open / Center position
  }
#else
  // Standard Analog Read on A3 / Pin 3 with hysteresis:
  int rawValue = analogRead(KNOB_PIN);

  if (currentState == STATE_NORMAL) {
    if (rawValue > THRESHOLD_HIGH + HYSTERESIS) return STATE_PARTY;
    if (rawValue > THRESHOLD_LOW + HYSTERESIS)  return STATE_CONCERT;
    return STATE_NORMAL;
  } else if (currentState == STATE_CONCERT) {
    if (rawValue < THRESHOLD_LOW - HYSTERESIS)  return STATE_NORMAL;
    if (rawValue > THRESHOLD_HIGH + HYSTERESIS) return STATE_PARTY;
    return STATE_CONCERT;
  } else { // STATE_PARTY
    if (rawValue < THRESHOLD_LOW - HYSTERESIS)  return STATE_NORMAL;
    if (rawValue < THRESHOLD_HIGH - HYSTERESIS) return STATE_CONCERT;
    return STATE_PARTY;
  }
#endif
}

// ============================================================================
// Arduino setup() and loop()
// ============================================================================
unsigned long lastKnobCheckMillis = 0;
const unsigned long KNOB_SAMPLE_INTERVAL = 60; // Sample knob every 60ms

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0)); // Seed PRNG from unused analog pin

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  // Initial read of knob position
  StateId initialId = readKnobState(STATE_NORMAL);
  switch (initialId) {
    case STATE_CONCERT: systemContext.transitionTo(&concertState); break;
    case STATE_PARTY:   systemContext.transitionTo(&partyState);   break;
    default:            systemContext.transitionTo(&normalState);  break;
  }

  Serial.println(F("Plattenregal NeoPixel Controller Initialized."));
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Periodically check knob position with hysteresis
  if (currentMillis - lastKnobCheckMillis >= KNOB_SAMPLE_INTERVAL) {
    lastKnobCheckMillis = currentMillis;

    StateId desiredState = readKnobState(systemContext.getCurrentStateId());
    if (desiredState != systemContext.getCurrentStateId()) {
      switch (desiredState) {
        case STATE_CONCERT: systemContext.transitionTo(&concertState); break;
        case STATE_PARTY:   systemContext.transitionTo(&partyState);   break;
        case STATE_NORMAL: systemContext.transitionTo(&normalState);  break;
        // default: systemContext.transitionTo(&normalState);  break;
      }
    }
  }

  // 2. Non-blocking update of current lighting state
  systemContext.update(currentMillis);
}
