"""Blender 4.5.14: the Chult T-rex (Claude, 2026-10-15), after build_raptor.py.

blender --background --python Tools/build_trex.py
  -> SourceAssets/Enemies/TRex/SK_TRex.fbx              (skinned mesh, no clips)
     SourceAssets/Enemies/TRex/AS_TRex_<Clip>.fbx       (Idle, Walk, Run, Attack, Jump, Death)
     SourceAssets/Enemies/TRex/T_TRex{Scales,Scales_N,Pattern}.png
     SourceAssets/Enemies/TRex/trex-clips.json
     Unreal/Chuck3D/Source/Chuck3D/TRexClipData.h

Source: Quaternius's CC0 "T-Rex" (SourceAssets/Enemies/TRex/Source,
manifest.json; unmodified), from the same pack and rig as the raptor. This
script turns it into the T-rex under the giant trees upriver (user 2026-10-15):
- scaled to 12 m nose to tail, towering over 65 cm Chuck. Facing +X,
  centimetres, Chuck's FBX conventions.
- The low-poly, faceted body is subdivided once and smooth shaded.
- UVs follow the body: u along the spine (snout to tail tip), v round it, so a
  procedural pattern (dark dorsal bands, a pale belly) runs nose to tail; a
  tileable scale texture and normal map (generated here, no downloads) add
  skin detail through the same UVs.
- The clips keep their authored motion; the bone translations are rescaled
  with the rig, and each clip's natural ground speed (from its stance feet)
  and the bite's moment are measured for the runtime (TRexClipData.h).
"""
from pathlib import Path
import json
import math
import sys

import bpy
import numpy as np
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'SourceAssets/Enemies/TRex/Source/Quaternius_TRex.glb'
OUT = ROOT / 'SourceAssets/Enemies/TRex'
HEADER = ROOT / 'Unreal/Chuck3D/Source/Chuck3D/TRexClipData.h'
FPS = 24
FBX = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False,
           primary_bone_axis='Y', secondary_bone_axis='X', use_armature_deform_only=False,
           mesh_smooth_type='FACE')

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = .01
scene.render.fps = FPS
bpy.ops.import_scene.gltf(filepath=str(SRC))

arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
body = bpy.data.objects['Trex']
# Free the rig from the glTF root node (its 0.01 scale) keeping its world transform.
world = arm.matrix_world.copy()
arm.parent = None
arm.matrix_world = world
for o in list(bpy.data.objects):
    if o not in (arm, body):
        bpy.data.objects.remove(o, do_unlink=True)
for a in bpy.data.actions:
    a.name = a.name.split('_')[-1]          # 'Armature|TRex_Run' -> 'Run'
CLIPS = ['Idle', 'Walk', 'Run', 'Attack', 'Jump', 'Death']
LOOPING = {'Idle', 'Walk', 'Run'}


def world_bone(name):
    return arm.matrix_world @ arm.pose.bones[name].head


# Scale-free reference before any change: the jump's hip lift over the hips' rest height.
def hips_ratio():
    arm.animation_data.action = bpy.data.actions['Jump']
    scene.frame_set(0); bpy.context.view_layer.update(); rest = world_bone('Hips').copy()
    scene.frame_set(13); bpy.context.view_layer.update(); up = world_bone('Hips').copy()
    arm.animation_data.action = None; scene.frame_set(0); bpy.context.view_layer.update()
    ground = min((body.matrix_world @ v.co).z for v in body.data.vertices)
    return (up.z - rest.z) / (rest.z - ground), (up - rest).length / (rest.z - ground)


def rescale(m, factor):
    """Bake matrix m (rotation and uniform scale) into the rig and mesh; scale the
    clips' bone translations (bone space, in the rig's units) by factor."""
    arm.matrix_world = m @ arm.matrix_world
    bpy.ops.object.select_all(action='DESELECT')
    for o in (arm, body):
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); bpy.context.view_layer.objects.active = body
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for act in bpy.data.actions:
        for fc in act.fcurves:
            if fc.data_path.endswith('.location'):
                for kp in fc.keyframe_points:
                    kp.co.y *= factor; kp.handle_left.y *= factor; kp.handle_right.y *= factor
    bpy.context.view_layer.update()


def length():
    xs = [(body.matrix_world @ v.co).x for v in body.data.vertices]
    return max(xs) - min(xs)


before = hips_ratio()
# Re-orient (the source faces -Y), then scale to the target length, measured.
s_obj = arm.matrix_world.to_scale().x
rescale(Matrix.Rotation(math.radians(90), 4, 'Z'), s_obj)
TARGET = 1200.
c = TARGET / length()
rescale(Matrix.Scale(c, 4), c)
after = hips_ratio()
print('CHUCK_TREX_RESCALE length', round(length(), 1), 'jump_lift', [round(v, 3) for v in before], [round(v, 3) for v in after])
if abs(length() - TARGET) > 1 or abs(before[0] - after[0]) > .02 or abs(before[1] - after[1]) > .02:
    raise RuntimeError('Rescaled clip does not match the source motion')

# Smooth the faceted body: one subdivision, applied under the armature. The
# source is flat shaded with every face's corners split apart; weld them first
# (same place, same weights) or the subdivision shrinks each face into its own
# blob with gaps between.
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True); bpy.context.view_layer.objects.active = body
import bmesh
bm = bmesh.new(); bm.from_mesh(body.data)
before_verts = len(bm.verts)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=.05)
# The source relies on double-sided drawing: about a quarter of its faces face
# inward. Turn each to face out: a ray from a face along its normal crosses the
# body's surface an even number of times if it points out, odd if in.
from mathutils.bvhtree import BVHTree
bm.faces.ensure_lookup_table()
tree = BVHTree.FromBMesh(bm)
flipped = 0
for f in bm.faces:
    n = f.normal.copy(); o = f.calc_center_median() + n * .02
    crossings = 0
    for _ in range(40):
        loc, nrm, idx, dist = tree.ray_cast(o, n)
        if loc is None: break
        crossings += 1; o = loc + n * .02
    if crossings % 2: f.normal_flip(); flipped += 1
bm.to_mesh(body.data); bm.free(); body.data.update()
print('CHUCK_TREX_WELD verts', before_verts, '->', len(body.data.vertices), 'faces_turned_out', flipped)
sub = body.modifiers.new('Smooth', 'SUBSURF'); sub.levels = 1; sub.render_levels = 1
while body.modifiers[0] != sub:
    bpy.ops.object.modifier_move_up(modifier=sub.name)
bpy.ops.object.modifier_apply(modifier=sub.name)
for p in body.data.polygons:
    p.use_smooth = True
names = {'Green': 'TRexHide', 'LightGreen': 'TRexPale', 'LightYellow': 'TRexBelly', 'Black': 'TRexDark', 'Red': 'TRexMouth'}
for m in body.data.materials:
    m.name = names.get(m.name, m.name)

# UVs along the body: the spine from snout to tail tip; v is the angle round it (0.5 on top).
spine_names = ['Head_end', 'Head', 'Neck', 'Shoulders', 'Torso', 'Hips', 'Tail1', 'Tail2', 'Tail3', 'Tail4', 'Tail5']
pts = []
for n in spine_names:
    if n == 'Head_end':
        b = arm.data.bones['Head']; pts.append(arm.matrix_world @ (b.tail_local + (b.tail_local - b.head_local) * 6))
    else:
        pts.append(arm.matrix_world @ arm.data.bones[n].head_local)
pts.append(arm.matrix_world @ (arm.data.bones['Tail5'].tail_local + (arm.data.bones['Tail5'].tail_local - arm.data.bones['Tail5'].head_local) * 6))
seg_len = [(pts[i + 1] - pts[i]).length for i in range(len(pts) - 1)]
total = sum(seg_len)


def spine_coord(p):
    best = (1e9, 0., 0)
    acc = 0.
    for i in range(len(pts) - 1):
        a, b = pts[i], pts[i + 1]
        d = b - a
        t = max(0., min(1., (p - a).dot(d) / d.length_squared))
        q = a + d * t
        dist = (p - q).length
        if dist < best[0]:
            best = (dist, (acc + t * seg_len[i]) / total, i)
        acc += seg_len[i]
    i = best[2]
    d = (pts[i + 1] - pts[i]).normalized()
    side = Vector((0, 0, 1)).cross(d).normalized()
    up = d.cross(side).normalized()
    q = pts[i] + d * (p - pts[i]).dot(d)
    r = p - q
    ang = math.atan2(r.dot(side), r.dot(up))       # 0 on top
    return best[1], .5 + ang / (2 * math.pi)


me = body.data
uv = me.uv_layers.new(name='UVMap') if not me.uv_layers else me.uv_layers[0]
for poly in me.polygons:
    coords = [spine_coord(me.vertices[me.loops[li].vertex_index].co) for li in poly.loop_indices]
    us = [c[1] for c in coords]
    if max(us) - min(us) > .5:                       # the seam under the belly: keep a face on one side
        coords = [(c[0], c[1] + 1 if c[1] < .5 else c[1]) for c in coords]
    for li, c in zip(poly.loop_indices, coords):
        uv.data[li].uv = (1 - c[0], c[1])

# Textures (seeded, tileable).
rng = np.random.default_rng(1515)


def save_png(name, rgba):
    h, w, _ = rgba.shape
    img = bpy.data.images.new(name, w, h, alpha=True)
    img.pixels.foreach_set(np.clip(rgba, 0, 1).astype(np.float32).ravel())
    img.filepath_raw = str(OUT / f'{name}.png'); img.file_format = 'PNG'; img.save()
    print('CHUCK_TREX_TEXTURE', name, w, h)


N = 512; CELLS = 30
y, x = np.mgrid[0:N, 0:N] / N
jit = rng.uniform(.15, .85, (CELLS, CELLS, 2))
f1 = np.full((N, N), 9.); f2 = np.full((N, N), 9.); cell_tone = np.zeros((N, N))
tones = rng.uniform(-1, 1, (CELLS, CELLS))
for cy in range(CELLS):
    for cx in range(CELLS):
        px = (cx + jit[cy, cx, 0]) / CELLS; py = (cy + jit[cy, cx, 1]) / CELLS
        # rows offset like overlapping scales
        dx = (x - px + .5) % 1 - .5; dy = ((y - py + .5) % 1 - .5) * 1.25
        d = np.sqrt(dx * dx + dy * dy) * CELLS
        closer = d < f1
        f2 = np.where(closer, f1, np.minimum(f2, d)); cell_tone = np.where(closer, tones[cy, cx], cell_tone); f1 = np.where(closer, d, f1)
edge = np.clip((f2 - f1) * 3.2, 0, 1)
height = edge ** .6 * (1 - .25 * f1)
tone = .78 + .12 * cell_tone + .18 * edge
save_png('T_TRexScales', np.dstack([tone, tone, tone, np.ones_like(tone)]))
gx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 3.; gy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 3.
nrm = np.dstack([-gx, -gy, np.ones_like(height)]); nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
save_png('T_TRexScales_N', np.dstack([nrm * .5 + .5, np.ones_like(height)]))
# Pattern: u along the body (0 snout .. 1 tail tip), v round it (.5 on top).
W, H = 1024, 256
v, u = np.mgrid[0:H, 0:W] / np.array([H, W])[:, None, None]
top = np.clip(1 - np.abs(v - .5) / .32, 0, 1)                    # dorsal half
belly = np.clip((np.abs(v - .5) - .3) / .15, 0, 1)
wobble = .35 * np.sin(v * 2 * math.pi * 2 + u * 13) + .2 * np.sin(u * 47)
bands = (np.sin((u * 17 + wobble) * 2 * math.pi) > .55).astype(float) * np.clip((u - .12) / .08, 0, 1)
mask = np.clip(bands * top ** .7, 0, 1)                          # R: dark dorsal bands
save_png('T_TRexPattern', np.dstack([mask, belly, top, np.ones_like(u)]))   # G: belly, B: back

# Export the skinned mesh (no clips) and each clip on the armature alone.
bpy.ops.object.select_all(action='DESELECT')
arm.name = 'SK_TRex_Rig'; body.name = 'SK_TRex'
arm.animation_data.action = None
arm.data.pose_position = 'REST'            # bind the mesh in its rest pose, whatever was last evaluated
scene.frame_set(0); bpy.context.view_layer.update()
for o in (arm, body): o.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.fbx(filepath=str(OUT / 'SK_TRex.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'}, bake_anim=False, **FBX)
arm.data.pose_position = 'POSE'

clips = {}
for name in CLIPS:
    act = bpy.data.actions[name]
    arm.animation_data.action = act
    f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])
    # Natural ground speed: how fast a planted back foot slides back (the body's speed over it).
    speeds = []
    head_x = []
    for side in ('L', 'R'):
        seq = []
        for f in range(f0, f1 + 1):
            scene.frame_set(f); bpy.context.view_layer.update()
            seq.append(world_bone('BackFoot.' + side))
        # Each contiguous stretch with the foot down: how fast it slides back.
        low = min(p.z for p in seq)
        down = [p.z < low + 12. for p in seq]     # (the raptor's 4 cm, at three times its size)
        i = 0
        while i < len(seq):
            if not down[i]: i += 1; continue
            j = i
            while j + 1 < len(seq) and down[j + 1]: j += 1
            if j - i >= 2:
                dist = seq[i].x - seq[j].x
                if dist > 15: speeds.append(dist / ((j - i) / FPS))
            i = j + 1
    for f in range(f0, f1 + 1):
        scene.frame_set(f); bpy.context.view_layer.update()
        head_x.append(world_bone('Head').x)
    bite = (int(np.argmax(head_x)) + f0) / FPS
    clips[name] = {'length_s': (f1 - f0) / FPS, 'ground_speed': round(sum(speeds) / len(speeds), 1) if speeds else 0.,
                   'reach_moment_s': round(bite, 3), 'head_reach_cm': round(max(head_x) - head_x[0], 1), 'loop': name in LOOPING}
    bpy.ops.object.select_all(action='DESELECT'); arm.select_set(True); bpy.context.view_layer.objects.active = arm
    scene.frame_start, scene.frame_end = f0, f1      # the bake covers the scene range: the clip's own
    bpy.ops.export_scene.fbx(filepath=str(OUT / f'AS_TRex_{name}.fbx'), use_selection=True, object_types={'ARMATURE'},
                             bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
                             bake_anim_simplify_factor=0., bake_anim_step=1., **FBX)
    print('CHUCK_TREX_CLIP', name, json.dumps(clips[name]))
arm.animation_data.action = None
scene.frame_set(0)

# Measurements for the runtime (rest pose).
arm.data.pose_position = 'REST'
bpy.context.view_layer.update()
ev = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
vs = [ev.matrix_world @ v.co for v in ev.data.vertices]
size = {'length_cm': round(max(p.x for p in vs) - min(p.x for p in vs), 1), 'width_cm': round(max(p.y for p in vs) - min(p.y for p in vs), 1),
        'height_cm': round(max(p.z for p in vs) - min(p.z for p in vs), 1), 'snout_x_cm': round(max(p.x for p in vs), 1),
        'tail_x_cm': round(min(p.x for p in vs), 1), 'hips_z_cm': round(world_bone('Hips').z, 1), 'head_z_cm': round(world_bone('Head').z, 1),
        'tris': sum(len(p.vertices) - 2 for p in ev.data.polygons)}
(OUT / 'trex-clips.json').write_text(json.dumps({'size': size, 'clips': clips}, indent=2))
lines = ['#pragma once', '// Generated by Tools/build_trex.py (the Chult T-rex); centimetres, seconds.', 'namespace TRexClipData', '{']
for k, val in size.items():
    if k != 'tris':
        lines.append(f'    constexpr float {"".join(w.capitalize() for w in k.split("_"))} = {val:.1f}f;')
for name, c in clips.items():
    lines.append(f'    constexpr float {name}Length = {c["length_s"]:.4f}f;')
    lines.append(f'    constexpr float {name}GroundSpeed = {c["ground_speed"]:.1f}f;')
    lines.append(f'    constexpr float {name}ReachMoment = {c["reach_moment_s"]:.4f}f;')
lines.append('}')
HEADER.write_text('\n'.join(lines) + '\n', encoding='utf-8')
print('CHUCK_TREX_READY', json.dumps(size))
