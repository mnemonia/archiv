/*
 * ============================================================================
 * BarCounter Light & Interactive Game System (60 NeoPixel LEDs / 1 meter)
 * ============================================================================
 * 
 * Target: Arduino Uno R3 (ATmega328P)
 * Features:
 *   - Compile-time Dual Implementation:
 *       Implementation A: Serial Streaming to Linux Virtual Strip Visualizer (Dev)
 *       Implementation B: Direct Hardware Adafruit_NeoPixel Output (Production)
 *   - Global Rotary Knob Mood / Base-Color Control:
 *       Analog Pin A3 reads potentiometer wiper (0-5V).
 *       Keeps full range of rainbow spectrum (0..65535 across 0..1010 counts),
 *       with pure White added at the end of the dial (1012-1023 raw).
 *       Dynamically defines the initial- or mood-color for ALL modes!
 *   - 6 Ambient Visualization Modes (Ultra-slow, smooth, subtle, non-hectic):
 *       1. Breathing / Pulse (7.5s Meditative Sine Breath in Rotary Base-Color)
 *       2. Twinkle / Sparkle (Slow Floating Candlelight & Starfield in Rotary Base-Color)
 *       3. Fire / Flame (Cozy Slow-Ember Hearth Fire in Rotary Flame Tint)
 *       4. Comet / Meteor (Gentle Gliding Shooting Star in Rotary Base-Color)
 *       5. Scanner / Cylon (6.0s Smooth Larson Eye in Rotary Base-Color)
 *       6. Color Wipe (11s Meditative Chromatic Roll Anchored on Rotary Base-Color)
 *   - 6 Fast Competitive 2-Player 1-Button Games (One per mode):
 *       1. "Resonance Pulse" (Rhythm Wave Tug-of-War)
 *       2. "Sparkle Rush" (Nova Sparkle Reflex Deflector)
 *       3. "Flame Tug" (Bellows Forge Combustion Clash)
 *       4. "Meteor Deflector" (High-Speed Comet Rally)
 *       5. "Cylon Clash" (Hyper-Pong Beam Duel)
 *       6. "Territory Paint" (Rapid Wipe Wars)
 *   - Ambient Illumination Guard:
 *       Enforces that average strip luminosity NEVER drops below 35% threshold,
 *       guaranteeing room / bar counter illumination even during games.
 * 
 * Hardware Wiring:
 *   - Rotary Color Knob:      Pin A3 <--> Potentiometer Wiper (Outer pins to 5V and GND)
 *   - Rotary Brightness Knob: Pin A2 <--> Potentiometer Wiper (Outer pins to 5V and GND)
 *   - Player 1 Button:        Pin 2  <--> GND (uses internal pull-up)
 *   - Player 2 Button:        Pin 4  <--> GND (uses internal pull-up)
 *   - Mode Switch:            Pin 7  <--> GND (uses internal pull-up)
 *   - Physical Strip:         Pin 6  <--> NeoPixel DIN (Production Implementation B)
 * ============================================================================
 */

// ============================================================================
// 1. Development vs. Production Target Selection
// ============================================================================
#define BACKEND_VIRTUAL_SERIAL   0  // Implementation A: Stream pixel data to Linux Visualizer
#define BACKEND_ADAFRUIT_REAL    1  // Implementation B: Drive physical WS2812B strip via Adafruit library

// >>> CONFIGURE ACTIVE TARGET HERE <<<
#define STRIP_BACKEND            BACKEND_ADAFRUIT_REAL

// ============================================================================
// 2. Hardware Pin & Strip Configuration
// ============================================================================
#define NUM_LEDS                 15     // 60 LEDs per meter
#define LED_PIN                  6      // Output data pin for physical strip (Backend B)

#define PIN_COLOR_KNOB           A3     // Rotary knob potentiometer wiper for global color setting
#define PIN_BRIGHTNESS_KNOB      A2     // Rotary knob potentiometer wiper for global brightness setting
#define PIN_BTN_P1               2      // Player 1 input button (active LOW)
#define PIN_BTN_P2               4      // Player 2 input button (active LOW)
#define PIN_BTN_MODE             7      // Mode toggle button (active LOW)

#define SERIAL_BAUD              115200

// Minimum illumination threshold (0-255 scale, 88 ≈ 34.5% perceptual luminance)
#define MIN_LUMEN_THRESHOLD      88

#if (STRIP_BACKEND == BACKEND_ADAFRUIT_REAL)
  #include <Adafruit_NeoPixel.h>
#endif

// ============================================================================
// 3. Unified NeoPixel Driver (Implementation A & B)
// ============================================================================
class NeoPixelDriver {
private:
  uint8_t buffer[NUM_LEDS * 3]; // R, G, B per LED
  uint8_t globalBrightness;

#if (STRIP_BACKEND == BACKEND_ADAFRUIT_REAL)
  Adafruit_NeoPixel realStrip;
#endif

public:
  NeoPixelDriver() : globalBrightness(255)
#if (STRIP_BACKEND == BACKEND_ADAFRUIT_REAL)
    , realStrip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800)
#endif
  {}

  void begin() {
    clear();
    Serial.begin(SERIAL_BAUD);
#if (STRIP_BACKEND == BACKEND_VIRTUAL_SERIAL)
    Serial.print(F("\n[NEO_INIT:LEDS="));
    Serial.print(NUM_LEDS);
    Serial.println(F(":BACKEND_VIRTUAL_SERIAL]"));
#elif (STRIP_BACKEND == BACKEND_ADAFRUIT_REAL)
    realStrip.begin();
    realStrip.show();
#endif
  }

  void setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= NUM_LEDS) return;
    uint16_t offset = index * 3;
    buffer[offset]     = r;
    buffer[offset + 1] = g;
    buffer[offset + 2] = b;
  }

  void setPixelColor(uint16_t index, uint32_t color) {
    uint8_t r = (uint8_t)(color >> 16);
    uint8_t g = (uint8_t)(color >> 8);
    uint8_t b = (uint8_t)color;
    setPixelColor(index, r, g, b);
  }

  uint32_t getPixelColor(uint16_t index) const {
    if (index >= NUM_LEDS) return 0;
    uint16_t offset = index * 3;
    return ((uint32_t)buffer[offset] << 16) |
           ((uint32_t)buffer[offset + 1] << 8) |
           ((uint32_t)buffer[offset + 2]);
  }

  void setBrightness(uint8_t b) {
    globalBrightness = b;
#if (STRIP_BACKEND == BACKEND_ADAFRUIT_REAL)
    realStrip.setBrightness(b);
#endif
  }

  uint8_t getBrightness() const {
    return globalBrightness;
  }

  void clear() {
    memset(buffer, 0, sizeof(buffer));
  }

  uint16_t numPixels() const {
    return NUM_LEDS;
  }

  // Enforces that average strip luminance never drops below MIN_LUMEN_THRESHOLD
  void enforceIlluminationThreshold() {
    uint32_t totalLuminance = 0;
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      uint16_t idx = i * 3;
      // Perceptual luminance weights: (0.299*R + 0.587*G + 0.114*B)
      uint32_t lum = (uint32_t)buffer[idx] * 77 +
                     (uint32_t)buffer[idx + 1] * 150 +
                     (uint32_t)buffer[idx + 2] * 29;
      totalLuminance += (lum >> 8);
    }
    uint16_t avgLuminance = totalLuminance / NUM_LEDS;

    // If below required threshold, boost all pixels smoothly
    if (avgLuminance < MIN_LUMEN_THRESHOLD) {
      uint16_t deficit = MIN_LUMEN_THRESHOLD - avgLuminance;
      // Normalized warm baseline boost (0.299*1.48 + 0.587*0.88 + 0.114*0.28 ≈ 1.0)
      uint8_t boostR = (uint8_t)min(255, (deficit * 379) >> 8); // 1.48 * deficit
      uint8_t boostG = (uint8_t)min(255, (deficit * 225) >> 8); // 0.88 * deficit
      uint8_t boostB = (uint8_t)min(255, (deficit * 72)  >> 8); // 0.28 * deficit

      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        uint16_t idx = i * 3;
        buffer[idx]     = (uint8_t)min(255, (int)buffer[idx] + boostR);
        buffer[idx + 1] = (uint8_t)min(255, (int)buffer[idx + 1] + boostG);
        buffer[idx + 2] = (uint8_t)min(255, (int)buffer[idx + 2] + boostB);
      }
    }
  }

  void show() {
    enforceIlluminationThreshold();

#if (STRIP_BACKEND == BACKEND_VIRTUAL_SERIAL)
    // Implementation A: Transmit 185-byte binary frame to Linux Visualizer
    uint8_t header[4] = { 0xAA, 0x55, 0x01, NUM_LEDS };
    Serial.write(header, 4);

    uint8_t checksum = 0xAA ^ 0x55 ^ 0x01 ^ NUM_LEDS;
    for (uint16_t i = 0; i < sizeof(buffer); i++) {
      uint8_t byteVal = buffer[i];
      if (globalBrightness < 255) {
        byteVal = (uint8_t)(((uint16_t)byteVal * globalBrightness) >> 8);
      }
      Serial.write(byteVal);
      checksum ^= byteVal;
    }
    Serial.write(checksum);

#elif (STRIP_BACKEND == BACKEND_ADAFRUIT_REAL)
    // Implementation B: Direct Hardware Output via Adafruit_NeoPixel Library
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      uint16_t idx = i * 3;
      realStrip.setPixelColor(i, buffer[idx], buffer[idx + 1], buffer[idx + 2]);
    }
    realStrip.show();
#endif
  }

  static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }

  static uint32_t ColorHSV(uint16_t hue, uint8_t sat = 255, uint8_t val = 255) {
#if (STRIP_BACKEND == BACKEND_ADAFRUIT_REAL)
    return Adafruit_NeoPixel::ColorHSV(hue, sat, val);
#else
    // Standalone compact HSV to RGB converter for Backend A
    uint8_t r, g, b;
    uint8_t base = ((255 - sat) * val) >> 8;
    switch ((hue / 10922) % 6) {
      case 0:
        r = val;
        g = (((val - base) * (hue % 10922)) / 10922) + base;
        b = base;
        break;
      case 1:
        r = (((val - base) * (10922 - (hue % 10922))) / 10922) + base;
        g = val;
        b = base;
        break;
      case 2:
        r = base;
        g = val;
        b = (((val - base) * (hue % 10922)) / 10922) + base;
        break;
      case 3:
        r = base;
        g = (((val - base) * (10922 - (hue % 10922))) / 10922) + base;
        b = val;
        break;
      case 4:
        r = (((val - base) * (hue % 10922)) / 10922) + base;
        g = base;
        b = val;
        break;
      default:
        r = val;
        g = base;
        b = (((val - base) * (10922 - (hue % 10922))) / 10922) + base;
        break;
    }
    return Color(r, g, b);
#endif
  }
};

NeoPixelDriver strip;

// ============================================================================
// 4. Global Rotary Knob Controller (Rainbow Palette Mood / Base-Color with White)
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

  // Thresholds for White zone added at the end of potentiometer travel (1012-1023 out of 1023)
  static const int WHITE_ZONE_EXIT  = 1005;
  static const int WHITE_ZONE_ENTER = 1015;

  void applyRaw(int raw) {
    if (isWhiteMode) {
      if (raw < WHITE_ZONE_EXIT) {
        isWhiteMode = false;
      }
    } else {
      if (raw > WHITE_ZONE_ENTER) {
        isWhiteMode = true;
      }
    }

    if (isWhiteMode) {
      currentHue = 0;
      currentR = 255;
      currentG = 255;
      currentB = 255;
    } else {
      // Keep full range of rainbow (0..65535 hue) across entire dial (0..1010 counts)
      int clampedRaw = min(1010, max(0, raw));
      currentHue = (uint16_t)(((uint32_t)clampedRaw * 65535UL) / 1010UL);
      uint32_t rgb = NeoPixelDriver::ColorHSV(currentHue, 255, 255);
      currentR = (uint8_t)(rgb >> 16);
      currentG = (uint8_t)(rgb >> 8);
      currentB = (uint8_t)rgb;
    }
  }

public:
  RotaryColorKnob(uint8_t p)
    : pin(p), lastRaw(-1), currentHue(0), currentR(255), currentG(0), currentB(0),
      isWhiteMode(false), lastReadTime(0), virtualOverride(false), potAnchorRaw(-1) {}

  void begin() {
    pinMode(pin, INPUT);
    update(true);
  }

  void update(bool force = false) {
    unsigned long now = millis();
    if (!force && now - lastReadTime < 35) return; // 35ms update
    lastReadTime = now;

    int raw = analogRead(pin);

    if (virtualOverride) {
      // Soft Takeover: Virtual control (web app/serial) is currently active.
      // Ignore normal ADC noise and floating pin jitter.
      // Only release virtual override if physical potentiometer is turned
      // by more than 16 counts (~1.5% of knob travel).
      if (potAnchorRaw >= 0 && abs(raw - potAnchorRaw) > 16) {
        virtualOverride = false;
        lastRaw = raw;
        applyRaw(raw);
      }
      return;
    }

    // Normal hardware potentiometer tracking
    if (force || lastRaw < 0 || abs(raw - lastRaw) > 4) {
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
    currentR = 255;
    currentG = 255;
    currentB = 255;
    virtualOverride = true;
    potAnchorRaw = analogRead(pin);
  }

  void setHue(uint16_t hue) {
    isWhiteMode = false;
    currentHue = hue;
    uint32_t rgb = NeoPixelDriver::ColorHSV(currentHue, 255, 255);
    currentR = (uint8_t)(rgb >> 16);
    currentG = (uint8_t)(rgb >> 8);
    currentB = (uint8_t)rgb;
    virtualOverride = true;
    potAnchorRaw = analogRead(pin);
  }

  void stepHue(int16_t delta) {
    if (isWhiteMode) {
      setHue(0); // From White at end, wrap back to start of Rainbow (Red)
    } else if (currentHue >= 65535) {
      setWhite(); // Reached end of rainbow -> add White at end
    } else {
      uint32_t next = (uint32_t)currentHue + delta;
      if (next >= 65535) {
        setHue(65535); // Reach full end of rainbow
      } else {
        setHue((uint16_t)next);
      }
    }
  }

  bool isVirtualOverride() const { return virtualOverride; }
};

RotaryColorKnob rotaryKnob(PIN_COLOR_KNOB);
uint32_t global_color = NeoPixelDriver::Color(255, 0, 0);

// ============================================================================
// 5. Global Rotary Brightness Knob Controller
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
    // Map 0..1023 smoothly to full 0..255 brightness
    currentBrightness = (uint8_t)(((uint32_t)raw * 255UL) / 1023UL);
  }

public:
  RotaryBrightnessKnob(uint8_t p)
    : pin(p), lastRaw(-1), currentBrightness(255), lastReadTime(0),
      virtualOverride(false), potAnchorRaw(-1) {}

  void begin() {
    pinMode(pin, INPUT);
    update(true);
  }

  void update(bool force = false) {
    unsigned long now = millis();
    if (!force && now - lastReadTime < 35) return; // 35ms update
    lastReadTime = now;

    int raw = analogRead(pin);

    if (virtualOverride) {
      // Soft Takeover: Virtual control (web app/serial) is currently active.
      // Ignore normal ADC noise and floating pin jitter.
      // Only release virtual override if physical potentiometer is turned
      // by more than 16 counts (~1.5% of knob travel).
      if (potAnchorRaw >= 0 && abs(raw - potAnchorRaw) > 16) {
        virtualOverride = false;
        lastRaw = raw;
        applyRaw(raw);
      }
      return;
    }

    // Normal hardware potentiometer tracking
    if (force || lastRaw < 0 || abs(raw - lastRaw) > 4) {
      lastRaw = raw;
      applyRaw(raw);
    }
  }

  uint8_t getBrightness() const { return currentBrightness; }

  void setBrightness(uint8_t b) {
    currentBrightness = b;
    virtualOverride = true;
    potAnchorRaw = analogRead(pin);
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
// 6. Debounced Input Manager (Two Players + Mode Switch)
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
Button btnMode(PIN_BTN_MODE);

// ============================================================================
// 7. System Modes & States
// ============================================================================
enum SystemMode {
  MODE_BREATHING_PULSE = 0,
  MODE_TWINKLE_SPARKLE = 1,
  MODE_FIRE_FLAME      = 2,
  MODE_COMET_METEOR    = 3,
  MODE_CYLON_SCANNER   = 4,
  MODE_COLOR_WIPE      = 5,
  NUM_MODES            = 6
};

SystemMode currentMode = MODE_BREATHING_PULSE;
bool inGameMode = false;

// ============================================================================
// 8. Mode 1: Breathing / Pulse & "Resonance Pulse" Game
// ============================================================================
class BreathingPulseController {
private:
  unsigned long lastUpdate = 0;
  const unsigned long BREATH_PERIOD = 7500; // 7.5s ultra-slow soothing breath

  int16_t nexusPosition;
  unsigned long p1Cooldown;
  unsigned long p2Cooldown;
  bool roundOver;
  unsigned long roundOverTime;

public:
  BreathingPulseController() : nexusPosition(NUM_LEDS / 2), p1Cooldown(0), p2Cooldown(0), roundOver(false) {}

  void resetGame() {
    nexusPosition = NUM_LEDS / 2;
    p1Cooldown = 0;
    p2Cooldown = 0;
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < 25) return;
    lastUpdate = currentMillis;

    float phase = (float)(currentMillis % BREATH_PERIOD) / (float)BREATH_PERIOD;
    float wave = (sin(phase * 2.0f * PI) + 1.0f) * 0.5f;

    if (!inGameMode) {
      // Ambient: Breathing pulse in the Rotary Knob Mood-Color!
      float factor = 0.42f + wave * 0.40f;
      uint8_t r = (uint8_t)(rotaryKnob.getR() * factor);
      uint8_t g = (uint8_t)(rotaryKnob.getG() * factor);
      uint8_t b = (uint8_t)(rotaryKnob.getB() * factor);
      uint32_t color = NeoPixelDriver::Color(r, g, b);

      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, color);
      }
    } else {
      // Game: Fast Resonance Pulse
      if (roundOver) {
        if (currentMillis - roundOverTime > 2000) resetGame();
        return;
      }

      bool inResonance = (wave >= 0.88f);
      int16_t pushStep = max(1, (int)(NUM_LEDS / 15)); // Proportional pulse push

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
            p1Cooldown = currentMillis + 400;
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
            p2Cooldown = currentMillis + 400;
          }
        }
      }

      // Background ambient floor follows rotary base color
      float baseFactor = 0.35f + wave * 0.20f;
      uint8_t rBase = (uint8_t)(rotaryKnob.getR() * baseFactor);
      uint8_t gBase = (uint8_t)(rotaryKnob.getG() * baseFactor);
      uint8_t bBase = (uint8_t)(rotaryKnob.getB() * baseFactor);
      uint32_t baseCol = NeoPixelDriver::Color(rBase, gBase, bBase);
      for (uint16_t i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, baseCol);

      int glowWidth = max(2, (int)(NUM_LEDS * 0.13f + 0.5f));
      for (int i = 0; i <= nexusPosition; i++) {
        int dist = nexusPosition - i;
        uint8_t b = (dist < glowWidth) ? (255 - (dist * 210) / glowWidth) : 45;
        strip.setPixelColor(i, NeoPixelDriver::Color(15, (b * 130) >> 8, b));
      }
      for (int i = nexusPosition; i < NUM_LEDS; i++) {
        int dist = i - nexusPosition;
        uint8_t r = (dist < glowWidth) ? (255 - (dist * 210) / glowWidth) : 45;
        strip.setPixelColor(i, NeoPixelDriver::Color(r, (r * 65) >> 8, 15));
      }

      uint32_t nexusCol = inResonance ? NeoPixelDriver::Color(255, 255, 240) : NeoPixelDriver::Color(255, 190, 60);
      if (nexusPosition >= 0 && nexusPosition < NUM_LEDS) strip.setPixelColor(nexusPosition, nexusCol);
    }
    strip.show();
  }
};

BreathingPulseController pulseMode;

// ============================================================================
// 9. Mode 2: Twinkle / Sparkle & "Sparkle Rush" Game
// ============================================================================
class TwinkleSparkleController {
private:
  unsigned long lastUpdate = 0;
  uint8_t twinkleBrightness[NUM_LEDS];
  int8_t  twinkleDelta[NUM_LEDS];

  float novaPos;
  float novaSpeed;
  bool roundOver;
  unsigned long roundOverTime;

public:
  TwinkleSparkleController() : novaPos((float)NUM_LEDS * 0.5f), novaSpeed(max(0.25f, (float)NUM_LEDS * (0.95f / 60.0f))), roundOver(false) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      twinkleBrightness[i] = random(55, 160);
      twinkleDelta[i] = random(0, 2) == 0 ? 1 : -1;
    }
  }

  void resetGame() {
    novaPos = (float)NUM_LEDS * 0.5f;
    float baseSpeed = max(0.25f, (float)NUM_LEDS * (0.95f / 60.0f));
    novaSpeed = (random(0, 2) == 0 ? baseSpeed : -baseSpeed);
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < (inGameMode ? 20 : 50)) return;
    lastUpdate = currentMillis;

    // Drifting starfield
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      int nextB = (int)twinkleBrightness[i] + twinkleDelta[i];
      if (nextB >= 220) {
        twinkleDelta[i] = -1;
      } else if (nextB <= 58) {
        twinkleDelta[i] = 1;
      }
      twinkleBrightness[i] = (uint8_t)constrain(nextB, 58, 220);
    }

    if (!inGameMode) {
      // Ambient: Starry shimmer tinted by the Rotary Knob Mood-Color!
      uint8_t baseR = rotaryKnob.getR();
      uint8_t baseG = rotaryKnob.getG();
      uint8_t baseB = rotaryKnob.getB();

      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        uint8_t br = twinkleBrightness[i];
        uint8_t r = (uint8_t)(((uint16_t)baseR * br) >> 8);
        uint8_t g = (uint8_t)(((uint16_t)baseG * br) >> 8);
        uint8_t b = (uint8_t)(((uint16_t)baseB * br) >> 8);
        // Subtle white highlight on brightest sparkles
        if (br > 195) {
          uint8_t spark = (br - 195) * 2;
          r = min(255, (int)r + spark);
          g = min(255, (int)g + spark);
          b = min(255, (int)b + spark);
        }
        strip.setPixelColor(i, NeoPixelDriver::Color(r, g, b));
      }
    } else {
      // Game: Fast Nova Deflector
      if (roundOver) {
        if (currentMillis - roundOverTime > 2000) resetGame();
        return;
      }

      novaPos += novaSpeed;

      uint16_t shieldLeds = max((uint16_t)1, (uint16_t)((NUM_LEDS * 15 + 50) / 100)); // ~15% defense zone
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

      uint8_t baseR = rotaryKnob.getR();
      uint8_t baseG = rotaryKnob.getG();
      uint8_t baseB = rotaryKnob.getB();
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        uint8_t br = twinkleBrightness[i];
        uint8_t r = (uint8_t)(((uint16_t)baseR * br) >> 8);
        uint8_t g = (uint8_t)(((uint16_t)baseG * br) >> 8);
        uint8_t b = (uint8_t)(((uint16_t)baseB * br) >> 8);
        strip.setPixelColor(i, NeoPixelDriver::Color(r, g, b));
      }

      // Defense Shields (scaled to NUM_LEDS)
      for (uint16_t i = 0; i < shieldLeds; i++) strip.setPixelColor(i, NeoPixelDriver::Color(30, 110, 210));
      for (uint16_t i = NUM_LEDS - shieldLeds; i < NUM_LEDS; i++) strip.setPixelColor(i, NeoPixelDriver::Color(210, 50, 140));

      int center = (int)round(novaPos);
      if (center >= 0 && center < NUM_LEDS) {
        strip.setPixelColor(center, NeoPixelDriver::Color(255, 255, 240));
        if (center > 0) strip.setPixelColor(center - 1, NeoPixelDriver::Color(255, 200, 50));
        if (center < NUM_LEDS - 1) strip.setPixelColor(center + 1, NeoPixelDriver::Color(255, 200, 50));
      }
    }
    strip.show();
  }
};

TwinkleSparkleController twinkleMode;

// ============================================================================
// 10. Mode 3: Fire / Flame & "Flame Tug" Game
// ============================================================================
class FireFlameController {
private:
  unsigned long lastUpdate = 0;
  uint8_t heat[NUM_LEDS];
  int16_t flameClashPos;
  unsigned long lastTapP1;
  unsigned long lastTapP2;
  bool roundOver;
  unsigned long roundOverTime;

  uint32_t heatToColor(uint8_t temp, bool overrideColor = false, uint8_t orR = 0, uint8_t orG = 0, uint8_t orB = 0) {
    temp = max((uint8_t)80, temp);
    uint8_t t192 = (uint8_t)(((uint16_t)temp * 191) >> 8);
    uint8_t ramp = (t192 & 0x3F) << 2;

    uint8_t baseR = overrideColor ? orR : rotaryKnob.getR();
    uint8_t baseG = overrideColor ? orG : rotaryKnob.getG();
    uint8_t baseB = overrideColor ? orB : rotaryKnob.getB();

    if (t192 > 0x80) { // Hottest: blend toward white hot
      uint8_t r = min(255, (int)baseR + ramp);
      uint8_t g = min(255, (int)baseG + ramp);
      uint8_t b = min(255, (int)baseB + ramp);
      return NeoPixelDriver::Color(r, g, b);
    } else if (t192 > 0x40) { // Medium: full saturation of base color
      uint8_t factor = (t192 << 1);
      uint8_t r = (uint8_t)(((uint16_t)baseR * factor) >> 8);
      uint8_t g = (uint8_t)(((uint16_t)baseG * factor) >> 8);
      uint8_t b = (uint8_t)(((uint16_t)baseB * factor) >> 8);
      return NeoPixelDriver::Color(r, g, b);
    } else { // Coolest: dark ember base color
      uint8_t factor = max((uint8_t)60, (uint8_t)(t192 << 2));
      uint8_t r = (uint8_t)(((uint16_t)baseR * factor) >> 8);
      uint8_t g = (uint8_t)(((uint16_t)baseG * factor) >> 8);
      uint8_t b = (uint8_t)(((uint16_t)baseB * factor) >> 8);
      return NeoPixelDriver::Color(r, g, b);
    }
  }

public:
  FireFlameController() : flameClashPos(NUM_LEDS / 2), lastTapP1(0), lastTapP2(0), roundOver(false) {
    memset(heat, 90, sizeof(heat));
  }

  void resetGame() {
    flameClashPos = NUM_LEDS / 2;
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < (inGameMode ? 25 : 60)) return;
    lastUpdate = currentMillis;

    // Slow gentle cooling
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      uint8_t cooldown = inGameMode ? random(2, 5) : 1;
      heat[i] = (heat[i] > cooldown + 78) ? (heat[i] - cooldown) : 78;
    }

    if (!inGameMode) {
      // Ambient: Hearth fire in Rotary Knob Flame Tint!
      for (int k = NUM_LEDS - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
      }
      if (random(0, 10) < 2) {
        int sparkPos = random(0, NUM_LEDS);
        heat[sparkPos] = min(255, heat[sparkPos] + random(60, 110));
      }
      for (uint16_t i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, heatToColor(heat[i], false));
    } else {
      // Game: Fast Flame Tug
      if (roundOver) {
        if (currentMillis - roundOverTime > 2000) resetGame();
        return;
      }

      int16_t pushStep = max(1, (int)(NUM_LEDS / 30));

      if (btnP1.wasPressed()) {
        if (currentMillis - lastTapP1 > 120) {
          lastTapP1 = currentMillis;
          flameClashPos += pushStep;
          for (int j = 0; j <= flameClashPos; j++) heat[j] = min(255, heat[j] + 30);
          if (flameClashPos >= (int16_t)NUM_LEDS - 2) {
            roundOver = true;
            roundOverTime = currentMillis;
            flameClashPos = NUM_LEDS - 1;
          }
        }
      }

      if (btnP2.wasPressed()) {
        if (currentMillis - lastTapP2 > 120) {
          lastTapP2 = currentMillis;
          flameClashPos -= pushStep;
          for (int j = flameClashPos; j < NUM_LEDS; j++) heat[j] = min(255, heat[j] + 30);
          if (flameClashPos <= 1) {
            roundOver = true;
            roundOverTime = currentMillis;
            flameClashPos = 0;
          }
        }
      }

      // P1: Blue Forge; P2: Red Forge
      for (int i = 0; i <= flameClashPos; i++) strip.setPixelColor(i, heatToColor(heat[i], true, 0, 120, 255));
      for (int i = flameClashPos; i < NUM_LEDS; i++) strip.setPixelColor(i, heatToColor(heat[i], true, 255, 60, 0));
      if (flameClashPos >= 0 && flameClashPos < NUM_LEDS) {
        strip.setPixelColor(flameClashPos, NeoPixelDriver::Color(255, 255, 240));
      }
    }
    strip.show();
  }
};

FireFlameController fireMode;

// ============================================================================
// 11. Mode 4: Comet / Meteor & "Meteor Deflector" Game
// ============================================================================
class CometMeteorController {
private:
  unsigned long lastUpdate = 0;
  float headPos = 0.0f;
  float speed = 0.15f;
  int8_t direction = 1;

  float gameMeteorPos;
  float gameMeteorSpeed;
  bool roundOver = false;
  unsigned long roundOverTime = 0;

public:
  CometMeteorController() : gameMeteorPos((float)NUM_LEDS * 0.5f), gameMeteorSpeed(max(0.3f, (float)NUM_LEDS * (1.1f / 60.0f))) {}

  void resetGame() {
    gameMeteorPos = (float)NUM_LEDS * 0.5f;
    float baseSpeed = max(0.3f, (float)NUM_LEDS * (1.1f / 60.0f));
    gameMeteorSpeed = (random(0, 2) == 0 ? baseSpeed : -baseSpeed);
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < (inGameMode ? 20 : 45)) return;
    lastUpdate = currentMillis;

    if (!inGameMode) {
      // Ambient: Gliding comet in Rotary Knob Base-Color!
      uint8_t baseR = rotaryKnob.getR();
      uint8_t baseG = rotaryKnob.getG();
      uint8_t baseB = rotaryKnob.getB();

      headPos += speed * direction;
      if (headPos >= (float)(NUM_LEDS - 1)) {
        direction = -1;
        headPos = (float)(NUM_LEDS - 1);
      } else if (headPos <= 0.0f) {
        direction = 1;
        headPos = 0.0f;
      }

      // Base floor of rotary base color
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, NeoPixelDriver::Color(
          (baseR * 65) >> 8,
          (baseG * 65) >> 8,
          (baseB * 65) >> 8
        ));
      }

      // Comet tail fades in base color (scaled to NUM_LEDS)
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
          strip.setPixelColor(idx, NeoPixelDriver::Color(r, g, b));
        }
      }
    } else {
      // Game: Fast Meteor Deflector
      if (roundOver) {
        if (currentMillis - roundOverTime > 2000) resetGame();
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

      uint8_t baseR = rotaryKnob.getR();
      uint8_t baseG = rotaryKnob.getG();
      uint8_t baseB = rotaryKnob.getB();
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, NeoPixelDriver::Color((baseR * 60) >> 8, (baseG * 60) >> 8, (baseB * 60) >> 8));
      }
      for (uint16_t i = 0; i < defenseLeds; i++) strip.setPixelColor(i, NeoPixelDriver::Color(10, 80, 220));
      for (uint16_t i = NUM_LEDS - defenseLeds; i < NUM_LEDS; i++) strip.setPixelColor(i, NeoPixelDriver::Color(220, 60, 10));

      int idx = (int)round(gameMeteorPos);
      if (idx >= 0 && idx < NUM_LEDS) {
        strip.setPixelColor(idx, NeoPixelDriver::Color(255, 255, 240));
        if (idx > 0) strip.setPixelColor(idx - 1, NeoPixelDriver::Color(255, 180, 40));
        if (idx < NUM_LEDS - 1) strip.setPixelColor(idx + 1, NeoPixelDriver::Color(255, 180, 40));
      }
    }
    strip.show();
  }
};

CometMeteorController cometMode;

// ============================================================================
// 12. Mode 5: Scanner / Cylon & "Cylon Clash" Game
// ============================================================================
class CylonScannerController {
private:
  unsigned long lastUpdate = 0;
  const unsigned long SWEEP_PERIOD = 6000;

  float puckPos;
  float puckSpeed;
  bool roundOver = false;
  unsigned long roundOverTime = 0;

public:
  CylonScannerController() : puckPos((float)NUM_LEDS * 0.5f), puckSpeed(max(0.3f, (float)NUM_LEDS * (1.2f / 60.0f))) {}

  void resetGame() {
    puckPos = (float)NUM_LEDS * 0.5f;
    float baseSpeed = max(0.3f, (float)NUM_LEDS * (1.2f / 60.0f));
    puckSpeed = (random(0, 2) == 0 ? baseSpeed : -baseSpeed);
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < (inGameMode ? 18 : 35)) return;
    lastUpdate = currentMillis;

    if (!inGameMode) {
      // Ambient: Larson scanner sweeping in Rotary Knob Base-Color!
      uint8_t baseR = rotaryKnob.getR();
      uint8_t baseG = rotaryKnob.getG();
      uint8_t baseB = rotaryKnob.getB();

      float phase = (float)(currentMillis % SWEEP_PERIOD) / (float)SWEEP_PERIOD;
      float center = (float)(NUM_LEDS - 1) * 0.5f;
      float margin = max(0.5f, min(2.0f, (float)NUM_LEDS * 0.05f));
      float amplitude = max(0.5f, center - margin);
      float eyePos = center + amplitude * sin(phase * 2.0f * PI);
      float glowRadius = max(2.0f, min(6.0f, (float)NUM_LEDS * 0.1f));

      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        float dist = abs((float)i - eyePos);
        if (dist < 1.0f) {
          strip.setPixelColor(i, NeoPixelDriver::Color(
            min(255, (int)baseR + 80),
            min(255, (int)baseG + 80),
            min(255, (int)baseB + 80)
          ));
        } else if (dist < glowRadius) {
          uint8_t glow = (uint8_t)(255 * (1.0f - dist / glowRadius));
          strip.setPixelColor(i, NeoPixelDriver::Color(
            ((uint16_t)baseR * glow) >> 8,
            ((uint16_t)baseG * glow) >> 8,
            ((uint16_t)baseB * glow) >> 8
          ));
        } else {
          strip.setPixelColor(i, NeoPixelDriver::Color(
            (baseR * 60) >> 8,
            (baseG * 60) >> 8,
            (baseB * 60) >> 8
          ));
        }
      }
    } else {
      // Game: Fast Cylon Clash Pong
      if (roundOver) {
        if (currentMillis - roundOverTime > 2000) resetGame();
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

      uint8_t baseR = rotaryKnob.getR();
      uint8_t baseG = rotaryKnob.getG();
      uint8_t baseB = rotaryKnob.getB();
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, NeoPixelDriver::Color((baseR * 55) >> 8, (baseG * 55) >> 8, (baseB * 55) >> 8));
      }
      for (uint16_t i = 0; i < defenseLeds; i++) strip.setPixelColor(i, NeoPixelDriver::Color(15, 95, 215));
      for (uint16_t i = NUM_LEDS - defenseLeds; i < NUM_LEDS; i++) strip.setPixelColor(i, NeoPixelDriver::Color(215, 75, 15));

      int idx = (int)round(puckPos);
      if (idx >= 0 && idx < NUM_LEDS) {
        strip.setPixelColor(idx, NeoPixelDriver::Color(255, 255, 240));
        if (idx > 0) strip.setPixelColor(idx - 1, NeoPixelDriver::Color(255, 180, 50));
        if (idx < NUM_LEDS - 1) strip.setPixelColor(idx + 1, NeoPixelDriver::Color(255, 180, 50));
      }
    }
    strip.show();
  }
};

CylonScannerController cylonMode;

// ============================================================================
// 13. Mode 6: Color Wipe & "Territory Paint / Wipe Wars" Game
// ============================================================================
class ColorWipeController {
private:
  unsigned long lastUpdate = 0;
  uint16_t wipeIdx = 0;
  uint8_t paletteIdx = 0;

  int16_t paintBoundary;
  bool roundOver = false;
  unsigned long roundOverTime = 0;

public:
  ColorWipeController() : wipeIdx(0), paletteIdx(0), paintBoundary(NUM_LEDS / 2), roundOver(false), roundOverTime(0) {}

  void resetGame() {
    paintBoundary = NUM_LEDS / 2;
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < (inGameMode ? 20 : 180)) return;
    lastUpdate = currentMillis;

    if (!inGameMode) {
      // Ambient: Progressive color wipe starting from Rotary Knob Base-Color!
      // If White is selected, cycles through 4 elegant white temperature tints.
      // If Rainbow is selected, cycles through harmonic offsets: 0 (base), +60°, +120°, +180°
      uint32_t wipeCol;
      if (rotaryKnob.isWhite()) {
        const uint32_t whiteTints[4] = {
          NeoPixelDriver::Color(255, 255, 255), // Pure Crisp White
          NeoPixelDriver::Color(255, 220, 170), // Warm Candle White
          NeoPixelDriver::Color(215, 235, 255), // Cool Daylight White
          NeoPixelDriver::Color(255, 245, 215)  // Soft Neutral White
        };
        wipeCol = whiteTints[paletteIdx % 4];
      } else {
        uint16_t baseHue = rotaryKnob.getHue();
        uint16_t wipeHue = baseHue + (paletteIdx * 10922);
        wipeCol = NeoPixelDriver::ColorHSV(wipeHue, 255, 255);
      }

      strip.setPixelColor(wipeIdx, wipeCol);
      wipeIdx++;
      if (wipeIdx >= NUM_LEDS) {
        wipeIdx = 0;
        paletteIdx = (paletteIdx + 1) % 4;
      }
    } else {
      // Game: Fast Wipe Wars
      if (roundOver) {
        if (currentMillis - roundOverTime > 2000) resetGame();
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
        strip.setPixelColor(i, NeoPixelDriver::Color(14, 165, 233));
      }
      for (int i = paintBoundary + 1; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, NeoPixelDriver::Color(225, 29, 72));
      }
      if (paintBoundary >= 0 && paintBoundary < NUM_LEDS) {
        strip.setPixelColor(paintBoundary, NeoPixelDriver::Color(255, 255, 255));
      }
    }
    strip.show();
  }
};

ColorWipeController wipeMode;

// ============================================================================
// 14. Serial Command Parser (Optional Virtual Control from Linux)
// ============================================================================
void handleSerialCommands() {
  static char numBuf[8];
  static uint8_t numIdx = 0;
  enum NumTarget { TARGET_NONE, TARGET_HUE, TARGET_BRIGHTNESS };
  static NumTarget numTarget = TARGET_NONE;
  static unsigned long lastDigitTime = 0;

  // Auto-commit number if line terminator was missed and 60ms elapsed
  if (numTarget != TARGET_NONE && numIdx > 0 && (millis() - lastDigitTime > 60)) {
    numBuf[numIdx] = '\0';
    long val = atol(numBuf);
    if (numTarget == TARGET_HUE) {
      if (val < 0 || val > 65535) {
        rotaryKnob.setWhite();
      } else {
        rotaryKnob.setHue((uint16_t)val);
      }
      global_color = rotaryKnob.getRGB();
    } else if (numTarget == TARGET_BRIGHTNESS) {
      if (val < 0) val = 0;
      if (val > 255) val = 255;
      brightnessKnob.setBrightness((uint8_t)val);
      strip.setBrightness(brightnessKnob.getBrightness());
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
        // Terminator reached (e.g. \n, \r, or next command)
        numBuf[numIdx] = '\0';
        if (numIdx > 0) {
          long val = atol(numBuf);
          if (numTarget == TARGET_HUE) {
            if (val < 0 || val > 65535) {
              rotaryKnob.setWhite();
            } else {
              rotaryKnob.setHue((uint16_t)val);
            }
            global_color = rotaryKnob.getRGB();
          } else if (numTarget == TARGET_BRIGHTNESS) {
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            brightnessKnob.setBrightness((uint8_t)val);
            strip.setBrightness(brightnessKnob.getBrightness());
          }
        } else {
          // If 'b' arrived without digits, step brightness by +25
          if (numTarget == TARGET_BRIGHTNESS) {
            brightnessKnob.stepBrightness(25);
            strip.setBrightness(brightnessKnob.getBrightness());
          }
        }
        numTarget = TARGET_NONE;
        numIdx = 0;
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t' || c == ';') {
          continue; // Consume whitespace terminator
        }
        // Fall through to process c as next command
      }
    }

    if (c == 'w' || c == 'W') {
      rotaryKnob.setWhite();
      global_color = rotaryKnob.getRGB();
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
      btnMode.triggerVirtualPress();
    } else if (c == 'g' || c == 'G') {
      inGameMode = !inGameMode;
      if (inGameMode) {
        pulseMode.resetGame();
        twinkleMode.resetGame();
        fireMode.resetGame();
        cometMode.resetGame();
        cylonMode.resetGame();
        wipeMode.resetGame();
      }
    } else if (c == 'c' || c == 'C') {
      // Step hue by +4000 (~22 degrees around the rainbow)
      rotaryKnob.stepHue(4000);
      global_color = rotaryKnob.getRGB();
    }
  }
}

// ============================================================================
// 15. Arduino setup() and loop()
// ============================================================================
void setup() {
  rotaryKnob.begin();
  global_color = rotaryKnob.getRGB();

  brightnessKnob.begin();
  strip.setBrightness(brightnessKnob.getBrightness());

  btnP1.begin();
  btnP2.begin();
  btnMode.begin();

  randomSeed(analogRead(A0));

  strip.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Read hardware inputs
  rotaryKnob.update();
  global_color = rotaryKnob.getRGB();

  brightnessKnob.update();
  strip.setBrightness(brightnessKnob.getBrightness());

  btnP1.update();
  btnP2.update();
  btnMode.update();

  handleSerialCommands();

  // 2. Handle Mode Switch Button (Cycle through all 6 modes)
  if (btnMode.wasPressed()) {
    currentMode = (SystemMode)(((int)currentMode + 1) % NUM_MODES);
    inGameMode = false;
    strip.clear();
  }

  // 3. Hold P1 + P2 for 800ms to toggle Game Mode
  if (btnP1.isDown() && btnP2.isDown()) {
    static unsigned long dualHoldTime = 0;
    if (dualHoldTime == 0) dualHoldTime = currentMillis;
    if (currentMillis - dualHoldTime > 800) {
      inGameMode = !inGameMode;
      dualHoldTime = currentMillis + 2000;
      if (inGameMode) {
        pulseMode.resetGame();
        twinkleMode.resetGame();
        fireMode.resetGame();
        cometMode.resetGame();
        cylonMode.resetGame();
        wipeMode.resetGame();
      }
    }
  }

  // 4. Update Active Mode Controller
  switch (currentMode) {
    case MODE_BREATHING_PULSE: pulseMode.update(currentMillis); break;
    case MODE_TWINKLE_SPARKLE: twinkleMode.update(currentMillis); break;
    case MODE_FIRE_FLAME:      fireMode.update(currentMillis); break;
    case MODE_COMET_METEOR:    cometMode.update(currentMillis); break;
    case MODE_CYLON_SCANNER:   cylonMode.update(currentMillis); break;
    case MODE_COLOR_WIPE:      wipeMode.update(currentMillis); break;
  }
}
