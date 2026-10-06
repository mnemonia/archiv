#!/usr/bin/env python3
"""
===============================================================================
Virtual NeoPixel Strip Protocol Tester & Simulation Harness
===============================================================================
Automated verification tests for the 60-LED Arduino Serial protocol (Implementation A),
optical framing, XOR checksum integrity, and minimum luminance guard validation.

Usage:
  python3 simulator.py --test-protocol
  python3 simulator.py --test-threshold
===============================================================================
"""

import sys
import math
import random

NUM_LEDS = 60
PAYLOAD_SIZE = NUM_LEDS * 3
TOTAL_FRAME_SIZE = 4 + PAYLOAD_SIZE + 1  # 185 bytes


def build_frame(pixels):
    """Encodes a list of 60 (R, G, B) tuples into the Implementation A binary packet."""
    assert len(pixels) == NUM_LEDS, f"Expected {NUM_LEDS} pixels, got {len(pixels)}"

    header = bytes([0xAA, 0x55, 0x01, NUM_LEDS])
    payload = bytearray()
    for r, g, b in pixels:
        payload.append(int(r) & 0xFF)
        payload.append(int(g) & 0xFF)
        payload.append(int(b) & 0xFF)

    # Calculate XOR checksum
    chk = 0xAA ^ 0x55 ^ 0x01 ^ NUM_LEDS
    for byte_val in payload:
        chk ^= byte_val

    return header + bytes(payload) + bytes([chk])


def decode_frame(raw_bytes):
    """Decodes and validates a raw 185-byte Implementation A packet."""
    if len(raw_bytes) != TOTAL_FRAME_SIZE:
        return None, f"Invalid packet length: {len(raw_bytes)} (expected {TOTAL_FRAME_SIZE})"

    if raw_bytes[0] != 0xAA or raw_bytes[1] != 0x55:
        return None, f"Invalid sync bytes: {hex(raw_bytes[0])} {hex(raw_bytes[1])}"

    cmd, count = raw_bytes[2], raw_bytes[3]
    if cmd != 0x01:
        return None, f"Invalid command byte: {cmd}"
    if count != NUM_LEDS:
        return None, f"Invalid LED count: {count}"

    payload = raw_bytes[4:4 + PAYLOAD_SIZE]
    expected_chk = raw_bytes[-1]

    calc_chk = 0xAA ^ 0x55 ^ cmd ^ count
    for b in payload:
        calc_chk ^= b

    if calc_chk != expected_chk:
        return None, f"Checksum error: calculated {hex(calc_chk)}, expected {hex(expected_chk)}"

    pixels = []
    for i in range(0, len(payload), 3):
        pixels.append((payload[i], payload[i + 1], payload[i + 2]))

    return pixels, "OK"


def calculate_perceptual_luminance(pixels):
    """Calculates average perceptual luminance (0-255 scale)."""
    total = sum((0.299 * r + 0.587 * g + 0.114 * b) for r, g, b in pixels)
    return total / len(pixels)


def test_protocol():
    """Runs automated unit tests for binary framing and checksum verification."""
    print("[TEST] Running Protocol Framing & Checksum Tests...")

    # Test 1: Uniform warm-white frame
    test_pixels_1 = [(255, 148, 38) for _ in range(NUM_LEDS)]
    packet = build_frame(test_pixels_1)
    assert len(packet) == TOTAL_FRAME_SIZE, f"Length mismatch: {len(packet)}"

    decoded, status = decode_frame(packet)
    assert status == "OK", f"Decode failed: {status}"
    assert decoded == test_pixels_1, "Decoded pixels do not match original"
    print("  ✓ Test 1 Passed: Warm-white frame encode/decode verified.")

    # Test 2: Multi-color gradient frame
    test_pixels_2 = [(i * 4, 255 - i * 4, (i * 7) % 256) for i in range(NUM_LEDS)]
    packet_2 = build_frame(test_pixels_2)
    decoded_2, status_2 = decode_frame(packet_2)
    assert status_2 == "OK", f"Decode failed: {status_2}"
    assert decoded_2 == test_pixels_2, "Gradient pixels mismatch"
    print("  ✓ Test 2 Passed: Dynamic gradient frame verified.")

    # Test 3: Corrupted Checksum Rejection
    corrupted_packet = bytearray(packet_2)
    corrupted_packet[-1] ^= 0xFF  # Invert checksum
    decoded_3, status_3 = decode_frame(bytes(corrupted_packet))
    assert decoded_3 is None, "Corrupted packet was unexpectedly accepted"
    assert "Checksum error" in status_3, f"Expected checksum error, got: {status_3}"
    print("  ✓ Test 3 Passed: Corrupted packet rejection verified.")

    # Test 4: Truncated Packet Rejection
    truncated_packet = packet_2[:100]
    decoded_4, status_4 = decode_frame(truncated_packet)
    assert decoded_4 is None, "Truncated packet was unexpectedly accepted"
    print("  ✓ Test 4 Passed: Truncated packet rejection verified.")

    print("\n[SUCCESS] All Protocol Framing Tests Passed Successfully!\n")


def test_threshold_guard():
    """Simulates the Arduino Ambient Illumination Guard algorithm."""
    print("[TEST] Running Ambient Illumination Guard Algorithm Tests...")
    MIN_LUMEN_THRESHOLD = 88 # ~35% of 255

    # Simulate completely black input buffer
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

    # Test Guard on Pitch Black frame
    apply_illumination_guard(black_pixels)
    post_lum = calculate_perceptual_luminance(black_pixels)
    assert post_lum >= MIN_LUMEN_THRESHOLD * 0.95, f"Post-guard luminance too low: {post_lum}"
    print(f"  ✓ Pitch-black frame successfully boosted to {post_lum:.1f}/255 ({post_lum/255*100:.1f}%)")

    # Test Guard on Low Flame Ember frame
    low_flame = [[40, 10, 0] for _ in range(NUM_LEDS)]
    apply_illumination_guard(low_flame)
    post_flame_lum = calculate_perceptual_luminance(low_flame)
    assert post_flame_lum >= MIN_LUMEN_THRESHOLD * 0.95, f"Post-guard flame luminance too low: {post_flame_lum}"
    print(f"  ✓ Low-ember flame frame boosted to {post_flame_lum:.1f}/255 ({post_flame_lum/255*100:.1f}%)")

    print("\n[SUCCESS] Ambient Illumination Guard Tests Passed Successfully!\n")


def main():
    if "--test-threshold" in sys.argv:
        test_threshold_guard()
    elif "--test-protocol" in sys.argv or len(sys.argv) == 1:
        test_protocol()
        test_threshold_guard()
    else:
        print("Unknown argument. Use --test-protocol or --test-threshold.")


if __name__ == "__main__":
    main()
