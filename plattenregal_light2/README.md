# Plattenregal Light 2 Controller (SK6812 RGB LED Strip)

An Arduino-based lighting controller designed for vinyl record shelf (*Plattenregal*) illumination. It drives an addressable strip of **18 SK6812 RGB LEDs** using the **FastLED** library, featuring a non-blocking object-oriented state architecture, a master luminosity controller, and a 2-state switching knob.

---

## 1. System Overview & Core Features

* **Target Hardware**: Arduino Uno, Nano, Pro Mini, or compatible ATmega328P microcontrollers.
* **Lighting Hardware**: 18 individually addressable **SK6812 RGB LEDs** (GRB color order, $800\,\text{kHz}$ single-wire protocol).
* **Two Operating Modes**:
  1. **`NormalSmoothBreathingWarmWhite`**: Calibrated $\sim 1900\,\text{K}$ warm candlelight white with organic, soothing sinusoidal breathing pulsation.
  2. **`SmoothRgbRainbowLowFrequencyWave`**: Gentle, low-frequency traveling rainbow wave traversing across all 18 LEDs.
* **2-State Digital Mode Switch (Pin 11)**: Selects between the two modes via a physical toggle switch or 2-position rotary switch.
* **Master BrightnessController (Analog Pin A2)**:
  * Dedicated potentiometer controlling overall strip luminosity from $0\%$ (off) to $100\%$ (full).
  * **Strict Color Invariance Rule**: The brightness knob modulates luminosity only and has **no influence on the colors** set by the active mode.
* **Flicker-Free Design**:
  * Temporal dithering disabled (`FastLED.setDither(0)`) to eliminate high-frequency micro-flickering on SK6812 chips.
  * Analog input filtering with 4-sample oversampling, 3-count noise deadband, and Exponential Moving Average (EMA) smoothing to eliminate wiper jitter and rail-dip crosstalk.
* **100% Reactive & Non-Blocking**:
  * Driven strictly by monotonic `millis()` timing.
  * **Zero `delay()`**, **zero `delayMicroseconds()`**, and **zero spin-locks**.

---

## 2. Hardware Wiring & Pin Mapping

### Pin Connection Summary

| Arduino Pin | Peripheral | Connection Details | Purpose |
| :--- | :--- | :--- | :--- |
| **`Digital Pin 6`** | **SK6812 LED Strip** | Connected to Strip DIN via $330\text{--}470\,\Omega$ series resistor | FastLED SK6812 high-speed data line |
| **`Digital Pin 11`** | **2-State Switch** | Connected to Switch Terminal (`INPUT_PULLUP` to GND) | **LOW**: Warm White Breathing<br>**HIGH**: Rainbow Wave |
| **`Analog Pin A2`** | **Brightness Pot** | Potentiometer Wiper (Center Pin) | Global luminosity control ($0\%\text{--}100\%$) |
| **`5V`** | **Power Supply** | Common 5V DC Rail | Powers Arduino, Strip VDD, and Potentiometer |
| **`GND`** | **Ground** | Common Ground Bus | Shared Ground reference across all components |

> [!TIP]
> Place a $1000\,\mu\text{F}$ electrolytic capacitor across the $+5\,\text{V}$ and $\text{GND}$ power lines near the LED strip input to absorb transient power surges.

---

## 3. Operating Modes

```text
                  2-State Switching Knob
             ┌──────────────────────────────┐
             │                              │
             ▼                              ▼
 [NormalSmoothBreathingWarmWhite]   [SmoothRgbRainbowLowFrequencyWave]
 - Warm Candlelight White (~1900K)   - Low-Frequency Rainbow Wave (~12.5s)
 - Sinusoidal Breathing (~4.5s)      - 100% Saturated Pure Spectrum
 - 50 FPS Non-blocking               - 50 FPS Non-blocking
```

### Mode 1: `NormalSmoothBreathingWarmWhite`
* **Visual Character**: Gentle, organic pulsation across all 18 LEDs in calibrated warm candlelight white.
* **Color Calibration**:
  * Calibrated to `CRGB(255, 147, 41)` ($\sim 1900\,\text{K}$ candle flame / FastLED Candle standard).
  * The RGB channel ratios are scaled proportionally during breathing so the warm candlelight tone remains strictly constant throughout the entire breathing stroke.
* **Breathing Dynamics**:
  * **Period**: 4.5 seconds per breath cycle (`BREATH_PERIOD = 4500 ms`).
  * **Waveform**: Sinusoidal $\frac{1 - \cos(\theta)}{2}$ curve for organic respiration.
  * **Range**: Oscillates smoothly between $35\%$ and $90\%$ of peak brightness, preserving an ambient warm glow without harsh shutoffs or power-rail bounce.
* **Cadence**: 50 FPS (`UPDATE_INTERVAL = 20 ms`).

### Mode 2: `SmoothRgbRainbowLowFrequencyWave`
* **Visual Character**: Serene, low-frequency flowing rainbow wave traversing the vinyl shelf.
* **Dynamics**:
  * **Temporal Frequency**: Full $360^\circ$ rainbow color rotation advances slowly over $\approx 12.5$ seconds (`hueAccumulator += 105` at 50 FPS).
  * **Spatial Wave**: The full $360^\circ$ rainbow color cycle is stretched across $8 \times \text{NUM\_LEDS}$ (144 LEDs, increased by factor 4 over the earlier $2 \times \text{NUM\_LEDS}$), spreading each color broadly so that adjacent LEDs shift by only $\approx 1.77$ hue units for an ultra-subtle, serene monochromatic gradient.
  * **Vivid Spectrum**: Pixels are rendered at full saturation and value (`CHSV(pixelHue, 255, 255)`).
  * **Brightness Independence**: Master luminosity is governed purely by the `BrightnessController` on Pin A2, keeping colors pure and unshifted.
* **Cadence**: 50 FPS (`UPDATE_INTERVAL = 20 ms`).

---

## 4. Global BrightnessController (Pin A2)

* **Independent Operation**: Modulates the FastLED master scale factor via `FastLED.setBrightness()`.
* **Zero Color Distortion**:
  * Scaling is applied globally during hardware signal transmission.
  * Color values in the animation buffer are never altered or clamped by brightness adjustments.
  * Warm white remains true warm white; rainbow hues remain vibrant and saturated at all brightness levels.
* **Noise Suppression & Anti-Flicker**:
  * **4-Sample ADC Oversampling**: Averages ADC samples to cancel high-frequency electrical noise.
  * **3-Count Deadband**: Rejects wiper microphonics and minor voltage fluctuations.
  * **Exponential Moving Average (EMA)**: Applies a smooth filter ($\frac{3 \times \text{filtered} + \text{raw}}{4}$) for an analog, weighted feel.
  * **Full End-Stop Clamping**: Raw values $\le 3$ cleanly turn the LEDs off ($0$); raw values $\ge 1020$ reach maximum brightness ($255$).

---

## 5. Mode Switch Operation

The controller uses a physical 2-position switch or toggle switch connected to **Digital Pin 11**:
* Configured with internal pull-up (`INPUT_PULLUP`).
* **Terminal connected to GND (LOW)**: Mode 0 — `NormalSmoothBreathingWarmWhite`
* **Terminal open / disconnected (HIGH)**: Mode 1 — `SmoothRgbRainbowLowFrequencyWave`
* Includes a $50\,\text{ms}$ non-blocking software debouncer to suppress contact bounce and prevent lighting glitches during switching.

