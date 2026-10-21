# Virtual NeoPixel Strip (12m / 720 LEDs) & Bar Counter Arcade System

High-fidelity Dual-Target FastLED controller for Arduino Uno R3 with Linux-based photorealistic visualizer for development and standalone physical deployment.

---

## 1. Dual-Target Architecture

The system provides two compile-time implementations in [`barcounter_light.ino`](file:///home/nils/archiv_master/barcounter_light/barcounter_light.ino):

```cpp
#define BACKEND_VIRTUAL_SERIAL   0  // Implementation A: Stream pixel data to Linux Visualizer
#define BACKEND_FASTLED_REAL     1  // Implementation B: Drive physical SK6812 strip via FastLED library

// >>> CONFIGURE ACTIVE TARGET HERE <<<
#define STRIP_BACKEND            BACKEND_FASTLED_REAL
```

### Implementation A: Virtual Visualizer (Development & Testing)
- **Use Case**: Developing, testing animations, and tuning games on Linux without physical LEDs connected.
- **Protocol (CMD 0x02)**: When `strip.show()` is called, the Arduino transmits an extended 2172-byte binary frame (`0xAA 0x55 0x02 [COUNT_HI] [COUNT_LO] [GamerA_RGB] [GamerB_RGB] [720 * 3 bytes RGB] [XOR_CHECKSUM]`) over USB Serial at **115200 baud**.
- **Visualizer**: [`bridge.py`](file:///home/nils/archiv_master/virtual_neopixel/bridge.py) captures frames and renders them on an HTML5/Canvas UI with realistic 5050 LED geometry, acrylic diffuser optics, shelf reflections, dedicated player station hardware LEDs, and live score pips.

### Implementation B: Physical FastLED (Production Deployment)
- **Use Case**: Standalone deployment on the 12-meter bar counter. **No Linux machine required.**
- **Protocol**: Drives the physical 12-meter SK6812 LED strip on **Pin 6** and dedicated player LEDs on **Pin 3 & Pin 5** via the **FastLED** driver framework.
- Serial streaming code is compiled out; zero CPU overhead on the ATmega328P.

---

## 2. Hardware Wiring (Arduino Uno R3)

| Component | Arduino Pin | Mode | Connection Details | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **Player Station A Button** | `Pin 2` | `INPUT_PULLUP` | Pushbutton terminal to Pin 2, other to GND | Action button for Player A (Left side) |
| **Player Station A LED** | `Pin 3` | `OUTPUT` | Data IN via $330\text{--}470\,\Omega$ resistor, $+5\text{V}$, GND | Dedicated SK6812 LED mounted next to Button A |
| **Player Station B Button** | `Pin 4` | `INPUT_PULLUP` | Pushbutton terminal to Pin 4, other to GND | Action button for Player B (Right side) |
| **Player Station B LED** | `Pin 5` | `OUTPUT` | Data IN via $330\text{--}470\,\Omega$ resistor, $+5\text{V}$, GND | Dedicated SK6812 LED mounted next to Button B |
| **Physical Strip Data** | `Pin 6` | `OUTPUT` | FastLED Data IN via $330\text{--}470\,\Omega$ resistor | Drives 12m SK6812 strip (720 addressable LEDs) |
| **Game Selection Pot** | `Pin A1` | `INPUT` (Analog) | 10k potentiometer wiper (0..1023) | Selects active arcade game across 6 equal dial zones |
| **Master Brightness Pot** | `Pin A2` | `INPUT` (Analog) | 10k potentiometer wiper (0..1023) | Master luminosity ($0\%\text{--}100\%$) across all modes |
| **Mood / Base-Color Pot** | `Pin A3` | `INPUT` (Analog) | 10k potentiometer wiper (0..1023) | 360° rainbow spectrum ($0\text{--}65535$ hue) + pure White |
| **Light Behavior Pot** | `Pin A4` | `INPUT` (Analog) | 10k potentiometer wiper (0..1023) | Selects active ambient mode across 6 equal dial zones |
| **Entropy Source** | `Pin A0` | `INPUT` (Floating) | Left unconnected | Hardware entropy source for `randomSeed()` |

---

## 3. Dedicated Player Station LEDs (Pins 3 & 5)

Each player station has a dedicated single SK6812 RGB LED mounted right beside the action button:

| System State | Player A LED (Pin 3) | Player B LED (Pin 5) | Behavioral Purpose |
| :--- | :--- | :--- | :--- |
| **`STATE_IDLE_AMBIENT`** | Soft Cyan (`CRGB(0, 180, 255)` @ 27%) | Soft Coral Red (`CRGB(255, 30, 70)` @ 27%) | Subtle ambient beacon guiding bar patrons to the buttons |
| **`STATE_CHALLENGE`** *(P1 initiated)* | **Solid Cyan** (100% full brightness) | **Blinking Coral Red** (250ms ON / 250ms OFF, max 10x) | Visually summons Player B across the bar counter to duel |
| **`STATE_CHALLENGE`** *(P2 initiated)* | **Blinking Cyan** (250ms ON / 250ms OFF, max 10x) | **Solid Coral Red** (100% full brightness) | Visually summons Player A across the bar counter to duel |
| **`STATE_CHALLENGE`** *(Timeout)* | Station color (or solid Amber if Robot) | Station color (or solid Amber if Robot) | Turns solid **Neon Amber Gold** when RobotGamer takes over |
| **`STATE_PLAYING`** *(Normal)* | Active Side A Color (`getSideColor(SIDE_A)`) | Active Side B Color (`getSideColor(SIDE_B)`) | Continuous player identity indicator |
| **`STATE_PLAYING`** *(Button Press)* | **Dazzling White Flash (80 ms)** | **Dazzling White Flash (80 ms)** | **Instant tactile/visual feedback** on every button tap |
| **`STATE_PLAYING`** *(Round Over)* | Winner: Rapid Pulse; Loser: Dimmed | Winner: Rapid Pulse; Loser: Dimmed | Immediate round outcome feedback |
| **`STATE_MATCH_OVER`** | Winner: 100ms Victory Strobe; Loser: OFF | Winner: 100ms Victory Strobe; Loser: OFF | Synchronized with 12m strip match celebration |

---

## 4. The 6 Ambient Modes & 6 Arcade Games

### The 6 Ambient Lighting Modes (Potentiometer A4)
1. **Breathing / Pulse**: Meditative 7.5s sinusoidal breathing cycle between 42% and 82% brightness in rotary base color.
2. **Twinkle / Sparkle**: Drifting starry shimmer with brilliant white glints across 12 meters.
3. **Fire / Flame**: Thermodynamic hearth fire with convective cooling, thermal heat transfer, and embers.
4. **Comet / Meteor**: Gentle shooting star gliding across 12 meters with 20% fading tail.
5. **Scanner / Cylon**: Smooth 6.0s Larson scanner eye with Gaussian halo across 12 meters.
6. **Color Wipe**: Progressive chromatic roll cycling harmonic color offsets.

### The 6 Competitive Arcade Games (Potentiometer A1)
1. **Resonance Pulse**: Rhythm Wave Tug-of-War. Tap button during peak breath ($\ge 88\%$ resonance) to push the energy nexus 48 LEDs toward opponent's goal.
2. **Sparkle Rush**: High-Velocity Nova Deflector. Deflect the bouncing Nova inside your 108-LED defense shield; edge returns grant up to $+112\%$ speed boost.
3. **Flame Tug**: Bellows Forge Combustion Clash. Rapid-fire button mashing pumps your forge's bellows, advancing your combustion front by 24 LEDs per tap.
4. **Meteor Deflector**: High-Speed Comet Pong. Deflect the incoming meteor within your defense sector with speed acceleration.
5. **Cylon Clash**: Hyper-Pong Beam Duel. Intercept the high-speed laser puck as it enters your sector; each return accelerates the volley by $+115\%$.
6. **Territory Paint**: Rapid Wipe Wars. Rapidly tap to advance your color paint stroke across the 12 meters (24 LEDs per tap) until one side claims the entire counter.

---

## 5. Live Scoreboard Overlay at Strip Ends
- **Side A Score Meter**: Strip LEDs `0..4` illuminate inward in Player A's color (or Robot's Amber).
- **Side B Score Meter**: Strip LEDs `719..715` illuminate inward in Player B's color (or Robot's Amber).
- Matches are played first to **5 points** (`MATCH_WIN_SCORE = 5`).
- Match victory triggers a **4-second celebratory victory light show** across all 12 meters before returning to ambient lighting.

---

## 6. Running the Linux Visualizer

### Quick Start (Demo Mode or with Arduino):
```bash
# Auto-detects /dev/ttyACM0 or falls back to built-in 720-LED demo generator:
python3 virtual_neopixel/bridge.py
```

Then open your browser at:
👉 **`http://localhost:8080`**

### Keyboard & UI Controls in the Visualizer:
- **`[A]` Key**: Player Station A Action Button (Pin 2 / Pin 3)
- **`[L]` Key**: Player Station B Action Button (Pin 4 / Pin 5)
- **`[G]` Key**: Game-Start Challenge / Toggle Match
- **`[M]` Key / Slider**: Cycle or Dial Pot A4 Ambient Light Mode (0..5)
- **`[K]` Key / Slider**: Cycle or Dial Pot A1 Game Selection (0..5)
- **`[C]` Key / Slider**: Cycle or Dial Pot A3 Mood Color (Rainbow 0°..360° + Pure White)
- **`[B]` Key / Slider**: Adjust Pot A2 Master Brightness (0..255)

### Running Automated Protocol Verification:
```bash
python3 virtual_neopixel/simulator.py
```
Validates the 720-LED CMD 0x02 protocol framing, dedicated gamer LEDs, XOR checksum integrity, legacy CMD 0x01 compatibility, and minimum illumination guard.
