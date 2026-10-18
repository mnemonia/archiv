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
* **Potentiometer Color Selection (Pin A3)**:
  An analog potentiometer knob on **Analog Pin A3** allows real-time continuous color selection across three dedicated rotation zones ($0^\circ\text{--}360^\circ$):
  * **$0^\circ\text{ to }180^\circ$ (ADC $0\text{--}511$) — Full RGB Color Selection**:
    Smooth continuous 360-degree rainbow spectrum with 16-bit sub-hue interpolation. Sweeps seamlessly from Red through Orange, Yellow, Green, Cyan, Blue, Magenta, and back to Red.
  * **$180^\circ\text{ to }270^\circ$ (ADC $512\text{--}767$) — Designer Curated Tones**:
    Continuous blend through three tuned ambient tones:
    * At $180^\circ$: **`nice-blue`** (rich luminous sapphire blue, `CRGB(10, 95, 255)`)
    * At $225^\circ$: **`nice-red`** (warm vibrant ruby red, `CRGB(255, 20, 30)`)
    * At $270^\circ$: **`nice-purple`** (deep royal amethyst purple, `CRGB(175, 25, 240)`)
  * **$270^\circ\text{ to }360^\circ$ (ADC $768\text{--}1023$) — Tunable White (Cold-White to Warm-White)**:
    Continuous Correlated Color Temperature (CCT) tuning from crisp daylight cold-white ($\sim 6500\text{K}$, `CRGB(225, 240, 255)`) through neutral white ($\sim 4000\text{K}$) to cozy incandescent tungsten warm-white ($\sim 2700\text{K}$, `CRGB(255, 145, 35)`). Full clockwise ($360^\circ$) restores the original warm white.
* **Anti-Flicker Hardware Filtering**:
  4-sample oversampling with deadband and Exponential Moving Average (EMA) filtering eliminates ADC noise and prevents micro-flicker while breathing.
* **Animation Dynamics**:
  * Employs a low-frequency sinusoidal breathing wave ($\sin$) across the selected color.
  * Brightness gently sways between **40%** (minimum factor 0.40) and **85%** (maximum factor 0.85).
  * A full breath cycle takes **4.5 seconds** (`PULSE_PERIOD = 4500 ms`), providing a calming, organic rhythm without harsh flicker.
* **Update Cadence**: 40 FPS (25 ms interval).

---

### Mode 2: `ConcertSmoothRGBBreath`
* **Mood & Visual Character**: Ultra-slow, fluid, and serene ambient illumination designed for music listening, lounge environments, and meditative spaces.
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
* Classic multi-meteor perimeter chase with fading comet tails (`fadeToBlackBy`) and random glitter.

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
| **Color Potentiometer** | `Analog Pin A3` | 10k Potentiometer wiper for color selection in `NormalPulsingWarmWhite` |
| **Floating Seed** | `Analog Pin A0` | Unconnected pin used for `randomSeed(analogRead(A0))` |

### Power Supply & Injection Guidelines
* 350 SK6812 LEDs at full white can draw up to $\approx 10.5\text{ A}$ at 5V DC.
* **Firmware Brightness Ceiling**: The controller configures a steady brightness ceiling (`FastLED.setBrightness(180)`) and disables temporal dithering (`FastLED.setDither(0)`) to eliminate any high-frequency flicker and power-supply hunting.
* **Power Injection**: For best color uniformity and to prevent voltage drop over the 5.82-meter run, inject 5V DC power at both ends of the strip (or at diagonal corners) from an external 5V regulated power supply (rated for at least 4A–6A). Connect Arduino GND to power supply GND.

---

## 5. Software Architecture

* **Framework**: FastLED (`#include <FastLED.h>`)
* **LED Driver**: Native `SK6812` clockless controller with `GRB` color order.
* **State Machine Pattern**:
  * `LightState`: Abstract base class declaring lifecycle hooks `enter(ctx)` and `update(ctx, currentMillis)`.
  * `LightContext`: Manages current state pointer and handles serial diagnostic transitions.
  * Dedicated concrete classes: `NormalPulsingWarmWhiteState`, `ConcertSmoothRGBBreathState`, and `PartyMeteorSparkleState`.
* **Zero Delay Guarantee**: Every routine is driven by monotonic `millis()` elapsed timers, ensuring smooth frame rates and responsive switch detection at all times.

