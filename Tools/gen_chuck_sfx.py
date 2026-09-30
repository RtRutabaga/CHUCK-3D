"""Synthesize Chuck's movement sound effects (2026-09-29 user request:
walking, running, jumping, slashing and rolling SFX suited to the game).

python Tools/gen_chuck_sfx.py   ->  SourceAssets/Audio/SFX/*.wav (+ manifest.json)

Pure Python (stdlib only), seeded, so the set is reproducible and carries no
third-party licence. Style: grounded and quiet under the docks soundtrack. A
65 cm rat on planks and cobbles: soft paw pads with a faint claw tick, never
cartoon boings; the jacket's cloth carries the jumps and rolls; the claw slash
is an air swish, no metal ring. Chuck is silent: no vocal efforts.
Timings follow the clip manifest (roll contacts, slash strike).
"""
import json
import math
import random
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets/Audio/SFX'
MANIFEST = ROOT / 'SourceAssets/Chuck/V1/Animations/manifest.json'
SR = 48000
PEAK = 10 ** (-3 / 20)          # files peak at -3 dBFS; the runtime sets the mix


class Biquad:
    """RBJ cookbook biquad; set() can retune it per sample (sweeps)."""
    def __init__(self, kind, freq, q=.707):
        self.kind = kind; self.x1 = self.x2 = self.y1 = self.y2 = 0.
        self.set(freq, q)

    def set(self, freq, q=.707):
        w = 2 * math.pi * min(freq, SR * .45) / SR
        c, s = math.cos(w), math.sin(w); a = s / (2 * q)
        if self.kind == 'lp':
            b = ((1 - c) / 2, 1 - c, (1 - c) / 2)
        elif self.kind == 'hp':
            b = ((1 + c) / 2, -(1 + c), (1 + c) / 2)
        else:  # band-pass, 0 dB peak
            b = (a, 0., -a)
        a0 = 1 + a
        self.b = [v / a0 for v in b]; self.a = (-2 * c / a0, (1 - a) / a0)

    def __call__(self, x):
        b0, b1, b2 = self.b; a1, a2 = self.a
        y = b0 * x + b1 * self.x1 + b2 * self.x2 - a1 * self.y1 - a2 * self.y2
        self.x2, self.x1, self.y2, self.y1 = self.x1, x, self.y1, y
        return y


def filt(sig, *stages):
    out = sig
    for kind, freq, q in stages:
        f = Biquad(kind, freq, q)
        out = [f(x) for x in out]
    return out


def buf(seconds):
    return [0.] * int(seconds * SR)


def add(dst, src, at, gain=1.):
    start = int(at * SR)
    for i, v in enumerate(src):
        if 0 <= start + i < len(dst):
            dst[start + i] += v * gain


def noise(rng, seconds):
    return [rng.uniform(-1, 1) for _ in range(int(seconds * SR))]


def env(seconds, attack, decay, hold=0.):
    """Linear attack, optional hold, exponential decay (time constant `decay`)."""
    out = []
    for i in range(int(seconds * SR)):
        t = i / SR
        if t < attack:
            out.append(t / attack)
        elif t < attack + hold:
            out.append(1.)
        else:
            out.append(math.exp(-(t - attack - hold) / decay))
    return out


def mul(a, b):
    return [x * y for x, y in zip(a, b)]


def modes(rng, seconds, freqs, decays, amps):
    """Damped sinusoids: the planks' short resonance under a paw."""
    out = buf(seconds)
    for f, d, a in zip(freqs, decays, amps):
        ph = rng.uniform(0, math.tau)
        for i in range(len(out)):
            t = i / SR
            out[i] += a * math.sin(math.tau * f * t + ph) * math.exp(-t / d)
    return out


# ---------------------------------------------------------------- elements
def paw(rng, surface, force, scuff=0.):
    """One paw touchdown. force ~0.6 (saunter) .. 1.4 (landing)."""
    length = .2
    out = buf(length)
    # Pad impact: a short, dull burst - small and soft (a rat's pad, not a boot).
    pad = mul(noise(rng, length), env(length, .0015, .010 + .004 * force))
    cut = 900 + 500 * force if surface == 'wood' else 1400 + 600 * force
    add(out, filt(pad, ('lp', cut, .7), ('lp', cut, .7)), 0, 1.2 * force)
    if surface == 'wood':
        # Planks ring briefly and low under the step.
        base = rng.uniform(150, 210) * (1.08 - .08 * force)
        add(out, modes(rng, length, [base, base * 2.31, base * 3.87], [.03, .018, .009], [.5, .22, .1]), 0, .55 * force)
    else:
        # Stone: no ring, a gritty contact instead.
        grit = mul(noise(rng, length), env(length, .001, .012))
        add(out, filt(grit, ('bp', rng.uniform(2200, 3200), 1.2)), 0, .35 * force)
    # Claw tips: one or two faint ticks just after the pad.
    for k in range(rng.choice((1, 2))):
        tick = mul(noise(rng, .01), env(.01, .0003, .0015))
        add(out, filt(tick, ('hp', 3500, .7), ('bp', rng.uniform(4500, 7000), 2.)), .006 + .007 * k + rng.uniform(0, .004), .25)
    if scuff:
        # Running: the paw skids a hair as it pushes off.
        s = mul(noise(rng, .08), env(.08, .006, .02))
        add(out, filt(s, ('bp', rng.uniform(1600, 2600), .9)), .012, scuff)
    return out


def cloth(rng, seconds, center, sweep=0., amount=1., flutter=.35):
    """Jacket fabric: band-passed noise, a slow sweep and a rustling flutter."""
    n = noise(rng, seconds)
    f = Biquad('bp', center, .8)
    out = []
    phase = rng.uniform(0, math.tau)
    for i, x in enumerate(n):
        u = i / len(n)
        f.set(center * (1 + sweep * math.sin(math.pi * u)), .8)
        wob = 1 - flutter + flutter * (.5 + .5 * math.sin(math.tau * 23 * i / SR + phase + 3 * math.sin(math.tau * 7 * i / SR)))
        out.append(f(x) * wob)
    return [v * amount for v in out]


def swish(rng, seconds, peak_at, f0, f1, f2, q=1.6):
    """Air parted by a fast limb: a band sweeping up to the cut and back down."""
    n = noise(rng, seconds)
    f = Biquad('bp', f0, q)
    out = []
    for i, x in enumerate(n):
        t = i / SR
        u = min(1., t / peak_at)
        freq = f0 + (f1 - f0) * u * u if t < peak_at else f1 + (f2 - f1) * min(1., (t - peak_at) / (seconds - peak_at))
        f.set(freq, q)
        # Rises steeply into the cut, dies away quickly after it.
        a = (t / peak_at) ** 3 if t < peak_at else math.exp(-(t - peak_at) / .035)
        out.append(f(x) * a)
    return out


def finish(sig, fade=.01):
    n = int(fade * SR)
    for i in range(min(n, len(sig))):
        sig[-1 - i] *= i / n
    peak = max(1e-9, max(abs(v) for v in sig))
    return [v * PEAK / peak for v in sig]


def write(name, sig, entries, kind, **info):
    path = OUT / f'{name}.wav'
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1., min(1., v)) * 32767)) for v in sig))
    entries.append({'name': name, 'file': f'{name}.wav', 'kind': kind, 'duration_s': round(len(sig) / SR, 4)} | info)


# ---------------------------------------------------------------- the set
def main():
    OUT.mkdir(parents=True, exist_ok=True)
    clips = {c['name']: c for c in json.loads(MANIFEST.read_text(encoding='utf-8'))['clips']}
    roll_ev, slash_ev = clips['Roll']['events_s'], clips['SlashRight']['events_s']
    entries = []
    for surface in ('wood', 'stone'):
        for i in range(6):
            rng = random.Random(f'walk-{surface}-{i}')
            write(f'SFX_Step_Walk_{surface.title()}_{i:02d}', finish(paw(rng, surface, rng.uniform(.55, .7))), entries, 'step_walk', surface=surface)
        for i in range(6):
            rng = random.Random(f'run-{surface}-{i}')
            write(f'SFX_Step_Run_{surface.title()}_{i:02d}', finish(paw(rng, surface, rng.uniform(.9, 1.05), scuff=.35)), entries, 'step_run', surface=surface)
    for i in range(3):
        # Jump takeoff: both paws push off with a scuff, the jacket flaps up.
        rng = random.Random(f'jump-{i}')
        out = buf(.35)
        add(out, paw(rng, 'wood', .55, scuff=.45), 0)
        add(out, paw(rng, 'wood', .45, scuff=.3), .012)
        add(out, mul(cloth(rng, .3, 700, sweep=.9), env(.3, .04, .08, .03)), .02, .5)
        write(f'SFX_Jump_{i:02d}', finish(out), entries, 'jump')
    for i in range(3):
        # Landing: two paws just apart, heavier, and the jacket settles.
        rng = random.Random(f'land-{i}')
        out = buf(.45)
        add(out, paw(rng, 'wood', 1.3), 0)
        add(out, paw(rng, 'wood', 1.15), rng.uniform(.012, .022))
        add(out, mul(cloth(rng, .35, 900, sweep=-.3, flutter=.5), env(.35, .01, .09)), .005, .35)
        write(f'SFX_Land_{i:02d}', finish(out), entries, 'land')
    for i in range(4):
        # Claw slash: a quick sleeve flick, then the swish peaking on the clip's strike.
        rng = random.Random(f'slash-{i}')
        strike = slash_ev['strike']
        out = buf(strike + .22)
        add(out, mul(cloth(rng, .1, 1100, flutter=.2), env(.1, .02, .03)), max(0., slash_ev['wind_up'] - .05), .25)
        add(out, swish(rng, strike + .2, strike, rng.uniform(450, 600), rng.uniform(2600, 3300), rng.uniform(1000, 1400)), 0, 1.)
        # The finest edge of it: a thin, high hiss riding the peak.
        add(out, filt(swish(rng, strike + .2, strike, 3000, 6500, 4000, q=.9), ('hp', 4500, .7)), 0, .18)
        write(f'SFX_Slash_{i:02d}', finish(out), entries, 'slash', strike_s=strike)
    for i in range(2):
        # Roll: push off, shoulder, back and hip meet the boards in turn under a
        # continuous jacket rustle, then the paws plant (Roll clip events).
        rng = random.Random(f'roll-{i}')
        lift, s0, s1, plant = roll_ev['lift'], roll_ev['spin_start'], roll_ev['spin_end'], roll_ev['plant']
        out = buf(roll_ev['settled'] + .1)
        add(out, paw(rng, 'wood', .5, scuff=.4), max(0., lift - .03))
        span = s1 - s0
        for k, (u, w) in enumerate(((.25, 1.1), (.5, .8), (.75, .7))):   # shoulder, back, hip
            thump = mul(noise(rng, .15), env(.15, .004, .03 + .01 * k))
            add(out, filt(thump, ('lp', 420, .7), ('lp', 420, .7)), s0 + span * u + rng.uniform(-.01, .01), 1.6 * w)
            add(out, modes(rng, .15, [rng.uniform(95, 125), 240], [.04, .02], [.5, .15]), s0 + span * u, .6 * w)
        add(out, mul(cloth(rng, span + .1, 800, sweep=.5, flutter=.55), env(span + .1, .05, .12, span * .5)), s0, .55)
        add(out, paw(rng, 'wood', .8), plant)
        add(out, paw(rng, 'wood', .7), plant + .018)
        write(f'SFX_Roll_{i:02d}', finish(out), entries, 'roll', contacts_s=[round(s0 + span * u, 3) for u in (.25, .5, .75)], plant_s=plant)
    for i in range(3):
        # Grass shred (2026-09-30): a crisp tear of many fibres snapping, a
        # leafy rustle as the clippings fly, over a brief swish.
        rng = random.Random(f'shred-{i}')
        out = buf(.45)
        for _ in range(38):
            tick = mul(noise(rng, .006), env(.006, .0002, .0008 + rng.uniform(0, .0008)))
            at = .09 * (rng.random() ** 1.6)
            add(out, filt(tick, ('hp', 2200, .7), ('bp', rng.uniform(2500, 7500), 1.4)), at, rng.uniform(.25, .7))
        add(out, mul(cloth(rng, .4, rng.uniform(1600, 2200), sweep=-.35, flutter=.6), env(.4, .01, .09, .03)), .01, .55)
        add(out, swish(rng, .14, .05, 700, 1800, 900), 0, .25)
        write(f'SFX_Shred_{i:02d}', finish(out), entries, 'shred')
    for i in range(3):
        # Clay jar breaking (2026-09-30): a dull fired-clay crack (terracotta
        # rings briefly and low - no porcelain ping), shards knocking apart,
        # then a light scatter as the pieces land on the stones.
        rng = random.Random(f'jar-{i}')
        out = buf(.75)
        crack = mul(noise(rng, .06), env(.06, .0005, .006))
        add(out, filt(crack, ('hp', 700, .7)), 0, 1.)
        base = rng.uniform(620, 760)
        add(out, modes(rng, .2, [base, base * 1.63, base * 2.41, base * 3.3], [.045, .03, .018, .011], [.6, .4, .25, .15]), 0, .7)
        add(out, filt(mul(noise(rng, .08), env(.08, .001, .02)), ('lp', 350, .7)), 0, .8)     # the body giving way
        for _ in range(12):
            f0 = rng.uniform(1400, 3600)
            tick = modes(rng, .05, [f0, f0 * 1.5], [.012, .007], [.6, .3])
            add(out, tick, rng.uniform(.005, .12), rng.uniform(.15, .4))
        for _ in range(9):
            f0 = rng.uniform(1800, 4200)
            add(out, modes(rng, .04, [f0, f0 * 1.7], [.01, .006], [.5, .25]), rng.uniform(.28, .6), rng.uniform(.06, .18))
        write(f'SFX_JarBreak_{i:02d}', finish(out), entries, 'jar_break')
    for i in range(3):
        # Cigarette pickup: a soft paper tick and a brief crinkle as it's
        # pocketed. Quiet and dry: a small reward, not a chime.
        rng = random.Random(f'pickup-{i}')
        out = buf(.2)
        tap = mul(noise(rng, .02), env(.02, .0005, .003))
        add(out, filt(tap, ('bp', rng.uniform(2400, 3200), 1.5)), 0, .8)
        for k in range(6):
            fib = mul(noise(rng, .01), env(.01, .0003, .0012))
            add(out, filt(fib, ('hp', 3000, .7), ('bp', rng.uniform(4000, 7000), 1.8)), .012 + k * rng.uniform(.006, .012), rng.uniform(.2, .45))
        add(out, mul(cloth(rng, .12, 1500, flutter=.3), env(.12, .01, .03)), .03, .25)    # into the jacket
        write(f'SFX_Pickup_{i:02d}', finish(out), entries, 'pickup')
    (OUT / 'manifest.json').write_text(json.dumps({'sample_rate': SR, 'peak_dbfs': -3, 'generator': 'Tools/gen_chuck_sfx.py', 'sounds': entries}, indent=1) + '\n', encoding='utf-8')
    print('CHUCK_SFX_READY', len(entries), 'sounds')


if __name__ == '__main__':
    main()
