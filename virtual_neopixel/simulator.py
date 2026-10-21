#!/usr/bin/env python3
"""
===============================================================================
Virtual NeoPixel Strip Protocol Tester & Simulation Harness (12m / 720 LEDs)
===============================================================================
Automated verification tests for the 720-LED FastLED Serial protocol,
optical framing, XOR checksum integrity, dedicated gamer LEDs, and minimum
luminance guard validation.

Usage:
  python3 simulator.py --test-protocol
  python3 simulator.py --test-threshold
===============================================================================
"""

import sys
import math
import random

NUM_LEDS = 720
PAYLOAD_SIZE = NUM_LEDS * 3
# Extended protocol: Header 3 bytes (0xAA 0x55 0x02) + 2 bytes count + 6 bytes gamer LEDs + payload + 1 byte checksum
TOTAL_FRAME_SIZE_EXTENDED = 3 + 2 + 6 + PAYLOAD_SIZE + 1  # 2172 bytes


def build_extended_frame(pixels, gamer_a=(0, 180, 255), gamer_b=(255, 30, 70)):
    """Encodes 720 RGB pixels + 2 dedicated gamer LEDs into CMD 0x02 binary frame."""
    count = len(pixels)
    header = bytes([
        0xAA, 0x55, 0x02,
        (count >> 8) & 0xFF, count & 0xFF,
        gamer_a[0] & 0xFF, gamer_a[1] & 0xFF, gamer_a[2] & 0xFF,
        gamer_b[0] & 0xFF, gamer_b[1] & 0xFF, gamer_b[2] & 0xFF
    ])

    payload = bytearray()
    for r, g, b in pixels:
        payload.append(int(r) & 0xFF)
        payload.append(int(g) & 0xFF)
        payload.append(int(b) & 0xFF)

    # Calculate XOR checksum
    chk = 0
    for b in header:
        chk ^= b
    for b in payload:
        chk ^= b

    return header + bytes(payload) + bytes([chk])


def decode_extended_frame(raw_bytes):
    """Decodes and validates a raw CMD 0x02 extended packet."""
    if len(raw_bytes) < 12:
        return None, None, None, f"Packet too short: {len(raw_bytes)}"

    if raw_bytes[0] != 0xAA or raw_bytes[1] != 0x55:
        return None, None, None, f"Invalid sync bytes: {hex(raw_bytes[0])} {hex(raw_bytes[1])}"

    cmd = raw_bytes[2]
    if cmd != 0x02:
        return None, None, None, f"Invalid command byte: {cmd}"

    count = (raw_bytes[3] << 8) | raw_bytes[4]
    gamer_a = (raw_bytes[5], raw_bytes[6], raw_bytes[7])
    gamer_b = (raw_bytes[8], raw_bytes[9], raw_bytes[10])

    expected_len = 11 + (count * 3) + 1
    if len(raw_bytes) != expected_len:
        return None, None, None, f"Packet length mismatch: got {len(raw_bytes)}, expected {expected_len}"

    payload = raw_bytes[11:11 + count * 3]
    expected_chk = raw_bytes[-1]

    calc_chk = 0
    for b in raw_bytes[:11]:
        calc_chk ^= b
    for b in payload:
        calc_chk ^= b

    if calc_chk != expected_chk:
        return None, None, None, f"Checksum error: calculated {hex(calc_chk)}, expected {hex(expected_chk)}"

    pixels = []
    for i in range(0, len(payload), 3):
        pixels.append((payload[i], payload[i + 1], payload[i + 2]))

    return pixels, gamer_a, gamer_b, "OK"


def build_legacy_frame(pixels):
    """Encodes a 60-LED legacy packet (CMD 0x01)."""
    count = len(pixels)
    header = bytes([0xAA, 0x55, 0x01, count])
    payload = bytearray()
    for r, g, b in pixels:
        payload.append(int(r) & 0xFF)
        payload.append(int(g) & 0xFF)
        payload.append(int(b) & 0xFF)

    chk = 0xAA ^ 0x55 ^ 0x01 ^ count
    for b in payload:
        chk ^= b

    return header + bytes(payload) + bytes([chk])


def decode_legacy_frame(raw_bytes):
    """Decodes a 60-LED legacy packet (CMD 0x01)."""
    if len(raw_bytes) < 5:
        return None, "Packet too short"
    if raw_bytes[0] != 0xAA or raw_bytes[1] != 0x55 or raw_bytes[2] != 0x01:
        return None, "Invalid sync or cmd"
    count = raw_bytes[3]
    expected_len = 4 + count * 3 + 1
    if len(raw_bytes) != expected_len:
        return None, f"Length mismatch: {len(raw_bytes)} vs {expected_len}"
    payload = raw_bytes[4:4 + count * 3]
    expected_chk = raw_bytes[-1]
    calc_chk = 0xAA ^ 0x55 ^ 0x01 ^ count
    for b in payload:
        calc_chk ^= b
    if calc_chk != expected_chk:
        return None, "Checksum mismatch"
    pixels = []
    for i in range(0, len(payload), 3):
        pixels.append((payload[i], payload[i + 1], payload[i + 2]))
    return pixels, "OK"


def calculate_perceptual_luminance(pixels):
    """Calculates average perceptual luminance (0-255 scale)."""
    total = sum((0.299 * r + 0.587 * g + 0.114 * b) for r, g, b in pixels)
    return total / len(pixels)


def test_protocol():
    """Runs automated unit tests for 720-LED framing, dedicated gamer LEDs, and checksum verification."""
    print("[TEST] Running Protocol Framing & Checksum Tests (12m / 720 LEDs)...")

    # Test 1: 720-LED Extended Frame Encode & Decode
    test_pixels_1 = [(255, 148, 38) for _ in range(NUM_LEDS)]
    test_gamer_a = (0, 180, 255)
    test_gamer_b = (255, 30, 70)
    packet_1 = build_extended_frame(test_pixels_1, test_gamer_a, test_gamer_b)
    assert len(packet_1) == TOTAL_FRAME_SIZE_EXTENDED, f"Length mismatch: {len(packet_1)} vs {TOTAL_FRAME_SIZE_EXTENDED}"

    decoded_1, g_a, g_b, status_1 = decode_extended_frame(packet_1)
    assert status_1 == "OK", f"Decode failed: {status_1}"
    assert decoded_1 == test_pixels_1, "Decoded pixels do not match original"
    assert g_a == test_gamer_a, "Gamer A LED mismatch"
    assert g_b == test_gamer_b, "Gamer B LED mismatch"
    print("  ✓ Test 1 Passed: 720-LED extended frame (CMD 0x02) encode/decode verified.")

    # Test 2: Dynamic Multi-Color 720-LED Gradient
    test_pixels_2 = [(i % 256, (i * 3) % 256, (i * 7) % 256) for i in range(NUM_LEDS)]
    packet_2 = build_extended_frame(test_pixels_2, (255, 255, 255), (255, 170, 0))
    decoded_2, g_a2, g_b2, status_2 = decode_extended_frame(packet_2)
    assert status_2 == "OK", f"Decode failed: {status_2}"
    assert decoded_2 == test_pixels_2, "Gradient pixels mismatch"
    assert g_a2 == (255, 255, 255)
    assert g_b2 == (255, 170, 0)
    print("  ✓ Test 2 Passed: 720-LED gradient with RobotGamer Amber LED verified.")

    # Test 3: Corrupted Checksum Rejection
    corrupted_packet = bytearray(packet_2)
    corrupted_packet[-1] ^= 0xFF
    decoded_3, _, _, status_3 = decode_extended_frame(bytes(corrupted_packet))
    assert decoded_3 is None, "Corrupted packet was unexpectedly accepted"
    assert "Checksum error" in status_3, f"Expected checksum error, got: {status_3}"
    print("  ✓ Test 3 Passed: Corrupted checksum rejection verified.")

    # Test 4: Truncated Packet Rejection
    truncated = packet_2[:500]
    decoded_4, _, _, status_4 = decode_extended_frame(truncated)
    assert decoded_4 is None, "Truncated packet was unexpectedly accepted"
    print("  ✓ Test 4 Passed: Truncated packet rejection verified.")

    # Test 5: Legacy 60-LED CMD 0x01 Support
    legacy_pixels = [(100, 150, 200) for _ in range(60)]
    legacy_packet = build_legacy_frame(legacy_pixels)
    decoded_leg, leg_status = decode_legacy_frame(legacy_packet)
    assert leg_status == "OK"
    assert decoded_leg == legacy_pixels
    print("  ✓ Test 5 Passed: Legacy 60-LED CMD 0x01 packet compatibility verified.")

    print("\n[SUCCESS] All Protocol Framing Tests Passed Successfully!\n")


def test_threshold_guard():
    """Simulates the Arduino Ambient Illumination Guard algorithm across 720 LEDs."""
    print("[TEST] Running Ambient Illumination Guard Algorithm Tests (720 LEDs)...")
    MIN_LUMEN_THRESHOLD = 88  # ~35% of 255

    # Simulate pitch-black 720-LED buffer
    black_pixels = [[0, 0, 0] for _ in range(NUM_LEDS)]

    def apply_illumination_guard(buffer):
        total_lum = sum((0.299 * r + 0.587 * g + 0.114 * b) for r, g, b in buffer)
        avg_lum = total_lum / len(buffer)

        if avg_lum < MIN_LUMEN_THRESHOLD:
            deficit = MIN_LUMEN_THRESHOLD - avg_lum
            boost_r = int(min(255, deficit * 1.48))
            boost_g = int(min(255, deficit * 0.88))
            boost_b = int(min(255, deficit * 0.28))
            for i in range(len(buffer)):
                buffer[i][0] = min(255, buffer[i][0] + boost_r)
                buffer[i][1] = min(255, buffer[i][1] + boost_g)
                buffer[i][2] = min(255, buffer[i][2] + boost_b)

    apply_illumination_guard(black_pixels)
    post_lum = calculate_perceptual_luminance(black_pixels)
    assert post_lum >= MIN_LUMEN_THRESHOLD * 0.95, f"Post-guard luminance too low: {post_lum}"
    print(f"  ✓ 720-LED pitch-black frame boosted to {post_lum:.1f}/255 ({post_lum/255*100:.1f}%)")

    # Test Guard on Low Flame Ember frame
    low_flame = [[40, 10, 0] for _ in range(NUM_LEDS)]
    apply_illumination_guard(low_flame)
    post_flame_lum = calculate_perceptual_luminance(low_flame)
    assert post_flame_lum >= MIN_LUMEN_THRESHOLD * 0.95
    print(f"  ✓ 720-LED ember frame boosted to {post_flame_lum:.1f}/255 ({post_flame_lum/255*100:.1f}%)")
    print("\n[SUCCESS] Ambient Illumination Guard Tests Passed Successfully!\n")


def main():
    if "--test-protocol" in sys.argv:
        test_protocol()
    elif "--test-threshold" in sys.argv:
        test_threshold_guard()
    else:
        test_protocol()
        test_threshold_guard()


if __name__ == "__main__":
    main()
