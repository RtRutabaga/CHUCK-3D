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


def oak(seed, wisteria, deep=False, moss=None):
    # deep (2026-11-20, the Deepwood): mossed to the crown, nearly twice the roots and heavier, some arching clear of the
    # ground, and moss hanging in curtains from the limbs (slot FeyHangMoss: U across a curtain, V down it).
    # moss (2026-11-23, the wonder pass): a slot to hang the Deepwood's strands from an ordinary oak (FeyHangMossPurple).
    rng = random.Random(seed); k = Kit(['FeyBark', 'FeyLeaf', 'FeyPlant', moss or 'FeyHangMoss'] if deep or moss else ['FeyBark', 'FeyLeaf', 'FeyPlant']); col = Kit(['FeyBark'])
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
        if (deep or moss) and depth < 2: hangs.extend(path[2:])
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
    # Moss hanging from the limbs (2026-11-21, the user: the curtains read as straight-edged rectangles that shimmered).
    # Now beards of separate strands: each a thin ribbon crossed with another (so it has body from every side), tapering
    # from a few centimetres to a thread, wandering a little as it falls, of differing lengths; a few drape in a sag between
    # two points along the limb. Slot FeyHangMoss: U across a ribbon, V down it; vertex R how far down (for the sway).
    if deep or moss:
        mi = slot_of(k, moss or 'FeyHangMoss'); rng.shuffle(hangs)
        def strand(path, w0, w1, turn, level=False):
            n = len(path) - 1
            for cross in (0., math.pi / 2):
                rows = []
                for j, q in enumerate(path):
                    v = j / n; a = turn + cross + v * rng.uniform(-.6, .6); w = lerp(w0, w1, v ** .7)
                    side = (Vector((0, 0, 1)) if level and cross else Vector((math.cos(a), math.sin(a), 0))) * w * .5
                    rows.append((k.vert(q - side, (v, 0, 0, 0)), k.vert(q + side, (v, 0, 0, 0))))
                for j in range(n):
                    k.face((rows[j][0], rows[j][1], rows[j + 1][1], rows[j + 1][0]), mi, [(0, j / n), (1, j / n), (1, (j + 1) / n), (0, (j + 1) / n)])
        for p in hangs[:int(len(hangs) * .5)]:
            if p.z < 260: continue
            yaw = rng.uniform(0, math.pi); w = rng.uniform(30, 80); length = min(rng.uniform(80, 280), p.z - 60)
            ax = Vector((math.cos(yaw), math.sin(yaw), 0)); deep_ax = Vector((-ax.y, ax.x, 0)); drift = Vector((rng.uniform(-14, 14), rng.uniform(-14, 14), 0))
            for i in range(rng.randint(8, 14)):
                top = p + ax * rng.uniform(-w, w) * .5 + deep_ax * rng.uniform(-8, 8) + Vector((0, 0, rng.uniform(-10, -2)))
                ln = length * rng.uniform(.35, 1.); segs = 7; ph = rng.uniform(0, 6.3); amp = rng.uniform(2, 7)
                path = [top + drift * (j / segs) ** 2 + (ax * math.sin(ph + j * .9) + deep_ax * math.cos(ph * 1.3 + j * .7)) * amp * (j / segs) - Vector((0, 0, ln * j / segs)) for j in range(segs + 1)]
                strand(path, rng.uniform(3.5, 7.), rng.uniform(.6, 1.4), rng.uniform(0, math.pi))
            for i in range(rng.randint(1, 3)):   # drapes: a sag between two points along the limb
                a0 = rng.uniform(-w, 0) * .5; a1 = a0 + rng.uniform(25, 55); sag = rng.uniform(18, 55); segs = 8
                path = [p + ax * lerp(a0, a1, j / segs) + Vector((0, 0, -6 - sag * math.sin(math.pi * j / segs))) for j in range(segs + 1)]
                strand(path, rng.uniform(4., 7.), rng.uniform(3., 5.), yaw + math.pi / 2, True)
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


# The maze's hedges (2026-11-21, the user: make the hedges' leaves look better). A shell of leaf sprigs over one section of
# hedge, 300 cm along (X), 120 cm through (Y), 840 cm tall (Z), centred on its middle, to stand over the hedge's own box
# (which stays dark, the depth between the sprigs): sprigs rooted a little inside each face and leaning out of it, a ragged
# crown along the top, darker low down and deep in. Instanced along every hedge (FeyMap.cpp), stretched a little to fit.
def hedge_leaves(k):
    rng = random.Random(1213); hx, hy, hz = 150., 60., 420.
    def sprig(c, out, size, lean_up=0.):
        a = rng.uniform(0, 6.28); t = Vector((math.cos(a), math.sin(a), rng.uniform(-.6, .6)))
        t = (t - out * t.dot(out)).normalized()
        f = (out * rng.uniform(.35, .75) + t * rng.uniform(.6, 1.) + Vector((0, 0, lean_up - .15))).normalized()
        sv = f.cross(out); sv = (sv if sv.length > 1e-3 else f.cross(Vector((0, 0, 1)))).normalized() * size * .36
        root = c - out * rng.uniform(1, 5)
        depth = rng.uniform(0, 1); shade = rng.uniform(.7, 1.) * lerp(.68, 1., (c.z + hz) / (2 * hz)) * lerp(.8, 1., depth)
        u0, v0, u1, v1 = rng.choice(LEAF_RECTS)
        q = [k.vert(root - sv, (shade, 0, 0, 0)), k.vert(root + sv, (shade, 0, 0, 0)), k.vert(root + sv + f * size, (shade, 0, 0, 0)), k.vert(root - sv + f * size, (shade, 0, 0, 0))]
        k.face(q, 0, [(u0, v1), (u1, v1), (u1, v0), (u0, v0)])
    step = 13.   # (2026-11-21: small sprigs, close: a clipped hedge's small leaves, not a jungle's)
    for side in (-1, 1):   # the long faces
        out = Vector((0, side, 0)); nx = int(2 * hx / step); nz = int(2 * hz / step)
        for i in range(nx):
            for j in range(nz):
                c = Vector((-hx + (i + rng.random()) * step, side * hy, -hz + (j + rng.random()) * step))
                sprig(c, out, rng.uniform(12, 20))
    for side in (-1, 1):   # the ends (shown where a hedge stops at a way through)
        out = Vector((side, 0, 0))
        for i in range(int(2 * hy / step)):
            for j in range(int(2 * hz / step)):
                sprig(Vector((side * hx, -hy + (i + rng.random()) * step, -hz + (j + rng.random()) * step)), out, rng.uniform(12, 19))
    for i in range(int(2 * hx / 12.)):   # the top, and a ragged crown along its edges
        for j in range(int(2 * hy / 12.)):
            sprig(Vector((-hx + (i + rng.random()) * 12., -hy + (j + rng.random()) * 12., hz)), Vector((0, 0, 1)), rng.uniform(13, 21), .5)
        for side in (-1, 1):
            sprig(Vector((-hx + (i + rng.random()) * 12., side * hy * rng.uniform(.75, 1.), hz - rng.uniform(0, 10))), Vector((0, side * .6, .8)).normalized(), rng.uniform(16, 26), .6)


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


def write(k, name, recalc=True):
    clear(); me = bpy.data.meshes.new(name); me.from_pydata(k.v, [], k.f); me.update()
    used = sorted(set(k.m)); remap = {old: new for new, old in enumerate(used)}
    for i in used: me.materials.append(bpy.data.materials.new(k.slots[i]))
    uv = me.uv_layers.new(name='UVMap'); at = 0
    for poly, slot, face_uv in zip(me.polygons, k.m, k.uv):
        poly.material_index = remap[slot]; poly.use_smooth = True
        for li, u in zip(poly.loop_indices, face_uv): uv.data[li].uv = u
    col = me.color_attributes.new('Col', 'FLOAT_COLOR', 'POINT')
    for i, c in enumerate(k.c): col.data[i].color = (c[0], c[1], c[2], min(c[3] / 10., 1.) if GLOWS & set(k.slots) else c[3])
    if recalc: bm = bmesh.new(); bm.from_mesh(me); bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(me); bm.free()   # (the arches' stones are wound outward already)
    ob = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(ob); export(ob, name, 'modelled')


def wanted(name): return not KIT_ONLY or name in KIT_ONLY


def build(name, slots, fn, *args):
    if not wanted(name): return
    k = Kit(slots); fn(k, *args); write(k, name)


build('SM_FeyCapBlue', ['FeyFungus'], fungus_blue); build('SM_FeyCapAmber', ['FeyFungusBig'], fungus_amber); build('SM_FeyCapViolet', ['FeyFungus'], fungus_violet)
build('SM_FeyBellLamp', ['FeyPlant', 'FeyFungus', 'FeyBlade'], bell_lamp); build('SM_FeyFoxglove', ['FeyPlant', 'FeyBlade'], foxglove)
build('SM_FeyBroadLeaf', ['FeyPlant', 'FeyBlade'], leaf_plant, 4, 9, 62, 30, broad_col); build('SM_FeyCrimsonLeaf', ['FeyPlant', 'FeyBladeRed'], leaf_plant, 7, 8, 52, 24, crimson_col)
build('SM_FeyHedgeLeaves', ['FeyHedgeLeaf'], hedge_leaves)
build('SM_FeyLilyPad', ['FeyPad'], lily_pad); build('SM_FeyLily', ['FeyPlant', 'FeyPetal'], lily_flower); build('SM_FeyPetals', ['FeyPetal'], petals); build('SM_FeyFarTree', ['FeyFar'], far_tree)
for index, (seed, hanging) in enumerate(((11, 46), (23, 0))):
    if wanted(f'SM_FeyOak{index}'):
        tree, collision = oak(seed, hanging); write(tree, f'SM_FeyOak{index}'); write(collision, f'SM_FeyOak{index}Collision')
for index, seed in enumerate((31, 47)):   # the Deepwood's (2026-11-20)
    if wanted(f'SM_FeyOakDeep{index}'):
        tree, collision = oak(seed, 0, True); write(tree, f'SM_FeyOakDeep{index}'); write(collision, f'SM_FeyOakDeep{index}Collision')


# ---------------------------------------------------------------- the wonder pass (2026-11-23; docs/regions/FEYWILD.md#the-wonder-pass)
# The user: "the giant flying jelly fish and the unique giant plants and etc." A landmark per district (giant bell flowers,
# fly agarics, blue caps, a lily, flower spires, a great wisteria, oaks hung with purple moss), rounded archway ruins, floating
# islands with falls, and the jellyfish. Same vocabulary and materials as the kit above; new slots FeyStone (M_FeyStone),
# FeyFall (M_FeyFall), FeyJelly (M_FeyJelly) and FeyHangMossPurple (M_FeyHangMossPurple).
def merge(k, sub, slot):
    base = len(k.v); k.v += sub.v; k.c += sub.c
    for f, uv in zip(sub.f, sub.uv): k.face(tuple(i + base for i in f), slot, uv)


def bell_flower(k, base, yaw, height, reach, bell, stem_r, glow=1.):
    """bell_lamp at any size: an arching stalk ending in a hanging lit bell (bell: the mouth's radius)."""
    rot = Matrix.Rotation(yaw, 3, 'Z'); pts = []; b = bell / 30.
    for i in range(19):
        t = i / 18; a = t * 2.65
        pts.append(base + rot @ Vector((reach * (1 - math.cos(a)) * .62, 0, height * math.sin(min(a, math.pi / 2)) + (-44 * b * smooth(1.57, 2.65, a) if a > 1.57 else 0))))
    tube(k, pts, [lerp(stem_r, stem_r * .4, i / 18) for i in range(19)], 12, lambda p, t, a: ((.2, .3, .07) if t < .7 else (.46, .36, .1)) + (0,))
    tip = pts[-1]; down = (rot @ Vector((.18, 0, -1))).normalized(); pale = (1., .78, .25)
    pr = [(2.2 * b, 0, (*pale, .5 * glow)), (11 * b, -5 * b, (*pale, 1.1 * glow)), (16 * b, -18 * b, (*pale, 1.6 * glow)), (17 * b, -30 * b, (1., .8, .3, 2.1 * glow)),
          (23 * b, -40 * b, (1., .84, .36, 1.5 * glow)), (30 * b, -43 * b, (1., .7, .3, .9 * glow))]
    rm = down.to_track_quat('-Z', 'Y').to_matrix(); rows = []
    for ri, (r, z, c) in enumerate(pr):
        row = []
        for s in range(40):
            a = 2 * math.pi * s / 40; lobe = .5 + .5 * math.cos(a * 5)
            f = 1 + (ri >= 4) * (lobe - .5) * .36; zz = z - (ri == 5) * lobe * 7 * b
            row.append(k.vert(tip + rm @ Vector((r * f * math.cos(a), r * f * math.sin(a), zz)), c))
        rows.append(row)
    fungus = slot_of(k, 'FeyFungus'); ugrid(k, rows, lambda r: 3 + .98 * r / 5, fungus)
    heart = []
    for r, z in ((.5, 0), (7, -3), (8, -11), (5, -20), (.5, -24)):
        heart.append([k.vert(tip + rm @ (Vector((r * math.cos(a * math.pi / 6), r * math.sin(a * math.pi / 6), z - 14)) * b), (1, .9, .6, 9. * glow)) for a in range(12)])
    ugrid(k, heart, lambda r: 4.5, fungus)


def giant_bell(k):
    """The riverbank's and the Blooming Path's landmark: three great bell flowers arching from one clump, 3 to 5.6 m."""
    rng = random.Random(71)
    for yaw, h, reach, bell, r in ((0., 560, 210, 92, 9.), (2.3, 430, 160, 70, 7.), (4.3, 330, 120, 56, 6.)):
        bell_flower(k, Vector((math.cos(yaw) * 25, math.sin(yaw) * 25, -10)), yaw, h, reach, bell, r, .7)
    for i in range(9):
        a = i * 2.4 + rng.uniform(-.3, .3); d = Vector((math.cos(a), math.sin(a), rng.uniform(.25, .7))).normalized()
        blade(k, Vector((0, 0, 4 + i * 6)), d, rng.uniform(150, 230), rng.uniform(46, 64), Vector((-math.sin(a), math.cos(a), 0)),
              lambda u, j: (*mix((.1, .28, .05), (.36, .54, .1), abs(j) * .4 + u * .4), 0.), droop=rng.uniform(.3, .5), rows=14, wave=.015)


def wart(k, c, n, r, col, slot):
    t = n.normalized(); x, y = frame_along(t); rows = []
    for rr, h in ((1., 0.), (.92, .3), (.62, .62), (.05, .78)):
        rows.append([k.vert(c + (x * math.cos(a) + y * math.sin(a)) * r * rr + t * r * h, col) for a in [2 * math.pi * s / 9 for s in range(9)]])
    k.grid(rows, True, slot)


def toadstool(k, at, h, cap, seed, warts):
    """A fly agaric: a red cap flecked with white warts, cream gills and stem, a ring on the stem."""
    red, cream = (.6, .03, .018), (.95, .9, .78)
    lean = Vector((math.sin(seed) * h * .05, math.cos(seed) * h * .04, 0))
    mushroom(k, at, h, cap, red, cream, (.92, .88, .78), .02, .22, lean, seed, 36, 54)
    rng = random.Random(int(seed * 100)); tip = at + Vector((0, 0, h)) + lean; pl = slot_of(k, 'FeyPlant')
    for i in range(warts):
        th = rng.uniform(.15, 1.4); a = rng.uniform(0, 6.28); sz = cap * rng.uniform(.035, .075) * lerp(1., .6, th / 1.4)
        r = cap * math.cos(th) ** .72; z = cap * (.03 + .5 * math.sin(th) ** 1.15)
        n = Vector((math.cos(th) * math.cos(a), math.cos(th) * math.sin(a), math.sin(th) * 1.6)).normalized()
        wart(k, tip + Vector((r * math.cos(a), r * math.sin(a), z)) - n * sz * .3, n, sz, (.96, .93, .85, 0.), pl)
    # the skirt round the stem, under the cap
    ring = []
    for ri, (rf, dz) in enumerate(((.12, -.16), (.2, -.22), (.23, -.3))):
        ring.append([k.vert(at + lean * .8 + Vector((cap * rf * math.cos(a), cap * rf * math.sin(a), h + cap * dz - .05 * h)), (.93, .9, .82, 0.)) for a in [2 * math.pi * s / 24 + .3 * ri for s in range(24)]])
    k.grid(ring, True, pl)


def giant_toadstools(k):
    """Redcap Hollow's and the Crossroads' edges: a fly agaric 3.3 m tall with two smaller ones."""
    toadstool(k, Vector((0, 0, 0)), 330, 230, 1.1, 70); toadstool(k, Vector((260, 120, 0)), 160, 110, 2.6, 26); toadstool(k, Vector((-190, 170, 0)), 95, 64, 4.0, 14)


def giant_blue(k):
    """The Deepwood's landmark: a stand of tall glowing blue caps, the tallest 4.6 m, shelves up its stem."""
    for at, h, cap, seed in ((Vector((0, 0, 0)), 460, 175, .7), (Vector((210, -90, 0)), 330, 125, 2.1), (Vector((-150, -160, 0)), 250, 95, 3.3), (Vector((60, 200, 0)), 170, 70, 4.4)):
        mushroom(k, at, h, cap, (.05, .17, .55), (.2, .55, .95), (.26, .38, .52), .9, 1.8, Vector((math.sin(seed) * h * .06, math.cos(seed) * h * .05, 0)), seed, 36, 54)
    rng = random.Random(8); sub = Kit(['FeyPlant'])
    for i in range(7):
        a = rng.uniform(0, 6.28); z = rng.uniform(60, 330); o = Vector((math.cos(a), math.sin(a), 0))
        bracket(sub, Vector((math.sin(.7) * 460 * .06 * z / 460, math.cos(.7) * 460 * .05 * z / 460, z)) + o * 21, o, rng.uniform(18, 34), i, (.08, .12, .28), (.35, .7, 1.))
    merge(k, sub, slot_of(k, 'FeyPlant'))


def giant_lily(k):
    """The Lily Mere's landmark: a water lily 3 m across (it sits on a scaled lily pad, SM_FeyLilyPad)."""
    s = 5.5; ps = slot_of(k, 'FeyPetal'); pl = slot_of(k, 'FeyPlant')
    def petal(u, j): return (*mix((1., .84, .92), (.95, .2, .56), smooth(.25, 1., u) * (.6 + .4 * abs(j))), .12)
    for layer, (n, l, w, rise) in enumerate(((11, 26, 11, .28), (9, 21, 10, .85), (7, 15, 8, 1.7), (5, 9, 6, 3.))):
        for i in range(n):
            a = 2 * math.pi * (i + layer * .5) / n
            blade(k, Vector((2 * s * math.cos(a), 2 * s * math.sin(a), (2 + layer * 1.2) * s)), Vector((math.cos(a), math.sin(a), rise)), l * s, w * s,
                  Vector((-math.sin(a), math.cos(a), 0)), petal, droop=-.12, rows=12, slot=ps)
    revolve(k, [(.2 * s, 5 * s, (1, .75, .15, 3.)), (4 * s, 6 * s, (1, .75, .15, 3.)), (3.4 * s, 10 * s, (1, .85, .25, 4.)), (.2 * s, 11 * s, (1, .85, .25, 4.))], 20, slot=pl)
    rng = random.Random(4)
    for i in range(46):
        a = rng.uniform(0, 6.28); r = rng.uniform(3.4, 5.4) * s; top = Vector((r * 1.25 * math.cos(a), r * 1.25 * math.sin(a), rng.uniform(13, 15.5) * s))
        tube(k, [Vector((r * math.cos(a), r * math.sin(a), 7 * s)), top], [1.6, 1.1], 5, lambda p, t, b: (1., .82, .3, 2.5), pl)
        wart(k, top, top - Vector((0, 0, 8 * s)), 3.2, (1., .7, .18, 6.), pl)


def giant_spire(k):
    """The orchard's and the Needle Garden's landmark: three flower spires like great foxgloves, the tallest 5.4 m."""
    rng = random.Random(13)
    for at, s, hue in ((Vector((0, 0, 0)), 4.2, 0.), (Vector((150, 70, 0)), 3.1, .5), (Vector((-60, 160, 0)), 2.4, 1.)):
        pts = [at + Vector((math.sin(i * .45) * 2 * s, math.cos(i * .3) * s, i * 16 * s)) for i in range(9)]
        tube(k, pts, [lerp(1.9, .5, i / 8) * s for i in range(9)], 8, lambda p, t, a: (.18, .3, .07, 0.))
        n = int(30 * s * .8)
        deep, pale = mix((.55, .08, .62), (.85, .1, .5), hue), mix((.95, .78, 1.), (1., .78, .9), hue)
        for i in range(n):
            t = i / (n - 1); a = i * 2.4; z = lerp(52, 126, t) * s; fs = lerp(1., .35, t) * s * .85
            o = Vector((math.cos(a), math.sin(a), 0)); c = at + Vector((math.sin(z / s / 16 * .45) * 2 * s, math.cos(z / s / 16 * .3) * s, z))
            ax = (o + Vector((0, 0, -.65))).normalized()
            revolve(k, [(.8 * fs, 0, (*deep, 0.)), (3.4 * fs, 3 * fs, (*deep, 0.)), (4.8 * fs, 11 * fs, (*deep, .05)), (6.2 * fs, 17 * fs, (*pale, .4))], 12, c + o * 1.2 * s, ax)
        for i in range(7):
            a = i * 2.4 + hue; d = Vector((math.cos(a), math.sin(a), .55)).normalized()
            blade(k, at + Vector((0, 0, 3 + i * 4)), d, rng.uniform(36, 48) * s, 16 * s, Vector((-math.sin(a), math.cos(a), 0)), lambda u, j: (*mix((.1, .26, .05), (.3, .46, .1), abs(j) * .5), 0.), rows=14)


def stone_block(k, M, size, seed, slot, col):
    """A dressed stone: a box with rounded edges (its faces flat, so it shades and takes the stone's projection true), a
    little uneven. M places its centre and axes. Faces from a cube's six grids, finer toward the edges, welded."""
    h = Vector((size[0] / 2, size[1] / 2, size[2] / 2)); r = min(7., min(h) * .3); inner = h - Vector((r, r, r))
    us = (-1, -.985, -.94, -.86, -.6, -.2, .2, .6, .86, .94, .985, 1); n = len(us); index = {}
    centre = M @ Vector((0, 0, 0))
    def vert(p):
        q = Vector((max(-inner.x, min(inner.x, p.x)), max(-inner.y, min(inner.y, p.y)), max(-inner.z, min(inner.z, p.z))))
        d = p - q; p = q + (d.normalized() * r if d.length > 1e-6 else d)
        key = (round(p.x, 2), round(p.y, 2), round(p.z, 2))
        if key not in index:
            pn = p.normalized() if p.length > 1e-3 else Vector((0, 0, 1))
            index[key] = k.vert(M @ (p + pn * 1.6 * lumps(p.x * 4 + seed * 50, p.y * 4, p.z * 4, seed)), col)
        return index[key]
    for ax in range(3):
        for sgn in (-1, 1):
            a1, a2 = (ax + 1) % 3, (ax + 2) % 3; grid = []
            for i in range(n):
                row = []
                for j in range(n):
                    p = Vector((0, 0, 0)); p[ax] = sgn * h[ax]; p[a1] = us[i] * h[a1]; p[a2] = us[j] * h[a2]
                    row.append(vert(p))
                grid.append(row)
            for i in range(n - 1):
                for j in range(n - 1):
                    q = [grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]]
                    q = [v for x, v in enumerate(q) if v != q[x - 1]]   # (edges welded: collapsed corners dropped)
                    if len(set(q)) < 3: continue
                    pts = [Vector(k.v[v]) for v in q]; nrm = (pts[1] - pts[0]).cross(pts[2] - pts[0])
                    if nrm.length < 1e-9 and len(pts) > 3: nrm = (pts[2] - pts[0]).cross(pts[3] - pts[0])
                    if nrm.dot(sum(pts, Vector()) / len(pts) - centre) < 0: q.reverse()   # (outward, whatever the grid's winding)
                    k.face(tuple(q), slot)


def box(k, M, size):
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    v = [k.vert(M @ Vector((sx * hx, sy * hy, sz * hz)), (0, 0, 0, 0)) for sz in (-1, 1) for sy in (-1, 1) for sx in (-1, 1)]
    for q in ((0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)): k.face(tuple(v[i] for i in q))


def ivy_on(k, rng, path, leaves, slot):
    """A trailing ivy stem along path with leaves along it (on the blade material)."""
    tube(k, path, [lerp(1.6, .6, i / (len(path) - 1)) for i in range(len(path))], 5, lambda p, t, a: (.16, .14, .06, 0.), slot, cap=False)
    for i in range(leaves):
        t = rng.uniform(.05, 1.); j = min(int(t * (len(path) - 1)), len(path) - 2); f = t * (len(path) - 1) - j; c = path[j].lerp(path[j + 1], f)
        a = rng.uniform(0, 6.28); d = Vector((math.cos(a), math.sin(a), rng.uniform(-.6, .4))).normalized()
        blade(k, c, d, rng.uniform(8, 14), rng.uniform(7, 11), d.cross(Vector((0, 0, 1))).normalized() if abs(d.z) < .95 else Vector((1, 0, 0)),
              lambda u, j_: (*mix((.05, .16, .03), (.16, .3, .06), u * .5 + abs(j_) * .3), 0.), droop=.2, rows=4, slot=slot)


def arch(k, col, broken, seed):
    """A rounded archway of dressed stone, a ruin (4.6 m to its crown, 3.4 m clear between its piers), mossed on top by the
    stone material, ivy trailing from it and moss hanging under it. broken: one pier fallen to two courses, half the arch
    gone, its stones lying by it. col: the collision (a box per stone)."""
    rng = random.Random(seed); st = slot_of(k, 'FeyArchStone'); bl = slot_of(k, 'FeyBlade'); hm = slot_of(k, 'FeyHangMoss')
    Ri, Ro, z0, depth = 170., 245., 250., 74.
    def place(c, yaw_x=Vector((1, 0, 0)), up=Vector((0, 0, 1))):
        x = yaw_x.normalized(); z = up.normalized(); y = z.cross(x).normalized(); x = y.cross(z).normalized()
        M = Matrix.Identity(4)
        for r in range(3): M[r][0], M[r][1], M[r][2], M[r][3] = x[r], y[r], z[r], c[r]
        return M
    def put(M, size):
        stone_block(k, M, size, rng.uniform(0, 1), st, (rng.uniform(0, 1), 0, 0, 0)); box(col, M, (size[0] * .96, size[1] * .96, size[2] * .96))
    for side in (-1, 1):
        courses = 2 if broken and side > 0 else 4; x = side * (Ri + 42)
        put(place(Vector((x, 0, 16))), (104, depth + 18, 32))
        for i in range(courses):
            put(place(Vector((x + rng.uniform(-3, 3), rng.uniform(-2, 2), 32 + 27 + i * 55))), (82 + rng.uniform(-6, 6), depth, 52))
        if not (broken and side > 0): put(place(Vector((x, 0, z0 - 6))), (96, depth + 8, 18))   # the impost
    n = 13
    for i in range(n):
        if broken and i < 6: continue
        a0, a1 = math.pi * i / n, math.pi * (i + 1) / n; a = (a0 + a1) / 2; rad = Vector((math.cos(a), 0, math.sin(a)))
        c = Vector((0, 0, z0)) + rad * ((Ri + Ro) / 2) + Vector((0, rng.uniform(-2, 2), 0)); tang = Vector((-math.sin(a), 0, math.cos(a)))
        key = 1.18 if i == n // 2 else 1.
        put(place(c, tang, rad), ((Ri + Ro) / 2 * (a1 - a0) - 3.5, depth * (1.04 if key > 1 else 1.), (Ro - Ri) * key))
    if broken:   # the fallen stones, half sunk
        for i in range(4):
            c = Vector((rng.uniform(80, 420), rng.uniform(-120, 160), rng.uniform(4, 14))); yaw = rng.uniform(0, 6.28); tilt = rng.uniform(-.4, .4)
            put(place(c, Vector((math.cos(yaw), math.sin(yaw), tilt)), Vector((tilt * .5, 0, 1))), (48, depth, 78))
    # Ivy trailing from the arch's back down over its faces and the piers.
    top = [a for a in [math.pi * (i + .5) / 9 for i in range(9)] if not (broken and a < math.pi * 6 / 13)]
    for a in top:
        for face in (-1, 1):
            if rng.random() < .35: continue
            p0 = Vector((0, 0, z0)) + Vector((math.cos(a), 0, math.sin(a))) * (Ro + 3); path = [p0 + Vector((0, face * rng.uniform(0, 20), 4))]
            drop = rng.uniform(80, 260); wx = rng.uniform(-30, 30)
            for j in range(1, 9):
                t = j / 8; q = p0 + Vector((wx * t, face * (depth / 2 + 4), -drop * t ** 1.2 + 10 * math.sin(t * 7 + a)))
                path.append(q)
            ivy_on(k, rng, path, int(drop / 6), bl)
    # Moss hanging in a few beards under the arch.
    for i in range(14):
        a = rng.uniform(.15, .85) * math.pi
        if broken and a < math.pi * 6 / 13: continue
        p = Vector((0, 0, z0)) + Vector((math.cos(a), 0, math.sin(a))) * (Ri - 2) + Vector((0, rng.uniform(-depth / 2, depth / 2), 0)); ln = rng.uniform(25, 90)
        for cross in (0., math.pi / 2):
            rows = []; yaw = rng.uniform(0, math.pi) + cross
            for j in range(6):
                v = j / 5; w = lerp(rng.uniform(4, 7), .8, v ** .7); sd = Vector((math.cos(yaw), math.sin(yaw), 0)) * w * .5
                q = p + Vector((math.sin(j * .9 + i) * 2 * v, 0, -ln * v)); rows.append((k.vert(q - sd, (v, 0, 0, 0)), k.vert(q + sd, (v, 0, 0, 0))))
            for j in range(5): k.face((rows[j][0], rows[j][1], rows[j + 1][1], rows[j + 1][0]), hm, [(0, j / 5), (1, j / 5), (1, (j + 1) / 5), (0, (j + 1) / 5)])


def island(k, seed, R, D, trees, falls):
    """A floating island seen from afar: a grassy top, a craggy rock cone under it, roots trailing from it, a few trees (one
    a wisteria) and thin falls pouring off its rim."""
    rng = random.Random(seed); st = slot_of(k, 'FeyStone'); pl = slot_of(k, 'FeyPlant'); fa = slot_of(k, 'FeyFall'); N = 48
    edge = [R * (1 + .16 * math.sin(2 * math.pi * j / N * 3 + seed) + .09 * math.sin(2 * math.pi * j / N * 7 + seed * 2)) for j in range(N)]
    grass = lambda: (*mix((.1, .24, .045), (.2, .34, .07), rng.random()), 0.)
    rows = [[k.vert(Vector((0, 0, R * .07)), grass()) for j in range(N)]]
    for i in range(1, 7):
        t = i / 6; rows.append([k.vert(Vector((edge[j] * t * math.cos(2 * math.pi * j / N), edge[j] * t * math.sin(2 * math.pi * j / N), R * .07 * (1 - t * t) + 30 * lumps(j * 40, i * 90, 0, seed))), grass()) for j in range(N)])
    k.grid(rows, True, pl)
    under = [rows[-1]]
    for i in range(1, 13):
        t = i / 12; row = []
        for j in range(N):
            a = 2 * math.pi * j / N; r = edge[j] * (1 - t) ** 1.25 * (1 + .22 * lumps(j * 70, i * 110, seed * 9, seed)) + (40 if i == 1 else 0)
            row.append(k.vert(Vector((r * math.cos(a), r * math.sin(a), -D * t ** .9 - (60 if i == 1 else 0) + 90 * lumps(j * 50, i * 30, 7, seed))), (rng.random(), 0, 0, 0)))
        under.append(row)
    k.grid(under, True, st)
    for i in range(18):   # roots trailing from the rock
        a = rng.uniform(0, 6.28); r0 = R * rng.uniform(.25, .75); z0 = -D * (1 - r0 / R) ** 1.1 * .7; ln = rng.uniform(400, 1400)
        path = [Vector((r0 * math.cos(a), r0 * math.sin(a), z0)) + Vector((40 * math.sin(j + i), 40 * math.cos(j * .7 + i), -ln * j / 7)) for j in range(8)]
        tube(k, path, [lerp(22, 3, j / 7) for j in range(8)], 6, lambda p, t, b: (.11, .075, .045, 0.), pl)
    for i in range(trees):   # lumpy crowns (one a wisteria), as the far trees
        a = rng.uniform(0, 6.28); r = R * rng.uniform(0, .6); base = Vector((r * math.cos(a), r * math.sin(a), R * .06)); h = rng.uniform(500, 900)
        tube(k, [base, base + Vector((rng.uniform(-60, 60), rng.uniform(-60, 60), h))], [70, 30], 7, lambda p, t, b: (.13, .09, .06, 0.), pl)
        crown = (.42, .26, .62) if i == 0 else (.08, .2, .05)
        for c, cr in ((Vector((0, 0, h + 150)), 380), (Vector((220, 80, h + 40)), 280), (Vector((-200, -110, h)), 260)):
            ring = []
            for ii in range(1, 6):
                th = math.pi * ii / 6
                ring.append([k.vert(base + c + Vector((cr * rng.uniform(.85, 1.15) * math.sin(th) * math.cos(2 * math.pi * s / 9), cr * rng.uniform(.85, 1.15) * math.sin(th) * math.sin(2 * math.pi * s / 9), cr * .8 * math.cos(th))),
                                     (*mix(crown, (crown[0] * 1.6, crown[1] * 1.5, crown[2] * 1.4), 1 - ii / 5), 0.)) for s in range(9)])
            k.grid(ring, True, pl)
    for i in range(falls):   # thin falls off the rim
        j = rng.randrange(N); a = 2 * math.pi * j / N; c = Vector((edge[j] * math.cos(a), edge[j] * math.sin(a), 0)); out = Vector((math.cos(a), math.sin(a), 0)); side = Vector((-out.y, out.x, 0))
        L = rng.uniform(2600, 3600); grid = []
        for ii in range(13):
            t = ii / 12; w = lerp(70, 260, t); p = c + out * (40 + 260 * t ** .6) - Vector((0, 0, L * t))
            grid.append([k.vert(p + side * (w * (s / 2 - .5) * 2), (1., 0, 0, lerp(10., 0., t ** 1.4))) for s in range(3)])   # (alpha /10 on export: the island has a glowing slot)
        for ii in range(12):
            for s in range(2): k.face((grid[ii][s], grid[ii][s + 1], grid[ii + 1][s + 1], grid[ii + 1][s]), fa, [(s / 2, 1 - ii / 12), ((s + 1) / 2, 1 - ii / 12), ((s + 1) / 2, 1 - (ii + 1) / 12), (s / 2, 1 - (ii + 1) / 12)])


def jelly(k):
    """A giant flying jellyfish (the user: purple allowed), 12 m across its bell, tentacles to 26 m, drifting high over an
    open district. Vertex colour for M_FeyJelly (which moves it): R how far along a part (the bell 0 at its crown to .35 at its
    rim, an arm or tentacle 0 at its root to 1 at its tip), G a phase, B the part (0 the bell, .25 its underside, .6 an
    oral arm, 1 a tentacle), A unused."""
    R, H, N = 600., 380., 64; rng = random.Random(77)
    def bell(scale_r, scale_h, dz, part, flip):
        rows = []
        for i in range(15):
            t = i / 14; th = t * math.pi * .5 * 1.06; row = []
            for j in range(N):
                a = 2 * math.pi * j / N; lobe = .5 + .5 * math.cos(a * 16)
                r = R * scale_r * math.sin(min(th, math.pi / 2)) * (1 + .05 * smooth(.8, 1, t) * (lobe - .5) + .025 * math.sin(a * 3))
                z = H * scale_h * math.cos(th) + dz - smooth(.85, 1., t) * lobe * 30
                row.append(k.vert(Vector((r * math.cos(a), r * math.sin(a), z)), (t * .35, j / N, part, 0.)))
            rows.append(row)
        k.grid(list(reversed(rows)) if flip else rows, True, 0)
        return rows
    out = bell(1., 1., 0., 0., False); inn = bell(.95, .86, -8., .25, True)
    k.grid([out[-1], inn[-1]], True, 0)
    for arm in range(4):   # oral arms: ruffled ribbons from under the middle
        a0 = arm * math.pi / 2 + .4; L = rng.uniform(1300, 1700); rows = []
        for i in range(31):
            s = i / 30; c = Vector((60 * math.cos(a0) + 70 * s * math.cos(a0), 60 * math.sin(a0) + 70 * s * math.sin(a0), H * .25 - L * s))
            w = 150 * (1 - .6 * s); tw = a0 + s * 4.; side = Vector((math.cos(tw), math.sin(tw), 0)); nrm = Vector((-side.y, side.x, 0))
            ruf = lambda e: nrm * (math.sin(s * 46 + e * 3) * 34 * abs(e))
            rows.append([k.vert(c + side * (w * e) + ruf(e), (s, arm / 4, .6, 0.)) for e in (-1., -.5, 0., .5, 1.)])
        k.grid(rows, False, 0)
    for t in range(32):   # tentacles from the rim
        a = 2 * math.pi * (t + .5) / 32; L = rng.uniform(2000, 2700); path = []
        for i in range(30):
            s = i / 29; path.append(Vector(((R * .94 + 80 * s) * math.cos(a), (R * .94 + 80 * s) * math.sin(a), -30 - L * s)))
        rows = []
        for i, p in enumerate(path):
            s = i / 29; r = lerp(6., 1.2, s)
            rows.append([k.vert(p + Vector((r * math.cos(2 * math.pi * q / 4), r * math.sin(2 * math.pi * q / 4), 0)), (s, t / 32, 1., 0.)) for q in range(4)])
        k.grid(rows, True, 0)


build('SM_FeyGiantBell', ['FeyPlant', 'FeyFungus', 'FeyBlade'], giant_bell)
build('SM_FeyGiantToadstool', ['FeyFungusBig', 'FeyPlant'], giant_toadstools)
build('SM_FeyGiantBlue', ['FeyFungusBig', 'FeyPlant'], giant_blue)
build('SM_FeyGiantLily', ['FeyPlant', 'FeyPetal'], giant_lily)
build('SM_FeyGiantSpire', ['FeyPlant', 'FeyBlade'], giant_spire)
for index, broken in enumerate((False, True)):
    name = f'SM_FeyArch{index}'
    if wanted(name):
        kk = Kit(['FeyArchStone', 'FeyBlade', 'FeyHangMoss']); cc = Kit(['FeyArchStone']); arch(kk, cc, broken, 401 + index); write(kk, name, False); write(cc, name + 'Collision')
# (2026-11-24, the user: no trees on the floating islands) Grass, rock, roots and falls only.
for index, (seed, R, D, trees, falls) in enumerate(((501, 1500, 2300, 0, 2), (502, 900, 1500, 0, 1), (503, 600, 1100, 0, 1))):
    build(f'SM_FeyIsland{index}', ['FeyStone', 'FeyPlant', 'FeyFall'], island, seed, R, D, trees, falls)
build('SM_FeyJelly', ['FeyJelly'], jelly)
if wanted('SM_FeyOakWisteria'):
    tree, collision = oak(59, 160); write(tree, 'SM_FeyOakWisteria'); write(collision, 'SM_FeyOakWisteriaCollision')
if wanted('SM_FeyOakPurpleMoss'):
    tree, collision = oak(37, 10, moss='FeyHangMossPurple'); write(tree, 'SM_FeyOakPurpleMoss'); write(collision, 'SM_FeyOakPurpleMossCollision')


# ---------------------------------------------------------------- the hag's cottage (2026-11-23)
# The user: "build a Hag's cottage in the deep wood just off of the trail so that the player passes close to it". A crooked
# fairy-tale cottage: a footing of mossy stones, a dark timber frame filled with old daub (Poly Haven worn_mossy_plasterwall),
# a deep sagging thatch (Poly Haven thatch_roof_angled) with a bound ridge and rounded eaves, a leaning stone chimney on one
# gable, an arched plank door with iron straps, small leaded windows glowing amber, a stag's skull over the door, herbs and
# garlic hung under the eaves, a broom, a barrel and bucket, jars on a sill, pumpkins, a crooked stick fence round the front
# garden with its gate toward the path, and an iron cauldron on three stones (its fire and its smoke are the game's).
# Local frame: the front (the door) faces +X, the ridge runs along Y; the ground at z 0. FeyMap.cpp places it and adds the
# lights, the chimney's smoke, the cauldron's fire and the collision (constants there: keep in step).
HAG_W, HAG_D, HAG_H = 260., 210., 225.          # half width (Y), half depth (X), wall height
HAG_RIDGE, HAG_EAVE = 485., 208.                 # the roof's ridge and eave heights at the walls
HAG_CHIMNEY = Vector((-20., HAG_W + 48., 0.))    # the chimney stack's foot (outside the +Y gable)


def beam(k, a, b, w, h, slot, col=(0., 0., 0., 0.), seed=0., sag=0., up=None):
    """A squared timber from a to b, w wide and h deep, a little crooked; UV along its length in 1.5 m."""
    d = (b - a); L = d.length; d = d.normalized(); u = up or (Vector((0, 0, 1)) if abs(d.z) < .9 else Vector((1, 0, 0)))
    x = d.cross(u).normalized(); y = x.cross(d).normalized(); rows = []; n = max(2, int(L / 60))
    for i in range(n + 1):
        t = i / n; c = a + d * (L * t) - y * (sag * math.sin(math.pi * t)) + (x * math.sin(t * 5 + seed) + y * math.cos(t * 4 + seed)) * 1.2
        row = []
        for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            row.append(k.vert(c + x * (sx * w / 2 * (1 + .04 * math.sin(t * 9 + seed))) + y * (sy * h / 2), col))
        rows.append(row)
    for i in range(n):
        for s in range(4):
            j = (s + 1) % 4
            k.face((rows[i][s], rows[i][j], rows[i + 1][j], rows[i + 1][s]), slot, [(i * L / n / 150, s * .25), (i * L / n / 150, (s + 1) * .25), ((i + 1) * L / n / 150, (s + 1) * .25), ((i + 1) * L / n / 150, s * .25)])
    for end in (0, n):
        r = rows[end]; k.face(tuple(r) if end else tuple(reversed(r)), slot)


def panel(k, p0, along, up, length, height, out, slot, bulge=4., seed=0., holes=()):
    """A daub panel: a gently bulging, uneven sheet, UV in 1.5 m; holes (u0, v0, u1, v1 in cm) left open for windows/doors."""
    nu, nv = max(2, int(length / 25)), max(2, int(height / 25)); grid = []
    for i in range(nu + 1):
        row = []
        for j in range(nv + 1):
            u, v = length * i / nu, height * j / nv
            bump = bulge * math.sin(math.pi * i / nu) * math.sin(math.pi * j / nv) + 1.4 * lumps(u * 3 + seed * 40, v * 3, seed * 70, seed)
            row.append(k.vert(p0 + along * u + up * v + out * bump, (0., 0., 0., 0.)))
        grid.append(row)
    for i in range(nu):
        for j in range(nv):
            u0, v0, u1, v1 = length * i / nu, height * j / nv, length * (i + 1) / nu, height * (j + 1) / nv
            if any(h[0] <= (u0 + u1) / 2 <= h[2] and h[1] <= (v0 + v1) / 2 <= h[3] for h in holes): continue
            k.face((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]), slot, [(u0 / 150, v0 / 150), (u1 / 150, v0 / 150), (u1 / 150, v1 / 150), (u0 / 150, v1 / 150)])


def roof_z(x, y):
    """The thatch's top at (x, y): from the ridge down to the eaves, sagging in the middle and between the rafters."""
    t = min(abs(x) / (HAG_D + 75.), 1.); sag = 18. * math.cos(math.pi * y / (2 * (HAG_W + 55.))) * (1 - t)
    return HAG_RIDGE - sag - (HAG_RIDGE - HAG_EAVE + 18) * (t ** 1.08) + 6 * math.sin(math.pi * t) - 7 * math.sin(math.pi * t) ** 2 * math.cos(y / 70.)


def hag_cottage(k):
    rng = random.Random(1313)
    TH, WL, PK, ST, PL, BL, BK = (slot_of(k, n) for n in ('FeyHagThatch', 'FeyHagWall', 'FeyPlank', 'FeyArchStone', 'FeyPlant', 'FeyBlade', 'FeyBark'))
    Z = Vector((0, 0, 1))
    def M(c, x=Vector((1, 0, 0)), z=Vector((0, 0, 1))):
        x = x.normalized(); z = z.normalized(); y = z.cross(x).normalized(); x = y.cross(z).normalized(); m = Matrix.Identity(4)
        for r in range(3): m[r][0], m[r][1], m[r][2], m[r][3] = x[r], y[r], z[r], c[r]
        return m
    # The footing: a course of rounded mossy stones under the walls.
    for side, (a, b) in enumerate(((Vector((HAG_D, -HAG_W, 0)), Vector((HAG_D, HAG_W, 0))), (Vector((HAG_D, HAG_W, 0)), Vector((-HAG_D, HAG_W, 0))),
                                   (Vector((-HAG_D, HAG_W, 0)), Vector((-HAG_D, -HAG_W, 0))), (Vector((-HAG_D, -HAG_W, 0)), Vector((HAG_D, -HAG_W, 0))))):
        L = (b - a).length; d = (b - a).normalized(); u = 0.
        while u < L:
            s = rng.uniform(32, 48); c = a + d * (u + s / 2) + Vector((0, 0, rng.uniform(10, 16)))
            stone_block(k, M(c, d, Vector((rng.uniform(-.05, .05), rng.uniform(-.05, .05), 1))), (s, rng.uniform(34, 42), rng.uniform(26, 34)), rng.random(), ST, (rng.random(), 0, 0, 0))
            u += s + 1
    base = 30.
    # Walls: daub panels inside a dark timber frame, a little crooked.
    lean = Vector((4., -3., 0.))
    def wp(x, y, z): return Vector((x, y, z)) + lean * (z / HAG_H)
    door = (140., 0., 235., 182.)          # along the front from its -Y end: u0, v0, u1, v1
    win_f = (330., 70., 400., 135.)
    win_s = (150., 75., 215., 135.)        # on the -Y side
    walls = [(wp(HAG_D, -HAG_W, base), Vector((0, 1, 0)), 2 * HAG_W, Vector((1, 0, 0)), (door, win_f)),
             (wp(-HAG_D, HAG_W, base), Vector((0, -1, 0)), 2 * HAG_W, Vector((-1, 0, 0)), ()),
             (wp(-HAG_D, -HAG_W, base), Vector((1, 0, 0)), 2 * HAG_D, Vector((0, -1, 0)), (win_s,)),
             (wp(HAG_D, HAG_W, base), Vector((-1, 0, 0)), 2 * HAG_D, Vector((0, 1, 0)), ())]
    for p0, along, L, out, holes in walls:
        panel(k, p0 - out * 3, along, (Vector((0, 0, HAG_H - base)) + lean * ((HAG_H - base) / HAG_H)).normalized(), L, HAG_H - base - 6, out, WL, 4., rng.random(), holes)
    # the gable triangles over the side walls (+Y and -Y ends), daub with a timber king post
    for gy, sgn in ((HAG_W, 1.), (-HAG_W, -1.)):
        grid = []; nu, nv = 18, 12
        for i in range(nu + 1):
            row = []
            for j in range(nv + 1):
                x = -HAG_D + 2 * HAG_D * i / nu; top = roof_z(x, gy) - 26; zz = HAG_H + (top - HAG_H) * j / nv
                row.append(k.vert(wp(x, gy + sgn * (-3 + 2 * math.sin(math.pi * j / nv) * math.sin(math.pi * i / nu)), zz), (0., 0., 0., 0.)))
            grid.append(row)
        for i in range(nu):
            for j in range(nv):
                q = (grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]); x0 = 2 * HAG_D * i / nu
                k.face(q if sgn > 0 else tuple(reversed(q)), WL, [(x0 / 150, j / nv), ((x0 + 2 * HAG_D / nu) / 150, j / nv), ((x0 + 2 * HAG_D / nu) / 150, (j + 1) / nv), (x0 / 150, (j + 1) / nv)])
        beam(k, wp(0, gy + sgn * 4, HAG_H), wp(0, gy + sgn * 4, roof_z(0, gy) - 20), 16, 14, PK, seed=gy)
        for sx in (-1, 1): beam(k, wp(sx * HAG_D * .95, gy + sgn * 4, HAG_H + 4), wp(0, gy + sgn * 4, roof_z(0, gy) - 30), 14, 12, PK, seed=sx + gy)
    # the frame: corner and middle posts, sill and top plates, braces
    for x in (-HAG_D, HAG_D):
        for y in (-HAG_W, -HAG_W * .3, HAG_W * .3, HAG_W):
            beam(k, wp(x, y, base - 4), wp(x, y, HAG_H + 6), 20, 20, PK, seed=x + y)
    for y in (-HAG_W, HAG_W):
        beam(k, wp(0, y, base - 4), wp(0, y, HAG_H + 6), 18, 18, PK, seed=y * 2)
    for z in (base + 2, HAG_H - 2):
        for (a, b) in ((( HAG_D, -HAG_W - 12), (HAG_D, HAG_W + 12)), ((-HAG_D, -HAG_W - 12), (-HAG_D, HAG_W + 12)), ((-HAG_D - 12, -HAG_W), (HAG_D + 12, -HAG_W)), ((-HAG_D - 12, HAG_W), (HAG_D + 12, HAG_W))):
            beam(k, wp(a[0], a[1], z), wp(b[0], b[1], z), 18, 16, PK, seed=z + a[0], sag=3)
    for x, y0, y1 in ((-HAG_D, -HAG_W * .3, HAG_W * .3), (-HAG_D, HAG_W * .3, HAG_W)):
        beam(k, wp(x - 1, y0, base + 10), wp(x - 1, y1, HAG_H - 12), 12, 10, PK, seed=y0)
    for y, x0, x1 in ((-HAG_W, -HAG_D, 0.), (HAG_W, 0., HAG_D)):
        beam(k, wp(x0, y + (-1 if y < 0 else 1), base + 10), wp(x1, y + (-1 if y < 0 else 1), HAG_H - 12), 12, 10, PK, seed=x0 + 9)
    # The door: arched, of planks, iron straps and a ring; its frame.
    fx = HAG_D + 3; dy0 = -HAG_W + door[0]; dy1 = -HAG_W + door[2]; dz0 = base + door[1]; dw = dy1 - dy0; dh = door[3] - door[1]
    for i in range(5):
        y0 = dy0 + dw * i / 5 + 1; y1 = dy0 + dw * (i + 1) / 5 - 1; ym = (y0 + y1) / 2; top = dz0 + dh - 42 + 42 * math.sqrt(max(0., 1 - ((ym - (dy0 + dy1) / 2) / (dw / 2)) ** 2))
        beam(k, wp(fx, ym, dz0), wp(fx, ym, top), y1 - y0, 5, PK, (rng.uniform(0., .2), 0, 0, 0), seed=i * 3.1)
    iron = (.035, .032, .03, 0.)
    for z in (dz0 + 35, dz0 + 120):
        beam(k, wp(fx + 3.5, dy0 + 4, z), wp(fx + 3.5, dy1 - 18, z), 5, 1.2, PL, iron, seed=z)
    ring = [wp(fx + 5, dy1 - 22 + 6 * math.cos(a), dz0 + 92 - 6 * math.sin(a) - 6) for a in [i * math.pi / 6 for i in range(13)]]
    tube(k, ring, [1.] * 13, 6, lambda p, t, a: iron, PL, cap=False)
    for i in range(13):   # the arch of the frame
        a = math.pi * i / 12; arch = wp(fx + 2, (dy0 + dy1) / 2 - math.cos(a) * (dw / 2 + 6), dz0 + dh - 42 + math.sin(a) * 48)
        if i: beam(k, prev, arch, 12, 10, PK, seed=i)
        prev = arch
    for y in (dy0 - 6, dy1 + 6): beam(k, wp(fx + 2, y, dz0 - 2), wp(fx + 2, y, dz0 + dh - 42), 12, 10, PK, seed=y)
    beam(k, wp(fx + 10, dy0 - 15, dz0 - 4), wp(fx + 10, dy1 + 15, dz0 - 4), 22, 8, PK, seed=7.7)   # the step: a slab of timber
    # Windows: a frame with a cross, leaded panes glowing amber, a shutter hanging askew.
    def window(c, along, out, w, h, shutter):
        corners = [c - along * w / 2, c + along * w / 2, c + along * w / 2 + Z * h, c - along * w / 2 + Z * h]
        for i in range(4): beam(k, corners[i] + out * 3, corners[(i + 1) % 4] + out * 3, 9, 8, PK, seed=i + c.x)
        beam(k, c + out * 3, c + Z * h + out * 3, 4, 5, PK, seed=c.y); beam(k, c - along * w / 2 + Z * h / 2 + out * 3, c + along * w / 2 + Z * h / 2 + out * 3, 4, 5, PK, seed=c.z)
        nu, nv = 6, 6; g = [[k.vert(c - along * w / 2 + along * (w * i / nu) + Z * (h * j / nv) - out * 2, (1., .58, .22, 3.2 + 1.2 * ((i + j) % 2))) for j in range(nv + 1)] for i in range(nu + 1)]
        for i in range(nu):
            for j in range(nv): k.face((g[i][j], g[i + 1][j], g[i + 1][j + 1], g[i][j + 1]), PL)
        for i in range(1, nu):   # the leading: thin dark diamonds
            beam(k, c - along * w / 2 + along * (w * i / nu) + out * .5, c - along * w / 2 + along * (w * i / nu) + Z * h + out * .5, 1., 1., PL, iron)
        if shutter:
            hinge = c + along * (w / 2 + 6) + out * 6
            for i in range(4):
                a0 = hinge + along * (i * w / 4 * .95) * .95 + out * (i * 9)
                beam(k, a0 + Z * (8 - i * 4), a0 + Z * (h - 2 - i * 4), w / 4 - 2, 3, PK, (.3, 0, 0, 0), seed=i * 1.7)
    window(wp(HAG_D, -HAG_W + (win_f[0] + win_f[2]) / 2, base + win_f[1]), Vector((0, 1, 0)), Vector((1, 0, 0)), win_f[2] - win_f[0], win_f[3] - win_f[1], True)
    window(wp(HAG_D - (win_s[0] + win_s[2]) / 2 + 0., -HAG_W, base + win_s[1]), Vector((-1, 0, 0)), Vector((0, -1, 0)), win_s[2] - win_s[0], win_s[3] - win_s[1], False)
    # The roof: deep thatch over the whole, eaves rolled under, a bound ridge.
    Y0, Y1 = -HAG_W - 62., HAG_W + 62.; X1 = HAG_D + 78.; nu, nv = 30, 16; thick = 34.
    for sx in (-1., 1.):
        top = []; bot = []
        for i in range(nu + 1):
            y = Y0 + (Y1 - Y0) * i / nu; trow = []; brow = []
            for j in range(nv + 1):
                t = j / nv; x = sx * X1 * t; z = roof_z(x, y) + 2.5 * lumps(y * 2, x * 2, 5, 3)
                trow.append(k.vert(Vector((x, y, z)), (0., .5 * smooth(.55, 1., t) * (.6 + .4 * math.sin(y / 60. + sx)), 0., 0.)))
                brow.append(k.vert(Vector((x * .97, y, z - thick)), (.6, 0., 0., 0.)))
            top.append(trow); bot.append(brow)
        for i in range(nu):
            for j in range(nv):
                v0, v1 = j / nv * X1 * 1.35 / 150, (j + 1) / nv * X1 * 1.35 / 150; u0, u1 = (Y0 + (Y1 - Y0) * i / nu) / 150, (Y0 + (Y1 - Y0) * (i + 1) / nu) / 150
                q = (top[i][j], top[i + 1][j], top[i + 1][j + 1], top[i][j + 1])
                k.face(q if sx > 0 else tuple(reversed(q)), TH, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)] if sx > 0 else [(u0, v1), (u1, v1), (u1, v0), (u0, v0)])
                q = (bot[i][j], bot[i][j + 1], bot[i + 1][j + 1], bot[i + 1][j])
                k.face(q if sx > 0 else tuple(reversed(q)), TH, [(u0, v0), (u0, v1), (u1, v1), (u1, v0)])
        # the eave's rolled edge, and the verges at each gable
        for i in range(nu):
            q = (top[i][nv], top[i + 1][nv], bot[i + 1][nv], bot[i][nv]); u0, u1 = (Y0 + (Y1 - Y0) * i / nu) / 150, (Y0 + (Y1 - Y0) * (i + 1) / nu) / 150
            k.face(q if sx > 0 else tuple(reversed(q)), TH, [(u0, 0), (u1, 0), (u1, .25), (u0, .25)])
        for i, sgn in ((0, -1), (nu, 1)):
            for j in range(nv):
                q = (top[i][j], top[i][j + 1], bot[i][j + 1], bot[i][j])
                k.face(q if sx * sgn < 0 else tuple(reversed(q)), TH, [(0, j / nv), (0, (j + 1) / nv), (.25, (j + 1) / nv), (.25, j / nv)])
    ridge = [Vector((0, Y0 - 6 + (Y1 - Y0 + 12) * i / 24, roof_z(0, Y0 + (Y1 - Y0) * i / 24) + 6)) for i in range(25)]
    tube(k, ridge, [24.] * 25, 12, lambda p, t, a: (0., .3, 0., 0.), TH)
    for i in range(12):   # the ridge's binding: crossed hazel spars
        y = Y0 + 20 + (Y1 - Y0 - 40) * i / 11; zr = roof_z(0, y) + 6
        for sgn in (-1, 1): beam(k, Vector((-30, y - sgn * 14, zr + 2)), Vector((30, y + sgn * 14, zr + 2)), 2.2, 2.2, PK, (.2, 0, 0, 0), seed=i, up=Vector((0, 0, 1)))
    # The chimney: a leaning stack of stones on the +Y gable, a cap stone; the smoke is the game's.
    c0 = HAG_CHIMNEY; courses = 17
    for i in range(courses):
        t = i / (courses - 1); w = lerp(78, 56, t); d = lerp(66, 50, t); z = 14 + i * 34 + (40 if i == courses - 1 else 0)
        c = c0 + Vector((0, 0, z)) + Vector((6 * t * t, 10 * t * t, 0))
        n = 2 if w > 64 else 2
        for j in range(n):
            ox = (j - (n - 1) / 2) * w / n + rng.uniform(-3, 3)
            stone_block(k, M(c + Vector((0, ox * .0, 0)) + Vector((0, 0, 0)) + Vector((rng.uniform(-2, 2), 0, 0)) + Vector((0, (j - .5) * d / 2 if i % 2 else 0, 0)), Vector((1, 0, rng.uniform(-.03, .03))), Vector((rng.uniform(-.04, .04), rng.uniform(-.04, .04), 1))),
                        (w / (1 if i % 2 == 0 else 1) + rng.uniform(-4, 4), d / (1 if i % 2 == 0 else 2) + rng.uniform(-3, 3), rng.uniform(28, 34)), rng.random(), ST, (rng.random(), 0, 0, 0))
            if i % 2 == 0: break
    top_c = c0 + Vector((6, 10, 14 + (courses - 1) * 34 + 40 + 24))
    stone_block(k, M(top_c, Vector((1, 0, 0))), (88, 76, 12), .3, ST, (.4, 0, 0, 0))
    # A stag's skull over the door, antlers spread.
    sk = wp(HAG_D + 9, (dy0 + dy1) / 2, dz0 + dh + 2); bone = (.62, .58, .5, 0.)
    revolve(k, [(.5, 0, bone), (11, 3, bone), (13, 12, bone), (10, 22, bone), (6, 30, bone), (2, 34, bone)], 12, sk + Vector((0, 0, -26)), Vector((.25, 0, -1)).normalized(), PL)
    for sgn in (-1, 1):
        path = [sk + Vector((0, sgn * 8, 6))]; d = Vector((0, sgn, .9)).normalized(); p = path[0]
        for j in range(9):
            d = (d + Vector((0, sgn * .1, -.07 if j > 4 else .02))).normalized(); p = p + d * 7.5; path.append(p)
        tube(k, path, [lerp(3., 1., j / 9) for j in range(10)], 6, lambda p_, t, a: bone, PL)
        for j in (3, 5, 7):
            q = path[j]; tine = [q, q + Vector((4, sgn * 3, 11)), q + Vector((7, sgn * 4, 19))]
            tube(k, tine, [1.6, 1.1, .6], 5, lambda p_, t, a: bone, PL)
    for sgn in (-1, 1): revolve(k, [(.2, 0, (0, 0, 0, 0)), (3, 1.5, (0., 0., 0., 0.)), (.2, 3, (0, 0, 0, 0))], 8, sk + Vector((6, sgn * 6, -8)), Vector((1, 0, 0)), PL)   # its eye holes
    # Herbs, garlic and dried flowers hung from the front eave; a lantern by the door.
    for i in range(7):
        y = -HAG_W + 30 + i * 70 + rng.uniform(-10, 10)
        if abs(y - (dy0 + dy1) / 2) < 60: continue
        top = Vector((HAG_D + 40, y, roof_z(HAG_D + 40, y) - thick - 2)); ln = rng.uniform(35, 60)
        tube(k, [top, top - Vector((0, 0, 10))], [.5, .5], 4, lambda p_, t, a: (.25, .2, .1, 0.), PL, cap=False)
        hue = rng.choice(((.32, .36, .12), (.42, .3, .1), (.36, .22, .3), (.5, .44, .2)))
        for j in range(14):
            a = rng.uniform(0, 6.28); d = Vector((math.cos(a) * .25, math.sin(a) * .25, -1)).normalized()
            blade(k, top - Vector((0, 0, 10)), d, ln * rng.uniform(.7, 1.), rng.uniform(4, 7), Vector((-math.sin(a), math.cos(a), 0)), lambda u, j_: (*mix(hue, (hue[0] * .6, hue[1] * .5, hue[2] * .4), u), 0.), droop=-.05, rows=4, slot=BL)
    lan = wp(HAG_D + 34, dy1 + 34, dz0 + 150)
    tube(k, [lan + Vector((-26, 0, 30)), lan + Vector((0, 0, 30)), lan + Vector((0, 0, 18))], [1.4, 1.4, 1.], 5, lambda p_, t, a: iron, PL, cap=False)
    revolve(k, [(.5, 18, iron), (9, 13, iron), (9.5, 11, iron), (8, 10, (1., .62, .25, 6.)), (8, -6, (1., .62, .25, 6.)), (9, -7, iron), (6, -11, iron), (.5, -12, iron)], 8, lan, None, PL)
    # A broom leaning by the door, a barrel with a bucket on it, jars on the window sill, pumpkins.
    bb = wp(HAG_D + 18, dy0 - 30, 0)
    beam(k, bb + Vector((14, 0, 0)), bb + Vector((-4, 8, 150)), 3, 3, PK, (.1, 0, 0, 0), seed=3)
    for j in range(40):
        a = rng.uniform(0, 6.28); r = rng.uniform(0, 7)
        blade(k, bb + Vector((14, 0, 32)) + Vector((r * math.cos(a), r * math.sin(a), 0)), Vector((math.cos(a) * .25 + .1, math.sin(a) * .25, -1)), rng.uniform(26, 34), 2., Vector((-math.sin(a), math.cos(a), 0)), lambda u, j_: (.52, .4, .18, 0.), droop=0., rows=2, slot=BL)
    bar = wp(HAG_D + 40, HAG_W - 45, 0)
    prof = [(.5, 0, (0, 0, 0, 0))] + [(lerp(30, 36, math.sin(math.pi * t)), t * 85, (0, 0, 0, 0)) for t in [i / 8 for i in range(9)]] + [(.5, 85, (0, 0, 0, 0))]
    revolve(k, prof, 18, bar, None, PK)
    for z in (8, 42, 77): revolve(k, [(lerp(30, 36, math.sin(math.pi * z / 85)) + 1, z - 2.5, iron), (lerp(30, 36, math.sin(math.pi * z / 85)) + 1.4, z, iron), (lerp(30, 36, math.sin(math.pi * z / 85)) + 1, z + 2.5, iron)], 18, bar, None, PL)
    revolve(k, [(.5, 0, (0, 0, 0, 0)), (14, 0, (0, 0, 0, 0)), (17, 26, (0, 0, 0, 0)), (16, 27, (0, 0, 0, 0)), (.5, 3, (0, 0, 0, 0))], 14, bar + Vector((4, -6, 85)), None, PK)
    sill = wp(HAG_D + 9, -HAG_W + (win_f[0] + win_f[2]) / 2, base + win_f[1] - 6)
    beam(k, sill + Vector((0, -45, 0)), sill + Vector((0, 45, 0)), 16, 5, PK, seed=4.4)
    for j, (col, h) in enumerate((((.18, .35, .2), 22), ((.45, .2, .1), 15), ((.25, .2, .4), 18), ((.6, .55, .3), 11))):
        jc = sill + Vector((0, -32 + j * 21, 2.5))
        revolve(k, [(.5, 0, (*col, 0.)), (5, 0, (*col, 0.)), (6, h * .6, (*col, .3)), (3, h * .85, (*col, 0.)), (3.5, h, (.3, .22, .12, 0.)), (.5, h + 1, (.3, .22, .12, 0.))], 10, jc, None, PL)
    for j in range(5):
        a = rng.uniform(-.6, .6); r = rng.uniform(110, 190); pc = Vector((HAG_D + 60 + r * math.cos(a) * .6, HAG_W * .6 + r * math.sin(a), 0)); s = rng.uniform(.7, 1.3)
        rows = []
        for i in range(9):
            th = math.pi * i / 8; row = []
            for q in range(16):
                a2 = 2 * math.pi * q / 16; rib = 1 + .1 * abs(math.cos(a2 * 4))
                row.append(k.vert(pc + Vector((22 * s * rib * math.sin(th) * math.cos(a2), 22 * s * rib * math.sin(th) * math.sin(a2), 15 * s * (1 - math.cos(th)) - 1)), (.75, .3, .05, 0.)))
            rows.append(row)
        k.grid(rows, True, PL)
        tube(k, [pc + Vector((0, 0, 29 * s)), pc + Vector((2, 1, 36 * s)), pc + Vector((5, 1, 38 * s))], [2.2, 1.6, 1.], 5, lambda p_, t, a: (.25, .3, .1, 0.), PL)
    # The cauldron: iron, on three stones, its brew glowing a sickly green (the fire under it is the game's).
    cc = Vector((HAG_D + 230, -HAG_W + 40, 0))
    for i in range(3):
        a = i * 2.09 + .3; stone_block(k, M(cc + Vector((44 * math.cos(a), 44 * math.sin(a), 21)), Vector((math.cos(a), math.sin(a), 0))), (32, 28, 44), i * .3, ST, (rng.random(), 0, 0, 0))
    pot = [(.5, 22, iron), (24, 24, iron), (42, 42, iron), (46, 62, iron), (40, 80, iron), (36, 86, iron), (38, 90, iron), (35, 90, iron), (34, 84, iron)]
    cc = cc + Vector((0, 0, 26))   # (up on its stones, room for the fire under it)
    revolve(k, pot, 28, cc, None, PL)
    revolve(k, [(33, 82, (.25, .85, .2, 3.)), (.5, 83.5, (.4, 1., .3, 4.))], 28, cc, None, PL)
    for a in (0., math.pi):
        ear = [cc + Vector((46 * math.cos(a), 46 * math.sin(a), 82)), cc + Vector((52 * math.cos(a), 52 * math.sin(a), 92)), cc + Vector((46 * math.cos(a), 46 * math.sin(a), 98))]
        tube(k, ear, [2.4, 2.4, 2.4], 6, lambda p_, t, a_: iron, PL)
    # A crooked stick fence round the front garden, its gate gap toward +X.
    gc = Vector((HAG_D + 20, 0, 0)); R = 470.
    a = -1.75
    while a < 1.75:
        if abs(a) < .22: a += .2; continue
        p = gc + Vector((R * math.cos(a) * .72, R * math.sin(a), 0)); h = rng.uniform(85, 115)
        beam(k, p + Vector((0, 0, -10)), p + Vector((rng.uniform(-6, 6), rng.uniform(-6, 6), h)), 7, 7, BK if False else PK, (.4, 0, 0, 0), seed=a * 10)
        a2 = a + .2
        if not (abs(a2) < .22) and a2 < 1.75:
            q = gc + Vector((R * math.cos(a2) * .72, R * math.sin(a2), 0))
            for z in (38, 78): beam(k, p + Vector((0, 0, z + rng.uniform(-5, 5))), q + Vector((0, 0, z + rng.uniform(-5, 5))), 4, 4, PK, (.3, 0, 0, 0), seed=a + z, sag=4)
        a += .2


build('SM_FeyHagCottage', ['FeyHagThatch', 'FeyHagWall', 'FeyPlank', 'FeyArchStone', 'FeyPlant', 'FeyBlade', 'FeyBark'], hag_cottage)


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
