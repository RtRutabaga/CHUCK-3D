"""Original synthetic sounds for Chuck climbing bark (Claude, 2026-10-16); no recordings or dependencies.

python -I Tools/gen_climb_sfx.py   (then Tools/import_climb_sfx.py imports them)

SFX_ClimbBark_00..05  ~0.16 s: one paw catching bark on the way up: a soft pad
                      thump, the click of claws biting, a short dry scrape as
                      they drag and catch on the fibres, and a few crumbs of
                      bark. Played at each paw contact of the wall-run cycle.
Mono 48 kHz 16-bit, peaks normalised to -3 dBFS, seeded. Hashes in climb-manifest.json.
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
    print('CHUCK_CLIMB_SFX', name, round(len(pcm) / RATE, 3))


class OnePole:
    def __init__(self, k): self.k = k; self.y = 0.
    def __call__(self, x): self.y += self.k * (x - self.y); return self.y


class Resonator:
    """A two-pole band-pass at f Hz with bandwidth bw Hz."""
    def __init__(self, f, bw):
        r = math.exp(-math.pi * bw / RATE)
        self.a1 = -2 * r * math.cos(2 * math.pi * f / RATE); self.a2 = r * r; self.g = 1 - r
        self.y1 = self.y2 = 0.
    def __call__(self, x):
        y = self.g * x - self.a1 * self.y1 - self.a2 * self.y2
        self.y2, self.y1 = self.y1, y
        return y


def env(t, attack, decay):
    return t / attack if t < attack else math.exp(-(t - attack) / decay)


def bark(variant):
    rng = random.Random(4410 + variant * 37)
    dur = .16 + .02 * (variant % 3); n = int(RATE * dur)
    # Pad: a muffled low knock on the trunk.
    pad_f = 170 + 25 * rng.random(); pad = Resonator(pad_f, 90)
    # Claws: a bright click, then a scrape whose band slides down as they drag.
    click = Resonator(3600 + 900 * rng.random(), 1800)
    scrape_lo = 2600 + 500 * rng.random()
    scrape = [Resonator(scrape_lo, 900), Resonator(scrape_lo * 1.9, 1500)]
    smooth = OnePole(.5)
    drag_start = .008 + .006 * rng.random(); drag_len = .07 + .04 * rng.random()
    # The claws catch and release on the fibres: a stuttering, uneven grain.
    catches = sorted(rng.uniform(drag_start, drag_start + drag_len) for _ in range(5 + variant % 3))
    crumbs = [(rng.uniform(.04, dur - .03), rng.uniform(.3, .7)) for _ in range(3 + variant % 2)]
    crumb_rs = Resonator(1800 + 600 * rng.random(), 1200)
    out = []
    grain = 1.
    for i in range(n):
        t = i / RATE
        x = rng.uniform(-1, 1)
        thump = pad(x) * env(t, .002, .025) * 3.
        tick = click(x) * env(t, .0005, .006) * 2.2
        s = 0.
        if t >= drag_start:
            u = (t - drag_start) / drag_len
            if u < 1:
                if rng.random() < .004: grain = rng.uniform(.35, 1.)
                hit = sum(math.exp(-((t - c) / .0025) ** 2) for c in catches)
                level = math.sin(math.pi * min(1., u)) ** .7 * grain * (.6 + .9 * hit)
                bands = scrape[0](x) * (1 - .35 * u) + .55 * scrape[1](x)
                s = smooth(bands) * level * 4.5
        c = 0.
        for at, amp in crumbs:
            if 0 <= t - at < .004: c += crumb_rs(rng.uniform(-1, 1)) * amp * env(t - at, .0003, .0015) * 3
        tail = min(1., (dur - t) / .02)   # no click at the end
        out.append((thump + tick + s + c) * tail)
    write(f'SFX_ClimbBark_{variant:02d}', out)


for v in range(6): bark(v)
(ROOT / 'climb-manifest.json').write_text(json.dumps(dict(
    generator='Tools/gen_climb_sfx.py', sample_rate=RATE, license='original synthetic (Claude, CHUCK 3D)', sounds=entries), indent=1))
