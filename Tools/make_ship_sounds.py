"""Cut the ship's recorded sounds from CC0 recordings (Claude, 2026-11-03).

python -I Tools/make_ship_sounds.py <download folder>

The originals are not kept in the repository (about 160 MB); each is listed with
its page, download URL and SHA-256 in SourceAssets/Audio/SFX/ship-recorded-manifest.json,
and this script checks those hashes before cutting. The download folder holds:
  bsb/<NNNN>.wav   BigSoundBank (Joseph Sardin, CC0) BWF downloads,
                   https://bigsoundbank.com/UPLOAD/bwf-en/<NNNN>.wav
  x/<name>.wav     from Still North Media's "Medieval sound effects - Weapon impacts"
                   (Ben Jaszczak & Brian Nelson, CC0), OpenGameArt:
                   medieval_sfx_weapon_on_weapon_1_of_2.7z / _2_of_2.7z

Writes to SourceAssets/Audio/SFX (mono 16-bit, peaks at -3 dBFS):
  SFX_SwordClash_00..09   single sabre-on-sword hits, each to the end of its ring
  SFX_SwordBind_00..01    hilt-on-blade hits (heavier, for the bind)
  SFX_SwordScrape_00      blade sliding along blade (the bind's shove)
  SFX_SwordSwish_00       a sword cutting the air
  SFX_DeckStep_00..08     boot steps on a wooden pontoon
  SFX_DeckStamp_00..03    two-footed jumps and hard steps on wood (the dancer)
  SFX_ShipWater_00        30 s loop of froth and small waves (the sea along the hull, on deck)
  SFX_ShipLap_00          30 s loop of water lapping round moored hulls (below decks)
  SFX_ShipRigging_00      36 s loop of wind whistling in masts and rigging (on deck)
Only cuts, loop crossfades, fades, downsampling (192 kHz sources to 48 kHz),
stereo-to-mono mixing and peak normalisation are applied.
"""
import array, hashlib, json, math, sys, wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
SRC = Path(sys.argv[1])
MANIFEST = ROOT / 'ship-recorded-manifest.json'

sources = json.loads(MANIFEST.read_text())['sources'] if MANIFEST.exists() else None


def read(rel, start=0., dur=None):
    p = SRC / rel
    with wave.open(str(p)) as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        a = min(n, int(start * rate)); b = n if dur is None else min(n, a + int(dur * rate))
        w.setpos(a)
        raw = w.readframes(b - a)
    if width == 2:
        data = array.array('h', raw); scale = 32768.
    elif width == 3:
        data = [int.from_bytes(raw[i:i + 3], 'little', signed=True) for i in range(0, len(raw), 3)]; scale = 8388608.
    else:
        raise RuntimeError(f'{rel}: unsupported sample width {width}')
    m = len(data) // ch
    mono = [sum(data[i * ch + c] for c in range(ch)) / ch / scale for i in range(m)] if ch > 1 else [x / scale for x in data]
    return mono, rate


def down4(s):
    """192 kHz to 48 kHz: a 4-tap box then a 3-tap smoothing, keeping every fourth sample."""
    box = [(s[i] + s[i + 1] + s[i + 2] + s[i + 3]) * .25 for i in range(0, len(s) - 3, 4)]
    return [(box[max(i - 1, 0)] + 2 * box[i] + box[min(i + 1, len(box) - 1)]) * .25 for i in range(len(box))]


entries = []


def write(name, samples, rate, note):
    peak = max(abs(x) for x in samples) or 1.
    gain = .7079 / peak   # -3 dBFS
    pcm = array.array('h', (max(-32767, min(32767, int(x * gain * 32767))) for x in samples))
    path = ROOT / f'{name}.wav'
    with wave.open(str(path), 'wb') as w:
        w.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
        w.writeframes(pcm.tobytes())
    entries.append(dict(name=name, file=path.name, duration=round(len(pcm) / rate, 3), rate=rate,
                        sha256=hashlib.sha256(path.read_bytes()).hexdigest(), note=note))
    print('CHUCK_SHIP_SOUND', name, round(len(pcm) / rate, 2))


def fade(s, rate, fin=.003, fout=.08):
    s = s[:]
    a, b = int(fin * rate), int(fout * rate)
    for i in range(min(a, len(s))): s[i] *= i / a
    for i in range(min(b, len(s))): s[-1 - i] *= i / b
    return s


def loop(samples, rate, start, length, xf):
    a, n, f = int(start * rate), int(length * rate), int(xf * rate)
    body = samples[a:a + n]; tail = samples[a + n:a + n + f]
    out = body[:]
    for i in range(f):
        t = i / f
        out[i] = tail[i] * math.cos(t * math.pi / 2) + body[i] * math.sin(t * math.pi / 2)
    return out


def envelope(s, rate, win=.01):
    w = max(1, int(win * rate))
    return [math.sqrt(sum(x * x for x in s[i:i + w]) / w) for i in range(0, len(s) - w, w)], w


def onsets(s, rate, rel=.12, gap=.12):
    """Local maxima of the 10 ms envelope above `rel` of its peak and well above its floor, at least `gap` apart: (sample, level)."""
    env, w = envelope(s, rate)
    floor = sorted(env)[len(env) // 5]; th = max(floor * 6, max(env) * rel)
    out = []
    for i in range(1, len(env) - 1):
        if env[i] > th and env[i] >= env[i - 1] and env[i] >= env[i + 1]:
            if out and (i * w - out[-1][0]) < gap * rate:
                if env[i] > out[-1][1]: out[-1] = (i * w, env[i])
                continue
            out.append((i * w, env[i]))
    return out


def hits(s, rate, longest, lead=.015):
    """Each onset to the next (or `longest` s), starting `lead` s before its attack; quietest hits last."""
    ons = onsets(s, rate)
    cuts = []
    for k, (i, lv) in enumerate(ons):
        a = max(0, i - int(.02 * rate))
        # the attack: the first sample within 25 ms before the envelope peak that reaches a quarter of it
        seg = s[a:i + int(.01 * rate)]
        pk = max((abs(x) for x in seg), default=0.)
        j = next((q for q, x in enumerate(seg) if abs(x) > pk * .25), 0)
        start = max(0, a + j - int(lead * rate))
        end = min(len(s), start + int(longest * rate), ons[k + 1][0] - int(.03 * rate) if k + 1 < len(ons) else len(s))
        if end - start > .25 * rate: cuts.append((lv, s[start:end], start / rate))
    return sorted(cuts, key=lambda c: -c[0])


def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()


# Check the downloads against the recorded hashes (first run records them).
if sources:
    for src in sources:
        got = sha(SRC / src['local'])
        if got != src['sha256']: raise SystemExit(f"CHUCK_SHIP_SOUND_SOURCE_MISMATCH {src['local']} {got}")

# ---- Steel.
clash = []
for f in ('Sabre Katana Blade on Blade.wav', 'Sabre Norse Sword Blade on Blade.wav'):
    s, r = read('x/' + f)
    clash += [(lv, down4(c), f, t) for lv, c, t in hits(s, r, .9)]
clash.sort(key=lambda c: -c[0])
for k, (lv, c, f, t) in enumerate(clash[:10]):
    write(f'SFX_SwordClash_{k:02d}', fade(c, 48000, .002, .12), 48000, f'Still North Media "{f}" at {t:.2f} s (192 to 48 kHz)')
s, r = read('x/Sabre Norse Sword Hilt on Blade.wav')
for k, (lv, c, t) in enumerate(hits(s, r, 1.)[:2]):
    write(f'SFX_SwordBind_{k:02d}', fade(down4(c), 48000, .002, .15), 48000, f'Still North Media "Sabre Norse Sword Hilt on Blade.wav" at {t:.2f} s (192 to 48 kHz)')
s, r = read('bsb/0129.wav', .2)
write('SFX_SwordScrape_00', fade(s, r, .01, .1), r, 'BSB #0129 "Sword", from 0.2 s')
s, r = read('bsb/0128.wav')
write('SFX_SwordSwish_00', fade(s, r, .005, .05), r, 'BSB #0128 "Sword Through the Air"')

# ---- Feet on the deck.
s, r = read('bsb/1845.wav')
steps = sorted(hits(s, r, .32, .01), key=lambda c: c[2])[:9]
for k, (lv, c, t) in enumerate(steps):
    write(f'SFX_DeckStep_{k:02d}', fade(c, r, .002, .1), r, f'BSB #1845 "Walk on Pontoon" at {t:.2f} s')
stamps = []
for n in ('1841', '1842'):
    s, r = read(f'bsb/{n}.wav')
    lv, c, t = hits(s, r, .45, .01)[0]
    stamps.append((c, r, f'BSB #{n} "Jumping on wooden pontoon" at {t:.2f} s'))
s, r = read('bsb/1516.wav', 0., 2.5)
for lv, c, t in sorted(hits(s, r, .3, .01)[:2], key=lambda c: c[2]):
    stamps.append((c, r, f'BSB #1516 "Steps on a Wooden Floor #2" at {t:.2f} s'))
for k, (c, r, note) in enumerate(stamps):
    write(f'SFX_DeckStamp_{k:02d}', fade(c, r, .002, .12), r, note)

# ---- The sea and the wind.
s, r = read('bsb/1447.wav', 4., 34.)
write('SFX_ShipWater_00', loop(s, r, 0., 30., 3.), r, 'BSB #1447 "Small Waves, Froth and Beach", 4-34 s loop (3 s crossfade)')
s, r = read('bsb/1444.wav', 60., 34.)
write('SFX_ShipLap_00', loop(s, r, 0., 30., 3.), r, 'BSB #1444 "Pontoon Marina", 60-90 s loop (3 s crossfade)')
s, r = read('bsb/2443.wav', 59., 40.)
write('SFX_ShipRigging_00', loop(s, r, 0., 36., 3.), r, 'BSB #2443 "Masts Whistling #1", 59-95 s loop (3 s crossfade)')

if not sources:
    raise SystemExit('CHUCK_SHIP_SOUND_NO_MANIFEST (write the sources list first)')
MANIFEST.write_text(json.dumps(dict(
    provenance='Cut by Tools/make_ship_sounds.py from CC0 recordings; originals not kept in the repository (see sources)',
    sources=sources, sounds=entries), indent=2) + '\n')
print('CHUCK_SHIP_SOUNDS_READY', len(entries))
