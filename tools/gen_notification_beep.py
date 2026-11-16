"""Generate the notification beep of BattleRoyale (an original synthesized sound, no third-party audio).

A short rising two-note chime: E6 then B6, each a sine with a soft second harmonic, 4 ms attack and an exponential decay,
380 ms in all with a fade-out tail, peak at 0.6 of full scale. Writes a 44.1 kHz mono 16-bit WAV next to the output and
encodes it to Ogg Vorbis with ffmpeg (native vorbis encoder).

Usage: python -I tools/gen_notification_beep.py <output.ogg>
"""
import math
import os
import shutil
import struct
import subprocess
import sys
import wave

RATE = 44100
LENGTH_S = 0.38
PEAK = 0.6
NOTES = (  # (frequency Hz, start s, decay time constant s)
    (1318.51, 0.000, 0.070),
    (1975.53, 0.075, 0.110),
)
ATTACK_S = 0.004
FADE_OUT_S = 0.030


def sample(t):
    value = 0.0
    for freq, start, tau in NOTES:
        local = t - start
        if local < 0:
            continue
        env = min(1.0, local / ATTACK_S) * math.exp(-local / tau)
        value += env * (math.sin(2 * math.pi * freq * local) + 0.12 * math.sin(2 * math.pi * 2 * freq * local))
    return value


def main():
    out_ogg = os.path.abspath(sys.argv[1])
    frames = int(RATE * LENGTH_S)
    raw = [sample(i / RATE) for i in range(frames)]
    fade_from = frames - int(RATE * FADE_OUT_S)
    for i in range(fade_from, frames):
        raw[i] *= (frames - i) / (frames - fade_from)
    scale = PEAK / max(abs(v) for v in raw)
    pcm = b"".join(struct.pack("<h", int(round(v * scale * 32767))) for v in raw)

    wav_path = os.path.splitext(out_ogg)[0] + ".wav"
    os.makedirs(os.path.dirname(out_ogg), exist_ok=True)
    with wave.open(wav_path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm)

    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        sys.exit("ffmpeg not found on PATH")
    subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", wav_path, "-c:a", "vorbis", "-strict",
                    "experimental", "-ac", "2", "-q:a", "6", out_ogg], check=True)
    os.remove(wav_path)
    print("wrote", out_ogg, os.path.getsize(out_ogg), "bytes,", frames, "samples")


if __name__ == "__main__":
    main()
