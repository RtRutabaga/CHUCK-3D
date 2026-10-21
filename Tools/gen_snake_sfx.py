"""Original synthetic sounds for the temple snakes (Claude, 2026-10-21;
docs/CHULT-TEMPLE-SNAKES.md). No recordings, no dependencies (stdlib only);
seeded, so a re-run reproduces the same files. The helpers follow
Tools/gen_skeleton_sfx.py (and so the traps' stone-chamber acoustic).

python Tools/gen_snake_sfx.py -> SourceAssets/Audio/SFX/SFX_Snake*.wav (+ snake-sfx-manifest.json)

  SFX_SnakeHiss_00..02     the tell: a long breathy hiss through the glottis, swelling and
                           trembling with the breath.
  SFX_SnakeStrike_00..02   the strike: a short whoosh, the body slapping the floor, a soft snap of the jaws.
  SFX_SnakeSlither_00..03  dry scales sliding over stone: a hushed, grainy rustle.
  SFX_SnakeDeath_00..01    the death throes: thrashing on stone, slaps and rustles, slowing.
"""
import hashlib, json, math, random, struct, wave
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
root.mkdir(parents=True, exist_ok=True)
RATE = 48000
PEAK = 23197   # -3 dBFS


def onepole(x, a):
    y, out = 0., []
    for v in x:
        y += a * (v - y); out.append(y)
    return out


def highpass(x, a):
    low = onepole(x, a)
    return [v - l for v, l in zip(x, low)]


def chamber(x, rng, wet=.3, tail=.9):
    """A stone hall: early reflections 2-6 m away, then a dark diffuse tail."""
    taps = [(rng.uniform(.006, .022), rng.uniform(.35, .55)) for _ in range(5)]
    t = .025
    while t < tail:
        taps.append((t, .3 * math.exp(-t / (tail / 3.5)) * rng.uniform(.5, 1.)))
        t += rng.uniform(.003, .009)
    n = len(x) + int(tail * RATE)
    w = [0.] * n
    for delay, g in taps:
        d = int(delay * RATE); g *= rng.choice((-1., 1.))
        for i, v in enumerate(x):
            if v: w[i + d] += g * v
    w = onepole(w, .22)
    return [(x[i] if i < len(x) else 0.) + wet * w[i] for i in range(n)]


def ring(t, modes):
    return sum(a * math.sin(2 * math.pi * f * t + p) * math.exp(-t / d) for f, a, d, p in modes)


def knock(rng, n, start, gain, freqs, decay=.012, noise=.4):
    """One small hard contact at start: a click and a few short partials."""
    out = [0.] * n
    modes = [(f * rng.uniform(.95, 1.05), rng.uniform(.5, 1.), decay * rng.uniform(.7, 1.3), rng.uniform(0, 6.3)) for f in freqs]
    s = int(start * RATE)
    for i in range(s, n):
        t = (i - s) / RATE
        if t > decay * 8: break
        out[i] = gain * (ring(t, modes) + noise * rng.uniform(-1, 1) * math.exp(-t * 1400))
    return out


def add(a, b):
    return [x + y for x, y in zip(a, b)]



def hiss(rng):
    dur = rng.uniform(.8, 1.1); n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    band = highpass(onepole(noise, .55), .25)          # sibilant: most energy 3-9 kHz
    trem = rng.uniform(5, 8)
    out = []
    for i in range(n):
        t = i / RATE
        env = min(1., t / .06) * (1 - max(0., (t - dur + .25) / .25)) * (.85 + .15 * math.sin(2 * math.pi * trem * t))
        out.append(band[i] * env)
    return out


def strike(rng):
    dur = .45; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    air = highpass(onepole(noise, .2), .04)
    low = onepole(onepole(noise, .02), .03)
    out = []
    for i in range(n):
        t = i / RATE
        whoosh = math.exp(-((t - .06) / .035) ** 2)
        slap = math.exp(-max(0., t - .1) * 30) * (t > .1)
        out.append(air[i] * .9 * whoosh + low[i] * 7 * slap)
    out = add(out, knock(rng, n, .12, .35, (900, 1700, 2600), decay=.006, noise=.6))   # the jaws
    return out


def slither(rng):
    dur = rng.uniform(.5, .8); n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    grain = highpass(onepole(noise, .35), .08)
    out = []
    for i in range(n):
        t = i / RATE
        env = math.sin(math.pi * t / dur) ** 1.5
        out.append(grain[i] * env * (.6 + .4 * rng.random()))
    for _ in range(40):   # scale edges ticking
        out = add(out, knock(rng, n, rng.uniform(0, dur - .02), rng.uniform(.03, .1), (3500, 5200, 7000), decay=.002, noise=.8))
    return out


def death(rng):
    dur = 1.6; n = int(dur * RATE); out = [0.] * n
    t = 0.
    while t < dur - .2:
        s = slither(rng)[:int(.3 * RATE)]
        g = math.exp(-t * 1.2) * rng.uniform(.6, 1.)
        k = int(t * RATE)
        for i, v in enumerate(s):
            if k + i < n: out[k + i] += v * g
        noise = [rng.uniform(-1, 1) for _ in range(int(.08 * RATE))]
        low = onepole(onepole(noise, .03), .04)
        for i, v in enumerate(low):
            if k + i < n: out[k + i] += v * 6 * g * math.exp(-i / RATE * 40)
        t += rng.uniform(.12, .25) + t * .15
    return out


def write(name, samples):
    peak = max(abs(x) for x in samples) or 1.
    fade = int(.04 * RATE)
    pcm = [int(x / peak * PEAK * min(1., (len(samples) - i) / fade)) for i, x in enumerate(samples)]
    path = root / (name + '.wav')
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        out.writeframes(struct.pack('<' + 'h' * len(pcm), *pcm))
    return dict(name=name, file=path.name, duration=round(len(pcm) / RATE, 3), sha256=hashlib.sha256(path.read_bytes()).hexdigest(), peak_dbfs=-3.0)


entries = []
for name, make, count, seed, wet, tail in (
        ('SFX_SnakeHiss', hiss, 3, 20261041, .25, .7),
        ('SFX_SnakeStrike', strike, 3, 20261042, .3, .7),
        ('SFX_SnakeSlither', slither, 4, 20261043, .2, .5),
        ('SFX_SnakeDeath', death, 2, 20261044, .3, .9)):
    for v in range(count):
        rng = random.Random(seed * 10 + v)
        entries.append(write(f'{name}_{v:02d}', chamber(make(rng), rng, wet=wet, tail=tail)))
(root / 'snake-sfx-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthetic snake sounds; Tools/gen_snake_sfx.py', sounds=entries), indent=2) + '\n')
print('CHUCK_SNAKE_WAV_GENERATED', len(entries))
