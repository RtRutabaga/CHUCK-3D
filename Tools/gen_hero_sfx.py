"""Original synthetic sounds for the temple heroes' fight (Claude, 2026-10-22; docs/CHULT-TEMPLE-HEROES.md).
No recordings, no dependencies (stdlib only); seeded, so a re-run reproduces the same files. The helpers
follow Tools/gen_skeleton_sfx.py (its stone-hall acoustic).

python Tools/gen_hero_sfx.py -> SourceAssets/Audio/SFX/SFX_Hero*.wav (+ hero-sfx-manifest.json)

  SFX_HeroSwordWhoosh_00..03   an arming sword cut through the air: a band of noise sweeping up and
                               down in pitch as the blade passes (the Doppler of its tip), with the
                               thin ring of the steel; its loudest moment at 0.18 s.
  SFX_HeroSwordHitBone_00..03  the blade going into bone: a hard crack, the dull knock of the mass behind
                               it, the blade's own short ring, splinters scattering.
  SFX_HeroShieldBlock_00..02   a blow taken on the shield: a deep wooden thud, the leather slap, the
                               steel rim's brief ring.
  SFX_HeroArmourHit_00..02     a bony fist on plate: a clank (steel plate's inharmonic partials) over a thud.
  SFX_HeroArmourStep_00..03    an armoured footfall: the boot's thud, mail rings shivering after it.
  SFX_HeroShieldBash_00..01    the shield driven into a skeleton: a heavy thump and bones knocked loose.
  SFX_HeroBowDraw_00           the longbow drawn: the wood's creak and the string's stretch.
  SFX_HeroBowRelease_00..02    the loose: the string's twang, the limbs' thump.
  SFX_HeroArrowFlight_00..01   an arrow passing: a short hiss.
  SFX_HeroArrowHit_00..02      an arrow into bone: a crack and the shaft's quivering buzz.
  SFX_HeroBoltCast_00..01      the wizard's bolt: a rising whoosh crackling with sparks.
  SFX_HeroBoltImpact_00..01    the bolt bursting: a low thump, a crackle, bones scattered.
  SFX_HeroChannel_00           the binding, a 4 s loop: a low drone in slow beats, a shimmer over it.
  SFX_HeroSkullBounce_00..02   a struck-off skull hitting the floor and rolling.
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
    if len(b) > len(a): a = a + [0.] * (len(b) - len(a))
    return [x + (b[i] if i < len(b) else 0.) for i, x in enumerate(a)]


def svf_band(x, freqs, q=4.):
    """A state-variable band-pass whose centre follows freqs[i] (Hz) sample by sample."""
    lo = bp = 0.; out = []
    for v, f in zip(x, freqs):
        g = 2 * math.sin(math.pi * min(f, RATE / 6.5) / RATE)
        hi = v - lo - bp / q
        bp += g * hi; lo += g * bp
        out.append(bp)
    return out


BONE = (700, 1300, 2100, 3300)
SMALL = (1900, 3100, 4600)
STEEL = (930, 1480, 2310, 3720, 5100)        # plate: inharmonic, long
BLADE = (2840, 4410, 6230)                    # a thin sword: high, bright


def whoosh(rng):
    dur = .5; n = int(dur * RATE); peak = .18 + rng.uniform(-.01, .01)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    freqs = [700 + 2600 * math.exp(-(((i / RATE) - peak) / .06) ** 2) for i in range(n)]
    band = svf_band(noise, freqs, q=2.2)
    out = []
    for i in range(n):
        t = i / RATE
        env = math.exp(-((t - peak) / (.05 if t < peak else .08)) ** 2)
        tone = .08 * math.sin(2 * math.pi * (3100 + 900 * (t - peak) / .1) * t) * env   # the steel singing
        out.append(band[i] * env * 1.4 + tone)
    return out


def sword_bone(rng):
    dur = .6; n = int(dur * RATE)
    out = knock(rng, n, 0., 1., (520, 1100, 1900, 2900), decay=.02, noise=.9)            # the crack
    out = add(out, knock(rng, n, .003, .9, (140, 260, 410), decay=.035, noise=.3))     # the mass behind it
    out = add(out, knock(rng, n, .002, .35, BLADE, decay=.08, noise=.05))             # the blade's ring
    for _ in range(rng.randint(5, 9)):
        out = add(out, knock(rng, n, rng.uniform(.04, .35), rng.uniform(.1, .3), SMALL, decay=.005, noise=.4))
    return out


def shield_block(rng):
    dur = .7; n = int(dur * RATE)
    out = knock(rng, n, 0., 1., (95, 170, 310, 480), decay=.06, noise=.5)               # boards
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    slap = highpass(onepole(noise, .4), .1)
    out = add(out, [slap[i] * .5 * math.exp(-(i / RATE) * 90) for i in range(n)])     # leather face
    out = add(out, knock(rng, n, .004, .3, STEEL, decay=.09, noise=.05))              # the rim
    return out


def armour_hit(rng):
    dur = .9; n = int(dur * RATE)
    out = knock(rng, n, 0., .9, STEEL, decay=.12, noise=.35)
    out = add(out, knock(rng, n, .002, .7, (120, 230, 380), decay=.03, noise=.3))
    out = add(out, knock(rng, n, .006, .25, BONE, decay=.01, noise=.4))               # the knuckles
    return out


def armour_step(rng):
    dur = .5; n = int(dur * RATE)
    out = knock(rng, n, 0., 1., (90, 180, 340), decay=.03, noise=.7)
    for _ in range(rng.randint(14, 22)):   # mail rings shivering
        out = add(out, knock(rng, n, rng.uniform(.005, .22), rng.uniform(.03, .09), (4200, 5600, 7100), decay=.004, noise=.2))
    out = add(out, knock(rng, n, rng.uniform(.01, .03), .18, STEEL, decay=.05, noise=.1))
    return out


def shield_bash(rng):
    dur = 1.; n = int(dur * RATE)
    out = knock(rng, n, 0., 1., (70, 130, 240, 400), decay=.08, noise=.6)
    out = add(out, knock(rng, n, .01, .7, BONE, decay=.02, noise=.6))
    for _ in range(12):
        out = add(out, knock(rng, n, rng.uniform(.05, .6), rng.uniform(.1, .35), BONE if rng.random() < .5 else SMALL, decay=.007, noise=.4))
    return out


def bow_draw(rng):
    dur = .75; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    creak = svf_band(noise, [180 + 60 * math.sin(i / RATE * 23) for i in range(n)], q=9.)
    out = []
    for i in range(n):
        t = i / RATE
        env = min(1., t / .1) * (1 - t / dur) ** .5
        pulses = .6 + .4 * (math.sin(t * 2 * math.pi * 31) > .4)
        out.append(creak[i] * env * pulses * .8)
    return out


def bow_release(rng):
    dur = .5; n = int(dur * RATE)
    f0 = rng.uniform(98, 115)
    out = []
    for i in range(n):
        t = i / RATE
        s = sum(math.sin(2 * math.pi * f0 * k * (1 + .002 * k * k) * t) / k for k in range(1, 9)) * math.exp(-t * 18)
        out.append(s * .6)
    out = add(out, knock(rng, n, 0., .8, (85, 160, 300), decay=.025, noise=.5))       # the limbs' thump
    return out


def arrow_flight(rng):
    dur = .35; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    band = svf_band(noise, [2200 - 1500 * (i / n) for i in range(n)], q=3.)
    return [band[i] * math.sin(math.pi * i / n) ** 1.5 for i in range(n)]


def arrow_hit(rng):
    dur = .5; n = int(dur * RATE)
    out = knock(rng, n, 0., 1., (600, 1250, 2200), decay=.018, noise=.8)
    out = add(out, [.35 * math.sin(2 * math.pi * 145 * (i / RATE)) * math.exp(-(i / RATE) * 22) * (1 + .5 * math.sin(2 * math.pi * 31 * i / RATE)) for i in range(n)])
    for _ in range(4):
        out = add(out, knock(rng, n, rng.uniform(.03, .2), rng.uniform(.08, .2), SMALL, decay=.005))
    return out


def bolt_cast(rng):
    dur = .8; n = int(dur * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    band = svf_band(noise, [400 + 3000 * (i / n) ** 1.5 for i in range(n)], q=1.8)
    out = []
    for i in range(n):
        t = i / RATE
        env = (t / .45) ** 2 if t < .45 else math.exp(-(t - .45) * 9)
        tone = .25 * math.sin(2 * math.pi * (220 + 500 * t) * t) * env
        out.append(band[i] * env + tone)
    for _ in range(40):   # sparks
        out = add(out, knock(rng, n, rng.uniform(.1, .6), rng.uniform(.05, .25), (3300, 5200, 7400), decay=.002, noise=.9))
    return out


def bolt_impact(rng):
    dur = 1.3; n = int(dur * RATE)
    out = [math.sin(2 * math.pi * (70 - 30 * (i / RATE)) * (i / RATE)) * math.exp(-(i / RATE) * 7) for i in range(n)]
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    crackle = highpass(noise, .2)
    out = add(out, [crackle[i] * .5 * math.exp(-(i / RATE) * 6) * (rng.random() < .3) for i in range(n)])
    for _ in range(18):
        out = add(out, knock(rng, n, rng.uniform(.05, .9), rng.uniform(.1, .4), BONE if rng.random() < .5 else SMALL, decay=.007, noise=.4))
    return out


def channel(rng):
    """Exactly 4 s, every partial a whole number of cycles in it: it loops without a seam."""
    dur = 4.; n = int(dur * RATE)
    parts = [(55., .5), (82.5, .35), (110.25, .3), (165., .2), (220.5, .12), (331., .06), (440.25, .05)]
    out = []
    for i in range(n):
        t = i / RATE
        s = 0.
        for f, a in parts:
            fq = round(f * dur) / dur
            s += a * math.sin(2 * math.pi * fq * t)
        beat = .75 + .25 * math.sin(2 * math.pi * .5 * t)      # 2 beats in 4 s
        shimmer = .04 * math.sin(2 * math.pi * (round(1760 * dur) / dur) * t) * (.5 + .5 * math.sin(2 * math.pi * 1.25 * t))
        out.append(s * beat + shimmer)
    return out


def skull_bounce(rng):
    dur = 1.1; n = int(dur * RATE); out = [0.] * n
    t, g = 0., 1.
    while t < .8 and g > .08:
        out = add(out, knock(rng, n, t, g, (420, 950, 1800), decay=.025, noise=.6))
        t += .22 * g + .03; g *= .55
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    roll = highpass(onepole(noise, .3), .06)
    return add(out, [roll[i] * .12 * max(0., 1 - abs(i / RATE - .75) / .3) for i in range(n)])


def write(name, samples, loop=False):
    peak = max(abs(x) for x in samples) or 1.
    fade = int(.04 * RATE)
    pcm = [int(x / peak * PEAK * (1. if loop else min(1., (len(samples) - i) / fade))) for i, x in enumerate(samples)]
    path = root / (name + '.wav')
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        out.writeframes(struct.pack('<' + 'h' * len(pcm), *pcm))
    return dict(name=name, file=path.name, duration=round(len(pcm) / RATE, 3), sha256=hashlib.sha256(path.read_bytes()).hexdigest(), peak_dbfs=-3.0, loop=loop)


entries = []
for name, make, count, seed, wet, tail in (
        ('SFX_HeroSwordWhoosh', whoosh, 4, 20261101, .22, .6),
        ('SFX_HeroSwordHitBone', sword_bone, 4, 20261102, .3, .8),
        ('SFX_HeroShieldBlock', shield_block, 3, 20261103, .3, .8),
        ('SFX_HeroArmourHit', armour_hit, 3, 20261104, .3, .9),
        ('SFX_HeroArmourStep', armour_step, 4, 20261105, .25, .6),
        ('SFX_HeroShieldBash', shield_bash, 2, 20261106, .3, .9),
        ('SFX_HeroBowDraw', bow_draw, 1, 20261107, .15, .4),
        ('SFX_HeroBowRelease', bow_release, 3, 20261108, .25, .6),
        ('SFX_HeroArrowFlight', arrow_flight, 2, 20261109, .15, .4),
        ('SFX_HeroArrowHit', arrow_hit, 3, 20261110, .3, .7),
        ('SFX_HeroBoltCast', bolt_cast, 2, 20261111, .3, .9),
        ('SFX_HeroBoltImpact', bolt_impact, 2, 20261112, .4, 1.4),
        ('SFX_HeroSkullBounce', skull_bounce, 3, 20261113, .3, .8)):
    for v in range(count):
        rng = random.Random(seed * 10 + v)
        entries.append(write(f'{name}_{v:02d}', chamber(make(rng), rng, wet=wet, tail=tail)))
entries.append(write('SFX_HeroChannel_00', channel(random.Random(202611140)), loop=True))   # dry: it loops
(root / 'hero-sfx-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthetic hero sounds; Tools/gen_hero_sfx.py', sounds=entries), indent=2) + '\n')
print('CHUCK_HERO_WAV_GENERATED', len(entries))
