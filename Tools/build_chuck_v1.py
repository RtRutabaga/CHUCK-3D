"""Blender 4.5 LTS: Chuck rig v1 source builder (docs/RIG-CONTRACT-V1.md).

blender --background --python Tools/build_chuck_v1.py

Separate entry point from the legacy generator. It reuses the latest authored
geometry by running Tools/build_chuck_model.py with CHUCK_GEOMETRY_ONLY=True
(which skips every legacy export), replaces the legs and paws for the accepted
41-bone table (SourceAssets/Chuck/rig_proposal.json), skins one mesh with
graded weights and writes only SourceAssets/Chuck/V1/*:
  Chuck_V1.blend, SK_Chuck.fbx, Animations/AS_Chuck_<Clip>.fbx,
  Animations/manifest.json, rig_v1_metadata.json
The legacy SourceAssets/Chuck exports and 14-bone rig are never touched.
"""
import json
import math
import runpy
import sys
from pathlib import Path
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Tools'))
from chuck_v1_pose import Poser  # noqa: E402
import chuck_v1_shape as SHAPE  # noqa: E402
from chuck_v1_shape import Z  # noqa: E402

V1 = ROOT / 'SourceAssets' / 'Chuck' / 'V1'
ANIM = V1 / 'Animations'
ANIM.mkdir(parents=True, exist_ok=True)
# v1.1 shape amendment: the accepted v1.0 table mapped through chuck_v1_shape
# (longer legs, narrower torso/arms, head compressed under the 65 cm ear tip).
TABLE = SHAPE.load_effective_table(ROOT)
G = runpy.run_path(str(ROOT / 'Tools/build_chuck_model.py'), init_globals={'CHUCK_GEOMETRY_ONLY': True})
chain_tube, smoothstep, fur_surface, paw_parts = G['chain_tube'], G['smoothstep'], G['fur_surface'], G['paw_parts']
head_axis_z, jacket_point = G['head_axis_z'], G['jacket_point']
scene = bpy.context.scene
FPS = 30
scene.render.fps = FPS

def H(name): return Vector(TABLE[name]['head'])
def T(name): return Vector(TABLE[name]['tail'])
def label(obj): return obj.name.split('.')[0]
def side_of(p): return 'L' if p.y > 0 else 'R'

# ---------------------------------------------------------------- geometry
# The legacy study geometry is authored in v1.0 source space; map every part
# through the same shape function as the bone table (world space, since some
# parts keep an object location).
for obj in [o for o in scene.objects if o.type in ('MESH', 'CURVE')]:
    kind = SHAPE.kind_of(label(obj))
    mw = obj.matrix_world; inv = mw.inverted()
    def moved(co):
        return inv @ Vector(SHAPE.point(mw @ Vector(co[:3]), kind))
    if obj.type == 'MESH':
        for v in obj.data.vertices:
            v.co = moved(v.co)
        obj.data.update()
    else:
        for spline in obj.data.splines:
            for bp in spline.bezier_points:
                bp.co = moved(bp.co)
            for pt in spline.points:
                pt.co = (*moved(pt.co), pt.co[3])
ear_top = max((o.matrix_world @ v.co).z for o in scene.objects if label(o) == 'Ear' for v in o.data.vertices)
assert abs(ear_top - 65.) < 1e-3, f'ear tip must stay at 65 cm, got {ear_top}'
# Whiskers at 0.028 cm radius alias into dotted lines at game distance in
# Unreal; v1 uses 0.035 cm, finer than before for the more numerous whiskers of
# the snout/fur target (legacy output unchanged).
for obj in [o for o in scene.objects if label(o) == 'Whisker']:
    obj.data.bevel_depth = .035
for obj in [o for o in scene.objects if label(o) in ('Leg', 'LegFur')]:
    bpy.data.objects.remove(obj, do_unlink=True)

def leg_radius(t):
    # Rat haunch (goal images): full thigh that stays thick to the knee
    # (t ~ .46), peaking just below the jacket hem, then a slimmer shin.
    return (4.0 + .6 * smoothstep(t, 0., .14) - .5 * smoothstep(t, .14, .38)
            - 1.6 * smoothstep(t, .36, .6) - .75 * smoothstep(t, .6, 1.))

PAW_ORIGIN = {}
for s, y in (('L', 1), ('R', -1)):
    hip, knee, hock = H(f'thigh_{s}'), H(f'calf_{s}'), H(f'foot_{s}')
    # Knee-forward plantigrade leg; its domed end sinks into the heel mound.
    chain_tube('Leg', [hip + Vector((0, 0, 1.2)), knee, hock, hock + Vector((-.3, 0, -1.3))],
               leg_radius, 'Fur', caps=(.9, .6), rings=28)
    # Paw placed so the heel mound surrounds the hock, the toes start at the
    # toes_ head (ball) and the sole touches Z=0.
    PAW_ORIGIN[s] = Vector((-.8, H(f'foot_{s}').y, 1.96))  # follows the leg spread
    paw_parts(PAW_ORIGIN[s])
for i, part in enumerate([o for o in scene.objects if label(o) == 'Leg']):
    fur_surface(part, 'LegFur', 'Fur', 1800, 131 + i, (.22, .55), (0, 0, -.8), lambda p, n: p.z < Z(21))

# ---------------------------------------------------------------- weights
def seg(p, a, b):
    d = b - a; length = d.length; u = d / length
    along = (p - a).dot(u)
    return along, length, max(0., -along, along - length), (p - (a + u * max(0., min(length, along)))).length

def chain_weights(p, bones, blend=2.5, falloff=1.2):
    """Graded weights along a bone chain: full weight inside a bone's span,
    fading over `blend` cm past its ends, and fading with distance beyond the
    nearest bone (`falloff` cm) so a vertex that merely projects inside a far
    bone's span (e.g. the ball of the paw vs the calf) is not driven by it."""
    geo = {b: seg(p, H(b), T(b)) for b in bones}
    nearest = min(g[3] for g in geo.values())
    ws = {b: (1 - smoothstep(g[2], 0., blend)) * math.exp(-(g[3] - nearest) / falloff) for b, g in geo.items()}
    total = sum(ws.values())
    if total < 1e-6:
        return {min(bones, key=lambda b: seg(p, H(b), T(b))[3]): 1.}
    return {b: w / total for b, w in ws.items() if w / total > 1e-5}

def mix(*pairs):
    out = {}
    for amount, weights in pairs:
        for b, w in weights.items():
            out[b] = out.get(b, 0.) + amount * w
    return out

SPINE = ['pelvis', 'spine_01', 'spine_02', 'chest', 'neck']

def thigh_pull(p, strength=.75, radius=(5., 11.5), height=(Z(21.), Z(28.))):
    pull = {}
    for s in 'LR':
        hip, knee = H(f'thigh_{s}'), H(f'calf_{s}')
        _, _, _, r = seg(p, hip, knee)
        pull[f'thigh_{s}'] = strength * (1 - smoothstep(r, *radius)) * (1 - smoothstep(p.z, *height))
    total = sum(pull.values())
    if total > .8:
        pull = {b: w * .8 / total for b, w in pull.items()}
    return pull

def torso_weights(p):
    base = chain_weights(p, SPINE)
    s = side_of(p)
    clav = .5 * smoothstep(abs(p.y), 5. * SHAPE.TORSO_NARROW, 8.5 * SHAPE.TORSO_NARROW) * (1 - smoothstep(abs(p.z - Z(41.5)), 1.5, 4.))
    pull = thigh_pull(p, .6, (4., 8.), (Z(18.), Z(23.)))
    rest = 1 - clav - sum(pull.values())
    return mix((rest, base), (clav, {f'clavicle_{s}': 1.}), (1., pull))

def arm_frame(p):
    s = side_of(p)
    shoulder, elbow = H(f'upperarm_{s}'), H(f'lowerarm_{s}')
    a, length, _, r = seg(p, shoulder, elbow)
    return s, a, length, r

def garment_weights(p):
    s, a, length, r = arm_frame(p)
    chain = smoothstep(a, -2., 2.5) * (1 - smoothstep(r, 3., 6.)) * (1 - smoothstep(a, 4., 9.))
    pull = thigh_pull(p)
    rest = (1 - chain) * (1 - sum(pull.values()))
    return mix((rest, torso_weights(p)), ((1 - sum(pull.values())) * chain, {f'upperarm_{s}': 1.}), (1., pull))

def sleeve_weights(p):
    s, a, length, r = arm_frame(p)
    along = smoothstep(a, -2., 2.5)
    return mix((along, chain_weights(p, [f'upperarm_{s}', f'lowerarm_{s}'])),
               ((1 - along) * .6, {f'clavicle_{s}': 1.}), ((1 - along) * .4, {'chest': 1.}))

def hand_weights(p, lab):
    s = side_of(p)
    if lab == 'Hand':
        return chain_weights(p, [f'lowerarm_{s}', f'hand_{s}'], 1.5)
    digit = f'thumb_{s}' if lab in ('Thumb', 'ThumbClaw') else f'fingers_{s}'
    if lab.endswith('Claw'):
        return {digit: 1.}  # rigid on its digit
    return chain_weights(p, [f'hand_{s}', digit], 1.)

JAWED = ('Head', 'MuzzleLight', 'Mouth', 'CheekFur')

def head_weights(p, lab):
    neck = .6 * (1 - smoothstep(p.z, Z(46.5), Z(49.5))) * (1 - smoothstep(p.x, 2., 5.))
    jaw = 0.
    if lab in JAWED:
        jaw = smoothstep(p.x, 3.5, 7.) * (1 - smoothstep(p.z - Z(head_axis_z(SHAPE.source_x(p.x)) - 1.3), -.6, .6))
    ear = 0.
    if lab == 'Ear':
        ear = smoothstep(p.z, Z(58.5), Z(61.))
    return mix((1 - neck - jaw - ear, {'head': 1.}), (neck, {'neck': 1.}), (jaw, {'jaw': 1.}),
               (ear, {f'ear_{side_of(p)}': 1.}))

def leg_weights(p):
    """Pelvis -> thigh -> calf on the leg tube; around the hock the tube
    grades from calf to foot over about 1.5 cm, and its end (inside the heel
    mound) is fully foot."""
    s = side_of(p)
    a = seg(p, H(f'thigh_{s}'), T(f'thigh_{s}'))[0]
    pelvis = 1 - smoothstep(a, -1.5, 2.5)
    hock = H(f'foot_{s}')
    calf_axis = (hock - H(f'calf_{s}')).normalized()
    beyond = (p - hock).dot(calf_axis)  # +: past the hock along the shin
    foot = smoothstep(beyond, -1.5, .5)
    limb = chain_weights(p, [f'thigh_{s}', f'calf_{s}'])
    return mix((pelvis, {'pelvis': 1.}), ((1 - pelvis) * (1 - foot), limb), ((1 - pelvis) * foot, {f'foot_{s}': 1.}))

def paw_weights(p):
    """The paw (heel mound included) is rigid on foot/toes. The ankle bend
    happens on the fur leg tube, whose end sits inside the heel mound and is
    weighted to the foot too, so the join cannot open."""
    s = side_of(p)
    return chain_weights(p, [f'foot_{s}', f'toes_{s}'], 1.8)

TAIL = [f'tail_{i}' for i in range(6)]
# Tail mesh radius at each bone's head and tail (legacy tail tapers 1.8 -> .13 cm).
TAIL_RADIUS = {b: (1.78 - 1.5 * i / 6, 1.78 - 1.5 * (i + 1) / 6) for i, b in enumerate(TAIL)}
FIELDS = {
    'torso': (('Torso', 'LightChest', 'ChestFur', 'BellyFur'), lambda p, l: torso_weights(p)),
    'garment': (('OpenJacket', 'Zipper', 'ZipperTape', 'Pocket', 'HemStitch', 'BackSeam'), lambda p, l: garment_weights(p)),
    'sleeve': (('Sleeve', 'Cuff'), lambda p, l: sleeve_weights(p)),
    'hand': (('Hand', 'Finger', 'Thumb', 'FingerClaw', 'ThumbClaw'), hand_weights),
    'head': (('Head', 'MuzzleLight', 'Nose', 'EyeLid', 'Eye', 'Mouth', 'Whisker', 'CheekFur', 'Ear'), head_weights),
    'leg': (('Leg', 'LegFur'), lambda p, l: leg_weights(p)),
    'paw': (('Foot', 'Heel', 'Toe', 'Claw'), lambda p, l: paw_weights(p)),
    'tail': (('Tail',), lambda p, l: chain_weights(p, TAIL, 3.)),
}
FIELD_OF = {lab: fn for labels, fn in FIELDS.values() for lab in labels}

parts = [o for o in scene.objects if o.type in ('MESH', 'CURVE')]
unknown = sorted({label(o) for o in parts} - set(FIELD_OF))
assert not unknown, f'Unweighted source parts: {unknown}'
sole = {'L': [], 'R': []}
for part in parts:
    lab = label(part)
    bpy.ops.object.select_all(action='DESELECT'); part.select_set(True)
    bpy.context.view_layer.objects.active = part
    bpy.ops.object.convert(target='MESH')
    part = bpy.context.object
    strands = part.data.attributes.new('chuck_strand', 'BOOLEAN', 'FACE')
    # Strands and zipper teeth are sub-texel; they get a flat per-material island.
    is_strand = lab.endswith('Fur') or lab in ('Whisker', 'Zipper')
    for d in strands.data: d.value = is_strand
    # Geometric fur tufts, removed in the groom variant (SK_Chuck_Groomed).
    tufts = part.data.attributes.new('chuck_tuft', 'BOOLEAN', 'FACE')
    for d in tufts.data: d.value = lab.endswith('Fur')
    groups = {}
    for v in part.data.vertices:
        p = part.matrix_world @ v.co
        if lab in ('Foot', 'Heel', 'Toe', 'Claw'):
            sole[side_of(p)].append((p.copy(), lab))
        weights = FIELD_OF[lab](p, lab)
        total = sum(weights.values())
        for bone, w in weights.items():
            if w / total < 1e-4: continue
            if bone not in groups: groups[bone] = part.vertex_groups.new(name=bone)
            groups[bone].add([v.index], w / total, 'REPLACE')

bpy.ops.object.select_all(action='DESELECT')
for part in parts: part.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
body = bpy.context.object; body.name = 'SK_Chuck'; body.data.name = 'SK_Chuck'
scene.cursor.location = (0, 0, 0)
bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
# UVs: one automatic unwrap channel so textures can be authored later (the
# legacy parts had none). Only real surfaces are unwrapped; fur tufts and
# whiskers (tagged chuck_strand) are parked on one tiny island per material
# along the top edge, since they use flat colour and would otherwise swamp the packing.
import bmesh
t_uv = __import__('time').time()
bpy.context.view_layer.objects.active = body
bpy.ops.object.mode_set(mode='EDIT')
bm = bmesh.from_edit_mesh(body.data)
tag = bm.faces.layers.bool.get('chuck_strand')
for f in bm.faces: f.select_set(not f[tag])
bmesh.update_edit_mesh(body.data)
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=.003, area_weight=0., scale_to_bounds=False)
bm = bmesh.from_edit_mesh(body.data)
uv = bm.loops.layers.uv.active
tag = bm.faces.layers.bool.get('chuck_strand')
strand_faces = 0
for f in bm.faces:
    if not f[tag]:
        # Keep the top 1.5% strip free for the parked strand islands.
        for l in f.loops: l[uv].uv = l[uv].uv * .985
        continue
    if f[tag]:
        strand_faces += 1
        # One tiny island per material along the top edge, so each strand
        # material bakes/samples its own flat colour.
        x0 = .004 + .008 * f.material_index
        for i, l in enumerate(f.loops):
            l[uv].uv = (x0 + .003 * (i == 1), .996 + .003 * (i == 2))
bmesh.update_edit_mesh(body.data)
bpy.ops.object.mode_set(mode='OBJECT')
body.data.uv_layers[0].name = 'UVMap'
body.data.attributes.remove(body.data.attributes['chuck_strand'])
print('CHUCK_V1_UV', round(__import__('time').time() - t_uv, 1), 's; strand faces parked', strand_faces)
# Weights were written as ratios; renormalize defensively after the join.
for v in body.data.vertices:
    total = sum(g.weight for g in v.groups)
    for g in v.groups: g.weight /= total

# ---------------------------------------------------------------- armature
arm_data = bpy.data.armatures.new('ChuckV1')
rig = bpy.data.objects.new('SK_Chuck_Rig', arm_data)
scene.collection.objects.link(rig)
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
for name, bone in TABLE.items():
    eb = arm_data.edit_bones.new(name)
    eb.head, eb.tail, eb.roll = bone['head'], bone['tail'], 0.
    eb.use_deform = bone['deform']; eb.use_connect = False
for name, bone in TABLE.items():
    if bone['parent']: arm_data.edit_bones[name].parent = arm_data.edit_bones[bone['parent']]
bpy.ops.object.mode_set(mode='OBJECT')
body.parent = rig
mod = body.modifiers.new('Chuck v1 skin', 'ARMATURE'); mod.object = rig
for name, bone in TABLE.items():
    if not bone['deform']:
        assert name not in body.vertex_groups, f'helper {name} must not deform'

# Sole markers measured on the actual paws (contract: confirm the proposed
# heel/ball/toe ground positions). Z is the paw's lowest point.
markers = {}
for s, pts in sole.items():
    flesh = [p for p, lab in pts if lab != 'Claw']
    ground = min(p.z for p in flesh)
    low = [p for p in flesh if p.z < ground + .35]
    heel = min(low, key=lambda p: p.x).x; toe = max(low, key=lambda p: p.x).x
    ball = H(f'toes_{s}').x
    # Markers sit on the paw centreline at ground height; x values measured.
    markers[s] = {k: [round(x, 3), PAW_ORIGIN[s].y, 0.0] for k, x in (('heel', heel), ('ball', ball), ('toe', toe))}
    markers[s]['lowest_z'] = round(ground, 3)

# ---------------------------------------------------------------- clips
poser = Poser(rig)
CLIPS = []

def author(name, frames, pose_fn, loop, extra):
    action = bpy.data.actions.new(f'AS_Chuck_{name}')
    rig.animation_data_create(); rig.animation_data.action = action
    reach = 0.
    for f in range(frames):
        poser.reset()
        reach = max(reach, pose_fn(f / frames if loop else f / max(1, frames - 1), f))
        poser.clear_ground(TAIL, TAIL_RADIUS, .9)  # margin covers mesh sag between joints
        poser.key_all(f)
    rig.animation_data.action = None
    entry = {'name': name, 'file': f'Animations/AS_Chuck_{name}.fbx', 'fps': FPS,
             'first_frame': 0, 'last_frame': frames - 1, 'frame_count': frames,
             # Loops: frames/fps (last frame interpolates to frame 0); one-shots: last frame time.
             'duration_s': round((frames if loop else frames - 1) / FPS, 4), 'loop': loop,
             'loop_note': 'no duplicate endpoint sample; last frame interpolates to frame 0' if loop else '',
             'root_motion': False, 'max_leg_reach_ratio': round(reach, 4)}
    entry.update(extra)
    CLIPS.append((action, entry))

NEUTRAL_BALL = {s: H(f'toes_{s}') for s in 'LR'}

def plant_rest(pelvis_drop=0.):
    r = 0.
    for s in 'LR':
        r = max(r, poser.leg(s, NEUTRAL_BALL[s]))
    return r

def curl(s, degrees):
    """Curl the four fingers toward the palm, which faces the thigh (medial):
    about X, sign by side (_L is +Y, so its palm faces -Y)."""
    poser.rotate(f'fingers_{s}', 'X', -degrees if s == 'L' else degrees)

# ---- body language (user direction 2026-09-27): "this aplomb chain smoking
# rat in a kick ass jacket saunters down the dock ... cool and collected".
# Walk: a saunter with a touch of swagger. Speed from dynamic similarity:
# Froude Fr = v^2/(g L) ~ 0.25 is the preferred walk for any biped; a saunter
# is about 0.7x that. Chuck's leg (hip 22.5 cm) gives ~62 cm/s (Fr ~0.17),
# with a 31 cm stride and a 0.5 s cycle (the old 95 cm/s, 0.3 s cycle, Fr 0.41,
# read as a scurry). User follow-up: more swagger, brisk, "finna whoop your ass":
# 72 cm/s over a 14-frame cycle (Fr ~0.24, a purposeful walk). Swagger cues:
# large thorax-pelvis relative twist (the kinematic marker of aggressive gait,
# PMC5283505), shoulders dipping into a roll each step (Johnson & Tassinary
# 2007), chest out, arms carried wide with bent elbows, fists, a springy bounce.
WALK = {'speed_cm_s': 72.0, 'period_frames': 14, 'stance_fraction': .6, 'pelvis_drop_cm': .6, 'lift_cm': 2.0}
PERIOD = WALK['period_frames'] / FPS
STANCE_CM = WALK['speed_cm_s'] * PERIOD * WALK['stance_fraction']
TOE_OUT = {'L': 7., 'R': -7.}  # saunter paws turn slightly out (degrees about Z)
# Idle contrapposto: weight on the right leg; the free left paw rests a little
# forward and out, turned out. Transitions start/end on this stance.
IDLE_FOOT = {'L': (1.6, .9, 12.), 'R': (0., -.2, -6.)}  # (dx, dy, heading)
TAU = 2 * math.pi

def carriage(k, w, look=0., breath=0.):
    """Upper body and pelvis for walk intensity k (0 = the aplomb idle stance,
    1 = full saunter) at gait phase angle w. look: head/chest turn (deg)."""
    idle = 1. - k
    lag = TAU * .06    # shoulders trail the hips (overlap)
    mid_l = TAU * .31  # left paw mid-stance
    # Pelvis: sway over the standing paw, dip at contact, twist toward the
    # forward leg, drop on the swing side. Idle: weight on the right leg (its
    # hip up), a little lower so the free knee softens.
    poser.translate('pelvis', (0, .45 * k * math.cos(w - mid_l) - .9 * idle,
                               -WALK['pelvis_drop_cm'] * k - .7 * k * math.cos(2 * w) - .45 * idle - .15 * breath))
    poser.rotate('pelvis', 'Z', -7 * k * math.cos(w))
    poser.rotate('pelvis', 'X', 3.2 * k * math.cos(w - mid_l) - 4 * idle)
    # Chest counter-rotates harder than the hips twist (the swagger), a beat
    # later; shoulders tilt against the hips (contrapposto when standing).
    poser.rotate('spine_02', 'Z', 9 * k * math.cos(w - lag) + look * .25)
    poser.rotate('chest', 'Z', 12 * k * math.cos(w - lag) + look * .2)
    # Rolling shoulder dip toward the standing side each step.
    poser.rotate('chest', 'X', -5 * k * math.cos(w - mid_l - lag) + 3 * idle)
    # Chest up, a slight lean back, breathing in the ribcage.
    poser.rotate('spine_01', 'Y', -3 * k - 2 * idle)
    poser.rotate('spine_02', 'Y', -1. * breath)
    poser.rotate('chest', 'Y', -1.5 - 2 * k - .6 * breath)  # chest out when walking
    # Head steady and chin up: it cancels most of the chest twist.
    poser.rotate('neck', 'Y', .6 * breath)
    poser.rotate('head', 'Z', look * .55 - 11 * k * math.cos(w - lag))  # steady gaze over the twist  # cancels most of the net chest yaw (+5 deg)
    poser.rotate('head', 'Y', -3 - 1. * idle + 2.5 * k + 1.2 * k * math.cos(2 * w - lag))  # chin level, a nod on each step
    # Tail: lazy, trailing sway.
    for i, b in enumerate(TAIL):
        poser.rotate(b, 'Z', 6.5 * k * math.sin(w - .7 * (i + 1)) - look * .08 * (i + 1) - 3 * idle)
    # Arms loose from the shoulder: swing opposite the legs with a lag, carried
    # slightly out from the jacket, elbows bend more on the forward swing, the
    # hands trail. Idle: relaxed hang, elbows soft, the left hand a touch forward.
    for side, sign in (('L', 1), ('R', -1)):
        swing = math.cos(w - lag * 1.5 + (0 if side == 'L' else math.pi))  # +1 = arm back
        poser.rotate(f'upperarm_{side}', 'Y', 22 * k * swing + (-3 if side == 'L' else 1) * idle)
        poser.rotate(f'upperarm_{side}', 'X', sign * (11 * k + 1 * idle))  # arms carried wide
        fwd = max(0., -swing)
        poser.rotate(f'lowerarm_{side}', 'Y', -8 - 14 * k * fwd - 12 * k - (4 if side == 'L' else 2) * idle)  # elbows bent
        poser.rotate(f'hand_{side}', 'Y', 7 * k * math.cos(w - lag * 3 + (0 if side == 'L' else math.pi)))
        # Loose, half-closed hands when walking. A true fist needs finger joints the
        # v1 rig lacks (one fingers_* bone bends all four at the knuckle).
        curl(side, 16 + 18 * k + 4 * k * fwd + (4 if side == 'L' else 0) * idle)

def plant_idle():
    r = 0.
    for side in 'LR':
        dx, dy, h = IDLE_FOOT[side]
        r = max(r, poser.leg(side, NEUTRAL_BALL[side] + Vector((dx, dy, 0)), heading=h))
    return r

def idle(phase, f):
    """Aplomb hold (4 s): contrapposto on the right leg, chest up, chin up, a
    slow look off to the side and back, one unhurried breath per 2 s and a small
    chin lift at 60% as if drawing on the cigarette."""
    w = TAU * phase
    breath = math.sin(2 * w)
    look = 9 * math.sin(w + .6) + 3 * math.sin(3 * w)
    draw = math.exp(-((phase - .6) / .05) ** 2)
    carriage(0., 0., look, breath)
    poser.rotate('head', 'Y', -2.5 * draw)
    for side, sign in (('L', 1), ('R', -1)):
        poser.rotate(f'ear_{side}', 'X', sign * 1.2 * math.sin(4 * w + (0 if side == 'L' else 1.3)))
        poser.rotate(f'upperarm_{side}', 'Y', -.8 * breath)
    poser.update()
    r = plant_idle()
    for side in 'LR': poser.hand_goal(side)
    return r

def foot_cycle(phase):
    """(forward offset of the ball from neutral, lift, foot pitch, toe pitch)."""
    st = WALK['stance_fraction']
    half = STANCE_CM / 2
    if phase < st:
        u = phase / st
        offset = half - STANCE_CM * u  # planted: moves back at walking speed
        roll = smoothstep(u, .7, 1.)  # heel lifts around the ball at push-off
        return offset, 0., 24 * roll, 0.  # toes stay flat and planted through the roll
    u = (phase - st) / (1 - st)
    ease = smoothstep(u, 0., 1.)
    offset = -half + STANCE_CM * ease
    return offset, WALK['lift_cm'] * math.sin(math.pi * u), 24 * (1 - smoothstep(u, 0., .6)), 10 * math.sin(math.pi * u)

def walk(phase, f):
    w = TAU * phase
    carriage(1., w)
    poser.update()
    feet = {'L': foot_cycle(phase), 'R': foot_cycle((phase + .5) % 1)}
    r = 0.
    for side in 'LR':
        offset, lift, fp, tp = feet[side]
        ball = NEUTRAL_BALL[side] + Vector((offset, 0, lift))
        r = max(r, poser.leg(side, ball, fp, tp, heading=TOE_OUT[side]))
    for side in 'LR': poser.hand_goal(side)
    return r

def stance_intervals(offset_phase):
    st = WALK['stance_fraction']
    a, b = offset_phase % 1, (offset_phase + st) % 1
    spans = [(a, b)] if a < b else [(a, 1.), (0., b)]
    return [[round(x * PERIOD, 4), round(y * PERIOD, 4)] for x, y in spans]

author('Idle', 120, idle, True, {'notes': 'Aplomb hold (4 s): contrapposto on the right leg, chest and chin up, slow look drift, calm breathing, a small chin lift as if drawing on the cigarette.'})
author('WalkLoop', WALK['period_frames'], walk, True, {
    'reference_speed_cm_s': WALK['speed_cm_s'], 'stride_cycle_cm': round(WALK['speed_cm_s'] * PERIOD, 3),
    'stance_travel_cm': round(STANCE_CM, 3), 'stance_fraction': WALK['stance_fraction'],
    'stance_intervals_s': {'foot_L': stance_intervals(0.), 'foot_R': stance_intervals(.5)},
    'events_s': {'foot_L_plant': 0.0, 'foot_R_plant': round(PERIOD / 2, 4),
                 'foot_L_lift': round(WALK['stance_fraction'] * PERIOD, 4),
                 'foot_R_lift': round(((.5 + WALK['stance_fraction']) % 1) * PERIOD, 4)},
    'notes': 'Saunter with a touch of swagger. In place (root fixed); during stance the ball of the planted paw moves backward in component space at exactly the reference speed; heel lifts around the ball in the last 30% of stance (toe roll); paws toe out 7 deg.'})

# ---- transitions, turns and jumps (world-space footstep planner)
# Paws are planned in world space while the capsule (root) moves or turns, so
# a planted paw stays world-locked; component space = Rz(-yaw) (world - travel).
def speed_ramp(t, dur, up):
    f = smoothstep(t / dur, 0., 1.)
    return WALK['speed_cm_s'] * (f if up else 1 - f)

def travel(t, dur, up):
    """Distance covered by a smoothstep speed ramp up to time t (closed form)."""
    u = max(0., min(1., t / dur))
    ramp = u ** 3 - u ** 4 / 2  # integral of smoothstep
    return WALK['speed_cm_s'] * dur * (ramp if up else u - ramp)

# Heel-lift window = last 30% of WalkLoop stance, so transitions match the loop.
ROLL = .3 * WALK['stance_fraction'] * PERIOD

def plan_foot(t, start, steps):
    """start: (x, y, heading). steps: [(lift_s, land_s, (x, y, heading))].
    Returns world ball (x, y, lift), heading, foot pitch, toe pitch."""
    pos = start
    for lift, land, target in steps:
        if t < lift:
            roll = smoothstep(t, lift - ROLL, lift)
            return (pos[0], pos[1], 0.), pos[2], 24 * roll, 0.
        if t < land:
            u = (t - lift) / (land - lift); e = smoothstep(u, 0., 1.)
            x = pos[0] + (target[0] - pos[0]) * e; y = pos[1] + (target[1] - pos[1]) * e
            h = pos[2] + (target[2] - pos[2]) * e
            return ((x, y, WALK['lift_cm'] * math.sin(math.pi * u)), h,
                    24 * (1 - smoothstep(u, 0., .6)), 12 * math.sin(math.pi * u))
        pos = target
    return (pos[0], pos[1], 0.), pos[2], 0., 0.

def stance_from_steps(steps, dur):
    spans, t0 = [], 0.
    for lift, land, _ in steps:
        if t0 >= dur: break
        if lift > t0: spans.append([round(t0, 4), round(min(lift, dur), 4)])
        t0 = land
    if t0 < dur: spans.append([round(t0, 4), round(dur, 4)])
    return spans

def to_component(x, y, dist, yaw):
    c, s = math.cos(-yaw), math.sin(-yaw)
    x -= dist
    return x * c - y * s, x * s + y * c

def pose_planned(t, feet, dist, yaw, k, w, lean=0., look=0.):
    """Carriage at walk intensity k (0 aplomb idle .. 1 saunter) and gait phase
    angle w, then both legs from the planned world feet."""
    carriage(k, w, look)
    if lean: poser.rotate('spine_01', 'Y', lean)
    comp = {}
    for side in 'LR':
        (x, y, lift), heading, fp, tp = feet[side]
        cx, cy = to_component(x, y, dist, yaw)
        comp[side] = (Vector((cx, cy, NEUTRAL_BALL[side].z + lift)), heading - math.degrees(yaw), fp, tp)
    poser.update()
    r = 0.
    for side in 'LR':
        ball, heading, fp, tp = comp[side]
        r = max(r, poser.leg(side, ball, fp, tp, heading=heading))
    for side in 'LR': poser.hand_goal(side)
    return r

def loop_entry(side):
    """Component x of the ball at WalkLoop frame 0."""
    return NEUTRAL_BALL[side].x + foot_cycle(0. if side == 'L' else .5)[0]

# Rest = the idle contrapposto stance; loop = the saunter's paw line and toe-out.
RB = {s: (NEUTRAL_BALL[s].x + IDLE_FOOT[s][0], NEUTRAL_BALL[s].y + IDLE_FOOT[s][1], IDLE_FOOT[s][2]) for s in 'LR'}
LOOP_Y = {s: NEUTRAL_BALL[s].y for s in 'LR'}
SWING = (1 - WALK['stance_fraction']) * PERIOD  # as in WalkLoop
# WalkLoop lift of foot_R after the seam (frame 0 has foot_L just planted).
R_LIFT = ((.5 + WALK['stance_fraction']) % 1) * PERIOD
L_LIFT = WALK['stance_fraction'] * PERIOD

# WalkStart: 0 -> walk speed over START_T, ending on WalkLoop frame 0
# (foot_L just planted at +half, foot_R in late stance).
START_T = .5
D_end = travel(START_T, START_T, True)
START_R = (.08, .08 + SWING)
start_steps = {
    # R's second entry is its next WalkLoop lift after the seam, so the heel is
    # already rolling at the seam exactly as in the loop.
    'R': [(START_R[0], START_R[1], (loop_entry('R') + D_end, LOOP_Y['R'], TOE_OUT['R'])),
          (START_T + R_LIFT, START_T + R_LIFT + SWING, (loop_entry('R') + D_end, LOOP_Y['R'], TOE_OUT['R']))],
    'L': [(START_T - SWING, START_T, (loop_entry('L') + D_end, LOOP_Y['L'], TOE_OUT['L']))],
}
def walk_start(phase, f):
    t = f / FPS
    feet = {s: plan_foot(t, RB[s], start_steps[s]) for s in 'LR'}
    return pose_planned(t, feet, travel(t, START_T, True), 0., smoothstep(t, 0., START_T),
                        2 * math.pi * (t - START_T) / PERIOD)
START_FRAMES = round(START_T * FPS)

# WalkStop: from WalkLoop frame 0 into the aplomb idle stance, walk speed -> 0 over STOP_T.
STOP_T = .5
D_stop = travel(STOP_T, STOP_T, False)
stop_init = {s: (loop_entry(s), LOOP_Y[s], TOE_OUT[s]) for s in 'LR'}
STOP_R = (R_LIFT, R_LIFT + SWING + .03)
STOP_L = (L_LIFT, L_LIFT + SWING + .04)
stop_steps = {
    'R': [(STOP_R[0], STOP_R[1], (RB['R'][0] + D_stop, RB['R'][1], RB['R'][2]))],
    'L': [(STOP_L[0], STOP_L[1], (RB['L'][0] + D_stop, RB['L'][1], RB['L'][2]))],
}
def walk_stop(phase, f):
    t = f / FPS
    feet = {s: plan_foot(t, stop_init[s], stop_steps[s]) for s in 'LR'}
    return pose_planned(t, feet, travel(t, STOP_T, False), 0., 1 - smoothstep(t, 0., STOP_T),
                        2 * math.pi * t / PERIOD)
STOP_FRAMES = round(STOP_L[1] * FPS) + 3  # last plant, then a short settle hold

# Turn in place 90 degrees over 0.6 s. yaw = capsule rotation; the inside paw
# steps twice, the outside paw once; head and chest lead the turn.
TURN_T = .6
def turn_clip(sign):
    lead, trail = ('L', 'R') if sign > 0 else ('R', 'L')
    final = 90. * sign
    def world_rest(s, deg):
        a = math.radians(deg); x, y, _ = RB[s]
        return (x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a), deg + RB[s][2])
    steps = {lead: [(.04, .2, world_rest(lead, final * .55)), (.36, .52, world_rest(lead, final))],
             trail: [(.2, .36, world_rest(trail, final))]}
    def pose(phase, f):
        t = f / FPS
        yaw = math.radians(final) * smoothstep(t, .02, .5)
        look = final * .35 * (smoothstep(t, 0., .15) - smoothstep(t, .35, .6))
        feet = {s: plan_foot(t, RB[s], steps[s]) for s in 'LR'}
        return pose_planned(t, feet, 0., yaw, .35 * math.sin(math.pi * min(1., t / TURN_T)),
                            2 * math.pi * t / PERIOD, 0., look)
    return pose, steps
TURN_FRAMES = round(TURN_T * FPS) + 2

# Jumps: root stays put; runtime supplies the ballistic capsule motion.
def jump_start(phase, f):
    t = f / FPS
    crouch = smoothstep(t, 0., .16) * (1 - smoothstep(t, .16, .25))
    extend = smoothstep(t, .16, .25)
    poser.translate('pelvis', (0, 0, -4.2 * crouch + 1.0 * extend))
    poser.rotate('spine_01', 'Y', 10 * crouch - 3 * extend)
    poser.rotate('spine_02', 'Y', 6 * crouch)
    poser.rotate('head', 'Y', -6 * crouch)
    for s in 'LR':
        poser.rotate(f'upperarm_{s}', 'Y', 25 * crouch - 35 * extend)
        poser.rotate(f'lowerarm_{s}', 'Y', -20 * crouch - 10 * extend)
    for i, b in enumerate(TAIL): poser.rotate(b, 'Y', -4 * crouch + 3 * extend * (i + 1) / 6)
    poser.update()
    r = 0.
    for s in 'LR':
        r = max(r, poser.leg(s, NEUTRAL_BALL[s], 34 * extend, 0.))
    for s in 'LR': poser.hand_goal(s)
    return r

def jump_loop(phase, f):
    w = 2 * math.pi * phase
    poser.translate('pelvis', (0, 0, 1.5))
    poser.rotate('spine_01', 'Y', -2 + .8 * math.sin(w))
    for s in 'LR':
        poser.rotate(f'upperarm_{s}', 'Y', -28 + 3 * math.sin(w))
        poser.rotate(f'upperarm_{s}', 'X', 12 if s == 'L' else -12)
        poser.rotate(f'lowerarm_{s}', 'Y', -18)
        curl(s, 15)
    for i, b in enumerate(TAIL): poser.rotate(b, 'Y', 3 + 1.5 * math.sin(w - .5 * i))
    poser.update()
    r = 0.
    for s in 'LR':
        # Paws tucked under the body, toes pointing slightly down for landing.
        r = max(r, poser.leg(s, NEUTRAL_BALL[s] + Vector((1.5, 0, 5.5 + .4 * math.sin(w))), -8, 14))
    for s in 'LR': poser.hand_goal(s)
    return r

def jump_land(phase, f):
    t = f / FPS
    absorb = smoothstep(t, 0., .08) * (1 - smoothstep(t, .1, .4))
    poser.translate('pelvis', (0, 0, -4.5 * absorb))
    poser.rotate('spine_01', 'Y', 9 * absorb)
    poser.rotate('spine_02', 'Y', 5 * absorb)
    poser.rotate('head', 'Y', -5 * absorb)
    arms = 1 - smoothstep(t, 0., .35)
    for s in 'LR':
        poser.rotate(f'upperarm_{s}', 'Y', -28 * arms + 8 * absorb)
        poser.rotate(f'lowerarm_{s}', 'Y', -18 * arms - 6 * absorb)
    for i, b in enumerate(TAIL): poser.rotate(b, 'Y', 3 * arms - 4 * absorb)
    poser.update()
    r = 0.
    for s in 'LR':
        r = max(r, poser.leg(s, NEUTRAL_BALL[s], 0., 0.))
    for s in 'LR': poser.hand_goal(s)
    return r

# ---- agility (user vision 2026-09-27): "he busts into a roll and a side jump
# ... then reassumes his cool and collected saunter". Root fixed as for every
# clip. Roll: the runtime drives the capsule along the manifest's per-frame
# travel. Side jump: the runtime launches the capsule sideways at takeoff and
# stops it on touchdown; the clip carries the landing's momentum in the
# pelvis. Both start and end in the aplomb idle stance (RB), so Idle,
# WalkStart and the settle steps pick up from them unchanged.
import numpy as np
from mathutils import Matrix

def body_min_z():
    """Lowest deformed vertex of the (tufted) body mesh in the current pose."""
    ev = body.evaluated_get(bpy.context.evaluated_depsgraph_get()); m = ev.to_mesh()
    co = np.empty(len(m.vertices) * 3); m.vertices.foreach_get('co', co)
    ev.to_mesh_clear()
    return float(co[2::3].min())

def ik_goals():
    """Foot goal helpers follow the posed hocks (clips carry goal data)."""
    for side in 'LR':
        goal = rig.pose.bones[f'ik_foot_{side}']
        goal.location = poser.rest[f'ik_foot_{side}'].to_3x3().inverted() @ (
            poser.head(f'foot_{side}') - poser.rest_head[f'ik_foot_{side}'])
        poser.hand_goal(side)

# Forward roll: a quick dive off both paws, one tucked revolution over the
# shoulders, paws come round and plant, and he rises back into the aplomb
# stance. ~1 m (1.5 of his heights) in 0.8 s. Speed: smoothstep up to the peak
# by 0.1 s, down to rest by 0.62 s; the paws plant at 0.56 s on the planner's
# world positions while the last of the momentum bleeds off.
ROLL_T, ROLL_PEAK = .8, 215.
ROLL_LIFT, ROLL_PLANT, ROLL_SPIN = .08, .56, (.1, .54)
ROLL_PIVOT = Vector((1., 0., 14.))  # centre of the tucked ball, before the ground fit
def roll_speed(t):
    return ROLL_PEAK * smoothstep(t, 0., .1) * (1 - smoothstep(t, .42, .62))
_ROLL_N = 800
_roll_d = [0.]
for _i in range(_ROLL_N):
    _a, _b = ROLL_T * _i / _ROLL_N, ROLL_T * (_i + 1) / _ROLL_N
    _roll_d.append(_roll_d[-1] + .5 * (roll_speed(_a) + roll_speed(_b)) * (_b - _a))
def roll_travel(t):
    x = max(0., min(1., t / ROLL_T)) * _ROLL_N; i = min(int(x), _ROLL_N - 1)
    return _roll_d[i] + (_roll_d[i + 1] - _roll_d[i]) * (x - i)
ROLL_D = roll_travel(ROLL_T)
ROLL_SPIN_D = (roll_travel(ROLL_SPIN[0]), roll_travel(ROLL_SPIN[1]))

def roll_pose(t, lift_z):
    d = roll_travel(t)
    tuck = smoothstep(t, .02, .16) * (1 - smoothstep(t, .5, .7))
    spin = 360 * smoothstep((d - ROLL_SPIN_D[0]) / (ROLL_SPIN_D[1] - ROLL_SPIN_D[0]), 0., 1.)
    wrapped = spin - 360 if spin > 180 else spin
    carriage(0., 0.)
    # Tuck in the body frame: crouch over the paws, spine and head curl down
    # (chin to chest, the cigarette kept clear), forearms hug in, tail curls.
    poser.translate('pelvis', (2. * tuck, .9 * tuck, -7. * tuck))  # cancels the idle hip shift
    poser.rotate('pelvis', 'X', 4. * tuck)
    for bone, deg in (('spine_01', 30), ('spine_02', 34), ('chest', 30), ('neck', 25), ('head', 38)):
        poser.rotate(bone, 'Y', deg * tuck)
    for side, sign in (('L', 1), ('R', -1)):
        poser.rotate(f'upperarm_{side}', 'Y', -58 * tuck)
        poser.rotate(f'upperarm_{side}', 'X', -sign * 6 * tuck)
        poser.rotate(f'lowerarm_{side}', 'Y', -72 * tuck)
        curl(side, 50 * tuck)
    for b in TAIL:  # wraps round his left hip instead of whipping over
        poser.rotate(b, 'Y', 10 * tuck)
        poser.rotate(b, 'Z', -18 * tuck)
    poser.update()
    # One revolution about the ball centre (+Y: nose down first), then the
    # ground fit's lift. Children follow the pelvis.
    pb = rig.pose.bones['pelvis']
    pb.matrix = (Matrix.Translation(Vector((0., 0., lift_z))) @ Matrix.Translation(ROLL_PIVOT)
                 @ Matrix.Rotation(math.radians(spin), 4, 'Y') @ Matrix.Translation(-ROLL_PIVOT) @ pb.matrix)
    poser.update()
    spin_m = Matrix.Rotation(math.radians(spin), 3, 'Y')
    off = smoothstep(t, ROLL_LIFT - .02, ROLL_LIFT + .06)
    on = smoothstep(t, ROLL_PLANT - .1, ROLL_PLANT)
    r = 0.
    for side in 'LR':
        x, y, h = RB[side]
        z = NEUTRAL_BALL[side].z
        planted = Vector((x - d, y, z))            # world-locked while the capsule moves
        landed = Vector((x + ROLL_D - d, y, z))     # final stance, world-locked from the plant
        tucked_body = NEUTRAL_BALL[side] + Vector((2., 0., 10.))  # knees to chest
        tucked = ROLL_PIVOT + spin_m @ (tucked_body - ROLL_PIVOT) + Vector((0., 0., lift_z))
        ball = planted.lerp(tucked, off).lerp(landed, on)
        pitch = (1 - on) * off * (wrapped + 25.)
        heading = h + (0. - h) * off * (1 - on)
        pole = Vector((1., 0., 0.)).lerp(spin_m @ Vector((1., 0., .4)), off * (1 - on))
        r = max(r, poser.leg(side, ball, pitch, 0., pole=tuple(pole.normalized()), heading=heading))
    ik_goals()
    return r

def roll(phase, f):
    t = f / FPS
    r = roll_pose(t, 0.)
    # Ground fit: while off the paws the tucked ball rests on the ground;
    # otherwise only lift out of it.
    air = smoothstep(t, ROLL_LIFT, ROLL_LIFT + .06) * (1 - smoothstep(t, ROLL_PLANT - .06, ROLL_PLANT))
    low = body_min_z() - .05
    lift = -low * air + max(0., -low) * (1 - air)
    if abs(lift) > .02:
        poser.reset()
        r = roll_pose(t, lift)
    return r

# Side jump: load onto the far leg, spring sideways, lean into the flight with
# the lead arm out and the tail counter-swinging, land on both paws in the
# aplomb stance and let the hips carry on a little before settling. Launch
# 190 cm/s sideways, apex 16 cm (air 0.4 s, ~77 cm: over a body height).
SIDE_T, SIDE_TAKEOFF, SIDE_LAND = .8, .1, .5
SIDE_SPEED, SIDE_APEX = 190., 16.
SIDE_GRAVITY = 980. * .8  # ChuckCharacter GravityScale 0.8
SIDE_VZ = math.sqrt(2 * SIDE_GRAVITY * SIDE_APEX)

def side_jump(sign):
    """sign +1: Chuck's left (+Y), -1: right."""
    lead, trail = ('L', 'R') if sign > 0 else ('R', 'L')
    def pose(phase, f):
        t = f / FPS
        load = smoothstep(t, 0., .08) * (1 - smoothstep(t, .08, .13))
        air = smoothstep(t, .08, .18) * (1 - smoothstep(t, SIDE_LAND - .08, SIDE_LAND))
        absorb = smoothstep(t, SIDE_LAND - .02, SIDE_LAND + .07) * (1 - smoothstep(t, SIDE_LAND + .1, SIDE_T))
        carriage(0., 0.)
        poser.translate('pelvis', (0., sign * (-2.5 * load + 3. * absorb), -4.5 * load + 1. * air - 4.5 * absorb))
        # Lean into the jump (top toward the travel), head held level.
        poser.rotate('pelvis', 'X', -sign * (6 * load + 20 * air + 8 * absorb))
        poser.rotate('chest', 'X', -sign * 4 * air)
        poser.rotate('head', 'X', sign * (5 * load + 18 * air + 6 * absorb))
        poser.rotate('spine_01', 'Y', 8 * load + 7 * absorb)
        poser.rotate('chest', 'Y', 3 * load + 3 * absorb)
        for side in 'LR':
            out = 1 if side == 'L' else -1   # abduction sign (away from the body)
            if side == lead:
                poser.rotate(f'upperarm_{side}', 'X', out * (55 * air + 14 * absorb))
                poser.rotate(f'lowerarm_{side}', 'Y', -30 * air)
            else:
                poser.rotate(f'upperarm_{side}', 'Y', -32 * air)
                poser.rotate(f'upperarm_{side}', 'X', out * (8 * air + 10 * absorb))
                poser.rotate(f'lowerarm_{side}', 'Y', -30 * air)
            curl(side, 22 * air)
        for i, b in enumerate(TAIL):
            poser.rotate(b, 'Z', sign * (4 * air - 1.5 * absorb) * (i + 1) / 3)
            poser.rotate(b, 'Y', 4 * air)
        poser.update()
        r = 0.
        for side in 'LR':
            x, y, h = RB[side]
            ball = Vector((x, y, NEUTRAL_BALL[side].z))
            if side == lead:
                reach = smoothstep(t, .08, .2) * (1 - smoothstep(t, .36, SIDE_LAND - .02))
                ball += Vector((0., sign * 6., 6.)) * reach
                fp, tp = 0., 12 * reach
            else:
                tuck = smoothstep(t, .07, .16) * (1 - smoothstep(t, .36, SIDE_LAND))
                ball += Vector((-1.5, sign * 3.5, 9.)) * tuck
                fp = 22 * smoothstep(t, .03, .09) * (1 - smoothstep(t, .1, .16))  # push off the ball
                tp = 18 * tuck
            flight = smoothstep(t, .08, .16) * (1 - smoothstep(t, .4, SIDE_LAND))
            r = max(r, poser.leg(side, ball, fp, tp, heading=h * (1 - flight)))
        ik_goals()
        return r
    return pose

def samples(fn, dur, n=9):
    return [[round(dur * i / (n - 1), 4), round(fn(dur * i / (n - 1)), 3)] for i in range(n)]

author('WalkStart', START_FRAMES + 1, walk_start, False, {
    'nominal_speed_profile_cm_s': samples(lambda t: speed_ramp(t, START_T, True), START_T),
    'capsule_travel_cm_per_frame': [round(travel(f / FPS, START_T, True), 4) for f in range(START_FRAMES + 1)],
    'travel_cm': round(D_end, 3), 'ends_on': 'WalkLoop frame 0 (foot_L planted, foot_R late stance)',
    'stance_intervals_s': {f'foot_{s}': stance_from_steps(start_steps[s], START_T) for s in 'LR'},
    'events_s': {'foot_R_lift': START_R[0], 'foot_R_plant': round(START_R[1], 4), 'foot_L_lift': round(START_T - SWING, 4), 'foot_L_plant': START_T},
    'duration_travel_s': START_T,
    'notes': f"Speed rises with a smoothstep from 0 to {WALK['speed_cm_s']:g} cm/s; planted paws are world-locked against that travel; starts from the aplomb idle stance."})
author('WalkStop', STOP_FRAMES + 1, walk_stop, False, {
    'nominal_speed_profile_cm_s': samples(lambda t: speed_ramp(t, STOP_T, False), STOP_T),
    'capsule_travel_cm_per_frame': [round(travel(f / FPS, STOP_T, False), 4) for f in range(STOP_FRAMES + 1)],
    'travel_cm': round(D_stop, 3), 'starts_from': 'WalkLoop frame 0', 'settle_hold_s': round(3 / FPS, 4),
    'stance_intervals_s': {f'foot_{s}': stance_from_steps(stop_steps[s], STOP_FRAMES / FPS) for s in 'LR'},
    'events_s': {'foot_R_lift': round(STOP_R[0], 4), 'foot_R_plant': round(STOP_R[1], 4),
                 'foot_L_lift': round(STOP_L[0], 4), 'foot_L_plant': round(STOP_L[1], 4)},
    'duration_travel_s': STOP_T,
    'notes': f"Speed falls with a smoothstep from {WALK['speed_cm_s']:g} to 0 cm/s; ends in the aplomb idle stance."})
for name, sign in (('TurnLeft90', 1), ('TurnRight90', -1)):
    fn, steps = turn_clip(sign)
    author(name, TURN_FRAMES + 1, fn, False, {
        'capsule_yaw_deg_profile': samples(lambda t, sign=sign: 90 * sign * smoothstep(t, .02, .5), TURN_T),
        'capsule_yaw_deg_per_frame': [round(90 * sign * smoothstep(f / FPS, .02, .5), 4) for f in range(TURN_FRAMES + 1)],
        'yaw_sign_note': 'source +Z yaw (counter-clockwise from above, toward +Y = runtime left); verify sign after import',
        'stance_intervals_s': {f'foot_{s}': stance_from_steps(steps[s], TURN_FRAMES / FPS) for s in 'LR'},
        'events_s': {f'foot_{s}_{kind}_{i}': t for s in 'LR' for i, (lift, land, _) in enumerate(steps[s])
                     for kind, t in (('lift', lift), ('plant', land))},
        'notes': 'Turn in place: capsule yaw per profile, root fixed; paws world-locked while planted; inside paw steps twice, outside paw once.'})
author('JumpStart', round(.25 * FPS) + 1, jump_start, False, {
    'events_s': {'crouch_bottom': .16, 'takeoff': .25},
    'stance_intervals_s': {'foot_L': [[0., .25]], 'foot_R': [[0., .25]]},
    'notes': 'Anticipation crouch then extension onto the toes; capsule launch is runtime-driven at takeoff.'})
author('JumpLoop', 12, jump_loop, True, {
    'stance_intervals_s': {'foot_L': [], 'foot_R': []},
    'notes': 'Airborne hold with tucked paws and balancing arms/tail; loops while airborne.'})
author('JumpLand', 13, jump_land, False, {
    'events_s': {'contact': 0., 'max_compression': .09, 'settled': .4},
    'stance_intervals_s': {'foot_L': [[0., .4]], 'foot_R': [[0., .4]]},
    'notes': 'Contact at frame 0 with toes slightly pointed; pelvis absorbs 4.5 cm and recovers to neutral.'})

author('Roll', round(ROLL_T * FPS) + 1, roll, False, {
    'capsule_travel_cm_per_frame': [round(roll_travel(f / FPS), 4) for f in range(round(ROLL_T * FPS) + 1)],
    'capsule_speed_cm_s_profile': samples(roll_speed, ROLL_T),
    'travel_cm': round(ROLL_D, 3), 'duration_travel_s': ROLL_T,
    'stance_intervals_s': {'foot_L': [[0., ROLL_LIFT], [ROLL_PLANT, ROLL_T]], 'foot_R': [[0., ROLL_LIFT], [ROLL_PLANT, ROLL_T]]},
    'events_s': {'lift': ROLL_LIFT, 'spin_start': ROLL_SPIN[0], 'spin_end': ROLL_SPIN[1], 'plant': ROLL_PLANT, 'settled': ROLL_T},
    'notes': 'Forward roll from and back to the aplomb stance. Root fixed: the runtime moves the capsule along capsule_travel_cm_per_frame (facing direction). One tucked revolution; the ball is ground-fitted to the deformed mesh while off the paws.'})
for name, sign in (('SideJumpLeft', 1), ('SideJumpRight', -1)):
    author(name, round(SIDE_T * FPS) + 1, side_jump(sign), False, {
        'launch': {'lateral_cm_s': SIDE_SPEED, 'vertical_cm_s': round(SIDE_VZ, 3), 'gravity_cm_s2': SIDE_GRAVITY,
                   'apex_cm': SIDE_APEX, 'direction': 'source +Y (Chuck left)' if sign > 0 else 'source -Y (Chuck right)'},
        'stance_intervals_s': {'foot_L': [[0., SIDE_TAKEOFF], [SIDE_LAND, SIDE_T]], 'foot_R': [[0., SIDE_TAKEOFF], [SIDE_LAND, SIDE_T]]},
        'events_s': {'takeoff': SIDE_TAKEOFF, 'land': SIDE_LAND, 'settled': SIDE_T},
        'notes': 'Side jump from and back to the aplomb stance, facing unchanged. Root fixed: the runtime launches the capsule sideways at takeoff, holds the clip just before land while airborne, and stops the capsule at touchdown (the clip carries the momentum in the hips).'})

# ---------------------------------------------------------------- export
FBX = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False,
           primary_bone_axis='Y', secondary_bone_axis='X', use_armature_deform_only=False,
           mesh_smooth_type='FACE')
poser.reset()
rig.animation_data_create(); rig.animation_data.action = None
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); body.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.export_scene.fbx(filepath=str(V1 / 'SK_Chuck.fbx'), use_selection=True,
                         object_types={'ARMATURE', 'MESH'}, bake_anim=False, **FBX)
# Groom variant: the same skinned mesh, skeleton, UVs and materials without the
# geometric fur tufts, for use with the strand groom (V1/Groom). Unreal imports
# it against the SK_Chuck skeleton; SK_Chuck stays the no-groom fallback.
groomed = body.copy(); groomed.data = body.data.copy()
groomed.name = groomed.data.name = 'SK_Chuck_Groomed'
scene.collection.objects.link(groomed)
import bmesh
bm = bmesh.new(); bm.from_mesh(groomed.data)
tuft = bm.faces.layers.bool.get('chuck_tuft')
bmesh.ops.delete(bm, geom=[f for f in bm.faces if f[tuft]], context='FACES')
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bm.to_mesh(groomed.data); bm.free()
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); groomed.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.export_scene.fbx(filepath=str(V1 / 'SK_Chuck_Groomed.fbx'), use_selection=True,
                         object_types={'ARMATURE', 'MESH'}, bake_anim=False, **FBX)
groomed.hide_set(True); groomed.hide_render = True
print('CHUCK_V1_GROOMED_VARIANT tris', sum(len(p.vertices) - 2 for p in groomed.data.polygons))
manifest = {'contract': 'docs/RIG-CONTRACT-V1.md', 'skeleton': '/Game/Characters/Chuck/V1/SK_Chuck',
            'fps': FPS, 'clips': []}
for action, entry in CLIPS:
    rig.animation_data.action = action
    scene.frame_start, scene.frame_end = entry['first_frame'], entry['last_frame']
    bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(V1 / entry['file']), use_selection=True, object_types={'ARMATURE'},
                             bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
                             bake_anim_simplify_factor=0., bake_anim_step=1., **FBX)
    action.use_fake_user = True
    manifest['clips'].append(entry)
rig.animation_data.action = None
scene.frame_start, scene.frame_end = 0, 59
(ANIM / 'manifest.json').write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
# Keep the runtime's distance matching, stance windows and turn profile in step.
runpy.run_path(str(ROOT / 'Tools/gen_chuck_clip_data.py'), run_name='__main__')

tris = sum(len(p.vertices) - 2 for p in body.data.polygons)
zs = [v.co.z for v in body.data.vertices]
meta = {'contract': 'docs/RIG-CONTRACT-V1.md', 'table': 'SourceAssets/Chuck/rig_proposal.json',
        'shape_amendment': {'module': 'Tools/chuck_v1_shape.py', 'version': 'v1.1', 'lift_cm': SHAPE.LIFT,
                            'torso_narrow': SHAPE.TORSO_NARROW, 'arm_narrow': SHAPE.ARM_NARROW,
                            'effective_heads_cm': {n: b['head'] for n, b in TABLE.items()}},
        'bones': len(TABLE), 'deforming': sum(b['deform'] for b in TABLE.values()),
        'triangles': tris, 'bounds_z_cm': [round(min(zs), 3), round(max(zs), 3)],
        'materials': sorted({m.name.split('.')[0] for m in body.data.materials}),
        'sole_markers_source_cm': markers,
        'sole_markers_unreal_component_cm': {s: {k: ([v[0], -v[1], v[2]] if isinstance(v, list) else v)
                                                 for k, v in m.items()} for s, m in markers.items()},
        'paw_origin_source_cm': {s: [round(c, 3) for c in o] for s, o in PAW_ORIGIN.items()},
        'fbx_settings': {k: v for k, v in FBX.items()} | {'object_types_mesh': ['ARMATURE', 'MESH'],
                                                            'object_types_clips': ['ARMATURE']},
        'edit_bone_roll': 0.0, 'bone_local_axis': 'Blender +Y head-to-tail',
        'rest_matrices_source_armature_space': {b.name: [[round(x, 6) for x in row] for row in b.matrix_local]
                                                for b in rig.data.bones}}
(V1 / 'rig_v1_metadata.json').write_text(json.dumps(meta, indent=1) + '\n', encoding='utf-8')
# Save in the rest pose: clearing the action leaves the last exported clip's
# pose on the bones, which later scripts (groom, bakes) would evaluate.
rig.animation_data.action = None
poser.reset()
bpy.ops.wm.save_as_mainfile(filepath=str(V1 / 'Chuck_V1.blend'))
print('CHUCK_V1_READY', 'tris', tris, 'clips', [e['name'] for _, e in CLIPS],
      'reach', [e['max_leg_reach_ratio'] for _, e in CLIPS], 'markers', markers)
