"""Blender 4.5.14 (for its audio decoder and numpy): the Feywild great hare's sounds (Claude, 2026-11-25;
docs/regions/FEYWILD-HARE.md).

blender --background --factory-startup --python Tools/build_fey_hare_sfx.py
  <- SourceAssets/Audio/SFX/Feywild/Source/BigSoundBank/bsb_1376.mp3 ("Bunny hutch", Joseph Sardin, CC0; rabbits
     stamping their feet in their hutches) and bsb_0137.mp3 ("Feet in leaves", CC0), both fetched by
     Tools/Fetch-FeySounds.ps1 and recorded in its manifest
  -> SourceAssets/Audio/SFX/SFX_FeyHare*.wav and fey-hare-sfx-manifest.json (Tools/import_fey_sfx.py imports them)

  FeyHareThump_00-02   the warning thump of a hind foot: the loudest stamps found in the hutch recording (found by
                       their onsets, not by hand), their low end kept
  FeyHareBound_00-03   the soft thud of its feet striking turf in the bound: stamps slowed and darkened, with a brush of
                       leaves under them
  FeyHareSqueal_00-01  the short squeal as it throws him: synthesised (a harsh, falling voice through two formants with
                       breath), as the crows' calls are; no recording was used
"""
from pathlib import Path
import hashlib, json, math, sys, wave

import aud
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Tools'))
SRC = ROOT / 'SourceAssets/Audio/SFX/Feywild/Source/BigSoundBank'
OUT = ROOT / 'SourceAssets/Audio/SFX'
SR = 48000
PEAK = 10 ** (-3 / 20)
rng = np.random.default_rng(20261125)
MANIFEST = {}


def load(path):
    s = aud.Sound(str(path)).resample(SR, False)
    a = np.asarray(s.data(), dtype=np.float64)
    return a.mean(axis=1) if a.ndim > 1 else a


def onepole(a, cut_hz, high=False):
    k = math.exp(-2 * math.pi * cut_hz / SR); y = np.empty_like(a); z = 0.
    for i, v in enumerate(a): z = (1 - k) * v + k * z; y[i] = z
    return a - y if high else y


def speed(a, k):
    n = max(2, int(len(a) / k)); x = np.linspace(0, len(a) - 1, n)
    return np.interp(x, np.arange(len(a)), a)


def fade(a, fin=.004, fout=.06):
    a = a.copy(); i = min(len(a), int(fin * SR)); o = min(len(a), int(fout * SR))
    if i: a[:i] *= np.linspace(0, 1, i)
    if o: a[-o:] *= np.linspace(1, 0, o)
    return a


def write(name, a, gain_db=0.):
    a = a - a.mean(); m = np.abs(a).max() or 1.
    a = fade(a / m * PEAK * 10 ** (gain_db / 20))
    pcm = (np.clip(a, -1, 1) * 32767).astype('<i2')
    path = OUT / f'{name}.wav'
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
    MANIFEST[name] = {'seconds': round(len(a) / SR, 2), 'loop': False, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    print('CHUCK_FEY_HARE_SFX', name, f'{len(a) / SR:.2f}s')


# ---- the stamps: onsets in the hutch recording, the strongest isolated ones
hutch = load(SRC / 'bsb_1376.mp3')
low = onepole(onepole(hutch, 900.), 900.)                      # (a stamp is a low thud; the rustling is higher)
hop = int(.005 * SR)
env = np.sqrt(np.convolve(low ** 2, np.ones(hop) / hop, 'same'))[::hop]
floor = np.median(env) + 1e-9
peaks = []
for i in range(4, len(env) - 4):
    if env[i] == env[i - 4:i + 5].max() and env[i] > 6 * floor and env[i] > 3 * env[max(0, i - 12):i - 4].mean():
        peaks.append((env[i], i * hop))
peaks.sort(reverse=True)
chosen = []
for v, at in peaks:
    if all(abs(at - c) > .35 * SR for c in chosen): chosen.append(at)
    if len(chosen) >= 7: break
print('CHUCK_FEY_HARE_STAMPS', len(peaks), [round(c / SR, 2) for c in chosen])
if len(chosen) < 7: raise RuntimeError('too few stamps found in bsb_1376')
stamps = [hutch[max(0, c - int(.01 * SR)):c + int(.24 * SR)] for c in chosen]
for i, s in enumerate(stamps[:3]):
    body = onepole(s, 2600.) + .6 * onepole(onepole(s, 220.), 220.) * 3.
    write(f'SFX_FeyHareThump_{i:02d}', body)

leaves = load(SRC / 'bsb_0137.mp3')
for i, s in enumerate(stamps[3:7]):
    thud = onepole(speed(s, .78), 1500.)
    at = int(rng.uniform(.5, len(leaves) / SR - 1.) * SR)
    brush = onepole(onepole(leaves[at:at + len(thud)], 4000.), 300., high=True)
    brush = brush / (np.abs(brush).max() or 1.) * np.abs(thud).max() * .35 * np.exp(-np.arange(len(thud)) / (.08 * SR))
    write(f'SFX_FeyHareBound_{i:02d}', thud + brush, -2.)


# ---- the squeal (synthesised)
class Formant:
    def __init__(self, f, b):
        r = math.exp(-math.pi * b / SR); self.a1 = 2 * r * math.cos(2 * math.pi * f / SR); self.a2 = -r * r; self.g = 1 - r; self.y1 = self.y2 = 0.
    def __call__(self, x):
        y = self.g * x + self.a1 * self.y1 + self.a2 * self.y2; self.y2, self.y1 = self.y1, y; return y


for k in range(2):
    dur = .34 + .08 * k; n = int(dur * SR); f0 = 1450. + 180. * k
    F = [Formant(2900. + 200 * k, 500.), Formant(5200., 900.)]
    out = np.zeros(n); ph = 0.
    for i in range(n):
        u = i / n
        f = f0 * (1.12 - .35 * u) * (1 + .03 * math.sin(2 * math.pi * 38 * u) + .02 * rng.uniform(-1, 1))
        ph += f / SR
        src = (1. if (ph % 1.) < .3 else -.4) + .55 * rng.uniform(-1, 1)
        env = min(1., u / .05) * (1 - u) ** 1.4
        out[i] = env * (F[0](src) * 1.3 + F[1](src) * .5)
    write(f'SFX_FeyHareSqueal_{k:02d}', np.tanh(out / (np.abs(out).max() or 1.) * 2.2), -4.)

(OUT / 'fey-hare-sfx-manifest.json').write_text(json.dumps({'sources': 'Feywild/Source/manifest.json: bsb_1376 (Bunny hutch), bsb_0137 (Feet in leaves); CC0. The squeal is synthesised.', 'sounds': MANIFEST}, indent=1) + '\n', encoding='utf-8')
print('CHUCK_FEY_HARE_SFX_READY', len(MANIFEST))
