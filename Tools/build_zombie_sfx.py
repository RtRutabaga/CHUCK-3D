"""Blender 4.5.14 (for its audio decoder and numpy): the zombies' sounds from recorded
CC0 sources (Claude, 2026-10-16).

blender --background --factory-startup --python Tools/build_zombie_sfx.py
  SourceAssets/Audio/SFX/Zombies/Source/<pack>/*  (CC0, see Source/manifest.json)
  -> SourceAssets/Audio/SFX/SFX_Zombie*.wav   (the same 12 names: mono, 48 kHz, 16-bit, peak -3 dBFS)
     SourceAssets/Audio/SFX/zombie-manifest.json
Then Tools/import_zombie_sfx.py imports them as before. Supersedes Tools/gen_zombie_sfx.py.

User request: "find download and implement better noises for the zombies". The
vocals are Joseph Sardin's studio zombie recordings (BigSoundBank #2-#6) and
artisticdude's zombie pack (OpenGameArt); the hits are Kenney's punches; the
lunge's air is a rubberduck attack whoosh. Cuts were chosen from spectrograms
and envelope phrases, not auditioned by Claude:
- Rasp (the ambient groan every 4-7 s, and the grab): three breathy moans (#4, #5);
- Swing (the lunge): the loudest snarls of #2 and #3 over a whoosh;
- ClawHit (Chuck's scratch lands): a fleshy punch and a short grunt;
- ChuckHit (its blow lands on Chuck): a heavy punch and a body thump;
- Death: #6's burst-and-fade over a slow, low groan.
"""
from pathlib import Path
import hashlib, json, wave

import aud
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'SourceAssets/Audio/SFX/Zombies/Source'
OUT = ROOT / 'SourceAssets/Audio/SFX'
SR = 48000
PEAK = 10 ** (-3 / 20)


def load(rel):
    s = aud.Sound(str(SRC / rel)).resample(SR, False)
    a = np.asarray(s.data(), dtype=np.float64)
    return a.mean(axis=1) if a.ndim > 1 else a


def cut(x, start, end):
    return x[int(start * SR):int(end * SR)]


def rate(x, r):
    n = int(len(x) / r)
    return np.interp(np.arange(n) * r, np.arange(len(x)), x)


def fade(x, fin=.005, fout=.05):
    x = x.copy(); a = min(len(x), int(fin * SR)); b = min(len(x), int(fout * SR))
    if a: x[:a] *= np.linspace(0, 1, a)
    if b: x[-b:] *= np.linspace(1, 0, b) ** 2
    return x


def mix(*layers):
    n = max(int(o * SR) + len(s) for s, g, o in layers)
    out = np.zeros(n)
    for s, g, o in layers:
        i = int(o * SR); out[i:i + len(s)] += s * 10 ** (g / 20)
    return out


entries = []


def write(name, x, tail=.08, length=None):
    if length: x = x[:int(length * SR)]
    x = fade(x, .006, tail)
    x = x / max(np.abs(x).max(), 1e-9) * PEAK
    path = OUT / f'{name}.wav'
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype('<i2').tobytes())
    entries.append({'name': name, 'duration': round(len(x) / SR, 3), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    print('CHUCK_ZOMBIE_SFX', name, round(len(x) / SR, 2))


B = 'BigSoundBank/bsb_zombie_'
z2106, z2108, z2109, z2110, z2111 = (load(f'{B}{i}.flac') for i in (2106, 2108, 2109, 2110, 2111))
K, A, Z = 'Kenney-impact/', 'rubberduck-80-creature-2/', 'artisticdude-zombies/'

write('SFX_ZombieRasp_00', cut(z2109, 0.01, 1.25), tail=.25)
write('SFX_ZombieRasp_01', cut(z2109, 1.26, 2.69), tail=.3)
write('SFX_ZombieRasp_02', cut(z2110, 0.04, 1.23), tail=.25)
write('SFX_ZombieSwing_00', mix((cut(z2108, 4.34, 4.98), 0, 0), (load(A + 'attack_01.ogg'), -7, .04)), tail=.12)
write('SFX_ZombieSwing_01', mix((cut(z2106, 7.70, 8.32), 0, 0), (load(A + 'attack_05.ogg'), -7, .03)), tail=.12)
for v, (punch, grunt) in enumerate([('000', '24'), ('001', '5'), ('003', '6')]):
    write(f'SFX_ZombieClawHit_{v:02d}', mix((load(K + f'impactPunch_medium_{punch}.ogg'), 0, 0), (load(Z + f'zombie-{grunt}.wav'), -7, .03)), tail=.1, length=.42)
for v, (punch, soft) in enumerate([('000', '001'), ('001', '003'), ('004', '001')]):
    write(f'SFX_ZombieChuckHit_{v:02d}', mix((load(K + f'impactPunch_heavy_{punch}.ogg'), 0, 0), (rate(load(K + f'impactSoft_heavy_{soft}.ogg'), .9), -5, 0)), tail=.12, length=.42)
write('SFX_ZombieDeath_00', mix((cut(z2111, 0.04, 1.45), 0, 0), (rate(load(Z + 'zombie-16.wav'), .85), -7, .25)), tail=.5)

(OUT / 'zombie-manifest.json').write_text(json.dumps({'provenance': 'CC0 recordings (SFX/Zombies/Source/manifest.json) cut and mixed by Tools/build_zombie_sfx.py',
                                                     'sounds': entries}, indent=2), encoding='utf-8')
print('CHUCK_ZOMBIE_SFX_READY', len(entries))
