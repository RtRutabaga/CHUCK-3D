"""The ship's ambience (Claude, 2026-11-01; docs/SHIP.md): original deterministic synthetic sounds, no recordings.

python Tools/gen_ship_sfx.py      (Python 3 standard library only)

- SFX_ShipWash_00: the sea against the hull heard from inside, a seamless 21.9 s loop (three 7.3 s swells, the period
  the ship's lamps and hammocks swing with in ShipSetting.cpp): a low, muffled wash rising and falling with each swell,
  a little brighter hiss as each one breaks along the planking.
- SFX_ShipCreak_00..03: timbers working: stick-slip pulses gliding in pitch, ringing two wooden resonances.
Mono 48 kHz 16-bit, peaks -3 dBFS. Writes SourceAssets/Audio/SFX/ship-manifest.json.
"""
import hashlib, json, math, random, struct, wave
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
root.mkdir(parents=True, exist_ok=True)
RATE = 48000
entries = []


def write(name, samples, peak_db=-3.0):
    peak = max(abs(x) for x in samples) or 1
    scale = 32767 * 10 ** (peak_db / 20) / peak
    pcm = [int(max(-32767, min(32767, x * scale))) for x in samples]
    path = root / (name + '.wav')
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        out.writeframes(struct.pack('<' + 'h' * len(pcm), *pcm))
    entries.append(dict(name=name, file=path.name, duration=round(len(pcm) / RATE, 4),
                        sha256=hashlib.sha256(path.read_bytes()).hexdigest(), peak_dbfs=peak_db))


class Band:
    """Two-pole resonant band-pass (RBJ), constant peak gain."""
    def __init__(self, f, q):
        w = 2 * math.pi * f / RATE
        a = math.sin(w) / (2 * q)
        self.b0, self.b2 = a / (1 + a), -a / (1 + a)
        self.a1, self.a2 = -2 * math.cos(w) / (1 + a), (1 - a) / (1 + a)
        self.x1 = self.x2 = self.y1 = self.y2 = 0.0

    def __call__(self, x):
        y = self.b0 * x + self.b2 * self.x2 - self.a1 * self.y1 - self.a2 * self.y2
        self.x2, self.x1, self.y2, self.y1 = self.x1, x, self.y1, y
        return y


# ---- The wash: generated a little long, its tail crossfaded into its head so the loop has no seam.
rng = random.Random(20261101)
period, swells = 7.3, 3
length = int(RATE * period * swells)
fade = int(RATE * 1.5)
lo1 = lo2 = lo3 = 0.0
hiss_band = Band(1900, .7)
raw = []
for i in range(length + fade):
    t = i / RATE
    n = rng.uniform(-1, 1)
    lo1 += .012 * (n - lo1)          # a deep rumble
    lo2 += .05 * (n - lo2)           # the body of the wash
    phase = (t % period) / period
    swell = .55 + .45 * math.sin(2 * math.pi * phase - 1.2)
    # Each swell breaks along the planking: a brighter rush just after its crest, dying away.
    brk = max(0.0, math.sin(math.pi * min(1.0, max(0.0, (phase - .22) / .4)))) ** 2
    hiss = hiss_band(n) * brk
    lo3 += .3 * (hiss - lo3)
    raw.append(lo1 * 5.5 * (.7 + .3 * swell) + lo2 * 1.6 * swell + lo3 * .55)
wash = raw[:length]
for i in range(fade):
    k = i / fade
    wash[i] = wash[i] * k + raw[length + i] * (1 - k)
write('SFX_ShipWash_00', wash, -6.0)

# ---- Creaks.
for v in range(4):
    rng = random.Random(4100 + v)
    dur = .7 + .25 * v
    f0, f1 = rng.uniform(55, 80), rng.uniform(95, 150)
    bodies = [Band(rng.uniform(260, 380), 9), Band(rng.uniform(620, 880), 12), Band(rng.uniform(1300, 1700), 7)]
    out, nxt, t = [], 0.0, 0.0
    for i in range(int(RATE * dur)):
        t = i / RATE
        u = t / dur
        env = math.sin(math.pi * u) ** .7 * (1 - .4 * u)
        x = 0.0
        if t >= nxt:   # a slip: one pulse, the next a little early or late (stick-slip)
            x = rng.uniform(.7, 1.0)
            f = f0 + (f1 - f0) * (u if v % 2 else 1 - u) + rng.uniform(-6, 6)
            nxt = t + 1 / f * rng.uniform(.85, 1.15)
        x += rng.uniform(-1, 1) * .04
        y = bodies[0](x) * 1.0 + bodies[1](x) * .7 + bodies[2](x) * .25
        out.append(y * env)
    write(f'SFX_ShipCreak_{v:02d}', out)

(root / 'ship-manifest.json').write_text(json.dumps(dict(provenance='Original deterministic synthetic ship ambience; Tools/gen_ship_sfx.py', sounds=entries), indent=2) + '\n')
print('CHUCK_SHIP_WAV_GENERATED', len(entries))
