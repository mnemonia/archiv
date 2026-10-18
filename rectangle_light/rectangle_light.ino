/*
 * ============================================================================
 * Rectangle Light Controller (SK6812 LED Strip along a 2.36m x 0.55m Rectangle)
 * ============================================================================
 *
 * Target: Arduino Uno / Nano / compatible (ATmega328P or higher)
 * LED Strip: SK6812 RGB (60 LEDs per meter)
 *
 * Physical Geometry & LED Calculation:
 *   - Rectangle Dimensions: Width X = 2.36 m, Height Y = 0.55 m
 *   - Density: 60 LEDs / meter
 *   - Perimeter = 2 * (X + Y) = 2 * (2.36m + 0.55m) = 5.82 meters
 *   - Total Number of LEDs = 350 LEDs
 *       * Edge 0 (Bottom, X-axis):  142 LEDs (Indices   0 .. 141)
 *       * Edge 1 (Right,  Y-axis):   33 LEDs (Indices 142 .. 174)
 *       * Edge 2 (Top,    X-axis):  142 LEDs (Indices 175 .. 316)
 *       * Edge 3 (Left,   Y-axis):   33 LEDs (Indices 317 .. 349)
 *
 * Modes:
 *   1. NormalPulsingWarmWhite:
 *        Cozy ambient light with a gentle, meditative low-frequency sinusoidal
 *        breathing pulse across the entire frame. Includes dynamic color selection
 *        via potentiometer on Analog Pin A3 (0° to 360° rotation):
 *          -   0° to 180°: Full 360-degree chromatic RGB color selection.
 *          - 180° to 270°: Curated designer tones (nice-blue, nice-red, nice-purple).
 *          - 270° to 360°: Continuous CCT tunable white (cold-white to warm-white).
 *   2. ConcertSmoothRGBBreath:
 *        Synchronized planar wave moving through the 2.36m x 0.55m rectangle.
 *        Color and wave on Edge 0 (Bottom) and Edge 2 (Top) are matched on
 *        the LEDs vis-a-vis each other across the frame.
 *   3. PartyHUEMeteorSparkle:
 *        High-energy party spectacle with dynamic meteors and starlight sparkles.
 *        Cycles through three HUE background behaviors ("SmoothHUETransition",
 *        "RawPoliceAlarmLights", and "Supernova") stepped through in random order
 *        and random durations. Each meteor is assigned an independent stable
 *        random color with a smooth disappearing tail, unaffected by background behavior.
 *   4. PartyMeteorSparkle (Legacy):
 *        Classic multi-meteor perimeter chase with fading comet tails and sparkles.
 *
 * Initial State:
 *   - Boots initially into "NormalPulsingWarmWhite".
 *
 * Mode Selection (3-Step Digital Switch):
 *   3-position ON-OFF-ON toggle switch or 3-step rotary switch:
 *     - Step 1 (Left):   Pin 11 LOW,  Pin 12 HIGH -> NormalPulsingWarmWhite
 *     - Step 2 (Center): Pin 11 HIGH, Pin 12 HIGH -> ConcertSmoothRGBBreath
 *     - Step 3 (Right):  Pin 11 HIGH, Pin 12 LOW  -> PartyHUEMeteorSparkle
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
#define RECT_WIDTH_M     2.36f
#define RECT_HEIGHT_M    0.55f
#define LEDS_PER_METER   60

// LED layout calculation
#define LEDS_SIDE_BOTTOM (uint16_t)(RECT_WIDTH_M * LEDS_PER_METER + 0.5f)   // 142 LEDs
#define LEDS_SIDE_RIGHT  (uint16_t)(RECT_HEIGHT_M * LEDS_PER_METER + 0.5f)  //  33 LEDs
#define LEDS_SIDE_TOP    (uint16_t)(RECT_WIDTH_M * LEDS_PER_METER + 0.5f)   // 142 LEDs
#define LEDS_SIDE_LEFT   (uint16_t)(RECT_HEIGHT_M * LEDS_PER_METER + 0.5f)  //  33 LEDs

#define NUM_LEDS         (LEDS_SIDE_BOTTOM + LEDS_SIDE_RIGHT + LEDS_SIDE_TOP + LEDS_SIDE_LEFT) // 350 LEDs

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
  MODE_PARTY_HUE_METEOR_SPARKLE  = 2,
  MODE_PARTY_METEOR_SPARKLE      = 3
};

// HUE Background Behaviors for PartyHUEMeteorSparkle
enum HueBehavior : uint8_t {
  HUE_BEHAVIOR_SMOOTH_TRANSITION = 0,
  HUE_BEHAVIOR_POLICE_ALARM      = 1,
  HUE_BEHAVIOR_SUPERNOVA         = 2,
  HUE_BEHAVIOR_COUNT             = 3
};

// Meteor representation with independent stable color and smooth disappearing tail
struct PartyMeteor {
  bool active;
  float position;             // Continuous position along perimeter (0.0 .. NUM_LEDS - 1)
  float speed;                // Speed in LEDs per frame
  CRGB color;                 // Stable random color assigned at launch (unaffected by behavior)
  uint16_t distanceTraveled;  // LED units traversed since launch
  uint16_t maxDistance;       // Full perimeter + tail length
  uint8_t tailLength;         // Tail length in LEDs
};

// Dynamic starlight sparkle representation
struct PartySparkle {
  uint16_t index;             // LED strip index (0 .. NUM_LEDS - 1)
  uint8_t brightness;         // Current brightness (0 = inactive)
  uint8_t decay;              // Fade step per frame
  CRGB color;                 // Sparkle color (white or iridescent)
};

// Explicit Forward Declarations
uint8_t getLedX(uint16_t index);
void applyMode(ModeId mode);
ModeId readSelectorHardware(ModeId currentMode);

// ============================================================================
// Geometry Helper: Map any strip index (0..349) to X-coordinate (0..141)
// Edge 0 (Bottom):   0 .. 141 -> X = 0 .. 141
// Edge 1 (Right):  142 .. 174 -> X = 141
// Edge 2 (Top):    175 .. 316 -> X = 141 .. 0 (316 - i; vis-a-vis matched to Edge 0)
// Edge 3 (Left):   317 .. 349 -> X = 0
// ============================================================================
inline uint8_t getLedX(uint16_t index) {
  if (index < LEDS_SIDE_BOTTOM) {
    // Edge 0: Bottom (Indices 0 .. 141) -> X = 0 .. 141
    return (uint8_t)index;
  } else if (index < LEDS_SIDE_BOTTOM + LEDS_SIDE_RIGHT) {
    // Edge 1: Right (Indices 142 .. 174) -> X = 141
    return (uint8_t)(LEDS_SIDE_BOTTOM - 1);
  } else if (index < LEDS_SIDE_BOTTOM + LEDS_SIDE_RIGHT + LEDS_SIDE_TOP) {
    // Edge 2: Top (Indices 175 .. 316) -> X = 141 .. 0
    const uint16_t topEndIndex = LEDS_SIDE_BOTTOM + LEDS_SIDE_RIGHT + LEDS_SIDE_TOP - 1; // 316
    return (uint8_t)(topEndIndex - index);
  } else {
    // Edge 3: Left (Indices 317 .. 349) -> X = 0
    return 0;
  }
}

// ============================================================================
// 3. Hardware Selector & Potentiometer Configuration
// ============================================================================
// Digital 3-step switch pins (Internal pull-ups enabled)
const int PIN_SWITCH_STEP1 = 11; // Left / Step 1
const int PIN_SWITCH_STEP3 = 12; // Right / Step 3
                                 // Center / Step 2 = both pins HIGH

// Color selection potentiometer input on Analog Pin A3 (used in NormalPulsingWarmWhiteState)
const int PIN_COLOR_POT    = A3;

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
// Ambient lighting with a gentle sinusoidal breathing pulse.
// Incorporates an analog potentiometer on Pin A3 for continuous color selection:
//   -   0° to 180° (ADC   0 .. 511): Full chromatic RGB rainbow spectrum
//   - 180° to 270° (ADC 512 .. 767): Curated designer tones (nice-blue, nice-red, nice-purple)
//   - 270° to 360° (ADC 768 .. 1023): Tunable white (cold-white to warm-white)
// ============================================================================
class NormalPulsingWarmWhiteState : public LightState {
private:
  unsigned long lastUpdateMillis = 0;
  const unsigned long UPDATE_INTERVAL = 25; // 40 FPS smooth breathing
  const unsigned long PULSE_PERIOD = 4500;   // 4.5 seconds full breath cycle

  // Potentiometer reading & smoothing on Pin A3
  int filteredPotRaw = 1023; // Boot defaults to warm-white (1023)
  int lastPotRaw     = -1;

  // Gentle breathing range: sways between 40% and 85% brightness
  const float MIN_FACTOR = 0.40f;
  const float MAX_FACTOR = 0.85f;

  // Stable 4-sample analog read with deadband and EMA filtering to eliminate noise/flicker
  int readPotStable() {
    int sum = 0;
    for (uint8_t i = 0; i < 4; i++) {
      sum += analogRead(PIN_COLOR_POT);
    }
    int raw = sum >> 2;

    if (lastPotRaw < 0) {
      lastPotRaw = raw;
      filteredPotRaw = raw;
    } else {
      // 2-count deadband to absorb standard electrical noise
      if (abs(raw - lastPotRaw) > 2) {
        lastPotRaw = raw;
      }
      // Smooth exponential filter (EMA)
      filteredPotRaw = (filteredPotRaw * 3 + lastPotRaw) / 4;
    }
    return filteredPotRaw;
  }

  // Calculate base color from potentiometer rotation angle (0° to 360°)
  CRGB calculatePotColor(int rawAdc) {
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
    // Zone 2: 180° to 270° (ADC 512 .. 767) -> nice-blue, nice-red, nice-purple
    // Continuous blend through curated luxury tones
    // ------------------------------------------------------------------------
    else if (rawAdc <= 767) {
      const CRGB NICE_BLUE(10, 95, 255);    // Luminous royal sapphire blue
      const CRGB NICE_RED(255, 20, 30);     // Warm vibrant ruby red
      const CRGB NICE_PURPLE(175, 25, 240); // Deep royal amethyst purple

      int offset = rawAdc - 512; // 0 .. 255
      // 180° to 225° (ADC 512 .. 639): nice-blue to nice-red
      if (offset < 128) {
        uint8_t frac = (uint8_t)(((uint16_t)offset * 255U) / 127U);
        return blend(NICE_BLUE, NICE_RED, frac);
      }
      // 225° to 270° (ADC 640 .. 767): nice-red to nice-purple
      else {
        uint8_t frac = (uint8_t)(((uint16_t)(offset - 128) * 255U) / 127U);
        return blend(NICE_RED, NICE_PURPLE, frac);
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

public:
  ModeId getId() const override { return MODE_NORMAL_PULSING_WARM_WHITE; }
  const char* getName() const override { return "NormalPulsingWarmWhite"; }

  void enter(LightContext* ctx) override {
    FastLED.setDither(0);
    FastLED.setBrightness(180);
    lastUpdateMillis = 0;
    lastPotRaw = -1; // Reset to immediately read current physical potentiometer position
  }

  void update(LightContext* ctx, unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // 1. Read potentiometer on A3 with anti-flicker smoothing and calculate base color
    int potVal = readPotStable();
    CRGB baseColor = calculatePotColor(potVal);

    // 2. Smooth low-frequency sine wave modulation (0.0 to 1.0)
    float phase = (float)(currentMillis % PULSE_PERIOD) / (float)PULSE_PERIOD;
    float wave = (sin(phase * 2.0f * PI) + 1.0f) * 0.5f;

    // 3. Scale smoothly within breathing bounds (MIN_FACTOR to MAX_FACTOR)
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
// 6. Mode 2: ConcertSmoothRGBBreath
// Synchronized, slow planar wave moving gracefully through the 2.36m x 0.55m
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
  const unsigned long WAVE_PERIOD_MS           = 10000UL;  // 10.0 seconds to glide across 2.36m plane
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

    const uint16_t topEndIndex = LEDS_SIDE_BOTTOM + LEDS_SIDE_RIGHT + LEDS_SIDE_TOP - 1; // 316

    // 4. Render 142 positions along the 2.36m width (mirrored on Edge 0 and Edge 2)
    for (uint16_t x = 0; x < LEDS_SIDE_BOTTOM; x++) {
      uint16_t spatialPhase16 = (uint16_t)(((uint32_t)x * 65536UL) / LEDS_SIDE_BOTTOM);

      // Planar traveling wave along X-axis: 16-bit sin16 for super smooth brightness
      uint16_t waveAngle16 = waveProgress16 - spatialPhase16;
      uint16_t waveVal16 = (uint16_t)(sin16(waveAngle16) + 32768); // 0 .. 65535

      // Modulate local brightness smoothly between 70 (~27%) and 240 (~94%)
      uint8_t localBrightness = 70 + (uint8_t)(((uint32_t)waveVal16 * 170UL) >> 16);
      uint8_t finalBrightness = scale8(localBrightness, breathScale);

      // Harmonious spatial color offset across the 2.36m width (subtle 180° spread)
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

    // Space meteors evenly around the 350-LED rectangular perimeter
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
// 8. Mode 4: PartyHUEMeteorSparkle
// High-energy party spectacle featuring dynamic meteors, starlight sparkles,
// and three cycling background HUE behaviors ("SmoothHUETransition",
// "RawPoliceAlarmLights", "Supernova") stepped through in random order and
// random durations.
//
// Key Specifications:
//   - Each meteor is assigned an independent, vibrant random color at spawn.
//   - The meteor color stays strictly STABLE from start to end of its traverse.
//   - Each meteor has a smooth disappearing tail.
//   - Meteors are NOT affected by the background behavior (rendered via overlay
//     blending that preserves pure meteor chrominance over any background state).
// ============================================================================
class PartyHUEMeteorSparkleState : public LightState {
private:
  unsigned long lastUpdateMillis = 0;
  const unsigned long UPDATE_INTERVAL = 20; // 50 FPS fluid party rendering

  // HUE Background Behavior State
  HueBehavior currentBehavior;
  unsigned long behaviorStartMillis = 0;
  unsigned long behaviorDurationMs  = 0;

  // Active Meteors (3 concurrent meteors)
  static const uint8_t METEOR_COUNT = 3;
  PartyMeteor meteors[METEOR_COUNT];

  // Dynamic Sparkles (10 concurrent sparkle slots)
  static const uint8_t SPARKLE_COUNT = 10;
  PartySparkle sparkles[SPARKLE_COUNT];

  void spawnMeteor(uint8_t m, bool initialScatter) {
    meteors[m].active = true;
    if (initialScatter) {
      // Stagger initial meteors evenly around the perimeter
      meteors[m].position = (float)((NUM_LEDS / METEOR_COUNT) * m);
    } else {
      // Spawn at a random location along the perimeter
      meteors[m].position = (float)random16(NUM_LEDS);
    }
    // Random speed between 1.3 and 2.5 LEDs per update (~65 to 125 LEDs/s)
    meteors[m].speed = 1.3f + ((float)random8(120) / 100.0f);
    meteors[m].distanceTraveled = 0;
    meteors[m].tailLength = 22 + random8(16); // 22 to 37 LEDs
    meteors[m].maxDistance = NUM_LEDS + meteors[m].tailLength;

    // Requirement: "Use random color for each meteor. Keep color of meteor stable from start to end"
    // Pick an intensely vivid random saturated color (HSV full saturation & value)
    uint8_t randomHue = random8();
    meteors[m].color = CHSV(randomHue, 255, 255);
  }

  void selectNextBehavior(unsigned long currentMillis) {
    // Pick randomly among the OTHER behaviors to guarantee a change
    uint8_t offset = 1 + random8(HUE_BEHAVIOR_COUNT - 1);
    currentBehavior = (HueBehavior)(((uint8_t)currentBehavior + offset) % HUE_BEHAVIOR_COUNT);
    behaviorStartMillis = currentMillis;
    // Random duration: 8 to 22 seconds
    behaviorDurationMs = 8000UL + (unsigned long)random16(14000);

    Serial.print(F("[PartyHUEMeteorSparkle] Switched to HUE Behavior: "));
    Serial.print(getBehaviorName(currentBehavior));
    Serial.print(F(" (Duration: "));
    Serial.print(behaviorDurationMs / 1000);
    Serial.println(F("s)"));
  }

  const char* getBehaviorName(HueBehavior b) const {
    switch (b) {
      case HUE_BEHAVIOR_SMOOTH_TRANSITION: return "SmoothHUETransition";
      case HUE_BEHAVIOR_POLICE_ALARM:      return "RawPoliceAlarmLights";
      case HUE_BEHAVIOR_SUPERNOVA:         return "Supernova";
      default:                             return "Unknown";
    }
  }

  // --------------------------------------------------------------------------
  // HUE Behavior 1: SmoothHUETransition
  // Atmospheric, fluid ambient rainbow gradient gliding along the rectangle.
  // --------------------------------------------------------------------------
  void renderSmoothHUE(unsigned long currentMillis) {
    uint8_t cycleHue = (uint8_t)(currentMillis / 35); // 1 full rotation every ~9 seconds
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      uint8_t x = getLedX(i);
      // Harmonious spatial hue shift along the 2.36m width
      uint8_t pixelHue = cycleHue + (uint8_t)(((uint32_t)x * 128UL) / LEDS_SIDE_BOTTOM);
      // Moderate brightness (~90) so meteors & sparkles strongly stand out
      leds[i] = CHSV(pixelHue, 240, 90);
    }
  }

  // --------------------------------------------------------------------------
  // HUE Behavior 2: RawPoliceAlarmLights
  // Raw, high-energy emergency strobe bar: rapid Red/Blue alternating quad-strobes
  // split across the left and right halves of the rectangular frame.
  // --------------------------------------------------------------------------
  void renderPoliceAlarm(unsigned long currentMillis) {
    // 600 ms repeating wig-wag emergency strobe cycle
    uint16_t cycleTime = currentMillis % 600;

    bool leftFlash = false;
    bool rightFlash = false;

    // Left side (Red) quad-strobe: 0 .. 240 ms (four 30ms ON / 30ms OFF pulses)
    if (cycleTime < 240) {
      uint8_t sub = cycleTime % 60;
      if (sub < 30) leftFlash = true;
    }
    // Right side (Blue) quad-strobe: 300 .. 540 ms (four 30ms ON / 30ms OFF pulses)
    else if (cycleTime >= 300 && cycleTime < 540) {
      uint8_t sub = (cycleTime - 300) % 60;
      if (sub < 30) rightFlash = true;
    }

    const uint8_t MID_X = LEDS_SIDE_BOTTOM / 2; // 71

    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      uint8_t x = getLedX(i);
      if (x < MID_X) {
        // Left half: Red strobe
        if (leftFlash) {
          leds[i] = CRGB(255, 0, 0); // Blinding red flash
        } else {
          leds[i] = CRGB(8, 0, 0);   // Dark red standby glow
        }
      } else {
        // Right half: Blue strobe
        if (rightFlash) {
          leds[i] = CRGB(0, 0, 255); // Blinding blue flash
        } else {
          leds[i] = CRGB(0, 0, 12);  // Dark blue standby glow
        }
      }
    }
  }

  // --------------------------------------------------------------------------
  // HUE Behavior 3: Supernova
  // Cosmic explosion sequence: gathering unstable core, dazzling stellar blast,
  // outward-expanding shockwave, and cooling plasma nebula remnant.
  // --------------------------------------------------------------------------
  void renderSupernova(unsigned long currentMillis) {
    uint32_t t = (currentMillis - behaviorStartMillis) % 4000;
    const uint8_t MID_X = LEDS_SIDE_BOTTOM / 2; // 71

    if (t < 1600) {
      // Stage 1: Unstable Core Gathering: accelerating cosmic violet/magenta pulse
      uint16_t pulseSpeed = 8 + (t * 24) / 1600;
      uint8_t wave = sin8((uint8_t)((t * pulseSpeed) / 10));
      uint8_t bright = 25 + scale8(wave, 75); // 25 .. 100
      CRGB baseCol = CHSV(215, 240, bright);
      fill_solid(leds, NUM_LEDS, baseCol);
    } else if (t < 1900) {
      // Stage 2: Stellar Detonation: dazzling white/gold blast flash
      uint16_t dt = t - 1600; // 0 .. 300 ms
      uint8_t flashVal = 255 - (uint8_t)((dt * 70) / 300);
      CRGB blastCol(flashVal, flashVal, (uint8_t)(flashVal * 0.85f));
      fill_solid(leds, NUM_LEDS, blastCol);
    } else {
      // Stage 3: Outward Shockwave & Expanding Nebula: 1900 .. 4000 ms
      uint16_t dt = t - 1900; // 0 .. 2100 ms
      uint8_t waveRadius = (uint8_t)(((uint32_t)dt * MID_X) / 1500); // reaches edges in 1500ms
      if (waveRadius > MID_X + 5) waveRadius = MID_X + 5;
      uint8_t globalFade = 255 - (uint8_t)(((uint32_t)dt * 200UL) / 2100); // 255 -> 55

      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        uint8_t x = getLedX(i);
        int16_t distFromCenter = abs((int16_t)x - (int16_t)MID_X);

        if (abs(distFromCenter - waveRadius) <= 4) {
          // Shockwave crest: brilliant golden white
          leds[i] = CRGB(scale8(255, globalFade), scale8(220, globalFade), scale8(140, globalFade));
        } else if (distFromCenter < waveRadius) {
          // Hot expanding plasma remnant (fiery orange/magenta)
          uint8_t remnantHue = 15 + (distFromCenter * 2);
          uint8_t remnantVal = scale8(80, globalFade);
          leds[i] = CHSV(remnantHue, 240, remnantVal);
        } else {
          // Pre-shockwave cosmic dust
          leds[i] = CHSV(195, 255, scale8(35, globalFade));
        }
      }
    }
  }

  // --------------------------------------------------------------------------
  // Dynamic Sparkles Layer
  // Glinting starlight sparkles overlaid across the rectangular frame.
  // --------------------------------------------------------------------------
  void updateAndRenderSparkles() {
    // 1. Decay and render existing active sparkles
    for (uint8_t s = 0; s < SPARKLE_COUNT; s++) {
      if (sparkles[s].brightness > 0) {
        if (sparkles[s].brightness > sparkles[s].decay) {
          sparkles[s].brightness -= sparkles[s].decay;
        } else {
          sparkles[s].brightness = 0;
        }

        if (sparkles[s].brightness > 0) {
          leds[sparkles[s].index] = blend(leds[sparkles[s].index], sparkles[s].color, sparkles[s].brightness);
        }
      }
    }

    // 2. Randomly ignite new sparkles
    if (random8() < 90) { // ~35% chance per frame
      for (uint8_t s = 0; s < SPARKLE_COUNT; s++) {
        if (sparkles[s].brightness == 0) {
          sparkles[s].index = random16(NUM_LEDS);
          sparkles[s].brightness = 255;
          sparkles[s].decay = 30 + random8(35); // Lasts ~5 to 8 frames
          if (random8() < 140) {
            sparkles[s].color = CRGB::White;
          } else {
            sparkles[s].color = CHSV(random8(), 160, 255);
          }
          break; // Spawn 1 per trigger
        }
      }
    }
  }

  // --------------------------------------------------------------------------
  // Meteors Layer
  // Multi-meteor system with independent stable colors, smooth disappearing
  // tails, and overlay blending that keeps meteors unaffected by background behavior.
  // --------------------------------------------------------------------------
  void updateAndRenderMeteors() {
    for (uint8_t m = 0; m < METEOR_COUNT; m++) {
      if (!meteors[m].active) {
        spawnMeteor(m, false);
      }

      // Advance position along perimeter
      meteors[m].position += meteors[m].speed;
      if (meteors[m].position >= (float)NUM_LEDS) {
        meteors[m].position -= (float)NUM_LEDS;
      }
      meteors[m].distanceTraveled += (uint16_t)(meteors[m].speed + 0.5f);

      // Check if meteor completed its journey (full perimeter + tail)
      if (meteors[m].distanceTraveled >= meteors[m].maxDistance) {
        // Respawn with a brand new random color for next trip
        spawnMeteor(m, false);
      }

      int16_t headIdx = (int16_t)meteors[m].position;
      uint8_t tailLen = meteors[m].tailLength;

      // Requirement: "with a smooth disappearing tail"
      // As distanceTraveled exceeds NUM_LEDS, tail smoothly clears off
      if (meteors[m].distanceTraveled > NUM_LEDS) {
        uint16_t remaining = meteors[m].maxDistance - meteors[m].distanceTraveled;
        if (remaining < tailLen) {
          tailLen = (uint8_t)remaining;
        }
      }

      // Render tail from tip to head
      for (int16_t d = (int16_t)tailLen; d >= 0; d--) {
        int16_t pixelIdx = headIdx - d;
        while (pixelIdx < 0) pixelIdx += NUM_LEDS;
        while (pixelIdx >= NUM_LEDS) pixelIdx -= NUM_LEDS;

        // Quadratic falloff for luminous body and long smooth disappearing tail
        uint16_t ratio = 255 - (uint16_t)(((uint32_t)d * 255UL) / meteors[m].tailLength);
        uint8_t alpha = scale8(ratio, ratio);

        // Meteor color at distance d:
        CRGB meteorPixel;
        if (d == 0) {
          // Brilliant white-hot tip
          meteorPixel = blend(meteors[m].color, CRGB::White, 200);
        } else if (d == 1) {
          meteorPixel = blend(meteors[m].color, CRGB::White, 100);
        } else {
          // Requirement: "Keep color of meteor stable from start to end"
          meteorPixel = meteors[m].color;
        }

        // Requirement: "Meteor not affected by behavior"
        // Alpha blend ensures meteor's stable color overlays cleanly on background
        leds[pixelIdx] = blend(leds[pixelIdx], meteorPixel, alpha);
      }
    }
  }

public:
  ModeId getId() const override { return MODE_PARTY_HUE_METEOR_SPARKLE; }
  const char* getName() const override { return "PartyHUEMeteorSparkle"; }

  void enter(LightContext* ctx) override {
    FastLED.setDither(0);
    FastLED.setBrightness(220);
    lastUpdateMillis = 0;

    // Initialize meteors with initial scatter along perimeter
    for (uint8_t m = 0; m < METEOR_COUNT; m++) {
      spawnMeteor(m, true);
    }

    // Initialize sparkles as inactive
    for (uint8_t s = 0; s < SPARKLE_COUNT; s++) {
      sparkles[s].brightness = 0;
    }

    // Pick initial random HUE behavior and duration
    currentBehavior = (HueBehavior)random8(HUE_BEHAVIOR_COUNT);
    behaviorStartMillis = millis();
    behaviorDurationMs  = 8000UL + (unsigned long)random16(14000);

    Serial.print(F("[PartyHUEMeteorSparkle] Initialized with HUE Behavior: "));
    Serial.println(getBehaviorName(currentBehavior));
  }

  void update(LightContext* ctx, unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // 1. Step through behaviors in random order and random durations
    if (currentMillis - behaviorStartMillis >= behaviorDurationMs) {
      selectNextBehavior(currentMillis);
    }

    // 2. Render active HUE background behavior
    switch (currentBehavior) {
      case HUE_BEHAVIOR_POLICE_ALARM:
        renderPoliceAlarm(currentMillis);
        break;
      case HUE_BEHAVIOR_SUPERNOVA:
        renderSupernova(currentMillis);
        break;
      case HUE_BEHAVIOR_SMOOTH_TRANSITION:
      default:
        renderSmoothHUE(currentMillis);
        break;
    }

    // 3. Render dynamic sparkles layer
    updateAndRenderSparkles();

    // 4. Render meteors layer (stable random color per meteor, unaffected by behavior)
    updateAndRenderMeteors();

    FastLED.show();
  }
};

// ============================================================================
// 9. State Instances & Context
// ============================================================================
LightContext                systemContext;
NormalPulsingWarmWhiteState normalState;
ConcertSmoothRGBBreathState concertState;
PartyMeteorSparkleState     partyState;
PartyHUEMeteorSparkleState  partyHueState;

void applyMode(ModeId mode) {
  switch (mode) {
    case MODE_CONCERT_SMOOTH_RGB_BREATH:
      systemContext.transitionTo(&concertState);
      break;
    case MODE_PARTY_HUE_METEOR_SPARKLE:
      systemContext.transitionTo(&partyHueState);
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
// 10. 3-Step Digital Switch Reading & Debouncing
// ============================================================================
ModeId readSelectorHardware(ModeId currentMode) {
  int valLeft  = digitalRead(PIN_SWITCH_STEP1);
  int valRight = digitalRead(PIN_SWITCH_STEP3);

  if (valLeft == LOW) {
    return MODE_NORMAL_PULSING_WARM_WHITE; // Step 1 (Left)
  } else if (valRight == LOW) {
    return MODE_PARTY_HUE_METEOR_SPARKLE;  // Step 3 (Right)
  } else {
    return MODE_CONCERT_SMOOTH_RGB_BREATH; // Step 2 (Center / Open)
  }
}

ModeId        currentModeReading = MODE_NORMAL_PULSING_WARM_WHITE;
ModeId        lastRawReading     = MODE_NORMAL_PULSING_WARM_WHITE;
unsigned long lastDebounceMillis = 0;
const unsigned long DEBOUNCE_DELAY_MS = 50; // 50 ms debounce

// ============================================================================
// 11. Arduino setup() & loop()
// ============================================================================
void setup() {
  Serial.begin(115200);

  // Configure 3-step digital switch input pins (internal pull-ups enabled)
  pinMode(PIN_SWITCH_STEP1, INPUT_PULLUP);
  pinMode(PIN_SWITCH_STEP3, INPUT_PULLUP);

  // Configure color selection potentiometer input
  pinMode(PIN_COLOR_POT, INPUT);

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

  Serial.println(F("Rectangle Light (2.36m x 0.55m, 350 LEDs) Initialized."));
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

