#!/usr/bin/env python3
"""
===============================================================================
Virtual NeoPixel Strip Bridge & Linux Visualizer Host (12m / 720 LEDs)
===============================================================================
Bridges binary frame data from Arduino Uno R3 over USB Serial to an ultra-realistic
HTML5/Canvas visualizer running in your browser under Linux (Wayland & X11).

Protocol (from Arduino Uno FastLED implementation):
  Extended Protocol (CMD 0x02):
    Header:   0xAA 0x55 0x02 [COUNT_HI] [COUNT_LO] (NUM_LEDS=720)
    GamerLEDs: 6 bytes (Gamer A R,G,B; Gamer B R,G,B)
    Payload:  2160 bytes (RGB RGB ... for 720 LEDs)
    Checksum: XOR of all header + gamer LEDs + payload bytes

  Legacy Protocol (CMD 0x01):
    Header:   0xAA 0x55 0x01 [NUM_LEDS (1 byte)]
    Payload:  NUM_LEDS * 3 bytes
    Checksum: XOR of all header + payload bytes

Usage:
  python3 bridge.py              # Auto-detects /dev/ttyACM0 or /dev/ttyUSB0
  python3 bridge.py --port /dev/ttyACM0 --baud 115200
  python3 bridge.py --demo       # Runs built-in 720-LED simulated frame generator
===============================================================================
"""

import sys
import os
import time
import glob
import json
import math
import random
import threading
from http.server import HTTPServer, SimpleHTTPRequestHandler
import urllib.parse

# Attempt to import pyserial
try:
    import serial
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

HTTP_PORT = 8080
SERIAL_BAUD = 115200
NUM_LEDS = 720  # 12-meter SK6812 strip (60 LEDs/m)
WEB_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "web")

# Defined Gamer Colors (matching Arduino barcounter_light.ino)
COLOR_GAMER_A = [0, 180, 255]     # Electric Cyan (Station A, Pin 2/3)
COLOR_GAMER_B = [255, 30, 70]     # Neon Coral Red (Station B, Pin 4/5)
COLOR_GAMER_ROBOT = [255, 170, 0] # Neon Gold / Amber (RobotGamer NPC)

LIGHT_MODE_NAMES = [
    "Breathing / Pulse",
    "Twinkle / Sparkle",
    "Fire / Flame",
    "Comet / Meteor",
    "Scanner / Cylon",
    "Color Wipe"
]

GAME_NAMES = [
    "Resonance Pulse",
    "Sparkle Rush",
    "Flame Tug",
    "Meteor Deflector",
    "Cylon Clash",
    "Territory Paint"
]

# Global frame buffer shared with web clients
current_frame = {
    "num_leds": NUM_LEDS,
    "leds": [[0, 0, 0] for _ in range(NUM_LEDS)],
    "gamer_a": [0, 180, 255],
    "gamer_b": [255, 30, 70],
    "scores": [0, 0],
    "state": "idle_ambient",
    "challenger": "none",
    "against_robot": False,
    "active_mode_idx": 0,
    "active_mode_name": LIGHT_MODE_NAMES[0],
    "active_game_idx": 0,
    "active_game_name": GAME_NAMES[0],
    "fps": 0,
    "luminance": 0,
    "timestamp": time.time(),
    "source": "Waiting for Arduino..."
}
frame_lock = threading.Lock()
serial_handle = None


def auto_detect_serial_port():
    """Auto-detect common Arduino USB serial device ports on Linux."""
    ports = glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")
    if ports:
        return sorted(ports)[0]
    return None


def calculate_luminance(pixels):
    """Calculate average perceptual luminance across all LEDs (0-100%)."""
    if not pixels:
        return 0.0
    total = sum((0.299 * r + 0.587 * g + 0.114 * b) for r, g, b in pixels)
    avg = total / len(pixels)
    return round((avg / 255.0) * 100.0, 1)


def read_serial_loop(port_name, baud_rate):
    """Reads binary frames from Arduino Uno R3 supporting both CMD 0x02 (extended) and 0x01 (legacy)."""
    global current_frame, serial_handle

    while True:
        try:
            print(f"[Serial] Connecting to Arduino Uno at {port_name} ({baud_rate} baud)...")
            ser = serial.Serial(port_name, baud_rate, timeout=1.0)
            serial_handle = ser
            print(f"[Serial] Successfully connected to {port_name}!")
            time.sleep(1.8)  # Allow Arduino reset sequence

            frame_count = 0
            fps_start_time = time.time()
            current_fps = 0

            while True:
                # 1. Search for sync header: 0xAA 0x55
                b = ser.read(1)
                if not b or b != b'\xAA':
                    continue
                b2 = ser.read(1)
                if not b2 or b2 != b'\x55':
                    continue

                # 2. Read Command Byte
                cmd_byte = ser.read(1)
                if not cmd_byte:
                    continue
                cmd = cmd_byte[0]

                if cmd == 0x02:
                    # Extended Protocol (16-bit count + dedicated gamer LEDs)
                    # Next 8 bytes: count_hi, count_lo, gamerA(3), gamerB(3)
                    header_rest = ser.read(8)
                    if len(header_rest) < 8:
                        continue
                    count = (header_rest[0] << 8) | header_rest[1]
                    gamer_a = [header_rest[2], header_rest[3], header_rest[4]]
                    gamer_b = [header_rest[5], header_rest[6], header_rest[7]]

                    payload_len = count * 3
                    payload = ser.read(payload_len)
                    if len(payload) < payload_len:
                        continue

                    chk = ser.read(1)
                    if not chk:
                        continue
                    expected_chk = chk[0]

                    # Verify XOR Checksum
                    calc_chk = 0xAA ^ 0x55 ^ cmd
                    for hb in header_rest:
                        calc_chk ^= hb
                    for pb in payload:
                        calc_chk ^= pb

                    if calc_chk != expected_chk:
                        continue

                    # Decode RGB pixels
                    pixels = []
                    for i in range(0, payload_len, 3):
                        pixels.append([payload[i], payload[i + 1], payload[i + 2]])

                    frame_count += 1
                    now = time.time()
                    if now - fps_start_time >= 1.0:
                        current_fps = frame_count / (now - fps_start_time)
                        frame_count = 0
                        fps_start_time = now

                    with frame_lock:
                        current_frame["num_leds"] = len(pixels)
                        current_frame["leds"] = pixels
                        current_frame["gamer_a"] = gamer_a
                        current_frame["gamer_b"] = gamer_b
                        current_frame["fps"] = round(current_fps, 1)
                        current_frame["luminance"] = calculate_luminance(pixels)
                        current_frame["timestamp"] = now
                        current_frame["source"] = f"Arduino Uno ({port_name})"

                elif cmd == 0x01:
                    # Legacy Protocol (8-bit count)
                    meta = ser.read(1)
                    if not meta:
                        continue
                    count = meta[0]
                    payload_len = count * 3
                    payload = ser.read(payload_len)
                    if len(payload) < payload_len:
                        continue

                    chk = ser.read(1)
                    if not chk:
                        continue
                    expected_chk = chk[0]

                    calc_chk = 0xAA ^ 0x55 ^ cmd ^ count
                    for pb in payload:
                        calc_chk ^= pb

                    if calc_chk != expected_chk:
                        continue

                    pixels = []
                    for i in range(0, payload_len, 3):
                        pixels.append([payload[i], payload[i + 1], payload[i + 2]])

                    frame_count += 1
                    now = time.time()
                    if now - fps_start_time >= 1.0:
                        current_fps = frame_count / (now - fps_start_time)
                        frame_count = 0
                        fps_start_time = now

                    with frame_lock:
                        current_frame["num_leds"] = len(pixels)
                        current_frame["leds"] = pixels
                        current_frame["fps"] = round(current_fps, 1)
                        current_frame["luminance"] = calculate_luminance(pixels)
                        current_frame["timestamp"] = now
                        current_frame["source"] = f"Arduino Uno ({port_name})"

        except Exception as e:
            print(f"[Serial Error] {e}. Retrying in 2 seconds...")
            serial_handle = None
            time.sleep(2.0)


# =============================================================================
# Local 720-LED Simulation Engine (Full 12-Meter Bar Counter & Arcade Games)
# =============================================================================
demo_state = {
    "mode_idx": 0,
    "game_idx": 0,
    "hue": 0,          # 0..65535 matching Arduino RotaryColorKnob
    "is_white": False, # White at end of rainbow selection
    "brightness": 255, # 0..255 matching Arduino RotaryBrightnessKnob
    "system_state": "idle_ambient", # idle_ambient, challenge, playing, match_over
    "challenger": "none",           # none, player_a, player_b
    "against_robot": False,
    "challenge_blink_count": 0,
    "challenge_blink_state": False,
    "last_challenge_blink_time": 0.0,
    "scores": [0, 0],
    "win_score": 5,
    "round_over": False,
    "round_winner": 0, # 0 for Side A, 1 for Side B
    "round_over_time": 0.0,
    "match_over_time": 0.0,
    "last_press_time_a": 0.0,
    "last_press_time_b": 0.0,
    # Game Physics variables (scaled across 720 LEDs)
    "resonance_pos": 360,
    "nova_pos": 360.0,
    "nova_vel": 8.0,
    "flame_pos": 360,
    "meteor_pos": 360.0,
    "meteor_vel": 7.5,
    "cylon_pos": 360.0,
    "cylon_vel": 9.0,
    "paint_boundary": 360,
    "robot_last_act_time": 0.0
}


def hsv_to_rgb(hue_16, sat=255, val=255):
    """Converts 16-bit hue (0..65535) to RGB, matching NeoPixel Driver ColorHSV math."""
    h_idx = ((hue_16 // 10922) % 6)
    rem = hue_16 % 10922
    base = ((255 - sat) * val) >> 8
    if h_idx == 0:
        r, g, b = val, (((val - base) * rem) // 10922) + base, base
    elif h_idx == 1:
        r, g, b = (((val - base) * (10922 - rem)) // 10922) + base, val, base
    elif h_idx == 2:
        r, g, b = base, val, (((val - base) * rem) // 10922) + base
    elif h_idx == 3:
        r, g, b = base, (((val - base) * (10922 - rem)) // 10922) + base, val
    elif h_idx == 4:
        r, g, b = (((val - base) * rem) // 10922) + base, base, val
    else:
        r, g, b = val, base, (((val - base) * (10922 - rem)) // 10922) + base
    return [max(0, min(255, r)), max(0, min(255, g)), max(0, min(255, b))]


def constant_brightness_rgb(hue):
    """Calculates constant perceived brightness RGB across all hues (matching Arduino)."""
    sector = (hue // 10922) % 6
    frac = hue % 10922
    p = 0
    q = (255 * (10922 - frac)) // 10922
    t = (255 * frac) // 10922
    v = 255
    if sector == 0: r, g, b = v, t, p
    elif sector == 1: r, g, b = q, v, p
    elif sector == 2: r, g, b = p, v, t
    elif sector == 3: r, g, b = p, q, v
    elif sector == 4: r, g, b = t, p, v
    else: r, g, b = v, p, q

    p_bright = math.sqrt(0.299 * r * r + 0.587 * g * g + 0.114 * b * b)
    if p_bright > 0.001:
        scale = 85.8 / p_bright
        r = min(255, int(r * scale + 0.5))
        g = min(255, int(g * scale + 0.5))
        b = min(255, int(b * scale + 0.5))
    return [r, g, b]


def trigger_demo_button_press(side):
    """Simulates button press for Side A (0) or Side B (1) in demo mode."""
    global demo_state
    now = time.time()
    if side == 0:
        demo_state["last_press_time_a"] = now
    else:
        demo_state["last_press_time_b"] = now

    state = demo_state["system_state"]

    if state == "idle_ambient":
        # Start challenge!
        demo_state["system_state"] = "challenge"
        demo_state["challenger"] = "player_a" if side == 0 else "player_b"
        demo_state["challenge_blink_count"] = 0
        demo_state["challenge_blink_state"] = True
        demo_state["last_challenge_blink_time"] = now
        demo_state["scores"] = [0, 0]
        demo_state["round_over"] = False
        print(f"[Demo] Challenge initiated by {demo_state['challenger'].upper()}!")

    elif state == "challenge":
        challenger = demo_state["challenger"]
        if (challenger == "player_a" and side == 1) or (challenger == "player_b" and side == 0):
            # Accepted by human!
            demo_state["system_state"] = "playing"
            demo_state["against_robot"] = False
            demo_state["scores"] = [0, 0]
            demo_state["round_over"] = False
            reset_demo_game_physics()
            print("[Demo] Challenge accepted! Human duel starting (First to 5 points)!")
        elif (challenger == "player_a" and side == 0) or (challenger == "player_b" and side == 1):
            # Challenger pressed again -> cancel
            demo_state["system_state"] = "idle_ambient"
            print("[Demo] Challenge cancelled by challenger.")

    elif state == "playing":
        if demo_state["round_over"]:
            return  # Cooldown between rounds

        game = demo_state["game_idx"]
        # Game-specific input handling
        if game == 0:
            # Resonance Pulse: tap during peak breath
            breath_phase = (now * 0.84) % (2.0 * math.pi)
            in_resonance = math.sin(breath_phase) >= 0.76
            if in_resonance:
                if side == 0:
                    demo_state["resonance_pos"] += 48
                else:
                    demo_state["resonance_pos"] -= 48
                if demo_state["resonance_pos"] >= 680:
                    award_demo_point(0)
                elif demo_state["resonance_pos"] <= 40:
                    award_demo_point(1)

        elif game == 1:
            # Sparkle Rush: deflect Nova inside defense zone (108 LEDs)
            pos = demo_state["nova_pos"]
            if side == 0 and pos <= 108 and demo_state["nova_vel"] < 0:
                demo_state["nova_vel"] = abs(demo_state["nova_vel"]) * 1.12
            elif side == 1 and pos >= (NUM_LEDS - 108) and demo_state["nova_vel"] > 0:
                demo_state["nova_vel"] = -abs(demo_state["nova_vel"]) * 1.12

        elif game == 2:
            # Flame Tug: mashing advances forge by 24 LEDs
            if side == 0:
                demo_state["flame_pos"] += 24
            else:
                demo_state["flame_pos"] -= 24
            if demo_state["flame_pos"] >= 660:
                award_demo_point(0)
            elif demo_state["flame_pos"] <= 60:
                award_demo_point(1)

        elif game == 3:
            # Meteor Deflector: deflect meteor inside defense sector (108 LEDs)
            pos = demo_state["meteor_pos"]
            if side == 0 and pos <= 108 and demo_state["meteor_vel"] < 0:
                demo_state["meteor_vel"] = abs(demo_state["meteor_vel"]) * 1.14
            elif side == 1 and pos >= (NUM_LEDS - 108) and demo_state["meteor_vel"] > 0:
                demo_state["meteor_vel"] = -abs(demo_state["meteor_vel"]) * 1.14

        elif game == 4:
            # Cylon Clash: return puck inside defense zone
            pos = demo_state["cylon_pos"]
            if side == 0 and pos <= 100 and demo_state["cylon_vel"] < 0:
                demo_state["cylon_vel"] = abs(demo_state["cylon_vel"]) * 1.15
            elif side == 1 and pos >= (NUM_LEDS - 100) and demo_state["cylon_vel"] > 0:
                demo_state["cylon_vel"] = -abs(demo_state["cylon_vel"]) * 1.15

        elif game == 5:
            # Territory Paint: rapid taps advance paint boundary by 24 LEDs
            if side == 0:
                demo_state["paint_boundary"] += 24
            else:
                demo_state["paint_boundary"] -= 24
            if demo_state["paint_boundary"] >= (NUM_LEDS - 10):
                award_demo_point(0)
            elif demo_state["paint_boundary"] <= 10:
                award_demo_point(1)


def reset_demo_game_physics():
    """Resets round physics for the active game."""
    global demo_state
    demo_state["resonance_pos"] = NUM_LEDS // 2
    demo_state["nova_pos"] = float(NUM_LEDS // 2)
    demo_state["nova_vel"] = 8.0 if random.random() < 0.5 else -8.0
    demo_state["flame_pos"] = NUM_LEDS // 2
    demo_state["meteor_pos"] = float(NUM_LEDS // 2)
    demo_state["meteor_vel"] = 7.5 if random.random() < 0.5 else -7.5
    demo_state["cylon_pos"] = float(NUM_LEDS // 2)
    demo_state["cylon_vel"] = 9.0 if random.random() < 0.5 else -9.0
    demo_state["paint_boundary"] = NUM_LEDS // 2


def award_demo_point(winner_side):
    """Awards a round point to Side A (0) or Side B (1)."""
    global demo_state
    demo_state["round_over"] = True
    demo_state["round_winner"] = winner_side
    demo_state["round_over_time"] = time.time()
    demo_state["scores"][winner_side] += 1

    winner_name = "Player A" if winner_side == 0 else ("RobotGamer" if demo_state["against_robot"] else "Player B")
    print(f"[Demo Point] {winner_name} scores! Score: {demo_state['scores'][0]} - {demo_state['scores'][1]}")

    if demo_state["scores"][winner_side] >= demo_state["win_score"]:
        demo_state["system_state"] = "match_over"
        demo_state["match_over_time"] = time.time()
        print(f"[Demo Match Won] {winner_name} won the match! Triggering victory celebration.")


def run_demo_simulation_loop():
    """High-fidelity local simulation of all 6 ambient modes and 6 competitive games on 720 LEDs."""
    global current_frame, demo_state
    print("[Demo Mode] Initializing 12-meter (720 LEDs) simulation engine...")

    start_time = time.time()
    heat = [85] * NUM_LEDS
    twinkle_b = [random.randint(55, 200) for _ in range(NUM_LEDS)]
    twinkle_d = [random.choice([2, -2]) for _ in range(NUM_LEDS)]

    while True:
        now = time.time()
        elapsed = now - start_time

        # 1. State Machine Updates
        state = demo_state["system_state"]

        if state == "challenge":
            # Update challenge blink (250 ms toggle)
            if now - demo_state["last_challenge_blink_time"] >= 0.25:
                demo_state["last_challenge_blink_time"] = now
                demo_state["challenge_blink_state"] = not demo_state["challenge_blink_state"]
                if not demo_state["challenge_blink_state"]:
                    demo_state["challenge_blink_count"] += 1
                    if demo_state["challenge_blink_count"] >= 10:
                        # Timeout -> start against RobotGamer!
                        demo_state["system_state"] = "playing"
                        demo_state["against_robot"] = True
                        demo_state["scores"] = [0, 0]
                        demo_state["round_over"] = False
                        reset_demo_game_physics()
                        print("[Demo] Challenge timeout! RobotGamer has entered the duel!")

        elif state == "playing":
            # Check round reset timer (2.0s after point)
            if demo_state["round_over"]:
                if now - demo_state["round_over_time"] >= 2.0:
                    demo_state["round_over"] = False
                    reset_demo_game_physics()
            else:
                # Update RobotGamer AI moves
                if demo_state["against_robot"]:
                    robot_side = 1 if demo_state["challenger"] == "player_a" else 0
                    game = demo_state["game_idx"]
                    if now - demo_state["robot_last_act_time"] > 0.14:
                        if game == 0:
                            # Rhythm pulse: trigger near peak breath
                            phase = (now * 0.84) % (2.0 * math.pi)
                            if math.sin(phase) >= 0.82 and random.random() < 0.6:
                                trigger_demo_button_press(robot_side)
                                demo_state["robot_last_act_time"] = now
                        elif game == 1:
                            # Nova deflector
                            pos = demo_state["nova_pos"]
                            if (robot_side == 1 and pos >= 640 and demo_state["nova_vel"] > 0) or \
                               (robot_side == 0 and pos <= 80 and demo_state["nova_vel"] < 0):
                                trigger_demo_button_press(robot_side)
                                demo_state["robot_last_act_time"] = now
                        elif game == 2:
                            # Tug: rapid rhythmic mashing
                            if random.random() < 0.7:
                                trigger_demo_button_press(robot_side)
                                demo_state["robot_last_act_time"] = now
                        elif game == 3:
                            # Meteor deflector
                            pos = demo_state["meteor_pos"]
                            if (robot_side == 1 and pos >= 640 and demo_state["meteor_vel"] > 0) or \
                               (robot_side == 0 and pos <= 80 and demo_state["meteor_vel"] < 0):
                                trigger_demo_button_press(robot_side)
                                demo_state["robot_last_act_time"] = now
                        elif game == 4:
                            # Cylon clash
                            pos = demo_state["cylon_pos"]
                            if (robot_side == 1 and pos >= 635 and demo_state["cylon_vel"] > 0) or \
                               (robot_side == 0 and pos <= 85 and demo_state["cylon_vel"] < 0):
                                trigger_demo_button_press(robot_side)
                                demo_state["robot_last_act_time"] = now
                        elif game == 5:
                            # Territory paint: rapid strokes
                            if random.random() < 0.65:
                                trigger_demo_button_press(robot_side)
                                demo_state["robot_last_act_time"] = now

                # Update physics for movement games
                game = demo_state["game_idx"]
                if game == 1:
                    # Nova motion
                    demo_state["nova_pos"] += demo_state["nova_vel"]
                    if demo_state["nova_pos"] >= NUM_LEDS:
                        award_demo_point(0)
                    elif demo_state["nova_pos"] < 0:
                        award_demo_point(1)
                elif game == 3:
                    # Meteor motion
                    demo_state["meteor_pos"] += demo_state["meteor_vel"]
                    if demo_state["meteor_pos"] >= NUM_LEDS:
                        award_demo_point(0)
                    elif demo_state["meteor_pos"] < 0:
                        award_demo_point(1)
                elif game == 4:
                    # Cylon laser puck motion
                    demo_state["cylon_pos"] += demo_state["cylon_vel"]
                    if demo_state["cylon_pos"] >= NUM_LEDS:
                        award_demo_point(0)
                    elif demo_state["cylon_pos"] < 0:
                        award_demo_point(1)

        elif state == "match_over":
            if now - demo_state["match_over_time"] >= 4.0:
                demo_state["system_state"] = "idle_ambient"
                demo_state["scores"] = [0, 0]
                demo_state["round_over"] = False
                print("[Demo] Match celebration completed. Returning to ambient lighting.")

        # 2. Render Main Strip Frame (720 LEDs)
        if demo_state.get("is_white", False):
            base_r, base_g, base_b = [86, 86, 86]
        else:
            base_r, base_g, base_b = constant_brightness_rgb(demo_state["hue"])

        pixels = []

        if state == "match_over":
            # 4-second celebratory victory wave across all 720 LEDs in winner's color
            win_side = demo_state["round_winner"]
            if win_side == 0:
                win_col = COLOR_GAMER_A
            else:
                win_col = COLOR_GAMER_ROBOT if demo_state["against_robot"] else COLOR_GAMER_B

            wave = (math.sin((now - demo_state["match_over_time"]) * 10.0) + 1.0) * 0.5
            br_scale = 0.35 + wave * 0.65
            pr = int(win_col[0] * br_scale)
            pg = int(win_col[1] * br_scale)
            pb = int(win_col[2] * br_scale)
            pixels = [[pr, pg, pb] for _ in range(NUM_LEDS)]
            # Add sparkle victory glints
            for _ in range(25):
                idx = random.randint(0, NUM_LEDS - 1)
                pixels[idx] = [255, 255, 255]

        elif state == "playing":
            game = demo_state["game_idx"]
            col_a = COLOR_GAMER_A
            col_b = COLOR_GAMER_ROBOT if demo_state["against_robot"] else COLOR_GAMER_B

            if game == 0:
                # 1. Resonance Pulse
                wave = (math.sin(elapsed * 0.84) + 1.0) * 0.5
                is_res = wave >= 0.76
                bg_br = 0.25 + wave * 0.25
                for i in range(NUM_LEDS):
                    if i < demo_state["resonance_pos"]:
                        pixels.append([int(col_a[0] * bg_br), int(col_a[1] * bg_br), int(col_a[2] * bg_br)])
                    else:
                        pixels.append([int(col_b[0] * bg_br), int(col_b[1] * bg_br), int(col_b[2] * bg_br)])
                # Energy Nexus
                nexus = max(0, min(NUM_LEDS - 1, demo_state["resonance_pos"]))
                for off in range(-16, 17):
                    idx = nexus + off
                    if 0 <= idx < NUM_LEDS:
                        dist = abs(off) / 16.0
                        glow = 1.0 - dist
                        if is_res:
                            pixels[idx] = [int(255 * glow), int(255 * glow), int(255 * glow)]
                        else:
                            pixels[idx] = [int(255 * glow), int(200 * glow), int(50 * glow)]

            elif game == 1:
                # 2. Sparkle Rush
                # Starry floor
                for i in range(NUM_LEDS):
                    twinkle = random.randint(15, 60)
                    if i < 108:
                        # Shield A
                        pixels.append([int(col_a[0] * 0.55), int(col_a[1] * 0.55), int(col_a[2] * 0.55)])
                    elif i >= (NUM_LEDS - 108):
                        # Shield B
                        pixels.append([int(col_b[0] * 0.55), int(col_b[1] * 0.55), int(col_b[2] * 0.55)])
                    else:
                        pixels.append([twinkle, twinkle, int(twinkle * 1.5)])
                # Cosmic Nova
                nova = max(0, min(NUM_LEDS - 1, int(demo_state["nova_pos"])))
                for off in range(-12, 13):
                    idx = nova + off
                    if 0 <= idx < NUM_LEDS:
                        glow = 1.0 - (abs(off) / 12.0)
                        pixels[idx] = [int(255 * glow), int(240 * glow), int(180 * glow)]

            elif game == 2:
                # 3. Flame Tug
                split = max(10, min(NUM_LEDS - 10, demo_state["flame_pos"]))
                for i in range(NUM_LEDS):
                    fl = random.randint(80, 220) / 255.0
                    if i < split:
                        pixels.append([int(col_a[0] * fl), int(col_a[1] * fl), int(col_a[2] * fl)])
                    else:
                        pixels.append([int(col_b[0] * fl), int(col_b[1] * fl), int(col_b[2] * fl)])
                # White-hot molten collision front
                for off in range(-14, 15):
                    idx = split + off
                    if 0 <= idx < NUM_LEDS:
                        glow = 1.0 - (abs(off) / 14.0)
                        pixels[idx] = [min(255, pixels[idx][0] + int(255 * glow)),
                                       min(255, pixels[idx][1] + int(240 * glow)),
                                       min(255, pixels[idx][2] + int(200 * glow))]

            elif game == 3:
                # 4. Meteor Deflector
                for i in range(NUM_LEDS):
                    if i < 108:
                        pixels.append([int(col_a[0] * 0.6), int(col_a[1] * 0.6), int(col_a[2] * 0.6)])
                    elif i >= (NUM_LEDS - 108):
                        pixels.append([int(col_b[0] * 0.6), int(col_b[1] * 0.6), int(col_b[2] * 0.6)])
                    else:
                        pixels.append([12, 15, 24])
                # Flaming meteor
                met = max(0, min(NUM_LEDS - 1, int(demo_state["meteor_pos"])))
                for off in range(-16, 17):
                    idx = met + off
                    if 0 <= idx < NUM_LEDS:
                        glow = 1.0 - (abs(off) / 16.0)
                        pixels[idx] = [int(255 * glow), int(180 * glow), int(40 * glow)]

            elif game == 4:
                # 5. Cylon Clash
                for i in range(NUM_LEDS):
                    if i < 100:
                        pixels.append([int(col_a[0] * 0.5), int(col_a[1] * 0.5), int(col_a[2] * 0.5)])
                    elif i >= (NUM_LEDS - 100):
                        pixels.append([int(col_b[0] * 0.5), int(col_b[1] * 0.5), int(col_b[2] * 0.5)])
                    else:
                        pixels.append([10, 10, 15])
                puck = max(0, min(NUM_LEDS - 1, int(demo_state["cylon_pos"])))
                for off in range(-10, 11):
                    idx = puck + off
                    if 0 <= idx < NUM_LEDS:
                        glow = 1.0 - (abs(off) / 10.0)
                        pixels[idx] = [int(255 * glow), int(255 * glow), int(255 * glow)]

            else:
                # 6. Territory Paint
                bound = max(5, min(NUM_LEDS - 5, demo_state["paint_boundary"]))
                for i in range(NUM_LEDS):
                    if i < bound:
                        pixels.append(list(col_a))
                    else:
                        pixels.append(list(col_b))
                # White division line
                for off in range(-2, 3):
                    idx = bound + off
                    if 0 <= idx < NUM_LEDS:
                        pixels[idx] = [255, 255, 255]

        else:
            # Ambient Lighting Modes (0..5) on 720 LEDs
            mode_idx = demo_state["mode_idx"]

            if mode_idx == 0:
                # 1. Breathing / Pulse: 7.5s cycle wave
                wave = (math.sin(elapsed * 0.84) + 1.0) * 0.5
                factor = 0.42 + wave * 0.40
                r = int(base_r * factor)
                g = int(base_g * factor)
                b = int(base_b * factor)
                pixels = [[r, g, b] for _ in range(NUM_LEDS)]

            elif mode_idx == 1:
                # 2. Twinkle / Sparkle
                for i in range(NUM_LEDS):
                    twinkle_b[i] += twinkle_d[i]
                    if twinkle_b[i] >= 220:
                        twinkle_d[i] = -1
                    elif twinkle_b[i] <= 58:
                        twinkle_d[i] = 1
                    br = max(58, min(220, twinkle_b[i]))
                    r = (base_r * br) >> 8
                    g = (base_g * br) >> 8
                    b = (base_b * br) >> 8
                    if br > 195:
                        spark = (br - 195) * 2
                        r = min(255, r + spark)
                        g = min(255, g + spark)
                        b = min(255, b + spark)
                    pixels.append([r, g, b])

            elif mode_idx == 2:
                # 3. Fire / Flame
                for i in range(NUM_LEDS):
                    cooldown = random.randint(1, 3)
                    heat[i] = max(78, heat[i] - cooldown)
                for k in range(NUM_LEDS - 1, 1, -1):
                    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) // 3
                if random.random() < 0.4:
                    idx = random.randint(0, NUM_LEDS - 1)
                    heat[idx] = min(255, heat[idx] + random.randint(60, 110))

                for temp in heat:
                    t192 = int(temp * 191 / 255)
                    ramp = (t192 & 0x3F) << 2
                    if t192 > 0x80:
                        pixels.append([min(255, base_r + ramp), min(255, base_g + ramp), min(255, base_b + ramp)])
                    elif t192 > 0x40:
                        factor = t192 << 1
                        pixels.append([(base_r * factor) >> 8, (base_g * factor) >> 8, (base_b * factor) >> 8])
                    else:
                        factor = max(60, t192 << 2)
                        pixels.append([(base_r * factor) >> 8, (base_g * factor) >> 8, (base_b * factor) >> 8])

            elif mode_idx == 3:
                # 4. Comet / Meteor
                sweep = (math.sin(elapsed * 0.4) + 1.0) * 0.5 * (NUM_LEDS - 1)
                for i in range(NUM_LEDS):
                    dist = abs(i - sweep)
                    if dist < 2.0:
                        pixels.append([min(255, base_r + 160), min(255, base_g + 160), min(255, base_b + 160)])
                    elif dist < 30.0:
                        glow = 1.0 - dist / 30.0
                        pixels.append([int(base_r * glow), int(base_g * glow), int(base_b * glow)])
                    else:
                        pixels.append([(base_r * 65) >> 8, (base_g * 65) >> 8, (base_b * 65) >> 8])

            elif mode_idx == 4:
                # 5. Scanner / Cylon: 6.0s smooth Larson eye
                eye = (NUM_LEDS / 2.0) + (NUM_LEDS * 0.45) * math.sin(elapsed * 1.05)
                for i in range(NUM_LEDS):
                    dist = abs(i - eye)
                    if dist < 3.0:
                        pixels.append([min(255, base_r + 80), min(255, base_g + 80), min(255, base_b + 80)])
                    elif dist < 24.0:
                        glow = 1.0 - dist / 24.0
                        pixels.append([int(base_r * glow), int(base_g * glow), int(base_b * glow)])
                    else:
                        pixels.append([(base_r * 60) >> 8, (base_g * 60) >> 8, (base_b * 60) >> 8])

            else:
                # 6. Color Wipe
                wipe_idx = int(elapsed * 35.0) % NUM_LEDS
                pal_idx = int(elapsed * 0.1) % 4
                if demo_state.get("is_white", False):
                    white_tints = [
                        [86, 86, 86],
                        [96, 83, 64],
                        [78, 86, 96],
                        [90, 87, 76]
                    ]
                    wipe_col = list(white_tints[pal_idx])
                else:
                    wipe_hue = (demo_state["hue"] + pal_idx * 10922) % 65536
                    wipe_col = list(constant_brightness_rgb(wipe_hue))
                for i in range(NUM_LEDS):
                    if i <= wipe_idx:
                        pixels.append(list(wipe_col))
                    else:
                        pixels.append([(base_r * 60) >> 8, (base_g * 60) >> 8, (base_b * 60) >> 8])

        # 3. Live Scoreboard Overlay on Strip Ends (LEDs 0..4 and 719..715)
        if state in ("playing", "match_over"):
            col_a = COLOR_GAMER_A
            col_b = COLOR_GAMER_ROBOT if demo_state["against_robot"] else COLOR_GAMER_B
            for p in range(min(5, demo_state["scores"][0])):
                if p < len(pixels):
                    pixels[p] = list(col_a)
            for p in range(min(5, demo_state["scores"][1])):
                idx = NUM_LEDS - 1 - p
                if 0 <= idx < len(pixels):
                    pixels[idx] = list(col_b)

        elif state == "challenge":
            # Echo challenge on strip ends
            if demo_state["challenger"] == "player_a":
                pixels[0] = list(COLOR_GAMER_A)
                pixels[NUM_LEDS - 1] = list(COLOR_GAMER_B) if demo_state["challenge_blink_state"] else [0, 0, 0]
            else:
                pixels[NUM_LEDS - 1] = list(COLOR_GAMER_B)
                pixels[0] = list(COLOR_GAMER_A) if demo_state["challenge_blink_state"] else [0, 0, 0]

        # 4. Dedicated Station Gamer LEDs (Pin 3 for Station A, Pin 5 for Station B)
        col_side_a = COLOR_GAMER_A
        col_side_b = COLOR_GAMER_ROBOT if demo_state["against_robot"] else COLOR_GAMER_B

        if state == "idle_ambient":
            # Soft ambient beacon
            led_a = [int(COLOR_GAMER_A[0] * 0.28), int(COLOR_GAMER_A[1] * 0.28), int(COLOR_GAMER_A[2] * 0.28)]
            led_b = [int(COLOR_GAMER_B[0] * 0.28), int(COLOR_GAMER_B[1] * 0.28), int(COLOR_GAMER_B[2] * 0.28)]

        elif state == "challenge":
            if demo_state["challenger"] == "player_a":
                led_a = list(COLOR_GAMER_A)
                led_b = list(COLOR_GAMER_B) if demo_state["challenge_blink_state"] else [0, 0, 0]
            else:
                led_b = list(COLOR_GAMER_B)
                led_a = list(COLOR_GAMER_A) if demo_state["challenge_blink_state"] else [0, 0, 0]

        elif state == "playing":
            if demo_state["round_over"]:
                pulse = int((now * 8.0) % 2) == 0
                if demo_state["round_winner"] == 0:
                    led_a = [255, 255, 255] if pulse else list(col_side_a)
                    led_b = [int(col_side_b[0] * 0.2), int(col_side_b[1] * 0.2), int(col_side_b[2] * 0.2)]
                else:
                    led_b = [255, 255, 255] if pulse else list(col_side_b)
                    led_a = [int(col_side_a[0] * 0.2), int(col_side_a[1] * 0.2), int(col_side_a[2] * 0.2)]
            else:
                # In-game tactile button confirmation flash (80ms pure white)
                if now - demo_state["last_press_time_a"] < 0.08:
                    led_a = [255, 255, 255]
                else:
                    led_a = list(col_side_a)

                if now - demo_state["last_press_time_b"] < 0.08:
                    led_b = [255, 255, 255]
                else:
                    led_b = list(col_side_b)

        elif state == "match_over":
            strobe = int((now * 10.0) % 2) == 0
            win_side = demo_state["round_winner"]
            if win_side == 0:
                led_a = [255, 255, 255] if strobe else list(COLOR_GAMER_A)
                led_b = [0, 0, 0]
            else:
                win_col = COLOR_GAMER_ROBOT if demo_state["against_robot"] else COLOR_GAMER_B
                led_b = [255, 255, 255] if strobe else list(win_col)
                led_a = [0, 0, 0]

        # 5. Global Brightness Scaling
        bright = demo_state.get("brightness", 255)
        if bright < 255:
            pixels = [[(p[0] * bright) >> 8, (p[1] * bright) >> 8, (p[2] * bright) >> 8] for p in pixels]
            led_a = [(led_a[0] * bright) >> 8, (led_a[1] * bright) >> 8, (led_a[2] * bright) >> 8]
            led_b = [(led_b[0] * bright) >> 8, (led_b[1] * bright) >> 8, (led_b[2] * bright) >> 8]

        # 6. Publish Frame to Global State
        with frame_lock:
            current_frame["num_leds"] = len(pixels)
            current_frame["leds"] = pixels
            current_frame["gamer_a"] = led_a
            current_frame["gamer_b"] = led_b
            current_frame["scores"] = list(demo_state["scores"])
            current_frame["state"] = demo_state["system_state"]
            current_frame["challenger"] = demo_state["challenger"]
            current_frame["against_robot"] = demo_state["against_robot"]
            current_frame["active_mode_idx"] = demo_state["mode_idx"]
            current_frame["active_mode_name"] = LIGHT_MODE_NAMES[demo_state["mode_idx"]]
            current_frame["active_game_idx"] = demo_state["game_idx"]
            current_frame["active_game_name"] = GAME_NAMES[demo_state["game_idx"]]
            current_frame["fps"] = 45.0
            current_frame["luminance"] = calculate_luminance(pixels)
            current_frame["timestamp"] = now
            current_frame["source"] = f"Demo Mode (12m / 720 LEDs - {current_frame['active_mode_name']})"

        time.sleep(0.022)  # ~45 FPS


class VisualizerHTTPHandler(SimpleHTTPRequestHandler):
    """Custom HTTP Handler serving static visualizer GUI and real-time frame polling."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def do_GET(self):
        global demo_state
        parsed = urllib.parse.urlparse(self.path)

        if parsed.path == "/api/frame":
            # Real-time frame polling endpoint
            with frame_lock:
                data = json.dumps(current_frame).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return

        elif parsed.path == "/api/input":
            # Relay virtual keyboard buttons and knob adjustments to Arduino Serial & local demo
            query = urllib.parse.parse_qs(parsed.query)
            key = query.get("key", [""])[0]

            # Forward to Arduino hardware if connected
            if key and serial_handle and serial_handle.is_open:
                try:
                    payload = (key if key.endswith('\n') else (key + '\n')).encode("ascii")
                    serial_handle.write(payload)
                    print(f"[Input] Relayed command '{key.strip()}' to Arduino Uno")
                except Exception as ex:
                    print(f"[Input Error] {ex}")

            # Update local demo generator state
            if key:
                k = key.strip().lower()
                if k == '1':
                    trigger_demo_button_press(0)
                elif k == '2':
                    trigger_demo_button_press(1)
                elif k == 'g':
                    if demo_state["system_state"] == "idle_ambient":
                        trigger_demo_button_press(0)
                    else:
                        demo_state["system_state"] = "idle_ambient"
                        demo_state["scores"] = [0, 0]
                        demo_state["round_over"] = False
                        print("[Demo] Exited Game Mode -> Ambient Lighting")
                elif k.startswith('m'):
                    if len(k) > 1 and k[1:].isdigit():
                        demo_state["mode_idx"] = max(0, min(5, int(k[1:])))
                    else:
                        demo_state["mode_idx"] = (demo_state["mode_idx"] + 1) % 6
                    print(f"[Demo] Ambient Mode set to: {LIGHT_MODE_NAMES[demo_state['mode_idx']]}")
                elif k.startswith('k'):
                    if len(k) > 1 and k[1:].isdigit():
                        demo_state["game_idx"] = max(0, min(5, int(k[1:])))
                    else:
                        demo_state["game_idx"] = (demo_state["game_idx"] + 1) % 6
                    reset_demo_game_physics()
                    print(f"[Demo] Game set to: {GAME_NAMES[demo_state['game_idx']]}")
                elif k == 'c':
                    if demo_state.get("is_white", False):
                        demo_state["is_white"] = False
                        demo_state["hue"] = 0
                    elif demo_state["hue"] >= 65535:
                        demo_state["is_white"] = True
                    else:
                        demo_state["hue"] = min(65535, demo_state["hue"] + 4000)
                elif k == 'w':
                    demo_state["is_white"] = True
                elif k.startswith('h'):
                    try:
                        h_val = int(k[1:])
                        if h_val < 0 or h_val >= 65536:
                            demo_state["is_white"] = True
                        else:
                            demo_state["is_white"] = False
                            demo_state["hue"] = max(0, min(65535, h_val))
                    except ValueError:
                        pass
                elif k.startswith('b'):
                    try:
                        if len(k) > 1 and k[1:].isdigit():
                            demo_state["brightness"] = max(0, min(255, int(k[1:])))
                        else:
                            b_cur = demo_state.get("brightness", 255)
                            demo_state["brightness"] = 25 if b_cur >= 255 else b_cur + 25
                    except ValueError:
                        pass

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(b'{"status":"ok"}')
            return

        return super().do_GET()

    def log_message(self, format, *args):
        # Silence routine frame polling HTTP logs
        if "/api/frame" not in args[0]:
            super().log_message(format, *args)


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Linux Virtual NeoPixel Strip Host (12m / 720 LEDs)")
    parser.add_argument("--port", help="Serial port (e.g. /dev/ttyACM0)", default=None)
    parser.add_argument("--baud", help="Serial baud rate", default=SERIAL_BAUD, type=int)
    parser.add_argument("--http-port", help="Local web server port", default=HTTP_PORT, type=int)
    parser.add_argument("--demo", action="store_true", help="Force demo generator mode")
    args = parser.parse_args()

    port_to_use = args.port or auto_detect_serial_port()

    # Determine whether to use physical serial or demo mode
    if args.demo or not HAS_SERIAL or not port_to_use:
        if not HAS_SERIAL:
            print("[Warning] pyserial is not installed; running in local demo mode.")
        elif not port_to_use:
            print("[Info] No active Arduino detected on /dev/ttyACM* or /dev/ttyUSB*.")
            print("       Starting built-in 720-LED Demo Mode (connect Arduino anytime or run with --port).")
        t = threading.Thread(target=run_demo_simulation_loop, daemon=True)
        t.start()
    else:
        print(f"[Info] Found Arduino Serial candidate at: {port_to_use}")
        t = threading.Thread(target=read_serial_loop, args=(port_to_use, args.baud), daemon=True)
        t.start()

    # Start HTTP server
    server_address = ("0.0.0.0", args.http_port)
    httpd = HTTPServer(server_address, VisualizerHTTPHandler)
    url = f"http://localhost:{args.http_port}"

    print("=" * 76)
    print("  ARCHIV BAR - VIRTUAL NEOPIXEL BAR COUNTER (12m / 720 LEDs)")
    print(f"  Visualizer Web UI: {url}")
    print("  Physical Station Controls:")
    print("    - [A] Key  : Player Station A Action Button & Dedicated LED (Pin 2/3)")
    print("    - [L] Key  : Player Station B Action Button & Dedicated LED (Pin 4/5)")
    print("    - [G] Key  : Game-Start Challenge / Duel / Accept / Toggle")
    print("  Rotary Control Panel:")
    print("    - [M] / m<0-5> : Pot A4 Global Light Behavior Knob (6 Ambient Modes)")
    print("    - [K] / k<0-5> : Pot A1 Game Selection Knob (6 Competitive Games)")
    print("    - [C] / h<hue> : Pot A3 Mood Color Knob (360° Rainbow Spectrum + White)")
    print("    - [B] / b<val> : Pot A2 Master Brightness Knob (0-255 Luminosity)")
    print("=" * 76)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[Host] Shutting down...")
        httpd.server_close()


if __name__ == "__main__":
    main()
