"""Blender 4.5.14 (for its audio decoder and numpy): the Feywild's creature and mechanic sounds from recorded CC0 sources
(Claude, 2026-11-22; the user: "Download and implement sounds for things that need sounds").

blender --background --factory-startup --python Tools/build_fey_sfx.py
  SourceAssets/Audio/SFX/Feywild/Source/BigSoundBank/*.mp3   (CC0, Tools/Fetch-FeySounds.ps1, see Source/manifest.json)
  SourceAssets/Audio/SFX/Dinos/Source/{trazzz123,Kenney-impact}/*  (CC0, already in the project)
  -> SourceAssets/Audio/SFX/SFX_Fey*.wav (mono, 48 kHz, 16-bit, peak -3 dBFS; *Loop* files loop seamlessly)
     SourceAssets/Audio/SFX/fey-sfx-manifest.json
Then Tools/import_fey_sfx.py imports them.

Cuts were chosen from loudness envelopes; Claude cannot hear them, so none has been auditioned:
  displacer beast   roars: cat roars slowed an octave over the deep monster roar; growls: growling cats slowed; lash: whip
                    cracks; steps: soft heavy impacts, slowed
  thorn mites       chitter: chattering teeth, quickened; tell: tongue clicks; death: twigs breaking
  redcaps           cackle: delirious laughter, slowed and roughened (their notice and the tell before a blow)
  camp              fire: a fireplace; the cauldron: broth bubbling in a pot (both loops)
  lantern moths     a soft wing flutter (pigeon wings, quickened and quiet; a loop)
  reactive flowers  opening: a wind-chime shimmer over a rustle of leaves; closing: the chime reversed; vines: leaves
                    dragged, slowed, with twigs
  spitting orchids  swell: a straw's suck; spit: a cork's pop; the seed's hit: a small wood knock
  rising pads       water bubbling up, a small splash
  the maze's bees   three bumblebees layered into a swarm (a loop)
"""
from pathlib import Path
import hashlib, json, wave

import aud
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'SourceAssets/Audio/SFX/Feywild/Source'
DINO = ROOT / 'SourceAssets/Audio/SFX/Dinos/Source'
OUT = ROOT / 'SourceAssets/Audio/SFX'
SR = 48000
PEAK = 10 ** (-3 / 20)
rng = np.random.default_rng(20261122)
MANIFEST = {}


def load(path):
    s = aud.Sound(str(path)).resample(SR, False)
    a = np.asarray(s.data(), dtype=np.float64)
    return a.mean(axis=1) if a.ndim > 1 else a


def bsb(n): return load(SRC / f'BigSoundBank/bsb_{n}.mp3')


def cut(a, t0, t1): return a[int(t0 * SR):int(t1 * SR)].copy()


def speed(a, k):
    """Played k times as fast (pitch and tempo together, like a tape)."""
    n = max(2, int(len(a) / k)); x = np.linspace(0, len(a) - 1, n)
    return np.interp(x, np.arange(len(a)), a)


def fade(a, fin=.01, fout=.05):
    a = a.copy(); i = min(len(a), int(fin * SR)); o = min(len(a), int(fout * SR))
    if i: a[:i] *= np.linspace(0, 1, i)
    if o: a[-o:] *= np.linspace(1, 0, o)
    return a


def lowpass(a, cut_hz):
    k = np.exp(-2 * np.pi * cut_hz / SR); y = np.empty_like(a); z = 0.
    for i, v in enumerate(a): z = (1 - k) * v + k * z; y[i] = z
    return y


def mix(*parts):
    """(signal, gain, offset s) ..."""
    n = max(len(p) + int(o * SR) for p, g, o in parts); out = np.zeros(n)
    for p, g, o in parts: s = int(o * SR); out[s:s + len(p)] += p * g
    return out


def loop(a, xf=.6):
    """A seamless loop: the end crossfaded into the start."""
    n = int(xf * SR); body = a[:-n].copy(); tail = a[-n:]
    w = np.linspace(0, 1, n); body[:n] = body[:n] * w + tail * (1 - w)
    return body


def rough(a, amt=.25):
    return np.tanh(a * (1 + 6 * amt)) / np.tanh(1 + 6 * amt)


def write(name, a, gain_db=0., loops=False):
    a = a - a.mean(); m = np.abs(a).max() or 1.
    a = a / m * PEAK * 10 ** (gain_db / 20)
    if not loops: a = fade(a)
    pcm = (np.clip(a, -1, 1) * 32767).astype('<i2')
    path = OUT / f'{name}.wav'
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
    MANIFEST[name] = {'seconds': round(len(a) / SR, 2), 'loop': loops, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    print('CHUCK_FEY_SFX', name, f'{len(a) / SR:.2f}s')


# ---------------------------------------------------------------- the displacer beast
deep = load(DINO / 'trazzz123/monster_roar.wav')
for i, (n, t0, t1) in enumerate((('1881', 0, 1.2), ('1884', 0, .9))):
    roar = speed(cut(bsb(n), t0, t1), .5)
    under = speed(cut(deep, .2 + i * 1.1, .2 + i * 1.1 + len(roar) / SR * .9), .9)
    write(f'SFX_FeyBeastRoar_{i:02d}', mix((roar, 1., 0), (lowpass(under, 900), .45, .05)))
for i, (n, t0, t1) in enumerate((('1886', .2, 3.), ('1887', 1.4, 4.6), ('1888', .9, 4.6))):
    write(f'SFX_FeyBeastGrowl_{i:02d}', lowpass(speed(cut(bsb(n), t0, t1), .62), 2600), -2.)
for i, n in enumerate(('2949', '2951')):
    a = bsb(n); p = int(np.abs(a).argmax())
    write(f'SFX_FeyBeastLash_{i:02d}', cut(a, max(0, p / SR - .12), p / SR + .7))
for i, f in enumerate(('impactSoft_heavy_000', 'impactSoft_heavy_002', 'impactSoft_heavy_004')):
    write(f'SFX_FeyBeastStep_{i:02d}', lowpass(speed(load(DINO / f'Kenney-impact/{f}.ogg'), .78), 1400), -6.)

# ---------------------------------------------------------------- thorn mites
teeth = bsb('0789')
env = np.array([np.sqrt(np.mean(teeth[k:k + 2400] ** 2)) for k in range(0, len(teeth) - 2400, 2400)])
peaks = [int(i) for i in np.argsort(-env)[:40]]
starts = []
for p in peaks:
    if all(abs(p - q) > 8 for q in starts): starts.append(p)
for i, p in enumerate(sorted(starts[:3])):
    write(f'SFX_FeyMiteChitter_{i:02d}', speed(cut(teeth, max(0, p * .05 - .1), p * .05 + .55), 1.6), -4.)
clicks = bsb('1038'); p = int(np.abs(clicks).argmax())
write('SFX_FeyMiteClick_00', speed(cut(clicks, max(0, p / SR - .05), p / SR + .25), 1.4), -3.)
twigs = bsb('1299'); p = int(np.abs(twigs).argmax())
write('SFX_FeyMiteDeath_00', speed(cut(twigs, max(0, p / SR - .1), p / SR + .5), 1.3), -3.)

# ---------------------------------------------------------------- redcaps
for i, (n, t0, t1) in enumerate((('2093', .2, 1.9), ('2096', .25, 1.7))):
    write(f'SFX_FeyRedcapCackle_{i:02d}', rough(lowpass(speed(cut(bsb(n), t0, t1), .82), 3200), .3), -2.)

# ---------------------------------------------------------------- the camp
write('SFX_FeyFireLoop_00', loop(lowpass(cut(bsb('0030'), 12., 32.), 6000)), -6., loops=True)
write('SFX_FeyCauldronLoop_00', loop(cut(bsb('0491'), .2, 6.8), .5), -8., loops=True)

# ---------------------------------------------------------------- lantern moths, flowers, vines, orchids, pads
write('SFX_FeyMothLoop_00', loop(lowpass(speed(cut(bsb('0476'), .2, 1.9), 1.35), 2400), .25), -12., loops=True)
leaves = bsb('0137')
chime = cut(bsb('2087'), .3, 2.1)
write('SFX_FeyFlowerOpen_00', mix((chime, .8, .25), (lowpass(cut(leaves, 1., 2.2), 3000), .6, 0)))
write('SFX_FeyFlowerClose_00', mix((cut(bsb('2088'), .2, 2.2)[::-1], .7, 0), (lowpass(cut(leaves, 6., 7.), 3000), .6, .9)))
write('SFX_FeyVines_00', mix((speed(lowpass(cut(leaves, 3., 5.6), 2200), .7), 1., 0), (cut(twigs, .2, 1.6), .35, .4)), -2.)
suck = bsb('1244'); p = int(np.abs(suck).argmax())
write('SFX_FeyOrchidSwell_00', fade(speed(cut(suck, max(0, p / SR - 1.2), p / SR + .1), 1.25), .3, .05), -8.)
write('SFX_FeyOrchidSpit_00', cut(bsb('0211'), 0., .45), -2.)
write('SFX_FeyOrchidHit_00', speed(load(DINO / 'Kenney-impact/impactWood_heavy_000.ogg'), 1.6), -4.)
bub = bsb('0150'); env = np.array([np.sqrt(np.mean(bub[k:k + 4800] ** 2)) for k in range(0, len(bub) - 4800, 4800)])
b0 = int(np.argmax(np.convolve(env, np.ones(20), 'valid'))) * .1
write('SFX_FeyPadsRise_00', mix((cut(bub, b0, b0 + 2.2), 1., 0), (cut(bsb('1529'), 0., 1.), .5, 1.7)), -2.)

# ---------------------------------------------------------------- the maze's bees (stage 6): a swarm, three bumblebees layered
bee = bsb('1000'); env = np.array([np.sqrt(np.mean(bee[k:k + 4800] ** 2)) for k in range(0, len(bee) - 4800, 4800)])
b0 = int(np.argmax(np.convolve(env, np.ones(30), 'valid'))) * .1; seg = cut(bee, b0, b0 + 3.2)
bee2 = bsb('1001'); seg2 = cut(bee2, .5, 3.7) if len(bee2) > 3.8 * SR else seg
swarm = mix((seg, 1., 0), (speed(seg2, 1.18)[:len(seg)], .8, 0), (speed(seg, .87)[:len(seg)], .7, 0))
write('SFX_FeyBeeLoop_00', loop(swarm[:len(seg)], .5), -4., loops=True)

(OUT / 'fey-sfx-manifest.json').write_text(json.dumps({'sources': 'Feywild/Source/manifest.json and Dinos/Source/manifest.json (CC0)', 'sounds': MANIFEST}, indent=1) + '\n', encoding='utf-8')
print('CHUCK_FEY_SFX_BUILT', len(MANIFEST))
