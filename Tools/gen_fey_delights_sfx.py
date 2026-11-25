"""The Feywild's small delights' sounds: original synthesis, no recordings and no dependencies (Claude, 2026-11-24).

py -3 Tools/gen_fey_delights_sfx.py  ->  SourceAssets/Audio/SFX/SFX_FeyFairyBell_00.wav, SFX_FeyCrowCaw_00/01.wav,
                                         SFX_FeySporePuff_00.wav and fey-delights-sfx-manifest.json
(Tools/import_fey_sfx.py imports them with the rest.)

  FairyBell  a faint small bell: inharmonic partials of a struck bell, each dying at its own rate, a slow shimmer.
  CrowCaw    two harsh caws: a buzzy, falling voice (a pulse train through two nasal formants) with breath in it.
  SporePuff  a soft dry puff: a short burst of low-passed noise with a little tick at the start.
"""
import hashlib, json, math, random, struct, wave
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
RATE = 48000


def write(name, samples):
    peak = max(1e-6, max(abs(v) for v in samples))
    data = b''.join(struct.pack('<h', int(max(-1., min(1., v / peak * .89)) * 32767)) for v in samples)
    path = OUT / f'{name}.wav'
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE); w.writeframes(data)
    return {'seconds': round(len(samples) / RATE, 2), 'loop': False, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


class Formant:
    """A two-pole resonator (a formant) at F Hz with bandwidth B Hz."""
    def __init__(self, f, b):
        r = math.exp(-math.pi * b / RATE); self.a1 = 2 * r * math.cos(2 * math.pi * f / RATE); self.a2 = -r * r; self.g = 1 - r; self.y1 = self.y2 = 0.
    def __call__(self, x):
        y = self.g * x + self.a1 * self.y1 + self.a2 * self.y2; self.y2, self.y1 = self.y1, y; return y


def bell():
    rng = random.Random(7); n = int(RATE * 3.2); out = []
    base = 1890.
    parts = [(1., 1., 1.6), (2.76, .5, .9), (5.40, .32, .55), (8.93, .18, .35), (1.003, .55, 1.9), (13.3, .07, .2)]
    for i in range(n):
        t = i / RATE; v = 0.
        for ratio, amp, decay in parts:
            v += amp * math.sin(2 * math.pi * base * ratio * t + ratio) * math.exp(-t / decay)
        v *= (1 - math.exp(-t * 900.)) * (1 + .08 * math.sin(2 * math.pi * 5.3 * t))
        out.append(v + rng.uniform(-1, 1) * .002 * math.exp(-t * 30.))
    return out


def caw(seed, calls):
    rng = random.Random(seed); out = []; t0 = 0.
    for k in range(calls):
        dur = rng.uniform(.28, .38); f0 = rng.uniform(560, 640); n = int(RATE * dur)
        f1, f2, f3 = Formant(1250 + rng.uniform(-80, 80), 260), Formant(2300, 400), Formant(3600, 700)
        phase = 0.; hp = 0.
        for i in range(n):
            u = i / n; f = f0 * (1.08 - .3 * u) * (1 + .04 * rng.uniform(-1, 1))
            phase += f / RATE
            pulse = 1. if (phase % 1.) < .12 else 0.   # (a narrow pulse train: buzzy, rich in harmonics)
            src = pulse + .35 * rng.uniform(-1, 1)      # (and rough breath)
            env = min(1., u / .06) * (1 - u) ** .7 * (1 + .25 * math.sin(2 * math.pi * 31 * u))
            y = f1(src) * 1.2 + f2(src) * .8 + f3(src) * .25
            hp = .995 * hp + .005 * y
            out.append((y - hp) * env)
        gap = int(RATE * rng.uniform(.14, .22)); out += [0.] * gap
    return out + [0.] * int(RATE * .2)


def puff():
    rng = random.Random(3); n = int(RATE * .7); out = []; lp = lp2 = 0.
    for i in range(n):
        t = i / RATE; x = rng.uniform(-1, 1)
        lp += .08 * (x - lp); lp2 += .05 * (lp - lp2)
        env = min(1., t / .012) * math.exp(-t / .16)
        tick = math.exp(-t / .003) * rng.uniform(-1, 1) * .4
        out.append(lp2 * env * 4. + tick)
    return out


manifest = {
    'SFX_FeyFairyBell_00': write('SFX_FeyFairyBell_00', bell()),
    'SFX_FeyCrowCaw_00': write('SFX_FeyCrowCaw_00', caw(11, 2)),
    'SFX_FeyCrowCaw_01': write('SFX_FeyCrowCaw_01', caw(23, 3)),
    'SFX_FeySporePuff_00': write('SFX_FeySporePuff_00', puff()),
}
(OUT / 'fey-delights-sfx-manifest.json').write_text(json.dumps({'sources': 'synthesised by Tools/gen_fey_delights_sfx.py (no recordings)', 'sounds': manifest}, indent=1) + '\n', encoding='utf-8')
print('CHUCK_FEY_DELIGHTS_SFX', ', '.join(f'{k} {v["seconds"]}s' for k, v in manifest.items()))
