"""Original synthetic sounds for the Chult raptor (Claude, 2026-10-14); no recordings or dependencies.
Superseded 2026-10-16 by Tools/build_dino_sfx.py (recorded CC0 sources); running this overwrites those WAVs.

python -I Tools/gen_raptor_sfx.py   (then Tools/import_raptor_sfx.py imports them)

SFX_RaptorStep_00..02  ~0.3 s: a heavy clawed footfall: a low thump that drops in
                       pitch, under a short crunch of leaf litter
SFX_RaptorHiss_00      ~0.5 s: the tell before a lunge: a swelling breathy hiss
                       with a low rasp under it
SFX_RaptorSnap_00..01  ~0.2 s: jaws closing: two sharp clacks and a short tear
SFX_RaptorCall_00      ~0.9 s: noticing him: a rasping, falling chirp-growl
SFX_RaptorDeath_00     ~1.4 s: a lower call that sinks and breaks off
Mono 48 kHz 16-bit, peaks normalised to -3 dBFS, seeded. Hashes in raptor-manifest.json.
"""
import hashlib, json, math, random, struct, wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
ROOT.mkdir(parents=True, exist_ok=True)
RATE = 48000
entries = []


def write(name, samples):
    peak = max(abs(x) for x in samples) or 1
    pcm = [int(max(-1., min(1., x / peak)) * 23197) for x in samples]
    path = ROOT / f'{name}.wav'
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        out.writeframes(struct.pack('<' + 'h' * len(pcm), *pcm))
    entries.append(dict(name=name, file=path.name, duration=round(len(pcm) / RATE, 3),
                        sha256=hashlib.sha256(path.read_bytes()).hexdigest(), peak_dbfs=-3.0))
    print('CHUCK_RAPTOR_SFX', name, round(len(pcm) / RATE, 2))


class OnePole:
    def __init__(self, k): self.k = k; self.y = 0.
    def __call__(self, x): self.y += self.k * (x - self.y); return self.y


class Resonator:
    """A two-pole band-pass (formant) at f Hz with bandwidth bw Hz."""
    def __init__(self, f, bw):
        r = math.exp(-math.pi * bw / RATE)
        self.a1 = -2 * r * math.cos(2 * math.pi * f / RATE); self.a2 = r * r; self.g = 1 - r
        self.y1 = self.y2 = 0.
    def __call__(self, x):
        y = self.g * x - self.a1 * self.y1 - self.a2 * self.y2
        self.y2, self.y1 = self.y1, y
        return y


def env(t, attack, decay):
    return (t / attack if t < attack else math.exp(-(t - attack) / decay))


def step(variant):
    rng = random.Random(7301 + variant)
    dur = .3; n = int(RATE * dur)
    lp = OnePole(.06); hp_lo = OnePole(.02)
    f0 = 78 + 9 * variant; phase = 0.
    out = []
    for i in range(n):
        t = i / RATE
        f = f0 * (1 - .45 * min(1, t / .12))
        phase += 2 * math.pi * f / RATE
        thump = math.sin(phase) * env(t, .004, .07)
        x = rng.uniform(-1, 1)
        band = lp(x) - hp_lo(x)                     # leaf-litter crunch
        crunch = band * env(t, .002, .035) * (1 + .6 * rng.random()) * 2.2
        clicks = (rng.random() < .004) * rng.uniform(-1, 1) * env(t, .001, .05)   # claws
        out.append(thump * 1.6 + crunch + clicks * .8)
    write(f'SFX_RaptorStep_{variant:02d}', out)


def hiss():
    rng = random.Random(4402)
    dur = .5; n = int(RATE * dur)
    hi = OnePole(.55); lo = OnePole(.18); rasp = 0.
    out = []
    for i in range(n):
        t = i / RATE
        x = rng.uniform(-1, 1)
        breath = hi(x) - lo(x)
        swell = min(1, t / .18) * (1 if t < .38 else max(0, 1 - (t - .38) / .12))
        rasp += 2 * math.pi * (95 + 25 * t) / RATE
        growl = (((rasp / (2 * math.pi)) % 1) * 2 - 1) * (.5 + .5 * math.sin(2 * math.pi * 31 * t)) * .35
        out.append((breath * 1.8 + growl * .5) * swell)
    write('SFX_RaptorHiss_00', out)


def snap(variant):
    rng = random.Random(9921 + variant)
    dur = .2; n = int(RATE * dur)
    res = [Resonator(1900 + 300 * variant, 500), Resonator(3600, 900)]
    out = []
    clack_at = [.0, .045 + .01 * variant]
    for i in range(n):
        t = i / RATE
        x = rng.uniform(-1, 1)
        click = sum(env(t - c, .0005, .008) if t >= c else 0 for c in clack_at)
        y = sum(r(x * click) for r in res) * 6
        tear = x * env(t, .01, .05) * .25
        out.append(y + tear)
    write(f'SFX_RaptorSnap_{variant:02d}', out)


def call(name, dur, f_hi, f_lo, seed, depth):
    rng = random.Random(seed)
    n = int(RATE * dur)
    formants = [Resonator(700, 180), Resonator(1500, 260), Resonator(2600, 420)]
    phase = 0.; noise_lp = OnePole(.3)
    out = []
    for i in range(n):
        t = i / RATE; u = t / dur
        f = f_lo + (f_hi - f_lo) * (1 - u) ** 1.6 + 18 * math.sin(2 * math.pi * 6 * t)
        phase += 2 * math.pi * f / RATE
        saw = ((phase / (2 * math.pi)) % 1) * 2 - 1
        am = .55 + .45 * math.sin(2 * math.pi * depth * t + rng.uniform(0, .2))   # the rasp
        src = saw * am + noise_lp(rng.uniform(-1, 1)) * .5
        y = sum(r(src) for r in formants)
        shape = min(1, t / .05) * (1 - u) ** .6 * (1 if u < .8 or name != 'SFX_RaptorDeath_00' else max(0, 1 - (u - .8) / .1))
        out.append(y * shape)
    write(name, out)


for v in range(3): step(v)
hiss()
for v in range(2): snap(v)
call('SFX_RaptorCall_00', .9, 980, 420, 3311, 38)
call('SFX_RaptorDeath_00', 1.4, 620, 160, 3312, 22)
(ROOT / 'raptor-manifest.json').write_text(json.dumps({'generator': 'Tools/gen_raptor_sfx.py', 'license': 'original, synthetic (Claude for CHUCK 3D)', 'sounds': entries}, indent=2))
print('CHUCK_RAPTOR_SFX_READY', len(entries))
