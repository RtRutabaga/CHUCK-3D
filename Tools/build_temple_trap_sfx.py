"""Blender 4.5.14 (for its audio decoder and numpy): the temple traps' sounds from recorded
CC0 sources (Claude, 2026-10-27).

blender --background --factory-startup --python Tools/build_temple_trap_sfx.py
  SourceAssets/Audio/SFX/Temple/Source/<pack>/*  (CC0, see Source/manifest.json)
  -> SourceAssets/Audio/SFX/SFX_Temple*.wav   (the same 32 names: mono, 48 kHz, 16-bit)
     SourceAssets/Audio/SFX/temple-trap-manifest.json, temple-run-manifest.json
Then Tools/import_temple_trap_sfx.py imports them as before (it takes every SFX_Temple*.wav).
Supersedes Tools/gen_temple_trap_sfx.py and Tools/gen_temple_run_sfx.py (synthetic).

User request: "Find and integrate free sounds for the various temple traps, they should be a
bit quieter than they are now". Sources: Joseph Sardin's BigSoundBank recordings, LEGIT
Audio's Fire Staff pack and artisticdude's swishes (OpenGameArt), Kenney's Impact Sounds.
Cuts were chosen from envelope scans, not auditioned by Claude. Each slot is levelled to the
synthetic sound's loudness it replaces (LOUD, the mean of its loudest quarter of 50 ms frames),
less EXTRA dB for the loudest families; AChultTempleTraps::Play then scales all of them by
TrapVolume. Every sound gets the same short stone-chamber reverb, seeded.
"""
from pathlib import Path
import hashlib, json, wave

import aud
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'SourceAssets/Audio/SFX/Temple/Source'
OUT = ROOT / 'SourceAssets/Audio/SFX'
SR = 48000
CEIL = 10 ** (-1 / 20)

# Loudness of the synthetic sounds these replace (measured 2026-10-27, dBFS).
LOUD = {'SpikeRattle': -21.7, 'SpikeThrust': -24.5, 'DartClick': -27.0, 'DartShot': -23.1, 'DartHit': -28.6,
        'SlabCrack': -24.3, 'SlabFall': -14.5, 'RunAxeSwoosh': -18.2, 'RunAxeCreak': -16.4, 'RunFireHiss': -15.0,
        'RunFireRoar': -15.7, 'RunSawSpin': -12.7, 'RunSawDrop': -21.4, 'RunHit': -23.5}
# The loudest families come down further: the saw's whine repeats every 1.1 s near it.
EXTRA = {'RunSawSpin': -5, 'RunFireHiss': -3, 'RunFireRoar': -2, 'RunAxeCreak': -3, 'SlabFall': -2}


def load(rel):
    s = aud.Sound(str(SRC / rel)).resample(SR, False)
    a = np.asarray(s.data(), dtype=np.float64)
    return a.mean(axis=1) if a.ndim > 1 else a


def cut(x, start, end):
    return x[int(start * SR):int(end * SR)]


def rate(x, r):
    n = int(len(x) / r)
    return np.interp(np.arange(n) * r, np.arange(len(x)), x)


def eq(x, low=None, high=None):
    """Gentle (2nd-order-like) low-pass above `low` Hz and high-pass below `high` Hz."""
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1 / SR) + 1e-9
    g = np.ones_like(f)
    if low: g /= np.sqrt(1 + (f / low) ** 4)
    if high: g /= np.sqrt(1 + (high / f) ** 4)
    return np.fft.irfft(X * g, len(x))


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


rng = np.random.default_rng(2710)
_t = np.arange(int(1.1 * SR)) / SR
IR = rng.standard_normal(len(_t)) * np.exp(-_t * 6.9 / 1.0)   # RT60 about 1 s
IR = eq(IR, low=3500, high=150); IR[:int(.012 * SR)] = 0
for d, g in ((.017, .5), (.029, .35), (.043, .25)):            # corridor walls
    IR[int(d * SR)] += g * np.abs(IR).max() * 4
IR /= np.sqrt((IR ** 2).sum())


def room(x, wet=-15):
    y = np.fft.irfft(np.fft.rfft(np.pad(x, (0, len(IR)))) * np.fft.rfft(np.pad(IR, (0, len(x)))), len(x) + len(IR))
    return np.pad(x, (0, len(IR))) + y * 10 ** (wet / 20)


def loud(x):
    hop = int(.05 * SR); n = max(1, len(x) // hop)
    e = np.array([np.sqrt(np.mean(x[i * hop:(i + 1) * hop] ** 2)) + 1e-12 for i in range(n)])
    return 20 * np.log10(np.sort(e)[-max(1, n // 4):].mean())


def limit(x, ceil=CEIL, release=.08):
    """Peak limiter: 2 ms blocks, instant attack (one block early), exponential release."""
    blk = int(.002 * SR); n = -(-len(x) // blk)
    peaks = np.abs(np.pad(x, (0, n * blk - len(x)))).reshape(n, blk).max(axis=1)
    peaks = np.maximum(peaks, np.concatenate([peaks[1:], [0]]))
    want = np.minimum(1, ceil / np.maximum(peaks, 1e-9))
    g = np.empty(n); cur = 1.; k = np.exp(-.002 / release)
    for i, w in enumerate(want):
        cur = w if w < cur else w + (cur - w) * k
        g[i] = cur
    return x * np.interp(np.arange(len(x)), np.arange(n) * blk + blk / 2, g)


entries = {'trap': [], 'run': []}


def write(slot, i, x, length, wet=-15, tail=.15):
    name = f'SFX_Temple{slot}_{i:02d}'
    x = room(x, wet)[:int(length * SR)]
    x = fade(x, .003, tail)
    target = LOUD[slot] + EXTRA.get(slot, 0)
    for _ in range(3):   # level, limit the peaks that then stand above -1 dBFS, re-level
        x = limit(x * 10 ** ((target - loud(x)) / 20))
    if np.abs(x).max() > CEIL: x *= CEIL / np.abs(x).max()
    path = OUT / f'{name}.wav'
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype('<i2').tobytes())
    entries['run' if slot.startswith('Run') else 'trap'].append(
        {'name': name, 'duration': round(len(x) / SR, 3), 'loudness_db': round(loud(x), 1), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    print('CHUCK_TEMPLE_TRAP_SFX', name, round(len(x) / SR, 2), round(loud(x), 1))


B, F, S, K = 'BigSoundBank/', 'LEGIT-fire-staff/', 'artisticdude-swishes/', 'Kenney-impact/'
chain1, chain3, heavy_chain = load(B + 'bsb_0355.wav'), load(B + 'bsb_0357.wav'), load(B + 'bsb_0359.wav')
scrape, archery, arrow = load(B + 'bsb_0327.wav'), load(B + 'bsb_0549.wav'), load(B + 'bsb_0548.wav')
whoosh, tomb, stone = load(B + 'bsb_0573.flac'), load(B + 'bsb_0580.wav'), load(B + 'bsb_1022.flac')
creak, saw = load(B + 'bsb_0616.wav'), load(B + 'bsb_1135.flac')
fire4, fire5, fire21 = (load(F + f'LEGIT_FIR_Fire_Staff {n}-Audio.wav') for n in (4, 5, 21))
k = lambda n: load(K + n + '.ogg')

# Spikes: the tell is iron shivering in the grate (a small chain shaken, its top taken off),
# the thrust a blade scraped on stone over an iron clank and a low thud of the mechanism.
write('SpikeRattle', 0, mix((eq(cut(chain1, .06, .74), low=6000, high=300), 0, 0), (k('impactMetal_light_002'), -14, .02)), .6, tail=.12)
write('SpikeRattle', 1, mix((eq(cut(chain3, .00, .70), low=6000, high=300), 0, 0), (k('impactMetal_light_004'), -14, .05)), .6, tail=.12)
for v, (m, mine, r) in enumerate([('000', '000', 1.), ('002', '002', .93), ('004', '004', 1.07)]):
    write('SpikeThrust', v, mix((rate(eq(cut(scrape, .06, .45), low=7000), r), 0, 0), (eq(k(f'impactMetal_heavy_{m}'), low=5000), -4, .03),
                                (rate(k(f'impactMining_{mine}'), .7), -6, .05)), 1.3)
# Darts: a launcher cocking (two dry knocks), the release (a bow's twang and the arrow's
# air), the strike (an arrow into a target).
for v, (a, b) in enumerate([('impactWood_light_001', 'impactMetal_light_000'), ('impactWood_light_003', 'impactMetal_light_001')]):
    write('DartClick', v, mix((eq(k(a), low=5000), 0, 0), (eq(k(b), low=4000), -10, 0), (eq(k(a), low=4000), -3, .13)), .7)
write('DartShot', 0, mix((cut(archery, .08, .91), 0, 0)), .9)
write('DartShot', 1, mix((cut(archery, .08, .30), 0, 0), (load(S + 'swish-11.wav'), -4, .06)), .7)
write('DartShot', 2, mix((cut(archery, .08, .30), -2, 0), (rate(load(S + 'swish-13.wav'), .9), -3, .05)), .7)
for v, s in enumerate((.10, 1.58, .78)):
    write('DartHit', v, mix((eq(cut(arrow, s, s + .55), low=6000), 0, 0)), .8)
# The cracked slab: stone grinding on stone (a tombstone pushed open) with grit,
# then its fall: a stone dropped, slowed for weight, ringing in the rift.
write('SlabCrack', 0, mix((cut(tomb, .26, 1.55), 0, 0), (k('impactMining_001'), -10, .1)), 1.4)
write('SlabCrack', 1, mix((cut(tomb, 4.53, 5.82), 0, 0), (k('impactMining_003'), -10, .2)), 1.4)
write('SlabFall', 0, mix((rate(cut(stone, 1.55, 4.40), .85), 0, 0)), 3.2, wet=-10, tail=.6)
write('SlabFall', 1, mix((rate(cut(stone, 11.55, 14.40), .85), 0, 0)), 3.2, wet=-10, tail=.6)
# The trap run: the pendulum axe's heavy whoosh (slowed) and its iron pivot creaking; the
# fire jets' rising hiss and roar; the saw blade's whine and its clanking drop; a hit.
for v, s in enumerate((2.53, 8.23, 9.44)):
    write('RunAxeSwoosh', v, rate(cut(whoosh, s - .45, s + .55), .72), 1.5, tail=.3)
write('RunAxeCreak', 0, eq(cut(creak, .05, 1.20), low=5000, high=200), 1.2, tail=.25)
write('RunAxeCreak', 1, eq(cut(creak, 12.25, 13.40), low=5000, high=200), 1.2, tail=.25)
write('RunFireHiss', 0, fade(eq(cut(fire21, .03, 1.40), high=400), .25, .1), 1.3, tail=.2)
write('RunFireHiss', 1, fade(eq(cut(fire21, 1.70, 3.10), high=400), .25, .1), 1.3, tail=.2)
write('RunFireRoar', 0, cut(fire4, .08, 2.60), 2.4, tail=.5)
write('RunFireRoar', 1, cut(fire5, .00, 2.50), 2.4, tail=.5)
write('RunSawSpin', 0, fade(eq(cut(saw, 2.0, 3.8), low=6000), .2, .3), 1.7, tail=.3)
write('RunSawSpin', 1, fade(eq(cut(saw, 9.0, 10.8), low=6000), .2, .3), 1.7, tail=.3)
write('RunSawDrop', 0, mix((cut(heavy_chain, 2.18, 3.05), 0, 0), (eq(k('impactMetal_heavy_001'), low=5000), -4, .02)), 1.7, tail=.4)
write('RunSawDrop', 1, mix((cut(heavy_chain, 3.27, 4.10), 0, 0), (eq(k('impactMetal_heavy_003'), low=5000), -4, .02)), 1.7, tail=.4)
write('RunHit', 0, mix((k('impactPunch_heavy_000'), 0, 0), (k('impactMetal_medium_001'), -9, .01)), .85)
write('RunHit', 1, mix((k('impactPunch_heavy_002'), 0, 0), (k('impactMetal_medium_003'), -9, .01)), .85)

prov = 'CC0 recordings (SFX/Temple/Source/manifest.json) cut and mixed by Tools/build_temple_trap_sfx.py'
(OUT / 'temple-trap-manifest.json').write_text(json.dumps({'provenance': prov, 'sounds': entries['trap']}, indent=2), encoding='utf-8')
(OUT / 'temple-run-manifest.json').write_text(json.dumps({'provenance': prov, 'sounds': entries['run']}, indent=2), encoding='utf-8')
print('CHUCK_TEMPLE_TRAP_SFX_READY', len(entries['trap']) + len(entries['run']))
