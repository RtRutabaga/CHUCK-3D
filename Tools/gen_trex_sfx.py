"""Original synthetic sounds for the Chult T-rex (Claude, 2026-10-15); no recordings or dependencies.
After gen_raptor_sfx.py, an octave and more lower and far heavier.

python -I Tools/gen_trex_sfx.py   (then Tools/import_trex_sfx.py imports them)

SFX_TRexStep_00..02   ~0.9 s: a footfall that shakes the ground: a deep sub thump,
                      a long rumble and a crunch of crushed litter
SFX_TRexRoar_00..01   ~2.4 s: a rumbling, rasping roar that swells and falls
SFX_TRexSnap_00       ~0.35 s: huge jaws closing: a deep clack and a tear
SFX_TRexGrowl_00      ~1.8 s: a low, breathy rumble (stalking under the trees)
SFX_TRexDeath_00      ~2.6 s: a roar that sinks and breaks off (not used yet)
Mono 48 kHz 16-bit, peaks normalised to -3 dBFS, seeded. Hashes in trex-manifest.json.
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
    print('CHUCK_TREX_SFX', name, round(len(pcm) / RATE, 2))


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
    rng = random.Random(8801 + variant)
    dur = .9; n = int(RATE * dur)
    lp = OnePole(.02); hp_lo = OnePole(.004); rumble = OnePole(.003)
    f0 = 44 + 5 * variant; phase = 0.
    out = []
    for i in range(n):
        t = i / RATE
        f = f0 * (1 - .5 * min(1, t / .25))
        phase += 2 * math.pi * f / RATE
        thump = math.sin(phase) * env(t, .006, .18)
        x = rng.uniform(-1, 1)
        low = rumble(x) * env(t, .01, .35) * 9
        band = lp(x) - hp_lo(x)
        crunch = band * env(t, .003, .06) * (1 + .6 * rng.random()) * 1.6
        out.append(thump * 2.2 + low + crunch)
    write(f'SFX_TRexStep_{variant:02d}', out)


def roar(name, dur, f_hi, f_lo, seed, depth, fall=False):
    rng = random.Random(seed)
    n = int(RATE * dur)
    formants = [Resonator(330, 120), Resonator(820, 200), Resonator(1650, 380), Resonator(2900, 700)]
    phase = 0.; noise_lp = OnePole(.12); sub = 0.
    out = []
    for i in range(n):
        t = i / RATE; u = t / dur
        f = f_lo + (f_hi - f_lo) * math.sin(math.pi * min(1, u * 1.15)) ** .7 * (1 - .5 * u) + 6 * math.sin(2 * math.pi * 4 * t)
        phase += 2 * math.pi * f / RATE; sub += 2 * math.pi * f * .5 / RATE
        saw = ((phase / (2 * math.pi)) % 1) * 2 - 1
        am = .5 + .5 * math.sin(2 * math.pi * depth * t + rng.uniform(0, .3))      # the rasp
        src = saw * am + noise_lp(rng.uniform(-1, 1)) * 1.2
        y = sum(r(src) for r in formants) + math.sin(sub) * .35
        shape = min(1, t / .25) * (1 - u) ** .5 * (1 if u < .82 or not fall else max(0, 1 - (u - .82) / .1))
        out.append(y * shape)
    write(name, out)


def snap():
    rng = random.Random(9931)
    dur = .35; n = int(RATE * dur)
    res = [Resonator(700, 260), Resonator(1500, 500), Resonator(160, 60)]
    out = []
    for i in range(n):
        t = i / RATE
        x = rng.uniform(-1, 1)
        click = env(t, .0008, .02) + .6 * (env(t - .07, .0008, .02) if t >= .07 else 0)
        y = sum(r(x * click) for r in res) * 7
        tear = x * env(t, .02, .09) * .3
        out.append(y + tear)
    write('SFX_TRexSnap_00', out)


def growl():
    rng = random.Random(5512)
    dur = 1.8; n = int(RATE * dur)
    formants = [Resonator(260, 90), Resonator(600, 160)]
    phase = 0.; lo = OnePole(.05)
    out = []
    for i in range(n):
        t = i / RATE; u = t / dur
        phase += 2 * math.pi * (52 + 8 * math.sin(2 * math.pi * .7 * t)) / RATE
        saw = ((phase / (2 * math.pi)) % 1) * 2 - 1
        am = .55 + .45 * math.sin(2 * math.pi * 17 * t)
        breath = lo(rng.uniform(-1, 1)) * 2.5
        y = sum(r(saw * am + breath) for r in formants)
        out.append(y * math.sin(math.pi * u) ** .8)
    write('SFX_TRexGrowl_00', out)


for v in range(3): step(v)
roar('SFX_TRexRoar_00', 2.4, 165, 70, 6611, 23)
roar('SFX_TRexRoar_01', 2.0, 185, 80, 6612, 27)
snap()
growl()
roar('SFX_TRexDeath_00', 2.6, 140, 40, 6613, 15, fall=True)
(ROOT / 'trex-manifest.json').write_text(json.dumps({'generator': 'Tools/gen_trex_sfx.py', 'license': 'original, synthetic (Claude for CHUCK 3D)', 'sounds': entries}, indent=2))
print('CHUCK_TREX_SFX_READY', len(entries))
