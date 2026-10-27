# Rectangle Light Controller (SK6812 LED Strip)

An Arduino firmware controller driving a **350-LED SK6812 RGB** strip mounted along a **2.36 m × 0.55 m rectangular frame** using the **FastLED** library. Designed with a non-blocking state machine architecture and mode selection via a 3-step switch knob.

---

## 1. Physical Geometry & LED Layout

The LED strip runs along the perimeter of a 2.36 m × 0.55 m rectangular frame. At a density of 60 LEDs per meter, the total strip perimeter is 5.82 meters with 350 addressable LEDs:

```text
                  Edge 2: Top (2.36 m / 142 LEDs, Indices 175..316)
       ┌─────────────────────────────────────────────────────────────┐
       │ < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < │
Edge 3 │                                                             │ Edge 1
Left   │ v                                                         ^ │ Right
(0.55m)│ v                                                         ^ │ (0.55m)
33 LEDs│                                                             │ 33 LEDs
(317.. │                                                             │ (142..
  349) │ > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > │  174)
       └─────────────────────────────────────────────────────────────┘
  (Data IN)       Edge 0: Bottom (2.36 m / 142 LEDs, Indices 0..141)
```

| Edge | Axis | Length | LED Count | Pixel Indices | Direction |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Edge 0** | Bottom ($X$) | 2.36 m | 142 LEDs | `0 .. 141` | Left $\rightarrow$ Right |
| **Edge 1** | Right ($Y$) | 0.55 m | 33 LEDs | `142 .. 174` | Bottom $\rightarrow$ Top |
| **Edge 2** | Top ($X$) | 2.36 m | 142 LEDs | `175 .. 316` | Right $\rightarrow$ Left |
| **Edge 3** | Left ($Y$) | 0.55 m | 33 LEDs | `317 .. 349` | Top $\rightarrow$ Bottom |
| **Total** | **Perimeter** | **5.82 m** | **350 LEDs** | `0 .. 349` | Closed loop |

---

## 2. Mode Explanations

The controller implements three distinct lighting modes using an object-oriented state pattern. All animations run non-blockingly using `millis()` intervals.

### Mode 1: `NormalPulsingWarmWhite` *(Default / Boot Mode)*
* **Mood & Visual Character**: Cozy architectural mood light with a gentle sinusoidal breathing pulse across the entire frame.
* **Dual-Section Potentiometer Control (Pin A3)**:
  An analog potentiometer knob on **Analog Pin A3** provides a split-control interface:
  * **$0^\circ\text{ to }180^\circ$ (ADC $0\text{--}511$) — `ColorController` Section (16 Distinct Steps)**:
    Divided into 16 discrete steps (32 ADC counts each):
    * **Step 1**: Cold-White ($\sim 6500\text{K}$ crisp daylight white, `CRGB(230, 245, 255)`)
    * **Step 2**: Warm-White ($\sim 1900\text{K}$ candlelight warm white, `CRGB(255, 147, 41)`)
    * **Steps 3..16**: 14 curated HUE steps across the $360^\circ$ chromatic spectrum:
      1. Step 3: Red (`CHSV(0, 255, 255)`)
      2. Step 4: Deep Orange (`CHSV(16, 255, 255)`)
      3. Step 5: Amber Gold (`CHSV(32, 255, 255)`)
      4. Step 6: Yellow (`CHSV(48, 255, 255)`)
      5. Step 7: Lime Green (`CHSV(72, 255, 255)`)
      6. Step 8: Pure Green (`CHSV(96, 255, 255)`)
      * Step 9: Emerald Teal (`CHSV(116, 255, 255)`)
      * Step 10: Cyan (`CHSV(136, 255, 255)`)
      * Step 11: Ocean Blue (`CHSV(156, 255, 255)`)
      * Step 12: Royal Blue (`CHSV(176, 255, 255)`)
      * Step 13: Indigo (`CHSV(196, 255, 255)`)
      * Step 14: Violet / Purple (`CHSV(216, 255, 255)`)
      * Step 15: Magenta (`CHSV(232, 255, 255)`)
      * Step 16: Rose Pink (`CHSV(246, 255, 255)`)
  * **$180^\circ\text{ to }360^\circ$ (ADC $512\text{--}1023$) — `BrightnessController` Section**:
    Smoothly modulates master brightness from 0% to 100% luminosity (`FastLED.setBrightness(0..255)`). When rotating in the $0^\circ\text{ to }180^\circ$ color-selection zone, brightness remains locked at its active level.
* **Color Selection Protocol**:
  1. **Real-Time Preview**: When rotated inside the $0^\circ\text{--}180^\circ$ color zone, the system immediately switches to high brightness (`FastLED.setBrightness(255)`) and displays the selected color across all 350 LEDs.
  2. **1000ms Stability Commitment**: When the knob remains untouched in the color zone for **1000 ms**, the color is committed to memory as the active breathing color.
  3. **Visual Confirmation Indication**: An immediate 400 ms double-pulse visual confirmation flash (`blend(baseColor, CRGB::White, 160)`) signals that the selection was accepted.
  4. **Resume Breathing**: Master brightness is restored to the `BrightnessController` level, and calm breathing resumes in the new color.
* **Anti-Flicker Hardware Filtering**:
  4-sample oversampling with 4-count deadband and Exponential Moving Average (EMA) filtering eliminates ADC noise and wiper jitter.
* **Animation Dynamics**:
  * Employs a low-frequency sinusoidal breathing wave ($\sin$) across the selected color.
  * Brightness gently sways between **40%** (minimum factor 0.40) and **85%** (maximum factor 0.85).
  * A full breath cycle takes **4.5 seconds** (`PULSE_PERIOD = 4500 ms`), providing a calming, organic rhythm without harsh flicker.
* **Update Cadence**: 40 FPS (25 ms interval).

---

### Mode 2: `ConcertSmoothRGBBreath`
* **Mood & Visual Character**: Ultra-slow, fluid, and serene ambient illumination designed for music listening, lounge environments, and meditative spaces.
* **Potentiometer Brightness Control (Pin A3)**:
  In this mode, the potentiometer knob on **Analog Pin A3** exclusively controls master brightness across the entire $0^\circ\text{ to }360^\circ$ rotation (ADC $0\text{--}1023 \rightarrow 0\text{--}255$), with no color selection or dead zones.
* **Animation Dynamics**:
  * **Ultra-Slow 300-Second Full RGB Rotation**: The base color wheel rotates through the full $360^\circ$ spectrum over **300 seconds (5 full minutes)** (`COLOR_ROTATION_PERIOD_MS = 300000 ms`).
  * **Super Smooth 16-Bit Sub-Hue Blending**: Uses FastLED's `blend()` with 8-bit fractional interpolation between adjacent integer hues ($65,536$ continuous color micro-steps). Completely eliminates discrete color jumping, ensuring imperceptibly smooth analog-like color drift.
  * **Planar Wave through the Rectangle**: A gentle traveling brightness wave sweeps across the $2.36\,\text{m}$ width over a leisurely **10.0-second period** using FastLED's 16-bit sinusoidal interpolation (`sin16`).
  * **Vis-à-Vis Color & Wave Synchronization**: Edge 0 (Bottom) and Edge 2 (Top) are mirrored across the $X$-axis ($x_{\text{top}} = 316 - i$). The LED directly opposite across the frame (vis-à-vis) receives the **exact same color and wave brightness/phase** at every point along the 2.36-meter span.
  * **Overarching Breathing Envelope**: A gentle 8.0-second respiratory wave (`BREATH_PERIOD_MS = 8000 ms`) slowly modulates overall luminosity between $80\%$ and $100\%$.
* **Update Cadence**: 50 FPS (20 ms interval).

---

### Mode 3: `PartyHUEMeteorSparkle`
* **Mood & Visual Character**: Ultra-vibrant, multifaceted party spectacle combining dynamic perimeter meteors, starlight sparkles, and cycling atmospheric background HUE behaviors.
* **Potentiometer Brightness Control (Pin A3)**:
  The potentiometer knob on **Analog Pin A3** exclusively controls master brightness across the entire $0^\circ\text{ to }360^\circ$ rotation (ADC $0\text{--}1023 \rightarrow 0\text{--}255$), providing instant, smooth dimming control without altering background behaviors or meteor colors.
* **Three Cycling HUE Behaviors (Random Order & Random Durations)**:
  * The background continuously transitions through three distinct lighting behaviors, chosen in random order with random durations ($8\text{--}22\text{ seconds}$ each):
    1. **`SmoothHUETransition`**: Fluid, flowing atmospheric ambient rainbow gradient continuously gliding along the 2.36 m rectangular span. Kept at moderate luminance (~35%) to give high contrast for the meteors.
    2. **`RawPoliceAlarmLights`**: High-voltage emergency strobe bar emulation. Generates authentic alternating quad-strobes in deep Red (left half, $X < 71$) and electric Blue (right half, $X \ge 71$) with dark inter-flash gaps for high-contrast impact.
    3. **`Supernova`**: Multi-phase cosmic explosion cycle:
       - *Buildup*: Accelerating deep cosmic magenta pulsation.
       - *Detonation*: Dazzling white-hot stellar explosion flashing across the entire frame.
       - *Shockwave Expansion*: High-speed radiant shockwave expanding outward from the center to both perimeter edges, leaving behind expanding fiery nebula plasma.
* **Meteors (Stable Random Color, Not Affected by Behavior)**:
  * **Independent Stable Random Colors**: When a meteor spawns, it is assigned a random, highly saturated color (`CHSV(random8(), 255, 255)`). This color stays **strictly stable from launch to conclusion** and is never shifted or desaturated by the active background behavior.
  * **Smooth Disappearing Tails**: Each meteor renders an analytical non-linear tail ($22\text{--}37\text{ LEDs}$). As the meteor completes its journey (`NUM_LEDS + tailLength`), the tail gracefully clears off the strip before a new meteor is launched with a fresh random color.
  * **Overlay Blending**: Meteors are composited using alpha overlay blending (`blend(background, meteorColor, alpha)`), ensuring that meteor heads (with white-hot cores) and luminous tails cleanly dominate over any background behavior without color distortion (e.g., a green meteor retains its pure green color even while traversing red police strobes).
* **Dynamic Starlight Sparkles**:
  * An independent sparkle layer randomly ignites brilliant pure-white and neon glints across the perimeter, creating an energetic diamond sparkle effect that organically decays over several frames.
* **Update Cadence**: 50 FPS (20 ms interval).

---

### Mode 4 (Legacy): `PartyMeteorSparkle`
* Classic multi-meteor perimeter chase with fading comet tails (`fadeToBlackBy`) and random glitter. Potentiometer controls master brightness.

---

## 3. Mode Selection (3-Step Digital Switch)

Mode selection is driven directly by a **3-position ON-OFF-ON toggle switch** or a **3-step rotary switch** wired to two digital pins with internal pull-up resistors:
* **Pin 11 (`PIN_SWITCH_STEP1`)**: Left terminal (Step 1)
* **Pin 12 (`PIN_SWITCH_STEP3`)**: Right terminal (Step 3)
* **GND**: Center / Common terminal

| Switch Position | Pin 11 | Pin 12 | Selected Mode | Notes |
| :--- | :---: | :---: | :--- | :--- |
| **Step 1 (Left)** | `LOW` | `HIGH` | **`NormalPulsingWarmWhite`** | Boot default (color controlled via A3 pot) |
| **Step 2 (Center)** | `HIGH` | `HIGH` | **`ConcertSmoothRGBBreath`** | Open / floating position |
| **Step 3 (Right)** | `HIGH` | `LOW` | **`PartyHUEMeteorSparkle`** | Switched to GND |

*Includes a 50 ms debounce filter (`DEBOUNCE_DELAY_MS`) to prevent state flapping during mechanical switching.*

---

## 4. Hardware Wiring & Safety

### Pin Assignments
| Function | Arduino Pin | Description |
| :--- | :--- | :--- |
| **LED Data (DIN)** | `Digital Pin 6` | Output to SK6812 DIN (via a 330–470 $\Omega$ series resistor) |
| **Switch Step 1** | `Digital Pin 11` | Left switch terminal (`INPUT_PULLUP`) |
| **Switch Step 3** | `Digital Pin 12` | Right switch terminal (`INPUT_PULLUP`) |
| **Dual-Control Potentiometer** | `Analog Pin A3` | 10k Potentiometer wiper. Dual-Section control in `NormalPulsingWarmWhite` (0°..180° ColorController, 180°..360° BrightnessController); full 0°..360° master brightness control in Concert and Party modes |
| **Floating Seed** | `Analog Pin A0` | Unconnected pin used for `randomSeed(analogRead(A0))` |

### Power Supply & Injection Guidelines
* 350 SK6812 LEDs at full white can draw up to $\approx 10.5\text{ A}$ at 5V DC.
* **Firmware Brightness Safety**: The controller initializes brightness safely from the potentiometer setting and disables temporal dithering (`FastLED.setDither(0)`) to eliminate any high-frequency flicker and power-supply hunting.
* **Power Injection**: For best color uniformity and to prevent voltage drop over the 5.82-meter run, inject 5V DC power at both ends of the strip (or at diagonal corners) from an external 5V regulated power supply (rated for at least 4A–6A). Connect Arduino GND to power supply GND.

---

## 5. Software Architecture

* **Framework**: FastLED (`#include <FastLED.h>`)
* **LED Driver**: Native `SK6812` clockless controller with `GRB` color order.
* **State Machine Pattern**:
  * `LightState`: Abstract base class declaring lifecycle hooks `enter(ctx)` and `update(ctx, currentMillis)`.
  * `LightContext`: Manages current state pointer and handles serial diagnostic transitions.
  * Dedicated concrete classes: `NormalPulsingWarmWhiteState`, `ConcertSmoothRGBBreathState`, `PartyHUEMeteorSparkleState`, and `PartyMeteorSparkleState`.
  * Dedicated input controllers: `ColorController` (16 steps) and `BrightnessController` (EMA-filtered analog input with dual-zone support).
* **Zero Delay Guarantee**: Every routine is driven by monotonic `millis()` elapsed timers, ensuring smooth frame rates and responsive switch detection at all times.

