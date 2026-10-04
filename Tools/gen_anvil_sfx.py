"""Original synthetic forge sounds for the plaza blacksmith (user 2026-10-04: his
motion and sound effects); no external recordings or dependencies (stdlib only).

python Tools/gen_anvil_sfx.py -> SourceAssets/Audio/SFX/*.wav (+ anvil-manifest.json)

  SFX_AnvilStrike_00..03  the heavy blow: hammer on hot iron on the anvil. A hard
                          contact click, the soft bar's dull thud and the stump's
                          thump under it, the anvil body's inharmonic ring (damped
                          by the work and the stump) and the hammer's own short ring.
  SFX_AnvilTap_00..03     the light tap on the bare face between blows: a bright,
                          longer "ting", little thud.
  SFX_TongsClink_00..02   picking up / setting down the bar: two small iron contacts.
  SFX_ForgeLoop_00        the forge beside him: a low breathing roar, a faint hiss
                          and crackles; 8 s, seamless (looped by the runtime).

Every one-shot gets the same short stone-plaza echo: sparse early reflections
off the shopfronts, then a diffuse tail, low-passed. Seeded: reproducible.
"""
import hashlib, json, math, random, struct, wave
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
root.mkdir(parents=True, exist_ok=True)
RATE = 48000
PEAK = 23197   # -3 dBFS


def onepole(x, a):
    """One-pole low-pass, coefficient a (0..1: higher passes more)."""
    y, out = 0., []
    for v in x:
        y += a * (v - y); out.append(y)
    return out


def highpass(x, a):
    low = onepole(x, a)
    return [v - l for v, l in zip(x, low)]


def echo(x, rng, wet=.22, tail=.42):
    """Stone plaza: early reflections (shopfronts 3-9 m away), then a diffuse decaying tail."""
    taps = [(rng.uniform(.011, .028), rng.uniform(.35, .5)) for _ in range(4)]
    t = .03
    while t < tail:
        taps.append((t, .32 * math.exp(-t / (tail / 4.)) * rng.uniform(.5, 1.)))
        t += rng.uniform(.004, .011)
    n = len(x) + int(tail * RATE)
    w = [0.] * n
    for delay, g in taps:
        d = int(delay * RATE)
        g *= rng.choice((-1., 1.))
        for i, v in enumerate(x):
            if v: w[i + d] += g * v
    w = onepole(w, .35)   # stone and air take the top off
    return [(x[i] if i < len(x) else 0.) + wet * w[i] for i in range(n)]


def partials(t, modes):
    return sum(a * math.sin(2 * math.pi * f * t + p) * math.exp(-t / d) for f, a, d, p in modes)


def strike(rng):
    dur = .75
    n = int(dur * RATE)
    f0 = 690 * rng.uniform(.95, 1.05)
    # Anvil body modes (inharmonic, damped by the hot bar and the stump) and the hammer's own.
    anvil = [(f0 * r * rng.uniform(.985, 1.015), a * rng.uniform(.8, 1.2), d * rng.uniform(.85, 1.15), rng.uniform(0, 6.3))
             for r, a, d in ((1., .34, .2), (1.72, .3, .16), (2.61, .26, .1), (3.9, .2, .06), (5.3, .12, .04), (7.4, .07, .025))]
    hammer = [(f * rng.uniform(.97, 1.03), a, d, rng.uniform(0, 6.3)) for f, a, d in ((3150, .1, .035), (4420, .08, .025), (6100, .05, .015))]
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    low = onepole(noise, .02)
    click = highpass(noise, .3)
    out = []
    for i in range(n):
        t = i / RATE
        att = 1 - math.exp(-t * 6000)
        thud = low[i] * 2.6 * math.exp(-t * 45) + .16 * math.sin(2 * math.pi * 95 * t) * math.exp(-t * 26)   # soft bar, stump
        out.append(click[i] * 1.1 * math.exp(-t * 700) + thud + att * (partials(t, anvil) + partials(t, hammer)))
    return out


def tap(rng):
    dur = .9
    n = int(dur * RATE)
    f0 = 1180 * rng.uniform(.96, 1.04)
    ring = [(f0 * r * rng.uniform(.99, 1.01), a * rng.uniform(.8, 1.2), d * rng.uniform(.85, 1.15), rng.uniform(0, 6.3))
            for r, a, d in ((1., .42, .45), (2.76, .3, .28), (5.4, .16, .12), (8.9, .07, .05))]
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    click = highpass(noise, .4)
    return [click[i] * .6 * math.exp(-i / RATE * 1200) + (1 - math.exp(-i / RATE * 8000)) * partials(i / RATE, ring) for i in range(n)]


def clink(rng):
    dur = .3
    n = int(dur * RATE)
    hits = [(0., 1.), (rng.uniform(.045, .075), rng.uniform(.45, .7))]
    out = [0.] * n
    for start, g in hits:
        modes = [(f * rng.uniform(.95, 1.05), a, d, rng.uniform(0, 6.3)) for f, a, d in ((2380, .3, .03), (3710, .25, .022), (5230, .18, .015), (7020, .1, .01))]
        s = int(start * RATE)
        for i in range(s, n):
            t = (i - s) / RATE
            out[i] += g * (partials(t, modes) + rng.uniform(-1, 1) * .25 * math.exp(-t * 900))
    return out


def forge_loop(rng):
    dur, fade = 8., 1.
    n = int((dur + fade) * RATE)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    rumble = onepole(onepole(noise, .006), .01)
    hiss = highpass(onepole(noise, .25), .04)
    swells = [(rng.uniform(.25, .7), rng.uniform(0, 6.3), rng.uniform(.15, .3)) for _ in range(3)]
    pops = [(rng.uniform(0, dur + fade), rng.uniform(.15, .6), rng.uniform(1800, 4200)) for _ in range(int(7 * (dur + fade)))]
    out = []
    for i in range(n):
        t = i / RATE
        breath = 1 + sum(a * math.sin(2 * math.pi * f * t + p) for f, p, a in swells)
        out.append(rumble[i] * 38. * breath + hiss[i] * .05 * breath)
    for start, g, f in pops:   # crackles: tiny filtered pops
        s = int(start * RATE)
        for k in range(int(.012 * RATE)):
            if s + k < n:
                t = k / RATE
                out[s + k] += g * math.sin(2 * math.pi * f * t) * math.exp(-t * 700) * (1 if k else 2)
    # Seamless: the extra second fades into the start.
    m = int(dur * RATE)
    for k in range(int(fade * RATE)):
        w = k / (fade * RATE)
        out[k] = out[k] * w + out[m + k] * (1 - w)
    return out[:m]


def write(name, samples, loop=False):
    peak = max(abs(x) for x in samples) or 1.
    fade = int(.04 * RATE)
    pcm = []
    for i, x in enumerate(samples):
        g = 1. if loop else min(1., (len(samples) - i) / fade)
        pcm.append(int(x / peak * PEAK * g))
    path = root / (name + '.wav')
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        out.writeframes(struct.pack('<' + 'h' * len(pcm), *pcm))
    return dict(name=name, file=path.name, duration=round(len(pcm) / RATE, 3), loop=loop,
                sha256=hashlib.sha256(path.read_bytes()).hexdigest(), peak_dbfs=-3.0)


entries = []
for v in range(4):
    rng = random.Random(51207 + v)
    entries.append(write(f'SFX_AnvilStrike_{v:02d}', echo(strike(rng), rng)))
for v in range(4):
    rng = random.Random(62011 + v)
    entries.append(write(f'SFX_AnvilTap_{v:02d}', echo(tap(rng), rng, wet=.18)))
for v in range(3):
    rng = random.Random(73303 + v)
    entries.append(write(f'SFX_TongsClink_{v:02d}', echo(clink(rng), rng, wet=.15, tail=.25)))
entries.append(write('SFX_ForgeLoop_00', forge_loop(random.Random(84400)), loop=True))
(root / 'anvil-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthetic forge sounds; Tools/gen_anvil_sfx.py', sounds=entries), indent=2) + '\n')
print('CHUCK_ANVIL_WAV_GENERATED', len(entries))
