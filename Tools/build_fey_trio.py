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

  * FeyCentaur: the horse's body from his hips back (signed distance fields, Tools/sdf_kit.py: chest, barrel, croup,
    four legs with knees, hocks, fetlocks and hooves, feathering at the fetlocks, the tail's dock), on bones of its own
    under the kit's Root (h_front, h_rear; h_fl_/h_fr_/h_hl_/h_hr_ upper, lower, cannon, pastern; h_tail1-4), the human's
    pelvis re-parented to h_front. A coat of suede tinted bay, hooves dark; the tail and the fetlocks' feathering as hair
    cards (the beards' lock texture). Over his back a purple velvet blanket with a gilt border, a girth strap, saddlebags.
    His human legs go. The horse stands lower than the human's floor: its hooves are at z = -LIFT (the game raises him).
  * FeySatyr: goat's legs: the kit's leg bones moved to a goat's (the hock high and back, a long cannon down to a hoof),
    the human legs gone, a furred SDF leg on them under shaggy fur cards; ram's horns curling back from his brow (rigid
    on the head); a leather strap across his chest from the belt.
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
    'coat':      dict(set='scuba_suede', folder=HZ.SURF_REL, tint=(.34, .17, .075), rough=1.0, metal=0., tile_cm=None),
    'coat_dark': dict(set='scuba_suede', folder=HZ.SURF_REL, tint=(.06, .035, .02), rough=1.0, metal=0., tile_cm=None),
    'hoof':      dict(set='fabric_leather_02', folder=HZ.SURF_REL, tint=(.028, .024, .02), rough=.6, metal=0., tile_cm=None),
    'horn':      dict(set=None, tint=(.38, .3, .2), rough=.5, metal=0., tile_cm=None),
    'blanket':   dict(set='velour_velvet', folder=HZ.SURF_REL, tint=(.13, .022, .16), rough=1.0, metal=0., tile_cm=None),
    'felt':      dict(set='poly_wool_herringbone', folder=HZ.SURF_REL, tint=(.05, .12, .03), rough=1.0, metal=0., tile_cm=None),
    'bronze':    dict(set='metal054a', folder=HZ.SURF_REL, tint=(.72, .48, .2), rough=.9, metal=1.0, tile_cm=40.),
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
        E = np.exp(-np.clip(D - dmin, 0, 40) / s) * (D - dmin < 18)
        W = {}
        for (bone, _, _), e in zip(self.parts, E): W[bone] = W.get(bone, 0) + e
        names = list(W); M = np.stack([W[n] for n in names]); order = np.argsort(-M, 0); keep = np.zeros_like(M, bool)
        for k in range(min(4, len(names))): keep[order[k], np.arange(M.shape[1])] = True
        M = np.where(keep, M, 0); M /= np.maximum(M.sum(0), 1e-9)
        return {n: M[i] for i, n in enumerate(names)}
    def tags(self, P):
        D = self.dists(P); idx = np.argmin(D, 0)
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
def centaur(rig, meshes, slots, rng):
    F = Final(rig); P0 = F.j('pelvis')
    # The horse is drawn below in design units (a withers of 118 at x 0 by the human's hips) and stretched to a horse's
    # size: heights x SZ from its own floor, lengths x SX (at the end, every vertex and bone behind the hips).
    SZ, SX = 1.27, 1.16
    zf = P0.z + 4. - 118. * SZ                                     # the horse's floor (below the human's)
    global LIFT; LIFT = -zf
    def Z(h): return zf + h * SZ
    # bones (cm, final frame)
    new_bone(rig, 'h_front', 'Root', V(-58, 0, Z(108)), V(6, 0, Z(118)))
    new_bone(rig, 'h_rear', 'h_front', V(-58, 0, Z(108)), V(-132, 0, Z(112)))
    HZ.kit.activate(rig); bpy.ops.object.mode_set(mode='EDIT')
    rig.data.edit_bones['pelvis'].parent = rig.data.edit_bones['h_front']; rig.data.edit_bones['pelvis'].use_connect = False
    for s in ('l', 'r'):
        for b in ('thigh', 'calf', 'foot', 'ball'): rig.data.edit_bones[f'{b}_{s}'].use_deform = False
    bpy.ops.object.mode_set(mode='OBJECT')
    legs = {}
    for side, y in (('l', 14.), ('r', -14.)):
        fl = [V(-4, y, Z(112)), V(-3, y * 1.05, Z(78)), V(1, y * .95, Z(42)), V(2, y * .95, Z(17)), V(7, y * .95, Z(0))]
        hl = [V(-122, y * 1.1, Z(108)), V(-104, y * 1.2, Z(72)), V(-131, y * 1.05, Z(44)), V(-127, y, Z(17)), V(-122, y, Z(0))]
        legs[f'f{side}'] = fl; legs[f'h{side}'] = hl
        for kind, pts, parent in (('f', fl, 'h_front'), ('h', hl, 'h_rear')):
            names = [f'h_{kind}{side}_upper', f'h_{kind}{side}_lower', f'h_{kind}{side}_cannon', f'h_{kind}{side}_pastern']
            par = parent
            for i, nm in enumerate(names):
                new_bone(rig, nm, par, pts[i], pts[i + 1], V(1, 0, 0)); par = nm
    tail = [V(-138, 0, Z(124)), V(-152, 0, Z(118)), V(-160, 0, Z(100)), V(-165, 0, Z(78)), V(-167, 0, Z(55))]
    par = 'h_rear'
    for i in range(4): new_bone(rig, f'h_tail{i + 1}', par, tail[i], tail[i + 1], V(0, 0, 1)); par = f'h_tail{i + 1}'
    # the body
    B = Body()
    B.ell('h_front', (-6, 0, Z(98)), (29, 25, 31), k=6)                   # chest
    B.ell('h_front', (-30, 0, Z(95)), (34, 30, 31), k=7)                   # barrel, front
    B.ell('h_rear', (-80, 0, Z(95)), (44, 31, 31), k=7)                    # barrel, back
    B.ell('h_rear', (-118, 0, Z(104)), (30, 27, 26), k=7)                  # croup
    B.ell('h_rear', (-124, 0, Z(118)), (22, 22, 12), k=6)                  # the rump's top
    B.ell('h_front', (-50, 0, Z(80)), (44, 22, 14), k=8)                   # belly
    B.ell('h_front', (-2, 0, Z(119)), (16, 17, 13), k=6)                   # the human's hips into the withers
    for side, y in (('l', 1.), ('r', -1.)):
        B.ell('h_front', (-8, 15 * y, Z(96)), (18, 10, 24), k=5)           # shoulder muscle
        B.ell(f'h_f{side}_upper', (-4, 14 * y, Z(84)), (11, 9, 16), k=5)   # forearm's top
        B.ell('h_rear', (-110, 18 * y, Z(94)), (24, 11, 24), k=6)          # haunch
        f, h = legs[f'f{side}'], legs[f'h{side}']
        B.tube(f'h_f{side}_lower', [f[1] + V(0, 0, 6), f[2]], [11.5, 6.4], k=3)
        B.ball(f'h_f{side}_lower', f[2], 6.2, k=2)                          # knee
        B.tube(f'h_f{side}_cannon', [f[2], f[3]], [5.4, 5.], k=2)
        B.ball(f'h_f{side}_cannon', f[3], 5.6, k=1.5)                       # fetlock
        B.tube(f'h_h{side}_upper', [h[0], h[1] + V(4, 0, 4)], [18, 11], k=5)  # stifle
        B.ell(f'h_h{side}_lower', (h[1] + h[2]) * .5 + V(-2, 0, 4), (10, 8.5, 19), k=4, axis=None)   # gaskin
        B.tube(f'h_h{side}_lower', [h[1], h[2]], [9.5, 6.], k=3)
        B.ball(f'h_h{side}_lower', h[2] + V(-2, 0, 0), 5.6, k=2)             # hock
        B.tube(f'h_h{side}_cannon', [h[2], h[3]], [5.4, 5.], k=2)
        B.ball(f'h_h{side}_cannon', h[3], 5.6, k=1.5)
        for leg, kind in ((f, 'f'), (h, 'h')):
            B.tube(f'h_{kind}{side}_pastern', [leg[3], leg[4] + V(0, 0, 8)], [4.4, 4.6], k=1.5)
            B.tube(f'h_{kind}{side}_pastern', [leg[4] + V(0, 0, 8.5), leg[4] + V(1, 0, .3)], [5.4, 7.4], tag='hoof', k=.8)   # hoof
            for i in range(5):   # feathering at the fetlock: a darker coat (the cards over it are the hair)
                a = 2 * math.pi * i / 5; B.ball(f'h_{kind}{side}_cannon', leg[3] + V(2 * math.cos(a) - 1, 2 * math.sin(a), -4), 4.2, tag='feather', k=1.2)
    B.tube('h_tail1', [tail[0], tail[1] + V(-2, 0, -2)], [8, 4.5], k=3)      # the tail's dock
    base = sdf_object('Horse', B, 1.4, .45)
    P = np.array([v.co[:] for v in base.data.vertices])
    tags = B.tags(P); Wt = B.weights(P)
    parts = split_by_tag(base, tags, {'Hero_coat': {'coat'}, 'Hero_hoof': {'hoof'}, 'Hero_coat_dark': {'feather'}})
    for kind in ('coat', 'hoof', 'coat_dark'): HZ.pbr_slot(slots, kind)
    made = []
    for ob in parts:
        Q = np.array([v.co[:] for v in ob.data.vertices]); Wq = B.weights(Q)
        made.append(put_mesh(ob, rig, Wq))
    # Hair: the tail (long, from the dock down), the feathering over the hooves; the mane's line where the human meets
    # the horse (a short brush at the waist, under the belt).
    hair = []
    tail_roots = []
    for i in range(1100):
        t = rng.uniform(0, 1); a = rng.uniform(0, 2 * math.pi); c = tail[0].lerp(tail[1], t)
        n = V(0, math.cos(a), math.sin(a)); tail_roots.append((c + n * 4, n, (tail[2] - tail[0] + V(-6, 0, -40)).normalized()))
    tob = cards('Tail', tail_roots, rng, 'HorseHair', 88, 2.6, .9, .12); hair.append(('tail', tob))
    feather = []
    for leg in legs.values():
        for i in range(36):
            a = rng.uniform(0, 2 * math.pi); n = V(math.cos(a), math.sin(a), 0)
            feather.append((leg[3] + n * 5.2 + V(0, 0, rng.uniform(-5, 6)), n, V(0, 0, -1)))
    fob = cards('Feather', feather, rng, 'HorseHair', 16, 2.4, .2, .25); hair.append(('feather', fob))
    slots['HorseHair'] = {'type': 'hair', 'texture': 'Textures/beard_locks.png', 'tint': [.05, .032, .02]}
    for kind, ob in hair:
        Q = np.array([v.co[:] for v in ob.data.vertices])
        if kind == 'tail':
            Wq = {}
            for i in range(4):
                a, b = np.array(tail[i]), np.array(tail[i + 1]); d = b - a
                t = np.clip(((Q - a) @ d) / (d @ d), 0, 1); dist = np.linalg.norm(Q - (a + t[:, None] * d), axis=1)
                Wq[f'h_tail{i + 1}'] = np.exp(-dist / 12.)
            # past the dock the hair hangs from the last bones
            S = sum(Wq.values()); Wq = {k: v / np.maximum(S, 1e-9) for k, v in Wq.items()}
            made.append(put_mesh(ob, rig, Wq))
        else:
            made.append(put_mesh(ob, rig, B.weights(Q)))
    # The blanket over his back: a cloth draped on the coat from behind the withers to the croup's front, falling to
    # mid-flank, with a gilt border; a girth strap; saddlebags behind it.
    from mathutils.bvhtree import BVHTree
    coat_ob = next(o for o in made if o.data.materials and o.data.materials[0].name == 'Hero_coat')
    bm = bmesh.new(); bm.from_mesh(coat_ob.data); bm.transform(Matrix.Scale(100., 4)); tree = BVHTree.FromBMesh(bm); bm.free()
    def drape(x0, x1, half, drop, lift, nu=30, nv=22):
        rows = []
        for i in range(nu + 1):
            x = x0 + (x1 - x0) * i / nu; row = []
            for j in range(nv + 1):
                s = -1 + 2 * j / nv; a = s * math.radians(100)
                c0 = V(x, 0, Z(96)); d = V(0, math.sin(a), math.cos(a))   # (from outside, inward to the body's middle)
                o = c0 + d * 120.; hit = tree.ray_cast(o, -d, 200.)
                p = hit[0] if hit[0] is not None else c0 + d * 30
                hang = max(0., abs(s) - .55) / .45
                row.append(p + V(0, 0, lift) + V(0, math.copysign(1, s), 0) * (1.2 + hang * 3.) + V(0, 0, -drop * hang * hang))
            rows.append(row)
        return rows
    rows = drape(-28, -98, 1., 14., 1.6)
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
        prev = None
        for i in range(len(line) - 1):
            a, b = line[i], line[i + 1]; d = (b - a).normalized(); n1 = d.cross(V(0, 0, 1)).normalized() if abs(d.z) < .9 else V(1, 0, 0); n2 = d.cross(n1)
            ring_a = [bm.verts.new(a + (n1 * math.cos(t) + n2 * math.sin(t)) * 1.1) for t in [k * math.pi / 3 for k in range(6)]]
            ring_b = [bm.verts.new(b + (n1 * math.cos(t) + n2 * math.sin(t)) * 1.1) for t in [k * math.pi / 3 for k in range(6)]]
            for k in range(6): bm.faces.new((ring_a[k], ring_a[(k + 1) % 6], ring_b[(k + 1) % 6], ring_b[k]))
    me = bpy.data.meshes.new('BlanketTrim'); bm.to_mesh(me); bm.free(); trim = bpy.data.objects.new('BlanketTrim', me); bpy.context.collection.objects.link(trim)
    set_material(trim, HZ.pbr_slot(slots, 'gilt').name)
    girth = []
    bm = bmesh.new(); prev = None
    for k in range(41):
        a = 2 * math.pi * k / 40; d = V(0, math.sin(a), math.cos(a))
        hit = tree.ray_cast(V(-34, 0, Z(96)) + d * 60, -d, 100.)
        p = (hit[0] if hit[0] is not None else V(-34, 0, Z(96)) + d * 30) + d * .8
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
        hit = tree.ray_cast(V(-104, 70 * y, Z(96)), V(0, -y, 0), 200.)
        c = (hit[0] if hit[0] is not None else V(-104, 30 * y, Z(96))) + V(0, 7 * y, -4)
        bmesh.ops.translate(bm, verts=bm.verts, vec=c)
        me = bpy.data.meshes.new('Bag'); bm.to_mesh(me); bm.free(); ob = bpy.data.objects.new('Bag', me); bpy.context.collection.objects.link(ob)
        set_material(ob, HZ.pbr_slot(slots, 'leather').name); bags.append(ob)
    for ob in [blanket, trim, girth] + bags:
        Q = np.array([v.co[:] for v in ob.data.vertices])
        made.append(put_mesh(ob, rig, {k: v for k, v in B.weights(Q).items() if k in ('h_front', 'h_rear')} or None))
    # Lengths to a horse's: x scaled about the human's hips (every vertex made, every horse bone).
    for ob in made:
        for v in ob.data.vertices: v.co.x *= SX
        ob.data.update()
    HZ.kit.activate(rig); bpy.ops.object.mode_set(mode='EDIT')
    for eb in rig.data.edit_bones:
        if eb.name.startswith('h_'): eb.head.x *= SX; eb.tail.x *= SX
    bpy.ops.object.mode_set(mode='OBJECT')
    for ob in made:
        if not ob.vertex_groups:   # (a fallback: everything on the back moves with the body)
            g = ob.vertex_groups.new(name='h_rear'); g.add(list(range(len(ob.data.vertices))), 1., 'REPLACE')
    # A wide leather band round his waist where man becomes horse, and a strap across his chest with a bronze boss.
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
    for kind in ('coat_dark', 'hoof'): HZ.pbr_slot(slots, kind)
    HZ.PBR['coat_dark'] = dict(HZ.PBR['coat_dark'], tint=(.07, .042, .024))
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
    slots['GoatFur'] = {'type': 'hair', 'texture': 'Textures/beard_locks.png', 'tint': [.09, .055, .03]}
    Q = np.array([v.co[:] for v in fur.data.vertices]); made.append(put_mesh(fur, rig, B.weights(Q)))
    # Ram's horns: from above the temples, curling back and down round the ears, ridged.
    head_h, head_t = F.j('head'), F.j('head', True)
    top = head_t; fwd = V(1, 0, 0)
    bm = bmesh.new()
    for y in (1., -1.):
        pts = []
        c0 = head_h.lerp(head_t, .86) + V(3.2, 4.2 * y, 0)
        for i in range(34):
            t = i / 33; a = t * math.radians(330)
            r = 6.5 * (1 - .45 * t)
            p = c0 + V(-r * math.sin(a) - 3 * t, y * (1.8 + 4.2 * t), r * (1 - math.cos(a)) * .9 + 2.5 * math.sin(math.pi * t) - 7 * t * t)
            pts.append(p)
        prev = None
        for i, p in enumerate(pts):
            t = i / 33; d = (pts[min(i + 1, 33)] - pts[max(i - 1, 0)]).normalized(); n1 = d.cross(V(0, 1, 0)).normalized(); n2 = d.cross(n1)
            rad = 2.7 * (1 - .82 * t) + .25
            ridge = 1 + .09 * math.sin(i * 2.3)
            ring = [bm.verts.new(p + (n1 * math.cos(k * math.pi / 5) + n2 * math.sin(k * math.pi / 5)) * rad * ridge * (1.15 if k in (0, 1, 9) else 1.)) for k in range(10)]
            if prev:
                for k in range(10): bm.faces.new((prev[k], prev[(k + 1) % 10], ring[(k + 1) % 10], ring[k]))
            prev = ring
        bm.faces.new(prev)
    me = bpy.data.meshes.new('Horns'); bm.to_mesh(me); bm.free(); horns = bpy.data.objects.new('Horns', me); bpy.context.collection.objects.link(horns)
    for p in horns.data.polygons: p.use_smooth = True
    set_material(horns, HZ.pbr_slot(slots, 'horn').name); made.append(put_mesh(horns, rig, rigid={'head': 1.}))
    # A leather strap across his chest, from his right hip over his left shoulder, with a bronze boss.
    sh, hp = F.j('clavicle_l', True), F.j('thigh_r')
    path = [hp + V(10, -4, 6), (hp + sh) * .5 + V(15, 0, 4), sh + V(6, 0, 6), sh + V(-6, 0, 7), (hp + sh) * .5 + V(-13, 0, 2), hp + V(-10, -4, 6)]
    from mathutils.bvhtree import BVHTree
    body = next(o for o in meshes if o.data.materials and o.data.materials[0].name == 'Skin')
    bb = bmesh.new(); bb.from_mesh(body.data); bb.transform(Matrix.Scale(100., 4)); btree = BVHTree.FromBMesh(bb); bb.free()
    dense = []
    for i in range(len(path) - 1):
        for k in range(6): dense.append(path[i].lerp(path[i + 1], k / 6))
    dense.append(path[-1])
    path = []
    for p in dense:   # onto the skin, a little proud of it
        loc, nrm, _, _ = btree.find_nearest(p)
        path.append(loc + nrm * .7 if loc is not None else p)
    bm = bmesh.new(); prev = None
    for i, p in enumerate(path):
        d = (path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized(); loc, nrm, _, _ = btree.find_nearest(p); w = d.cross(nrm if nrm is not None else V(1, 0, 0)).normalized() * 2.4
        cur = [bm.verts.new(p - w), bm.verts.new(p + w)]
        if prev: bm.faces.new((prev[0], prev[1], cur[1], cur[0]))
        prev = cur
    me = bpy.data.meshes.new('Strap'); bm.to_mesh(me); bm.free(); strap = bpy.data.objects.new('Strap', me); bpy.context.collection.objects.link(strap)
    HZ.kit.activate(strap); m = strap.modifiers.new('Sol', 'SOLIDIFY'); m.thickness = .5; bpy.ops.object.modifier_apply(modifier=m.name)
    set_material(strap, HZ.pbr_slot(slots, 'leather').name); made.append(put_mesh(strap, rig, rigid={'spine_03': .6, 'spine_02': .4}))
    return made


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
    for tag, d, aim_z, lens in (('front', V(1, -.3, .05), mid, 40), ('side', V(0, -1, .05), mid, 40), ('three', V(-.7, -.7, .25), mid, 40), ('head', V(1, -.35, 0), hi - h * .08, 120)):
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam)
        target = V(-.4 if name == 'FeyCentaur' else 0., 0, aim_z)
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
