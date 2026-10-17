# Rectangle Light Controller (SK6812 LED Strip)

An Arduino firmware controller driving a **300-LED SK6812 RGB** strip mounted along a **2.0 m × 0.5 m rectangular frame** using the **FastLED** library. Designed with a non-blocking state machine architecture and mode selection via a 3-step switch knob.

---

## 1. Physical Geometry & LED Layout

The LED strip runs along the perimeter of a 2.0 m × 0.5 m rectangular frame. At a density of 60 LEDs per meter, the total strip length is 5.0 meters with 300 addressable LEDs:

```text
                  Edge 2: Top (2.0 m / 120 LEDs, Indices 150..269)
       ┌─────────────────────────────────────────────────────────────┐
       │ < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < │
Edge 3 │                                                             │ Edge 1
Left   │ v                                                         ^ │ Right
(0.5m) │ v                                                         ^ │ (0.5m)
30 LEDs│                                                             │ 30 LEDs
(270.. │                                                             │ (120..
  299) │ > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > │  149)
       └─────────────────────────────────────────────────────────────┘
  (Data IN)       Edge 0: Bottom (2.0 m / 120 LEDs, Indices 0..119)
```

| Edge | Axis | Length | LED Count | Pixel Indices | Direction |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Edge 0** | Bottom ($X$) | 2.0 m | 120 LEDs | `0 .. 119` | Left $\rightarrow$ Right |
| **Edge 1** | Right ($Y$) | 0.5 m | 30 LEDs | `120 .. 149` | Bottom $\rightarrow$ Top |
| **Edge 2** | Top ($X$) | 2.0 m | 120 LEDs | `150 .. 269` | Right $\rightarrow$ Left |
| **Edge 3** | Left ($Y$) | 0.5 m | 30 LEDs | `270 .. 299` | Top $\rightarrow$ Bottom |
| **Total** | **Perimeter** | **5.0 m** | **300 LEDs** | `0 .. 299` | Closed loop |

---

## 2. Mode Explanations

The controller implements three distinct lighting modes using an object-oriented state pattern. All animations run non-blockingly using `millis()` intervals.

### Mode 1: `NormalPulsingWarmWhite` *(Default / Boot Mode)*
* **Mood & Visual Character**: Cozy, warm architectural mood light suitable for everyday relaxation or reading.
* **Color Temperature**: Incandescent warm-white spectrum ($\sim 2700\text{K}$, $R=255, G=145, B=35$) that mimics the soft glow of classic tungsten filament lamps.
* **Animation Dynamics**:
  * Employs a low-frequency sinusoidal breathing wave ($\sin$) across the entire frame.
  * Brightness gently sways between **40%** (minimum factor 0.40) and **85%** (maximum factor 0.85).
  * A full breath cycle takes **4.5 seconds** (`PULSE_PERIOD = 4500 ms`), providing a calming, organic rhythm without harsh flicker.
* **Update Cadence**: 40 FPS (25 ms interval).

---

### Mode 2: `ConcertSmoothRGBBreath`
* **Mood & Visual Character**: Fluid, immersive ambient illumination designed for music playback and lounge settings.
* **Animation Dynamics**:
  * **Circulating Chromatic Gradient**: Maps the complete $360^\circ$ RGB color wheel around the 5.0-meter perimeter. All colors flow continuously along the edges of the rectangle (`pixelHue = baseHue + i * 256 / NUM_LEDS`).
  * **Chromatic Drift**: The root hue rotates smoothly across time over a $\sim 11$-second rotation cycle.
  * **Synchronized Breathing Envelope**: A sine-wave breath curve (`BREATH_PERIOD = 4000 ms`) swells and dims the entire strip's intensity between **35% and 85%** in tandem with the color roll.
* **Update Cadence**: 50 FPS (20 ms interval).

---

### Mode 3: `PartyMeteorSparkle`
* **Mood & Visual Character**: High-energy, dynamic celebratory mode combining fast-moving comets and random glitter.
* **Animation Dynamics**:
  * **Triple Meteor Chase**: Three equidistant meteors (spaced 100 LEDs apart) race around the 300-LED perimeter.
  * **Comet Trails**: On each frame, previous pixel values decay exponentially using FastLED's `fadeToBlackBy(leds, NUM_LEDS, 50)`, creating smooth, trailing tails behind each meteor head.
  * **Chromatic Meteor Heads**: Meteor heads cycle hues as they travel, capped by leading white-hot tips.
  * **Random Sparkles / Glitter**: Every frame generates random starlight sparkles (alternating between intense white flashes and saturated neon points) that pop onto random LEDs and naturally decay into the background.
* **Update Cadence**: $\sim 55$ FPS (18 ms interval).

---

## 3. Mode Selection (3-Step Switch Knob)

The controller supports two selectable hardware configurations defined by `#define ACTIVE_SELECTOR` in the source code:

### Configuration A: Digital 3-Step Switch (`SELECTOR_DIGITAL_SWITCH`, Default)
Uses an **ON-OFF-ON 3-position toggle switch** or a **3-step rotary switch** wired to two digital pins with internal pull-up resistors:
* **Pin 11 (`PIN_SWITCH_STEP1`)**: Left terminal (Step 1)
* **Pin 12 (`PIN_SWITCH_STEP3`)**: Right terminal (Step 3)
* **GND**: Center / Common terminal

| Switch Position | Pin 11 | Pin 12 | Selected Mode | Notes |
| :--- | :---: | :---: | :--- | :--- |
| **Step 1 (Left)** | `LOW` | `HIGH` | **`NormalPulsingWarmWhite`** | Boot default |
| **Step 2 (Center)** | `HIGH` | `HIGH` | **`ConcertSmoothRGBBreath`** | Open / floating position |
| **Step 3 (Right)** | `HIGH` | `LOW` | **`PartyMeteorSparkle`** | Switched to GND |

*Includes a 50 ms debounce filter to prevent state flapping during mechanical switching.*

---

### Configuration B: Analog Potentiometer Knob (`SELECTOR_ANALOG_KNOB`)
Set `#define ACTIVE_SELECTOR SELECTOR_ANALOG_KNOB` to use a 10k rotary potentiometer knob:
* **Pin A3 (`PIN_ANALOG_KNOB`)**: Potentiometer wiper
* **5V & GND**: Outer potentiometer terminals

The continuous $0\text{--}1023$ analog reading is divided into 3 steps with a **35-count hysteresis window** to eliminate border jitter:
* **0 .. 341 counts** $\rightarrow$ `NormalPulsingWarmWhite`
* **342 .. 682 counts** $\rightarrow$ `ConcertSmoothRGBBreath`
* **683 .. 1023 counts** $\rightarrow$ `PartyMeteorSparkle`

---

## 4. Hardware Wiring & Safety

### Pin Assignments
| Function | Arduino Pin | Description |
| :--- | :--- | :--- |
| **LED Data (DIN)** | `Digital Pin 6` | Output to SK6812 DIN (via a 330–470 $\Omega$ series resistor) |
| **Switch Step 1** | `Digital Pin 11` | Left switch terminal (`INPUT_PULLUP`) |
| **Switch Step 3** | `Digital Pin 12` | Right switch terminal (`INPUT_PULLUP`) |
| **Analog Knob** | `Analog Pin A3` | Optional potentiometer wiper input |
| **Floating Seed** | `Analog Pin A0` | Unconnected pin used for `randomSeed(analogRead(A0))` |

### Power Supply & Injection Guidelines
* 300 SK6812 LEDs at full white can draw up to $\approx 9\text{ A}$ at 5V DC.
* **Firmware Safety Limit**: The controller enables FastLED's internal power manager configured for safe operation:
  ```cpp
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2500); // Clamps strip power to 5V / 2.5A
  ```
* **Power Injection**: For best color uniformity and to prevent voltage drop over the 5-meter run, inject 5V DC power at both ends of the strip (or at diagonal corners) from an external 5V regulated power supply (rated for at least 3A–5A). Connect Arduino GND to power supply GND.

---

## 5. Software Architecture

* **Framework**: FastLED (`#include <FastLED.h>`)
* **LED Driver**: Native `SK6812` clockless controller with `GRB` color order.
* **State Machine Pattern**:
  * `LightState`: Abstract base class declaring lifecycle hooks `enter(ctx)` and `update(ctx, currentMillis)`.
  * `LightContext`: Manages current state pointer and handles serial diagnostic transitions.
  * Dedicated concrete classes: `NormalPulsingWarmWhiteState`, `ConcertSmoothRGBBreathState`, and `PartyMeteorSparkleState`.
* **Zero Delay Guarantee**: Every routine is driven by monotonic `millis()` elapsed timers, ensuring smooth frame rates and responsive switch detection at all times.

