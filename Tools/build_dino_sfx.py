"""Blender 4.5.14 (for its audio decoder and numpy): the raptor's and T-rex's sounds
from recorded CC0 sources (Claude, 2026-10-16).

blender --background --factory-startup --python Tools/build_dino_sfx.py
  SourceAssets/Audio/SFX/Dinos/Source/<pack>/*  (CC0, see Source/manifest.json)
  -> SourceAssets/Audio/SFX/SFX_Raptor*.wav, SFX_TRex*.wav   (mono, 48 kHz, 16-bit, peak -3 dBFS)
     SourceAssets/Audio/SFX/dino-sfx-manifest.json
Then Tools/import_raptor_sfx.py and Tools/import_trex_sfx.py import them as before.

User request: replace the synthetic placeholders (Tools/gen_raptor_sfx.py,
gen_trex_sfx.py, which this supersedes for these files) with real sounds. The
user downloaded t-rex_calls.mp3; the rest were downloaded by Claude with the
user's approval. Cuts were chosen from spectrograms (the sounds were not
auditioned by Claude): the T-rex's roars are the two tonal bellows that end
t-rex_calls, its growl and death the raspy sections before them, with a
deep monster roar under the roars for weight; the raptor's call is a
screech from tiny-vicious-creature over the raspy small-dino call; its hiss
is breath noise, its death a slowed scream. Jaw snaps are wood knocks with
teeth clicks; footfalls are soft heavy thuds with leaf crunch (the T-rex's
with a synthetic sub-bass shake).
"""
from pathlib import Path
import hashlib, json, wave

import aud
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'SourceAssets/Audio/SFX/Dinos/Source'
OUT = ROOT / 'SourceAssets/Audio/SFX'
SR = 48000
PEAK = 10 ** (-3 / 20)
rng = np.random.default_rng(20261016)


def load(rel):
    s = aud.Sound(str(SRC / rel)).resample(SR, False)
    a = np.asarray(s.data(), dtype=np.float64)
    return a.mean(axis=1) if a.ndim > 1 else a


def cut(x, start, length=None):
    i = int(start * SR)
    return x[i:] if length is None else x[i:i + int(length * SR)]


def rate(x, r):
    """Play at speed r (r < 1: slower and deeper)."""
    n = int(len(x) / r)
    return np.interp(np.arange(n) * r, np.arange(len(x)), x)


def fade(x, fin=.005, fout=.05):
    x = x.copy(); a = min(len(x), int(fin * SR)); b = min(len(x), int(fout * SR))
    if a: x[:a] *= np.linspace(0, 1, a)
    if b: x[-b:] *= np.linspace(1, 0, b) ** 2
    return x


def mix(*layers):
    """(signal, gain dB, offset s) layers summed."""
    n = max(int(o * SR) + len(s) for s, g, o in layers)
    out = np.zeros(n)
    for s, g, o in layers:
        i = int(o * SR); out[i:i + len(s)] += s * 10 ** (g / 20)
    return out


def lowpass(x, hz):
    a = np.exp(-2 * np.pi * hz / SR); y = np.zeros_like(x); acc = 0.
    for i, v in enumerate(x):
        acc = (1 - a) * v + a * acc; y[i] = acc
    return y


def highpass(x, hz):
    return x - lowpass(x, hz)


def peak_at(x, start=0., length=None, before=.02, after=.12):
    """The window round the loudest instant (a click within a crunch)."""
    seg = cut(x, start, length); i = int(np.argmax(np.abs(seg))) + int(start * SR)
    return x[max(0, i - int(before * SR)):i + int(after * SR)]


def loudest(x, start, end, length):
    """Start of the loudest length-second window between start and end (s)."""
    seg = cut(x, start, end - start) ** 2; w = int(length * SR)
    c = np.convolve(seg, np.ones(w), 'valid')
    return start + int(np.argmax(c[::480])) * 480 / SR


def sub_shake(seconds, hz, decay):
    t = np.arange(int(seconds * SR)) / SR
    return np.sin(2 * np.pi * hz * t * (1 - .15 * t)) * np.exp(-t / decay) * (1 - np.exp(-t / .004))


entries = []


def write(name, x, tail=.05):
    x = fade(x, .003, tail)
    x = x / max(np.abs(x).max(), 1e-9) * PEAK
    pcm = (np.clip(x, -1, 1) * 32767).astype('<i2').tobytes()
    path = OUT / f'{name}.wav'
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm)
    entries.append({'name': name, 'file': path.name, 'duration': round(len(x) / SR, 3), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    print('CHUCK_DINO_SFX', name, round(len(x) / SR, 2))


trex = load('CaveboyTup/t-rex_calls.mp3')
sandworm = load('trazzz123/monster_roar.wav')
raspy = load('CaveboyTup/small_dino_raspy_calls.mp3')
vicious = load('Darsycho/tiny-vicious-creature.ogg')
R1, R2 = 'rubberduck-80-creature/', 'rubberduck-80-creature-2/'
K = 'Kenney-impact/'

# --- T-rex -----------------------------------------------------------------
# (2026-11-12, user: "find and implement new better raptor and T-rex sounds"): real animals under the old bellows, slowed
# far down the way film creature voices are built - a donkey's bray at about 0.4x for the roar's tonal body, a cat's roar at
# 0.3x for its snarl, a horse's breath at 0.45x for the growl's breathing (BigSoundBank, CC0; Source/BigSoundBank).
B = 'BigSoundBank/'
bray, catroar, hbreath = load(B + 'bsb_1551.mp3'), load(B + 'bsb_1881.mp3'), load(B + 'bsb_1543.mp3')
inhale = lowpass(rate(hbreath, .5), 1400)
write('SFX_TRexRoar_00', mix((inhale, -6, 0),
                             (lowpass(rate(fade(cut(bray, 1.85, 1.25), .04, .2), .42), 1600), 0, .45),
                             (rate(cut(trex, 36.58, 3.6), .9), -3, .5),
                             (lowpass(rate(cut(catroar, 0., .95), .3), 1300), -5, .55),
                             (highpass(lowpass(cut(sandworm, .12, 4.), 900), 40), -10, .5)), tail=.9)
write('SFX_TRexRoar_01', mix((lowpass(rate(fade(cut(bray, 4.05, .45), .03, .1), .38), 1500), 0, 0),
                             (lowpass(rate(fade(cut(bray, 4.95, .6), .03, .25), .4), 1500), -2, .9),
                             (rate(fade(cut(trex, loudest(trex, 26.19, 35.19, 3.3), 3.3), .12, .01), .9), -4, .1),
                             (lowpass(rate(cut(catroar, .1, .8), .32), 1200), -6, .2),
                             (highpass(lowpass(cut(sandworm, 1.6, 3.6), 900), 40), -11, .1)), tail=.8)
write('SFX_TRexGrowl_00', mix((lowpass(rate(cut(catroar, 0., .95), .27), 900), 0, 0),
                              (lowpass(rate(hbreath, .45), 1100), -4, .2),
                              (rate(cut(trex, 10.42, 3.1), .9), -5, 0)), tail=.8)
write('SFX_TRexDeath_00', mix((lowpass(rate(fade(cut(bray, 6.05, .6), .03, .3), .35), 1200), 0, 0), (rate(cut(trex, 4.16, 4.4), .85), -3, .2)), tail=1.4)
clack = mix((rate(load(K + 'impactWood_heavy_000.ogg'), .6), 0, 0), (rate(load(K + 'impactPunch_heavy_002.ogg'), .7), -3, 0),
            (rate(peak_at(load('StarNinjas-crunch/crunch.5.ogg')), .6), -4, .01))
write('SFX_TRexSnap_00', clack[:int(.45 * SR)], tail=.15)
for v, (soft, grass) in enumerate([('000', '000'), ('002', '001'), ('004', '003')]):
    layers = [(rate(load(K + f'impactSoft_heavy_{soft}.ogg'), .6), 0, 0), (rate(load(K + f'footstep_grass_{grass}.ogg'), .7), -9, .01),
              (sub_shake(.9, 34 + 3 * v, .22), -4, 0)]
    if v == 1: layers.append((rate(load(R2 + 'stomp_01.ogg'), .6), -6, 0))
    write(f'SFX_TRexStep_{v:02d}', mix(*layers)[:int(.9 * SR)], tail=.35)

# --- Raptor ----------------------------------------------------------------
# (2026-11-12): a bird's piercing call lowered a little for the shriek (screaming piha, 0.78x), a horse's whinny sped up
# for its rattling tail, a barn owl's hiss for the tell, a horse's neigh slowed for the death cry (BigSoundBank, CC0).
piha, neigh, owl = load(B + 'bsb_1762.mp3'), load(B + 'bsb_0863.mp3'), load(B + 'bsb_1400.mp3')
write('SFX_RaptorCall_00', mix((highpass(rate(fade(cut(piha, 0., 1.2), .01, .2), .78), 300), 0, 0),
                               (highpass(rate(fade(cut(neigh, .38, .75), .01, .2), 1.3), 400), -5, .32),
                               (rate(cut(vicious, 7.28, .72), .8), -8, .02)), tail=.25)
write('SFX_RaptorHiss_00', mix((highpass(rate(fade(cut(owl, .35, 1.25), .05, .3), .9), 700), 0, 0), (rate(load(R2 + 'breath_02.ogg'), .85), -9, 0)), tail=.2)
write('SFX_RaptorDeath_00', mix((rate(fade(cut(neigh, .3, 1.), .01, .4), .72), 0, 0), (rate(load(R1 + 'scream_02.ogg'), .7), -5, .05)), tail=.5)
for v, (wood, crunch, air) in enumerate([('002', 'crunch.5', 'attack_01'), ('004', 'crunch.6', 'attack_05')]):
    snap = mix((rate(load(K + f'impactWood_heavy_{wood}.ogg'), 1.3), 0, 0), (peak_at(load(f'StarNinjas-crunch/{crunch}.ogg')), -3, .005),
               (load(R2 + f'{air}.ogg'), -9, 0))
    write(f'SFX_RaptorSnap_{v:02d}', snap[:int(.28 * SR)], tail=.08)
for v, (grass, soft) in enumerate([('002', '000'), ('003', '001'), ('004', '002')]):
    step = mix((rate(load(K + f'footstep_grass_{grass}.ogg'), .85), 0, 0), (rate(load(K + f'impactSoft_medium_{soft}.ogg'), .9), -6, 0))
    write(f'SFX_RaptorStep_{v:02d}', step[:int(.35 * SR)], tail=.12)

(OUT / 'dino-sfx-manifest.json').write_text(json.dumps({'generator': 'Tools/build_dino_sfx.py', 'sources': 'Dinos/Source/manifest.json',
                                                       'license': 'CC0 sources, cut and mixed for CHUCK 3D', 'sounds': entries}, indent=2), encoding='utf-8')
print('CHUCK_DINO_SFX_READY', len(entries))
