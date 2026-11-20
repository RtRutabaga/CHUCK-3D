"""Blender 4.5.14: the Feywild's plant and fungus kit, first pass for the look slice (Claude, 2026-11-18; docs/regions/FEYWILD.md).

blender --background --factory-startup --python Tools/build_feywild_kit.py
  -> SourceAssets/Setting/Feywild/Derived/*.fbx and manifest.json

Modelled here, no downloads: glowing blue caps, amber-gilled giants, violet clusters, bracket shelves, bell-flower lamps,
foxgloves, broad dewy leaves, crimson-veined leaves, lily pads and pink water lilies, fallen petals, and two ancient oaks
with roots, moss, leaf clusters and hanging wisteria (each with a trunk-and-roots collision mesh).
Derived from the CC0 scans (Tools/Fetch-Feywild.ps1): grass_medium_02, celandine_01 and a clump of moss_01 strands.

Units are centimetres, Z up, pivots at the base. Vertex colour on the modelled plants: RGB albedo (linear), A glow.
On the oaks' bark: R is how much moss grows there. Material slots: FeyBark, FeyLeaf, FeyPlant, and the scans' own.
"""
from pathlib import Path
import hashlib
import json
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Setting/Feywild'
OUT = SOURCE / 'Derived'
OUT.mkdir(parents=True, exist_ok=True)
REPORT = []
KIT_ONLY = [n.strip() for n in os.environ.get('CHUCK_FEY_KIT_ONLY', '').split(',') if n.strip()]   # rebuild only these, keeping the rest of the manifest


def lerp(a, b, t): return a + (b - a) * t
def mix(a, b, t): return tuple(lerp(x, y, t) for x, y in zip(a, b))
def clamp(x, a=0., b=1.): return max(a, min(b, x))
def smooth(a, b, x):
    t = clamp((x - a) / (b - a)); return t * t * (3 - 2 * t)
def lumps(x, y, z, s=0.): return (math.sin(x * .031 + s) * math.cos(y * .027 - s) + .6 * math.sin(y * .067 + z * .041 + s * 2) + .4 * math.cos(z * .093 + x * .071 - s)) / 2.


class Kit:
    """Vertices with a colour each, faces with a material slot and per-corner UVs."""
    def __init__(self, slots):
        self.slots = list(slots); self.v = []; self.c = []; self.f = []; self.m = []; self.uv = []

    def vert(self, p, col):
        self.v.append(tuple(p)); self.c.append(tuple(col) if len(col) == 4 else (*col, 0.)); return len(self.v) - 1

    def face(self, idx, slot=0, uv=None):
        self.f.append(tuple(idx)); self.m.append(slot)
        self.uv.append(uv or [(self.v[i][0] / 200., self.v[i][2] / 200.) for i in idx])

    def grid(self, rows, closed_u, slot=0, uv=None):
        """rows: list of equal-length vertex index rows; quads between them."""
        n = len(rows[0])
        for r in range(len(rows) - 1):
            for i in range(n if closed_u else n - 1):
                j = (i + 1) % n
                q = (rows[r][i], rows[r][j], rows[r + 1][j], rows[r + 1][i])
                self.face(q, slot, uv(r, i) if uv else None)


def frame_along(d, hint=None):
    d = d.normalized(); a = hint or (Vector((0, 0, 1)) if abs(d.z) < .95 else Vector((1, 0, 0)))
    x = d.cross(a).normalized(); return x, d.cross(x).normalized()


def tube(k, path, radii, sides=10, col=lambda p, t, a: (.5, .4, .3, 0), slot=0, lump=0., seed=0., cap=True):
    rows = []; x_prev = None; length = 0.
    for i, p in enumerate(path):
        d = (path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized()
        if x_prev is None: x, y = frame_along(d)
        else:
            x = (x_prev - d * x_prev.dot(d)).normalized(); y = d.cross(x).normalized()
        x_prev = x; row = []; t = i / (len(path) - 1)
        if i: length += (p - path[i - 1]).length
        for s in range(sides):
            a = 2 * math.pi * s / sides; n = x * math.cos(a) + y * math.sin(a)
            r = radii[i] * (1 + lump * lumps(p.x + n.x * 90, p.y + n.y * 90, p.z + n.z * 90 + a * 40, seed))
            q = p + n * r; row.append(k.vert(q, col(q, t, a)))
        rows.append(row)
    k.grid(rows, True, slot)
    if cap: k.face(tuple(reversed(rows[-1])), slot)
    return rows


def revolve(k, profile, segs=24, at=Vector((0, 0, 0)), axis=None, slot=0, shape=None):
    """profile: (radius, height, colour); shape(ring, angle) -> (radius factor, height offset, colour or None)."""
    rot = axis.to_track_quat('Z', 'Y').to_matrix() if axis else Matrix.Identity(3); rows = []
    for ri, (r, z, c) in enumerate(profile):
        row = []
        for s in range(segs):
            a = 2 * math.pi * s / segs; fr, dz, cc = shape(ri, a) if shape else (1., 0., None)
            row.append(k.vert(at + rot @ Vector((r * fr * math.cos(a), r * fr * math.sin(a), z + dz)), cc or c))
        rows.append(row)
    k.grid(rows, True, slot)
    return rows


def ugrid(k, rows, v, slot=0):
    """A closed ring grid whose U runs 0..1 round it and whose V is v(row): the fungus material reads both."""
    n = len(rows[0])
    for r in range(len(rows) - 1):
        for i in range(n):
            j = (i + 1) % n
            k.face((rows[r][i], rows[r][j], rows[r + 1][j], rows[r + 1][i]), slot, [(i / n, v(r)), ((i + 1) / n, v(r)), ((i + 1) / n, v(r + 1)), (i / n, v(r + 1))])


def slot_of(k, name): return k.slots.index(name) if name in k.slots else 0


ACROSS = (-1, -.62, -.28, 0, .28, .62, 1)


def blade(k, base, direction, length, width, side, col, droop=.35, rows=12, fold=.3, slot=None, wave=0.):
    """A solid leaf or petal: a folded blade drooping along its length. col(u along, v across -1..1) -> rgba."""
    d = direction.normalized(); s = side.normalized(); up = s.cross(d).normalized(); grid = []
    for r in range(rows + 1):
        u = r / rows; w = width * .5 * math.sin(math.pi * min(1, u ** .75)) ** .8
        ce = base + d * (length * u) - up * (droop * length * u * u) + s * (wave * length * math.sin(u * 7.) * u)
        row = []
        for j in ACROSS:
            row.append(k.vert(ce + s * (j * w) + up * (abs(j) * w * fold), col(u, j)))
        grid.append(row)
    if slot is None: slot = next((i for i, n in enumerate(k.slots) if n.startswith(('FeyBlade', 'FeyPetal'))), 0)
    for r in range(rows):
        for a in range(len(ACROSS) - 1):
            q = (grid[r][a], grid[r][a + 1], grid[r + 1][a + 1], grid[r + 1][a])
            k.face(q, slot, [(r / rows, (ACROSS[a] + 1) / 2), (r / rows, (ACROSS[a + 1] + 1) / 2), ((r + 1) / rows, (ACROSS[a + 1] + 1) / 2), ((r + 1) / rows, (ACROSS[a] + 1) / 2)])


# ---------------------------------------------------------------- the fungi
def spots(a, r, seed): return smooth(.45, .8, math.sin(a * 7 + seed) * math.cos(r * .19 + a * 3 + seed) * .5 + .5)


def mushroom(k, at, h, cap_r, top, under, stem, glow_cap, glow_under, lean=Vector((0, 0, 0)), seed=0., segs=24, gills=26):
    """V says which part the fungus material is painting: 0-1 gills (stem to rim), 1-2 dome (rim to crown), 2-3 stem (foot to cap)."""
    tip = at + Vector((0, 0, h)) + lean; n = 14; sides = 16; rows = []
    for i in range(n + 1):
        t = i / n; c = at.lerp(tip, t) + lean * (.25 * math.sin(math.pi * t))
        r = cap_r * (lerp(.19, .115, smooth(0, .55, t)) + .11 * smooth(.2, 0, t) ** 2 + .03 * math.exp(-((t - .8) / .065) ** 2) + .05 * smooth(.9, 1., t))
        row = []
        for s_ in range(sides):
            a = 2 * math.pi * s_ / sides; w = 1 + .05 * math.sin(a * 3 + seed + t * 4)
            row.append(k.vert(c + Vector((r * w * math.cos(a), r * w * math.sin(a), 0)), (*mix(mix((.1, .08, .07), stem, smooth(0, .22, t)), under, smooth(.6, 1., t) * .6), lerp(glow_under * .1, glow_under * .6, t ** 2.5))))
        rows.append(row)
    ugrid(k, rows, lambda r: 2 + .98 * r / n)
    wob = lambda a, t: 1 + .035 * t * math.sin(a * 3 + seed) + .02 * t * math.sin(a * 7 + seed * 2)
    ring = segs * 2; under_rows = []
    for ri in range(8):
        t = ri / 7; row = []
        for s_ in range(ring):
            a = 2 * math.pi * s_ / ring; r = cap_r * lerp(.1, .985, t) * wob(a, t)
            row.append(k.vert(tip + Vector((r * math.cos(a), r * math.sin(a), cap_r * (-.015 + .075 * math.sin(t * math.pi * .5) - .1 * smooth(.82, 1., t)))), (*under, glow_under)))
        under_rows.append(row)
    ugrid(k, under_rows, lambda r: .98 * r / 7)
    dome = []
    for ri in range(13):
        t = ri / 12; th = t * math.pi * .5; row = []
        lip = smooth(.16, 0., t)
        for s_ in range(ring):
            a = 2 * math.pi * s_ / ring; r = cap_r * (math.cos(th) ** .72 + .015 * lip) * wob(a, 1 - t)
            z = cap_r * (-.1 * lip + .03 + .5 * math.sin(th) ** 1.15)
            row.append(k.vert(tip + Vector((r * math.cos(a), r * math.sin(a), z)), (*mix(top, under, lip * .55), glow_cap * (.3 + lip))))
        dome.append(row)
    ugrid(k, dome, lambda r: 1 + .98 * r / 12)


def fungus_blue(k):
    mushroom(k, Vector((0, 0, 0)), 34, 19, (.05, .17, .55), (.2, .55, .95), (.26, .38, .52), 2.0, 3.0, seed=1.3)
    mushroom(k, Vector((24, 8, 0)), 20, 11, (.05, .17, .55), (.2, .55, .95), (.26, .38, .52), 2.0, 3.0, Vector((4, 1, 0)), 2.7, 18)
    mushroom(k, Vector((-14, 20, 0)), 14, 8, (.05, .17, .55), (.2, .55, .95), (.26, .38, .52), 2.0, 3.0, Vector((-3, 2, 0)), 4.1, 16)


def fungus_amber(k):
    mushroom(k, Vector((0, 0, 0)), 230, 150, (.17, .09, .16), (.95, .5, .17), (.55, .36, .24), .1, 2.1, Vector((14, -6, 0)), 2.2, 36, 54)


def fungus_violet(k):
    rng = random.Random(5)
    for i in range(6):
        a = i * 2.4; r = 6 + i * 8
        h = rng.uniform(22, 48); c = h * rng.uniform(.3, .4)
        mushroom(k, Vector((r * math.cos(a), r * math.sin(a), 0)), h, c, (.2, .08, .36), (.68, .42, .62), (.42, .34, .44), .4, 1.3, Vector((rng.uniform(-5, 5), rng.uniform(-5, 5), 0)), i * 1.7, 16, 16)


def bracket(k, at, out, w, seed, col_top=(.32, .19, .1), col_under=(1., .6, .25)):
    """A shelf fungus on a trunk: at on the bark, out along the bark's normal."""
    o = out.normalized(); s = Vector((0, 0, 1)).cross(o).normalized(); rows = []
    for r in range(6):
        t = r / 5; row = []
        for j in range(13):
            u = j / 12 * math.pi; reach = w * .62 * math.sin(u) ** .6 * t
            p = at + s * (math.cos(u) * w * lerp(.95, 1., t)) + o * reach + Vector((0, 0, w * .16 * math.sin(math.pi * t) ** .8 - w * .1 * t * t))
            row.append(k.vert(p, (*mix(col_top, (.75, .5, .28), t ** 3), .05 + .3 * t ** 4)))
        rows.append(row)
    k.grid(rows, False)
    under = []
    for r in range(4):
        t = r / 3; row = []
        for j in range(13):
            u = j / 12 * math.pi; reach = w * .62 * math.sin(u) ** .6 * t
            row.append(k.vert(at + s * (math.cos(u) * w) + o * reach + Vector((0, 0, -w * .1 * t * t - w * .05)), (*col_under, 1.5)))
        under.append(row)
    k.grid(list(reversed(under)), False)


def bell_lamp(k):
    pts = []
    for i in range(15):
        t = i / 14; a = t * 2.65
        pts.append(Vector((62 * (1 - math.cos(a)) * .62, 0, 175 * math.sin(min(a, math.pi / 2)) + (-44 * smooth(1.57, 2.65, a) if a > 1.57 else 0))))
    tube(k, pts, [lerp(2.6, 1.0, i / 14) for i in range(15)], 7, lambda p, t, a: ((.24, .32, .08) if t < .7 else (.5, .38, .1)) + (0,))
    tip = pts[-1]; down = Vector((.18, 0, -1)).normalized(); pale = (1., .56, .2)
    pr = [(2.2, 0, (*pale, .5)), (11, -5, (*pale, 1.1)), (16, -18, (*pale, 1.6)), (17, -30, (1., .62, .24, 2.1)), (23, -40, (1., .68, .3, 1.5)), (30, -43, (1., .5, .26, .9))]
    rot = down.to_track_quat('-Z', 'Y').to_matrix()
    rows = []
    for ri, (r, z, c) in enumerate(pr):
        row = []
        for s in range(30):
            a = 2 * math.pi * s / 30; lobe = .5 + .5 * math.cos(a * 5)
            f = 1 + (ri >= 4) * (lobe - .5) * .36; zz = z - (ri == 5) * lobe * 7
            row.append(k.vert(tip + rot @ Vector((r * f * math.cos(a), r * f * math.sin(a), zz)), c))
        rows.append(row)
    fungus = slot_of(k, 'FeyFungus'); ugrid(k, rows, lambda r: 3 + .98 * r / 5, fungus)
    # The lit heart inside the bell.
    heart = []
    for r, z in ((.5, 0), (7, -3), (8, -11), (5, -20), (.5, -24)):
        heart.append([k.vert(tip + rot @ Vector((r * math.cos(a * math.pi / 6), r * math.sin(a * math.pi / 6), z - 14)), (1, .88, .55, 9.)) for a in range(12)])
    ugrid(k, heart, lambda r: 4.5, fungus)
    for i in range(3):
        a = i * 2.1; d = Vector((math.cos(a), math.sin(a), .35)).normalized()
        blade(k, Vector((2, 0, 18 + i * 22)), d, 42, 14, Vector((-math.sin(a), math.cos(a), 0)), lambda u, j: (*mix((.14, .32, .06), (.42, .58, .12), abs(j) * .4 + u * .4), 0.))


def raceme(k, top, length, rng, sway=Vector((0, 0, 0))):
    """Hanging wisteria: florets down a thread, wide at the top and tapering to buds."""
    n = int(length / 2.1); pale, deep = (.62, .5, .98), (.2, .08, .62)
    path = [top + Vector((0, 0, -length * i / 5)) + sway * (i / 5) ** 2 for i in range(6)]
    tube(k, path, [.5, .45, .4, .35, .3, .2], 4, lambda p, t, a: (.2, .24, .08, 0.), cap=False)
    for i in range(n):
        t = i / (n - 1); a = i * 2.4 + rng.uniform(-.3, .3); r = lerp(7.5, 1.0, t ** 1.2) * rng.uniform(.8, 1.15)
        c = top + Vector((r * math.cos(a), r * math.sin(a), -length * t)) + sway * t ** 2
        s = lerp(3.3, 1.2, t) * rng.uniform(.85, 1.2); col = (*mix(pale, deep, smooth(.05, .95, t + rng.uniform(-.15, .15))), .03)
        ox = Vector((math.cos(a), math.sin(a), -.25)).normalized() * s * 1.25; oy = Vector((-math.sin(a), math.cos(a), 0)) * s; oz = Vector((0, 0, s * .8))
        idx = [k.vert(c + o, col) for o in (ox, oy, -ox * .6, -oy, oz, -oz)]
        for q in ((0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4), (1, 0, 5), (2, 1, 5), (3, 2, 5), (0, 3, 5)):
            k.face(tuple(idx[x] for x in q))


# ---------------------------------------------------------------- flowers and leaves
def foxglove(k):
    rng = random.Random(3); pts = [Vector((math.sin(i * .5) * 2, 0, i * 16)) for i in range(9)]
    tube(k, pts, [lerp(1.7, .4, i / 8) for i in range(9)], 6, lambda p, t, a: (.2, .34, .08, 0.))
    for i in range(30):
        t = i / 29; a = i * 2.4; z = lerp(52, 126, t); s = lerp(1., .35, t)
        o = Vector((math.cos(a), math.sin(a), 0)); base = Vector((0, 0, z)) + o * 1.2; ax = (o + Vector((0, 0, -.65))).normalized()
        pink, pale = (.85, .1, .5), (1., .75, .88)
        revolve(k, [(.8 * s, 0, (*pink, 0.)), (3.4 * s, 3 * s, (*pink, 0.)), (4.8 * s, 11 * s, (*pink, 0.)), (6.2 * s, 17 * s, (*pale, .1))], 8, base, ax)
    for i in range(7):
        a = i * 2.4; d = Vector((math.cos(a), math.sin(a), .55)).normalized()
        blade(k, Vector((0, 0, 3 + i * 4)), d, rng.uniform(36, 48), 16, Vector((-math.sin(a), math.cos(a), 0)), lambda u, j: (*mix((.1, .26, .05), (.3, .46, .1), abs(j) * .5), 0.))


def leaf_plant(k, seed, count, length, width, col):
    rng = random.Random(seed)
    for i in range(count):
        a = i * 2.4 + rng.uniform(-.25, .25); rise = rng.uniform(.5, 1.15); l = length * rng.uniform(.75, 1.15)
        d = Vector((math.cos(a), math.sin(a), rise)).normalized()
        stalk = [d * (l * .3 * j / 3) for j in range(4)]
        tube(k, stalk, [1.3, 1.1, .9, .7], 5, lambda p, t, b: (*col(.0, 0.)[:3], 0.), cap=False)
        blade(k, stalk[-1], Vector((math.cos(a), math.sin(a), rise * .5)), l, width * rng.uniform(.85, 1.15), Vector((-math.sin(a), math.cos(a), 0)), col, droop=rng.uniform(.3, .55), rows=8, wave=.02)


def broad_col(u, j): return (*mix((.06, .2, .035), (.22, .38, .06), u * .55 + abs(j) * .3), 0.)
def crimson_col(u, j): return (*mix((.07, .012, .07), (.2, .025, .14), u), 0.)


def lily_pad(k):
    """The FeyPad material paints the veins from the local position; the colour here is the pad's body."""
    n = 72; rows = [[k.vert((0, 0, 1.2), (.1, .26, .05, 0.)) for s_ in range(n)]]
    for r in range(1, 9):
        t = r / 8; row = []
        for s_ in range(n):
            a = .1 + (2 * math.pi - .2) * s_ / (n - 1); w = 1 + .018 * math.sin(a * 5 + 1) + .012 * math.sin(a * 11)
            z = 1.2 + .7 * math.sin(t * 7 + a * 3) * t * .5 + 5.2 * smooth(.88, 1., t) ** 1.5
            row.append(k.vert((50 * t * w * math.cos(a), 50 * t * w * math.sin(a), z), (*mix((.08, .24, .05), (.2, .38, .08), t * .7), 0.)))
        rows.append(row)
    k.grid(rows, False)


def lily_flower(k):
    def petal(u, j): return (*mix((1., .78, .86), (.95, .16, .5), smooth(.25, 1., u) * (.6 + .4 * abs(j))), .06)
    for layer, (n, l, w, rise) in enumerate(((9, 24, 11, .35), (8, 20, 10, .95), (6, 14, 8, 1.9))):
        for i in range(n):
            a = 2 * math.pi * (i + layer * .5) / n; blade(k, Vector((2 * math.cos(a), 2 * math.sin(a), 3 + layer * 1.2)), Vector((math.cos(a), math.sin(a), rise)), l, w, Vector((-math.sin(a), math.cos(a), 0)), petal, droop=-.12, rows=6)
    revolve(k, [(.2, 5, (1, .75, .15, 1.6)), (4, 6, (1, .75, .15, 1.6)), (3.4, 10, (1, .85, .25, 2.2)), (.2, 11, (1, .85, .25, 2.2))], 10)


def petals(k):
    rng = random.Random(9)
    for i in range(14):
        a = rng.uniform(0, 6.28); r = rng.uniform(0, 42); c = Vector((r * math.cos(a), r * math.sin(a), .6)); t = rng.uniform(0, 6.28)
        d = Vector((math.cos(t), math.sin(t), .03)); s = Vector((-math.sin(t), math.cos(t), 0))
        blade(k, c, d, rng.uniform(5, 8), rng.uniform(3.5, 5), s, lambda u, j: (*mix((1., .55, .75), (.85, .14, .45), u), .03), droop=-.05, rows=3)


def far_tree(k):
    """A distant tree: a dark trunk and three lumpy crowns, about 9 m tall. Vertex colour R is how lit it is, from under the crown to its top."""
    rng = random.Random(21)
    for c, r in ((Vector((0, 0, 620)), 380), (Vector((230, 90, 500)), 270), (Vector((-210, -120, 470)), 250)):
        rows = []
        for i in range(1, 5):
            th = math.pi * i / 5; row = []
            for s_ in range(7):
                a = 2 * math.pi * (s_ + i * .5) / 7; rr = r * rng.uniform(.82, 1.16)
                row.append(k.vert(c + Vector((rr * math.sin(th) * math.cos(a), rr * math.sin(th) * math.sin(a), rr * .78 * math.cos(th))), (lerp(1., .22, i / 4), 0, 0, 0)))
            rows.append(row)
        k.grid(rows, True)
        top = k.vert(c + Vector((0, 0, r * .8)), (1., 0, 0, 0)); bot = k.vert(c - Vector((0, 0, r * .7)), (.15, 0, 0, 0))
        for s_ in range(7):
            k.face((top, rows[0][(s_ + 1) % 7], rows[0][s_])); k.face((bot, rows[-1][s_], rows[-1][(s_ + 1) % 7]))
    tube(k, [Vector((0, 0, -30)), Vector((0, 0, 460))], [70, 42], 5, lambda p, t, a: (.1, 0, 0, 0), cap=False)


# ---------------------------------------------------------------- the ancient oaks
LEAF_RECTS = [(.016, .02, .145, .40), (.165, .02, .34, .39), (.52, .05, .65, .36), (.70, .02, .82, .41), (.02, .63, .18, .96), (.22, .58, .355, .96)]


def oak(seed, wisteria, deep=False):
    # deep (2026-11-20, the Deepwood): mossed to the crown, nearly twice the roots and heavier, some arching clear of the
    # ground, and moss hanging in curtains from the limbs (slot FeyHangMoss: U across a curtain, V down it).
    rng = random.Random(seed); k = Kit(['FeyBark', 'FeyLeaf', 'FeyPlant', 'FeyHangMoss'] if deep else ['FeyBark', 'FeyLeaf', 'FeyPlant']); col = Kit(['FeyBark'])
    def bark(p, t, a):
        m = clamp(.75 - p.z / (1500. if deep else 620.) + .3 * lumps(p.x * 3, p.y * 3, p.z * 2, seed) + .25 * math.cos(a) + (.35 if deep else 0.)); return (m, 0, 0, 0)
    lean = Vector((rng.uniform(-90, 90), rng.uniform(-90, 90), 0)); trunk = []
    for i in range(15):
        t = i / 14; twist = t * 3.1 + seed
        trunk.append(Vector((0, 0, -40 + 800 * t)) + lean * t * t + Vector((math.cos(twist), math.sin(twist), 0)) * 34 * math.sin(math.pi * t))
    radii = [lerp(165, 78, smooth(0, 1, i / 14)) * (1 + .22 * smooth(.3, 0, i / 14)) for i in range(15)]
    tube(k, trunk, radii, 24, bark, 0, .17, seed); tube(col, trunk[::2], radii[::2], 10, lambda p, t, a: (0, 0, 0, 0), 0, .1, seed)
    # Great roots running out over the ground.
    roots = 16 if deep else 9
    for i in range(roots):
        a = i * 2 * math.pi / roots + rng.uniform(-.2, .2); reach = rng.uniform(600, 1250) if deep else rng.uniform(430, 820); d = Vector((math.cos(a), math.sin(a), 0)); s = Vector((-d.y, d.x, 0)); wob = rng.uniform(.5, 1.5)
        arch = rng.uniform(40, 120) if deep and i % 3 == 0 else 0.   # (some run clear of the ground in a low arch)
        path = [d * lerp(95, reach, j / 11) + s * (70 * math.sin(j * .7 + i) * j / 11 * wob) + Vector((0, 0, lerp(190, -22, smooth(0, .75, j / 11)) + 24 * math.sin(j * 1.1 + i) + arch * math.sin(math.pi * j / 11))) for j in range(12)]
        rad = [lerp(66, 9, (j / 11) ** .8) * (1.35 if deep else 1.) for j in range(12)]
        tube(k, path, rad, 12, bark, 0, .16, seed + i); tube(col, path[::2], rad[::2], 6, lambda p, t, a: (0, 0, 0, 0), 0, .08, seed + i)
    # Limbs: long, low, twisting; branches off them; leaf clusters and wisteria at the ends.
    tips = []; hangs = []
    def limb(start, d, length, r, depth):
        n = 9 if depth == 0 else 6; p = start.copy(); path = [p.copy()]; dd = d.normalized()
        for j in range(n):
            dd = (dd + Vector((rng.uniform(-.28, .28), rng.uniform(-.28, .28), rng.uniform(-.2, .16) + (.05 if depth else -.03)))).normalized()
            p = p + dd * (length / n); path.append(p.copy())
        rad = [lerp(r, r * .42, j / n) for j in range(n + 1)]
        tube(k, path, rad, max(5, 12 - depth * 3), bark, 0, .12, seed + depth)
        if deep and depth < 2: hangs.extend(path[2:])
        if depth < 3:
            for j in range(2, n + 1, 1 if depth else 2):
                side = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-.25, .55))).normalized()
                limb(path[j], (dd * .6 + side).normalized(), length * rng.uniform(.42, .6), rad[j] * .62, depth + 1)
        else: tips.append((path[-1], dd))
    for i in range(6):
        a = i * 2 * math.pi / 6 + rng.uniform(-.35, .35); h = rng.randint(8, 13)
        limb(trunk[h], Vector((math.cos(a), math.sin(a), rng.uniform(.05, .5))), rng.uniform(520, 820), radii[h] * .52, 0)
    limb(trunk[-1], Vector((.2, -.1, 1)), 420, radii[-1] * .7, 1)
    for tip, d in tips:
        for n in range(rng.randint(15, 22)):
            c = tip + Vector((rng.gauss(0, 62), rng.gauss(0, 62), rng.gauss(6, 34))); a = rng.uniform(0, 6.28); ls = rng.uniform(34, 62)
            f = Vector((math.cos(a), math.sin(a), rng.uniform(-.7, .35))).normalized(); s = f.cross(Vector((rng.uniform(-.4, .4), rng.uniform(-.4, .4), 1))).normalized() * ls * .32
            u0, v0, u1, v1 = rng.choice(LEAF_RECTS); shade = rng.uniform(.65, 1.)
            q = [k.vert(c - s, (shade, 0, 0, 0)), k.vert(c + s, (shade, 0, 0, 0)), k.vert(c + s + f * ls, (shade, 0, 0, 0)), k.vert(c - s + f * ls, (shade, 0, 0, 0))]
            k.face(q, 1, [(u0, v1), (u1, v1), (u1, v0), (u0, v0)])
    # Moss hanging in curtains from the limbs.
    if deep:
        mi = slot_of(k, 'FeyHangMoss'); rng.shuffle(hangs)
        for p in hangs[:int(len(hangs) * .45)]:
            if p.z < 260: continue
            yaw = rng.uniform(0, math.pi); w = rng.uniform(30, 75); length = min(rng.uniform(90, 300), p.z - 60)
            ax = Vector((math.cos(yaw), math.sin(yaw), 0)) * w * .5; bend = Vector((rng.uniform(-12, 12), rng.uniform(-12, 12), 0))
            rows = []
            for r in range(5):
                v = r / 4; top = p + Vector((0, 0, -8)) + bend * v * v - Vector((0, 0, length * v))
                rows.append([k.vert(top - ax, (v, 0, 0, 0)), k.vert(top + ax, (v, 0, 0, 0))])
            for r in range(4): k.face((rows[r][0], rows[r][1], rows[r + 1][1], rows[r + 1][0]), mi, [(0, r / 4), (1, r / 4), (1, (r + 1) / 4), (0, (r + 1) / 4)])
    # Wisteria hanging from the lower ends, shelves up the trunk.
    if wisteria:
        low = sorted(tips, key=lambda x: x[0].z)[:int(len(tips) * .5)]; rng.shuffle(low)
        pl = Kit(['FeyPlant'])
        for tip, d in low[:wisteria]:
            for n in range(rng.randint(2, 4)): raceme(pl, tip + Vector((rng.uniform(-35, 35), rng.uniform(-35, 35), -6)), rng.uniform(70, 165), rng, Vector((rng.uniform(-8, 8), rng.uniform(-8, 8), 0)))
        base = len(k.v); k.v += pl.v; k.c += pl.c
        for f, uv in zip(pl.f, pl.uv): k.face(tuple(i + base for i in f), 2, uv)
    pl = Kit(['FeyPlant'])
    for i in range(9):
        j = rng.randint(1, 6); a = rng.uniform(0, 6.28); o = Vector((math.cos(a), math.sin(a), 0))
        bracket(pl, trunk[j] + o * radii[j] * .96, o, rng.uniform(26, 58), i)
    base = len(k.v); k.v += pl.v; k.c += pl.c
    for f, uv in zip(pl.f, pl.uv): k.face(tuple(i + base for i in f), 2, uv)
    return k, col


# ---------------------------------------------------------------- export
def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system = 'METRIC'; bpy.context.scene.unit_settings.scale_length = .01


def export(ob, name, source):
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
    ob.name = name; ob.data.name = name; file = OUT / (name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=str(file), use_selection=True, object_types={'MESH'}, apply_unit_scale=True, axis_forward='-Y', axis_up='Z',
                             bake_anim=False, mesh_smooth_type='FACE', add_leaf_bones=False, path_mode='STRIP', colors_type='LINEAR')
    ob.data.calc_loop_triangles()
    rec = dict(file=file.name, source=source, triangles=len(ob.data.loop_triangles), dimensions_cm=[round(float(d), 3) for d in ob.dimensions],
               slots=[m.name for m in ob.data.materials], sha256=hashlib.sha256(file.read_bytes()).hexdigest().upper())
    REPORT.append(rec); print('CHUCK_FEYWILD_KIT', rec)


GLOWS = {'FeyPlant', 'FeyFungus', 'FeyFungusBig', 'FeyBlade', 'FeyBladeRed', 'FeyPetal', 'FeyPad'}


def write(k, name):
    clear(); me = bpy.data.meshes.new(name); me.from_pydata(k.v, [], k.f); me.update()
    used = sorted(set(k.m)); remap = {old: new for new, old in enumerate(used)}
    for i in used: me.materials.append(bpy.data.materials.new(k.slots[i]))
    uv = me.uv_layers.new(name='UVMap'); at = 0
    for poly, slot, face_uv in zip(me.polygons, k.m, k.uv):
        poly.material_index = remap[slot]; poly.use_smooth = True
        for li, u in zip(poly.loop_indices, face_uv): uv.data[li].uv = u
    col = me.color_attributes.new('Col', 'FLOAT_COLOR', 'POINT')
    for i, c in enumerate(k.c): col.data[i].color = (c[0], c[1], c[2], min(c[3] / 10., 1.) if GLOWS & set(k.slots) else c[3])
    bm = bmesh.new(); bm.from_mesh(me); bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(ob); export(ob, name, 'modelled')


def wanted(name): return not KIT_ONLY or name in KIT_ONLY


def build(name, slots, fn, *args):
    if not wanted(name): return
    k = Kit(slots); fn(k, *args); write(k, name)


build('SM_FeyCapBlue', ['FeyFungus'], fungus_blue); build('SM_FeyCapAmber', ['FeyFungusBig'], fungus_amber); build('SM_FeyCapViolet', ['FeyFungus'], fungus_violet)
build('SM_FeyBellLamp', ['FeyPlant', 'FeyFungus', 'FeyBlade'], bell_lamp); build('SM_FeyFoxglove', ['FeyPlant', 'FeyBlade'], foxglove)
build('SM_FeyBroadLeaf', ['FeyPlant', 'FeyBlade'], leaf_plant, 4, 9, 62, 30, broad_col); build('SM_FeyCrimsonLeaf', ['FeyPlant', 'FeyBladeRed'], leaf_plant, 7, 8, 52, 24, crimson_col)
build('SM_FeyLilyPad', ['FeyPad'], lily_pad); build('SM_FeyLily', ['FeyPlant', 'FeyPetal'], lily_flower); build('SM_FeyPetals', ['FeyPetal'], petals); build('SM_FeyFarTree', ['FeyFar'], far_tree)
for index, (seed, hanging) in enumerate(((11, 46), (23, 0))):
    if wanted(f'SM_FeyOak{index}'):
        tree, collision = oak(seed, hanging); write(tree, f'SM_FeyOak{index}'); write(collision, f'SM_FeyOak{index}Collision')
for index, seed in enumerate((31, 47)):   # the Deepwood's (2026-11-20)
    if wanted(f'SM_FeyOakDeep{index}'):
        tree, collision = oak(seed, 0, True); write(tree, f'SM_FeyOakDeep{index}'); write(collision, f'SM_FeyOakDeep{index}Collision')


# ---------------------------------------------------------------- derived from the scans
def scan(asset, picks, ratio):
    for index, pick in enumerate(picks):
        bpy.ops.wm.open_mainfile(filepath=str(SOURCE / f'{asset}_2k.blend'))
        src = bpy.data.objects[pick]; ob = src.copy(); ob.data = src.data.copy(); ob.modifiers.clear(); bpy.context.collection.objects.link(ob)
        ob.hide_set(False); ob.hide_viewport = False; ob.location = (0, 0, 0)
        bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        if ratio < 1:
            d = ob.modifiers.new('Budget', 'DECIMATE'); d.ratio = ratio; bpy.ops.object.modifier_apply(modifier=d.name)
        z0 = min(v.co.z for v in ob.data.vertices)
        for v in ob.data.vertices: v.co = Vector((v.co.x * 100, v.co.y * 100, (v.co.z - z0) * 100))
        ob.data.update(); bpy.context.scene.unit_settings.system = 'METRIC'; bpy.context.scene.unit_settings.scale_length = .01
        export(ob, {'grass_medium_02': 'SM_FeyGrass', 'celandine_01': 'SM_FeyCelandine'}[asset] + str(index), asset)


def moss_clump():
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE / 'moss_01_2k.blend'))
    parts = [bpy.data.objects[f'moss_01_{x}_LOD1'] for x in 'abcdef']; rng = random.Random(4); bm = bmesh.new()
    for i in range(46):
        src = rng.choice(parts); r = 26 * math.sqrt(rng.random()); a = rng.uniform(0, 6.28); s = rng.uniform(90, 150)
        at = Matrix.Translation((r * math.cos(a), r * math.sin(a), 0)) @ Matrix.Rotation(rng.uniform(0, 6.28), 4, 'Z') @ Matrix.Scale(s, 4)
        off = Matrix.Translation(-Vector((src.location.x, src.location.y, 0))); tmp = src.data.copy(); tmp.transform(at @ Matrix.Translation((0, 0, 0))); bm.from_mesh(tmp); bpy.data.meshes.remove(tmp)
    me = bpy.data.meshes.new('SM_FeyMoss'); bm.to_mesh(me); bm.free(); me.materials.append(parts[0].data.materials[0])
    ob = bpy.data.objects.new('SM_FeyMoss', me); bpy.context.collection.objects.link(ob)
    bpy.context.scene.unit_settings.system = 'METRIC'; bpy.context.scene.unit_settings.scale_length = .01; export(ob, 'SM_FeyMoss', 'moss_01')


if not KIT_ONLY:
    scan('grass_medium_02', ['grass_medium_02_a', 'grass_medium_02_c'], .4)
    scan('celandine_01', ['celandine_01_a_LOD1', 'celandine_01_b_LOD1'], 1.)
    moss_clump()
else:
    fresh = {r['file'] for r in REPORT}
    REPORT = [r for r in json.loads((OUT / 'manifest.json').read_text(encoding='utf-8'))['assets'] if r['file'] not in fresh] + REPORT
(OUT / 'manifest.json').write_text(json.dumps({'tool': 'Blender 4.5.14', 'assets': REPORT}, indent=1) + '\n', encoding='utf-8')
print('CHUCK_FEYWILD_KIT_DONE', len(REPORT))
