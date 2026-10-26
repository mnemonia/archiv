# Plattenregal Light 2 Controller (SK6812 RGB LED Strip)

An Arduino-based lighting controller designed for vinyl record shelf (*Plattenregal*) illumination. It drives an addressable strip of **36 SK6812 RGB LEDs** arranged in a dual 18-LED row configuration (forward row A to B: LEDs 0..17, return row B to A: LEDs 18..35) using the **FastLED** library, featuring a non-blocking object-oriented state architecture, a master luminosity controller, and a 2-state switching knob. Both rows are synchronized so that each LED vis-à-vis shares the exact same color value.

---

## 1. System Overview & Core Features

* **Target Hardware**: Arduino Uno, Nano, Pro Mini, or compatible ATmega328P microcontrollers.
* **Lighting Hardware**: 36 individually addressable **SK6812 RGB LEDs** (GRB color order, $800\,\text{kHz}$ single-wire protocol):
  * **Forward row (A to B)**: LEDs `0 .. 17`.
  * **Return row (B to A)**: LEDs `18 .. 35`.
  * **Vis-à-Vis Synchronization**: For every position $x$ along the shelf ($0 \le x < 18$), `leds[x]` and `leds[35 - x]` are synchronized to the identical color.
* **Two Operating Modes**:
  1. **`NormalSmoothBreathingWarmWhite`**: Calibrated organic, soothing sinusoidal breathing pulsation across all 36 LEDs with real-time color selection.
  2. **`SmoothRgbRainbowLowFrequencyWave`**: Gentle, low-frequency traveling rainbow wave traversing the shelf, keeping opposing LEDs vis-à-vis in exact chromatic synchrony.
* **2-State Digital Mode Switch (Pin 11)**: Selects between the two modes via a physical toggle switch or 2-position rotary switch.
* **Dual-Section Potentiometer Control (Analog Pin A2)**:
  * **$0^\circ \text{ to } 180^\circ$ (ADC $0 \dots 511$) — ColorController Section**:
    * Step 1: Cold-White ($\sim 6500\,\text{K}$ crisp daylight white)
    * Step 2: Warm-White ($\sim 1900\,\text{K}$ candlelight warm white)
    * Steps 3..16: 14 distinct HUE steps across the $360^\circ$ chromatic spectrum
  * **$180^\circ \text{ to } 360^\circ$ (ADC $512 \dots 1023$) — BrightnessController Section**:
    * Dedicated potentiometer controlling overall strip luminosity from $0\%$ (off) to $100\%$ (full).
    * When the knob is rotated in the $0^\circ\text{--}180^\circ$ color-selection zone, brightness remains locked at its active level so colors can be previewed without extinguishing the strip.
* **Color Selection Protocol in `NormalSmoothBreathingWarmWhite`**:
  * **High Brightness During Selection**: Rotating the knob while in the color-selection zone ($0^\circ\text{--}180^\circ$) automatically boosts the strip to high brightness ($100\%$) so colors are clearly visible and vibrant.
  * **Color Accept Indication**: When the knob rests for $1000\,\text{ms}$ with no change, the color is accepted and confirmed with a crisp visual double-pulse animation on the strip before returning to master brightness and resuming calm breathing.
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
 - Sinusoidal Breathing (30s)        - 100% Saturated Pure Spectrum
 - 50 FPS Non-blocking               - 50 FPS Non-blocking
```

### Mode 1: `NormalSmoothBreathingWarmWhite`
* **Visual Character**: Gentle, organic pulsation across all 36 LEDs with customizable color selection via the ColorController section ($0^\circ\text{--}180^\circ$).
* **Color Selection Protocol**:
  * **Default Color**: Step 2 Warm-White (`CRGB(255, 147, 41)` $\sim 1900\,\text{K}$ candle flame / FastLED Candle standard).
  * **High-Brightness Real-Time Preview**: When the mode switch is on Mode 0 and the knob rotates in the $0^\circ\text{--}180^\circ$ zone, the strip automatically operates at maximum brightness ($255$) so the user can clearly preview and distinguish all colors across all 36 LEDs.
  * **Visual Color Accept Indication**: When the knob rests for $1000\,\text{ms}$ without movement, the selected color is accepted. The strip confirms the acceptance with a crisp $400\,\text{ms}$ visual double-pulse animation (`blend(baseColor, CRGB::White, 160)`), then restores the user's master brightness setting and smoothly transitions into 30-second sinusoidal breathing in the new color.
  * **16-Step Color Palette**:
    * **Step 1**: Cold-White ($\sim 6500\,\text{K}$, `CRGB(230, 245, 255)`)
    * **Step 2**: Warm-White ($\sim 1900\,\text{K}$ Candlelight, `CRGB(255, 147, 41)`)
    * **Steps 3..16**: 14 HUE steps (Red, Deep Orange, Amber Gold, Yellow, Lime Green, Pure Green, Emerald Teal, Cyan, Ocean Blue, Royal Blue, Indigo, Violet/Purple, Magenta, Rose Pink).
* **Breathing Dynamics**:
  * **Period**: 30 seconds per breath cycle (`BREATH_PERIOD = 30000 ms`) for a deeply calming, slow respiration.
  * **Waveform**: Sinusoidal $\frac{1 - \cos(\theta)}{2}$ curve for organic respiration.
  * **Range**: Oscillates smoothly between $75\%$ and $100\%$ of brightness, preserving an ambient warm glow without harsh shutoffs.
  * **Flicker-Free Rendering**: Strip output is guarded against double-updates and only re-latched when rendered color or brightness changes, eliminating PWM phase-reset beat frequencies.
* **Cadence**: 50 FPS (`UPDATE_INTERVAL = 20 ms`).

### Mode 2: `SmoothRgbRainbowLowFrequencyWave`
* **Visual Character**: Serene, low-frequency flowing rainbow wave traversing the vinyl shelf across dual synchronized rows.
* **Vis-à-Vis Synchronization**:
  * Forward strip (A to B, LEDs `0 .. 17`) and return strip (B to A, LEDs `18 .. 35`) are matched vis-à-vis.
  * For each position $x \in [0, 17]$, `leds[x]` and `leds[35 - x]` display the exact same color simultaneously.
* **Dynamics**:
  * **Temporal Frequency**: Full $360^\circ$ rainbow color rotation advances slowly at 50 FPS.
  * **Spatial Wave**: The full $360^\circ$ rainbow color cycle is stretched across $8 \times \text{NUM\_LEDS\_ROW}$ (144 LEDs, increased by factor 4 over earlier $2 \times \text{NUM\_LEDS\_ROW}$), spreading each color broadly so that adjacent LEDs along the shelf shift by only $\approx 1.77$ hue units for an ultra-subtle, serene monochromatic gradient.
  * **Vivid Spectrum**: Pixels are rendered at full saturation and value (`CHSV(pixelHue, 255, 255)`).
  * **Brightness Independence**: Master luminosity is governed purely by the `BrightnessController` in the $180^\circ\text{--}360^\circ$ zone on Pin A2, keeping colors pure and unshifted.
* **Cadence**: 50 FPS (`UPDATE_INTERVAL = 20 ms`).

---

## 4. Dual-Section Potentiometer Control (Pin A2)

* **Split Dial Zones**:
  * **$0^\circ \text{ to } 180^\circ$ (ADC $0 \dots 511$) — ColorController Section**:
    * Governs color selection in `NormalSmoothBreathingWarmWhite` across 16 stepped slots (Cold-White, Warm-White, 14 HUE steps).
    * Brightness remains held constant at its last adjusted level.
  * **$180^\circ \text{ to } 360^\circ$ (ADC $512 \dots 1023$) — BrightnessController Section**:
    * Governs master luminosity from $0\%$ (off) to $100\%$ (full) across both modes.
* **Zero Color Distortion in Brightness Section**:
  * Scaling is applied globally during hardware signal transmission via `FastLED.setBrightness()`.
  * Color values in the animation buffer are never altered or clamped by brightness adjustments.
* **Noise Suppression & Anti-Flicker**:
  * **4-Sample ADC Oversampling**: Averages ADC samples to cancel high-frequency electrical noise.
  * **3-Count Deadband**: Rejects wiper microphonics and minor voltage fluctuations.
  * **Exponential Moving Average (EMA)**: Applies a smooth filter ($\frac{3 \times \text{filtered} + \text{raw}}{4}$) for an analog, weighted feel.
  * **Full End-Stop Clamping**: Raw values $\le 516$ cleanly turn the LEDs off ($0$); raw values $\ge 1018$ reach maximum brightness ($255$).

---

## 5. Mode Switch Operation

The controller uses a physical 2-position switch or toggle switch connected to **Digital Pin 11**:
* Configured with internal pull-up (`INPUT_PULLUP`).
* **Terminal connected to GND (LOW)**: Mode 0 — `NormalSmoothBreathingWarmWhite`
* **Terminal open / disconnected (HIGH)**: Mode 1 — `SmoothRgbRainbowLowFrequencyWave`
* Includes a $50\,\text{ms}$ non-blocking software debouncer to suppress contact bounce and prevent lighting glitches during switching.

