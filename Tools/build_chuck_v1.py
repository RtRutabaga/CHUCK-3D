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
    s = side_of(p)
    a = seg(p, H(f'thigh_{s}'), T(f'thigh_{s}'))[0]
    pelvis = 1 - smoothstep(a, -1.5, 2.5)
    return mix((pelvis, {'pelvis': 1.}), (1 - pelvis, chain_weights(p, [f'thigh_{s}', f'calf_{s}', f'foot_{s}'])))

def paw_weights(p):
    s = side_of(p)
    return chain_weights(p, [f'calf_{s}', f'foot_{s}', f'toes_{s}'], 1.8)

TAIL = [f'tail_{i}' for i in range(6)]
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
        poser.key_all(f)
    rig.animation_data.action = None
    entry = {'name': name, 'file': f'Animations/AS_Chuck_{name}.fbx', 'fps': FPS,
             'first_frame': 0, 'last_frame': frames - 1, 'frame_count': frames,
             'duration_s': round(frames / FPS, 4), 'loop': loop,
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
        return offset, 0., 24 * roll, -24 * roll
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
