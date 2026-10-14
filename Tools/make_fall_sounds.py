"""Cut the fall cutscene's air loops and ground thud from the user's CC0 recordings (Claude, 2026-10-13).

python -I Tools/make_fall_sounds.py

Sources (BigSoundBank, Joseph Sardin, CC0; downloaded by the user, kept
unmodified in SourceAssets/Audio/SFX/BigSoundBank, hashes in its manifest):
  BSB_0595_Wind.wav                  "Wind", studio Foley wind
  BSB_1633_FlagInTheWind2_decoded.wav "Flag in the Wind #2" (FLAC decoded to 16-bit PCM, no gain)
  BSB_0155_WhistlingOfTheWind2.wav   "Whistling of the Wind #2"
  BSB_2456_BodyFall5.wav             "Body fall #5" (optional until present)
  BSB_1837_JumpingOnDirtRoad1.wav    "Jumping on dirt road #1" (optional until present)

Writes to SourceAssets/Audio/SFX (mono 16-bit, peaks at -3 dBFS):
  SFX_FallRush_00     30 s seamless loop of the wind (10-40 s, 3 s equal-power crossfade)
  SFX_FallFlap_00     42 s loop of the flapping cloth (his jacket)
  SFX_FallWhistle_00  36 s loop of the whistling (the high, fast part of the fall)
  SFX_FallThud_00     the body hitting the jungle floor: the body fall low-passed
                      and slowed a fifth (tile becomes soft earth) under the dirt
                      impact, both trimmed to their hit
Only loop cuts, crossfades, the thud's filtering/pitch/layering and peak
normalisation are applied; nothing else is changed in the recordings.
"""
import array, hashlib, json, math, wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
SRC = ROOT / 'BigSoundBank'


def read(name):
    with wave.open(str(SRC / name)) as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 2:
        data = array.array('h', raw); scale = 32768.
    elif width == 3:
        data = [int.from_bytes(raw[i:i + 3], 'little', signed=True) for i in range(0, len(raw), 3)]; scale = 8388608.
    else:
        raise RuntimeError(f'{name}: unsupported sample width {width}')
    mono = [sum(data[i * ch + c] for c in range(ch)) / ch / scale for i in range(n)]
    return mono, rate


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
    print('CHUCK_FALL_SOUND', name, round(len(pcm) / rate, 2))


def loop(samples, rate, start, length, fade):
    """Body [start, start+length) whose last `fade` seconds are crossfaded (equal power) into its start."""
    a, n, f = int(start * rate), int(length * rate), int(fade * rate)
    body = samples[a:a + n]
    tail = samples[a + n:a + n + f]
    out = body[:]
    for i in range(f):
        t = i / f
        out[i] = tail[i] * math.cos(t * math.pi / 2) + body[i] * math.sin(t * math.pi / 2)
    return out


def lowpass(samples, rate, cutoff):
    k = 1 - math.exp(-2 * math.pi * cutoff / rate)
    y = 0.; out = []
    for x in samples:
        y += k * (x - y); out.append(y)
    return out


def resample(samples, factor):
    """Play `factor` times slower (linear interpolation): pitch and speed drop together."""
    n = int(len(samples) * factor)
    out = []
    for i in range(n):
        p = i / factor; j = int(p); t = p - j
        out.append(samples[j] * (1 - t) + samples[min(j + 1, len(samples) - 1)] * t)
    return out


def hit(samples, rate, before=.02, after=1.2):
    """Trim to the loudest moment: a little before it, `after` seconds following, faded out."""
    peak = max(range(len(samples)), key=lambda i: abs(samples[i]))
    a = max(0, peak - int(before * rate)); b = min(len(samples), peak + int(after * rate))
    out = samples[a:b]
    fade = int(.25 * rate)
    for i in range(fade):
        out[-1 - i] *= i / fade
    return out


wind, r = read('BSB_0595_Wind.wav')
write('SFX_FallRush_00', loop(wind, r, 10., 30., 3.), r, 'BSB #0595 Wind, 10-40 s loop')
flap, r = read('BSB_1633_FlagInTheWind2_decoded.wav')
write('SFX_FallFlap_00', loop(flap, r, 3., 42., 3.), r, 'BSB #1633 Flag in the Wind #2, 3-45 s loop')
whistle, r = read('BSB_0155_WhistlingOfTheWind2.wav')
write('SFX_FallWhistle_00', loop(whistle, r, 2., 36., 3.), r, 'BSB #0155 Whistling of the Wind #2, 2-38 s loop')

body_path, dirt_path = SRC / 'BSB_2456_BodyFall5.wav', SRC / 'BSB_1837_JumpingOnDirtRoad1.wav'
if body_path.exists() and dirt_path.exists():
    body, rb = read(body_path.name)
    dirt, rd = read(dirt_path.name)
    if rb != rd: raise RuntimeError('thud sources differ in sample rate')
    # Tile to soft earth: slow it a fifth (deeper, heavier) and take off the hard top.
    body = lowpass(resample(hit(body, rb, .02, 1.2), 1.5), rb, 900.)
    dirt = hit(dirt, rd, .01, .8)
    n = max(len(body), len(dirt))
    body += [0.] * (n - len(body)); dirt += [0.] * (n - len(dirt))
    pb = max(abs(x) for x in body) or 1.; pd = max(abs(x) for x in dirt) or 1.
    write('SFX_FallThud_00', [body[i] / pb + .45 * dirt[i] / pd for i in range(n)], rb,
          'BSB #2456 Body fall #5 (x1.5 slower, 900 Hz low-pass) + 0.45 x BSB #1837 Jumping on dirt road #1, aligned on their hits')
else:
    print('CHUCK_FALL_THUD_PENDING sources missing:', [p.name for p in (body_path, dirt_path) if not p.exists()])

(ROOT / 'fall-recorded-manifest.json').write_text(json.dumps(dict(
    provenance='Loops and layers cut by Tools/make_fall_sounds.py from CC0 BigSoundBank recordings (Joseph Sardin); originals in BigSoundBank/',
    sounds=entries), indent=2) + '\n')
print('CHUCK_FALL_SOUNDS_READY', len(entries))
