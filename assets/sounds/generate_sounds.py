#!/usr/bin/env python3
"""Generates the client's sound effects (16-bit mono WAV, 22050 Hz).

They are synthesized here instead of downloaded, so their origin and licence
are clear. Run from this folder: python3 generate_sounds.py
"""

import math
import random
import struct
import wave

RATE = 22050


def save(name, samples):
    with wave.open(name, "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 12000)) for s in samples))


def tone(seconds, start_hz, end_hz, square=False, noise=0.0):
    """A pitch slide from start_hz to end_hz that fades out."""
    count = int(seconds * RATE)
    phase = 0.0
    samples = []
    for i in range(count):
        progress = i / count
        phase += 2 * math.pi * (start_hz + (end_hz - start_hz) * progress) / RATE
        value = math.sin(phase)
        if square:
            value = 1.0 if value >= 0 else -1.0
        value = value * (1 - noise) + random.uniform(-1, 1) * noise
        samples.append(value * (1 - progress) ** 2)
    return samples


random.seed(42)  # Same files on every run
save("shot.wav", tone(0.12, 1400, 500, square=True))
save("explosion.wav", tone(0.45, 160, 40, noise=0.85))
save("player_down.wav", tone(0.8, 520, 90, square=True, noise=0.3))
save("player_joined.wav", tone(0.12, 660, 660) + tone(0.18, 990, 990))
