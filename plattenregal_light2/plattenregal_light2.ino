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
 *   2. Dual-Section Potentiometer Control (Analog Pin A2):
 *      The physical rotary potentiometer is partitioned into two distinct functional zones:
 *        - First 180° (ADC 0 .. 511): ColorController Section
 *            * Step 1: Cold-White (~6500K crisp daylight white)
 *            * Step 2: Warm-White (~1900K candlelight warm white)
 *            * Steps 3..16: 14 distinct HUE steps across the 360° chromatic spectrum
 *        - 180° to 360° (ADC 512 .. 1023): BrightnessController Section
 *            * Governs global master luminosity from 0% (off) to 100% (full).
 *            * When knob is in the 0°..180° color-selection zone, brightness remains locked
 *              at its active level so colors can be previewed without extinguishing the strip.
 *
 *   Color Selection Protocol in NormalSmoothBreathingWarmWhite:
 *     - When in NormalSmoothBreathingWarmWhite and knob is in the color-selection zone (0°..180°),
 *       rotating the knob immediately previews the selected color in real-time.
 *     - In case 1000ms elapse without change (knob held stable), that color is committed
 *       as the new base breathing color.
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

// Dual-Zone Potentiometer (Wiper connected to A2, outer legs to 5V and GND)
// First 180° (ADC 0..511): ColorController | 180° to 360° (ADC 512..1023): BrightnessController
#define PIN_BRIGHTNESS_POT    A2
#define PIN_POTENTIOMETER     A2

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
// ColorController Class (Analog Pin A2, 0° to 180° / ADC 0 .. 511)
// Maps the first 180° of potentiometer rotation into 16 distinct color steps:
//   - Step 1: Cold-White (~6500K crisp daylight white)
//   - Step 2: Warm-White (~1900K candlelight warm white)
//   - Steps 3..16: 14 distinct HUE steps across the 360° chromatic spectrum
// ============================================================================
class ColorController {
public:
  static const uint8_t NUM_STEPS = 16;

  static bool isInColorZone(int rawAdc) {
    return (rawAdc >= 0 && rawAdc < 512);
  }

  static uint8_t rawToStep(int rawAdc) {
    int clamped = constrain(rawAdc, 0, 511);
    uint8_t step = (uint8_t)(clamped / 32); // 16 steps of 32 ADC counts each (0 .. 15)
    if (step >= NUM_STEPS) step = NUM_STEPS - 1;
    return step;
  }

  static CRGB getColorForStep(uint8_t step) {
    switch (step) {
      case 0:  return CRGB(230, 245, 255); // Step 1: Cold-White (~6500K crisp daylight white)
      case 1:  return CRGB(255, 147, 41);  // Step 2: Warm-White (~1900K candlelight white)
      case 2:  return CHSV(0, 255, 255);   // Step 3: Red
      case 3:  return CHSV(16, 255, 255);  // Step 4: Deep Orange
      case 4:  return CHSV(32, 255, 255);  // Step 5: Amber Gold
      case 5:  return CHSV(48, 255, 255);  // Step 6: Yellow
      case 6:  return CHSV(72, 255, 255);  // Step 7: Lime Green
      case 7:  return CHSV(96, 255, 255);  // Step 8: Pure Green
      case 8:  return CHSV(116, 255, 255); // Step 9: Emerald Teal
      case 9:  return CHSV(136, 255, 255); // Step 10: Cyan
      case 10: return CHSV(156, 255, 255); // Step 11: Ocean Blue
      case 11: return CHSV(176, 255, 255); // Step 12: Royal Blue
      case 12: return CHSV(196, 255, 255); // Step 13: Indigo
      case 13: return CHSV(216, 255, 255); // Step 14: Violet / Purple
      case 14: return CHSV(232, 255, 255); // Step 15: Magenta
      case 15: return CHSV(246, 255, 255); // Step 16: Rose Pink
      default: return CRGB(255, 147, 41);
    }
  }

  static const char* getStepName(uint8_t step) {
    static const char* const STEP_NAMES[NUM_STEPS] = {
      "Step 1: Cold-White (~6500K)",
      "Step 2: Warm-White (~1900K Candlelight)",
      "Step 3: Red",
      "Step 4: Deep Orange",
      "Step 5: Amber Gold",
      "Step 6: Yellow",
      "Step 7: Lime Green",
      "Step 8: Pure Green",
      "Step 9: Emerald Teal",
      "Step 10: Cyan",
      "Step 11: Ocean Blue",
      "Step 12: Royal Blue",
      "Step 13: Indigo",
      "Step 14: Violet / Purple",
      "Step 15: Magenta",
      "Step 16: Rose Pink"
    };
    if (step < NUM_STEPS) return STEP_NAMES[step];
    return "Unknown Step";
  }
};

// ============================================================================
// BrightnessController Class (Analog Pin A2, 180° to 360° / ADC 512 .. 1023)
// Governs global master luminosity from 0% to 100% when the potentiometer is
// rotated in the 180° to 360° section. When in the 0° to 180° color-selection
// zone, brightness remains locked at its active level.
// ============================================================================
class BrightnessController {
private:
  uint8_t pin;
  unsigned long sampleInterval;
  unsigned long lastCheckMillis;
  int filteredRaw;
  int lastRaw;
  uint8_t currentBrightness;

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
    readFilteredRaw();
    // If booted in 180°..360° brightness zone, map initial brightness
    if (filteredRaw >= 512) {
      if (filteredRaw <= 516) currentBrightness = 0;
      else if (filteredRaw >= 1018) currentBrightness = 255;
      else currentBrightness = (uint8_t)(((uint32_t)(filteredRaw - 516) * 255UL) / (1018UL - 516UL));
    }
    FastLED.setBrightness(currentBrightness);
  }

  bool update(unsigned long currentMillis, bool force = false) {
    if (!force && (currentMillis - lastCheckMillis < sampleInterval)) return false;
    lastCheckMillis = currentMillis;

    int raw = readFilteredRaw();

    // Section 180° to 360° (ADC 512 .. 1023) controls Brightness
    // Below 512 is the ColorController section (brightness remains constant)
    if (raw < 512) {
      return false;
    }

    uint8_t targetBrightness;
    if (raw <= 516) {
      targetBrightness = 0;
    } else if (raw >= 1018) {
      targetBrightness = 255;
    } else {
      targetBrightness = (uint8_t)(((uint32_t)(raw - 516) * 255UL) / (1018UL - 516UL));
    }

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
// Mode 1: NormalSmoothBreathingWarmWhite
// Organic, tranquil breathing pulsation across all 36 LEDs.
// Supports real-time color selection via the ColorController section (0° to 180°).
// Selection protocol:
//   - Mode NormalSmoothBreathingWarmWhite is current mode.
//   - Knob in color-selection zone (0° to 180° / ADC 0 .. 511).
//   - The rotation changes the color (real-time preview across all 36 LEDs at high brightness).
//   - In case 1000ms no change, this color is set as the active breathing color and indicated
//     with a visual confirmation double-pulse on the strip.
// ============================================================================
class NormalSmoothBreathingWarmWhite : public LightMode {
private:
  enum ColorSelectState {
    COLOR_STATE_NORMAL_BREATHING,
    COLOR_STATE_PREVIEW_HIGH_BRIGHTNESS,
    COLOR_STATE_ACCEPT_INDICATION
  };

  BrightnessController& brightnessCtrl;
  unsigned long lastUpdateMillis;
  const unsigned long UPDATE_INTERVAL = 20;    // 50 FPS smooth breath evaluation
  const unsigned long BREATH_PERIOD   = 30000; // 30 seconds calm, slow breathing cycle

  // Breathing depth: oscillates subtly between 75% and 100% of brightness
  // to maintain an ambient warm glow without harsh shutoffs or power-rail bounce
  const float MIN_FACTOR = 0.75f;
  const float MAX_FACTOR = 1.00f;

  // High brightness forced during color selection for crystal-clear preview
  const uint8_t HIGH_SELECTION_BRIGHTNESS = 255;

  // Active base color (defaults to Step 2: Warm-White CRGB(255, 147, 41))
  CRGB baseColor;

  // Color selection protocol state
  ColorSelectState colorState;
  CRGB previewColor;
  uint8_t lastObservedStep;
  int lastObservedRaw;
  unsigned long lastColorChangeMillis;
  unsigned long acceptStartTime;
  const unsigned long ACCEPT_DURATION = 400; // 400ms visual confirmation indication

  CRGB lastDisplayedColor;
  uint8_t lastBrightness;
  bool firstFrame;

public:
  NormalSmoothBreathingWarmWhite(BrightnessController& bc)
    : brightnessCtrl(bc),
      lastUpdateMillis(0),
      baseColor(CRGB(255, 147, 41)), // Default: Step 2 Warm-White (~1900K candle flame)
      colorState(COLOR_STATE_NORMAL_BREATHING),
      previewColor(CRGB(255, 147, 41)),
      lastObservedStep(255),
      lastObservedRaw(-1),
      lastColorChangeMillis(0),
      acceptStartTime(0),
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
    colorState = COLOR_STATE_NORMAL_BREATHING;
    FastLED.setBrightness(brightnessCtrl.getBrightness());
    lastObservedStep = 255;
    lastObservedRaw = -1;
  }

  void setBaseColor(const CRGB& color) {
    baseColor = color;
    firstFrame = true;
  }

  CRGB getBaseColor() const {
    return baseColor;
  }

  void update(unsigned long currentMillis) override {
    if (currentMillis - lastUpdateMillis < UPDATE_INTERVAL) return;
    lastUpdateMillis = currentMillis;

    int raw = brightnessCtrl.getRawValue();

    // ------------------------------------------------------------------------
    // 1. Color Accept Confirmation Indication
    // ------------------------------------------------------------------------
    if (colorState == COLOR_STATE_ACCEPT_INDICATION) {
      unsigned long elapsed = currentMillis - acceptStartTime;
      if (elapsed < ACCEPT_DURATION) {
        // Crisp double-pulse visual confirmation:
        // 0..80ms: Dim (10% intensity dip)
        // 80..180ms: Flash Bright (accepted color with brilliant white highlight)
        // 180..260ms: Dim (10% intensity dip)
        // 260..360ms: Flash Bright (accepted color with brilliant white highlight)
        // 360..400ms: Settle into accepted color
        bool isFlashPhase = (elapsed >= 80 && elapsed < 180) || (elapsed >= 260 && elapsed < 360);
        CRGB frameColor;
        if (isFlashPhase) {
          frameColor = blend(baseColor, CRGB::White, 160); // Dazzling bright confirmation flash
        } else {
          frameColor = baseColor;
          frameColor.nscale8_video(30); // Contrast dip
        }

        fill_solid(leds, NUM_LEDS, frameColor);
        FastLED.show();
        return;
      } else {
        // Confirmation animation complete -> return to normal breathing at master brightness
        colorState = COLOR_STATE_NORMAL_BREATHING;
        FastLED.setBrightness(brightnessCtrl.getBrightness());
        firstFrame = true;
        lastDisplayedColor = CRGB(0, 0, 0); // Force redraw
      }
    }

    // ------------------------------------------------------------------------
    // 2. Color Selection Protocol:
    // When knob is in color-selection zone (0° to 180° / ADC 0 .. 511):
    // Rotation changes the color (real-time preview at high brightness).
    // In case 1000ms no change, this color is set as color and confirmed.
    // ------------------------------------------------------------------------
    if (ColorController::isInColorZone(raw)) {
      uint8_t step = ColorController::rawToStep(raw);

      if (lastObservedStep == 255) {
        // Initial sample upon entering color zone
        lastObservedStep = step;
        lastObservedRaw = raw;
      } else if (step != lastObservedStep || (colorState == COLOR_STATE_PREVIEW_HIGH_BRIGHTNESS && abs(raw - lastObservedRaw) > 3)) {
        // Rotation detected!
        if (step != lastObservedStep) {
          lastObservedStep = step;
          previewColor = ColorController::getColorForStep(step);
          Serial.print(F("[Color Preview (High Brightness)] "));
          Serial.println(ColorController::getStepName(step));
        }
        lastObservedRaw = raw;
        lastColorChangeMillis = currentMillis;

        if (colorState != COLOR_STATE_PREVIEW_HIGH_BRIGHTNESS) {
          colorState = COLOR_STATE_PREVIEW_HIGH_BRIGHTNESS;
          // Ensure high brightness during color selection
          FastLED.setBrightness(HIGH_SELECTION_BRIGHTNESS);
        }
        firstFrame = true;
      }

      if (colorState == COLOR_STATE_PREVIEW_HIGH_BRIGHTNESS) {
        // If 1000ms elapse without change -> accept color and indicate confirmation
        if (currentMillis - lastColorChangeMillis >= 1000) {
          baseColor = previewColor;
          colorState = COLOR_STATE_ACCEPT_INDICATION;
          acceptStartTime = currentMillis;

          Serial.println(F("========================================"));
          Serial.print(F("[Color Accepted] Active color COMMITTED to: "));
          Serial.println(ColorController::getStepName(lastObservedStep));
          Serial.println(F("[Color Accepted] Visual confirmation indication triggered!"));
          Serial.println(F("========================================"));
          return;
        }
      }
    } else {
      // Knob is in BrightnessController section (180° to 360° / ADC 512 .. 1023)
      if (colorState == COLOR_STATE_PREVIEW_HIGH_BRIGHTNESS) {
        // Exited color zone before 1000ms elapsed -> cancel preview, restore master brightness
        colorState = COLOR_STATE_NORMAL_BREATHING;
        FastLED.setBrightness(brightnessCtrl.getBrightness());
        firstFrame = true;
        Serial.println(F("[Color Selection] Exited color zone -> Kept previously set color."));
      }
      lastObservedStep = 255;
      lastObservedRaw = -1;
    }

    uint8_t currentBrightness = FastLED.getBrightness();

    // ------------------------------------------------------------------------
    // 3. Render to LED Strip:
    // If previewing -> Solid real-time preview of selected color across all 36 LEDs
    // Otherwise -> Calm sinusoidal breathing pulsation in active baseColor
    // ------------------------------------------------------------------------
    if (colorState == COLOR_STATE_PREVIEW_HIGH_BRIGHTNESS) {
      if (firstFrame || previewColor != lastDisplayedColor || currentBrightness != lastBrightness) {
        firstFrame = false;
        lastDisplayedColor = previewColor;
        lastBrightness = currentBrightness;

        fill_solid(leds, NUM_LEDS, previewColor);
        FastLED.show();
      }
    } else {
      // Organic sinusoidal breathing waveform (1 - cos(theta)) / 2
      // Smoothly and calmly traverses from trough (min) to peak (max) over 30 seconds
      float phase = (float)(currentMillis % BREATH_PERIOD) / (float)BREATH_PERIOD;
      float wave = 0.5f * (1.0f - cos(phase * 2.0f * PI));

      // Dynamic factor within calm breathing range (75% to 100%)
      float factor = MIN_FACTOR + wave * (MAX_FACTOR - MIN_FACTOR);

      // Scale RGB channels equally to preserve exact color balance
      CRGB pulseColor(
        (uint8_t)(baseColor.r * factor + 0.5f),
        (uint8_t)(baseColor.g * factor + 0.5f),
        (uint8_t)(baseColor.b * factor + 0.5f)
      );

      // Anti-flicker guard: only update strip when displayed color or brightness changes
      if (firstFrame || pulseColor != lastDisplayedColor || currentBrightness != lastBrightness) {
        firstFrame = false;
        lastDisplayedColor = pulseColor;
        lastBrightness = currentBrightness;

        fill_solid(leds, NUM_LEDS, pulseColor);
        FastLED.show();
      }
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

NormalSmoothBreathingWarmWhite   modeBreathingWarmWhite(brightnessController);
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

