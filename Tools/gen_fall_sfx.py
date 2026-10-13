"""Original synthetic sounds for the fall-to-Chult cutscene; no recordings or dependencies.

python Tools/gen_fall_sfx.py   (then Tools/import_fall_cutscene.py imports them)

SFX_FallWind_00   16 s seamless loop: a soft rush of air with slow gusts (the
                  cutscene raises it with his speed; it never dominates the score)
SFX_LeafCrash_00..02  about 0.6 s: a thrash of leaves with a dull branch knock,
                  for his passage down through the canopy
Mono 48 kHz 16-bit, peaks at -3 dBFS, seeded. Hashes in fall-manifest.json.
"""
import hashlib, json, math, random, struct, wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
ROOT.mkdir(parents=True, exist_ok=True)
RATE = 48000
entries = []


def write(name, samples):
    peak = max(abs(x) for x in samples) or 1
    pcm = [int(x / peak * 23197) for x in samples]
    path = ROOT / f'{name}.wav'
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        out.writeframes(struct.pack('<' + 'h' * len(pcm), *pcm))
    entries.append(dict(name=name, file=path.name, duration=round(len(pcm) / RATE, 3),
                        sha256=hashlib.sha256(path.read_bytes()).hexdigest(), peak_dbfs=-3.0))
    print('CHUCK_FALL_SFX', name, round(len(pcm) / RATE, 2))


def wind():
    rng = random.Random(91711)
    length, overlap = 16.0, 2.0
    n = int(RATE * (length + overlap))
    low = band = high = 0.0
    gusts = [(rng.uniform(.05, .16), rng.uniform(0, 2 * math.pi)) for _ in range(3)]
    raw = []
    for i in range(n):
        t = i / RATE
        x = rng.uniform(-1, 1)
        low += .02 * (x - low)          # body of the rush
        band += .12 * (x - band)
        high = band - low               # a breathier band above it
        # Gust periods chosen to repeat over the loop length.
        g = 1 + sum(.28 * math.sin(2 * math.pi * round(f * length) / length * t + p) for f, p in gusts)
        whistle = .04 * math.sin(2 * math.pi * (510 + 40 * math.sin(2 * math.pi * t / 8)) * t) * max(0, g - 1.1)
        raw.append((low * 3.2 + high * .9) * g + whistle * low * 8)
    # Crossfade the overlap back into the start so the loop is seamless.
    k = int(RATE * overlap); body = int(RATE * length)
    out = raw[:body]
    for i in range(k):
        a = i / k
        out[i] = raw[body + i] * (1 - a) + raw[i] * a
    write('SFX_FallWind_00', out)


def crash(variant):
    rng = random.Random(55130 + variant)
    dur = .55 + .06 * variant
    n = int(RATE * dur)
    clicks = sorted(rng.uniform(0, dur * .8) for _ in range(140))
    knock_at = rng.uniform(.04, .12)
    lp = hp_prev = hp = 0.0
    s = []
    ci = 0; active = []
    for i in range(n):
        t = i / RATE
        while ci < len(clicks) and clicks[ci] <= t:
            active.append([clicks[ci], rng.uniform(.004, .012), rng.uniform(.3, 1.)]); ci += 1
        x = rng.uniform(-1, 1)
        hp = .9 * (hp + x - hp_prev); hp_prev = x
        crinkle = sum(a * math.exp(-(t - st) / d) for st, d, a in active if t - st < 6 * d) * hp
        active = [c for c in active if t - c[0] < 6 * c[1]]
        swish = x * .25 * math.exp(-t * 5) * (1 - math.exp(-t * 60))
        lp += .01 * (x - lp)
        knock = 0.0
        if t >= knock_at:
            tk = t - knock_at
            knock = (math.sin(2 * math.pi * 95 * tk) * .6 + lp * 6) * math.exp(-tk * 22)
        s.append((crinkle * .5 + swish + knock * .8) * min(1, (dur - t) / .06))
    write(f'SFX_LeafCrash_{variant:02d}', s)


wind()
for v in range(3): crash(v)
(ROOT / 'fall-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthetic wind and leaf crashes; Tools/gen_fall_sfx.py', sounds=entries), indent=2) + '\n')
print('CHUCK_FALL_SFX_GENERATED', len(entries))
