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
 *        Synchronized planar wave moving through the 2.0m x 0.5m rectangle.
 *        Color and wave on Edge 0 (Bottom) and Edge 2 (Top) are matched on
 *        the LEDs vis-a-vis each other across the frame.
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
// 2. State Machine Enums & Forward Declarations
// Must be defined before any function definitions to ensure Arduino auto-generated
// prototypes find ModeId in scope.
// ============================================================================
enum ModeId : uint8_t {
  MODE_NORMAL_PULSING_WARM_WHITE = 0,
  MODE_CONCERT_SMOOTH_RGB_BREATH = 1,
  MODE_PARTY_METEOR_SPARKLE      = 2
};

// Explicit Forward Declarations
uint8_t getLedX(uint16_t index);
void applyMode(ModeId mode);
ModeId readSelectorHardware(ModeId currentMode);

// ============================================================================
// Geometry Helper: Map any strip index (0..299) to X-coordinate (0..119)
// Edge 0 (Bottom): 0 .. 119 -> X = 0 .. 119
// Edge 1 (Right): 120 .. 149 -> X = 119
// Edge 2 (Top):   150 .. 269 -> X = 119 .. 0 (269 - i; vis-a-vis matched to Edge 0)
// Edge 3 (Left):  270 .. 299 -> X = 0
// ============================================================================
inline uint8_t getLedX(uint16_t index) {
  if (index < LEDS_SIDE_BOTTOM) {
    // Edge 0: Bottom (Indices 0 .. 119) -> X = 0 .. 119
    return (uint8_t)index;
  } else if (index < LEDS_SIDE_BOTTOM + LEDS_SIDE_RIGHT) {
    // Edge 1: Right (Indices 120 .. 149) -> X = 119
    return (uint8_t)(LEDS_SIDE_BOTTOM - 1);
  } else if (index < LEDS_SIDE_BOTTOM + LEDS_SIDE_RIGHT + LEDS_SIDE_TOP) {
    // Edge 2: Top (Indices 150 .. 269) -> X = 119 .. 0
    const uint16_t topEndIndex = LEDS_SIDE_BOTTOM + LEDS_SIDE_RIGHT + LEDS_SIDE_TOP - 1; // 269
    return (uint8_t)(topEndIndex - index);
  } else {
    // Edge 3: Left (Indices 270 .. 299) -> X = 0
    return 0;
  }
}

// ============================================================================
// 3. Hardware Selector Configuration (3-Step Switch Knob)
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
// 4. State Machine Base Class
// ============================================================================
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
// Synchronized, slow planar wave moving gracefully through the 2.0m x 0.5m
// rectangle plane. Features a 300-second (5-minute) full RGB chromatic rotation
// with sub-hue linear blending (blend) and 16-bit sinusoidal wave interpolation
// (sin16) for super smooth, continuous color drift and wave motion.
// Edge 0 (Bottom) and Edge 2 (Top) are perfectly matched vis-a-vis across the frame.
// ============================================================================
class ConcertSmoothRGBBreathState : public LightState {
private:
  unsigned long lastUpdateMillis = 0;
  const unsigned long UPDATE_INTERVAL = 25; // 40 FPS rock-solid update interval

  // Ultra-slow, serene timing constants
  const unsigned long COLOR_ROTATION_PERIOD_MS = 300000UL; // 300 seconds (5 min) full RGB rotation
  const unsigned long WAVE_PERIOD_MS           = 10000UL;  // 10.0 seconds to glide across 2.0m plane
  const unsigned long BREATH_PERIOD_MS         = 8000UL;   // 8.0 seconds gentle respiratory breath cycle

public:
  ModeId getId() const override { return MODE_CONCERT_SMOOTH_RGB_BREATH; }
  const char* getName() const override { return "ConcertSmoothRGBBreath"; }

  void enter(LightContext* ctx) override {
    // Disable temporal dithering to prevent high-frequency shimmer/flicker
    FastLED.setDither(0);
    // Set a steady, calibrated brightness ceiling (prevents voltage sag & power-limiter hunting)
    FastLED.setBrightness(180);
    lastUpdateMillis = 0;
  }

  void update(LightContext* ctx, unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // 1. Full 360-degree color rotation over 300 seconds (16-bit precision: 0..65535)
    uint32_t colorTime = currentMillis % COLOR_ROTATION_PERIOD_MS;
    uint16_t globalHue16 = (uint16_t)((colorTime * 65536ULL) / COLOR_ROTATION_PERIOD_MS);

    // 2. Slow planar wave progress (16-bit precision: 0..65535 over 10.0 seconds)
    uint32_t waveTime = currentMillis % WAVE_PERIOD_MS;
    uint16_t waveProgress16 = (uint16_t)((waveTime * 65536ULL) / WAVE_PERIOD_MS);

    // 3. Gentle overarching breath envelope (8.0 seconds cycle, 16-bit sine: sin16)
    uint32_t breathTime = currentMillis % BREATH_PERIOD_MS;
    uint16_t breathAngle16 = (uint16_t)((breathTime * 65536ULL) / BREATH_PERIOD_MS);
    uint16_t breathSine16 = (uint16_t)(sin16(breathAngle16) + 32768); // 0 .. 65535
    // Modulate breath smoothly between 80% and 100% (scale factor 204 .. 255)
    uint8_t breathScale = 204 + (uint8_t)(((uint32_t)breathSine16 * 51UL) >> 16);

    const uint16_t topEndIndex = LEDS_SIDE_BOTTOM + LEDS_SIDE_RIGHT + LEDS_SIDE_TOP - 1; // 269

    // 4. Render 120 positions along the 2.0m width (mirrored on Edge 0 and Edge 2)
    for (uint16_t x = 0; x < LEDS_SIDE_BOTTOM; x++) {
      uint16_t spatialPhase16 = (uint16_t)(((uint32_t)x * 65536UL) / LEDS_SIDE_BOTTOM);

      // Planar traveling wave along X-axis: 16-bit sin16 for super smooth brightness
      uint16_t waveAngle16 = waveProgress16 - spatialPhase16;
      uint16_t waveVal16 = (uint16_t)(sin16(waveAngle16) + 32768); // 0 .. 65535

      // Modulate local brightness smoothly between 70 (~27%) and 240 (~94%)
      uint8_t localBrightness = 70 + (uint8_t)(((uint32_t)waveVal16 * 170UL) >> 16);
      uint8_t finalBrightness = scale8(localBrightness, breathScale);

      // Harmonious spatial color offset across the 2.0m width (subtle 180° spread)
      uint16_t spatialHue16 = (uint16_t)(((uint32_t)x * 32768UL) / LEDS_SIDE_BOTTOM);
      uint16_t ledHue16 = globalHue16 - spatialHue16;

      // Continuous 16-bit color lookup with native FastLED linear interpolation
      // and scale8_video brightness scaling (completely artifact-free)
      CRGB pixelColor = ColorFromPaletteExtended(RainbowColors_p, ledHue16, finalBrightness, LINEARBLEND);

      // Apply to Edge 0 (Bottom) and Edge 2 (Top) vis-a-vis
      leds[x] = pixelColor;
      leds[topEndIndex - x] = pixelColor;
    }

    // 5. Fill Edge 1 (Right vertical) to match rightmost LED seamlessly
    fill_solid(&leds[LEDS_SIDE_BOTTOM], LEDS_SIDE_RIGHT, leds[LEDS_SIDE_BOTTOM - 1]);

    // 6. Fill Edge 3 (Left vertical) to match leftmost LED seamlessly
    fill_solid(&leds[NUM_LEDS - LEDS_SIDE_LEFT], LEDS_SIDE_LEFT, leds[0]);

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
  return MODE_CONCERT_SMOOTH_RGB_BREATH;

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
  // Disable temporal dithering to eliminate high-frequency 25-50 Hz flicker
  FastLED.setDither(0);
  // Set fixed brightness ceiling (prevents power supply brownout and dynamic limiter hunting)
  FastLED.setBrightness(180);
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

