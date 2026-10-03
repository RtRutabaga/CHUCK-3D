"""Blender 4.5.14: retarget the CMU idle / talk takes (Tools/Fetch-CMUMocap.ps1)
onto the humans' shared skeleton (MPFB game_engine; bones mapped by name, TAKE).

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
CLIPS = {            # clip: (take, seconds to skip after the T-pose, seconds to keep or None, loops)
    'StandHip': ('111_28', 1.0, None, True),     # weight on one leg, a hand to the hip
    'StandLook': ('77_02', 1.0, None, True),     # standing, looking about
    'Talk': ('18_08', 1.0, None, True),          # explaining with the hands
    'React': ('79_73', 1.4, 2.0, False),         # scratched by the rat: hands to the chest, a lean back, a small shift of the feet
    # The sewer zombie (user 2026-10-03), an old man's carriage: hunched,
    # knees bent. The walk is in place (the runtime moves it at the clip's
    # own speed); the collapse is a lay-down played fast (ADockNPC).
    'ZombieIdle': ('137_33', .3, 1.8, True),     # "Old Man Walk" before he sets off: stooped, swaying
    # (137_32 "Old Man Wait" leans on its knees: catching its breath, not dead)
    'ZombieWalk': ('137_33', 2.5, 6.0, 'walk'),  # "Old Man Walk": a slow, bent-kneed shuffle
    'ZombieFall': ('113_08', 1.0, 4.5, False),   # "Lay down": knees go, down onto the floor, flat on its back
    # (cut at 2.6 s it was still propped on its elbows, as if getting up; 111_12 "Lay down" ends sitting)
    # Tried and dropped for the zombie: 104_41 ZombieWalk (arms out), 91_24
    # HurtLegWalk, 104_13 StumbleWalk, 90_16 "fall on face" (a dive).
    # Tried and dropped: 140_06/07 "Idle" (a crouched ready stance), 113_21
    # "Standing still" (head tipped far back), 141_20 "Waiting" (fidgety, 5 s),
    # 76_06 "avoid stepping on something" (a cartoonish hop with flailing arms).
}
# The humans' rig (MPFB game_engine, Unreal mannequin names) -> the CMU take's
# bone. Bones with no take bone (the root, every finger: CMU has no finger
# motion worth using) follow their parent; the runtime poses the fingers.
TAKE = {'pelvis': 'Hips', 'spine_01': 'LowerBack', 'spine_02': 'Spine', 'spine_03': 'Spine1', 'neck_01': 'Neck', 'head': 'Head'}
for side, Side in (('l', 'Left'), ('r', 'Right')):
    TAKE.update({f'clavicle_{side}': f'{Side}Shoulder', f'upperarm_{side}': f'{Side}Arm', f'lowerarm_{side}': f'{Side}ForeArm',
                 f'hand_{side}': f'{Side}Hand', f'thigh_{side}': f'{Side}UpLeg', f'calf_{side}': f'{Side}Leg',
                 f'foot_{side}': f'{Side}Foot', f'ball_{side}': f'{Side}ToeBase'})
# Bones aimed at the reference: (rig child joint, take bone, take child joint).
# Only the limbs: the A-pose and the T-pose differ there. The spine, neck,
# head and collarbones keep this rig's own neutral carriage (MakeHuman's neck
# sits forward of the chest; forcing it onto CMU's straight neck tips the head
# back); the clip moves them from there. The hand aims at the middle finger
# (the take's FingerBase sits on its Hand joint, so its index joint is used).
AIM = {}
for side, Side in (('l', 'Left'), ('r', 'Right')):
    AIM.update({f'upperarm_{side}': (f'lowerarm_{side}', f'{Side}Arm', f'{Side}ForeArm'),
                f'lowerarm_{side}': (f'hand_{side}', f'{Side}ForeArm', f'{Side}Hand'),
                f'hand_{side}': (f'middle_01_{side}', f'{Side}Hand', f'{Side}HandIndex1'),
                f'thigh_{side}': (f'calf_{side}', f'{Side}UpLeg', f'{Side}Leg'),
                f'calf_{side}': (f'foot_{side}', f'{Side}Leg', f'{Side}Foot'),
                f'foot_{side}': (f'ball_{side}', f'{Side}Foot', f'{Side}ToeBase')})
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


def retarget(rig, clip, take, skip, keep, loops=True):
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
    turn = yaw_between(lateral(take_heads, 'LeftUpLeg', 'RightUpLeg'), lateral(rig_heads, 'thigh_l', 'thigh_r'))
    scale = ((rig_heads['pelvis'] - (rig_heads['foot_l'] + rig_heads['foot_r']) * .5).z /
             (take_heads['Hips'] - (take_heads['LeftFoot'] + take_heads['RightFoot']) * .5).z)
    order = sorted(rig.data.bones, key=lambda b: len(b.parent_recursive))   # parents first
    aimed = {}
    for b in order:
        D = aimed[b.parent.name] if b.parent else Quaternion()
        if b.name in AIM:
            child, tb, tc = AIM[b.name]
            if (rig_heads[child] - rig_heads[b.name]).length > 1e-5 and (take_heads[tc] - take_heads[tb]).length > 1e-5:
                now = D @ (rig_heads[child] - rig_heads[b.name]).normalized()
                want = turn @ (take_heads[tc] - take_heads[tb]).normalized()
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
        change = {}   # each rig bone's change of world rotation since the T-pose
        for b in order:
            t = TAKE.get(b.name)
            change[b.name] = (turn @ rots[t] @ take_ref[t].inverted() @ turn.inverted() if t
                              else (change[b.parent.name] if b.parent else Quaternion()))
        pose = {n: change[n] @ rig_T[n] for n in rig_T}
        hips = turn @ (heads['Hips'] - take_heads['Hips']) * scale
        facing = turn @ lateral(heads, 'LeftUpLeg', 'RightUpLeg')
        frames.append((pose, hips, facing))
    # Remove drift: the clip's mean facing and mean ground position are the NPC's own.
    mean_side = sum((fr[2] for fr in frames), Vector()).normalized()
    unturn = yaw_between(mean_side, lateral(rig_heads, 'thigh_l', 'thigh_r'))
    centre = sum((unturn @ fr[1] for fr in frames), Vector()) / len(frames); centre.z = 0
    frames = [({n: unturn @ q for n, q in pose.items()}, unturn @ hips - centre) for pose, hips, _ in frames]
    speed = 0.
    if loops == 'walk':
        # In place: take out the steady travel (a least-squares line through
        # the hips); the runtime moves the NPC at this speed instead.
        k = len(frames); mid = (k - 1) / 2
        mean = sum((fr[1] for fr in frames), Vector()) / k
        slope = sum(((t - mid) * (fr[1] - mean) for t, fr in enumerate(frames)), Vector()) / sum((t - mid) ** 2 for t in range(k))
        slope.z = 0
        speed = slope.length * FPS * 100.
        frames = [(pose, hips - slope * (t - mid)) for t, (pose, hips) in enumerate(frames)]
        # Loop on a matching step: from at least 1.5 s on, the frame whose legs
        # best match the first (and room after it for the seam's blend).
        legs = ('thigh_l', 'thigh_r', 'calf_l', 'calf_r', 'foot_l', 'foot_r')
        def gap(a, b):
            return sum(min(a[n].rotation_difference(b[n]).angle, 2 * math.pi - a[n].rotation_difference(b[n]).angle) for n in legs)
        seam = 6
        best = min(range(int(1.5 * FPS), len(frames) - seam), key=lambda f: gap(frames[f][0], frames[0][0]))
        frames = frames[:best + seam]
        print('CHUCK_WALK_LOOP', clip, f'speed_cm_s={speed:.1f}', f'cycle_frames={best}', f'seam_error={gap(frames[best][0], frames[0][0]):.3f}')
    # Loop: the first second becomes a blend from the clip's end back into its
    # start. (A one-shot is kept as it is; the runtime blends it in and out.)
    n = (6 if loops == 'walk' else min(LOOP, len(frames) // 3)) if loops else 0
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
    return {'fbx': f'Anim/AS_Human_{clip}.fbx', 'take': take, 'frames': len(looped), 'fps': FPS, 'loops': bool(loops), 'speed_cm_s': round(speed, 1)}


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
for clip, (take, skip, keep, loops) in CLIPS.items():
    manifest['clips'][clip] = retarget(rig, clip, take, skip, keep, loops)
(OUT / 'manifest.json').write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
print('CHUCK_CLIPS_READY', sorted(manifest['clips']))
