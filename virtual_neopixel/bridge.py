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


def run_demo_simulation_loop():
    """Generates realistic test frames locally when no physical Arduino is connected."""
    global current_frame
    print("[Demo Mode] Generating local simulated NeoPixel frames...")
    modes = ["Breathing / Pulse", "Twinkle / Sparkle", "Fire / Flame"]
    mode_idx = 0
    start_time = time.time()
    last_mode_switch = start_time

    heat = [85] * NUM_LEDS
    twinkle_b = [random.randint(55, 200) for _ in range(NUM_LEDS)]
    twinkle_d = [random.choice([2, -2]) for _ in range(NUM_LEDS)]

    while True:
        now = time.time()
        elapsed = now - start_time

        # Cycle modes every 12 seconds in demo mode
        if now - last_mode_switch > 12.0:
            mode_idx = (mode_idx + 1) % len(modes)
            last_mode_switch = now

        pixels = []
        cur_mode = modes[mode_idx]

        if mode_idx == 0:
            # 1. Breathing Warm-White
            wave = (math.sin(elapsed * 1.8) + 1.0) * 0.5
            factor = 0.40 + wave * 0.50
            r = int(255 * factor)
            g = int(148 * factor)
            b = int(38 * factor)
            pixels = [[r, g, b] for _ in range(NUM_LEDS)]

        elif mode_idx == 1:
            # 2. Twinkle Fairy Lights
            for i in range(NUM_LEDS):
                twinkle_b[i] += twinkle_d[i]
                if twinkle_b[i] >= 240:
                    twinkle_d[i] = -random.randint(2, 5)
                elif twinkle_b[i] <= 55:
                    twinkle_d[i] = random.randint(2, 5)
                val = max(55, min(255, twinkle_b[i]))
                pixels.append([val, int(val * 0.74), int(val * 0.45)])

        else:
            # 3. Fire / Flame
            for i in range(NUM_LEDS):
                cooldown = random.randint(2, 5)
                heat[i] = max(75, heat[i] - cooldown)
            for k in range(NUM_LEDS - 1, 1, -1):
                heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) // 3
            if random.random() < 0.6:
                idx = random.randint(0, NUM_LEDS - 1)
                heat[idx] = min(255, heat[idx] + random.randint(90, 160))

            for temp in heat:
                t192 = int(temp * 191 / 255)
                ramp = (t192 & 0x3F) << 2
                if t192 > 0x80:
                    pixels.append([255, 255, ramp])
                elif t192 > 0x40:
                    pixels.append([255, ramp, 0])
                else:
                    pixels.append([ramp, 0, 0])

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
            if key and serial_handle and serial_handle.is_open:
                try:
                    serial_handle.write(key.encode("ascii"))
                    print(f"[Input] Sent key '{key}' to Arduino Uno")
                except Exception as ex:
                    print(f"[Input Error] {ex}")

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
    print("    - [M] Key  : Toggle Mode (Breathing -> Twinkle -> Fire)")
    print("    - [G] Key  : Start / Stop Competitive Game")
    print("=" * 72)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[Host] Shutting down...")
        httpd.server_close()


if __name__ == "__main__":
    main()

