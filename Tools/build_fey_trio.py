"""Blender 4.5.14 + MPFB 2.0.17: the Crossroads trio (Claude, 2026-11-23; the user, with an image,
References/Feywild/crossroads-trio.png: "create these guys and have them mill around next to the giant tree in crossroads
... put a lot of work into making look good"): a centaur, a laughing satyr riding on his back, and a scowling old gnome on
the satyr's shoulders, pointing the way.

blender --background --python Tools/build_fey_trio.py [-- FeyCentaur FeySatyr FeyGnome] [--review <dir>]
  -> SourceAssets/NPCs/Fey/<Name>/SK_<Name>.fbx (+ GR_*.abc grooms), SourceAssets/NPCs/Fey/manifest.json

Each is a human from the kit (Tools/build_npc_humans.py; SourceAssets/NPCs/Fey/trio.json in its format, with MakeHuman's
expression units baked into the faces: the satyr's open laugh, the gnome's scowl and smirk, the centaur's set mouth), the
kit's strand hair and beards, its clothes (the gnome's shirt, jerkin, breeches and boots; belts; the centaur's helmet and
leather bracers), and what this script adds, in each one's own final frame (centimetres, +X forward, +Y his left, the
human's feet at z 0), with the temple heroes' surfaces and helpers (Tools/build_temple_heroes.py, imported):

  * FeyCentaur: the horse's body from his hips back (signed distance fields, Tools/sdf_kit.py; rebuilt 2026-11-24 as a
    horse is built: a trunk lofted over a riding horse's top line, under line and widths, the muscles over it, a horse's
    joints and slanted hooves, dark points below the knees and hocks, feathering at the fetlocks), on bones of its own
    under the kit's Root (h_front, h_rear; h_fl_/h_fr_/h_hl_/h_hr_ upper, lower, cannon, pastern; h_tail1-4), the human's
    pelvis re-parented to h_front. A coat of suede tinted bay, hooves dark; the tail and the fetlocks' feathering as hair
    cards (the beards' lock texture). Over his back a purple velvet blanket with a gilt border, a girth strap, saddlebags.
    His human legs go. The horse stands lower than the human's floor: its hooves are at z = -LIFT (the game raises him).
  * FeySatyr: goat's legs: the kit's leg bones moved to a goat's (the hock high and back, a long cannon down to a hoof),
    the human legs gone, a furred SDF leg on them under shaggy fur cards; ram's horns curling back from his brow (rigid
    on the head); a wreath of ivy and small flowers round his head; panpipes tucked into his sash (his green velvet
    waistcoat, red neckerchief and saffron sash are the kit's, trio.json).
  * FeyCentaur also wears the kit's dark leather vest with a rolled bronze edge (trio.json).
  * FeyGnome: a tall, soft, pointed green felt hat with a band, its tip flopping back (made in the kit's hook, so the
    strand hair lies under it); a pouch on his belt.

Clips: Tools/build_fey_trio_anims.py (one long loop for all three together).
"""
from pathlib import Path
import importlib.util
import json
import math
import random
import shutil
import sys
import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Tools'))
_s = importlib.util.spec_from_file_location('heroes', ROOT / 'Tools/build_temple_heroes.py')
HZ = importlib.util.module_from_spec(_s); _s.loader.exec_module(HZ)
kit = HZ.kit
import sdf_kit as K  # noqa: E402

OUT = ROOT / 'SourceAssets/NPCs/Fey'
kit.OUT, kit.TEX, kit.REVIEW = OUT, OUT / 'Textures', None
SPEC = json.loads((OUT / 'trio.json').read_text(encoding='utf-8'))['npcs']
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None
ONLY = [a for a in argv if a in SPEC]
LIFT = 58.          # how far the centaur's hooves are below the human's floor (cm): the game raises him by it
SATYR_SEAT = None   # filled by the anims script from the centaur's rest (kept here for reference)

# The expression units (MakeHuman, CC0) as targets 'expr:<unit>'.
import bl_ext.user_default.mpfb as _mpfb  # noqa: E402
EXPR = Path(_mpfb.__file__).parent / 'data/targets/expression/units/caucasian'
_full = kit.TargetService.target_full_path
def _expr_path(name):
    if name.startswith('expr:'):
        p = EXPR / f'{name[5:]}.target.gz'
        if not p.exists(): raise ValueError(f'No expression unit {name}')
        return str(p)
    return _full(name)
kit.TargetService.target_full_path = staticmethod(_expr_path)

HZ.PBR.update({
    'coat':      dict(set='scuba_suede', folder=HZ.SURF_REL, tint=(.4, .15, .05), rough=.75, metal=0., tile_cm=None),   # (a bay's coat, a little sheen)
    'coat_dark': dict(set='scuba_suede', folder=HZ.SURF_REL, tint=(.06, .035, .02), rough=1.0, metal=0., tile_cm=None),
    'hoof':      dict(set='fabric_leather_02', folder=HZ.SURF_REL, tint=(.028, .024, .02), rough=.6, metal=0., tile_cm=None),
    'horn':      dict(set=None, tint=(.38, .3, .2), rough=.5, metal=0., tile_cm=None),
    'blanket':   dict(set='velour_velvet', folder=HZ.SURF_REL, tint=(.13, .022, .16), rough=1.0, metal=0., tile_cm=None),
    'felt':      dict(set='poly_wool_herringbone', folder=HZ.SURF_REL, tint=(.05, .12, .03), rough=1.0, metal=0., tile_cm=None),
    'bronze':    dict(set='metal054a', folder=HZ.SURF_REL, tint=(.72, .48, .2), rough=.9, metal=1.0, tile_cm=40.),
    'leaf':      dict(set=None, tint=(.05, .15, .035), rough=.55, metal=0., tile_cm=None),
    'petal':     dict(set=None, tint=(.82, .5, .62), rough=.6, metal=0., tile_cm=None),
    'bloom':     dict(set=None, tint=(.85, .62, .1), rough=.6, metal=0., tile_cm=None),
    'reed':      dict(set=None, tint=(.46, .36, .17), rough=.55, metal=0., tile_cm=None),
})


def V(*a): return Vector(a)


# ------------------------------------------------------------------ signed distance bodies (as Tools/build_phleg_creatures.py)
class Body:
    """Primitives owned by a bone (for the weights) and tagged (for the slot), in centimetres."""
    def __init__(self): self.parts = []
    def add(self, bone, tag, op): self.parts.append((bone, tag, op)); return op
    def tube(self, bone, pts, rad, tag='coat', k=2.): return self.add(bone, tag, K.Tube('add', k, [np.array(p, float) for p in pts], list(rad)))
    def ell(self, bone, c, r, tag='coat', k=2., axis=None):
        return self.add(bone, tag, K.Ellipsoids('add', k, [(np.array(c, float), np.array(r, float), None if axis is None else K.frame(np.array(axis, float)))]))
    def ball(self, bone, c, r, tag='coat', k=1.): return self.add(bone, tag, K.Balls('add', k, [np.array(c, float)], [r]))
    def ops(self): return [op for _, _, op in self.parts]
    def dists(self, P):
        lo, hi = P.min(0) - 1e4, P.max(0) + 1e4
        return np.stack([op.eval(P, lo, hi) for _, _, op in self.parts])
    def weights(self, P, s=3.):
        D = self.dists(P); dmin = D.min(0)
        own = np.array([b is not None for b, _, _ in self.parts])
        D = np.where(own[:, None], D, 1e4); dmin = D.min(0)
        E = np.exp(-np.clip(D - dmin, 0, 40) / s) * (D - dmin < 18)
        W = {}
        for (bone, _, _), e in zip(self.parts, E):
            if bone: W[bone] = W.get(bone, 0) + e
        names = list(W); M = np.stack([W[n] for n in names]); order = np.argsort(-M, 0); keep = np.zeros_like(M, bool)
        for k in range(min(4, len(names))): keep[order[k], np.arange(M.shape[1])] = True
        M = np.where(keep, M, 0); M /= np.maximum(M.sum(0), 1e-9)
        return {n: M[i] for i, n in enumerate(names)}
    def tags(self, P):
        D = self.dists(P); own = np.array([b is not None for b, _, _ in self.parts])
        idx = np.argmin(np.where(own[:, None], D, 1e4), 0)
        return [self.parts[i][1] for i in idx]


def sdf_object(name, body, h=1.6, ratio=.5):
    V_, Q, _ = K.field_mesh(name, body.ops(), h)
    ob = K.make_mesh(name, V_, Q)
    if ratio < 1:
        HZ.kit.activate(ob); m = ob.modifiers.new('Dec', 'DECIMATE'); m.ratio = ratio; bpy.ops.object.modifier_apply(modifier=m.name)
    for p in ob.data.polygons: p.use_smooth = True
    return ob


# ------------------------------------------------------------------ the frame
class Final:
    """The kit's final frame for this character (cm, +X forward, +Y left, feet at 0), and the rig in it."""
    def __init__(self, rig):
        self.rig = rig
    def j(self, b, tail=False):
        bone = self.rig.data.bones[b]; return (self.rig.matrix_world @ (bone.tail_local if tail else bone.head_local)) * 100.


def m_of(p): return Vector(p) * .01


def put_mesh(ob, rig, W=None, rigid=None):
    """Vertices from cm to the rig's metres, skinned (weights W per vertex, or one bone/blend for all)."""
    for v in ob.data.vertices: v.co = v.co * .01
    ob.data.update()
    for g in list(ob.vertex_groups): ob.vertex_groups.remove(g)
    if rigid:
        for b, w in rigid.items():
            g = ob.vertex_groups.new(name=b); g.add(list(range(len(ob.data.vertices))), w, 'REPLACE')
    elif W:
        for b, w in W.items():
            idx = np.nonzero(w > 1e-3)[0]
            if not len(idx): continue
            g = ob.vertex_groups.get(b) or ob.vertex_groups.new(name=b)
            for i in idx: g.add([int(i)], float(w[i]), 'REPLACE')
    ob.parent = rig
    if not any(m.type == 'ARMATURE' for m in ob.modifiers): ob.modifiers.new('Armature', 'ARMATURE').object = rig
    return ob


def new_bone(rig, name, parent, head, tail, roll_up=V(0, 0, 1)):
    """head/tail in the final frame (cm)."""
    HZ.kit.activate(rig); bpy.ops.object.mode_set(mode='EDIT')
    eb = rig.data.edit_bones.new(name); eb.head = m_of(head); eb.tail = m_of(tail)
    eb.align_roll(roll_up); eb.parent = rig.data.edit_bones[parent] if parent else None; eb.use_deform = True
    bpy.ops.object.mode_set(mode='OBJECT')


def set_material(ob, slot):
    ob.data.materials.clear(); ob.data.materials.append(kit.slot_material(slot))


def split_by_tag(base, tags, groups):
    """One SDF object into one object per slot group: groups {slot: [tags]}."""
    out = []
    for slot, keep in groups.items():
        ob = base.copy(); ob.data = base.data.copy(); bpy.context.collection.objects.link(ob)
        bm = bmesh.new(); bm.from_mesh(ob.data); bm.verts.ensure_lookup_table()
        drop = [f for f in bm.faces if not all(tags[v.index] in keep for v in f.verts) and not (any(tags[v.index] in keep for v in f.verts) and sum(tags[v.index] in keep for v in f.verts) >= 2)]
        bmesh.ops.delete(bm, geom=drop, context='FACES')
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
        bm.to_mesh(ob.data); bm.free()
        if len(ob.data.polygons): set_material(ob, slot); ob.name = f'{base.name}_{slot}'; out.append(ob)
        else: bpy.data.objects.remove(ob)
    bpy.data.objects.remove(base)
    return out


def cards(name, roots, rng, slot, length, width, droop, jitter=.3):
    """Hair cards: roots [(point cm, normal, along)], each a tapering lock down `along` and out a little, curving."""
    bm = bmesh.new(); uvs = []; LOCKS = kit.LOCKS
    for p, n, along in roots:
        L = length * rng.uniform(.7, 1.15); w = width * rng.uniform(.8, 1.2)
        d = (along + n * rng.uniform(.1, .45) + V(rng.uniform(-jitter, jitter), rng.uniform(-jitter, jitter), rng.uniform(-jitter, jitter) * .5)).normalized()
        side = d.cross(n).normalized() if d.cross(n).length > 1e-3 else d.cross(V(0, 0, 1)).normalized()
        k = rng.randrange(LOCKS); u0, u1 = k / LOCKS + .002, (k + 1) / LOCKS - .002
        pts = [p - n * .4]
        for i in range(1, 5):
            t = i / 4; pts.append(pts[-1] + (d + V(0, 0, -droop * t)).normalized() * (L / 4))
        col = [(bm.verts.new(q - side * w / 2 * (1 - .5 * i / 4)), bm.verts.new(q + side * w / 2 * (1 - .5 * i / 4)), 1 - i / 4) for i, q in enumerate(pts)]
        for a, b in zip(col, col[1:]):
            f = bm.faces.new((a[0], a[1], b[1], b[0])); uvs.append((f, ((u0, a[2]), (u1, a[2]), (u1, b[2]), (u0, b[2]))))
    me = bpy.data.meshes.new(name); ob = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(ob)
    uv = bm.loops.layers.uv.new('UVMap')
    for f, co in uvs:
        for loop, c in zip(f.loops, co): loop[uv].uv = c
    bm.to_mesh(me); bm.free(); set_material(ob, slot)
    return ob


def surface_points(ob_cm, count, rng, where):
    """Points over an object's faces (cm), with normals, where(point, normal) says yes."""
    me = ob_cm.data; me.calc_loop_triangles(); tris = me.loop_triangles
    areas = np.array([t.area for t in tris]); cum = np.cumsum(areas); out = []; tries = 0
    while len(out) < count and tries < count * 20:
        tries += 1
        t = tris[int(np.searchsorted(cum, rng.uniform(0, cum[-1])))]
        a, b, c = (me.vertices[i].co for i in t.vertices); r1, r2 = rng.random(), rng.random()
        if r1 + r2 > 1: r1, r2 = 1 - r1, 1 - r2
        p = a + (b - a) * r1 + (c - a) * r2; n = Vector(t.normal)
        if where(p, n): out.append((p.copy(), n.copy()))
    return out


def delete_legs(rig, meshes, keep_above):
    """The human's legs go (the horse's or the goat's replace them): faces whose corners are all weighted mostly to the
    leg bones and below keep_above (cm)."""
    legs = {f'{b}_{s}' for b in ('thigh', 'calf', 'foot', 'ball') for s in 'lr'}
    bones = {b.name for b in rig.data.bones}   # (MakeHuman's body has groups that are not bones: 'body', 'Left', ...)
    for ob in meshes:
        if ob.type != 'MESH' or not ob.vertex_groups: continue
        names = {g.index: g.name for g in ob.vertex_groups}
        Mw = ob.matrix_world; flag = []
        for v in ob.data.vertices:   # (the mesh's own weights: bmesh's deform layer came back empty here)
            tot = sum(g.weight for g in v.groups if names.get(g.group) in bones) or 1.; lw = sum(g.weight for g in v.groups if names.get(g.group) in legs)
            flag.append(lw / tot > .5 and (Mw @ v.co).z * 100. < keep_above)
        bm = bmesh.new(); bm.from_mesh(ob.data); bm.verts.ensure_lookup_table()
        drop = [f for f in bm.faces if all(flag[v.index] for v in f.verts)]
        print('CHUCK_TRIO_LEGS', ob.name, len(drop))
        if drop:
            bmesh.ops.delete(bm, geom=drop, context='FACES')
            bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
            bm.to_mesh(ob.data)
        bm.free()
    for ob in [o for o in meshes if o.type == 'MESH' and not len(o.data.polygons)]:
        meshes.remove(ob); bpy.data.objects.remove(ob)


# ------------------------------------------------------------------ the centaur's horse half
def hermite(xs, ys):
    """A smooth curve through (xs ascending, ys): cubic Hermite with finite-difference tangents (numpy in, numpy out)."""
    xs, ys = np.asarray(xs, float), np.asarray(ys, float); m = np.gradient(ys, xs)
    def f(x):
        x = np.clip(x, xs[0], xs[-1]); i = np.clip(np.searchsorted(xs, x) - 1, 0, len(xs) - 2)
        h = xs[i + 1] - xs[i]; t = (x - xs[i]) / h; t2, t3 = t * t, t * t * t
        return (2 * t3 - 3 * t2 + 1) * ys[i] + (t3 - 2 * t2 + t) * h * m[i] + (-2 * t3 + 3 * t2) * ys[i + 1] + (t3 - t2) * h * m[i + 1]
    return f


# The trunk's lines, from a light riding horse of 1.5 m at the withers (cm: x along him, the human's hips at 0; heights
# above the horse's floor; half widths): the top line (the human's hips, withers, a slight dip of the back, loin, croup,
# the tail's root, the buttock), the under line (breast, sternum between the forelegs, girth, belly, the flank rising to
# the stifle, under the buttock) and the width (chest, the ribs' spring, hips, quarters).
TRUNK = [  # x,   top,  under, half width
    (24., 127., 106., 7.),
    (20., 138., 99., 15.),
    (12., 145., 91., 20.5),
    (0., 148., 86., 23.5),
    (-20., 148., 82., 26.5),
    (-40., 145., 80., 29.5),
    (-60., 142., 79., 30.5),
    (-80., 142., 80.5, 30.5),
    (-100., 144., 85., 29.5),
    (-115., 146., 92., 28.5),
    (-130., 147., 100., 27.5),
    (-145., 145., 104., 25.5),
    (-160., 141., 107., 22.5),
    (-170., 137., 111., 17.5),
    (-176., 131., 118., 8.),
]


# Each hind thigh lofted down its own height (cm above the floor): its front and back edges, its outer and inner sides
# (his left; mirrored). The broad, flat-sided wedge of a horse's thigh from the croup down past the stifle (its front edge)
# to the gaskin and the hock.
THIGH = [  # h,  front x, back x, outer y, inner y
    (56., -153., -166., 16., 10.5),
    (63., -148., -167., 17., 10.),
    (72., -138., -166., 19.5, 9.),
    (82., -126., -168., 23.5, 8.),
    (94., -116., -172., 27., 6.5),
    (108., -116., -177., 27.5, 4.5),
    (122., -121., -178., 26., 3.),
    (134., -128., -174., 22., 2.),
    (141., -134., -166., 15., 2.),
]


def vloft(table, zf, side, p=2.6):
    """A field for a shape lofted up the height (rows of `table`), its section a rounded box (superellipse, power p)."""
    hs = [r[0] for r in table]; xf, xb, yo, yi = (hermite(hs, [r[k] for r in table]) for k in (1, 2, 3, 4))
    h_lo, h_hi = hs[0], hs[-1]
    def f(P):
        x, y, h = P[:, 0], P[:, 1] * side, P[:, 2] - zf
        a, b, o, i = xf(h), xb(h), yo(h), yi(h)
        rx, ry = np.maximum((a - b) * .5, .5), np.maximum((o - i) * .5, .5)
        q = (np.abs((x - (a + b) * .5) / rx) ** p + np.abs((y - (o + i) * .5) / ry) ** p) ** (1. / p)
        d = (q - 1.) * np.minimum(rx, ry)
        d = np.where(h > h_hi, np.maximum(d, h - h_hi), d)
        return np.where(h < h_lo, np.maximum(d, h_lo - h), d)
    return f


def centaur(rig, meshes, slots, rng):
    """A horse's body from the human's hips back, built as a horse is: a lofted trunk on its true lines, with the
    muscles over it (shoulder and its point, triceps, the two breast muscles parted by their groove, forearm, haunch,
    point of hip, stifle, gaskin, hamstring), the joints a horse has (elbow, flat-fronted knee, the hock's point, round
    fetlock with its ergot) and slanted hooves; dark points (bay) below the knees and hocks."""
    F = Final(rig); P0 = F.j('pelvis')
    zf = P0.z + 4. - 150.                                          # the horse's floor (below the human's)
    global LIFT; LIFT = -zf
    def Z(h): return zf + h
    def H(x, y, h): return V(x, y, Z(h))
    # the joints (cm, his left side; mirrored)
    FORE = dict(shoulder=(15., 17., 108.), elbow=(-6., 16., 86.), knee=(-1., 15., 50.), fet=(1., 15., 18.), toe=(8., 15., .5))
    HIND = dict(hip=(-140., 20., 120.), stifle=(-118., 25.5, 96.), hock=(-158., 13.5, 56.), fet=(-156., 13., 18.), toe=(-149., 13., .5))
    def J(d, k, s): x, y, h = d[k]; return H(x, y * s, h)
    new_bone(rig, 'h_front', 'Root', H(-60, 0, 128), H(6, 0, 144))
    new_bone(rig, 'h_rear', 'h_front', H(-60, 0, 128), H(-150, 0, 132))
    HZ.kit.activate(rig); bpy.ops.object.mode_set(mode='EDIT')
    rig.data.edit_bones['pelvis'].parent = rig.data.edit_bones['h_front']; rig.data.edit_bones['pelvis'].use_connect = False
    for s in ('l', 'r'):
        for b in ('thigh', 'calf', 'foot', 'ball'): rig.data.edit_bones[f'{b}_{s}'].use_deform = False
    bpy.ops.object.mode_set(mode='OBJECT')
    legs = {}
    for side, s in (('l', 1.), ('r', -1.)):
        fl = [J(FORE, k, s) for k in ('shoulder', 'elbow', 'knee', 'fet', 'toe')]
        hl = [J(HIND, k, s) for k in ('hip', 'stifle', 'hock', 'fet', 'toe')]
        legs[f'f{side}'] = fl; legs[f'h{side}'] = hl
        for kind, pts, parent in (('f', fl, 'h_front'), ('h', hl, 'h_rear')):
            names = [f'h_{kind}{side}_upper', f'h_{kind}{side}_lower', f'h_{kind}{side}_cannon', f'h_{kind}{side}_pastern']
            par = parent
            for i, nm in enumerate(names):
                new_bone(rig, nm, par, pts[i], pts[i + 1], V(1, 0, 0)); par = nm
    tail = [H(-171, 0, 134), H(-183, 0, 128), H(-190, 0, 112), H(-194, 0, 92), H(-196, 0, 70)]
    par = 'h_rear'
    for i in range(4): new_bone(rig, f'h_tail{i + 1}', par, tail[i], tail[i + 1], V(0, 0, 1)); par = f'h_tail{i + 1}'

    # --- the trunk: a loft over the lines above, its section an egg (narrower over the back than at the ribs' spring)
    xs = [r[0] for r in TRUNK][::-1]
    top_f, und_f, wid_f = (hermite(xs, [r[k] for r in TRUNK][::-1]) for k in (1, 2, 3))
    x_lo, x_hi = xs[0], xs[-1]
    def trunk(P):
        x, y, z = P[:, 0], P[:, 1], P[:, 2] - zf
        t, b, w = top_f(x), und_f(x), wid_f(x)
        c, hh = (t + b) * .5, np.maximum((t - b) * .5, .5)
        v = (z - c) / hh
        we = np.maximum(w * (1. - .13 * np.clip(v, -1, 1) - .05 * np.clip(v, -1, 1) ** 2), .5)
        q = np.sqrt((y / we) ** 2 + v ** 2)
        d = (q - 1.) * np.minimum(we, hh)
        d = np.where(x > x_hi, np.maximum(d, x - x_hi), d)
        return np.where(x < x_lo, np.maximum(d, x_lo - x), d)
    B = Body()
    box = lambda x0, x1: ((x0, -40., zf + 70.), (x1, 40., zf + 160.))
    for bone, (x0, x1) in (('h_front', (-95., 30.)), ('h_rear', (-185., -55.))):
        lo, hi = box(x0, x1)
        def part(P, x0=x0, x1=x1):   # (the same trunk, each half owned by its bone; they overlap from -95 to -55;
            # outside its half a part fades, so the weights blend over ~10 cm: the surface is the overlap's, unchanged)
            return trunk(P) + .6 * np.maximum(0., np.maximum(x0 - P[:, 0], P[:, 0] - x1))
        B.add(bone, 'coat', K.Func('add', 0., lo, hi, part))
    # The human's lower belly and back into the horse's front: an ellipsoid round the cut skin (so its edge is inside).
    skin = [o for o in meshes if o.data.materials and o.data.materials[0].name == 'Skin']
    cut = P0.z - 4.
    ring = [(o.matrix_world @ v.co) * 100. for o in skin for v in o.data.vertices]
    ring = [p for p in ring if cut - 1. < p.z < cut + 16. and abs(p.y) < 24.]
    belt = [(o.matrix_world @ v.co) * 100. for o in meshes if o.name.startswith('belt') for v in o.data.vertices]
    top = min((p.z for p in belt if abs(p.y) < 24.), default=cut + 6.) + 1.5   # (up to just under his belt)
    ring = [p for p in ring if p.z < top]
    if ring:
        rx0, rx1 = min(p.x for p in ring), max(p.x for p in ring); ry = max(abs(p.y) for p in ring)
        zc, rz = (cut - 10. + top) * .5, (top - cut + 10.) * .5
        B.ell('h_front', ((rx0 + rx1) * .5 - 1., 0, zc), ((rx1 - rx0) * .5 + 1.8, ry + 1.8, rz), k=6)
        print('CHUCK_TRIO_JOIN', f'x={rx0:.0f}..{rx1:.0f} y={ry:.0f} cut={cut:.0f} top={top:.0f}')
    for side, s in (('l', 1.), ('r', -1.)):
        f, h = legs[f'f{side}'], legs[f'h{side}']
        sh, el, kn, fe, to = f
        hp, st, ho, hf, ht = h
        def ax(a, b): return np.array(b - a, float)
        # the forehand: shoulder blade and its muscle, its point, the triceps over the elbow, the breast muscle; the
        # forearm growing out of them, muscled in front at the top; a flat-fronted knee (subtle: horses are smooth)
        B.ell('h_front', H(-2, 18. * s, 126), (7., 12.5, 24), k=6., axis=ax(H(-20, 0, 147), H(16, 0, 110)))
        B.ball('h_front', H(15, 14.5 * s, 107), 7., k=6.)
        B.ell('h_front', H(-4, 18. * s, 96), (13., 9., 15.), k=6.)
        B.ell('h_front', H(12, 8.5 * s, 102), (9.5, 8.5, 12.5), k=7.)
        B.ball(f'h_f{side}_upper', el + V(-5, 0, 2), 5.5, k=4.)                                                  # elbow
        B.tube(f'h_f{side}_lower', [el + V(2, 0, 10), el + V(1, 0, -6), kn + V(0, 0, 4)], [8., 7.2, 4.8], k=5.)
        B.ell(f'h_f{side}_lower', H(2, 15.5 * s, 75), (6.2, 5.6, 12.5), k=4.)                                    # forearm's extensor
        B.ell(f'h_f{side}_lower', kn + V(.5, 0, 0), (5.4, 4.9, 6.8), k=2.5)                                      # the knee
        B.tube(f'h_f{side}_cannon', [kn + V(.5, 0, -4), fe + V(.5, 0, 4)], [3.4, 3.2], tag='points', k=1.6)       # cannon bone
        B.tube(f'h_f{side}_cannon', [kn + V(-3.0, 0, -6), fe + V(-3.3, 0, 3)], [2.3, 2.5], tag='points', k=1.8)   # the tendons behind
        B.ball(f'h_f{side}_cannon', fe, 4.6, tag='points', k=2.)                                                  # fetlock
        B.ball(f'h_f{side}_cannon', fe + V(-4.3, 0, -1.5), 2.6, tag='points', k=1.8)                              # ergot
        # the hindquarter: each thigh lofted (THIGH), owned in overlapping bands by the croup, the thigh bone and the
        # gaskin's; a soft point of hip
        th = vloft(THIGH, zf, s)
        for bone, (h0, h1) in (('h_rear', (100., 200.)), (f'h_h{side}_upper', (80., 112.)), (f'h_h{side}_lower', (0., 92.))):
            def band(P, h0=h0, h1=h1, th=th):   # (fading outside its band, as the trunk's halves)
                hh = P[:, 2] - zf; return th(P) + .6 * np.maximum(0., np.maximum(h0 - hh, hh - h1))
            B.add(bone, 'coat', K.Func('add', 3. if bone == 'h_rear' else 0., (-190., -35., zf + 50.), (-105., 35., zf + 150.), band))
        B.ell(f'h_h{side}_lower', ho + V(.5, 0, -2), (6.0, 5.0, 7.5), k=2.5)                                      # hock
        B.ball(f'h_h{side}_lower', ho + V(-5, 0, 3), 2.8, k=2.5)                                                  # its point
        B.tube(f'h_h{side}_cannon', [ho + V(1, 0, -5), hf + V(.5, 0, 4)], [3.6, 3.3], tag='points', k=1.6)
        B.tube(f'h_h{side}_cannon', [ho + V(-3.2, 0, -7), hf + V(-3.4, 0, 3)], [2.4, 2.5], tag='points', k=1.8)
        B.ball(f'h_h{side}_cannon', hf, 4.7, tag='points', k=2.)
        B.ball(f'h_h{side}_cannon', hf + V(-4.3, 0, -1.5), 2.6, tag='points', k=1.8)
        for leg, kind in ((f, 'f'), (h, 'h')):
            fet, toe = leg[3], leg[4]
            coronet = toe + V(-2.5, 0, 8.)
            B.tube(f'h_{kind}{side}_pastern', [fet + V(0, 0, -1), coronet + V(0, 0, 1.5)], [4.3, 4.2], tag='points', k=1.4)
            B.tube(f'h_{kind}{side}_pastern', [coronet + V(-.5, 0, .5), toe + V(.5, 0, 2.)], [5.0, 6.6], tag='hoof', k=.5)   # hoof
    # the soles flat on the floor
    B.add(None, 'hoof', K.Func('sub', 0., (-220., -45., zf - 30.), (60., 45., zf + .5), lambda P: P[:, 2] - zf))
    # the groove between the breast muscles
    B.add('h_front', 'coat', K.Ellipsoids('sub', 2.5, [(np.array(H(24, 0, 103), float), np.array((5., 1.8, 11.)), None)]))
    B.tube('h_tail1', [tail[0] + V(3, 0, 1), tail[1] + V(-1, 0, -1)], [5.2, 4.], k=4)                       # the tail's dock
    base = sdf_object('Horse', B, 1.15, .45)
    P = np.array([v.co[:] for v in base.data.vertices])
    tags = B.tags(P)
    parts = split_by_tag(base, tags, {'Hero_coat': {'coat'}, 'Hero_hoof': {'hoof'}, 'Hero_coat_dark': {'points'}})
    for kind in ('coat', 'hoof', 'coat_dark'): HZ.pbr_slot(slots, kind)
    made = []
    for ob in parts:
        Q = np.array([v.co[:] for v in ob.data.vertices]); made.append(put_mesh(ob, rig, B.weights(Q)))
    # Hair: the tail (long, from the dock down), the feathering over the hooves.
    hair = []
    tail_roots = []
    for i in range(1300):
        t = rng.uniform(0, 1); a = rng.uniform(0, 2 * math.pi); c = tail[0].lerp(tail[1], t)
        n = V(0, math.cos(a), math.sin(a)); tail_roots.append((c + n * 4, n, (tail[2] - tail[0] + V(-8, 0, -40)).normalized()))
    tob = cards('Tail', tail_roots, rng, 'HorseHair', 92, 2.6, .9, .12); hair.append(('tail', tob))
    feather = []
    for leg in legs.values():
        for i in range(44):
            a = rng.uniform(-math.pi * .9, math.pi * .9) + math.pi; n = V(math.cos(a), math.sin(a), 0)   # (mostly behind the fetlock)
            feather.append((leg[3] + n * 4.8 + V(0, 0, rng.uniform(-4, 9)), n, V(0, 0, -1)))
    fob = cards('Feather', feather, rng, 'HorseHair', 15, 2.4, .25, .25); hair.append(('feather', fob))
    slots['HorseHair'] = {'type': 'hair', 'texture': 'Textures/beard_locks.png', 'tint': [.03, .02, .014]}
    for kind, ob in hair:
        Q = np.array([v.co[:] for v in ob.data.vertices])
        if kind == 'tail':
            Wq = {}
            for i in range(4):
                a, b = np.array(tail[i]), np.array(tail[i + 1]); d = b - a
                t = np.clip(((Q - a) @ d) / (d @ d), 0, 1); dist = np.linalg.norm(Q - (a + t[:, None] * d), axis=1)
                Wq[f'h_tail{i + 1}'] = np.exp(-dist / 12.)
            S = sum(Wq.values()); Wq = {k: v / np.maximum(S, 1e-9) for k, v in Wq.items()}
            made.append(put_mesh(ob, rig, Wq))
        else:
            made.append(put_mesh(ob, rig, B.weights(Q)))
    # The blanket over his back: a cloth draped on the coat from behind the withers to the loin, falling to mid-flank,
    # with a gilt border; a girth strap behind the elbows; saddlebags at the croup's front.
    from mathutils.bvhtree import BVHTree
    coat_ob = next(o for o in made if o.data.materials and o.data.materials[0].name == 'Hero_coat')
    bm = bmesh.new(); bm.from_mesh(coat_ob.data); bm.transform(Matrix.Scale(100., 4)); tree = BVHTree.FromBMesh(bm); bm.free()
    def drape(x0, x1, drop, lift, nu=34, nv=24):
        rows = []
        for i in range(nu + 1):
            x = x0 + (x1 - x0) * i / nu; row = []
            for j in range(nv + 1):
                s = -1 + 2 * j / nv; a = s * math.radians(104)
                c0 = H(x, 0, 112); d = V(0, math.sin(a), math.cos(a))   # (from outside, inward to the body's middle)
                o = c0 + d * 120.; hit = tree.ray_cast(o, -d, 200.)
                p = hit[0] if hit[0] is not None else c0 + d * 30
                hang = max(0., abs(s) - .55) / .45
                row.append(p + V(0, 0, lift) + V(0, math.copysign(1, s), 0) * (1.2 + hang * 3.) + V(0, 0, -drop * hang * hang))
            rows.append(row)
        return rows
    rows = drape(-30, -112, 12., 1.6)
    bm = bmesh.new(); vs = [[bm.verts.new(p) for p in row] for row in rows]
    for i in range(len(vs) - 1):
        for j in range(len(vs[0]) - 1): bm.faces.new((vs[i][j], vs[i][j + 1], vs[i + 1][j + 1], vs[i + 1][j]))
    me = bpy.data.meshes.new('Blanket'); bm.to_mesh(me); bm.free(); blanket = bpy.data.objects.new('Blanket', me); bpy.context.collection.objects.link(blanket)
    HZ.kit.activate(blanket); m = blanket.modifiers.new('Sol', 'SOLIDIFY'); m.thickness = .9; m.offset = 1.; bpy.ops.object.modifier_apply(modifier=m.name)
    set_material(blanket, HZ.pbr_slot(slots, 'blanket').name)
    border = []
    for row in (rows[0], rows[-1]): border.append([p + V(0, 0, .9) for p in row])
    border.append([rows[i][0] + V(0, -.9, 0) for i in range(len(rows))]); border.append([rows[i][-1] + V(0, .9, 0) for i in range(len(rows))])
    bm = bmesh.new()
    for line in border:
        for i in range(len(line) - 1):
            a, b = line[i], line[i + 1]; d = (b - a).normalized(); n1 = d.cross(V(0, 0, 1)).normalized() if abs(d.z) < .9 else V(1, 0, 0); n2 = d.cross(n1)
            ring_a = [bm.verts.new(a + (n1 * math.cos(t) + n2 * math.sin(t)) * 1.1) for t in [k * math.pi / 3 for k in range(6)]]
            ring_b = [bm.verts.new(b + (n1 * math.cos(t) + n2 * math.sin(t)) * 1.1) for t in [k * math.pi / 3 for k in range(6)]]
            for k in range(6): bm.faces.new((ring_a[k], ring_a[(k + 1) % 6], ring_b[(k + 1) % 6], ring_b[k]))
    me = bpy.data.meshes.new('BlanketTrim'); bm.to_mesh(me); bm.free(); trim = bpy.data.objects.new('BlanketTrim', me); bpy.context.collection.objects.link(trim)
    set_material(trim, HZ.pbr_slot(slots, 'gilt').name)
    bm = bmesh.new(); prev = None
    for k in range(41):
        a = 2 * math.pi * k / 40; d = V(0, math.sin(a), math.cos(a))
        hit = tree.ray_cast(H(-24, 0, 112) + d * 60, -d, 100.)
        p = (hit[0] if hit[0] is not None else H(-24, 0, 112) + d * 30) + d * .8
        cur = [bm.verts.new(p + V(4, 0, 0)), bm.verts.new(p + V(-4, 0, 0))]
        if prev: bm.faces.new((prev[0], prev[1], cur[1], cur[0]))
        prev = cur
    me = bpy.data.meshes.new('Girth'); bm.to_mesh(me); bm.free(); girth = bpy.data.objects.new('Girth', me); bpy.context.collection.objects.link(girth)
    HZ.kit.activate(girth); m = girth.modifiers.new('Sol', 'SOLIDIFY'); m.thickness = .6; bpy.ops.object.modifier_apply(modifier=m.name)
    set_material(girth, HZ.pbr_slot(slots, 'leather').name)
    bags = []
    for y in (1., -1.):
        bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.)
        for v in bm.verts: v.co = V(v.co.x * 30, v.co.y * 11, v.co.z * 26)
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=3., segments=3, affect='EDGES')
        hit = tree.ray_cast(H(-118, 70 * y, 124), V(0, -y, 0), 200.)
        c = (hit[0] if hit[0] is not None else H(-118, 30 * y, 124)) + V(0, 6.5 * y, -6)
        bmesh.ops.translate(bm, verts=bm.verts, vec=c)
        me = bpy.data.meshes.new('Bag'); bm.to_mesh(me); bm.free(); ob = bpy.data.objects.new('Bag', me); bpy.context.collection.objects.link(ob)
        set_material(ob, HZ.pbr_slot(slots, 'leather').name); bags.append(ob)
    for ob in [blanket, trim, girth] + bags:
        Q = np.array([v.co[:] for v in ob.data.vertices])
        made.append(put_mesh(ob, rig, {k: v for k, v in B.weights(Q).items() if k in ('h_front', 'h_rear')} or None))
    hoofs = [o for o in made if o.data.materials and o.data.materials[0].name == 'Hero_hoof']
    LIFT = -min(v.co.z for o in hoofs for v in o.data.vertices) * 100. - .5
    print('CHUCK_TRIO_LIFT', round(LIFT, 1), 'floor', round(-zf, 1))
    for ob in made:
        if not ob.vertex_groups:   # (a fallback: everything on the back moves with the body)
            g = ob.vertex_groups.new(name='h_rear'); g.add(list(range(len(ob.data.vertices))), 1., 'REPLACE')
    return made


# ------------------------------------------------------------------ the satyr's goat legs and horns
def satyr(rig, meshes, slots, rng):
    F = Final(rig)
    # The legs' bones to a goat's: the knee forward, the hock high and back, a long cannon down to the hoof.
    goat = {}
    for s in 'lr':
        hip, knee = F.j(f'thigh_{s}'), F.j(f'calf_{s}')
        knee = V(hip.x + 10, knee.y, hip.z * .6)              # (forward, and higher than a man's)
        hock = V(knee.x - 17, knee.y, hip.z * .32)             # (high and back)
        fet = V(hock.x + 6, hock.y, 10.)
        hoof = V(fet.x + 8, fet.y, 0.)
        goat[s] = (hip, knee, hock, fet, hoof)
        HZ.kit.activate(rig); bpy.ops.object.mode_set(mode='EDIT'); eb = rig.data.edit_bones
        eb[f'thigh_{s}'].tail = m_of(knee); eb[f'calf_{s}'].head = m_of(knee); eb[f'calf_{s}'].tail = m_of(hock)
        eb[f'foot_{s}'].head = m_of(hock); eb[f'foot_{s}'].tail = m_of(fet); eb[f'ball_{s}'].head = m_of(fet); eb[f'ball_{s}'].tail = m_of(hoof)
        for b in ('thigh', 'calf', 'foot', 'ball'): eb[f'{b}_{s}'].align_roll(Vector((1, 0, 0)))
        bpy.ops.object.mode_set(mode='OBJECT')
    waist = F.j('spine_01').z
    delete_legs(rig, meshes, waist - 2.)
    B = Body()
    for s, y in (('l', 1.), ('r', -1.)):
        hip, knee, hock, fet, hoof = goat[s]
        B.ell(f'thigh_{s}', (hip + knee) * .5 + V(1, 1.5 * y, 2), (12, 11, (hip - knee).length * .62), k=5, axis=None)
        B.tube(f'thigh_{s}', [hip + V(0, 0, 8), knee], [13, 8.5], k=4)
        B.tube(f'calf_{s}', [knee, hock], [8, 5], k=3)
        B.ell(f'calf_{s}', (knee * .65 + hock * .35) + V(-2, 0, 0), (7, 6, 12), k=3)
        B.ball(f'calf_{s}', hock + V(-2, 0, 0), 4.6, k=2)
        B.tube(f'foot_{s}', [hock, fet], [3.8, 3.2], k=2)
        B.tube(f'ball_{s}', [fet, hoof + V(-2, 0, 5)], [3.3, 3.5], k=1.5)
        B.tube(f'ball_{s}', [hoof + V(-3, 0, 5.5), hoof + V(1, 0, .4)], [4.2, 5.6], tag='hoof', k=.6)   # cloven hoof
    B.ell('pelvis', F.j('pelvis') + V(-1, 0, -4), (11, 14.5, 10), k=5)       # the furred seat
    base = sdf_object('GoatLegs', B, 1.0, .5)
    P = np.array([v.co[:] for v in base.data.vertices])
    tags = B.tags(P)
    dark = HZ.PBR['coat_dark']; HZ.PBR['coat_dark'] = dict(dark, tint=(.17, .095, .045))   # (his legs a warm goat brown)
    for kind in ('coat_dark', 'hoof'): HZ.pbr_slot(slots, kind)
    HZ.PBR['coat_dark'] = dark
    parts = split_by_tag(base, tags, {'Hero_coat_dark': {'coat'}, 'Hero_hoof': {'hoof'}})
    made = []
    for ob in parts:
        Q = np.array([v.co[:] for v in ob.data.vertices]); made.append(put_mesh(ob, rig, B.weights(Q)))
    # Shaggy fur over the legs: long on the thighs and haunches, shorter below the hock, none on the hooves.
    fur_src = next(o for o in parts)
    roots = []
    for s in 'lr':
        hip, knee, hock, fet, hoof = goat[s]
        for i in range(900):
            t = rng.random() ** .8; a = rng.uniform(0, 2 * math.pi)
            if t < .55: c = hip.lerp(knee, t / .55); r = 11.5 - 3 * t
            else: c = knee.lerp(hock, (t - .55) / .45); r = 7 - 2 * (t - .55)
            n = V(math.cos(a), math.sin(a), 0); roots.append((c + n * r, n, V(-.25, 0, -1).normalized()))
    pv = F.j('pelvis')
    for i in range(500):
        a = rng.uniform(0, 2 * math.pi); n = V(math.cos(a), math.sin(a) * 1.1, -.2).normalized()
        roots.append((pv + V(-2, 0, -6) + V(n.x * 14, n.y * 16, n.z * 12 + rng.uniform(-4, 6)), n, V(0, 0, -1)))
    fur = cards('Fur', roots, rng, 'GoatFur', 13, 3.0, .35, .3)
    slots['GoatFur'] = {'type': 'hair', 'texture': 'Textures/beard_locks.png', 'tint': [.2, .12, .06]}
    Q = np.array([v.co[:] for v in fur.data.vertices]); made.append(put_mesh(fur, rig, B.weights(Q)))
    # Ram's horns (2026-11-24, as the reference's: low, not rising over his head, where the gnome sits): from above the
    # temples up a little and back, then down behind the ears and forward round under them, spiralling outward, ridged.
    head_h, head_t = F.j('head'), F.j('head', True)
    bm = bmesh.new()
    for y in (1., -1.):
        root = head_h.lerp(head_t, .8) + V(3.4, 4.6 * y, 0)
        centre = root + V(-4.2, 0, -5.)
        a0 = math.atan2(5., 4.2)
        pts = []
        for i in range(40):
            t = i / 39; a = a0 + t * math.radians(310); r = 6.5 * (1 - .42 * t)
            pts.append(V(centre.x + r * math.cos(a), root.y + y * (1.2 + 5.2 * t), centre.z + r * math.sin(a)))
        prev = None
        for i, p in enumerate(pts):
            t = i / 39; d = (pts[min(i + 1, 39)] - pts[max(i - 1, 0)]).normalized(); n1 = d.cross(V(0, 1, 0)).normalized(); n2 = d.cross(n1)
            rad = 2.5 * (1 - .78 * t) + .3
            ridge = 1 + .09 * math.sin(i * 2.6)
            ring = [bm.verts.new(p + (n1 * math.cos(k * math.pi / 5) + n2 * math.sin(k * math.pi / 5)) * rad * ridge * (1.12 if k in (0, 1, 9) else 1.)) for k in range(10)]
            if prev:
                for k in range(10): bm.faces.new((prev[k], prev[(k + 1) % 10], ring[(k + 1) % 10], ring[k]))
            prev = ring
        bm.faces.new(prev)
    me = bpy.data.meshes.new('Horns'); bm.to_mesh(me); bm.free(); horns = bpy.data.objects.new('Horns', me); bpy.context.collection.objects.link(horns)
    for p in horns.data.polygons: p.use_smooth = True
    set_material(horns, HZ.pbr_slot(slots, 'horn').name); made.append(put_mesh(horns, rig, rigid={'head': 1.}))
    # (His chest strap went 2026-11-24: he wears a waistcoat now, trio.json.) The skin, for what sits on it:
    from mathutils.bvhtree import BVHTree
    body = next(o for o in meshes if o.data.materials and o.data.materials[0].name == 'Skin')
    bb = bmesh.new(); bb.from_mesh(body.data); bb.transform(Matrix.Scale(100., 4)); btree = BVHTree.FromBMesh(bb); bb.free()
    made += satyr_wreath(F, body, slots, rng)
    made += satyr_pipes(F, rig, slots, btree)
    return made


def leaf_mesh(bm, at, n, along, size, curl=.25):
    """An ivy leaf: a pointed, lobed blade (a fan of triangles) at `at` (cm), facing n, its tip along `along`."""
    side = along.cross(n).normalized(); along = n.cross(side).normalized()
    c = bm.verts.new(at); ring = []
    for k in range(11):
        a = math.pi * k / 10   # round from one side to the other, through the tip
        r = size * (.55 + .45 * math.sin(a) ** 3 + .18 * math.sin(3 * a) ** 2)
        p = at + side * math.cos(a) * r * .8 + along * (math.sin(a) * r + size * .15) + n * curl * size * math.sin(a) * .5
        ring.append(bm.verts.new(p))
    for k in range(10): bm.faces.new((c, ring[k], ring[k + 1]))


def satyr_wreath(F, body, slots, rng):
    """A wreath of ivy round his head at the brow, over the horns' roots, with small flowers in it (rigid on the head)."""
    me = body.data; Mw = body.matrix_world
    head_h, head_t = F.j('head'), F.j('head', True)
    eyes = [o for o in body.parent.children if o.data.materials and o.data.materials[0].name == 'Eye'] if body.parent else []
    ez = [(o.matrix_world @ v.co).z * 100. for o in eyes for v in o.data.vertices]
    z0 = (sum(ez) / len(ez) if ez else (head_h.z + (head_t.z - head_h.z) * .58)) + 6.   # (round the brow, over the horns' roots)
    names = {g.index: g.name for g in body.vertex_groups}
    pts = []
    for v in me.vertices:
        p = (Mw @ v.co) * 100.
        if abs(p.z - z0) < 1.2 and any(names.get(g.group) == 'head' and g.weight > .5 for g in v.groups): pts.append(p)
    cx = sum(p.x for p in pts) / len(pts); cy = sum(p.y for p in pts) / len(pts)
    bins = [0.] * 36
    for p in pts:
        a = math.atan2(p.y - cy, p.x - cx); k = int((a + math.pi) / (2 * math.pi) * 36) % 36
        bins[k] = max(bins[k], math.hypot(p.x - cx, p.y - cy))
    for k in range(36):
        if not bins[k]: bins[k] = max(bins)
    lb, pb, bb = bmesh.new(), bmesh.new(), bmesh.new()
    for i in range(150):
        a = rng.uniform(-math.pi, math.pi); k = int((a + math.pi) / (2 * math.pi) * 36) % 36
        r = bins[k] + 1.6 + rng.uniform(0, 1.2)
        tilt = .3 * math.cos(a)   # (lower at the brow, higher behind)
        at = V(cx + math.cos(a) * r, cy + math.sin(a) * r, z0 - 2. + rng.uniform(-1.2, 1.4) - tilt * 6.)
        out = V(math.cos(a), math.sin(a), rng.uniform(.0, .5)).normalized()
        tang = V(-math.sin(a), math.cos(a), 0) * rng.choice((-1, 1)) + V(0, 0, rng.uniform(-.4, .5))
        leaf_mesh(lb, at, out, tang.normalized(), rng.uniform(1.4, 2.3))
    for i in range(14):
        a = 2 * math.pi * (i + rng.uniform(-.3, .3)) / 14; k = int((a % (2 * math.pi)) / (2 * math.pi) * 36) % 36
        k = int(((a + math.pi) % (2 * math.pi)) / (2 * math.pi) * 36) % 36
        aa = -math.pi + (k + .5) * 2 * math.pi / 36
        r = bins[k] + 3.2
        at = V(cx + math.cos(aa) * r, cy + math.sin(aa) * r, z0 - 1.5 - .3 * math.cos(aa) * 6.)
        out = V(math.cos(aa), math.sin(aa), .35).normalized(); side = out.cross(V(0, 0, 1)).normalized(); up = side.cross(out)
        centre = bb.verts.new(at + out * .5)
        for j in range(5):   # five petals
            b = 2 * math.pi * j / 5
            d = side * math.cos(b) + up * math.sin(b); e = side * math.cos(b + .5) + up * math.sin(b + .5); f = side * math.cos(b - .5) + up * math.sin(b - .5)
            c0 = pb.verts.new(at); pb.faces.new((c0, pb.verts.new(at + f * .9 + d * .4), pb.verts.new(at + d * 1.6 + out * .2), pb.verts.new(at + e * .9 + d * .4)))
        ring = [bb.verts.new(at + out * .3 + (side * math.cos(2 * math.pi * j / 6) + up * math.sin(2 * math.pi * j / 6)) * .45) for j in range(6)]
        for j in range(6): bb.faces.new((centre, ring[j], ring[(j + 1) % 6]))
    out = []
    for nm, bmx, kind in (('Wreath', lb, 'leaf'), ('Petals', pb, 'petal'), ('Blooms', bb, 'bloom')):
        mesh = bpy.data.meshes.new(nm); bmx.to_mesh(mesh); bmx.free(); ob = bpy.data.objects.new(nm, mesh); bpy.context.collection.objects.link(ob)
        HZ.kit.activate(ob); m = ob.modifiers.new('Sol', 'SOLIDIFY'); m.thickness = .12; m.offset = 0.; bpy.ops.object.modifier_apply(modifier=m.name)
        for p in ob.data.polygons: p.use_smooth = True
        set_material(ob, HZ.pbr_slot(slots, kind).name); out.append(put_mesh(ob, F.rig, rigid={'head': 1.}))
    print('CHUCK_TRIO_WREATH', f'z={z0:.0f}', f'r={min(bins):.1f}..{max(bins):.1f}')
    return out


def satyr_pipes(F, rig, slots, btree):
    """Panpipes: seven reeds of falling length bound side by side, tucked upright into his sash at the front of his belly,
    on his left, leaning out a little (rigid, pelvis and spine)."""
    pv = F.j('pelvis'); waist = F.j('spine_01').z
    y = pv.y + 8.5; z = waist - 1.
    hit = btree.ray_cast(V(pv.x + 60., y, z), V(-1, 0, 0), 100.)
    front = (hit[0].x if hit[0] is not None else pv.x + 12.) + 2.4
    base = V(front, y, z - 4.)
    bm = bmesh.new()
    for i in range(7):
        L = 16. - i * 1.6
        bmesh.ops.create_cone(bm, cap_ends=True, segments=8, radius1=.6, radius2=.6, depth=L,
                              matrix=Matrix.Translation(V(0, i * 1.25, L * .5)))
    for z_ in (2.5, 6.):   # the binding
        bmesh.ops.create_cube(bm, size=1., matrix=Matrix.Translation(V(0, 3.75, z_)) @ Matrix.Diagonal((1.7, 9.8, .9, 1.)))
    me = bpy.data.meshes.new('Pipes'); bm.to_mesh(me); bm.free(); ob = bpy.data.objects.new('Pipes', me); bpy.context.collection.objects.link(ob)
    R = Matrix.Rotation(math.radians(-18), 3, 'X') @ Matrix.Rotation(math.radians(10), 3, 'Y')   # (leaning out to his left, a little forward)
    for v in ob.data.vertices: v.co = base + R @ v.co
    for p in ob.data.polygons: p.use_smooth = True
    set_material(ob, HZ.pbr_slot(slots, 'reed').name)
    print('CHUCK_TRIO_PIPES', tuple(round(c) for c in base))
    return [put_mesh(ob, rig, rigid={'pelvis': .5, 'spine_01': .5})]


# ------------------------------------------------------------------ the gnome's hat (in the kit's hook: its frame)
def gnome_hat(info, slots, hide):
    """A tall soft felt hat, its brim a rolled band over the brow, its cone rising and flopping back. Kit frame (metres,
    facing -Y, his left +X), rigid on the head."""
    rig = info.rig; HZ.HERO['rig'] = rig
    head_h = rig.matrix_world @ rig.data.bones['head'].head_local; head_t = rig.matrix_world @ rig.data.bones['head'].tail_local
    nose, chin, mid = kit.face_marks(info)
    top_z = max(c.z for i, c in enumerate(info.co) if info.bone[i] == 'head')
    hz = top_z - .088   # the band sits low on the brow, down to the ears
    cx, cy = 0., (head_h.y + head_t.y) * .5 + .004
    r0x, r0y = .116, .13
    bm = bmesh.new(); rings = []
    path = []
    for i in range(26):
        t = i / 25
        c = V(cx, cy + .02 * t + .28 * t ** 2.4, hz + .5 * t - .14 * t ** 3)
        path.append(c)
    for i, c in enumerate(path):
        t = i / 25; rx = r0x * (1 - t) ** 1.05 + .004; ry = r0y * (1 - t) ** 1.05 + .004
        ring = [bm.verts.new(c + V(rx * math.cos(a) * (1 + .03 * math.sin(a * 3 + t * 6)), ry * math.sin(a), 0)) for a in [2 * math.pi * k / 24 for k in range(24)]]
        if rings:
            for k in range(24): bm.faces.new((rings[-1][k], rings[-1][(k + 1) % 24], ring[(k + 1) % 24], ring[k]))
        rings.append(ring)
    bm.faces.new(rings[-1])
    me = bpy.data.meshes.new('hat'); bm.to_mesh(me); bm.free(); hat = bpy.data.objects.new('hat', me); bpy.context.collection.objects.link(hat)
    HZ.kit.activate(hat); m = hat.modifiers.new('Sol', 'SOLIDIFY'); m.thickness = .006; m.offset = -1.; bpy.ops.object.modifier_apply(modifier=m.name)
    for p in hat.data.polygons: p.use_smooth = True
    hat.data.materials.append(HZ.pbr_slot(slots, 'felt'))
    # the band
    bm = bmesh.new(); rings = []
    for z, s in ((-.012, 1.035), (0., 1.06), (.022, 1.06), (.03, 1.03)):
        ring = [bm.verts.new(V(cx + r0x * s * math.cos(a), cy + r0y * s * math.sin(a), hz + z)) for a in [2 * math.pi * k / 24 for k in range(24)]]
        if rings:
            for k in range(24): bm.faces.new((rings[-1][k], rings[-1][(k + 1) % 24], ring[(k + 1) % 24], ring[k]))
        rings.append(ring)
    me = bpy.data.meshes.new('hatband'); bm.to_mesh(me); bm.free(); band = bpy.data.objects.new('hatband', me); bpy.context.collection.objects.link(band)
    band.data.materials.append(HZ.pbr_slot(slots, 'leather_dk'))
    for o in (hat, band): HZ.rigid(o, {'head': 1.})
    # the scalp under it is never seen
    for i, c in enumerate(info.co):
        if info.bone[i] == 'head' and c.z > hz + .01: hide[i] = True
    parts = [hat, band]
    HZ.finish(parts)
    return parts


# ------------------------------------------------------------------ build each
def export(name, rig, meshes):
    out = OUT / name; out.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    for o in [rig] + meshes: o.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(out / f'SK_{name}.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'}, **kit.FBX)


def build(name, spec):
    rng = random.Random(hash(name) & 0xffff)
    HZ.WRAPPED.clear(); HZ.HERO.clear()
    extra = gnome_hat if name == 'FeyGnome' else None
    result = kit.build(name, spec, extra=extra)
    rig = bpy.data.objects['HumanRig']; HZ.HERO['rig'] = rig
    meshes = [o for o in bpy.data.objects if o.type == 'MESH' and o.parent == rig]
    slots = result['slots']
    if name == 'FeyCentaur':
        delete_legs(rig, meshes, 200.)
        cut = Final(rig).j('pelvis').z - 4.   # (and the skin below his belt: the horse's withers are there)
        for ob in [o for o in meshes if o.data.materials and o.data.materials[0].name == 'Skin']:
            bm = bmesh.new(); bm.from_mesh(ob.data)
            bmesh.ops.delete(bm, geom=[f for f in bm.faces if all(v.co.z * 100. < cut for v in f.verts)], context='FACES')
            bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS'); bm.to_mesh(ob.data); bm.free()
        made = centaur(rig, meshes, slots, rng); result['lift_cm'] = round(LIFT, 1)
    elif name == 'FeySatyr':
        made = satyr(rig, meshes, slots, rng)
    else:
        made = []
    if made:
        pbr = [o for o in made if o.data.materials and o.data.materials[0].name.startswith('Hero_')]
        HZ.finish(pbr)
        for o in made:
            if not o.data.uv_layers: kit.box_uv(o, 50.)
        HZ.bake_colours(pbr)
        meshes += made
    # the beards' lock texture for the hair cards
    src = ROOT / 'SourceAssets/NPCs/Humans/Textures/beard_locks.png'; (OUT / 'Textures').mkdir(parents=True, exist_ok=True)
    if not (OUT / 'Textures/beard_locks.png').exists(): shutil.copy2(src, OUT / 'Textures/beard_locks.png')
    export(name, rig, meshes)
    result['bones'] = [b.name for b in rig.data.bones]
    result['tris'] = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in meshes)
    lo = min((o.matrix_world @ v.co).z for o in meshes for v in o.data.vertices) * 100
    hi = max((o.matrix_world @ v.co).z for o in meshes for v in o.data.vertices) * 100
    print('CHUCK_FEY_TRIO', name, f"tris={result['tris']}", f'z={lo:.0f}..{hi:.0f}', sorted(slots))
    if REVIEW: review(name, rig, meshes, slots)
    return result


def review(name, rig, meshes, slots):
    import os
    s = bpy.context.scene
    for slot, info in slots.items():
        m = bpy.data.materials.get(slot)
        if not m: continue
        t = info.get('tint', (.6, .5, .45)); k = .55 if info.get('type') in ('card', 'hair') else 1.
        if info.get('type') == 'skin': t = (.62, .44, .34)
        if info.get('type') == 'eye': t = (.1, .08, .06)
        m.diffuse_color = (min(1, t[0] * k * 1.6), min(1, t[1] * k * 1.6), min(1, t[2] * k * 1.6), 1)
    s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.color_type = 'MATERIAL'; s.display.shading.light = 'STUDIO'; s.display.shading.show_cavity = True
    lo = min((o.matrix_world @ v.co).z for o in meshes for v in o.data.vertices); hi = max((o.matrix_world @ v.co).z for o in meshes for v in o.data.vertices)
    mid = (lo + hi) / 2; h = hi - lo
    views = [('front', V(1, -.3, .05), mid, 40), ('side', V(0, -1, .05), mid, 40), ('three', V(-.7, -.7, .25), mid, 40), ('head', V(1, -.35, 0), hi - h * .08, 120)]
    if name == 'FeyCentaur': views += [('hside', V(0, -1, .08), lo + .8, 70), ('hback', V(-.75, -.6, .2), lo + .8, 70), ('hfront', V(.8, -.5, .12), lo + .8, 70)]
    for tag, d, aim_z, lens in views:
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam)
        target = V((-.78 if tag[0] == 'h' and tag != 'head' else -.4) if name == 'FeyCentaur' else 0., 0, aim_z)
        cam.location = target + d.normalized() * h * 2.6; cam.rotation_euler = (target - cam.location).to_track_quat('-Z', 'Y').to_euler(); cam.data.lens = lens; s.camera = cam
        s.render.resolution_x, s.render.resolution_y = 900, 1000
        s.render.filepath = os.path.join(REVIEW, f'{name}_{tag}.png'); bpy.ops.render.render(write_still=True)


if __name__ == '__main__':
    manifest_path = OUT / 'manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else {'npcs': {}}
    manifest['generator'] = 'Tools/build_fey_trio.py'; manifest['mpfb'] = '2.0.17'
    for name, spec in SPEC.items():
        if ONLY and name not in ONLY: continue
        manifest['npcs'][name] = build(name, spec)
    manifest_path.write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
    print('CHUCK_FEY_TRIO_READY', sorted(manifest['npcs']))
