"""The Deepwood jellyfish's sounds (Claude, 2026-11-26; docs/regions/FEYWILD-JELLYFISH.md). Synthesised, no recordings.

blender --background --factory-startup --python Tools/build_fey_jelly_sfx.py      (Blender 4.5.14, for its numpy)
  -> SourceAssets/Audio/SFX/SFX_FeyJelly*.wav and fey-jelly-sfx-manifest.json (Tools/import_fey_sfx.py imports them)

  FeyJellyPulse_00-02   each beat of the bell: a soft, wet push of air (band-passed noise swelling and falling away)
                        over a low, rounded thump, as if heard through water
  FeyJellyHum           its presence, looping: a faint airy chord (a fifth and a tenth, slowly beating) with breath
                        under it, 8 s, whole cycles so the loop is seamless
"""
from pathlib import Path
import hashlib, json, math, wave

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets/Audio/SFX'
SR = 48000
PEAK = 10 ** (-3 / 20)
rng = np.random.default_rng(20261126)
MANIFEST = {}


def onepole(a, cut_hz, high=False):
    k = math.exp(-2 * math.pi * cut_hz / SR); y = np.empty_like(a); z = 0.
    for i, v in enumerate(a): z = (1 - k) * v + k * z; y[i] = z
    return a - y if high else y


def fade(a, fin=.004, fout=.06):
    a = a.copy(); i = min(len(a), int(fin * SR)); o = min(len(a), int(fout * SR))
    if i: a[:i] *= np.linspace(0, 1, i)
    if o: a[-o:] *= np.linspace(1, 0, o)
    return a


def write(name, a, gain_db=0., loop=False):
    a = a - a.mean(); m = np.abs(a).max() or 1.
    a = a / m * PEAK * 10 ** (gain_db / 20)
    if not loop: a = fade(a)
    pcm = (np.clip(a, -1, 1) * 32767).astype('<i2')
    path = OUT / f'{name}.wav'
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
    MANIFEST[name] = {'seconds': round(len(a) / SR, 2), 'loop': loop, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    print('CHUCK_FEY_JELLY_SFX', name, f'{len(a) / SR:.2f}s')


# ---- the pulse
for k in range(3):
    dur = .62 + .06 * k; n = int(dur * SR); t = np.arange(n) / SR
    swell = np.minimum(1., t / .09) ** 1.5 * np.exp(-np.maximum(0., t - .09) / (.16 + .03 * k))
    air = onepole(onepole(rng.standard_normal(n), 900. + 120 * k), 260., high=True) * swell
    f = 74. - 22. * np.minimum(1., t / .3)
    thump = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / .13) * np.minimum(1., t / .02)
    wet = np.sin(2 * np.pi * (420. + 60 * k) * t + 3. * np.sin(2 * np.pi * 7. * t)) * np.exp(-((t - .1) / .05) ** 2) * .12
    write(f'SFX_FeyJellyPulse_{k:02d}', onepole(air * .9 + thump * .7 + wet, 2400.), -6.)

# ---- the hum (whole cycles of every partial over 8 s: a seamless loop)
dur = 8.; n = int(dur * SR); t = np.arange(n) / SR
out = np.zeros(n)
for f, a in ((110., .5), (165., .32), (275.25, .14), (330., .08)):
    fq = round(f * dur) / dur
    trem = 1. + .25 * np.sin(2 * np.pi * (round(.25 * dur) / dur) * t + f)
    out += a * np.sin(2 * np.pi * fq * t) * trem
x = int(.25 * SR)   # (the breath runs x past the end and is crossfaded into its own start)
breath = rng.standard_normal(n + x)
breath = onepole(onepole(breath, 700.), 150., high=True)
breath *= .5 + .5 * np.sin(2 * np.pi * (1. / dur) * (np.arange(n + x) / SR) * 2) ** 2
ramp = np.linspace(0, 1, x)
breath[:x] = breath[:x] * ramp + breath[n:n + x] * (1 - ramp)
breath = breath[:n]
write('SFX_FeyJellyHum', out * .8 + breath * .35, -10., loop=True)

(OUT / 'fey-jelly-sfx-manifest.json').write_text(json.dumps({'sources': 'Synthesised (Tools/build_fey_jelly_sfx.py); no recordings.', 'sounds': MANIFEST}, indent=1) + '\n', encoding='utf-8')
print('CHUCK_FEY_JELLY_SFX_READY', len(MANIFEST))
