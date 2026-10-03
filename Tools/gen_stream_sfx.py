"""Original synthetic shallow-water paw steps; no external recordings or dependencies."""
import hashlib, json, math, random, struct, wave
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
root.mkdir(parents=True, exist_ok=True)
entries = []
for variant in range(6):
    rng = random.Random(73190 + variant)
    rate = 48000
    duration = .28 + variant * .018
    samples = []
    low = 0.0
    drops = [(rng.uniform(.025, .15), rng.uniform(700, 1700), rng.uniform(.035, .08)) for _ in range(9)]
    for i in range(int(rate * duration)):
        t = i / rate
        noise = rng.uniform(-1, 1)
        low += .085 * (noise - low)
        impact = (low * 2.4 + noise * .18) * (1 - math.exp(-t * 1300)) * math.exp(-t * 28)
        spray = noise * .14 * (1 - math.exp(-t * 180)) * math.exp(-t * 15)
        droplets = sum(.035 * math.sin(2 * math.pi * f * (t - start) * (1 - .8 * (t - start))) * math.exp(-(t - start) / decay)
                       for start, f, decay in drops if t >= start)
        samples.append((impact + spray + droplets) * min(1, (duration - t) / .035))
    peak = max(abs(x) for x in samples)
    pcm = [int(x / peak * 23197) for x in samples]
    name = f'SFX_StreamSplash_{variant:02d}'
    path = root / (name + '.wav')
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
        out.writeframes(struct.pack('<' + 'h' * len(pcm), *pcm))
    entries.append(dict(name=name, file=path.name, duration=len(pcm)/rate,
                        sha256=hashlib.sha256(path.read_bytes()).hexdigest(), peak_dbfs=-3.0))
(root / 'stream-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthetic shallow-water footsteps; Tools/gen_stream_sfx.py', sounds=entries), indent=2) + '\n')
print('CHUCK_STREAM_WAV_GENERATED', len(entries))
