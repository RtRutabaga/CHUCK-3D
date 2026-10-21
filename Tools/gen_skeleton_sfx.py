"""Original synthetic sounds for the temple skeletons (Claude, 2026-10-21;
docs/CHULT-TEMPLE-SKELETONS.md). No recordings, no dependencies (stdlib only);
seeded, so a re-run reproduces the same files. The helpers follow
Tools/gen_temple_trap_sfx.py, so they share its stone-chamber acoustic.

python Tools/gen_skeleton_sfx.py -> SourceAssets/Audio/SFX/SFX_Skeleton*.wav (+ skeleton-sfx-manifest.json)

  SFX_SkeletonStep_00..03      a bare bony foot on stone: a dry tap, then the small
                               bones of the foot and hand settling with a faint rattle.
  SFX_SkeletonRattle_00..01    the alert and the tell: teeth chattering, a quick run of
                               clacks with the ribs rattling under it.
  SFX_SkeletonSwing_00..02     a limb swung hard: a short dry whoosh.
  SFX_SkeletonHit_00..02       a claw raked across bone: a scrape and a hard, ringing tick.
  SFX_SkeletonCollapse_00..01  falling to pieces: a cascade of bones on stone, the skull
                               and pelvis heavier, thinning out as they settle.
  SFX_SkeletonRise_00..01      getting up from the floor: bones grinding and scraping on
                               stone, joints clicking into place.
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


# Bone is dry and hollow: its partials sit lower and die faster than iron's.
BONE = (700, 1300, 2100, 3300)
SMALL = (1900, 3100, 4600)
TEETH = (2400, 3600, 5200)


def step(rng):
    dur = .3; n = int(dur * RATE)
    out = knock(rng, n, 0., 1., (380, 820, 1500), decay=.014, noise=.7)            # heel bone on the floor
    out = add(out, knock(rng, n, rng.uniform(.012, .025), .55, BONE, decay=.009))   # the forefoot
    for _ in range(rng.randint(3, 6)):                                                # small bones settling
        out = add(out, knock(rng, n, rng.uniform(.03, .16), rng.uniform(.08, .22), SMALL, decay=.005, noise=.3))
    return out


def rattle(rng):
    dur = .65; n = int(dur * RATE); out = [0.] * n
    t, rate = 0., rng.uniform(13, 17)
    while t < dur - .06:   # the teeth: quick clacks, slightly irregular, loudest in the middle
        g = .5 + .5 * math.sin(math.pi * t / dur)
        out = add(out, knock(rng, n, t, g * rng.uniform(.7, 1.), TEETH, decay=.004, noise=.5))
        t += 1. / rate * rng.uniform(.8, 1.2)
    for _ in range(18):   # the ribs and loose small bones under it
        out = add(out, knock(rng, n, rng.uniform(0, dur - .05), rng.uniform(.06, .18), BONE, decay=.006, noise=.3))
    return out


def swing(rng):
    dur = .32; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    band = highpass(onepole(noise, .18), .05)
    peak = rng.uniform(.11, .15)
    out = []
    for i in range(n):
        t = i / RATE
        env = math.exp(-((t - peak) / .055) ** 2)
        out.append(band[i] * env)
    return add(out, knock(rng, n, peak + .02, .12, SMALL, decay=.004, noise=.2))   # a knuckle tick at the end


def hit(rng):
    dur = .35; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    scrape = highpass(onepole(noise, .55), .12)
    out = []
    for i in range(n):
        t = i / RATE
        out.append(scrape[i] * .9 * min(1., t / .004) * math.exp(-t * 26))
    out = add(out, knock(rng, n, .01, 1., (900, 1800, 3400, 5100), decay=.016, noise=.5))
    return add(out, knock(rng, n, rng.uniform(.06, .1), .25, SMALL, decay=.006))


def collapse(rng):
    dur = 1.6; n = int(dur * RATE); out = [0.] * n
    t = 0.
    while t < dur - .1:   # the cascade, thinning out
        out = add(out, knock(rng, n, t, rng.uniform(.25, .8) * math.exp(-t * 1.1), BONE if rng.random() < .6 else SMALL, decay=.008, noise=.5))
        t += rng.uniform(.006, .03) + t * .05
    for when, gain, freqs in ((rng.uniform(.18, .26), 1., (240, 520, 980)), (rng.uniform(.35, .5), .85, (330, 700, 1300)),   # pelvis, ribcage
                              (rng.uniform(.55, .7), .9, (420, 950, 1800))):                                                  # the skull, bouncing once
        out = add(out, knock(rng, n, when, gain, freqs, decay=.03, noise=.6))
        out = add(out, knock(rng, n, when + rng.uniform(.12, .16), gain * .4, freqs, decay=.02, noise=.4))
    return out


def rise(rng):
    dur = 1.7; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    grind = highpass(onepole(noise, .25), .05)
    out = []
    for i in range(n):
        t = i / RATE
        env = sum(math.exp(-((t - c) / .09) ** 2) for c in (.25, .7, 1.15))   # three efforts: onto an elbow, a knee, up
        out.append(grind[i] * .45 * env)
    for _ in range(26):   # joints clicking, bones knocking the floor
        out = add(out, knock(rng, n, rng.uniform(0, dur - .05), rng.uniform(.15, .5), BONE if rng.random() < .5 else SMALL, decay=.007, noise=.4))
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
        ('SFX_SkeletonStep', step, 4, 20261031, .3, .7),
        ('SFX_SkeletonRattle', rattle, 2, 20261032, .3, .8),
        ('SFX_SkeletonSwing', swing, 3, 20261033, .2, .5),
        ('SFX_SkeletonHit', hit, 3, 20261034, .3, .7),
        ('SFX_SkeletonCollapse', collapse, 2, 20261035, .35, 1.2),
        ('SFX_SkeletonRise', rise, 2, 20261036, .3, 1.)):
    for v in range(count):
        rng = random.Random(seed * 10 + v)
        entries.append(write(f'{name}_{v:02d}', chamber(make(rng), rng, wet=wet, tail=tail)))
(root / 'skeleton-sfx-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthetic skeleton sounds; Tools/gen_skeleton_sfx.py', sounds=entries), indent=2) + '\n')
print('CHUCK_SKELETON_WAV_GENERATED', len(entries))
