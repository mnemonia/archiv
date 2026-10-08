#!/usr/bin/env python3
"""
===============================================================================
Virtual NeoPixel Strip Bridge & Linux Visualizer Host
===============================================================================
Bridges binary frame data from Arduino Uno R3 over USB Serial to an ultra-realistic
HTML5/Canvas visualizer running in your browser under Linux (Wayland & X11).

Protocol (from Arduino Uno R3 Implementation A):
  Header:   0xAA 0x55 0x01 (CMD_FRAME) [NUM_LEDS=60]
  Payload:  180 bytes (RGB RGB ... for 60 LEDs)
  Checksum: XOR of all header + payload bytes

Usage:
  python3 bridge.py              # Auto-detects /dev/ttyACM0 or /dev/ttyUSB0
  python3 bridge.py --port /dev/ttyACM0 --baud 115200
  python3 bridge.py --demo       # Runs built-in simulated Arduino frame generator
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
NUM_LEDS = 60
WEB_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "web")

# Global frame buffer shared with web clients
current_frame = {
    "leds": [[0, 0, 0] for _ in range(NUM_LEDS)],
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
    """Calculate average perceptual luminance across all 60 LEDs (0-100%)."""
    if not pixels:
        return 0.0
    total = sum((0.299 * r + 0.587 * g + 0.114 * b) for r, g, b in pixels)
    avg = total / len(pixels)
    return round((avg / 255.0) * 100.0, 1)


def read_serial_loop(port_name, baud_rate):
    """Reads binary frames from Arduino Uno R3 according to Implementation A protocol."""
    global current_frame, serial_handle

    while True:
        try:
            print(f"[Serial] Connecting to Arduino Uno at {port_name} ({baud_rate} baud)...")
            ser = serial.Serial(port_name, baud_rate, timeout=1.0)
            serial_handle = ser
            print(f"[Serial] Successfully connected to {port_name}!")
            time.sleep(1.8) # Allow Arduino reset sequence

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

                # 2. Read Command & LED Count
                meta = ser.read(2)
                if len(meta) < 2:
                    continue
                cmd, count = meta[0], meta[1]
                if cmd != 0x01 or count != NUM_LEDS:
                    continue

                # 3. Read 180 bytes RGB payload
                payload_len = count * 3
                payload = ser.read(payload_len)
                if len(payload) < payload_len:
                    continue

                # 4. Read XOR Checksum
                chk = ser.read(1)
                if not chk:
                    continue
                expected_chk = chk[0]

                # Verify Checksum
                calc_chk = 0xAA ^ 0x55 ^ cmd ^ count
                for byte_val in payload:
                    calc_chk ^= byte_val

                if calc_chk != expected_chk:
                    # Checksum mismatch; discard corrupted packet
                    continue

                # Decode into 60 RGB pixel tuples
                pixels = []
                for i in range(0, payload_len, 3):
                    r = payload[i]
                    g = payload[i + 1]
                    b = payload[i + 2]
                    pixels.append([r, g, b])

                frame_count += 1
                now = time.time()
                if now - fps_start_time >= 1.0:
                    current_fps = frame_count / (now - fps_start_time)
                    frame_count = 0
                    fps_start_time = now

                with frame_lock:
                    current_frame["leds"] = pixels
                    current_frame["fps"] = round(current_fps, 1)
                    current_frame["luminance"] = calculate_luminance(pixels)
                    current_frame["timestamp"] = now
                    current_frame["source"] = f"Arduino Uno ({port_name})"

        except Exception as e:
            print(f"[Serial Error] {e}. Retrying in 2 seconds...")
            serial_handle = None
            time.sleep(2.0)


demo_state = {
    "mode_idx": 0,
    "hue": 0,          # 0..65535 matching Arduino RotaryColorKnob
    "in_game": False
}


def hsv_to_rgb(hue_16, sat=255, val=255):
    """Converts 16-bit hue (0..65535) to RGB, matching NeoPixel Driver ColorHSV math."""
    h_idx = ((hue_16 // 10922) % 6)
    rem = hue_16 % 10922
    base = ((255 - sat) * val) >> 8
    if h_idx == 0:
        r = val
        g = (((val - base) * rem) // 10922) + base
        b = base
    elif h_idx == 1:
        r = (((val - base) * (10922 - rem)) // 10922) + base
        g = val
        b = base
    elif h_idx == 2:
        r = base
        g = val
        b = (((val - base) * rem) // 10922) + base
    elif h_idx == 3:
        r = base
        g = (((val - base) * (10922 - rem)) // 10922) + base
        b = val
    elif h_idx == 4:
        r = (((val - base) * rem) // 10922) + base
        g = base
        b = val
    else:
        r = val
        g = base
        b = (((val - base) * (10922 - rem)) // 10922) + base
    return [max(0, min(255, r)), max(0, min(255, g)), max(0, min(255, b))]


def run_demo_simulation_loop():
    """Generates realistic test frames locally when no physical Arduino is connected."""
    global current_frame, demo_state
    print("[Demo Mode] Generating local simulated NeoPixel frames...")
    modes = [
        "Breathing / Pulse",
        "Twinkle / Sparkle",
        "Fire / Flame",
        "Chase / Marquee",
        "Comet / Meteor",
        "Scanner / Cylon",
        "Color Wipe"
    ]
    start_time = time.time()
    last_mode_switch = start_time

    heat = [85] * NUM_LEDS
    twinkle_b = [random.randint(55, 200) for _ in range(NUM_LEDS)]
    twinkle_d = [random.choice([2, -2]) for _ in range(NUM_LEDS)]

    while True:
        now = time.time()
        elapsed = now - start_time

        # Cycle modes every 15 seconds if unprompted in demo mode
        if now - last_mode_switch > 15.0:
            demo_state["mode_idx"] = (demo_state["mode_idx"] + 1) % len(modes)
            last_mode_switch = now

        mode_idx = demo_state["mode_idx"]
        cur_mode = modes[mode_idx]
        base_r, base_g, base_b = hsv_to_rgb(demo_state["hue"])
        pixels = []

        if mode_idx == 0:
            # 1. Breathing / Pulse
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
            # 4. Chase / Marquee
            step = int(elapsed * 4.5) % 4
            for i in range(NUM_LEDS):
                if (i + step) % 4 == 0:
                    pixels.append([min(255, base_r + 40), min(255, base_g + 40), min(255, base_b + 40)])
                else:
                    pixels.append([(base_r * 75) >> 8, (base_g * 75) >> 8, (base_b * 75) >> 8])

        elif mode_idx == 4:
            # 5. Comet / Meteor
            sweep = (math.sin(elapsed * 0.5) + 1.0) * 0.5 * 59
            for i in range(NUM_LEDS):
                dist = abs(i - sweep)
                if dist < 1.0:
                    pixels.append([min(255, base_r + 160), min(255, base_g + 160), min(255, base_b + 160)])
                elif dist < 10.0:
                    glow = 1.0 - dist / 10.0
                    pixels.append([int(base_r * glow), int(base_g * glow), int(base_b * glow)])
                else:
                    pixels.append([(base_r * 65) >> 8, (base_g * 65) >> 8, (base_b * 65) >> 8])

        elif mode_idx == 5:
            # 6. Scanner / Cylon
            eye = 29.5 + 27.5 * math.sin(elapsed * 1.05)
            for i in range(NUM_LEDS):
                dist = abs(i - eye)
                if dist < 1.0:
                    pixels.append([min(255, base_r + 80), min(255, base_g + 80), min(255, base_b + 80)])
                elif dist < 6.0:
                    glow = 1.0 - dist / 6.0
                    pixels.append([int(base_r * glow), int(base_g * glow), int(base_b * glow)])
                else:
                    pixels.append([(base_r * 60) >> 8, (base_g * 60) >> 8, (base_b * 60) >> 8])

        else:
            # 7. Color Wipe
            wipe_idx = int(elapsed * 5.5) % NUM_LEDS
            pal_idx = int(elapsed * 0.1) % 4
            wipe_hue = (demo_state["hue"] + pal_idx * 10922) % 65536
            wipe_col = hsv_to_rgb(wipe_hue)
            for i in range(NUM_LEDS):
                if i <= wipe_idx:
                    pixels.append(wipe_col)
                else:
                    pixels.append([(base_r * 60) >> 8, (base_g * 60) >> 8, (base_b * 60) >> 8])

        # Enforce Ambient Illumination Guard (>= 35%)
        lum = calculate_luminance(pixels)
        if lum < 35.0:
            deficit = int((35.0 - lum) * 2.55)
            boost_r = min(255, int(deficit * 1.48))
            boost_g = min(255, int(deficit * 0.88))
            boost_b = min(255, int(deficit * 0.28))
            for p in pixels:
                p[0] = min(255, p[0] + boost_r)
                p[1] = min(255, p[1] + boost_g)
                p[2] = min(255, p[2] + boost_b)

        with frame_lock:
            current_frame["leds"] = pixels
            current_frame["fps"] = 45.0
            current_frame["luminance"] = calculate_luminance(pixels)
            current_frame["timestamp"] = now
            current_frame["source"] = f"Demo Mode ({cur_mode})"

        time.sleep(0.022) # ~45 FPS


class VisualizerHTTPHandler(SimpleHTTPRequestHandler):
    """Custom HTTP Handler serving both the static visualizer GUI and real-time frame SSE."""

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
            # Relay virtual keyboard buttons from browser back to Arduino Serial
            query = urllib.parse.parse_qs(parsed.query)
            key = query.get("key", [""])[0]

            # Forward to Arduino hardware if connected
            if key and serial_handle and serial_handle.is_open:
                try:
                    serial_handle.write(key.encode("ascii"))
                    print(f"[Input] Sent key '{key}' to Arduino Uno")
                except Exception as ex:
                    print(f"[Input Error] {ex}")

            # Also update local demo generator state
            if key:
                k = key.lower()
                if k == 'm':
                    demo_state["mode_idx"] = (demo_state["mode_idx"] + 1) % 7
                    print(f"[Demo] Switched mode to {demo_state['mode_idx']}")
                elif k == 'c':
                    demo_state["hue"] = (demo_state["hue"] + 4000) % 65536
                    print(f"[Demo] Stepped hue to {demo_state['hue']}")
                elif k.startswith('h'):
                    try:
                        h_val = int(k[1:])
                        demo_state["hue"] = max(0, min(65535, h_val))
                        print(f"[Demo] Set hue to {demo_state['hue']}")
                    except ValueError:
                        pass
                elif k == 'g':
                    demo_state["in_game"] = not demo_state["in_game"]
                    print(f"[Demo] Toggled game: {demo_state['in_game']}")

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(b'{"status":"ok"}')
            return

        return super().do_GET()

    def log_message(self, format, *args):
        # Silence routine static asset HTTP logs to keep console clean
        if "/api/frame" not in args[0]:
            super().log_message(format, *args)


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Linux Virtual NeoPixel Strip Host")
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
            print("       Starting built-in Demo Mode (connect Arduino anytime or run with --port).")
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

    print("=" * 72)
    print("  VISUAL VIRTUAL NEOPIXEL STRIP (60 LEDs/m) - LINUX HOST")
    print(f"  Visualizer Web UI: {url}")
    print("  Controls in Browser:")
    print("    - [A] Key  : Player 1 Button")
    print("    - [L] Key  : Player 2 Button")
    print("    - [M] Key  : Toggle Mode (Cycles 7 Modes)")
    print("    - [G] Key  : Start / Stop Competitive Game")
    print("    - [C] Key  : Cycle / Step Rotary Base Color (Rainbow Palette)")
    print("=" * 72)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[Host] Shutting down...")
        httpd.server_close()


if __name__ == "__main__":
    main()

