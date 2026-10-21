/*
 * ============================================================================
 * Plattenregal Light Controller (SK6812 LED Strip)
 * ============================================================================
 *
 * Target: Arduino Uno / Nano / compatible (ATmega328P or higher)
 * LED Strip: SK6812 RGB (15 LEDs)
 *
 * Hardware Controls:
 *   - 3-State Toggle Switch (ON-OFF-ON):
 *       * Left Position:   Pin 11 LOW,  Pin 12 HIGH -> STATE_NORMAL  (Pulsing in Potentiometer Color)
 *       * Center Position: Pin 11 HIGH, Pin 12 HIGH -> STATE_CONCERT (Smooth Rainbow)
 *       * Right Position:  Pin 11 HIGH, Pin 12 LOW  -> STATE_PARTY   (Random Colors)
 *   - Rotary Brightness Controller (Analog Pin A2):
 *       * Master luminosity knob active across ALL states (0% to 100% brightness).
 *   - Rotary Color Controller (Analog Pin A3):
 *       * Color selection knob active in NormalState (ignored in Concert and Party):
 *           -   0° to 180° (ADC   0 .. 511): Full chromatic RGB color selection.
 *           - 180° to 270° (ADC 512 .. 767): Nice-red, nice-blue, and nice-purple.
 *           - 270° to 360° (ADC 768 .. 1023): Cold-white to warm-white transition.
 *
 * Non-blocking Architecture:
 *   - Driven strictly via millis() state updates without delay().
 * ============================================================================
 */

#include <FastLED.h>

// ============================================================================
// Pin & Hardware Configuration
// ============================================================================
#define LED_PIN       6     // Data input pin for the SK6812 strip
#define NUM_LEDS      15    // Total number of SK6812 RGB LEDs

// 3-State Toggle Switch (ON-OFF-ON):
// Outer terminals connected to Pins 11 and 12, Center/Common connected to GND.
const int SWITCH_PIN_LEFT    = 11;
const int SWITCH_PIN_RIGHT   = 12;

// Potentiometer Analog Inputs:
// Outer terminals connected to 5V and GND, wipers connected to analog pins.
const int PIN_BRIGHTNESS_POT = A2; // Global brightness / luminosity control for all states
const int PIN_COLOR_POT      = A3; // Color selection potentiometer for NormalState

// ============================================================================
// FastLED LED Buffer
// ============================================================================
CRGB leds[NUM_LEDS];

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
// BrightnessController Class (Analog Pin A2)
// Encapsulates analog reading, jitter filtering, and FastLED luminosity scaling
// ============================================================================
class BrightnessController {
private:
  uint8_t pin;
  unsigned long sampleInterval;
  int filteredRaw;
  int lastRaw;
  uint8_t currentBrightness;
  unsigned long lastCheckMillis;

  // 4-sample average with 2-count deadband and EMA smoothing to eliminate noise/jitter
  int readFilteredRaw() {
    int sum = 0;
    for (uint8_t i = 0; i < 4; i++) {
      sum += analogRead(pin);
    }
    int raw = sum >> 2;

    if (lastRaw < 0) {
      lastRaw = raw;
      filteredRaw = raw;
    } else {
      // 2-count deadband to absorb potentiometer wiper electrical noise
      if (abs(raw - lastRaw) > 2) {
        lastRaw = raw;
      }
      // Smooth exponential moving average (EMA)
      filteredRaw = (filteredRaw * 3 + lastRaw) / 4;
    }
    return filteredRaw;
  }

public:
  BrightnessController(uint8_t analogPin, unsigned long sampleIntervalMs = 20, uint8_t initialBrightness = 180)
    : pin(analogPin),
      sampleInterval(sampleIntervalMs),
      filteredRaw(-1),
      lastRaw(-1),
      currentBrightness(initialBrightness),
      lastCheckMillis(0) {}

  void begin() {
    pinMode(pin, INPUT);
  }

  void update(unsigned long currentMillis, bool force = false) {
    if (!force && (currentMillis - lastCheckMillis < sampleInterval)) return;
    lastCheckMillis = currentMillis;

    int raw = readFilteredRaw();

    // Map 0..1023 analog reading to 0..255 global luminosity scale
    uint8_t newBrightness = (uint8_t)(((uint32_t)raw * 255UL) / 1023UL);
    if (newBrightness != currentBrightness) {
      currentBrightness = newBrightness;
      FastLED.setBrightness(currentBrightness);
      FastLED.show(); // Immediate visual feedback when turning brightness knob
    }
  }

  uint8_t getBrightness() const {
    return currentBrightness;
  }

  int getRawValue() const {
    return filteredRaw;
  }
};

// ============================================================================
// ColorController Class (Analog Pin A3)
// Encapsulates color potentiometer sampling, anti-flicker filtering, and
// definition/calculation of the current base-color across the 3 dial zones:
//   -   0° to 180° (ADC   0 .. 511): Full chromatic RGB color selection.
//   - 180° to 270° (ADC 512 .. 767): Nice-red, nice-blue, and nice-purple.
//   - 270° to 360° (ADC 768 .. 1023): Cold-white to warm-white transition.
// ============================================================================
class ColorController {
private:
  uint8_t pin;
  unsigned long sampleInterval;
  int filteredRaw;
  int lastRaw;
  CRGB currentBaseColor;
  unsigned long lastCheckMillis;

  // 4-sample average with 2-count deadband and EMA smoothing to eliminate noise/jitter
  int readFilteredRaw() {
    int sum = 0;
    for (uint8_t i = 0; i < 4; i++) {
      sum += analogRead(pin);
    }
    int raw = sum >> 2;

    if (lastRaw < 0) {
      lastRaw = raw;
      filteredRaw = raw;
    } else {
      // 2-count deadband to absorb potentiometer wiper electrical noise
      if (abs(raw - lastRaw) > 2) {
        lastRaw = raw;
      }
      // Smooth exponential moving average (EMA)
      filteredRaw = (filteredRaw * 3 + lastRaw) / 4;
    }
    return filteredRaw;
  }

public:
  ColorController(uint8_t analogPin, unsigned long sampleIntervalMs = 20)
    : pin(analogPin),
      sampleInterval(sampleIntervalMs),
      filteredRaw(-1),
      lastRaw(-1),
      currentBaseColor(CRGB(255, 145, 35)), // Default warm white
      lastCheckMillis(0) {}

  void begin() {
    pinMode(pin, INPUT);
  }

  void reset() {
    lastRaw = -1;
  }

  // Calculate base color from potentiometer rotation angle (0° to 360° / ADC 0..1023)
  CRGB calculateBaseColor(int rawAdc) const {
    // ------------------------------------------------------------------------
    // Zone 1: 0° to 180° (ADC 0 .. 511) -> Full RGB Color Selection
    // 16-bit hue interpolation across full 360° rainbow with sub-step blending
    // ------------------------------------------------------------------------
    if (rawAdc <= 511) {
      uint16_t hue16 = (uint16_t)(((uint32_t)rawAdc * 65535UL) / 511UL);
      uint8_t h1 = (uint8_t)(hue16 >> 8);
      uint8_t frac = (uint8_t)(hue16 & 0xFF);
      CRGB c1, c2;
      hsv2rgb_rainbow(CHSV(h1, 255, 255), c1);
      hsv2rgb_rainbow(CHSV((uint8_t)(h1 + 1), 255, 255), c2);
      return blend(c1, c2, frac);
    }
    // ------------------------------------------------------------------------
    // Zone 2: 180° to 270° (ADC 512 .. 767) -> nice-red, nice-blue, nice-purple
    // Continuous blend through curated luxury tones
    // ------------------------------------------------------------------------
    else if (rawAdc <= 767) {
      const CRGB NICE_RED(255, 20, 30);     // Warm vibrant ruby red
      const CRGB NICE_BLUE(10, 95, 255);    // Luminous royal sapphire blue
      const CRGB NICE_PURPLE(175, 25, 240); // Deep royal amethyst purple

      int offset = rawAdc - 512; // 0 .. 255
      // 180° to 225° (ADC 512 .. 639): nice-red to nice-blue
      if (offset < 128) {
        uint8_t frac = (uint8_t)(((uint16_t)offset * 255U) / 127U);
        return blend(NICE_RED, NICE_BLUE, frac);
      }
      // 225° to 270° (ADC 640 .. 767): nice-blue to nice-purple
      else {
        uint8_t frac = (uint8_t)(((uint16_t)(offset - 128) * 255U) / 127U);
        return blend(NICE_BLUE, NICE_PURPLE, frac);
      }
    }
    // ------------------------------------------------------------------------
    // Zone 3: 270° to 360° (ADC 768 .. 1023) -> cold-white to warm-white
    // Continuous CCT correlated color temperature transition
    // ------------------------------------------------------------------------
    else {
      const CRGB COLD_WHITE(225, 240, 255); // Crisp daylight cold white (~6500K)
      const CRGB WARM_WHITE(255, 145, 35);  // Cozy incandescent warm white (~2700K)

      int offset = rawAdc - 768; // 0 .. 255
      uint8_t frac = (uint8_t)offset;
      return blend(COLD_WHITE, WARM_WHITE, frac);
    }
  }

  void update(unsigned long currentMillis, bool force = false) {
    if (!force && (currentMillis - lastCheckMillis < sampleInterval)) return;
    lastCheckMillis = currentMillis;

    int raw = readFilteredRaw();
    currentBaseColor = calculateBaseColor(raw);
  }

  CRGB getBaseColor() const {
    return currentBaseColor;
  }

  int getRawValue() const {
    return filteredRaw;
  }
};

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
// Breathing pulsation using the base color selected by the ColorController (Pin A3):
//   -   0° to 180° (ADC   0 .. 511): Full chromatic RGB color selection.
//   - 180° to 270° (ADC 512 .. 767): Nice-red, nice-blue, and nice-purple.
//   - 270° to 360° (ADC 768 .. 1023): Cold-white to warm-white transition.
// ============================================================================
class NormalState : public LightState {
private:
  ColorController* colorController;
  unsigned long lastUpdateMillis = 0;
  const unsigned long UPDATE_INTERVAL = 25; // 40 FPS smooth breathing
  const unsigned long PULSE_PERIOD = 4500;   // 4.5 seconds full breath cycle

  // Subtle pulsing range: sways between 45% and 85% of brightness
  const float MIN_FACTOR = 0.45f;
  const float MAX_FACTOR = 0.85f;

public:
  NormalState(ColorController* colorCtrl = nullptr)
    : colorController(colorCtrl), lastUpdateMillis(0) {}

  void setColorController(ColorController* colorCtrl) {
    colorController = colorCtrl;
  }

  StateId getId() const override { return STATE_NORMAL; }
  const char* getName() const override { return "NORMAL (Color Pot Pulse)"; }

  void enter(LightContext* ctx) override {
    lastUpdateMillis = 0;
    if (colorController != nullptr) {
      colorController->reset();
      colorController->update(millis(), true);
    }
  }

  void update(LightContext* ctx, unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // 1. Update and retrieve active base color from ColorController
    if (colorController != nullptr) {
      colorController->update(currentMillis);
    }
    CRGB baseColor = (colorController != nullptr) ? colorController->getBaseColor() : CRGB(255, 145, 35);

    // 2. Calculate low-frequency sine wave modulation (0.0 to 1.0)
    float phase = (float)(currentMillis % PULSE_PERIOD) / (float)PULSE_PERIOD;
    float wave = (sin(phase * 2.0f * PI) + 1.0f) * 0.5f;

    // 3. Scale smoothly within subtle range
    float factor = MIN_FACTOR + wave * (MAX_FACTOR - MIN_FACTOR);

    CRGB pulseColor(
      (uint8_t)(baseColor.r * factor),
      (uint8_t)(baseColor.g * factor),
      (uint8_t)(baseColor.b * factor)
    );

    fill_solid(leds, NUM_LEDS, pulseColor);
    FastLED.show();
  }
};

// ============================================================================
// State 2: CONCERT
// Colors smoothly change through rainbow colors.
// Luminosity is governed by the Brightness controller on Pin A2 (color pot ignored).
// ============================================================================
class ConcertState : public LightState {
private:
  unsigned long lastUpdateMillis = 0;
  const unsigned long UPDATE_INTERVAL = 20; // 50 FPS smooth rainbow transition
  uint16_t hueAccumulator = 0;

public:
  StateId getId() const override { return STATE_CONCERT; }
  const char* getName() const override { return "CONCERT (Smooth Rainbow)"; }

  void enter(LightContext* ctx) override {
    lastUpdateMillis = 0;
  }

  void update(LightContext* ctx, unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // Advance rainbow hue counter smoothly: full 360-degree rotation every ~7 seconds
    hueAccumulator += 187;
    uint8_t baseHue = (uint8_t)(hueAccumulator >> 8);

    for (int i = 0; i < NUM_LEDS; i++) {
      // Flow rainbow across all LEDs
      uint8_t pixelHue = baseHue + (uint8_t)((uint32_t)i * 256UL / NUM_LEDS);
      leds[i] = CHSV(pixelHue, 255, 255);
    }
    FastLED.show();
  }
};

// ============================================================================
// State 3: PARTY
// Color of each LED is individual and determined by random.
// Luminosity is governed by the Brightness controller on Pin A2 (color pot ignored).
// ============================================================================
class PartyState : public LightState {
private:
  unsigned long lastUpdateMillis = 0;
  const unsigned long UPDATE_INTERVAL = 90; // High-energy party beat interval (90 ms)
  const uint8_t LEDS_PER_BEAT = 15;         // Number of random LEDs refreshed per beat

  // Helper to generate a vibrant random saturated color
  CRGB getRandomVibrantColor() {
    return CHSV(random8(), 255, 255);
  }

public:
  StateId getId() const override { return STATE_PARTY; }
  const char* getName() const override { return "PARTY (Random Colors)"; }

  void enter(LightContext* ctx) override {
    // Immediately assign every LED its own individual random color
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = getRandomVibrantColor();
    }
    FastLED.show();
    lastUpdateMillis = millis();
  }

  void update(LightContext* ctx, unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // Pick a burst of random LEDs and assign them new random vivid colors
    for (int k = 0; k < LEDS_PER_BEAT; k++) {
      int randomPixel = random(0, NUM_LEDS);
      leds[randomPixel] = getRandomVibrantColor();
    }
    FastLED.show();
  }
};

// ============================================================================
// State Instances & Context
// ============================================================================
BrightnessController brightnessController(PIN_BRIGHTNESS_POT);
ColorController      colorController(PIN_COLOR_POT);
LightContext         systemContext;
NormalState          normalState(&colorController);
ConcertState         concertState;
PartyState           partyState;

// Optional wrapper for backward compatibility
inline void updateBrightness(unsigned long currentMillis, bool force = false) {
  brightnessController.update(currentMillis, force);
}

// ============================================================================
// 3-State Switch Reader & Debouncing
// ============================================================================
StateId readSwitchState() {
  int leftVal  = digitalRead(SWITCH_PIN_LEFT);
  int rightVal = digitalRead(SWITCH_PIN_RIGHT);

  if (leftVal == LOW) {
    return STATE_NORMAL;
  } else if (rightVal == LOW) {
    return STATE_PARTY;
  } else {
    return STATE_CONCERT;
  }
}

StateId currentSwitchState = STATE_NORMAL;
StateId lastReading        = STATE_NORMAL;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50; // 50 ms debounce delay

void applyState(StateId id) {
  switch (id) {
    case STATE_CONCERT: systemContext.transitionTo(&concertState); break;
    case STATE_PARTY:   systemContext.transitionTo(&partyState);   break;
    default:            systemContext.transitionTo(&normalState);  break;
  }
}

// ============================================================================
// Arduino setup() and loop()
// ============================================================================
void setup() {
  // Configure input pins for 3-state toggle switch with internal pull-ups
  pinMode(SWITCH_PIN_LEFT, INPUT_PULLUP);
  pinMode(SWITCH_PIN_RIGHT, INPUT_PULLUP);

  // Initialize hardware controllers
  brightnessController.begin();
  colorController.begin();

  Serial.begin(115200);
  randomSeed(analogRead(A0));

  // Initialize FastLED for SK6812 LED strip (GRB color order)
  FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS);
  // Disable temporal dithering to eliminate high-frequency flicker
  FastLED.setDither(0);

  // Read initial values from potentiometers
  brightnessController.update(millis(), true);
  colorController.update(millis(), true);

  FastLED.clear();
  FastLED.show();

  // Initial read of 3-state switch position
  currentSwitchState = readSwitchState();
  lastReading        = currentSwitchState;
  lastDebounceTime   = millis();

  applyState(currentSwitchState);

  Serial.println(F("Plattenregal FastLED SK6812 Controller Initialized."));
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Debounced 3-state switch reading
  StateId reading = readSwitchState();

  if (reading != lastReading) {
    lastDebounceTime = currentMillis;
    lastReading = reading;
  }

  if ((currentMillis - lastDebounceTime) >= DEBOUNCE_DELAY) {
    if (reading != currentSwitchState) {
      currentSwitchState = reading;
      applyState(currentSwitchState);
    }
  }

  // 2. Read global brightness potentiometer and update FastLED luminosity
  brightnessController.update(currentMillis);

  // 3. Non-blocking update of active lighting state
  systemContext.update(currentMillis);
}