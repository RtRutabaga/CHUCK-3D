"""Blender 4.5.14: the beholder of the temple sanctum (Claude, 2026-10-23; docs/CHULT-TEMPLE-BEHOLDER.md).

User: "start creating the beholder, it should look really cool and detailed". The 2D game's
Phase 6 final chamber: the adventurers battle a beholder (npcs/beholder.png: a mauve orb, one
great red-irised eye, a toothed grin, eyes on stalks). Built here, no downloads.

blender --background --factory-startup --python Tools/build_beholder.py [-- --review DIR] [--quick]
  -> SourceAssets/Enemies/Beholder/SK_Beholder.fbx       skinned, no clips (ATempleBeholder poses it)
     SourceAssets/Enemies/Beholder/T_Beholder_D.png      skin colour (baked)
     SourceAssets/Enemies/Beholder/T_Beholder_N.png      skin normal (baked from the sculpt-detail mesh)
     SourceAssets/Enemies/Beholder/T_Beholder_ORM.png    R ambient occlusion, G roughness, B wrinkle mask
     SourceAssets/Enemies/Beholder/T_BeholderEye.png     iris, pupil and bloodshot sclera (polar UVs)
     SourceAssets/Enemies/Beholder/beholder.json         sizes, bones and where the eyes are

How it is made. The creature is a signed distance field: an ellipsoid body with low lumps, a
fleshy socket ring with a heavy brow, a wide grin (lips, a slit through to a mouth cavity,
gums), warts, chitin plates shingled over the crown and back, scars, and ten ringed eyestalks
fused into the crown, each ending in a bulb with an eye hole. Separate fields make the two
eyelids (shells with a rolled margin and fold creases) and the tongue. Each field is meshed
twice by surface nets, and the vertices are moved onto the surface by Newton steps:
  * high (0.75 cm): only for baking; vertex colour, roughness, wrinkle strength and
    distance-field ambient occlusion are computed on it;
  * low (2.4 cm): the game mesh, about 70k triangles, with even quads that skin well.
Cycles bakes the high mesh onto the low mesh's UVs: colour (plus fine mottling), the normal
(with wrinkles and pores from procedural bump), and roughness/AO. The eyes (a polar-mapped iris
texture made here), the main eye's cornea and the teeth are built directly.

Bones (component-space posing at runtime): root, body, jaw (hinged through the mouth corners),
tongue_0/1, lid_upper, lid_lower, eye, and for each stalk k = 0..9 stalk{k}_0..5 along it and
stalk{k}_eye. Facing +X, centimetres, Z up, the body's centre at the origin.
"""
from pathlib import Path
import json
import math
import sys
import time

import bmesh
import bpy
import numpy as np
from mathutils import Vector, kdtree

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets/Enemies/Beholder'
OUT.mkdir(parents=True, exist_ok=True)
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None
QUICK = '--quick' in argv
HIGH_H = 1.4 if QUICK else 0.75        # cm, the sculpt-detail mesh
LOW_H = 3.0                            # cm, the game mesh (lids and tongue: LOW_PART)
LOW_PART = 1.8
TEX = 1024 if QUICK else 4096
T0 = time.time()


def log(*a):
    print('CHUCK_BEHOLDER', f'{time.time() - T0:6.1f}s', *a, flush=True)


# ------------------------------------------------------------------ proportions (cm)
R_BODY = np.array([104., 102., 97.])   # the orb
E = np.array([72., 0., 12.])           # the great eye's centre (it bulges 10 cm out of the orb)
R_EYE = 42.
SOCKET = R_EYE + 8.5                   # the socket's sphere
IRIS, PUPIL = 28., 12.                 # degrees from the eye's axis
A_MAX = 70.                            # the grin's half-width, degrees of azimuth
JAW = np.array([31., 0., -42.])        # the hinge (through the mouth corners)


def mouth_e(a):
    """Elevation (deg) of the slit's middle at azimuth a (deg): low in the middle, corners up."""
    return -42. + 18. * (a / A_MAX) ** 2


def mouth_gap(a):
    """The slit's half-gap (cm): a grin open on the teeth, closing at the corners."""
    return .8 + 5.6 * np.sqrt(np.clip(1 - (a / A_MAX) ** 2, 0, 1))


def lid_edge(alpha, upper):
    """Lid margins (deg above the eye's horizon) at alpha (deg round the eye's axis): the upper
    lid low in the middle (a scowl), the lower lid rising to the corners (an almond)."""
    if upper: return 13. + 13. * (alpha / 65.) ** 2
    return -27. + 7. * (alpha / 65.) ** 2


def r_dir(d):
    """The orb's radius along unit directions d (N,3)."""
    return 1. / np.sqrt(((d / R_BODY) ** 2).sum(-1))


def surf(a, e, inset=0.):
    """The point on the orb at azimuth a, elevation e (deg), `inset` cm inside."""
    a, e = np.radians(a), np.radians(e)
    d = np.stack([np.cos(e) * np.cos(a), np.cos(e) * np.sin(a), np.sin(e)], -1)
    return d * (r_dir(d) - inset)[..., None]


def normal_at(p):
    n = p / R_BODY ** 2
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


# ------------------------------------------------------------------ noise
def _hash(ix, iy, iz, seed):
    h = (ix * 374761393 + iy * 668265263 + iz * 1274126177 + seed * 144665) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h = h ^ (h >> 16)
    return (h & 0xFFFFFF).astype(np.float64) / float(0xFFFFFF)


def vnoise(P, seed=0):
    """Value noise in [-1, 1] at P (N,3), unit cells."""
    f = np.floor(P); t = P - f; i = f.astype(np.int64)
    u = t * t * (3 - 2 * t)
    out = np.zeros(len(P))
    for dx in (0, 1):
        wx = u[:, 0] if dx else 1 - u[:, 0]
        for dy in (0, 1):
            wy = u[:, 1] if dy else 1 - u[:, 1]
            for dz in (0, 1):
                wz = u[:, 2] if dz else 1 - u[:, 2]
                out += wx * wy * wz * _hash(i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz, seed)
    return out * 2 - 1


def fbm(P, octaves=3, seed=0):
    out = np.zeros(len(P)); amp, tot = 1., 0.
    for o in range(octaves):
        out += amp * vnoise(P * (2 ** o) + o * 17.3, seed + o); tot += amp; amp *= .5
    return out / tot


# ------------------------------------------------------------------ distance field
def smin(a, b, k):
    if k <= 0: return np.minimum(a, b)
    h = np.clip(.5 + .5 * (b - a) / k, 0, 1)
    return b + (a - b) * h - k * h * (1 - h)


def smax(a, b, k):
    return -smin(-a, -b, k)


def sd_ellipsoid(P, c, r, rot=None):
    q = P - c
    if rot is not None: q = q @ rot          # rot's columns are the local axes
    k0 = np.linalg.norm(q / r, axis=1); k1 = np.linalg.norm(q / (r * r), axis=1)
    return k0 * (k0 - 1.) / np.maximum(k1, 1e-9)


class Op:
    """One step of a field: kind 'add' (smooth union), 'sub' (smooth subtraction) or 'int'."""
    kind, k = 'add', 0.

    def box_hit(self, lo, hi):
        return not (np.any(self.lo > hi) or np.any(self.hi < lo))


class Func(Op):
    def __init__(self, kind, k, lo, hi, f):
        self.kind, self.k, self.lo, self.hi, self.f = kind, k, np.asarray(lo, float), np.asarray(hi, float), f

    def eval(self, P, lo, hi):
        return self.f(P) if self.box_hit(lo, hi) else None


class Tube(Op):
    """A chain of round cones through pts (M,3) with radii rad (M,), hard-unioned."""
    def __init__(self, kind, k, pts, rad):
        pts, rad = np.asarray(pts, float), np.asarray(rad, float)
        self.kind, self.k = kind, k
        self.A, self.B, self.ra, self.rb = pts[:-1], pts[1:], rad[:-1], rad[1:]
        m = np.maximum(self.ra, self.rb)[:, None]
        self.slo, self.shi = np.minimum(self.A, self.B) - m, np.maximum(self.A, self.B) + m
        self.lo, self.hi = self.slo.min(0), self.shi.max(0)

    def eval(self, P, lo, hi):
        if not self.box_hit(lo, hi): return None
        sel = ~(np.any(self.slo > hi, 1) | np.any(self.shi < lo, 1))
        if not sel.any(): return None
        A, B, ra, rb = self.A[sel], self.B[sel], self.ra[sel], self.rb[sel]
        ba = B - A; bb = np.maximum((ba * ba).sum(1), 1e-9)
        out = np.full(len(P), 1e4)
        for s in range(0, len(A), 48):
            pa = P[:, None, :] - A[None, s:s + 48]
            h = np.clip((pa * ba[None, s:s + 48]).sum(2) / bb[None, s:s + 48], 0, 1)
            d = np.linalg.norm(pa - ba[None, s:s + 48] * h[..., None], axis=2) - (ra[None, s:s + 48] + (rb - ra)[None, s:s + 48] * h)
            out = np.minimum(out, d.min(1))
        return out


class Balls(Op):
    """Many spheres, hard-unioned."""
    def __init__(self, kind, k, c, r):
        self.kind, self.k, self.c, self.r = kind, k, np.asarray(c, float), np.asarray(r, float)
        self.slo, self.shi = self.c - self.r[:, None], self.c + self.r[:, None]
        self.lo, self.hi = self.slo.min(0), self.shi.max(0)

    def eval(self, P, lo, hi):
        if not self.box_hit(lo, hi): return None
        sel = ~(np.any(self.slo > hi, 1) | np.any(self.shi < lo, 1))
        if not sel.any(): return None
        d = np.linalg.norm(P[:, None, :] - self.c[None, sel], axis=2) - self.r[None, sel]
        return d.min(1)


class Ellipsoids(Op):
    """Oriented ellipsoids (centre, radii, local axes as columns), hard-unioned."""
    def __init__(self, kind, k, items):
        self.kind, self.k, self.items = kind, k, items
        self.slo = np.array([c - r.max() for c, r, _ in items]); self.shi = np.array([c + r.max() for c, r, _ in items])
        self.lo, self.hi = self.slo.min(0), self.shi.max(0)

    def eval(self, P, lo, hi):
        if not self.box_hit(lo, hi): return None
        sel = np.nonzero(~(np.any(self.slo > hi, 1) | np.any(self.shi < lo, 1)))[0]
        if not len(sel): return None
        out = np.full(len(P), 1e4)
        for i in sel:
            c, r, rot = self.items[i]
            out = np.minimum(out, sd_ellipsoid(P, c, r, rot))
        return out


PAD = 1.5


def evaluate(ops, P, lo=None, hi=None, margin=0.):
    """The field at P. Elements further than k + margin from P's box are skipped: exact for the
    sign and for distances under `margin` (pass the largest distance the caller relies on)."""
    if lo is None: lo, hi = P.min(0), P.max(0)
    d = np.full(len(P), 1e4)
    for op in ops:
        s = op.eval(P, lo - op.k - PAD - margin, hi + op.k + PAD + margin)
        if s is None: continue
        if op.kind == 'add': d = smin(d, s, op.k)
        elif op.kind == 'sub': d = smax(d, -s, op.k)
        else: d = smax(d, s, op.k)
    return d


def evaluate_binned(ops, P, size=14., margin=0.):
    out = np.empty(len(P))
    keys = np.floor(P / size).astype(np.int64)
    key = (keys[:, 0] + 4096) + 8192 * ((keys[:, 1] + 4096) + 8192 * (keys[:, 2] + 4096))
    order = np.argsort(key, kind='stable'); ks = key[order]
    cuts = np.nonzero(np.diff(ks))[0] + 1
    for g in np.split(order, cuts):
        Q = P[g]; out[g] = evaluate(ops, Q, Q.min(0), Q.max(0), margin)
    return out


def gradient(ops, P, h=.25):
    off = np.array([[h, 0, 0], [-h, 0, 0], [0, h, 0], [0, -h, 0], [0, 0, h], [0, 0, -h]])
    Q = (P[:, None, :] + off[None]).reshape(-1, 3)
    d = evaluate_binned(ops, Q).reshape(-1, 6)
    g = np.stack([d[:, 0] - d[:, 1], d[:, 2] - d[:, 3], d[:, 4] - d[:, 5]], 1) / (2 * h)
    return g


def project(ops, V, step, iters=3):
    """Newton steps onto the zero set."""
    for _ in range(iters):
        d = evaluate_binned(ops, V); g = gradient(ops, V)
        gg = np.maximum((g * g).sum(1), 1e-6)
        mv = (d / gg)[:, None] * g
        n = np.linalg.norm(mv, axis=1, keepdims=True)
        V = V - mv * np.minimum(1., step / np.maximum(n, 1e-9))
    g = gradient(ops, V)
    return V, g / np.maximum(np.linalg.norm(g, axis=1, keepdims=True), 1e-9)


# ------------------------------------------------------------------ surface nets
CORN = np.array([[(i >> 0) & 1, (i >> 1) & 1, (i >> 2) & 1] for i in range(8)], float)
EDGES = [(0, 1), (2, 3), (4, 5), (6, 7), (0, 2), (1, 3), (4, 6), (5, 7), (0, 4), (1, 5), (2, 6), (3, 7)]


def polygonize(ops, h, lo, hi):
    """Surface nets over the active blocks; returns (verts (N,3), quads (M,4))."""
    B = 16
    lo = np.asarray(lo, float); hi = np.asarray(hi, float)
    n = np.ceil((hi - lo) / h).astype(int) + 2
    nb = (n + B - 1) // B
    bi = np.stack(np.meshgrid(*[np.arange(k) for k in nb], indexing='ij'), -1).reshape(-1, 3)
    centres = lo + (bi * B + B / 2) * h
    thr = B * h * .87 * 1.6 + 2.
    dc = evaluate_binned(ops, centres, size=B * h * 2, margin=thr)
    act = bi[np.abs(dc) < thr]
    S = n + 4
    keys_l, pos_l, quads_l = [], [], []
    idx = np.arange(-1, B + 1)
    for b in act:
        s = b * B
        g = [s[i] + idx for i in range(3)]
        X = lo[0] + g[0] * h; Y = lo[1] + g[1] * h; Z = lo[2] + g[2] * h
        P = np.stack(np.meshgrid(X, Y, Z, indexing='ij'), -1).reshape(-1, 3)
        G = evaluate(ops, P, P.min(0), P.max(0)).reshape(B + 2, B + 2, B + 2)
        neg = G < 0
        if neg.all() or not neg.any(): continue
        c = [G[int(o[0]):int(o[0]) + B + 1, int(o[1]):int(o[1]) + B + 1, int(o[2]):int(o[2]) + B + 1] for o in CORN]
        acc = np.zeros((B + 1, B + 1, B + 1, 3)); num = np.zeros((B + 1, B + 1, B + 1))
        for i, j in EDGES:
            a_, b_ = c[i], c[j]
            m = (a_ < 0) != (b_ < 0)
            if not m.any(): continue
            t = np.where(m, a_ / np.where(m, a_ - b_, 1.), 0.)
            acc += m[..., None] * (CORN[i] + t[..., None] * (CORN[j] - CORN[i]))
            num += m
        cell = num > 0
        ci = np.argwhere(cell)
        gc = ci + (s - 1)                                      # global cell index
        pos = lo + (gc + acc[cell] / num[cell][:, None]) * h
        key = (gc[:, 0] + 2) + S[0] * ((gc[:, 1] + 2) + S[1] * (gc[:, 2] + 2))
        keys_l.append(key); pos_l.append(pos)
        own = slice(1, B + 1)
        for ax in range(3):
            o = [own, own, own]; o2 = [own, own, own]; o2[ax] = slice(2, B + 2)
            g0 = neg[tuple(o)]; g1 = neg[tuple(o2)]
            m = g0 != g1
            if not m.any(): continue
            pts = np.argwhere(m) + 1                             # local point index of the edge's start
            inside_first = g0[m]
            u, v = (ax + 1) % 3, (ax + 2) % 3
            cells = []
            for du, dv in ((-1, -1), (0, -1), (0, 0), (-1, 0)):
                cc = pts.copy(); cc[:, u] += du; cc[:, v] += dv
                gcc = cc + (s - 1)
                cells.append((gcc[:, 0] + 2) + S[0] * ((gcc[:, 1] + 2) + S[1] * (gcc[:, 2] + 2)))
            q = np.stack(cells, 1)
            q[~inside_first] = q[~inside_first][:, ::-1]
            quads_l.append(q)
    keys = np.concatenate(keys_l); pos = np.concatenate(pos_l)
    uk, first = np.unique(keys, return_index=True)
    verts = pos[first]
    quads = np.searchsorted(uk, np.concatenate(quads_l))
    # drop degenerate quads (repeated corners)
    ok = np.ones(len(quads), bool)
    for i in range(4):
        for j in range(i + 1, 4): ok &= quads[:, i] != quads[:, j]
    quads = quads[ok]
    used = np.unique(quads)
    remap = np.full(len(verts), -1); remap[used] = np.arange(len(used))
    return verts[used], remap[quads]


# ------------------------------------------------------------------ the creature's fields
rng = np.random.default_rng(20261023)
STALKS = []      # per stalk: dict(samples (M,3), s (M,), radius (M,), L, base, tip, gaze, eye)


def catmull(Q, n=600):
    """A Catmull-Rom curve through the points Q (end tangents mirrored)."""
    Q = np.asarray(Q, float)
    Q = np.vstack([2 * Q[0] - Q[1], Q, 2 * Q[-1] - Q[-2]])
    out = []
    per = max(2, n // (len(Q) - 3))
    for i in range(1, len(Q) - 2):
        p0, p1, p2, p3 = Q[i - 1], Q[i], Q[i + 1], Q[i + 2]
        t = np.linspace(0, 1, per, endpoint=False)[:, None]
        out.append(.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t ** 2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    out.append(Q[-2][None])
    return np.vstack(out)


def stalk_layout():
    """Ten stalks from the crown: thick, ringed, each rising, swaying to one side and back, its
    tip turning out and down to look. Front stalks lean over the eye; back ones look out."""
    spec = [(30, 52, 136), (-30, 52, 128), (72, 44, 146), (-72, 44, 150), (116, 46, 140),
            (-116, 46, 134), (158, 52, 126), (-158, 52, 132), (88, 74, 156), (-88, 74, 152)]
    up = np.array([0., 0., 1.]); fwd = np.array([1., 0., 0.])
    for k, (a, e, L) in enumerate(spec):
        a += rng.uniform(-5, 5); e += rng.uniform(-3, 3)
        base = surf(a, e); n = normal_at(base[None])[0]
        P0 = base - n * 7.
        horiz = np.array([n[0], n[1], 0.]); horiz /= max(np.linalg.norm(horiz), 1e-6)
        side = np.cross(up, horiz); side /= np.linalg.norm(side)
        w = rng.uniform(.08, .16) * (1 if k % 2 else -1)
        lean = fwd * (.45 if abs(a) < 60 else .1 if abs(a) < 100 else 0.)
        Q = [P0,
             P0 + n * L * .26,
             P0 + n * L * .38 + up * L * .3 + side * w * L,
             P0 + n * L * .5 + up * L * .56 - side * w * .6 * L + horiz * L * .08 + lean * L * .25]
        look = horiz * .85 + lean * 1.2 - up * .35 + side * w * 1.5
        look /= np.linalg.norm(look)
        Q.append(Q[-1] + look * L * .2)
        C = catmull(Q)
        seg = np.linalg.norm(np.diff(C, axis=0), axis=1); s = np.concatenate([[0], np.cumsum(seg)])
        Ltot = s[-1]
        ss = np.arange(0, Ltot, .9); ss = np.append(ss, Ltot)
        pts = np.stack([np.interp(ss, s, C[:, i]) for i in range(3)], 1)
        u = ss / Ltot
        rad = 5. + 6.5 * (1 - u) ** 1.6 + .5 * np.sin(2 * np.pi * ss / 3.8) * (.45 + .55 * u) \
            + 3.5 * np.exp(-((ss - 9) / 5.) ** 2)
        gaze = pts[-1] - pts[-4]; gaze /= np.linalg.norm(gaze)
        tip = pts[-1]
        bulb = tip + gaze * 2.5
        STALKS.append(dict(k=k, samples=pts, s=ss, radius=rad, L=Ltot, base=base, P0=P0, tip=tip, gaze=gaze,
                           bulb=bulb, eye=bulb + gaze * 4., normal=n))


def body_ops():
    ops = []
    lo, hi = -R_BODY - 8, R_BODY + 8
    ops.append(Func('add', 0., lo, hi, lambda P: sd_ellipsoid(P, np.zeros(3), R_BODY) - 1.8 * fbm(P / 26., 3, 11)))
    # The socket's mound: a ring round the eye, heavy over it (the brow), dipping in the middle.
    ring, rr = [], []
    for phi in np.linspace(0, 360, 145):
        p = math.radians(phi)
        up = math.sin(p)
        psi = math.radians(62. - 5. * max(0., up) * math.exp(-((phi - 90) / 30) ** 2))
        q = np.array([math.cos(psi), math.sin(psi) * math.cos(p), math.sin(psi) * math.sin(p)]) * (SOCKET + 2.)
        ring.append(E + q)
        brow = 11.5 * math.exp(-((phi - 90) / 44) ** 2)
        bag = 4. * math.exp(-((phi - 270) / 45) ** 2)
        rr.append(3.2 + brow + bag)
    ops.append(Tube('add', 12., ring, rr))
    brow = []
    for phi in np.linspace(28, 152, 60):
        p = math.radians(phi); psi = math.radians(49.)
        q = np.array([math.cos(psi), math.sin(psi) * math.cos(p), math.sin(psi) * math.sin(p)]) * (SOCKET + 4.)
        brow.append(E + q - np.array([0., 0., 4. * math.exp(-((phi - 90) / 26) ** 2)]))
    ph = np.linspace(28, 152, 60)
    taper = np.sqrt(np.sin(np.pi * (ph - 28) / 124)) * .92 + .08          # it melts into the mound at its ends
    ops.append(Tube('add', 7., brow, (3.5 + 4.5 * np.exp(-((ph - 90) / 34) ** 2)) * taper))
    # Cheeks at the corners of the grin.
    cheeks = []
    for sgn in (1, -1):
        c = surf(sgn * (A_MAX + 4), mouth_e(A_MAX) + 4, inset=4.)
        n = normal_at(c[None])[0]
        cheeks.append((c, np.array([11., 11., 8.]), frame(n)))
    ops.append(Ellipsoids('add', 7., cheeks))
    # Lips: rolled flesh just outside the slit.
    for upper in (True, False):
        a = np.linspace(-A_MAX + 1, A_MAX - 1, 120)
        rl = 2.6 + 1.3 * np.sqrt(np.clip(1 - (a / A_MAX) ** 2, 0, 1))
        off = (mouth_gap(a) + .95 * rl) / 100. * 180 / np.pi
        e = mouth_e(a) + (off if upper else -off)
        pts = surf(a, e, inset=1.2)
        ops.append(Tube('add', 2.2, pts, rl))
        BODY_INFO.setdefault('lips', []).append((pts, rl))
    # Warts, scattered (fewer on the face).
    W = []
    while len(W) < 150:
        d = rng.normal(size=3); d /= np.linalg.norm(d)
        a, e = math.degrees(math.atan2(d[1], d[0])), math.degrees(math.asin(d[2]))
        if abs(a) < 80 and -60 < e < 40 and rng.random() < .85: continue
        p = surf(a, e)
        r = rng.uniform(.8, 2.6)
        W.append((p - normal_at(p[None])[0] * r * .45, r))
    ops.append(Balls('add', 1.1, [w[0] for w in W], [w[1] for w in W]))
    # Chitin plates, shingled over the crown and down the back: broad, irregular, overlapping.
    plates = []
    for e in np.arange(-6, 86, 10.5):
        step = 19. / max(.22, math.cos(math.radians(e)))
        for a in np.arange(-180 + (e * 5) % step, 180, step):
            a2 = ((a + 180 + rng.uniform(-3, 3)) % 360) - 180
            e2 = e + rng.uniform(-2, 2)
            if abs(a2) < 64 and e2 < 66: continue                   # leave the brow and face bare
            if abs(a2) < 100 and e2 < 20: continue                  # and the flanks below the crown
            p = surf(a2, e2)
            if any(np.linalg.norm(p - st['base']) < 18 for st in STALKS): continue
            if rng.random() < .08: continue
            n = normal_at(p[None])[0]
            mer = np.array([-math.sin(math.radians(e2)) * math.cos(math.radians(a2)), -math.sin(math.radians(e2)) * math.sin(math.radians(a2)), math.cos(math.radians(e2))])
            back = -mer if e2 < 80 else np.array([-1., 0., 0.])   # shingles run down and back
            side = np.cross(n, back); side /= np.linalg.norm(side)
            twist = math.radians(rng.uniform(-14, 14))
            side, back = side * math.cos(twist) + back * math.sin(twist), back * math.cos(twist) - side * math.sin(twist)
            tilt = math.radians(rng.uniform(7, 12))
            zl = n * math.cos(tilt) - back * math.sin(tilt)
            yl = np.cross(zl, side)
            rot = np.stack([side, yl, zl], 1)
            plates.append((p - n * 1.4, np.array([rng.uniform(13, 17), rng.uniform(15, 20), rng.uniform(3.2, 4.2)]), rot))
    ops.append(Ellipsoids('add', 1.3, plates))
    BODY_INFO['plates'] = plates
    # A ridge of bony knobs down the spine, from the crown to the back's middle.
    knobs, kr = [], []
    for e in np.linspace(78, -20, 15):
        p = surf(180., e); n = normal_at(p[None])[0]
        r = 3.2 + 2.6 * smooth(-20, 50, e) + rng.uniform(-.4, .4)
        knobs.append(p + n * (2.5 + r * .3)); kr.append(r)
    ops.append(Balls('add', 2.4, knobs, kr))
    BODY_INFO['warts'] = W
    # Eyestalks, fused into the crown.
    for st in STALKS:
        ops.append(Tube('add', 5.5, st['samples'], st['radius']))
        g = st['gaze']
        ops.append(Ellipsoids('add', 2.5, [(st['bulb'], np.array([10.5, 10.5, 11.]), frame(g))]))
    # The socket, the mouth cavity, the slit.
    ops.append(Func('sub', 4.5, E - SOCKET, E + SOCKET, lambda P: np.maximum(np.linalg.norm(P - E, axis=1) - SOCKET, (E[0] - 22) - P[:, 0])))
    ops.append(Func('sub', 3., [0, -95, -95], [92, 95, -10], cavity))
    ops.append(Func('sub', 1.2, [8, -110, -110], [120, 110, -10], slit))
    # Gums along the slit's walls.
    for upper in (True, False):
        a = np.linspace(-A_MAX + 5, A_MAX - 5, 90)
        r = 95. if upper else 91.
        off = np.degrees((mouth_gap(a) + 3.) / r)
        e = mouth_e(a) + (off if upper else -off)
        ae, ee = np.radians(a), np.radians(e)
        pts = np.stack([np.cos(ee) * np.cos(ae), np.cos(ee) * np.sin(ae), np.sin(ee)], 1) * r
        ops.append(Tube('add', 2., pts, np.full(len(pts), 3.)))
    # Old scars.
    scars = []
    for a, e, da, de in ((-118, 8, 26, 9), (128, -18, -18, 16), (-60, -62, 22, -6), (150, 30, 12, 14), (-150, -30, -16, 10)):
        t = np.linspace(0, 1, 30)
        pts = surf(a + da * t, e + de * t + 2 * np.sin(t * 5), inset=.5)
        scars.append(pts)
        ops.append(Tube('sub', .6, pts, 1.1 * np.sin(np.pi * np.clip(t * 1.1, 0, 1)) + .15))
    BODY_INFO['scars'] = scars
    # The stalk eyes' holes.
    ops.append(Balls('sub', 1.2, [st['bulb'] + st['gaze'] * 8. for st in STALKS], [7.8] * len(STALKS)))
    return ops


BODY_INFO = {}


def frame(z):
    """Local axes (columns x, y, z) with z along the given direction."""
    z = z / np.linalg.norm(z)
    x = np.cross([0., 0., 1.], z)
    if np.linalg.norm(x) < 1e-3: x = np.cross([0., 1., 0.], z)
    x /= np.linalg.norm(x); y = np.cross(z, x)
    return np.stack([x, y, z], 1)


def angles(P):
    r = np.linalg.norm(P, axis=1); rxy = np.hypot(P[:, 0], P[:, 1])
    a = np.degrees(np.arctan2(P[:, 1], P[:, 0])); e = np.degrees(np.arctan2(P[:, 2], rxy))
    return r, rxy, a, e


def cavity(P):
    r, rxy, a, e = angles(P)
    hc = 8. * np.sqrt(np.clip(1 - (a / (A_MAX + 3)) ** 2, 0, 1)) + 2.
    de = r * (np.sin(np.radians(np.minimum(np.abs(e - mouth_e(a)), 89.))) - np.sin(np.radians(hc)))
    da = rxy * np.sin(np.radians(np.clip(np.abs(a) - (A_MAX - 4), -89, 89)))
    return np.maximum.reduce([de, da, r - 86., 30. - r])


def slit(P):
    r, rxy, a, e = angles(P)
    de = r * np.sin(np.radians(np.minimum(np.abs(e - mouth_e(a)), 89.))) - mouth_gap(np.clip(a, -A_MAX, A_MAX))
    da = rxy * np.sin(np.radians(np.clip(np.abs(a) - A_MAX, -89, 89)))
    return np.maximum.reduce([de, da, 86. - r, 10. - P[:, 0]])


def lid_ops(upper):
    def shell(P):
        q = P - E; r = np.linalg.norm(q, axis=1)
        al = np.degrees(np.arctan2(q[:, 1], q[:, 0])); th = np.degrees(np.arctan2(q[:, 2], np.hypot(q[:, 0], q[:, 1])))
        edge = lid_edge(np.clip(al, -80, 80), upper)
        dth = (edge - th) if upper else (th - edge)
        region = r * np.sin(np.radians(np.clip(dth, -89, 89)))
        sh = np.abs(r - (R_EYE + 4.4)) - 2.8
        return np.maximum.reduce([sh, region, -14. - q[:, 0]])
    ops = [Func('add', 0., E - R_EYE - 8, E + R_EYE + 8, shell)]
    def arc(off_deg, rad):
        al = np.linspace(-82, 82, 100)
        th = lid_edge(al, upper) + (off_deg if upper else -off_deg)
        a_, t_ = np.radians(al), np.radians(th)
        return E + np.stack([np.cos(t_) * np.cos(a_), np.cos(t_) * np.sin(a_), np.sin(t_)], 1) * rad
    ops.append(Tube('add', 1.5, arc(3., R_EYE + 4.4), np.full(100, 3.4)))
    folds = (10., 17.) if upper else (9.,)
    for f in folds:
        ops.append(Tube('sub', .8, arc(f, R_EYE + 7.4), np.full(100, 1.)))
    return ops


def tongue_line():
    r = np.linspace(34, 78, 40)
    e = -52. + np.degrees(4.6 / r) + 1.5 * (r - 34) / 44
    a = 3. * np.sin(r / 9.)
    ae, ee = np.radians(a), np.radians(e)
    return np.stack([np.cos(ee) * np.cos(ae), np.cos(ee) * np.sin(ae), np.sin(ee)], 1) * r[:, None], r


def tongue_ops():
    pts, r = tongue_line()
    items = []
    for i in range(0, len(pts) - 1, 2):
        d = pts[i + 1] - pts[i]; d /= np.linalg.norm(d)
        up = np.cross(d, [0., 1., 0.]); up = up / np.linalg.norm(up)       # normal to the cavity's floor
        side = np.cross(up, d)
        u = (r[i] - 34) / 44
        items.append((pts[i], np.array([10.5 - 3. * u ** 2, 5., 3.4 - .8 * u]), np.stack([side, d, up], 1)))
    ops = [Ellipsoids('add', 0., items)]
    d = np.gradient(pts, axis=0); d /= np.linalg.norm(d, axis=1, keepdims=True)
    ups = np.cross(d, [0., 1., 0.]); ups /= np.linalg.norm(ups, axis=1, keepdims=True)
    groove = pts + ups * 3.6
    ops.append(Tube('sub', 1.5, groove[4:-2], np.full(len(groove) - 6, 1.)))
    return ops


# ------------------------------------------------------------------ meshes
def make_mesh(name, V, Q):
    me = bpy.data.meshes.new(name)
    me.vertices.add(len(V)); me.vertices.foreach_set('co', V.astype(np.float32).ravel())
    me.loops.add(len(Q) * 4); me.loops.foreach_set('vertex_index', Q.astype(np.int32).ravel())
    me.polygons.add(len(Q)); me.polygons.foreach_set('loop_start', np.arange(0, len(Q) * 4, 4, dtype=np.int32))
    me.update(calc_edges=True); me.validate(clean_customdata=False)
    me.polygons.foreach_set('use_smooth', np.ones(len(Q), bool))
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
    return ob


def mesh_from(name, verts, faces, smooth_=True):
    me = bpy.data.meshes.new(name); me.from_pydata([tuple(v) for v in verts], [], [tuple(f) for f in faces]); me.update()
    for p in me.polygons: p.use_smooth = smooth_
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
    return ob


def field_mesh(name, ops, h, pad=10.):
    lo = np.min([op.lo - op.k for op in ops if op.kind == 'add'], 0) - pad
    hi = np.max([op.hi + op.k for op in ops if op.kind == 'add'], 0) + pad
    V, Q = polygonize(ops, h, lo, hi)
    V, N = project(ops, V, h * .5)
    log(name, f'h={h}', f'verts={len(V)}', f'quads={len(Q)}')
    return V, Q, N


def sphere_eye(name, centre, axis, radius, rings, segs, psi_max=180.):
    """A sphere with its pole along `axis`; UVs polar for the iris texture."""
    F = frame(np.asarray(axis, float))
    verts, uvs, faces = [], [], []
    psis = [psi_max * (i / rings) ** 1.25 for i in range(rings + 1)]
    for i, ps in enumerate(psis):
        for j in range(segs if i else 1):
            b = 2 * math.pi * j / segs
            p_ = math.radians(ps)
            loc = np.array([math.sin(p_) * math.cos(b), math.sin(p_) * math.sin(b), math.cos(p_)])
            verts.append(centre + F @ (loc * radius))
            rho = min(ps / 70., 1.)
            uvs.append((.5 + .5 * rho * math.cos(b), .5 + .5 * rho * math.sin(b)))
    def vid(i, j): return 0 if i == 0 else 1 + (i - 1) * segs + (j % segs)
    for j in range(segs): faces.append((0, vid(1, j), vid(1, j + 1)))
    for i in range(1, rings):
        for j in range(segs): faces.append((vid(i, j), vid(i + 1, j), vid(i + 1, j + 1), vid(i, j + 1)))
    ob = mesh_from(name, verts, faces)
    uv = ob.data.uv_layers.new(name='UVMap')
    for poly in ob.data.polygons:
        for li in poly.loop_indices:
            uv.data[li].uv = uvs[ob.data.loops[li].vertex_index]
    return ob


def cornea():
    F = frame(np.array([1., 0., 0.]))
    verts, faces, uvs = [], [], []
    rings, segs, top = 16, 72, 34.
    for i in range(rings + 1):
        ps = top * i / rings
        r = R_EYE + .15 + 1.05 * math.cos(min(ps / top, 1) * math.pi / 2) ** 2
        for j in range(segs if i else 1):
            b = 2 * math.pi * j / segs; p_ = math.radians(ps)
            loc = np.array([math.sin(p_) * math.cos(b), math.sin(p_) * math.sin(b), math.cos(p_)])
            verts.append(E + F @ (loc * r))
            rho = ps / 70.; uvs.append((.5 + .5 * rho * math.cos(b), .5 + .5 * rho * math.sin(b)))
    def vid(i, j): return 0 if i == 0 else 1 + (i - 1) * segs + (j % segs)
    for j in range(segs): faces.append((0, vid(1, j), vid(1, j + 1)))
    for i in range(1, rings):
        for j in range(segs): faces.append((vid(i, j), vid(i + 1, j), vid(i + 1, j + 1), vid(i, j + 1)))
    ob = mesh_from('cornea', verts, faces)
    uv = ob.data.uv_layers.new(name='UVMap')
    for poly in ob.data.polygons:
        for li in poly.loop_indices: uv.data[li].uv = uvs[ob.data.loops[li].vertex_index]
    return ob


def teeth():
    """Two rows of uneven fangs set in the gums: the upper row in front, the lower behind."""
    verts, faces, cols, owner = [], [], [], []
    for upper in (True, False):
        n = 17 if upper else 16
        r0 = 94.5 if upper else 90.5
        for i in range(n):
            a = -A_MAX * .82 + 2 * A_MAX * .82 * (i + (.0 if upper else .5)) / n + rng.uniform(-1.2, 1.2)
            if abs(a) > A_MAX * .82: continue                         # the corners close on the lips: no room
            g = float(mouth_gap(a))
            off = math.degrees((g + 2.8) / r0)
            e = float(mouth_e(a)) + (off if upper else -off)
            root = surf(a, e)[()] / np.linalg.norm(surf(a, e)) * r0
            # along the meridian toward the slit, a little outward
            ar, er = math.radians(a), math.radians(e)
            mer = np.array([-math.sin(er) * math.cos(ar), -math.sin(er) * math.sin(ar), math.cos(er)])
            out = root / np.linalg.norm(root)
            d = (-mer if upper else mer) + out * .18; d /= np.linalg.norm(d)
            side = np.cross(d, out); side /= np.linalg.norm(side)
            fang = abs(abs(a) - A_MAX * .42) < 5
            L = g + (6.5 if upper else 5.5) + rng.uniform(0, 3.) + (7. if fang else 0.)
            L *= .35 + .8 * math.sqrt(max(0., 1 - (a / A_MAX) ** 2))   # short near the corners, where the gap closes
            rb = rng.uniform(2.8, 3.6) + (.9 if fang else 0.)
            broken = rng.random() < .15
            hook = -out * .12 * L
            base = len(verts)
            rings, sides = 9, 10
            for k in range(rings):
                t = k / (rings - 1)
                if broken: t = t * .78
                c = root - d * 2.2 + d * (L + 2.2) * t + hook * t * t
                rad = rb * (1 - t ** 1.25) * (1 if k < rings - 1 or broken else 0) + (.12 if k == rings - 1 and not broken else 0)
                for j in range(sides):
                    b = 2 * math.pi * j / sides
                    verts.append(c + (side * math.cos(b) + np.cross(d, side) * math.sin(b) * .72) * rad)
                    tone = smooth(0., .35, t)
                    col = np.array([.33, .22, .11]) * (1 - tone) + np.array([.80, .72, .55]) * tone
                    col = col * (.9 + .2 * rng.random()) + (np.array([.08, .08, .07]) * smooth(.8, 1., t))
                    cols.append(col); owner.append(upper)
            for k in range(rings - 1):
                for j in range(sides):
                    a0 = base + k * sides + j; a1 = base + k * sides + (j + 1) % sides
                    faces.append((a0, a1, a1 + sides, a0 + sides))
            tip = base + (rings - 1) * sides
            faces.append(tuple(range(tip + sides - 1, tip - 1, -1)))
    ob = mesh_from('teeth', verts, faces)
    attr = ob.data.color_attributes.new('Col', 'FLOAT_COLOR', 'POINT')
    attr.data.foreach_set('color', np.concatenate([np.array(cols), np.ones((len(cols), 1))], 1).astype(np.float32).ravel())
    return ob, np.array(owner)


# ------------------------------------------------------------------ surface look (on the high mesh)
def lin(*c):
    return (np.array(c, float) / 255.) ** 2.2


def surface_look(V, N, ao_ops, part):
    """Vertex colour (linear), and aux = (AO, roughness, wrinkle) for the bake."""
    n = len(V)
    if part != 'body':
        base = lin(118, 70, 112) if part == 'lid' else lin(140, 40, 56)
        col = np.tile(base, (n, 1)) * (.85 + .25 * (fbm(V / 9., 3, 5) * .5 + .5))[:, None]
        if part == 'lid':
            # the margin wet and pinker, the outer lid darker
            q = V - E; th = np.degrees(np.arctan2(q[:, 2], np.hypot(q[:, 0], q[:, 1])))
            al = np.degrees(np.arctan2(q[:, 1], q[:, 0]))
            up = th > 0
            dist = np.where(up, th - lid_edge(np.clip(al, -80, 80), True), lid_edge(np.clip(al, -80, 80), False) - th)
            margin = smooth(6, 1.5, dist)
            col = col * (1 - margin[:, None]) + lin(150, 66, 88) * margin[:, None]
            rough = .42 - .2 * margin; wr = .9 - .4 * margin
        else:
            rough = np.full(n, .2); wr = np.full(n, .55)
        ao = sdf_ao(V, N, ao_ops)
        return col, np.stack([ao, rough, wr], 1)
    dirn = V / np.linalg.norm(V, axis=1, keepdims=True)
    dorsal = smooth(-.1, .75, .7 * (-dirn[:, 0]) + .6 * dirn[:, 2])
    belly = smooth(.15, .9, -dirn[:, 2])
    col = np.tile(lin(122, 78, 118), (n, 1))
    col = col * (1 - dorsal[:, None]) + lin(80, 48, 86) * dorsal[:, None]
    col = col * (1 - .7 * belly[:, None]) + lin(156, 112, 142) * (.7 * belly[:, None])
    mott = fbm(V / 13., 4, 21) * .5 + .5
    col *= (.78 + .38 * mott)[:, None]
    blot = smooth(.25, .45, fbm(V / 34., 3, 33))
    col *= (1 - .18 * blot)[:, None]
    rough = .58 + .08 * fbm(V / 6., 2, 41)
    wr = np.full(n, .5) + .1 * belly
    # Plates: darker, glossier chitin, each its own shade, pale worn edges from the AO later.
    pd = evaluate_binned([Ellipsoids('add', 0., BODY_INFO['plates'])], V)
    pm = smooth(1.2, -.3, pd)
    shade = .8 + .4 * (vnoise(V / 9., 51) * .5 + .5)
    pc = lin(66, 40, 74) * shade[:, None]
    col = col * (1 - pm[:, None]) + pc * pm[:, None]; rough = rough * (1 - pm) + .34 * pm; wr = wr * (1 - pm) + .12 * pm
    # Warts: paler, rough.
    W = BODY_INFO['warts']
    wd = evaluate_binned([Balls('add', 0., [w[0] for w in W], [w[1] for w in W])], V)
    wm = smooth(.6, -.2, wd)
    col = col * (1 - wm[:, None]) + lin(168, 128, 150) * wm[:, None]; rough = rough * (1 - wm) + .66 * wm
    # Socket ring: the inner rim wet and inflamed, veins round it.
    de = np.linalg.norm(V - E, axis=1)
    rim = smooth(SOCKET + 5, SOCKET + .5, de) * (V[:, 0] > E[0] - 24)
    col = col * (1 - rim[:, None]) + lin(156, 70, 92) * rim[:, None]; rough = rough * (1 - rim) + .22 * rim
    near = smooth(SOCKET + 26, SOCKET + 6, de)
    wr = np.maximum(wr, near * .95)
    # Lips and the mouth.
    r, rxy, a, e = angles(V)
    inside = ((cavity(V) < 2.5) | ((slit(V) < .8) & (r < 97.))) & (de > SOCKET + 1)
    ld = evaluate_binned([Tube('add', 0., pts, rl) for pts, rl in BODY_INFO['lips']], V)
    lm = smooth(2.5, .2, ld) * ~inside
    col = col * (1 - lm[:, None]) + lin(98, 46, 78) * lm[:, None]; rough = rough * (1 - lm) + .34 * lm; wr = np.maximum(wr, lm)
    gum = inside & (np.abs(e - mouth_e(np.clip(a, -A_MAX, A_MAX))) < np.degrees((mouth_gap(np.clip(a, -A_MAX, A_MAX)) + 6.) / r))
    mcol = np.where(gum[:, None], lin(158, 64, 82), lin(104, 24, 38))
    im = inside.astype(float)
    col = col * (1 - im[:, None]) + mcol * im[:, None]; rough = rough * (1 - im) + .15 * im; wr = wr * (1 - im) + .3 * im
    corners = smooth(16, 4, np.minimum(np.linalg.norm(V - surf(A_MAX, mouth_e(A_MAX)), axis=1), np.linalg.norm(V - surf(-A_MAX, mouth_e(A_MAX)), axis=1)))
    wr = np.maximum(wr, corners)
    # Veins: dark lines near the eye and up the stalks' bases.
    vn = 1 - np.abs(fbm(V / 7., 3, 61))
    vein = smooth(.9, .97, vn) * np.maximum(smooth(SOCKET + 30, SOCKET + 4, de), .35)
    col = col * (1 - .6 * vein[:, None]) + lin(54, 30, 86) * (.6 * vein[:, None])
    # Stalks: ringed, the tips darker, the eye holes' rims wet.
    kd, meta = STALK_KD
    sd = np.zeros(n); st_t = np.zeros(n); dmin = np.full(n, 1e9)
    for i in range(n):
        co, idx, dist = kd.find(V[i]); dmin[i] = dist; sd[i] = meta[idx][0]; st_t[i] = meta[idx][1]
    on = dmin < 14
    for st in STALKS:
        m = on & (sd == st['k'])
        if not m.any(): continue
        u = st_t[m] / st['L']
        tipdark = smooth(.78, .98, u)
        c = col[m] * (1 - tipdark[:, None]) + lin(86, 50, 92) * tipdark[:, None]
        rr = np.linalg.norm(V[m] - st['eye'], axis=1)
        wet = smooth(11.5, 8., rr)
        c = c * (1 - wet[:, None]) + lin(156, 70, 92) * wet[:, None]
        col[m] = c; rough[m] = rough[m] * (1 - wet) + .22 * wet
        wr[m] = np.maximum(wr[m], .75 * smooth(14, 22, st_t[m]))
    # Scars: pale, smoother.
    sc = np.full(n, 1e4)
    for pts in BODY_INFO['scars']:
        sc = np.minimum(sc, evaluate_binned([Tube('add', 0., pts, np.full(len(pts), 2.))], V))
    sm = smooth(1.5, -.5, sc)
    col = col * (1 - sm[:, None]) + lin(176, 130, 146) * sm[:, None]; rough = rough * (1 - sm) + .45 * sm; wr = wr * (1 - .6 * sm)
    ao = sdf_ao(V, N, ao_ops)
    return np.clip(col, 0, 1), np.stack([ao, np.clip(rough, .05, 1), np.clip(wr, 0, 1)], 1)


def sdf_ao(V, N, ops):
    occ = np.zeros(len(V))
    for delta, wt in ((.9, .5), (2.8, .35), (8., .25)):
        acc = np.zeros(len(V)); tot = 0.
        for i in range(1, 5):
            d = evaluate_binned(ops, V + N * (i * delta), margin=i * delta)
            w = .5 ** i
            acc += w * np.clip((i * delta - d) / (i * delta), 0, 1); tot += w
        occ += wt * acc / tot
    return np.clip(1 - 1.3 * occ, .05, 1)


# ------------------------------------------------------------------ eye texture
def eye_texture(size):
    y, x = np.mgrid[0:size, 0:size]
    u = (x + .5) / size - .5; v = (y + .5) / size - .5
    rho = np.sqrt(u * u + v * v) / .5; beta = np.arctan2(v, u)
    psi = np.clip(rho, 0, 1) * 70.
    P = np.stack([np.cos(beta) * 30, np.sin(beta) * 30, psi * .4], -1).reshape(-1, 3)
    fib = (vnoise(np.stack([beta.ravel() * 26, psi.ravel() * .35, np.zeros(size * size)], 1), 7) * .5 + .5).reshape(size, size)
    fib2 = (vnoise(np.stack([beta.ravel() * 70, psi.ravel() * .9, np.ones(size * size) * 3], 1), 8) * .5 + .5).reshape(size, size)
    blot = (fbm(P / 4., 3, 9) * .5 + .5).reshape(size, size)
    col = np.zeros((size, size, 3))
    iris_t = np.clip((psi - PUPIL) / (IRIS - PUPIL), 0, 1)
    crimson = lin(176, 26, 26); deep = lin(84, 6, 12); gold = lin(232, 132, 40)
    ic = crimson[None, None] * (.55 + .7 * fib[..., None] * (.6 + .4 * fib2[..., None]))
    ic = ic * (1 - .5 * smooth(.55, .75, blot)[..., None]) + deep * (.5 * smooth(.55, .75, blot))[..., None]
    coll = .45 * np.exp(-((psi - (PUPIL + 4.)) / 1.8) ** 2) * (.3 + .7 * fib2) * (.4 + .6 * fib)
    ic = ic * (1 - coll[..., None]) + gold * coll[..., None]
    ic = ic * (1 - smooth(IRIS - 3, IRIS - .3, psi))[..., None] + lin(30, 4, 8) * smooth(IRIS - 3, IRIS - .3, psi)[..., None]
    ic = ic * (1 - smooth(PUPIL + 1.8, PUPIL, psi))[..., None] + lin(40, 6, 8) * smooth(PUPIL + 1.8, PUPIL, psi)[..., None]
    sclera = lin(228, 212, 180)[None, None] * (.92 + .1 * blot[..., None])
    sclera = sclera * (1 - smooth(58, 70, psi))[..., None] + lin(168, 104, 100) * smooth(58, 70, psi)[..., None]
    # bloodshot: branching vessels from the back toward the iris
    vmask = np.zeros((size, size))
    def plot(ps, be, w):
        px = ((.5 + .5 * (ps / 70.) * np.cos(be)) * size).astype(int); py = ((.5 + .5 * (ps / 70.) * np.sin(be)) * size).astype(int)
        rad = max(1, int(w * size / 2048))
        for dx in range(-rad, rad + 1):
            for dy in range(-rad, rad + 1):
                if dx * dx + dy * dy > rad * rad: continue
                ok = (px + dx >= 0) & (px + dx < size) & (py + dy >= 0) & (py + dy < size)
                np.maximum.at(vmask, (py[ok] + dy, px[ok] + dx), 1.)
    def vessel(ps, be, w, depth):
        pts_p, pts_b = [], []
        stop = rng.uniform(IRIS + 3, 48)
        db = rng.normal(0, .004)
        while ps > stop and w > .6:
            pts_p.append(ps); pts_b.append(be)
            ps -= .12; db += rng.normal(0, .0016); db *= .97; be += db; w *= .9975
            if depth < 3 and rng.random() < .012:
                vessel(ps, be, w * .7, depth + 1)
        if pts_p: plot(np.array(pts_p), np.array(pts_b), w)
    for _ in range(46):
        vessel(70., rng.uniform(0, 2 * np.pi), rng.uniform(2.2, 4.), 0)
    k = 3
    vm = vmask.copy()
    for ax in (0, 1):
        vm = sum(np.roll(vm, s, ax) for s in range(-k, k + 1)) / (2 * k + 1)
    vm = np.clip(vm * 1.6, 0, 1)
    sclera = sclera * (1 - .85 * vm[..., None]) + lin(150, 20, 24) * (.85 * vm[..., None])
    col = np.where((psi < IRIS)[..., None], ic, sclera)
    col = np.where((psi < PUPIL)[..., None], lin(6, 3, 4) * np.ones(3), col)
    edge = smooth(IRIS + 1.5, IRIS - .5, psi)
    col = col * (1 - edge[..., None]) + ic * edge[..., None] * (psi >= PUPIL)[..., None]
    return np.clip(col, 0, 1)


def save_png(name, rgb, colour=True):
    h, w, _ = rgb.shape
    img = bpy.data.images.new(name, w, h, alpha=False, float_buffer=False)
    if not colour: img.colorspace_settings.name = 'Non-Color'
    data = rgb ** (1 / 2.2) if colour else rgb
    rgba = np.concatenate([data, np.ones((h, w, 1))], 2)
    img.pixels.foreach_set(np.clip(rgba, 0, 1).astype(np.float32).ravel())
    img.filepath_raw = str(OUT / f'{name}.png'); img.file_format = 'PNG'; img.save()
    return img


# ------------------------------------------------------------------ bake
def bake_material(ob):
    """The high mesh's look: vertex colour with speckle, procedural wrinkles and pores as bump."""
    m = bpy.data.materials.new('BakeSrc_' + ob.name); m.use_nodes = True
    nt = m.node_tree; N = nt.nodes; L = nt.links
    for x in list(N): N.remove(x)
    out = N.new('ShaderNodeOutputMaterial')
    bsdf = N.new('ShaderNodeBsdfPrincipled')
    col = N.new('ShaderNodeAttribute'); col.attribute_name = 'col'
    aux = N.new('ShaderNodeAttribute'); aux.attribute_name = 'aux'
    sep = N.new('ShaderNodeSeparateColor'); L.new(aux.outputs['Color'], sep.inputs['Color'])
    tc = N.new('ShaderNodeTexCoord')
    speck = N.new('ShaderNodeTexNoise'); speck.inputs['Scale'].default_value = .9; speck.inputs['Detail'].default_value = 6.
    L.new(tc.outputs['Object'], speck.inputs['Vector'])
    sp = N.new('ShaderNodeMapRange'); sp.inputs['To Min'].default_value = .86; sp.inputs['To Max'].default_value = 1.14
    L.new(speck.outputs['Fac'], sp.inputs['Value'])
    mul = N.new('ShaderNodeMix'); mul.data_type = 'RGBA'; mul.blend_type = 'MULTIPLY'; mul.inputs['Factor'].default_value = 1.
    L.new(col.outputs['Color'], mul.inputs['A']); L.new(sp.outputs['Result'], mul.inputs['B'])
    L.new(mul.outputs['Result'], bsdf.inputs['Base Color'])
    # wrinkles: cell edges (creases) + pores + soft lumps
    vor = N.new('ShaderNodeTexVoronoi'); vor.feature = 'DISTANCE_TO_EDGE'; vor.inputs['Scale'].default_value = .3
    L.new(tc.outputs['Object'], vor.inputs['Vector'])
    crease = N.new('ShaderNodeMapRange'); crease.inputs['From Min'].default_value = 0.; crease.inputs['From Max'].default_value = .06
    crease.inputs['To Min'].default_value = -1.; crease.inputs['To Max'].default_value = 0.
    L.new(vor.outputs['Distance'], crease.inputs['Value'])
    vor2 = N.new('ShaderNodeTexVoronoi'); vor2.feature = 'DISTANCE_TO_EDGE'; vor2.inputs['Scale'].default_value = 1.05
    L.new(tc.outputs['Object'], vor2.inputs['Vector'])
    crease2 = N.new('ShaderNodeMapRange'); crease2.inputs['From Max'].default_value = .07; crease2.inputs['To Min'].default_value = -.15; crease2.inputs['To Max'].default_value = 0.
    L.new(vor2.outputs['Distance'], crease2.inputs['Value'])
    pores = N.new('ShaderNodeTexNoise'); pores.inputs['Scale'].default_value = 2.2; pores.inputs['Detail'].default_value = 2.
    L.new(tc.outputs['Object'], pores.inputs['Vector'])
    lump = N.new('ShaderNodeTexNoise'); lump.inputs['Scale'].default_value = .35; lump.inputs['Detail'].default_value = 4.
    L.new(tc.outputs['Object'], lump.inputs['Vector'])
    a1 = N.new('ShaderNodeMath'); a1.operation = 'ADD'; L.new(crease.outputs['Result'], a1.inputs[0]); L.new(crease2.outputs['Result'], a1.inputs[1])
    pw = N.new('ShaderNodeMath'); pw.operation = 'MULTIPLY'; pw.inputs[1].default_value = .1; L.new(pores.outputs['Fac'], pw.inputs[0])
    a2 = N.new('ShaderNodeMath'); a2.operation = 'ADD'; L.new(a1.outputs['Value'], a2.inputs[0]); L.new(pw.outputs['Value'], a2.inputs[1])
    lw = N.new('ShaderNodeMath'); lw.operation = 'MULTIPLY'; lw.inputs[1].default_value = .8; L.new(lump.outputs['Fac'], lw.inputs[0])
    a3 = N.new('ShaderNodeMath'); a3.operation = 'ADD'; L.new(a2.outputs['Value'], a3.inputs[0]); L.new(lw.outputs['Value'], a3.inputs[1])
    bump = N.new('ShaderNodeBump'); bump.inputs['Distance'].default_value = .16
    L.new(sep.outputs['Blue'], bump.inputs['Strength']); L.new(a3.outputs['Value'], bump.inputs['Height'])
    L.new(bump.outputs['Normal'], bsdf.inputs['Normal'])
    L.new(sep.outputs['Green'], bsdf.inputs['Roughness'])
    L.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    # the ORM pass: emission of (AO, roughness, wrinkle)
    emit = N.new('ShaderNodeEmission'); emit.name = 'ORM'
    L.new(aux.outputs['Color'], emit.inputs['Color'])
    ob.data.materials.clear(); ob.data.materials.append(m)
    return m, out, bsdf, emit


def bake_all(low, highs):
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 1
    sc.render.bake.use_selected_to_active = True
    sc.render.bake.cage_extrusion = .8; sc.render.bake.max_ray_distance = 4.
    sc.render.bake.margin = 8; sc.render.bake.use_clear = True
    mats = [bake_material(h) for h in highs]
    skin = low.data.materials[0]; skin.use_nodes = True
    imgs = {}
    for name, colour in (('T_Beholder_D', True), ('T_Beholder_N', False), ('T_Beholder_ORM', False)):
        img = bpy.data.images.new(name, TEX, TEX, alpha=False, float_buffer=not colour)
        if not colour: img.colorspace_settings.name = 'Non-Color'
        imgs[name] = img
    node = skin.node_tree.nodes.new('ShaderNodeTexImage')
    bpy.ops.object.select_all(action='DESELECT')
    for h in highs: h.select_set(True)
    low.select_set(True); bpy.context.view_layer.objects.active = low
    for name, kind, kw in (('T_Beholder_N', 'NORMAL', dict(normal_space='TANGENT')),
                           ('T_Beholder_D', 'DIFFUSE', dict(pass_filter={'COLOR'})),
                           ('T_Beholder_ORM', 'EMIT', {})):
        node.image = imgs[name]; skin.node_tree.nodes.active = node
        for m, out, bsdf, emit in mats:
            L = m.node_tree.links
            L.new((emit if kind == 'EMIT' else bsdf).outputs[0], out.inputs['Surface'])
        t = time.time()
        bpy.ops.object.bake(type=kind, use_selected_to_active=True, margin=8, **kw)
        log('bake', name, f'{time.time() - t:.1f}s')
        img = imgs[name]
        img.filepath_raw = str(OUT / f'{name}.png'); img.file_format = 'PNG'
        img.save()
    skin.node_tree.nodes.remove(node)
    return imgs


# ------------------------------------------------------------------ rig
def build_rig():
    arm = bpy.data.armatures.new('BeholderRig'); rig = bpy.data.objects.new('BeholderRig', arm)
    bpy.context.scene.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    eb = {}
    def bone(name, head, tail, parent=None):
        b = arm.edit_bones.new(name); b.head = tuple(map(float, head)); b.tail = tuple(map(float, tail))
        if parent: b.parent = eb[parent]
        eb[name] = b
    bone('root', (0, 0, 0), (0, 0, -20))
    bone('body', (0, 0, 0), (0, 0, 30), 'root')
    bone('jaw', JAW, JAW + np.array([40., 0, -22.]), 'body')
    tl, tr = tongue_line()
    bone('tongue_0', tl[0], tl[len(tl) // 2], 'jaw')
    bone('tongue_1', tl[len(tl) // 2], tl[-1], 'tongue_0')
    bone('lid_upper', E, E + np.array([0, 0, 20.]), 'body')
    bone('lid_lower', E, E + np.array([0, 0, -20.]), 'body')
    bone('eye', E, E + np.array([25., 0, 0]), 'body')
    for st in STALKS:
        k = st['k']; s = st['s']; pts = st['samples']
        S0, S1 = 9., st['L'] - 2.
        bs = np.linspace(S0, S1, 7)
        st['bone_s'] = bs
        def at(x): return np.array([np.interp(x, s, pts[:, i]) for i in range(3)])
        for i in range(6):
            bone(f'stalk{k}_{i}', at(bs[i]), at(bs[i + 1]), 'body' if i == 0 else f'stalk{k}_{i - 1}')
        bone(f'stalk{k}_eye', st['eye'], st['eye'] + st['gaze'] * 8., f'stalk{k}_5')
    bpy.ops.object.mode_set(mode='OBJECT')
    return rig


def add_weights(ob, weights):
    """weights: dict bone -> (N,) array."""
    for name, w in weights.items():
        idx = np.nonzero(w > 1e-3)[0]
        if not len(idx): continue
        vg = ob.vertex_groups.get(name) or ob.vertex_groups.new(name=name)
        for i in idx: vg.add([int(i)], float(w[i]), 'REPLACE')


def body_weights(V):
    n = len(V)
    W = {}
    r, rxy, a, e = angles(V)
    ac = np.clip(a, -A_MAX, A_MAX)
    below = smooth(1.5, -3.5, e - mouth_e(ac))                                  # below the slit's middle
    az = smooth(A_MAX + 16, A_MAX - 2, np.abs(a))
    chin = smooth(-88, -66, e)
    front = smooth(JAW[0] - 30, JAW[0] - 6, V[:, 0])
    jaw = below * az * chin * front
    jaw[np.linalg.norm(V - E, axis=1) < SOCKET + 3] = 0
    kd, meta = STALK_KD
    stalk_w = {}
    rest = np.ones(n)
    for i in range(n):
        co, idx, dist = kd.find(V[i])
        k, sv = meta[idx]
        st = STALKS[k]
        rad = float(np.interp(sv, st['s'], st['radius']))
        in_bulb = np.linalg.norm(V[i] - st['bulb']) < 15.       # the bulb is far fatter than the stalk's tip
        if dist > rad * 1.8 + 4 and not in_bulb: continue
        if in_bulb: sv = st['L']
        w = float(smooth(4., 15., sv))
        if w <= 0: continue
        bs = st['bone_s']; cen = (bs[:-1] + bs[1:]) / 2
        if sv <= cen[0]: parts = [(0, 1.)]
        elif sv >= cen[-1]: parts = [(5, 1.)]
        else:
            j = int(np.searchsorted(cen, sv)) - 1; f = (sv - cen[j]) / (cen[j + 1] - cen[j])
            parts = [(j, 1 - f), (j + 1, f)]
        for j, f in parts:
            stalk_w.setdefault(f'stalk{k}_{j}', np.zeros(n))[i] += w * f
        rest[i] -= w
    W['jaw'] = rest * jaw
    W['body'] = rest * (1 - jaw)
    W.update(stalk_w)
    return W


# ------------------------------------------------------------------ build
def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = 'METRIC'; sc.unit_settings.scale_length = .01


def build():
    clear_scene()
    stalk_layout()
    global STALK_KD
    tot = sum(len(st['samples']) for st in STALKS)
    kd = kdtree.KDTree(tot); meta = []
    for st in STALKS:
        for p, s in zip(st['samples'], st['s']):
            kd.insert(Vector(p), len(meta)); meta.append((st['k'], s))
    kd.balance(); STALK_KD = (kd, meta)
    B = body_ops(); LU = lid_ops(True); LL = lid_ops(False); TG = tongue_ops()
    eyeball = [Func('add', 0., E - R_EYE, E + R_EYE, lambda P: np.linalg.norm(P - E, axis=1) - R_EYE)]
    occluders = LU + LL + TG + eyeball
    parts = {'body': B, 'lid_upper': LU, 'lid_lower': LL, 'tongue': TG}
    skin_mat = bpy.data.materials.new('Skin')
    lows, highs = [], []
    for name, ops in parts.items():
        V, Q, N = field_mesh(name + '_low', ops, LOW_H if name == 'body' else LOW_PART)
        lo = make_mesh(name + '_low', V, Q); lo.data.materials.append(skin_mat)
        lows.append((name, lo, V))
        V, Q, N = field_mesh(name + '_high', ops, HIGH_H)
        hi = make_mesh(name + '_high', V, Q)
        kind = 'body' if name == 'body' else ('tongue' if name == 'tongue' else 'lid')
        ao_ops = [op for o in parts.values() for op in o] + eyeball      # everything shades everything
        col, aux = surface_look(V, N, ao_ops if name != 'body' else B + occluders, kind)
        a1 = hi.data.color_attributes.new('col', 'FLOAT_COLOR', 'POINT')
        a1.data.foreach_set('color', np.concatenate([col, np.ones((len(col), 1))], 1).astype(np.float32).ravel())
        a2 = hi.data.color_attributes.new('aux', 'FLOAT_COLOR', 'POINT')
        a2.data.foreach_set('color', np.concatenate([aux, np.ones((len(aux), 1))], 1).astype(np.float32).ravel())
        highs.append(hi)
        log('look', name, f'ao_mean={aux[:, 0].mean():.2f}')
    rig = build_rig()
    # weights on the low skin parts
    for name, ob, V in lows:
        if name == 'body': add_weights(ob, body_weights(V))
        elif name == 'tongue':
            tl, tr = tongue_line()
            rr = np.linalg.norm(V, axis=1)
            t1 = smooth(48, 62, rr)
            add_weights(ob, {'tongue_0': 1 - t1, 'tongue_1': t1})
        else: add_weights(ob, {name: np.ones(len(V))})
    # join the skin, unwrap it once, bake
    bpy.ops.object.select_all(action='DESELECT')
    for _, ob, _ in lows: ob.select_set(True)
    bpy.context.view_layer.objects.active = lows[0][1]
    bpy.ops.object.join()
    skin = bpy.context.view_layer.objects.active; skin.name = 'Beholder'
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=.002, area_weight=0.)
    bpy.ops.object.mode_set(mode='OBJECT')
    log('uv', f'faces={len(skin.data.polygons)}')
    bake_all(skin, highs)
    for h in highs: bpy.data.objects.remove(h)
    # eyes, cornea, teeth
    eye_mat, cornea_mat, teeth_mat = bpy.data.materials.new('Eye'), bpy.data.materials.new('Cornea'), bpy.data.materials.new('Teeth')
    extra = []
    main = sphere_eye('eye_main', E, [1., 0., 0.], R_EYE, 40, 72, psi_max=160.); main.data.materials.append(eye_mat)
    add_weights(main, {'eye': np.ones(len(main.data.vertices))}); extra.append(main)
    co = cornea(); co.data.materials.append(cornea_mat); add_weights(co, {'eye': np.ones(len(co.data.vertices))}); extra.append(co)
    for st in STALKS:
        s = sphere_eye(f'eye_{st["k"]}', st['eye'], st['gaze'], 7., 16, 32, psi_max=150.); s.data.materials.append(eye_mat)
        add_weights(s, {f'stalk{st["k"]}_eye': np.ones(len(s.data.vertices))}); extra.append(s)
    th, owner = teeth(); th.data.materials.append(teeth_mat)
    th.data.uv_layers.new(name='UVMap')
    add_weights(th, {'body': owner.astype(float), 'jaw': (~owner).astype(float)}); extra.append(th)
    bpy.ops.object.select_all(action='DESELECT')
    for o in extra: o.select_set(True)
    skin.select_set(True); bpy.context.view_layer.objects.active = skin
    bpy.ops.object.join()
    ob = skin
    ob.parent = rig
    m = ob.modifiers.new('Armature', 'ARMATURE'); m.object = rig
    # every vertex weighted?
    wsum = np.zeros(len(ob.data.vertices))
    for v in ob.data.vertices:
        for g in v.groups: wsum[v.index] += g.weight
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    log('mesh', f'tris={tris}', f'verts={len(ob.data.vertices)}', f'bones={len(rig.data.bones)}', f'unweighted={(wsum < .99).sum()}',
        f'materials={[x.name for x in ob.data.materials]}')
    return rig, ob


FBX = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False,
           primary_bone_axis='Y', secondary_bone_axis='X', use_armature_deform_only=False,
           mesh_smooth_type='FACE')


def review_materials(ob):
    imgs = {n: bpy.data.images.get(n) for n in ('T_Beholder_D', 'T_Beholder_N', 'T_Beholder_ORM')}
    eye_img = bpy.data.images.load(str(OUT / 'T_BeholderEye.png'))
    for m in ob.data.materials:
        m.use_nodes = True; nt = m.node_tree; N = nt.nodes; L = nt.links
        for x in list(N): N.remove(x)
        out = N.new('ShaderNodeOutputMaterial'); b = N.new('ShaderNodeBsdfPrincipled'); L.new(b.outputs[0], out.inputs[0])
        if m.name == 'Skin':
            d = N.new('ShaderNodeTexImage'); d.image = imgs['T_Beholder_D']
            nn = N.new('ShaderNodeTexImage'); nn.image = imgs['T_Beholder_N']
            o = N.new('ShaderNodeTexImage'); o.image = imgs['T_Beholder_ORM']
            sep = N.new('ShaderNodeSeparateColor'); L.new(o.outputs['Color'], sep.inputs[0])
            mul = N.new('ShaderNodeMix'); mul.data_type = 'RGBA'; mul.blend_type = 'MULTIPLY'; mul.inputs['Factor'].default_value = 1.
            L.new(d.outputs['Color'], mul.inputs['A']); L.new(sep.outputs['Red'], mul.inputs['B'])
            L.new(mul.outputs['Result'], b.inputs['Base Color'])
            nm = N.new('ShaderNodeNormalMap'); L.new(nn.outputs['Color'], nm.inputs['Color']); L.new(nm.outputs['Normal'], b.inputs['Normal'])
            L.new(sep.outputs['Green'], b.inputs['Roughness'])
            b.inputs['Subsurface Weight'].default_value = .15
            b.inputs['Subsurface Radius'].default_value = (1., .2, .3)
        elif m.name == 'Eye':
            t = N.new('ShaderNodeTexImage'); t.image = eye_img; L.new(t.outputs['Color'], b.inputs['Base Color'])
            b.inputs['Roughness'].default_value = .12
        elif m.name == 'Cornea':
            b.inputs['Base Color'].default_value = (1, 1, 1, 1); b.inputs['Roughness'].default_value = .02
            b.inputs['Transmission Weight'].default_value = 1.; b.inputs['Alpha'].default_value = .12
            m.blend_method = 'BLEND' if hasattr(m, 'blend_method') else None
        else:
            a = N.new('ShaderNodeAttribute'); a.attribute_name = 'Col'; L.new(a.outputs['Color'], b.inputs['Base Color'])
            b.inputs['Roughness'].default_value = .35


def review(rig, ob):
    import os
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'; sc.cycles.samples = 48; sc.cycles.use_denoising = True
    review_materials(ob)
    world = bpy.data.worlds.new('W'); sc.world = world; world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (.05, .05, .06, 1)
    for nm, rot, en, col in (('Key', (55, 0, 35), 4., (1., .85, .7)), ('Rim', (60, 0, 200), 3., (.6, .7, 1.)), ('Fill', (80, 0, -60), 1.2, (1, 1, 1))):
        l = bpy.data.objects.new(nm, bpy.data.lights.new(nm, 'SUN')); sc.collection.objects.link(l)
        l.rotation_euler = tuple(math.radians(x) for x in rot); l.data.energy = en; l.data.color = col
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
    sc.render.resolution_x, sc.render.resolution_y = 1100, 1100
    def shot(name, loc, look, lens=35):
        cam.location = loc; d = Vector(look) - Vector(loc); cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
        cam.data.lens = lens; cam.data.clip_end = 5000
        sc.render.filepath = os.path.join(REVIEW, f'Beholder_{name}.png'); bpy.ops.render.render(write_still=True)
    shot('front', (520, 60, 40), (0, 0, 40))
    shot('three', (380, -380, 140), (0, 0, 40))
    shot('side', (0, -560, 60), (0, 0, 50))
    shot('back', (-420, 260, 200), (0, 0, 40))
    shot('top', (60, -40, 560), (0, 0, 60))
    shot('below', (420, -120, -320), (0, 0, 0))
    shot('eye', (230, -40, 30), (80, 0, 10), 50)
    shot('mouth', (210, 70, -60), (70, 0, -60), 45)
    # posed: jaw open, lid half shut, stalks curling
    bpy.context.view_layer.objects.active = rig; bpy.ops.object.mode_set(mode='POSE')
    pb = rig.pose.bones
    def turn(name, axis, deg):
        b = pb[name]; b.rotation_mode = 'QUATERNION'
        from mathutils import Quaternion
        M = rig.data.bones[name].matrix_local.to_3x3()
        b.rotation_quaternion = Quaternion(M.inverted() @ Vector(axis), math.radians(deg)) @ b.rotation_quaternion
    turn('jaw', (0, 1, 0), 24); turn('lid_upper', (0, 1, 0), 16); turn('eye', (0, 1, 0), 10)
    for st in STALKS:
        for i in range(1, 6):
            turn(f'stalk{st["k"]}_{i}', tuple(np.cross(st['gaze'], [0, 0, 1]) / max(1e-6, np.linalg.norm(np.cross(st['gaze'], [0, 0, 1])))), 9 * math.sin(st['k'] + i))
    bpy.ops.object.mode_set(mode='OBJECT')
    shot('posed', (420, -300, 80), (0, 0, 30))
    shot('posed_mouth', (230, -60, -70), (70, 0, -60), 45)
    bpy.context.view_layer.objects.active = rig; bpy.ops.object.mode_set(mode='POSE')
    for b in pb: b.rotation_quaternion = (1, 0, 0, 0)
    bpy.ops.object.mode_set(mode='OBJECT')


if __name__ == '__main__':
    save_png('T_BeholderEye', eye_texture(512 if QUICK else 2048))
    rig, ob = build()
    if REVIEW: review(rig, ob)
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True); ob.select_set(True); bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SK_Beholder.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'}, bake_anim=False, **FBX)
    info = {'generator': 'Tools/build_beholder.py', 'blender': bpy.app.version_string, 'quick': QUICK,
            'body_radii_cm': R_BODY.tolist(), 'eye_centre': E.tolist(), 'eye_radius': R_EYE, 'jaw_hinge': JAW.tolist(),
            'stalks': [{'k': st['k'], 'base': st['base'].tolist(), 'eye': st['eye'].tolist(), 'gaze': st['gaze'].tolist(), 'length_cm': round(st['L'], 1)} for st in STALKS],
            'bones': [b.name for b in rig.data.bones], 'tris': sum(len(p.vertices) - 2 for p in ob.data.polygons), 'texture': TEX}
    (OUT / 'beholder.json').write_text(json.dumps(info, indent=1) + '\n', encoding='utf-8')
    log('done', OUT / 'SK_Beholder.fbx')
