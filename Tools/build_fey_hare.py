"""Blender 4.5.14: the Feywild's great hare (Claude, 2026-11-25; docs/regions/FEYWILD-HARE.md, approved with its
recommendations).

An exaggerated brown hare about 1.3 m from nose to scut and some 80 cm at the shoulder, big enough for Chuck (65 cm) to
ride behind its shoulders: long black-tipped ears, a tawny agouti coat with a pale belly, a ginger chest, large amber
eyes set high on the sides of the head, a white scut black on top; faint pale-gold flecks in the coat and a few seed
heads caught in it (no glow). Signed distance fields with the creatures' kit (Tools/build_phleg_creatures.py, imported
unchanged, as Tools/build_fey_creatures.py does), on a skeleton of its own (BONES below), weighted by the parts' blended
distances (each part belongs to a bone), standing alert on all fours. The game poses it in code (FeyHare.cpp: a
procedural gait on a poseable mesh); nothing here is animated.

Its frame: centimetres, +X forward, +Y its left, the ground at z 0. `spine` is the body's root (its head mid-back), the
hips hang behind it pointing back, so that flexing the back is two pitches about one joint. `saddle` is a bone with no
weights where Chuck's seat goes (on the back behind the shoulders).

Colour: vertex colour RGB the albedo with its occlusion; A the sleek mask (eyes, nose, claws: no fur texture there).
The coat is painted in a neutral tawny; the game tints each hare (moss-grey to dusk-gold, a mix across the herd).

blender --background --factory-startup --python Tools/build_fey_hare.py -- [--review DIR]
  -> SourceAssets/Enemies/Feywild/Hare/SK_FeyHare.fbx and hare.json (bones, the saddle, the UV scale)
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
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None

_spec = importlib.util.spec_from_file_location('phleg_creatures', ROOT / 'Tools/build_phleg_creatures.py')
P = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(P)
P.OUT = ROOT / 'SourceAssets/Enemies/Feywild/Hare'
P.REVIEW = REVIEW
import sdf_kit as K  # noqa: E402
from sdf_kit import fbm, smooth  # noqa: E402
Body, ell, V3, lin = P.Body, P.ell, P.V3, P.lin


def norm(v):
    v = np.asarray(v, float); return v / np.linalg.norm(v)


# ------------------------------------------------------------------ the skeleton (cm; +Y its left)
BONES = {
    'root':      ((0, 0, 0), (0, 0, 12), None),
    'spine':     ((-14, 0, 64), (8, 0, 64), 'root'),
    'chest':     ((8, 0, 64), (28, 0, 60), 'spine'),
    'neck':      ((28, 0, 62), (40, 0, 76), 'chest'),
    'head':      ((40, 0, 76), (64, 0, 74), 'neck'),
    'hips':      ((-14, 0, 64), (-36, 0, 60), 'spine'),
    'scut':      ((-40, 0, 62), (-48, 0, 68), 'hips'),
    'saddle':    ((-3, 0, 80), (-3, 0, 88), 'spine'),
}
for s, S in ((1, 'l'), (-1, 'r')):
    BONES.update({
        f'ear1_{S}':  ((45, 5 * s, 85), (43, 9 * s, 104), 'head'),
        f'ear2_{S}':  ((43, 9 * s, 104), (39, 12 * s, 123), f'ear1_{S}'),
        f'fupper_{S}': ((24, 8.5 * s, 54), (20, 8.5 * s, 33), 'chest'),
        f'flower_{S}': ((20, 8.5 * s, 33), (23, 8.5 * s, 7), f'fupper_{S}'),
        f'fpaw_{S}':  ((23, 8.5 * s, 7), (31, 8.5 * s, 1.5), f'flower_{S}'),
        f'thigh_{S}': ((-28, 11.5 * s, 56), (-13, 11.5 * s, 37), 'hips'),
        f'shin_{S}':  ((-13, 11.5 * s, 37), (-33, 11.5 * s, 17), f'thigh_{S}'),
        f'hfoot_{S}': ((-33, 11.5 * s, 17), (-12, 11.5 * s, 3), f'shin_{S}'),
        f'htoe_{S}':  ((-12, 11.5 * s, 3), (-2, 11.5 * s, 1.2), f'hfoot_{S}'),
    })
J = {n: (V3(*h), V3(*t)) for n, (h, t, _) in BONES.items()}
def H(b): return J[b][0]
def T(b): return J[b][1]
def along(b, t): return H(b) + (T(b) - H(b)) * t
def dirn(b): return norm(T(b) - H(b))


def build_rig():
    arm = bpy.data.armatures.new('SK_FeyHare_Rig')
    rig = bpy.data.objects.new('SK_FeyHare_Rig', arm)
    bpy.context.scene.collection.objects.link(rig)
    P.select_only(rig)
    bpy.ops.object.mode_set(mode='EDIT')
    from mathutils import Vector
    for name, (h, t, parent) in BONES.items():
        b = arm.edit_bones.new(name)
        b.head = Vector(h) * .01; b.tail = Vector(t) * .01; b.roll = 0.
    for name, (_, _, parent) in BONES.items():
        if parent: arm.edit_bones[name].parent = arm.edit_bones[parent]
    bpy.ops.object.mode_set(mode='OBJECT')
    return rig


def ear_frame(S):
    """An ear segment's axes: z along it, x its thin axis (the flat faces open out to the side and a little forward)."""
    s = 1 if S == 'l' else -1
    def f(b):
        z = dirn(b); x = V3(.35, s, 0.); x = x - z * x.dot(z); x = norm(x); y = np.cross(z, x)
        return np.stack([x, y, z], 1)
    return f


def limb(B, bone, r0, r1, tag='coat', k=2.5, ext0=0., ext1=0.):
    a, b = H(bone) - dirn(bone) * ext0, T(bone) + dirn(bone) * ext1
    return B.tube(bone, [a, (a + b) / 2, b], [r0, (r0 + r1) / 2 * 1.05, r1], tag, k)


EYES = {S: V3(53.5, 7.4 * s, 82.5) for s, S in ((1, 'l'), (-1, 'r'))}
EYE_R = 3.3


def hare_body():
    B = Body(tag_reach=.7)
    # The trunk: a long barrel rising to a high, round rump; the chest deep and narrow between the forelegs.
    B.ells('spine', [ell((-4, 0, 63), (23, 15.5, 15.5))], k=9)
    B.ells('chest', [ell((16, 0, 58), (16, 13.5, 16.5)), ell((22, 0, 50), (10, 10, 9))], k=9)
    B.ells('hips', [ell((-27, 0, 63), (17, 17, 17.5)), ell((-35, 0, 60), (12, 15, 14))], k=9)
    B.ells('spine', [ell((-5, 0, 52), (20, 13, 8))], tag='belly', k=8)              # the belly's underline
    # The neck short and thick, the scruff over it (where Chuck's paws hold).
    B.tube('neck', [V3(26, 0, 60), V3(34, 0, 68), V3(41, 0, 76)], [12.5, 11., 9.], k=6)
    B.ells('neck', [ell((30, 0, 70), (9, 9.5, 6))], k=5)
    B.ells('neck', [ell((33, 0, 59), (8, 8, 8))], tag='chestfur', k=5)
    # The head: a long rounded skull, the muzzle blunt and deep, the nose split, big eyes high on the sides.
    B.ells('head', [ell((49, 0, 80), (12.5, 8.6, 9.5)), ell((59, 0, 77), (8.5, 6.6, 7.2))], k=5)
    B.ells('head', [ell((54, 6.2, 76.5), (6.5, 3.8, 5.)), ell((54, -6.2, 76.5), (6.5, 3.8, 5.))], k=3.5)    # cheeks
    B.ells('head', [ell((65.8, 0, 77.6), (2.3, 3.2, 2.5))], tag='nose', k=1.4)
    for s in (1, -1):
        B.ells('head', [ell((64.6, 1.7 * s, 74.6), (2.4, 2.1, 2.2))], tag='muzzle', k=1.)                     # the split upper lip
    B.ells('head', [ell((60.5, 0, 72.2), (4.6, 3.6, 2.4))], tag='muzzle', k=1.5)                              # the chin
    for S, c in EYES.items():
        s = 1 if S == 'l' else -1
        B.ells('head', [ell(c - V3(0, .4 * s, 0), (4.4, 2.4, 4.1))], tag='ring', k=1.)                       # the pale eye ring
        B.balls('head', [c], [EYE_R], tag='eye', k=.3)
    # The ears: long, broad and flat, a little cupped, black at the tips.
    for S in 'lr':
        fr = ear_frame(S)
        for b, r in ((f'ear1_{S}', (1.7, 5.6, 11.)), (f'ear2_{S}', (1.5, 5.2, 11.5))):
            B.ells(b, [(along(b, .5), np.array(r, float), fr(b))], tag='ear', k=1.6 if b.startswith('ear2') else 2.2)
        B.ells(f'ear1_{S}', [(H(f'ear1_{S}') + V3(0, 0, 1), np.array((2.6, 4.2, 4.), float), fr(f'ear1_{S}'))], tag='ear', k=2.)
        B.ells(f'ear2_{S}', [(along(f'ear2_{S}', .86), np.array((1.3, 3.8, 4.5), float), fr(f'ear2_{S}'))], tag='tip', k=1.)
        # the cup: a shallow hollow in each ear's outer face
        for b in (f'ear1_{S}', f'ear2_{S}'):
            x = fr(b)[:, 0]
            B.sub(K.Ellipsoids('sub', .8, [(along(b, .55) + x * 1.9, np.array((1.1, 3.9, 9.5), float), fr(b))]))
    # The scut: white, black on top.
    B.ells('scut', [ell((-44, 0, 66), (6.5, 6., 6.5))], tag='scut', k=3)
    # Forelegs: slender, long; small paws with claws.
    for S in 'lr':
        limb(B, f'fupper_{S}', 6.4, 4.4, ext0=3.)
        B.ells(f'fupper_{S}', [ell(along(f'fupper_{S}', .25), (7.5, 5.5, 9.))], k=4)                        # the shoulder's muscle
        limb(B, f'flower_{S}', 3.2, 2.7)
        limb(B, f'fpaw_{S}', 2.8, 2.3)
        B.ells(f'fpaw_{S}', [ell(along(f'fpaw_{S}', .7) + V3(0, 0, -.3), (4.5, 2.8, 1.9))], k=1.2)
        for y in (-1.2, 0., 1.2):
            p = T(f'fpaw_{S}') + V3(0, y, .4)
            B.tube(f'fpaw_{S}', [p, p + V3(1.8, 0, -.9)], [.55, .1], tag='claw', k=.25)
    # Hind legs: great thighs, long shins, the long hind foot flat on the ground at the toes.
    for s, S in ((1, 'l'), (-1, 'r')):
        B.ells(f'thigh_{S}', [ell((-22, 12.5 * s, 50), (15.5, 7.5, 15.)), ell((-17, 11.5 * s, 41), (9., 6.5, 8.))], k=6)
        limb(B, f'shin_{S}', 5.5, 3.4)
        B.ells(f'shin_{S}', [ell(along(f'shin_{S}', .3), (6., 4.2, 7.))], k=3)
        limb(B, f'hfoot_{S}', 3.2, 2.6, ext0=1.)
        B.ells(f'hfoot_{S}', [ell(along(f'hfoot_{S}', .55) + V3(0, 0, -.8), (3.4, 2.4, 11.), dirn(f'hfoot_{S}'))], k=1.5)
        limb(B, f'htoe_{S}', 2.6, 2.)
        for y in (-1.3, 0., 1.3):
            p = T(f'htoe_{S}') + V3(0, y, .3)
            B.tube(f'htoe_{S}', [p, p + V3(2., 0, -.8)], [.6, .1], tag='claw', k=.25)
    # A few seed heads caught in the fur of the back and flanks (round tufts of pale down).
    rng = np.random.default_rng(91)
    for i in range(7):
        bone = ('spine', 'hips', 'spine', 'chest', 'hips', 'spine', 'neck')[i]
        a = rng.uniform(-1.1, 1.1); x = {'spine': rng.uniform(-14, 6), 'hips': rng.uniform(-34, -18), 'chest': rng.uniform(10, 20), 'neck': 30}[bone]
        r0 = {'spine': 15.5, 'hips': 17., 'chest': 14., 'neck': 9.5}[bone]; cz = {'spine': 63, 'hips': 63, 'chest': 58, 'neck': 70}[bone]
        c = V3(x, r0 * math.sin(a) * .98, cz + r0 * math.cos(a) * .98)
        B.ells(bone, [ell(c, (1.3, 1.3, .7))], tag='seed', k=.6)
    # Fur: a soft, lumpy lie of the coat (more on the body, little on the face and paws).
    B.disp(lambda Q: .45 * fbm(Q / 5., 2, 93) + .2 * fbm(Q / 1.4, 2, 95))
    return B


def hare_look(body, ops, V, N):
    tags = body.tag_mask(V)
    def m(t): return tags.get(t, np.zeros(len(V)))[:, None]
    n1 = fbm(V / 7., 2, 101) * .5 + .5
    tick = fbm(V / 1.3, 2, 103) * .5 + .5                                   # the agouti ticking
    back = lin(122, 92, 60)[None] * (.78 + .35 * n1[:, None])
    back = back * (1 - .45 * smooth(.55, .8, tick)[:, None]) + lin(196, 160, 104)[None] * (.35 * smooth(.62, .3, tick)[:, None])
    flank = lin(166, 116, 68)[None] * (.85 + .25 * n1[:, None])
    up = smooth(.15, .75, N[:, 2])[:, None]
    col = flank * (1 - up) + back * up
    # pale belly and inner legs; a ginger chest and neck front; the paws paler
    belly = np.maximum(smooth(-.25, -.75, N[:, 2]), m('belly')[:, 0] * smooth(.1, -.4, N[:, 2]))[:, None]
    col = col * (1 - belly) + lin(226, 214, 196)[None] * belly
    chestfur = np.maximum(m('chestfur')[:, 0], smooth(.2, .8, N[:, 0]) * smooth(70, 50, V[:, 2]) * smooth(10, 24, V[:, 0]))[:, None]
    col = col * (1 - .6 * chestfur) + lin(186, 120, 64)[None] * (.6 * chestfur)
    low = smooth(14, 3, V[:, 2])[:, None]
    col = col * (1 - .45 * low) + lin(200, 180, 150)[None] * (.45 * low)
    # the face: paler round the muzzle, the eye ring, the nose a dusky brown
    col = col * (1 - m('muzzle')) + lin(214, 196, 168)[None] * m('muzzle')
    col = col * (1 - m('ring')) + lin(226, 206, 170)[None] * m('ring')
    col = col * (1 - m('nose')) + lin(96, 70, 62)[None] * m('nose')
    # ears: the coat on their backs, paler inside the cup, black tips
    ear = np.maximum(m('ear'), m('tip'))
    inner = np.zeros(len(V))
    for S in 'lr':
        s = 1 if S == 'l' else -1
        for b in (f'ear1_{S}', f'ear2_{S}'):
            x = ear_frame(S)(b)[:, 0]
            near = np.linalg.norm(np.cross(V - H(b), dirn(b)), axis=1) < 7.
            t = ((V - H(b)) @ dirn(b)) / np.linalg.norm(T(b) - H(b))
            inner = np.maximum(inner, near * (t > -.1) * (t < 1.1) * smooth(.15, .55, N @ x))
    inner = (inner * ear[:, 0])[:, None]
    col = col * (1 - .75 * inner) + lin(196, 168, 150)[None] * (.75 * inner)
    col = col * (1 - m('tip')) + lin(22, 18, 16)[None] * m('tip')
    # the scut: white, black on top
    top = smooth(.1, .5, N[:, 2])[:, None] * m('scut')
    col = col * (1 - m('scut')) + lin(236, 232, 224)[None] * m('scut')
    col = col * (1 - top) + lin(30, 24, 20)[None] * top
    # amber eyes with a dark pupil toward their outer face; claws; seed heads
    eye = m('eye')
    pupil = np.zeros(len(V))
    for S, c in EYES.items():
        s = 1 if S == 'l' else -1
        out = norm(V3(.25, s, .1))
        pupil = np.maximum(pupil, smooth(.72, .9, ((V - c) @ out) / EYE_R) * (np.linalg.norm(V - c, axis=1) < EYE_R + 1.))
    eyec = lin(206, 138, 36)[None] * (1 - pupil[:, None]) + lin(8, 6, 5)[None] * pupil[:, None]
    col = col * (1 - eye) + eyec * eye
    col = col * (1 - m('claw')) + lin(58, 48, 40)[None] * m('claw')
    col = col * (1 - m('seed')) + lin(224, 210, 168)[None] * m('seed')
    # the Feywild's touch: faint pale-gold flecks on the back and flanks
    fl = fbm(V / .9, 2, 107) * .5 + .5
    fleck = (smooth(.78, .9, fl) * smooth(-.2, .4, N[:, 2]) * (1 - ear[:, 0]) * (1 - eye[:, 0]))[:, None]
    col = col * (1 - .45 * fleck) + lin(240, 214, 140)[None] * (.45 * fleck)
    ao = P.sdf_ao(V, N, ops, .8)
    col = col * (.38 + .62 * ao)[:, None]
    sleek = np.clip(eye[:, 0] + .8 * m('nose')[:, 0] + .9 * m('claw')[:, 0], 0, 1)
    return col, sleek


def export(rig, ob):
    P.to_metres(ob)
    ob.parent = rig
    mod = ob.modifiers.new('Armature', 'ARMATURE'); mod.object = rig
    P.OUT.mkdir(parents=True, exist_ok=True)
    P.select_only(rig, ob)
    ob.select_set(True); rig.select_set(True); bpy.context.view_layer.objects.active = rig
    fbx = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False, primary_bone_axis='Y',
               secondary_bone_axis='X', use_armature_deform_only=False, mesh_smooth_type='FACE', colors_type='LINEAR')
    bpy.ops.export_scene.fbx(filepath=str(P.OUT / 'SK_FeyHare.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'}, bake_anim=False, **fbx)
    K.log('fbx', 'SK_FeyHare')


if __name__ == '__main__':
    bpy.ops.wm.read_factory_settings(use_empty=True)
    rig = build_rig()
    body = hare_body()
    ob, V = P.field_object('SK_FeyHare', body, .55, .28, hare_look)
    W = body.weights(V)
    P.add_weights(ob, W)
    uv = round(P.unwrap(ob), 2)
    P.add_material(ob, 'Fur')
    if REVIEW: P.review(ob, 'SK_FeyHare')
    Vz = P.verts_of(ob)
    export(rig, ob)
    info = {'uv_cm': uv, 'bones': {n: {'head': list(h), 'tail': list(t), 'parent': p} for n, (h, t, p) in BONES.items()},
            'bounds_cm': [[round(float(x), 1) for x in Vz.min(0)], [round(float(x), 1) for x in Vz.max(0)]],
            'tris': sum(len(p.vertices) - 2 for p in ob.data.polygons),
            'note': 'Tools/build_fey_hare.py; the game poses it in code (FeyHare.cpp). saddle: Chuck\'s seat, no weights.'}
    (P.OUT / 'hare.json').write_text(json.dumps(info, indent=1) + '\n', encoding='utf-8')
    print('CHUCK_FEY_HARE_BUILT', info['tris'], info['bounds_cm'])
