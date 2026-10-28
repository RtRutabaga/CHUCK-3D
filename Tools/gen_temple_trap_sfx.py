"""SUPERSEDED 2026-10-27 by Tools/build_temple_trap_sfx.py (recorded CC0 sounds); running this overwrites them.

Original synthetic sounds for the temple interior's traps (Claude, 2026-10-20;
docs/CHULT-TEMPLE-INTERIOR.md). No recordings, no dependencies (stdlib only); seeded,
so a re-run reproduces the same files.

python Tools/gen_temple_trap_sfx.py -> SourceAssets/Audio/SFX/SFX_Temple*.wav (+ temple-trap-manifest.json)

  SFX_TempleSpikeRattle_00..01  the tell: iron spikes shivering in their grate, a dry
                                clatter of small metallic knocks, half a second.
  SFX_TempleSpikeThrust_00..02  the thrust: a hard scrape up through the grate, iron
                                ringing, a thud of the mechanism under the floor.
  SFX_TempleDartClick_00..01    a launcher cocking inside the wall: two stone-on-wood clicks.
  SFX_TempleDartShot_00..02     a dart leaving a serpent's mouth: a short airy "fft".
  SFX_TempleDartHit_00..02      a dart striking the far wall: a dry woody knock with a
                                tick of iron.
  SFX_TempleSlabCrack_00..01    a cracked slab giving: stone crackle and grit.
  SFX_TempleSlabFall_00..01     the slab breaking away: a crumble, grit and a deep,
                                distant fall into the rift.

Each gets the same dark stone-chamber reverb: close early reflections off the
corridor walls, a longer diffuse tail, the top end taken off.
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


def spike_rattle(rng):
    dur = .5; n = int(dur * RATE); out = [0.] * n
    t = 0.
    while t < dur - .05:
        g = rng.uniform(.35, 1.) * (.4 + .6 * t / dur)   # growing as the spikes stir
        out = add(out, knock(rng, n, t, g, (2600, 3900, 5600, 7100), decay=.01))
        t += rng.uniform(.012, .04)
    return out


def spike_thrust(rng):
    dur = .7; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    scrape = highpass(onepole(noise, .5), .08)
    low = onepole(onepole(noise, .01), .02)
    iron = [(f * rng.uniform(.97, 1.03), a, d, rng.uniform(0, 6.3)) for f, a, d in ((540, .3, .16), (1210, .25, .12), (2330, .2, .08), (3720, .12, .05))]
    out = []
    for i in range(n):
        t = i / RATE
        swell = min(1., t / .015) * math.exp(-max(0., t - .03) * 30)
        out.append(scrape[i] * 1.6 * swell + low[i] * 9 * math.exp(-t * 18) + .7 * min(1., t / .02) * ring(t, iron))
    return out


def dart_click(rng):
    dur = .25; n = int(dur * RATE)
    return add(knock(rng, n, 0., 1., (900, 1650, 2400), decay=.008, noise=.6),
               knock(rng, n, rng.uniform(.07, .1), .7, (1100, 1900, 3100), decay=.007, noise=.6))


def dart_shot(rng):
    dur = .3; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    band = highpass(onepole(noise, .35), .1)
    out = []
    for i in range(n):
        t = i / RATE
        env = min(1., t / .006) * math.exp(-t * 22)
        out.append(band[i] * env + .25 * math.sin(2 * math.pi * (300 - 400 * t) * t) * math.exp(-t * 60))
    return out


def dart_hit(rng):
    dur = .3; n = int(dur * RATE)
    return add(knock(rng, n, 0., 1., (420, 780, 1350), decay=.02, noise=.9), knock(rng, n, .002, .4, (3400, 5200), decay=.006, noise=.1))


def slab_crack(rng):
    dur = .6; n = int(dur * RATE); out = [0.] * n
    t = 0.
    while t < dur - .03:
        out = add(out, knock(rng, n, t, rng.uniform(.3, 1.), (1500, 2300, 3700), decay=.004, noise=1.))
        t += rng.uniform(.004, .03) if rng.random() < .7 else rng.uniform(.05, .1)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    grit = highpass(noise, .2)
    return [o + g * .15 * (1 - i / n) for i, (o, g) in enumerate(zip(out, grit))]


def slab_fall(rng):
    dur = 1.8; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    rumble = onepole(onepole(noise, .004), .008)
    grit = highpass(onepole(noise, .4), .06)
    out = []
    for i in range(n):
        t = i / RATE
        out.append(rumble[i] * 60 * math.exp(-t * 2.2) * min(1., t / .04) + grit[i] * .5 * math.exp(-t * 5))
    for _ in range(14):   # pieces breaking off
        out = add(out, knock(rng, n, rng.uniform(0, .6), rng.uniform(.2, .6), (900, 1500, 2600), decay=.008, noise=.8))
    # Far down in the rift, the slab lands: a muffled boom.
    s = int(rng.uniform(1.1, 1.3) * RATE)
    for i in range(s, n):
        t = (i - s) / RATE
        out[i] += .5 * math.sin(2 * math.pi * 55 * t) * math.exp(-t * 7) + rumble[i] * 20 * math.exp(-t * 9)
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
        ('SFX_TempleSpikeRattle', spike_rattle, 2, 20261020, .3, .8),
        ('SFX_TempleSpikeThrust', spike_thrust, 3, 20261021, .35, 1.),
        ('SFX_TempleDartClick', dart_click, 2, 20261022, .3, .6),
        ('SFX_TempleDartShot', dart_shot, 3, 20261023, .25, .6),
        ('SFX_TempleDartHit', dart_hit, 3, 20261024, .3, .7),
        ('SFX_TempleSlabCrack', slab_crack, 2, 20261025, .3, .8),
        ('SFX_TempleSlabFall', slab_fall, 2, 20261026, .4, 1.4)):
    for v in range(count):
        rng = random.Random(seed * 10 + v)
        entries.append(write(f'{name}_{v:02d}', chamber(make(rng), rng, wet=wet, tail=tail)))
(root / 'temple-trap-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthetic temple trap sounds; Tools/gen_temple_trap_sfx.py', sounds=entries), indent=2) + '\n')
print('CHUCK_TEMPLE_TRAP_WAV_GENERATED', len(entries))
