"""Blender 4.5 LTS: contract checks for SourceAssets/Chuck/V1/Chuck_V1.blend.

blender --background SourceAssets/Chuck/V1/Chuck_V1.blend --python SourceAssets/Chuck/V1/check_v1.py

Checks the delivered source against docs/RIG-CONTRACT-V1.md and the accepted
table (SourceAssets/Chuck/rig_proposal.json), and re-imports SK_Chuck.fbx
into an empty scene to confirm the exported hierarchy. Never saves.
"""
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector

V1 = Path(bpy.data.filepath).parent
TABLE = json.loads((V1.parent / 'rig_proposal.json').read_text(encoding='utf-8-sig'))
MANIFEST = json.loads((V1 / 'Animations/manifest.json').read_text(encoding='utf-8'))
failures = []
def check(ok, label, detail=''):
    print('CHUCK_V1_CHECK', 'PASS' if ok else 'FAIL', label, detail)
    if not ok: failures.append(label)

rig = bpy.data.objects['SK_Chuck_Rig']; body = bpy.data.objects['SK_Chuck']
bones = rig.data.bones
check(set(bones.keys()) == set(TABLE), 'bone names = accepted table', f'{len(bones)} bones')
check(all((bones[n].parent.name if bones[n].parent else None) == b['parent'] for n, b in TABLE.items()), 'parents')
err = max(max((bones[n].head_local - Vector(b['head'])).length, (bones[n].tail_local - Vector(b['tail'])).length)
          for n, b in TABLE.items())
check(err < 1e-3, 'rest head/tail = table', f'max error {err:.2e} cm')
check(all(bones[n].use_deform == b['deform'] for n, b in TABLE.items()), 'deform flags', f"{sum(b.use_deform for b in bones)} deforming")
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
roll = max(abs(eb.roll) for eb in rig.data.edit_bones)
bpy.ops.object.mode_set(mode='OBJECT')
check(roll < 1e-5, 'edit-bone roll zero', f'{roll:.2e}')

zs = [v.co.z for v in body.data.vertices]
check(abs(max(zs) - 65) < .05, 'ear top 65 cm', f'{max(zs):.3f}')
check(-.05 < min(zs) < .1, 'paw soles at Z=0', f'{min(zs):.3f}')
mats = sorted({m.name.split('.')[0] for m in body.data.materials})
check(mats == sorted(['Fur', 'Chest', 'Jacket', 'Seam', 'Skin', 'Eye', 'Claw', 'Metal', 'Whisker']), 'nine material slots', mats)
groups = {g.index: g.name for g in body.vertex_groups}
deform = {n for n, b in TABLE.items() if b['deform']}
check(set(groups.values()) <= deform, 'only deforming bones skin the mesh', sorted(set(groups.values()) - deform))
used = {groups[g.group] for v in body.data.vertices for g in v.groups if g.weight > 1e-3}
unused = sorted(deform - used)
check(not unused, 'every deforming bone has weights', unused)
bad = sum(1 for v in body.data.vertices if abs(sum(g.weight for g in v.groups) - 1) > 1e-3)
check(bad == 0, 'weight totals == 1', f'{bad} bad of {len(body.data.vertices)}')
maxinf = max(len([g for g in v.groups if g.weight > 1e-4]) for v in body.data.vertices)
check(maxinf <= 8, 'max influences <= 8', maxinf)
# Left/right: vertices clearly on +Y must not be driven by _R bones and vice versa.
cross = 0
for v in body.data.vertices:
    if abs(v.co.y) < 3: continue
    wrong = '_R' if v.co.y > 0 else '_L'
    cross += any(groups[g.group].endswith(wrong) and g.weight > .01 for g in v.groups)
check(cross == 0, 'left/right convention (+Y is _L)', f'{cross} crossed vertices')
uv = body.data.uv_layers
check(len(uv) == 1, 'one UV channel', [l.name for l in uv])
if uv:
    coords = [d.uv for d in uv[0].data]
    inside = all(-1e-4 <= c.x <= 1 + 1e-4 and -1e-4 <= c.y <= 1 + 1e-4 for c in coords)
    check(inside, 'UVs inside 0..1')
mod = next((m for m in body.modifiers if m.type == 'ARMATURE'), None)
check(mod is not None and mod.object == rig, 'armature modifier bound to rig')

for clip in MANIFEST['clips']:
    action = bpy.data.actions.get(f"AS_Chuck_{clip['name']}")
    ok = action is not None and tuple(action.frame_range) == (clip['first_frame'], clip['last_frame'])
    check(ok, f"clip {clip['name']} frames", action.frame_range[:] if action else None)
    check((V1 / clip['file']).exists(), f"clip {clip['name']} fbx exists")
    check(clip['root_motion'] is False, f"clip {clip['name']} root fixed")
    if action:
        root_curves = [c for c in action.fcurves if c.data_path.startswith('pose.bones["root"]')]
        moved = max((abs(k.co[1] - (1 if c.data_path.endswith('rotation_quaternion') and c.array_index == 0 else 0))
                     for c in root_curves for k in c.keyframe_points), default=0)
        check(moved < 1e-6, f"clip {clip['name']} root bone static", f'{moved:.2e}')

# Ground: no deformed vertex below the floor in any frame of any clip.
rig.animation_data_create()
for clip in MANIFEST['clips']:
    rig.animation_data.action = bpy.data.actions[f"AS_Chuck_{clip['name']}"]
    low = (9., None)
    for f in range(clip['first_frame'], clip['last_frame'] + 1):
        bpy.context.scene.frame_set(f)
        ev = body.evaluated_get(bpy.context.evaluated_depsgraph_get()); m = ev.to_mesh()
        z = min(v.co.z for v in m.vertices)
        ev.to_mesh_clear()
        if z < low[0]: low = (z, f)
    check(low[0] > -.1, f"clip {clip['name']} stays above ground", f'min z {low[0]:.3f} cm at frame {low[1]}')
rig.animation_data.action = None

# Seams: WalkStart's last frame and WalkStop's first frame must equal
# WalkLoop frame 0 so the transitions blend without a pop.
def pose_at(action_name, frame):
    rig.animation_data_create(); rig.animation_data.action = bpy.data.actions[action_name]
    bpy.context.scene.frame_set(frame); bpy.context.view_layer.update()
    return {pb.name: pb.matrix.copy() for pb in rig.pose.bones}
def seam_error(a, b):
    loc = max((a[n].translation - b[n].translation).length for n in a)
    rot = max(math.degrees(a[n].to_quaternion().rotation_difference(b[n].to_quaternion()).angle) for n in a)
    return loc, rot
clips = {c['name']: c for c in MANIFEST['clips']}
if {'WalkLoop', 'WalkStart', 'WalkStop'} <= set(clips):
    loop0 = pose_at('AS_Chuck_WalkLoop', 0)
    for name, frame in (('WalkStart', clips['WalkStart']['last_frame']), ('WalkStop', 0)):
        loc, rot = seam_error(pose_at(f'AS_Chuck_{name}', frame), loop0)
        check(loc < .05 and rot < .5, f'seam {name} ~ WalkLoop frame 0', f'{loc:.3f} cm, {rot:.2f} deg')
rig.animation_data.action = None

# Re-import the delivered FBX into a clean scene and verify the hierarchy.
bpy.ops.wm.read_homefile(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(V1 / 'SK_Chuck.fbx'))
arms = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE']
check(len(arms) == 1, 'fbx: one armature')
if arms:
    names = set(arms[0].data.bones.keys())
    check(names == set(TABLE), 'fbx: 41 named bones incl. helpers', f'{len(names)}; extra {sorted(names - set(TABLE))}')
    fb = arms[0].data.bones
    scale = arms[0].matrix_world.to_scale()[0]
    err = max(((arms[0].matrix_world @ fb[n].head_local) - Vector(b['head']) * (1 if scale > .5 else .01)).length
              for n, b in TABLE.items() if n in fb)
    print('CHUCK_V1_INFO fbx armature object scale', [round(s, 4) for s in arms[0].matrix_world.to_scale()],
          'max world head error', round(err, 5))
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
check(len(meshes) == 1, 'fbx: one skinned mesh')

# Groom variant and strand groom (optional delivery; checked when present).
groom_meta = V1 / 'Groom' / 'groom_metadata.json'
if groom_meta.exists():
    from mathutils.bvhtree import BVHTree
    meta = json.loads(groom_meta.read_text(encoding='utf-8'))
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(V1 / 'SK_Chuck_Groomed.fbx'))
    gmesh = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    garm = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE']
    check(len(gmesh) == 1 and len(garm) == 1 and set(garm[0].data.bones.keys()) == set(TABLE),
          'groom variant fbx: one mesh on the same 41-bone skeleton')
    before = set(bpy.data.objects)
    bpy.ops.wm.alembic_import(filepath=str(V1 / meta['file']))
    curves = {o.name.split('.')[0]: o for o in set(bpy.data.objects) - before if o.type == 'CURVES'}
    check(set(curves) == set(meta['groups']), 'groom groups', sorted(curves))
    for name, o in curves.items():
        n = len(o.data.curves)
        check(n == meta['groups'][name]['strands'], f'groom {name} strand count', n)
    if gmesh and curves:
        m = gmesh[0]
        deps = bpy.context.evaluated_depsgraph_get()
        tree = BVHTree.FromObject(m, deps)
        # The FBX mesh keeps source-centimetre local coordinates under a 0.01
        # (metre) object scale; the Alembic curves are raw source centimetres.
        worst, samples = 0., 0
        for o in curves.values():
            pts = o.data.points; step = meta['points_per_strand']
            for c in range(0, len(o.data.curves), 97):
                root = pts[c * step].position
                hit = tree.find_nearest(root)
                worst = max(worst, (hit[0] - root).length); samples += 1
        check(worst < .1, 'groom roots on SK_Chuck_Groomed surface', f'{samples} sampled, max {worst:.4f} cm')

print('CHUCK_V1_CHECK_DONE', 'FAIL' if failures else 'PASS', failures)
sys.exit(1 if failures else 0)
