"""Blender 4.5.14: retarget the CMU idle / talk takes (Tools/Fetch-CMUMocap.ps1)
onto the humans' shared skeleton (MPFB cmu_mb: the same bone names).

blender --background --python Tools/build_npc_mocap.py
  -> SourceAssets/NPCs/Humans/Anim/AS_Human_<Clip>.fbx (30 fps, looping; carries the worker's mesh for its bind pose)

The rig is read from the dock worker's FBX (the skeleton every human shares).
Each take's first frame is a T-pose (added by the BVH conversion); the rig's
A-pose is first aimed joint by joint onto that T-pose, so that from then on
every bone can follow the take's change of world rotation:
    rig(f) = take(f) * take(T)^-1 * rig(T)
The take is turned to face the rig's way; drift is removed (the clip's mean
facing and mean position become the NPC's own), the hips keep their small
movements at the rig's scale, and the last second is blended into the first
so the clip loops. The runtime (ADockNPC) applies the clip as changes of
rotation from this rig's rest, so it fits every body on the skeleton.
"""
from pathlib import Path
import json
import math
import sys
import bpy
from mathutils import Quaternion, Vector

ROOT = Path(__file__).resolve().parents[1]
TAKES = ROOT / 'SourceAssets/Mocap/CMU'
OUT = ROOT / 'SourceAssets/NPCs/Humans/Anim'
RIG_FBX = ROOT / 'SourceAssets/NPCs/Humans/DockWorker/SK_DockWorker.fbx'
FPS = 30
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None
LOOP = 30            # frames blended across the loop seam (1 s)
CLIPS = {            # clip: (take, seconds to skip after the T-pose, seconds to keep or None)
    'StandHip': ('111_28', 1.0, None),     # weight on one leg, a hand to the hip
    'StandLook': ('77_02', 1.0, None),     # standing, looking about
    'Talk': ('18_08', 1.0, None),          # explaining with the hands
    # Tried and dropped: 140_06/07 "Idle" (a crouched ready stance), 113_21
    # "Standing still" (head tipped far back), 141_20 "Waiting" (fidgety, 5 s).
}
# The joint that sets each bone's direction at the reference (None: follows
# its parent). Only the limbs are aimed: the A-pose and the T-pose differ
# there. The spine, neck, head and collarbones keep this rig's own neutral
# carriage (MakeHuman's neck sits forward of the chest; forcing it onto CMU's
# straight neck tips the head back); the clip moves them from there. The hand
# aims at the finger: in the CMU takes FingerBase sits on the Hand's joint.
AIM = {'Hips': None, 'LHipJoint': None, 'LeftUpLeg': 'LeftLeg', 'LeftLeg': 'LeftFoot', 'LeftFoot': 'LeftToeBase', 'LeftToeBase': None,
       'RHipJoint': None, 'RightUpLeg': 'RightLeg', 'RightLeg': 'RightFoot', 'RightFoot': 'RightToeBase', 'RightToeBase': None,
       'LowerBack': None, 'Spine': None, 'Spine1': None, 'Neck': None, 'Neck1': None, 'Head': None,
       'LeftShoulder': None, 'LeftArm': 'LeftForeArm', 'LeftForeArm': 'LeftHand', 'LeftHand': 'LeftHandFinger1',
       'LeftFingerBase': 'LeftHandFinger1', 'LeftHandFinger1': None, 'LThumb': None,
       'RightShoulder': None, 'RightArm': 'RightForeArm', 'RightForeArm': 'RightHand', 'RightHand': 'RightHandFinger1',
       'RightFingerBase': 'RightHandFinger1', 'RightHandFinger1': None, 'RThumb': None}
TAKE_NAME = {'LeftHandFinger1': 'LeftHandIndex1', 'RightHandFinger1': 'RightHandIndex1'}   # rig -> take
FBX = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False, primary_bone_axis='Y',
           secondary_bone_axis='X', use_armature_deform_only=False, bake_anim=True, bake_anim_use_all_actions=False,
           bake_anim_use_nla_strips=False, bake_anim_step=1., bake_anim_simplify_factor=0., bake_anim_force_startend_keying=True)


def clear():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    for block in (bpy.data.actions, bpy.data.armatures, bpy.data.meshes):
        for item in list(block): block.remove(item)


def load_rig():
    bpy.ops.import_scene.fbx(filepath=str(RIG_FBX), automatic_bone_orientation=False, ignore_leaf_bones=False)
    rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
    return rig   # its skinned meshes stay: they give each clip's FBX a bind pose


def world_rest(arm):
    """World rotation and head position of each bone at rest."""
    _, q, _ = arm.matrix_world.decompose()
    return ({b.name: q @ b.matrix_local.to_quaternion() for b in arm.data.bones},
            {b.name: arm.matrix_world @ b.head_local for b in arm.data.bones})


def world_pose(arm):
    _, q, _ = arm.matrix_world.decompose()
    return ({p.name: q @ p.matrix.to_quaternion() for p in arm.pose.bones},
            {p.name: arm.matrix_world @ p.head for p in arm.pose.bones})


def lateral(heads, left, right):
    v = heads[right] - heads[left]; v.z = 0
    return v.normalized()


def yaw_between(a, b):
    """Rotation about Z taking horizontal direction a onto b."""
    return Quaternion((0, 0, 1), math.atan2(a.x * b.y - a.y * b.x, a.x * b.x + a.y * b.y))


def retarget(rig, clip, take, skip, keep):
    bpy.ops.import_anim.bvh(filepath=str(TAKES / f'{take}.bvh'), axis_forward='-Z', axis_up='Y', rotate_mode='NATIVE',
                            update_scene_fps=False, update_scene_duration=False)
    bvh = bpy.context.object
    scene = bpy.context.scene
    start, end = int(bvh.animation_data.action.frame_range[0]), int(bvh.animation_data.action.frame_range[1])
    step = round(1. / (float(open(TAKES / f'{take}.bvh').read().split('Frame Time:')[1].split()[0]) * FPS))   # 120 fps -> every 4th
    # The reference: the take's T-pose (its first frame) and the rig aimed onto it.
    scene.frame_set(start)
    take_ref, take_heads = world_pose(bvh)
    rig_rest, rig_heads = world_rest(rig)
    turn = yaw_between(lateral(take_heads, 'LeftUpLeg', 'RightUpLeg'), lateral(rig_heads, 'LeftUpLeg', 'RightUpLeg'))
    scale = (rig_heads['Hips'] - (rig_heads['LeftFoot'] + rig_heads['RightFoot']) * .5).z / \
            (take_heads['Hips'] - (take_heads['LeftFoot'] + take_heads['RightFoot']) * .5).z
    aimed = {}
    for b in sorted(rig.data.bones, key=lambda b: len(b.parent_recursive)):   # parents first
        D = aimed[b.parent.name] if b.parent else Quaternion()
        child = AIM.get(b.name)
        tn = lambda n: TAKE_NAME.get(n, n)
        if child and (rig_heads[child] - rig_heads[b.name]).length > 1e-5 and (take_heads[tn(child)] - take_heads[tn(b.name)]).length > 1e-5:
            now = D @ (rig_heads[child] - rig_heads[b.name]).normalized()
            want = turn @ (take_heads[tn(child)] - take_heads[tn(b.name)]).normalized()
            D = now.rotation_difference(want) @ D
        aimed[b.name] = D
    rig_T = {n: aimed[n] @ rig_rest[n] for n in aimed}
    # Every kept frame, as world rotations and hip offsets for the rig.
    first = start + max(1, round(skip * 120))
    last = end if keep is None else min(end, first + round(keep * 120))
    frames = []
    for f in range(first, last + 1, step):
        scene.frame_set(f)
        rots, heads = world_pose(bvh)
        pose = {n: turn @ rots[TAKE_NAME.get(n, n)] @ take_ref[TAKE_NAME.get(n, n)].inverted() @ turn.inverted() @ rig_T[n] for n in rig_T}
        hips = turn @ (heads['Hips'] - take_heads['Hips']) * scale
        facing = turn @ lateral(heads, 'LeftUpLeg', 'RightUpLeg')
        frames.append((pose, hips, facing))
    # Remove drift: the clip's mean facing and mean ground position are the NPC's own.
    mean_side = sum((fr[2] for fr in frames), Vector()).normalized()
    unturn = yaw_between(mean_side, lateral(rig_heads, 'LeftUpLeg', 'RightUpLeg'))
    centre = sum((unturn @ fr[1] for fr in frames), Vector()) / len(frames); centre.z = 0
    frames = [({n: unturn @ q for n, q in pose.items()}, unturn @ hips - centre) for pose, hips, _ in frames]
    # Loop: the first second becomes a blend from the clip's end back into its start.
    n = min(LOOP, len(frames) // 3)
    m = len(frames) - n
    looped = []
    for f in range(m):
        if f < n:
            w = f / n
            a, b = frames[f + m], frames[f]
            looped.append(({k: a[0][k].slerp(b[0][k], w) for k in a[0]}, a[1].lerp(b[1], w)))
        else:
            looped.append(frames[f])
    bpy.data.objects.remove(bvh)
    # Keyframe the rig: world rotations -> each bone's own (basis) rotation.
    _, qobj, sobj = rig.matrix_world.decompose()
    rig.animation_data_create()
    action = bpy.data.actions.new(f'AS_Human_{clip}'); rig.animation_data.action = action
    for p in rig.pose.bones:
        p.rotation_mode = 'QUATERNION'
    for i, (pose, hips) in enumerate(looped):
        arm_q = {k: qobj.inverted() @ v for k, v in pose.items()}
        for p in rig.pose.bones:
            b = p.bone
            rest = b.matrix_local.to_quaternion()
            if b.parent:
                rel = b.parent.matrix_local.to_quaternion().inverted() @ rest
                basis = rel.inverted() @ arm_q[b.parent.name].inverted() @ arm_q[b.name]
            else:
                basis = rest.inverted() @ arm_q[b.name]
                p.location = rest.inverted() @ (qobj.inverted() @ Vector(c / s for c, s in zip(hips, sobj)))
                p.keyframe_insert('location', frame=i + 1)
            p.rotation_quaternion = basis
            p.keyframe_insert('rotation_quaternion', frame=i + 1)
    scene.frame_start, scene.frame_end = 1, len(looped)
    OUT.mkdir(parents=True, exist_ok=True)
    # Exported with the worker's skinned meshes: without a skin there is no bind
    # pose and importers take the pose at export time as the rest.
    bpy.ops.object.select_all(action='DESELECT')
    for o in [rig] + [c for c in rig.children if c.type == 'MESH']: o.select_set(True)
    bpy.context.view_layer.objects.active = rig
    path = OUT / f'AS_Human_{clip}.fbx'
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'ARMATURE', 'MESH'}, **FBX)
    if REVIEW: review(clip, len(looped))
    rig.animation_data.action = None
    for p in rig.pose.bones:
        p.rotation_quaternion = Quaternion(); p.location = Vector()
    print('CHUCK_CLIP', clip, take, f'frames={len(looped)}', f'seconds={len(looped) / FPS:.1f}', f'scale={scale:.4f}')
    return {'fbx': f'Anim/AS_Human_{clip}.fbx', 'take': take, 'frames': len(looped), 'fps': FPS}


def review(clip, frames):
    """The keyed rig itself (no FBX round trip), front three-quarter, at four times."""
    import os
    s = bpy.context.scene
    if 'Review' not in bpy.data.objects:
        cam = bpy.data.objects.new('Review', bpy.data.cameras.new('Review')); s.collection.objects.link(cam)
        cam.location = (4.2, -1.6, 1.0); cam.rotation_euler = (math.radians(88), 0, math.radians(69)); cam.data.lens = 50
        s.camera = cam; s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.light = 'STUDIO'
        s.render.resolution_x, s.render.resolution_y = 500, 700
    for k in range(4):
        f = 1 + k * (frames - 1) // 3
        s.frame_set(f); s.render.filepath = os.path.join(REVIEW, f'{clip}_{k}.png'); bpy.ops.render.render(write_still=True)


clear()
rig = load_rig()
bpy.context.scene.render.fps, bpy.context.scene.render.fps_base = FPS, 1.   # after the import, which sets the file's rate
manifest = {'generator': 'Tools/build_npc_mocap.py', 'source': 'SourceAssets/Mocap/CMU (CMU Graphics Lab Motion Capture Database)', 'clips': {}}
for clip, (take, skip, keep) in CLIPS.items():
    manifest['clips'][clip] = retarget(rig, clip, take, skip, keep)
(OUT / 'manifest.json').write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
print('CHUCK_CLIPS_READY', sorted(manifest['clips']))
