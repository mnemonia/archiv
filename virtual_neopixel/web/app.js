/* ============================================================================
   Virtual NeoPixel Strip (12m / 720 LEDs) - Optical Engine & Client Application
   ============================================================================ */

const NUM_LEDS = 720; // 12-meter SK6812 strip (60 LEDs/m)

// Canvas Elements
const stripCanvas = document.getElementById("stripCanvas");
const stripCtx = stripCanvas.getContext("2d");
const glowCanvas = document.getElementById("ambientGlowCanvas");
const glowCtx = glowCanvas.getContext("2d");

// DOM Controls & Telemetry
const statusIndicator = document.getElementById("statusIndicator");
const sourceValue = document.getElementById("sourceValue");
const fpsValue = document.getElementById("fpsValue");
const luxValue = document.getElementById("luxValue");
const stateBadge = document.getElementById("stateBadge");
const matchScoreValue = document.getElementById("matchScoreValue");
const meterInfo = document.getElementById("meterInfo");

const modeTitle = document.getElementById("modeTitle");
const gameTitle = document.getElementById("gameTitle");
const statusCallout = document.getElementById("statusCallout");
const calloutPill = document.getElementById("calloutPill");
const calloutText = document.getElementById("calloutText");
const gameInstruction = document.getElementById("gameInstruction");

const diffuserOverlay = document.getElementById("diffuserOverlay");
const shelfBackdrop = document.getElementById("shelfBackdrop");

// Player Hardware Stations
const btnP1 = document.getElementById("btnP1");
const btnP2 = document.getElementById("btnP2");
const ledStationA = document.getElementById("ledStationA");
const ledDieA = document.getElementById("ledDieA");
const ledStationB = document.getElementById("ledStationB");
const ledDieB = document.getElementById("ledDieB");
const roleLabelA = document.getElementById("roleLabelA");
const roleLabelB = document.getElementById("roleLabelB");
const pipsA = document.querySelectorAll("#pipsA .pip");
const pipsB = document.querySelectorAll("#pipsB .pip");

// Control Panel & Action Buttons
const btnToggleGame = document.getElementById("btnToggleGame");
const btnCycleMode = document.getElementById("btnCycleMode");
const btnCycleGame = document.getElementById("btnCycleGame");

const lightKnobSlider = document.getElementById("lightKnobSlider");
const lightKnobText = document.getElementById("lightKnobText");
const gameKnobSlider = document.getElementById("gameKnobSlider");
const gameKnobText = document.getElementById("gameKnobText");
const hueSlider = document.getElementById("hueSlider");
const hueValue = document.getElementById("hueValue");
const colorPreview = document.getElementById("colorPreview");
const brightnessSlider = document.getElementById("brightnessSlider");
const brightnessValue = document.getElementById("brightnessValue");

const diffuserSelect = document.getElementById("diffuserSelect");
const shelfTextureSelect = document.getElementById("shelfTextureSelect");
const bloomSlider = document.getElementById("bloomSlider");

// Viewport modes: 'full' (all 720 LEDs), 'stationA' (0..120), 'center' (240..480), 'stationB' (600..719)
let activeView = "full";
const viewButtons = {
  full: document.getElementById("viewFull"),
  stationA: document.getElementById("viewStationA"),
  center: document.getElementById("viewCenter"),
  stationB: document.getElementById("viewStationB")
};

// Mode & Game Names
const LIGHT_MODE_NAMES = [
  "Breathing / Pulse",
  "Twinkle / Sparkle",
  "Fire / Flame",
  "Comet / Meteor",
  "Scanner / Cylon",
  "Color Wipe"
];

const GAME_NAMES = [
  "Resonance Pulse",
  "Sparkle Rush",
  "Flame Tug",
  "Meteor Deflector",
  "Cylon Clash",
  "Territory Paint"
];

const GAME_HINTS = [
  "Rhythm Tug-of-War: Tap your button at peak inhalation (>=88% resonance)!",
  "Nova Reflex Deflector: Smash the cosmic Nova back within your 108-LED defense zone!",
  "Bellows Forge Clash: Rapid, rhythmic button mashing advances your plasma front!",
  "Meteor Deflector: Deflect the incoming meteor deep in your sector for speed bonus!",
  "Cylon Clash: Intercept the high-speed laser puck before it breaches your barrier!",
  "Territory Paint: Rapidly pump paint strokes to claim all 12 meters of the bar!"
];

// Strip & Hardware State
let ledBuffer = Array.from({ length: NUM_LEDS }, () => [0, 0, 0]);
let ledGamerA = [0, 180, 255];
let ledGamerB = [255, 30, 70];
let scores = [0, 0];
let systemState = "idle_ambient";
let challenger = "none";
let againstRobot = false;
let activeModeIdx = 0;
let activeGameIdx = 0;
let bloomIntensity = 0.85;

let currentHueDeg = 0;
let isWhiteMode = false;
let currentMoodRGB = [157, 0, 0];
let currentBrightness = 255;

// Initial Optics
diffuserOverlay.className = "diffuser-overlay acrylic";

// Constant Perceptual Brightness Color Wheel
function hsvToRgb(hDeg, s = 1.0, v = 1.0) {
  const h = ((hDeg % 360) + 360) % 360;
  const c = v * s;
  const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
  const m = v - c;
  let r1 = 0, g1 = 0, b1 = 0;
  if (h < 60) { r1 = c; g1 = x; b1 = 0; }
  else if (h < 120) { r1 = x; g1 = c; b1 = 0; }
  else if (h < 180) { r1 = 0; g1 = c; b1 = x; }
  else if (h < 240) { r1 = 0; g1 = x; b1 = c; }
  else if (h < 300) { r1 = x; g1 = 0; b1 = c; }
  else { r1 = c; g1 = 0; b1 = x; }
  return [
    Math.round((r1 + m) * 255),
    Math.round((g1 + m) * 255),
    Math.round((b1 + m) * 255)
  ];
}

function constantBrightnessRgb(hDeg) {
  const [r, g, b] = hsvToRgb(hDeg);
  const pBright = Math.sqrt(0.299 * r * r + 0.587 * g * g + 0.114 * b * b);
  if (pBright > 0.001) {
    const scale = 85.8 / pBright;
    return [
      Math.min(255, Math.round(r * scale)),
      Math.min(255, Math.round(g * scale)),
      Math.min(255, Math.round(b * scale))
    ];
  }
  return [r, g, b];
}

function getHueName(h) {
  if (h >= 345 || h < 15) return "Red";
  if (h < 45) return "Orange";
  if (h < 75) return "Yellow";
  if (h < 150) return "Green";
  if (h < 195) return "Cyan";
  if (h < 255) return "Blue";
  if (h < 285) return "Purple";
  if (h < 345) return "Magenta";
  return "Red";
}

// ----------------------------------------------------------------------------
// 1. Viewport Selection (Full 12m vs Zoom Modes)
// ----------------------------------------------------------------------------
function setView(view) {
  activeView = view;
  Object.keys(viewButtons).forEach((k) => {
    if (viewButtons[k]) {
      viewButtons[k].classList.toggle("active", k === view);
    }
  });

  if (view === "full") {
    meterInfo.textContent = "Viewing: 0.0m – 12.0m (All 720 LEDs)";
  } else if (view === "stationA") {
    meterInfo.textContent = "Viewing: 0.0m – 2.0m (Station A & Shield LEDs 0..120)";
  } else if (view === "center") {
    meterInfo.textContent = "Viewing: 4.0m – 8.0m (Center Arena LEDs 240..480)";
  } else if (view === "stationB") {
    meterInfo.textContent = "Viewing: 10.0m – 12.0m (Station B & Shield LEDs 600..719)";
  }
}

Object.keys(viewButtons).forEach((k) => {
  if (viewButtons[k]) {
    viewButtons[k].addEventListener("click", () => setView(k));
  }
});

// ----------------------------------------------------------------------------
// 2. Photorealistic 720-LED Rendering Engine
// ----------------------------------------------------------------------------
function drawStrip() {
  const width = stripCanvas.width;
  const height = stripCanvas.height;

  // Clear Strip PCB
  stripCtx.fillStyle = "#0c0e12";
  stripCtx.fillRect(0, 0, width, height);

  // PCB Copper Bus Lines (+5V top, GND bottom)
  stripCtx.strokeStyle = "#272b35";
  stripCtx.lineWidth = 2;
  stripCtx.beginPath();
  stripCtx.moveTo(0, 10);
  stripCtx.lineTo(width, 10);
  stripCtx.moveTo(0, height - 10);
  stripCtx.lineTo(width, height - 10);
  stripCtx.stroke();

  // Clear Wall Radiance Canvas
  glowCtx.clearRect(0, 0, glowCanvas.width, glowCanvas.height);

  // Determine LED range to draw based on viewport
  let startIdx = 0;
  let endIdx = NUM_LEDS;

  if (activeView === "stationA") {
    startIdx = 0; endIdx = 120;
  } else if (activeView === "center") {
    startIdx = 240; endIdx = 480;
  } else if (activeView === "stationB") {
    startIdx = 600; endIdx = 720;
  }

  const visibleCount = endIdx - startIdx;
  const pitch = width / visibleCount;
  const isHighDensity = pitch < 5.0;

  for (let i = startIdx; i < endIdx; i++) {
    const pixel = ledBuffer[i] || [0, 0, 0];
    const r = pixel[0];
    const g = pixel[1];
    const b = pixel[2];

    const screenX = (i - startIdx) * pitch;
    const centerX = screenX + pitch / 2;
    const centerY = height / 2;

    if (isHighDensity) {
      // High-density full 12-meter view: draw crisp rectangular 5050 sub-assemblies
      const ledWidth = Math.max(1.2, pitch - 0.4);
      const ledHeight = 22;

      // White 5050 package casing
      stripCtx.fillStyle = "#374151";
      stripCtx.fillRect(centerX - ledWidth / 2, centerY - ledHeight / 2, ledWidth, ledHeight);

      // Core RGB emitter
      stripCtx.fillStyle = `rgb(${r}, ${g}, ${b})`;
      stripCtx.fillRect(centerX - ledWidth / 2, centerY - (ledHeight * 0.7) / 2, ledWidth, ledHeight * 0.7);

      const lum = 0.299 * r + 0.587 * g + 0.114 * b;
      if (lum > 170) {
        stripCtx.fillStyle = "rgba(255, 255, 255, 0.4)";
        stripCtx.fillRect(centerX - ledWidth / 2, centerY - 2, ledWidth, 4);
      }
    } else {
      // Zoomed-in view: detailed 5050 SMD package with notch, copper pads, dome
      const ledSize = Math.min(pitch * 0.78, 30);

      // Solder pads
      stripCtx.fillStyle = "#a8783e";
      stripCtx.fillRect(centerX - ledSize / 2 - 3, centerY - ledSize / 2, 2, ledSize);
      stripCtx.fillRect(centerX + ledSize / 2 + 1, centerY - ledSize / 2, 2, ledSize);

      // 5050 square casing
      stripCtx.fillStyle = "#e5e7eb";
      stripCtx.fillRect(centerX - ledSize / 2, centerY - ledSize / 2, ledSize, ledSize);

      // Orientation notch
      stripCtx.fillStyle = "#9ca3af";
      stripCtx.beginPath();
      stripCtx.moveTo(centerX - ledSize / 2, centerY - ledSize / 2);
      stripCtx.lineTo(centerX - ledSize / 2 + 4, centerY - ledSize / 2);
      stripCtx.lineTo(centerX - ledSize / 2, centerY - ledSize / 2 + 4);
      stripCtx.closePath();
      stripCtx.fill();

      // Phosphor dome
      stripCtx.fillStyle = "#d1d5db";
      stripCtx.beginPath();
      stripCtx.arc(centerX, centerY, ledSize * 0.38, 0, Math.PI * 2);
      stripCtx.fill();

      // RGB Emitter Die
      stripCtx.fillStyle = `rgb(${r}, ${g}, ${b})`;
      stripCtx.beginPath();
      stripCtx.arc(centerX, centerY, ledSize * 0.32, 0, Math.PI * 2);
      stripCtx.fill();

      // White Hot-Spot Core
      const lum = 0.299 * r + 0.587 * g + 0.114 * b;
      if (lum > 140) {
        const coreAlpha = (lum - 140) / 115;
        stripCtx.fillStyle = `rgba(255, 255, 255, ${coreAlpha * 0.6})`;
        stripCtx.beginPath();
        stripCtx.arc(centerX, centerY, ledSize * 0.15, 0, Math.PI * 2);
        stripCtx.fill();
      }
    }

    // Ambient Radiance / Wall bloom (stepped to keep 60 FPS)
    const step = isHighDensity ? 6 : 2;
    if (i % step === 0) {
      const lum = 0.299 * r + 0.587 * g + 0.114 * b;
      if (lum > 20 && bloomIntensity > 0) {
        const glowRadius = Math.max(16, (lum / 255) * 85 * bloomIntensity);
        const glowAlpha = Math.min(0.7, (lum / 255) * 0.55 * bloomIntensity);

        const grad = glowCtx.createRadialGradient(
          centerX, 140, 2,
          centerX, 140, glowRadius
        );
        grad.addColorStop(0, `rgba(${r}, ${g}, ${b}, ${glowAlpha})`);
        grad.addColorStop(0.5, `rgba(${r}, ${g}, ${b}, ${glowAlpha * 0.3})`);
        grad.addColorStop(1, "rgba(0, 0, 0, 0)");

        glowCtx.fillStyle = grad;
        glowCtx.beginPath();
        glowCtx.arc(centerX, 140, glowRadius, 0, Math.PI * 2);
        glowCtx.fill();
      }
    }
  }
}

// ----------------------------------------------------------------------------
// 3. Update Hardware Station LEDs & Live Scoreboard Pips
// ----------------------------------------------------------------------------
function updateHardwareStationUI() {
  // Update Dedicated Station A LED (Pin 3)
  const [ra, ga, ba] = ledGamerA;
  const luma = 0.299 * ra + 0.587 * ga + 0.114 * ba;
  ledDieA.style.backgroundColor = `rgb(${ra}, ${ga}, ${ba})`;
  if (luma > 20) {
    const blur = Math.round((luma / 255) * 18);
    ledStationA.style.boxShadow = `0 0 ${blur}px rgba(${ra}, ${ga}, ${ba}, 0.9), inset 0 2px 4px rgba(0,0,0,0.8)`;
  } else {
    ledStationA.style.boxShadow = "inset 0 2px 4px rgba(0,0,0,0.8)";
  }

  // Update Dedicated Station B LED (Pin 5)
  const [rb, gb, bb] = ledGamerB;
  const lumb = 0.299 * rb + 0.587 * gb + 0.114 * bb;
  ledDieB.style.backgroundColor = `rgb(${rb}, ${gb}, ${bb})`;
  if (lumb > 20) {
    const blur = Math.round((lumb / 255) * 18);
    ledStationB.style.boxShadow = `0 0 ${blur}px rgba(${rb}, ${gb}, ${bb}, 0.9), inset 0 2px 4px rgba(0,0,0,0.8)`;
  } else {
    ledStationB.style.boxShadow = "inset 0 2px 4px rgba(0,0,0,0.8)";
  }

  // Station Labels (Show RobotGamer if autonomous NPC is playing)
  if (againstRobot) {
    if (challenger === "player_a") {
      roleLabelB.textContent = "RobotGamer (Autonomous NPC) • Neon Amber";
      roleLabelA.textContent = "Player A • Electric Cyan";
    } else {
      roleLabelA.textContent = "RobotGamer (Autonomous NPC) • Neon Amber";
      roleLabelB.textContent = "Player B • Neon Coral Red";
    }
  } else {
    roleLabelA.textContent = "Player A • Electric Cyan";
    roleLabelB.textContent = "Player B • Neon Coral Red";
  }

  // Score Pips (Strip ends: LEDs 0..4 and 719..715)
  const scoreA = scores[0];
  const scoreB = scores[1];

  pipsA.forEach((pip, idx) => {
    pip.className = "pip";
    if (idx < scoreA) {
      pip.classList.add(againstRobot && challenger === "player_b" ? "active-robot" : "active-a");
    }
  });

  pipsB.forEach((pip, idx) => {
    pip.className = "pip";
    if (idx < scoreB) {
      pip.classList.add(againstRobot && challenger === "player_a" ? "active-robot" : "active-b");
    }
  });

  matchScoreValue.textContent = `A [${scoreA}] - [${scoreB}] B`;

  // Status Callout Banner
  if (systemState === "challenge") {
    stateBadge.textContent = "CHALLENGE";
    stateBadge.style.color = "#f43f5e";
    calloutPill.className = "callout-pill challenge";
    calloutPill.textContent = "CHALLENGE ACTIVE";
    calloutText.textContent = `Player ${challenger === "player_a" ? "A" : "B"} initiated a challenge! Opponent LED is blinking to summon them.`;
  } else if (systemState === "playing") {
    stateBadge.textContent = "PLAYING";
    stateBadge.style.color = "#10b981";
    calloutPill.className = "callout-pill playing";
    calloutPill.textContent = againstRobot ? "VS ROBOTGAMER" : "2-PLAYER DUEL";
    calloutText.textContent = `Match in progress: ${GAME_NAMES[activeGameIdx]} (First to 5 points wins!)`;
  } else if (systemState === "match_over") {
    stateBadge.textContent = "VICTORY";
    stateBadge.style.color = "#facc15";
    calloutPill.className = "callout-pill victory";
    calloutPill.textContent = "MATCH WON!";
    calloutText.textContent = "Champion crowned! 4-second victory light celebration across all 12 meters!";
  } else {
    stateBadge.textContent = "AMBIENT";
    stateBadge.style.color = "#38bdf8";
    calloutPill.className = "callout-pill";
    calloutPill.textContent = "AMBIENT MODE";
    calloutText.textContent = `Bar counter is running ${LIGHT_MODE_NAMES[activeModeIdx]}. Press [A] or [L] to start a match!`;
  }

  gameInstruction.textContent = systemState === "playing" ? GAME_HINTS[activeGameIdx] :
    "Press Player A [A] or Player B [L] to challenge opponent! Opponent's dedicated LED blinks 10x to summon them.";
}

// ----------------------------------------------------------------------------
// 4. Real-Time Telemetry & Frame Polling from bridge.py
// ----------------------------------------------------------------------------
let isConnected = false;

async function fetchFrame() {
  try {
    const res = await fetch("/api/frame", { cache: "no-store" });
    if (res.ok) {
      const data = await res.json();
      if (data.leds && data.leds.length > 0) {
        ledBuffer = data.leds;
      }
      if (data.gamer_a) ledGamerA = data.gamer_a;
      if (data.gamer_b) ledGamerB = data.gamer_b;
      if (data.scores) scores = data.scores;
      if (data.state) systemState = data.state;
      if (data.challenger) challenger = data.challenger;
      if (data.against_robot !== undefined) againstRobot = data.against_robot;

      if (data.active_mode_idx !== undefined && data.active_mode_idx !== activeModeIdx) {
        activeModeIdx = data.active_mode_idx;
        modeTitle.textContent = LIGHT_MODE_NAMES[activeModeIdx];
        lightKnobSlider.value = activeModeIdx;
        lightKnobText.textContent = `Mode ${activeModeIdx + 1}: ${LIGHT_MODE_NAMES[activeModeIdx].split(" ")[0]}`;
      }

      if (data.active_game_idx !== undefined && data.active_game_idx !== activeGameIdx) {
        activeGameIdx = data.active_game_idx;
        gameTitle.textContent = GAME_NAMES[activeGameIdx];
        gameKnobSlider.value = activeGameIdx;
        gameKnobText.textContent = `Game ${activeGameIdx + 1}: ${GAME_NAMES[activeGameIdx].split(" ")[0]}`;
      }

      fpsValue.textContent = `${data.fps} FPS`;
      luxValue.textContent = `${data.luminance}%`;
      sourceValue.textContent = data.source || "Connected";

      if (!isConnected) {
        isConnected = true;
        statusIndicator.classList.remove("offline");
      }
    }
  } catch (err) {
    if (isConnected) {
      isConnected = false;
      statusIndicator.classList.add("offline");
      sourceValue.textContent = "Offline (Local Demo)";
    }
    runLocalDemoFallback();
  }

  updateHardwareStationUI();
  drawStrip();
  requestAnimationFrame(fetchFrame);
}

// ----------------------------------------------------------------------------
// 5. Local Standalone Simulation Fallback (when opened directly in browser)
// ----------------------------------------------------------------------------
let localTime = 0;
let localModeIdx = 0;
let lastLocalModeSwitch = performance.now();

function runLocalDemoFallback() {
  localTime += 0.022;
  const now = performance.now();

  if (now - lastLocalModeSwitch > 15000) {
    localModeIdx = (localModeIdx + 1) % 6;
    lastLocalModeSwitch = now;
    activeModeIdx = localModeIdx;
    modeTitle.textContent = LIGHT_MODE_NAMES[activeModeIdx];
  }

  const [baseR, baseG, baseB] = currentMoodRGB;

  // 1. Breathing / Pulse
  const wave = (Math.sin(localTime * 0.84) + 1.0) * 0.5;
  const factor = 0.42 + wave * 0.40;
  const r = Math.round(baseR * factor);
  const g = Math.round(baseG * factor);
  const b = Math.round(baseB * factor);

  for (let i = 0; i < NUM_LEDS; i++) {
    ledBuffer[i] = [r, g, b];
  }

  // Beacon LEDs
  ledGamerA = [0, 180, 255];
  ledGamerB = [255, 30, 70];

  fpsValue.textContent = "45 FPS (Demo)";
  luxValue.textContent = "42%";
}

// ----------------------------------------------------------------------------
// 6. User Inputs & Serial Key Relay
// ----------------------------------------------------------------------------
async function sendInput(key) {
  try {
    await fetch(`/api/input?key=${encodeURIComponent(key)}`);
  } catch (e) {
    // Local preview mode
  }
}

// Button A Press
function triggerP1() {
  btnP1.classList.add("pressed");
  setTimeout(() => btnP1.classList.remove("pressed"), 120);
  sendInput("1");
}

// Button B Press
function triggerP2() {
  btnP2.classList.add("pressed");
  setTimeout(() => btnP2.classList.remove("pressed"), 120);
  sendInput("2");
}

// Challenge / Toggle Game
function triggerGameToggle() {
  sendInput("g");
}

// Cycle Ambient Mode
function triggerCycleMode() {
  sendInput("m");
}

// Cycle Game
function triggerCycleGame() {
  sendInput("k");
}

// Rotary Mood Color
let pendingHue = null;
let hueSendTimer = null;

function throttledSendHue(hue) {
  pendingHue = hue;
  if (!hueSendTimer) {
    hueSendTimer = setTimeout(() => {
      if (pendingHue !== null) {
        sendInput(`h${pendingHue}`);
        pendingHue = null;
      }
      hueSendTimer = null;
    }, 40);
  }
}

function updateMoodColorDisplay(deg, sendToArduino = true) {
  if (deg > 360 || deg === "white") {
    isWhiteMode = true;
    currentHueDeg = 375;
    currentMoodRGB = [86, 86, 86];
    if (hueSlider) hueSlider.value = 375;
    if (colorPreview) {
      colorPreview.style.background = "#565656";
      colorPreview.style.boxShadow = "0 0 10px rgba(255, 255, 255, 0.5)";
    }
    if (hueValue) hueValue.textContent = "White (Pure)";
    if (sendToArduino) sendInput("w");
  } else {
    isWhiteMode = false;
    currentHueDeg = ((deg % 360) + 360) % 360;
    currentMoodRGB = constantBrightnessRgb(currentHueDeg);
    if (hueSlider) hueSlider.value = currentHueDeg;
    if (colorPreview) {
      colorPreview.style.background = `rgb(${currentMoodRGB[0]}, ${currentMoodRGB[1]}, ${currentMoodRGB[2]})`;
      colorPreview.style.boxShadow = `0 0 8px rgba(${currentMoodRGB[0]}, ${currentMoodRGB[1]}, ${currentMoodRGB[2]}, 0.8)`;
    }
    if (hueValue) {
      hueValue.textContent = `${Math.round(currentHueDeg)}° (${getHueName(currentHueDeg)})`;
    }
    if (sendToArduino) {
      const arduinoHue = Math.round((currentHueDeg / 360.0) * 65535);
      throttledSendHue(arduinoHue);
    }
  }
}

// Rotary Brightness
let pendingBrightness = null;
let brightnessSendTimer = null;

function throttledSendBrightness(b) {
  pendingBrightness = b;
  if (!brightnessSendTimer) {
    brightnessSendTimer = setTimeout(() => {
      if (pendingBrightness !== null) {
        sendInput(`b${pendingBrightness}`);
        pendingBrightness = null;
      }
      brightnessSendTimer = null;
    }, 40);
  }
}

function updateBrightnessDisplay(b, sendToArduino = true) {
  currentBrightness = Math.max(0, Math.min(255, Math.round(b)));
  if (brightnessSlider) brightnessSlider.value = currentBrightness;
  if (brightnessValue) {
    brightnessValue.textContent = `${Math.round((currentBrightness / 255) * 100)}% (${currentBrightness})`;
  }
  if (sendToArduino) throttledSendBrightness(currentBrightness);
}

// Wire Event Listeners
btnP1.addEventListener("click", triggerP1);
btnP2.addEventListener("click", triggerP2);
btnToggleGame.addEventListener("click", triggerGameToggle);
btnCycleMode.addEventListener("click", triggerCycleMode);
btnCycleGame.addEventListener("click", triggerCycleGame);

lightKnobSlider.addEventListener("input", (e) => {
  const val = parseInt(e.target.value, 10);
  activeModeIdx = val;
  modeTitle.textContent = LIGHT_MODE_NAMES[val];
  lightKnobText.textContent = `Mode ${val + 1}: ${LIGHT_MODE_NAMES[val].split(" ")[0]}`;
  sendInput(`m${val}`);
});

gameKnobSlider.addEventListener("input", (e) => {
  const val = parseInt(e.target.value, 10);
  activeGameIdx = val;
  gameTitle.textContent = GAME_NAMES[val];
  gameKnobText.textContent = `Game ${val + 1}: ${GAME_NAMES[val].split(" ")[0]}`;
  sendInput(`k${val}`);
});

hueSlider.addEventListener("input", (e) => {
  updateMoodColorDisplay(parseFloat(e.target.value), true);
});

brightnessSlider.addEventListener("input", (e) => {
  updateBrightnessDisplay(parseInt(e.target.value, 10), true);
});

// Keyboard Shortcuts: A = P1, L = P2, G = Game/Challenge, M = Mode, K = Game Cycle, C = Color, B = Brightness
window.addEventListener("keydown", (e) => {
  if (e.repeat) return;
  const key = e.key.toLowerCase();
  if (key === "a") triggerP1();
  else if (key === "l") triggerP2();
  else if (key === "g") triggerGameToggle();
  else if (key === "m") triggerCycleMode();
  else if (key === "k") triggerCycleGame();
  else if (key === "c") {
    const next = currentHueDeg + 22;
    updateMoodColorDisplay(next >= 360 ? 375 : next, true);
  } else if (key === "b") {
    const nextB = currentBrightness >= 255 ? 25 : currentBrightness + 25;
    updateBrightnessDisplay(nextB, true);
  }
});

// Optical Customization
diffuserSelect.addEventListener("change", (e) => {
  diffuserOverlay.className = `diffuser-overlay ${e.target.value}`;
});

shelfTextureSelect.addEventListener("change", (e) => {
  shelfBackdrop.className = `shelf-backdrop ${e.target.value}`;
});

bloomSlider.addEventListener("input", (e) => {
  bloomIntensity = e.target.value / 100.0;
});

// Initialize
updateMoodColorDisplay(0, false);
updateBrightnessDisplay(255, false);
setView("full");

// Start Render & Polling Loop
requestAnimationFrame(fetchFrame);
