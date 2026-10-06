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
 *   - 7 Ambient Visualization Modes (Ultra-slow, smooth, subtle, non-hectic):
 *       1. Breathing / Pulse (7.5s Meditative Warm-White 2700K Sine Breath)
 *       2. Twinkle / Sparkle (Slow Floating Candlelight & Starfield)
 *       3. Fire / Flame (Cozy Slow-Ember Hearth Fire)
 *       4. Chase / Marquee (Vintage Slow-Crawling Theater Marquee)
 *       5. Comet / Meteor (Gentle Gliding Shooting Star with Dissolving Tail)
 *       6. Scanner / Cylon (6.0s Smooth Larson Eye with Sine Deceleration)
 *       7. Color Wipe (11s Meditative Chromatic Roll)
 *   - 7 Fast Competitive 2-Player 1-Button Games (One per mode):
 *       1. "Resonance Pulse" (Rhythm Wave Tug-of-War)
 *       2. "Sparkle Rush" (Nova Sparkle Reflex Deflector)
 *       3. "Flame Tug" (Bellows Forge Combustion Clash)
 *       4. "Marquee Intercept" (Phase-Lock Precision Target Catch)
 *       5. "Meteor Deflector" (High-Speed Comet Rally)
 *       6. "Cylon Clash" (Hyper-Pong Beam Duel)
 *       7. "Territory Paint" (Rapid Wipe Wars)
 *   - Ambient Illumination Guard:
 *       Enforces that average strip luminosity NEVER drops below 35% threshold,
 *       guaranteeing room / bar counter illumination even during games.
 * 
 * Hardware Wiring:
 *   - Player 1 Button: Pin 2 <--> GND (uses internal pull-up)
 *   - Player 2 Button: Pin 4 <--> GND (uses internal pull-up)
 *   - Mode Switch:     Pin 7 <--> GND (uses internal pull-up)
 *   - Physical Strip:  Pin 6 <--> NeoPixel DIN (Production Implementation B)
 * ============================================================================
 */

// ============================================================================
// 1. Development vs. Production Target Selection
// ============================================================================
#define BACKEND_VIRTUAL_SERIAL   0  // Implementation A: Stream pixel data to Linux Visualizer
#define BACKEND_ADAFRUIT_REAL    1  // Implementation B: Drive physical WS2812B strip via Adafruit library

// >>> CONFIGURE ACTIVE TARGET HERE <<<
#define STRIP_BACKEND            BACKEND_VIRTUAL_SERIAL

// ============================================================================
// 2. Hardware Pin & Strip Configuration
// ============================================================================
#define NUM_LEDS                 60     // 60 LEDs per meter
#define LED_PIN                  6      // Output data pin for physical strip (Backend B)

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
#if (STRIP_BACKEND == BACKEND_VIRTUAL_SERIAL)
    Serial.begin(SERIAL_BAUD);
    Serial.println(F("\n[NEO_INIT:60_LEDS:BACKEND_VIRTUAL_SERIAL]"));
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

    // If below required threshold, boost all pixels smoothly with warm white
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
};

NeoPixelDriver strip;

// ============================================================================
// 4. Debounced Input Manager (Two Players + Mode Switch)
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
// 5. System Modes & States
// ============================================================================
enum SystemMode {
  MODE_BREATHING_PULSE = 0,
  MODE_TWINKLE_SPARKLE = 1,
  MODE_FIRE_FLAME      = 2,
  MODE_THEATER_CHASE   = 3,
  MODE_COMET_METEOR    = 4,
  MODE_CYLON_SCANNER   = 5,
  MODE_COLOR_WIPE      = 6,
  NUM_MODES            = 7
};

SystemMode currentMode = MODE_BREATHING_PULSE;
bool inGameMode = false;

// ============================================================================
// 6. Mode 1: Breathing / Pulse (7.5s Meditative Breath) & "Resonance Pulse"
// ============================================================================
class BreathingPulseController {
private:
  unsigned long lastUpdate = 0;
  const unsigned long BREATH_PERIOD = 7500; // 7.5s ultra-slow soothing breath

  int8_t nexusPosition;
  unsigned long p1Cooldown;
  unsigned long p2Cooldown;
  bool roundOver;
  unsigned long roundOverTime;

public:
  BreathingPulseController() : nexusPosition(30), p1Cooldown(0), p2Cooldown(0), roundOver(false) {}

  void resetGame() {
    nexusPosition = 30;
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
      // Ambient: Ultra-slow, very subtle warm-white breathing (42% to 82%)
      float factor = 0.42f + wave * 0.40f;
      uint8_t r = (uint8_t)(255 * factor);
      uint8_t g = (uint8_t)(148 * factor);
      uint8_t b = (uint8_t)(38 * factor);
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

      if (btnP1.wasPressed()) {
        if (currentMillis >= p1Cooldown) {
          if (inResonance) {
            nexusPosition += 4;
            if (nexusPosition >= 58) {
              roundOver = true;
              roundOverTime = currentMillis;
              nexusPosition = 59;
            }
          } else {
            p1Cooldown = currentMillis + 400;
          }
        }
      }

      if (btnP2.wasPressed()) {
        if (currentMillis >= p2Cooldown) {
          if (inResonance) {
            nexusPosition -= 4;
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

      float baseFactor = 0.35f + wave * 0.20f;
      uint32_t baseWarm = NeoPixelDriver::Color((uint8_t)(255 * baseFactor), (uint8_t)(140 * baseFactor), (uint8_t)(35 * baseFactor));
      for (uint16_t i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, baseWarm);

      for (int i = 0; i <= nexusPosition; i++) {
        int dist = nexusPosition - i;
        uint8_t b = (dist < 8) ? (255 - dist * 26) : 45;
        strip.setPixelColor(i, NeoPixelDriver::Color(15, (b * 130) >> 8, b));
      }
      for (int i = nexusPosition; i < NUM_LEDS; i++) {
        int dist = i - nexusPosition;
        uint8_t r = (dist < 8) ? (255 - dist * 26) : 45;
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
// 7. Mode 2: Twinkle / Sparkle (Slow Floating Candlelight) & "Sparkle Rush"
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
  TwinkleSparkleController() : novaPos(30.0f), novaSpeed(0.95f), roundOver(false) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      twinkleBrightness[i] = random(55, 160);
      twinkleDelta[i] = random(0, 2) == 0 ? 1 : -1;
    }
  }

  void resetGame() {
    novaPos = 30.0f;
    novaSpeed = (random(0, 2) == 0 ? 0.95f : -0.95f);
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < (inGameMode ? 20 : 50)) return; // 50ms slow ambient!
    lastUpdate = currentMillis;

    // Ambient: Gentle drifting starfield, unhurried delta
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
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        uint8_t b = twinkleBrightness[i];
        strip.setPixelColor(i, NeoPixelDriver::Color(b, (b * 190) >> 8, (b * 115) >> 8));
      }
    } else {
      // Game: Fast Nova Deflector
      if (roundOver) {
        if (currentMillis - roundOverTime > 2000) resetGame();
        return;
      }

      novaPos += novaSpeed;

      if (btnP1.wasPressed()) {
        if (novaPos >= 0.0f && novaPos <= 9.0f && novaSpeed < 0.0f) {
          float bonus = (9.0f - novaPos) * 0.15f;
          novaSpeed = abs(novaSpeed) * 1.12f + bonus;
          novaSpeed = min(novaSpeed, 2.8f);
        }
      }

      if (btnP2.wasPressed()) {
        if (novaPos >= 50.0f && novaPos <= 59.0f && novaSpeed > 0.0f) {
          float bonus = (novaPos - 50.0f) * 0.15f;
          novaSpeed = -(abs(novaSpeed) * 1.12f + bonus);
          novaSpeed = max(novaSpeed, -2.8f);
        }
      }

      if (novaPos < -1.0f || novaPos > 60.0f) {
        roundOver = true;
        roundOverTime = currentMillis;
      }

      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        uint8_t b = twinkleBrightness[i];
        strip.setPixelColor(i, NeoPixelDriver::Color(b, (b * 180) >> 8, (b * 105) >> 8));
      }

      // Defense Shields
      for (uint16_t i = 0; i < 9; i++) strip.setPixelColor(i, NeoPixelDriver::Color(30, 110, 210));
      for (uint16_t i = 51; i < 60; i++) strip.setPixelColor(i, NeoPixelDriver::Color(210, 50, 140));

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
// 8. Mode 3: Fire / Flame (Cozy Slow Embers) & "Flame Tug"
// ============================================================================
class FireFlameController {
private:
  unsigned long lastUpdate = 0;
  uint8_t heat[NUM_LEDS];
  int8_t flameClashPos;
  unsigned long lastTapP1;
  unsigned long lastTapP2;
  bool roundOver;
  unsigned long roundOverTime;

  uint32_t heatToColor(uint8_t temp, bool blueFlame = false) {
    temp = max((uint8_t)80, temp);
    uint8_t t192 = (uint8_t)(((uint16_t)temp * 191) >> 8);
    uint8_t ramp = (t192 & 0x3F) << 2;

    if (!blueFlame) {
      if (t192 > 0x80) return NeoPixelDriver::Color(255, 255, ramp);
      if (t192 > 0x40) return NeoPixelDriver::Color(255, ramp, 0);
      return NeoPixelDriver::Color(ramp, 0, 0);
    } else {
      if (t192 > 0x80) return NeoPixelDriver::Color(ramp, 255, 255);
      if (t192 > 0x40) return NeoPixelDriver::Color(0, ramp, 255);
      return NeoPixelDriver::Color(0, 0, ramp);
    }
  }

public:
  FireFlameController() : flameClashPos(30), lastTapP1(0), lastTapP2(0), roundOver(false) {
    memset(heat, 90, sizeof(heat));
  }

  void resetGame() {
    flameClashPos = 30;
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < (inGameMode ? 25 : 60)) return; // 60ms slow ambient!
    lastUpdate = currentMillis;

    // Slow gentle cooling
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      uint8_t cooldown = inGameMode ? random(2, 5) : 1;
      heat[i] = (heat[i] > cooldown + 78) ? (heat[i] - cooldown) : 78;
    }

    if (!inGameMode) {
      // Ambient: Slow cozy hearth smoothing
      for (int k = NUM_LEDS - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
      }
      if (random(0, 10) < 2) { // Calm, infrequent spark
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

      if (btnP1.wasPressed()) {
        if (currentMillis - lastTapP1 > 120) {
          lastTapP1 = currentMillis;
          flameClashPos += 2;
          for (int j = 0; j <= flameClashPos; j++) heat[j] = min(255, heat[j] + 30);
          if (flameClashPos >= 58) {
            roundOver = true;
            roundOverTime = currentMillis;
            flameClashPos = 59;
          }
        }
      }

      if (btnP2.wasPressed()) {
        if (currentMillis - lastTapP2 > 120) {
          lastTapP2 = currentMillis;
          flameClashPos -= 2;
          for (int j = flameClashPos; j < NUM_LEDS; j++) heat[j] = min(255, heat[j] + 30);
          if (flameClashPos <= 1) {
            roundOver = true;
            roundOverTime = currentMillis;
            flameClashPos = 0;
          }
        }
      }

      for (int i = 0; i <= flameClashPos; i++) strip.setPixelColor(i, heatToColor(heat[i], true));
      for (int i = flameClashPos; i < NUM_LEDS; i++) strip.setPixelColor(i, heatToColor(heat[i], false));
      if (flameClashPos >= 0 && flameClashPos < NUM_LEDS) {
        strip.setPixelColor(flameClashPos, NeoPixelDriver::Color(255, 255, 240));
      }
    }
    strip.show();
  }
};

FireFlameController fireMode;

// ============================================================================
// 9. Mode 4: Chase / Marquee (Vintage Slow-Crawling) & "Marquee Intercept"
// ============================================================================
class TheaterChaseController {
private:
  unsigned long lastUpdate = 0;
  uint8_t stepOffset = 0;

  // Game: Marquee Intercept
  int8_t targetScore = 0; // -10 (P2 wins) to +10 (P1 wins)
  bool roundOver = false;
  unsigned long roundOverTime = 0;

public:
  void resetGame() {
    targetScore = 0;
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    unsigned long interval = inGameMode ? 32 : 220; // 220ms ultra-slow ambient marquee!
    if (currentMillis - lastUpdate < interval) return;
    lastUpdate = currentMillis;

    stepOffset = (stepOffset + 1) % 4;

    if (!inGameMode) {
      // Ambient: Vintage slow theater marquee with warm golden amber glow
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        if ((i + stepOffset) % 4 == 0) {
          strip.setPixelColor(i, NeoPixelDriver::Color(255, 190, 60)); // Highlight bulb
        } else {
          strip.setPixelColor(i, NeoPixelDriver::Color(80, 45, 12));   // Ambient warm floor
        }
      }
    } else {
      // Game: Fast Marquee Intercept
      // Active marquee dot cycles fast. Target zones: P1 on 10..18, P2 on 42..50
      if (roundOver) {
        if (currentMillis - roundOverTime > 2000) resetGame();
        return;
      }

      uint8_t activeHotIndex = (stepOffset * 15) % NUM_LEDS;

      if (btnP1.wasPressed()) {
        if (activeHotIndex >= 10 && activeHotIndex <= 18) {
          targetScore += 2;
          if (targetScore >= 10) {
            roundOver = true;
            roundOverTime = currentMillis;
          }
        }
      }

      if (btnP2.wasPressed()) {
        if (activeHotIndex >= 42 && activeHotIndex <= 50) {
          targetScore -= 2;
          if (targetScore <= -10) {
            roundOver = true;
            roundOverTime = currentMillis;
          }
        }
      }

      // Render base warm background
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, NeoPixelDriver::Color(70, 40, 15));
      }

      // Render P1 Target Zone (Cyan) and P2 Target Zone (Amber)
      for (int i = 10; i <= 18; i++) strip.setPixelColor(i, NeoPixelDriver::Color(20, 90, 180));
      for (int i = 42; i <= 50; i++) strip.setPixelColor(i, NeoPixelDriver::Color(180, 70, 20));

      // Fast spinning marquee dots
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        if ((i + stepOffset) % 6 == 0) {
          strip.setPixelColor(i, NeoPixelDriver::Color(255, 255, 220));
        }
      }

      // Tug-of-war tug marker in center based on score
      int markerPos = 30 + targetScore * 2;
      markerPos = constrain(markerPos, 0, 59);
      strip.setPixelColor(markerPos, NeoPixelDriver::Color(255, 255, 255));
    }
    strip.show();
  }
};

TheaterChaseController chaseMode;

// ============================================================================
// 10. Mode 5: Comet / Meteor (Gentle Gliding Star) & "Meteor Deflector"
// ============================================================================
class CometMeteorController {
private:
  unsigned long lastUpdate = 0;
  float headPos = 0.0f;
  float speed = 0.15f; // Very slow gliding speed (~13s across strip)
  int8_t direction = 1;

  // Game: Meteor Deflector state
  float gameMeteorPos = 30.0f;
  float gameMeteorSpeed = 1.1f;
  bool roundOver = false;
  unsigned long roundOverTime = 0;

public:
  void resetGame() {
    gameMeteorPos = 30.0f;
    gameMeteorSpeed = (random(0, 2) == 0 ? 1.1f : -1.1f);
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < (inGameMode ? 20 : 45)) return;
    lastUpdate = currentMillis;

    if (!inGameMode) {
      // Ambient: Ultra-slow graceful gliding comet with dissolving tail
      headPos += speed * direction;
      if (headPos >= 59.0f) {
        direction = -1;
        headPos = 59.0f;
      } else if (headPos <= 0.0f) {
        direction = 1;
        headPos = 0.0f;
      }

      // Draw warm ambient floor first
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, NeoPixelDriver::Color(65, 38, 12));
      }

      // Draw soft comet tail (8 LEDs long)
      for (int t = 0; t < 12; t++) {
        float p = headPos - (t * direction);
        int idx = (int)round(p);
        if (idx >= 0 && idx < NUM_LEDS) {
          uint8_t factor = (12 - t) * 18;
          strip.setPixelColor(idx, NeoPixelDriver::Color(255, min(255, 140 + factor), factor));
        }
      }
    } else {
      // Game: Fast Meteor Deflector
      if (roundOver) {
        if (currentMillis - roundOverTime > 2000) resetGame();
        return;
      }

      gameMeteorPos += gameMeteorSpeed;

      if (btnP1.wasPressed()) {
        if (gameMeteorPos >= 0.0f && gameMeteorPos <= 9.0f && gameMeteorSpeed < 0.0f) {
          float bonus = (9.0f - gameMeteorPos) * 0.18f;
          gameMeteorSpeed = abs(gameMeteorSpeed) * 1.14f + bonus;
          gameMeteorSpeed = min(gameMeteorSpeed, 3.2f);
        }
      }

      if (btnP2.wasPressed()) {
        if (gameMeteorPos >= 50.0f && gameMeteorPos <= 59.0f && gameMeteorSpeed > 0.0f) {
          float bonus = (gameMeteorPos - 50.0f) * 0.18f;
          gameMeteorSpeed = -(abs(gameMeteorSpeed) * 1.14f + bonus);
          gameMeteorSpeed = max(gameMeteorSpeed, -3.2f);
        }
      }

      if (gameMeteorPos < -1.0f || gameMeteorPos > 60.0f) {
        roundOver = true;
        roundOverTime = currentMillis;
      }

      for (uint16_t i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, NeoPixelDriver::Color(60, 35, 15));
      for (int i = 0; i < 9; i++) strip.setPixelColor(i, NeoPixelDriver::Color(10, 80, 220));
      for (int i = 51; i < 60; i++) strip.setPixelColor(i, NeoPixelDriver::Color(220, 60, 10));

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
// 11. Mode 6: Scanner / Cylon (6.0s Smooth Larson Eye) & "Cylon Clash"
// ============================================================================
class CylonScannerController {
private:
  unsigned long lastUpdate = 0;
  const unsigned long SWEEP_PERIOD = 6000; // 6.0 seconds per full dual sweep

  // Game: Cylon Clash
  float puckPos = 30.0f;
  float puckSpeed = 1.2f;
  bool roundOver = false;
  unsigned long roundOverTime = 0;

public:
  void resetGame() {
    puckPos = 30.0f;
    puckSpeed = (random(0, 2) == 0 ? 1.2f : -1.2f);
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < (inGameMode ? 18 : 35)) return;
    lastUpdate = currentMillis;

    if (!inGameMode) {
      // Ambient: Larson scanner with smooth cosine edge deceleration
      float phase = (float)(currentMillis % SWEEP_PERIOD) / (float)SWEEP_PERIOD;
      float eyePos = 29.5f + 27.5f * sin(phase * 2.0f * PI);

      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        float dist = abs((float)i - eyePos);
        if (dist < 1.0f) {
          strip.setPixelColor(i, NeoPixelDriver::Color(255, 170, 40));
        } else if (dist < 6.0f) {
          uint8_t glow = (uint8_t)(255 * (1.0f - dist / 6.0f));
          strip.setPixelColor(i, NeoPixelDriver::Color(glow, (glow * 60) >> 8, 15));
        } else {
          strip.setPixelColor(i, NeoPixelDriver::Color(60, 32, 10)); // Ambient floor
        }
      }
    } else {
      // Game: Fast Cylon Clash Pong
      if (roundOver) {
        if (currentMillis - roundOverTime > 2000) resetGame();
        return;
      }

      puckPos += puckSpeed;

      if (btnP1.wasPressed()) {
        if (puckPos >= 0.0f && puckPos <= 9.0f && puckSpeed < 0.0f) {
          float bonus = (9.0f - puckPos) * 0.16f;
          puckSpeed = abs(puckSpeed) * 1.15f + bonus;
          puckSpeed = min(puckSpeed, 3.4f);
        }
      }

      if (btnP2.wasPressed()) {
        if (puckPos >= 50.0f && puckPos <= 59.0f && puckSpeed > 0.0f) {
          float bonus = (puckPos - 50.0f) * 0.16f;
          puckSpeed = -(abs(puckSpeed) * 1.15f + bonus);
          puckSpeed = max(puckSpeed, -3.4f);
        }
      }

      if (puckPos < -1.0f || puckPos > 60.0f) {
        roundOver = true;
        roundOverTime = currentMillis;
      }

      for (uint16_t i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, NeoPixelDriver::Color(55, 30, 10));
      for (int i = 0; i < 9; i++) strip.setPixelColor(i, NeoPixelDriver::Color(15, 95, 215));
      for (int i = 51; i < 60; i++) strip.setPixelColor(i, NeoPixelDriver::Color(215, 75, 15));

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
// 12. Mode 7: Color Wipe (11s Meditative Roll) & "Territory Paint / Wipe Wars"
// ============================================================================
class ColorWipeController {
private:
  unsigned long lastUpdate = 0;
  uint8_t wipeIdx = 0;
  uint8_t paletteIdx = 0;

  // Game: Territory Paint state
  int8_t paintBoundary = 30; // 0..59 (0 = P2 wins, 59 = P1 wins)
  bool roundOver = false;
  unsigned long roundOverTime = 0;

  const uint32_t PALETTES[4] = {
    0xFFA028, // Sunset Amber
    0x9333EA, // Twilight Purple
    0x0EA5E9, // Ocean Teal
    0xE11D48  // Rose Quartz
  };

public:
  void resetGame() {
    paintBoundary = 30;
    roundOver = false;
  }

  void update(unsigned long currentMillis) {
    if (currentMillis - lastUpdate < (inGameMode ? 20 : 180)) return; // 180ms per LED ≈ 11s wipe!
    lastUpdate = currentMillis;

    if (!inGameMode) {
      // Ambient: Slow meditative progressive color roll
      strip.setPixelColor(wipeIdx, PALETTES[paletteIdx]);
      wipeIdx++;
      if (wipeIdx >= NUM_LEDS) {
        wipeIdx = 0;
        paletteIdx = (paletteIdx + 1) % 4;
      }
    } else {
      // Game: Fast Wipe Wars (Territory Paint)
      if (roundOver) {
        if (currentMillis - roundOverTime > 2000) resetGame();
        return;
      }

      if (btnP1.wasPressed()) {
        paintBoundary += 2;
        if (paintBoundary >= 59) {
          roundOver = true;
          roundOverTime = currentMillis;
          paintBoundary = 59;
        }
      }

      if (btnP2.wasPressed()) {
        paintBoundary -= 2;
        if (paintBoundary <= 0) {
          roundOver = true;
          roundOverTime = currentMillis;
          paintBoundary = 0;
        }
      }

      // Render P1 Paint (Neon Cyan)
      for (int i = 0; i <= paintBoundary; i++) {
        strip.setPixelColor(i, NeoPixelDriver::Color(14, 165, 233));
      }
      // Render P2 Paint (Hot Magenta)
      for (int i = paintBoundary + 1; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, NeoPixelDriver::Color(225, 29, 72));
      }
      // Paint Clash Wave in middle
      if (paintBoundary >= 0 && paintBoundary < NUM_LEDS) {
        strip.setPixelColor(paintBoundary, NeoPixelDriver::Color(255, 255, 255));
      }
    }
    strip.show();
  }
};

ColorWipeController wipeMode;

// ============================================================================
// 13. Serial Command Parser (Optional Virtual Control from Linux)
// ============================================================================
void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '1') {
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
        chaseMode.resetGame();
        cometMode.resetGame();
        cylonMode.resetGame();
        wipeMode.resetGame();
      }
    }
  }
}

// ============================================================================
// 14. Arduino setup() and loop()
// ============================================================================
void setup() {
  btnP1.begin();
  btnP2.begin();
  btnMode.begin();

  randomSeed(analogRead(A0));

  strip.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  btnP1.update();
  btnP2.update();
  btnMode.update();

#if (STRIP_BACKEND == BACKEND_VIRTUAL_SERIAL)
  handleSerialCommands();
#endif

  // Handle Mode Switch Button (Cycle through all 7 modes)
  if (btnMode.wasPressed()) {
    currentMode = (SystemMode)(((int)currentMode + 1) % NUM_MODES);
    inGameMode = false;
    strip.clear();
  }

  // Hold P1 + P2 for 800ms to toggle Game Mode
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
        chaseMode.resetGame();
        cometMode.resetGame();
        cylonMode.resetGame();
        wipeMode.resetGame();
      }
    }
  }

  // Update Active Mode Controller
  switch (currentMode) {
    case MODE_BREATHING_PULSE: pulseMode.update(currentMillis); break;
    case MODE_TWINKLE_SPARKLE: twinkleMode.update(currentMillis); break;
    case MODE_FIRE_FLAME:      fireMode.update(currentMillis); break;
    case MODE_THEATER_CHASE:   chaseMode.update(currentMillis); break;
    case MODE_COMET_METEOR:    cometMode.update(currentMillis); break;
    case MODE_CYLON_SCANNER:   cylonMode.update(currentMillis); break;
    case MODE_COLOR_WIPE:      wipeMode.update(currentMillis); break;
  }
}
