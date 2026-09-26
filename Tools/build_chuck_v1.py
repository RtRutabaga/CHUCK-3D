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

V1 = ROOT / 'SourceAssets' / 'Chuck' / 'V1'
ANIM = V1 / 'Animations'
ANIM.mkdir(parents=True, exist_ok=True)
TABLE = json.loads((ROOT / 'SourceAssets/Chuck/rig_proposal.json').read_text(encoding='utf-8-sig'))
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
for obj in [o for o in scene.objects if label(o) in ('Leg', 'LegFur')]:
    bpy.data.objects.remove(obj, do_unlink=True)

def leg_radius(t):
    return 4.3 - 1.6 * smoothstep(t, .08, .5) - .8 * smoothstep(t, .55, 1.)

PAW_ORIGIN = {}
for s, y in (('L', 1), ('R', -1)):
    hip, knee, hock = H(f'thigh_{s}'), H(f'calf_{s}'), H(f'foot_{s}')
    # Knee-forward plantigrade leg; its domed end sinks into the heel mound.
    chain_tube('Leg', [hip + Vector((0, 0, 1.2)), knee, hock, hock + Vector((-.3, 0, -1.3))],
               leg_radius, 'Fur', caps=(.9, .6), rings=28)
    # Paw placed so the heel mound surrounds the hock, the toes start at the
    # toes_ head (ball) and the sole touches Z=0.
    PAW_ORIGIN[s] = Vector((-.8, y * 7, 1.96))
    paw_parts(PAW_ORIGIN[s])
for i, part in enumerate([o for o in scene.objects if label(o) == 'Leg']):
    fur_surface(part, 'LegFur', 'Fur', 1800, 131 + i, (.22, .55), (0, 0, -.8), lambda p, n: p.z < 21)

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

def thigh_pull(p, strength=.75, radius=(5., 11.5), height=(21., 28.)):
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
    clav = .5 * smoothstep(abs(p.y), 5., 8.5) * (1 - smoothstep(abs(p.z - 41.5), 1.5, 4.))
    pull = thigh_pull(p, .6, (4., 8.), (18., 23.))
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
    digit = f'thumb_{s}' if abs(p.y) > 15.0 else f'fingers_{s}'
    return chain_weights(p, [f'hand_{s}', digit], 1.)

JAWED = ('Head', 'MuzzleLight', 'Mouth', 'CheekFur')

def head_weights(p, lab):
    neck = .6 * (1 - smoothstep(p.z, 46.5, 49.5)) * (1 - smoothstep(p.x, 2., 5.))
    jaw = 0.
    if lab in JAWED:
        jaw = smoothstep(p.x, 3.5, 7.) * (1 - smoothstep(p.z - (head_axis_z(p.x) - 1.3), -.6, .6))
    ear = 0.
    if lab == 'Ear':
        ear = smoothstep(p.z, 58.5, 61.)
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
    'hand': (('Hand', 'Finger'), hand_weights),
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

def idle(phase, f):
    w = 2 * math.pi * phase
    poser.translate('pelvis', (0, 0, -.25 - .25 * math.sin(w)))
    poser.rotate('spine_02', 'Y', -1.0 * math.sin(w))  # breath lifts the ribcage
    poser.rotate('chest', 'Y', -.6 * math.sin(w))
    poser.rotate('neck', 'Y', .8 * math.sin(w))
    poser.rotate('head', 'Z', 2.5 * math.sin(w + .7))
    poser.rotate('head', 'Y', 1.2 * math.sin(2 * w + .3))
    for s, k in (('L', 1), ('R', -1)):
        poser.rotate(f'upperarm_{s}', 'Y', -1.5 * math.sin(w + .4))
        poser.rotate(f'lowerarm_{s}', 'Y', -4 - 1. * math.sin(w + .9))
        poser.rotate(f'fingers_{s}', 'Y', -8)
        poser.rotate(f'ear_{s}', 'X', k * 1.5 * math.sin(2 * w + (0 if s == 'L' else 1.1)))
    for i, b in enumerate(TAIL):
        poser.rotate(b, 'Z', 3.5 * math.sin(w - .5 * i))
    poser.update()
    r = plant_rest()
    for s in 'LR': poser.hand_goal(s)
    return r

WALK = {'speed_cm_s': 95.0, 'period_frames': 9, 'stance_fraction': .6, 'pelvis_drop_cm': 1.6, 'lift_cm': 2.4}
PERIOD = WALK['period_frames'] / FPS
STANCE_CM = WALK['speed_cm_s'] * PERIOD * WALK['stance_fraction']

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
    return offset, WALK['lift_cm'] * math.sin(math.pi * u), 24 * (1 - smoothstep(u, 0., .6)), 12 * math.sin(math.pi * u)

def walk(phase, f):
    w = 2 * math.pi * phase
    offsets = {}
    poser.translate('pelvis', (0, .35 * math.cos(w - .6 * math.pi), -WALK['pelvis_drop_cm'] - .45 * math.cos(2 * w)))
    poser.rotate('pelvis', 'Z', 3 * math.sin(w))
    poser.rotate('spine_02', 'Z', -2 * math.sin(w))
    poser.rotate('chest', 'Z', -1.5 * math.sin(w))
    poser.rotate('spine_01', 'Y', -2.5)  # slight forward lean while walking
    poser.rotate('head', 'Y', 2.0 + .8 * math.cos(2 * w))
    poser.rotate('head', 'Z', -1.2 * math.sin(w))
    for i, b in enumerate(TAIL):
        poser.rotate(b, 'Z', 4 * math.sin(w - .6 * (i + 1)))
    feet = {'L': foot_cycle(phase), 'R': foot_cycle((phase + .5) % 1)}
    for s, k in (('L', 1), ('R', -1)):
        other = feet['R' if s == 'L' else 'L'][0] / (STANCE_CM / 2)
        poser.rotate(f'upperarm_{s}', 'Y', -12 * other)
        poser.rotate(f'lowerarm_{s}', 'Y', -10 - 6 * max(0., other))
        poser.rotate(f'fingers_{s}', 'Y', -10)
    poser.update()
    r = 0.
    for s in 'LR':
        offset, lift, fp, tp = feet[s]
        ball = NEUTRAL_BALL[s] + Vector((offset, 0, lift))
        r = max(r, poser.leg(s, ball, fp, tp))
    for s in 'LR': poser.hand_goal(s)
    return r

def stance_intervals(offset_phase):
    st = WALK['stance_fraction']
    a, b = offset_phase % 1, (offset_phase + st) % 1
    spans = [(a, b)] if a < b else [(a, 1.), (0., b)]
    return [[round(x * PERIOD, 4), round(y * PERIOD, 4)] for x, y in spans]

author('Idle', 60, idle, True, {'notes': 'Neutral hold: breathing, restrained head/ear/tail motion; paws planted at rest.'})
author('WalkLoop', WALK['period_frames'], walk, True, {
    'reference_speed_cm_s': WALK['speed_cm_s'], 'stride_cycle_cm': round(WALK['speed_cm_s'] * PERIOD, 3),
    'stance_travel_cm': round(STANCE_CM, 3), 'stance_fraction': WALK['stance_fraction'],
    'stance_intervals_s': {'foot_L': stance_intervals(0.), 'foot_R': stance_intervals(.5)},
    'events_s': {'foot_L_plant': 0.0, 'foot_R_plant': round(PERIOD / 2, 4),
                 'foot_L_lift': round(WALK['stance_fraction'] * PERIOD, 4),
                 'foot_R_lift': round(((.5 + WALK['stance_fraction']) % 1) * PERIOD, 4)},
    'notes': 'In place (root fixed). During stance the ball of the planted paw moves backward in component space at exactly the reference speed; heel lifts around the ball in the last 30% of stance (toe roll).'})

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
    """Upper body at walk intensity k (0 idle .. 1 walking) and gait phase
    angle w, then both legs from the planned world feet."""
    poser.translate('pelvis', (0, .35 * k * math.cos(w - .6 * math.pi),
                               -WALK['pelvis_drop_cm'] * k - .45 * k * math.cos(2 * w)))
    poser.rotate('pelvis', 'Z', 3 * k * math.sin(w))
    poser.rotate('spine_02', 'Z', -2 * k * math.sin(w) + look * .25)
    poser.rotate('chest', 'Z', -1.5 * k * math.sin(w) + look * .2)
    poser.rotate('spine_01', 'Y', -2.5 * k + lean)
    poser.rotate('head', 'Z', look * .55 - 1.2 * k * math.sin(w))
    poser.rotate('head', 'Y', 2.0 * k + .8 * k * math.cos(2 * w))
    for i, b in enumerate(TAIL):
        poser.rotate(b, 'Z', 4 * k * math.sin(w - .6 * (i + 1)) - look * .08 * (i + 1))
    comp = {}
    for s in 'LR':
        (x, y, lift), heading, fp, tp = feet[s]
        cx, cy = to_component(x, y, dist, yaw)
        comp[s] = (Vector((cx, cy, NEUTRAL_BALL[s].z + lift)), heading - math.degrees(yaw), fp, tp)
    half = STANCE_CM / 2
    for s in 'LR':
        other = 'R' if s == 'L' else 'L'
        drive = max(-1., min(1., (comp[other][0].x - NEUTRAL_BALL[other].x) / half))
        poser.rotate(f'upperarm_{s}', 'Y', -12 * k * drive)
        poser.rotate(f'lowerarm_{s}', 'Y', -4 - 6 * k - 6 * k * max(0., drive))
        poser.rotate(f'fingers_{s}', 'Y', -8 - 2 * k)
    poser.update()
    r = 0.
    for s in 'LR':
        ball, heading, fp, tp = comp[s]
        r = max(r, poser.leg(s, ball, fp, tp, heading=heading))
    for s in 'LR': poser.hand_goal(s)
    return r

def loop_entry(side):
    """Component x of the ball at WalkLoop frame 0."""
    return NEUTRAL_BALL[side].x + foot_cycle(0. if side == 'L' else .5)[0]

RB = {s: (NEUTRAL_BALL[s].x, NEUTRAL_BALL[s].y, 0.) for s in 'LR'}
SWING = (1 - WALK['stance_fraction']) * PERIOD  # 0.12 s, as in WalkLoop

# WalkStart: 0 -> 95 cm/s over 0.4 s, ending on WalkLoop frame 0
# (foot_L just planted at +half, foot_R in late stance).
START_T = .4
D_end = travel(START_T, START_T, True)
start_steps = {
    # R's second entry is its next WalkLoop lift (0.03 s after the seam), so the
    # heel is already rolling at the seam exactly as in the loop.
    'R': [(.1, .25, (loop_entry('R') + D_end, RB['R'][1], 0.)),
          (START_T + .03, START_T + .15, (loop_entry('R') + D_end, RB['R'][1], 0.))],
    'L': [(START_T - SWING, START_T, (loop_entry('L') + D_end, RB['L'][1], 0.))],
}
def walk_start(phase, f):
    t = f / FPS
    feet = {s: plan_foot(t, RB[s], start_steps[s]) for s in 'LR'}
    return pose_planned(t, feet, travel(t, START_T, True), 0., smoothstep(t, 0., START_T),
                        2 * math.pi * (t - START_T) / PERIOD)
START_FRAMES = round(START_T * FPS)

# WalkStop: from WalkLoop frame 0 to the neutral hold, 95 -> 0 cm/s over 0.4 s.
STOP_T = .4
D_stop = travel(STOP_T, STOP_T, False)
stop_init = {s: (loop_entry(s), RB[s][1], 0.) for s in 'LR'}
stop_steps = {
    'R': [(.03, .03 + SWING + .03, (RB['R'][0] + D_stop, RB['R'][1], 0.))],
    'L': [(.2, .2 + SWING + .04, (RB['L'][0] + D_stop, RB['L'][1], 0.))],
}
def walk_stop(phase, f):
    t = f / FPS
    feet = {s: plan_foot(t, stop_init[s], stop_steps[s]) for s in 'LR'}
    return pose_planned(t, feet, travel(t, STOP_T, False), 0., 1 - smoothstep(t, 0., STOP_T),
                        2 * math.pi * t / PERIOD)
STOP_FRAMES = round(STOP_T * FPS) + 3  # short settle hold

# Turn in place 90 degrees over 0.6 s. yaw = capsule rotation; the inside paw
# steps twice, the outside paw once; head and chest lead the turn.
TURN_T = .6
def turn_clip(sign):
    lead, trail = ('L', 'R') if sign > 0 else ('R', 'L')
    final = 90. * sign
    def world_rest(s, deg):
        a = math.radians(deg); x, y, _ = RB[s]
        return (x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a), deg)
    steps = {lead: [(.04, .2, world_rest(lead, final * .55)), (.36, .52, world_rest(lead, final))],
             trail: [(.2, .36, world_rest(trail, final))]}
    def pose(phase, f):
        t = f / FPS
        yaw = math.radians(final) * smoothstep(t, .02, .5)
        look = final * .35 * (smoothstep(t, 0., .15) - smoothstep(t, .35, .6))
        feet = {s: plan_foot(t, RB[s], steps[s]) for s in 'LR'}
        return pose_planned(t, feet, 0., yaw, .35 * math.sin(math.pi * min(1., t / TURN_T)),
                            2 * math.pi * t / .3, 0., look)
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
        poser.rotate(f'fingers_{s}', 'Y', -15)
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

def samples(fn, dur, n=9):
    return [[round(dur * i / (n - 1), 4), round(fn(dur * i / (n - 1)), 3)] for i in range(n)]

author('WalkStart', START_FRAMES + 1, walk_start, False, {
    'nominal_speed_profile_cm_s': samples(lambda t: speed_ramp(t, START_T, True), START_T),
    'capsule_travel_cm_per_frame': [round(travel(f / FPS, START_T, True), 4) for f in range(START_FRAMES + 1)],
    'travel_cm': round(D_end, 3), 'ends_on': 'WalkLoop frame 0 (foot_L planted, foot_R late stance)',
    'stance_intervals_s': {f'foot_{s}': stance_from_steps(start_steps[s], START_T) for s in 'LR'},
    'events_s': {'foot_R_lift': .1, 'foot_R_plant': .25, 'foot_L_lift': round(START_T - SWING, 4), 'foot_L_plant': START_T},
    'notes': 'Speed rises with a smoothstep from 0 to 95 cm/s; planted paws are world-locked against that travel.'})
author('WalkStop', STOP_FRAMES + 1, walk_stop, False, {
    'nominal_speed_profile_cm_s': samples(lambda t: speed_ramp(t, STOP_T, False), STOP_T),
    'capsule_travel_cm_per_frame': [round(travel(f / FPS, STOP_T, False), 4) for f in range(STOP_FRAMES + 1)],
    'travel_cm': round(D_stop, 3), 'starts_from': 'WalkLoop frame 0', 'settle_hold_s': round(3 / FPS, 4),
    'stance_intervals_s': {f'foot_{s}': stance_from_steps(stop_steps[s], STOP_FRAMES / FPS) for s in 'LR'},
    'events_s': {'foot_R_lift': .03, 'foot_R_plant': round(.06 + SWING, 4),
                 'foot_L_lift': .2, 'foot_L_plant': round(.24 + SWING, 4)},
    'notes': 'Speed falls with a smoothstep from 95 to 0 cm/s; ends in the neutral stance.'})
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

tris = sum(len(p.vertices) - 2 for p in body.data.polygons)
zs = [v.co.z for v in body.data.vertices]
meta = {'contract': 'docs/RIG-CONTRACT-V1.md', 'table': 'SourceAssets/Chuck/rig_proposal.json',
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
bpy.ops.wm.save_as_mainfile(filepath=str(V1 / 'Chuck_V1.blend'))
print('CHUCK_V1_READY', 'tris', tris, 'clips', [e['name'] for _, e in CLIPS],
      'reach', [e['max_leg_reach_ratio'] for _, e in CLIPS], 'markers', markers)
