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
  * The satyr's Root follows the centaur's h_front bone (his pelvis on the saddle behind the man's hips), his hands reach
    the centaur's shoulders and his hooves its flanks by IK; the gnome's Root follows the satyr's chest (his pelvis on the
    satyr's neck), his boots hang over the satyr's chest, his left hand holds the satyr's left horn, his right arm points.
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


# ------------------------------------------------------------------ the riders
def back_top(objs, x, y):
    from mathutils.bvhtree import BVHTree
    import bmesh
    best = None
    for o in objs:
        bm = bmesh.new(); bm.from_mesh(o.data); bm.transform(o.matrix_world); tree = BVHTree.FromBMesh(bm); bm.free()
        hit = tree.ray_cast(V(x, y, 4.), V(0, 0, -1), 10.)
        if hit[0] is not None: best = max(best if best is not None else -9., hit[0].z)
    return best


cover = [o for o in RIG['FeyCentaur'][1] if o.data.materials and o.data.materials[0].name in ('Hero_blanket', 'Hero_coat')]
SEAT_X = -.42
top = back_top(cover, SEAT_X, 0.)
S_PELVIS = rest(S, 'pelvis')
SEAT = V(SEAT_X, 0., top + .075)                        # where the satyr's pelvis sits
S_OFF = Matrix.Translation(SEAT - S_PELVIS)              # his rest moved onto the saddle
C_FRONT_REST = C.matrix_world @ C.data.bones['h_front'].matrix_local
print('CHUCK_TRIO_SEAT', tuple(round(x, 3) for x in SEAT), 'back', round(top, 3))
# the gnome's pelvis on the satyr's neck, a little back, in the satyr's seated rest
G_PELVIS = rest(G, 'pelvis')
NECK = S_OFF @ rest(S, 'neck_01')
G_SEAT = NECK + V(-.13, 0., .035)
G_OFF = Matrix.Translation(G_SEAT - G_PELVIS)
S_CHEST_REST = S_OFF @ (S.matrix_world @ S.data.bones['spine_03'].matrix_local)

# Targets carried by the bones they rest on (placed in the riders' seated rest).
for side, sgn in (('l', 1.), ('r', -1.)):
    hand = empty(f's_hand_{side}', rest(C, f'clavicle_{side}', True) + V(-.06, -.035 * sgn, .075)); on_bone(hand, C, f'clavicle_{side}')
    elbow = empty(f's_elbow_{side}', SEAT + V(-.05, .55 * sgn, .25)); on_bone(elbow, C, 'h_front')
    ik(S, f'lowerarm_{side}', hand, elbow)
    hoof = empty(f's_hoof_{side}', V(SEAT_X + .18, .3 * sgn, top - .66)); on_bone(hoof, C, 'h_front')
    knee = empty(f's_knee_{side}', V(SEAT_X + .9, .52 * sgn, top - .05)); on_bone(knee, C, 'h_front')
    ik(S, f'calf_{side}', hoof, knee)
    boot = empty(f'g_boot_{side}', S_OFF @ rest(S, 'spine_03') + V(.19, .12 * sgn, -.04)); on_bone(boot, S, 'spine_03')
    gknee = empty(f'g_knee_{side}', S_OFF @ rest(S, 'spine_03') + V(.5, .2 * sgn, .25)); on_bone(gknee, S, 'spine_03')
    ik(G, f'calf_{side}', boot, gknee)
horn = empty('g_horn', S_OFF @ rest(S, 'head', True) + V(-.035, .075, -.015)); on_bone(horn, S, 'head')
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
    for tag, t, d in (('stand', 4., V(1.2, -1, .35)), ('walk', 14., V(.9, -1.2, .3)), ('walk_side', 15.2, V(0, -1, .15)), ('stand2', 26., V(-1, -1, .4)), ('closeup', 5., V(1, -.4, .2))):
        f = int(t * FPS); s.frame_set(f)
        sp, _ = walked(t); p, h = path_at(sp)
        target = p + Matrix.Rotation(h, 3, 'Z') @ V(-.4, 0, 1.4 if tag != 'closeup' else 2.2)
        dist = 5.5 if tag != 'closeup' else 2.4
        cam.location = target + (Matrix.Rotation(h, 3, 'Z') @ d).normalized() * dist
        cam.rotation_euler = (target - cam.location).to_track_quat('-Z', 'Y').to_euler()
        s.render.filepath = os.path.join(REVIEW, f'trio_{tag}.png'); bpy.ops.render.render(write_still=True)
print('CHUCK_TRIO_DONE')
