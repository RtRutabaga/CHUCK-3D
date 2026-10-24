"""Blender 4.5.14: the beholder of the temple sanctum (Claude; docs/CHULT-TEMPLE-BEHOLDER.md).

2026-10-23, user: "start creating the beholder, it should look really cool and detailed".
2026-10-24, user: "Make the beholder look more like this [a colour concept and a clay render], use free
resources where helpful". The second pass follows those references: a lumpy, brain-like cranium tapering
to a chin; a great green eye with a slit pupil, deep under a massive brow and stacked bags; a huge gaping
maw filling the lower face, ringed with thick lips, ridged red gums and rows of long fangs, a broad tongue
lolling out over the lower teeth; thick wrinkled eyestalks hooking forward like question marks, each
cupping a big slit-pupilled eye; wet maroon skin, tan-pink on its ridges, dark in its folds.

blender --background --factory-startup --python Tools/build_beholder.py [-- --review DIR] [--quick]
  -> SourceAssets/Enemies/Beholder/SK_Beholder.fbx       skinned, no clips (ATempleBeholder poses it)
     SourceAssets/Enemies/Beholder/T_Beholder_D.png      skin colour (baked)
     SourceAssets/Enemies/Beholder/T_Beholder_N.png      skin normal (baked from the sculpt-detail mesh)
     SourceAssets/Enemies/Beholder/T_Beholder_ORM.png    R ambient occlusion, G roughness, B wrinkle mask
     SourceAssets/Enemies/Beholder/T_BeholderEye.png     green iris, slit pupil, bloodshot sclera (polar UVs)
     SourceAssets/Enemies/Beholder/beholder.json         sizes, bones and where the eyes are
Needs SourceAssets/Surfaces/Beholder/leather037_disp_2k.jpg (Tools/Fetch-BeholderSkin.ps1, ambientCG, CC0):
its pebbled grain, box-projected at 70 cm a tile, is the skin's finest detail in the bake.

How it is made (Tools/sdf_kit.py). The creature is a signed distance field: ellipsoids for the cranium,
jaw, chin and forehead; sunk spheres for the cranium's lobes and lumps; tubes laid on the face (snapped to
its surface) for the brow, the bags under the eye, the folds from eye to mouth, forehead furrows, lips,
chin folds and gums; spikes along the jaw; ten eyestalks with cupped bulbs; the socket, the maw's
opening and its cavity carved out; and, for the sculpt-detail mesh only, a displacement of wrinkle folds
round the eye and mouth, furrows on the forehead, lumps and big pebbles. Separate fields make the two
eyelids and the tongue. Each is meshed twice by surface nets (0.75 cm for baking, 3 cm for the game; lids
and tongue 1.8 cm) and the vertices moved onto the surface. Colour follows the surface's curvature (from
the field's Laplacian): pale on ridges, dark in folds. Cycles bakes colour, normal and roughness/AO.

Bones: root, body, jaw (hinged at the maw's corners; the maw is open at rest), tongue_0..3, lid_upper,
lid_lower, eye, and for each stalk k = 0..9 stalk{k}_0..5 and stalk{k}_eye. Facing +X, centimetres, Z up.
"""
from pathlib import Path
import importlib.util
import json
import math
import sys
import time

import bpy
import numpy as np
from mathutils import Vector, kdtree

ROOT = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location('sdf_kit', ROOT / 'Tools/sdf_kit.py')
kit = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(kit)
from_kit = ['log', 'smooth', 'vnoise', 'fbm', 'sd_ellipsoid', 'Func', 'Tube', 'Balls', 'Disp', 'Ellipsoids', 'evaluate',
            'evaluate_binned', 'gradient', 'frame', 'make_mesh', 'mesh_from', 'field_mesh']
globals().update({n: getattr(kit, n) for n in from_kit})

OUT = ROOT / 'SourceAssets/Enemies/Beholder'
OUT.mkdir(parents=True, exist_ok=True)
GRAIN = ROOT / 'SourceAssets/Surfaces/Beholder/leather037_disp_2k.jpg'
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None
QUICK = '--quick' in argv
HIGH_H = 1.4 if QUICK else 0.75
LOW_H, LOW_PART = 3.0, 1.8
TEX = 1024 if QUICK else 4096
rng = np.random.default_rng(20261024)


# ------------------------------------------------------------------ proportions (cm)
E = np.array([66., 0., 28.])            # the great eye's centre (a third of the face across, 37 % down)
R_EYE = 33.
SOCKET = R_EYE + 8.
IRIS = 32.                              # degrees from the eye's axis; the slit pupil is inside it
MY = 70.                                # the maw's half-width at its corners
JAW = np.array([38., 0., -20.])         # the hinge, through the maw's corners
STALK_EYE = 13.


def lid_edge(alpha, upper):
    """Lid margins (deg above the eye's horizon) at alpha (deg round its axis): an almond, wide open."""
    if upper: return 30. - 12. * (alpha / 60.) ** 2
    return -32. + 12. * (alpha / 60.) ** 2


def z_up(y):
    """The maw's upper edge (front view): nearly level, dipping a little in the middle."""
    u = np.clip(np.asarray(y, float) / MY, -1, 1)
    return -18. - 6. * (1 - u * u)


def z_low(y):
    """The maw's lower edge: a deep U down toward the chin."""
    u = np.clip(np.asarray(y, float) / MY, -1, 1)
    return -18. - 72. * np.clip(1 - u * u, 0, 1) ** .75


def mouth2d(y, z):
    """Approximate distance (cm, front view) outside the maw's opening (negative inside)."""
    return np.maximum.reduce([z - z_up(y), z_low(y) - z, np.abs(y) - MY])


# The plain head, for placing things on its surface.
CRANIUM = (np.array([-5., 0., 44.]), np.array([85., 96., 82.]))
JAWMASS = (np.array([22., 0., -45.]), np.array([75., 82., 70.]))
CHIN = (np.array([42., 0., -92.]), np.array([42., 48., 26.]))
FOREHEAD = (np.array([58., 0., 70.]), np.array([30., 48., 26.]))
BASE = [Func('add', 0., CRANIUM[0] - CRANIUM[1], CRANIUM[0] + CRANIUM[1], lambda P: sd_ellipsoid(P, *CRANIUM)),
        Func('add', 30., JAWMASS[0] - JAWMASS[1], JAWMASS[0] + JAWMASS[1], lambda P: sd_ellipsoid(P, *JAWMASS)),
        Func('add', 18., CHIN[0] - CHIN[1], CHIN[0] + CHIN[1], lambda P: sd_ellipsoid(P, *CHIN)),
        Func('add', 16., FOREHEAD[0] - FOREHEAD[1], FOREHEAD[0] + FOREHEAD[1], lambda P: sd_ellipsoid(P, *FOREHEAD))]


def front_x(y, z):
    """Where the plain head's front surface is (x) at front-view (y, z)."""
    y = np.atleast_1d(np.asarray(y, float)); z = np.broadcast_to(np.asarray(z, float), y.shape)
    lo = np.full(y.shape, 10.); hi = np.full(y.shape, 170.)       # x=10 is inside the head for every face point used
    for _ in range(36):
        mid = (lo + hi) / 2
        inside = evaluate(BASE, np.stack([mid, y, z], 1), margin=200.) < 0
        lo = np.where(inside, mid, lo); hi = np.where(inside, hi, mid)
    return (lo + hi) / 2


def on_face(y, z, out=0.):
    """Front-view points snapped onto the plain head, pushed `out` cm along +x."""
    y = np.atleast_1d(np.asarray(y, float)); z = np.broadcast_to(np.asarray(z, float), y.shape)
    return np.stack([front_x(y, z) + out, y, z], 1)


def ray_surface(dirs, origin=np.zeros(3)):
    """Where rays from inside the head (origin) along unit dirs (N,3) leave the plain head."""
    lo = np.zeros(len(dirs)); hi = np.full(len(dirs), 220.)
    for _ in range(36):
        mid = (lo + hi) / 2
        inside = evaluate(BASE, origin + dirs * mid[:, None], margin=250.) < 0
        lo = np.where(inside, mid, lo); hi = np.where(inside, hi, mid)
    return origin + dirs * ((lo + hi) / 2)[:, None]


def base_normal(P):
    g = gradient(BASE, np.atleast_2d(P))
    return g / np.linalg.norm(g, axis=1, keepdims=True)


def unit(a, e):
    a, e = np.radians(a), np.radians(e)
    return np.stack([np.cos(e) * np.cos(a), np.cos(e) * np.sin(a), np.sin(e)], -1)


def catmull(Q, n=600):
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


def resample(C, step):
    seg = np.linalg.norm(np.diff(C, axis=0), axis=1); s = np.concatenate([[0], np.cumsum(seg)])
    ss = np.append(np.arange(0, s[-1], step), s[-1])
    return np.stack([np.interp(ss, s, C[:, i]) for i in range(3)], 1), ss


# ------------------------------------------------------------------ the stalks
STALKS = []


def stalk_layout():
    """Ten thick stalks from the cranium's sides and top. Each leaves the head along its surface's normal,
    swells up and out in a smooth arc, then hooks forward and down like a question mark; the eye on its end
    faces the front. The direction blends smoothly along it (a cubic Bezier of directions), so no kinks."""
    spec = [(35, 58, 96), (-35, 58, 92), (75, 42, 100), (-75, 42, 102), (100, 14, 96), (-100, 14, 94),
            (118, -16, 86), (-118, -16, 88), (14, 80, 102), (-14, 80, 98)]
    up = np.array([0., 0., 1.]); fwd = np.array([1., 0., 0.])
    bases = ray_surface(np.array([unit(a + rng.uniform(-4, 4), e + rng.uniform(-3, 3)) for a, e, _ in spec]), CRANIUM[0] * np.array([1, 0, 1]))
    normals = base_normal(bases)
    nrm = lambda v: v / np.linalg.norm(v)
    for k, ((a, e, L), base, n) in enumerate(zip(spec, bases, normals)):
        P0 = base - n * 9.
        horiz = np.array([n[0], n[1], 0.])
        horiz = nrm(horiz) if np.linalg.norm(horiz) > .2 else nrm(np.array([.2, np.sign(a), 0.]))
        lift = .8 if e > 30 else .45 if e > 0 else .25
        d1 = nrm(n + up * lift)
        d2 = nrm(fwd * .9 + up * .25 + horiz * .45)
        d3 = nrm(fwd * .75 + horiz * .25 - up * (.75 if e > 30 else .45))
        t = np.linspace(0, 1, 400)[:, None]
        T = n * (1 - t) ** 3 + 3 * d1 * (1 - t) ** 2 * t + 3 * d2 * (1 - t) * t ** 2 + d3 * t ** 3
        T += np.cross(T, up) * (.12 * np.sin(np.pi * t) * (1 if k % 2 else -1))           # a slight sideways S
        T /= np.linalg.norm(T, axis=1, keepdims=True)
        C = P0 + np.cumsum(T, 0) * (L / len(t))
        pts, ss = resample(C, .9)
        Ltot = ss[-1]; u = ss / Ltot
        rad = 10. + 2.8 * (1 - u) ** 1.5 + 5.5 * np.exp(-((ss - 12) / 8.) ** 2) + .45 * np.sin(2 * np.pi * ss / 3.) * (.6 + .4 * u) \
            + 1.5 * np.exp(-((u - .94) / .05) ** 2)
        tang = nrm(pts[-1] - pts[-4])
        gaze = nrm(tang + fwd * .7)
        bulb = pts[-1] + tang * 3.
        STALKS.append(dict(k=k, samples=pts, s=ss, radius=rad, L=Ltot, base=base, tip=pts[-1], gaze=gaze, bulb=bulb,
                           eye=bulb + gaze * 4., normal=n))


# ------------------------------------------------------------------ the body's field
INFO = {}


def tube_on_face(y, z, rad, out=0., k=3., kind='add'):
    pts = on_face(y, z, out)
    return Tube(kind, k, pts, np.broadcast_to(np.asarray(rad, float), (len(pts),)).copy()), pts


def body_ops():
    ops = list(BASE)
    ops[0] = Func('add', 0., CRANIUM[0] - CRANIUM[1] - 6, CRANIUM[0] + CRANIUM[1] + 6, lambda P: sd_ellipsoid(P, *CRANIUM) - 2.2 * fbm(P / 30., 3, 11))
    # The cranium's lobes: big sunk spheres over the top, sides and back (brain-like).
    dirs, rads = [], []
    while len(dirs) < 11:
        a, e = rng.uniform(-180, 180), rng.uniform(15, 82)
        if abs(a) < 40 and e < 62: continue
        dirs.append(unit(a, e)); rads.append(rng.uniform(20, 30))
    P = ray_surface(np.array(dirs), CRANIUM[0] * np.array([1, 0, 1])); n = base_normal(P)
    ops.append(Balls('add', 14., P - n * np.array(rads)[:, None] * .74, rads))
    # Lumps at the temples (a big one on its left, as the reference) and on the cheeks.
    lumps = [((60., 50.), 14., .55), ((-58., 54.), 10., .6), ((74., 6.), 8., .55), ((-72., 12.), 9., .55), ((40., 82.), 9., .6)]
    LP = on_face([l[0][0] for l in lumps], [l[0][1] for l in lumps])
    ops.append(Balls('add', 6., LP - np.array([[l[1] * l[2], 0, 0] for l in lumps]), [l[1] for l in lumps]))
    # Cheek masses at the maw's corners.
    ch = []
    for s in (1, -1):
        c = on_face(s * 75., -20., -9.)[0]
        ch.append((c, np.array([16., 15., 17.]), frame(base_normal(c)[0])))
    ops.append(Ellipsoids('add', 10., ch))
    # The brow: a massive ridge over the eye, overhanging it, thickest in the middle.
    ph = np.linspace(8, 172, 70); p = np.radians(ph)
    taper = np.sqrt(np.clip(np.sin(np.pi * (ph - 8) / 164), 0, 1)) * .85 + .15
    t, _ = tube_on_face(50 * np.cos(p), E[2] + 39 * np.sin(p) - 2, (7. + 9. * np.exp(-((ph - 90) / 42) ** 2)) * taper, out=6., k=8.)
    ops.append(t)
    # Bags under the eye, stacked, and the folds from its corners down to the maw.
    for j in range(3):
        ph = np.linspace(196, 344, 60); p = np.radians(ph)
        t, _ = tube_on_face((38 + 6 * j) * np.cos(p), E[2] + (31 + 5.5 * j) * np.sin(p), (4.6 - .6 * j) * (np.sin(np.pi * (ph - 196) / 148) * .7 + .3), out=-.5, k=3.)
        ops.append(t)
    for s in (1, -1):
        for j in range(2):
            C = catmull([[0, s * (42 + 7 * j), 22 - 3 * j], [0, s * (60 + 7 * j), 4 - 3 * j], [0, s * (72 + 6 * j), -12 - 4 * j], [0, s * (78 + 4 * j), -26]], 80)
            t, _ = tube_on_face(C[:, 1], C[:, 2], 2.6 - .4 * j, out=-.6, k=3.5)
            ops.append(t)
    # Furrows over the brow.
    for j in range(2):
        ph = np.linspace(30, 150, 50); p = np.radians(ph)
        t, _ = tube_on_face((40 + 6 * j) * np.cos(p), E[2] + (52 + 7 * j) * np.sin(p), 2.6 * np.sin(np.pi * (ph - 30) / 120) + .4, out=-.5, k=2.5)
        ops.append(t)
    # Lips: thick, rolled, round the maw.
    INFO['lips'] = []
    for upper in (True, False):
        y = np.linspace(-MY + 1, MY - 1, 110); u = y / MY
        rl = (3.8 + 2.2 * (1 - u * u)) if upper else (4.2 + 3. * (1 - u * u))
        z = z_up(y) + .8 * rl if upper else z_low(y) - .8 * rl
        t, pts = tube_on_face(y, z, rl, out=-1., k=3.)
        ops.append(t); INFO['lips'].append((pts, rl))
    # Folds under the lower lip, round the chin.
    for j in range(2):
        y = np.linspace(-42 + 6 * j, 42 - 6 * j, 50)
        t, _ = tube_on_face(y, z_low(y) - 14 - 8 * j, 2.6 * np.sin(np.pi * (y - y[0]) / (y[-1] - y[0])) + .3, out=-.3, k=2.5)
        ops.append(t)
    # Small warts, all over.
    W = []
    while len(W) < 140:
        d = rng.normal(size=3); d /= np.linalg.norm(d)
        W.append(d)
    WP = ray_surface(np.array(W)); wr = rng.uniform(.8, 2.4, len(W))
    ops.append(Balls('add', 1., WP - base_normal(WP) * (wr * .5)[:, None], wr))
    INFO['warts'] = (WP, wr)
    # Spikes along the jaw's lower sides, pointing down and back.
    for s in (1, -1):
        for j in range(5):
            b = on_face(s * (50 + 6 * j), -82 + 4.5 * j, -3.)[0]
            nb = base_normal(b)[0]
            d = nb * .55 + np.array([-.35, 0, -.75]); d /= np.linalg.norm(d)
            L = 13. - j * 1.2
            pts = np.array([b - d * 3, b + d * L * .5, b + d * L])
            ops.append(Tube('add', 1.5, pts, np.array([3.4, 1.8, .25])))
    # The eyestalks, fused into the cranium with fleshy collars, and their cupped bulbs.
    for st in STALKS:
        ops.append(Tube('add', 6., st['samples'], st['radius']))
        ops.append(Ellipsoids('add', 3., [(st['bulb'], np.array([15.5, 15.5, 14.]), frame(st['gaze']))]))
    # Carved out: the socket, the maw's opening and its cavity.
    ops.append(Func('sub', 4., E - SOCKET, E + SOCKET, lambda P: np.maximum(np.linalg.norm(P - E, axis=1) - SOCKET, (E[0] - 14) - P[:, 0])))
    ops.append(Func('sub', 3., [30, -MY - 6, -100], [140, MY + 6, -12], lambda P: np.maximum(mouth2d(P[:, 1], P[:, 2]), 35. - P[:, 0])))
    ops.append(Func('sub', 6., CAVITY[0] - CAVITY[1], CAVITY[0] + CAVITY[1], lambda P: sd_ellipsoid(P, *CAVITY)))
    # Gums, ridged red flesh just inside the lips, where the teeth stand.
    INFO['gums'] = []
    for upper in (True, False):
        y = np.linspace(-MY + 6, MY - 6, 90)
        z = z_up(y) + 3.5 if upper else z_low(y) - 3.5
        t, pts = tube_on_face(y, z, 5.4 if upper else 5.8, out=-6., k=2.5)
        ops.append(t); INFO['gums'].append(pts)
    # The stalks' eye holes.
    ops.append(Balls('sub', 1.4, [st['bulb'] + st['gaze'] * 12. for st in STALKS], [13.6] * len(STALKS)))
    return ops


CAVITY = (np.array([20., 0., -55.]), np.array([62., 68., 40.]))


def detail_disp(P):
    """Sculpt detail for the fine mesh only, outward positive (cm): wrinkle folds round the eye and the
    maw and across the middle of the forehead; everywhere, soft lumps and toad-like pebbles."""
    x, y, z = P[:, 0], P[:, 1], P[:, 2]
    w = 1.3 * fbm(P / 7., 3, 71)
    peb = 1 - np.abs(vnoise(P / 1.9, 73))
    w += .32 * smooth(.45, .95, peb) + .15 * smooth(.5, 1., 1 - np.abs(vnoise(P / 4.5, 79)))
    front = smooth(20, 45, x)
    warp = 2.5 * fbm(P / 11., 2, 75)
    rho = np.sqrt((y / 1.1) ** 2 + (z - E[2]) ** 2)
    band = smooth(40, 46, rho) * smooth(66, 56, rho) * front
    w += band * .8 * (np.abs(np.sin(np.pi * (rho + warp) / 5.5)) - .62)
    m2 = mouth2d(y, z)
    bandm = smooth(8, 13, m2) * smooth(30, 22, m2) * front * smooth(E[2] - 14, E[2] - 26, z)
    w += bandm * .7 * (np.abs(np.sin(np.pi * (m2 + warp) / 5.)) - .62)
    bandf = smooth(70, 76, z) * smooth(100, 92, z) * front * smooth(48, 34, np.abs(y))
    w += bandf * .6 * (np.abs(np.sin(np.pi * (z + 4. * fbm(P / 14., 2, 77)) / 6.5)) - .62)
    gum = smooth(-2, -6, m2) * smooth(-14, -9, m2) * smooth(30, 45, x)
    w += gum * .7 * (np.abs(np.sin(np.pi * y / 3.4)) - .5)
    return w


def lid_ops(upper):
    def shell(P):
        q = P - E; r = np.linalg.norm(q, axis=1)
        al = np.degrees(np.arctan2(q[:, 1], q[:, 0])); th = np.degrees(np.arctan2(q[:, 2], np.hypot(q[:, 0], q[:, 1])))
        edge = lid_edge(np.clip(al, -80, 80), upper)
        dth = (edge - th) if upper else (th - edge)
        region = r * np.sin(np.radians(np.clip(dth, -89, 89)))
        sh = np.abs(r - (R_EYE + 3.8)) - 2.4
        return np.maximum.reduce([sh, region, -22. - q[:, 0]])
    ops = [Func('add', 0., E - R_EYE - 8, E + R_EYE + 8, shell)]
    def arc(off_deg, rad):
        al = np.linspace(-82, 82, 100)
        th = lid_edge(al, upper) + (off_deg if upper else -off_deg)
        a_, t_ = np.radians(al), np.radians(th)
        return E + np.stack([np.cos(t_) * np.cos(a_), np.cos(t_) * np.sin(a_), np.sin(t_)], 1) * rad
    ops.append(Tube('add', 1.4, arc(3., R_EYE + 3.8), np.full(100, 3.)))
    for f in ((7., 12., 17.) if upper else (7., 12.)):
        ops.append(Tube('sub', .8, arc(f, R_EYE + 6.4), np.full(100, .9)))
    return ops


TONGUE_PATH = [(5, 2, -58), (38, -2, -61), (68, -8, -61), (92, -16, -58), (107, -25, -63), (114, -36, -73), (111, -47, -82)]


def tongue_samples():
    pts, ss = resample(catmull(TONGUE_PATH), 2.)
    d = np.gradient(pts, axis=0); d /= np.linalg.norm(d, axis=1, keepdims=True)
    side = np.array([0., 1., 0.])[None] - d * d[:, 1:2]; side /= np.linalg.norm(side, axis=1, keepdims=True)
    up = np.cross(d, side)
    return pts, ss, d, side, up


def tongue_ops():
    pts, ss, d, side, up = tongue_samples()
    u = ss / ss[-1]
    items = [(pts[i], np.array([16. - 5. * u[i] ** 1.5, 6.5, 5.6 - 2. * u[i]]), np.stack([side[i], d[i], up[i]], 1)) for i in range(len(pts))]
    ops = [Ellipsoids('add', 1.5, items)]
    g = pts + up * (5.9 - 2. * u)[:, None]
    m = u < .9
    ops.append(Tube('sub', 1.4, g[m], np.full(m.sum(), 1.2)))
    return ops


# ------------------------------------------------------------------ teeth
def teeth():
    """Rows of long conical fangs: an outer row above and below with great canines, a smaller inner row
    behind each; ivory, stained at the roots."""
    rows = [  # upper, n, half-span, inset, base radius, length(u), canine at, canine length
        (True, 15, 62., 7., (4.6, 6.0), lambda u: 16. + 9. * (1 - u * u), 34., 38.),
        (False, 14, 56., 7., (4.2, 5.6), lambda u: 13. + 8. * (1 - u * u), 30., 30.),
        (True, 11, 46., 15., (3.0, 3.8), lambda u: 10. + 4. * (1 - u * u), None, 0.),
        (False, 10, 42., 15., (2.9, 3.6), lambda u: 9. + 4. * (1 - u * u), None, 0.)]
    verts, faces, cols, owner = [], [], [], []
    for upper, n, span, inset, rb_rng, Lf, canine, cL in rows:
        ys = np.linspace(-span, span, n) + rng.uniform(-1.5, 1.5, n)
        for y in ys:
            u = abs(y) / MY
            z0 = (z_up(y) + 2.) if upper else (z_low(y) - 2.)
            root = on_face(y, z0, -inset)[0]
            d = np.array([.18, .08 * y / MY, -1.]) if upper else np.array([.15, .06 * y / MY, 1.])
            d /= np.linalg.norm(d)
            L = float(Lf(u)) + rng.uniform(0, 3.)
            rb = rng.uniform(*rb_rng)
            if canine is not None and abs(abs(y) - canine) < 5.5: L, rb = cL, rb_rng[1] + 1.4
            broken = rng.random() < .05
            side = np.cross(d, [1., 0., 0.]); side /= np.linalg.norm(side)
            fw = np.cross(side, d)
            base = len(verts); rings, sides_ = 10, 12
            for kk in range(rings):
                t = kk / (rings - 1) * (.86 if broken else 1.)
                c = root - d * 3. + d * (L + 3.) * t - fw * (.1 * L) * t * t
                rad = rb * (1 - t ** 1.15) + (.1 if kk == rings - 1 else 0.)
                for j in range(sides_):
                    b = 2 * math.pi * j / sides_
                    verts.append(c + (side * math.cos(b) + fw * math.sin(b) * .85) * rad)
                    tone = smooth(0., .3, t)
                    col = np.array([.30, .19, .09]) * (1 - tone) + np.array([.82, .74, .55]) * tone
                    cols.append(col * (.9 + .2 * rng.random()) + np.array([.08, .08, .07]) * smooth(.8, 1., t)); owner.append(upper)
            for kk in range(rings - 1):
                for j in range(sides_):
                    a0 = base + kk * sides_ + j; a1 = base + kk * sides_ + (j + 1) % sides_
                    faces.append((a0, a1, a1 + sides_, a0 + sides_))
            tip = base + (rings - 1) * sides_
            faces.append(tuple(range(tip + sides_ - 1, tip - 1, -1)))
    ob = mesh_from('teeth', verts, faces)
    attr = ob.data.color_attributes.new('Col', 'FLOAT_COLOR', 'POINT')
    attr.data.foreach_set('color', np.concatenate([np.array(cols), np.ones((len(cols), 1))], 1).astype(np.float32).ravel())
    log('teeth', f'count={sum(r[1] for r in rows)}')
    return ob, np.array(owner)


# ------------------------------------------------------------------ eyes
def sphere_eye(name, centre, axis, radius, rings, segs, psi_max=180.):
    """A sphere with its pole along `axis`; polar UVs for the iris texture (frame(): the UV's +v is up)."""
    F = frame(np.asarray(axis, float))
    verts, uvs, faces = [], [], []
    psis = [psi_max * (i / rings) ** 1.25 for i in range(rings + 1)]
    for i, ps in enumerate(psis):
        for j in range(segs if i else 1):
            b = 2 * math.pi * j / segs; p_ = math.radians(ps)
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
        for li in poly.loop_indices: uv.data[li].uv = uvs[ob.data.loops[li].vertex_index]
    return ob


def cornea():
    F = frame(np.array([1., 0., 0.]))
    verts, faces, uvs = [], [], []
    rings, segs, top = 16, 72, IRIS + 3.
    for i in range(rings + 1):
        ps = top * i / rings
        r = R_EYE + .15 + 1.1 * math.cos(min(ps / top, 1) * math.pi / 2) ** 2
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


def lin(*c):
    return (np.array(c, float) / 255.) ** 2.2


def eye_texture(size):
    """Polar map (psi 0..70 degrees from the eye's axis to the UV's rim): a green iris, gold round a
    vertical slit pupil, fibrous, a dark limbal ring; a yellowed, bloodshot sclera."""
    y, x = np.mgrid[0:size, 0:size]
    u = (x + .5) / size - .5; v = (y + .5) / size - .5
    rho = np.sqrt(u * u + v * v) / .5; beta = np.arctan2(v, u)
    psi = np.clip(rho, 0, 1) * 70.
    px, py = psi * np.cos(beta), psi * np.sin(beta)          # degrees across (horizontal) and up
    n2 = size * size
    fib = (vnoise(np.stack([beta.ravel() * 30, psi.ravel() * .3, np.zeros(n2)], 1), 7) * .5 + .5).reshape(size, size)
    fib2 = (vnoise(np.stack([beta.ravel() * 80, psi.ravel() * .8, np.ones(n2) * 3], 1), 8) * .5 + .5).reshape(size, size)
    blot = (fbm(np.stack([px.ravel() / 4, py.ravel() / 4, np.zeros(n2)], 1), 3, 9) * .5 + .5).reshape(size, size)
    t = np.clip(psi / IRIS, 0, 1)
    gold, yel, green, deep = lin(196, 128, 34), lin(170, 176, 48), lin(70, 122, 34), lin(26, 52, 16)
    ic = np.where((t < .3)[..., None], gold + (yel - gold) * (t / .3)[..., None],
                  np.where((t < .7)[..., None], yel + (green - yel) * ((t - .3) / .4)[..., None], green + (deep - green) * ((t - .7) / .3)[..., None]))
    ic = ic * (.6 + .65 * fib[..., None] * (.55 + .45 * fib2[..., None]))
    ic = ic * (1 - .35 * smooth(.55, .8, blot))[..., None]
    limb = smooth(IRIS - 3.5, IRIS - .3, psi)
    ic = ic * (1 - limb[..., None]) + lin(14, 22, 8) * limb[..., None]
    slit = (px / 3.4) ** 2 + (py / (IRIS * .88)) ** 2
    rim = smooth(2.2, 1., slit)
    ic = ic * (1 - .6 * rim[..., None])
    sclera = lin(230, 210, 176)[None, None] * (.92 + .1 * blot[..., None])
    sclera = sclera * (1 - smooth(52, 70, psi))[..., None] + lin(186, 96, 90) * smooth(52, 70, psi)[..., None]
    vmask = np.zeros((size, size))
    def plot(ps, be, w):
        pxi = ((.5 + .5 * (ps / 70.) * np.cos(be)) * size).astype(int); pyi = ((.5 + .5 * (ps / 70.) * np.sin(be)) * size).astype(int)
        rad = max(1, int(w * size / 2048))
        for dx in range(-rad, rad + 1):
            for dy in range(-rad, rad + 1):
                if dx * dx + dy * dy > rad * rad: continue
                ok = (pxi + dx >= 0) & (pxi + dx < size) & (pyi + dy >= 0) & (pyi + dy < size)
                np.maximum.at(vmask, (pyi[ok] + dy, pxi[ok] + dx), 1.)
    def vessel(ps, be, w, depth):
        a_p, a_b = [], []
        stop = rng.uniform(IRIS + 2, 52); db = rng.normal(0, .004)
        while ps > stop and w > .6:
            a_p.append(ps); a_b.append(be)
            ps -= .12; db += rng.normal(0, .0016); db *= .97; be += db; w *= .9975
            if depth < 3 and rng.random() < .012: vessel(ps, be, w * .7, depth + 1)
        if a_p: plot(np.array(a_p), np.array(a_b), w)
    for _ in range(54): vessel(70., rng.uniform(0, 2 * np.pi), rng.uniform(2.2, 4.), 0)
    vm = vmask.copy()
    for ax in (0, 1): vm = sum(np.roll(vm, s, ax) for s in range(-3, 4)) / 7
    vm = np.clip(vm * 1.6, 0, 1)
    sclera = sclera * (1 - .85 * vm[..., None]) + lin(156, 22, 26) * (.85 * vm[..., None])
    col = np.where((psi < IRIS)[..., None], ic, sclera)
    edge = smooth(IRIS + 1.2, IRIS - .5, psi)
    col = col * (1 - edge[..., None]) + ic * edge[..., None]
    col = np.where((slit < 1.)[..., None], lin(4, 3, 3) * np.ones(3), col)
    return np.clip(col, 0, 1)


def save_png(name, rgb, colour=True):
    h, w, _ = rgb.shape
    img = bpy.data.images.new(name, w, h, alpha=False, float_buffer=False)
    if not colour: img.colorspace_settings.name = 'Non-Color'
    data = rgb ** (1 / 2.2) if colour else rgb
    img.pixels.foreach_set(np.clip(np.concatenate([data, np.ones((h, w, 1))], 2), 0, 1).astype(np.float32).ravel())
    img.filepath_raw = str(OUT / f'{name}.png'); img.file_format = 'PNG'; img.save()
    return img


# ------------------------------------------------------------------ surface look (fine mesh)
def laplacian(ops, V, d):
    off = np.array([[d, 0, 0], [-d, 0, 0], [0, d, 0], [0, -d, 0], [0, 0, d], [0, 0, -d]])
    Q = (V[:, None, :] + off[None]).reshape(-1, 3)
    s = evaluate_binned(ops, Q, margin=d + 1).reshape(-1, 6).sum(1)
    c = evaluate_binned(ops, V, margin=d + 1)
    return (s - 6 * c) / (d * d)


def sdf_ao(V, N, ops):
    occ = np.zeros(len(V))
    for delta, wt in ((.9, .5), (2.8, .35), (8., .25)):
        acc = np.zeros(len(V)); tot = 0.
        for i in range(1, 5):
            dd = evaluate_binned(ops, V + N * (i * delta), margin=i * delta)
            w = .5 ** i; acc += w * np.clip((i * delta - dd) / (i * delta), 0, 1); tot += w
        occ += wt * acc / tot
    return np.clip(1 - 1.3 * occ, .05, 1)


def surface_look(V, N, ops, ao_ops, part):
    n = len(V)
    convex = np.clip(laplacian(ops, V, 4.) * 3.5, -1, 1)          # the folds' scale: at the pebbles' it read as camouflage
    mott = fbm(V / 12., 4, 21) * .5 + .5
    if part == 'lid':
        col = np.tile(lin(150, 66, 86), (n, 1)); rough = np.full(n, .28); wr = np.full(n, .55)
    elif part == 'tongue':
        col = np.tile(lin(170, 64, 84), (n, 1)); rough = np.full(n, .14); wr = np.full(n, .35)
        col = col * (1 - .3 * smooth(.0, -.6, convex))[:, None]
    else:
        col = np.tile(lin(122, 44, 72), (n, 1)); rough = np.full(n, .34); wr = np.full(n, .9)
        kd, meta = STALK_KD
        dmin = np.empty(n); sk = np.empty(n, int); sv = np.empty(n)
        for i in range(n):
            co, idx, dist = kd.find(V[i]); dmin[i] = dist; sk[i], sv[i] = meta[idx]
        onst = smooth(22, 15, dmin) * smooth(4, 16, sv)
        col = col * (1 - onst[:, None]) + lin(136, 46, 96) * onst[:, None]
        for st in STALKS:
            m = sk == st['k']
            rr = np.linalg.norm(V[m] - st['eye'], axis=1)
            wet = smooth(17.5, 14., rr)
            col[m] = col[m] * (1 - wet[:, None]) + lin(176, 84, 96) * wet[:, None]
            rough[m] = rough[m] * (1 - wet) + .2 * wet
        x, y, z = V[:, 0], V[:, 1], V[:, 2]
        # pale on the brow, forehead and cheeks (the reference's tan face)
        face = smooth(30, 70, x) * smooth(-30, 10, z) * smooth(110, 80, z) * (1 - onst)
        col = col * (1 - .35 * face[:, None]) + lin(176, 104, 98) * (.35 * face[:, None])
        de = np.linalg.norm(V - E, axis=1)
        rim = smooth(SOCKET + 5, SOCKET + .5, de) * (x > E[0] - 16)
        col = col * (1 - rim[:, None]) + lin(176, 88, 100) * rim[:, None]; rough = rough * (1 - rim) + .2 * rim
        # the maw: wet deep red inside, red ridged gums, darker wet lips
        cav = np.minimum(sd_ellipsoid(V, *CAVITY), np.maximum(mouth2d(y, z), 35. - x))
        inside = smooth(3., .5, cav)
        gd = evaluate_binned([Tube('add', 0., g, np.zeros(len(g))) for g in INFO['gums']], V)
        gum = smooth(9., 4., gd)
        mcol = lin(150, 30, 42) * (1 - gum[:, None]) + lin(190, 58, 70) * gum[:, None]
        col = col * (1 - inside[:, None]) + mcol * inside[:, None]
        rough = rough * (1 - inside) + .12 * inside; wr = wr * (1 - inside) + .3 * inside
        ld = evaluate_binned([Tube('add', 0., p, r) for p, r in INFO['lips']], V)
        lip = smooth(2.5, .2, ld) * (1 - inside)
        col = col * (1 - .6 * lip[:, None]) + lin(150, 48, 72) * (.6 * lip[:, None]); rough = rough * (1 - lip) + .22 * lip
        # veins, dark, near the eye and up the stalks
        vn = 1 - np.abs(fbm(V / 7., 3, 61))
        vein = smooth(.9, .97, vn) * np.maximum(smooth(SOCKET + 34, SOCKET + 6, de), .2 * onst + .1)
        col = col * (1 - .5 * vein[:, None]) + lin(70, 16, 50) * (.5 * vein[:, None])
        WP, wrr = INFO['warts']
        wd = evaluate_binned([Balls('add', 0., WP, wrr)], V)
        wm = smooth(.6, -.2, wd)
        col = col * (1 - wm[:, None]) + lin(184, 120, 112) * wm[:, None]
    col = col * (.88 + .22 * mott)[:, None]
    hi = smooth(.04, .55, convex)[:, None]; lo = smooth(-.04, -.55, convex)[:, None]
    col = col * (1 - .7 * hi) + lin(206, 140, 124) * (.7 * hi)
    col = col * (1 - .6 * lo) + lin(52, 14, 32) * (.6 * lo)
    rough = rough + .1 * lo[:, 0] - .04 * hi[:, 0]
    ao = sdf_ao(V, N, ao_ops)
    if part == 'body': ao = ao * (1 - .5 * inside) + .5 * inside * np.maximum(ao, .45)
    return np.clip(col, 0, 1), np.stack([ao, np.clip(rough, .05, 1), np.clip(wr, 0, 1)], 1)


# ------------------------------------------------------------------ bake
def bake_material(ob, grain):
    """The fine mesh's look: vertex colour; the leather grain (box-projected) as bump, darkening its pits."""
    m = bpy.data.materials.new('BakeSrc_' + ob.name); m.use_nodes = True
    nt = m.node_tree; N = nt.nodes; L = nt.links
    for x in list(N): N.remove(x)
    out = N.new('ShaderNodeOutputMaterial'); bsdf = N.new('ShaderNodeBsdfPrincipled')
    col = N.new('ShaderNodeAttribute'); col.attribute_name = 'col'
    aux = N.new('ShaderNodeAttribute'); aux.attribute_name = 'aux'
    sep = N.new('ShaderNodeSeparateColor'); L.new(aux.outputs['Color'], sep.inputs['Color'])
    tc = N.new('ShaderNodeTexCoord')
    mp = N.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / 70., 1 / 70., 1 / 70.)
    L.new(tc.outputs['Object'], mp.inputs['Vector'])
    g = N.new('ShaderNodeTexImage'); g.image = grain; g.projection = 'BOX'; g.projection_blend = .3
    L.new(mp.outputs['Vector'], g.inputs['Vector'])
    pits = N.new('ShaderNodeMapRange'); pits.inputs['To Min'].default_value = .88; pits.inputs['To Max'].default_value = 1.05
    L.new(g.outputs['Color'], pits.inputs['Value'])
    mul = N.new('ShaderNodeMix'); mul.data_type = 'RGBA'; mul.blend_type = 'MULTIPLY'; mul.inputs['Factor'].default_value = 1.
    L.new(col.outputs['Color'], mul.inputs['A']); L.new(pits.outputs['Result'], mul.inputs['B'])
    L.new(mul.outputs['Result'], bsdf.inputs['Base Color'])
    soft = N.new('ShaderNodeTexNoise'); soft.inputs['Scale'].default_value = .25; soft.inputs['Detail'].default_value = 3.
    L.new(tc.outputs['Object'], soft.inputs['Vector'])
    sw = N.new('ShaderNodeMath'); sw.operation = 'MULTIPLY'; sw.inputs[1].default_value = .6; L.new(soft.outputs['Fac'], sw.inputs[0])
    h = N.new('ShaderNodeMath'); h.operation = 'ADD'; L.new(g.outputs['Color'], h.inputs[0]); L.new(sw.outputs['Value'], h.inputs[1])
    bump = N.new('ShaderNodeBump'); bump.inputs['Distance'].default_value = .3
    L.new(sep.outputs['Blue'], bump.inputs['Strength']); L.new(h.outputs['Value'], bump.inputs['Height'])
    L.new(bump.outputs['Normal'], bsdf.inputs['Normal'])
    L.new(sep.outputs['Green'], bsdf.inputs['Roughness'])
    L.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    emit = N.new('ShaderNodeEmission'); L.new(aux.outputs['Color'], emit.inputs['Color'])
    ob.data.materials.clear(); ob.data.materials.append(m)
    return m, out, bsdf, emit


def bake_all(low, highs):
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 1
    sc.render.bake.use_selected_to_active = True
    sc.render.bake.cage_extrusion = 2.; sc.render.bake.max_ray_distance = 5.
    sc.render.bake.margin = 8; sc.render.bake.use_clear = True
    grain = bpy.data.images.load(str(GRAIN)); grain.colorspace_settings.name = 'Non-Color'
    mats = [bake_material(h, grain) for h in highs]
    skin = low.data.materials[0]; skin.use_nodes = True
    imgs = {}
    for name, colour in (('T_Beholder_D', True), ('T_Beholder_N', False), ('T_Beholder_ORM', False)):
        img = bpy.data.images.new(name, TEX, TEX, alpha=False, float_buffer=False)
        if not colour: img.colorspace_settings.name = 'Non-Color'
        imgs[name] = img
    node = skin.node_tree.nodes.new('ShaderNodeTexImage')
    bpy.ops.object.select_all(action='DESELECT')
    for hh in highs: hh.select_set(True)
    low.select_set(True); bpy.context.view_layer.objects.active = low
    for name, kind, kw in (('T_Beholder_N', 'NORMAL', dict(normal_space='TANGENT')),
                           ('T_Beholder_D', 'DIFFUSE', dict(pass_filter={'COLOR'})),
                           ('T_Beholder_ORM', 'EMIT', {})):
        node.image = imgs[name]; skin.node_tree.nodes.active = node
        for m, out, bsdf, emit in mats:
            m.node_tree.links.new((emit if kind == 'EMIT' else bsdf).outputs[0], out.inputs['Surface'])
        t = time.time()
        bpy.ops.object.bake(type=kind, use_selected_to_active=True, margin=8, **kw)
        log('bake', name, f'{time.time() - t:.1f}s')
        imgs[name].filepath_raw = str(OUT / f'{name}.png'); imgs[name].file_format = 'PNG'; imgs[name].save()
    skin.node_tree.nodes.remove(node)
    return imgs


# ------------------------------------------------------------------ rig and weights
TONGUE_BONES = (0., .3, .55, .78, 1.)


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
    bone('jaw', JAW, JAW + np.array([40., 0, -30.]), 'body')
    pts, ss, *_ = tongue_samples()
    def at(f): return np.array([np.interp(f * ss[-1], ss, pts[:, i]) for i in range(3)])
    for i in range(4):
        bone(f'tongue_{i}', at(TONGUE_BONES[i]), at(TONGUE_BONES[i + 1]), 'jaw' if i == 0 else f'tongue_{i - 1}')
    bone('lid_upper', E, E + np.array([0, 0, 20.]), 'body')
    bone('lid_lower', E, E + np.array([0, 0, -20.]), 'body')
    bone('eye', E, E + np.array([25., 0, 0]), 'body')
    for st in STALKS:
        k = st['k']; s = st['s']; p = st['samples']
        bs = np.linspace(10., st['L'] - 2., 7); st['bone_s'] = bs
        def sat(x): return np.array([np.interp(x, s, p[:, i]) for i in range(3)])
        for i in range(6):
            bone(f'stalk{k}_{i}', sat(bs[i]), sat(bs[i + 1]), 'body' if i == 0 else f'stalk{k}_{i - 1}')
        bone(f'stalk{k}_eye', st['eye'], st['eye'] + st['gaze'] * 8., f'stalk{k}_5')
    bpy.ops.object.mode_set(mode='OBJECT')
    return rig


def add_weights(ob, weights):
    for name, w in weights.items():
        idx = np.nonzero(w > 1e-3)[0]
        if not len(idx): continue
        vg = ob.vertex_groups.get(name) or ob.vertex_groups.new(name=name)
        for i in idx: vg.add([int(i)], float(w[i]), 'REPLACE')


def jaw_weight(V):
    x, y, z = V[:, 0], V[:, 1], V[:, 2]
    zs = (z_up(y) + z_low(y)) / 2
    w = smooth(zs + 4, zs - 4, z) * smooth(MY + 26, MY + 6, np.abs(y)) * smooth(-45, -15, x)
    w[np.linalg.norm(V - E, axis=1) < SOCKET + 4] = 0
    return w


def body_weights(V):
    n = len(V)
    jaw = jaw_weight(V)
    kd, meta = STALK_KD
    stalk_w = {}; rest = np.ones(n)
    for i in range(n):
        co, idx, dist = kd.find(V[i])
        k, sv = meta[idx]; st = STALKS[k]
        rad = float(np.interp(sv, st['s'], st['radius']))
        in_bulb = np.linalg.norm(V[i] - st['bulb']) < 20.
        if dist > rad * 1.8 + 4 and not in_bulb: continue
        if in_bulb: sv = st['L']
        w = float(smooth(5., 17., sv))
        if w <= 0: continue
        bs = st['bone_s']; cen = (bs[:-1] + bs[1:]) / 2
        if sv <= cen[0]: parts = [(0, 1.)]
        elif sv >= cen[-1]: parts = [(5, 1.)]
        else:
            j = int(np.searchsorted(cen, sv)) - 1; f = (sv - cen[j]) / (cen[j + 1] - cen[j])
            parts = [(j, 1 - f), (j + 1, f)]
        for j, f in parts: stalk_w.setdefault(f'stalk{k}_{j}', np.zeros(n))[i] += w * f
        rest[i] -= w
    W = {'jaw': rest * jaw, 'body': rest * (1 - jaw)}
    W.update(stalk_w)
    return W


def tongue_weights(V):
    pts, ss, *_ = tongue_samples()
    k = kdtree.KDTree(len(pts))
    for i, p in enumerate(pts): k.insert(Vector(p), i)
    k.balance()
    f = np.array([ss[k.find(v)[1]] for v in V]) / ss[-1]
    cen = np.array([(TONGUE_BONES[i] + TONGUE_BONES[i + 1]) / 2 for i in range(4)])
    W = {f'tongue_{i}': np.zeros(len(V)) for i in range(4)}
    for vi, fv in enumerate(f):
        if fv <= cen[0]: W['tongue_0'][vi] = 1; continue
        if fv >= cen[-1]: W['tongue_3'][vi] = 1; continue
        j = int(np.searchsorted(cen, fv)) - 1; t = (fv - cen[j]) / (cen[j + 1] - cen[j])
        W[f'tongue_{j}'][vi] = 1 - t; W[f'tongue_{j + 1}'][vi] = t
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
    kd = kdtree.KDTree(sum(len(st['samples']) for st in STALKS)); meta = []
    for st in STALKS:
        for p, s in zip(st['samples'], st['s']): kd.insert(Vector(p), len(meta)); meta.append((st['k'], s))
    kd.balance(); STALK_KD = (kd, meta)
    B = body_ops()
    Bhigh = B + [Disp([-200, -200, -200], [200, 200, 300], detail_disp)]
    LU, LL, TG = lid_ops(True), lid_ops(False), tongue_ops()
    eyeball = [Func('add', 0., E - R_EYE, E + R_EYE, lambda P: np.linalg.norm(P - E, axis=1) - R_EYE)]
    parts = {'body': (B, Bhigh), 'lid_upper': (LU, LU), 'lid_lower': (LL, LL), 'tongue': (TG, TG)}
    everything = Bhigh + LU + LL + TG + eyeball
    skin_mat = bpy.data.materials.new('Skin')
    lows, highs = [], []
    for name, (low_ops, high_ops) in parts.items():
        V, Q, N = field_mesh(name + '_low', low_ops, LOW_H if name == 'body' else LOW_PART)
        lo = make_mesh(name + '_low', V, Q); lo.data.materials.append(skin_mat)
        lows.append((name, lo, V))
        V, Q, N = field_mesh(name + '_high', high_ops, HIGH_H)
        hi = make_mesh(name + '_high', V, Q)
        kind = 'body' if name == 'body' else ('tongue' if name == 'tongue' else 'lid')
        col, aux = surface_look(V, N, high_ops, everything, kind)
        for nm, arr in (('col', col), ('aux', aux)):
            a = hi.data.color_attributes.new(nm, 'FLOAT_COLOR', 'POINT')
            a.data.foreach_set('color', np.concatenate([arr, np.ones((len(arr), 1))], 1).astype(np.float32).ravel())
        highs.append(hi)
        log('look', name, f'ao_mean={aux[:, 0].mean():.2f}')
    rig = build_rig()
    for name, ob, V in lows:
        if name == 'body': add_weights(ob, body_weights(V))
        elif name == 'tongue': add_weights(ob, tongue_weights(V))
        else: add_weights(ob, {name: np.ones(len(V))})
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
    for hh in highs: bpy.data.objects.remove(hh)
    eye_mat, cornea_mat, teeth_mat = bpy.data.materials.new('Eye'), bpy.data.materials.new('Cornea'), bpy.data.materials.new('Teeth')
    extra = []
    main = sphere_eye('eye_main', E, [1., 0., 0.], R_EYE, 40, 72, psi_max=160.); main.data.materials.append(eye_mat)
    add_weights(main, {'eye': np.ones(len(main.data.vertices))}); extra.append(main)
    co = cornea(); co.data.materials.append(cornea_mat); add_weights(co, {'eye': np.ones(len(co.data.vertices))}); extra.append(co)
    for st in STALKS:
        s = sphere_eye(f'eye_{st["k"]}', st['eye'], st['gaze'], STALK_EYE, 18, 36, psi_max=150.); s.data.materials.append(eye_mat)
        add_weights(s, {f'stalk{st["k"]}_eye': np.ones(len(s.data.vertices))}); extra.append(s)
    th, owner = teeth(); th.data.materials.append(teeth_mat); th.data.uv_layers.new(name='UVMap')
    add_weights(th, {'body': owner.astype(float), 'jaw': (~owner).astype(float)}); extra.append(th)
    bpy.ops.object.select_all(action='DESELECT')
    for o in extra: o.select_set(True)
    skin.select_set(True); bpy.context.view_layer.objects.active = skin
    bpy.ops.object.join()
    ob = skin; ob.parent = rig
    m = ob.modifiers.new('Armature', 'ARMATURE'); m.object = rig
    wsum = np.zeros(len(ob.data.vertices))
    for v in ob.data.vertices:
        for g in v.groups: wsum[v.index] += g.weight
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    log('mesh', f'tris={tris}', f'verts={len(ob.data.vertices)}', f'bones={len(rig.data.bones)}', f'unweighted={(wsum < .99).sum()}',
        f'materials={[x.name for x in ob.data.materials]}')
    return rig, ob


FBX = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False,
           primary_bone_axis='Y', secondary_bone_axis='X', use_armature_deform_only=False, mesh_smooth_type='FACE')


def review_materials(ob, clay=False):
    imgs = {n: bpy.data.images.get(n) for n in ('T_Beholder_D', 'T_Beholder_N', 'T_Beholder_ORM')}
    eye_img = bpy.data.images.get('T_BeholderEye.png') or bpy.data.images.load(str(OUT / 'T_BeholderEye.png'))
    for m in ob.data.materials:
        m.use_nodes = True; nt = m.node_tree; N = nt.nodes; L = nt.links
        for x in list(N): N.remove(x)
        out = N.new('ShaderNodeOutputMaterial'); b = N.new('ShaderNodeBsdfPrincipled'); L.new(b.outputs[0], out.inputs[0])
        if clay and m.name != 'Cornea':
            b.inputs['Base Color'].default_value = (.42, .42, .42, 1); b.inputs['Roughness'].default_value = .55
            if m.name == 'Skin':
                nn = N.new('ShaderNodeTexImage'); nn.image = imgs['T_Beholder_N']
                nm = N.new('ShaderNodeNormalMap'); L.new(nn.outputs['Color'], nm.inputs['Color']); L.new(nm.outputs['Normal'], b.inputs['Normal'])
            continue
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
            b.inputs['Subsurface Weight'].default_value = .2; b.inputs['Subsurface Radius'].default_value = (1., .25, .3)
        elif m.name == 'Eye':
            t = N.new('ShaderNodeTexImage'); t.image = eye_img; L.new(t.outputs['Color'], b.inputs['Base Color'])
            b.inputs['Roughness'].default_value = .1
        elif m.name == 'Cornea':
            b.inputs['Base Color'].default_value = (1, 1, 1, 1); b.inputs['Roughness'].default_value = .02
            b.inputs['Transmission Weight'].default_value = 1.; b.inputs['Alpha'].default_value = .12
        else:
            a = N.new('ShaderNodeAttribute'); a.attribute_name = 'Col'
            tint = N.new('ShaderNodeMix'); tint.data_type = 'RGBA'; tint.blend_type = 'MULTIPLY'; tint.inputs['Factor'].default_value = 1.
            L.new(a.outputs['Color'], tint.inputs['A']); tint.inputs['B'].default_value = (.62, .55, .42, 1)
            L.new(tint.outputs['Result'], b.inputs['Base Color']); b.inputs['Roughness'].default_value = .4


def review(rig, ob):
    import os
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'; sc.cycles.samples = 48; sc.cycles.use_denoising = True
    world = bpy.data.worlds.new('W'); sc.world = world; world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (.03, .035, .045, 1)
    for nm, rot, en, col in (('Key', (50, 0, 20), 4., (1., .88, .76)), ('Rim', (65, 0, 205), 3.5, (.6, .75, 1.)), ('Fill', (80, 0, -70), 1.2, (1, 1, 1))):
        l = bpy.data.objects.new(nm, bpy.data.lights.new(nm, 'SUN')); sc.collection.objects.link(l)
        l.rotation_euler = tuple(math.radians(x) for x in rot); l.data.energy = en; l.data.color = col
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
    sc.render.resolution_x, sc.render.resolution_y = 1100, 1100
    def shot(name, loc, look, lens=35):
        cam.location = loc; d = Vector(look) - Vector(loc); cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
        cam.data.lens = lens; cam.data.clip_end = 5000
        sc.render.filepath = os.path.join(REVIEW, f'Beholder_{name}.png'); bpy.ops.render.render(write_still=True)
    review_materials(ob, clay=True)
    shot('clay', (470, -190, 90), (0, 0, 0))
    shot('clay_front', (560, 0, 30), (0, 0, 10))
    review_materials(ob)
    shot('ref', (470, -190, 90), (0, 0, 0))
    shot('front', (560, 0, 30), (0, 0, 10))
    shot('side', (0, -600, 40), (0, 0, 10))
    shot('back', (-430, 280, 220), (0, 0, 30))
    shot('below', (420, -120, -330), (0, 0, -10))
    shot('eye', (210, -40, 40), (70, 0, 28), 50)
    shot('mouth', (230, -70, -50), (70, 0, -60), 40)
    bpy.context.view_layer.objects.active = rig; bpy.ops.object.mode_set(mode='POSE')
    pb = rig.pose.bones
    from mathutils import Quaternion
    def turn(name, axis, deg):
        b = pb[name]; b.rotation_mode = 'QUATERNION'
        M = rig.data.bones[name].matrix_local.to_3x3()
        b.rotation_quaternion = Quaternion(M.inverted() @ Vector(axis), math.radians(deg)) @ b.rotation_quaternion
    turn('jaw', (0, 1, 0), 12); turn('lid_upper', (0, 1, 0), 30)
    for i in range(4): turn(f'tongue_{i}', (0, 0, 1), 10)
    for st in STALKS:
        ax = np.cross(st['gaze'], [0, 0, 1]); ax /= max(1e-6, np.linalg.norm(ax))
        for i in range(1, 6): turn(f'stalk{st["k"]}_{i}', tuple(ax), 9 * math.sin(st['k'] + i))
    bpy.ops.object.mode_set(mode='OBJECT')
    shot('posed', (440, -280, 80), (0, 0, 0))
    bpy.context.view_layer.objects.active = rig; bpy.ops.object.mode_set(mode='POSE')
    for b in pb: b.rotation_quaternion = (1, 0, 0, 0)
    bpy.ops.object.mode_set(mode='OBJECT')


if __name__ == '__main__':
    if not GRAIN.exists(): raise SystemExit(f'Missing {GRAIN}: run Tools/Fetch-BeholderSkin.ps1')
    eye = eye_texture(512 if QUICK else 2048)
    rig, ob = build()
    save_png('T_BeholderEye', eye)
    if REVIEW: review(rig, ob)
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True); ob.select_set(True); bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SK_Beholder.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'}, bake_anim=False, **FBX)
    info = {'generator': 'Tools/build_beholder.py', 'blender': bpy.app.version_string, 'quick': QUICK,
            'eye_centre': E.tolist(), 'eye_radius': R_EYE, 'lid_close_deg': float(lid_edge(0, True) - lid_edge(0, False)),
            'jaw_hinge': JAW.tolist(), 'stalk_eye_radius': STALK_EYE,
            'stalks': [{'k': st['k'], 'base': st['base'].tolist(), 'eye': st['eye'].tolist(), 'gaze': st['gaze'].tolist(), 'length_cm': round(float(st['L']), 1)} for st in STALKS],
            'bones': [b.name for b in rig.data.bones], 'tris': sum(len(p.vertices) - 2 for p in ob.data.polygons), 'texture': TEX,
            'grain': 'SourceAssets/Surfaces/Beholder/leather037_disp_2k.jpg (ambientCG Leather037, CC0)'}
    (OUT / 'beholder.json').write_text(json.dumps(info, indent=1) + '\n', encoding='utf-8')
    log('done', OUT / 'SK_Beholder.fbx')
