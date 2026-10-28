"""SUPERSEDED 2026-10-27 by Tools/build_temple_trap_sfx.py (recorded CC0 sounds); running this overwrites them.

Original synthetic sounds for the temple trap run (Claude, 2026-10-22; docs/CHULT-TEMPLE-RUN.md).
No recordings, no dependencies (stdlib only); seeded, so a re-run reproduces the same files.
The helpers and the stone-chamber reverb follow Tools/gen_temple_trap_sfx.py (which this does not run or change).

python Tools/gen_temple_run_sfx.py -> SourceAssets/Audio/SFX/SFX_TempleRun*.wav (+ temple-run-manifest.json)

  SFX_TempleRunAxeSwoosh_00..02  a great blade through the bottom of its swing: a heavy, low whoosh.
  SFX_TempleRunAxeCreak_00..01   the axe's iron pivot at the end of a swing: a dry stick-slip groan.
  SFX_TempleRunFireHiss_00..01   a fire mouth's tell: gas hissing up, a few sparks.
  SFX_TempleRunFireRoar_00..01   the burst: a soft thump, then a roaring, fluttering jet that dies away.
  SFX_TempleRunSawSpin_00..01    a saw's whine: a bright metallic tone with its teeth's chatter.
  SFX_TempleRunSawDrop_00..01    a saw dropping on its hanger: an iron clank and the whine rising.
  SFX_TempleRunHit_00..01        a trap catching him: a dull knock and a scrape.
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
    """A stone corridor: early reflections 2-6 m away, then a dark diffuse tail."""
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


def axe_swoosh(rng):
    dur = .6; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    out, lo, band = [], 0., 0.
    for i in range(n):
        t = i / RATE
        # The blade's passing: a swell peaking mid-way, its pitch sweeping down (a moving source).
        env = math.exp(-((t - .26) / .11) ** 2)
        a = .03 + .12 * env * (1.2 - t)
        lo += a * (noise[i] - lo); band += .5 * (lo - band)
        out.append((lo - band) * 6 * env + .15 * math.sin(2 * math.pi * (95 - 40 * t) * t) * env)
    return out


def axe_creak(rng):
    dur = .5; n = int(dur * RATE); out = [0.] * n
    t, f0 = 0., rng.uniform(260, 360)
    while t < dur - .03:
        g = math.sin(math.pi * t / dur) * rng.uniform(.5, 1.)
        out = add(out, knock(rng, n, t, g, (f0, f0 * 2.03, f0 * 3.1), decay=.018, noise=.25))
        t += rng.uniform(.012, .022) * (1 + .6 * math.sin(t * 20))
    return out


def fire_hiss(rng):
    dur = .7; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    hiss = highpass(onepole(noise, .7), .25)
    out = [hiss[i] * min(1., (i / n) * 1.5) ** 1.5 * .7 for i in range(n)]
    for _ in range(6):   # sparks
        out = add(out, knock(rng, n, rng.uniform(.1, .65), rng.uniform(.15, .4), (3200, 5100, 7300), decay=.003, noise=.6))
    return out


def fire_roar(rng):
    dur = 1.4; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    roar = onepole(onepole(noise, .06), .12)
    air = highpass(onepole(noise, .45), .08)
    out = []
    for i in range(n):
        t = i / RATE
        env = min(1., t / .05) * (1. if t < 1.05 else math.exp(-(t - 1.05) * 9))
        flutter = .75 + .25 * math.sin(2 * math.pi * 13 * t + math.sin(t * 7))
        thump = math.sin(2 * math.pi * 62 * t) * math.exp(-t * 14) * 1.2
        out.append(roar[i] * 14 * env * flutter + air[i] * .35 * env + thump)
    return out


def saw_spin(rng):
    dur = 1.1; n = int(dur * RATE)
    f = rng.uniform(820, 960); teeth = rng.uniform(38, 46)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    grit = highpass(noise, .3)
    out = []
    for i in range(n):
        t = i / RATE
        env = min(1., t / .15) * min(1., (dur - t) / .25)
        chatter = .7 + .3 * math.sin(2 * math.pi * teeth * t)
        tone = math.sin(2 * math.pi * f * t) + .45 * math.sin(2 * math.pi * f * 2.01 * t + 1) + .25 * math.sin(2 * math.pi * f * 3.97 * t)
        out.append((tone * .5 * chatter + grit[i] * .12 * chatter) * env)
    return out


def saw_drop(rng):
    dur = .9; n = int(dur * RATE)
    out = knock(rng, n, 0., 1.2, (380, 910, 1760, 2900), decay=.05, noise=.6)
    f = rng.uniform(700, 820)
    for i in range(int(.05 * RATE), n):
        t = i / RATE - .05
        g = min(1., t / .2) * math.exp(-max(0., t - .45) * 6) * .35
        ff = f * (1 + .25 * min(1., t / .4))
        out[i] += g * (math.sin(2 * math.pi * ff * t) + .4 * math.sin(2 * math.pi * ff * 2.01 * t))
    return out


def hit(rng):
    dur = .35; n = int(dur * RATE)
    out = knock(rng, n, 0., 1., (140, 260, 520), decay=.03, noise=.8)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    scrape = highpass(onepole(noise, .3), .1)
    return [o + s * .3 * math.exp(-i / RATE * 9) for i, (o, s) in enumerate(zip(out, scrape))]


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
        ('SFX_TempleRunAxeSwoosh', axe_swoosh, 3, 20261101, .35, .9),
        ('SFX_TempleRunAxeCreak', axe_creak, 2, 20261102, .3, .7),
        ('SFX_TempleRunFireHiss', fire_hiss, 2, 20261103, .25, .6),
        ('SFX_TempleRunFireRoar', fire_roar, 2, 20261104, .35, 1.),
        ('SFX_TempleRunSawSpin', saw_spin, 2, 20261105, .25, .6),
        ('SFX_TempleRunSawDrop', saw_drop, 2, 20261106, .3, .8),
        ('SFX_TempleRunHit', hit, 2, 20261107, .25, .5)):
    for v in range(count):
        rng = random.Random(seed * 10 + v)
        entries.append(write(f'{name}_{v:02d}', chamber(make(rng), rng, wet=wet, tail=tail)))
(root / 'temple-run-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthetic temple trap-run sounds; Tools/gen_temple_run_sfx.py', sounds=entries), indent=2) + '\n')
print('CHUCK_TEMPLE_RUN_WAV_GENERATED', len(entries))
