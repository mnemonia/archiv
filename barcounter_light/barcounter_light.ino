/*
 * ============================================================================
 * BarCounter Light & Interactive Game System (SK6812 LED Strip: 12m, 60 LED/m)
 * ============================================================================
 * 
 * Hardware Target: Arduino (ATmega328P / Mega2560 / Uno R4 / ESP32 / compatible)
 * LED Strip: 12 meters SK6812 RGB (720 addressable LEDs @ 60 LEDs/m)
 * LED Driver: FastLED (<FastLED.h>) with temporal dithering disabled (FastLED.setDither(0))
 * 
 * Physical Controls:
 *   - Rotary Game Selection Knob (Analog Pin A1):
 *       6 dedicated dial zones (0..1023 ADC) selecting the active competitive game:
 *         * Zone 0 (  0.. 170): Game 1 - "Resonance Pulse" (Rhythm Wave Tug-of-War)
 *         * Zone 1 (171.. 341): Game 2 - "Sparkle Rush" (Nova Sparkle Reflex Deflector)
 *         * Zone 2 (342.. 511): Game 3 - "Flame Tug" (Bellows Forge Combustion Clash)
 *         * Zone 3 (512.. 682): Game 4 - "Meteor Deflector" (High-Speed Comet Rally)
 *         * Zone 4 (683.. 852): Game 5 - "Cylon Clash" (Hyper-Pong Beam Duel)
 *         * Zone 5 (853..1023): Game 6 - "Territory Paint" (Rapid Wipe Wars)
 *   - Rotary Master Brightness Knob (Analog Pin A2):
 *       Master luminosity knob governing global strip output (0% to 100%) across ALL modes.
 *   - Rotary Color / Mood Knob (Analog Pin A3):
 *       Full 360° rainbow spectrum (0..65535 hue across 0..1010 ADC counts) + Pure White (1012..1023).
 *       Normalizes RMS perceptual brightness (P ≈ 85.8) across all hues and White, ensuring
 *       color adjustment has ZERO influence on overall strip brightness.
 *   - Rotary Global Light Behavior Knob (Analog Pin A4):
 *       6 dedicated dial zones (0..1023 ADC) selecting the active ambient lighting mode:
 *         * Zone 0 (  0.. 170): Mode 1 - "Breathing / Pulse" (7.5s Meditative Breath)
 *         * Zone 1 (171.. 341): Mode 2 - "Twinkle / Sparkle" (Slow Starfield Shimmer)
 *         * Zone 2 (342.. 511): Mode 3 - "Fire / Flame" (Cozy Slow-Ember Hearth Fire)
 *         * Zone 3 (512.. 682): Mode 4 - "Comet / Meteor" (Gentle Gliding Shooting Star)
 *         * Zone 4 (683.. 852): Mode 5 - "Scanner / Cylon" (6.0s Smooth Larson Eye)
 *         * Zone 5 (853..1023): Mode 6 - "Color Wipe" (Progressive Chromatic Roll)
 *   - Player 1 Button (Digital Pin 2, active LOW with INPUT_PULLUP)
 *   - Player 2 Button (Digital Pin 4, active LOW with INPUT_PULLUP)
 *   - Game Mode Launch / Toggle:
 *       * Hold P1 + P2 buttons simultaneously for 800 ms (or send serial 'g').
 * 
 * Hardware Pin Summary:
 *   - Pin A1: Rotary Game Selection Potentiometer
 *   - Pin A2: Rotary Master Brightness Potentiometer
 *   - Pin A3: Rotary Mood / Base-Color Potentiometer
 *   - Pin A4: Rotary Global Light Behavior Potentiometer
 *   - Pin A0: Unconnected / floating pin for randomSeed()
 *   - Pin 2:  Player 1 Push Button (GND)
 *   - Pin 4:  Player 2 Push Button (GND)
 *   - Pin 6:  SK6812 LED Strip Data IN (via 330-470 Ohm resistor)
 * ============================================================================
 */

#include <FastLED.h>

// ============================================================================
// 1. Strip Geometry & Hardware Pin Configuration
// ============================================================================
#define STRIP_LENGTH_METERS      12     // 12 meters total bar counter length
#define LEDS_PER_METER           60     // 60 LEDs per meter
#define NUM_LEDS                 (STRIP_LENGTH_METERS * LEDS_PER_METER) // 720 LEDs

#define LED_PIN                  6      // Output data pin for physical SK6812 strip

#define PIN_GAME_KNOB            A1     // Rotary knob wiper for game selection
#define PIN_BRIGHTNESS_KNOB      A2     // Rotary knob wiper for global brightness setting
#define PIN_COLOR_KNOB           A3     // Rotary knob wiper for global mood/color setting
#define PIN_LIGHT_BEHAVIOR_KNOB  A4     // Rotary knob wiper for ambient light behavior selection
#define PIN_ENTROPY              A0     // Floating analog pin for randomSeed()

#define PIN_BTN_P1               2      // Player 1 input button (active LOW)
#define PIN_BTN_P2               4      // Player 2 input button (active LOW)

#define SERIAL_BAUD              115200

// Target backend: FastLED physical strip or optional serial streaming
#define BACKEND_VIRTUAL_SERIAL   0
#define BACKEND_FASTLED_REAL     1
#define STRIP_BACKEND            BACKEND_FASTLED_REAL

// ============================================================================
// 2. FastLED Strip Buffer & Driver Class
// ============================================================================
CRGB leds[NUM_LEDS];

class FastLEDDriver {
public:
  void begin() {
    FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setDither(0); // Disable temporal dithering to eliminate PWM shimmer
    clear();
    FastLED.show();
    Serial.begin(SERIAL_BAUD);
  }

  void setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < NUM_LEDS) {
      leds[index] = CRGB(r, g, b);
    }
  }

  void setPixelColor(uint16_t index, uint32_t color) {
    if (index < NUM_LEDS) {
      leds[index] = CRGB((uint8_t)(color >> 16), (uint8_t)(color >> 8), (uint8_t)color);
    }
  }

  uint32_t getPixelColor(uint16_t index) const {
    if (index >= NUM_LEDS) return 0;
    return ((uint32_t)leds[index].r << 16) | ((uint32_t)leds[index].g << 8) | leds[index].b;
  }

  void setBrightness(uint8_t b) {
    FastLED.setBrightness(b);
  }

  uint8_t getBrightness() const {
    return FastLED.getBrightness();
  }

  void clear() {
    FastLED.clear();
  }

  uint16_t numPixels() const {
    return NUM_LEDS;
  }

  void show() {
#if (STRIP_BACKEND == BACKEND_VIRTUAL_SERIAL)
    // Optional high-speed serial streaming to desktop visualizer
    uint8_t header[4] = { 0xAA, 0x55, 0x01, (uint8_t)(NUM_LEDS > 255 ? 255 : NUM_LEDS) };
    Serial.write(header, 4);
    uint8_t checksum = header[0] ^ header[1] ^ header[2] ^ header[3];
    uint8_t br = FastLED.getBrightness();
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      uint8_t r = scale8(leds[i].r, br);
      uint8_t g = scale8(leds[i].g, br);
      uint8_t b = scale8(leds[i].b, br);
      Serial.write(r); Serial.write(g); Serial.write(b);
      checksum ^= r ^ g ^ b;
    }
    Serial.write(checksum);
#endif
    FastLED.show();
  }

  static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }

  // Constant Perceptual Brightness Color Wheel
  // Normalizes RMS perceptual brightness (P = sqrt(0.299*R^2 + 0.587*G^2 + 0.114*B^2))
  // to a uniform P ≈ 85.8 across all 360° of rainbow hues, ensuring RotaryColorKnob
  // has zero influence on the overall brightness of the strip.
  static uint32_t ColorConstantBrightness(uint16_t hue) {
    uint8_t sector = (hue / 10922) % 6;
    uint16_t frac = hue % 10922;

    uint8_t p = 0;
    uint8_t q = (uint8_t)(((uint32_t)255 * (10922 - frac)) / 10922);
    uint8_t t = (uint8_t)(((uint32_t)255 * frac) / 10922);
    uint8_t v = 255;

    uint8_t r, g, b;
    switch (sector) {
      case 0: r = v; g = t; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = t; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t; g = p; b = v; break;
      default: r = v; g = p; b = q; break;
    }

    float pBright = sqrt(0.299f * (float)r * (float)r +
                         0.587f * (float)g * (float)g +
                         0.114f * (float)b * (float)b);
    if (pBright > 0.001f) {
      float scale = 85.8f / pBright;
      r = (uint8_t)min(255, (int)((float)r * scale + 0.5f));
      g = (uint8_t)min(255, (int)((float)g * scale + 0.5f));
      b = (uint8_t)min(255, (int)((float)b * scale + 0.5f));
    }
    return Color(r, g, b);
  }

  static uint32_t ColorHSV(uint16_t hue, uint8_t sat = 255, uint8_t val = 255) {
    CRGB c;
    hsv2rgb_rainbow(CHSV((uint8_t)(hue >> 8), sat, val), c);
    return Color(c.r, c.g, c.b);
  }
};

FastLEDDriver strip;

// ============================================================================
// 3. Stable Analog Input & Knob Tracking
// ============================================================================
// Discards first conversion after ADC MUX channel switch to pre-charge internal
// sample-and-hold capacitor, eliminating crosstalk/ghosting between adjacent analog pins.
static int readAnalogStable(uint8_t pin) {
  analogRead(pin);        // Select MUX channel and discard first reading
  delayMicroseconds(35);  // Settle time for sample-and-hold capacitor
  int val1 = analogRead(pin);
  delayMicroseconds(20);
  int val2 = analogRead(pin);
  return (val1 + val2) >> 1; // 2-sample average
}

// Timestamp of last active adjustment of RotaryBrightnessKnob
// Used by RotaryColorKnob and RotaryGameKnob to lock against rail-dip crosstalk
unsigned long lastBrightnessTurnTime = 0;

// ============================================================================
// 4. Game Modes Enum & Rotary Game Selection Knob (Analog Pin A1)
// ============================================================================
enum GameId {
  GAME_RESONANCE_PULSE  = 0,
  GAME_SPARKLE_RUSH     = 1,
  GAME_FLAME_TUG        = 2,
  GAME_METEOR_DEFLECTOR = 3,
  GAME_CYLON_CLASH      = 4,
  GAME_TERRITORY_PAINT  = 5,
  NUM_GAMES             = 6
};

const char* const GAME_NAMES[NUM_GAMES] = {
  "Resonance Pulse",
  "Sparkle Rush",
  "Flame Tug",
  "Meteor Deflector",
  "Cylon Clash",
  "Territory Paint"
};

class RotaryGameKnob {
private:
  uint8_t pin;
  int lastRaw;
  GameId currentGame;
  unsigned long lastReadTime;

  GameId rawToGame(int raw) const {
    int clamped = constrain(raw, 0, 1023);
    int slot = clamped / 171; // 6 slots: 0..5
    if (slot >= NUM_GAMES) slot = NUM_GAMES - 1;
    return (GameId)slot;
  }

public:
  RotaryGameKnob(uint8_t p)
    : pin(p), lastRaw(-1), currentGame(GAME_RESONANCE_PULSE), lastReadTime(0) {}

  void begin() {
    pinMode(pin, INPUT);
    update(true);
  }

  bool update(bool force = false) {
    unsigned long now = millis();
    if (!force && now - lastReadTime < 40) return false;
    lastReadTime = now;

    int raw = readAnalogStable(pin);

    // Deadband filter to prevent jitter right at the boundary between game zones
    int deadband = (now - lastBrightnessTurnTime < 400) ? 24 : 12;

    if (force || lastRaw < 0 || abs(raw - lastRaw) > deadband) {
      lastRaw = raw;
      GameId newGame = rawToGame(raw);
      if (newGame != currentGame) {
        currentGame = newGame;
        return true; // Game selection changed!
      }
    }
    return false;
  }

  GameId getSelectedGame() const { return currentGame; }
  const char* getSelectedGameName() const { return GAME_NAMES[currentGame]; }
  int getRaw() const { return lastRaw; }
};

RotaryGameKnob gameKnob(PIN_GAME_KNOB);

// ============================================================================
// 5. Global Rotary Brightness Knob (Analog Pin A2)
// ============================================================================
class RotaryBrightnessKnob {
private:
  uint8_t pin;
  int lastRaw;
  uint8_t currentBrightness;
  unsigned long lastReadTime;
  bool virtualOverride;
  int potAnchorRaw;

  void applyRaw(int raw) {
    uint8_t newB = (uint8_t)(((uint32_t)raw * 255UL) / 1023UL);
    if (newB != currentBrightness) {
      currentBrightness = newB;
      lastBrightnessTurnTime = millis();
    }
  }

public:
  RotaryBrightnessKnob(uint8_t p)
    : pin(p), lastRaw(-1), currentBrightness(200), lastReadTime(0),
      virtualOverride(false), potAnchorRaw(-1) {}

  void begin() {
    pinMode(pin, INPUT);
    update(true);
  }

  void update(bool force = false) {
    unsigned long now = millis();
    if (!force && now - lastReadTime < 35) return;
    lastReadTime = now;

    int raw = readAnalogStable(pin);

    if (virtualOverride) {
      if (potAnchorRaw >= 0 && abs(raw - potAnchorRaw) > 20) {
        virtualOverride = false;
        lastRaw = raw;
        applyRaw(raw);
      }
      return;
    }

    if (force || lastRaw < 0 || abs(raw - lastRaw) > 6) {
      lastRaw = raw;
      applyRaw(raw);
    }
  }

  uint8_t getBrightness() const { return currentBrightness; }

  void setBrightness(uint8_t b) {
    currentBrightness = b;
    lastBrightnessTurnTime = millis();
    virtualOverride = true;
    potAnchorRaw = readAnalogStable(pin);
  }

  void stepBrightness(int16_t delta = 25) {
    int nextB = (int)currentBrightness + delta;
    if (nextB > 255) nextB = (delta > 0) ? 25 : 255;
    if (nextB < 0) nextB = 0;
    setBrightness((uint8_t)nextB);
  }

  bool isVirtualOverride() const { return virtualOverride; }
};

RotaryBrightnessKnob brightnessKnob(PIN_BRIGHTNESS_KNOB);

// ============================================================================
// 6. Global Rotary Mood / Base-Color Knob (Analog Pin A3)
// ============================================================================
class RotaryColorKnob {
private:
  uint8_t pin;
  int lastRaw;
  uint16_t currentHue;
  uint8_t currentR, currentG, currentB;
  bool isWhiteMode;
  unsigned long lastReadTime;
  bool virtualOverride;
  int potAnchorRaw;

  static const int WHITE_ZONE_EXIT  = 1005;
  static const int WHITE_ZONE_ENTER = 1015;

  void applyRaw(int raw) {
    if (isWhiteMode) {
      if (raw < WHITE_ZONE_EXIT) isWhiteMode = false;
    } else {
      if (raw > WHITE_ZONE_ENTER) isWhiteMode = true;
    }

    if (isWhiteMode) {
      currentHue = 0;
      currentR = 86;
      currentG = 86;
      currentB = 86;
    } else {
      int clampedRaw = min(1010, max(0, raw));
      currentHue = (uint16_t)(((uint32_t)clampedRaw * 65535UL) / 1010UL);
      uint32_t rgb = FastLEDDriver::ColorConstantBrightness(currentHue);
      currentR = (uint8_t)(rgb >> 16);
      currentG = (uint8_t)(rgb >> 8);
      currentB = (uint8_t)rgb;
    }
  }

public:
  RotaryColorKnob(uint8_t p)
    : pin(p), lastRaw(-1), currentHue(0), currentR(157), currentG(0), currentB(0),
      isWhiteMode(false), lastReadTime(0), virtualOverride(false), potAnchorRaw(-1) {}

  void begin() {
    pinMode(pin, INPUT);
    update(true);
  }

  void update(bool force = false) {
    unsigned long now = millis();
    if (!force && now - lastReadTime < 35) return;
    lastReadTime = now;

    int raw = readAnalogStable(pin);

    if (virtualOverride) {
      if (potAnchorRaw >= 0 && abs(raw - potAnchorRaw) > 20) {
        virtualOverride = false;
        lastRaw = raw;
        applyRaw(raw);
      }
      return;
    }

    int deadband = (now - lastBrightnessTurnTime < 400) ? 24 : 8;

    if (force || lastRaw < 0 || abs(raw - lastRaw) > deadband) {
      lastRaw = raw;
      applyRaw(raw);
    }
  }

  bool isWhite() const { return isWhiteMode; }
  uint16_t getHue() const { return currentHue; }
  uint8_t getR() const { return currentR; }
  uint8_t getG() const { return currentG; }
  uint8_t getB() const { return currentB; }
  uint32_t getRGB() const {
    return ((uint32_t)currentR << 16) | ((uint32_t)currentG << 8) | currentB;
  }

  void setWhite() {
    isWhiteMode = true;
    currentHue = 0;
    currentR = 86; currentG = 86; currentB = 86;
    virtualOverride = true;
    potAnchorRaw = readAnalogStable(pin);
  }

  void setHue(uint16_t hue) {
    isWhiteMode = false;
    currentHue = hue;
    uint32_t rgb = FastLEDDriver::ColorConstantBrightness(currentHue);
    currentR = (uint8_t)(rgb >> 16);
    currentG = (uint8_t)(rgb >> 8);
    currentB = (uint8_t)rgb;
    virtualOverride = true;
    potAnchorRaw = readAnalogStable(pin);
  }

  void stepHue(int16_t delta) {
    if (isWhiteMode) {
      setHue(0);
    } else if (currentHue >= 65535) {
      setWhite();
    } else {
      uint32_t next = (uint32_t)currentHue + delta;
      if (next >= 65535) setHue(65535);
      else setHue((uint16_t)next);
    }
  }

  bool isVirtualOverride() const { return virtualOverride; }
};

RotaryColorKnob rotaryColorKnob(PIN_COLOR_KNOB);
uint32_t global_color = FastLEDDriver::Color(157, 0, 0);

// ============================================================================
// 7. Ambient Light Modes Enum & Global Light Behavior Knob (Analog Pin A4)
// Selects one of the 6 ambient lighting modes across 6 equal dial zones
// ============================================================================
enum LightModeId {
  LIGHT_BREATHING_PULSE = 0,
  LIGHT_TWINKLE_SPARKLE = 1,
  LIGHT_FIRE_FLAME      = 2,
  LIGHT_COMET_METEOR    = 3,
  LIGHT_CYLON_SCANNER   = 4,
  LIGHT_COLOR_WIPE      = 5,
  NUM_LIGHT_MODES       = 6
};

const char* const LIGHT_MODE_NAMES[NUM_LIGHT_MODES] = {
  "Breathing / Pulse",
  "Twinkle / Sparkle",
  "Fire / Flame",
  "Comet / Meteor",
  "Scanner / Cylon",
  "Color Wipe"
};

class GlobalLightBehaviorKnob {
private:
  uint8_t pin;
  int lastRaw;
  LightModeId currentMode;
  unsigned long lastReadTime;

  LightModeId rawToMode(int raw) const {
    int clamped = constrain(raw, 0, 1023);
    int slot = clamped / 171; // 6 slots: 0..5
    if (slot >= NUM_LIGHT_MODES) slot = NUM_LIGHT_MODES - 1;
    return (LightModeId)slot;
  }

public:
  GlobalLightBehaviorKnob(uint8_t p)
    : pin(p), lastRaw(-1), currentMode(LIGHT_BREATHING_PULSE), lastReadTime(0) {}

  void begin() {
    pinMode(pin, INPUT);
    update(true);
  }

  bool update(bool force = false) {
    unsigned long now = millis();
    if (!force && now - lastReadTime < 40) return false;
    lastReadTime = now;

    int raw = readAnalogStable(pin);

    int deadband = (now - lastBrightnessTurnTime < 400) ? 24 : 12;

    if (force || lastRaw < 0 || abs(raw - lastRaw) > deadband) {
      lastRaw = raw;
      LightModeId newMode = rawToMode(raw);
      if (newMode != currentMode) {
        currentMode = newMode;
        return true; // Light behavior selection changed!
      }
    }
    return false;
  }

  LightModeId getSelectedMode() const { return currentMode; }
  const char* getSelectedModeName() const { return LIGHT_MODE_NAMES[currentMode]; }
  int getRaw() const { return lastRaw; }
};

GlobalLightBehaviorKnob lightBehaviorKnob(PIN_LIGHT_BEHAVIOR_KNOB);

// ============================================================================
// 8. Debounced Button Input (Two Players)
// ============================================================================
class Button {
private:
  uint8_t pin;
  bool lastState;
  bool stableState;
  unsigned long lastDebounceTime;
  const unsigned long debounceDelay = 25; // ms
  bool pressedEvent;

public:
  Button(uint8_t p) : pin(p), lastState(HIGH), stableState(HIGH), lastDebounceTime(0), pressedEvent(false) {}

  void begin() {
    pinMode(pin, INPUT_PULLUP);
  }

  void update() {
    bool reading = digitalRead(pin);
    if (reading != lastState) {
      lastDebounceTime = millis();
      lastState = reading;
    }

    if ((millis() - lastDebounceTime) >= debounceDelay) {
      if (reading != stableState) {
        stableState = reading;
        if (stableState == LOW) {
          pressedEvent = true;
        }
      }
    }
  }

  bool wasPressed() {
    if (pressedEvent) {
      pressedEvent = false;
      return true;
    }
    return false;
  }

  bool isDown() const {
    return stableState == LOW;
  }

  void triggerVirtualPress() {
    pressedEvent = true;
  }
};

Button btnP1(PIN_BTN_P1);
Button btnP2(PIN_BTN_P2);

LightModeId currentLightMode = LIGHT_BREATHING_PULSE;
bool inGameMode = false;

// ============================================================================
// 9. Ambient Light Modes (Separated from Game Logic)
// ============================================================================

// --- Light Mode 1: Breathing / Pulse ---
class BreathingPulseLight {
private:
  unsigned long lastUpdate = 0;
  const unsigned long BREATH_PERIOD = 7500; // 7.5s ultra-slow soothing breath

public:
  void enter() { lastUpdate = 0; }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 25) return;
    lastUpdate = currentMillis;

    float phase = (float)(currentMillis % BREATH_PERIOD) / (float)BREATH_PERIOD;
    float wave = (sin(phase * 2.0f * PI) + 1.0f) * 0.5f;

    float factor = 0.42f + wave * 0.40f;
    uint8_t r = (uint8_t)(rotaryColorKnob.getR() * factor);
    uint8_t g = (uint8_t)(rotaryColorKnob.getG() * factor);
    uint8_t b = (uint8_t)(rotaryColorKnob.getB() * factor);
    uint32_t color = FastLEDDriver::Color(r, g, b);

    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, color);
    }
    strip.show();
  }
};

// --- Light Mode 2: Twinkle / Sparkle ---
class TwinkleSparkleLight {
private:
  unsigned long lastUpdate = 0;
  uint8_t twinkleBrightness[NUM_LEDS];
  uint8_t twinkleDeltaBits[(NUM_LEDS + 7) / 8];

  bool getDirectionUp(uint16_t i) const {
    return (twinkleDeltaBits[i >> 3] >> (i & 7)) & 1;
  }

  void setDirectionUp(uint16_t i, bool up) {
    if (up) twinkleDeltaBits[i >> 3] |= (1 << (i & 7));
    else    twinkleDeltaBits[i >> 3] &= ~(1 << (i & 7));
  }

public:
  TwinkleSparkleLight() {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      twinkleBrightness[i] = random(55, 160);
      setDirectionUp(i, random(0, 2) == 0);
    }
  }

  void enter() { lastUpdate = 0; }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 50) return;
    lastUpdate = currentMillis;

    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      int nextB = (int)twinkleBrightness[i] + (getDirectionUp(i) ? 1 : -1);
      if (nextB >= 220) {
        setDirectionUp(i, false);
      } else if (nextB <= 58) {
        setDirectionUp(i, true);
      }
      twinkleBrightness[i] = (uint8_t)constrain(nextB, 58, 220);
    }

    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();

    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      uint8_t br = twinkleBrightness[i];
      uint8_t r = (uint8_t)(((uint16_t)baseR * br) >> 8);
      uint8_t g = (uint8_t)(((uint16_t)baseG * br) >> 8);
      uint8_t b = (uint8_t)(((uint16_t)baseB * br) >> 8);
      if (br > 195) {
        uint8_t spark = (br - 195) * 2;
        r = min(255, (int)r + spark);
        g = min(255, (int)g + spark);
        b = min(255, (int)b + spark);
      }
      strip.setPixelColor(i, FastLEDDriver::Color(r, g, b));
    }
    strip.show();
  }
};

// --- Light Mode 3: Fire / Flame ---
class FireFlameLight {
private:
  unsigned long lastUpdate = 0;
  uint8_t heat[NUM_LEDS];

  uint32_t heatToColor(uint8_t temp) {
    temp = max((uint8_t)80, temp);
    uint8_t t192 = (uint8_t)(((uint16_t)temp * 191) >> 8);
    uint8_t ramp = (t192 & 0x3F) << 2;

    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();

    if (t192 > 0x80) { // Hottest: blend toward white hot
      uint8_t r = min(255, (int)baseR + ramp);
      uint8_t g = min(255, (int)baseG + ramp);
      uint8_t b = min(255, (int)baseB + ramp);
      return FastLEDDriver::Color(r, g, b);
    } else if (t192 > 0x40) { // Medium: full saturation of base color
      uint8_t factor = (t192 << 1);
      uint8_t r = (uint8_t)(((uint16_t)baseR * factor) >> 8);
      uint8_t g = (uint8_t)(((uint16_t)baseG * factor) >> 8);
      uint8_t b = (uint8_t)(((uint16_t)baseB * factor) >> 8);
      return FastLEDDriver::Color(r, g, b);
    } else { // Coolest: dark ember base color
      uint8_t factor = max((uint8_t)60, (uint8_t)(t192 << 2));
      uint8_t r = (uint8_t)(((uint16_t)baseR * factor) >> 8);
      uint8_t g = (uint8_t)(((uint16_t)baseG * factor) >> 8);
      uint8_t b = (uint8_t)(((uint16_t)baseB * factor) >> 8);
      return FastLEDDriver::Color(r, g, b);
    }
  }

public:
  FireFlameLight() {
    memset(heat, 90, sizeof(heat));
  }

  void enter() { lastUpdate = 0; }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 60) return;
    lastUpdate = currentMillis;

    // Cooling
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      heat[i] = (heat[i] > 79) ? (heat[i] - 1) : 78;
    }

    // Thermal diffusion
    for (int k = NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }
    if (random(0, 10) < 2) {
      int sparkPos = random(0, NUM_LEDS);
      heat[sparkPos] = min(255, heat[sparkPos] + random(60, 110));
    }
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, heatToColor(heat[i]));
    }
    strip.show();
  }
};

// --- Light Mode 4: Comet / Meteor ---
class CometMeteorLight {
private:
  unsigned long lastUpdate = 0;
  float headPos = 0.0f;
  float speed = 0.8f;
  int8_t direction = 1;

public:
  void enter() {
    lastUpdate = 0;
    headPos = 0.0f;
    direction = 1;
    speed = max(0.25f, (float)NUM_LEDS * 0.0035f);
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 40) return;
    lastUpdate = currentMillis;

    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();

    headPos += speed * direction;
    if (headPos >= (float)(NUM_LEDS - 1)) {
      direction = -1;
      headPos = (float)(NUM_LEDS - 1);
    } else if (headPos <= 0.0f) {
      direction = 1;
      headPos = 0.0f;
    }

    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, FastLEDDriver::Color(
        (baseR * 65) >> 8,
        (baseG * 65) >> 8,
        (baseB * 65) >> 8
      ));
    }

    uint16_t tailLength = max((uint16_t)3, (uint16_t)(NUM_LEDS * 0.20f + 0.5f));
    for (uint16_t t = 0; t < tailLength; t++) {
      float p = headPos - (t * direction);
      int idx = (int)round(p);
      if (idx >= 0 && idx < NUM_LEDS) {
        uint8_t factor = (uint8_t)(((uint32_t)(tailLength - t) * 240) / tailLength);
        uint8_t r = (uint8_t)(((uint16_t)baseR * factor) >> 8);
        uint8_t g = (uint8_t)(((uint16_t)baseG * factor) >> 8);
        uint8_t b = (uint8_t)(((uint16_t)baseB * factor) >> 8);
        if (t == 0) {
          r = min(255, (int)r + 160);
          g = min(255, (int)g + 160);
          b = min(255, (int)b + 160);
        }
        strip.setPixelColor(idx, FastLEDDriver::Color(r, g, b));
      }
    }
    strip.show();
  }
};

// --- Light Mode 5: Scanner / Cylon ---
class CylonScannerLight {
private:
  unsigned long lastUpdate = 0;
  const unsigned long SWEEP_PERIOD = 6000;

public:
  void enter() { lastUpdate = 0; }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 35) return;
    lastUpdate = currentMillis;

    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();

    float phase = (float)(currentMillis % SWEEP_PERIOD) / (float)SWEEP_PERIOD;
    float center = (float)(NUM_LEDS - 1) * 0.5f;
    float margin = max(0.5f, min(2.0f, (float)NUM_LEDS * 0.05f));
    float amplitude = max(0.5f, center - margin);
    float eyePos = center + amplitude * sin(phase * 2.0f * PI);
    float glowRadius = max(2.0f, min(36.0f, (float)NUM_LEDS * 0.06f));

    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      float dist = abs((float)i - eyePos);
      if (dist < 1.0f) {
        strip.setPixelColor(i, FastLEDDriver::Color(
          min(255, (int)baseR + 80),
          min(255, (int)baseG + 80),
          min(255, (int)baseB + 80)
        ));
      } else if (dist < glowRadius) {
        uint8_t glow = (uint8_t)(255 * (1.0f - dist / glowRadius));
        strip.setPixelColor(i, FastLEDDriver::Color(
          ((uint16_t)baseR * glow) >> 8,
          ((uint16_t)baseG * glow) >> 8,
          ((uint16_t)baseB * glow) >> 8
        ));
      } else {
        strip.setPixelColor(i, FastLEDDriver::Color(
          (baseR * 60) >> 8,
          (baseG * 60) >> 8,
          (baseB * 60) >> 8
        ));
      }
    }
    strip.show();
  }
};

// --- Light Mode 6: Color Wipe ---
class ColorWipeLight {
private:
  unsigned long lastUpdate = 0;
  uint16_t wipeIdx = 0;
  uint8_t paletteIdx = 0;

public:
  void enter() {
    lastUpdate = 0;
    wipeIdx = 0;
    paletteIdx = 0;
  }

  void update(unsigned long currentMillis) {
    unsigned long interval = max((unsigned long)15, (unsigned long)(10000UL / NUM_LEDS));
    if (currentMillis - lastUpdate < interval) return;
    lastUpdate = currentMillis;

    uint32_t wipeCol;
    if (rotaryColorKnob.isWhite()) {
      const uint32_t whiteTints[4] = {
        FastLEDDriver::Color(86, 86, 86), // Pure Crisp White
        FastLEDDriver::Color(96, 83, 64), // Warm Candle White
        FastLEDDriver::Color(78, 86, 96), // Cool Daylight White
        FastLEDDriver::Color(90, 87, 76)  // Soft Neutral White
      };
      wipeCol = whiteTints[paletteIdx % 4];
    } else {
      uint16_t baseHue = rotaryColorKnob.getHue();
      uint16_t wipeHue = baseHue + (paletteIdx * 10922);
      wipeCol = FastLEDDriver::ColorConstantBrightness(wipeHue);
    }

    strip.setPixelColor(wipeIdx, wipeCol);
    wipeIdx++;
    if (wipeIdx >= NUM_LEDS) {
      wipeIdx = 0;
      paletteIdx = (paletteIdx + 1) % 4;
    }
    strip.show();
  }
};

// ============================================================================
// 10. Competitive 2-Player Arcade Games (Extracted Classes)
// ============================================================================

// --- Game 1: "Resonance Pulse" (Rhythm Wave Tug-of-War) ---
class ResonancePulseGame {
private:
  unsigned long lastUpdate = 0;
  const unsigned long BREATH_PERIOD = 7500;
  int16_t nexusPosition;
  unsigned long p1Cooldown;
  unsigned long p2Cooldown;
  bool roundOver;
  unsigned long roundOverTime;

public:
  ResonancePulseGame() : nexusPosition(NUM_LEDS / 2), p1Cooldown(0), p2Cooldown(0), roundOver(false), roundOverTime(0) {}

  void reset() {
    nexusPosition = NUM_LEDS / 2;
    p1Cooldown = 0;
    p2Cooldown = 0;
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 25) return;
    lastUpdate = currentMillis;

    if (roundOver) {
      if (currentMillis - roundOverTime > 2000) reset();
      return;
    }

    float phase = (float)(currentMillis % BREATH_PERIOD) / (float)BREATH_PERIOD;
    float wave = (sin(phase * 2.0f * PI) + 1.0f) * 0.5f;
    bool inResonance = (wave >= 0.88f);
    int16_t pushStep = max(1, (int)(NUM_LEDS / 15));

    if (btnP1.wasPressed()) {
      if (currentMillis >= p1Cooldown) {
        if (inResonance) {
          nexusPosition += pushStep;
          if (nexusPosition >= (int16_t)NUM_LEDS - 2) {
            roundOver = true;
            roundOverTime = currentMillis;
            nexusPosition = NUM_LEDS - 1;
          }
        } else {
          p1Cooldown = currentMillis + 400; // Misfire penalty cooldown
        }
      }
    }

    if (btnP2.wasPressed()) {
      if (currentMillis >= p2Cooldown) {
        if (inResonance) {
          nexusPosition -= pushStep;
          if (nexusPosition <= 1) {
            roundOver = true;
            roundOverTime = currentMillis;
            nexusPosition = 0;
          }
        } else {
          p2Cooldown = currentMillis + 400; // Misfire penalty cooldown
        }
      }
    }

    // Ambient background floor in mood color
    float baseFactor = 0.35f + wave * 0.20f;
    uint8_t rBase = (uint8_t)(rotaryColorKnob.getR() * baseFactor);
    uint8_t gBase = (uint8_t)(rotaryColorKnob.getG() * baseFactor);
    uint8_t bBase = (uint8_t)(rotaryColorKnob.getB() * baseFactor);
    uint32_t baseCol = FastLEDDriver::Color(rBase, gBase, bBase);
    for (uint16_t i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, baseCol);

    int glowWidth = max(2, (int)(NUM_LEDS * 0.13f + 0.5f));
    for (int i = 0; i <= nexusPosition; i++) {
      int dist = nexusPosition - i;
      uint8_t b = (dist < glowWidth) ? (255 - (dist * 210) / glowWidth) : 45;
      strip.setPixelColor(i, FastLEDDriver::Color(15, (b * 130) >> 8, b));
    }
    for (int i = nexusPosition; i < NUM_LEDS; i++) {
      int dist = i - nexusPosition;
      uint8_t r = (dist < glowWidth) ? (255 - (dist * 210) / glowWidth) : 45;
      strip.setPixelColor(i, FastLEDDriver::Color(r, (r * 65) >> 8, 15));
    }

    uint32_t nexusCol = inResonance ? FastLEDDriver::Color(255, 255, 240) : FastLEDDriver::Color(255, 190, 60);
    if (nexusPosition >= 0 && nexusPosition < NUM_LEDS) strip.setPixelColor(nexusPosition, nexusCol);
    strip.show();
  }
};

// --- Game 2: "Sparkle Rush" (Nova Sparkle Reflex Deflector) ---
class SparkleRushGame {
private:
  unsigned long lastUpdate = 0;
  float novaPos;
  float novaSpeed;
  bool roundOver;
  unsigned long roundOverTime;

public:
  SparkleRushGame()
    : novaPos((float)NUM_LEDS * 0.5f),
      novaSpeed(max(0.25f, (float)NUM_LEDS * (0.95f / 60.0f))),
      roundOver(false), roundOverTime(0) {}

  void reset() {
    novaPos = (float)NUM_LEDS * 0.5f;
    float baseSpeed = max(0.25f, (float)NUM_LEDS * (0.95f / 60.0f));
    novaSpeed = (random(0, 2) == 0 ? baseSpeed : -baseSpeed);
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 20) return;
    lastUpdate = currentMillis;

    if (roundOver) {
      if (currentMillis - roundOverTime > 2000) reset();
      return;
    }

    novaPos += novaSpeed;
    uint16_t shieldLeds = max((uint16_t)1, (uint16_t)((NUM_LEDS * 15 + 50) / 100)); // ~15%
    float maxSpeed = max(0.8f, (float)NUM_LEDS * (2.8f / 60.0f));

    if (btnP1.wasPressed()) {
      if (novaPos >= 0.0f && novaPos <= (float)shieldLeds && novaSpeed < 0.0f) {
        float bonus = ((float)shieldLeds - novaPos) * (1.5f / (float)shieldLeds);
        novaSpeed = abs(novaSpeed) * 1.12f + bonus;
        novaSpeed = min(novaSpeed, maxSpeed);
      }
    }

    if (btnP2.wasPressed()) {
      float p2ZoneStart = (float)(NUM_LEDS - 1 - shieldLeds);
      if (novaPos >= p2ZoneStart && novaPos <= (float)(NUM_LEDS - 1) && novaSpeed > 0.0f) {
        float bonus = (novaPos - p2ZoneStart) * (1.5f / (float)shieldLeds);
        novaSpeed = -(abs(novaSpeed) * 1.12f + bonus);
        novaSpeed = max(novaSpeed, -maxSpeed);
      }
    }

    if (novaPos < -1.0f || novaPos > (float)NUM_LEDS) {
      roundOver = true;
      roundOverTime = currentMillis;
    }

    // Baseline starfield floor
    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, FastLEDDriver::Color((baseR * 45) >> 8, (baseG * 45) >> 8, (baseB * 45) >> 8));
    }

    // Defense Shields
    for (uint16_t i = 0; i < shieldLeds; i++) strip.setPixelColor(i, FastLEDDriver::Color(30, 110, 210));
    for (uint16_t i = NUM_LEDS - shieldLeds; i < NUM_LEDS; i++) strip.setPixelColor(i, FastLEDDriver::Color(210, 50, 140));

    // Nova head & halo
    int center = (int)round(novaPos);
    if (center >= 0 && center < NUM_LEDS) {
      strip.setPixelColor(center, FastLEDDriver::Color(255, 255, 240));
      if (center > 0) strip.setPixelColor(center - 1, FastLEDDriver::Color(255, 200, 50));
      if (center < NUM_LEDS - 1) strip.setPixelColor(center + 1, FastLEDDriver::Color(255, 200, 50));
    }
    strip.show();
  }
};

// --- Game 3: "Flame Tug" (Bellows Forge Combustion Clash) ---
class FlameTugGame {
private:
  unsigned long lastUpdate = 0;
  int16_t flameClashPos;
  unsigned long lastTapP1;
  unsigned long lastTapP2;
  bool roundOver;
  unsigned long roundOverTime;

public:
  FlameTugGame()
    : flameClashPos(NUM_LEDS / 2), lastTapP1(0), lastTapP2(0), roundOver(false), roundOverTime(0) {}

  void reset() {
    flameClashPos = NUM_LEDS / 2;
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 25) return;
    lastUpdate = currentMillis;

    if (roundOver) {
      if (currentMillis - roundOverTime > 2000) reset();
      return;
    }

    int16_t pushStep = max(1, (int)(NUM_LEDS / 30));

    if (btnP1.wasPressed()) {
      if (currentMillis - lastTapP1 > 120) { // 120ms anti-spam window
        lastTapP1 = currentMillis;
        flameClashPos += pushStep;
        if (flameClashPos >= (int16_t)NUM_LEDS - 2) {
          roundOver = true;
          roundOverTime = currentMillis;
          flameClashPos = NUM_LEDS - 1;
        }
      }
    }

    if (btnP2.wasPressed()) {
      if (currentMillis - lastTapP2 > 120) { // 120ms anti-spam window
        lastTapP2 = currentMillis;
        flameClashPos -= pushStep;
        if (flameClashPos <= 1) {
          roundOver = true;
          roundOverTime = currentMillis;
          flameClashPos = 0;
        }
      }
    }

    // P1: Blue Plasma Forge (0..clash) -> gradients from deep blue to white-hot
    for (int i = 0; i <= flameClashPos; i++) {
      uint8_t factor = (uint8_t)(((uint32_t)i * 255UL) / max(1, flameClashPos));
      uint8_t r = (factor > 180) ? (factor - 180) * 3 : 0;
      uint8_t g = (factor * 160) >> 8;
      uint8_t b = 70 + ((uint16_t)factor * 185) / 255;
      strip.setPixelColor(i, FastLEDDriver::Color(r, g, b));
    }

    // P2: Red Inferno Forge (clash..NUM_LEDS) -> gradients from white-hot to deep red
    for (int i = flameClashPos + 1; i < NUM_LEDS; i++) {
      int distFromClash = i - flameClashPos;
      int span = NUM_LEDS - 1 - flameClashPos;
      uint8_t factor = (uint8_t)(255 - (((uint32_t)distFromClash * 255UL) / max(1, span)));
      uint8_t r = 70 + ((uint16_t)factor * 185) / 255;
      uint8_t g = (factor * 100) >> 8;
      uint8_t b = (factor > 180) ? (factor - 180) * 3 : 0;
      strip.setPixelColor(i, FastLEDDriver::Color(r, g, b));
    }

    if (flameClashPos >= 0 && flameClashPos < NUM_LEDS) {
      strip.setPixelColor(flameClashPos, FastLEDDriver::Color(255, 255, 240));
    }
    strip.show();
  }
};

// --- Game 4: "Meteor Deflector" (High-Speed Comet Rally) ---
class MeteorDeflectorGame {
private:
  unsigned long lastUpdate = 0;
  float gameMeteorPos;
  float gameMeteorSpeed;
  bool roundOver;
  unsigned long roundOverTime;

public:
  MeteorDeflectorGame()
    : gameMeteorPos((float)NUM_LEDS * 0.5f),
      gameMeteorSpeed(max(0.3f, (float)NUM_LEDS * (1.1f / 60.0f))),
      roundOver(false), roundOverTime(0) {}

  void reset() {
    gameMeteorPos = (float)NUM_LEDS * 0.5f;
    float baseSpeed = max(0.3f, (float)NUM_LEDS * (1.1f / 60.0f));
    gameMeteorSpeed = (random(0, 2) == 0 ? baseSpeed : -baseSpeed);
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 20) return;
    lastUpdate = currentMillis;

    if (roundOver) {
      if (currentMillis - roundOverTime > 2000) reset();
      return;
    }

    gameMeteorPos += gameMeteorSpeed;
    uint16_t defenseLeds = max((uint16_t)1, (uint16_t)((NUM_LEDS * 15 + 50) / 100)); // ~15%
    float maxSpeed = max(0.8f, (float)NUM_LEDS * (3.2f / 60.0f));

    if (btnP1.wasPressed()) {
      if (gameMeteorPos >= 0.0f && gameMeteorPos <= (float)defenseLeds && gameMeteorSpeed < 0.0f) {
        float bonus = ((float)defenseLeds - gameMeteorPos) * (1.8f / (float)defenseLeds);
        gameMeteorSpeed = abs(gameMeteorSpeed) * 1.14f + bonus;
        gameMeteorSpeed = min(gameMeteorSpeed, maxSpeed);
      }
    }

    if (btnP2.wasPressed()) {
      float p2ZoneStart = (float)(NUM_LEDS - 1 - defenseLeds);
      if (gameMeteorPos >= p2ZoneStart && gameMeteorPos <= (float)(NUM_LEDS - 1) && gameMeteorSpeed > 0.0f) {
        float bonus = (gameMeteorPos - p2ZoneStart) * (1.8f / (float)defenseLeds);
        gameMeteorSpeed = -(abs(gameMeteorSpeed) * 1.14f + bonus);
        gameMeteorSpeed = max(gameMeteorSpeed, -maxSpeed);
      }
    }

    if (gameMeteorPos < -1.0f || gameMeteorPos > (float)NUM_LEDS) {
      roundOver = true;
      roundOverTime = currentMillis;
    }

    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, FastLEDDriver::Color((baseR * 50) >> 8, (baseG * 50) >> 8, (baseB * 50) >> 8));
    }
    for (uint16_t i = 0; i < defenseLeds; i++) strip.setPixelColor(i, FastLEDDriver::Color(10, 80, 220));
    for (uint16_t i = NUM_LEDS - defenseLeds; i < NUM_LEDS; i++) strip.setPixelColor(i, FastLEDDriver::Color(220, 60, 10));

    int idx = (int)round(gameMeteorPos);
    if (idx >= 0 && idx < NUM_LEDS) {
      strip.setPixelColor(idx, FastLEDDriver::Color(255, 255, 240));
      if (idx > 0) strip.setPixelColor(idx - 1, FastLEDDriver::Color(255, 180, 40));
      if (idx < NUM_LEDS - 1) strip.setPixelColor(idx + 1, FastLEDDriver::Color(255, 180, 40));
    }
    strip.show();
  }
};

// --- Game 5: "Cylon Clash" (Hyper-Pong Beam Duel) ---
class CylonClashGame {
private:
  unsigned long lastUpdate = 0;
  float puckPos;
  float puckSpeed;
  bool roundOver;
  unsigned long roundOverTime;

public:
  CylonClashGame()
    : puckPos((float)NUM_LEDS * 0.5f),
      puckSpeed(max(0.3f, (float)NUM_LEDS * (1.2f / 60.0f))),
      roundOver(false), roundOverTime(0) {}

  void reset() {
    puckPos = (float)NUM_LEDS * 0.5f;
    float baseSpeed = max(0.3f, (float)NUM_LEDS * (1.2f / 60.0f));
    puckSpeed = (random(0, 2) == 0 ? baseSpeed : -baseSpeed);
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 18) return;
    lastUpdate = currentMillis;

    if (roundOver) {
      if (currentMillis - roundOverTime > 2000) reset();
      return;
    }

    puckPos += puckSpeed;
    uint16_t defenseLeds = max((uint16_t)1, (uint16_t)((NUM_LEDS * 15 + 50) / 100)); // ~15%
    float maxSpeed = max(0.8f, (float)NUM_LEDS * (3.4f / 60.0f));

    if (btnP1.wasPressed()) {
      if (puckPos >= 0.0f && puckPos <= (float)defenseLeds && puckSpeed < 0.0f) {
        float bonus = ((float)defenseLeds - puckPos) * (1.6f / (float)defenseLeds);
        puckSpeed = abs(puckSpeed) * 1.15f + bonus;
        puckSpeed = min(puckSpeed, maxSpeed);
      }
    }

    if (btnP2.wasPressed()) {
      float p2ZoneStart = (float)(NUM_LEDS - 1 - defenseLeds);
      if (puckPos >= p2ZoneStart && puckPos <= (float)(NUM_LEDS - 1) && puckSpeed > 0.0f) {
        float bonus = (puckPos - p2ZoneStart) * (1.6f / (float)defenseLeds);
        puckSpeed = -(abs(puckSpeed) * 1.15f + bonus);
        puckSpeed = max(puckSpeed, -maxSpeed);
      }
    }

    if (puckPos < -1.0f || puckPos > (float)NUM_LEDS) {
      roundOver = true;
      roundOverTime = currentMillis;
    }

    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, FastLEDDriver::Color((baseR * 50) >> 8, (baseG * 50) >> 8, (baseB * 50) >> 8));
    }
    for (uint16_t i = 0; i < defenseLeds; i++) strip.setPixelColor(i, FastLEDDriver::Color(15, 95, 215));
    for (uint16_t i = NUM_LEDS - defenseLeds; i < NUM_LEDS; i++) strip.setPixelColor(i, FastLEDDriver::Color(215, 75, 15));

    int idx = (int)round(puckPos);
    if (idx >= 0 && idx < NUM_LEDS) {
      strip.setPixelColor(idx, FastLEDDriver::Color(255, 255, 240));
      if (idx > 0) strip.setPixelColor(idx - 1, FastLEDDriver::Color(255, 180, 50));
      if (idx < NUM_LEDS - 1) strip.setPixelColor(idx + 1, FastLEDDriver::Color(255, 180, 50));
    }
    strip.show();
  }
};

// --- Game 6: "Territory Paint" (Rapid Wipe Wars) ---
class TerritoryPaintGame {
private:
  unsigned long lastUpdate = 0;
  int16_t paintBoundary;
  bool roundOver;
  unsigned long roundOverTime;

public:
  TerritoryPaintGame() : paintBoundary(NUM_LEDS / 2), roundOver(false), roundOverTime(0) {}

  void reset() {
    paintBoundary = NUM_LEDS / 2;
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 20) return;
    lastUpdate = currentMillis;

    if (roundOver) {
      if (currentMillis - roundOverTime > 2000) reset();
      return;
    }

    int16_t pushStep = max(1, (int)(NUM_LEDS / 30));

    if (btnP1.wasPressed()) {
      paintBoundary += pushStep;
      if (paintBoundary >= (int16_t)NUM_LEDS - 1) {
        roundOver = true;
        roundOverTime = currentMillis;
        paintBoundary = NUM_LEDS - 1;
      }
    }

    if (btnP2.wasPressed()) {
      paintBoundary -= pushStep;
      if (paintBoundary <= 0) {
        roundOver = true;
        roundOverTime = currentMillis;
        paintBoundary = 0;
      }
    }

    for (int i = 0; i <= paintBoundary; i++) {
      strip.setPixelColor(i, FastLEDDriver::Color(14, 165, 233)); // Sky Blue
    }
    for (int i = paintBoundary + 1; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, FastLEDDriver::Color(225, 29, 72));  // Neon Rose
    }
    if (paintBoundary >= 0 && paintBoundary < NUM_LEDS) {
      strip.setPixelColor(paintBoundary, FastLEDDriver::Color(255, 255, 255));
    }
    strip.show();
  }
};

// ============================================================================
// 11. Mode & Game Instances
// ============================================================================
BreathingPulseLight lightBreathingPulse;
TwinkleSparkleLight lightTwinkleSparkle;
FireFlameLight      lightFireFlame;
CometMeteorLight    lightCometMeteor;
CylonScannerLight   lightCylonScanner;
ColorWipeLight      lightColorWipe;

ResonancePulseGame  gameResonancePulse;
SparkleRushGame     gameSparkleRush;
FlameTugGame        gameFlameTug;
MeteorDeflectorGame gameMeteorDeflector;
CylonClashGame      gameCylonClash;
TerritoryPaintGame  gameTerritoryPaint;

void resetActiveGame(GameId game) {
  switch (game) {
    case GAME_RESONANCE_PULSE:  gameResonancePulse.reset(); break;
    case GAME_SPARKLE_RUSH:     gameSparkleRush.reset(); break;
    case GAME_FLAME_TUG:        gameFlameTug.reset(); break;
    case GAME_METEOR_DEFLECTOR: gameMeteorDeflector.reset(); break;
    case GAME_CYLON_CLASH:      gameCylonClash.reset(); break;
    case GAME_TERRITORY_PAINT:  gameTerritoryPaint.reset(); break;
  }
}

void updateActiveGame(GameId game, unsigned long currentMillis) {
  switch (game) {
    case GAME_RESONANCE_PULSE:  gameResonancePulse.update(currentMillis); break;
    case GAME_SPARKLE_RUSH:     gameSparkleRush.update(currentMillis); break;
    case GAME_FLAME_TUG:        gameFlameTug.update(currentMillis); break;
    case GAME_METEOR_DEFLECTOR: gameMeteorDeflector.update(currentMillis); break;
    case GAME_CYLON_CLASH:      gameCylonClash.update(currentMillis); break;
    case GAME_TERRITORY_PAINT:  gameTerritoryPaint.update(currentMillis); break;
  }
}

void updateActiveLight(LightModeId mode, unsigned long currentMillis) {
  switch (mode) {
    case LIGHT_BREATHING_PULSE: lightBreathingPulse.update(currentMillis); break;
    case LIGHT_TWINKLE_SPARKLE: lightTwinkleSparkle.update(currentMillis); break;
    case LIGHT_FIRE_FLAME:      lightFireFlame.update(currentMillis); break;
    case LIGHT_COMET_METEOR:    lightCometMeteor.update(currentMillis); break;
    case LIGHT_CYLON_SCANNER:   lightCylonScanner.update(currentMillis); break;
    case LIGHT_COLOR_WIPE:      lightColorWipe.update(currentMillis); break;
  }
}

// ============================================================================
// 12. Serial Command Parser
// ============================================================================
void handleSerialCommands() {
  static char numBuf[8];
  static uint8_t numIdx = 0;
  enum NumTarget { TARGET_NONE, TARGET_HUE, TARGET_BRIGHTNESS, TARGET_GAME };
  static NumTarget numTarget = TARGET_NONE;
  static unsigned long lastDigitTime = 0;

  if (numTarget != TARGET_NONE && numIdx > 0 && (millis() - lastDigitTime > 60)) {
    numBuf[numIdx] = '\0';
    long val = atol(numBuf);
    if (numTarget == TARGET_HUE) {
      if (val < 0 || val > 65535) rotaryColorKnob.setWhite();
      else rotaryColorKnob.setHue((uint16_t)val);
      global_color = rotaryColorKnob.getRGB();
    } else if (numTarget == TARGET_BRIGHTNESS) {
      if (val < 0) val = 0;
      if (val > 255) val = 255;
      brightnessKnob.setBrightness((uint8_t)val);
      strip.setBrightness(brightnessKnob.getBrightness());
    } else if (numTarget == TARGET_GAME) {
      if (val >= 0 && val < NUM_GAMES) {
        resetActiveGame((GameId)val);
      }
    }
    numTarget = TARGET_NONE;
    numIdx = 0;
  }

  while (Serial.available()) {
    char c = Serial.read();

    if (numTarget != TARGET_NONE) {
      if (c >= '0' && c <= '9') {
        if (numIdx < sizeof(numBuf) - 1) {
          numBuf[numIdx++] = c;
          lastDigitTime = millis();
        }
        continue;
      } else {
        numBuf[numIdx] = '\0';
        if (numIdx > 0) {
          long val = atol(numBuf);
          if (numTarget == TARGET_HUE) {
            if (val < 0 || val > 65535) rotaryColorKnob.setWhite();
            else rotaryColorKnob.setHue((uint16_t)val);
            global_color = rotaryColorKnob.getRGB();
          } else if (numTarget == TARGET_BRIGHTNESS) {
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            brightnessKnob.setBrightness((uint8_t)val);
            strip.setBrightness(brightnessKnob.getBrightness());
          }
        } else {
          if (numTarget == TARGET_BRIGHTNESS) {
            brightnessKnob.stepBrightness(25);
            strip.setBrightness(brightnessKnob.getBrightness());
          }
        }
        numTarget = TARGET_NONE;
        numIdx = 0;
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t' || c == ';') continue;
      }
    }

    if (c == 'w' || c == 'W') {
      rotaryColorKnob.setWhite();
      global_color = rotaryColorKnob.getRGB();
    } else if (c == 'h' || c == 'H') {
      numTarget = TARGET_HUE;
      numIdx = 0;
      lastDigitTime = millis();
    } else if (c == 'b' || c == 'B') {
      numTarget = TARGET_BRIGHTNESS;
      numIdx = 0;
      lastDigitTime = millis();
    } else if (c == '1') {
      btnP1.triggerVirtualPress();
    } else if (c == '2') {
      btnP2.triggerVirtualPress();
    } else if (c == 'm' || c == 'M') {
      currentLightMode = (LightModeId)(((int)currentLightMode + 1) % NUM_LIGHT_MODES);
      strip.clear();
      Serial.print(F("[Serial] Ambient Mode: "));
      Serial.println(LIGHT_MODE_NAMES[currentLightMode]);
    } else if (c == 'g' || c == 'G') {
      inGameMode = !inGameMode;
      if (inGameMode) {
        resetActiveGame(gameKnob.getSelectedGame());
        strip.clear();
        Serial.print(F("[Game Mode Activated] -> "));
        Serial.println(gameKnob.getSelectedGameName());
      } else {
        strip.clear();
        Serial.println(F("[Game Mode Deactivated] -> Ambient Lighting"));
      }
    } else if (c == 'c' || c == 'C') {
      rotaryColorKnob.stepHue(4000);
      global_color = rotaryColorKnob.getRGB();
    }
  }
}

// ============================================================================
// 13. Arduino setup() and loop()
// ============================================================================
void setup() {
  rotaryColorKnob.begin();
  global_color = rotaryColorKnob.getRGB();

  brightnessKnob.begin();
  gameKnob.begin();
  lightBehaviorKnob.begin();

  btnP1.begin();
  btnP2.begin();

  randomSeed(analogRead(PIN_ENTROPY));

  strip.begin();
  strip.setBrightness(brightnessKnob.getBrightness());

  currentLightMode = lightBehaviorKnob.getSelectedMode();

  Serial.println(F("BarCounter Light & Game System Initialized (FastLED SK6812, 12m)."));
  Serial.print(F("Initial Ambient Mode: "));
  Serial.println(lightBehaviorKnob.getSelectedModeName());
  Serial.print(F("Initial Game Selected: "));
  Serial.println(gameKnob.getSelectedGameName());
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Read hardware inputs
  rotaryColorKnob.update();
  global_color = rotaryColorKnob.getRGB();

  brightnessKnob.update();
  strip.setBrightness(brightnessKnob.getBrightness());

  // Check Game Selection Potentiometer on Pin A1
  if (gameKnob.update()) {
    GameId newGame = gameKnob.getSelectedGame();
    Serial.print(F("[Game Potentiometer A1] -> Game "));
    Serial.print((int)newGame + 1);
    Serial.print(F(": "));
    Serial.println(gameKnob.getSelectedGameName());
    if (inGameMode) {
      resetActiveGame(newGame);
      strip.clear();
    }
  }

  // Check Global Light Behavior Potentiometer on Pin A4
  if (lightBehaviorKnob.update()) {
    currentLightMode = lightBehaviorKnob.getSelectedMode();
    Serial.print(F("[Light Behavior Potentiometer A4] -> Mode "));
    Serial.print((int)currentLightMode + 1);
    Serial.print(F(": "));
    Serial.println(lightBehaviorKnob.getSelectedModeName());
    if (inGameMode) {
      inGameMode = false;
      Serial.println(F("[Exit Game] -> Returning to Ambient Lighting Mode"));
    }
    strip.clear();
  }

  btnP1.update();
  btnP2.update();

  handleSerialCommands();

  // 2. Dual Button Hold (P1 + P2 for 800 ms) toggles Game Mode
  static unsigned long dualHoldTime = 0;
  if (btnP1.isDown() && btnP2.isDown()) {
    if (dualHoldTime == 0) dualHoldTime = currentMillis;
    if (currentMillis - dualHoldTime > 800) {
      inGameMode = !inGameMode;
      dualHoldTime = currentMillis + 2000;
      if (inGameMode) {
        GameId sel = gameKnob.getSelectedGame();
        resetActiveGame(sel);
        strip.clear();
        Serial.print(F("[Dual Hold] Launching Game: "));
        Serial.println(gameKnob.getSelectedGameName());
      } else {
        strip.clear();
        Serial.println(F("[Dual Hold] Exited Game Mode -> Ambient Lighting"));
      }
    }
  } else {
    dualHoldTime = 0;
  }

  // 3. Update Active Controller
  if (inGameMode) {
    updateActiveGame(gameKnob.getSelectedGame(), currentMillis);
  } else {
    updateActiveLight(currentLightMode, currentMillis);
  }
}
