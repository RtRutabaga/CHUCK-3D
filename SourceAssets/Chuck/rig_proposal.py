"""Blender 4.5 LTS: PROPOSED production rig for Chuck (not exported, not in use).

blender --background SourceAssets/Chuck/Chuck.blend --python SourceAssets/Chuck/rig_proposal.py -- <out_dir>

Builds the proposed skeleton from the PROPOSAL table below beside the current
model, writes rig_proposal.json (the table, for Codex/runtime review) into
<out_dir>, and renders overlay views with joints drawn as spheres/sticks.
Never saves the .blend and never touches the FBX exports or the current
14-bone contract. See SourceAssets/Chuck/RIG-PROPOSAL.md.

Coordinates: centimetres, Z up, nose +X, feet at Z=0, Blender +Y = runtime
_L side (FBX reflects Y; unchanged convention).
"""
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector

out = Path(sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else 'RigProposal')
out.mkdir(parents=True, exist_ok=True)

# name: (head, tail, parent, deform, note). _L entries are mirrored to _R.
PROPOSAL = {
    'root':        ((0, 0, 0), (0, 0, 6), None, False, 'Ground, capsule-aligned; runtime moves the actor, not this bone'),
    'pelvis':      ((-2, 0, 19.5), (-2, 0, 24.5), 'root', True, 'Hip centre; pelvis height/tilt correction for contact'),
    'spine_01':    ((-2, 0, 24.5), (-1.3, 0, 30.5), 'pelvis', True, 'Belly'),
    'spine_02':    ((-1.3, 0, 30.5), (-.5, 0, 37), 'spine_01', True, 'Ribcage; roll/curl'),
    'chest':       ((-.5, 0, 37), (0, 0, 43), 'spine_02', True, 'Shoulder girdle'),
    'neck':        ((0, 0, 43), (.8, 0, 47.5), 'chest', True, 'Collar clearance when head pitches'),
    'head':        ((.8, 0, 47.5), (8, 0, 53), 'neck', True, 'Name kept from current rig; pivot moves up from (0,0,46)'),
    'jaw':         ((2, 0, 49.6), (15.5, 0, 49.2), 'head', True, 'Hinge under the cheek; restrained open/close of lower lip + chin'),
    'ear_L':       ((-1.6, 5.2, 58.5), (-1.8, 6.2, 64), 'head', True, 'Restrained secondary motion only'),
    'clavicle_L':  ((0, 3, 42.5), (0, 9.6, 41.5), 'chest', True, 'Shrug/reach for climb'),
    'upperarm_L':  ((0, 10.4, 41), (-1, 14, 29), 'clavicle_L', True, 'Replaces arm_L (same head)'),
    'lowerarm_L':  ((-1, 14, 29), (3, 14, 22), 'upperarm_L', True, 'Replaces forearm_L (same head)'),
    'hand_L':      ((3, 14, 22), (4.6, 14, 18.6), 'lowerarm_L', True, 'Wrist; grip target for climbing'),
    'fingers_L':   ((4.6, 14, 18.6), (5.6, 14, 15.6), 'hand_L', True, 'One curl bone for the four fingers'),
    'thumb_L':     ((4.1, 15.2, 19.4), (6.1, 15.6, 17.6), 'hand_L', True, 'Opposes fingers for grip'),
    'thigh_L':     ((-2, 6, 19.5), (1.5, 6.8, 11.5), 'pelvis', True, 'Knee moves FORWARD of the hip (rat anatomy)'),
    'calf_L':      ((1.5, 6.8, 11.5), (-3.2, 7, 3.4), 'thigh_L', True, 'Shin runs back down to the hock/ankle'),
    'foot_L':      ((-3.2, 7, 3.4), (3.4, 7, 1.0), 'calf_L', True, 'Ankle/hock to ball; heel stays on ground in stance'),
    'toes_L':      ((3.4, 7, 1.0), (8.2, 7, .5), 'foot_L', True, 'Toe roll at push-off'),
    'tail_0':      ((-6, 0, 17), (-12, .5, 11.5), 'pelvis', True, ''),
    'tail_1':      ((-12, .5, 11.5), (-19, 1.5, 7), 'tail_0', True, ''),
    'tail_2':      ((-19, 1.5, 7), (-27, 3, 4.2), 'tail_1', True, ''),
    'tail_3':      ((-27, 3, 4.2), (-35, 5, 2.8), 'tail_2', True, ''),
    'tail_4':      ((-35, 5, 2.8), (-43, 8, 2.1), 'tail_3', True, ''),
    'tail_5':      ((-43, 8, 2.1), (-51, 13, 2.5), 'tail_4', True, ''),
    # Non-deforming helpers the runtime/animation can target.
    'ik_foot_L':   ((-3.2, 7, 3.4), (-3.2, 7, 8.4), 'root', False, 'Foot IK goal, lives under root'),
    'ik_hand_L':   ((3, 14, 22), (3, 14, 27), 'root', False, 'Hand IK goal (climb holds)'),
    'socket_cigarette': ((12.6, 2.5, 49.3), (17.0, 4.6, 48.7), 'jaw', False, 'Cigarette held in the left mouth corner; head = filter end at the lips, tail = lit end/smoke origin'),
}

def mirrored():
    table = {}
    for name, (head, tail, parent, deform, note) in PROPOSAL.items():
        table[name] = (head, tail, parent, deform, note)
        if name.endswith('_L'):
            flip = lambda p: (p[0], -p[1], p[2])
            rparent = parent[:-2] + '_R' if parent and parent.endswith('_L') else parent
            table[name[:-2] + '_R'] = (flip(head), flip(tail), rparent, deform, note)
    return table

TABLE = mirrored()
(out / 'rig_proposal.json').write_text(json.dumps(
    {n: {'head': h, 'tail': t, 'parent': p, 'deform': d, 'note': note}
     for n, (h, t, p, d, note) in TABLE.items()}, indent=1))

# Sanity: parents exist, children start at/near their parent's chain.
for name, (_, _, parent, _, _) in TABLE.items():
    assert parent is None or parent in TABLE, (name, parent)
print('CHUCK_RIG_PROPOSAL bones', len(TABLE), 'deforming', sum(1 for v in TABLE.values() if v[3]))

# Build a real armature too, so it can be inspected interactively if saved by hand.
arm_data = bpy.data.armatures.new('ChuckRigProposal')
arm = bpy.data.objects.new('ChuckRigProposal', arm_data)
bpy.context.scene.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
for name, (head, tail, parent, deform, _) in TABLE.items():
    b = arm_data.edit_bones.new(name); b.head = head; b.tail = tail; b.use_deform = deform
for name, (_, _, parent, _, _) in TABLE.items():
    if parent: arm_data.edit_bones[name].parent = arm_data.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT')

# Workbench cannot draw armatures, so draw joints as geometry for the renders.
scene = bpy.context.scene
for obj in scene.objects:
    obj.hide_render = obj.name not in ('SK_ChuckBody', 'SM_ChuckFoot')
foot = bpy.data.objects['SM_ChuckFoot']; foot.location = (4, 7, 2.5)
twin = foot.copy(); twin.location = (4, -7, 2.5); scene.collection.objects.link(twin)
body = bpy.data.objects['SK_ChuckBody']
for m in list(body.data.materials) + list(foot.data.materials):
    m.diffuse_color = (*m.diffuse_color[:3], .28)  # ghosted body
joint = bpy.data.materials.new('Joint'); joint.diffuse_color = (1, .55, .05, 1)
helper = bpy.data.materials.new('Helper'); helper.diffuse_color = (.1, .8, 1, 1)
for name, (head, tail, parent, deform, _) in TABLE.items():
    mat = joint if deform else helper
    bpy.ops.mesh.primitive_uv_sphere_add(radius=.55 if deform else .4, location=head, segments=12, ring_count=8)
    bpy.context.object.data.materials.append(mat)
    a, b = Vector(head), Vector(tail)
    bpy.ops.mesh.primitive_cylinder_add(radius=.18, depth=(b - a).length, location=(a + b) / 2, vertices=8)
    stick = bpy.context.object; stick.data.materials.append(mat)
    stick.rotation_mode = 'QUATERNION'; stick.rotation_quaternion = (b - a).to_track_quat('Z', 'Y')

scene.render.engine = 'BLENDER_WORKBENCH'
sh = scene.display.shading
sh.light = 'STUDIO'; sh.color_type = 'MATERIAL'; sh.show_xray = True; sh.xray_alpha = .45
scene.render.resolution_x = scene.render.resolution_y = 1000
scene.world.color = (.55, .6, .65)
cam_data = bpy.data.cameras.new('RigCam'); cam = bpy.data.objects.new('RigCam', cam_data)
scene.collection.objects.link(cam); scene.camera = cam
cam_data.type = 'ORTHO'
for view, eye, scale in (('front', (200, 0, 33), 78), ('side', (0, -200, 33), 78),
                         ('three_quarter', (150, -120, 55), 82), ('head_side', (8, -120, 52), 26),
                         ('leg_side', (0, -120, 12), 30)):
    target = Vector((6, 0, 52)) if view == 'head_side' else Vector((0, 0, 12)) if view == 'leg_side' else Vector((0, 0, 33))
    cam.location = eye; cam_data.ortho_scale = scale
    cam.rotation_euler = (target - Vector(eye)).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = str(out / f'rig_{view}.png')
    bpy.ops.render.render(write_still=True)
print('CHUCK_RIG_PROPOSAL_READY', out)
