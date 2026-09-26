"""Blender 4.5 LTS: v1 rig pose evidence and walk measurements.

blender --background SourceAssets/Chuck/V1/Chuck_V1.blend --python SourceAssets/Chuck/V1/review_v1.py -- <out_dir>

Renders neutral views, the contract's non-gameplay pose studies (crouch/curl,
forward knee flexion, toe roll, overhead grip, mouth-held cigarette alignment)
and WalkLoop frames. Measures the evaluated (deformed) paw sole during
WalkLoop stance against the 95 cm/s reference. Never saves the .blend.
"""
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector

V1 = Path(bpy.data.filepath).parent
sys.path.insert(0, str(V1.parents[2] / 'Tools'))
from chuck_v1_pose import Poser  # noqa: E402

out = Path(sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else 'ReviewV1')
out.mkdir(parents=True, exist_ok=True)
scene = bpy.context.scene
rig = bpy.data.objects['SK_Chuck_Rig']; body = bpy.data.objects['SK_Chuck']
manifest = json.loads((V1 / 'Animations/manifest.json').read_text(encoding='utf-8'))
poser = Poser(rig)
rig.animation_data_create(); rig.animation_data.action = None

bpy.ops.mesh.primitive_plane_add(size=400, location=(0, 0, -.02))
bpy.ops.mesh.primitive_cube_add(size=1, location=(45, 0, 90))
bar = bpy.context.object; bar.scale = (4, 4, 180); bar.hide_render = True
scene.render.engine = 'BLENDER_WORKBENCH'
sh = scene.display.shading
sh.light = 'STUDIO'; sh.color_type = 'MATERIAL'; sh.show_cavity = True; sh.cavity_type = 'BOTH'
scene.render.resolution_x = scene.render.resolution_y = 900
scene.world.color = (.55, .6, .65)
cam_data = bpy.data.cameras.new('V1Cam'); cam = bpy.data.objects.new('V1Cam', cam_data)
scene.collection.objects.link(cam); scene.camera = cam

def shot(name, eye, target, ortho=None, lens=50):
    cam.location = eye
    cam.rotation_euler = (Vector(target) - Vector(eye)).to_track_quat('-Z', 'Y').to_euler()
    cam_data.type = 'ORTHO' if ortho else 'PERSP'
    if ortho: cam_data.ortho_scale = ortho
    else: cam_data.lens = lens
    scene.render.filepath = str(out / f'{name}.png')
    bpy.ops.render.render(write_still=True)

def neutral_legs():
    for s in 'LR':
        poser.leg(s, poser.rest_head[f'toes_{s}'])

# Neutral views.
poser.reset(); neutral_legs()
mid = (0, 0, 33)
shot('neutral_front', (200, 0, 33), mid, 78)
shot('neutral_side', (0, -200, 33), mid, 78)
shot('neutral_three_quarter', (150, -120, 55), mid, 82)
shot('neutral_rear', (-200, 0, 33), mid, 78)
bar.hide_render = False
shot('neutral_scale', (20, -400, 90), (20, 0, 90), 200)
bar.hide_render = True
shot('neutral_legs_side', (0, -90, 12), (0, 0, 11), lens=50)

# Contract pose studies (non-gameplay).
poser.reset()
poser.translate('pelvis', (0, 0, -5.5))
for b, deg in (('spine_01', 16), ('spine_02', 16), ('chest', 10), ('neck', -12), ('head', -10)):
    poser.rotate(b, 'Y', deg)
for s in 'LR':
    poser.rotate(f'upperarm_{s}', 'Y', -35); poser.rotate(f'lowerarm_{s}', 'Y', -40)
    poser.rotate(f'fingers_{s}', 'Y', -40)
poser.update(); neutral_legs()
shot('pose_crouch_curl_side', (0, -140, 25), (0, 0, 24), lens=50)
shot('pose_crouch_curl_three_quarter', (110, -95, 40), (0, 0, 25), lens=50)

poser.reset(); poser.leg('R', poser.rest_head['toes_R'])
poser.leg('L', poser.rest_head['toes_L'] + Vector((5, 0, 7.5)), -15, 10)
shot('pose_knee_flex_side', (0, 90, 14), (0, 0, 12), lens=50)
shot('pose_knee_flex_front', (90, 25, 14), (0, 3, 12), lens=50)

poser.reset(); poser.leg('R', poser.rest_head['toes_R'])
poser.leg('L', poser.rest_head['toes_L'], 38, -38)
shot('pose_toe_roll_side', (2, 45, 5), (0, 7, 4), lens=60)

poser.reset()
poser.rotate('clavicle_L', 'X', 18); poser.rotate('clavicle_R', 'X', -18)
poser.rotate('upperarm_L', 'Y', -165); poser.rotate('upperarm_R', 'Y', -150)
poser.rotate('lowerarm_L', 'Y', -15); poser.rotate('lowerarm_R', 'Y', -25)
for s in 'LR':
    poser.rotate(f'fingers_{s}', 'Y', -75); poser.rotate(f'thumb_{s}', 'Z', 35 if s == 'L' else -35)
poser.rotate('spine_02', 'Y', -4); poser.rotate('head', 'Y', -12)
poser.update(); neutral_legs()
shot('pose_overhead_grip_side', (0, -150, 45), (0, 0, 42), lens=45)
shot('pose_overhead_grip_three_quarter', (120, -100, 55), (0, 0, 45), lens=45)

# Cigarette alignment: a temporary white stick + ember placed on the posed
# socket_cigarette (filter at the head, lit end at the tail), jaw slightly open.
poser.reset(); neutral_legs()
poser.rotate('jaw', 'Y', 6); poser.update()
sock = rig.pose.bones['socket_cigarette']
a, b = rig.matrix_world @ sock.head, rig.matrix_world @ sock.tail
white = bpy.data.materials.new('Paper'); white.diffuse_color = (.92, .9, .85, 1)
ember = bpy.data.materials.new('Ember'); ember.diffuse_color = (1, .35, .05, 1)
bpy.ops.mesh.primitive_cylinder_add(radius=.28, depth=(b - a).length, location=(a + b) / 2, vertices=16)
stick = bpy.context.object; stick.data.materials.append(white)
stick.rotation_mode = 'QUATERNION'; stick.rotation_quaternion = (b - a).to_track_quat('Z', 'Y')
bpy.ops.mesh.primitive_uv_sphere_add(radius=.3, location=b, segments=12, ring_count=8)
bpy.context.object.data.materials.append(ember)
shot('pose_cigarette_side', (8, -70, 52), (9, 0, 50), lens=60)
shot('pose_cigarette_three_quarter', (45, -40, 58), (10, 1, 50), lens=60)
shot('pose_cigarette_front', (70, 8, 52), (10, 1, 50), lens=60)
for o in (stick, bpy.context.object): bpy.data.objects.remove(o, do_unlink=True)

# WalkLoop frames and evaluated-mesh contact measurement.
poser.reset()
walk = next(c for c in manifest['clips'] if c['name'] == 'WalkLoop')
rig.animation_data.action = bpy.data.actions['AS_Chuck_WalkLoop']
frames = range(walk['first_frame'], walk['last_frame'] + 1)
deps = bpy.context.evaluated_depsgraph_get()

def sole_vertex(side):
    """Sole vertex under the ball of the paw, chosen at a frame where that paw
    is planted (first stance frame), from the evaluated deformed mesh."""
    start = walk['stance_intervals_s'][f'foot_{side}'][0][0]
    scene.frame_set(walk['first_frame'] + math.ceil(start * walk['fps'] - 1e-6))
    ball = rig.matrix_world @ rig.pose.bones[f'toes_{side}'].head
    target = Vector((ball.x, ball.y, 0.))
    ev = body.evaluated_get(bpy.context.evaluated_depsgraph_get()); m = ev.to_mesh()
    idx = min((v for v in m.vertices if v.co.z < .6), key=lambda v: (v.co - target).length).index
    ev.to_mesh_clear()
    return idx

idx = {s: sole_vertex(s) for s in 'LR'}
track = {s: [] for s in 'LR'}
for f in list(frames) + [walk['first_frame'] + len(frames)]:  # wrap to close the loop
    scene.frame_set(f if f <= walk['last_frame'] else walk['first_frame'])
    deps = bpy.context.evaluated_depsgraph_get()
    ev = body.evaluated_get(deps); m = ev.to_mesh()
    for s in 'LR': track[s].append(m.vertices[idx[s]].co.copy())
    ev.to_mesh_clear()
    if f <= walk['last_frame']:
        shot(f'walk_side_f{f:02d}', (0, -110, 16), (0, 0, 15), lens=50)
fps = walk['fps']; period = walk['frame_count'] / fps
stance = walk['stance_intervals_s']
report = {}
for s in 'LR':
    spans = stance[f'foot_{s}']
    speeds, heights = [], []
    for i in range(len(frames)):
        t0, t1 = i / fps, (i + 1) / fps
        if any(a <= t0 and t1 <= b + 1e-6 for a, b in spans):
            d = track[s][i + 1] - track[s][i]
            speeds.append(d.x * fps); heights.append(track[s][i].z)
    slip = [abs(v + walk['reference_speed_cm_s']) for v in speeds]
    report[s] = {'stance_samples': len(speeds),
                 'sole_speed_cm_s': [round(v, 2) for v in speeds],
                 'max_world_slip_cm_s_if_capsule_moves_at_reference': round(max(slip), 3) if slip else None,
                 'sole_z_cm_during_stance': [round(z, 3) for z in heights]}
(out / 'walk_contact_report.json').write_text(json.dumps(report, indent=1))
print('CHUCK_V1_WALK_CONTACT', json.dumps(report))
print('CHUCK_V1_REVIEW_READY', out)
