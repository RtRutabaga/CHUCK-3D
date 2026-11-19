"""The Feywild's waterfall: an original synthetic loop, no recordings and no dependencies (Claude, 2026-11-18).

py -3 Tools/gen_feywild_sfx.py  ->  SourceAssets/Audio/SFX/SFX_FeyFalls_00.wav and feywild-manifest.json
Layers of filtered noise (the deep body, the rush, the hiss) with slow swells and sparse gurgles; the end is crossfaded into the start.
"""
import hashlib, json, math, random, struct, wave
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
rng = random.Random(51177)
rate, seconds, fade = 48000, 16, 48000 * 2
count = rate * seconds + fade
low = mid_a = mid_b = high = 0.
gurgles = [(rng.uniform(0, seconds + 2), rng.uniform(.05, .16), rng.uniform(240, 620), rng.uniform(.3, 1.)) for _ in range(90)]
gurgles.sort(); active = []; next_g = 0
out = []
for i in range(count):
    t = i / rate; n = rng.uniform(-1, 1)
    low += .012 * (n - low); mid_a += .09 * (n - mid_a); mid_b += .33 * (n - mid_b); high = n - mid_b
    swell = 1 + .16 * math.sin(2 * math.pi * t * 3 / seconds) + .1 * math.sin(2 * math.pi * t * 7 / seconds + 1.3)
    flutter = 1 + .12 * math.sin(2 * math.pi * t * 41 / seconds + 2.1) * math.sin(2 * math.pi * t * 67 / seconds)
    value = (low * 3.4 * swell + (mid_a - low) * 1.25 * flutter + (mid_b - mid_a) * .62 * swell + high * .075)
    while next_g < len(gurgles) and gurgles[next_g][0] <= t: active.append(gurgles[next_g]); next_g += 1
    for g in active[:]:
        age = t - g[0]
        if age > g[1]: active.remove(g); continue
        value += .05 * g[3] * math.sin(2 * math.pi * g[2] * age * (1 + 1.4 * age)) * math.sin(math.pi * age / g[1])
    out.append(value)
loop = out[:rate * seconds]
for i in range(fade):
    k = i / fade; loop[i] = out[rate * seconds + i] * math.cos(k * math.pi / 2) + loop[i] * math.sin(k * math.pi / 2)
peak = max(abs(x) for x in loop)
pcm = [int(x / peak * 23197) for x in loop]
name = 'SFX_FeyFalls_00'; path = root / (name + '.wav')
with wave.open(str(path), 'wb') as f:
    f.setparams((1, 2, rate, 0, 'NONE', 'not compressed')); f.writeframes(struct.pack('<' + 'h' * len(pcm), *pcm))
entry = dict(name=name, file=path.name, duration=len(pcm) / rate, sha256=hashlib.sha256(path.read_bytes()).hexdigest(), peak_dbfs=-3.0)
(root / 'feywild-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthetic waterfall loop; Tools/gen_feywild_sfx.py', sounds=[entry]), indent=2) + '\n')
print('CHUCK_FEY_SFX_GENERATED', entry['duration'])
