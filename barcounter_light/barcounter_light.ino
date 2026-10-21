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
 *   - Player A Button (Digital Pin 2, active LOW with INPUT_PULLUP)
 *   - Player B Button (Digital Pin 4, active LOW with INPUT_PULLUP)
 * 
 * GamerHandler Architecture:
 *   - Dedicated GamerHandler class encapsulates gamer identities (Player A, Player B, RobotGamer),
 *     side assignments (Side A / Side B), button debouncing, round win reporting, and live scorekeeping.
 *   - Used across all 6 arcade games, decoupling game physics from raw hardware pins and player roles.
 * 
 * Game-Start Behavior & Challenge Protocol:
 *   - In ambient mode, Player A (or B) presses their button to issue a game challenge.
 *   - The Challenger's single SK6812 LED at their end of the strip turns solid in their defined color.
 *   - The Challenged Player's single SK6812 LED at the opposite end blinks (up to 10 times, 250ms ON / 250ms OFF).
 *   - If the second player presses their button during the 10 blinks, the 2-player human match starts immediately!
 *   - If no human player accepts within 10 blinks, the game automatically starts against an autonomous NPC: RobotGamer!
 * 
 * Point Counter & Strip End Point Indicators:
 *   - For each point scored, the next LED moving inward from that player's end illuminates in their defined color:
 *       * Player A: Electric Cyan (LED 0 inward)
 *       * Player B: Neon Coral Red (LED NUM_LEDS - 1 inward)
 *       * RobotGamer: Neon Gold / Amber (Robot's end inward)
 *   - First player to reach MATCH_WIN_SCORE (5 points) triggers a celebratory victory light show across the strip.
 * 
 * Hardware Pin Summary:
 *   - Pin A1: Rotary Game Selection Potentiometer
 *   - Pin A2: Rotary Master Brightness Potentiometer
 *   - Pin A3: Rotary Mood / Base-Color Potentiometer
 *   - Pin A4: Rotary Global Light Behavior Potentiometer
 *   - Pin A0: Unconnected / floating pin for randomSeed()
 *   - Pin 2:  Player A Push Button (GND)
 *   - Pin 3:  Dedicated LED for Gamer A (single SK6812 RGB LED next to Button A)
 *   - Pin 4:  Player B Push Button (GND)
 *   - Pin 5:  Dedicated LED for Gamer B (single SK6812 RGB LED next to Button B)
 *   - Pin 6:  SK6812 LED Strip Data IN (via 330-470 Ohm resistor)
 * ============================================================================
 */

#include <FastLED.h>

// ============================================================================
// Core Enums & Function Forward Declarations
// Defined at top of file so Arduino preprocessor auto-generated prototypes compile
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

enum GameId {
  GAME_RESONANCE_PULSE  = 0,
  GAME_SPARKLE_RUSH     = 1,
  GAME_FLAME_TUG        = 2,
  GAME_METEOR_DEFLECTOR = 3,
  GAME_CYLON_CLASH      = 4,
  GAME_TERRITORY_PAINT  = 5,
  NUM_GAMES             = 6
};

enum SystemState {
  STATE_IDLE_AMBIENT,     // Ambient bar lighting mode (controlled by lightBehaviorKnob)
  STATE_CHALLENGE,        // Player pressed button -> challenge initiated, opponent end LED blinking up to 10x
  STATE_PLAYING,          // Active game match in progress (2 human players or human vs RobotGamer)
  STATE_MATCH_OVER        // Match complete (a player reached MATCH_WIN_SCORE) -> victory animation
};

enum GamerId {
  GAMER_NONE     = 0,
  GAMER_PLAYER_A = 1,
  GAMER_PLAYER_B = 2,
  GAMER_ROBOT    = 3
};

enum PlayerSide {
  SIDE_A = 0, // Left end of bar counter (LED index 0, Pin 2)
  SIDE_B = 1  // Right end of bar counter (LED index NUM_LEDS - 1, Pin 4)
};

enum NumTarget {
  TARGET_NONE       = 0,
  TARGET_HUE        = 1,
  TARGET_BRIGHTNESS = 2,
  TARGET_GAME       = 3,
  TARGET_MODE       = 4
};

// Forward declarations (resolves Arduino builder preprocessor symbol resolution)
void resetActiveGame(GameId game);
void updateActiveGame(GameId game, unsigned long currentMillis);
void updateActiveLight(LightModeId mode, unsigned long currentMillis);
void startChallenge(GamerId who);
void startGame(bool againstRobot);
void updateChallenge(unsigned long currentMillis);
void updatePlaying(unsigned long currentMillis);
void updateMatchOver(unsigned long currentMillis);
void renderScoreOverlay();
void applySerialNumericCommand(NumTarget target, long val);
void handleSerialCommands();

// ============================================================================
// 1. Strip Geometry & Board Architecture Configuration
// ============================================================================
// Physical bar counter installation: 12 meters with 60 LEDs/m (720 physical LEDs).
//
// Hardware SRAM Constraints:
//   - ATmega328P (Arduino Uno / Nano / Pro Mini):
//       Total hardware SRAM is strictly 2,048 bytes (2 KB).
//       Allocating 720 RGB LEDs in RAM requires 720 * 3 = 2,160 bytes, which
//       exceeds the board's entire physical SRAM before loading any other code.
//       Therefore, on ATmega328P, NUM_LEDS automatically scales to 120 (or 60)
//       virtual LEDs to fit safely in SRAM (~1KB dynamic memory, leaving ~1KB stack).
//   - High-Memory Microcontrollers (Arduino Mega 2560, Uno R4, ESP32, Teensy, RP2040):
//       These boards have 8 KB to 512 KB SRAM, so NUM_LEDS defaults to the full
//       720 physical LEDs.
//
// To manually override the LED count, define NUM_LEDS before compiling.
#ifndef NUM_LEDS
  #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__) || (defined(RAMEND) && RAMEND <= 2048)
    #define NUM_LEDS                 120    // Scaled for Arduino Uno / Nano 2KB SRAM
  #else
    #define STRIP_LENGTH_METERS      12     // 12 meters total bar counter length
    #define LEDS_PER_METER           60     // 60 LEDs per meter
    #define NUM_LEDS                 (STRIP_LENGTH_METERS * LEDS_PER_METER) // 720 LEDs
  #endif
#endif

#define LED_PIN                  6      // Output data pin for physical SK6812 strip

#define PIN_GAME_KNOB            A1     // Rotary knob wiper for game selection
#define PIN_BRIGHTNESS_KNOB      A2     // Rotary knob wiper for global brightness setting
#define PIN_COLOR_KNOB           A3     // Rotary knob wiper for global mood/color setting
#define PIN_LIGHT_BEHAVIOR_KNOB  A4     // Rotary knob wiper for ambient light behavior selection
#define PIN_ENTROPY              A0     // Floating analog pin for randomSeed()

#define PIN_BTN_P1               2      // Player A input button (active LOW, momentary to GND)
#define PIN_LED_GAMER_A          3      // Dedicated SK6812 LED for Gamer A (next to Player A button)
#define PIN_BTN_P2               4      // Player B input button (active LOW, momentary to GND)
#define PIN_LED_GAMER_B          5      // Dedicated SK6812 LED for Gamer B (next to Player B button)

#define SERIAL_BAUD              115200

// Target backend: FastLED physical strip or optional serial streaming
#define BACKEND_VIRTUAL_SERIAL   0
#define BACKEND_FASTLED_REAL     1
#define STRIP_BACKEND            BACKEND_FASTLED_REAL

// ============================================================================
// 2. FastLED Strip Buffer & Driver Class
// ============================================================================
CRGB leds[NUM_LEDS];
CRGB ledGamerA[1]; // Single dedicated SK6812 LED for Gamer A on Pin 3
CRGB ledGamerB[1]; // Single dedicated SK6812 LED for Gamer B on Pin 5

class FastLEDDriver {
public:
  void begin() {
    // Register dedicated Gamer LEDs and main 12m strip
    FastLED.addLeds<SK6812, PIN_LED_GAMER_A, GRB>(ledGamerA, 1);
    FastLED.addLeds<SK6812, PIN_LED_GAMER_B, GRB>(ledGamerB, 1);
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

  void setPixelColor(uint16_t index, const CRGB& color) {
    if (index < NUM_LEDS) {
      leds[index] = color;
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
    ledGamerA[0] = CRGB::Black;
    ledGamerB[0] = CRGB::Black;
  }

  uint16_t numPixels() const {
    return NUM_LEDS;
  }

  void show() {
    renderScoreOverlay(); // Overlay challenge status & point indicators onto strip ends
#if (STRIP_BACKEND == BACKEND_VIRTUAL_SERIAL)
    // High-speed serial streaming to desktop visualizer (Extended CMD 0x02 protocol)
    // Header: 0xAA 0x55 0x02 [count_hi] [count_lo] [gamerA_r] [gamerA_g] [gamerA_b] [gamerB_r] [gamerB_g] [gamerB_b]
    uint8_t br = FastLED.getBrightness();
    CRGB gA = ledGamerA[0];
    CRGB gB = ledGamerB[0];
    uint8_t header[11] = {
      0xAA, 0x55, 0x02,
      (uint8_t)(NUM_LEDS >> 8), (uint8_t)(NUM_LEDS & 0xFF),
      scale8(gA.r, br), scale8(gA.g, br), scale8(gA.b, br),
      scale8(gB.r, br), scale8(gB.g, br), scale8(gB.b, br)
    };
    Serial.write(header, 11);
    uint8_t checksum = 0;
    for (uint8_t h = 0; h < 11; h++) checksum ^= header[h];
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

  // Portable integer square root for 16-bit integers (avoids pulling in floating-point libm)
  static inline uint8_t isqrt16(uint16_t val) {
    uint16_t res = 0;
    uint16_t bit = 1U << 14;
    while (bit > val) bit >>= 2;
    while (bit != 0) {
      if (val >= res + bit) {
        val -= res + bit;
        res = (res >> 1) + bit;
      } else {
        res >>= 1;
      }
      bit >>= 2;
    }
    return (uint8_t)res;
  }

  // Constant Perceptual Brightness Color Wheel (Integer Math - Zero libm / float dependency)
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

    // P^2 = (77*R^2 + 150*G^2 + 29*B^2) >> 8
    uint32_t r2 = (uint32_t)r * r;
    uint32_t g2 = (uint32_t)g * g;
    uint32_t b2 = (uint32_t)b * b;
    uint16_t pSquared = (uint16_t)((77UL * r2 + 150UL * g2 + 29UL * b2) >> 8);
    uint8_t pBright = isqrt16(pSquared);
    if (pBright > 0) {
      uint16_t scaleFactor = ((uint32_t)86 * 256) / pBright;
      r = (uint8_t)min(255, (int)(((uint32_t)r * scaleFactor) >> 8));
      g = (uint8_t)min(255, (int)(((uint32_t)g * scaleFactor) >> 8));
      b = (uint8_t)min(255, (int)(((uint32_t)b * scaleFactor) >> 8));
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
// 4. Game Modes Configuration & Rotary Game Selection Knob (Analog Pin A1)
// ============================================================================
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
// 7. Ambient Light Modes & Global Light Behavior Knob (Analog Pin A4)
// Selects one of the 6 ambient lighting modes across 6 equal dial zones
// ============================================================================
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

// ============================================================================
// System State, Gamer Identification & GamerHandler Class
// Encapsulates player identities (Player A, Player B, Robot), sides, input & scoring
// ============================================================================
const char* const GAMER_NAMES[4] = {
  "None",
  "Player A",
  "Player B",
  "RobotGamer"
};

// Player & Robot Defined Colors
const CRGB COLOR_GAMER_A     = CRGB(0, 180, 255); // Electric Cyan (Player A)
const CRGB COLOR_GAMER_B     = CRGB(255, 30, 70);  // Neon Coral Red (Player B)
const CRGB COLOR_GAMER_ROBOT = CRGB(255, 170, 0);  // Neon Gold / Amber (RobotGamer)

const uint8_t MATCH_WIN_SCORE = 5;

// ============================================================================
// GamerHandler: Class handling which is the current player (A, B or Robot)
// ============================================================================
class GamerHandler {
private:
  Button& btnA;
  Button& btnB;

  GamerId sideAGamer; // Gamer currently assigned to Side A (Left: GAMER_PLAYER_A or GAMER_ROBOT)
  GamerId sideBGamer; // Gamer currently assigned to Side B (Right: GAMER_PLAYER_B or GAMER_ROBOT)
  GamerId challenger; // Gamer who initiated the challenge (GAMER_PLAYER_A or GAMER_PLAYER_B)

  uint8_t scoreA;
  uint8_t scoreB;
  const uint8_t winScore;

  bool roundOver;
  unsigned long roundOverTime;
  PlayerSide roundWinnerSide;
  bool scoreTallied;

  // Challenge blinking state
  uint8_t challengeBlinkCount;
  bool challengeBlinkState;
  unsigned long lastChallengeBlinkTime;

  // Button press visual feedback timestamps
  unsigned long lastPressTimeA;
  unsigned long lastPressTimeB;

public:
  GamerHandler(Button& bA, Button& bB)
    : btnA(bA), btnB(bB),
      sideAGamer(GAMER_PLAYER_A), sideBGamer(GAMER_PLAYER_B), challenger(GAMER_NONE),
      scoreA(0), scoreB(0), winScore(MATCH_WIN_SCORE),
      roundOver(false), roundOverTime(0), roundWinnerSide(SIDE_A), scoreTallied(false),
      challengeBlinkCount(0), challengeBlinkState(false), lastChallengeBlinkTime(0),
      lastPressTimeA(0), lastPressTimeB(0) {}

  // 1. Current Player Identity Queries
  GamerId getCurrentPlayer(PlayerSide side) const {
    return (side == SIDE_A) ? sideAGamer : sideBGamer;
  }

  const char* getCurrentPlayerName(PlayerSide side) const {
    return GAMER_NAMES[(uint8_t)getCurrentPlayer(side)];
  }

  bool isRobot(PlayerSide side) const {
    return getCurrentPlayer(side) == GAMER_ROBOT;
  }

  bool isHuman(PlayerSide side) const {
    return !isRobot(side) && getCurrentPlayer(side) != GAMER_NONE;
  }

  PlayerSide getRobotSide() const {
    return (sideAGamer == GAMER_ROBOT) ? SIDE_A : SIDE_B;
  }

  GamerId getChallenger() const {
    return challenger;
  }

  // 2. Defined Colors
  CRGB getGamerColor(GamerId gamer) const {
    switch (gamer) {
      case GAMER_PLAYER_A: return COLOR_GAMER_A;
      case GAMER_PLAYER_B: return COLOR_GAMER_B;
      case GAMER_ROBOT:    return COLOR_GAMER_ROBOT;
      default:             return CRGB::Black;
    }
  }

  CRGB getSideColor(PlayerSide side) const {
    return getGamerColor(getCurrentPlayer(side));
  }

  // 3. Input Abstraction
  bool wasPressed(PlayerSide side) {
    if (side == SIDE_A) {
      if (btnA.wasPressed()) {
        lastPressTimeA = millis();
        return true;
      }
      return false;
    } else {
      if (btnB.wasPressed()) {
        lastPressTimeB = millis();
        return true;
      }
      return false;
    }
  }

  bool isDown(PlayerSide side) const {
    return (side == SIDE_A) ? btnA.isDown() : btnB.isDown();
  }

  void triggerVirtualPress(PlayerSide side) {
    if (side == SIDE_A) {
      btnA.triggerVirtualPress();
      lastPressTimeA = millis();
    } else {
      btnB.triggerVirtualPress();
      lastPressTimeB = millis();
    }
  }

  void triggerRobotPress() {
    triggerVirtualPress(getRobotSide());
  }

  // 4. Round Win & Scoring
  void resetRound() {
    roundOver = false;
    scoreTallied = false;
  }

  void setRoundWinner(PlayerSide side, unsigned long currentMillis) {
    roundOver = true;
    roundWinnerSide = side;
    roundOverTime = currentMillis;
    scoreTallied = false;
  }

  bool isRoundOver() const { return roundOver; }
  unsigned long getRoundOverTime() const { return roundOverTime; }
  PlayerSide getRoundWinnerSide() const { return roundWinnerSide; }
  GamerId getRoundWinnerGamer() const { return getCurrentPlayer(roundWinnerSide); }
  CRGB getRoundWinnerColor() const { return getSideColor(roundWinnerSide); }

  bool isScoreTallied() const { return scoreTallied; }
  void markScoreTallied() { scoreTallied = true; }

  void awardRoundPoint() {
    if (scoreTallied) return;
    scoreTallied = true;
    if (roundWinnerSide == SIDE_A) scoreA++;
    else scoreB++;

    Serial.print(F("[POINT!] "));
    Serial.print(getCurrentPlayerName(roundWinnerSide));
    Serial.print(F(" scores! Match Score: [Side A ("));
    Serial.print(getCurrentPlayerName(SIDE_A));
    Serial.print(F("): "));
    Serial.print(scoreA);
    Serial.print(F("] - [Side B ("));
    Serial.print(getCurrentPlayerName(SIDE_B));
    Serial.print(F("): "));
    Serial.print(scoreB);
    Serial.println(F("]"));
  }

  uint8_t getScore(PlayerSide side) const {
    return (side == SIDE_A) ? scoreA : scoreB;
  }

  bool isMatchWon() const {
    return (scoreA >= winScore || scoreB >= winScore);
  }

  PlayerSide getMatchWinnerSide() const {
    return (scoreA >= winScore) ? SIDE_A : SIDE_B;
  }

  GamerId getMatchWinnerGamer() const {
    return getCurrentPlayer(getMatchWinnerSide());
  }

  CRGB getMatchWinnerColor() const {
    return getSideColor(getMatchWinnerSide());
  }

  void resetScores() {
    scoreA = 0;
    scoreB = 0;
    roundOver = false;
    scoreTallied = false;
  }

  // 5. Challenge Management
  void startChallenge(GamerId who) {
    challenger = who;
    sideAGamer = GAMER_PLAYER_A;
    sideBGamer = GAMER_PLAYER_B;
    challengeBlinkCount = 0;
    challengeBlinkState = true;
    lastChallengeBlinkTime = millis();
    resetScores();
  }

  bool updateChallengeBlink(unsigned long currentMillis) {
    const unsigned long BLINK_INTERVAL = 250;
    if (currentMillis - lastChallengeBlinkTime >= BLINK_INTERVAL) {
      lastChallengeBlinkTime = currentMillis;
      challengeBlinkState = !challengeBlinkState;
      if (!challengeBlinkState) {
        challengeBlinkCount++;
        Serial.print(F("[Challenge Blink] "));
        Serial.print(challengeBlinkCount);
        Serial.println(F(" / 10"));
        if (challengeBlinkCount >= 10) {
          return true; // 10 blinks completed without human response
        }
      }
    }
    return false;
  }

  uint8_t getChallengeBlinkCount() const { return challengeBlinkCount; }
  bool getChallengeBlinkState() const { return challengeBlinkState; }

  void startMatch(bool againstRobot) {
    if (againstRobot) {
      if (challenger == GAMER_PLAYER_A) {
        sideAGamer = GAMER_PLAYER_A;
        sideBGamer = GAMER_ROBOT;
      } else {
        sideAGamer = GAMER_ROBOT;
        sideBGamer = GAMER_PLAYER_B;
      }
    } else {
      sideAGamer = GAMER_PLAYER_A;
      sideBGamer = GAMER_PLAYER_B;
    }
    resetScores();
    resetRound();
  }

  // 6. Dedicated Gamer LED & Strip Overlay Rendering
  void updateGamerLeds(CRGB& ledA, CRGB& ledB, SystemState state, unsigned long currentMillis) {
    if (state == STATE_IDLE_AMBIENT) {
      // Soft ambient indicator: low-brightness signature color so patrons easily spot buttons in a dim bar
      ledA = COLOR_GAMER_A;
      ledA.nscale8_video(70);
      ledB = COLOR_GAMER_B;
      ledB.nscale8_video(70);
    } else if (state == STATE_CHALLENGE) {
      if (challenger == GAMER_PLAYER_A) {
        // Player A challenged: Button A solid Cyan, Button B blinking Coral Red
        ledA = COLOR_GAMER_A;
        ledB = challengeBlinkState ? COLOR_GAMER_B : CRGB::Black;
      } else if (challenger == GAMER_PLAYER_B) {
        // Player B challenged: Button B solid Coral Red, Button A blinking Cyan
        ledB = COLOR_GAMER_B;
        ledA = challengeBlinkState ? COLOR_GAMER_A : CRGB::Black;
      }
    } else if (state == STATE_PLAYING) {
      CRGB baseA = getSideColor(SIDE_A);
      CRGB baseB = getSideColor(SIDE_B);

      if (roundOver) {
        // Winner button LED pulses bright, loser button LED dims
        bool pulse = ((currentMillis / 120) % 2 == 0);
        if (roundWinnerSide == SIDE_A) {
          ledA = pulse ? CRGB::White : baseA;
          ledB = baseB;
          ledB.nscale8_video(40);
        } else {
          ledB = pulse ? CRGB::White : baseB;
          ledA = baseA;
          ledA.nscale8_video(40);
        }
      } else {
        // Active play: display side color, flash bright white on button press (80ms tactile confirmation)
        if (currentMillis - lastPressTimeA < 80) {
          ledA = CRGB(255, 255, 255);
        } else {
          ledA = baseA;
        }

        if (currentMillis - lastPressTimeB < 80) {
          ledB = CRGB(255, 255, 255);
        } else {
          ledB = baseB;
        }
      }
    } else if (state == STATE_MATCH_OVER) {
      // Match victory celebration: Champion's button LED strobes triumphantly
      bool strobe = ((currentMillis / 100) % 2 == 0);
      PlayerSide winner = getMatchWinnerSide();
      CRGB winCol = getSideColor(winner);
      if (winner == SIDE_A) {
        ledA = strobe ? CRGB::White : winCol;
        ledB = CRGB::Black;
      } else {
        ledB = strobe ? CRGB::White : winCol;
        ledA = CRGB::Black;
      }
    }
  }

  void renderScoreOverlay(CRGB* leds, uint16_t numLeds, SystemState state) {
    if (state == STATE_PLAYING || state == STATE_MATCH_OVER) {
      // Side A Score: illuminate next LED moving inward from index 0
      CRGB colA = getSideColor(SIDE_A);
      for (uint8_t i = 0; i < scoreA && i < numLeds; i++) {
        leds[i] = colA;
      }

      // Side B Score: illuminate next LED moving inward from numLeds - 1
      CRGB colB = getSideColor(SIDE_B);
      for (uint8_t i = 0; i < scoreB && i < numLeds; i++) {
        leds[numLeds - 1 - i] = colB;
      }
    } else if (state == STATE_CHALLENGE) {
      if (challenger == GAMER_PLAYER_A) {
        // Challenger A: solid on single SK6812 at LED 0
        leds[0] = COLOR_GAMER_A;
        // Opponent B: blinking on single SK6812 at LED numLeds - 1
        leds[numLeds - 1] = challengeBlinkState ? COLOR_GAMER_B : CRGB::Black;
      } else if (challenger == GAMER_PLAYER_B) {
        // Challenger B: solid on single SK6812 at LED numLeds - 1
        leds[numLeds - 1] = COLOR_GAMER_B;
        // Opponent A: blinking on single SK6812 at LED 0
        leds[0] = challengeBlinkState ? COLOR_GAMER_A : CRGB::Black;
      }
    }
  }
};

GamerHandler gamerHandler(btnP1, btnP2);

SystemState systemState = STATE_IDLE_AMBIENT;
LightModeId currentLightMode = LIGHT_BREATHING_PULSE;
bool inGameMode = false;
unsigned long matchOverTime = 0;

// Score and Challenge Overlay Hook
// Automatically called inside FastLEDDriver::show() right before FastLED.show()
void renderScoreOverlay() {
  gamerHandler.renderScoreOverlay(leds, NUM_LEDS, systemState);
  gamerHandler.updateGamerLeds(ledGamerA[0], ledGamerB[0], systemState, millis());
}

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

    // FastLED sin8 integer sine wave (0..255) avoids floating-point libm
    uint8_t angle = (uint8_t)(((currentMillis % BREATH_PERIOD) * 256UL) / BREATH_PERIOD);
    uint8_t wave = sin8(angle);
    uint8_t factor = 107 + scale8(wave, 102); // 42%..82% brightness factor

    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();

    uint32_t color = FastLEDDriver::Color(
      scale8(baseR, factor),
      scale8(baseG, factor),
      scale8(baseB, factor)
    );

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

public:
  TwinkleSparkleLight() {}

  void enter() { lastUpdate = 0; }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 50) return;
    lastUpdate = currentMillis;

    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();

    // Procedural twinkle brightness calculation per pixel (zero RAM array footprint)
    // Combines time offset with prime spatial stride to produce organic, non-repeating twinkle
    uint16_t timeStep = (uint16_t)(currentMillis >> 4);
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      uint8_t angle = (uint8_t)(timeStep * 3 + i * 47);
      uint8_t wave = sin8(angle);
      uint8_t br = 58 + scale8(wave, 162); // 58..220

      uint8_t r = scale8(baseR, br);
      uint8_t g = scale8(baseG, br);
      uint8_t b = scale8(baseB, br);

      if (br > 195) {
        uint8_t spark = (br - 195) * 2;
        r = qadd8(r, spark);
        g = qadd8(g, spark);
        b = qadd8(b, spark);
      }
      strip.setPixelColor(i, FastLEDDriver::Color(r, g, b));
    }
    strip.show();
  }
};

// --- Light Mode 3: Fire / Flame ---
class FireFlameLight {
private:
  static const uint8_t FIRE_CELLS = 32; // Compact simulation resolution (32 bytes RAM instead of 720)
  unsigned long lastUpdate = 0;
  uint8_t heat[FIRE_CELLS];

  uint32_t heatToColor(uint8_t temp) {
    temp = max((uint8_t)80, temp);
    uint8_t t192 = (uint8_t)(((uint16_t)temp * 191) >> 8);
    uint8_t ramp = (t192 & 0x3F) << 2;

    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();

    if (t192 > 0x80) { // Hottest: blend toward white hot
      uint8_t r = qadd8(baseR, ramp);
      uint8_t g = qadd8(baseG, ramp);
      uint8_t b = qadd8(baseB, ramp);
      return FastLEDDriver::Color(r, g, b);
    } else if (t192 > 0x40) { // Medium: full saturation of base color
      uint8_t factor = (t192 << 1);
      return FastLEDDriver::Color(scale8(baseR, factor), scale8(baseG, factor), scale8(baseB, factor));
    } else { // Coolest: dark ember base color
      uint8_t factor = max((uint8_t)60, (uint8_t)(t192 << 2));
      return FastLEDDriver::Color(scale8(baseR, factor), scale8(baseG, factor), scale8(baseB, factor));
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
    for (uint8_t i = 0; i < FIRE_CELLS; i++) {
      heat[i] = (heat[i] > 79) ? (heat[i] - 1) : 78;
    }

    // Thermal diffusion
    for (int k = FIRE_CELLS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }
    if (random(0, 10) < 2) {
      int sparkPos = random(0, FIRE_CELLS);
      heat[sparkPos] = min(255, heat[sparkPos] + random(60, 110));
    }

    // Render mapped heat cells across physical strip
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      uint8_t cell = (uint8_t)(((uint32_t)i * FIRE_CELLS) / NUM_LEDS);
      strip.setPixelColor(i, heatToColor(heat[cell]));
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

    uint8_t angle = (uint8_t)(((currentMillis % SWEEP_PERIOD) * 256UL) / SWEEP_PERIOD);
    int16_t s = (int16_t)sin8(angle) - 128; // -128..+127 integer sine wave
    float center = (float)(NUM_LEDS - 1) * 0.5f;
    float margin = max(0.5f, min(2.0f, (float)NUM_LEDS * 0.05f));
    float amplitude = max(0.5f, center - margin);
    float eyePos = center + amplitude * ((float)s / 128.0f);
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
  GamerHandler& gamers;
  unsigned long lastUpdate = 0;
  const unsigned long BREATH_PERIOD = 7500;
  int16_t nexusPosition;
  unsigned long p1Cooldown;
  unsigned long p2Cooldown;

public:
  ResonancePulseGame(GamerHandler& gh)
    : gamers(gh), nexusPosition(NUM_LEDS / 2), p1Cooldown(0), p2Cooldown(0) {}

  void reset() {
    nexusPosition = NUM_LEDS / 2;
    p1Cooldown = 0;
    p2Cooldown = 0;
    gamers.resetRound();
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 25) return;
    lastUpdate = currentMillis;

    if (gamers.isRoundOver()) {
      if (currentMillis - gamers.getRoundOverTime() > 2000) {
        reset();
      } else {
        uint8_t flash = ((currentMillis - gamers.getRoundOverTime()) / 120) % 2 == 0 ? 255 : 40;
        CRGB winCol = gamers.getRoundWinnerColor();
        uint16_t start = (gamers.getRoundWinnerSide() == SIDE_A) ? (NUM_LEDS - 40) : 0;
        uint16_t end = (gamers.getRoundWinnerSide() == SIDE_A) ? NUM_LEDS : 40;
        for (uint16_t i = start; i < end; i++) {
          strip.setPixelColor(i, FastLEDDriver::Color(scale8(winCol.r, flash), scale8(winCol.g, flash), scale8(winCol.b, flash)));
        }
        strip.show();
      }
      return;
    }

    // FastLED sin8 integer wave avoids floating-point libm
    uint8_t angle = (uint8_t)(((currentMillis % BREATH_PERIOD) * 256UL) / BREATH_PERIOD);
    uint8_t wave = sin8(angle);
    bool inResonance = (wave >= 225); // ~88% of 255
    int16_t pushStep = max(1, (int)(NUM_LEDS / 15));

    // Player on Side A (Player A or Robot)
    if (gamers.wasPressed(SIDE_A)) {
      if (currentMillis >= p1Cooldown) {
        if (inResonance) {
          nexusPosition += pushStep;
          if (nexusPosition >= (int16_t)NUM_LEDS - 2) {
            nexusPosition = NUM_LEDS - 1;
            gamers.setRoundWinner(SIDE_A, currentMillis);
          }
        } else {
          p1Cooldown = currentMillis + 400; // Misfire penalty cooldown
        }
      }
    }

    // Player on Side B (Player B or Robot)
    if (gamers.wasPressed(SIDE_B)) {
      if (currentMillis >= p2Cooldown) {
        if (inResonance) {
          nexusPosition -= pushStep;
          if (nexusPosition <= 1) {
            nexusPosition = 0;
            gamers.setRoundWinner(SIDE_B, currentMillis);
          }
        } else {
          p2Cooldown = currentMillis + 400; // Misfire penalty cooldown
        }
      }
    }

    // Ambient background floor in mood color (integer scaling)
    uint8_t factor = 89 + scale8(wave, 51); // 0.35 + (wave/255)*0.20
    uint8_t rBase = scale8(rotaryColorKnob.getR(), factor);
    uint8_t gBase = scale8(rotaryColorKnob.getG(), factor);
    uint8_t bBase = scale8(rotaryColorKnob.getB(), factor);
    uint32_t baseCol = FastLEDDriver::Color(rBase, gBase, bBase);
    for (uint16_t i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, baseCol);

    int glowWidth = max(2, (int)(NUM_LEDS * 0.13f + 0.5f));
    CRGB colA = gamers.getSideColor(SIDE_A);
    CRGB colB = gamers.getSideColor(SIDE_B);
    for (int i = 0; i <= nexusPosition; i++) {
      int dist = nexusPosition - i;
      uint8_t b = (dist < glowWidth) ? (255 - (dist * 210) / glowWidth) : 45;
      strip.setPixelColor(i, FastLEDDriver::Color((colA.r * b) >> 8, (colA.g * b) >> 8, (colA.b * b) >> 8));
    }
    for (int i = nexusPosition; i < NUM_LEDS; i++) {
      int dist = i - nexusPosition;
      uint8_t r = (dist < glowWidth) ? (255 - (dist * 210) / glowWidth) : 45;
      strip.setPixelColor(i, FastLEDDriver::Color((colB.r * r) >> 8, (colB.g * r) >> 8, (colB.b * r) >> 8));
    }

    uint32_t nexusCol = inResonance ? FastLEDDriver::Color(255, 255, 240) : FastLEDDriver::Color(255, 190, 60);
    if (nexusPosition >= 0 && nexusPosition < NUM_LEDS) strip.setPixelColor(nexusPosition, nexusCol);
    strip.show();
  }
};

// --- Game 2: "Sparkle Rush" (Nova Sparkle Reflex Deflector) ---
class SparkleRushGame {
private:
  GamerHandler& gamers;
  unsigned long lastUpdate = 0;
  float novaPos;
  float novaSpeed;

public:
  SparkleRushGame(GamerHandler& gh)
    : gamers(gh), novaPos((float)NUM_LEDS * 0.5f),
      novaSpeed(max(0.25f, (float)NUM_LEDS * (0.95f / 60.0f))) {}

  void reset() {
    novaPos = (float)NUM_LEDS * 0.5f;
    float baseSpeed = max(0.25f, (float)NUM_LEDS * (0.95f / 60.0f));
    novaSpeed = (random(0, 2) == 0 ? baseSpeed : -baseSpeed);
    gamers.resetRound();
  }

  float getNovaPos() const { return novaPos; }
  float getNovaSpeed() const { return novaSpeed; }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 20) return;
    lastUpdate = currentMillis;

    if (gamers.isRoundOver()) {
      if (currentMillis - gamers.getRoundOverTime() > 2000) {
        reset();
      } else {
        uint8_t flash = ((currentMillis - gamers.getRoundOverTime()) / 120) % 2 == 0 ? 255 : 40;
        CRGB winCol = gamers.getRoundWinnerColor();
        uint16_t start = (gamers.getRoundWinnerSide() == SIDE_A) ? (NUM_LEDS - 40) : 0;
        uint16_t end = (gamers.getRoundWinnerSide() == SIDE_A) ? NUM_LEDS : 40;
        for (uint16_t i = start; i < end; i++) {
          strip.setPixelColor(i, FastLEDDriver::Color(scale8(winCol.r, flash), scale8(winCol.g, flash), scale8(winCol.b, flash)));
        }
        strip.show();
      }
      return;
    }

    novaPos += novaSpeed;
    uint16_t shieldLeds = max((uint16_t)1, (uint16_t)((NUM_LEDS * 15 + 50) / 100)); // ~15%
    float maxSpeed = max(0.8f, (float)NUM_LEDS * (2.8f / 60.0f));

    if (gamers.wasPressed(SIDE_A)) {
      if (novaPos >= 0.0f && novaPos <= (float)shieldLeds && novaSpeed < 0.0f) {
        float bonus = ((float)shieldLeds - novaPos) * (1.5f / (float)shieldLeds);
        novaSpeed = abs(novaSpeed) * 1.12f + bonus;
        novaSpeed = min(novaSpeed, maxSpeed);
      }
    }

    if (gamers.wasPressed(SIDE_B)) {
      float p2ZoneStart = (float)(NUM_LEDS - 1 - shieldLeds);
      if (novaPos >= p2ZoneStart && novaPos <= (float)(NUM_LEDS - 1) && novaSpeed > 0.0f) {
        float bonus = (novaPos - p2ZoneStart) * (1.5f / (float)shieldLeds);
        novaSpeed = -(abs(novaSpeed) * 1.12f + bonus);
        novaSpeed = max(novaSpeed, -maxSpeed);
      }
    }

    if (novaPos < -1.0f) {
      gamers.setRoundWinner(SIDE_B, currentMillis); // Nova slipped past Side A -> Side B scored!
    } else if (novaPos > (float)NUM_LEDS) {
      gamers.setRoundWinner(SIDE_A, currentMillis); // Nova slipped past Side B -> Side A scored!
    }

    // Baseline starfield floor
    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, FastLEDDriver::Color((baseR * 45) >> 8, (baseG * 45) >> 8, (baseB * 45) >> 8));
    }

    // Defense Shields colored by gamer on that side!
    CRGB colA = gamers.getSideColor(SIDE_A);
    CRGB colB = gamers.getSideColor(SIDE_B);
    for (uint16_t i = 0; i < shieldLeds; i++) strip.setPixelColor(i, FastLEDDriver::Color((colA.r * 200) >> 8, (colA.g * 200) >> 8, (colA.b * 200) >> 8));
    for (uint16_t i = NUM_LEDS - shieldLeds; i < NUM_LEDS; i++) strip.setPixelColor(i, FastLEDDriver::Color((colB.r * 200) >> 8, (colB.g * 200) >> 8, (colB.b * 200) >> 8));

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
  GamerHandler& gamers;
  unsigned long lastUpdate = 0;
  int16_t flameClashPos;
  unsigned long lastTapP1;
  unsigned long lastTapP2;

public:
  FlameTugGame(GamerHandler& gh)
    : gamers(gh), flameClashPos(NUM_LEDS / 2), lastTapP1(0), lastTapP2(0) {}

  void reset() {
    flameClashPos = NUM_LEDS / 2;
    gamers.resetRound();
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 25) return;
    lastUpdate = currentMillis;

    if (gamers.isRoundOver()) {
      if (currentMillis - gamers.getRoundOverTime() > 2000) {
        reset();
      } else {
        uint8_t flash = ((currentMillis - gamers.getRoundOverTime()) / 120) % 2 == 0 ? 255 : 40;
        CRGB winCol = gamers.getRoundWinnerColor();
        uint16_t start = (gamers.getRoundWinnerSide() == SIDE_A) ? (NUM_LEDS - 40) : 0;
        uint16_t end = (gamers.getRoundWinnerSide() == SIDE_A) ? NUM_LEDS : 40;
        for (uint16_t i = start; i < end; i++) {
          strip.setPixelColor(i, FastLEDDriver::Color(scale8(winCol.r, flash), scale8(winCol.g, flash), scale8(winCol.b, flash)));
        }
        strip.show();
      }
      return;
    }

    int16_t pushStep = max(1, (int)(NUM_LEDS / 30));

    if (gamers.wasPressed(SIDE_A)) {
      if (currentMillis - lastTapP1 > 120) { // 120ms anti-spam window
        lastTapP1 = currentMillis;
        flameClashPos += pushStep;
        if (flameClashPos >= (int16_t)NUM_LEDS - 2) {
          flameClashPos = NUM_LEDS - 1;
          gamers.setRoundWinner(SIDE_A, currentMillis);
        }
      }
    }

    if (gamers.wasPressed(SIDE_B)) {
      if (currentMillis - lastTapP2 > 120) { // 120ms anti-spam window
        lastTapP2 = currentMillis;
        flameClashPos -= pushStep;
        if (flameClashPos <= 1) {
          flameClashPos = 0;
          gamers.setRoundWinner(SIDE_B, currentMillis);
        }
      }
    }

    CRGB colA = gamers.getSideColor(SIDE_A);
    CRGB colB = gamers.getSideColor(SIDE_B);

    // Side A Forge (0..clash) -> gradients from gamer A color to white-hot
    for (int i = 0; i <= flameClashPos; i++) {
      uint8_t factor = (uint8_t)(((uint32_t)i * 255UL) / max(1, flameClashPos));
      uint8_t r = (factor > 180) ? (factor - 180) * 3 : 0;
      uint8_t g = (factor * 160) >> 8;
      uint8_t b = 70 + ((uint16_t)factor * 185) / 255;
      strip.setPixelColor(i, FastLEDDriver::Color(
        (colA.r * (255 - factor) + (uint16_t)r * factor) >> 8,
        (colA.g * (255 - factor) + (uint16_t)g * factor) >> 8,
        (colA.b * (255 - factor) + (uint16_t)b * factor) >> 8
      ));
    }

    // Side B Forge (clash..NUM_LEDS) -> gradients from white-hot to gamer B color
    for (int i = flameClashPos + 1; i < NUM_LEDS; i++) {
      int distFromClash = i - flameClashPos;
      int span = NUM_LEDS - 1 - flameClashPos;
      uint8_t factor = (uint8_t)(255 - (((uint32_t)distFromClash * 255UL) / max(1, span)));
      uint8_t r = 70 + ((uint16_t)factor * 185) / 255;
      uint8_t g = (factor * 100) >> 8;
      uint8_t b = (factor > 180) ? (factor - 180) * 3 : 0;
      strip.setPixelColor(i, FastLEDDriver::Color(
        (colB.r * (255 - factor) + (uint16_t)r * factor) >> 8,
        (colB.g * (255 - factor) + (uint16_t)g * factor) >> 8,
        (colB.b * (255 - factor) + (uint16_t)b * factor) >> 8
      ));
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
  GamerHandler& gamers;
  unsigned long lastUpdate = 0;
  float gameMeteorPos;
  float gameMeteorSpeed;

public:
  MeteorDeflectorGame(GamerHandler& gh)
    : gamers(gh), gameMeteorPos((float)NUM_LEDS * 0.5f),
      gameMeteorSpeed(max(0.3f, (float)NUM_LEDS * (1.1f / 60.0f))) {}

  void reset() {
    gameMeteorPos = (float)NUM_LEDS * 0.5f;
    float baseSpeed = max(0.3f, (float)NUM_LEDS * (1.1f / 60.0f));
    gameMeteorSpeed = (random(0, 2) == 0 ? baseSpeed : -baseSpeed);
    gamers.resetRound();
  }

  float getMeteorPos() const { return gameMeteorPos; }
  float getMeteorSpeed() const { return gameMeteorSpeed; }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 20) return;
    lastUpdate = currentMillis;

    if (gamers.isRoundOver()) {
      if (currentMillis - gamers.getRoundOverTime() > 2000) {
        reset();
      } else {
        uint8_t flash = ((currentMillis - gamers.getRoundOverTime()) / 120) % 2 == 0 ? 255 : 40;
        CRGB winCol = gamers.getRoundWinnerColor();
        uint16_t start = (gamers.getRoundWinnerSide() == SIDE_A) ? (NUM_LEDS - 40) : 0;
        uint16_t end = (gamers.getRoundWinnerSide() == SIDE_A) ? NUM_LEDS : 40;
        for (uint16_t i = start; i < end; i++) {
          strip.setPixelColor(i, FastLEDDriver::Color(scale8(winCol.r, flash), scale8(winCol.g, flash), scale8(winCol.b, flash)));
        }
        strip.show();
      }
      return;
    }

    gameMeteorPos += gameMeteorSpeed;
    uint16_t defenseLeds = max((uint16_t)1, (uint16_t)((NUM_LEDS * 15 + 50) / 100)); // ~15%
    float maxSpeed = max(0.8f, (float)NUM_LEDS * (3.2f / 60.0f));

    if (gamers.wasPressed(SIDE_A)) {
      if (gameMeteorPos >= 0.0f && gameMeteorPos <= (float)defenseLeds && gameMeteorSpeed < 0.0f) {
        float bonus = ((float)defenseLeds - gameMeteorPos) * (1.8f / (float)defenseLeds);
        gameMeteorSpeed = abs(gameMeteorSpeed) * 1.14f + bonus;
        gameMeteorSpeed = min(gameMeteorSpeed, maxSpeed);
      }
    }

    if (gamers.wasPressed(SIDE_B)) {
      float p2ZoneStart = (float)(NUM_LEDS - 1 - defenseLeds);
      if (gameMeteorPos >= p2ZoneStart && gameMeteorPos <= (float)(NUM_LEDS - 1) && gameMeteorSpeed > 0.0f) {
        float bonus = (gameMeteorPos - p2ZoneStart) * (1.8f / (float)defenseLeds);
        gameMeteorSpeed = -(abs(gameMeteorSpeed) * 1.14f + bonus);
        gameMeteorSpeed = max(gameMeteorSpeed, -maxSpeed);
      }
    }

    if (gameMeteorPos < -1.0f) {
      gamers.setRoundWinner(SIDE_B, currentMillis);
    } else if (gameMeteorPos > (float)NUM_LEDS) {
      gamers.setRoundWinner(SIDE_A, currentMillis);
    }

    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, FastLEDDriver::Color((baseR * 50) >> 8, (baseG * 50) >> 8, (baseB * 50) >> 8));
    }
    CRGB colA = gamers.getSideColor(SIDE_A);
    CRGB colB = gamers.getSideColor(SIDE_B);
    for (uint16_t i = 0; i < defenseLeds; i++) strip.setPixelColor(i, FastLEDDriver::Color((colA.r * 200) >> 8, (colA.g * 200) >> 8, (colA.b * 200) >> 8));
    for (uint16_t i = NUM_LEDS - defenseLeds; i < NUM_LEDS; i++) strip.setPixelColor(i, FastLEDDriver::Color((colB.r * 200) >> 8, (colB.g * 200) >> 8, (colB.b * 200) >> 8));

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
  GamerHandler& gamers;
  unsigned long lastUpdate = 0;
  float puckPos;
  float puckSpeed;

public:
  CylonClashGame(GamerHandler& gh)
    : gamers(gh), puckPos((float)NUM_LEDS * 0.5f),
      puckSpeed(max(0.3f, (float)NUM_LEDS * (1.2f / 60.0f))) {}

  void reset() {
    puckPos = (float)NUM_LEDS * 0.5f;
    float baseSpeed = max(0.3f, (float)NUM_LEDS * (1.2f / 60.0f));
    puckSpeed = (random(0, 2) == 0 ? baseSpeed : -baseSpeed);
    gamers.resetRound();
  }

  float getPuckPos() const { return puckPos; }
  float getPuckSpeed() const { return puckSpeed; }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 18) return;
    lastUpdate = currentMillis;

    if (gamers.isRoundOver()) {
      if (currentMillis - gamers.getRoundOverTime() > 2000) {
        reset();
      } else {
        uint8_t flash = ((currentMillis - gamers.getRoundOverTime()) / 120) % 2 == 0 ? 255 : 40;
        CRGB winCol = gamers.getRoundWinnerColor();
        uint16_t start = (gamers.getRoundWinnerSide() == SIDE_A) ? (NUM_LEDS - 40) : 0;
        uint16_t end = (gamers.getRoundWinnerSide() == SIDE_A) ? NUM_LEDS : 40;
        for (uint16_t i = start; i < end; i++) {
          strip.setPixelColor(i, FastLEDDriver::Color(scale8(winCol.r, flash), scale8(winCol.g, flash), scale8(winCol.b, flash)));
        }
        strip.show();
      }
      return;
    }

    puckPos += puckSpeed;
    uint16_t defenseLeds = max((uint16_t)1, (uint16_t)((NUM_LEDS * 15 + 50) / 100)); // ~15%
    float maxSpeed = max(0.8f, (float)NUM_LEDS * (3.4f / 60.0f));

    if (gamers.wasPressed(SIDE_A)) {
      if (puckPos >= 0.0f && puckPos <= (float)defenseLeds && puckSpeed < 0.0f) {
        float bonus = ((float)defenseLeds - puckPos) * (1.6f / (float)defenseLeds);
        puckSpeed = abs(puckSpeed) * 1.15f + bonus;
        puckSpeed = min(puckSpeed, maxSpeed);
      }
    }

    if (gamers.wasPressed(SIDE_B)) {
      float p2ZoneStart = (float)(NUM_LEDS - 1 - defenseLeds);
      if (puckPos >= p2ZoneStart && puckPos <= (float)(NUM_LEDS - 1) && puckSpeed > 0.0f) {
        float bonus = (puckPos - p2ZoneStart) * (1.6f / (float)defenseLeds);
        puckSpeed = -(abs(puckSpeed) * 1.15f + bonus);
        puckSpeed = max(puckSpeed, -maxSpeed);
      }
    }

    if (puckPos < -1.0f) {
      gamers.setRoundWinner(SIDE_B, currentMillis);
    } else if (puckPos > (float)NUM_LEDS) {
      gamers.setRoundWinner(SIDE_A, currentMillis);
    }

    uint8_t baseR = rotaryColorKnob.getR();
    uint8_t baseG = rotaryColorKnob.getG();
    uint8_t baseB = rotaryColorKnob.getB();
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, FastLEDDriver::Color((baseR * 50) >> 8, (baseG * 50) >> 8, (baseB * 50) >> 8));
    }
    CRGB colA = gamers.getSideColor(SIDE_A);
    CRGB colB = gamers.getSideColor(SIDE_B);
    for (uint16_t i = 0; i < defenseLeds; i++) strip.setPixelColor(i, FastLEDDriver::Color((colA.r * 200) >> 8, (colA.g * 200) >> 8, (colA.b * 200) >> 8));
    for (uint16_t i = NUM_LEDS - defenseLeds; i < NUM_LEDS; i++) strip.setPixelColor(i, FastLEDDriver::Color((colB.r * 200) >> 8, (colB.g * 200) >> 8, (colB.b * 200) >> 8));

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
  GamerHandler& gamers;
  unsigned long lastUpdate = 0;
  int16_t paintBoundary;

public:
  TerritoryPaintGame(GamerHandler& gh)
    : gamers(gh), paintBoundary(NUM_LEDS / 2) {}

  void reset() {
    paintBoundary = NUM_LEDS / 2;
    gamers.resetRound();
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 20) return;
    lastUpdate = currentMillis;

    if (gamers.isRoundOver()) {
      if (currentMillis - gamers.getRoundOverTime() > 2000) {
        reset();
      } else {
        uint8_t flash = ((currentMillis - gamers.getRoundOverTime()) / 120) % 2 == 0 ? 255 : 40;
        CRGB winCol = gamers.getRoundWinnerColor();
        uint16_t start = (gamers.getRoundWinnerSide() == SIDE_A) ? (NUM_LEDS - 40) : 0;
        uint16_t end = (gamers.getRoundWinnerSide() == SIDE_A) ? NUM_LEDS : 40;
        for (uint16_t i = start; i < end; i++) {
          strip.setPixelColor(i, FastLEDDriver::Color(scale8(winCol.r, flash), scale8(winCol.g, flash), scale8(winCol.b, flash)));
        }
        strip.show();
      }
      return;
    }

    int16_t pushStep = max(1, (int)(NUM_LEDS / 30));

    if (gamers.wasPressed(SIDE_A)) {
      paintBoundary += pushStep;
      if (paintBoundary >= (int16_t)NUM_LEDS - 1) {
        paintBoundary = NUM_LEDS - 1;
        gamers.setRoundWinner(SIDE_A, currentMillis);
      }
    }

    if (gamers.wasPressed(SIDE_B)) {
      paintBoundary -= pushStep;
      if (paintBoundary <= 0) {
        paintBoundary = 0;
        gamers.setRoundWinner(SIDE_B, currentMillis);
      }
    }

    CRGB colA = gamers.getSideColor(SIDE_A);
    CRGB colB = gamers.getSideColor(SIDE_B);
    for (int i = 0; i <= paintBoundary; i++) {
      strip.setPixelColor(i, colA);
    }
    for (int i = paintBoundary + 1; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, colB);
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

ResonancePulseGame  gameResonancePulse(gamerHandler);
SparkleRushGame     gameSparkleRush(gamerHandler);
FlameTugGame        gameFlameTug(gamerHandler);
MeteorDeflectorGame gameMeteorDeflector(gamerHandler);
CylonClashGame      gameCylonClash(gamerHandler);
TerritoryPaintGame  gameTerritoryPaint(gamerHandler);

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
// 12. RobotGamer Autonomous NPC Controller
// Simulates realistic opponent button presses across all 6 games via GamerHandler
// ============================================================================
class RobotGamer {
private:
  GamerHandler& gamers;
  unsigned long lastActionTime = 0;
  unsigned long nextActionInterval = 150;
  bool hasDeflectedThisPass = false;
  bool hasPressedThisPeak = false;
  unsigned long targetPeakTime = 0;

public:
  RobotGamer(GamerHandler& gh)
    : gamers(gh), lastActionTime(0), nextActionInterval(150),
      hasDeflectedThisPass(false), hasPressedThisPeak(false), targetPeakTime(0) {}

  void resetRound() {
    hasDeflectedThisPass = false;
    hasPressedThisPeak = false;
    targetPeakTime = 0;
    lastActionTime = millis();
    nextActionInterval = random(130, 180);
  }

  void update(unsigned long currentMillis, GameId game) {
    PlayerSide robotSide = gamers.getRobotSide();

    if (gamers.isRoundOver()) return;

    switch (game) {
      case GAME_RESONANCE_PULSE: {
        const unsigned long BREATH_PERIOD = 7500;
        uint8_t angle = (uint8_t)(((currentMillis % BREATH_PERIOD) * 256UL) / BREATH_PERIOD);
        uint8_t wave = sin8(angle); // 0..255 integer wave

        if (wave >= 225) { // In resonance crest
          if (!hasPressedThisPeak && targetPeakTime == 0) {
            targetPeakTime = currentMillis + random(70, 210);
          }
          if (targetPeakTime > 0 && currentMillis >= targetPeakTime && !hasPressedThisPeak) {
            gamers.triggerRobotPress();
            hasPressedThisPeak = true;
            targetPeakTime = 0;
          }
        } else if (wave < 191) {
          hasPressedThisPeak = false;
          targetPeakTime = 0;
        }
        break;
      }

      case GAME_SPARKLE_RUSH: {
        float pos = gameSparkleRush.getNovaPos();
        float spd = gameSparkleRush.getNovaSpeed();
        uint16_t shield = max((uint16_t)1, (uint16_t)((NUM_LEDS * 15 + 50) / 100));

        if (robotSide == SIDE_B) {
          float zoneStart = (float)(NUM_LEDS - 1 - shield);
          if (spd > 0.0f) {
            if (pos >= zoneStart + shield * 0.40f && !hasDeflectedThisPass) {
              gamers.triggerRobotPress();
              hasDeflectedThisPass = true;
            }
          } else {
            hasDeflectedThisPass = false;
          }
        } else { // Robot is SIDE_A
          if (spd < 0.0f) {
            if (pos <= (float)shield * 0.60f && !hasDeflectedThisPass) {
              gamers.triggerRobotPress();
              hasDeflectedThisPass = true;
            }
          } else {
            hasDeflectedThisPass = false;
          }
        }
        break;
      }

      case GAME_FLAME_TUG: {
        // High-cadence bellows tapping (anti-spam is 120ms)
        if (currentMillis - lastActionTime >= nextActionInterval) {
          gamers.triggerRobotPress();
          lastActionTime = currentMillis;
          nextActionInterval = random(130, 180);
        }
        break;
      }

      case GAME_METEOR_DEFLECTOR: {
        float pos = gameMeteorDeflector.getMeteorPos();
        float spd = gameMeteorDeflector.getMeteorSpeed();
        uint16_t defense = max((uint16_t)1, (uint16_t)((NUM_LEDS * 15 + 50) / 100));

        if (robotSide == SIDE_B) {
          float zoneStart = (float)(NUM_LEDS - 1 - defense);
          if (spd > 0.0f) {
            if (pos >= zoneStart + defense * 0.40f && !hasDeflectedThisPass) {
              gamers.triggerRobotPress();
              hasDeflectedThisPass = true;
            }
          } else {
            hasDeflectedThisPass = false;
          }
        } else { // Robot is SIDE_A
          if (spd < 0.0f) {
            if (pos <= (float)defense * 0.60f && !hasDeflectedThisPass) {
              gamers.triggerRobotPress();
              hasDeflectedThisPass = true;
            }
          } else {
            hasDeflectedThisPass = false;
          }
        }
        break;
      }

      case GAME_CYLON_CLASH: {
        float pos = gameCylonClash.getPuckPos();
        float spd = gameCylonClash.getPuckSpeed();
        uint16_t defense = max((uint16_t)1, (uint16_t)((NUM_LEDS * 15 + 50) / 100));

        if (robotSide == SIDE_B) {
          float zoneStart = (float)(NUM_LEDS - 1 - defense);
          if (spd > 0.0f) {
            if (pos >= zoneStart + defense * 0.40f && !hasDeflectedThisPass) {
              gamers.triggerRobotPress();
              hasDeflectedThisPass = true;
            }
          } else {
            hasDeflectedThisPass = false;
          }
        } else { // Robot is SIDE_A
          if (spd < 0.0f) {
            if (pos <= (float)defense * 0.60f && !hasDeflectedThisPass) {
              gamers.triggerRobotPress();
              hasDeflectedThisPass = true;
            }
          } else {
            hasDeflectedThisPass = false;
          }
        }
        break;
      }

      case GAME_TERRITORY_PAINT: {
        // High-frequency turf war button mashing
        if (currentMillis - lastActionTime >= nextActionInterval) {
          gamers.triggerRobotPress();
          lastActionTime = currentMillis;
          nextActionInterval = random(125, 175);
        }
        break;
      }
    }
  }
};

RobotGamer robotGamer(gamerHandler);

// ============================================================================
// 13. Game Challenge, Play & Match Transition Methods
// ============================================================================
void startChallenge(GamerId who) {
  systemState = STATE_CHALLENGE;
  inGameMode = false;
  gamerHandler.startChallenge(who);

  Serial.println(F("========================================"));
  Serial.print(F("[CHALLENGE] "));
  Serial.print(gamerHandler.getCurrentPlayerName(who == GAMER_PLAYER_A ? SIDE_A : SIDE_B));
  Serial.println(F(" initiated a game challenge!"));
  Serial.print(F("Opponent LED is blinking (max 10 times) in "));
  Serial.print(who == GAMER_PLAYER_A ? F("Player B") : F("Player A"));
  Serial.println(F("'s color. Press opponent button to accept!"));
  Serial.println(F("If no opponent accepts, RobotGamer will enter the match."));
  Serial.println(F("========================================"));
}

void startGame(bool againstRobot) {
  systemState = STATE_PLAYING;
  inGameMode = true;
  gamerHandler.startMatch(againstRobot);
  robotGamer.resetRound();
  resetActiveGame(gameKnob.getSelectedGame());
  strip.clear();

  Serial.println(F("========================================"));
  Serial.print(F("[GAME START] Active Game: "));
  Serial.println(gameKnob.getSelectedGameName());
  if (againstRobot) {
    Serial.print(F("Match Type: 1-Player vs RobotGamer (Robot plays on "));
    Serial.print(gamerHandler.getRobotSide() == SIDE_A ? F("Side A") : F("Side B"));
    Serial.println(F(")"));
  } else {
    Serial.println(F("Match Type: 2-Player Human Duel (Player A vs Player B)!"));
  }
  Serial.println(F("First to 5 points wins the match! Points indicated at strip ends."));
  Serial.println(F("========================================"));
}

void updateChallenge(unsigned long currentMillis) {
  // Continue updating ambient background on inner LEDs
  updateActiveLight(currentLightMode, currentMillis);

  GamerId challenger = gamerHandler.getChallenger();
  if (challenger == GAMER_PLAYER_A) {
    if (gamerHandler.wasPressed(SIDE_B)) {
      startGame(false); // Player B accepted!
      return;
    }
    if (gamerHandler.wasPressed(SIDE_A)) {
      // Challenger pressed again -> cancel challenge
      systemState = STATE_IDLE_AMBIENT;
      strip.clear();
      Serial.println(F("[Challenge Cancelled] Returning to ambient lighting."));
      return;
    }
  } else if (challenger == GAMER_PLAYER_B) {
    if (gamerHandler.wasPressed(SIDE_A)) {
      startGame(false); // Player A accepted!
      return;
    }
    if (gamerHandler.wasPressed(SIDE_B)) {
      // Challenger pressed again -> cancel challenge
      systemState = STATE_IDLE_AMBIENT;
      strip.clear();
      Serial.println(F("[Challenge Cancelled] Returning to ambient lighting."));
      return;
    }
  }

  // Update challenge blink and check timeout (10 blinks = 5.0 seconds)
  if (gamerHandler.updateChallengeBlink(currentMillis)) {
    Serial.println(F("[Challenge Timeout] No human opponent accepted within 10 blinks."));
    Serial.println(F("Starting game against autonomous NPC: RobotGamer!"));
    startGame(true); // Launch against RobotGamer
    return;
  }

  strip.show(); // Refresh strip on blink toggle
}

void updatePlaying(unsigned long currentMillis) {
  GameId game = gameKnob.getSelectedGame();

  // If match is against autonomous NPC, update RobotGamer
  if (gamerHandler.isRobot(gamerHandler.getRobotSide())) {
    robotGamer.update(currentMillis, game);
  }

  // Update game frame and mechanics
  updateActiveGame(game, currentMillis);

  // Check round outcome and point tracking via GamerHandler
  if (gamerHandler.isRoundOver() && !gamerHandler.isScoreTallied()) {
    gamerHandler.awardRoundPoint();

    if (gamerHandler.isRobot(gamerHandler.getRobotSide())) {
      robotGamer.resetRound();
    }

    // Check match victory condition (5 points)
    if (gamerHandler.isMatchWon()) {
      systemState = STATE_MATCH_OVER;
      inGameMode = false;
      matchOverTime = currentMillis;

      Serial.println(F("========================================"));
      Serial.print(F("[MATCH WON!] "));
      Serial.print(gamerHandler.getCurrentPlayerName(gamerHandler.getMatchWinnerSide()));
      Serial.println(F(" has won the match (5 points)!"));
      Serial.println(F("Triggering victory celebration on the LED strip!"));
      Serial.println(F("========================================"));
    }
  }
}

void updateMatchOver(unsigned long currentMillis) {
  const unsigned long CELEBRATION_DURATION = 4000; // 4 seconds celebration
  if (currentMillis - matchOverTime >= CELEBRATION_DURATION) {
    systemState = STATE_IDLE_AMBIENT;
    gamerHandler.resetScores();
    strip.clear();
    Serial.println(F("[Match Celebration Complete] Returning to ambient lighting mode."));
    return;
  }

  // Celebratory victory light effect across the strip in winner's color
  CRGB winColor = gamerHandler.getMatchWinnerColor();

  // Fast celebratory pulse wave using FastLED sin8 (avoids floating-point libm)
  uint8_t angle = (uint8_t)((((currentMillis - matchOverTime) % 600) * 256UL) / 600);
  uint8_t wave = sin8(angle);
  uint8_t br = 70 + scale8(wave, 185);

  CRGB pulseCol = CRGB(
    scale8(winColor.r, br),
    scale8(winColor.g, br),
    scale8(winColor.b, br)
  );

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = pulseCol;
    if (random(0, 35) == 0) {
      leds[i] = CRGB(255, 255, 255); // Sparkling victory stars
    }
  }

  strip.show(); // Note: renderScoreOverlay() will keep the winning score illuminated at the ends!
}

// ============================================================================
// 14. Serial Command Parser
// ============================================================================
void applySerialNumericCommand(NumTarget target, long val) {
  if (target == TARGET_HUE) {
    if (val < 0 || val > 65535) rotaryColorKnob.setWhite();
    else rotaryColorKnob.setHue((uint16_t)val);
    global_color = rotaryColorKnob.getRGB();
  } else if (target == TARGET_BRIGHTNESS) {
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    brightnessKnob.setBrightness((uint8_t)val);
    strip.setBrightness(brightnessKnob.getBrightness());
  } else if (target == TARGET_GAME) {
    if (val >= 0 && val < NUM_GAMES) {
      resetActiveGame((GameId)val);
      Serial.print(F("[Serial] Selected Game: "));
      Serial.println(GAME_NAMES[val]);
    }
  } else if (target == TARGET_MODE) {
    if (val >= 0 && val < NUM_LIGHT_MODES) {
      currentLightMode = (LightModeId)val;
      strip.clear();
      Serial.print(F("[Serial] Ambient Mode: "));
      Serial.println(LIGHT_MODE_NAMES[currentLightMode]);
    }
  }
}

void handleSerialCommands() {
  static char numBuf[8];
  static uint8_t numIdx = 0;
  static NumTarget numTarget = TARGET_NONE;
  static unsigned long lastDigitTime = 0;

  if (numTarget != TARGET_NONE && numIdx > 0 && (millis() - lastDigitTime > 60)) {
    numBuf[numIdx] = '\0';
    applySerialNumericCommand(numTarget, atol(numBuf));
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
          applySerialNumericCommand(numTarget, atol(numBuf));
        } else {
          if (numTarget == TARGET_BRIGHTNESS) {
            brightnessKnob.stepBrightness(25);
            strip.setBrightness(brightnessKnob.getBrightness());
          } else if (numTarget == TARGET_MODE) {
            currentLightMode = (LightModeId)(((int)currentLightMode + 1) % NUM_LIGHT_MODES);
            strip.clear();
            Serial.print(F("[Serial] Ambient Mode: "));
            Serial.println(LIGHT_MODE_NAMES[currentLightMode]);
          } else if (numTarget == TARGET_GAME) {
            GameId nextG = (GameId)(((int)gameKnob.getSelectedGame() + 1) % NUM_GAMES);
            resetActiveGame(nextG);
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
    } else if (c == 'k' || c == 'K') {
      numTarget = TARGET_GAME;
      numIdx = 0;
      lastDigitTime = millis();
    } else if (c == 'm' || c == 'M') {
      numTarget = TARGET_MODE;
      numIdx = 0;
      lastDigitTime = millis();
    } else if (c == '1') {
      gamerHandler.triggerVirtualPress(SIDE_A);
    } else if (c == '2') {
      gamerHandler.triggerVirtualPress(SIDE_B);
    } else if (c == 'g' || c == 'G') {
      if (systemState == STATE_IDLE_AMBIENT) {
        startChallenge(GAMER_PLAYER_A);
      } else {
        systemState = STATE_IDLE_AMBIENT;
        inGameMode = false;
        gamerHandler.resetScores();
        strip.clear();
        Serial.println(F("[Serial 'g'] Exited Game Mode -> Ambient Lighting"));
      }
    } else if (c == 'c' || c == 'C') {
      rotaryColorKnob.stepHue(4000);
      global_color = rotaryColorKnob.getRGB();
    }
  }
}

// ============================================================================
// 15. Arduino setup() and loop()
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
  systemState = STATE_IDLE_AMBIENT;
  gamerHandler.resetScores();

  Serial.println(F("BarCounter Light & Game System Initialized (FastLED SK6812, 12m)."));
  Serial.print(F("Initial Ambient Mode: "));
  Serial.println(lightBehaviorKnob.getSelectedModeName());
  Serial.print(F("Initial Game Selected: "));
  Serial.println(gameKnob.getSelectedGameName());
  Serial.println(F("Press Player A (Pin 2) or Player B (Pin 4) button to challenge!"));
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
    if (systemState == STATE_PLAYING) {
      resetActiveGame(newGame);
      gamerHandler.resetScores();
      if (gamerHandler.isRobot(gamerHandler.getRobotSide())) robotGamer.resetRound();
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
    if (systemState != STATE_IDLE_AMBIENT) {
      systemState = STATE_IDLE_AMBIENT;
      inGameMode = false;
      gamerHandler.resetScores();
      Serial.println(F("[Exit Game] -> Returning to Ambient Lighting Mode"));
    }
    strip.clear();
  }

  btnP1.update();
  btnP2.update();

  handleSerialCommands();

  // 2. Dual Button Hold (P1 + P2 for 800 ms) aborts game and returns to ambient
  static unsigned long dualHoldTime = 0;
  if (gamerHandler.isDown(SIDE_A) && gamerHandler.isDown(SIDE_B)) {
    if (dualHoldTime == 0) dualHoldTime = currentMillis;
    if (currentMillis - dualHoldTime > 800) {
      dualHoldTime = currentMillis + 2000;
      if (systemState != STATE_IDLE_AMBIENT) {
        systemState = STATE_IDLE_AMBIENT;
        inGameMode = false;
        gamerHandler.resetScores();
        strip.clear();
        Serial.println(F("[Dual Hold] Exited Game Mode -> Ambient Lighting"));
      } else {
        startChallenge(GAMER_PLAYER_A);
      }
    }
  } else {
    dualHoldTime = 0;
  }

  // 3. System State Machine Execution
  switch (systemState) {
    case STATE_IDLE_AMBIENT:
      // Ambient lighting mode. Check if Player A or B initiates a game challenge!
      if (gamerHandler.wasPressed(SIDE_A)) {
        startChallenge(GAMER_PLAYER_A);
      } else if (gamerHandler.wasPressed(SIDE_B)) {
        startChallenge(GAMER_PLAYER_B);
      } else {
        updateActiveLight(currentLightMode, currentMillis);
      }
      break;

    case STATE_CHALLENGE:
      updateChallenge(currentMillis);
      break;

    case STATE_PLAYING:
      updatePlaying(currentMillis);
      break;

    case STATE_MATCH_OVER:
      updateMatchOver(currentMillis);
      break;
  }
}
