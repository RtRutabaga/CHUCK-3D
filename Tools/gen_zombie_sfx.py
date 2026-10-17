"""Original seeded zombie/body-contact synthesis; Python standard library only.
Superseded 2026-10-16 by Tools/build_zombie_sfx.py (recorded CC0 sources); running this overwrites those WAVs.

No voice for Chuck, recordings, samples, or third-party dependencies.
"""
import hashlib
import json
import math
import random
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
RATE = 48000
entries = []
for kind, duration, count in [('Rasp', 1.05, 3), ('Swing', .32, 2),
                              ('ClawHit', .23, 3), ('ChuckHit', .3, 3), ('Death', 1.3, 1)]:
    for variant in range(count):
        rng = random.Random(814 + len(entries))
        samples = []
        low = mid = phase = 0.
        for i in range(int(duration * RATE)):
            t = i / RATE
            noise = rng.uniform(-1, 1)
            low += .045 * (noise - low)
            mid += .32 * (noise - mid)
            if kind in ('Rasp', 'Death'):
                # Unvoiced breath with a rough, falling chest resonance.
                phase += 2 * math.pi * (65 + 13 * variant) * (1 - .38 * t / duration) / RATE
                envelope = min(1, t / .06) * (1 - t / duration) ** 1.5
                value = envelope * (low * 3 + .17 * math.sin(phase) * (1 + 2 * mid))
            elif kind == 'Swing':
                envelope = math.sin(math.pi * t / duration) ** 2
                value = (mid - low) * envelope * .8
            else:
                # Body thump under cloth/skin scrape: claws sharper, received blows lower.
                claw = kind == 'ClawHit'
                phase += 2 * math.pi * (155 if claw else 95) * math.exp(-t * 8) / RATE
                envelope = min(1, t / .002)
                value = envelope * (.55 * math.sin(phase) * math.exp(-t * 32)
                    + (mid - low) * (1.2 if claw else .55) * math.exp(-t * (36 if claw else 25))
                    + low * 1.5 * math.exp(-t * 20))
            samples.append(value * min(1, (duration - t) / .015))
        peak = max(map(abs, samples))
        name = f'SFX_Zombie{kind}_{variant:02d}'
        path = ROOT / f'{name}.wav'
        with wave.open(str(path), 'wb') as out:
            out.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
            out.writeframes(struct.pack('<' + 'h' * len(samples),
                                       *(int(v / peak * 23197) for v in samples)))
        entries.append(dict(name=name, duration=duration, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
(ROOT / 'zombie-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthesis; no samples', sounds=entries), indent=2) + '\n')
print(f'CHUCK_ZOMBIE_SFX generated={len(entries)} bytes={sum((ROOT / (e["name"] + ".wav")).stat().st_size for e in entries)}')
