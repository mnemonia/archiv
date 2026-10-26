/*
 * ============================================================================
 * Plattenregal Light 2 Controller (SK6812 RGB LED Strip)
 * ============================================================================
 *
 * Target: Arduino Uno / Nano / Pro Mini / compatible (ATmega328P or higher)
 * LED Strip: SK6812 RGB (36 LEDs: 18 LEDs A->B and 18 LEDs B->A vis-a-vis synchronized)
 * FastLED Library: Used for hardware driving, timing, and global brightness scaling
 *
 * Hardware Controls:
 *   1. 2-State Digital Mode Switch (Pin 11):
 *      Selects between the two primary lighting modes:
 *        - Pin 11 to GND (LOW):   "NormalSmoothBreathingWarmWhite"
 *        - Pin 11 open (HIGH):    "SmoothRgbRainbowLowFrequencyWave"
 *      Features integrated 50ms software debouncing with internal pull-up (INPUT_PULLUP)
 *      to completely eliminate contact chatter and flickering during switching.
 *
 *   2. Global BrightnessController (Rotary Potentiometer on Analog Pin A2):
 *      Master luminosity knob active across BOTH modes (0% to 100% brightness).
 *      STRICT RULE: Modulates strip luminosity only. It has ZERO influence on
 *      the color spectrum, hue, or saturation set by the active mode.
 *
 * Flicker Prevention & Non-Blocking Architecture:
 *   - FastLED temporal dithering disabled (FastLED.setDither(0)) to eliminate
 *     micro-flickering and PWM inter-frame beat frequencies on SK6812.
 *   - Analog oversampling (4-sample average), 3-count noise deadband, and smooth
 *     Exponential Moving Average (EMA) filtering absorb wiper noise and rail dips.
 *   - Pure monotonic millis() scheduling with ZERO delay(), ZERO delayMicroseconds(),
 *     and ZERO spin-locks for maximum responsiveness.
 * ============================================================================
 */

#include <FastLED.h>

// ============================================================================
// Hardware Pin & LED Strip Configuration
// Dual 18-LED row configuration (36 LEDs total):
//   - Forward row: A to B (LED indices 0 .. 17)
//   - Return row:  B to A (LED indices 18 .. 35)
// ============================================================================
#define LED_PIN               6     // Data input pin for the SK6812 strip (via 330-470 Ohm resistor)
#define NUM_LEDS_ROW          18    // Number of LEDs along one direction (A to B, or B to A)
#define NUM_LEDS              (2 * NUM_LEDS_ROW) // Total number of SK6812 RGB LEDs (36 LEDs)

// Digital 2-Position Mode Switch (Pin 11 with INPUT_PULLUP, switch terminal to GND)
#define PIN_MODE_SWITCH       11

// Global Brightness Potentiometer (Wiper connected to A2, outer legs to 5V and GND)
#define PIN_BRIGHTNESS_POT    A2

// ============================================================================
// FastLED Buffer & Type Declarations
// ============================================================================
CRGB leds[NUM_LEDS];

enum LightModeId {
  MODE_NORMAL_SMOOTH_BREATHING_WARM_WHITE = 0,
  MODE_SMOOTH_RGB_RAINBOW_LOW_FREQUENCY_WAVE = 1
};

// Forward declaration for function prototypes (protects Arduino preprocessor)
void applyMode(LightModeId modeId);

// Abstract Base Class for Lighting Modes
class LightMode {
public:
  virtual ~LightMode() {}
  virtual LightModeId getId() const = 0;
  virtual const char* getName() const = 0;
  virtual void enter() {}
  virtual void update(unsigned long currentMillis) = 0;
};

// ============================================================================
// Mode 1: NormalSmoothBreathingWarmWhite
// Organic, tranquil breathing pulsation across all 36 LEDs in calibrated warm candlelight white.
// Color is locked to ~1900K warm candlelight white (CRGB(255, 147, 41)).
// ============================================================================
class NormalSmoothBreathingWarmWhite : public LightMode {
private:
  unsigned long lastUpdateMillis;
  const unsigned long UPDATE_INTERVAL = 20;    // 50 FPS smooth breath evaluation
  const unsigned long BREATH_PERIOD   = 30000; // 30 seconds calm, slow breathing cycle

  // Breathing depth: oscillates subtly between 30% and 90% of brightness
  // to maintain an ambient warm glow without harsh shutoffs or power-rail bounce
  const float MIN_FACTOR = 0.75f;
  const float MAX_FACTOR = 1.00f;

  // Calibrated warm candlelight white (~1900K candle flame tone / FastLED Candle standard)
  // R:G:B ratios produce a rich, cozy, golden-amber candle glow while preserving white luminance
  const CRGB WARM_WHITE = CRGB(255, 147, 41);
  
  CRGB lastDisplayedColor;
  uint8_t lastBrightness;
  bool firstFrame;

public:
  NormalSmoothBreathingWarmWhite()
    : lastUpdateMillis(0),
      lastDisplayedColor(0, 0, 0),
      lastBrightness(255),
      firstFrame(true) {}

  LightModeId getId() const override {
    return MODE_NORMAL_SMOOTH_BREATHING_WARM_WHITE;
  }

  const char* getName() const override {
    return "NormalSmoothBreathingWarmWhite";
  }

  void enter() override {
    lastUpdateMillis = 0;
    firstFrame = true;
  }

  void update(unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // Organic sinusoidal breathing waveform (1 - cos(theta)) / 2
    // Smoothly and calmly traverses from trough (min) to peak (max) over 30 seconds
    float phase = (float)(currentMillis % BREATH_PERIOD) / (float)BREATH_PERIOD;
    float wave = 0.5f * (1.0f - cos(phase * 2.0f * PI));

    // Dynamic factor within calm breathing range
    float factor = MIN_FACTOR + wave * (MAX_FACTOR - MIN_FACTOR);

    // Scale RGB channels equally to preserve exact candlelight color balance
    CRGB pulseColor(
      (uint8_t)(WARM_WHITE.r * factor + 0.5f),
      (uint8_t)(WARM_WHITE.g * factor + 0.5f),
      (uint8_t)(WARM_WHITE.b * factor + 0.5f)
    );

    uint8_t currentBrightness = FastLED.getBrightness();

    // Anti-flicker guard: only update strip when displayed color or brightness changes
    if (firstFrame || pulseColor != lastDisplayedColor || currentBrightness != lastBrightness) {
      firstFrame = false;
      lastDisplayedColor = pulseColor;
      lastBrightness = currentBrightness;

      fill_solid(leds, NUM_LEDS, pulseColor);
      FastLED.show();
    }
  }
};

// ============================================================================
// Mode 2: SmoothRgbRainbowLowFrequencyWave
// Gentle, low-frequency traveling rainbow wave across the dual 18-LED rows (36 LEDs).
// The strip goes from A to B (LEDs 0..17) and returns from B to A (LEDs 18..35).
// Both strips are synchronized so that each LED vis-a-vis has the exact same color value.
// Hue advances slowly, producing a tranquil ambient wave.
// The rainbow wave is stretched across 8 * NUM_LEDS_ROW (144 LEDs, increased by
// a factor of 4 over the previous 2 * NUM_LEDS_ROW), allowing colors to spread out
// broadly across the entire shelf with subtle, monochromatic-gradient transitions.
// Colors are generated at 100% saturation and value (CHSV(hue, 255, 255)),
// leaving luminosity control strictly to the global BrightnessController.
// ============================================================================
class SmoothRgbRainbowLowFrequencyWave : public LightMode {
private:
  unsigned long lastUpdateMillis;
  const unsigned long UPDATE_INTERVAL = 20;   // 50 FPS smooth rainbow motion
  uint16_t hueAccumulator;

  // At 50 FPS (20ms interval), advancing by 10 per frame:
  // 65536 / 10 ≈ 6553 frames -> 6553 * 20ms ≈ 131s per full 360° rainbow rotation.
  const uint16_t HUE_INCREMENT = 10;//105;

  // Spatial color spread factor:
  // Stretched by factor 4 over previous 2*NUM_LEDS_ROW -> 8 * NUM_LEDS_ROW (144 LEDs).
  // (Set to 4 if 4*NUM_LEDS_ROW total spread is preferred).
  const uint8_t COLOR_SPREAD_FACTOR = 8;

public:
  SmoothRgbRainbowLowFrequencyWave() : lastUpdateMillis(0), hueAccumulator(0) {}

  LightModeId getId() const override {
    return MODE_SMOOTH_RGB_RAINBOW_LOW_FREQUENCY_WAVE;
  }

  const char* getName() const override {
    return "SmoothRgbRainbowLowFrequencyWave";
  }

  void enter() override {
    lastUpdateMillis = 0;
  }

  void update(unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    // Advance low-frequency temporal wave
    hueAccumulator += HUE_INCREMENT;
    uint8_t baseHue = (uint8_t)(hueAccumulator >> 8);

    // Stretched spatial rainbow wave across the shelf (A to B):
    // Full 360° color cycle spans COLOR_SPREAD_FACTOR * NUM_LEDS_ROW.
    // The strip runs A to B (indices 0..17) and returns B to A (indices 18..35).
    // Both strips are synchronized: each LED vis-a-vis has the exact same color value.
    for (uint8_t x = 0; x < NUM_LEDS_ROW; x++) {
      uint8_t pixelHue = baseHue + (uint8_t)(((uint32_t)x * 256UL) / ((uint32_t)COLOR_SPREAD_FACTOR * NUM_LEDS_ROW));
      CRGB color = CHSV(pixelHue, 255, 255);

      // Forward strip: A to B (indices 0 .. 17)
      leds[x] = color;

      // Return strip: B to A (indices 18 .. 35), vis-a-vis with forward strip
      leds[(2 * NUM_LEDS_ROW - 1) - x] = color;
    }
    FastLED.show();
  }
};

// ============================================================================
// BrightnessController Class (Analog Pin A2)
// Encapsulates non-blocking analog sampling, noise/jitter suppression,
// and FastLED master luminosity scaling without shifting active mode colors.
// ============================================================================
class BrightnessController {
private:
  uint8_t pin;
  unsigned long sampleInterval;
  unsigned long lastCheckMillis;
  int filteredRaw;
  int lastRaw;
  uint8_t currentBrightness;

  // 4-sample average with deadband and Exponential Moving Average (EMA)
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
      // 4-count deadband to absorb wiper electrical/thermal noise
      if (abs(raw - lastRaw) > 3) {
        lastRaw = raw;
        // Smooth exponential moving average: 75% historical + 25% new sample
        filteredRaw = (filteredRaw * 3 + lastRaw) / 4;
      }
    }
    return filteredRaw;
  }

public:
  BrightnessController(uint8_t analogPin, unsigned long sampleIntervalMs = 25, uint8_t initialBrightness = 180)
    : pin(analogPin),
      sampleInterval(sampleIntervalMs),
      lastCheckMillis(0),
      filteredRaw(-1),
      lastRaw(-1),
      currentBrightness(initialBrightness) {}

  void begin() {
    pinMode(pin, INPUT);
    FastLED.setBrightness(currentBrightness);
  }

  bool update(unsigned long currentMillis, bool force = false) {
    if (!force && (currentMillis - lastCheckMillis < sampleInterval)) return false;
    lastCheckMillis = currentMillis;

    int raw = readFilteredRaw();

    // Map 0..1023 analog reading to 0..255 global luminosity scale
    // Clamping at ends ensures 0 is off and 1023 is 100% full brightness
    uint8_t targetBrightness;
    if (raw <= 4) {
      targetBrightness = 0;
    } else if (raw >= 1018) {
      targetBrightness = 255;
    } else {
      targetBrightness = (uint8_t)(((uint32_t)raw * 255UL) / 1023UL);
    }

    // Apply hysteresis deadband (>= 2 counts) to eliminate wiper microphonic flicker
    if (force || targetBrightness == 0 || targetBrightness == 255 ||
        abs((int)targetBrightness - (int)currentBrightness) >= 2) {
      if (targetBrightness != currentBrightness || force) {
        currentBrightness = targetBrightness;
        FastLED.setBrightness(currentBrightness);
        return true;
      }
    }
    return false;
  }

  uint8_t getBrightness() const {
    return currentBrightness;
  }

  int getRawValue() const {
    return filteredRaw;
  }
};

// ============================================================================
// ModeSwitchController Class
// Handles the 2-position digital switch (Pin 11) with zero-flicker debouncing.
//   Terminal connected to GND (LOW): MODE_NORMAL_SMOOTH_BREATHING_WARM_WHITE
//   Terminal open / VCC (HIGH):     MODE_SMOOTH_RGB_RAINBOW_LOW_FREQUENCY_WAVE
// ============================================================================
class ModeSwitchController {
private:
  uint8_t pin;
  LightModeId currentMode;
  LightModeId lastReading;
  unsigned long lastDebounceMillis;
  const unsigned long DEBOUNCE_DELAY = 50; // 50ms software debounce for clean switching

  LightModeId readRaw() const {
    // Active LOW when switch connects pin to GND:
    //   LOW  = MODE_NORMAL_SMOOTH_BREATHING_WARM_WHITE
    //   HIGH = MODE_SMOOTH_RGB_RAINBOW_LOW_FREQUENCY_WAVE
    return (digitalRead(pin) == LOW)
      ? MODE_NORMAL_SMOOTH_BREATHING_WARM_WHITE
      : MODE_SMOOTH_RGB_RAINBOW_LOW_FREQUENCY_WAVE;
  }

public:
  ModeSwitchController(uint8_t switchPin = PIN_MODE_SWITCH)
    : pin(switchPin),
      currentMode(MODE_NORMAL_SMOOTH_BREATHING_WARM_WHITE),
      lastReading(MODE_NORMAL_SMOOTH_BREATHING_WARM_WHITE),
      lastDebounceMillis(0) {}

  void begin() {
    pinMode(pin, INPUT_PULLUP);
    currentMode = readRaw();
    lastReading = currentMode;
  }

  bool update(unsigned long currentMillis) {
    LightModeId reading = readRaw();

    if (reading != lastReading) {
      lastDebounceMillis = currentMillis;
      lastReading = reading;
    }

    if ((currentMillis - lastDebounceMillis) >= DEBOUNCE_DELAY) {
      if (currentMode != reading) {
        currentMode = reading;
        return true; // Mode transition triggered!
      }
    }
    return false;
  }

  LightModeId getMode() const {
    return currentMode;
  }

  const char* getModeName() const {
    return (currentMode == MODE_NORMAL_SMOOTH_BREATHING_WARM_WHITE)
      ? "NormalSmoothBreathingWarmWhite"
      : "SmoothRgbRainbowLowFrequencyWave";
  }
};

// ============================================================================
// Lighting Context (Coordinator)
// ============================================================================
class LightContext {
private:
  LightMode* currentMode;

public:
  LightContext() : currentMode(nullptr) {}

  void transitionTo(LightMode* newMode) {
    if (newMode == currentMode) return;
    currentMode = newMode;
    if (currentMode != nullptr) {
      Serial.print(F("[Mode Switch] -> "));
      Serial.println(currentMode->getName());
      currentMode->enter();
    }
  }

  void update(unsigned long currentMillis) {
    if (currentMode != nullptr) {
      currentMode->update(currentMillis);
    }
  }

  LightModeId getCurrentModeId() const {
    if (currentMode != nullptr) {
      return currentMode->getId();
    }
    return MODE_NORMAL_SMOOTH_BREATHING_WARM_WHITE;
  }

  const char* getCurrentModeName() const {
    if (currentMode != nullptr) {
      return currentMode->getName();
    }
    return "None";
  }
};

// ============================================================================
// Controller Instances
// ============================================================================
BrightnessController             brightnessController(PIN_BRIGHTNESS_POT);
ModeSwitchController             modeSwitchController(PIN_MODE_SWITCH);
LightContext                     lightContext;

NormalSmoothBreathingWarmWhite   modeBreathingWarmWhite;
SmoothRgbRainbowLowFrequencyWave modeRainbowWave;

void applyMode(LightModeId modeId) {
  if (modeId == MODE_SMOOTH_RGB_RAINBOW_LOW_FREQUENCY_WAVE) {
    lightContext.transitionTo(&modeRainbowWave);
  } else {
    lightContext.transitionTo(&modeBreathingWarmWhite);
  }
}

// ============================================================================
// Arduino setup() & loop()
// ============================================================================
void setup() {
  Serial.begin(115200);

  // 1. Initialize FastLED for SK6812 strip (GRB color order)
  FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS);

  // 2. Disable temporal dithering to eliminate high-frequency micro-flicker
  FastLED.setDither(0);

  FastLED.clear();
  FastLED.show();

  // 3. Initialize hardware input controllers
  brightnessController.begin();
  modeSwitchController.begin();

  // 4. Sample initial brightness potentiometer reading
  brightnessController.update(millis(), true);

  // 5. Apply initial mode based on the 2-state switching knob position
  applyMode(modeSwitchController.getMode());

  Serial.println(F("Plattenregal Light 2 Controller Initialized."));
  Serial.print(F("Active Mode: "));
  Serial.println(modeSwitchController.getModeName());
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Non-blocking check for mode changes from the 2-state switching knob
  if (modeSwitchController.update(currentMillis)) {
    applyMode(modeSwitchController.getMode());
  }

  // 2. Read global brightness potentiometer and update FastLED luminosity
  //    (Modulates overall brightness only; does not affect active mode colors)
  brightnessController.update(currentMillis);

  // 3. Non-blocking update of the active lighting animation
  lightContext.update(currentMillis);
}

