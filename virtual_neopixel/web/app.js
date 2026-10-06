/* ============================================================================
   Virtual NeoPixel Strip (60 LEDs/m) - Optical Engine & Client Application
   ============================================================================ */

const NUM_LEDS = 60;

// Canvas Elements
const stripCanvas = document.getElementById("stripCanvas");
const stripCtx = stripCanvas.getContext("2d");
const glowCanvas = document.getElementById("ambientGlowCanvas");
const glowCtx = glowCanvas.getContext("2d");

// DOM Controls
const statusIndicator = document.getElementById("statusIndicator");
const sourceValue = document.getElementById("sourceValue");
const fpsValue = document.getElementById("fpsValue");
const luxValue = document.getElementById("luxValue");
const guardStatus = document.getElementById("guardStatus");
const modeTitle = document.getElementById("modeTitle");
const gameInstruction = document.getElementById("gameInstruction");
const diffuserOverlay = document.getElementById("diffuserOverlay");
const shelfBackdrop = document.getElementById("shelfBackdrop");

const btnP1 = document.getElementById("btnP1");
const btnP2 = document.getElementById("btnP2");
const btnToggleMode = document.getElementById("btnToggleMode");
const btnToggleGame = document.getElementById("btnToggleGame");

const diffuserSelect = document.getElementById("diffuserSelect");
const shelfTextureSelect = document.getElementById("shelfTextureSelect");
const bloomSlider = document.getElementById("bloomSlider");

// Strip State
let ledBuffer = Array.from({ length: NUM_LEDS }, () => [0, 0, 0]);
let activeGame = false;
let currentModeName = "Breathing / Pulse";
let bloomIntensity = 0.85;

// Set initial optics
diffuserOverlay.className = "diffuser-overlay acrylic";

// ----------------------------------------------------------------------------
// 1. Photorealistic 60-LED NeoPixel Rendering Pipeline
// ----------------------------------------------------------------------------
function drawStrip() {
  const width = stripCanvas.width;
  const height = stripCanvas.height;

  // Clear Strip PCB
  stripCtx.fillStyle = "#111317"; // Black flexible PCB background
  stripCtx.fillRect(0, 0, width, height);

  // PCB Traces (Copper solder pads & power bus lines)
  stripCtx.strokeStyle = "#272b35";
  stripCtx.lineWidth = 2;
  stripCtx.beginPath();
  stripCtx.moveTo(0, 12);
  stripCtx.lineTo(width, 12);
  stripCtx.moveTo(0, height - 12);
  stripCtx.lineTo(width, height - 12);
  stripCtx.stroke();

  // Spacing: 60 LEDs evenly pitched
  const pitch = width / NUM_LEDS;
  const ledSize = Math.min(pitch * 0.75, 26);

  // Clear Ambient Glow Canvas
  glowCtx.clearRect(0, 0, glowCanvas.width, glowCanvas.height);

  for (let i = 0; i < NUM_LEDS; i++) {
    const [r, g, b] = ledBuffer[i];
    const centerX = i * pitch + pitch / 2;
    const centerY = height / 2;

    // --- A. Draw PCB Solder Pads (+5V, Din, Dout, GND) ---
    stripCtx.fillStyle = "#a8783e"; // Copper pad
    stripCtx.fillRect(centerX - ledSize / 2 - 3, centerY - ledSize / 2, 2, ledSize);
    stripCtx.fillRect(centerX + ledSize / 2 + 1, centerY - ledSize / 2, 2, ledSize);

    // --- B. Draw 5050 SMD Package (White square casing with beveled notch) ---
    stripCtx.fillStyle = "#e5e7eb";
    stripCtx.fillRect(centerX - ledSize / 2, centerY - ledSize / 2, ledSize, ledSize);

    // Pin 1 orientation corner notch
    stripCtx.fillStyle = "#9ca3af";
    stripCtx.beginPath();
    stripCtx.moveTo(centerX - ledSize / 2, centerY - ledSize / 2);
    stripCtx.lineTo(centerX - ledSize / 2 + 4, centerY - ledSize / 2);
    stripCtx.lineTo(centerX - ledSize / 2, centerY - ledSize / 2 + 4);
    stripCtx.closePath();
    stripCtx.fill();

    // Internal circular phosphor dome
    stripCtx.fillStyle = "#d1d5db";
    stripCtx.beginPath();
    stripCtx.arc(centerX, centerY, ledSize * 0.38, 0, Math.PI * 2);
    stripCtx.fill();

    // --- C. Draw RGB Sub-Emitter Die Emission ---
    stripCtx.fillStyle = `rgb(${r}, ${g}, ${b})`;
    stripCtx.beginPath();
    stripCtx.arc(centerX, centerY, ledSize * 0.32, 0, Math.PI * 2);
    stripCtx.fill();

    // White core hot-spot if bright
    const luminance = 0.299 * r + 0.587 * g + 0.114 * b;
    if (luminance > 140) {
      const coreAlpha = (luminance - 140) / 115;
      stripCtx.fillStyle = `rgba(255, 255, 255, ${coreAlpha * 0.6})`;
      stripCtx.beginPath();
      stripCtx.arc(centerX, centerY, ledSize * 0.15, 0, Math.PI * 2);
      stripCtx.fill();
    }

    // --- D. Draw Ambient Radiance / Radial Bloom onto Shelf Wall ---
    if (luminance > 15 && bloomIntensity > 0) {
      const glowRadius = Math.max(20, (luminance / 255) * 110 * bloomIntensity);
      const glowAlpha = Math.min(0.85, (luminance / 255) * 0.75 * bloomIntensity);

      const radialGrad = glowCtx.createRadialGradient(
        centerX, 140, 4,
        centerX, 140, glowRadius
      );
      radialGrad.addColorStop(0, `rgba(${r}, ${g}, ${b}, ${glowAlpha})`);
      radialGrad.addColorStop(0.4, `rgba(${r}, ${g}, ${b}, ${glowAlpha * 0.4})`);
      radialGrad.addColorStop(1, "rgba(0, 0, 0, 0)");

      glowCtx.fillStyle = radialGrad;
      glowCtx.beginPath();
      glowCtx.arc(centerX, 140, glowRadius, 0, Math.PI * 2);
      glowCtx.fill();
    }
  }
}

// ----------------------------------------------------------------------------
// 2. Real-Time Telemetry & Frame Polling from bridge.py
// ----------------------------------------------------------------------------
let lastFrameTime = performance.now();
let framesReceived = 0;
let isConnected = false;

async function fetchFrame() {
  try {
    const res = await fetch("/api/frame", { cache: "no-store" });
    if (res.ok) {
      const data = await res.json();
      if (data.leds && data.leds.length === NUM_LEDS) {
        ledBuffer = data.leds;
      }
      fpsValue.textContent = `${data.fps} FPS`;
      luxValue.textContent = `${data.luminance}%`;
      sourceValue.textContent = data.source || "Connected";

      // Guard check: luminance >= 35%
      if (data.luminance >= 33.0) {
        guardStatus.textContent = "SAFE (>=35%)";
        guardStatus.style.color = "#10b981";
      } else {
        guardStatus.textContent = "WARN (<35%)";
        guardStatus.style.color = "#f59e0b";
      }

      if (!isConnected) {
        isConnected = true;
        statusIndicator.classList.remove("offline");
      }
    }
  } catch (err) {
    // If bridge server is offline, fallback to standalone simulation
    if (isConnected) {
      isConnected = false;
      statusIndicator.classList.add("offline");
      sourceValue.textContent = "Offline (Local Demo)";
    }
    runLocalDemoFallback();
  }

  drawStrip();
  requestAnimationFrame(fetchFrame);
}

// Local fallback simulation if running index.html directly without bridge.py
let localTime = 0;
let localModeIdx = 0;
let lastModeSwitch = performance.now();
const heatMap = Array.from({ length: NUM_LEDS }, () => 85);
let wipeIndex = 0;
let wipePalette = 0;
const WIPES = [[255, 160, 40], [147, 51, 234], [14, 165, 233], [225, 29, 72]];

function runLocalDemoFallback() {
  const now = performance.now();
  localTime += 0.012; // Very slow time step

  if (now - lastModeSwitch > 15000) { // Switch mode every 15s in demo
    localModeIdx = (localModeIdx + 1) % modes.length;
    lastModeSwitch = now;
    modeIndex = localModeIdx;
    modeTitle.textContent = modes[modeIndex].title;
    if (activeGame) {
      gameInstruction.textContent = modes[modeIndex].game;
    }
  }

  const mode = localModeIdx;

  if (mode === 0) {
    // 1. Breathing / Pulse: 7.5s ultra-slow breath
    const wave = (Math.sin(localTime * 0.84) + 1.0) * 0.5;
    const factor = 0.42 + wave * 0.40;
    const r = Math.round(255 * factor);
    const g = Math.round(148 * factor);
    const b = Math.round(38 * factor);
    for (let i = 0; i < NUM_LEDS; i++) ledBuffer[i] = [r, g, b];
  } else if (mode === 1) {
    // 2. Twinkle / Sparkle: slow drifting candle shimmer
    for (let i = 0; i < NUM_LEDS; i++) {
      const b = 60 + Math.round(90 * ((Math.sin(localTime * 1.2 + i * 0.4) + 1.0) * 0.5));
      ledBuffer[i] = [b, Math.round(b * 0.74), Math.round(b * 0.45)];
    }
  } else if (mode === 2) {
    // 3. Fire / Flame: slow cozy fireplace embers
    for (let i = 0; i < NUM_LEDS; i++) {
      const fl = Math.sin(localTime * 1.5 + i * 0.25) * 20;
      const t = Math.max(78, Math.min(220, 105 + fl));
      ledBuffer[i] = [Math.round(t), Math.round(t * 0.45), Math.round(t * 0.08)];
    }
  } else if (mode === 3) {
    // 4. Chase / Marquee: slow vintage crawling marquee (220ms step)
    const step = Math.floor(localTime * 4.5) % 4;
    for (let i = 0; i < NUM_LEDS; i++) {
      if ((i + step) % 4 === 0) ledBuffer[i] = [255, 190, 60];
      else ledBuffer[i] = [80, 45, 12];
    }
  } else if (mode === 4) {
    // 5. Comet / Meteor: 12s graceful gliding shooting star
    const sweep = (Math.sin(localTime * 0.5) + 1.0) * 0.5 * 59;
    for (let i = 0; i < NUM_LEDS; i++) {
      const dist = Math.abs(i - sweep);
      if (dist < 1.0) ledBuffer[i] = [255, 255, 240];
      else if (dist < 8.0) {
        const glow = Math.round(255 * (1.0 - dist / 8.0));
        ledBuffer[i] = [glow, Math.round(glow * 0.65), 20];
      } else {
        ledBuffer[i] = [65, 38, 12];
      }
    }
  } else if (mode === 5) {
    // 6. Scanner / Cylon: 6s smooth Larson eye with cosine deceleration
    const eye = 29.5 + 27.5 * Math.sin(localTime * 1.05);
    for (let i = 0; i < NUM_LEDS; i++) {
      const dist = Math.abs(i - eye);
      if (dist < 1.0) ledBuffer[i] = [255, 170, 40];
      else if (dist < 6.0) {
        const glow = Math.round(255 * (1.0 - dist / 6.0));
        ledBuffer[i] = [glow, Math.round(glow * 0.24), 10];
      } else {
        ledBuffer[i] = [60, 32, 10];
      }
    }
  } else {
    // 7. Color Wipe: slow progressive chromatic roll
    const wipePos = Math.floor(localTime * 5.5) % NUM_LEDS;
    const col = WIPES[Math.floor(localTime * 0.1) % WIPES.length];
    for (let i = 0; i < NUM_LEDS; i++) {
      if (i <= wipePos) ledBuffer[i] = col;
      else ledBuffer[i] = [70, 40, 15];
    }
  }

  luxValue.textContent = "42%";
  fpsValue.textContent = "60 FPS (Demo)";
}

// ----------------------------------------------------------------------------
// 3. User Controls & Arduino Key Relay
// ----------------------------------------------------------------------------
async function sendInput(key) {
  try {
    await fetch(`/api/input?key=${encodeURIComponent(key)}`);
  } catch (e) {
    // Ignore network error if in standalone preview
  }
}

// Player 1 Button Press
function triggerP1() {
  btnP1.classList.add("pressed");
  setTimeout(() => btnP1.classList.remove("pressed"), 120);
  sendInput("1");
}

// Player 2 Button Press
function triggerP2() {
  btnP2.classList.add("pressed");
  setTimeout(() => btnP2.classList.remove("pressed"), 120);
  sendInput("2");
}

// 7 Modes Configuration
const modes = [
  { title: "Breathing / Pulse", game: "Resonance Pulse (Tap at the peak of the breath!)" },
  { title: "Twinkle / Sparkle", game: "Sparkle Rush (Reflect the Nova in your zone!)" },
  { title: "Fire / Flame", game: "Flame Tug (Pump bellows with steady 3-4 Hz rhythm!)" },
  { title: "Chase / Marquee", game: "Marquee Intercept (Lock the rotating dot in your zone!)" },
  { title: "Comet / Meteor", game: "Meteor Deflector (Smash the comet back at your goal line!)" },
  { title: "Scanner / Cylon", game: "Cylon Clash (Return the hyper-fast laser beam!)" },
  { title: "Color Wipe", game: "Territory Paint (Rapid tap to wipe & paint the strip!)" }
];
let modeIndex = 0;

function triggerModeToggle() {
  modeIndex = (modeIndex + 1) % modes.length;
  currentModeName = modes[modeIndex].title;
  modeTitle.textContent = currentModeName;
  if (activeGame) {
    gameInstruction.textContent = modes[modeIndex].game;
  }
  sendInput("m");
}

// Toggle Game
function triggerGameToggle() {
  activeGame = !activeGame;
  if (activeGame) {
    btnToggleGame.classList.add("active-game");
    btnToggleGame.textContent = "Stop Game [G]";
    gameInstruction.textContent = modes[modeIndex].game;
  } else {
    btnToggleGame.classList.remove("active-game");
    btnToggleGame.textContent = "Start Game [G]";
    gameInstruction.textContent = "Double-click or press [G] to start competitive match!";
  }
  sendInput("g");
}

btnP1.addEventListener("click", triggerP1);
btnP2.addEventListener("click", triggerP2);
btnToggleMode.addEventListener("click", triggerModeToggle);
btnToggleGame.addEventListener("click", triggerGameToggle);

// Keyboard Shortcuts: A = P1, L = P2, M = Mode, G = Game
window.addEventListener("keydown", (e) => {
  if (e.repeat) return;
  const key = e.key.toLowerCase();
  if (key === "a") triggerP1();
  else if (key === "l") triggerP2();
  else if (key === "m") triggerModeToggle();
  else if (key === "g") triggerGameToggle();
});

// ----------------------------------------------------------------------------
// 4. Optical & Environment Customization
// ----------------------------------------------------------------------------
diffuserSelect.addEventListener("change", (e) => {
  diffuserOverlay.className = `diffuser-overlay ${e.target.value}`;
});

shelfTextureSelect.addEventListener("change", (e) => {
  shelfBackdrop.className = `shelf-backdrop ${e.target.value}`;
});

bloomSlider.addEventListener("input", (e) => {
  bloomIntensity = e.target.value / 100.0;
});

// Start loop
requestAnimationFrame(fetchFrame);

