"""Original synthetic sounds for the beholder's fight (Claude, 2026-10-25; docs/CHULT-TEMPLE-BEHOLDER.md#the-fight).
No recordings, no dependencies (stdlib only); seeded, so a re-run reproduces the same files. The helpers and the
stone-hall acoustic follow Tools/gen_hero_sfx.py.

python Tools/gen_beholder_sfx.py -> SourceAssets/Audio/SFX/SFX_Beholder*.wav (+ beholder-sfx-manifest.json)

  SFX_BeholderRayCharge_00..01      a stalk's eye gathering a ray: a rising, shimmering whine.
  SFX_BeholderRayDisintegrate_00    the disintegration ray: a harsh electric tearing with a bright crackle.
  SFX_BeholderRayEnervate_00        the enervation ray: a low, wavering, hissing groan.
  SFX_BeholderRayTelekinetic_00     the telekinetic ray: a deep warbling hum and a rush of air.
  SFX_BeholderRayParalyze_00        the paralysing ray: a crystalline ringing crackle.
  SFX_BeholderRayFear_00            the fear ray: a dissonant, falling shriek.
  SFX_BeholderRayImpact_00..02      a ray striking stone: a sizzling burst, a thump, crackles.
  SFX_BeholderDisintegrate_00       a skeleton turned to dust: a crackling sizzle into a dry crumbling hiss.
  SFX_BeholderConeCharge_00         the central eye opening its cone: a swelling drone, air drawn in, a climbing whine.
  SFX_BeholderConeBlast_00          the cone loosed: a heavy boom, a rushing blast of air, the hall's rumble.
  SFX_BeholderRoar_00..01           the bite's lunge: a huge guttural roar.
  SFX_BeholderSnap_00..01           the jaws snapping shut: a hard clack and a wet crunch.
  SFX_BeholderGrowl_00..02          a low rumbling growl (its snarls).
  SFX_BeholderHurt_00..01           struck by an arrow or a bolt: a pained bellow.
"""
import hashlib, json, math, random, struct, wave
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
root.mkdir(parents=True, exist_ok=True)
RATE = 48000
PEAK = 23197   # -3 dBFS
TAU = 2 * math.pi


def onepole(x, a):
    y, out = 0., []
    for v in x:
        y += a * (v - y); out.append(y)
    return out


def highpass(x, a):
    low = onepole(x, a)
    return [v - l for v, l in zip(x, low)]


def chamber(x, rng, wet=.3, tail=.9):
    """The sanctum's stone: a few early reflections and a dense decaying tail (sparse, so it stays quick)."""
    taps = [(rng.uniform(.006, .022), rng.uniform(.35, .55)) for _ in range(5)]
    t = .025
    while t < tail:
        taps.append((t, .3 * math.exp(-t / (tail / 3.5)) * rng.uniform(.5, 1.)))
        t += rng.uniform(.004, .011)
    n = len(x) + int(tail * RATE)
    w = [0.] * n
    nz = [(i, v) for i, v in enumerate(x) if v]
    for delay, g in taps:
        d = int(delay * RATE); g *= rng.choice((-1., 1.))
        for i, v in nz: w[i + d] += g * v
    w = onepole(w, .2)
    return [(x[i] if i < len(x) else 0.) + wet * w[i] for i in range(n)]


def add(a, b, at=0.):
    s = int(at * RATE)
    if s + len(b) > len(a): a = a + [0.] * (s + len(b) - len(a))
    for i, v in enumerate(b): a[s + i] += v
    return a


def svf(x, freqs, q=4., mode='band'):
    """A state-variable filter whose centre follows freqs (Hz; a number or a list per sample)."""
    lo = bp = 0.; out = []
    fixed = not isinstance(freqs, list)
    for i, v in enumerate(x):
        f = freqs if fixed else freqs[i]
        g = 2 * math.sin(math.pi * min(f, RATE / 6.5) / RATE)
        hi = v - lo - bp / q
        bp += g * hi; lo += g * bp
        out.append(bp if mode == 'band' else lo if mode == 'low' else hi)
    return out


def noise(rng, n):
    return [rng.uniform(-1, 1) for _ in range(n)]


def env(n, attack, release, hold=0.):
    out = []
    for i in range(n):
        t = i / RATE
        if t < attack: out.append((t / attack) ** 2)
        elif t < attack + hold: out.append(1.)
        else: out.append(math.exp(-(t - attack - hold) / release))
    return out


def ring(n, start, modes, rng):
    """Struck partials: (Hz, gain, decay s)."""
    out = [0.] * n
    s = int(start * RATE)
    ph = [rng.uniform(0, TAU) for _ in modes]
    for i in range(s, n):
        t = (i - s) / RATE
        v = sum(a * math.sin(TAU * f * t + p) * math.exp(-t / d) for (f, a, d), p in zip(modes, ph))
        if t > max(d for _, _, d in modes) * 7: break
        out[i] = v
    return out


def crackle(rng, n, rate, gain, start=0., stop=None, bright=.5):
    """Random electric snaps: short bursts at about rate per second."""
    out = [0.] * n
    t = start; stop = stop or n / RATE
    while t < stop:
        t += rng.expovariate(rate)
        s = int(t * RATE)
        g = gain * rng.uniform(.3, 1.)
        for k in range(int(.004 * RATE)):
            if s + k >= n: break
            out[s + k] += g * rng.uniform(-1, 1) * math.exp(-k / (RATE * .0008))
    return highpass(out, bright)


def voice(rng, dur, f0, formants, growl=28., rough=.5, breath=.25, shape=None):
    """A creature's throat: a jittered glottal pulse train through formant resonators, growled and driven."""
    n = int(dur * RATE)
    ph = 0.; src = []
    jit = 0.
    for i in range(n):
        t = i / RATE
        u = t / dur
        f = f0(u) if callable(f0) else f0
        jit += (rng.uniform(-1, 1) * .08 - jit) * .002
        ph += f * (1 + jit) / RATE
        ph -= math.floor(ph)
        p = math.sin(math.pi * ph / .35) if ph < .35 else 0.
        am = 1 - rough * (.5 + .5 * math.sin(TAU * growl * t + 2 * math.sin(TAU * 3.1 * t)))
        src.append(p * am + breath * rng.uniform(-1, 1) * am)
    out = [0.] * n
    for fc, gain, q in formants:
        b = svf(src, fc, q)
        for i in range(n): out[i] += gain * b[i]
    e = shape(n) if shape else env(n, .12, dur * .35, dur * .35)
    return [math.tanh(2.4 * v * e[i]) for i, v in enumerate(out)]


def ray_charge(rng):
    dur = .5; n = int(dur * RATE)
    out = []
    ph = ph2 = 0.
    for i in range(n):
        t = i / RATE; u = t / dur
        f = 280 + 1300 * u ** 1.6
        ph += f / RATE; ph2 += f * 1.503 / RATE
        trem = .6 + .4 * math.sin(TAU * (14 + 30 * u) * t)
        out.append((math.sin(TAU * ph) + .4 * math.sin(TAU * ph2)) * trem * u ** 1.2 * .5)
    hiss = svf(noise(rng, n), [1500 + 5000 * (i / n) for i in range(n)], 3.)
    return add(out, [hiss[i] * .5 * (i / n) ** 2 for i in range(n)])


def ray_disintegrate(rng):
    dur = .75; n = int(dur * RATE)
    e = env(n, .01, .3, .25)
    out = []; ph = 0.
    for i in range(n):
        t = i / RATE
        f = 85 + 30 * math.sin(TAU * 7 * t) + rng.uniform(-6, 6)
        ph += f / RATE
        saw = 2 * (ph - math.floor(ph)) - 1
        fm = math.sin(TAU * 1830 * t + 3 * math.sin(TAU * 211 * t))
        out.append((math.tanh(3 * saw) * .55 + .25 * fm) * e[i])
    out = add(out, [v * e[i] for i, v in enumerate(svf(noise(rng, n), 3800, 1.5))])
    return add(out, crackle(rng, n, 140, 1.4, 0., .65, .3))


def ray_enervate(rng):
    dur = .85; n = int(dur * RATE)
    e = env(n, .05, .35, .3)
    groan = voice(rng, dur, lambda u: 62 - 16 * u, [(260, 1., 6.), (520, .6, 7.)], growl=9., rough=.6, breath=.5, shape=lambda m: env(m, .05, .35, .3))
    hiss = svf(noise(rng, n), [900 + 600 * math.sin(TAU * 2.3 * i / RATE) for i in range(n)], 2.)
    return [groan[i] * .7 + hiss[i] * .6 * e[i] for i in range(n)]


def ray_telekinetic(rng):
    dur = .85; n = int(dur * RATE)
    e = env(n, .04, .3, .35)
    out = []; ph = 0.
    for i in range(n):
        t = i / RATE
        f = 150 + 60 * math.sin(TAU * 8.5 * t)
        ph += f / RATE
        out.append((math.sin(TAU * ph) + .5 * math.sin(TAU * 2 * ph + .3) + .25 * math.sin(TAU * 3 * ph)) * .45 * e[i])
    air = svf(noise(rng, n), [500 + 1800 * math.exp(-((i / RATE - .2) / .15) ** 2) for i in range(n)], 1.8)
    return add(out, [air[i] * .9 * e[i] for i in range(n)])


def ray_paralyze(rng):
    dur = .75; n = int(dur * RATE)
    out = ring(n, 0., [(2093, .5, .35), (3310, .45, .28), (4970, .35, .2), (6440, .25, .15), (1567, .3, .45)], rng)
    e = env(n, .005, .35, .2)
    out = [v * (.7 + .3 * math.sin(TAU * 23 * i / RATE)) for i, v in enumerate(out)]
    out = add(out, [v * e[i] for i, v in enumerate(svf(noise(rng, n), 6000, 2.))])
    return add(out, crackle(rng, n, 90, 1., 0., .6, .5))


def ray_fear(rng):
    dur = .85; n = int(dur * RATE)
    e = env(n, .03, .3, .3)
    out = []; a = b = c = 0.
    for i in range(n):
        t = i / RATE; u = t / dur
        f = 880 - 420 * u + 25 * math.sin(TAU * 6 * t)
        a += f / RATE; b += f * 1.059 / RATE; c += f * .5 / RATE
        saw = lambda p: 2 * (p - math.floor(p)) - 1
        out.append((saw(a) + saw(b) * .9 + .5 * math.sin(TAU * c)) * .3 * e[i])
    out = svf(out, 2400, .9)
    return add(out, [v * e[i] * .3 for i, v in enumerate(svf(noise(rng, n), 4200, 2.5))])


def ray_impact(rng):
    dur = .7; n = int(dur * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        out.append(math.sin(TAU * (70 * math.exp(-t * 6)) * t * 1.0) * math.exp(-t * 16) * 1.1)   # the thump
    burst = noise(rng, n)
    lp = svf(burst, [9000 * math.exp(-i / (RATE * .12)) + 500 for i in range(n)], 1.2)
    out = add(out, [lp[i] * math.exp(-(i / RATE) * 7) * 1.2 for i in range(n)])
    return add(out, crackle(rng, n, 70, 1.1, 0., .55, .35))


def disintegrate(rng):
    dur = 1.5; n = int(dur * RATE)
    out = crackle(rng, n, 220, 1.2, 0., .6, .3)
    sizzle = svf(noise(rng, n), [4500 - 2500 * (i / n) for i in range(n)], 1.4)
    out = add(out, [sizzle[i] * .9 * math.exp(-max(0., i / RATE - .25) * 2.5) for i in range(n)])
    # Bone dust settling: tiny dry ticks thinning out.
    for _ in range(70):
        t = rng.uniform(.3, 1.4)
        out = add(out, [rng.uniform(-1, 1) * .35 * math.exp(-k / (RATE * .0006)) for k in range(int(.003 * RATE))], t)
    return out


def cone_charge(rng):
    dur = 1.7; n = int(dur * RATE)
    out = []; a = b = 0.
    for i in range(n):
        t = i / RATE; u = t / dur
        a += (38 + 34 * u * u) / RATE; b += (600 + 1700 * u ** 2.2) / RATE
        drone = math.tanh(2 * math.sin(TAU * a) + .6 * math.sin(TAU * 2.01 * a))
        out.append((drone * .7 + .22 * math.sin(TAU * b) * (.6 + .4 * math.sin(TAU * 31 * t))) * u ** 1.4)
    suck = svf(noise(rng, n), [300 + 2200 * (i / n) ** 1.5 for i in range(n)], 1.6)
    return add(out, [suck[i] * .8 * (i / n) ** 2 for i in range(n)])


def cone_blast(rng):
    dur = 2.6; n = int(dur * RATE)
    out = []
    ph = 0.
    for i in range(n):
        t = i / RATE
        ph += (32 + 60 * math.exp(-t * 9)) / RATE
        out.append(math.tanh(2.2 * math.sin(TAU * ph)) * math.exp(-t * 2.2) * 1.2)
    crack = svf(noise(rng, n), [6000 * math.exp(-i / (RATE * .05)) + 300 for i in range(n)], 1.)
    out = add(out, [crack[i] * math.exp(-(i / RATE) * 10) * 1.5 for i in range(n)])
    gust = svf(noise(rng, n), [1400 - 900 * min(1., i / (RATE * 1.4)) for i in range(n)], 1.3)
    out = add(out, [gust[i] * 1.1 * (1 - math.exp(-(i / RATE) * 30)) * math.exp(-(i / RATE) * 1.3) for i in range(n)])
    rumble = svf(noise(rng, n), 55, 3., 'low')
    return add(out, [rumble[i] * 6. * math.exp(-(i / RATE) * 1.1) for i in range(n)])


def roar(rng):
    dur = 1.35
    v = voice(rng, dur, lambda u: 78 + 22 * math.sin(math.pi * min(1., u * 1.4)) - 18 * u, [(330, 1., 5.), (820, .8, 6.), (2300, .35, 8.), (140, .5, 3.)],
              growl=31., rough=.55, breath=.35, shape=lambda m: env(m, .14, .38, .55))
    n = len(v)
    wet = svf(noise(rng, n), 1600, 1.)
    return [v[i] + wet[i] * .2 * env(n, .1, .4, .5)[i] for i in range(n)]


def snap(rng):
    dur = .55; n = int(dur * RATE)
    out = ring(n, 0., [(380, 1., .03), (910, .7, .02), (1750, .5, .012), (160, .8, .05)], rng)
    out = add(out, [rng.uniform(-1, 1) * math.exp(-k / (RATE * .002)) for k in range(int(.02 * RATE))])
    wet = svf(noise(rng, n), 900, 1.3)
    out = add(out, [wet[i] * .8 * math.exp(-max(0., i / RATE - .02) * 14) * (1 if i / RATE > .015 else 0) for i in range(n)])
    for _ in range(9):   # teeth on teeth, gristle
        out = add(out, ring(int(.05 * RATE), 0., [(rng.uniform(2200, 4200), .35, .006)], rng), rng.uniform(.005, .12))
    return out


def growl(rng):
    dur = 1.7
    return voice(rng, dur, lambda u: 50 + 8 * math.sin(TAU * .6 * u) + rng.uniform(-1, 1), [(240, 1., 5.), (600, .55, 6.), (1500, .2, 7.), (110, .6, 3.)],
                 growl=23., rough=.7, breath=.4, shape=lambda m: env(m, .3, .5, .7))


def hurt(rng):
    dur = .95
    return voice(rng, dur, lambda u: 125 - 45 * u, [(420, 1., 5.), (1050, .7, 6.), (2600, .3, 8.)],
                 growl=37., rough=.45, breath=.3, shape=lambda m: env(m, .04, .3, .2))


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
        ('SFX_BeholderRayCharge', ray_charge, 2, 20261201, .25, .7),
        ('SFX_BeholderRayDisintegrate', ray_disintegrate, 1, 20261202, .3, 1.),
        ('SFX_BeholderRayEnervate', ray_enervate, 1, 20261203, .3, 1.),
        ('SFX_BeholderRayTelekinetic', ray_telekinetic, 1, 20261204, .3, 1.),
        ('SFX_BeholderRayParalyze', ray_paralyze, 1, 20261205, .3, 1.),
        ('SFX_BeholderRayFear', ray_fear, 1, 20261206, .3, 1.),
        ('SFX_BeholderRayImpact', ray_impact, 3, 20261207, .35, 1.1),
        ('SFX_BeholderDisintegrate', disintegrate, 1, 20261208, .3, 1.),
        ('SFX_BeholderConeCharge', cone_charge, 1, 20261209, .3, 1.2),
        ('SFX_BeholderConeBlast', cone_blast, 1, 20261210, .45, 2.2),
        ('SFX_BeholderRoar', roar, 2, 20261211, .4, 1.6),
        ('SFX_BeholderSnap', snap, 2, 20261212, .3, .9),
        ('SFX_BeholderGrowl', growl, 3, 20261213, .35, 1.2),
        ('SFX_BeholderHurt', hurt, 2, 20261214, .35, 1.2)):
    for v in range(count):
        rng = random.Random(seed * 10 + v)
        entries.append(write(f'{name}_{v:02d}', chamber(make(rng), rng, wet=wet, tail=tail)))
        print(entries[-1]['name'], entries[-1]['duration'], flush=True)
(root / 'beholder-sfx-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthetic beholder sounds; Tools/gen_beholder_sfx.py', sounds=entries), indent=2) + '\n')
print('CHUCK_BEHOLDER_WAV_GENERATED', len(entries))
