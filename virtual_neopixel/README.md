# Virtual NeoPixel Strip (60 LEDs/m) & Bar Counter Light System

High-fidelity Dual-Target NeoPixel controller for Arduino Uno R3 with Linux-based photorealistic visualizer for development and standalone production deployment.

---

## 1. Dual-Target Architecture

The system provides two compile-time implementations in [`barcounter_light.ino`](file:///home/nils/archiv_master/barcounter_light/barcounter_light.ino):

```cpp
#define BACKEND_VIRTUAL_SERIAL   0  // Implementation A: Stream pixel data to Linux Visualizer
#define BACKEND_ADAFRUIT_REAL    1  // Implementation B: Drive physical WS2812B strip via Adafruit library

// >>> CONFIGURE ACTIVE TARGET HERE <<<
#define STRIP_BACKEND            BACKEND_VIRTUAL_SERIAL
```

### Implementation A: Virtual Visualizer (Development)
- **Use Case**: Developing, testing animations, and tuning games on Linux without physical LEDs connected.
- **Protocol**: When `strip.show()` is called, the Arduino transmits a compact 185-byte binary frame (`0xAA 0x55 0x01 [60] [180 bytes RGB] [XOR_CHECKSUM]`) over USB Serial at **115200 baud**.
- **Visualizer**: [`bridge.py`](file:///home/nils/archiv_master/virtual_neopixel/bridge.py) captures the frames and renders them on an HTML5/Canvas UI with realistic 5050 LED geometry, acrylic diffuser simulation, and ambient wall radiance.

### Implementation B: Physical NeoPixel (Production)
- **Use Case**: Standalone deployment on the bar counter. **No Linux machine required.**
- **Protocol**: Directly drives the physical WS2812B LED strip on **Pin 6** via the official `Adafruit_NeoPixel` 800 kHz driver.
- All Serial streaming code is compiled out; zero overhead on the ATmega328P.

---

## 2. Hardware Wiring (Arduino Uno R3)

| Component | Arduino Pin | Mode | Notes |
| :--- | :--- | :--- | :--- |
| **Player 1 Button** | `Pin 2` | `INPUT_PULLUP` | Momentary pushbutton to GND |
| **Player 2 Button** | `Pin 4` | `INPUT_PULLUP` | Momentary pushbutton to GND |
| **Mode Switch** | `Pin 7` | `INPUT_PULLUP` | Momentary pushbutton to GND |
| **Physical Strip Data** | `Pin 6` | `OUTPUT` | Used in Implementation B (via 330Ω resistor) |

*Tip: Holding both Player 1 and Player 2 buttons simultaneously for 800 ms toggles Competitive Game Mode on and off.*

---

## 3. Visualization Modes & Two-Player Competitive Games

| Mode | Ambient Behavior | Competitive Game (Two 1-Button Controls) | Ambient Illumination Floor |
| :--- | :--- | :--- | :--- |
| **1. Breathing / Pulse** | Warm-white (2700K) gentle sinusoidal breathing pulse (3.6s cycle). | **"Resonance Pulse" (Rhythm Tug-of-War)**<br>An energy nexus breathes in the center. Tap your button at the apex of inhalation to fire a shockwave pushing the node toward the opponent's goal. | Clamped to $\ge 35\%$ average brightness. |
| **2. Twinkle / Sparkle** | Starry fairy lights with random shimmer and smooth fade envelopes. | **"Sparkle Rush" (Nova Reflector)**<br>A fast "Nova Sparkle" bounces across the strip. Press your button within your defense zone (P1: 0–8, P2: 51–59) to reflect it back with accelerated velocity. | Guaranteed $\ge 30\%$ ambient fairy sparkle. |
| **3. Fire / Flame** | Thermodynamic heat simulation (Fire2012 algorithm adapted for 60 LEDs). | **"Flame Tug" (Bellows Forge Clash)**<br>Blue Forge (P1) vs. Red Forge (P2). Tapping pumps oxygen into your bellows. Cadence anti-spam rewards a tactical 3–4 Hz rhythm. | Embers ensure continuous $\ge 35\%$ hearth illumination. |

---

## 4. Running the Linux Visualizer

### Quick Start (Demo Mode or with Arduino):
```bash
# Auto-detects /dev/ttyACM0 or falls back to demo mode if no Arduino is connected:
python3 virtual_neopixel/bridge.py
```

Then open your browser (e.g. Firefox) at:
👉 **`http://localhost:8080`**

### Keyboard Controls in the Visualizer:
- **`[A]` Key**: Player 1 Button
- **`[L]` Key**: Player 2 Button
- **`[M]` Key**: Toggle Mode
- **`[G]` Key**: Start / Stop Competition Game

### Running Automated Protocol Verification:
```bash
python3 virtual_neopixel/simulator.py --test-protocol
```

