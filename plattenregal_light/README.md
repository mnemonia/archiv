# Plattenregal Light Controller (SK6812 LED Strip)

An Arduino-based lighting controller for vinyl record shelf (*Plattenregal*) illumination. It drives a strip of **15 SK6812 RGB LEDs** using the **FastLED** library, featuring a non-blocking object-oriented state machine, master luminosity control, multi-zone color selection, and physical toggle switch mode control.

---

## 1. Overview & System Features

* **Target Hardware**: Arduino Uno, Nano, Pro Mini, or compatible ATmega328P microcontrollers.
* **Lighting Hardware**: 15 individually addressable SK6812 RGB LEDs (GRB color order).
* **Master Luminosity Controller**: Dedicated rotary potentiometer on **Pin A2** governing brightness ($0\%\text{--}100\%$) across **all states**.
* **Base Color Controller**: Dedicated rotary potentiometer on **Pin A3** active in **Normal Mode**, supporting a 3-zone $360^\circ$ dial mapping (RGB rainbow, luxury accents, and tunable white CCT).
* **3-State Toggle Switch**: ON-OFF-ON switch for switching between **Normal**, **Concert**, and **Party** modes.
* **Flicker-Free Performance**: FastLED temporal dithering disabled (`FastLED.setDither(0)`) combined with 4-sample ADC oversampling, a 2-count deadband, and Exponential Moving Average (EMA) filtering on both analog inputs.
* **Non-Blocking Architecture**: Fully driven by monotonic `millis()` scheduling with zero `delay()` calls.

---

## 2. Lighting Behaviors & Modes

The controller implements three distinct lighting modes using an object-oriented state pattern (`LightState`):

```
       3-State Switch (ON-OFF-ON)
      ┌───────────┬───────────┐
   Left (Pin 11) Center    Right (Pin 12)
      │           │           │
      ▼           ▼           ▼
  [NORMAL]    [CONCERT]    [PARTY]
  Breathing   Smooth       Random
  in Pot      Rainbow      Color
  Color       Flow         Beats
```

---

### Mode 1: `NormalState` (Left Switch Position)
* **Visual Character**: Gentle, organic breathing pulsation across all 15 LEDs, displaying the color chosen with the Color Potentiometer.
* **Color Potentiometer Selection (Pin A3)**:
  Active **only** in this mode. Turning the potentiometer sweeps continuously across three distinct color zones:

  ```text
  0° (ADC 0)              180° (ADC 512)           270° (ADC 768)          360° (ADC 1023)
  ├────────────────────────────┼────────────────────────────┼────────────────────────────┤
  │       Zone 1 (0°–180°)     │     Zone 2 (180°–270°)     │     Zone 3 (270°–360°)     │
  │   Full Chromatic Rainbow   │   Curated Accent Tones     │    Tunable White (CCT)     │
  │   Red → Green → Blue → Red │ Nice-Red → Blue → Purple   │ Cold-White → Warm-White    │
  └────────────────────────────┴────────────────────────────┴────────────────────────────┘
  ```

  1. **Zone 1: $0^\circ \text{ to } 180^\circ$ (ADC $0\text{--}511$) — Full Chromatic Rainbow**:
     Full 16-bit hue interpolation over FastLED's `hsv2rgb_rainbow` with sub-step blending. Provides an analog-smooth sweep through Red $\to$ Orange $\to$ Yellow $\to$ Green $\to$ Cyan $\to$ Blue $\to$ Magenta $\to$ Red without color stepping.
  2. **Zone 2: $180^\circ \text{ to } 270^\circ$ (ADC $512\text{--}767$) — Luxury Accent Tones**:
     * $180^\circ \text{ to } 225^\circ$: Seamless blend from **`nice-red`** `CRGB(255, 20, 30)` to **`nice-blue`** `CRGB(10, 95, 255)`.
     * $225^\circ \text{ to } 270^\circ$: Seamless blend from **`nice-blue`** `CRGB(10, 95, 255)` to **`nice-purple`** `CRGB(175, 25, 240)`.
  3. **Zone 3: $270^\circ \text{ to } 360^\circ$ (ADC $768\text{--}1023$) — Tunable White (CCT)**:
     Correlated Color Temperature transition from **`cold-white`** `CRGB(225, 240, 255)` ($\sim 6500\,\text{K}$ crisp daylight) to **`warm-white`** `CRGB(255, 145, 35)` ($\sim 2700\,\text{K}$ cozy incandescent). Full clockwise ($360^\circ$) restores classic warm white.

* **Breathing Dynamics**:
  * Driven by a sinusoidal wave ($\sin$) over a **4.5-second cycle** (`PULSE_PERIOD = 4500 ms`).
  * Brightness oscillates subtly between **45%** (`MIN_FACTOR = 0.45`) and **85%** (`MAX_FACTOR = 0.85`), preventing harsh shutoffs and power supply pumping.
* **Cadence**: 40 FPS (`UPDATE_INTERVAL = 25 ms`).

---

### Mode 2: `ConcertState` (Center Switch Position)
* **Visual Character**: Continuous, tranquil rainbow flow across the shelf, creating an ambient backdrop for music listening.
* **Dynamics**:
  * Full $360^\circ$ rainbow color rotation cycles every $\approx 7$ seconds.
  * Spatial hue gradient flows across the 15 LEDs simultaneously.
  * **Color Potentiometer**: Ignored in this mode.
  * **Brightness Potentiometer**: Actively controls master luminosity.
* **Cadence**: 50 FPS (`UPDATE_INTERVAL = 20 ms`).

---

### Mode 3: `PartyState` (Right Switch Position)
* **Visual Character**: High-energy party mode where every LED displays an individual, vivid random color.
* **Dynamics**:
  * On mode enter, all 15 LEDs are instantly populated with independent random saturated colors (`CHSV(random8(), 255, 255)`).
  * Every 90 ms (`UPDATE_INTERVAL = 90 ms`), a burst of random LEDs is updated with fresh random colors.
  * **Color Potentiometer**: Ignored in this mode.
  * **Brightness Potentiometer**: Actively controls master luminosity.
* **Cadence**: ~11 updates/sec (90 ms beat cycle).

---

## 3. Hardware Wiring & Pin Mapping

### Pin Connection Summary

| Arduino Pin | Peripheral | Connection Details | Purpose |
| :--- | :--- | :--- | :--- |
| **`Digital Pin 6`** | **SK6812 LED Strip** | Strip Data IN (via $330\text{--}470\,\Omega$ series resistor) | FastLED SK6812 signal output |
| **`Digital Pin 11`** | **Toggle Switch** | Left terminal (`INPUT_PULLUP`) | Detects `NormalState` (active LOW) |
| **`Digital Pin 12`** | **Toggle Switch** | Right terminal (`INPUT_PULLUP`) | Detects `PartyState` (active LOW) |
| **`Analog Pin A2`** | **Brightness Pot** | Potentiometer 1 Wiper (Center Pin) | Master luminosity ($0\%\text{--}100\%$) for **all states** |
| **`Analog Pin A3`** | **Color Pot** | Potentiometer 2 Wiper (Center Pin) | Base color selection in `NormalState` |
| **`Analog Pin A0`** | **Unconnected** | Left floating | Hardware entropy source for `randomSeed()` |
| **`5V`** | **Power Rail** | Strip VDD, Potentiometer Outer Pin 1 | 5V DC supply |
| **`GND`** | **Ground Rail** | Strip GND, Switch Common, Potentiometer Outer Pin 2 | Common system ground |

---

### Wiring Schematic

```text
                     +-----------------------+
                     |      Arduino Uno/     |
                     |      Nano (ATmega)    |
                     |                       |
                     |                   D6  |-----[ 330-470 Ohm ]-----> SK6812 Data IN
                     |                       |
       +5V <---------| 5V                D11 |--------------------+
       GND <---------| GND               D12 |-----------+        |
                     |                       |           |        |
                     |                   A2  |<---+      |        |
                     |                   A3  |<---+-+    |        |
                     +-----------------------+    | |    |        |
                                                  | |    |        |
     Master Brightness Pot (10k)                  | |    |        |
     +5V ---[ O ]--- Wiper -----------------------+ |    |        |
     GND ---[ O ]                                   |    |        |
                                                    |    |        |
     Color Selection Pot (10k)                      |    |        |
     +5V ---[ O ]--- Wiper -------------------------+    |        |
     GND ---[ O ]                                        |        |
                                                         |        |
     3-State Toggle Switch (ON-OFF-ON)                   |        |
     Left Terminal (Normal)   ---------------------------+        |
     Right Terminal (Party)   ------------------------------------+
     Center Terminal (Common) ---> GND
```

---

### 3-State Toggle Switch Truth Table

The switch is configured with Arduino internal pull-ups (`INPUT_PULLUP`):

| Switch Lever Position | Pin 11 Read | Pin 12 Read | Resolved State | Description |
| :--- | :---: | :---: | :--- | :--- |
| **Left** | `LOW` | `HIGH` | **`STATE_NORMAL`** | Pulsing in potentiometer-selected color |
| **Center** | `HIGH` | `HIGH` | **`STATE_CONCERT`** | Smooth traveling rainbow flow |
| **Right** | `HIGH` | `LOW` | **`STATE_PARTY`** | Random vibrant party colors |

*Debounced with a non-blocking 50 ms filter (`DEBOUNCE_DELAY = 50 ms`) to prevent false triggers during mechanical switching.*

---

## 4. Anti-Flicker & Stability Engineering

1. **Disabled Temporal Dithering**:
   FastLED's temporal dithering is explicitly turned off:
   ```cpp
   FastLED.setDither(0);
   ```
   This prevents high-frequency ($25\text{--}50\,\text{Hz}$) PWM shimmering that can occur when driving SK6812 LEDs at low-to-medium brightness levels.
2. **Analog ADC Stabilization**:
   Both `BrightnessController` and `ColorController` implement identical 3-stage hardware filtering:
   * **4-Sample Oversampling**: Multiple consecutive ADC reads are averaged to eliminate high-frequency electrical line noise.
   * **2-Count Deadband Filter**: Minor voltage jitter within $\pm 2$ ADC steps ($< 10\,\text{mV}$) is ignored, preventing stationary knobs from shivering.
   * **Exponential Moving Average (EMA)**: An exponential filter (`(filtered * 3 + raw) / 4`) provides analog response speed while keeping the output rock-steady.
3. **Decoupled State Updates**:
   Adjusting the master brightness potentiometer provides instant visual response (`FastLED.show()` is dispatched immediately when brightness changes) without interfering with the frame rate of the running lighting mode.

---

## 5. Software Architecture

The firmware is designed with modular, encapsulated C++ classes:

```
plattenregal_light.ino
├── Hardware Controllers
│   ├── BrightnessController (Pin A2)
│   │   ├── 4-sample ADC filter + deadband + EMA
│   │   └── FastLED.setBrightness() mapping (0..255)
│   │
│   └── ColorController (Pin A3)
│       ├── 4-sample ADC filter + deadband + EMA
│       └── calculateBaseColor() (3 dial zones, 0..1023)
│
├── State Machine Framework
│   ├── LightState (Abstract Base Class)
│   └── LightContext (State Transition & Update Manager)
│
├── Concrete Lighting States
│   ├── NormalState (Breathing sine wave + ColorController injection)
│   ├── ConcertState (Traveling rainbow hue accumulator)
│   └── PartyState (Random vibrant color generator)
│
└── System Loop & Switch Handler
    ├── readSwitchState() (50 ms debounce)
    ├── setup() (FastLED initialization & controller boot)
    └── loop() (Non-blocking controller & state updates)
```

---

## 6. Power & Electrical Guidelines

* **Current Consumption**:
  15 SK6812 RGB LEDs draw up to $\approx 60\,\text{mA}$ each at $100\%$ white ($15 \times 60\,\text{mA} = 900\,\text{mA}$ peak). During typical warm-white breathing and concert modes, current draw is under $400\text{--}500\,\text{mA}$.
* **Power Source**:
  A standard 5V 1A or 5V 2A USB power adapter is sufficient to power both the Arduino and the 15-LED strip.
* **Capacitor & Resistor Recommendations**:
  * Place a **$100\,\mu\text{F}\text{--}1000\,\mu\text{F}$ capacitor** across the 5V and GND rails near the LED strip input to absorb initial power surges.
  * Place a **$330\,\Omega\text{--}470\,\Omega$ resistor** inline on the data line between Arduino Pin 6 and the strip Data IN pin to prevent signal ringing and protect the first pixel.

