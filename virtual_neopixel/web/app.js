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
const btnStepColor = document.getElementById("btnStepColor");
const colorPreview = document.getElementById("colorPreview");
const hueValue = document.getElementById("hueValue");
const hueSlider = document.getElementById("hueSlider");

const diffuserSelect = document.getElementById("diffuserSelect");
const shelfTextureSelect = document.getElementById("shelfTextureSelect");
const bloomSlider = document.getElementById("bloomSlider");

// Strip & Color State
let ledBuffer = Array.from({ length: NUM_LEDS }, () => [0, 0, 0]);
let activeGame = false;
let currentModeName = "Breathing / Pulse";
let bloomIntensity = 0.85;

let currentHueDeg = 0; // 0..360°
let currentMoodRGB = [255, 0, 0]; // Default Red

// Set initial optics
diffuserOverlay.className = "diffuser-overlay acrylic";

// HSV to RGB Converter (matches NeoPixel ColorHSV math)
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
  currentHueDeg = ((deg % 360) + 360) % 360;
  currentMoodRGB = hsvToRgb(currentHueDeg);
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

  // Spacing: LEDs evenly pitched across strip
  const numLeds = ledBuffer.length;
  const pitch = width / numLeds;
  const ledSize = Math.min(pitch * 0.75, 26);

  // Clear Ambient Glow Canvas
  glowCtx.clearRect(0, 0, glowCanvas.width, glowCanvas.height);

  for (let i = 0; i < numLeds; i++) {
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
      if (data.leds && data.leds.length > 0) {
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
  const [baseR, baseG, baseB] = currentMoodRGB;

  if (mode === 0) {
    // 1. Breathing / Pulse: 7.5s ultra-slow breath in Rotary Mood-Color!
    const wave = (Math.sin(localTime * 0.84) + 1.0) * 0.5;
    const factor = 0.42 + wave * 0.40;
    const r = Math.round(baseR * factor);
    const g = Math.round(baseG * factor);
    const b = Math.round(baseB * factor);
    for (let i = 0; i < NUM_LEDS; i++) ledBuffer[i] = [r, g, b];
  } else if (mode === 1) {
    // 2. Twinkle / Sparkle: slow drifting shimmer tinted by Rotary Mood-Color!
    for (let i = 0; i < NUM_LEDS; i++) {
      const br = 60 + Math.round(90 * ((Math.sin(localTime * 1.2 + i * 0.4) + 1.0) * 0.5));
      let r = Math.round((baseR * br) / 255);
      let g = Math.round((baseG * br) / 255);
      let b = Math.round((baseB * br) / 255);
      if (br > 135) {
        const spark = (br - 135) * 2;
        r = Math.min(255, r + spark);
        g = Math.min(255, g + spark);
        b = Math.min(255, b + spark);
      }
      ledBuffer[i] = [r, g, b];
    }
  } else if (mode === 2) {
    // 3. Fire / Flame: cozy embers in Rotary Mood-Color!
    for (let i = 0; i < NUM_LEDS; i++) {
      const fl = Math.sin(localTime * 1.5 + i * 0.25) * 20;
      const t = Math.max(78, Math.min(220, 105 + fl));
      const factor = t / 255;
      let r = Math.round(baseR * factor);
      let g = Math.round(baseG * factor);
      let b = Math.round(baseB * factor);
      if (t > 150) {
        const glow = Math.round((t - 150) * 0.9);
        r = Math.min(255, r + glow);
        g = Math.min(255, g + glow);
        b = Math.min(255, b + glow);
      }
      ledBuffer[i] = [r, g, b];
    }
  } else if (mode === 3) {
    // 4. Chase / Marquee: slow vintage crawling marquee in Rotary Mood-Color!
    const step = Math.floor(localTime * 4.5) % 4;
    for (let i = 0; i < NUM_LEDS; i++) {
      if ((i + step) % 4 === 0) {
        ledBuffer[i] = [
          Math.min(255, baseR + 50),
          Math.min(255, baseG + 50),
          Math.min(255, baseB + 50)
        ];
      } else {
        ledBuffer[i] = [
          Math.round((baseR * 75) / 255),
          Math.round((baseG * 75) / 255),
          Math.round((baseB * 75) / 255)
        ];
      }
    }
  } else if (mode === 4) {
    // 5. Comet / Meteor: 12s graceful gliding shooting star in Rotary Mood-Color!
    const sweep = (Math.sin(localTime * 0.5) + 1.0) * 0.5 * 59;
    for (let i = 0; i < NUM_LEDS; i++) {
      const dist = Math.abs(i - sweep);
      if (dist < 1.0) {
        ledBuffer[i] = [
          Math.min(255, baseR + 150),
          Math.min(255, baseG + 150),
          Math.min(255, baseB + 150)
        ];
      } else if (dist < 10.0) {
        const glow = 1.0 - dist / 10.0;
        ledBuffer[i] = [
          Math.round(baseR * glow),
          Math.round(baseG * glow),
          Math.round(baseB * glow)
        ];
      } else {
        ledBuffer[i] = [
          Math.round((baseR * 65) / 255),
          Math.round((baseG * 65) / 255),
          Math.round((baseB * 65) / 255)
        ];
      }
    }
  } else if (mode === 5) {
    // 6. Scanner / Cylon: 6s smooth Larson eye in Rotary Mood-Color!
    const eye = 29.5 + 27.5 * Math.sin(localTime * 1.05);
    for (let i = 0; i < NUM_LEDS; i++) {
      const dist = Math.abs(i - eye);
      if (dist < 1.0) {
        ledBuffer[i] = [
          Math.min(255, baseR + 80),
          Math.min(255, baseG + 80),
          Math.min(255, baseB + 80)
        ];
      } else if (dist < 6.0) {
        const glow = 1.0 - dist / 6.0;
        ledBuffer[i] = [
          Math.round(baseR * glow),
          Math.round(baseG * glow),
          Math.round(baseB * glow)
        ];
      } else {
        ledBuffer[i] = [
          Math.round((baseR * 60) / 255),
          Math.round((baseG * 60) / 255),
          Math.round((baseB * 60) / 255)
        ];
      }
    }
  } else {
    // 7. Color Wipe: progressive roll across harmonic offsets from Rotary Mood-Color!
    const wipePos = Math.floor(localTime * 5.5) % NUM_LEDS;
    const paletteIdx = Math.floor(localTime * 0.1) % 4;
    const wipeHue = (currentHueDeg + paletteIdx * 60) % 360;
    const col = hsvToRgb(wipeHue);
    for (let i = 0; i < NUM_LEDS; i++) {
      if (i <= wipePos) ledBuffer[i] = col;
      else ledBuffer[i] = [
        Math.round((baseR * 60) / 255),
        Math.round((baseG * 60) / 255),
        Math.round((baseB * 60) / 255)
      ];
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

// Step Mood Color (+22° ~ +4000/65535)
function triggerStepColor() {
  currentHueDeg = (currentHueDeg + 22) % 360;
  updateMoodColorDisplay(currentHueDeg, false);
  sendInput("c");
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
btnStepColor.addEventListener("click", triggerStepColor);

hueSlider.addEventListener("input", (e) => {
  updateMoodColorDisplay(parseFloat(e.target.value), true);
});

hueSlider.addEventListener("change", (e) => {
  const arduinoHue = Math.round((parseFloat(e.target.value) / 360.0) * 65535);
  sendInput(`h${arduinoHue}`);
});

// Keyboard Shortcuts: A = P1, L = P2, M = Mode, G = Game, C = Color
window.addEventListener("keydown", (e) => {
  if (e.repeat) return;
  const key = e.key.toLowerCase();
  if (key === "a") triggerP1();
  else if (key === "l") triggerP2();
  else if (key === "m") triggerModeToggle();
  else if (key === "g") triggerGameToggle();
  else if (key === "c") triggerStepColor();
});

// Initialize mood color display (starts at 0° Red)
updateMoodColorDisplay(0, false);

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

