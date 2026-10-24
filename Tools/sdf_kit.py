"""Signed-distance-field modelling kit for Blender (Claude, 2026-10-24; first written for the beholder,
Tools/build_beholder.py). Numpy only, plus bpy for making meshes.

* Fields are lists of Ops evaluated in order: 'add' (smooth union), 'sub' (smooth subtraction), 'int'
  (smooth intersection) and 'disp' (a displacement added to the surface, outward positive). Every op
  has a bounding box so that blocks of points skip what is far away (`evaluate`'s margin keeps that
  exact for distances the caller relies on).
* `polygonize` meshes a field by surface nets over sparse blocks; `project` moves vertices onto the
  zero set by Newton steps; `field_mesh` does both and makes a Blender mesh-ready array.
"""
import math
import time

import bpy
import numpy as np

T0 = time.time()


def log(*a):
    print('CHUCK_BEHOLDER', f'{time.time() - T0:6.1f}s', *a, flush=True)


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


class Disp(Op):
    """A displacement of the surface so far (outward positive), as a function of position."""
    def __init__(self, lo, hi, f):
        self.kind, self.k, self.lo, self.hi, self.f = 'disp', 0., np.asarray(lo, float), np.asarray(hi, float), f

    def eval(self, P, lo, hi):
        return self.f(P) if self.box_hit(lo, hi) else None


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
        elif op.kind == 'disp': d = d - s
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


def frame(z):
    """Local axes (columns x, y, z) with z along the given direction."""
    z = z / np.linalg.norm(z)
    x = np.cross([0., 0., 1.], z)
    if np.linalg.norm(x) < 1e-3: x = np.cross([0., 1., 0.], z)
    x /= np.linalg.norm(x); y = np.cross(z, x)
    return np.stack([x, y, z], 1)



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
    lo = np.min([op.lo - op.k for op in ops if op.kind == 'add'], 0) - pad      # displacements stay within pad
    hi = np.max([op.hi + op.k for op in ops if op.kind == 'add'], 0) + pad
    V, Q = polygonize(ops, h, lo, hi)
    V, N = project(ops, V, h * .5)
    log(name, f'h={h}', f'verts={len(V)}', f'quads={len(Q)}')
    return V, Q, N


