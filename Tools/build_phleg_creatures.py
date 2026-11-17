"""Blender 4.5.14: Phlegethos's lemures, Pit Fiend and spined devils (Claude, 2026-11-16; docs/regions/PHLEGETHOS.md#creatures).

User: "do the npc's for the region (one change from the 2d game, in 2d we had both horned devils and pit fiend but i
dont want the separate horned devils in our game)". The 2D game's Phase 8 bestiary: lemures (the undead lifecycle,
slowest and most durable), spined devils (perched, flicking burning spines across a lane), the Pit Fiend (a colossus).

Everything is modelled here as signed distance fields (Tools/sdf_kit.py, as the beholder) - no downloaded models:

* SK_Lemure: a shapeless, melting mass of flesh in a man's shape, hunched, its face run like wax, folds glowing faintly
  from the heat inside, drips hanging from it. On the temple skeleton's own rig (Tools/build_temple_skeleton.py builds
  it again here, unchanged), so the skeleton's motion-capture clips drive it in Unreal.
* SK_PitFiend: a huge winged devil on the same rig (scaled up in the game), after the user's goal image
  (References/Phlegethos/pit-fiend-goal.jpg): crimson muscle, long horns sweeping out and up, charcoal armour (spiked
  pauldrons and bracers, a belt hung with skulls, thigh plates, knee guards, greaves), great bat wings (rigid on the
  upper spine), a long tail curled on the ground, a spiked mace in its right fist (its head's position goes to
  creatures.json as bone weights, for the game's fire on it).
* SM_SpinedDevil_{Body,Head,WingL,WingR,Tail}: a small gargoyle-like devil crouched on its haunches, bristling with
  spines; each part pivots at its joint so the game can turn the head, beat the wings and flick the tail.
* SM_DevilSpine: one thrown burning spine. SM_DevilPerch: a basalt column for it to crouch on.

Weights: every primitive belongs to a bone; a vertex is weighted to the bones of the primitives nearest it, softly, so
the flesh bends at the joints and the torso does not follow an arm. Colour: vertex colour RGB is the albedo (with its
ambient occlusion), A the glow (molten folds, eyes, mouth, spine tips); a tiling detail normal is added in Unreal.

blender --background --factory-startup --python Tools/build_phleg_creatures.py -- [--only lemure,fiend,devil] [--review DIR]
  -> SourceAssets/Enemies/Phlegethos/*.fbx and creatures.json (each mesh's UV scale, cm per UV unit)
"""
from pathlib import Path
import importlib.util
import json
import math
import sys

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Tools'))
import sdf_kit as K  # noqa: E402
from sdf_kit import Tube, Balls, Ellipsoids, Func, Disp, fbm, smooth, frame  # noqa: E402

OUT = ROOT / 'SourceAssets/Enemies/Phlegethos'
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
def arg(name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default
ONLY = (arg('--only') or 'lemure,fiend,devil').split(',')
REVIEW = arg('--review')
MANIFEST = {}


def lin(r, g, b):
    return (np.array([r, g, b], float) / 255.) ** 2.2


def V3(*a):
    return np.array(a, float)


def catmull(pts, n):
    """A smooth curve through pts, n samples."""
    P = np.asarray(pts, float); P = np.vstack([P[0] * 2 - P[1], P, P[-1] * 2 - P[-2]])
    out = []
    for s in np.linspace(0, len(P) - 3 - 1e-6, n):
        i = int(s) + 1; t = s - (i - 1)
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        out.append(.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    return np.array(out)


def ell(c, r, axis=None):
    """An ellipsoid op item: radii (x, y, z) in a frame whose z runs along axis (world axes if None)."""
    c = np.asarray(c, float); r = np.asarray(r, float)
    return (c, r, None if axis is None else frame(np.asarray(axis, float)))


class Body:
    """Primitives, each owned by a bone (for the weights) and tagged (for the colour)."""
    def __init__(self):
        self.parts = []   # (bone, tag, op)
        self.extra = []   # sub and disp ops

    def add(self, bone, tag, op):
        self.parts.append((bone, tag, op)); return op

    def tube(self, bone, pts, rad, tag='skin', k=2.):
        return self.add(bone, tag, Tube('add', k, pts, rad))

    def ells(self, bone, items, tag='skin', k=2.):
        return self.add(bone, tag, Ellipsoids('add', k, items))

    def balls(self, bone, c, r, tag='skin', k=1.):
        return self.add(bone, tag, Balls('add', k, c, r))

    def sub(self, op):
        op.kind = 'sub'; self.extra.append(op)

    def disp(self, f, lo=(-300, -300, -300), hi=(300, 300, 300)):
        self.extra.append(Disp(lo, hi, f))

    def ops(self):
        adds = [op for _, _, op in self.parts]
        subs = [op for op in self.extra if op.kind == 'sub']
        disps = [op for op in self.extra if op.kind == 'disp']
        return adds + subs + disps

    def part_dists(self, V):
        lo, hi = V.min(0) - 1e4, V.max(0) + 1e4
        return np.stack([op.eval(V, lo, hi) for _, _, op in self.parts])

    def weights(self, V, s=2.2):
        D = self.part_dists(V)
        dmin = D.min(0)
        E = np.exp(-np.clip(D - dmin, 0, 30) / s) * (D - dmin < 14)
        W = {}
        for (bone, _, _), e in zip(self.parts, E): W[bone] = W.get(bone, 0) + e
        names = list(W); M = np.stack([W[n] for n in names])
        # four bones at most, renormalised
        order = np.argsort(-M, 0); keep = np.zeros_like(M, bool)
        for k in range(min(4, len(names))): keep[order[k], np.arange(M.shape[1])] = True
        M = np.where(keep, M, 0); M /= np.maximum(M.sum(0), 1e-9)
        return {n: M[i] for i, n in enumerate(names)}

    def tag_mask(self, V, D=None):
        D = self.part_dists(V) if D is None else D
        dmin = D.min(0); out = {}
        for tag in {t for _, t, _ in self.parts}:
            idx = [i for i, (_, t, _) in enumerate(self.parts) if t == tag]
            dt = D[idx].min(0)
            out[tag] = smooth(1.6, .2, dt - dmin)
        return out


def laplacian(ops, V, d):
    off = np.array([[d, 0, 0], [-d, 0, 0], [0, d, 0], [0, -d, 0], [0, 0, d], [0, 0, -d]])
    Q = (V[:, None, :] + off[None]).reshape(-1, 3)
    s = K.evaluate_binned(ops, Q, margin=d + 1).reshape(-1, 6).sum(1)
    c = K.evaluate_binned(ops, V, margin=d + 1)
    return (s - 6 * c) / (d * d)


def sdf_ao(V, N, ops, scale=1.):
    occ = np.zeros(len(V))
    for delta, wt in ((.9 * scale, .5), (2.8 * scale, .35), (8. * scale, .25)):
        acc = np.zeros(len(V)); tot = 0.
        for i in range(1, 5):
            dd = K.evaluate_binned(ops, V + N * (i * delta), margin=i * delta)
            w = .5 ** i; acc += w * np.clip((i * delta - dd) / (i * delta), 0, 1); tot += w
        occ += wt * acc / tot
    return np.clip(1 - 1.3 * occ, .05, 1)


# ------------------------------------------------------------------ meshes
def set_colours(ob, col, alpha):
    me = ob.data
    a = me.color_attributes.get('Col') or me.color_attributes.new('Col', 'FLOAT_COLOR', 'POINT')
    a.data.foreach_set('color', np.concatenate([np.clip(col, 0, 1), np.clip(alpha, 0, 1)[:, None]], 1).astype(np.float32).ravel())
    me.color_attributes.active_color = a
    me.color_attributes.render_color_index = me.color_attributes.find('Col')


def verts_of(ob):
    V = np.empty(len(ob.data.vertices) * 3); ob.data.vertices.foreach_get('co', V); return V.reshape(-1, 3)


def select_only(*obs):
    for o in bpy.context.selected_objects: o.select_set(False)
    for o in obs: o.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]


def decimate(ob, ratio):
    if ratio >= 1: return
    select_only(ob)
    m = ob.modifiers.new('Dec', 'DECIMATE'); m.ratio = ratio; m.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=m.name)


def unwrap(ob):
    """UVs (smart project) and their scale: cm of surface per UV unit, for the detail texture's tiling."""
    select_only(ob)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=.003, area_weight=0.)
    bpy.ops.object.mode_set(mode='OBJECT')
    me = ob.data; uv = me.uv_layers.active.data
    a3 = a2 = 0.
    for p in me.polygons:
        a3 += p.area
        L = [uv[li].uv for li in p.loop_indices]
        for i in range(1, len(L) - 1):
            a2 += abs((L[i].x - L[0].x) * (L[i + 1].y - L[0].y) - (L[i + 1].x - L[0].x) * (L[i].y - L[0].y)) * .5
    return math.sqrt(a3 / max(a2, 1e-9))


def field_object(name, body, h, ratio, look):
    ops = body.ops()
    V, Q, _ = K.field_mesh(name, ops, h)
    ob = K.make_mesh(name, V, Q)
    decimate(ob, ratio)
    V = verts_of(ob)
    N = K.gradient(ops, V); N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
    col, alpha = look(body, ops, V, N)
    set_colours(ob, col, alpha)
    K.log(name, f'verts={len(V)}', f'tris={sum(len(p.vertices) - 2 for p in ob.data.polygons)}')
    return ob, V


def membrane(name, edges, nu, nv, thick, scallop, billow, col_fn):
    """A wing membrane panel spanning two polylines from their roots (row 0) to their tips, the trailing edge between
    the tips scalloped inward, billowed, given thickness. edges: (A, B) arrays of points."""
    A = catmull(edges[0], nv); B = catmull(edges[1], nv)
    root = (A[0] + B[0]) / 2; tips = (A[-1] + B[-1]) / 2
    inward = root - tips; inward /= np.linalg.norm(inward)
    n = np.cross(A[-1] - A[0], B[-1] - B[0]); n /= np.linalg.norm(n)
    P = np.zeros((nv, nu, 3))
    for j in range(nv):
        v = j / (nv - 1)
        for i in range(nu):
            u = i / (nu - 1)
            P[j, i] = A[j] + (B[j] - A[j]) * u + inward * scallop * math.sin(math.pi * u) * v ** 2 + n * billow * math.sin(math.pi * u) * math.sin(math.pi * v)
    V = P.reshape(-1, 3)
    faces = [(j * nu + i, j * nu + i + 1, (j + 1) * nu + i + 1, (j + 1) * nu + i) for j in range(nv - 1) for i in range(nu - 1)]
    ob = K.mesh_from(name, V, faces)
    select_only(ob)
    m = ob.modifiers.new('Sol', 'SOLIDIFY'); m.thickness = thick; m.offset = 0.; m.use_even_offset = True
    bpy.ops.object.modifier_apply(modifier=m.name)
    W = verts_of(ob)
    c, a = col_fn(W)
    set_colours(ob, c, a)
    return ob


def join(obs, name):
    select_only(*obs)
    bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active; ob.name = name; ob.data.name = name
    return ob


def to_metres(ob):
    V = verts_of(ob) * .01
    ob.data.vertices.foreach_set('co', V.astype(np.float32).ravel()); ob.data.update()


def add_material(ob, name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    ob.data.materials.clear(); ob.data.materials.append(m)


def add_weights(ob, W, offset=0):
    for name, w in W.items():
        idx = np.nonzero(w > 1e-3)[0]
        if not len(idx): continue
        vg = ob.vertex_groups.get(name) or ob.vertex_groups.new(name=name)
        for i in idx: vg.add([int(i) + offset], float(w[i]), 'REPLACE')


# ------------------------------------------------------------------ the rig (the temple skeleton's, unchanged)
def skeleton_module():
    saved = sys.argv
    sys.argv = [saved[0], '--', '--no-clips']
    spec = importlib.util.spec_from_file_location('temple_skeleton', ROOT / 'Tools/build_temple_skeleton.py')
    mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    sys.argv = saved
    return mod


TSK = skeleton_module()
J = {}


def rig_build():
    TSK.bake_ao = lambda obj: None
    TSK.clear()
    rig, obj = TSK.build()
    bpy.data.objects.remove(obj)
    for b in rig.data.bones: J[b.name] = (np.array(b.head_local) * 100., np.array(b.tail_local) * 100.)
    return rig


def H(b): return J[b][0]
def T(b): return J[b][1]
def along(b, t): return H(b) + (T(b) - H(b)) * t
def dirn(b): d = T(b) - H(b); return d / np.linalg.norm(d)


def export_skinned(rig, ob, name):
    to_metres(ob)
    ob.parent = rig
    m = ob.modifiers.new('Armature', 'ARMATURE'); m.object = rig
    OUT.mkdir(parents=True, exist_ok=True)
    select_only(rig, ob)
    ob.select_set(True); rig.select_set(True); bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(OUT / f'{name}.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'}, bake_anim=False, **TSK.FBX)
    K.log('fbx', name)


def export_static(ob, name):
    to_metres(ob)
    OUT.mkdir(parents=True, exist_ok=True)
    select_only(ob)
    fbx = dict(TSK.FBX); fbx.pop('primary_bone_axis'); fbx.pop('secondary_bone_axis'); fbx.pop('use_armature_deform_only')
    bpy.ops.export_scene.fbx(filepath=str(OUT / f'{name}.fbx'), use_selection=True, object_types={'MESH'}, bake_anim=False, **fbx)
    K.log('fbx', name)


# ------------------------------------------------------------------ the lemure
def limb(body, bone, r0, r1, tag='skin', k=2.5, ext0=0., ext1=0.):
    a, b = H(bone) - dirn(bone) * ext0, T(bone) + dirn(bone) * ext1
    return body.tube(bone, [a, (a + b) / 2, b], [r0, (r0 + r1) / 2 * 1.06, r1], tag, k)


EYES_L = [V3(20.5, 3.4, 158), V3(20.3, -3.6, 157.4)]


def lemure_body():
    B = Body()
    # The mass: hips, a belly sagging forward, a hunched back with a hump of flesh over the shoulders.
    B.ells('pelvis', [ell((0, 0, 99), (16, 18, 14))], k=4)
    B.ells('spine_01', [ell((7, 0, 106), (18, 19, 15)), ell((10, 0, 99), (13, 15, 9))], k=5)
    B.ells('spine_02', [ell((2, 0, 120), (17, 20, 13))], k=5)
    B.ells('spine_03', [ell((2, 0, 135), (16, 21, 14)), ell((-9, 0, 145), (14, 17, 12)), ell((6, 0, 141), (11, 17, 9))], k=5)
    for s, S in ((1, 'l'), (-1, 'r')):
        B.balls(f'clavicle_{S}', [V3(-1, 19 * s, 141)], [9.5], k=4)
    # The neck sunk into the hump; the head run down and forward like wax.
    B.tube('neck_01', [V3(-2, 0, 145), V3(4, 0, 151), V3(9, 0, 155)], [13, 12, 10.5], k=5)
    # (sunk forward into the hump, run down over where a neck would be)
    B.ells('head', [ell((12, 0, 158), (10, 9.5, 10)), ell((15, 0, 151), (7.5, 9.5, 6))], k=5)
    B.balls('head', [V3(15, 6, 152), V3(15.5, -6.5, 151), V3(10, 7.5, 155), V3(10, -7, 154)], [5, 4.6, 4.5, 4.4], k=4)
    B.balls('jaw', [V3(17, 0, 147.5)], [5.8], k=3)
    # Its features: two pits for eyes, the mouth a sagging slit.
    B.sub(Balls('sub', 1., EYES_L, [2.3, 2.1]))
    B.sub(Ellipsoids('sub', 1.2, [ell((20.5, 0, 149.5), (3, 4.5, 1.2))]))
    # chin and jowls running down
    for y, L in ((0, 11), (5, 8), (-4.5, 9)):
        p = V3(19, y, 145.5)
        B.tube('jaw', [p, p + V3(.8, 0, -L * .6), p + V3(.6, 0, -L)], [2.6, 1.6, 1.1], k=2.)
        B.balls('jaw', [p + V3(.6, 0, -L - 1)], [1.9], k=1.)
    # Arms: heavy, the flesh drooping off them; hands lumps with stubs of fingers.
    for S in 'lr':
        limb(B, f'upperarm_{S}', 9, 7.2, ext0=2)
        limb(B, f'lowerarm_{S}', 7, 5.8)
        d = dirn(f'lowerarm_{S}')
        B.ells(f'lowerarm_{S}', [ell(along(f'lowerarm_{S}', .45) - V3(0, 0, 3), (6, 6.5, 9), d)], k=3)
        hd = dirn(f'hand_{S}')
        B.ells(f'hand_{S}', [ell(along(f'hand_{S}', .7), (4, 5.5, 7), hd)], k=2.5)
        tip = T(f'hand_{S}') + hd * 4
        side = np.cross(hd, V3(1, 0, 0)); side /= np.linalg.norm(side)
        for f in (-1.5, 0, 1.5):
            B.tube(f'hand_{S}', [tip - hd * 2 + side * f * 1.6, tip + hd * 2.5 + side * f * 2 + V3(1.5, 0, 0)], [2, 1.4], k=1.2)
        # Legs: thick, short-looking stumps (the flesh hangs to the knees), lumpy shins, wide flat feet.
        limb(B, f'thigh_{S}', 12.5, 9.5, ext0=3)
        B.balls(f'thigh_{S}', [along(f'thigh_{S}', .9) + V3(3, 0, -2), along(f'thigh_{S}', .55) + V3(-4, 0, 0)], [9.5, 9], k=3)   # flesh sagging over the knee
        limb(B, f'calf_{S}', 8.8, 7)
        B.balls(f'calf_{S}', [along(f'calf_{S}', .85) + V3(0, 0, -1)], [8], k=3)   # and pooled at the ankle
        B.ells(f'calf_{S}', [ell(along(f'calf_{S}', .3) + V3(-2, 0, 0), (6.5, 7, 10), dirn(f'calf_{S}'))], k=3)
        B.tube(f'foot_{S}', [H(f'foot_{S}') + V3(0, 0, -2), along(f'foot_{S}', .6) + V3(0, 0, -1.5)], [5.8, 5], k=2.5)
        B.tube(f'ball_{S}', [H(f'ball_{S}') + V3(0, 0, -.5), T(f'ball_{S}') + V3(1, 0, -.2)], [4.6, 3.6], k=2)
    # Drips of flesh hanging from the belly, chest, back and arms, each ending in a gobbet.
    rng = np.random.default_rng(7)
    for bone, centre, n, spread in (('spine_01', V3(16, 0, 101), 5, 12), ('spine_03', V3(12, 0, 133), 4, 13),
                                    ('spine_02', V3(-14, 0, 117), 4, 12), ('spine_03', V3(-18, 0, 141), 3, 12)):
        for i in range(n):
            y = (i / max(n - 1, 1) - .5) * 2 * spread + rng.uniform(-2, 2)
            top = centre + V3(rng.uniform(-1.5, 1.5) * np.sign(centre[0]), y, rng.uniform(-2, 2))
            L = rng.uniform(7, 15)
            pts = [top, top + V3(np.sign(centre[0]) * 1.2, 0, -L * .55), top + V3(np.sign(centre[0]) * 1.8, rng.uniform(-1, 1), -L)]
            B.tube(bone, pts, [2.6, 1.5, 1.1], k=2.)
            B.balls(bone, [pts[-1] + V3(0, 0, -1)], [rng.uniform(1.5, 2.1)], k=1.)
    for S in 'lr':
        for t in (.35, .7):
            p = along(f'lowerarm_{S}', t) - V3(0, 0, 5.5)
            L = rng.uniform(6, 10)
            B.tube(f'lowerarm_{S}', [p, p + V3(0, 0, -L)], [2.2, 1.1], k=1.5)
            B.balls(f'lowerarm_{S}', [p + V3(0, 0, -L - .8)], [1.7], k=1.)
    # Melting: flesh run down in long sags (noise stretched upright), drooping rolls round it, fine pitting over that.
    def melt(P):
        run = 1.9 * fbm(P * np.array([1 / 8., 1 / 8., 1 / 26.]), 3, 31)
        rolls = np.sin(P[:, 2] / 3.4 + 3.2 * fbm(P / 13., 2, 37)) * (.9 * smooth(85, 100, P[:, 2]) * smooth(150, 138, P[:, 2]) + .55 * smooth(90, 80, P[:, 2]))
        return run + rolls + .35 * fbm(P / 2.4, 2, 33)
    B.disp(melt)
    return B


def lemure_look(body, ops, V, N):
    convex = np.clip(laplacian(ops, V, 2.5) * 3., -1, 1)
    mott = smooth(-.5, .5, fbm(V / 22., 2, 7))
    col = lin(156, 86, 78)[None] * (1 - mott[:, None]) + lin(128, 60, 70)[None] * mott[:, None]
    # streaks running down it, redder and wetter
    st = smooth(.15, .55, fbm(V * np.array([1 / 6., 1 / 6., 1 / 40.]), 3, 9))
    col = col * (1 - .55 * st[:, None]) + lin(168, 66, 52)[None] * (.55 * st[:, None])
    hi = smooth(.05, .6, convex)[:, None]; lo = smooth(-.05, -.6, convex)[:, None]
    col = col * (1 - .55 * hi) + lin(200, 140, 122)[None] * (.55 * hi)
    col = col * (1 - .55 * lo) + lin(60, 14, 18)[None] * (.55 * lo)
    ao = sdf_ao(V, N, ops)
    col = col * (.3 + .7 * ao)[:, None]
    # The heat inside showing in the deepest folds, low on the body; the eye pits embers.
    fold = smooth(-.35, -.85, convex) * smooth(.75, .3, ao) * smooth(150, 110, V[:, 2])
    eye = np.max([smooth(3.2, 1.6, np.linalg.norm(V - e, axis=1)) for e in EYES_L], 0)
    alpha = np.clip(.45 * fold + .8 * eye, 0, 1)
    return col, alpha


# ------------------------------------------------------------------ the Pit Fiend
def fiend_body():
    """(2026-11-16, second pass, toward the user's goal image References/Phlegethos/pit-fiend-goal.jpg) Crimson, heavily
    muscled; long horns sweeping out and up; charcoal armour: spiked pauldrons, a collar, spiked bracers, a belt hung with
    skulls, layered plates down the thighs, knee guards, greaves, armoured clawed feet; great bat wings; a long tail
    curling round on the ground beside it. The flaming mace in its right fist is a separate mesh (fiend_mace)."""
    B = Body()
    # Torso: a deep chest and broad back, heavy pectorals, a ridged gut, a narrow waist.
    B.ells('pelvis', [ell((0, 0, 99), (14, 17, 13))], k=3)
    B.ells('spine_01', [ell((4, 0, 111), (12, 14.5, 11))], k=4)
    B.ells('spine_02', [ell((3, 0, 124), (14.5, 19.5, 12))], k=4)
    B.ells('spine_03', [ell((3, 0, 137), (16.5, 25.5, 14)), ell((11, 9, 139), (8, 10.5, 7.5)), ell((11, -9, 139), (8, 10.5, 7.5)),
                        ell((-6, 12, 135), (9.5, 9.5, 13)), ell((-6, -12, 135), (9.5, 9.5, 13)), ell((-3, 11, 149), (8.5, 11, 6.5)), ell((-3, -11, 149), (8.5, 11, 6.5))], k=3)
    # serratus and the rows of the gut
    for z, bone in ((105, 'spine_01'), (111.5, 'spine_01'), (118, 'spine_02'), (124, 'spine_02')):
        for s in (1, -1):
            B.ells(bone, [ell((13.2, s * 4.4, z), (2.8, 4.6, 3.1))], k=1.4)
    for s in (1, -1):
        for z in (124, 129, 134):
            B.ells('spine_02', [ell((7, s * 15.5, z), (3, 3, 2.2), V3(.5, s * .3, -.6))], k=1.2)
    B.tube('neck_01', [V3(-3, 0, 146), V3(2, 0, 154), V3(5, 0, 161)], [13, 11, 9], k=4)
    # Head: a heavy skull, a deep brow, a fanged snarling muzzle (the lower jaw on the rig's jaw bone), pointed ears.
    B.ells('head', [ell((5, 0, 167), (10.5, 9.4, 10.5)), ell((12.5, 0, 163), (8, 7.4, 5.2)), ell((13, 0, 170.5), (3.8, 8.8, 2.7))], k=2.5)
    B.ells('head', [ell((14.8, 0, 166), (3.2, 2.6, 3.3), V3(1, 0, -.5))], k=1.5)   # nose bridge
    B.ells('jaw', [ell((12, 0, 157.4), (6.4, 5.8, 2.8), V3(1, 0, -.35))], k=2)
    for s in (1, -1):
        B.ells('head', [ell((1, s * 9.2, 168.5), (1.2, 2.4, 7.5), V3(-.6, s * .9, .45))], k=1.2)   # ears, swept back and out
        B.ells('head', [ell((10, s * 6.5, 160.5), (4, 2.5, 3), V3(1, s * .4, 0))], k=1.5)          # snarling cheeks
        # The horns: thick from the crown, out to the side, then up and in, the tips turned inward.
        horn = catmull([V3(4, s * 4, 173), V3(1.5, s * 10, 178), V3(-3, s * 20, 180.5), V3(-6.5, s * 30, 184), V3(-8.5, s * 37, 192),
                        V3(-7.5, s * 39.5, 203), V3(-3.5, s * 36.5, 212), V3(1, s * 30.5, 217)], 40)
        ht = np.linspace(0, 1, len(horn))
        B.tube('head', horn, 6.2 * (1 - ht) ** .75 + .4, tag='horn', k=1.2)
        # fangs, upper and lower
        B.tube('head', [V3(16.8, s * 2.7, 160.4), V3(17.1, s * 2.5, 156.8)], [.85, .1], tag='tooth', k=.2)
        B.tube('jaw', [V3(16.2, s * 2.3, 156.6), V3(16.5, s * 2.2, 159.8)], [.7, .08], tag='tooth', k=.2)
        B.tube('head', [V3(16.6, s * 1.1, 160), V3(16.8, s * 1., 158.4)], [.45, .08], tag='tooth', k=.2)
    B.sub(Balls('sub', .8, [V3(15, 3.4, 168), V3(15, -3.4, 168)], [1.9, 1.9]))
    B.balls('head', [V3(14.6, 3.4, 167.9), V3(14.6, -3.4, 167.9)], [1.25, 1.25], tag='eye', k=.3)
    B.sub(Ellipsoids('sub', .6, [ell((16.2, 0, 159.2), (4, 4, 1.3))]))    # the open snarl
    # The collar: a dark plate ring round the base of the neck.
    B.ells('spine_03', [ell((-1, 0, 148), (15.5, 21, 4.2))], tag='armour', k=.7)
    for S, s in (('l', 1), ('r', -1)):
        ua, la, hn = f'upperarm_{S}', f'lowerarm_{S}', f'hand_{S}'
        # Arms: a deltoid under a layered, spiked pauldron; biceps, triceps; a thick forearm in a spiked bracer.
        B.balls(ua, [V3(0, s * 22, 141)], [11], k=3)
        limb(B, ua, 10.4, 8.2)
        d = dirn(ua)
        B.ells(ua, [ell(along(ua, .45) + V3(3.5, 0, 0), (6, 6, 9.5), d), ell(along(ua, .5) + V3(-3.5, 0, 0), (5.5, 5.6, 10), d)], k=2)
        top = H(ua) + V3(0, s * 3, 5.5)
        B.ells(ua, [ell(top, (14, 12.5, 6.5), V3(0, s * .45, 1)), ell(top + V3(0, s * 5, -6.5), (12, 10.5, 5), V3(0, s * .9, .65)),
                    ell(top + V3(0, s * 8.5, -12.5), (10, 9, 4), V3(0, s * 1, .35))], tag='armour', k=.6)
        for i, (dx, dz) in enumerate(((-5, 0), (2, 1), (8, -1))):
            b = top + V3(dx, s * 5, 4 + dz)
            B.tube(ua, [b, b + V3(dx * .25, s * 4, 7.5 - i)], [2.1, .15], tag='armour', k=.5)
        limb(B, la, 8.2, 6.2)
        dl = dirn(la)
        B.ells(la, [ell(along(la, .3), (7.6, 7.8, 10), dl)], k=2.5)
        B.tube(la, [along(la, .22), along(la, .6), along(la, .96)], [9.2, 8.4, 7.2], tag='armour', k=.5)
        for t in (.22, .96):
            B.ells(la, [ell(along(la, t), (10.2 - 2 * t, 10.2 - 2 * t, 1.6), dl)], tag='armour', k=.4)
        out = np.cross(dl, V3(0, 0, 1)) * s; out /= max(np.linalg.norm(out), 1e-6)
        out = (out + V3(-.6, 0, 0)); out /= np.linalg.norm(out)
        for t in (.4, .62, .84):
            b = along(la, t) + out * 7
            B.tube(la, [b, b + out * 5.5 + dl * 2.5], [1.8, .12], tag='armour', k=.4)
        B.tube(la, [H(la) + V3(-4, s * 1, 0), H(la) + V3(-10.5, s * 2.5, 3)], [2, .2], tag='horn', k=.8)   # elbow spur
        hd = dirn(hn)
        side = np.cross(hd, V3(1, 0, 0)); side /= np.linalg.norm(side)
        if S == 'l':
            # The left hand open, clawed.
            B.ells(hn, [ell(along(hn, .6), (3, 5, 6.5), hd)], k=1.5)
            for f in (-1.5, -.5, .5, 1.5):
                base = T(hn) + side * f * 2 - hd * 1.5
                pts = [base, base + hd * 5 + side * f * .7, base + hd * 8 + V3(2.4, 0, 0) + side * f * 1]
                B.tube(hn, pts, [1.45, 1.2, .9], k=.8)
                B.tube(hn, [pts[-1], pts[-1] + hd * 1.6 + V3(2.6, 0, 0)], [.85, .05], tag='claw', k=.3)
            th = H(hn) + V3(3, 0, -2) - side * 3
            B.tube(hn, [th, th + V3(3, -s * .5, -3)], [1.5, 1.05], k=.8)
            B.tube(hn, [th + V3(3, -s * .5, -3), th + V3(5, -s * .6, -4)], [.95, .05], tag='claw', k=.3)
        else:
            # The right a fist round the mace's haft (MACE_GRIP).
            g = MACE_GRIP(); dmace = MACE_DIR()
            B.ells(hn, [ell(along(hn, .45), (3.2, 5, 5.5), hd), ell(g + hd * 1.5, (4.8, 5.4, 5.6), dmace)], k=1.6)
            for f in (-1.5, -.5, .5, 1.5):
                k_ = g + dmace * f * 2.1 + hd * 4.2
                B.balls(hn, [k_], [1.75], k=.8)                                 # knuckles over the haft
                B.tube(hn, [k_ + hd * 1, k_ + hd * 1.5 + np.cross(dmace, hd) * 3.5], [.8, .05], tag='claw', k=.3)
            B.tube(hn, [g - hd * 1 + np.cross(dmace, hd) * 4, g + hd * 2.5 + np.cross(dmace, hd) * 4.5], [1.6, 1.2], k=.8)   # the thumb across
        # Legs: thick; plates down the front and outside of each thigh (on the thigh, so they open with the stride),
        # knee guards with a spike, greaves, armoured clawed feet.
        th_, ca, ft, bl = f'thigh_{S}', f'calf_{S}', f'foot_{S}', f'ball_{S}'
        limb(B, th_, 13.5, 10, ext0=2)
        B.ells(th_, [ell(along(th_, .4) + V3(4.5, 0, 0), (8, 8.5, 14.5), dirn(th_))], k=3)
        for i in range(4):
            t = .1 + i * .17
            c = along(th_, t) + V3(12.5 - i * .6, -s * 2.5, 0)
            B.ells(th_, [ell(c, (1.6, 7.4 - i * .4, 5.6))], tag='armour', k=.45)
        for i in range(3):
            t = .08 + i * .18
            c = along(th_, t) + V3(0, s * 12.8, 0)
            B.ells(th_, [ell(c, (7.5 - i * .4, 1.6, 5.8))], tag='armour', k=.45)
        limb(B, ca, 9.4, 6.2)
        dc = dirn(ca)
        B.ells(ca, [ell(along(ca, .3) + V3(-3.5, 0, 0), (7, 6.8, 12), dc)], k=2.5)
        B.tube(ca, [along(ca, .16) + V3(2.5, 0, 0), along(ca, .5) + V3(2, 0, 0), along(ca, .9) + V3(1.5, 0, 0)], [8.8, 7.9, 6.6], tag='armour', k=.5)
        kn = H(ca) + V3(7.5, 0, 1)
        B.ells(ca, [ell(kn, (3.4, 7, 7.2), V3(1, 0, .15))], tag='armour', k=.5)
        B.tube(ca, [kn + V3(2.5, 0, 0), kn + V3(8, 0, 2.5)], [2.3, .15], tag='armour', k=.4)
        B.tube(ft, [H(ft) + V3(-3, 0, -2.5), along(ft, .7) + V3(0, 0, -2)], [6, 5.2], tag='armour', k=1.6)
        B.tube(ft, [H(ft) + V3(-4.5, 0, -3), H(ft) + V3(-10.5, 0, -5.6)], [2.3, .3], tag='claw', k=.6)  # heel spur
        bt = T(ft)
        for f in (-1, 0, 1):
            a = bt + V3(-1, f * 3.6, 0)
            pts = [a, a + V3(5.5, f * 1.5, -1.2), a + V3(9.5, f * 2.2, -3.2)]
            B.tube(bl, pts, [2.9, 2.4, 1.8], tag='armour', k=.8)
            B.tube(bl, [pts[-1], pts[-1] + V3(2.8, f * .4, -2.6)], [1.5, .1], tag='claw', k=.3)
    # The belt, its buckle, three skulls hung on it.
    B.ells('pelvis', [ell((1, 0, 104), (16.2, 19.2, 3.8))], tag='armour', k=.5)
    B.ells('pelvis', [ell((16.2, 0, 104), (2.4, 6, 5.2))], tag='armour', k=.5)
    for c in (V3(14.5, 10.5, 98.5), V3(16.2, -3.5, 97), V3(14, -12, 99)):
        B.balls('pelvis', [c], [3.2], tag='bone', k=.4)
        B.ells('pelvis', [ell(c + V3(1.6, 0, -2.6), (2, 2.4, 1.4))], tag='bone', k=.6)
        B.sub(Balls('sub', .4, [c + V3(2.8, 1.2, .2), c + V3(2.8, -1.2, .2)], [.95, .95]))
    # Spines down the back.
    for z, bone in ((108, 'spine_01'), (115, 'spine_01'), (122, 'spine_02'), (129, 'spine_02'), (136, 'spine_03'), (143, 'spine_03')):
        b = V3(-12.5 + (z - 108) * .05, 0, z)
        B.tube(bone, [b, b + V3(-5.5, 0, 3)], [1.6, .1], tag='horn', k=.8)
    # The tail (2026-11-16 third pass, on its own eight bones so the game can swing it): from under the spine's base, down
    # and back, its last half trailing on the ground behind, turning a little to its left; the end dark and barbed.
    tail = catmull([V3(-11, 0, 99), V3(-26, 0, 84), V3(-42, 2, 60), V3(-58, 5, 37), V3(-75, 9, 19), V3(-94, 15, 10),
                    V3(-113, 23, 8), V3(-130, 33, 9), V3(-142, 45, 11)], 64)
    r = np.linspace(7.5, 1.1, len(tail))
    seg = np.cumsum(np.r_[0, np.linalg.norm(np.diff(tail, axis=0), axis=1)]); seg /= seg[-1]
    joints = [tail[np.argmin(np.abs(seg - k / TAIL_BONES))] for k in range(TAIL_BONES + 1)]
    n_dark = int(len(tail) * .72)
    for k in range(TAIL_BONES):
        i0, i1 = np.argmin(np.abs(seg - k / TAIL_BONES)), np.argmin(np.abs(seg - (k + 1) / TAIL_BONES)) + 1
        i1 = min(i1, len(tail))
        for a, b, tag in ((i0, min(i1, n_dark + 1), 'skin'), (max(i0, n_dark), i1, 'tip')):
            if b - a >= 2: B.tube(f'tail_{k + 1:02d}', tail[a:b], r[a:b], tag=tag, k=3 if tag == 'skin' else 1.)
    B.tube(f'tail_{TAIL_BONES:02d}', [tail[-1], tail[-1] + (tail[-1] - tail[-3]) * 1.6], [1.1, .08], tag='tip', k=.5)
    for t_ in (.8, .87, .94):   # barbs down the end
        i = int(len(tail) * t_); d_ = tail[i + 1] - tail[i - 1]; d_ /= np.linalg.norm(d_)
        for s_ in (1, -1):
            side_ = np.cross(d_, V3(0, 0, 1)) * s_
            B.tube(f'tail_{min(TAIL_BONES, int(t_ * TAIL_BONES) + 1):02d}', [tail[i], tail[i] + side_ * 3.2 - d_ * 2 + V3(0, 0, 1.5)], [1., .06], tag='tip', k=.4)
    # Wing bones (rigid on the upper spine): the arm up and out from the shoulder blades, fingers spread down from the
    # wrist; the membranes between are added below.
    wing = {}
    for S, s in (('l', 1), ('r', -1)):
        root = V3(-12, s * 11, 145)
        Wp = lambda x, y, z: root + (V3(x, s * y, z) - root) * 1.5   # (the goal's great wings: the first pass's, 1.5 times)
        elbow, wrist, mid = Wp(-26, 40, 176), Wp(-30, 64, 160), Wp(-19, 25, 162)
        fingers = [[wrist, Wp(-38, 84, 146), Wp(-44, 101, 122)], [wrist, Wp(-41, 77, 135), Wp(-49, 84, 104)],
                   [wrist, Wp(-38, 66, 128), Wp(-44, 62, 98)]]
        flank = [wrist, Wp(-24, 34, 132), V3(-15, s * 15, 116)]
        # (third pass: on their own bones - arm, forearm, three fingers - so the game can fold, spread and beat them)
        B.tube(f'wing_arm_{S}', [root, mid, elbow], [5, 4.2, 3.4], tag='wing', k=2.5)
        B.tube(f'wing_fore_{S}', [elbow, wrist], [3.1, 2.4], tag='wing', k=1.5)
        for i, fp in enumerate(fingers):
            B.tube(f'wing_f{i + 1}_{S}', catmull(fp, 8), np.linspace(2.1, .6, 8), tag='wing', k=1.)
        B.tube(f'wing_arm_{S}', [elbow, elbow + V3(-1, s * 1.5, 9)], [2, .1], tag='claw', k=.5)    # a hook at the elbow
        B.tube(f'wing_fore_{S}', [wrist, wrist + V3(3, s * 2, 7)], [1.5, .1], tag='claw', k=.5)    # and the thumb's
        wing[S] = (root, elbow, wrist, fingers, mid, flank)
    B.disp(lambda P: .08 * fbm(P / 1.9, 2, 41) + .3 * fbm(P / 7.5, 2, 43))   # (smooth hide: the goal's)
    wing['tail'] = joints
    return B, wing


# The mace in the right fist: the grip's centre and the haft's direction (out, forward and up from the A-pose's hanging hand).
def MACE_GRIP(): return along('hand_r', .62) + np.cross(dirn('hand_r'), V3(1, 0, 0)) * -.0 + V3(1.2, 0, 0)
def MACE_DIR():
    d = V3(.3, -.1, 1.); return d / np.linalg.norm(d)   # (upright beside it, as the goal's)
MACE_HEAD_AT = 84.     # cm from the grip to the head's centre (rig units; 2.4 times in the game)


def fiend_mace():
    B = Body()
    g, d = MACE_GRIP(), MACE_DIR()
    e1 = np.cross(d, V3(1, 0, 0)); e1 /= np.linalg.norm(e1); e2 = np.cross(d, e1)
    # The haft: iron, bound in spirals of leather, collars at the grip and under the head, a pommel.
    B.tube('hand_r', [g - d * 26, g + d * 76], [1.9, 1.75], tag='iron', k=.5)
    for t in np.linspace(-14, 70, 120):
        a = t * .9
        B.balls('hand_r', [g + d * t + (e1 * math.cos(a) + e2 * math.sin(a)) * 1.6], [.75], tag='wrap', k=.4)
    for t, r in ((-26, 3.3), (-8.5, 2.6), (8.5, 2.6), (73, 3.)):
        B.ells('hand_r', [ell(g + d * t, (r, r, 1.3 if t > -20 else 2.6), d)], tag='iron', k=.4)
    hc = g + d * MACE_HEAD_AT
    # The head: a heavy iron core, six flanges standing off it, spikes between, a spike on top.
    B.ells('hand_r', [ell(hc, (6.8, 6.8, 10.5), d)], tag='head', k=.8)
    for i in range(6):
        a = i * math.pi / 3; rr = e1 * math.cos(a) + e2 * math.sin(a)
        B.tube('hand_r', [hc - d * 9 + rr * 4.5, hc - d * 2 + rr * 10.5, hc + d * 5 + rr * 10, hc + d * 10 + rr * 4.5], [1.5, 1.7, 1.6, 1.2], tag='head', k=.5)
        a2 = a + math.pi / 6; r2 = e1 * math.cos(a2) + e2 * math.sin(a2)
        B.tube('hand_r', [hc + r2 * 5.5, hc + r2 * 13.5 + d * 1.5], [2.2, .12], tag='head', k=.4)
    B.tube('hand_r', [hc + d * 9.5, hc + d * 19], [2.4, .12], tag='head', k=.5)
    B.disp(lambda P: .22 * fbm(P / 1.4, 2, 61))
    return B, hc


def mace_look(body, ops, V, N):
    D = body.part_dists(V); tags = body.tag_mask(V, D)
    convex = np.clip(laplacian(ops, V, 1.2) * 2., -1, 1)
    col = lin(46, 42, 42)[None] * np.ones((len(V), 1))
    col = col * (1 - tags.get('wrap', 0)[:, None]) + lin(78, 52, 36)[None] * tags.get('wrap', 0)[:, None]
    col = col * (1 - .5 * smooth(.05, .5, convex)[:, None]) + lin(110, 100, 96)[None] * (.5 * smooth(.05, .5, convex)[:, None])
    col = col * (1 - .6 * smooth(-.05, -.5, convex)[:, None]) + lin(10, 9, 9)[None] * (.6 * smooth(-.05, -.5, convex)[:, None])
    # The head red-hot in its cracks, black-crusted over that.
    hot = tags.get('head', 0) * smooth(.25, .65, fbm(V / 2.2, 3, 71) * .5 + .5)
    col = col * (1 - hot[:, None]) + lin(196, 62, 14)[None] * hot[:, None]
    ao = sdf_ao(V, N, ops, .6)
    col = col * (.35 + .65 * ao)[:, None]
    return col, np.zeros(len(V))   # (no glow: MI_PitFiend's GlowStrength is 30, for the eyes, and under Phlegethos's exposure the head burned pale at every alpha down to .015; the game's fire on it does the burning)


def affine_weights(P, bones):
    """P as an affine combination of the bones' heads: true under any affine map, so the game finds the same point from its
    own reference pose whatever its axes and units."""
    A = np.stack([np.append(H(b), 1.) for b in bones], 1)
    w = np.linalg.solve(A, np.append(P, 1.))
    return {b: round(float(x), 5) for b, x in zip(bones, w)}, float(np.linalg.cond(A))


EYES_F = [V3(14.6, 3.4, 167.9), V3(14.6, -3.4, 167.9)]


def fiend_look(body, ops, V, N):
    D = body.part_dists(V)
    tags = body.tag_mask(V, D)
    convex = np.clip(laplacian(ops, V, 2.) * 3., -1, 1)
    mott = fbm(V / 14., 2, 17) * .5 + .5
    col = lin(172, 46, 40)[None] * (1 - mott[:, None]) + lin(136, 30, 30)[None] * mott[:, None]
    # darker along the back and the outsides
    back = smooth(.1, -.7, N[:, 0])
    col = col * (1 - .4 * back[:, None]) + lin(86, 18, 18)[None] * (.4 * back[:, None])
    # dark blotches over the shoulders and upper chest (the goal's)
    blot = smooth(.62, .72, fbm(V / 3.2, 2, 23) * .5 + .5) * smooth(128, 138, V[:, 2]) * smooth(156, 148, V[:, 2])
    col = col * (1 - .8 * blot[:, None]) + lin(40, 8, 8)[None] * (.8 * blot[:, None])
    # chest and gut a hotter red
    front = smooth(.4, .9, N[:, 0]) * smooth(98, 108, V[:, 2]) * smooth(150, 138, V[:, 2]) * smooth(22, 10, np.abs(V[:, 1]))
    col = col * (1 - .4 * front[:, None]) + lin(196, 74, 56)[None] * (.4 * front[:, None])
    hi = smooth(.05, .6, convex)[:, None]; lo = smooth(-.05, -.6, convex)[:, None]
    col = col * (1 - .45 * hi) + lin(214, 104, 82)[None] * (.45 * hi)
    col = col * (1 - .7 * lo) + lin(40, 6, 8)[None] * (.7 * lo)
    # The armour: charcoal iron, its edges lit, its seams black.
    arm = np.clip(tags.get('armour', 0), 0, 1)
    ac = lin(50, 48, 52)[None] * (1 - .25 * mott[:, None])
    ac = ac * (1 - .6 * hi) + lin(118, 114, 120)[None] * (.6 * hi)
    ac = ac * (1 - .7 * lo) + lin(10, 10, 12)[None] * (.7 * lo)
    col = col * (1 - arm[:, None]) + ac * arm[:, None]
    # Horns dark maroon at the root to black at the tips, ringed; claws black; teeth yellowed; skulls old bone.
    ht = smooth(178, 208, V[:, 2])[:, None]
    horn = lin(86, 30, 26)[None] * (1 - ht) + lin(20, 14, 14)[None] * ht
    ring = .78 + .22 * np.sin(np.linalg.norm(V - V3(-3, 0, 176), axis=1) * 2.4)
    col = col * (1 - tags.get('horn', 0)[:, None]) + horn * ring[:, None] * tags.get('horn', 0)[:, None]
    col = col * (1 - tags.get('wing', 0)[:, None]) + lin(92, 22, 20)[None] * tags.get('wing', 0)[:, None]
    col = col * (1 - tags.get('tip', 0)[:, None]) + lin(26, 16, 18)[None] * tags.get('tip', 0)[:, None]
    col = col * (1 - tags.get('claw', 0)[:, None]) + lin(14, 11, 10)[None] * tags.get('claw', 0)[:, None]
    col = col * (1 - tags.get('tooth', 0)[:, None]) + lin(214, 196, 150)[None] * tags.get('tooth', 0)[:, None]
    col = col * (1 - tags.get('bone', 0)[:, None]) + lin(200, 186, 156)[None] * (1 - .5 * lo) * tags.get('bone', 0)[:, None]
    ao = sdf_ao(V, N, ops)
    col = col * (.3 + .7 * ao)[:, None]
    eye = np.max([smooth(1.7, 1., np.linalg.norm(V - e, axis=1)) for e in EYES_F], 0)
    col = col * (1 - eye[:, None]) + lin(255, 200, 80)[None] * eye[:, None]
    mouth = smooth(1.6, .5, np.abs(V[:, 2] - 159.2)) * smooth(10, 14, V[:, 0]) * smooth(4.5, 2.5, np.abs(V[:, 1])) * smooth(.6, .2, ao)
    alpha = np.clip(eye + .55 * mouth, 0, 1)
    return col, alpha


def membrane_colour(W):
    vein = smooth(.88, .97, 1 - np.abs(fbm(W / 5., 3, 51)))
    col = lin(132, 36, 32)[None] * np.ones((len(W), 1))
    col = col * (1 - .55 * vein[:, None]) + lin(70, 16, 16)[None] * (.55 * vein[:, None])
    col *= (.75 + .3 * (fbm(W / 14., 2, 53) * .5 + .5))[:, None]
    return col, np.zeros(len(W))   # (no glow: a vein mask here lit the membranes in patches)


TAIL_BONES = 8


def fiend_bones(wing):
    """The fiend's own bones beyond the temple rig (cm): each wing's arm, forearm and three fingers off spine_03, the
    tail's eight off the pelvis. Unreal merges them into the shared skeleton; the skeleton's clips leave them at rest and
    the game moves them (SkeletonAnim.cpp, the fiend's layers)."""
    bones = {}
    for S in ('l', 'r'):
        root, elbow, wrist, f, mid, flank = wing[S]
        bones[f'wing_arm_{S}'] = (root, elbow, 'spine_03')
        bones[f'wing_fore_{S}'] = (elbow, wrist, f'wing_arm_{S}')
        for i, fp in enumerate(f):
            bones[f'wing_f{i + 1}_{S}'] = (wrist, fp[-1], f'wing_fore_{S}')
            # (a leaf at each finger's end, so the game knows which way the finger points)
            d = fp[-1] - wrist; bones[f'wing_t{i + 1}_{S}'] = (fp[-1], fp[-1] + d / np.linalg.norm(d) * 5, f'wing_f{i + 1}_{S}')
    j = wing['tail']
    for k in range(TAIL_BONES): bones[f'tail_{k + 1:02d}'] = (j[k], j[k + 1], 'pelvis' if k == 0 else f'tail_{k:02d}')
    d = j[-1] - j[-2]; bones['tail_tip'] = (j[-1], j[-1] + d / np.linalg.norm(d) * 5, f'tail_{TAIL_BONES:02d}')
    return bones


def add_bones(rig, bones):
    select_only(rig); bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    eb = rig.data.edit_bones
    for name, (h, t, parent) in bones.items():
        b = eb.new(name); b.head = tuple(np.asarray(h) / 100.); b.tail = tuple(np.asarray(t) / 100.)
        b.parent = eb[parent]; b.use_connect = False; b.roll = 0.
    bpy.ops.object.mode_set(mode='OBJECT')
    for b in rig.data.bones: J[b.name] = (np.array(b.head_local) * 100., np.array(b.tail_local) * 100.)


def seg_near(P, A, B):
    """Each point's distance to segment AB and its fraction along it."""
    d = B - A; t = np.clip(((P - A) @ d) / max(d @ d, 1e-9), 0, 1)
    return np.linalg.norm(P - (A + t[:, None] * d), axis=1), t


def membrane_weights(W, edges):
    """A membrane's vertices between the bone edges round it: edges = [(points, bone of each point)]. Each vertex takes
    both edges, weighted by the other's distance; each edge's bones by where along it the vertex is nearest."""
    per = []
    for pts, bones in edges:
        D = np.full(len(W), 1e9); Wt = {n: np.zeros(len(W)) for n in set(bones)}
        for i in range(len(pts) - 1):
            d, t = seg_near(W, pts[i], pts[i + 1])
            m = d < D; D = np.where(m, d, D)
            for n in Wt: Wt[n] = np.where(m, 0., Wt[n])
            Wt[bones[i]] = Wt[bones[i]] + np.where(m, 1 - t, 0.); Wt[bones[i + 1]] = Wt[bones[i + 1]] + np.where(m, t, 0.)
        per.append((D, Wt))
    (Da, Wa), (Db, Wb) = per
    a = Db / np.maximum(Da + Db, 1e-6)
    out = {}
    for n, w in Wa.items(): out[n] = out.get(n, 0) + w * a
    for n, w in Wb.items(): out[n] = out.get(n, 0) + w * (1 - a)
    tot = sum(out.values()); return {n: w / np.maximum(tot, 1e-9) for n, w in out.items()}


def fiend_wings(wing):
    obs = []
    for S, s in (('l', 1), ('r', -1)):
        root, elbow, wrist, f, mid, flank = wing[S]
        # (between the fingers, the last to the flank, and the flank up under the arm to the shoulder)
        panels = [([wrist, elbow, mid, root], flank, 10, 14, 1.5, 2.), (f[0], f[1], 9, 16, 6.5, 3.), (f[1], f[2], 9, 16, 6.5, 3.),
                  (f[2], flank, 12, 16, 7.5, 3.5)]
        # The bone edges round the panels: the arm chain (wing_fore from the wrist, wing_arm from the elbow in), each
        # finger, and the flank from the wrist (wing_fore) to the body's side (spine_02).
        arm = ([wrist, elbow, mid, root], [f'wing_fore_{S}', f'wing_arm_{S}', f'wing_arm_{S}', f'wing_arm_{S}'])
        fing = [(catmull(fp, 8), [f'wing_f{i + 1}_{S}'] * 8) for i, fp in enumerate(f)]
        fl = (catmull(flank, 8), [f'wing_fore_{S}'] * 3 + ['spine_02'] * 5)
        edges = [[arm, fl], [fing[0], fing[1]], [fing[1], fing[2]], [fing[2], fl]]
        for i, (A, Bp, nu, nv, sc, bl) in enumerate(panels):
            ob = membrane(f'wing_{S}{i}', (A, Bp), nu, nv, .9, sc, bl * s, membrane_colour)
            add_weights(ob, membrane_weights(verts_of(ob), edges[i]))
            obs.append(ob)
    return obs


# ------------------------------------------------------------------ the spined devil (static parts, cm, each about its pivot)
def spines_on(B, bone, centre, radii, n, length, seed, tag='spine', facing=V3(-1, 0, .6), spread=.9, r0=.75):
    """n spines standing off an ellipsoid's surface on the side facing `facing`."""
    rng = np.random.default_rng(seed)
    fdir = facing / np.linalg.norm(facing)
    made = 0; tries = 0
    while made < n and tries < n * 40:
        tries += 1
        u = rng.normal(size=3); u /= np.linalg.norm(u)
        if np.dot(u, fdir) < 1 - spread: continue
        p = centre + u * radii * .92
        L = length * rng.uniform(.7, 1.2)
        tipd = (u + fdir * .6); tipd /= np.linalg.norm(tipd)
        B.tube(bone, [p, p + tipd * L], [r0, .04], tag=tag, k=.35)
        made += 1


def devil_body():
    B = Body()
    # Crouched on its haunches, leaning forward, forearms on its knees.
    B.ells('body', [ell((-1.5, 0, 22), (6.8, 6.4, 11.5), V3(.35, 0, 1)), ell((2, 0, 17), (5.6, 6, 6.5)), ell((-2, 0, 30), (6, 8.2, 5.2))], k=2.5)
    for s in (1, -1):
        B.ells('body', [ell((4, s * 6.5, 10), (9, 4.2, 4.6), V3(1, 0, .25))], k=1.8)      # thigh, forward
        B.tube('body', [V3(11, s * 6.8, 11), V3(9.5, s * 6.8, 5), V3(8, s * 6.6, 1.6)], [3.1, 2.4, 2], k=1.2)   # shin, down
        B.tube('body', [V3(6.5, s * 6.6, 1.2), V3(11, s * 6.8, 1.2)], [2.2, 1.6], k=1)   # foot
        for f in (-1, 0, 1):
            a = V3(11, s * 6.8 + f * 1.3, 1.3)
            B.tube('body', [a, a + V3(2.2, f * .6, -.2), a + V3(3.2, f * .8, -1.6)], [.75, .55, .1], tag='claw', k=.25)
        B.tube('body', [V3(1, s * 8, 28.5), V3(7, s * 10, 20), V3(13, s * 7.5, 13.5)], [2.6, 2.2, 1.8], k=1.2)   # arm
        B.ells('body', [ell((14, s * 7.2, 12.4), (2.4, 1.8, 2.2))], k=.8)
        for f in (-1, 0, 1):
            a = V3(15, s * 7.2 + f * 1.1, 12)
            B.tube('body', [a, a + V3(2, f * .4, -1.4), a + V3(2.6, f * .5, -3)], [.6, .45, .06], tag='claw', k=.25)
        spines_on(B, 'body', V3(7, s * 10, 20), V3(2.4, 2.4, 2.4), 4, 3.6, 11 + s, facing=V3(-.4, s * .8, .5), spread=.8, r0=.55)
    spines_on(B, 'body', V3(-1.5, 0, 22), V3(6.8, 6.4, 11.5), 34, 6.5, 3)
    spines_on(B, 'body', V3(-2, 0, 30), V3(6.5, 8.5, 5.5), 12, 5.5, 5, facing=V3(-.5, 0, 1))
    B.disp(lambda P: .22 * fbm(P / 1.2, 2, 61) + .3 * fbm(P / 4., 2, 63))
    return B


DEVIL_EYES = [V3(7.3, 2.15, 4.1), V3(7.3, -2.15, 4.1)]


def devil_head():
    B = Body()
    B.ells('head', [ell((3.5, 0, 3.5), (5, 4.3, 4.5)), ell((9.2, 0, 1.6), (5.2, 2.4, 2.2), V3(1, 0, -.3)), ell((6.6, 0, 5.5), (1.7, 4.3, 1.3))], k=1.5)
    B.ells('head', [ell((8.2, 0, -.7), (4.6, 2., 1.1), V3(1, 0, -.45))], k=.9)   # jaw
    B.tube('head', [V3(0, 0, -3), V3(2.5, 0, 1)], [3, 3.4], k=1.5)              # neck
    for s in (1, -1):
        B.ells('head', [ell((.4, s * 5.2, 7.6), (.5, 2.2, 6.8), V3(-.45, s * .85, .9))], k=.7)   # pointed bat ears
        horn = catmull([V3(3.6, s * 2.4, 7.4), V3(1.6, s * 3.5, 10.4), V3(-1.8, s * 3.8, 12.6), V3(-5.2, s * 3.2, 13.2)], 10)
        B.tube('head', horn, np.linspace(1.3, .08, 10), tag='claw', k=.5)
        B.tube('head', [V3(12, s * 1.1, .7), V3(12.2, s * 1., -1.7)], [.42, .04], tag='tooth', k=.12)
        B.tube('head', [V3(10.6, s * 1.4, .2), V3(10.8, s * 1.3, -1.2)], [.32, .04], tag='tooth', k=.1)
    B.sub(Balls('sub', .45, DEVIL_EYES, [1., 1.]))
    B.balls('head', DEVIL_EYES, [.6, .6], tag='eye', k=.12)
    B.sub(Ellipsoids('sub', .3, [ell((11, 0, .3), (2.8, 1.9, .45))]))
    spines_on(B, 'head', V3(2.5, 0, 4.5), V3(4.6, 4.2, 4.4), 10, 3.2, 21, facing=V3(-1, 0, .7), spread=.75, r0=.45)
    B.disp(lambda P: .15 * fbm(P / 1., 2, 65))
    return B


def devil_wing(s):
    B = Body()
    # From its pivot at the shoulder blade: folded up and back, the arm raised, three fingers down.
    elbow, wrist = V3(-6.5, s * 8, 12), V3(-9, s * 12, 23)
    B.tube('wing', [V3(0, 0, 0), elbow, wrist], [1.5, 1.2, .9], k=.8)
    tips = [V3(-20, s * 23, 7), V3(-21, s * 16, -3), V3(-14, s * 8, -8)]
    for t in tips: B.tube('wing', catmull([wrist, (wrist + t) / 2 + V3(-1, 0, 1), t], 6), np.linspace(.7, .2, 6), k=.4)
    B.tube('wing', [wrist, wrist + V3(1, s * .4, 2.6)], [.6, .05], tag='claw', k=.2)
    return B, wrist, tips


def devil_tail():
    B = Body()
    pts = catmull([V3(0, 0, 0), V3(-9, 0, -2), V3(-19, 2, 0), V3(-27, 3, 6), V3(-31, 1, 15), V3(-28, -1, 23)], 24)
    B.tube('tail', pts, np.linspace(2.6, .6, len(pts)), k=1.)
    rng = np.random.default_rng(71)
    for i in range(2, len(pts) - 1, 2):
        d = pts[i + 1] - pts[i - 1]; d /= np.linalg.norm(d)
        up = np.cross(d, V3(0, 1, 0)); up /= np.linalg.norm(up)
        if up[2] < 0: up = -up
        r = np.interp(i, [0, len(pts) - 1], [2.6, .6])
        for side in (-1, 1):
            o = (up + V3(0, side * .7, 0)); o /= np.linalg.norm(o)
            B.tube('tail', [pts[i] + o * r * .8, pts[i] + o * (r + rng.uniform(2, 3.6)) - d * 1.2], [.45, .04], tag='spine', k=.2)
    # the tuft of burning spines at its end (it flicks them)
    end = pts[-1]; d = pts[-1] - pts[-3]; d /= np.linalg.norm(d)
    for k in range(7):
        a = k / 7 * 2 * math.pi
        o = d + .45 * (np.cos(a) * V3(1, 0, 0) + np.sin(a) * V3(0, 1, 0)); o /= np.linalg.norm(o)
        B.tube('tail', [end - d * .5, end + o * 5.5], [.5, .05], tag='spine', k=.2)
    return B


def devil_look(body, ops, V, N, glow_tips=True):
    D = body.part_dists(V); tags = body.tag_mask(V, D)
    convex = np.clip(laplacian(ops, V, 1.) * 2., -1, 1)
    mott = smooth(-.5, .5, fbm(V / 7., 2, 81))
    col = lin(146, 52, 26)[None] * (1 - mott[:, None]) + lin(116, 36, 22)[None] * mott[:, None]
    belly = smooth(.3, .85, N[:, 0])
    col = col * (1 - .4 * belly[:, None]) + lin(170, 92, 54)[None] * (.4 * belly[:, None])
    back = smooth(0., -.8, N[:, 0])
    col = col * (1 - .4 * back[:, None]) + lin(60, 18, 12)[None] * (.4 * back[:, None])
    hi = smooth(.05, .6, convex)[:, None]; lo = smooth(-.05, -.6, convex)[:, None]
    col = col * (1 - .4 * hi) + lin(196, 110, 64)[None] * (.4 * hi)
    col = col * (1 - .45 * lo) + lin(30, 8, 6)[None] * (.45 * lo)
    sp = tags.get('spine', np.zeros(len(V)))
    col = col * (1 - sp[:, None]) + lin(36, 22, 16)[None] * sp[:, None]
    for t, c in (('claw', lin(18, 14, 12)), ('tooth', lin(190, 170, 130))):
        m = tags.get(t, np.zeros(len(V)))
        col = col * (1 - m[:, None]) + c[None] * m[:, None]
    ao = sdf_ao(V, N, ops, .4)
    col = col * (.3 + .7 * ao)[:, None]
    eye = np.zeros(len(V))
    if 'eye' in tags:
        eye = np.max([smooth(.85, .62, np.linalg.norm(V - e, axis=1)) for e in DEVIL_EYES], 0)
    col = col * (1 - eye[:, None]) + lin(255, 190, 60)[None] * eye[:, None]
    # (the spines' smouldering tips are added by spine_tip_glow)
    return col, eye.copy()


def spine_tip_glow(body, V):
    """Spine tips: thin parts of a spine (its tube's radius small where it is) glow."""
    sp = [op for _, t, op in body.parts if t == 'spine']
    if not sp: return np.zeros(len(V))
    lo, hi = V.min(0) - 1e4, V.max(0) + 1e4
    best = np.full(len(V), 1e4); rad = np.zeros(len(V))
    for op in sp:
        A, Bb = op.A[0], op.B[-1]
        d = op.eval(V, lo, hi)
        t = np.clip(((V - A) @ (Bb - A)) / max(np.dot(Bb - A, Bb - A), 1e-9), 0, 1)
        m = d < best; best = np.where(m, d, best); rad = np.where(m, t, rad)
    return smooth(.35, .8, rad) * smooth(.4, 0., best)


def build_devil():
    parts = {}
    body = devil_body()
    ob, V = field_object('SM_SpinedDevil_Body', body, .32, .55, lambda b, o, V, N: devil_look(b, o, V, N))
    a = spine_tip_glow(body, V); recolour_alpha(ob, a); parts['SM_SpinedDevil_Body'] = ob
    head = devil_head()
    ob, V = field_object('SM_SpinedDevil_Head', head, .2, .6, lambda b, o, V, N: devil_look(b, o, V, N))
    parts['SM_SpinedDevil_Head'] = ob
    for S, s in (('L', 1), ('R', -1)):
        wb, wrist, tips = devil_wing(s)
        ob, V = field_object(f'SM_SpinedDevil_Wing{S}_bones', wb, .2, .7, lambda b, o, V, N: devil_look(b, o, V, N, False))
        mem = []
        allp = [[wrist, V3(-4, s * 4, 6), V3(0, 0, 0)]] + [[wrist, (wrist + t) / 2 + V3(-1, 0, 1), t] for t in tips]
        for i in range(len(allp) - 1):
            mem.append(membrane(f'devwing{S}{i}', (allp[i], allp[i + 1]), 7, 10, .3, 1.6 if i else .6, .9 * s,
                                lambda W: (lin(70, 18, 12)[None] * np.ones((len(W), 1)) * (.8 + .3 * (fbm(W / 3., 2, 91) * .5 + .5))[:, None], np.zeros(len(W)))))
        parts[f'SM_SpinedDevil_Wing{S}'] = join([ob] + mem, f'SM_SpinedDevil_Wing{S}')
    tail = devil_tail()
    ob, V = field_object('SM_SpinedDevil_Tail', tail, .22, .6, lambda b, o, V, N: devil_look(b, o, V, N))
    recolour_alpha(ob, spine_tip_glow(tail, V)); parts['SM_SpinedDevil_Tail'] = ob
    # One thrown spine (along +X, its middle at the origin): a long barbed needle, hot at its end.
    sb = Body()
    sb.tube('spine', [V3(-9, 0, 0), V3(9, 0, 0)], [.75, .05], tag='spine', k=.2)
    for x in (2, 4.5):
        for a in range(3):
            o = V3(0, math.cos(a * 2.1), math.sin(a * 2.1))
            sb.tube('spine', [V3(x, 0, 0) + o * .3, V3(x - 1.8, 0, 0) + o * 1.3], [.25, .03], tag='spine', k=.1)
    ob, V = field_object('SM_DevilSpine', sb, .12, .5, lambda b, o, V, N: (lin(40, 24, 16)[None] * np.ones((len(V), 1)), smooth(-2, 6, V[:, 0])))
    parts['SM_DevilSpine'] = ob
    # Its perch: a basalt column 4.6 m tall, base at the origin (sunk into the lava or the ground), a rough top.
    pb = Body()
    def column(P):
        z = P[:, 2]; r = 30 + 14 * smooth(460, 0, z) + 3 * np.sin(z / 37.)
        return np.maximum(np.linalg.norm(P[:, :2], axis=1) - r, np.maximum(z - 460, -z))
    pb.add('perch', 'rock', Func('add', 0., (-50, -50, -2), (50, 50, 462), column))
    pb.disp(lambda P: 3.5 * fbm(P * np.array([1 / 18., 1 / 18., 1 / 40.]), 3, 101) + 1.2 * fbm(P / 5., 2, 103))
    ob, V = field_object('SM_DevilPerch', pb, 2.4, .4, lambda b, o, V, N: (np.tile([.15, .4, .0], (len(V), 1)) + np.array([0, 0, .5]) * smooth(.6, .9, N[:, 2])[:, None], np.zeros(len(V))))
    parts['SM_DevilPerch'] = ob
    if REVIEW: review_assembly(parts)
    for name, ob in parts.items():
        uvcm = unwrap(ob)
        add_material(ob, 'Skin' if name != 'SM_DevilPerch' else 'Rock')
        MANIFEST[name] = {'uv_cm': round(uvcm, 2)}
        if REVIEW: review(ob, name, static=True)
        export_static(ob, name)


# Where each part's pivot sits on the body (cm; Unreal's APhlegSpinedDevil uses the same).
DEVIL_PIVOTS = {'SM_SpinedDevil_Head': V3(4, 0, 33), 'SM_SpinedDevil_WingL': V3(-5, 5, 31), 'SM_SpinedDevil_WingR': V3(-5, -5, 31),
                'SM_SpinedDevil_Tail': V3(-8, 0, 9)}


def review_assembly(parts):
    import os
    from mathutils import Vector
    obs = [parts[n] for n in ('SM_SpinedDevil_Body', 'SM_SpinedDevil_Head', 'SM_SpinedDevil_WingL', 'SM_SpinedDevil_WingR', 'SM_SpinedDevil_Tail')]
    for n, p in DEVIL_PIVOTS.items(): parts[n].location = Vector(p)
    s = bpy.context.scene
    s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.light = 'STUDIO'; s.display.shading.color_type = 'VERTEX'
    for o in s.objects:
        if o.type == 'MESH': o.hide_render = o not in obs
    cam = bpy.data.objects.get('Review') or bpy.data.objects.new('Review', bpy.data.cameras.new('Review'))
    if cam.name not in s.collection.objects: s.collection.objects.link(cam)
    s.camera = cam; cam.data.lens = 50; s.render.resolution_x, s.render.resolution_y = 700, 700
    c = Vector((0, 0, 24))
    for tag, az in (('front', 30), ('side', 95), ('back', 210)):
        a = math.radians(az)
        cam.location = c + Vector((math.cos(a), math.sin(a), .3)).normalized() * 150
        cam.rotation_euler = (c - cam.location).to_track_quat('-Z', 'Y').to_euler()
        s.render.filepath = os.path.join(REVIEW, f'SpinedDevil_{tag}.png'); bpy.ops.render.render(write_still=True)
    for o in obs: o.location = (0, 0, 0)
    for o in s.objects: o.hide_render = False


def recolour_alpha(ob, a):
    attr = ob.data.color_attributes['Col']
    C = np.empty(len(ob.data.vertices) * 4, np.float32); attr.data.foreach_get('color', C); C = C.reshape(-1, 4)
    C[:, 3] = np.maximum(C[:, 3], a)
    attr.data.foreach_set('color', C.ravel())


# ------------------------------------------------------------------ review stills (workbench, vertex colour)
def review(ob, name, static=False):
    import os
    s = bpy.context.scene
    s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.light = 'STUDIO'; s.display.shading.color_type = 'VERTEX'
    cam = bpy.data.objects.get('Review') or bpy.data.objects.new('Review', bpy.data.cameras.new('Review'))
    if cam.name not in s.collection.objects: s.collection.objects.link(cam)
    for o in s.objects:
        if o.type == 'MESH': o.hide_render = o != ob
    Vr = verts_of(ob)
    lo, hi = Vr.min(0), Vr.max(0)
    c = (lo + hi) / 2; R = np.linalg.norm(hi - lo) * .5
    s.camera = cam; cam.data.lens = 50; s.render.resolution_x, s.render.resolution_y = 640, 800
    from mathutils import Vector
    for tag, az in (('front', 35), ('side', 100), ('back', 200)):
        a = math.radians(az)
        cam.location = Vector(c) + Vector((math.cos(a), math.sin(a), .25)).normalized() * R * 3.1
        d = Vector(c) - cam.location
        cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
        s.render.filepath = os.path.join(REVIEW, f'{name}_{tag}.png'); bpy.ops.render.render(write_still=True)


# ------------------------------------------------------------------ main
def build_skinned():
    rig = rig_build()
    if 'lemure' in ONLY:
        body = lemure_body()
        ob, V = field_object('SK_Lemure', body, .85, .5, lemure_look)
        add_weights(ob, body.weights(V))
        MANIFEST['SK_Lemure'] = {'uv_cm': round(unwrap(ob), 2)}
        add_material(ob, 'Skin')
        if REVIEW: review(ob, 'SK_Lemure')
        export_skinned(rig, ob, 'SK_Lemure')
    if 'fiend' in ONLY:
        body, wing = fiend_body()
        ob, V = field_object('SK_PitFiend', body, .7, .42, fiend_look)
        add_weights(ob, body.weights(V))
        add_bones(rig, fiend_bones(wing))
        wings = fiend_wings(wing)
        mb, head = fiend_mace()
        mace, _ = field_object('fiend_mace', mb, .55, .5, mace_look)
        add_weights(mace, {'hand_r': np.ones(len(mace.data.vertices))})
        ob = join([ob] + wings + [mace], 'SK_PitFiend')
        flame, cond = affine_weights(head, ['hand_r', 'upperarm_r', 'pelvis', 'ball_r'])
        K.log('mace head', head.round(1), flame, f'cond={cond:.0f}')
        MANIFEST['SK_PitFiend'] = {'uv_cm': round(unwrap(ob), 2), 'mace_head': flame}
        add_material(ob, 'Skin')
        if REVIEW: review(ob, 'SK_PitFiend')
        export_skinned(rig, ob, 'SK_PitFiend')


if __name__ == '__main__':
    if 'lemure' in ONLY or 'fiend' in ONLY: build_skinned()
    if 'devil' in ONLY:
        K.T0 = K.time.time()
        build_devil()
    old = json.loads((OUT / 'creatures.json').read_text()) if (OUT / 'creatures.json').exists() else {}
    old.update(MANIFEST)
    (OUT / 'creatures.json').write_text(json.dumps(old, indent=1) + '\n', encoding='utf-8')
    print('CHUCK_PHLEG_CREATURES_BUILT', sorted(MANIFEST))
