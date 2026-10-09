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

def author(name, frames, pose_fn, loop, extra, ground=True):
    """ground: keep the tail above the paws' floor (off for clips with no floor, hanging in the air)."""
    action = bpy.data.actions.new(f'AS_Chuck_{name}')
    rig.animation_data_create(); rig.animation_data.action = action
    reach = 0.
    for f in range(frames):
        poser.reset()
        reach = max(reach, pose_fn(f / frames if loop else f / max(1, frames - 1), f))
        if ground: poser.clear_ground(TAIL, TAIL_RADIUS, .9)  # margin covers mesh sag between joints
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
        poser.rotate(f'upperarm_{side}', 'X', sign * (11 * k + 5 * idle))  # arms carried wide; at rest a little off the body so the sleeves read
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

# ---- run (References/ArtDirection/Chuck-Run-Profile.png, Chuck-Run-Cycle-Sheet.png):
# a long stride with a flight phase, forward lean from the hips, arms pumping
# with loose fists at about 90 degrees, heel kicked up behind, knee driven high,
# tail streaming back. Dynamic similarity: a running Froude number ~1.5 on his
# 22.5 cm leg gives ~1.8 m/s; stride time scales with sqrt(leg length) (a
# human's ~0.7 s -> ~0.35 s); the reference's long, leaping stride takes the
# slower end, 0.4 s (38 cm steps). Duty factor 0.3 (both paws off the ground
# between steps).
# User 2026-09-29 "running a bit faster": 190 -> 225 cm/s with a quicker cycle
# (12 -> 10 frames) so the steps stay as long as before (37.5 cm, was 38).
RUN = {'speed_cm_s': 225.0, 'period_frames': 10, 'stance_fraction': .3}
RUN_PERIOD = RUN['period_frames'] / FPS
RUN_STANCE_CM = RUN['speed_cm_s'] * RUN_PERIOD * RUN['stance_fraction']

def hermite(keys, u, end_tangents=None):
    """Cubic Hermite through keys [(u, v0, v1, ...)] with Catmull-Rom tangents;
    end_tangents optionally fixes the first value's slope at both ends."""
    i = next(j for j in range(len(keys) - 1) if u <= keys[j + 1][0])
    (u0, *p0), (u1, *p1) = keys[i], keys[i + 1]
    def tangent(j):
        a, b = keys[max(j - 1, 0)], keys[min(j + 1, len(keys) - 1)]
        t = [(bv - av) / (b[0] - a[0]) for av, bv in zip(a[1:], b[1:])]
        if end_tangents and j in (0, len(keys) - 1): t[0] = end_tangents
        return t
    m0, m1 = tangent(i), tangent(i + 1)
    s = (u - u0) / (u1 - u0); h = u1 - u0
    h00, h10, h01, h11 = 2*s**3 - 3*s**2 + 1, s**3 - 2*s**2 + s, -2*s**3 + 3*s**2, s**3 - s**2
    return [h00 * a + h10 * h * ta + h01 * b + h11 * h * tb for a, b, ta, tb in zip(p0, p1, m0, m1)]

def run_foot(phase):
    """(forward offset of the ball from neutral, lift, foot pitch, toe pitch)."""
    st = RUN['stance_fraction']; half = RUN_STANCE_CM / 2
    if phase < st:
        u = phase / st
        # Forefoot strike; the heel stays up and rises into the toe push.
        return half - RUN_STANCE_CM * u, 0., 22 + 30 * smoothstep(u, .45, 1.), 0.
    u = (phase - st) / (1 - st)
    # Swing: the heel kicks up behind, the knee drives through high, the paw
    # reaches and is pulled back to meet the ground at running speed.
    keys = [(0., -half, 0., 52., 0.), (.12, -half - 4., 3., 62., 8.), (.38, -half - 2., 10., 80., 30.),
            (.62, 4., 9., 25., 12.), (.86, half + 2., 4., 8., -4.), (1., half, 0., 22., 0.)]
    ground = -RUN_STANCE_CM / st * (1 - st)  # dx/du of a planted paw: no slip at contact
    x, lift, fp, tp = hermite(keys, u, ground)
    return x, max(0., lift), fp, tp

def run_carriage(w):
    lag = TAU * .05
    mid_l = TAU * RUN['stance_fraction'] / 2
    # Lowest at each mid-stance, highest in flight; lean forward from the hips.
    poser.translate('pelvis', (0, .3 * math.cos(w - mid_l), -1.2 - 1.3 * math.cos(2 * (w - mid_l))))
    poser.rotate('pelvis', 'Y', 17)
    poser.rotate('pelvis', 'Z', -6 * math.cos(w))
    poser.rotate('pelvis', 'X', 2 * math.cos(w - mid_l))
    poser.rotate('spine_01', 'Y', 7)
    poser.rotate('spine_02', 'Y', 3)
    poser.rotate('spine_02', 'Z', 6 * math.cos(w - lag))
    poser.rotate('chest', 'Z', 8 * math.cos(w - lag))
    poser.rotate('chest', 'Y', 2)
    # Gaze level down the dock despite the lean.
    poser.rotate('neck', 'Y', -9)
    poser.rotate('head', 'Z', -9 * math.cos(w - lag))
    poser.rotate('head', 'Y', -15 + 1.5 * math.cos(2 * w - lag))
    # Tail streams out behind, a little wave through it.
    for i, (b, deg) in enumerate(zip(TAIL, (12, 4, 3, 2, 0, -2))):
        poser.rotate(b, 'Y', deg + 2.5 * math.sin(2 * w - .8 * (i + 1)))
        poser.rotate(b, 'Z', 4 * math.sin(w - .7 * (i + 1)))
    # Arms pump opposite the legs, elbows about 90 degrees, loose fists.
    for side, sign in (('L', 1), ('R', -1)):
        swing = math.cos(w - lag + (0 if side == 'L' else math.pi))  # +1 = arm back
        poser.rotate(f'upperarm_{side}', 'Y', 40 * swing - 10)
        poser.rotate(f'upperarm_{side}', 'X', sign * 8)
        poser.rotate(f'lowerarm_{side}', 'Y', -78 - 14 * max(0., -swing))
        poser.rotate(f'hand_{side}', 'Y', 6 * swing)
        curl(side, 48)

def run(phase, f):
    run_carriage(TAU * phase)
    poser.update()
    r = 0.
    for side in 'LR':
        x, lift, fp, tp = run_foot(phase if side == 'L' else (phase + .5) % 1)
        # Paws land a little closer to the midline at speed.
        ball = NEUTRAL_BALL[side] + Vector((x, -(1. if side == 'L' else -1.), lift))
        r = max(r, poser.leg(side, ball, fp, tp, heading=TOE_OUT[side] * .4))
    for side in 'LR': poser.hand_goal(side)
    return r

def run_stance(offset_phase):
    st = RUN['stance_fraction']
    a, b = offset_phase % 1, (offset_phase + st) % 1
    spans = [(a, b)] if a < b else [(a, 1.), (0., b)]
    return [[round(x * RUN_PERIOD, 4), round(y * RUN_PERIOD, 4)] for x, y in spans]

author('RunLoop', RUN['period_frames'], run, True, {
    'reference_speed_cm_s': RUN['speed_cm_s'], 'stride_cycle_cm': round(RUN['speed_cm_s'] * RUN_PERIOD, 3),
    'stance_travel_cm': round(RUN_STANCE_CM, 3), 'stance_fraction': RUN['stance_fraction'],
    'stance_intervals_s': {'foot_L': run_stance(0.), 'foot_R': run_stance(.5)},
    'events_s': {'foot_L_plant': 0.0, 'foot_R_plant': round(RUN_PERIOD / 2, 4),
                 'foot_L_lift': round(RUN['stance_fraction'] * RUN_PERIOD, 4),
                 'foot_R_lift': round(((.5 + RUN['stance_fraction']) % 1) * RUN_PERIOD, 4)},
    'phase_convention': 'as WalkLoop: phase 0 = foot_L touchdown, foot_R half a cycle later (runtime blends the two loops on a shared phase)',
    'notes': 'Run with a flight phase. In place (root fixed); during stance the planted ball moves backward in component space at exactly the reference speed; forefoot strike, heel up; swing meets the ground at running speed.'})

# Running jump: a leap out of the run. The clip is posed over the flight's
# normalized progress (the runtime maps clip time from the vertical speed), so
# it fits any landing height: the push into a split leap - lead leg reaching,
# trail leg stretched back, opposite arm forward, chest open, tail up for
# balance - then back to RunLoop frame 0 (foot_L touchdown), so a landing at a
# run drops straight into the stride.
RUN_JUMP_FRAMES = 15
RUN_JUMP_VZ = 190.  # cm/s up (the standing jump uses 170): ~23 cm apex, ~0.48 s, ~92 cm at run speed
def run_jump(phase, f):
    u = f / (RUN_JUMP_FRAMES - 1)
    leap = smoothstep(u, 0., .2) * (1 - smoothstep(u, .6, 1.))
    run_carriage(0.)
    poser.translate('pelvis', (0, 0, 3. * leap))
    poser.rotate('spine_01', 'Y', -3 * leap)
    poser.rotate('head', 'Y', -4 * leap)
    for side, sign in (('L', 1), ('R', -1)):
        back = 1 if side == 'L' else -1  # at RunLoop frame 0 the left arm is back
        poser.rotate(f'upperarm_{side}', 'Y', 18 * back * leap)
        poser.rotate(f'upperarm_{side}', 'X', sign * 10 * leap)
        poser.rotate(f'lowerarm_{side}', 'Y', 25 * leap)
    for i, b in enumerate(TAIL): poser.rotate(b, 'Y', (8 if i == 0 else 2) * leap)
    poser.update()
    split = {'L': (Vector((10., -1., 12.)), 10., -5.), 'R': (Vector((-20., 1., 12.)), 70., 40.)}
    r = 0.
    for side in 'LR':
        x, lift, fp, tp = run_foot(0. if side == 'L' else .5)
        stride = NEUTRAL_BALL[side] + Vector((x, -(1. if side == 'L' else -1.), lift))
        off, lfp, ltp = split[side]
        ball = stride.lerp(NEUTRAL_BALL[side] + off, leap)
        r = max(r, poser.leg(side, ball, fp + (lfp - fp) * leap, tp + (ltp - tp) * leap, heading=TOE_OUT[side] * .4))
    for side in 'LR': poser.hand_goal(side)
    return r

author('RunJump', RUN_JUMP_FRAMES, run_jump, False, {
    'launch': {'vertical_cm_s': RUN_JUMP_VZ, 'gravity_cm_s2': 980. * .8},
    'time_mapping': 'clip time = flight progress (vz0 - vz) / (2 vz0) x duration; last frame = RunLoop frame 0',
    'stance_intervals_s': {'foot_L': [], 'foot_R': []},
    'events_s': {'takeoff': 0., 'touchdown': round((RUN_JUMP_FRAMES - 1) / FPS, 4)},
    'ends_on': 'RunLoop frame 0 (foot_L touchdown)',
    'notes': 'Leap from the run: split legs, opposite arm forward, tail up; posed over normalized flight, ends on the run stride.'})

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
# User 2026-09-29: a side jump out of a walk is shorter, out of a run longer.
# Same clip (it holds before the landing for as long as he is airborne).
SIDE_SHORT = {'lateral_cm_s': 150., 'apex_cm': 13.}   # ~0.36 s air, ~55 cm
SIDE_LONG = {'lateral_cm_s': 265., 'apex_cm': 22.}    # ~0.47 s air, ~125 cm
def side_launch(g):
    return {'lateral_cm_s': g['lateral_cm_s'], 'vertical_cm_s': round(math.sqrt(2 * SIDE_GRAVITY * g['apex_cm']), 3), 'apex_cm': g['apex_cm']}

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

# ---- slash: Chuck's claw scratch (References/Original/PHASE-2.md: "brief
# forward movement, paw scratch"; no weapon). User vision: "slashes with
# agility". A fast diagonal rake from high on the striking side down across
# the body, stepping in 10 cm with the opposite paw (a boxer's cross), the
# torso winding up then unwinding hard, the other arm pulled back for balance,
# the gaze held forward, then back to the aplomb stance. SlashRight chains into
# SlashLeft. Root fixed: standing, the runtime moves the capsule along the
# manifest travel; on the move it plays the upper body over the stride.
SLASH_T, SLASH_STEP = .55, 12.  # base timeline (s) and step-in (cm)
# User 2026-09-28: "make it faster". The clip plays the base timeline 1.375x
# faster: 0.4 s, the cut itself (0.12-0.26 base) in about 0.1 s.
SLASH_RATE = 1.375
def slash_travel(t):
    return SLASH_STEP * smoothstep(t, .06, .38)

def slash_clip(side):
    sign = 1 if side == 'L' else -1  # +Y is Chuck's left
    other = 'R' if side == 'L' else 'L'
    # The opposite paw steps in with the strike; the striking side's paw follows.
    steps = {other: [(.06, .22, (RB[other][0] + SLASH_STEP, RB[other][1], RB[other][2]))],
             side: [(.28, .42, (RB[side][0] + SLASH_STEP, RB[side][1], RB[side][2]))]}
    # User direction 2026-09-28: "more like an almost sword slash, like how
    # Wolverine would slash", not a punch. So the wrist rides a wide, flat arc
    # at near full reach (~0.85) around the (turning) shoulder - high and back
    # outside, out wide, straight ahead at full extension, across and low on the
    # far side - with the arm kept long and the claws leading like a blade.
    path = [(.12, -8., sign * 10., 11.),
            (.17, 6., sign * 15., 6.),
            (.22, 17., sign * 3., -1.),
            (.27, 11., -sign * 12., -7.),
            (.34, 4., -sign * 12., -13.)]
    def pose(phase, f):
        t = f / FPS * SLASH_RATE  # authored on the base timeline, played faster
        wind = smoothstep(t, 0., .12) * (1 - smoothstep(t, .12, .22))
        strike = smoothstep(t, .12, .26) * (1 - smoothstep(t, .36, .54))
        twist = sign * 30 * wind - sign * 45 * strike
        carriage(0., 0.)
        poser.translate('pelvis', (1.2 * strike, 0, -1. * wind - 2.8 * strike))
        poser.rotate('pelvis', 'Z', .4 * twist)
        poser.rotate('spine_02', 'Z', .35 * twist)
        poser.rotate('chest', 'Z', .45 * twist)
        poser.rotate('head', 'Z', -.6 * twist)  # eyes stay on the target
        poser.rotate('spine_01', 'Y', 10 * strike - 3 * wind)
        poser.rotate('chest', 'X', sign * 6 * strike)  # shoulder drops into the cut
        poser.rotate('head', 'Y', 3 * strike)
        # The other arm pulls back and in: balance and guard.
        poser.rotate(f'upperarm_{other}', 'Y', 25 * strike + 8 * wind)
        poser.rotate(f'lowerarm_{other}', 'Y', -45 * (strike + .5 * wind))
        curl(other, 30 * strike)
        for i, b in enumerate(TAIL):
            poser.rotate(b, 'Z', -.25 * twist * (i + 1) / 3)
        poser.update()
        d = slash_travel(t)
        r = 0.
        for s in 'LR':
            (x, y, lift), h, fp, tp = plan_foot(t, RB[s], steps[s])
            cx, cy = to_component(x, y, d, 0.)
            r = max(r, poser.leg(s, Vector((cx, cy, NEUTRAL_BALL[s].z + lift)), fp, tp, heading=h))
        # Striking arm: IK along the path, blended from the carriage's own arm.
        w = smoothstep(t, .02, .1) * (1 - smoothstep(t, .36, .55))
        if w > 1e-3:
            fk_wrist = poser.head(f'hand_{side}')
            fk_elbow = poser.head(f'lowerarm_{side}')
            shoulder = poser.head(f'upperarm_{side}')
            u = min(max(t, path[0][0]), path[-1][0])
            target = shoulder + Vector(hermite(path, u))
            fk_pole = (fk_elbow - (shoulder + fk_wrist) / 2).normalized()
            # Elbow down and slightly out: the arm stays long through the arc.
            pole = fk_pole.lerp(Vector((-.3, sign * .3, -1.)).normalized(), w)
            poser.arm(side, fk_wrist.lerp(target, w), pole=tuple(pole))
            curl(side, -30 * w)  # fingers straight in line with the forearm: claws as the blade
            r = max(r, 0.)
        ik_goals()
        return r
    return pose, steps

# Low rake (2026-09-30): the claw slash for low targets - grass tufts and jars
# now, small rats later. The runtime picks it automatically when what the
# strike would reach is low, so it's the same button and timing. Same
# footwork and step-in as the slash; Chuck sinks into a crouch and folds
# forward from the hips while the wrist rides an arc from out and up on the
# striking side, down through the ground just ahead (about 8 cm up, 25 cm
# out), and across low to the far side. The arc is given as directions from
# the (moving) shoulder at 0.92 of the arm's reach, so it adapts to the pose.
SLASH_LOW_CROUCH = 7.   # cm the hips sink at the cut
def slash_low_clip(side):
    sign = 1 if side == 'L' else -1
    other = 'R' if side == 'L' else 'L'
    steps = {other: [(.06, .22, (RB[other][0] + SLASH_STEP, RB[other][1], RB[other][2]))],
             side: [(.28, .42, (RB[side][0] + SLASH_STEP, RB[side][1], RB[side][2]))]}
    dirs = [(.12, -.25, sign * .75, .05),
            (.17, .35, sign * .75, -.55),
            (.22, .62, sign * .12, -.78),
            (.27, .45, -sign * .55, -.72),
            (.34, .2, -sign * .65, -.6)]
    def pose(phase, f):
        t = f / FPS * SLASH_RATE
        wind = smoothstep(t, 0., .12) * (1 - smoothstep(t, .12, .22))
        strike = smoothstep(t, .12, .26) * (1 - smoothstep(t, .36, .54))
        low = smoothstep(t, .0, .16) * (1 - smoothstep(t, .34, .55))   # the crouch
        twist = sign * 22 * wind - sign * 30 * strike
        carriage(0., 0.)
        poser.translate('pelvis', (1.5 * low, 0, -SLASH_LOW_CROUCH * low - 1. * wind))
        poser.rotate('pelvis', 'Y', 14 * low)               # hips fold forward
        poser.rotate('pelvis', 'Z', .4 * twist)
        poser.rotate('spine_01', 'Y', 16 * low)
        poser.rotate('spine_02', 'Y', 8 * low)
        poser.rotate('spine_02', 'Z', .35 * twist)
        poser.rotate('chest', 'Z', .45 * twist)
        poser.rotate('chest', 'X', sign * 8 * strike)        # shoulder drops into the cut
        poser.rotate('head', 'Z', -.6 * twist)
        poser.rotate('neck', 'Y', -14 * low)                 # gaze on the ground just ahead, not the floor below
        poser.rotate('head', 'Y', -10 * low)
        poser.rotate(f'upperarm_{other}', 'Y', 20 * strike + 8 * wind)
        poser.rotate(f'upperarm_{other}', 'X', (1 if other == 'L' else -1) * 12 * low)   # out for balance
        poser.rotate(f'lowerarm_{other}', 'Y', -40 * (strike + .5 * wind))
        curl(other, 30 * strike)
        for i, b in enumerate(TAIL):
            poser.rotate(b, 'Z', -.25 * twist * (i + 1) / 3)
            poser.rotate(b, 'Y', -3 * low)                   # tail lifts a touch as he folds
        poser.update()
        d = slash_travel(t)
        r = 0.
        for s in 'LR':
            (x, y, lift), h, fp, tp = plan_foot(t, RB[s], steps[s])
            cx, cy = to_component(x, y, d, 0.)
            r = max(r, poser.leg(s, Vector((cx, cy, NEUTRAL_BALL[s].z + lift)), fp, tp, heading=h))
        w = smoothstep(t, .02, .1) * (1 - smoothstep(t, .36, .55))
        if w > 1e-3:
            fk_wrist = poser.head(f'hand_{side}')
            fk_elbow = poser.head(f'lowerarm_{side}')
            shoulder = poser.head(f'upperarm_{side}')
            reach = (fk_elbow - shoulder).length + (fk_wrist - fk_elbow).length
            u = min(max(t, dirs[0][0]), dirs[-1][0])
            target = shoulder + Vector(hermite(dirs, u)).normalized() * reach * .92
            fk_pole = (fk_elbow - (shoulder + fk_wrist) / 2).normalized()
            pole = fk_pole.lerp(Vector((-.2, sign * .6, -.4)).normalized(), w)   # elbow out: a raking arm
            poser.arm(side, fk_wrist.lerp(target, w), pole=tuple(pole))
            curl(side, -30 * w)
        ik_goals()
        return r
    return pose, steps

# Summon (2026-09-30): Chuck is a fey summon (References/Original/GAME-BIBLE.md:
# at zero Sanity he quietly disappears and returns at an Astral Anchor). He
# condenses out of the light curled low - deep crouch, back rounded, head
# down, arms folded in, tail wrapped round his feet - holds a breath, then
# rises unhurriedly into the aplomb stance, the head lifting last, and settles
# the jacket with a small roll of the shoulders. Played backward it is the
# vanish (sinking down into the light). Paws planted in the idle stance.
SUMMON_T = 1.6
SUMMON_CROUCH = 11.5  # cm the hips sink (13 put the rump 3 mm into the ground)
def summon(phase, f):
    t = f / FPS
    rise = smoothstep(t, .35, 1.1)
    fold = 1. - rise
    settle = smoothstep(t, 1.05, 1.25) * (1 - smoothstep(t, 1.3, 1.6))
    breath = math.sin(math.tau * t / 1.2) * fold
    carriage(0., 0., 0., .5 * breath)
    poser.translate('pelvis', (1.5 * fold, 0, -SUMMON_CROUCH * fold))
    poser.rotate('pelvis', 'Y', 22 * fold)
    poser.rotate('spine_01', 'Y', 18 * fold)
    poser.rotate('spine_02', 'Y', 12 * fold)
    poser.rotate('chest', 'Y', 8 * fold - 3 * settle)
    poser.rotate('chest', 'X', 2.5 * math.sin(math.tau * 2 * max(0., t - 1.05)) * settle)   # a shoulder roll
    head_curl = 1. - smoothstep(t, .55, 1.3)   # the head comes up last
    poser.rotate('neck', 'Y', 16 * head_curl)
    poser.rotate('head', 'Y', 14 * head_curl)
    for side, out in (('L', 1), ('R', -1)):
        poser.rotate(f'upperarm_{side}', 'Y', -38 * fold)          # arms folded in front
        poser.rotate(f'upperarm_{side}', 'X', -out * 14 * fold)
        poser.rotate(f'lowerarm_{side}', 'Y', -70 * fold)
        poser.rotate(f'hand_{side}', 'Y', 15 * fold)
        curl(side, 30 * fold)
    for i, b in enumerate(TAIL):                                     # wrapped round his feet
        poser.rotate(b, 'Z', 14 * fold * (1 if i < 3 else 1.3))
        poser.rotate(b, 'Y', -4 * fold)
    poser.update()
    r = plant_idle()
    for side in 'LR': poser.hand_goal(side)
    return r

# ---- parkour (user vision 2026-09-28: "feels like you're doing something
# difficult but it's not actually that hard"). Jump into a wall and Chuck runs
# up it for three steps; jump again to kick off it toward another wall.
# WallRun: loop of two steps up a wall in front of him. The runtime holds the
# capsule against the wall (its surface at x = capsule radius, 15 cm), so the
# paws plant on the plane x = WALL_X with their soles on it (toes up), and the
# loop phase follows the capsule's vertical travel (distance-matched, like the
# walk). Arms reach up alternately, head up, back slightly arched away.
WALL_X = 13.            # paw ball on the wall plane (capsule radius 15, 2 cm for the pad)
WALL_RUN = {'period_frames': 10, 'stride_cm': 30., 'stance_fraction': .55,
            'top_cm': 30., 'lift_off_cm': 4.}
WALL_PERIOD = WALL_RUN['period_frames'] / FPS

def wall_foot(phase):
    """(x, z, foot pitch, toe pitch) of the ball: planted on the wall and moving
    down relative to the rising body during stance; off the wall and up in swing."""
    st, top = WALL_RUN['stance_fraction'], WALL_RUN['top_cm']
    travel = WALL_RUN['stride_cm'] * st
    if phase < st:
        u = phase / st
        return WALL_X, top - travel * u, -90., -8.
    u = (phase - st) / (1 - st)
    e = smoothstep(u, 0., 1.)
    z = top - travel + travel * e
    return WALL_X - WALL_RUN['lift_off_cm'] * math.sin(math.pi * u), z, -90. + 35 * math.sin(math.pi * u), 10 * math.sin(math.pi * u)

def wall_run(phase, f):
    w = TAU * phase
    carriage(0., 0.)
    # Hips in toward the wall, a small bob per step; chest arched back a
    # touch so the head clears the wall; looking up the wall.
    poser.translate('pelvis', (4., .9, 1.5 + .8 * math.cos(2 * w)))
    poser.rotate('pelvis', 'X', 4.)  # cancels the idle hip tilt
    poser.rotate('pelvis', 'Z', 5 * math.cos(w))
    poser.rotate('spine_01', 'Y', -6)
    poser.rotate('chest', 'Z', -7 * math.cos(w))
    poser.rotate('neck', 'Y', -8)
    poser.rotate('head', 'Y', -16)
    for i, b in enumerate(TAIL):
        poser.rotate(b, 'Z', 6 * math.sin(w - .6 * (i + 1)))
    # Arms reach up the wall alternately, opposite the pushing leg.
    for side, sign in (('L', 1), ('R', -1)):
        reach = .5 + .5 * math.cos(w + (math.pi if side == 'L' else 0.))
        poser.rotate(f'upperarm_{side}', 'Y', -(70 + 60 * reach))
        poser.rotate(f'upperarm_{side}', 'X', sign * 12)
        poser.rotate(f'lowerarm_{side}', 'Y', -35 + 20 * reach)
        curl(side, -8)
    poser.update()
    r = 0.
    for side in 'LR':
        x, z, fp, tp = wall_foot(phase if side == 'L' else (phase + .5) % 1)
        ball = Vector((x, NEUTRAL_BALL[side].y, z))
        r = max(r, poser.leg(side, ball, fp, tp, pole=(.25, 0., 1.)))
    ik_goals()
    return r

# WallKick: the push off a wall now behind him (the runtime turns him to face
# the jump). Paws flat on the wall behind (toes down), knees bent, then the
# legs drive him off and tuck for the flight; arms swing forward and up.
WALL_KICK_FRAMES = 7
def wall_kick(phase, f):
    t = f / FPS
    drive = smoothstep(t, 0., .12)
    tuck = smoothstep(t, .1, .2)
    carriage(0., 0.)
    poser.translate('pelvis', (0, .9, 1. * drive))
    poser.rotate('pelvis', 'X', 4.)
    poser.rotate('spine_01', 'Y', 10 - 8 * drive)
    poser.rotate('head', 'Y', -6)
    for side, sign in (('L', 1), ('R', -1)):
        poser.rotate(f'upperarm_{side}', 'Y', 20 - 70 * drive + 30 * tuck)
        poser.rotate(f'upperarm_{side}', 'X', sign * 10)
        poser.rotate(f'lowerarm_{side}', 'Y', -40 + 15 * drive)
    for b in TAIL: poser.rotate(b, 'Y', 6 * drive)
    poser.update()
    r = 0.
    for side in 'LR':
        # On the wall behind (x = -WALL_X), pushing away, then tucked under the body.
        push = NEUTRAL_BALL[side] + Vector((-WALL_X - 3. - 8. * drive, 0., 10. - 4. * drive))
        tucked = NEUTRAL_BALL[side] + Vector((1.5, 0., 5.5))
        ball = push.lerp(tucked, tuck)
        r = max(r, poser.leg(side, ball, 90. * (1 - tuck) - 8 * tuck, 14 * tuck, pole=(1., 0., -.2)))
    ik_goals()
    return r

# ---- ledges (parkour phase 3). Hang: paws on a top edge in front of him.
# The runtime snugs the capsule to the wall (face at x = 15.5) with its centre
# HANG_DROP below the top, so in mesh space (origin = capsule bottom) the top
# edge is at z = HALF + HANG_DROP.
HALF = 32.5                 # capsule half height (mesh origin = capsule bottom)
HANG_DROP = 22.             # capsule centre below the top edge while hanging
FACE_X = 15.5               # wall face in front of him (capsule radius + 0.5)
TOP_Z = HALF + HANG_DROP    # the top edge in mesh space while hanging (54.5)
GRIP = {'L': Vector((FACE_X + 1.5, 9., TOP_Z + .8)), 'R': Vector((FACE_X + 1.5, -9., TOP_Z + .8))}

def hang_body(sway=0., pull=0.):
    """Hanging on the edge: chest in at the wall, arms up to the grip, paws
    scrabbling on the wall below, tail hanging. pull 0..1 bends the elbows."""
    carriage(0., 0.)
    poser.translate('pelvis', (3. + 2 * pull, .9, 1.5 * sway))
    poser.rotate('pelvis', 'X', 4.)
    poser.rotate('spine_01', 'Y', 6 + 6 * pull)
    poser.rotate('spine_02', 'Y', 4)
    poser.rotate('neck', 'Y', -10 - 6 * pull)
    poser.rotate('head', 'Y', -12)
    for i, b in enumerate(TAIL):
        poser.rotate(b, 'Z', 4 * sway * (i + 1) / 3)
    poser.update()

def grip_arms(world_offset=Vector((0, 0, 0)), weight=1.):
    """Wrists to the grip on the edge (shifted by the capsule's own motion so
    the paws stay put in the world); fingers curl over the edge."""
    for side, sign in (('L', 1), ('R', -1)):
        if weight <= 1e-3: continue
        fk = poser.head(f'hand_{side}')
        target = GRIP[side] - world_offset
        poser.arm(side, fk.lerp(target, weight), pole=(-.2, sign * .8, -.6))
        curl(side, 55 * weight)

def hang(phase, f):
    w = TAU * phase
    sway = math.sin(w)
    hang_body(sway)
    grip_arms()
    r = 0.
    for side in 'LR':
        # Paws against the wall below, one a little higher, shifting slowly.
        lift = 3. * math.sin(w + (0 if side == 'L' else math.pi))
        ball = Vector((FACE_X - 3., NEUTRAL_BALL[side].y, 17. + (4. if side == 'L' else 0.) + lift))
        r = max(r, poser.leg(side, ball, -75., -5., pole=(.3, 0., 1.)))
    ik_goals()
    return r

# PullUp: from the hang onto the top. The runtime moves the capsule along
# pull_path(t) (forward, up) from the hang position; paws and grip that are
# planted in the world are placed at (world - path) in mesh space.
PULL_T = .7
PULL_UP = HANG_DROP + HALF   # capsule rise: centre from top - drop to top + half
PULL_FWD = 34.               # capsule advance: from off the face to well onto the top
def pull_path(t):
    return Vector((PULL_FWD * smoothstep(t, .2, .6), 0., PULL_UP * smoothstep(t, .05, .5)))

def pull_up(phase, f):
    t = f / FPS
    off = pull_path(t)
    pull = smoothstep(t, 0., .25) * (1 - smoothstep(t, .3, .5))
    over = smoothstep(t, .2, .45) * (1 - smoothstep(t, .5, .7))
    stand = smoothstep(t, .45, .7)
    hang_body(0., pull)
    # Chest drives forward over the edge, then he stands up to the aplomb stance.
    poser.rotate('spine_01', 'Y', 14 * over - 6 * stand - 6 * pull * 0)
    poser.rotate('head', 'Y', 10 * stand)
    poser.translate('pelvis', (-3. * stand, 0, 0))
    poser.update()
    grip_arms(off, 1 - smoothstep(t, .28, .45))
    r = 0.
    top_plant = {s: Vector((PULL_FWD + RB[s][0], RB[s][1], TOP_Z + NEUTRAL_BALL[s].z)) for s in 'LR'}  # standing ball height on the top
    for side in 'LR':
        hanging = Vector((FACE_X - 3., NEUTRAL_BALL[side].y, 17. + (4. if side == 'L' else 0.)))
        if side == 'L':   # the knee comes up over the edge; the paw plants on the top
            a, b = .3, .58
            mid = Vector((8., NEUTRAL_BALL[side].y, 16.))
        else:             # the trailing paw steps up to its stance on the top
            a, b = .5, .65
            mid = None
        planted = top_plant[side] - off
        if t <= a: ball, fp = hanging, -75.
        elif t >= b: ball, fp = planted, 0.
        else:
            u = smoothstep(t, a, b)
            start = hanging
            ball = start.lerp(planted, u) + (Vector((0, 0, 10. * math.sin(math.pi * u))) if mid is None else (mid - start.lerp(planted, .5)) * math.sin(math.pi * u))
            fp = -75. * (1 - u)
        r = max(r, poser.leg(side, ball, fp, 0., pole=(1., 0., .3), heading=RB[side][2] * smoothstep(t, a, b)))
    ik_goals()
    return r

# Mantle: onto a knee-high ledge in his way (auto, from walking or standing).
# Authored for a 25 cm step; the runtime moves the capsule up the real step
# height and MANTLE_FWD past the face over MANTLE_T.
MANTLE_T, MANTLE_H, MANTLE_FWD = .35, 25., 28.
def mantle_path(t):
    return Vector((MANTLE_FWD * smoothstep(t, .08, .33), 0., MANTLE_H * smoothstep(t, .05, .25)))

def mantle(phase, f):
    t = f / FPS
    off = mantle_path(t)
    hop = smoothstep(t, 0., .12) * (1 - smoothstep(t, .2, .35))
    carriage(0., 0.)
    poser.translate('pelvis', (0, 0, -3. * hop))
    poser.rotate('spine_01', 'Y', 14 * hop)
    poser.rotate('head', 'Y', -8 * hop)
    poser.update()
    # Paws on the top for the hop (planted in the world), then away.
    face = 18.   # the ledge face from the capsule at the start (mantle begins within reach)
    grip = {s: Vector((face + 4., sign * 8., MANTLE_H + .8)) for s, sign in (('L', 1), ('R', -1))}
    wgt = smoothstep(t, 0., .06) * (1 - smoothstep(t, .18, .28))
    for side, sign in (('L', 1), ('R', -1)):
        if wgt > 1e-3:
            fk = poser.head(f'hand_{side}')
            poser.arm(side, fk.lerp(grip[side] - off, wgt), pole=(-.2, sign * .8, -.6))
            curl(side, 30 * wgt)
    r = 0.
    for side in 'LR':
        x, y, h = RB[side]
        start = Vector((x, y, NEUTRAL_BALL[side].z))
        final = Vector((x + MANTLE_FWD, y, NEUTRAL_BALL[side].z + MANTLE_H))
        a, b = (.03, .22) if side == 'L' else (.05, .3)  # a two-footed hop: both paws leave early
        u = smoothstep(t, a, b)
        world = start.lerp(final, u) + Vector((0, 0, 8. * math.sin(math.pi * u)))
        r = max(r, poser.leg(side, world - off, 20 * math.sin(math.pi * u), 10 * math.sin(math.pi * u), heading=h))
    ik_goals()
    return r

# Shimmy (parkour phase 4): hand over hand along the edge while hanging. A
# loop; each paw grips for half the cycle (world-locked, so it slides back in
# mesh space as the capsule moves) and reaches ahead for the other half. The
# runtime advances the phase by the capsule's sideways travel over the stride.
SHIMMY_STRIDE, SHIMMY_FRAMES = 16., 12
def shimmy_clip(sign):
    """sign +1: toward Chuck's left (+Y), -1: toward his right."""
    def pose(phase, f):
        w = TAU * phase
        hang_body(.5 * math.sin(2 * w))
        poser.rotate('chest', 'Z', sign * 3.)   # leaning into the travel
        poser.update()
        quarter = SHIMMY_STRIDE / 4
        for side, nominal, out in (('L', 9., 1), ('R', -9., -1)):
            lead = (side == 'L') == (sign > 0)
            p = phase if lead else (phase + .5) % 1
            if p < .5:   # gripping: slides from ahead to behind as he moves
                u = p / .5
                y = nominal + sign * quarter - sign * 2 * quarter * u
                reach = 0.
            else:        # reaching ahead to the next grip
                u = (p - .5) / .5
                y = nominal - sign * quarter + sign * 2 * quarter * smoothstep(u, 0., 1.)
                reach = math.sin(math.pi * u)
            target = Vector((FACE_X + 1.5 - 2. * reach, y, TOP_Z + .8 + 3. * reach))
            poser.arm(side, target, pole=(-.2, out * .8, -.6))
            curl(side, 55 - 35 * reach)
        r = 0.
        for side in 'LR':
            shuffle = math.sin(w + (0 if side == 'L' else math.pi))
            ball = Vector((FACE_X - 3., NEUTRAL_BALL[side].y + sign * 2. * shuffle, 17. + (4. if side == 'L' else 0.) + 2. * max(0., shuffle)))
            r = max(r, poser.leg(side, ball, -75., -5., pole=(.3, 0., 1.)))
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
        'launch_walking': side_launch(SIDE_SHORT), 'launch_running': side_launch(SIDE_LONG),
        'stance_intervals_s': {'foot_L': [[0., SIDE_TAKEOFF], [SIDE_LAND, SIDE_T]], 'foot_R': [[0., SIDE_TAKEOFF], [SIDE_LAND, SIDE_T]]},
        'events_s': {'takeoff': SIDE_TAKEOFF, 'land': SIDE_LAND, 'settled': SIDE_T},
        'notes': 'Side jump from and back to the aplomb stance, facing unchanged. Root fixed: the runtime launches the capsule sideways at takeoff, holds the clip just before land while airborne, and stops the capsule at touchdown (the clip carries the momentum in the hips).'})

for name, side in (('SlashRight', 'R'), ('SlashLeft', 'L')):
    fn, steps = slash_clip(side)
    frames = round(SLASH_T / SLASH_RATE * FPS) + 1
    fast = lambda v: round(v / SLASH_RATE, 4)
    author(name, frames, fn, False, {
        'capsule_travel_cm_per_frame': [round(slash_travel(f / FPS * SLASH_RATE), 4) for f in range(frames)],
        'travel_cm': SLASH_STEP, 'duration_travel_s': fast(SLASH_T), 'playback_rate_vs_base': SLASH_RATE,
        'stance_intervals_s': {f'foot_{s}': [[fast(a), fast(b)] for a, b in stance_from_steps(steps[s], SLASH_T)] for s in 'LR'},
        # Chain once the arc has crossed; the runtime adds a random pause for a natural flurry.
        'events_s': {k: fast(v) for k, v in {'wind_up': .12, 'strike': .22, 'follow_through': .34, 'chain_from': .3, 'recovered': SLASH_T}.items()},
        'striking_paw': f'hand_{side}',
        'upper_body_root': 'spine_01',
        'notes': 'Claw slash from and back to the aplomb stance. Standing: the runtime moves the capsule along capsule_travel_cm_per_frame. Moving: the runtime plays the spine_01 subtree over the stride. Chains into the other paw.'})
for name, side in (('SlashLowRight', 'R'), ('SlashLowLeft', 'L')):
    fn, steps = slash_low_clip(side)
    frames = round(SLASH_T / SLASH_RATE * FPS) + 1
    fast = lambda v: round(v / SLASH_RATE, 4)
    author(name, frames, fn, False, {
        'capsule_travel_cm_per_frame': [round(slash_travel(f / FPS * SLASH_RATE), 4) for f in range(frames)],
        'travel_cm': SLASH_STEP, 'duration_travel_s': fast(SLASH_T), 'playback_rate_vs_base': SLASH_RATE,
        'stance_intervals_s': {f'foot_{s}': [[fast(a), fast(b)] for a, b in stance_from_steps(steps[s], SLASH_T)] for s in 'LR'},
        'events_s': {k: fast(v) for k, v in {'wind_up': .12, 'strike': .22, 'follow_through': .34, 'chain_from': .3, 'recovered': SLASH_T}.items()},
        'striking_paw': f'hand_{side}', 'crouch_cm': SLASH_LOW_CROUCH,
        'upper_body_root': 'spine_01',
        'notes': 'Low rake: the claw slash for low targets (grass, jars, small rats), chosen by the runtime. Same timing, footwork and travel as the slash; crouched and folded forward, the wrist sweeping through the ground just ahead.'})
author('Summon', round(SUMMON_T * FPS) + 1, summon, False, {
    'events_s': {'rise_start': .35, 'standing': 1.1, 'settled': SUMMON_T}, 'crouch_cm': SUMMON_CROUCH,
    'stance_intervals_s': {'foot_L': [[0., SUMMON_T]], 'foot_R': [[0., SUMMON_T]]},
    'notes': 'Fey summon: curled low as he condenses out of the light, then rises into the aplomb stance and settles the jacket. Played backward by the runtime for the vanish at zero Sanity. Paws planted in the idle stance throughout.'})

author('WallRun', WALL_RUN['period_frames'], wall_run, True, {
    'stride_cycle_cm': WALL_RUN['stride_cm'], 'stance_fraction': WALL_RUN['stance_fraction'],
    'wall_plane_x_cm': WALL_X, 'axis': 'vertical: the planted paws move down in component space as the capsule rises',
    'stance_intervals_s': {'foot_L': [[0., round(WALL_RUN['stance_fraction'] * WALL_PERIOD, 4)]],
                           'foot_R': [[round(.5 * WALL_PERIOD, 4), round((.5 + WALL_RUN['stance_fraction']) * WALL_PERIOD, 4)]]},
    'notes': 'Run up a wall in front (surface at the capsule radius). Loop phase follows vertical capsule travel over stride_cycle_cm; paws on the wall plane with soles on it.'})
author('WallKick', WALL_KICK_FRAMES, wall_kick, False, {
    'stance_intervals_s': {'foot_L': [], 'foot_R': []},
    'events_s': {'launch': 0., 'tucked': .2},
    'notes': 'Push off a wall behind him (the runtime turns Chuck to face the jump), then tuck for the flight.'})

author('Hang', 36, hang, True, {
    'hang_drop_cm': HANG_DROP, 'wall_face_x_cm': FACE_X, 'grip_z_cm': TOP_Z,
    'stance_intervals_s': {'foot_L': [], 'foot_R': []},
    'notes': 'Hanging from a top edge in front: capsule snug to the wall, centre hang_drop_cm below the top; paws on the edge, feet on the wall below.'})
pull_frames = round(PULL_T * FPS) + 1
author('PullUp', pull_frames, pull_up, False, {
    'capsule_path_cm_per_frame': [[round(v, 4) for v in (pull_path(f / FPS).x, pull_path(f / FPS).z)] for f in range(pull_frames)],
    'path_axes': ['forward (into the wall top)', 'up'], 'rise_cm': PULL_UP, 'advance_cm': PULL_FWD,
    'stance_intervals_s': {'foot_L': [], 'foot_R': []},
    'events_s': {'grip_release': .45, 'knee_on_top': .45, 'standing': PULL_T},
    'notes': 'From Hang onto the top edge; the runtime moves the capsule along capsule_path_cm_per_frame.'})
mantle_frames = round(MANTLE_T * FPS) + 1
author('Mantle', mantle_frames, mantle, False, {
    'capsule_path_cm_per_frame': [[round(v, 4) for v in (mantle_path(f / FPS).x, mantle_path(f / FPS).z)] for f in range(mantle_frames)],
    'path_axes': ['forward', 'up'], 'reference_step_cm': MANTLE_H, 'advance_cm': MANTLE_FWD,
    'stance_intervals_s': {'foot_L': [], 'foot_R': []},
    'notes': 'Hop onto a knee-high ledge in his way; the runtime scales the rise to the real step height.'})

# Speed vault (user 2026-10-04, References: the running rat clearing a stone
# wall with one paw on its top): out of a run, over a low thin obstacle (a
# bench, a low wall) without breaking stride. He tips onto his left side over
# his planted left paw, the legs tucked together and swung past on the right,
# the free right arm out for balance, eyes ahead; then rights himself and lands
# on the left paw at RunLoop frame 0, running on. Authored for a 40 cm top, 35
# cm deep, its face 45 cm ahead of the capsule centre; the capsule clears the
# top by VAULT_CLEAR. The runtime measures the real obstacle and moves the
# capsule along the same shape (vault_path): forward evenly, up onto a plateau
# over the top, down again; clip time = progress x duration.
VAULT_T, VAULT_H, VAULT_FACE, VAULT_DEPTH, VAULT_FWD, VAULT_CLEAR = .5, 40., 45., 35., 125., 6.
def vault_plateau(u):
    return smoothstep(u, 0., .26) * (1 - smoothstep(u, .74, 1.))

def vault_path(t):
    u = min(1., t / VAULT_T)
    return Vector((VAULT_FWD * u, 0., (VAULT_H + VAULT_CLEAR) * vault_plateau(u)))

def speed_vault(phase, f):
    t = f / FPS; u = min(1., t / VAULT_T)
    off = vault_path(t)
    over = smoothstep(u, .1, .36) * (1 - smoothstep(u, .6, .92))     # tipped sideways over the top
    plant = smoothstep(u, .1, .24) * (1 - smoothstep(u, .52, .66))   # left paw on the top
    tuck = smoothstep(u, .04, .3) * (1 - smoothstep(u, .66, .97))    # legs together, swung right
    run_carriage(0.)
    # The body dips low over the top (the capsule passes clear above it), hips
    # rolled onto the left side, the legs swinging round to the right.
    # Chest first, nearly flat over it (the reference), rolled onto the left side.
    poser.translate('pelvis', (3. * over, -3. * over, -15. * over))
    poser.rotate('pelvis', 'Y', 26 * over)
    poser.rotate('pelvis', 'X', -48 * over)
    poser.rotate('pelvis', 'Z', -14 * over)
    poser.rotate('spine_01', 'Y', 10 * over)
    poser.rotate('chest', 'X', 18 * over)       # shoulders come back toward level
    poser.rotate('chest', 'Y', 8 * over)
    poser.rotate('neck', 'Y', -14 * over)
    poser.rotate('neck', 'X', 12 * over)
    poser.rotate('head', 'Y', -22 * over)       # chin up: eyes on where he's going
    poser.rotate('head', 'X', 16 * over)
    poser.rotate('head', 'Z', 14 * over)
    poser.rotate('upperarm_R', 'X', -55 * over)  # the free arm flung out and back for balance
    poser.rotate('upperarm_R', 'Y', 45 * over)
    poser.rotate('lowerarm_R', 'Y', -12 * over)
    curl('R', 30 * over)
    for i, b in enumerate(TAIL):
        poser.rotate(b, 'Z', -10 * over * (i + 1) / 3)   # the tail swings opposite the legs
        poser.rotate(b, 'Y', -6 * over)
    poser.update()
    # The left paw planted on the top, world-locked while the body passes over.
    grip = Vector((VAULT_FACE + VAULT_DEPTH * .5, 7., VAULT_H + .8))
    if plant > 1e-3:
        fk = poser.head('hand_L')
        poser.arm('L', fk.lerp(grip - off, plant), pole=(-.3, .8, -.5))
        curl('L', 28 * plant)
    r = 0.
    for side in 'LR':
        x, lift, fp, tp = run_foot(0. if side == 'L' else .5)
        stride = NEUTRAL_BALL[side] + Vector((x, -(1. if side == 'L' else -1.), lift))
        tucked = NEUTRAL_BALL[side] + Vector((-8. if side == 'L' else -14., -14. if side == 'L' else -10., 16. if side == 'L' else 12.))   # together, trailing behind on the right
        ball = stride.lerp(tucked, tuck)
        r = max(r, poser.leg(side, ball, fp * (1 - tuck) + 35 * tuck, tp * (1 - tuck) + 15 * tuck,
                             pole=(1., -.6 * tuck, 0.), heading=TOE_OUT[side] * .4 * (1 - tuck) - 25 * tuck))
    ik_goals()
    return r

vault_frames = round(VAULT_T * FPS) + 1
author('SpeedVault', vault_frames, speed_vault, False, {
    'capsule_path_cm_per_frame': [[round(v, 4) for v in (vault_path(f / FPS).x, vault_path(f / FPS).z)] for f in range(vault_frames)],
    'path_axes': ['forward', 'up'], 'reference_height_cm': VAULT_H, 'reference_face_cm': VAULT_FACE,
    'reference_depth_cm': VAULT_DEPTH, 'advance_cm': VAULT_FWD, 'clearance_cm': VAULT_CLEAR,
    'stance_intervals_s': {'foot_L': [], 'foot_R': []},
    'events_s': {'paw_plant': round(.24 * VAULT_T, 4), 'paw_release': round(.52 * VAULT_T, 4), 'touchdown': VAULT_T},
    'ends_on': 'RunLoop frame 0 (foot_L touchdown)',
    'notes': 'Speed vault over a low thin obstacle out of a run; the runtime fits the capsule path to the real obstacle (clip time = progress).'})

for name, sign in (('ShimmyLeft', 1), ('ShimmyRight', -1)):
    author(name, SHIMMY_FRAMES, shimmy_clip(sign), True, {
        'stride_cycle_cm': SHIMMY_STRIDE, 'hang_drop_cm': HANG_DROP,
        'stance_intervals_s': {'foot_L': [], 'foot_R': []},
        'notes': 'Hand over hand along a hang edge toward Chuck\'s ' + ('left' if sign > 0 else 'right') + '; loop phase follows the capsule\'s sideways travel over stride_cycle_cm.'})

# ---- strafe (user 2026-09-29: Counter-Strike-style strafe on Q/E, jump while
# strafing = side jump): facing held down the camera, stepping sideways.
# Walk: a step-together sidestep (lead paw steps out, the trail paw closes
# straight after, a beat of double support), no crossing. Run: the same
# step-close as a bounding shuffle with a flight phase (a chassé). A lateral
# stride is limited by how far a 20 cm leg can reach sideways, so the paws
# travel at most about 7 cm either side of neutral in stance, and the hips sink
# into an athletic crouch to give the legs room; cadence makes up the speed.
# Phase 0 = lead paw touchdown; the trail paw lands `lag` of a cycle later.
STRAFE = {'speed_cm_s': 55., 'period_frames': 12, 'stance_fraction': .65, 'lag': .35,
          'crouch_cm': 2.5, 'lift_cm': 2.2, 'bounce_cm': .5, 'widen_cm': 1.}
STRAFE_RUN = {'speed_cm_s': 150., 'period_frames': 10, 'stance_fraction': .3, 'lag': .15,
              'crouch_cm': 3.5, 'lift_cm': 4.5, 'bounce_cm': 1.4, 'widen_cm': 1.5}

def strafe_clip(sign, g):
    """sign +1: toward Chuck's left (+Y), -1: right. g: STRAFE or STRAFE_RUN."""
    lead, trail = ('L', 'R') if sign > 0 else ('R', 'L')
    period = g['period_frames'] / FPS
    st = g['stance_fraction']
    half = g['speed_cm_s'] * period * st / 2      # stance travel either side of neutral
    run = g is STRAFE_RUN
    def foot(p):
        """(lateral offset along the travel, lift, heel lift) at this paw's phase."""
        if p < st:
            u = p / st
            return half - 2 * half * u, 0., 14 * smoothstep(u, .75, 1.)
        u = (p - st) / (1 - st)
        return -half + 2 * half * smoothstep(u, 0., 1.), g['lift_cm'] * math.sin(math.pi * u), 14 * (1 - smoothstep(u, 0., .5))
    def pose(phase, f):
        w = TAU * phase
        mid = st * .5 + g['lag'] * .5       # centre of the step-close (lowest)
        carriage(0., 0.)
        # Undo the idle contrapposto shift: a square, athletic base.
        poser.translate('pelvis', (0, .9 - sign * .35 * math.cos(w - TAU * mid),
                                   .45 - g['crouch_cm'] - g['bounce_cm'] * math.cos(TAU * (phase - mid))))
        poser.rotate('pelvis', 'X', 4 - sign * (4. if run else 2.))     # lean into the travel
        poser.rotate('pelvis', 'Y', 8. if run else 3.)                  # hips back into the crouch
        poser.rotate('spine_01', 'Y', (2. if run else -1.))
        poser.rotate('chest', 'X', -3 + sign * (2. if run else 1.))
        poser.rotate('head', 'X', sign * (2. if run else 1.))           # gaze stays level
        poser.rotate('head', 'Y', -(6. if run else 2.))
        poser.rotate('chest', 'Z', sign * 4.)                           # shoulders open a touch toward the travel
        poser.rotate('head', 'Z', -sign * 4.)
        for side, out in (('L', 1), ('R', -1)):
            bob = math.cos(w - TAU * mid)
            poser.rotate(f'upperarm_{side}', 'X', out * (6. if run else 3.) + out * 1.5 * bob)
            poser.rotate(f'upperarm_{side}', 'Y', -(14. if run else 4.))
            poser.rotate(f'lowerarm_{side}', 'Y', -(46. if run else 18.))
            curl(side, 18 if run else 6)
        for i, b in enumerate(TAIL):
            poser.rotate(b, 'Z', -sign * (3. if run else 1.5) * (i + 1) / 3 + 2. * math.sin(w - .7 * (i + 1)))
            if run: poser.rotate(b, 'Y', 3. + 2. * math.sin(w - .8 * (i + 1)))
        poser.update()
        r = 0.
        for side in 'LR':
            p = phase if side == lead else (phase - g['lag']) % 1
            off, lift, heel = foot(p)
            wide = g['widen_cm'] * (1 if side == 'L' else -1)
            ball = NEUTRAL_BALL[side] + Vector((0., wide + sign * off, lift))
            r = max(r, poser.leg(side, ball, heel, 0., heading=TOE_OUT[side] * .5))
        for side in 'LR': poser.hand_goal(side)
        return r
    def stance(side):
        a = 0. if side == lead else g['lag']
        b = a + st
        spans = [(a, b)] if b <= 1 else [(a, 1.), (0., b - 1)]
        return [[round(x * period, 4), round(y * period, 4)] for x, y in sorted(spans)]
    return pose, {'foot_L': stance('L'), 'foot_R': stance('R')}, half

for name, sign, g in (('StrafeLeft', 1, STRAFE), ('StrafeRight', -1, STRAFE),
                      ('StrafeRunLeft', 1, STRAFE_RUN), ('StrafeRunRight', -1, STRAFE_RUN)):
    fn, spans, half = strafe_clip(sign, g)
    period = g['period_frames'] / FPS
    author(name, g['period_frames'], fn, True, {
        'reference_speed_cm_s': g['speed_cm_s'], 'stride_cycle_cm': round(g['speed_cm_s'] * period, 3),
        'stance_fraction': g['stance_fraction'], 'trail_lag_fraction': g['lag'],
        'stance_travel_cm': round(2 * half, 3), 'stance_intervals_s': spans,
        'direction': 'source +Y (Chuck left)' if sign > 0 else 'source -Y (Chuck right)',
        'phase_convention': 'phase 0 = lead paw touchdown; the trail paw closes trail_lag_fraction later',
        'notes': ('Bounding shuffle with a flight phase' if g is STRAFE_RUN else 'Step-together sidestep, no crossing')
                 + ', facing unchanged. Root fixed; the runtime advances the phase by sideways capsule travel over stride_cycle_cm.'})


# ---- sprint (user 2026-10-07: "sprint using all fours like a rat sprinting",
# a brief burst with a cooldown). Down onto all four paws in a half-bound, the
# rat's fast gait: the hind paws land almost together and drive, the body
# stretches out through a short extended flight, the forepaws land one after
# the other and pull, then the hind legs swing past them in the gathered
# flight with the back arched. Back low and long, head up and level, tail
# streaming straight back. 380 cm/s (1.7x the run) over an 8-frame stride
# (101 cm, about two body lengths, as galloping quadrupeds do). Hind stance
# 25% and fore stance 20% of the stride, each paw moving back at exactly the
# reference speed in component space while planted.
SPRINT = {'speed_cm_s': 380.0, 'period_frames': 8, 'hind_stance': .25, 'fore_stance': .2,
          'hind_lag': .07, 'fore_land': .44, 'fore_lag': .08}
SPRINT_PERIOD = SPRINT['period_frames'] / FPS
SPRINT_STRIDE = SPRINT['speed_cm_s'] * SPRINT_PERIOD
SPRINT_HIND_CM = SPRINT_STRIDE * SPRINT['hind_stance']
SPRINT_FORE_CM = SPRINT_STRIDE * SPRINT['fore_stance']
SPRINT_HIND_X = -2.5   # hind stance centre, forward of the neutral ball (cm)
SPRINT_FORE_Y = 8.5    # forepaws this far either side of the midline
SPRINT_WRIST_Z = 2.2   # wrist height with the palm flat on the ground

def sprint_hind(phase):
    """(forward offset of the ball from neutral, lift, foot pitch, toe pitch)."""
    st = SPRINT['hind_stance']; half = SPRINT_HIND_CM / 2
    if phase < st:
        u = phase / st
        return SPRINT_HIND_X + half - SPRINT_HIND_CM * u, 0., 30 + 34 * smoothstep(u, .35, 1.), -10 * smoothstep(u, .35, 1.)
    u = (phase - st) / (1 - st)
    # Kicked out behind, then folded up under the belly and swung far forward
    # past the forepaws' prints in the gathered flight, reaching down to land.
    keys = [(0., -half, 0., 64., -10.), (.15, -half - 4., 4., 80., 10.), (.42, -half + 4., 9., 70., 35.),
            (.72, half + 3., 8., 30., 15.), (.9, half + 2., 3., 26., 0.), (1., half, 0., 30., 0.)]
    ground = -SPRINT_HIND_CM / st * (1 - st)
    x, lift, fp, tp = hermite(keys, u, ground)
    return SPRINT_HIND_X + x, max(0., lift), fp, tp

def sprint_fore(phase):
    """(forward offset of the wrist from the shoulder line, lift, stance 0..1)."""
    st = SPRINT['fore_stance']; half = SPRINT_FORE_CM / 2
    if phase < st:
        u = phase / st
        return half - SPRINT_FORE_CM * u, 0., 1.
    u = (phase - st) / (1 - st)
    # Peel off behind, tuck up under the chest, reach well out in front.
    keys = [(0., -half, 0.), (.18, -half - 3., 3.5), (.45, -2., 8.), (.78, half + 5., 5.), (1., half, 0.)]
    ground = -SPRINT_FORE_CM / st * (1 - st)
    x, lift = hermite(keys, u, ground)
    return x, max(0., lift), 0.

def sprint(phase, f, leap=0.):
    """leap 0..1: stretched out in the sprint leap (0 = the plain stride)."""
    w = TAU * phase
    # Spine: gathered (hips tucked, back arched up) as the hind paws land,
    # stretched out as the forepaws reach for the ground.
    gather = math.cos(w - TAU * .02)          # +1 gathered, -1 extended
    gather += (-1.4 - gather) * leap          # the leap: stretched longer still
    # Lowest through each stance, highest in the two flights.
    bob = .5 * (math.cos(2 * (w - TAU * .1)) + 1)
    poser.translate('pelvis', (2., 0, -5.5 - 1.6 * bob + 2.5 * leap))
    poser.rotate('pelvis', 'Y', 62 + 6 * gather)
    poser.rotate('spine_01', 'Y', 10 + 3 * gather)
    poser.rotate('spine_02', 'Y', 10 + 2 * gather)
    poser.rotate('chest', 'Y', 6 - 4 * gather)
    poser.rotate('pelvis', 'Z', 2.5 * math.sin(w))
    # Neck and head come back up so the eyes look straight down the dock,
    # countering the spine's pitch so the head rides nearly level.
    poser.rotate('chest', 'Y', -3 * gather)
    poser.rotate('neck', 'Y', -44 - 9 * gather)
    poser.rotate('head', 'Y', -42 - 4 * gather + 2 * math.cos(2 * w) - 5 * leap)   # eyes on the landing
    for side, sign in (('L', 1), ('R', -1)):
        poser.rotate(f'ear_{side}', 'Y', 14 + 10 * leap)   # ears laid back (flat in the leap)
    # Tail streams out straight behind, low, a small wave through it; in the
    # leap it lifts a little for balance.
    for i, (b, deg) in enumerate(zip(TAIL, (-48, -4, -2, 0, 1, 1))):
        poser.rotate(b, 'Y', deg + 3 * math.sin(w - .9 * (i + 1)) + (10 if i == 0 else 2) * leap)
        poser.rotate(b, 'Z', 3 * math.sin(w - .7 * (i + 1)))
    poser.update()
    # Forepaws: two-bone arm IK to a wrist target under the shoulder line,
    # elbows back like a quadruped's, palms flat and fingers forward in
    # stance, the paw curled back under in swing.
    for side, sign in (('L', 1), ('R', -1)):
        land = SPRINT['fore_land'] + (SPRINT['fore_lag'] if side == 'L' else 0.)
        x, lift, plant = sprint_fore((phase - land) % 1)
        # The leap: both forepaws reach well out ahead, open, ready to land.
        x += (SPRINT_FORE_CM / 2 + 5. - x) * leap
        lift += (6. - lift) * leap
        # Shoulder blade: reaches forward with the paw, drawn back in stance.
        poser.translate(f'clavicle_{side}', (.12 * x, 0, -1.5))
        poser.update()
        shoulder = poser.head(f'upperarm_{side}')
        wrist = Vector((shoulder.x + 2. + x, sign * SPRINT_FORE_Y, SPRINT_WRIST_Z + lift))
        poser.arm(side, wrist, pole=(-1., .35 * sign, .25))
        curl_back = (1. - plant) * (1. - leap)
        # At full reach the solver stops short of the target: the paw stays on the arm.
        poser.aim(f'hand_{side}', poser.head(f'hand_{side}'), Vector((1., 0., -.05)).lerp(Vector((-.4, 0., -1.)), curl_back * smoothstep(lift, 0., 3.)))
        curl(side, 12 + 40 * curl_back)
    r = 0.
    for side in 'LR':
        x, lift, fp, tp = sprint_hind((phase - (0. if side == 'L' else SPRINT['hind_lag'])) % 1)
        # The leap: hind legs stretched out behind from the push, toes pointed.
        x += (SPRINT_HIND_X - SPRINT_HIND_CM / 2 - 12. - x) * leap
        lift += (8. - lift) * leap
        fp += (85. - fp) * leap
        tp += (35. - tp) * leap
        ball = NEUTRAL_BALL[side] + Vector((x, 0., lift))
        r = max(r, poser.leg(side, ball, fp, tp, heading=0.))
    ik_goals()
    return r

def sprint_stance(start, frac):
    a, b = start % 1, (start + frac) % 1
    spans = [(a, b)] if a < b else [(a, 1.), (0., b)]
    return [[round(x * SPRINT_PERIOD, 4), round(y * SPRINT_PERIOD, 4)] for x, y in sorted(spans)]

author('SprintLoop', SPRINT['period_frames'], sprint, True, {
    'reference_speed_cm_s': SPRINT['speed_cm_s'], 'stride_cycle_cm': round(SPRINT_STRIDE, 3),
    'stance_fraction': SPRINT['hind_stance'], 'fore_stance_fraction': SPRINT['fore_stance'],
    'stance_intervals_s': {'foot_L': sprint_stance(0., SPRINT['hind_stance']),
                           'foot_R': sprint_stance(SPRINT['hind_lag'], SPRINT['hind_stance']),
                           'hand_L': sprint_stance(SPRINT['fore_land'] + SPRINT['fore_lag'], SPRINT['fore_stance']),
                           'hand_R': sprint_stance(SPRINT['fore_land'], SPRINT['fore_stance'])},
    'hind_lag_fraction': SPRINT['hind_lag'],
    'phase_convention': 'phase 0 = foot_L touchdown, foot_R hind_lag_fraction later; forepaws land fore_land (R) and fore_land + fore_lag (L); the runtime advances the phase by travel over stride_cycle_cm',
    'notes': 'Four-legged half-bound sprint. In place (root fixed); planted hind balls and forepaw wrists move backward in component space at exactly the reference speed. Forepaws are placed by the clip only (no runtime hand IK).'})

# Sprint leap (user 2026-10-07: "jumping while sprinting ... a leap that is
# much further than a running jump while still being believable"). Out of the
# gallop he pushes off to 420 cm/s with 250 cm/s of lift (the running jump: 225
# and 190): about 0.64 s in the air, some 2.7 m (two and a half times the running
# jump's 1.1 m, about four of his heights) with a 40 cm apex. User follow-up the
# same day: "slightly further, long enough that there is at least one roof to
# roof jump" (was 380/230: 2.4 m, 36 cm) - the workshop roofs' 1.6 m gaps with
# a 45 cm step now go either way, down by landing and up by scrambling over. Posed over
# the flight's normalized progress like RunJump: the stride runs on from the
# end of the hind paws' push (phase .2) to the forepaws' touchdown (fore_land),
# and over the middle of the flight he stretches out long - forepaws reaching
# ahead, hind legs trailing, ears flat, tail lifted - then gathers to land on
# the forepaws, so the runtime carries straight on into the gallop.
SPRINT_LEAP_FRAMES = 18
SPRINT_LEAP_VZ = 250.
SPRINT_LEAP_SPEED = 420.   # the push-off: a little faster than the gallop

def sprint_leap(phase, f):
    u = f / (SPRINT_LEAP_FRAMES - 1)
    leap = smoothstep(u, 0., .25) * (1 - smoothstep(u, .6, 1.))
    return sprint(SPRINT['fore_land'] - .24 * (1 - u), f, leap)

author('SprintLeap', SPRINT_LEAP_FRAMES, sprint_leap, False, {
    'launch': {'vertical_cm_s': SPRINT_LEAP_VZ, 'horizontal_cm_s': SPRINT_LEAP_SPEED, 'gravity_cm_s2': 980. * .8},
    'time_mapping': 'clip time = flight progress (vz0 - vz) / (2 vz0) x duration; last frame = SprintLoop at fore_land_phase',
    'fore_land_phase': SPRINT['fore_land'],
    'stance_intervals_s': {'foot_L': [], 'foot_R': []},
    'events_s': {'takeoff': 0., 'touchdown': round((SPRINT_LEAP_FRAMES - 1) / FPS, 4)},
    'ends_on': 'SprintLoop at fore_land_phase (forepaw R touchdown)',
    'notes': 'Leap out of the sprint: stretched out long in the air, forepaws reaching, hind legs trailing; lands on the forepaws into the gallop.'})

# ---- brachiation (user 2026-10-09): swinging from the iron rings under the
# high wall lanterns, ring to ring, "look hard, be easy". Swing: both paws
# round the ring's bottom bar overhead, arms nearly straight, the body hanging
# from them. The runtime swings the whole mesh about the grip as a pendulum
# (plus the constant tilt that puts the capsule centre under the grip), so the
# clip only carries what the body does through the arc: posed by swing angle,
# clip time = (angle / SWING_MAX + 1) / 2 x duration. At the front of the arc
# (feet ahead) he pikes, knees up; at the back he arches, legs trailing, tail
# swept the other way. SwingLeap: letting go at the front of the arc and
# flying to the next ring, posed over flight progress: arms thrown forward
# and reaching, legs stretched long behind, then the paws close on the next
# ring overhead with the body behind it (SWING_CATCH), the pose Swing has at
# that angle, so the catch carries straight on into the swing.
SWING_GRIP = Vector((5.5, 0., 63.5))   # the ring's bottom bar in mesh space (origin = capsule bottom)
SWING_MAX = 40.                        # deg: the pendulum angle at either end of Swing
SWING_RELEASE, SWING_CATCH = .75, -.625   # SwingLeap starts and ends at these Swing poses (fraction of SWING_MAX)
SWING_FRAMES, SWING_LEAP_FRAMES = 21, 16

def swing_pose(s, reach=0.):
    """s -1 (back of the arc) .. 1 (front). reach 0..1: the leap's throw, arms
    forward off the ring and legs stretched long behind."""
    front, back = max(0., s), max(0., -s)
    stay = 1. - reach
    poser.translate('pelvis', (0., 0., 1.5 * stay))
    poser.rotate('pelvis', 'Y', 8 * front - 10 * back - 6 * reach)
    poser.rotate('spine_01', 'Y', 3 + 8 * front - 7 * back - 4 * reach)
    poser.rotate('spine_02', 'Y', 3 * front - 4 * back)
    poser.rotate('chest', 'Y', -5 - 2 * reach)              # chest open under the raised arms
    poser.rotate('neck', 'Y', -4)
    # The whole body pitches with the arc; the head takes back part of it so his
    # eyes stay on where he's going (and on the next ring as he flies).
    poser.rotate('head', 'Y', 4 - 12 * s + 8 * reach)   # (+: chin up)
    for side in 'LR':
        poser.rotate(f'ear_{side}', 'Y', 10 * reach + 6 * front)   # ears laid back by the air
    # Tail: hanging below him, swept against the swing; streams out behind in the leap.
    for i, b in enumerate(TAIL):
        k = (i + 1) / 6
        poser.rotate(b, 'Y', (-34 if i == 0 else -4) * stay + (16 * s if i == 0 else 5 * s) * stay
                     + (6 if i == 0 else 1) * reach)
        poser.rotate(b, 'Z', 5 * math.sin(2.1 * s + k * 2) * stay)
    poser.update()
    r = 0.
    # Paws round the bar, a little below it with the fingers curled over; in
    # the leap thrown forward and up toward the next ring.
    for side, sign in (('L', 1), ('R', -1)):
        held = Vector((SWING_GRIP.x - .5, sign * 3.4, SWING_GRIP.z - 2.))
        thrown = Vector((15., sign * 6.5, 55.))
        fk = held.lerp(thrown, reach)
        poser.arm(side, fk, pole=(-.2, sign * 1., -.1))
        curl(side, 70 * stay + 25 * reach)
    # Legs: dangling at rest, knees up at the front, trailing at the back,
    # stretched long behind in the leap (the left a little ahead of the right).
    for side in 'LR':
        lead = 1.5 if side == 'L' else -1.
        rest = Vector((3. + lead, NEUTRAL_BALL[side].y * .85, 4.))
        knees = Vector((13. + lead, NEUTRAL_BALL[side].y * .8, 16.))
        trail = Vector((-9. + lead, NEUTRAL_BALL[side].y * .85, 6.))
        long = Vector((-18. + 2 * lead, NEUTRAL_BALL[side].y * .7, 2.))   # nearly straight, behind and below
        ball = rest.lerp(knees, front) if s >= 0 else rest.lerp(trail, back)
        ball = ball.lerp(long, reach)
        pitch = 55 - 25 * front + 20 * back + 25 * reach
        r = max(r, poser.leg(side, ball, pitch, 15 + 10 * reach, pole=(1., 0., -.1 * back)))
    ik_goals()
    return r

def swing(phase, f):
    return swing_pose(-1. + 2. * f / (SWING_FRAMES - 1))

def swing_leap(phase, f):
    u = f / (SWING_LEAP_FRAMES - 1)
    throw = smoothstep(u, 0., .25) * (1 - smoothstep(u, .62, 1.))
    s = SWING_RELEASE + (SWING_CATCH - SWING_RELEASE) * smoothstep(u, .2, 1.)
    return swing_pose(s, throw)

author('Swing', SWING_FRAMES, swing, False, {
    'grip_cm': [SWING_GRIP.x, SWING_GRIP.z], 'swing_max_deg': SWING_MAX,
    'time_mapping': 'clip time = (pendulum angle / swing_max_deg + 1) / 2 x duration (positive: body ahead of the grip)',
    'stance_intervals_s': {'foot_L': [], 'foot_R': []},
    'notes': 'Hanging from a ring overhead by both paws; the runtime pitches the mesh about the grip by the pendulum angle and poses the clip by it.'}, ground=False)
author('SwingLeap', SWING_LEAP_FRAMES, swing_leap, False, {
    'grip_cm': [SWING_GRIP.x, SWING_GRIP.z], 'release_swing': SWING_RELEASE, 'catch_swing': SWING_CATCH,
    'time_mapping': 'clip time = flight progress x duration; first/last frames = Swing at release_swing / catch_swing x swing_max_deg',
    'stance_intervals_s': {'foot_L': [], 'foot_R': []},
    'notes': 'Ring to ring: off at the front of the arc, arms thrown forward, legs long behind, paws closing on the next ring with the body behind it.'}, ground=False)

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
