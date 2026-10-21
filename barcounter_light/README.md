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
* **Physical Action Buttons & Dedicated Player LEDs**:
  * **Player Station A**: Action Button (Pin 2) & Dedicated LED (Pin 3).
  * **Player Station B**: Action Button (Pin 4) & Dedicated LED (Pin 5).
  * **Game-Start Challenge Protocol**:
    * Pressing either player's button while in ambient mode initiates a challenge.
    * The challenger's dedicated LED illuminates solid in their defined color.
    * The challenged player's dedicated LED blinks (max 10 times, 250ms ON / 250ms OFF) directly beside their button to summon them to play.
    * If accepted within 10 blinks, a 2-player human duel begins.
    * If no player accepts, the game automatically starts against an autonomous NPC: **RobotGamer** (their station LED turns solid Amber Gold)!
  * **In-Game Tactile Button Confirmation**:
    * Pressing a player's button during gameplay flashes that player's dedicated LED dazzling white for 80 ms, giving instant tactile feedback.
  * **Point Counters & Strip-End Indicators**:
    * Strip ends serve as physical score indicators (LEDs illuminate inward for each point scored).
    * Match play is first to 5 points, culminating in a 4-second celebratory victory light show.
  * **Dual Button Hold (Pins 2 & 4 for 800 ms)**: Aborts/toggles Game Mode on/off.
  * Turning the Global Light Behavior Knob (Pin A4) while in Game Mode gracefully exits back to ambient lighting.

---

## 2. Hardware Wiring & Pin Mapping

### Pin Connection Summary

| Arduino Pin | Peripheral | Connection Details | Purpose |
| :--- | :--- | :--- | :--- |
| **`Digital Pin 6`** | **SK6812 Strip (DIN)** | Data IN via $330\text{--}470\,\Omega$ series resistor | FastLED SK6812 800 kHz signal output (12m / 720 LEDs) |
| **`Digital Pin 2`** | **Player A Button (P1)** | Momentary push button to GND (`INPUT_PULLUP`) | Player A action input (Left side) |
| **`Digital Pin 3`** | **Player A Dedicated LED** | Data IN via $330\text{--}470\,\Omega$ series resistor | Dedicated SK6812 RGB LED positioned next to Player A button |
| **`Digital Pin 4`** | **Player B Button (P2)** | Momentary push button to GND (`INPUT_PULLUP`) | Player B action input (Right side) |
| **`Digital Pin 5`** | **Player B Dedicated LED** | Data IN via $330\text{--}470\,\Omega$ series resistor | Dedicated SK6812 RGB LED positioned next to Player B button |
| **`Analog Pin A1`** | **Game Selection Pot** | 10k potentiometer wiper (center pin) | Selects active 2-player arcade game |
| **`Analog Pin A2`** | **Brightness Pot** | 10k potentiometer wiper (center pin) | Master luminosity ($0\%\text{--}100\%$) |
| **`Analog Pin A3`** | **Color / Mood Pot** | 10k potentiometer wiper (center pin) | Mood / base-color selection & white |
| **`Analog Pin A4`** | **Light Behavior Pot** | 10k potentiometer wiper (center pin) | Selects active ambient lighting mode |
| **`Analog Pin A0`** | **Unconnected** | Left floating | Hardware entropy source for `randomSeed()` |
| **`5V`** | **Power Supply** | Strip 5V VDD, Button LEDs 5V VDD, Pot Outer Pin 1 | 5V DC regulated power |
| **`GND`** | **Common Ground** | Strip GND, Button terminals, Button LEDs GND, Pot Outer Pin 2 | Common system ground |

---

### Wiring Schematic

```text
                     +-----------------------+
                     |        Arduino        |
                     |                       |
                     |                   D6  |-----[ 330-470 Ohm ]-----> 12m SK6812 Strip Data IN
                     |                   D5  |-----[ 330-470 Ohm ]-----> Player B Dedicated LED DIN
                     |                   D4  |<------------------------- Player B Button Terminal
                     |                   D3  |-----[ 330-470 Ohm ]-----> Player A Dedicated LED DIN
                     |                   D2  |<------------------------- Player A Button Terminal
                     |                       |
       +5V <---------| 5V                A1  |<--+ (Game Selection Pot Wiper)
       GND <---------| GND               A2  |<--+---+ (Brightness Pot Wiper)
                     |                   A3  |<--+---+---+ (Color Pot Wiper)
                     |                   A4  |<--+---+---+---+ (Light Behavior Pot Wiper)
                     +-----------------------+   |   |   |   |
                                                 |   |   |   |
     === PLAYER STATION A (Left End) ===         |   |   |   |
     Player A Button (Pin 2):                    |   |   |   |
       [ Push Button ] ---> Pin D2               |   |   |   |
       Other Terminal  ---> GND                  |   |   |   |
     Player A Dedicated LED (Pin 3):             |   |   |   |
       DIN             ---> Pin D3 (via 330 Ohm) |   |   |   |
       5V / GND        ---> +5V / GND            |   |   |   |
                                                 |   |   |   |
     === PLAYER STATION B (Right End) ===        |   |   |   |
     Player B Button (Pin 4):                    |   |   |   |
       [ Push Button ] ---> Pin D4               |   |   |   |
       Other Terminal  ---> GND                  |   |   |   |
     Player B Dedicated LED (Pin 5):             |   |   |   |
       DIN             ---> Pin D5 (via 330 Ohm) |   |   |   |
       5V / GND        ---> +5V / GND            |   |   |   |
                                                 |   |   |   |
     === QUAD-POTENTIOMETER CONTROL PANEL ===    |   |   |   |
     Game Selection Pot (10k Linear):            |   |   |   |
       +5V ---[ O ]--- Wiper --------------------+   |   |   |
       GND ---[ O ]                                  |   |   |
     Master Brightness Pot (10k Linear):             |   |   |
       +5V ---[ O ]--- Wiper ------------------------+   |   |
       GND ---[ O ]                                      |   |
     Color / Mood Selection Pot (10k Linear):            |   |
       +5V ---[ O ]--- Wiper ----------------------------+   |
       GND ---[ O ]                                          |
     Global Light Behavior Pot (10k Linear):                 |
       +5V ---[ O ]--- Wiper --------------------------------+
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

### Game-Start Challenge & Acceptance Behavior
The bar counter supports an intuitive, arcade-style challenge flow directly from ambient mode without navigating menus:

1. **Initiating a Challenge**:
   * Any guest standing at the bar can press their button (**Player A on Pin 2** or **Player B on Pin 4**) while in ambient mode.
   * The **Challenger's dedicated LED** (on Pin 3 for Player A, Pin 5 for Player B) illuminates **solidly** in their defined player color:
     * **Player A**: Electric Cyan (`CRGB(0, 180, 255)`)
     * **Player B**: Neon Coral Red (`CRGB(255, 30, 70)`)
   * The **Opponent's dedicated LED** (at the opposite player station) begins **blinking up to 10 times** in that player's defined color (250 ms ON / 250 ms OFF = 5.0 seconds total window), actively summoning the challenged player to press their button.
   * Strip end LEDs `0` and `719` synchronize with the challenge status while ambient lighting continues softly across the inner strip LEDs (`1 .. 718`).

2. **Accepting the Challenge (2-Player Human Duel)**:
   * If a second player presses the challenged button within the 10-blink window, the challenge is accepted!
   * Both dedicated LEDs flash brilliant white and the active game dialed on the **Game Selection Knob (A1)** starts immediately as a human-vs-human duel.

3. **Challenge Timeout & RobotGamer (Automatic-Player NPC Fallback)**:
   * If all 10 blinks expire without another human accepting, the system does not give up!
   * Instead, the game automatically launches against an autonomous NPC: **RobotGamer**!
   * If Player A initiated the challenge, RobotGamer controls Station B. If Player B initiated, RobotGamer controls Station A.
   * RobotGamer's dedicated button LED and scoreboard indicators illuminate in radiant **Neon Gold / Amber** (`CRGB(255, 170, 0)`).

4. **Cancelling a Challenge**:
   * The challenger can tap their button a second time before timeout to abort the challenge and return to ambient lighting.
   * Turning the **Global Light Behavior Knob (A4)** at any time also immediately cancels the challenge.

---

### Dedicated Player Station LEDs (Pins 3 & 5)
Each player station features a dedicated single SK6812 RGB LED wired directly on its own digital pin (Pin 3 for Gamer A, Pin 5 for Gamer B) mounted beside the action button. FastLED drives them as independent single-pixel channels synchronized with the main 12-meter strip:

| System State | Player A Dedicated LED (Pin 3) | Player B Dedicated LED (Pin 5) | Behavioral Purpose |
| :--- | :--- | :--- | :--- |
| **`STATE_IDLE_AMBIENT`** | Soft Cyan (`COLOR_GAMER_A` @ 27%) | Soft Coral Red (`COLOR_GAMER_B` @ 27%) | Subtle ambient beacon guiding bar patrons to the player buttons in a dim bar |
| **`STATE_CHALLENGE`** *(P1 initiated)* | **Solid Cyan** (100% full brightness) | **Blinking Coral Red** (250ms ON / 250ms OFF, max 10x) | Alerts Player B across the bar counter to accept the duel |
| **`STATE_CHALLENGE`** *(P2 initiated)* | **Blinking Cyan** (250ms ON / 250ms OFF, max 10x) | **Solid Coral Red** (100% full brightness) | Alerts Player A across the bar counter to accept the duel |
| **`STATE_CHALLENGE`** *(Timeout)* | Station A color (or solid Amber if Robot) | Station B color (or solid Amber if Robot) | Visual confirmation that RobotGamer has taken over that station |
| **`STATE_PLAYING`** *(Normal)* | Active Side A Color (`getSideColor(SIDE_A)`) | Active Side B Color (`getSideColor(SIDE_B)`) | Persistent color identity (Cyan/Coral Red or Amber) |
| **`STATE_PLAYING`** *(Button Pressed)* | **Dazzling White Flash (80 ms)** | **Dazzling White Flash (80 ms)** | Instant tactile and visual confirmation on every button tap |
| **`STATE_PLAYING`** *(Round Over)* | Winner: Rapid White/Color Pulse; Loser: Dimmed | Winner: Rapid White/Color Pulse; Loser: Dimmed | Immediate round outcome feedback |
| **`STATE_MATCH_OVER`** | Winner: 100ms Victory Strobe; Loser: OFF | Winner: 100ms Victory Strobe; Loser: OFF | Synchronized with 12m strip match celebration |

---

### RobotGamer Autonomous NPC Mechanics
`RobotGamer` simulates organic, human-like reaction times and anti-spam constraints across all 6 games:

* **Game 1 (Resonance Pulse)**: Listens to the 7.5s ambient breathing cycle. When resonance reaches $\ge 88\%$, RobotGamer triggers a virtual pulse with a randomized 70–210 ms human reaction delay.
* **Game 2 (Sparkle Rush)**: Tracks the high-velocity Nova. Once the Nova penetrates into Robot's defense shield (40% depth), Robot triggers a deflection and earns an edge velocity bonus.
* **Game 3 (Flame Tug)**: Competes in the button-mashing tug-of-war at a realistic, rapid cadence (130–180 ms intervals), perfectly honoring the 120 ms anti-spam window.
* **Game 4 (Meteor Deflector)**: Monitors incoming meteors and deflects inside the 108-LED defense zone with natural timing variation.
* **Game 5 (Cylon Clash)**: Intercepts the laser puck as it enters its goal sector, returning volleys with acceleration.
* **Game 6 (Territory Paint)**: Rapidly pumps paint strokes at 125–175 ms intervals to contest the boundary against the human player.

---

### Point Counter & Strip-End Point Indicators
Both players compete in a multi-round match to determine the bar champion:

* **Physical Point Meters at Strip Ends**:
  * The extreme ends of the 12-meter strip function as live scoreboards.
  * For each point scored in a round, the **next LED moving inward** from that player's end illuminates in their defined color:
    * **Player 1**: LEDs `0`, `1`, `2`, `3`, `4` (illuminating left-to-right inward) in Electric Cyan (`CRGB(0, 180, 255)`).
    * **Player 2**: LEDs `719`, `718`, `717`, `716`, `715` (illuminating right-to-left inward) in Neon Coral Red (`CRGB(255, 30, 70)`).
    * **RobotGamer**: Illuminated in brilliant Neon Gold / Amber (`CRGB(255, 170, 0)`).
  * **Driver-Level Overlay**: Scores are rendered via `renderScoreOverlay()` inside `FastLEDDriver::show()`, guaranteeing the point indicators are permanently visible and immune to being overwritten by game physics animations.

* **Match Win Condition & Victory Celebration**:
  * Matches are played to **5 points** (`MATCH_WIN_SCORE = 5`).
  * When a player reaches 5 points:
    * A **4-second victory celebration** takes over the LED strip!
    * Sweeping celebratory pulse waves illuminate the entire 12 meters in the winner's color, accompanied by twinkling white victory glints.
    * The winning 5-point indicator remains solidly lit at the champion's end.
    * After 4 seconds, scores reset and the system smoothly returns to ambient bar lighting (`STATE_IDLE_AMBIENT`).

---

### Entering & Exiting 2-Player Game Mode
* **Instant Challenge**: Press either **Player 1 (Pin 2)** or **Player 2 (Pin 4)** button to start a game.
* **Dual Button Hold**: Pressing and holding both buttons simultaneously for 800 ms toggles between Game and Ambient mode.
* **Knob Override**: Turning the **Global Light Behavior Knob (A4)** while playing or challenging immediately exits to ambient mode and resets match scores.

---

### Rotary Color & Mood Potentiometer (Pin A3)
* **$0^\circ \text{ to } 350^\circ$ ($0\text{--}1010$ ADC counts)**: Full $360^\circ$ chromatic rainbow.
* **$350^\circ \text{ to } 360^\circ$ ($1012\text{--}1023$ ADC counts)**: Pure neutral white zone with hysteresis.
* **Universal Mood Anchor**: Dynamically tints the base visual palette for all ambient modes and game background floors without changing luminance.

---

### Rotary Master Brightness Potentiometer (Pin A2)
* Adjusts global strip luminosity from complete darkness ($0$) to full brilliance ($255$) across both ambient modes and games.

---

## 4. GamerHandler Architecture & Player Abstraction

To completely decouple the 6 arcade games from physical hardware pins, hardcoded player labels, and fixed color palettes, all player management, side assignments, and score tracking are encapsulated in the **`GamerHandler`** class.

### Core Architecture & Responsibilities

```text
       Physical Hardware                          GamerHandler Class                          Game Engines
  +--------------------------+               +--------------------------+               +----------------------+
  | Pin 2 (Button A)         |-------------->|  Input Polling & Debounce|               | ResonancePulseGame   |
  | Pin 4 (Button B)         |-------------->|  Virtual & Robot Presses |-------------->| SparkleRushGame      |
  | Serial ('1' or '2')      |-------------->|                          |               | FlameTugGame         |
  +--------------------------+               |  Dynamic Side Assignment |               | MeteorDeflectorGame  |
                                             |  - SIDE_A (Left station) |               | CylonClashGame       |
  +--------------------------+               |  - SIDE_B (Right station)|               | TerritoryPaintGame   |
  | RobotGamer NPC           |-------------->|                          |               +----------------------+
  +--------------------------+               |  Identity & Colors       |                          |
                                             |  - Player A: Cyan        |                          | Reports Round
                                             |  - Player B: Coral Red   |<-------------------------+ Winner (SIDE_A
                                             |  - Robot: Amber Gold     |                            or SIDE_B)
                                             |                          |
                                             |  Score & Match Lifecycle |
                                             |  - Point Counters (0-5)  |
                                             +--------------------------+
                                                          |
                                                          v
                                             +--------------------------+
                                             | Output Drivers (show())  |
                                             | - Pin 3: Dedicated LED A |
                                             | - Pin 5: Dedicated LED B |
                                             | - Pin 6: 720-LED Strip   |
                                             +--------------------------+
```

1. **Player Identities (`GamerId`)**:
   * `GAMER_PLAYER_A`: Human player at left station (Default: Pin 2).
   * `GAMER_PLAYER_B`: Human player at right station (Default: Pin 4).
   * `GAMER_ROBOT`: Autonomous NPC with human-like reflex curves and anti-spam modeling.
   * `GAMER_NONE`: Neutral / unassigned state.

2. **Physical Sides (`PlayerSide`) & Station Hardware**:
   * `SIDE_A`: Left side of the counter (Button on Pin 2, Dedicated LED on Pin 3, Score LEDs `0..4`).
   * `SIDE_B`: Right side of the counter (Button on Pin 4, Dedicated LED on Pin 5, Score LEDs `719..715`).

3. **Dynamic Side Assignment & Challenge Resolution**:
   * When a challenge is issued, `GamerHandler::startChallenge(who)` stores the challenger identity.
   * **Human vs. Human Duel**:
     * `SIDE_A` is assigned to `GAMER_PLAYER_A`.
     * `SIDE_B` is assigned to `GAMER_PLAYER_B`.
   * **Human vs. Robot Duel (Timeout Fallback)**:
     * If Player A initiated the challenge: `SIDE_A = GAMER_PLAYER_A`, `SIDE_B = GAMER_ROBOT`.
     * If Player B initiated the challenge: `SIDE_A = GAMER_ROBOT`, `SIDE_B = GAMER_PLAYER_B`.
   * Game engines query `gamers.getCurrentPlayer(side)` or `gamers.isRobot(side)` to inspect player types without needing to know physical pin connections.

4. **Dynamic Color Theme Resolution**:
   * Instead of hardcoded colors, games request colors via `gamers.getSideColor(side)`:
     * **Player A**: Electric Cyan (`CRGB(0, 180, 255)`)
     * **Player B**: Neon Coral Red (`CRGB(255, 30, 70)`)
     * **RobotGamer**: Neon Amber / Gold (`CRGB(255, 170, 0)`)
   * When battling `RobotGamer`, the robot's side (defense shields, energy barriers, forge colors, point LEDs) automatically renders in radiant Amber Gold.

5. **Universal Input Abstraction**:
   * Games never query `btnP1` or `btnP2` directly. They invoke:
     * `gamers.wasPressed(side)`: Rising edge trigger with 25 ms hardware debounce.
     * `gamers.isDown(side)`: Instantaneous pressed state.
   * **Transparent Virtual Press Injection**:
     * Serial commands (`'1'` for Side A, `'2'` for Side B) call `gamers.triggerVirtualPress(side)`.
     * `RobotGamer` calls `gamers.triggerRobotPress()`, which routes into the correct side.
     * The game engines execute identically regardless of whether the press originated from a physical button, USB serial, or autonomous AI.

6. **Round & Match Scoring System**:
   * **Round Win Reporting**: When a point is scored, the game calls `gamers.setRoundWinner(side, now)`.
   * **Auto-Tally & Serial Announcement**: Points are awarded once per round via `gamers.awardRoundPoint()`, broadcasting live scores to Serial.
   * **Strip-End Scoreboard Overlay**: `gamers.renderScoreOverlay(leds, numLeds, state)` paints the score LEDs at indices `0..4` (Side A) and `719..715` (Side B) directly inside `FastLEDDriver::show()`, making the scoreboard visible on top of any game animation.
   * **Match Lifecycle**: `gamers.isMatchWon()` detects when a player reaches 5 points, routing to `STATE_MATCH_OVER` for the victory celebration before cleanly resetting via `gamers.resetScores()`.

### Game Integration Summary

All 6 games receive `GamerHandler& gamers` in their constructor:

| Game Class | Input Method | Colors Requested | Round Victory Method |
| :--- | :--- | :--- | :--- |
| `ResonancePulseGame` | `gamers.wasPressed(SIDE_A/B)` | `gamers.getSideColor(SIDE_A/B)` | `gamers.setRoundWinner(side, now)` |
| `SparkleRushGame` | `gamers.wasPressed(SIDE_A/B)` | `gamers.getSideColor(SIDE_A/B)` | `gamers.setRoundWinner(side, now)` |
| `FlameTugGame` | `gamers.wasPressed(SIDE_A/B)` | `gamers.getSideColor(SIDE_A/B)` | `gamers.setRoundWinner(side, now)` |
| `MeteorDeflectorGame` | `gamers.wasPressed(SIDE_A/B)` | `gamers.getSideColor(SIDE_A/B)` | `gamers.setRoundWinner(side, now)` |
| `CylonClashGame` | `gamers.wasPressed(SIDE_A/B)` | `gamers.getSideColor(SIDE_A/B)` | `gamers.setRoundWinner(side, now)` |
| `TerritoryPaintGame` | `gamers.wasPressed(SIDE_A/B)` | `gamers.getSideColor(SIDE_A/B)` | `gamers.setRoundWinner(side, now)` |

---

## 5. The 6 Ambient Lighting Modes

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

## 6. The 6 Competitive 2-Player Arcade Games

Separated from lighting modes into independent game engines driven by the `GamerHandler` interface. Side A (Left End, Pin 2 / LEDs `0..4`) battles Side B (Right End, Pin 4 / LEDs `719..715`) across the 12-meter bar counter (720 LEDs). Sides dynamically host Player A (Cyan), Player B (Coral Red), or RobotGamer (Amber) depending on challenge origin:

```text
Side A (Left End, Pin 2 / Score LEDs 0..4)              Side B (Right End, Pin 4 / Score LEDs 719..715)
  [ Player A or Robot ] ============= 12 METERS (720 LEDs) ============= [ Player B or Robot ]
```

---

### Game 1: "Resonance Pulse" (Rhythm Wave Tug-of-War)
* **Dial Zone**: Zone 0 (ADC $0\text{--}170$)
* **Mechanics**:
  * An oscillating energy nexus floats between Side A's territory and Side B's territory.
  * **The Resonance Window**: When the ambient breath wave reaches peak amplitude ($\ge 88\%$), the nexus turns dazzling white.
  * **Timing**: Press your button *only* during Resonance to push the nexus toward your opponent's goal (48 LEDs per pulse).
  * **Penalty**: Pressing outside Resonance triggers a **400 ms misfire penalty cooldown**.
* **Win Condition**: Pushing the nexus into the opponent's goal wins the round. The winner is reported via `gamers.setRoundWinner(side, now)` and automatically resets after 2 seconds.

---

### Game 2: "Sparkle Rush" (Nova Sparkle Reflex Deflector)
* **Dial Zone**: Zone 1 (ADC $171\text{--}341$)
* **Mechanics**:
  * A high-velocity cosmic Nova travels across a starry background between Side A's defense shield and Side B's defense shield (108 LEDs / $\approx 1.8\text{ meters}$ each, rendered in `gamers.getSideColor(side)`).
  * Press your button while the Nova is inside your shield to deflect it back.
  * **Edge Bonus**: Deflecting near the outer edge gives up to a $+112\%$ speed boost, accelerating the Nova across the bar.
* **Win Condition**: If the Nova breaches the defense shield and exits the strip, the opposing side scores the win.

---

### Game 3: "Flame Tug" (Bellows Forge Combustion Clash)
* **Dial Zone**: Zone 2 (ADC $342\text{--}511$)
* **Mechanics**:
  * Rapid-fire button-mashing tug-of-war! Side A and Side B power independent plasma forges rendered in their respective gamer colors.
  * Each press pumps your forge's bellows, advancing your combustion front by 24 LEDs ($\approx 40\text{ cm}$).
  * Built-in 120 ms anti-spam filtering rewards rapid, rhythmic, deliberate taps.
* **Win Condition**: Pushing the white-hot molten collision point into the opponent's forge wins the clash.

---

### Game 4: "Meteor Deflector" (High-Speed Comet Rally)
* **Dial Zone**: Zone 3 (ADC $512\text{--}682$)
* **Mechanics**:
  * A flaming meteor hurtles between Side A's energy barrier and Side B's energy barrier (rendered in each player's defined color).
  * Deflect the incoming meteor while inside your defense sector (108 LEDs).
  * Timing deep in your zone accelerates the meteor by $+114\%$ plus an edge-distance velocity bonus up to terminal velocity.
* **Win Condition**: Failing to deflect the meteor lets it crash into your terminal station, awarding the round to your opponent.

---

### Game 5: "Cylon Clash" (Hyper-Pong Beam Duel)
* **Dial Zone**: Zone 4 (ADC $683\text{--}852$)
* **Mechanics**:
  * Kinetic laser pong. A high-energy plasma puck rebounds between Side A's barrier and Side B's barrier.
  * Deflect the puck as it enters your barrier; each return increases speed by $+115\%$.
* **Win Condition**: First player whose barrier is breached loses the round.

---

### Game 6: "Territory Paint" (Rapid Wipe Wars)
* **Dial Zone**: Zone 5 (ADC $853\text{--}1023$)
* **Mechanics**:
  * High-speed territory capture! Side A commands its gamer color and Side B commands its gamer color, divided by a sharp white boundary.
  * Tap your button to advance your color paint stroke across the 12 meters (24 LEDs per tap).
* **Win Condition**: Completely painting the entire strip with your color claims the bar counter and wins the game.

---

## 7. Serial Commands & Remote Control

Connect at **115200 baud** to control the system via USB Serial:

| Command | Action |
| :--- | :--- |
| **`1`** | Injects virtual press for Side A (Player 1) via `gamers.triggerVirtualPress(SIDE_A)` |
| **`2`** | Injects virtual press for Side B (Player 2) via `gamers.triggerVirtualPress(SIDE_B)` |
| **`m`** or **`M`** | Steps to the next ambient lighting mode via serial |
| **`g`** or **`G`** | Toggles 2-player Game Mode on/off |
| **`w`** or **`W`** | Sets mood color to Pure White |
| **`h<0-65535>`** | Sets hue directly (e.g. `h0` for Red, `h21845` for Green, `h43690` for Blue) |
| **`b<0-255>`** | Sets global brightness directly (e.g. `b200`) |
| **`b`** | Steps brightness up by $+25$ (wraps at 255) |
| **`c`** or **`C`** | Steps hue by $+4000$ ($\approx 22^\circ$ around the color wheel) |

---

## 8. Power Supply & 12-Meter Strip Electrical Guidelines

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
