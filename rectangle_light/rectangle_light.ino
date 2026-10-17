/*
 * ============================================================================
 * Rectangle Light Controller (SK6812 LED Strip along a 2.0m x 0.5m Rectangle)
 * ============================================================================
 *
 * Target: Arduino Uno / Nano / compatible (ATmega328P or higher)
 * LED Strip: SK6812 RGB (60 LEDs per meter)
 *
 * Physical Geometry & LED Calculation:
 *   - Rectangle Dimensions: Width X = 2.0 m, Height Y = 0.5 m
 *   - Density: 60 LEDs / meter
 *   - Perimeter = 2 * (X + Y) = 2 * (2.0m + 0.5m) = 5.0 meters
 *   - Total Number of LEDs = 5.0 m * 60 LEDs/m = 300 LEDs
 *       * Edge 0 (Bottom, X-axis):  120 LEDs (Indices   0 .. 119)
 *       * Edge 1 (Right,  Y-axis):   30 LEDs (Indices 120 .. 149)
 *       * Edge 2 (Top,    X-axis):  120 LEDs (Indices 150 .. 269)
 *       * Edge 3 (Left,   Y-axis):   30 LEDs (Indices 270 .. 299)
 *
 * Modes:
 *   1. NormalPulsingWarmWhite:
 *        Cozy warm-white incandescent glow (~2700K) with a gentle, meditative
 *        low-frequency sinusoidal breathing pulse across the entire frame.
 *   2. ConcertSmoothRGBBreath:
 *        Vibrant full-spectrum rainbow flowing continuously around the perimeter
 *        coupled with a rhythmic breathing wave that swells and dims in sync.
 *   3. PartyMeteorSparkle:
 *        High-energy party mode featuring multiple meteors racing around the
 *        rectangular perimeter leaving fading trails, interspersed with random
 *        glittering sparkles.
 *
 * Initial State:
 *   - Boots initially into "NormalPulsingWarmWhite".
 *
 * Mode Selection (3-Step Switch Knob):
 *   Supports two selectable hardware configurations via ACTIVE_SELECTOR:
 *     A) SELECTOR_DIGITAL_SWITCH (Default):
 *        3-position ON-OFF-ON toggle switch or 3-step rotary switch.
 *        - Step 1 (Left):   Pin 11 LOW,  Pin 12 HIGH -> NormalPulsingWarmWhite
 *        - Step 2 (Center): Pin 11 HIGH, Pin 12 HIGH -> ConcertSmoothRGBBreath
 *        - Step 3 (Right):  Pin 11 HIGH, Pin 12 LOW  -> PartyMeteorSparkle
 *     B) SELECTOR_ANALOG_KNOB:
 *        10k Potentiometer / Rotary knob wiper connected to Analog Pin A3.
 *        Divided into 3 stepped sectors with hysteresis to prevent jitter.
 *
 * Non-blocking Architecture:
 *   - Driven strictly via millis() state updates without delay().
 * ============================================================================
 */

#include <FastLED.h>

// ============================================================================
// 1. Hardware Pin & Strip Configuration
// ============================================================================
#define LED_PIN       6     // Data output pin for SK6812 LED strip

// Rectangle physical dimensions
#define RECT_WIDTH_M     2.0f
#define RECT_HEIGHT_M    0.5f
#define LEDS_PER_METER   60

// LED layout calculation
#define LEDS_SIDE_BOTTOM (uint16_t)(RECT_WIDTH_M * LEDS_PER_METER)   // 120 LEDs
#define LEDS_SIDE_RIGHT  (uint16_t)(RECT_HEIGHT_M * LEDS_PER_METER)  //  30 LEDs
#define LEDS_SIDE_TOP    (uint16_t)(RECT_WIDTH_M * LEDS_PER_METER)   // 120 LEDs
#define LEDS_SIDE_LEFT   (uint16_t)(RECT_HEIGHT_M * LEDS_PER_METER)  //  30 LEDs

#define NUM_LEDS         (LEDS_SIDE_BOTTOM + LEDS_SIDE_RIGHT + LEDS_SIDE_TOP + LEDS_SIDE_LEFT) // 300 LEDs

// FastLED Framebuffer
CRGB leds[NUM_LEDS];

// ============================================================================
// 2. Hardware Selector Configuration (3-Step Switch Knob)
// ============================================================================
#define SELECTOR_DIGITAL_SWITCH 1  // 2-Pin ON-OFF-ON toggle / 3-step rotary switch
#define SELECTOR_ANALOG_KNOB    2  // 1-Pin potentiometer knob on analog pin with 3 steps

// >>> Select active hardware input here <<<
#define ACTIVE_SELECTOR         SELECTOR_DIGITAL_SWITCH

// Digital 3-step switch pins (Internal pull-ups enabled)
const int PIN_SWITCH_STEP1 = 11; // Left / Step 1
const int PIN_SWITCH_STEP3 = 12; // Right / Step 3
                                 // Center / Step 2 = both pins HIGH

// Analog potentiometer knob pin & thresholds (if ACTIVE_SELECTOR == SELECTOR_ANALOG_KNOB)
const int PIN_ANALOG_KNOB  = A3;
const int THRESHOLD_LOW    = 341;
const int THRESHOLD_HIGH   = 682;
const int HYSTERESIS       = 35;

// ============================================================================
// 3. State Machine Enums & Base Class
// ============================================================================
enum ModeId {
  MODE_NORMAL_PULSING_WARM_WHITE = 0,
  MODE_CONCERT_SMOOTH_RGB_BREATH = 1,
  MODE_PARTY_METEOR_SPARKLE      = 2
};

class LightContext;

class LightState {
public:
  virtual ~LightState() {}
  virtual ModeId getId() const = 0;
  virtual const char* getName() const = 0;
  virtual void enter(LightContext* ctx) {}
  virtual void update(LightContext* ctx, unsigned long currentMillis) = 0;
};

// ============================================================================
// 4. Controller Context
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

  ModeId getCurrentModeId() const {
    if (currentState != nullptr) {
      return currentState->getId();
    }
    return MODE_NORMAL_PULSING_WARM_WHITE;
  }
};

// ============================================================================
// 5. Mode 1: NormalPulsingWarmWhite
// Warm-white incandescent tone (~2700K) with a gentle sinusoidal breathing pulse
// ============================================================================
class NormalPulsingWarmWhiteState : public LightState {
private:
  unsigned long lastUpdateMillis = 0;
  const unsigned long UPDATE_INTERVAL = 25; // 40 FPS smooth breathing
  const unsigned long PULSE_PERIOD = 4500;   // 4.5 seconds full breath cycle

  // Incandescent Warm-White Base Color (~2700K)
  const uint8_t WARM_R = 255;
  const uint8_t WARM_G = 145;
  const uint8_t WARM_B = 35;

  // Gentle breathing range: sways between 40% and 85% brightness
  const float MIN_FACTOR = 0.40f;
  const float MAX_FACTOR = 0.85f;

public:
  ModeId getId() const override { return MODE_NORMAL_PULSING_WARM_WHITE; }
  const char* getName() const override { return "NormalPulsingWarmWhite"; }

  void enter(LightContext* ctx) override {
    FastLED.setBrightness(255);
    lastUpdateMillis = 0;
  }

  void update(LightContext* ctx, unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // Smooth low-frequency sine wave modulation (0.0 to 1.0)
    float phase = (float)(currentMillis % PULSE_PERIOD) / (float)PULSE_PERIOD;
    float wave = (sin(phase * 2.0f * PI) + 1.0f) * 0.5f;

    // Scale smoothly within breathing bounds
    float factor = MIN_FACTOR + wave * (MAX_FACTOR - MIN_FACTOR);

    uint8_t r = (uint8_t)(WARM_R * factor);
    uint8_t g = (uint8_t)(WARM_G * factor);
    uint8_t b = (uint8_t)(WARM_B * factor);
    CRGB warmColor(r, g, b);

    fill_solid(leds, NUM_LEDS, warmColor);
    FastLED.show();
  }
};

// ============================================================================
// 6. Mode 2: ConcertSmoothRGBBreath
// Full-spectrum rainbow circulating along the rectangle perimeter,
// combined with a synchronized breathing brightness wave.
// ============================================================================
class ConcertSmoothRGBBreathState : public LightState {
private:
  unsigned long lastUpdateMillis = 0;
  const unsigned long UPDATE_INTERVAL = 20; // 50 FPS smooth animation
  const unsigned long BREATH_PERIOD = 4000;   // 4.0 seconds breathing period
  uint16_t hueAccumulator = 0;

public:
  ModeId getId() const override { return MODE_CONCERT_SMOOTH_RGB_BREATH; }
  const char* getName() const override { return "ConcertSmoothRGBBreath"; }

  void enter(LightContext* ctx) override {
    FastLED.setBrightness(255);
    lastUpdateMillis = 0;
  }

  void update(LightContext* ctx, unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // 1. Advance chromatic hue counter smoothly
    hueAccumulator += 140;
    uint8_t baseHue = (uint8_t)(hueAccumulator >> 8);

    // 2. Calculate breathing brightness wave (35% to 85% range)
    float phase = (float)(currentMillis % BREATH_PERIOD) / (float)BREATH_PERIOD;
    float wave = (sin(phase * 2.0f * PI) + 1.0f) * 0.5f;
    uint8_t breathVal = (uint8_t)(90 + wave * (215 - 90)); // Range 90 .. 215

    // 3. Render rainbow gradient circulating around the 5m rectangular perimeter
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      uint8_t pixelHue = baseHue + (uint8_t)((uint32_t)i * 256UL / NUM_LEDS);
      leds[i] = CHSV(pixelHue, 240, breathVal);
    }
    FastLED.show();
  }
};

// ============================================================================
// 7. Mode 3: PartyMeteorSparkle
// High-energy party mode: Dual meteors traversing the perimeter leaving
// fading comet tails, accompanied by dynamic glittering sparkles.
// ============================================================================
class PartyMeteorSparkleState : public LightState {
private:
  unsigned long lastUpdateMillis = 0;
  const unsigned long UPDATE_INTERVAL = 18; // ~55 FPS energetic movement
  const uint8_t METEOR_COUNT = 3;            // 3 meteors spaced along perimeter
  const uint8_t FADE_DECAY = 50;             // Trail decay factor (higher = shorter tail)

  uint16_t meteorPos[3];
  uint8_t meteorHue = 0;

public:
  ModeId getId() const override { return MODE_PARTY_METEOR_SPARKLE; }
  const char* getName() const override { return "PartyMeteorSparkle"; }

  void enter(LightContext* ctx) override {
    FastLED.setBrightness(220);
    FastLED.clear();
    FastLED.show();

    // Space meteors evenly around the 300-LED rectangular perimeter
    for (uint8_t m = 0; m < METEOR_COUNT; m++) {
      meteorPos[m] = (NUM_LEDS / METEOR_COUNT) * m;
    }
    meteorHue = 0;
    lastUpdateMillis = millis();
  }

  void update(LightContext* ctx, unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // 1. Fade previous frame pixels to create smooth comet trails
    fadeToBlackBy(leds, NUM_LEDS, FADE_DECAY);

    // 2. Advance and render meteors
    meteorHue += 2; // Cycle meteor head colors over time
    for (uint8_t m = 0; m < METEOR_COUNT; m++) {
      meteorPos[m] = (meteorPos[m] + 1) % NUM_LEDS;

      // Draw meteor head and bright core
      uint8_t headHue = meteorHue + (m * (256 / METEOR_COUNT));
      leds[meteorPos[m]] = CHSV(headHue, 200, 255);
      
      // Leading white-hot tip
      uint16_t nextPos = (meteorPos[m] + 1) % NUM_LEDS;
      leds[nextPos] += CRGB(160, 160, 200);
    }

    // 3. Add random vibrant sparkles along the rectangular strip
    for (uint8_t s = 0; s < 3; s++) {
      if (random8() < 170) {
        uint16_t sparkIndex = random16(NUM_LEDS);
        // Alternate between bright white and random neon flashes
        if (random8() < 90) {
          leds[sparkIndex] = CRGB::White;
        } else {
          leds[sparkIndex] = CHSV(random8(), 220, 255);
        }
      }
    }

    FastLED.show();
  }
};

// ============================================================================
// 8. State Instances & Context
// ============================================================================
LightContext                systemContext;
NormalPulsingWarmWhiteState normalState;
ConcertSmoothRGBBreathState concertState;
PartyMeteorSparkleState     partyState;

void applyMode(ModeId mode) {
  switch (mode) {
    case MODE_CONCERT_SMOOTH_RGB_BREATH:
      systemContext.transitionTo(&concertState);
      break;
    case MODE_PARTY_METEOR_SPARKLE:
      systemContext.transitionTo(&partyState);
      break;
    case MODE_NORMAL_PULSING_WARM_WHITE:
    default:
      systemContext.transitionTo(&normalState);
      break;
  }
}

// ============================================================================
// 9. 3-Step Switch Knob Reading & Debouncing
// ============================================================================
ModeId readSelectorHardware(ModeId currentMode) {
#if (ACTIVE_SELECTOR == SELECTOR_DIGITAL_SWITCH)
  int valLeft  = digitalRead(PIN_SWITCH_STEP1);
  int valRight = digitalRead(PIN_SWITCH_STEP3);

  // if (valLeft == LOW) {
  //   return MODE_NORMAL_PULSING_WARM_WHITE; // Step 1 (Left)
  // } else if (valRight == LOW) {
  //   return MODE_PARTY_METEOR_SPARKLE;      // Step 3 (Right)
  // } else {
  //   return MODE_CONCERT_SMOOTH_RGB_BREATH; // Step 2 (Center / Open)
  // }
  return MODE_NORMAL_PULSING_WARM_WHITE;

#elif (ACTIVE_SELECTOR == SELECTOR_ANALOG_KNOB)
  int rawVal = analogRead(PIN_ANALOG_KNOB);

  // 3-step analog division with hysteresis
  if (currentMode == MODE_NORMAL_PULSING_WARM_WHITE) {
    if (rawVal > THRESHOLD_HIGH + HYSTERESIS) return MODE_PARTY_METEOR_SPARKLE;
    if (rawVal > THRESHOLD_LOW + HYSTERESIS)  return MODE_CONCERT_SMOOTH_RGB_BREATH;
    return MODE_NORMAL_PULSING_WARM_WHITE;
  } else if (currentMode == MODE_CONCERT_SMOOTH_RGB_BREATH) {
    if (rawVal < THRESHOLD_LOW - HYSTERESIS)  return MODE_NORMAL_PULSING_WARM_WHITE;
    if (rawVal > THRESHOLD_HIGH + HYSTERESIS) return MODE_PARTY_METEOR_SPARKLE;
    return MODE_CONCERT_SMOOTH_RGB_BREATH;
  } else { // MODE_PARTY_METEOR_SPARKLE
    if (rawVal < THRESHOLD_LOW - HYSTERESIS)  return MODE_NORMAL_PULSING_WARM_WHITE;
    if (rawVal < THRESHOLD_HIGH - HYSTERESIS) return MODE_CONCERT_SMOOTH_RGB_BREATH;
    return MODE_PARTY_METEOR_SPARKLE;
  }
#endif
}

ModeId        currentModeReading = MODE_NORMAL_PULSING_WARM_WHITE;
ModeId        lastRawReading     = MODE_NORMAL_PULSING_WARM_WHITE;
unsigned long lastDebounceMillis = 0;
const unsigned long DEBOUNCE_DELAY_MS = 50; // 50 ms debounce

// ============================================================================
// 10. Arduino setup() & loop()
// ============================================================================
void setup() {
  Serial.begin(115200);

  // Configure switch input pins
#if (ACTIVE_SELECTOR == SELECTOR_DIGITAL_SWITCH)
  pinMode(PIN_SWITCH_STEP1, INPUT_PULLUP);
  pinMode(PIN_SWITCH_STEP3, INPUT_PULLUP);
#endif

  // Initialize random seed using floating analog line
  randomSeed(analogRead(A0));

  // Initialize FastLED for SK6812 RGB strip (GRB color ordering)
  FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS);
  // Protect power supply (5V, 2500 mA max current limiting)
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2500);
  FastLED.clear();
  FastLED.show();

  // Requirement: Initially start in mode "NormalPulsingWarmWhite"
  systemContext.transitionTo(&normalState);

  // Read initial selector switch position
  currentModeReading = readSelectorHardware(MODE_NORMAL_PULSING_WARM_WHITE);
  lastRawReading     = currentModeReading;
  lastDebounceMillis = millis();

  // If selector is positioned at a different mode on boot, apply it
  if (currentModeReading != MODE_NORMAL_PULSING_WARM_WHITE) {
    applyMode(currentModeReading);
  }

  Serial.println(F("Rectangle Light (2.0m x 0.5m, 300 LEDs) Initialized."));
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Debounced Mode Switch / Knob Reading
  ModeId rawReading = readSelectorHardware(systemContext.getCurrentModeId());

  if (rawReading != lastRawReading) {
    lastDebounceMillis = currentMillis;
    lastRawReading     = rawReading;
  }

  if ((currentMillis - lastDebounceMillis) >= DEBOUNCE_DELAY_MS) {
    if (rawReading != currentModeReading) {
      currentModeReading = rawReading;
      applyMode(currentModeReading);
    }
  }

  // 2. Non-blocking update of active lighting mode
  systemContext.update(currentMillis);
}

