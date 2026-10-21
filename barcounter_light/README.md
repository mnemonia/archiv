# BarCounter Light & Interactive Game System

An Arduino-driven ambient lighting and competitive 2-player arcade gaming system for bar counter tops. Designed for **60 NeoPixel RGB LEDs / 1 meter** (configurable), powered by an **ATmega328P** (Arduino Uno / Nano) with dual-backend support (physical NeoPixel strip or serial stream to a virtual visualizer).

---

## 1. System Overview & Core Capabilities

* **Ambient Illumination**: 6 ultra-smooth, slow, non-hectic ambient lighting modes engineered specifically for bar and lounge atmospheres.
* **Instant Competitive Gaming**: 6 fast, reflex-driven 2-player 1-button arcade games—one corresponding to each ambient mode—activated instantly by holding both player buttons.
* **Global Rotary Color Knob (Pin A3)**:
  * Full 360° rainbow spectrum ($0\text{--}65535$ hue) mapped across $0\text{--}1010$ ADC counts.
  * **White Zone** added at the end of knob travel ($1012\text{--}1023$ counts).
  * **Constant Perceptual Brightness**: Employs RMS perceptual luminance normalization ($P = \sqrt{0.299R^2 + 0.587G^2 + 0.114B^2} \approx 85.8$) across all hues and white. Turning the color knob never alters overall strip brightness!
* **Global Rotary Brightness Knob (Pin A2)**:
  * Master brightness knob governing global strip luminosity ($0\%\text{--}100\%$).
  * Lossless software scaling in `.show()` prevents color ratio distortion and integer overflow.
* **Hardware Cross-Talk Lockout**:
  * Switching ADC channels discards initial conversion with settling delays.
  * Adjusting the brightness knob dynamically widens the color knob's deadband (from 8 to 24 counts for 400 ms) to suppress power rail sag and eliminate crosstalk between adjacent analog pins.
* **Minimum Illumination Threshold**:
  * Automatic luminance enforcement (`MIN_LUMEN_THRESHOLD = 88`, $\approx 34.5\%$ perceptual luminance) prevents animations from falling into total darkness.
* **Dual-Backend Support**:
  * Compile-time switch between physical WS2812B / SK6812 strip (`BACKEND_ADAFRUIT_REAL`) and high-speed binary serial streaming (`BACKEND_VIRTUAL_SERIAL`) to a Linux visualizer.

---

## 2. Hardware Wiring & Pin Mapping

### Pin Connection Summary

| Arduino Pin | Peripheral | Connection Details | Purpose |
| :--- | :--- | :--- | :--- |
| **`Digital Pin 6`** | **NeoPixel Strip (DIN)** | Data IN via $330\text{--}470\,\Omega$ series resistor | High-speed 800 kHz NeoPixel signal output |
| **`Digital Pin 2`** | **Player 1 Button (P1)** | Momentary push button to GND (`INPUT_PULLUP`) | P1 action input (Left side / Blue) |
| **`Digital Pin 4`** | **Player 2 Button (P2)** | Momentary push button to GND (`INPUT_PULLUP`) | P2 action input (Right side / Red) |
| **`Digital Pin 7`** | **Mode Button** | Momentary push button to GND (`INPUT_PULLUP`) | Cycles through the 6 ambient modes |
| **`Analog Pin A2`** | **Brightness Pot** | 10k potentiometer wiper (center pin) | Master luminosity ($0\%\text{--}100\%$) |
| **`Analog Pin A3`** | **Color Pot** | 10k potentiometer wiper (center pin) | Mood / base-color selection & white |
| **`Analog Pin A0`** | **Unconnected** | Left floating | Hardware entropy source for `randomSeed()` |
| **`5V`** | **Power Rail** | Strip VDD, Potentiometer Outer Pin 1 | 5V DC regulated power |
| **`GND`** | **Ground Rail** | Strip GND, Button terminals, Potentiometer Outer Pin 2 | Common system ground |

---

### Wiring Schematic

```text
                     +-----------------------+
                     |      Arduino Uno/     |
                     |      Nano (ATmega)    |
                     |                       |
                     |                   D6  |-----[ 330-470 Ohm ]-----> NeoPixel DIN
                     |                       |
       +5V <---------| 5V                 D2 |-----------+  (Player 1 Button)
       GND <---------| GND                D4 |-------+   |  (Player 2 Button)
                     |                    D7 |---+   |   |  (Mode Toggle Button)
                     |                       |   |   |   |
                     |                   A2  |<--+---+---+---+ (Brightness Pot Wiper)
                     |                   A3  |<--+---+---+---+-+ (Color Pot Wiper)
                     +-----------------------+   |   |   |   | |
                                                 |   |   |   | |
     Mode Button (Pin 7)                         |   |   |   | |
     [ Push Button ] ----------------------------+   |   |   | |
     Other Terminal ---> GND                         |   |   | |
                                                     |   |   | |
     Player 2 Button (Pin 4)                         |   |   | |
     [ Push Button ] --------------------------------+   |   | |
     Other Terminal ---> GND                             |   | |
                                                         |   | |
     Player 1 Button (Pin 2)                             |   | |
     [ Push Button ] ------------------------------------+   | |
     Other Terminal ---> GND                                 | |
                                                             | |
     Master Brightness Pot (10k Linear)                      | |
     +5V ---[ O ]--- Wiper ----------------------------------+ |
     GND ---[ O ]                                              |
                                                               |
     Color / Mood Selection Pot (10k Linear)                   |
     +5V ---[ O ]--- Wiper ------------------------------------+
     GND ---[ O ]
```

---

## 3. Physical Controls & Interaction

### Mode Toggle Button (Pin 7)
* **Single Tap**: Steps sequentially through all 6 ambient modes:
  `Breathing/Pulse` $\to$ `Twinkle/Sparkle` $\to$ `Fire/Flame` $\to$ `Comet/Meteor` $\to$ `Scanner/Cylon` $\to$ `Color Wipe` $\to$ *(repeats)*.
* Exits any active game and returns to ambient lighting.

### Entering & Exiting 2-Player Game Mode
* **Dual Button Hold**: **Press and hold both Player 1 (Pin 2) and Player 2 (Pin 4) buttons simultaneously for 800 ms**.
* Instantly launches the dedicated competitive game corresponding to the currently selected mode.
* Holding both buttons again for 800 ms returns to peaceful ambient lighting.

### Rotary Color Potentiometer (Pin A3)
* **0 to 1010 counts ($0^\circ\text{--}350^\circ$) — 360° Rainbow**:
  Full 16-bit hue range ($0\text{--}65535$). Sweeps through Red, Yellow, Green, Cyan, Blue, Magenta, and back to Red.
* **1012 to 1023 counts ($350^\circ\text{--}360^\circ$) — Pure White Zone**:
  Hysteresis-protected pure neutral white (`ENTER > 1015`, `EXIT < 1005`).
* **Universal Mood Anchor**: Dynamically defines the base color tint for all ambient modes!

### Rotary Brightness Potentiometer (Pin A2)
* Governs overall strip output from total black ($0$) to full brilliance ($255$).
* Scaled losslessly across the RGB color components at output time, preserving subtle hue balances.

---

## 4. The 6 Ambient Visualization Modes

Every ambient mode is intentionally tuned for bar atmospheres: ultra-slow, meditative, and organic without harsh strobing or hectic jumping.

| Mode # | Mode Name | Period / FPS | Ambient Behavior | Mood-Color Influence |
| :---: | :--- | :---: | :--- | :--- |
| **1** | **Breathing / Pulse** | 7.5 s cycle / 40 FPS | Meditative full-strip sinusoidal breath between 42% and 82% luminance | Pulses in the exact rotary base color |
| **2** | **Twinkle / Sparkle** | 50 ms step / 20 FPS | Drifting candlelight and starry shimmer with subtle white glints on peaks | All sparkles tinted by the rotary base color |
| **3** | **Fire / Flame** | 60 ms step / 16 FPS | Cozy hearth fire with convective cooling, thermal heat transfer, and embers | Heat spectrum rendered in rotary flame tint |
| **4** | **Comet / Meteor** | 45 ms step / 22 FPS | Gentle shooting star gliding along a low-luminance baseline floor | Comet head and fading tail follow rotary color |
| **5** | **Scanner / Cylon** | 6.0 s sweep / 28 FPS | Fluid Larson scanner with soft Gaussian glow falloff | Scanning eye and halo rendered in rotary color |
| **6** | **Color Wipe** | 180 ms/LED / 5.5 FPS | Progressive chromatic roll cycling harmonic color offsets | Starts from rotary color (or 4 white CCT tints) |

---

## 5. The 6 Competitive 2-Player Games

Each ambient mode has its own custom-tailored 2-player reflex duel. Player 1 (Left / Blue) battles Player 2 (Right / Red).

```text
Player 1 (Left)                                        Player 2 (Right)
  [ Pin 2 ]                                               [ Pin 4 ]
    Blue    =================== BATTLEFIELD ===================    Red
    Goal                                                          Goal
```

---

### Game 1: "Resonance Pulse" (Rhythm Wave Tug-of-War)
* **Parent Mode**: Breathing / Pulse
* **Visuals**: A glowing energy nexus floats between Player 1's Blue territory and Player 2's Red territory, surrounded by an ambient breathing field.
* **Game Mechanics**:
  * The bar's ambient breath wave continues to oscillate smoothly.
  * **The Resonance Window**: When the breath wave crests near its peak amplitude ($\text{wave} \ge 88\%$), the system enters **Resonance** and the nexus turns dazzling white-hot.
  * **Rhythm Precision**: Press your button *only* while the nexus is in Resonance to push the nexus toward your opponent's goal!
  * **Cooldown Penalty**: Pressing outside of Resonance misfires your pulse, locking your button in a **400 ms penalty cooldown**.
* **Victory Condition**: Pushing the nexus into the opponent's outer boundary wins the round. After 2 seconds, the board automatically resets.

---

### Game 2: "Sparkle Rush" (Nova Sparkle Reflex Deflector)
* **Parent Mode**: Twinkle / Sparkle
* **Visuals**: A high-velocity cosmic Nova travels across a twinkling starfield between Player 1's Blue defense shield and Player 2's Magenta defense shield.
* **Game Mechanics**:
  * Each player has a defense shield covering $\approx 15\%$ of the strip at their end.
  * As the Nova enters your defense zone, press your button to deflect it back towards your opponent.
  * **Edge Acceleration Bonus**: Deflecting the Nova at the very edge of your shield imparts an intense speed multiplier ($+112\%$ speed + up to $1.5\times$ depth bonus), sending it back at blinding speeds!
  * Early button presses while the Nova is outside your zone are ignored.
* **Victory Condition**: If the Nova breaches your shield and leaves the strip, the opponent scores the win.

---

### Game 3: "Flame Tug" (Bellows Forge Combustion Clash)
* **Parent Mode**: Fire / Flame
* **Visuals**: Two opposing forge fires clash in the center. Player 1 powers a Blue Plasma Forge; Player 2 powers a Red Inferno Forge. The collision point glows white-hot.
* **Game Mechanics**:
  * A furious, rapid-fire button-mashing tug-of-war!
  * Each button press pumps your forge's bellows, injecting thermal energy into your side and shifting the white-hot combustion boundary toward your opponent.
  * Built-in 120 ms anti-spam filtering rewards rapid, rhythmic, deliberate taps over simple jitter-holding.
* **Victory Condition**: Completely overwhelming the opponent's forge by pushing the molten clash point into their goal claims the victory.

---

### Game 4: "Meteor Deflector" (High-Speed Comet Rally)
* **Parent Mode**: Comet / Meteor
* **Visuals**: A flaming meteor with an incandescent core and trail hurtles back and forth between Player 1's Cyan energy barrier and Player 2's Orange energy barrier.
* **Game Mechanics**:
  * An orbital pong rally. Press your button while the meteor traverses your defense sector ($\approx 15\%$ of strip length) to deflect it.
  * **Kinetic Amplification**: Deflecting deep within your zone accelerates the meteor by $+114\%$ plus an edge-distance velocity bonus, up to terminal velocity.
* **Victory Condition**: Failing to deflect the meteor lets it crash into your terminal station, giving the win to the opposing player.

---

### Game 5: "Cylon Clash" (Hyper-Pong Beam Duel)
* **Parent Mode**: Scanner / Cylon
* **Visuals**: A high-energy plasma puck rebounds between Player 1's Blue laser barrier and Player 2's Amber laser barrier over a dim background grid.
* **Game Mechanics**:
  * High-stakes reflex duel based on the classic Larson eye scanner.
  * Time your barrier pulse as the puck reaches your baseline to ricochet it back.
  * Each successful return increases velocity by $+115\%$ plus a precision depth bonus.
  * Tests reaction speed and peripheral vision as the puck reaches maximum velocity.
* **Victory Condition**: Missing the puck results in a goal breach and immediate round loss.

---

### Game 6: "Territory Paint" (Rapid Wipe Wars)
* **Parent Mode**: Color Wipe
* **Visuals**: A dynamic territory-capture battleground. Player 1 commands Vibrant Sky Blue (`RGB 14, 165, 233`); Player 2 commands Neon Rose Red (`RGB 225, 29, 72`), divided by a sharp white boundary line.
* **Game Mechanics**:
  * High-speed territory capture tug-of-war!
  * Rapidly tap your button to paint the bar in your color.
  * Each press advances the paint boundary by 1 LED into enemy territory.
* **Victory Condition**: Completely painting the entire strip with your color claims the bar counter and wins the game.

---

## 6. Serial Interface & Linux Virtual Visualizer

The firmware includes a full bidirectional serial interface operating at **115200 baud**.

### Virtual Serial Commands
Send the following characters over USB Serial to control the board remotely:

| Command | Action |
| :--- | :--- |
| **`1`** | Simulates Player 1 button press |
| **`2`** | Simulates Player 2 button press |
| **`m`** or **`M`** | Toggles to the next ambient mode |
| **`g`** or **`G`** | Toggles 2-player Game Mode on/off |
| **`w`** or **`W`** | Sets rotary color to Pure White |
| **`h<0-65535>`** | Sets hue directly (e.g. `h0` for Red, `h21845` for Green, `h43690` for Blue) |
| **`b<0-255>`** | Sets global brightness directly (e.g. `b180`) |
| **`b`** | Steps brightness up by $+25$ (wraps at 255) |
| **`c`** or **`C`** | Steps hue by $+4000$ ($\approx 22^\circ$ around the color wheel) |

### Virtual Strip Visualizer Protocol (Backend A)
When `#define STRIP_BACKEND BACKEND_VIRTUAL_SERIAL` is enabled, the Arduino streams binary LED frames to a Linux desktop application:
* **Frame Header**: `[0xAA, 0x55, 0x01, NUM_LEDS]` (4 bytes)
* **Payload**: `[R0, G0, B0, R1, G1, B1, ..., Rn-1, Gn-1, Bn-1]` ($3 \times \text{NUM\_LEDS}$ bytes)
* **Checksum**: XOR of header bytes and all payload bytes (1 byte)

---

## 7. Electrical & Safety Guidelines

* **Current Consumption**:
  * 60 SK6812 / WS2812B LEDs at full white draw up to $\approx 3.6\text{ A}$ at 5V DC ($60\,\text{mA}$ per pixel).
  * In typical ambient modes with the constant perceptual brightness wheel ($P \approx 85.8$), current draw stays under **$0.8\text{--}1.2\text{ A}$**.
* **Power Supply**:
  * Use a regulated **5V DC power supply rated for at least 3A–4A** when running a full 1-meter strip (60 LEDs).
  * For test setups with 15 LEDs, standard USB power ($500\text{--}1000\,\text{mA}$) is sufficient.
  * Connect external power supply ground directly to the Arduino GND pin.
* **Protection Components**:
  * Place a **$470\,\mu\text{F}\text{--}1000\,\mu\text{F}$ electrolytic capacitor** across 5V and GND near the strip input to prevent initial power-on voltage spikes.
  * Place a **$330\,\Omega\text{--}470\,\Omega$ resistor** inline between Arduino Digital Pin 6 and the strip Data IN pin.

