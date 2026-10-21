# BarCounter Light & Interactive Game System (12m SK6812 Strip)

An Arduino-driven ambient lighting and competitive 2-player arcade gaming system for bar counter tops. Designed for a **12-meter SK6812 RGB LED strip (60 LEDs/m = 720 addressable LEDs)**, driven by the **FastLED** library, featuring independent ambient lighting controllers, extracted 2-player competitive arcade games, and a quad-potentiometer control panel for real-time behavior, game, color, and brightness tuning.

---

## 1. System Overview & Core Capabilities

* **Lighting Hardware**: 12-meter SK6812 RGB LED strip with 720 individually addressable LEDs (GRB color ordering).
* **LED Driver Framework**: Powered by **FastLED** (`#include <FastLED.h>`) with temporal dithering disabled (`FastLED.setDither(0)`) for flicker-free output.
* **Separation of Light & Game Engines**:
  * 6 standalone ambient lighting mode classes designed for relaxed bar atmospheres.
  * 6 standalone competitive 2-player arcade game classes with scaled physics for 12-meter counter tops.
* **Rotary Global Light Behavior Knob (Pin A4)**:
  * Dedicated analog potentiometer mapped across 6 equal dial zones ($0\text{--}1023$ ADC counts).
  * Selects the active ambient lighting mode directly via potentiometer rotation.
* **Rotary Game Selection Knob (Pin A1)**:
  * Dedicated analog potentiometer mapped across 6 equal dial zones ($0\text{--}1023$ ADC counts).
  * Selects which competitive 2-player game is active in real time with hysteresis filtering.
* **Rotary Master Brightness Knob (Pin A2)**:
  * Governs global strip luminosity ($0\%\text{--}100\%$) across **all** light and game modes.
* **Rotary Mood / Base-Color Knob (Pin A3)**:
  * Full 360° rainbow spectrum ($0\text{--}65535$ hue across $0\text{--}1010$ ADC counts) plus pure White ($1012\text{--}1023$).
  * Constant perceptual brightness normalization ($P = \sqrt{0.299R^2 + 0.587G^2 + 0.114B^2} \approx 85.8$) ensures adjusting color has **zero** effect on overall room luminosity.
* **Physical Action Buttons & Game Activation**:
  * Player 1 Button (Pin 2) & Player 2 Button (Pin 4).
  * **Dual Button Hold (Pins 2 & 4 for 800 ms)**: Toggles Game Mode on/off. When launched, it runs the game dialed on the Game Potentiometer (Pin A1).
  * Turning the Global Light Behavior Knob (Pin A4) while in Game Mode gracefully exits back to ambient lighting.

---

## 2. Hardware Wiring & Pin Mapping

### Pin Connection Summary

| Arduino Pin | Peripheral | Connection Details | Purpose |
| :--- | :--- | :--- | :--- |
| **`Digital Pin 6`** | **SK6812 Strip (DIN)** | Data IN via $330\text{--}470\,\Omega$ series resistor | FastLED SK6812 800 kHz signal output |
| **`Digital Pin 2`** | **Player 1 Button (P1)** | Momentary push button to GND (`INPUT_PULLUP`) | P1 action input (Left side / Blue) |
| **`Digital Pin 4`** | **Player 2 Button (P2)** | Momentary push button to GND (`INPUT_PULLUP`) | P2 action input (Right side / Red) |
| **`Analog Pin A1`** | **Game Selection Pot** | 10k potentiometer wiper (center pin) | Selects active 2-player arcade game |
| **`Analog Pin A2`** | **Brightness Pot** | 10k potentiometer wiper (center pin) | Master luminosity ($0\%\text{--}100\%$) |
| **`Analog Pin A3`** | **Color / Mood Pot** | 10k potentiometer wiper (center pin) | Mood / base-color selection & white |
| **`Analog Pin A4`** | **Light Behavior Pot** | 10k potentiometer wiper (center pin) | Selects active ambient lighting mode |
| **`Analog Pin A0`** | **Unconnected** | Left floating | Hardware entropy source for `randomSeed()` |
| **`5V`** | **Power Supply** | Strip 5V VDD, Potentiometer Outer Pin 1 | 5V DC regulated power |
| **`GND`** | **Common Ground** | Strip GND, Button terminals, Potentiometer Outer Pin 2 | Common system ground |

---

### Wiring Schematic

```text
                     +-----------------------+
                     |        Arduino        |
                     |                       |
                     |                   D6  |-----[ 330-470 Ohm ]-----> SK6812 Data IN
                     |                       |
       +5V <---------| 5V                 D2 |-----------+  (Player 1 Button)
       GND <---------| GND                D4 |-------+   |  (Player 2 Button)
                     |                       |       |   |
                     |                   A1  |<--+---+---+---+ (Game Selection Pot Wiper)
                     |                   A2  |<--+---+---+---+-+ (Brightness Pot Wiper)
                     |                   A3  |<--+---+---+---+-+-+ (Color Pot Wiper)
                     |                   A4  |<--+---+---+---+-+-+-+ (Light Behavior Pot Wiper)
                     +-----------------------+   |   |   |   | | | |
                                                 |   |   |   | | | |
     Player 2 Button (Pin 4)                     |   |   |   | | | |
     [ Push Button ] ----------------------------+   |   |   | | | |
     Other Terminal ---> GND                         |   |   | | | |
                                                     |   |   | | | |
     Player 1 Button (Pin 2)                         |   |   | | | |
     [ Push Button ] --------------------------------+   |   | | | |
     Other Terminal ---> GND                             |   | | | |
                                                         |   | | | |
     Game Selection Pot (10k Linear)                     |   | | | |
     +5V ---[ O ]--- Wiper ------------------------------+   | | | |
     GND ---[ O ]                                            | | | |
                                                             | | | |
     Master Brightness Pot (10k Linear)                      | | | |
     +5V ---[ O ]--- Wiper ----------------------------------+ | | |
     GND ---[ O ]                                              | | |
                                                               | | |
     Color / Mood Selection Pot (10k Linear)                   | | |
     +5V ---[ O ]--- Wiper ------------------------------------+ | |
     GND ---[ O ]                                                | |
                                                                 | |
     Global Light Behavior Pot (10k Linear)                      | |
     +5V ---[ O ]--- Wiper --------------------------------------+ |
     GND ---[ O ]
```

---

## 3. Physical Controls & Interaction

### Rotary Global Light Behavior Potentiometer (Pin A4)
Turning the knob on **Pin A4** directly selects the active ambient lighting mode across 6 equal dial zones:

| Dial Zone | ADC Range | Ambient Mode | Visual Character |
| :---: | :---: | :--- | :--- |
| **Zone 0** | $0 \dots 170$ | **Mode 1: Breathing / Pulse** | 7.5s Meditative sinusoidal breath in mood color |
| **Zone 1** | $171 \dots 341$ | **Mode 2: Twinkle / Sparkle** | Drifting candlelight and starry shimmer with white peak glints |
| **Zone 2** | $342 \dots 511$ | **Mode 3: Fire / Flame** | Cozy hearth fire with convective cooling and thermal diffusion |
| **Zone 3** | $512 \dots 682$ | **Mode 4: Comet / Meteor** | Gentle shooting star gliding along a low-luminance baseline floor |
| **Zone 4** | $683 \dots 852$ | **Mode 5: Scanner / Cylon** | Fluid 6.0s Larson scanner eye with soft Gaussian halo |
| **Zone 5** | $853 \dots 1023$ | **Mode 6: Color Wipe** | Progressive chromatic roll cycling harmonic color offsets |

*Turning this knob while in Game Mode automatically exits Game Mode and transitions to the dialed ambient mode.*

---

### Rotary Game Selection Potentiometer (Pin A1)
Turning the knob on **Pin A1** selects which competitive game is launched across 6 equal dial zones:

| Dial Zone | ADC Range | Selected Game | Genre / Mechanics |
| :---: | :---: | :--- | :--- |
| **Zone 0** | $0 \dots 170$ | **Game 1: Resonance Pulse** | Rhythm Wave Tug-of-War ($\ge 88\%$ resonance timing) |
| **Zone 1** | $171 \dots 341$ | **Game 2: Sparkle Rush** | High-Velocity Nova Deflector & Precision Shield |
| **Zone 2** | $342 \dots 511$ | **Game 3: Flame Tug** | Bellows Forge Combustion Clash (Rapid Button Mashing) |
| **Zone 3** | $512 \dots 682$ | **Game 4: Meteor Deflector** | Orbital Comet Pong Rally with Speed Acceleration |
| **Zone 4** | $683 \dots 852$ | **Game 5: Cylon Clash** | Hyper-Pong Beam Duel (High-Speed Larson Puck) |
| **Zone 5** | $853 \dots 1023$ | **Game 6: Territory Paint** | Rapid Wipe Wars (Paint Territory Capture) |

*Features a deadband filter with ADC sample pre-charging to eliminate jitter between game slots.*

---

### Entering & Exiting 2-Player Game Mode
* **Dual Button Hold**: **Press and hold both Player 1 (Pin 2) and Player 2 (Pin 4) buttons simultaneously for 800 ms**.
  * Instantly launches the game dialed on the **Game Selection Knob (A1)**.
  * Holding both buttons again for 800 ms exits back to ambient lighting.
* **Knob Override**: Turning the **Global Light Behavior Knob (A4)** while playing a game immediately exits Game Mode and restores ambient lighting.

---

### Rotary Color & Mood Potentiometer (Pin A3)
* **$0^\circ \text{ to } 350^\circ$ ($0\text{--}1010$ ADC counts)**: Full $360^\circ$ chromatic rainbow.
* **$350^\circ \text{ to } 360^\circ$ ($1012\text{--}1023$ ADC counts)**: Pure neutral white zone with hysteresis.
* **Universal Mood Anchor**: Dynamically tints the base visual palette for all ambient modes and game background floors without changing luminance.

---

### Rotary Master Brightness Potentiometer (Pin A2)
* Adjusts global strip luminosity from complete darkness ($0$) to full brilliance ($255$) across both ambient modes and games.

---

## 4. The 6 Ambient Lighting Modes

Separated from game logic into modular, dedicated classes:

| Mode # | Mode Class | Period / Interval | Visual Behavior | Mood Color Influence |
| :---: | :--- | :---: | :--- | :--- |
| **1** | `BreathingPulseLight` | 7.5 s sine cycle / 40 FPS | Meditative full-counter breathing wave between 42% and 82% brightness | Pulses in rotary base color |
| **2** | `TwinkleSparkleLight` | 50 ms step / 20 FPS | Drifting candlelight and starry shimmer with brilliant white glints | All sparkles tinted in rotary color |
| **3** | `FireFlameLight` | 60 ms step / 16 FPS | Cozy hearth fire with convective cooling, thermal heat transfer, and embers | Heat ramp rendered in flame tint |
| **4** | `CometMeteorLight` | 40 ms step / 25 FPS | Gentle shooting star gliding across 12 meters with 20% fading tail | Comet and floor follow rotary color |
| **5** | `CylonScannerLight` | 6.0 s sweep / 28 FPS | Smooth Larson scanner eye with soft Gaussian halo across 12 meters | Scanner rendered in rotary color |
| **6** | `ColorWipeLight` | Progressive / ~10 s wipe | Chromatic progressive wipe cycling 4 harmonic color angles (or 4 white CCTs) | Anchored on rotary base color |

---

## 5. The 6 Competitive 2-Player Arcade Games

Separated from lighting modes into independent game engines. Player 1 (Left / Blue, Pin 2) battles Player 2 (Right / Red, Pin 4) across the 12-meter bar counter (720 LEDs):

```text
Player 1 (Left End, Pin 2)                          Player 2 (Right End, Pin 4)
  [ Blue Base ] ================= 12 METERS (720 LEDs) ================= [ Red Base ]
```

---

### Game 1: "Resonance Pulse" (Rhythm Wave Tug-of-War)
* **Dial Zone**: Zone 0 (ADC $0\text{--}170$)
* **Mechanics**:
  * An oscillating energy nexus floats between Player 1's Blue territory and Player 2's Red territory.
  * **The Resonance Window**: When the ambient breath wave reaches peak amplitude ($\ge 88\%$), the nexus turns dazzling white.
  * **Timing**: Press your button *only* during Resonance to push the nexus toward your opponent's goal (48 LEDs per pulse).
  * **Penalty**: Pressing outside Resonance triggers a **400 ms misfire penalty cooldown**.
* **Win Condition**: Pushing the nexus into the opponent's goal wins the round. Automatically resets after 2 seconds.

---

### Game 2: "Sparkle Rush" (Nova Sparkle Reflex Deflector)
* **Dial Zone**: Zone 1 (ADC $171\text{--}341$)
* **Mechanics**:
  * A high-velocity cosmic Nova travels across a starry background between Player 1's Blue defense shield and Player 2's Magenta defense shield (108 LEDs / $\approx 1.8\text{ meters}$ each).
  * Press your button while the Nova is inside your shield to deflect it back.
  * **Edge Bonus**: Deflecting near the outer edge gives up to a $+112\%$ speed boost, accelerating the Nova across the bar.
* **Win Condition**: If the Nova breaches the defense shield and exits the strip, the opposing player scores the win.

---

### Game 3: "Flame Tug" (Bellows Forge Combustion Clash)
* **Dial Zone**: Zone 2 (ADC $342\text{--}511$)
* **Mechanics**:
  * Rapid-fire button-mashing tug-of-war! Player 1 powers a Blue Plasma Forge; Player 2 powers a Red Inferno Forge.
  * Each press pumps your forge's bellows, advancing your combustion front by 24 LEDs ($\approx 40\text{ cm}$).
  * Built-in 120 ms anti-spam filtering rewards rapid, rhythmic, deliberate taps.
* **Win Condition**: Pushing the white-hot molten collision point into the opponent's forge wins the clash.

---

### Game 4: "Meteor Deflector" (High-Speed Comet Rally)
* **Dial Zone**: Zone 3 (ADC $512\text{--}682$)
* **Mechanics**:
  * A flaming meteor hurtles between Player 1's Cyan energy barrier and Player 2's Orange energy barrier.
  * Deflect the incoming meteor while inside your defense sector (108 LEDs).
  * Timing deep in your zone accelerates the meteor by $+114\%$ plus an edge-distance velocity bonus up to terminal velocity.
* **Win Condition**: Failing to deflect the meteor lets it crash into your terminal station, awarding the round to your opponent.

---

### Game 5: "Cylon Clash" (Hyper-Pong Beam Duel)
* **Dial Zone**: Zone 4 (ADC $683\text{--}852$)
* **Mechanics**:
  * Kinetic laser pong. A high-energy plasma puck rebounds between Player 1's Blue barrier and Player 2's Amber barrier.
  * Deflect the puck as it enters your barrier; each return increases speed by $+115\%$.
* **Win Condition**: First player whose barrier is breached loses the round.

---

### Game 6: "Territory Paint" (Rapid Wipe Wars)
* **Dial Zone**: Zone 5 (ADC $853\text{--}1023$)
* **Mechanics**:
  * High-speed territory capture! Player 1 commands Sky Blue (`RGB 14, 165, 233`); Player 2 commands Neon Rose Red (`RGB 225, 29, 72`), divided by a sharp white boundary.
  * Tap your button to advance your color paint stroke across the 12 meters (24 LEDs per tap).
* **Win Condition**: Completely painting the entire strip with your color claims the bar counter and wins the game.

---

## 6. Serial Commands & Remote Control

Connect at **115200 baud** to control the system via USB Serial:

| Command | Action |
| :--- | :--- |
| **`1`** | Simulates Player 1 button press |
| **`2`** | Simulates Player 2 button press |
| **`m`** or **`M`** | Steps to the next ambient lighting mode via serial |
| **`g`** or **`G`** | Toggles 2-player Game Mode on/off |
| **`w`** or **`W`** | Sets mood color to Pure White |
| **`h<0-65535>`** | Sets hue directly (e.g. `h0` for Red, `h21845` for Green, `h43690` for Blue) |
| **`b<0-255>`** | Sets global brightness directly (e.g. `b200`) |
| **`b`** | Steps brightness up by $+25$ (wraps at 255) |
| **`c`** or **`C`** | Steps hue by $+4000$ ($\approx 22^\circ$ around the color wheel) |

---

## 7. Power Supply & 12-Meter Strip Electrical Guidelines

* **Peak Power Consumption**:
  * 720 SK6812 RGB LEDs at 100% full white draw up to $\approx 43\text{ A}$ at 5V DC ($60\,\text{mA}$ per LED).
  * In typical ambient modes and games with normalized brightness, current draw stays around **$6\text{--}12\text{ A}$**.
* **Power Supply Recommendation**:
  * Use a heavy-duty **5V DC regulated power supply (rated for at least 15A–20A)**.
* **Power Injection Across 12 Meters**:
  * Due to copper trace resistance over a 12-meter run, running power from only one end causes severe voltage drop (yellowing/browning of colors at the far end).
  * **Inject 5V and GND at least every 3 to 4 meters** (e.g. at 0m, 4m, 8m, and 12m) using thick 14–16 AWG power bus wires.
  * Connect the power supply ground directly to the Arduino GND pin.
* **Signal Protection**:
  * Place a **$330\,\Omega\text{--}470\,\Omega$ series resistor** inline between Arduino Digital Pin 6 and the strip Data IN pin.
  * Place a **$1000\,\mu\text{F}$ electrolytic capacitor** across 5V and GND at the strip power injection points.
