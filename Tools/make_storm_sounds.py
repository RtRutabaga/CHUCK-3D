"""Cut the ship's storm sounds from CC0 recordings (Claude, 2026-11-03; docs/SHIP.md "The chest, the trigger and the storm").

<blender>/4.5/python/bin/python.exe -I Tools/make_storm_sounds.py <download folder>
(numpy is needed: Blender's own Python has it.)

The originals are not kept in the repository; each is listed with its page, download URL and SHA-256 in
SourceAssets/Audio/SFX/storm-recorded-manifest.json, and this script checks those hashes before cutting. The download
folder holds:
  bsb/<NNNN>.wav   BigSoundBank (Joseph Sardin, CC0) BWF downloads, https://bigsoundbank.com/UPLOAD/bwf-en/<NNNN>.wav
  dec/<NNNN>.wav   BigSoundBank recordings served to us only as MP3 (https://bigsoundbank.com/UPLOAD/mp3/<NNNN>.mp3,
                   in mp3/), decoded by Tools/decode_storm_mp3.py (Blender's audaspace)

Writes to SourceAssets/Audio/SFX (mono 16-bit, peaks at -3 dBFS):
  SFX_StormRain_00          30 s loop of heavy rain on a terrace (the rain on deck)
  SFX_StormRainAbove_00     30 s loop of rain on a tent heard from inside (the rain overhead below decks; the game
                            also low-passes it)
  SFX_StormWind_00          40 s loop: strong wind with whistling wind under it (the gale on deck)
  SFX_StormWindBelow_00     40 s loop of wind heard from indoors (below decks)
  SFX_StormFlap_00          30 s loop of a flag flogging in strong wind (the canvas)
  SFX_StormSea_00           30 s loop of heavy waves (the sea in the storm, on deck)
  SFX_StormThunderNear_00.. close strikes: a sharp crack and its roll
  SFX_StormThunderFar_00..  distant thunder rolling
  SFX_StormSlam_00..        waves hitting rock (the sea slamming into the bow and the weather side)
Loops take the steadiest stretch of their recording (the window with the smallest swing in loudness, no spikes) and
crossfade their ends; one-shots are cut from just before their onset to where they have died away. Only cuts, fades,
crossfades, mixing (the gale's two winds, the whistle resampled to the wind's rate; stereo to mono) and peak normalisation are applied; the game does the
muffling below decks.
"""
import hashlib, json, sys, wave
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
SRC = Path(sys.argv[1])
MANIFEST = ROOT / 'storm-recorded-manifest.json'
PAGE = 'https://bigsoundbank.com/'
SOURCES = {
    '1019': ('Summer Rain on Terrace', 'summer-rain-on-terrace-s1019.html', 'bsb'),
    '0820': ('Rain and thunder in a tent', 'rain-and-storm-in-a-tent-s0820.html', 'bsb'),
    '0153': ('Strong wind #3', 'strong-wind-3-s0153.html', 'dec'),
    '0147': ('Whistling of the Wind #1', 'whistling-of-the-wind-1-s0147.html', 'bsb'),
    '1715': ('Wind from inside #2', 'wind-from-inside-2-s1715.html', 'bsb'),
    '1633': ('Flag in the Wind #2', 'flag-in-the-wind-2-s1633.html', 'dec'),
    '0698': ('Sea Waves', 'sea-waves-s0698.html', 'bsb'),
    '2570': ('Cliff #1', 'cliff-1-s2570.html', 'dec'),
    '2718': ('Thunder #1', 'thunder-s2718.html', 'bsb'),
    '3113': ('Thunder #2', 'thunder-2-s3113.html', 'bsb'),
    '3114': ('Thunder #3', 'thunder-3-s3114.html', 'bsb'),
    '3115': ('Thunder #4', 'thunder-4-s3115.html', 'bsb'),
    '3116': ('Thunder #5', 'thunder-5-s3116.html', 'bsb'),
    '3179': ('Thunder #6', 'thunder-6-s3179.html', 'bsb'),
    '3180': ('Thunder #7', 'thunder-7-s3180.html', 'bsb'),
    '3181': ('Thunder #8', 'thunder-8-s3181.html', 'bsb'),
    '3182': ('Thunder #9', 'thunder-9-s3182.html', 'bsb'),
    '3183': ('Thunder #10', 'thunder-10-s3183.html', 'bsb'),
    '3184': ('Thunder #11', 'thunder-11-s3184.html', 'bsb'),
}
# By the analysis of 2026-11-03: the sharpest onsets (a jump of 20 dB and more within 60 ms) are the near strikes.
NEAR = ['2718', '3115', '3183', '3114']
FAR = ['3113', '3116', '3179', '3180', '3181', '3182', '3184']


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


old = json.loads(MANIFEST.read_text()) if MANIFEST.exists() else None
known = {s['local']: s['sha256'] for s in old['sources']} if old else {}
sources = []
for n, (title, page, kind) in SOURCES.items():
    local = f'{kind}/{n}.wav'
    p = SRC / local
    h = sha(p)
    if local in known and known[local] != h:
        raise RuntimeError(f'{local}: hash differs from the manifest')
    url = f'{PAGE}UPLOAD/bwf-en/{n}.wav' if kind == 'bsb' else f'{PAGE}UPLOAD/mp3/{n}.mp3'
    entry = dict(local=local, title=title, author='Joseph Sardin (BigSoundBank)', license='CC0 1.0', page=PAGE + page, url=url,
                 bytes=p.stat().st_size, sha256=h)
    if kind == 'dec':
        entry['decoded_from'] = f'mp3/{n}.mp3'; entry['mp3_sha256'] = sha(SRC / 'mp3' / f'{n}.mp3')
    sources.append(entry)


def read(n):
    kind = SOURCES[n][2]
    with wave.open(str(SRC / kind / f'{n}.wav')) as w:
        ch, width, rate, count = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(count)
    if width == 2:
        d = np.frombuffer(raw, dtype='<i2').astype(np.float64) / 32768
    elif width == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        d = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        d = np.where(d >= 1 << 23, d - (1 << 24), d).astype(np.float64) / 8388608
    else:
        raise RuntimeError(f'{n}: unsupported sample width {width}')
    return d.reshape(-1, ch).mean(axis=1), rate


def env_db(d, rate, step):
    w = int(rate * step); m = len(d) // w
    return 20 * np.log10(np.sqrt((d[:m * w].reshape(m, w) ** 2).mean(axis=1)) + 1e-9)


entries = []


def write(name, d, rate, note):
    d = d * (.7079 / (np.max(np.abs(d)) or 1.))   # -3 dBFS
    pcm = np.clip(np.round(d * 32767), -32767, 32767).astype('<i2')
    path = ROOT / f'{name}.wav'
    with wave.open(str(path), 'wb') as w:
        w.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
        w.writeframes(pcm.tobytes())
    entries.append(dict(name=name, file=path.name, duration=round(len(pcm) / rate, 3), rate=rate, sha256=sha(path), note=note))
    print('CHUCK_STORM_SOUND', name, round(len(pcm) / rate, 2))


def steadiest(d, rate, length, margin=5.):
    """The start (s) of the window of this length whose loudness (1 s steps) swings least."""
    e = env_db(d, rate, 1.)
    L = int(length) + 2
    best, at = 1e9, margin
    for s in range(int(margin), len(e) - L - int(margin)):
        w = e[s:s + L]
        swing = np.percentile(w, 98) - np.percentile(w, 5)
        if swing < best: best, at = swing, s
    return float(at), best


def loop(n, length, cross=2.):
    d, rate = read(n)
    at, swing = steadiest(d, rate, length + cross)
    a = int(at * rate); L = int(length * rate); X = int(cross * rate)
    seg = d[a:a + L + X].copy()
    out = seg[:L].copy()
    t = np.linspace(0, 1, X)
    out[:X] = seg[:X] * np.sqrt(t) + seg[L:L + X] * np.sqrt(1 - t)   # equal-power: the tail runs into the head
    return out, rate, at, swing


def one_shot(n, start=None, longest=22., floor=-40., fade=1.5):
    d, rate = read(n)
    e = env_db(d, rate, .02)
    peak = e.max()
    if start is None:
        # From 0.1 s before the first moment within 12 dB of its peak.
        first = int(np.argmax(e > peak - 12)) * .02
        start = max(0., first - .1)
    a = int(start * rate)
    tail = e[int(start / .02):]
    # To where it has fallen to the floor (relative to its peak) and stays there for a second, or the longest.
    end = len(tail) * .02
    quiet = tail < peak + floor   # 40 dB under its peak
    run = 0
    for i, q in enumerate(quiet):
        run = run + 1 if q else 0
        if run * .02 >= 1.: end = (i - run + 1) * .02 + .5; break
    end = min(end, longest)
    seg = d[a:a + int(end * rate)].copy()
    fi, fo = int(.005 * rate), int(min(fade, end * .3) * rate)
    seg[:fi] *= np.linspace(0, 1, fi)
    seg[-fo:] *= np.linspace(1, 0, fo) ** 2
    return seg, rate, start


# ---- The loops.
x, r, at, sw = loop('1019', 30.); write('SFX_StormRain_00', x, r, f'1019 from {at:.0f} s, steadiest 32 s (swing {sw:.1f} dB), 2 s crossfade')
x, r, at, sw = loop('0820', 30.); write('SFX_StormRainAbove_00', x, r, f'0820 from {at:.0f} s, steadiest 32 s (swing {sw:.1f} dB), 2 s crossfade')
g, r, at, sw = loop('0153', 40.)
h, r2, at2, sw2 = loop('0147', 40.)
if r2 != r:   # (0147 is 44.1 kHz, the decoded 0153 48 kHz: the whistle is resampled to match, linearly)
    h = np.interp(np.arange(int(len(h) * r / r2)) * r2 / r, np.arange(len(h)), h)
n = min(len(g), len(h)); g, h = g[:n], h[:n]
g = g / np.max(np.abs(g)); h = h / np.max(np.abs(h))
write('SFX_StormWind_00', g + .45 * h, r, f'0153 from {at:.0f} s with 0147 from {at2:.0f} s at -7 dB under it, 40 s, 2 s crossfade')
x, r, at, sw = loop('1715', 40.); write('SFX_StormWindBelow_00', x, r, f'1715 from {at:.0f} s, steadiest 42 s (swing {sw:.1f} dB), 2 s crossfade')
x, r, at, sw = loop('1633', 30.); write('SFX_StormFlap_00', x, r, f'1633 from {at:.0f} s, steadiest 32 s (swing {sw:.1f} dB), 2 s crossfade')
x, r, at, sw = loop('0698', 30.); write('SFX_StormSea_00', x, r, f'0698 from {at:.0f} s, steadiest 32 s (swing {sw:.1f} dB), 2 s crossfade')

# ---- Thunder.
for i, n in enumerate(NEAR):
    x, r, s = one_shot(n); write(f'SFX_StormThunderNear_{i:02d}', x, r, f'{n} from {s:.2f} s')
for i, n in enumerate(FAR):
    x, r, s = one_shot(n); write(f'SFX_StormThunderFar_{i:02d}', x, r, f'{n} from {s:.2f} s')

# ---- The sea's slams: the strongest wave hits in the cliff recording, at least 5 s apart, each from 0.25 s before
# its hit for 3 s.
d, rate = read('2570')
e = env_db(d, rate, .05)
rise = np.concatenate([np.zeros(4), e[4:] - e[:-4]])   # the jump into each hit over 0.2 s
score = e + .6 * rise
order = np.argsort(score)[::-1]
picked = []
for i in order:
    t = i * .05
    if t < 6 or t > len(d) / rate - 4: continue   # (not the recording's first seconds: it starts from silence)
    if all(abs(t - p) > 5 for p in picked): picked.append(t)
    if len(picked) == 6: break
for k, t in enumerate(sorted(picked)):
    a = int((t - .25) * rate); seg = d[a:a + int(3. * rate)].copy()
    fi, fo = int(.004 * rate), int(1.2 * rate)
    seg[:fi] *= np.linspace(0, 1, fi); seg[-fo:] *= np.linspace(1, 0, fo) ** 2
    write(f'SFX_StormSlam_{k:02d}', seg, rate, f'2570 from {t - .25:.2f} s, 3 s')

MANIFEST.write_text(json.dumps(dict(provenance='Cut by Tools/make_storm_sounds.py from CC0 recordings; originals not kept in the repository (see sources)',
                                    sources=sources, outputs=entries), indent=2) + '\n')
print('CHUCK_STORM_SOUNDS', len(entries))
