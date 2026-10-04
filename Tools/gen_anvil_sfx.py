"""Original synthetic anvil strikes for the plaza blacksmith (user 2026-10-04); no
external recordings or dependencies.

python Tools/gen_anvil_sfx.py -> SourceAssets/Audio/SFX/SFX_AnvilStrike_00..03.wav (+ anvil-manifest.json)

A hammer on hot iron on an anvil: a hard, short contact click, the soft iron's
dull thud, and the anvil body's bright inharmonic ring (a bar's partials,
roughly 1 : 2.76 : 5.40 : 8.93), damped quickly by the stump. Each variant
detunes and reweights the partials a little. Seeded: reproducible.
"""
import hashlib, json, math, random, struct, wave
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
root.mkdir(parents=True, exist_ok=True)
RATE = 48000
entries = []
for variant in range(4):
    rng = random.Random(51207 + variant)
    duration = .9 + .08 * variant
    base = 1180 * rng.uniform(.96, 1.05)
    partials = [(base * r * rng.uniform(.985, 1.015), a * rng.uniform(.8, 1.2), d * rng.uniform(.85, 1.15))
                for r, a, d in ((1., .55, .42), (2.76, .42, .26), (5.40, .3, .14), (8.93, .16, .07), (13.3, .07, .04))]
    samples = []
    low = 0.
    for i in range(int(RATE * duration)):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        low += .18 * (noise - low)
        click = noise * math.exp(-t * 900) * .9                     # hard steel contact
        thud = low * 1.6 * (1 - math.exp(-t * 2500)) * math.exp(-t * 55)   # the soft hot bar
        ring = sum(a * math.sin(2 * math.pi * f * t + k) * math.exp(-t / d) for k, (f, a, d) in enumerate(partials))
        ring *= 1 - math.exp(-t * 4000)
        samples.append((click + thud + ring) * min(1., (duration - t) / .05))
    peak = max(abs(x) for x in samples)
    pcm = [int(x / peak * 23197) for x in samples]   # -3 dBFS
    name = f'SFX_AnvilStrike_{variant:02d}'
    path = root / (name + '.wav')
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        out.writeframes(struct.pack('<' + 'h' * len(pcm), *pcm))
    entries.append(dict(name=name, file=path.name, duration=round(len(pcm) / RATE, 3),
                        sha256=hashlib.sha256(path.read_bytes()).hexdigest(), peak_dbfs=-3.0))
(root / 'anvil-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthetic anvil strikes; Tools/gen_anvil_sfx.py', sounds=entries), indent=2) + '\n')
print('CHUCK_ANVIL_WAV_GENERATED', len(entries))
