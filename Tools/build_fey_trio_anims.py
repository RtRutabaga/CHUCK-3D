"""Blender 4.5.14: the Crossroads trio's clip (Claude, 2026-11-23; Tools/build_fey_trio.py made them). One loop of LOOP
seconds for all three together, authored as motion in one scene, so that played together (the game puts all three at the
same place and starts them together) the satyr stays on the centaur's back and the gnome on the satyr's shoulders.

blender --background --python Tools/build_fey_trio_anims.py [-- --review <dir>]
  -> SourceAssets/NPCs/Fey/<Name>/Anim/AS_<Name>_Mill.fbx, SourceAssets/NPCs/Fey/clip.json

The loop (the user: "have them mill around next to the giant tree"): the centaur stands (breathing, shifting his weight,
his tail switching, looking about), walks half way round a circle 4 m across, stands again, walks the other half back to
where he began, and stands; the gnome points the way (ahead while they walk, off to one side and another while they
stand), his left hand on the satyr's horn; the satyr rides with his hands on the centaur's shoulders, laughing in bursts,
looking about. Every motion is periodic in the loop, so it runs on without a seam.

  * The centaur's Root bone carries him round the circle (the clip's root motion stays in it: the game does not move the
    actor). His walk is a four-beat walk (left hind, left fore, right hind, right fore, a quarter apart), its phase driven
    by the distance walked, so the hooves stay planted while they bear him: each is planted where the body will be at the
    middle of its stance and swings forward in an arc to the next. Two-bone IK reaches each fetlock (the knee or the hock
    a hinge), the pastern points at the hoof; the body bobs and rolls with the steps; the tail sways.
  * The satyr's Root follows the centaur's h_front bone (his pelvis on the saddle behind the man's hips), his hands flat
    on the centaur's shoulder caps (2026-11-24, third round: found on the centaur's skin and vest, the roll that lays the
    palm down, lifted clear; CHUCK_TRIO_CLEAR reports the arms against the centaur). His legs go round the barrel as a rider's do (2026-11-24): knee, hock, fetlock and hoof
    each placed by searching the directions that clear the horse's coat (the thigh pressing in no more than 2 cm), the
    thigh over the blanket's curve, the shank close down the flank; IK and tracking put the bones there.
  * The gnome rides the satyr's shoulders piggyback: his seat on the shoulders behind the neck (measured on the satyr's
    seated body), his thighs forward over the shoulders, knees in front of the collarbones, shins down the satyr's chest
    (placed the same way, clear of the satyr's body); his left hand holds the satyr's left horn, his right arm points.
    Targets on the satyr's bones are parented with the satyr seated (before, they were parented with him standing at the
    origin and so rose twice: the gnome perched high).
  * After baking, CHUCK_TRIO_CLEAR reports how far the posed legs cut into the horse and the satyr.
Everything is baked to keys on every frame (30 fps) and exported per rig with its meshes.
"""
from pathlib import Path
import json
import math
import os
import sys
import bpy
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[1]
FEY = ROOT / 'SourceAssets/NPCs/Fey'
FPS = 30
LOOP = 60.
N = int(LOOP * FPS)
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None
FBX_EXPORT = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False, primary_bone_axis='Y',
                  secondary_bone_axis='X', use_armature_deform_only=True, bake_anim=True, bake_anim_use_all_actions=False,
                  bake_anim_use_nla_strips=False, bake_anim_step=1., bake_anim_simplify_factor=0., bake_anim_force_startend_keying=True,
                  mesh_smooth_type='FACE', colors_type='LINEAR')


def V(*a): return Vector(a)
def smooth(a, b, x): t = min(1., max(0., (x - a) / (b - a))); return t * t * (3 - 2 * t)
def W(k): return 2 * math.pi * k / LOOP     # a frequency with k whole cycles in the loop (no seam)


# ------------------------------------------------------------------ load the three
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene; scene.render.fps = FPS
RIG = {}
for name in ('FeyCentaur', 'FeySatyr', 'FeyGnome'):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(FEY / f'{name}/SK_{name}.fbx'), automatic_bone_orientation=False, ignore_leaf_bones=False)
    new = [o for o in bpy.data.objects if o not in before]
    rig = next(o for o in new if o.type == 'ARMATURE'); rig.name = f'Rig_{name}'
    RIG[name] = (rig, [o for o in new if o.type == 'MESH'])
C, S, G = (RIG[n][0] for n in ('FeyCentaur', 'FeySatyr', 'FeyGnome'))
scene.render.fps = FPS; scene.render.fps_base = 1.   # (the FBX imports set it back to their own 24)
for rig in (C, S, G):
    rig.animation_data_create(); rig.animation_data.action = bpy.data.actions.new(f'{rig.name}_Mill'); rig.data.pose_position = 'POSE'
    for pb in rig.pose.bones: pb.rotation_mode = 'QUATERNION'


def rest(rig, b, tail=False):
    bone = rig.data.bones[b]; return rig.matrix_world @ (bone.tail_local if tail else bone.head_local)


def empty(name, loc=V(0, 0, 0)):
    e = bpy.data.objects.new(name, None); bpy.context.collection.objects.link(e); e.location = loc; e.empty_display_size = .05
    return e


def ik(rig, bone, target, pole, chain=2, pole_angle=-90.):
    c = rig.pose.bones[bone].constraints.new('IK'); c.target = target; c.chain_count = chain
    if pole is not None: c.pole_target = pole; c.pole_angle = math.radians(pole_angle)
    return c


def on_bone(e, rig, bone):
    """e (placed in the rest pose, world) carried by rig's bone from now on."""
    bpy.context.view_layer.update()
    w = e.matrix_world.copy()
    e.parent = rig; e.parent_type = 'BONE'; e.parent_bone = bone
    bpy.context.view_layer.update()
    e.matrix_world = w


def key_quat(pb, q, f):
    pb.rotation_quaternion = q; pb.keyframe_insert('rotation_quaternion', frame=f)


def about(rig, bone, axis_world, angle):
    """A rotation of bone about a world axis (in its rest orientation), in its local space."""
    M = rig.matrix_world.to_3x3() @ rig.data.bones[bone].matrix_local.to_3x3()
    return Quaternion((M.inverted() @ axis_world).normalized(), angle)


def fingers(rig, side, which, amount, f, thumb=0.):
    """Fingers curled into the palm: about each bone's own bend axis (local X), less at the base."""
    for fn in ('index', 'middle', 'ring', 'pinky'):
        a = amount if fn in which else .12
        for j, k in ((1, .75), (2, 1.), (3, .8)):
            pb = rig.pose.bones.get(f'{fn}_0{j}_{side}')
            if pb: key_quat(pb, Quaternion(V(1, 0, 0), a * k), f)
    for j in (2, 3):
        pb = rig.pose.bones.get(f'thumb_0{j}_{side}')
        if pb: key_quat(pb, Quaternion(V(1, 0, 0), thumb), f)


# ------------------------------------------------------------------ the path round the circle
R = 2.0
CENTRE = V(0., R, 0.)
WALKS = [(8., 20.), (32., 44.)]
HALF = math.pi * R
EASE = 1.2


def walked(t):
    """Distance along the circle at t (eased in and out of each walk), and how fully he is walking (0..1)."""
    s = 0.; w = 0.
    for t0, t1 in WALKS:
        if t >= t1: s += HALF; continue
        if t > t0:
            u = (t - t0) / (t1 - t0); e = EASE / (t1 - t0); vmax = 1. / (1. - e)
            if u < e: fr = .5 * vmax * u * u / e
            elif u > 1 - e: fr = 1. - .5 * vmax * (1 - u) ** 2 / e
            else: fr = vmax * (u - e / 2)
            s += HALF * fr; w = smooth(0., e, u) * smooth(1., 1. - e, u)
        break
    return s, w


def path_at(s):
    a = s / R
    return CENTRE + V(math.sin(a) * R, -math.cos(a) * R, 0.), a


# ------------------------------------------------------------------ the centaur's legs
STRIDE, STANCE, STEP_LIFT = 1.55, .64, .11
LEGS = {}
for leg, off in (('hl', 0.), ('fl', .25), ('hr', .5), ('fr', .75)):
    LEGS[leg] = dict(fet=rest(C, f'h_{leg}_cannon', True), hoof=rest(C, f'h_{leg}_pastern', True), off=off)


def to_world(local, p, h): return p + Matrix.Rotation(h, 3, 'Z') @ local


def leg_targets(leg, t):
    L = LEGS[leg]; s, w = walked(t); p, h = path_at(s)
    rf, rh = to_world(L['fet'], p, h), to_world(L['hoof'], p, h)
    k = smooth(0., .35, w)
    if k <= 0.: return rf, rh
    phi = (s / STRIDE + L['off']) % 1.
    if phi < STANCE:
        pp, hh = path_at(s - (phi - STANCE / 2) * STRIDE); f, ho = to_world(L['fet'], pp, hh), to_world(L['hoof'], pp, hh)
    else:
        u = (phi - STANCE) / (1 - STANCE); u = u * u * (3 - 2 * u)
        sa = s - (phi - STANCE / 2) * STRIDE; pa, ha = path_at(sa); pb, hb = path_at(sa + STRIDE)
        f = to_world(L['fet'], pa, ha).lerp(to_world(L['fet'], pb, hb), u); ho = to_world(L['hoof'], pa, ha).lerp(to_world(L['hoof'], pb, hb), u)
        lift = math.sin(math.pi * u) * STEP_LIFT; f = f + V(0, 0, lift * 1.15); ho = ho + V(0, 0, lift)
    return rf.lerp(f, k), rh.lerp(ho, k)


LEG_T = {}
for leg in LEGS:
    ft, hf, pole = empty(f'fet_{leg}'), empty(f'hoof_{leg}'), empty(f'pole_{leg}')
    LEG_T[leg] = (ft, hf, pole)
    ik(C, f'h_{leg}_cannon', ft, pole)
    c = C.pose.bones[f'h_{leg}_pastern'].constraints.new('DAMPED_TRACK'); c.target = hf; c.track_axis = 'TRACK_Y'


# Each leg's IK pole angle fitted so that with its targets at rest the leg is at rest (2026-11-24): a fixed -90 twisted
# the forearm and cannon about their length against the shoulder, which pinched the top of the foreleg in game.
for leg, (ft, hf, pole) in LEG_T.items():
    L = LEGS[leg]; ft.location = L['fet']; hf.location = L['hoof']
    pole.location = L['fet'] + V(0, 0, .5) + V(.7 if leg[0] == 'f' else -.7, 0, 0)
    con = next(c for c in C.pose.bones[f'h_{leg}_cannon'].constraints if c.type == 'IK')
    best = None
    for a in range(-180, 180, 5):
        con.pole_angle = math.radians(a); bpy.context.view_layer.update()
        err = sum((C.pose.bones[b].matrix.to_quaternion().rotation_difference(C.data.bones[b].matrix_local.to_quaternion())).angle
                  for b in (f'h_{leg}_lower', f'h_{leg}_cannon'))
        if best is None or err < best[1]: best = (a, err)
    con.pole_angle = math.radians(best[0])
    print('CHUCK_TRIO_LEG_POLE', leg, best[0], 'twist_deg', round(math.degrees(best[1]), 1))


# ------------------------------------------------------------------ the riders
from mathutils.bvhtree import BVHTree


def mat(o):
    """An object's first material's name, without Blender's .001 (the three share slot names)."""
    return o.data.materials[0].name.split('.')[0] if o.data.materials else ''


ARM_BONES = ('upperarm', 'lowerarm', 'hand', 'thumb', 'index', 'middle', 'ring', 'pinky')


def tree_of(objs, M=Matrix.Identity(4), skip_arms=False):
    """One BVH (world, metres) over objects' undeformed meshes (their rest), moved by M; without the arms (faces with a
    corner mostly on an arm bone) if skip_arms: they are posed elsewhere."""
    verts, polys = [], []
    for o in objs:
        W = M @ o.matrix_world; base = len(verts)
        verts += [W @ v.co for v in o.data.vertices]
        arm = set()
        if skip_arms:
            names = {g.index: g.name for g in o.vertex_groups}
            arm = {v.index for v in o.data.vertices if sum(g.weight for g in v.groups if names.get(g.group, '').startswith(ARM_BONES)) > .5}
        polys += [[base + i for i in p.vertices] for p in o.data.polygons if not arm or not any(i in arm for i in p.vertices)]
    return BVHTree.FromPolygons(verts, polys)


def signed(tree, p, flip=1.):
    """Distance from p to the surface, negative inside (by the nearest face's normal)."""
    loc, n, _, d = tree.find_nearest(p)
    if loc is None: return 9.
    return d * (1. if (p - loc).dot(n) >= 0 else -1.) * flip


def clear(tree, a, b, ra, rb, t0=0., n=10, flip=1.):
    """The least clearance along a limb segment a..b of radius ra..rb (from t0 along it): negative where it cuts in."""
    return min(signed(tree, a.lerp(b, t0 + (1 - t0) * i / n), flip) - (ra + (rb - ra) * (t0 + (1 - t0) * i / n)) for i in range(n + 1))


def reach(tree, at, length, ra, rb, score, sgn, t0=.2, allow=0., flip=1., phis=range(-20, 101, 5), thetas=range(-180, 181, 10)):
    """The best direction for a limb from `at` of `length`: among those clearing the body (by `allow`), the lowest
    `score(dir, clearance)`; if none clears, the one cutting in least. phi down from level, theta round from forward
    toward his outside (sgn: +1 his left)."""
    best, fall = None, None
    for ph in phis:
        for th in thetas:
            a, b = math.radians(ph), math.radians(th)
            d = V(math.cos(a) * math.cos(b), sgn * math.cos(a) * math.sin(b), -math.sin(a))
            c = clear(tree, at, at + d * length, ra, rb, t0, flip=flip)
            if c >= -allow:
                k = score(d, c)
                if best is None or k < best[0]: best = (k, d, c)
            if fall is None or c > fall[1]: fall = (d, c)
    return (best[1], best[2]) if best else fall


def bone_len(rig, b): return (rest(rig, b, True) - rest(rig, b)).length


cover = [o for o in RIG['FeyCentaur'][1] if mat(o) in ('Hero_blanket', 'Hero_coat')]
coat = [o for o in RIG['FeyCentaur'][1] if mat(o) == 'Hero_coat']
HORSE = tree_of(coat)
H_FLIP = -1. if signed(HORSE, rest(C, 'h_front').lerp(rest(C, 'h_rear', True), .5)) > 0 else 1.   # (the barrel's middle is inside)


def back_top(x, y):
    hit = tree_of(cover).ray_cast(V(x, y, 4.), V(0, 0, -1), 10.)
    return hit[0].z if hit[0] is not None else None


SEAT_X = -.45
top = back_top(SEAT_X, 0.)
S_PELVIS = rest(S, 'pelvis')
SEAT = V(SEAT_X, 0., top + .10)                         # where the satyr's pelvis sits
S_OFF = Matrix.Translation(SEAT - S_PELVIS)              # his rest moved onto the saddle
C_FRONT_REST = C.matrix_world @ C.data.bones['h_front'].matrix_local
print('CHUCK_TRIO_SEAT', tuple(round(x, 3) for x in SEAT), 'back', round(top, 3))
S_CHEST_REST = S_OFF @ (S.matrix_world @ S.data.bones['spine_03'].matrix_local)
s_root = S.pose.bones['Root']; s_root_rest = S.data.bones['Root'].matrix_local.copy()
g_root = G.pose.bones['Root']; g_root_rest = G.data.bones['Root'].matrix_local.copy()
s_root.matrix = S_OFF @ s_root_rest                      # (seated while the targets are placed and parented)
bpy.context.view_layer.update()

# The satyr's legs round the barrel: each thigh over the blanket's curve (cutting in no more than a rider's thigh
# presses), the shank hanging close down the flank, the goat's cannon and hoof below it, clear of the coat.
S_LEGS = {}
for side, sgn in (('l', 1.), ('r', -1.)):
    hip = S_OFF @ rest(S, f'thigh_{side}')
    L1, L2, L3, L4 = (bone_len(S, f'{b}_{side}') for b in ('thigh', 'calf', 'foot', 'ball'))
    d1, c1 = reach(HORSE, hip, L1, .095, .075, lambda d, c: d.z * 1.0 - d.x * .6 + abs(d.y) * .15, sgn, t0=.3, allow=.02, flip=H_FLIP)
    knee = hip + d1 * L1
    d2, c2 = reach(HORSE, knee, L2, .075, .05, lambda d, c: d.z + d.x * .5 + 2.5 * max(0., c - .02), sgn, t0=.1, allow=.008, flip=H_FLIP)
    hock = knee + d2 * L2
    d3, c3 = reach(HORSE, hock, L3, .04, .035, lambda d, c: d.z - d.x * .2 + 2.5 * max(0., c - .02), sgn, t0=.1, allow=.0, flip=H_FLIP)
    fet = hock + d3 * L3
    d4, c4 = reach(HORSE, fet, L4, .035, .045, lambda d, c: d.z * .8 - d.x * .3 + 1.5 * max(0., c - .02), sgn, t0=.1, allow=.0, flip=H_FLIP)
    toe = fet + d4 * L4
    S_LEGS[side] = (hip, knee, hock, fet, toe)
    print('CHUCK_TRIO_SATYR_LEG', side, 'clear_cm', [round(c * 100, 1) for c in (c1, c2, c3, c4)], 'knee', tuple(round(x, 2) for x in knee), 'hoof', tuple(round(x, 2) for x in toe))
    t_hock = empty(f's_hock_{side}', hock); on_bone(t_hock, C, 'h_front')
    t_knee = empty(f's_knee_{side}', knee + (knee - (hip + hock) * .5).normalized() * .5); on_bone(t_knee, C, 'h_front')
    S_LEGS[side] += (ik(S, f'calf_{side}', t_hock, t_knee),)
    t_fet = empty(f's_fet_{side}', fet); on_bone(t_fet, C, 'h_front')
    t_toe = empty(f's_toe_{side}', toe); on_bone(t_toe, C, 'h_front')
    for b, t in (('foot', t_fet), ('ball', t_toe)):
        c = S.pose.bones[f'{b}_{side}'].constraints.new('DAMPED_TRACK'); c.target = t; c.track_axis = 'TRACK_Y'


def fit_pole(rig, chain_bone, mid_bone, want):
    """The IK's pole angle that puts the middle joint where it was solved for (the rig's own bone rolls decide it)."""
    con = rig.pose.bones[chain_bone].constraints[-1] if rig.pose.bones[chain_bone].constraints[-1].type == 'IK' else next(c for c in rig.pose.bones[chain_bone].constraints if c.type == 'IK')
    best = None
    for a in range(-180, 180, 15):
        con.pole_angle = math.radians(a); bpy.context.view_layer.update()
        e = ((rig.matrix_world @ rig.pose.bones[mid_bone].head) - want).length
        if best is None or e < best[1]: best = (a, e)
    con.pole_angle = math.radians(best[0]); bpy.context.view_layer.update()
    return best


for side in 'lr':
    a, e = fit_pole(S, f'calf_{side}', f'calf_{side}', S_LEGS[side][1])
    print('CHUCK_TRIO_SATYR_POLE', side, a, 'knee_err_cm', round(e * 100, 1))


# His hands flat on the centaur's shoulders (2026-11-24, third round, the user: "the satyrs arms should not pass impossibly
# through the centaur's clothes or body"). They were a wrist target near the collarbone with the hand left as it hung,
# so the fingers went into the vest. Now: the wrist just behind the top of each shoulder, the knuckles on it, both found
# on the centaur's skin and vest (rays down); the hand's roll chosen and the pair lifted until no part of the hand is in
# him; the elbow out to the side.
CUP_OBJS = [o for o in RIG['FeyCentaur'][1] if mat(o) in ('Skin', 'Vest', 'VestTrim', 'Belt')]
CUP = tree_of(CUP_OBJS, skip_arms=True)
C_FLIP = -1. if signed(CUP, rest(C, 'spine_03')) > 0 else 1.
def down(x, y):   # (from under the chin: the head is not what his hands rest on)
    z0 = rest(C, 'neck_01').z + .04
    h = CUP.ray_cast(V(x, y, z0), V(0, 0, -1), 1.); return h[0] if h[0] is not None else V(x, y, z0 - .1)
s_skin = [o for o in RIG['FeySatyr'][1] if o.data.materials and mat(o) == 'Skin'][0]
_names = {g.index: g.name for g in s_skin.vertex_groups}
HAND_V = {sd: [v.index for v in s_skin.data.vertices if sum(g.weight for g in v.groups if _names.get(g.group, '').endswith(f'_{sd}') and
                                                          _names.get(g.group, '').startswith(ARM_BONES[2:])) > .5] for sd in 'lr'}
TIP_V = {sd: [v.index for v in s_skin.data.vertices if sum(g.weight for g in v.groups if _names.get(g.group, '').startswith(('index_03', 'middle_03', 'ring_03', 'pinky_03'))
                                                         and _names.get(g.group, '').endswith(f'_{sd}')) > .5] for sd in 'lr'}
PALM_V = {sd: [v.index for v in s_skin.data.vertices if sum(g.weight for g in v.groups if _names.get(g.group, '') == f'hand_{sd}') > .5] for sd in 'lr'}
ARM_V = {sd: [v.index for v in s_skin.data.vertices if sum(g.weight for g in v.groups if _names.get(g.group, '') in (f'upperarm_{sd}', f'lowerarm_{sd}')) > .5] for sd in 'lr'}
def posed_points(o, idx):
    dg = bpy.context.evaluated_depsgraph_get(); e = o.evaluated_get(dg); me = e.to_mesh(); W = o.matrix_world
    pts = [W @ me.vertices[i].co for i in idx]; e.to_mesh_clear(); return pts
def under(tree, p):
    """How far p is under the surface above it, if it is inside (a ray up meets the surface from inside); else 0.
    (The nearest face's normal misleads by a garment's edge: a point over the vest's armhole read as 7 cm in.)"""
    h = tree.ray_cast(p, V(0, 0, 1), .5)
    return h[3] if h[0] is not None and h[1].z > 0. else 0.
def hand_depth(sd):
    bpy.context.view_layer.update()
    return -max(under(CUP, p) for p in posed_points(s_skin, HAND_V[sd]))
HAND_T = {}
for side, sgn in (('l', 1.), ('r', -1.)):
    a_, b = rest(C, f'clavicle_{side}'), rest(C, f'clavicle_{side}', True)
    q = V(b.x, b.y * 1.02, b.z)   # (the cap of the shoulder, over the joint: well out from the neck and the helmet)
    Lh = bone_len(S, f'hand_{side}')
    xw = q.x - .075; Pw, Pk = down(xw, q.y), down(xw + Lh, q.y)
    wrist = empty(f's_hand_{side}', Pw + V(0, 0, .035)); knuck = empty(f's_knuck_{side}', Pk + V(0, 0, .022))
    up = empty(f's_palm_up_{side}', Pk + V(0, 0, .4))
    for e in (wrist, knuck, up): on_bone(e, C, 'spine_03')   # (not the collarbone: it rolls when he gestures)
    elbow = empty(f's_elbow_{side}', SEAT + V(.02, .5 * sgn, .2)); on_bone(elbow, C, 'h_front')
    ik(S, f'lowerarm_{side}', wrist, elbow)
    c = S.pose.bones[f'hand_{side}'].constraints.new('DAMPED_TRACK'); c.target = knuck; c.track_axis = 'TRACK_Y'
    lt = S.pose.bones[f'hand_{side}'].constraints.new('LOCKED_TRACK'); lt.target = up; lt.lock_axis = 'LOCK_Y'
    best = None
    fingers(S, side, ('index', 'middle', 'ring', 'pinky'), .55, 0, .25)   # (curled as they will be)
    for ax in ('TRACK_X', 'TRACK_NEGATIVE_X', 'TRACK_Z', 'TRACK_NEGATIVE_Z'):   # (which way the back of the hand faces: up)
        lt.track_axis = ax; dpt = hand_depth(side)
        tips = posed_points(s_skin, TIP_V[side]); palm = posed_points(s_skin, PALM_V[side])
        k = sum(p.z for p in tips) / len(tips) - sum(p.z for p in palm) / len(palm)   # (palm down: the fingers curl down over him)
        if best is None or k < best[2]: best = (ax, dpt, k)
    lt.track_axis = best[0]
    for _ in range(4):   # lift him off until no part of the hand is inside (5 mm over the deepest)
        dpt = hand_depth(side)
        if dpt >= 0.: break
        for e in (wrist, knuck, up): e.matrix_world = Matrix.Translation(V(0, 0, .004 - dpt)) @ e.matrix_world
    for e in (wrist, knuck, up): e.matrix_world = Matrix.Translation(V(0, 0, .01)) @ e.matrix_world   # (the curled fingertips' room as he sways)
    HAND_T[side] = (wrist, knuck, up)
    print('CHUCK_TRIO_HAND_AT', side, 'clav', tuple(round(x, 2) for x in a_), tuple(round(x, 2) for x in b), 'q', tuple(round(x, 2) for x in q), 'Pw', tuple(round(x, 2) for x in Pw), 'Pk', tuple(round(x, 2) for x in Pk), 'hand', tuple(round(x, 2) for x in S.matrix_world @ S.pose.bones[f'hand_{side}'].head), 'neck', tuple(round(x, 2) for x in rest(C, 'neck_01')))
    print('CHUCK_TRIO_SATYR_HAND', side, best[0], 'clear_cm', round(hand_depth(side) * 100, 1),
          'arm_in_centaur', sum(under(CUP, p) > .01 for p in posed_points(s_skin, ARM_V[side])), 'of', len(ARM_V[side]))

# The gnome on the satyr's shoulders, piggyback: his seat on the shoulders behind the neck, his thighs forward over
# them, knees in front of the collarbones, his shins hanging down the satyr's chest; his left hand on the left horn.
SATYR_BODY = [o for o in RIG['FeySatyr'][1] if o.data.materials and mat(o) not in ('GoatFur', 'Hero_horn', 'Hero_leaf', 'Hero_petal', 'Hero_bloom', 'Hero_reed', 'Lash', 'Brow', 'Eye', 'Hero_coat_dark', 'Hero_hoof', 'Hero_goat_hoof')]
SAT = tree_of([o for o in SATYR_BODY if mat(o) == 'Skin'], S_OFF, skip_arms=True)   # (the skin alone: a garment's inner face reads as inside)
S_FLIP = -1. if signed(SAT, S_OFF @ rest(S, 'spine_02')) > 0 else 1.
NECK = S_OFF @ rest(S, 'neck_01')
tops = []
for y in (.075, -.075):
    hit = SAT.ray_cast(V(NECK.x - .02, y, NECK.z + .6), V(0, 0, -1), 1.)
    if hit[0] is not None: tops.append(hit[0].z)
sh_top = sum(tops) / len(tops) if tops else NECK.z
hit = SAT.ray_cast(V(NECK.x - .6, 0., sh_top + .04), V(1, 0, 0), 1.)
neck_back = hit[0].x if hit[0] is not None else NECK.x - .06
G_PELVIS = rest(G, 'pelvis')
gskin = [o for o in RIG['FeyGnome'][1] if mat(o) in ('Trousers', 'Skin')]
crotch = min(((o.matrix_world @ v.co).z for o in gskin for v in o.data.vertices if abs((o.matrix_world @ v.co).y) < .025 and (o.matrix_world @ v.co).z < G_PELVIS.z), default=G_PELVIS.z - .06)
G_SEAT = V(neck_back - .03, 0., sh_top + (G_PELVIS.z - crotch) + .01)   # (his crotch against the back of the neck)
G_OFF = Matrix.Translation(G_SEAT - G_PELVIS)
print('CHUCK_TRIO_GNOME_SEAT', tuple(round(x, 3) for x in G_SEAT), 'shoulders', round(sh_top, 3), 'neck_back', round(neck_back, 3), 'seat_depth', round(G_PELVIS.z - crotch, 3))
g_root.matrix = G_OFF @ g_root_rest
bpy.context.view_layer.update()
G_LEGS = {}
clav = {s: S_OFF @ rest(S, f'clavicle_{s}', True) for s in 'lr'}
for side, sgn in (('l', 1.), ('r', -1.)):
    hip = G_OFF @ rest(G, f'thigh_{side}')
    L1, L2, L3 = (bone_len(G, f'{b}_{side}') for b in ('thigh', 'calf', 'foot'))
    over = lambda d, c: d.z * .5 - d.x * 1.0 + 3. * max(0., c - .015) + 2. * abs(d.y - sgn * .25)   # (forward over the shoulder, lying on it)
    d1, c1 = reach(SAT, hip, L1, .07, .06, over, sgn, t0=.25, allow=.005, flip=S_FLIP, phis=range(-10, 61, 5))
    knee = hip + d1 * L1
    d2, c2 = reach(SAT, knee, L2, .055, .05, lambda d, c: d.z - d.x * .15 + 3. * max(0., c - .01), sgn, t0=.1, allow=.015, flip=S_FLIP)
    ankle = knee + d2 * L2
    G_LEGS[side] = (hip, knee, ankle)
    print('CHUCK_TRIO_GNOME_LEG', side, 'clear_cm', round(c1 * 100, 1), round(c2 * 100, 1), 'knee', tuple(round(x, 2) for x in knee), 'ankle', tuple(round(x, 2) for x in ankle))
    t_ank = empty(f'g_ankle_{side}', ankle); on_bone(t_ank, S, 'spine_03')
    t_knee = empty(f'g_knee_{side}', knee + (knee - (hip + ankle) * .5).normalized() * .4); on_bone(t_knee, S, 'spine_03')
    ik(G, f'calf_{side}', t_ank, t_knee)
    t_toe = empty(f'g_toe_{side}', ankle + V(.6, 0, -.55).normalized() * L3); on_bone(t_toe, S, 'spine_03')
    c = G.pose.bones[f'foot_{side}'].constraints.new('DAMPED_TRACK'); c.target = t_toe; c.track_axis = 'TRACK_Y'
for side in 'lr':
    a, e = fit_pole(G, f'calf_{side}', f'calf_{side}', G_LEGS[side][1])
    print('CHUCK_TRIO_GNOME_POLE', side, a, 'knee_err_cm', round(e * 100, 1))
horn = empty('g_horn', S_OFF @ rest(S, 'head').lerp(S_OFF @ rest(S, 'head', True), .8) + V(-.01, .085, .005)); on_bone(horn, S, 'head')   # (the left horn's crown)
helbow = empty('g_elbow_l', G_OFF @ rest(G, 'upperarm_l') + V(-.05, .25, -.1)); on_bone(helbow, S, 'spine_03')
ik(G, 'lowerarm_l', horn, helbow)
point = empty('g_point'); pelbow = empty('g_elbow_r')
ik(G, 'lowerarm_r', point, pelbow)


# ------------------------------------------------------------------ author every frame
scene.frame_start, scene.frame_end = 0, N
c_root = C.pose.bones['Root']; root_rest = C.data.bones['Root'].matrix_local.copy()
s_root = S.pose.bones['Root']; s_root_rest = S.data.bones['Root'].matrix_local.copy()
g_root = G.pose.bones['Root']; g_root_rest = G.data.bones['Root'].matrix_local.copy()
AIMS = []
for f in range(N + 1):
    t = f / FPS; scene.frame_set(f)
    s, w = walked(t); p, h = path_at(s); Rh = Matrix.Rotation(h, 3, 'Z')
    gait = s / STRIDE * 2 * math.pi
    # --- the centaur
    c_root.matrix = Matrix.Translation(p) @ Matrix.Rotation(h, 4, 'Z') @ root_rest
    c_root.keyframe_insert('location', frame=f); c_root.keyframe_insert('rotation_quaternion', frame=f)
    bob = w * .016 * math.cos(2 * gait); roll = w * .03 * math.sin(gait); pitch = w * .018 * math.sin(2 * gait + .6)
    shift = (1 - w) * .02 * math.sin(W(5) * t)
    pb = C.pose.bones['h_front']; pb.location = V(0, 0, bob); pb.keyframe_insert('location', frame=f)
    key_quat(pb, about(C, 'h_front', V(1, 0, 0), roll + shift) @ about(C, 'h_front', V(0, 1, 0), pitch), f)
    key_quat(C.pose.bones['h_rear'], about(C, 'h_rear', V(1, 0, 0), -roll * .6), f)
    for i in range(1, 5):
        sw = .1 * math.sin(W(12) * t - i * .7) * (1 + w) + (1 - w) * .45 * max(0., math.sin(W(4) * t)) ** 8 * math.sin(W(90) * t - i)
        key_quat(C.pose.bones[f'h_tail{i}'], about(C, f'h_tail{i}', V(0, 0, 1), sw) @ about(C, f'h_tail{i}', V(0, 1, 0), .04 * math.sin(W(7) * t + i)), f)
    for leg, (ft, hf, pole) in LEG_T.items():
        fet, hoof = leg_targets(leg, t)
        ft.location = fet; hf.location = hoof
        pole.location = fet + V(0, 0, .5) + Rh @ V(.7 if leg[0] == 'f' else -.7, 0, 0)
        for o in (ft, hf, pole): o.keyframe_insert('location', frame=f)
    # his human half: breathing, looking about standing, arms down and easy, fists loose
    for b, a in (('spine_01', .012), ('spine_02', .016), ('spine_03', .02)):
        key_quat(C.pose.bones[b], about(C, b, V(0, 1, 0), a * math.sin(W(20) * t)) @ about(C, b, V(0, 0, 1), (1 - w) * .05 * math.sin(W(2) * t + 1)), f)
    look = (1 - w) * (.42 * math.sin(W(2) * t) + .18 * math.sin(W(5) * t + 1))
    key_quat(C.pose.bones['neck_01'], about(C, 'neck_01', V(0, 0, 1), look * .4), f)
    key_quat(C.pose.bones['head'], about(C, 'head', V(0, 0, 1), look * .6) @ about(C, 'head', V(0, 1, 0), .04 * math.sin(W(3) * t)), f)
    for side, sgn in (('l', 1.), ('r', -1.)):
        swing = .05 * w * math.sin(gait + (0. if side == 'l' else math.pi))
        key_quat(C.pose.bones[f'upperarm_{side}'], about(C, f'upperarm_{side}', V(1, 0, 0), -sgn * math.radians(40)) @ about(C, f'upperarm_{side}', V(0, 1, 0), math.radians(-6) + swing), f)
        key_quat(C.pose.bones[f'lowerarm_{side}'], about(C, f'lowerarm_{side}', V(0, 1, 0), math.radians(-16)), f)
        fingers(C, side, ('index', 'middle', 'ring', 'pinky'), 1.0, f, .45)
    bpy.context.view_layer.update()
    # --- the satyr: his Root carried by the centaur's h_front (his rest moved onto the saddle)
    Mf = C.matrix_world @ C.pose.bones['h_front'].matrix
    carry = Mf @ C_FRONT_REST.inverted() @ S_OFF
    s_root.matrix = carry @ s_root_rest
    s_root.keyframe_insert('location', frame=f); s_root.keyframe_insert('rotation_quaternion', frame=f)
    laugh = max(0., math.sin(W(4) * t)) ** 2 * (.5 + .5 * math.sin(W(110) * t))
    key_quat(S.pose.bones['spine_01'], about(S, 'spine_01', V(0, 1, 0), math.radians(6) + .04 * laugh), f)
    key_quat(S.pose.bones['spine_03'], about(S, 'spine_03', V(0, 1, 0), -.05 * laugh) @ about(S, 'spine_03', V(0, 0, 1), .07 * math.sin(W(3) * t)), f)
    key_quat(S.pose.bones['head'], about(S, 'head', V(0, 1, 0), -.14 * laugh - .04) @ about(S, 'head', V(0, 0, 1), .3 * math.sin(W(3) * t + 2)), f)
    for side in ('l', 'r'): fingers(S, side, ('index', 'middle', 'ring', 'pinky'), .55, f, .25)
    bpy.context.view_layer.update()
    # --- the gnome: his Root carried by the satyr's chest
    Mc = S.matrix_world @ S.pose.bones['spine_03'].matrix
    g_carry = Mc @ S_CHEST_REST.inverted() @ G_OFF
    g_root.matrix = g_carry @ g_root_rest
    g_root.keyframe_insert('location', frame=f); g_root.keyframe_insert('rotation_quaternion', frame=f)
    if w > .3: aim = Rh @ V(1., .05, .05)
    else:
        a = .95 * math.sin(W(3) * t) + .3 * math.sin(W(7) * t); aim = Matrix.Rotation(h + a, 3, 'Z') @ V(1., 0., .15)
    aim.normalize(); AIMS.append(aim)
    sh = g_carry @ (G.matrix_world @ G.data.bones['upperarm_r'].head_local)
    arm_len = (rest(G, 'upperarm_r') - rest(G, 'hand_r')).length
    point.location = sh + aim * arm_len * 1.05; pelbow.location = sh + aim * arm_len * .5 + V(0, 0, -.25) + Rh @ V(0, -.15, 0)
    for o in (point, pelbow): o.keyframe_insert('location', frame=f)
    yaw = math.atan2(aim.y, aim.x) - h; yaw = (yaw + math.pi) % (2 * math.pi) - math.pi
    key_quat(G.pose.bones['spine_03'], about(G, 'spine_03', V(0, 0, 1), yaw * .25), f)
    key_quat(G.pose.bones['head'], about(G, 'head', V(0, 0, 1), yaw * .45) @ about(G, 'head', V(0, 1, 0), .05 * math.sin(W(9) * t)), f)
    fingers(G, 'r', ('middle', 'ring', 'pinky'), 1.3, f, .8)
    fingers(G, 'l', ('index', 'middle', 'ring', 'pinky'), 1.15, f, .6)
print('CHUCK_TRIO_AUTHORED', N + 1)


# ------------------------------------------------------------------ bake, export
def bake(rig):
    bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='POSE'); bpy.ops.pose.select_all(action='SELECT')
    bpy.ops.nla.bake(frame_start=0, frame_end=N, only_selected=False, visual_keying=True, clear_constraints=True, use_current_action=True, bake_types={'POSE'})
    bpy.ops.object.mode_set(mode='OBJECT')


for rig in (C, S, G): bake(rig)
print('CHUCK_TRIO_BAKED')


def posed_tree(objs):
    dg = bpy.context.evaluated_depsgraph_get(); verts, polys = [], []
    for o in objs:
        e = o.evaluated_get(dg); me = e.to_mesh(); W = o.matrix_world; base = len(verts)
        verts += [W @ v.co for v in me.vertices]; polys += [[base + i for i in p.vertices] for p in me.polygons]; e.to_mesh_clear()
    return BVHTree.FromPolygons(verts, polys), verts


# How far the riders' legs cut into what they sit on, posed, at a few moments (standing, walking, standing).
for t in (4., 14., 26., 38.):
    scene.frame_set(int(t * FPS))
    horse, _ = posed_tree(coat)
    _, legs = posed_tree([o for o in RIG['FeySatyr'][1] if mat(o) == 'Hero_coat_dark'])
    sd = [signed(horse, p, H_FLIP) for p in legs]
    sat, _ = posed_tree([o for o in SATYR_BODY if mat(o) == 'Skin'])
    _, gl = posed_tree([o for o in RIG['FeyGnome'][1] if mat(o) in ('Trousers', 'Boots')])
    gd = [signed(sat, p, S_FLIP) for p in gl]
    cup, _ = posed_tree(CUP_OBJS)
    hd = [-under(cup, p) for sd_ in 'lr' for p in posed_points(s_skin, HAND_V[sd_] + ARM_V[sd_])]
    print('CHUCK_TRIO_CLEAR', f't={t:.0f}', 'satyr_in_horse', sum(d < -.02 for d in sd), f'deepest_cm={min(sd) * 100:.1f}',
          'gnome_in_satyr', sum(d < -.02 for d in gd), f'deepest_cm={min(gd) * 100:.1f}',
          'satyr_arms_in_centaur', sum(d < -.005 for d in hd), f'deepest_cm={min(hd) * 100:.1f}', f'of={len(sd)},{len(gl)},{len(hd)}')


def export(name):
    rig, meshes = RIG[name]
    others = [r for n, (r, _) in RIG.items() if n != name]
    for i, r in enumerate(others): r.name = f'Other{i}'
    rig.name = 'HumanRig'
    out = FEY / name / 'Anim'; out.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    for o in [rig] + meshes: o.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(out / f'AS_{name}_Mill.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'}, **FBX_EXPORT)
    rig.name = f'Rig_{name}'
    for n, (r, _) in RIG.items():
        if n != name: r.name = f'Rig_{n}'
    print('CHUCK_TRIO_CLIP', name)


for name in RIG: export(name)
(FEY / 'clip.json').write_text(json.dumps({'loop_s': LOOP, 'fps': FPS, 'circle_m': [CENTRE.x, CENTRE.y, R], 'walks_s': WALKS, 'seat_m': list(SEAT),
                                           'note': 'All three clips start together at the same place; the centaur Root carries them round the circle.'}, indent=1) + '\n', encoding='utf-8')


# ------------------------------------------------------------------ review
if REVIEW:
    s = scene; s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.light = 'STUDIO'; s.display.shading.color_type = 'MATERIAL'; s.display.shading.show_cavity = True
    for m in bpy.data.materials:
        n = m.name
        m.diffuse_color = ((.62, .44, .34, 1) if n == 'Skin' else (.45, .25, .12, 1) if 'coat' in n else (.35, .05, .4, 1) if 'blanket' in n else (.05, .2, .05, 1) if 'felt' in n
                           else (.08, .06, .05, 1) if n in ('HorseHair', 'GoatFur') else (.6, .55, .45, 1) if 'horn' in n else (.3, .25, .2, 1))
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam); s.camera = cam; cam.data.lens = 35
    s.render.resolution_x, s.render.resolution_y = 1000, 800
    for tag, t, d in (('stand', 4., V(1.2, -1, .35)), ('walk', 14., V(.9, -1.2, .3)), ('walk_side', 15.2, V(0, -1, .15)), ('stand2', 26., V(-1, -1, .4)), ('closeup', 5., V(1, -.4, .2)), ('closeup_side', 5., V(0, -1, .1)), ('closeup_back', 5., V(-1, -.5, .3))):
        f = int(t * FPS); s.frame_set(f)
        sp, _ = walked(t); p, h = path_at(sp)
        target = p + Matrix.Rotation(h, 3, 'Z') @ V(-.4, 0, 1.4 if not tag.startswith('closeup') else 2.0)
        dist = 5.5 if not tag.startswith('closeup') else 2.4
        cam.location = target + (Matrix.Rotation(h, 3, 'Z') @ d).normalized() * dist
        cam.rotation_euler = (target - cam.location).to_track_quat('-Z', 'Y').to_euler()
        s.render.filepath = os.path.join(REVIEW, f'trio_{tag}.png'); bpy.ops.render.render(write_still=True)
print('CHUCK_TRIO_DONE')
