"""Generate small deterministic audio assets using only Python's stdlib."""

from __future__ import annotations

import math
import struct
import wave
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "games" / "hardware-test" / "assets" / "edge-beep.wav"
SAMPLE_RATE = 48_000
DURATION_SECONDS = 0.075
FREQUENCY_HZ = 880.0


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    frame_count = round(SAMPLE_RATE * DURATION_SECONDS)
    with wave.open(str(OUTPUT), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for frame in range(frame_count):
            progress = frame / frame_count
            envelope = (1.0 - progress) ** 2
            sample = round(12_000 * envelope * math.sin(
                2.0 * math.pi * FREQUENCY_HZ * frame / SAMPLE_RATE
            ))
            frames.extend(struct.pack("<hh", sample, sample))
        output.writeframes(frames)
    print(OUTPUT)


if __name__ == "__main__":
    main()
