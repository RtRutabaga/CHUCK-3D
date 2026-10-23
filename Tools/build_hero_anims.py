"""Blender 4.5.14: the temple heroes' clips, authored as motion (docs/CHULT-TEMPLE-HEROES.md).

blender --background --python Tools/build_hero_anims.py [-- Fighter|Wizard|Ranger ...] [--only Clip,Clip] [--review <dir>]
  -> SourceAssets/Heroes/<Name>/Anim/AS_Hero<Name>_<Clip>.fbx, SourceAssets/Heroes/<Name>/Anim/clips.json

No motion-capture library has a realistic sword-and-shield fighter that is free to use (CMU's
"swordplay" is two-handed play-acting, Quaternius' CC0 libraries are acrobatic game moves), so the
heroes' clips are authored here, as motion rather than poses:

  * The weapon leads. A cut is the sword's path: grip positions and blade directions keyed in time
    and joined by Catmull-Rom curves on time knots (velocity-continuous), slow in the wind-up,
    fastest at contact, decelerating in the follow-through. The blade's cutting edge always faces
    the way the blade is moving (taken from the tip's velocity), so every cut lands edge first.
  * The body drives it. Hips turn first, the chest a few frames after, the arm last (the kinetic
    chain); the front foot steps and plants just before contact; the weight goes back in the wind-up
    and forward through the cut; the knees sink as it lands.
  * Arms and legs are solved by analytic two-bone IK from the joints' rest positions (the FBX
    bones' tails are not reliable): the elbow and knee are hinges (the upper limb's twist puts the
    lower limb in its bend plane), the forearm takes all of the hand's twist (a wrist cannot twist),
    clavicles lift and reach with the hand. The sword is on weapon_r (a rigid child of the right
    hand), so the hand is placed from where the sword must be; the shield on shield_l (the left
    forearm), so the forearm is placed from where the shield must be.
  * Hits taken are motion too: an impulse through a damped spring on the hips, chest and head (the
    blow pushes, the body gives and comes back), a reactive step when it is big, arms thrown out
    for balance.

Each clip's moment of contact and the blade tip's path are measured from the solved pose and
written to clips.json (the game times its hit-stop and its cut tests from them).
Every clip is keyed on every frame at 30 fps and exported with the hero's skinned meshes (their
bind pose).
"""
from pathlib import Path
import json
import math
import sys
import bpy
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[1]
HEROES = ROOT / 'SourceAssets/Heroes'
FPS = 30
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None
ONLY_CLIPS = argv[argv.index('--only') + 1].split(',') if '--only' in argv else None
WHO = [a for a in argv if a in ('Fighter', 'Wizard', 'Ranger')] or ['Fighter', 'Wizard', 'Ranger']
FBX_EXPORT = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False, primary_bone_axis='Y',
                  secondary_bone_axis='X', use_armature_deform_only=False, bake_anim=True, bake_anim_use_all_actions=False,
                  bake_anim_use_nla_strips=False, bake_anim_step=1., bake_anim_simplify_factor=0., bake_anim_force_startend_keying=True,
                  mesh_smooth_type='FACE', colors_type='LINEAR')

V = Vector
clamp = lambda x, a=0., b=1.: min(b, max(a, x))
smooth = lambda x: (lambda t: t * t * (3 - 2 * t))(clamp(x))
Z = V((0, 0, 1))


def rot(axis, deg):
    return Quaternion(V(axis).normalized(), math.radians(deg))


def swing(a, b):
    """The smallest rotation taking direction a onto b."""
    a = a.normalized(); b = b.normalized()
    return a.rotation_difference(b)


def twist_about(q, axis):
    """The twist part of q about axis (swing-twist decomposition)."""
    axis = axis.normalized()
    p = V((q.x, q.y, q.z)).dot(axis) * axis
    t = Quaternion((q.w, p.x, p.y, p.z))
    if t.magnitude < 1e-9: return Quaternion()
    t.normalize()
    return t


def frame_from(d, e):
    """A rotation whose X is d and whose Y is e (orthogonalised): a blade frame (along, edge)."""
    d = d.normalized(); e = (e - d * e.dot(d))
    if e.length < 1e-6: e = d.orthogonal()
    e.normalize(); n = d.cross(e)
    return Matrix((d, e, n)).transposed().to_quaternion()


# ------------------------------------------------------------------ tracks
class Track:
    """Keys [(t, value)] (float or Vector) joined by a Catmull-Rom curve on time knots: smooth, and
    velocity-continuous through the keys. A key repeated holds; values clamp outside the range."""
    def __init__(self, keys):
        self.k = sorted(keys, key=lambda kv: kv[0])

    def __call__(self, t):
        k = self.k
        if t <= k[0][0]: return k[0][1].copy() if isinstance(k[0][1], Vector) else k[0][1]
        if t >= k[-1][0]: return k[-1][1].copy() if isinstance(k[-1][1], Vector) else k[-1][1]
        i = max(j for j in range(len(k) - 1) if k[j][0] <= t)
        t0, p0 = k[i]; t1, p1 = k[i + 1]
        def tangent(j):
            if j <= 0: a, b = k[0], k[1]
            elif j >= len(k) - 1: a, b = k[-2], k[-1]
            else: a, b = k[j - 1], k[j + 1]
            if b[0] - a[0] < 1e-9: return p0 * 0
            m = (b[1] - a[1]) / (b[0] - a[0])
            # a held key (the same value as a neighbour) stops there
            if j > 0 and j < len(k) - 1:
                same_prev = (k[j][1] - k[j - 1][1]); same_next = (k[j + 1][1] - k[j][1])
                if (same_prev.length if isinstance(same_prev, Vector) else abs(same_prev)) < 1e-9 or \
                   (same_next.length if isinstance(same_next, Vector) else abs(same_next)) < 1e-9:
                    return m * 0
            return m
        h = t1 - t0; s = (t - t0) / h
        m0, m1 = tangent(i) * h, tangent(i + 1) * h
        h00, h10, h01, h11 = 2 * s ** 3 - 3 * s ** 2 + 1, s ** 3 - 2 * s ** 2 + s, -2 * s ** 3 + 3 * s ** 2, s ** 3 - s ** 2
        return p0 * h00 + m0 * h10 + p1 * h01 + m1 * h11


class Spring:
    """A damped spring kicked by impulses (a hit's push on the body): x'' = -k x - c x' + kicks."""
    def __init__(self, k=180., c=16.):
        self.k, self.c = k, c

    def run(self, kicks, t_end, dt=1. / 240, norm=False):
        """kicks: [(t, velocity added)]; returns a function t -> displacement (norm: its peak is 1)."""
        xs, x, v, t, ks = [], 0., 0., 0., sorted(kicks)
        while t <= t_end + dt:
            while ks and ks[0][0] <= t: v += ks[0][1]; ks.pop(0)
            a = -self.k * x - self.c * v
            v += a * dt; x += v * dt; t += dt
            xs.append(x)
        if norm:
            peak = max(abs(x) for x in xs) or 1.
            xs = [x / peak for x in xs]
        return lambda tt: xs[min(len(xs) - 1, max(0, int(tt / dt)))]


# ------------------------------------------------------------------ the rig
class Rig:
    def __init__(self, name):
        self.name = name
        bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
        for block in (bpy.data.actions, bpy.data.armatures, bpy.data.meshes, bpy.data.materials):
            for item in list(block): block.remove(item)
        bpy.ops.import_scene.fbx(filepath=str(HEROES / f'Hero{name}/SK_Hero{name}.fbx'), automatic_bone_orientation=False, ignore_leaf_bones=False)
        self.obj = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
        mw = self.obj.matrix_world
        self.q_obj = mw.to_quaternion()
        bones = self.obj.data.bones
        self.parent = {b.name: (b.parent.name if b.parent else None) for b in bones}
        self.rest_q = {b.name: self.q_obj @ b.matrix_local.to_quaternion() for b in bones}
        self.rest_h = {b.name: mw @ b.head_local for b in bones}
        self.order = [b.name for b in sorted(bones, key=lambda b: len(b.parent_recursive))]
        self.children = {n: [c for c, p in self.parent.items() if p == n] for n in self.parent}
        H = self.rest_h
        self.ankle_h = (H['foot_l'].z + H['foot_r'].z) / 2
        for s in 'lr':
            assert (H[f'lowerarm_{s}'] - H[f'upperarm_{s}']).length > .1
        # the palm's way for each hand (rest), from the finger roots, as build_temple_heroes.grip_frame
        self.palm = {}
        for s in 'lr':
            along = (H[f'middle_01_{s}'] - H[f'hand_{s}']).normalized()
            across = (H[f'index_01_{s}'] - H[f'pinky_01_{s}']).normalized()
            palm = along.cross(across).normalized()
            if (H[f'thumb_01_{s}'] - H[f'hand_{s}']).dot(palm) < 0: palm = -palm
            self.palm[s] = (palm, along, across)

    def has(self, b):
        return b in self.parent


class Pose:
    """D[b]: each bone's change of world rotation from rest; heads from forward kinematics."""
    def __init__(self, rig):
        self.rig = rig
        self.D = {b: Quaternion() for b in rig.order}
        self.root_shift = V()   # the pelvis' move from its rest position
        self.loc = {}           # bone: world displacement of its head from where its parent carries it (the bowstring)
        self.scale = {}         # bone: uniform scale (the nocked arrow, hidden once loosed)

    def fk(self):
        R = self.rig; H = {}
        for b in R.order:
            p = R.parent[b]
            if p is None: H[b] = R.rest_h[b].copy()
            elif b == 'pelvis': H[b] = R.rest_h[b] + self.root_shift
            else: H[b] = H[p] + self.D[p] @ (R.rest_h[b] - R.rest_h[p])
        self.H = H
        return H

    def carry(self, b, q):
        """Set bone b's delta and give every descendant the same (rigidly carried)."""
        stack = [b]
        while stack:
            n = stack.pop(); self.D[n] = q.copy(); stack += self.rig.children[n]

    def turn(self, b, q):
        """Turn b (and what it carries) by world rotation q about its own joint."""
        stack = [b]
        while stack:
            n = stack.pop(); self.D[n] = q @ self.D[n]; stack += self.rig.children[n]


def two_bone(S, l1, l2, T, pole):
    """Elbow/knee position for a chain rooted at S reaching T, bending toward pole."""
    d = T - S; dist = clamp(d.length, abs(l1 - l2) + 1e-3, l1 + l2 - 1e-4)
    dn = d.normalized()
    a = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist)
    h = math.sqrt(max(0., l1 * l1 - a * a))
    p = pole - dn * pole.dot(dn)
    if p.length < 1e-6: p = dn.orthogonal()
    return S + dn * a + p.normalized() * h, S + dn * dist


def solve_limb(pose, upper, lower, end, target, pole, end_delta=None, twist_share=1.):
    """Place upper/lower so that end's head reaches target, the joint bending toward pole; the lower
    bone takes the end's twist (end_delta: the end bone's wanted delta, else it is carried)."""
    R = pose.rig; H = pose.fk()
    S = H[upper]
    l1 = (R.rest_h[lower] - R.rest_h[upper]).length; l2 = (R.rest_h[end] - R.rest_h[lower]).length
    E, Treach = two_bone(S, l1, l2, target, pole)
    u0 = pose.D[upper] @ (R.rest_h[lower] - R.rest_h[upper])        # as currently carried
    f0 = pose.D[upper] @ (R.rest_h[end] - R.rest_h[lower])
    u1, f1 = E - S, Treach - E
    sw = swing(u0, u1)
    # hinge: twist the upper bone about its new axis so the lower one lies in the bend plane
    f_c = sw @ f0
    n_want = u1.cross(f1); n_have = u1.cross(f_c)
    tw = Quaternion()
    if n_want.length > 1e-6 and n_have.length > 1e-6:
        tw = swing(n_have, n_want)
        tw = twist_about(tw, u1)
    qu = tw @ sw
    pose.turn(upper, qu)
    f_now = pose.D[lower] @ (R.rest_h[end] - R.rest_h[lower])
    pose.turn(lower, swing(f_now, f1))
    if end_delta is not None:
        # the forearm (shin) takes the end's twist about its axis; the end takes the rest
        need = end_delta @ pose.D[end].inverted()
        tw2 = twist_about(need, f1)
        if twist_share < 1.: tw2 = Quaternion().slerp(tw2, twist_share)
        pose.turn(lower, tw2)
        pose.carry(end, end_delta) if False else None
        cur = pose.D[end]
        pose.turn(end, end_delta @ cur.inverted())


def curl(pose, s, amount, thumb=None, spread=0.):
    """Close the fingers of hand s by `amount` (0 open .. 1 a fist round a grip)."""
    R = pose.rig
    palm, along, across = R.palm[s]
    for f, k in (('index', .9), ('middle', 1.), ('ring', 1.05), ('pinky', 1.1)):
        for j, base in ((1, 62), (2, 88), (3, 55)):
            b = f'{f}_0{j}_{s}'
            if not R.has(b): continue
            nxt = f'{f}_0{j + 1}_{s}' if j < 3 else None
            d_rest = ((R.rest_h[nxt] if nxt and R.has(nxt) else R.rest_h[b] + (R.rest_h[b] - R.rest_h[f'{f}_0{j - 1}_{s}'] if j > 1 else along * .02)) - R.rest_h[b]).normalized()
            axis_rest = d_rest.cross(palm).normalized()
            axis = pose.D[b] @ axis_rest
            pose.turn(b, Quaternion(axis, math.radians(base * amount * k)))
    th = amount if thumb is None else thumb
    for j, base in ((1, 18), (2, 30), (3, 40)):
        b = f'thumb_0{j}_{s}'
        if not R.has(b): continue
        nxt = f'thumb_0{j + 1}_{s}' if j < 3 else None
        d_rest = ((R.rest_h[nxt] if nxt and R.has(nxt) else R.rest_h[b] + (R.rest_h[b] - R.rest_h[f'thumb_0{j - 1}_{s}'])) - R.rest_h[b]).normalized()
        axis = pose.D[b] @ d_rest.cross(palm).normalized()
        pose.turn(b, Quaternion(axis, math.radians(base * th)))


# ------------------------------------------------------------------ a body spec at one time -> pose
def body_pose(rig, b):
    """b: dict of the body's state at one instant (see FighterMoves.guard)."""
    pose = Pose(rig); R = rig
    # Hips: shift, then yaw / pitch / roll about the pelvis joint.
    pose.root_shift = b['hips']
    q_hips = rot(Z, b['yaw']) @ rot(V((0, 1, 0)), b.get('pitch', 0.)) @ rot(V((1, 0, 0)), b.get('roll', 0.))
    pose.turn('pelvis', q_hips)
    # Legs first (the feet are planted where they are, wherever the hips go).
    for s in 'lr':
        ankle, fyaw, fpitch, toe = b['feet'][s]
        fq = rot(Z, fyaw) @ rot(V((0, 1, 0)), fpitch)
        H = pose.fk()
        fwd = q_hips @ V((1, 0, 0))
        knee_pole = (fwd + (V((0, .07 if s == 'l' else -.07, 0)))).normalized()
        knee_pole = rot(Z, fyaw * .5) @ knee_pole
        solve_limb(pose, f'thigh_{s}', f'calf_{s}', f'foot_{s}', ankle, knee_pole, end_delta=fq, twist_share=.0)
        if toe and R.has(f'ball_{s}'):
            lat = fq @ V((0, 1, 0))
            pose.turn(f'ball_{s}', Quaternion(lat, math.radians(toe)))
    # Spine: the chest's yaw / pitch / roll over the three spine bones, then the neck and head look.
    chest = b.get('chest', (0., 0., 0.))
    for bone, w in (('spine_01', .25), ('spine_02', .35), ('spine_03', .4)):
        q = rot(Z, chest[0] * w) @ rot(pose.D[bone] @ V((0, 1, 0)), chest[1] * w) @ rot(pose.D[bone] @ V((1, 0, 0)), chest[2] * w)
        pose.turn(bone, q)
    H = pose.fk()
    look = b.get('look')
    if look is not None:
        for bone, w in (('neck_01', .4), ('head', .6)):
            H = pose.fk()
            fwd = pose.D[bone] @ V((1, 0, 0))
            want = (look - H['head']).normalized()
            q = Quaternion().slerp(swing(fwd, want), w)
            pose.turn(bone, q)
    # Targets that depend on the spine's pose (a hand to the quiver on her back) are resolved now.
    for s in 'lr':
        arm = b['arms'][s]
        for kk in ('wrist', 'hand', 'pole'):
            if callable(arm.get(kk)): arm[kk] = arm[kk](pose)
    # Clavicles: raise and reach with the hands (from the wanted wrist positions).
    for s in 'lr':
        arm = b['arms'][s]
        H = pose.fk()
        wrist = arm['wrist'] if 'wrist' in arm else None
        if wrist is None: continue
        sh = H[f'upperarm_{s}']
        up = clamp((wrist.z - sh.z + .1) / .5) * 16. + arm.get('shrug', 0.)
        reach = clamp(((wrist - sh).dot(q_hips @ V((1, 0, 0)))) / .5) * 10.
        sgn = 1. if s == 'l' else -1.
        fwd = pose.D[f'clavicle_{s}'] @ V((1, 0, 0))
        pose.turn(f'clavicle_{s}', rot(fwd, -sgn * up) @ rot(Z, sgn * reach))
    # Arms.
    for s in 'lr':
        arm = b['arms'][s]
        solve_limb(pose, f'upperarm_{s}', f'lowerarm_{s}', f'hand_{s}', arm['wrist'], arm['pole'], end_delta=arm.get('hand'))
        if arm.get('fore') is not None:   # the forearm's wanted delta (the shield): set it, and the hand with it
            cur = pose.D[f'lowerarm_{s}']
            fix = twist_about(arm['fore'] @ cur.inverted(), pose.D[f'lowerarm_{s}'] @ (R.rest_h[f'hand_{s}'] - R.rest_h[f'lowerarm_{s}']))
            pose.turn(f'lowerarm_{s}', fix)
        curl(pose, s, arm.get('curl', .15), thumb=arm.get('thumb'))
    pose.fk()
    if b.get('post'): b['post'](pose); pose.fk()
    return pose


# ------------------------------------------------------------------ keying and export
def key(rig, poses, name):
    obj = rig.obj
    obj.animation_data_create()
    act = bpy.data.actions.new(name); obj.animation_data.action = act
    for p in obj.pose.bones: p.rotation_mode = 'QUATERNION'
    qo = rig.q_obj
    loc_bones = set().union(*[set(p.loc) for p in poses])
    scale_bones = set().union(*[set(p.scale) for p in poses])
    for i, pose in enumerate(poses):
        W = {b: qo.inverted() @ (pose.D[b] @ rig.rest_q[b]) for b in rig.order}   # armature-space rotations
        for p in obj.pose.bones:
            b = p.bone; n = b.name
            rest = b.matrix_local.to_quaternion()
            if b.parent:
                rel = b.parent.matrix_local.to_quaternion().inverted() @ rest
                basis = rel.inverted() @ W[b.parent.name].inverted() @ W[n]
            else:
                basis = rest.inverted() @ W[n]
            p.rotation_quaternion = basis
            p.keyframe_insert('rotation_quaternion', frame=i + 1)
            if n in loc_bones:
                d = pose.loc.get(n, V())
                pr = W[b.parent.name] @ b.parent.matrix_local.to_quaternion().inverted() @ rest
                p.location = pr.inverted() @ (qo.inverted() @ d)
                p.keyframe_insert('location', frame=i + 1)
            if n in scale_bones:
                k = pose.scale.get(n, 1.); p.scale = (k, k, k); p.keyframe_insert('scale', frame=i + 1)
            if n == 'pelvis':
                prest = b.parent.matrix_local.to_quaternion().inverted() @ rest if b.parent else rest
                # basis translation in the bone's rest frame: parent (Root) is unmoved
                p.location = (b.matrix_local.to_quaternion()).inverted() @ (qo.inverted() @ pose.root_shift)
                p.keyframe_insert('location', frame=i + 1)
    bpy.context.scene.frame_start, bpy.context.scene.frame_end = 1, len(poses)
    return act


def export(rig, clip):
    out = HEROES / f'Hero{rig.name}/Anim'; out.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    # The rig, the bare body (its skin gives the file its bind pose) and the small pieces on the prop bones
    # (each extra bone needs a skin in the file for Unreal to bind its track): the armour and gear would
    # add ~11 MB a clip to the repository for nothing the import uses.
    extra = {'weapon_r', 'shield_l', 'weapon_l', 'bowstring', 'arrow_r'}
    meshes = [c for c in rig.obj.children if c.type == 'MESH']
    body = [c for c in meshes if c.name == 'Human']
    for bone in extra:
        carriers = [c for c in meshes if bone in c.vertex_groups]
        if carriers: body.append(min(carriers, key=lambda c: len(c.data.vertices)))
    for o in [rig.obj] + (body or [c for c in rig.obj.children if c.type == 'MESH']): o.select_set(True)
    bpy.context.view_layer.objects.active = rig.obj
    path = out / f'AS_Hero{rig.name}_{clip}.fbx'
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'ARMATURE', 'MESH'}, **FBX_EXPORT)
    return path


def measure(rig, poses, contact_window=None):
    """The blade tip's world path (from the keyed rig, after FBX-space round trip: evaluated), its speed,
    and the frame of fastest travel inside contact_window (seconds)."""
    obj = rig.obj; scene = bpy.context.scene
    tips, grips = [], []
    tip_bone = 'weapon_tip' if 'weapon_tip' in obj.pose.bones else None
    for i in range(len(poses)):
        scene.frame_set(i + 1)
        if tip_bone:
            tips.append(obj.matrix_world @ obj.pose.bones[tip_bone].head)
            grips.append(obj.matrix_world @ obj.pose.bones['weapon_r'].head)
    if not tips: return {}
    speed = [0.] + [(tips[i] - tips[i - 1]).length * FPS for i in range(1, len(tips))]
    out = {'tip_speed_peak_m_s': round(max(speed), 2)}
    if contact_window:
        f0, f1 = int(contact_window[0] * FPS), int(contact_window[1] * FPS)
        z_want = contact_window[2] if len(contact_window) > 2 else None
        frames = range(f0, min(f1, len(speed) - 1) + 1)
        if z_want is None: k = max(frames, key=lambda j: speed[j])
        else:
            ahead = [j for j in frames if grips[j].lerp(tips[j], .62).x > .55] or list(frames)
            k = min(ahead, key=lambda j: abs(grips[j].lerp(tips[j], .62).z - z_want) + abs(grips[j].lerp(tips[j], .62).y))
        # the contact point: 62 % along the blade (the centre of percussion of an arming sword)
        p = grips[k].lerp(tips[k], .62)
        out.update(contact_s=round(k / FPS, 3), contact_frame=k, contact_point_cm=[round(c * 100, 1) for c in (p.x, -p.y, p.z)],
                   contact_tip_cm=[round(c * 100, 1) for c in (tips[k].x, -tips[k].y, tips[k].z)],
                   contact_speed_m_s=round(speed[k], 2))
    out['tip_cm'] = [[round(c * 100, 1) for c in (t.x, -t.y, t.z)] for t in tips[::2]]
    return out


def review(rig, clip, n, every=3):
    """A contact sheet of the skinned hero: side and front rows, every `every` frames."""
    import numpy as np, os
    s = bpy.context.scene
    s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.light = 'STUDIO'; s.display.shading.color_type = 'MATERIAL'
    s.render.resolution_x, s.render.resolution_y = 260, 340
    cam = bpy.data.objects.get('RevCam') or bpy.data.objects.new('RevCam', bpy.data.cameras.new('RevCam'))
    if cam.name not in s.collection.objects: s.collection.objects.link(cam)
    s.camera = cam; cam.data.type = 'ORTHO'; cam.data.ortho_scale = 3.2
    if 'RevGround' not in bpy.data.objects:
        bpy.ops.mesh.primitive_plane_add(size=8); bpy.context.object.name = 'RevGround'
    views = {'side': ((0.4, -4., 1.1), (90, 0, 0)), 'front': ((4., 0., 1.1), (90, 0, 90)), 'three': ((3.2, -2.6, 1.6), (78, 0, 51))}
    frames = list(range(1, n + 1, every))
    rows = []
    for tag, (loc, r) in views.items():
        cam.location = loc; cam.rotation_euler = tuple(math.radians(a) for a in r)
        tiles = []
        for f in frames:
            s.frame_set(f); s.render.filepath = os.path.join(REVIEW, '_t.png'); bpy.ops.render.render(write_still=True)
            im = bpy.data.images.load(os.path.join(REVIEW, '_t.png')); tiles.append(np.array(im.pixels[:]).reshape(340, 260, 4)); bpy.data.images.remove(im)
        rows.append(tiles)
    W = 10; blank = np.zeros_like(rows[0][0]); lines = []
    for i in range(0, len(frames), W):
        for tiles in rows:
            seg = tiles[i:i + W]; seg += [blank] * (W - len(seg)); lines.append(np.concatenate(seg, axis=1))
    sheet = np.concatenate(lines[::-1], axis=0); h, w = sheet.shape[:2]
    img = bpy.data.images.new(clip, w, h); img.pixels = sheet.ravel(); img.filepath_raw = os.path.join(REVIEW, f'{rig.name}_{clip}.png'); img.file_format = 'PNG'; img.save()


def review_keys(rig, clip, frames, tag='keys', view=((3.0, -2.2, 1.45), (82, 0, 54)), size=(520, 700), scale=2.6):
    """Chosen frames, larger, in one strip (frame numbers 1-based)."""
    import numpy as np, os
    s = bpy.context.scene
    s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.light = 'STUDIO'; s.display.shading.color_type = 'MATERIAL'
    s.render.resolution_x, s.render.resolution_y = size
    cam = bpy.data.objects.get('RevCam') or bpy.data.objects.new('RevCam', bpy.data.cameras.new('RevCam'))
    if cam.name not in s.collection.objects: s.collection.objects.link(cam)
    s.camera = cam; cam.data.type = 'ORTHO'; cam.data.ortho_scale = scale
    cam.location = view[0]; cam.rotation_euler = tuple(math.radians(a) for a in view[1])
    tiles = []
    for f in frames:
        s.frame_set(f); s.render.filepath = os.path.join(REVIEW, '_t.png'); bpy.ops.render.render(write_still=True)
        im = bpy.data.images.load(os.path.join(REVIEW, '_t.png')); tiles.append(np.array(im.pixels[:]).reshape(size[1], size[0], 4)); bpy.data.images.remove(im)
    sheet = np.concatenate(tiles, axis=1); h, w = sheet.shape[:2]
    img = bpy.data.images.new(clip + tag, w, h); img.pixels = sheet.ravel(); img.filepath_raw = os.path.join(REVIEW, f'{rig.name}_{clip}_{tag}.png'); img.file_format = 'PNG'; img.save()


# ------------------------------------------------------------------ the fighter
class Fighter:
    """Sword and shield. The frame: facing +X (toward his foe), his left +Y, feet at Z 0."""
    def __init__(self, rig):
        self.R = rig; H = rig.rest_h
        self.ank = rig.ankle_h
        self.target = V((2.2, 0., 1.55))       # where he looks: a foe's head two metres off
        # the sword's rest frame (weapon_r: grip -> tip), and where the grip sits on the hand
        self.sw_rest = frame_from(H['weapon_tip'] - H['weapon_r'], self._edge_rest())
        self.grip_off = H['weapon_r'] - H['hand_r']
        self.sh_rest_c = H['shield_l']

    def _edge_rest(self):
        # build_temple_heroes.make_sword: the blade's flat is the palm's way, so the edge is along x flat
        palm, along, across = self.R.palm['r']
        d = (self.R.rest_h['weapon_tip'] - self.R.rest_h['weapon_r']).normalized()
        return d.cross(palm)

    # -- the sword: grip position, blade direction, edge direction -> right hand wrist and delta
    def sword_hand(self, grip, blade, edge):
        q = frame_from(blade, edge) @ self.sw_rest.inverted()     # the sword's (and so the hand's) delta
        wrist = grip - q @ self.grip_off
        return wrist, q

    # -- the shield: centre, its face's way, its top's way -> left forearm delta and wrist
    def shield_arm(self, pose_h, centre, face, top):
        R = self.R; H = R.rest_h
        # the shield's rest frame: its face out = the bone's roll axis, as built (out_dir), its top toward the elbow
        fa = (H['hand_l'] - H['lowerarm_l']).normalized()
        out_rest = (H['shield_l'] - (H['lowerarm_l'] + H['hand_l']) * .5).normalized()
        q = frame_from(face, top) @ frame_from(out_rest, -fa).inverted()
        elbow = centre - q @ (H['shield_l'] - H['lowerarm_l'])
        wrist = elbow + q @ (H['hand_l'] - H['lowerarm_l'])
        return wrist, q, elbow

    def stance(self):
        """The guard's feet: left foot leading, the back foot turned out."""
        a = self.ank
        return {'l': V((.2, .13, a)), 'r': V((-.24, -.16, a))}

    def body(self, t, hips, yaw, chest, feet, grip, blade, edge, shield_c, shield_face, shield_top, look=None, curl_l=.85, pitch=6., roll=0., rpole=None):
        wrist_r, q_r = self.sword_hand(grip, blade, edge)
        # a first pose for the shoulder positions
        b = {'hips': hips, 'yaw': yaw, 'pitch': pitch, 'roll': roll, 'chest': chest, 'feet': feet, 'look': look or self.target, 'arms': {}}
        wrist_l, q_l, elbow_l = self.shield_arm(None, shield_c, shield_face, shield_top)
        b['arms']['r'] = {'wrist': wrist_r, 'pole': rpole if rpole is not None else V((-.3, -1., -.6)), 'hand': q_r, 'curl': 1., 'thumb': .9}
        b['arms']['l'] = {'wrist': wrist_l, 'pole': V((-.2, .6, -1.)), 'hand': q_l, 'curl': curl_l, 'thumb': .6}
        pose = body_pose(self.R, b)
        # the left elbow: re-aim the pole at where the shield needs it
        H = pose.H
        pole = elbow_l - (H['upperarm_l'] + wrist_l) * .5
        b['arms']['l']['pole'] = pole
        return body_pose(self.R, b)


def fighter_guard_state(F, t, breath=True):
    """The guard, as values (used by every clip as its start and end)."""
    a = F.ank
    br = math.sin(t * 2 * math.pi / 3.2) if breath else 0.
    sway = math.sin(t * 2 * math.pi / 6.4 + .7) if breath else 0.
    return dict(
        hips=V((-.02 + .006 * sway, .01, -.07 + .006 * br)), yaw=-27. + 1.5 * sway, chest=(-6. + 1. * sway, 8. + .8 * br, 0.),
        feet={'l': (V((.21, .12, a)), 8., 0., 0.), 'r': (V((-.25, -.14, a)), -42., 0., 0.)},
        grip=V((.08, -.33, 1.43 + .006 * br)), blade=V((.32, .02, .95)).normalized(), edge=V((1., 0., -.3)),
        shield_c=V((.46, .07, 1.25 + .008 * br)), shield_face=V((1., -.25, .05)).normalized(), shield_top=V((.05, .1, 1.)),
    )


def fighter_clip(F, keys, dur, extra=None):
    """keys: {channel: [(t, value)]} -> poses for every frame. Channels: hips, yaw, chest(3 floats as Vector),
    lfoot, rfoot (Vector ankle), lfoot_yaw, rfoot_yaw, lfoot_pitch..., grip, blade, shield_c, shield_face, shield_top."""
    tr = {k: Track(v) for k, v in keys.items()}
    n = int(round(dur * FPS)) + 1
    poses, prev_tip, edge = [], None, None
    blades, grips = [], []
    for i in range(n):
        t = i / FPS
        blades.append(tr['blade'](t).normalized()); grips.append(tr['grip'](t))
    for i in range(n):
        t = i / FPS
        g, d = grips[i], blades[i]
        # the edge leads: the blade's point's velocity across the blade (held where it is slow)
        j0, j1 = max(0, i - 1), min(n - 1, i + 1)
        tip0 = grips[j0] + blades[j0] * .6; tip1 = grips[j1] + blades[j1] * .6
        vel = (tip1 - tip0) * (FPS / max(1, j1 - j0))
        across = vel - d * vel.dot(d)
        want = tr['edge'](t) if 'edge' in tr else None
        if across.length > .9:
            e_new = across.normalized()
            w = clamp((across.length - .9) / 1.5)
            base = edge if edge is not None else (want if want is not None else e_new)
            edge = (base.lerp(e_new, w) - d * base.lerp(e_new, w).dot(d)).normalized()
        elif want is not None:
            edge = (edge.lerp(want, .25) if edge is not None else want)
        else:
            edge = edge if edge is not None else d.orthogonal()
        ch = tr['chest'](t)
        feet = {s: (tr[f'{s}foot'](t), tr[f'{s}foot_yaw'](t), tr.get(f'{s}foot_pitch', lambda t: 0.)(t), tr.get(f'{s}toe', lambda t: 0.)(t)) for s in 'lr'}
        pose = F.body(t, tr['hips'](t), tr['yaw'](t), (ch.x, ch.y, ch.z), feet, g, d, edge,
                      tr['shield_c'](t), tr['shield_face'](t).normalized(), tr['shield_top'](t).normalized(),
                      look=tr['look'](t) if 'look' in tr else None, pitch=tr['pitch'](t) if 'pitch' in tr else 6.,
                      roll=tr['roll'](t) if 'roll' in tr else 0., rpole=tr['rpole'](t) if 'rpole' in tr else None)
        poses.append(pose)
    return poses


def guard_keys(F, times):
    """The guard's values as keys at each of `times` (holds)."""
    g = fighter_guard_state(F, 0., breath=False)
    k = {}
    for t in times:
        for ch, v in (('hips', g['hips']), ('yaw', g['yaw']), ('chest', V(g['chest'])), ('grip', g['grip']), ('blade', g['blade']),
                      ('shield_c', g['shield_c']), ('shield_face', g['shield_face']), ('shield_top', g['shield_top'])):
            k.setdefault(ch, []).append((t, v.copy() if isinstance(v, Vector) else v))
        for s in 'lr':
            a, yaw, pitch, toe = g['feet'][s]
            k.setdefault(f'{s}foot', []).append((t, a.copy())); k.setdefault(f'{s}foot_yaw', []).append((t, yaw))
    return k


def add(k, ch, *pts):
    k.setdefault(ch, [])
    k[ch] = sorted(k[ch] + list(pts), key=lambda p: p[0])


def F_Guard(F):
    """The guard, breathing (a loop of 6.4 s: the breath's 3.2 s twice, the sway's once)."""
    n = int(6.4 * FPS)
    poses = []
    for i in range(n):
        t = i / FPS
        g = fighter_guard_state(F, t)
        feet = {s: g['feet'][s] for s in 'lr'}
        poses.append(F.body(t, g['hips'], g['yaw'], g['chest'], feet, g['grip'], g['blade'], g['edge'], g['shield_c'], g['shield_face'], g['shield_top']))
    return poses, {'loops': True}


def F_CutDown(F):
    """The forehand descending diagonal: the wind-up over his right shoulder, a step in with the
    left foot, hips, chest, arm, the blade whipping through high right to low left, the shield
    lifting out of its way; then back to guard."""
    a = F.ank
    k = guard_keys(F, (0., 1.45))
    # wind-up: weight back, coil, the sword back over the shoulder
    add(k, 'hips', (.26, V((-.08, .0, -.09))), (.42, V((.08, .02, -.12))), (.52, V((.17, .04, -.15))), (.7, V((.15, .04, -.13))), (1.05, V((.02, .02, -.08))))
    add(k, 'yaw', (.26, -40.), (.36, -30.), (.46, -2.), (.56, 12.), (.75, 8.), (1.1, -22.))
    add(k, 'chest', (.26, V((-14., 2., 0.))), (.36, V((-14., 5., 2.))), (.46, V((12., 11., -4.))), (.56, V((26., 15., -6.))), (.8, V((14., 11., -2.))), (1.15, V((-6., 8., 0.))))
    add(k, 'lfoot', (.28, V((.21, .12, a))), (.35, V((.31, .12, a + .07))), (.41, V((.42, .11, a))), (.9, V((.42, .11, a))), (1.0, V((.34, .12, a + .05))), (1.1, V((.21, .12, a))))
    add(k, 'lfoot_yaw', (.3, 8.), (.42, 4.), (1.1, 8.))
    add(k, 'rfoot_yaw', (.3, -42.), (.5, -30.), (1.1, -42.))
    # 2026-10-23 in-game: it must come down through a crouched skeleton's collarbones (neck 1.28 m), so its
    # contact is at about 1.3 m (it was 1.56, over their heads).
    add(k, 'grip', (.26, V((-.06, -.3, 1.72))), (.34, V((.04, -.34, 1.8))), (.40, V((.32, -.2, 1.6))), (.45, V((.53, .03, 1.3))), (.50, V((.55, .17, 1.03))), (.6, V((.38, .24, .92))), (.75, V((.34, .2, .98))), (1.1, V((.1, -.3, 1.38))))
    add(k, 'blade', (.26, V((-.8, .05, .6))), (.34, V((-.35, -.5, .8))), (.40, V((.6, -.42, .62))), (.45, V((.86, .14, -.36))), (.50, V((.48, .6, -.62))), (.6, V((-.2, .7, -.7))), (.75, V((-.1, .55, -.8))), (1.1, V((.32, .02, .95))))
    add(k, 'rpole', (0., V((-.3, -1., -.6))), (.26, V((-.4, -1., .5))), (.38, V((-.2, -1., .4))), (.46, V((.1, -1., -.3))), (.6, V((-.2, -.6, -1.))), (1.1, V((-.3, -1., -.6))), (1.45, V((-.3, -1., -.6))))
    add(k, 'shield_c', (.26, V((.4, .1, 1.22))), (.44, V((.42, .26, 1.2))), (.56, V((.34, .36, 1.16))), (.9, V((.42, .18, 1.22))))
    add(k, 'shield_face', (.26, V((1., -.15, .05))), (.44, V((.9, .5, 0.))), (.56, V((.6, .8, 0.))), (.9, V((1., 0., .05))))
    return fighter_clip(F, k, 1.45), {'contact': (.38, .52, 1.3)}


def F_CutFlat(F):
    """The forehand horizontal at neck height (a skeleton's head off its spine): the sword taken back
    flat behind his right shoulder, hips and chest unwinding through a step, the blade sweeping right
    to left level, his arm crossing his body after it; the shield swung out left for balance."""
    a = F.ank
    k = guard_keys(F, (0., 1.4))
    add(k, 'hips', (.24, V((-.07, 0., -.09))), (.40, V((.08, .02, -.12))), (.5, V((.15, .03, -.14))), (.75, V((.13, .03, -.12))), (1.05, V((.02, .01, -.08))))
    add(k, 'yaw', (.24, -46.), (.33, -34.), (.43, 2.), (.53, 24.), (.8, 16.), (1.1, -22.))
    add(k, 'chest', (.24, V((-18., 4., 0.))), (.33, V((-18., 4., 0.))), (.43, V((12., 7., 0.))), (.53, V((30., 9., -2.))), (.8, V((18., 8., 0.))), (1.12, V((-6., 8., 0.))))
    add(k, 'lfoot', (.26, V((.21, .12, a))), (.33, V((.31, .12, a + .07))), (.40, V((.42, .11, a))), (.88, V((.42, .11, a))), (.98, V((.34, .12, a + .05))), (1.08, V((.21, .12, a))))
    add(k, 'lfoot_yaw', (.3, 8.), (.4, 2.), (1.08, 8.))
    add(k, 'rfoot_yaw', (.3, -42.), (.52, -24.), (1.1, -42.))
    # at a skeleton's throat (1.76 m tall: about 1.53 m; 2026-10-23 in-game, 1.42 took its collarbones)
    add(k, 'grip', (.24, V((-.06, -.42, 1.62))), (.32, V((.03, -.47, 1.61))), (.38, V((.28, -.4, 1.59))), (.42, V((.47, -.15, 1.57))), (.47, V((.46, .14, 1.55))), (.56, V((.26, .3, 1.48))), (.72, V((.24, .26, 1.4))), (1.05, V((.08, -.3, 1.42))))
    add(k, 'blade', (.24, V((-.7, -.55, .45))), (.32, V((-.6, -.78, .12))), (.38, V((.15, -.99, 0.))), (.42, V((.97, .2, -.04))), (.47, V((.42, .9, -.08))), (.56, V((-.45, .88, -.15))), (.72, V((-.5, .8, -.2))), (1.05, V((.32, .02, .95))))
    add(k, 'rpole', (0., V((-.3, -1., -.6))), (.24, V((-.5, -1., .1))), (.38, V((-.3, -1., -.2))), (.44, V((-.1, -.5, -1.))), (.56, V((.2, .2, -1.))), (.75, V((.2, .1, -1.))), (1.05, V((-.3, -1., -.6))), (1.4, V((-.3, -1., -.6))))
    add(k, 'shield_c', (.24, V((.4, .1, 1.22))), (.44, V((.26, .38, 1.22))), (.56, V((.16, .42, 1.18))), (.9, V((.4, .18, 1.22))))
    add(k, 'shield_face', (.24, V((1., -.15, .05))), (.44, V((.35, .94, 0.))), (.56, V((.1, 1., 0.))), (.9, V((1., 0., .05))))
    return fighter_clip(F, k, 1.4), {'contact': (.34, .5, 1.55)}


def F_CutRise(F):
    """The rising cut, low right to high left, for a foe above him (a beholder's eye stalks): he sinks,
    the sword low and back by his right hip, then drives up off the back foot onto the front one,
    arching up after the blade, the arm reaching high; the shield tucked down out of the way."""
    a = F.ank
    k = guard_keys(F, (0., 1.5))
    add(k, 'hips', (.22, V((-.06, 0., -.16))), (.32, V((-.02, 0., -.15))), (.42, V((.12, .03, -.04))), (.55, V((.16, .04, -.03))), (.85, V((.1, .03, -.06))), (1.15, V((.0, .01, -.07))))
    add(k, 'yaw', (.22, -44.), (.31, -34.), (.41, 4.), (.52, 20.), (.85, 12.), (1.2, -22.))
    add(k, 'chest', (.22, V((-14., 16., 0.))), (.31, V((-12., 12., 0.))), (.41, V((10., -6., -4.))), (.52, V((22., -14., -6.))), (.85, V((12., -2., -2.))), (1.2, V((-6., 8., 0.))))
    add(k, 'pitch', (0., 6.), (.22, 12.), (.41, -2.), (.6, -4.), (1.2, 6.), (1.5, 6.))
    add(k, 'look', (0., F.target), (.25, V((2.2, -.1, 2.1))), (.45, V((2.0, .1, 2.7))), (.9, V((2.1, .05, 2.4))), (1.3, F.target), (1.5, F.target))
    add(k, 'lfoot', (.24, V((.21, .12, a))), (.31, V((.31, .12, a + .07))), (.38, V((.41, .11, a))), (.95, V((.41, .11, a))), (1.05, V((.33, .12, a + .05))), (1.15, V((.21, .12, a))))
    add(k, 'lfoot_yaw', (.3, 8.), (.4, 2.), (1.15, 8.))
    add(k, 'rfoot_yaw', (.3, -42.), (.5, -26.), (1.15, -42.))
    add(k, 'rtoe', (0., 0.), (.36, 0.), (.46, 28.), (.7, 20.), (.95, 0.), (1.5, 0.))
    add(k, 'rfoot_pitch', (0., 0.), (.36, 0.), (.46, -20.), (.7, -14.), (.95, 0.), (1.5, 0.))
    add(k, 'grip', (.22, V((-.08, -.4, 1.0))), (.30, V((.02, -.44, 1.02))), (.36, V((.28, -.34, 1.36))), (.41, V((.46, -.06, 1.74))), (.47, V((.38, .18, 1.96))), (.58, V((.18, .3, 1.96))), (.8, V((.2, .26, 1.86))), (1.15, V((.08, -.3, 1.42))))
    add(k, 'blade', (.22, V((-.6, -.4, -.7))), (.30, V((-.15, -.62, -.77))), (.36, V((.75, -.55, -.05))), (.41, V((.6, .28, .75))), (.47, V((.0, .55, .84))), (.58, V((-.55, .45, .7))), (.8, V((-.45, .4, .78))), (1.15, V((.32, .02, .95))))
    add(k, 'rpole', (0., V((-.3, -1., -.6))), (.22, V((-.4, -1., -.3))), (.36, V((-.2, -1., -.6))), (.42, V((.1, -1., -.5))), (.55, V((.4, -.3, -1.))), (.85, V((.3, -.3, -1.))), (1.15, V((-.3, -1., -.6))), (1.5, V((-.3, -1., -.6))))
    add(k, 'shield_c', (.22, V((.36, .14, 1.12))), (.42, V((.28, .3, 1.06))), (.6, V((.24, .34, 1.08))), (1.0, V((.42, .14, 1.2))))
    add(k, 'shield_face', (.22, V((1., .1, .05))), (.42, V((.5, .86, -.1))), (.6, V((.4, .9, -.1))), (1.0, V((1., -.1, .05))))
    return fighter_clip(F, k, 1.5), {'contact': (.32, .48, 2.1)}


def F_Overhead(F):
    """The heavy blow: the sword swung right back over his head until it hangs behind him, a long
    step, his whole weight dropping through the blade straight down in front, a deep crouch."""
    a = F.ank
    k = guard_keys(F, (0., 1.75))
    add(k, 'hips', (.34, V((-.1, 0., -.05))), (.44, V((-.02, .01, -.06))), (.52, V((.16, .03, -.16))), (.66, V((.2, .03, -.22))), (.95, V((.16, .03, -.17))), (1.35, V((.0, .01, -.07))))
    add(k, 'yaw', (.34, -30.), (.44, -24.), (.52, -8.), (.66, -2.), (1.0, -8.), (1.4, -27.))
    add(k, 'chest', (.34, V((-8., -10., 0.))), (.44, V((-6., -8., 0.))), (.52, V((2., 18., 0.))), (.66, V((4., 30., 0.))), (1.0, V((0., 20., 0.))), (1.4, V((-6., 8., 0.))))
    add(k, 'lfoot', (.36, V((.21, .12, a))), (.43, V((.34, .12, a + .08))), (.5, V((.5, .11, a))), (1.1, V((.5, .11, a))), (1.2, V((.36, .12, a + .06))), (1.32, V((.21, .12, a))))
    add(k, 'lfoot_yaw', (.36, 8.), (.5, 4.), (1.32, 8.))
    add(k, 'rfoot_yaw', (.36, -42.), (.6, -34.), (1.32, -42.))
    add(k, 'grip', (.20, V((-.04, -.26, 1.76))), (.34, V((-.1, -.22, 1.86))), (.43, V((.0, -.24, 1.96))), (.475, V((.3, -.18, 1.8))), (.51, V((.5, -.12, 1.45))), (.56, V((.47, -.06, 1.1))), (.66, V((.36, .0, .86))), (.95, V((.36, .0, .9))), (1.32, V((.08, -.3, 1.42))))
    add(k, 'blade', (.20, V((-.6, .0, .8))), (.34, V((-.88, .1, -.3))), (.43, V((-.3, -.05, .95))), (.475, V((.85, -.03, .5))), (.51, V((.95, 0., -.3))), (.56, V((.42, .05, -.9))), (.66, V((.12, .06, -.99))), (.95, V((.15, .05, -.98))), (1.32, V((.32, .02, .95))))
    add(k, 'rpole', (0., V((-.3, -1., -.6))), (.34, V((-.2, -.8, 1.))), (.45, V((-.1, -.9, .6))), (.52, V((-.1, -1., -.4))), (.66, V((-.2, -.6, -1.))), (1.32, V((-.3, -1., -.6))), (1.75, V((-.3, -1., -.6))))
    add(k, 'shield_c', (.34, V((.36, .16, 1.24))), (.52, V((.24, .34, 1.08))), (.7, V((.22, .36, 1.02))), (1.2, V((.42, .14, 1.2))))
    add(k, 'shield_face', (.34, V((1., .1, .05))), (.52, V((.45, .9, -.1))), (.7, V((.35, .93, -.1))), (1.2, V((1., -.1, .05))))
    return fighter_clip(F, k, 1.75), {'contact': (.44, .6, 1.55)}


def F_Bash(F):
    """The shield driven into a foe: drawn in to his chest, then punched out with a step and his
    shoulder behind it."""
    a = F.ank
    k = guard_keys(F, (0., 1.1))
    add(k, 'hips', (.18, V((-.07, .0, -.1))), (.3, V((.12, .03, -.12))), (.45, V((.1, .03, -.11))), (.8, V((.0, .01, -.07))))
    add(k, 'yaw', (.18, -40.), (.3, -20.), (.5, -24.), (.85, -27.))
    add(k, 'chest', (.18, V((-10., 6., 0.))), (.3, V((-14., 14., 0.))), (.55, V((-8., 11., 0.))), (.85, V((-6., 8., 0.))))
    add(k, 'lfoot', (.16, V((.21, .12, a))), (.22, V((.3, .12, a + .06))), (.29, V((.38, .11, a))), (.6, V((.38, .11, a))), (.7, V((.3, .12, a + .04))), (.8, V((.21, .12, a))))
    add(k, 'shield_c', (.18, V((.3, .08, 1.24))), (.3, V((.76, .02, 1.26))), (.4, V((.7, .03, 1.26))), (.75, V((.46, .07, 1.25))))
    add(k, 'shield_face', (.18, V((1., -.3, .05))), (.3, V((1., 0., .02))), (.75, V((1., -.25, .05))))
    add(k, 'grip', (.18, V((.0, -.34, 1.5))), (.3, V((-.04, -.36, 1.5))), (.8, V((.08, -.33, 1.43))))
    return fighter_clip(F, k, 1.1), {'contact_fixed': .3}


def strip(k, after):
    for ch in list(k): k[ch] = [kv for kv in k[ch] if kv[0] >= after]


def hit_keys(F, k, dur, push, twist, flail=0., shield_drop=0.):
    """A blow taken from in front. push: metres the hips are driven back at the peak, twist: degrees
    the chest is turned by it (+ toward his left). Three springs, each normalised to peak 1: the hips
    go first, the chest whips after them, the head snaps last, and all settle back."""
    fast = Spring(k=150., c=13.).run([(0.02, 1.)], dur, norm=True)
    slow = Spring(k=60., c=9.).run([(0.03, 1.)], dur, norm=True)
    head = Spring(k=260., c=14.).run([(0.05, 1.)], dur, norm=True)
    g = fighter_guard_state(F, 0., breath=False)
    n = int(dur * FPS) + 1
    for i in range(0, n, 2):
        t = i / FPS
        x, y, h = fast(t), slow(t), head(t)
        add(k, 'hips', (t, g['hips'] + V((-push * y, 0., -.25 * abs(push * y)))))
        add(k, 'chest', (t, V((g['chest'][0] + twist * x, g['chest'][1] - 180. * push * x, 0.))))
        add(k, 'yaw', (t, g['yaw'] + twist * .4 * y))
        add(k, 'pitch', (t, 6. - 120. * push * x - 90. * push * h))
        add(k, 'look', (t, F.target + V((0., twist * .01 * h, -6. * push * h))))
        add(k, 'grip', (t, g['grip'] + V((-1.2 * push * x, -flail * x, flail * .5 * x - .3 * push * y))))
        add(k, 'shield_c', (t, g['shield_c'] + V((-1.4 * push * x, .4 * push * y + shield_drop * .3 * x, -shield_drop * x))))


def F_HitLight(F):
    """Struck on the body: the hips knocked back a few centimetres, the chest rocked back and turned
    by the blow, the head snapping after it, the guard dropping for a moment; then square again."""
    k = guard_keys(F, (1.0,))
    strip(k, .99)
    hit_keys(F, k, .9, push=.05, twist=-10., flail=.05, shield_drop=.06)
    for ch, kv in guard_keys(F, (0.,)).items():
        if ch not in ('hips', 'chest', 'yaw', 'pitch', 'look', 'grip', 'shield_c'): add(k, ch, *kv)
    return fighter_clip(F, k, 1.0), {'hit': 0.}


def F_HitHeavy(F):
    """Knocked back: he gives two steps back, the sword arm flung out for balance and the shield
    swinging wide, the head thrown back; then he plants, gathers, and comes back into guard."""
    a = F.ank
    k = {}
    hit_keys(F, k, 1.9, push=.12, twist=-18., flail=.25, shield_drop=.25)
    g = guard_keys(F, (0., 1.9))
    for ch in ('shield_face', 'shield_top', 'lfoot_yaw', 'rfoot_yaw'): k[ch] = g[ch]
    add(k, 'rfoot', (0., V((-.25, -.14, a))), (.1, V((-.25, -.14, a))), (.2, V((-.4, -.17, a + .07))), (.3, V((-.55, -.18, a))), (1.1, V((-.55, -.18, a))), (1.25, V((-.42, -.16, a + .05))), (1.4, V((-.25, -.14, a))))
    add(k, 'lfoot', (0., V((.21, .12, a))), (.3, V((.21, .12, a))), (.42, V((.05, .13, a + .06))), (.52, V((-.08, .13, a))), (1.3, V((-.08, .13, a))), (1.45, V((.08, .13, a + .05))), (1.6, V((.21, .12, a))))
    k['hips'] = [(t, v + V((-.3 * smooth((t - .05) / .5) + .3 * smooth((t - 1.1) / .5), 0., 0.))) for t, v in k['hips']]
    add(k, 'blade', (0., V((.32, .02, .95))), (.15, V((.5, -.6, .6))), (.6, V((.4, -.5, .75))), (1.3, V((.32, .02, .95))), (1.9, V((.32, .02, .95))))
    return fighter_clip(F, k, 1.9), {'hit': 0.}


def F_BlockHit(F):
    """A blow taken on the shield: it is driven back at his chest and tipped, he rocks back over his
    back foot, his front foot sliding a little, and pushes it out again."""
    a = F.ank
    k = guard_keys(F, (.9,))
    strip(k, .89)
    sp = Spring(k=170., c=12.).run([(0.0, 1.)], .9)
    sl = Spring(k=70., c=10.).run([(0.02, 1.)], .9)
    g = fighter_guard_state(F, 0., breath=False)
    for i in range(0, 28, 2):
        t = i / FPS; x = sp(t) * 9.; y = sl(t) * 9.
        add(k, 'shield_c', (t, g['shield_c'] + V((-.14 * x, -.02 * x, .03 * x))))
        add(k, 'shield_face', (t, (g['shield_face'] + V((0., 0., .35 * x))).normalized()))
        add(k, 'hips', (t, g['hips'] + V((-.06 * y, 0., -.02 * y))))
        add(k, 'chest', (t, V((g['chest'][0] - 4. * x, g['chest'][1] - 10. * x, 0.))))
        add(k, 'pitch', (t, 6. - 6. * y))
        add(k, 'grip', (t, g['grip'] + V((-.06 * x, -.03 * x, .02 * x))))
    add(k, 'lfoot', (0., V((.21, .12, a))), (.08, V((.21, .12, a))), (.18, V((.17, .12, a))), (.6, V((.17, .12, a))), (.75, V((.21, .12, a + .02))), (.9, V((.21, .12, a))))
    for ch, kv in guard_keys(F, (0.,)).items():
        if ch not in k: add(k, ch, *kv)
    return fighter_clip(F, k, .9), {'hit': 0.}


def F_Step(F, dist, dur=.8):
    """A step in guard (dist + forward, - back): the leading foot first, the other after it. In place:
    the hips' travel comes out of the clip (root_x, per frame), and the game moves him by it."""
    a = F.ank
    k = guard_keys(F, (0., dur))
    g = fighter_guard_state(F, 0., breath=False)
    lead, trail = ('l', 'r') if dist > 0 else ('r', 'l')
    f = {s: g['feet'][s][0] for s in 'lr'}
    d = V((dist, 0., 0.))
    travel = Track([(0., 0.), (.05, 0.), (.4, .6), (.66, 1.), (dur, 1.)])
    k[f'{lead}foot'] = [(0., f[lead]), (.08, f[lead]), (.2, f[lead] + d * .5 + V((0, 0, .06))), (.32, f[lead] + d), (dur, f[lead] + d)]
    k[f'{trail}foot'] = [(0., f[trail]), (.34, f[trail]), (.46, f[trail] + d * .5 + V((0, 0, .05))), (.6, f[trail] + d), (dur, f[trail] + d)]
    n = int(round(dur * FPS)) + 1
    root = [dist * travel(i / FPS) for i in range(n)]
    # every foot key moved back by the travel at its time, and the hips kept at the guard's (plus a dip)
    for s in 'lr':
        k[f'{s}foot'] = [(t, v - V((dist * travel(t), 0., 0.))) for t, v in k[f'{s}foot']]
        # dense keys so the feet slide back exactly with the travel while planted
        dense = []
        foot = Track(k[f'{s}foot'] if False else [(t, v + V((dist * travel(t), 0., 0.))) for t, v in k[f'{s}foot']])
        for i in range(0, n):
            t = i / FPS
            dense.append((t, foot(t) - V((dist * travel(t), 0., 0.))))
        k[f'{s}foot'] = dense
    k['hips'] = [(0., g['hips']), (.4, g['hips'] + V((0, 0, -.015))), (dur, g['hips'])]
    return fighter_clip(F, k, dur), {'root_x': [round(r * 100, 2) for r in root]}


FIGHTER_CLIPS = {'Guard': F_Guard, 'CutDown': F_CutDown, 'CutFlat': F_CutFlat, 'CutRise': F_CutRise, 'Overhead': F_Overhead,
                 'Bash': F_Bash, 'BlockHit': F_BlockHit, 'HitLight': F_HitLight, 'HitHeavy': F_HitHeavy,
                 'Advance': lambda F: F_Step(F, .45), 'Retreat': lambda F: F_Step(F, -.4)}


# ------------------------------------------------------------------ hands on things (wizard, ranger)
class Hero:
    """Shared by the wizard and the ranger: hands placed from what they hold or how they open."""
    def __init__(self, rig):
        self.R = rig; H = rig.rest_h
        self.ank = rig.ankle_h
        self.grip_rest = {}
        for s in 'lr':
            palm, along, across = rig.palm[s]
            knuckles = H[f'middle_01_{s}']
            self.grip_rest[s] = H[f'hand_{s}'] + along * (knuckles - H[f'hand_{s}']).length * .72 + palm * .024

    def hand_q(self, s, along, palm):
        """The hand's delta for fingers pointing `along` with the palm facing `palm`."""
        p0, a0, _ = self.R.palm[s]
        return frame_from(along, palm) @ frame_from(a0, p0).inverted()

    def on_shaft(self, s, point, axis, palm):
        """A fist round a shaft at `point` along `axis` (the thumb's side toward +axis), the palm toward
        the shaft from `palm`'s side: the wrist and the hand's delta."""
        p0, a0, c0 = self.R.palm[s]
        q = frame_from(axis, palm) @ frame_from(c0, p0).inverted()
        wrist = point - q @ (self.grip_rest[s] - self.R.rest_h[f'hand_{s}'])
        return wrist, q

    def weapon(self, bone, tip, grip, axis, ref, ref_rest):
        """Hand delta and wrist for a prop on `bone` whose grip must be at `grip`, its axis along `axis`
        (toward `tip`'s rest), `ref` playing the part `ref_rest` did at rest."""
        R = self.R; H = R.rest_h
        hand = R.parent[bone]
        q = frame_from(axis, ref) @ frame_from(H[tip] - H[bone], ref_rest).inverted()
        wrist = grip - q @ (H[bone] - H[hand])
        return wrist, q


def hero_clip(H, tracks, dur, state):
    """Evaluate tracks every frame and pose the body state(t, values) returns."""
    tr = {k: Track(v) for k, v in tracks.items()}
    n = int(round(dur * FPS)) + 1
    poses = []
    for i in range(n):
        t = i / FPS
        val = {k: f(t) for k, f in tr.items()}
        poses.append(body_pose(H.R, state(t, val)))
    return poses


def feet_of(val, H):
    return {s: (val[f'{s}foot'], val[f'{s}foot_yaw'], val.get(f'{s}foot_pitch', 0.), val.get(f'{s}toe', 0.)) for s in 'lr'}


def springs(dur, kicks):
    """[(t, k, c)] -> functions: a spring per name, each kicked once at its t."""
    return {name: Spring(k=k, c=c).run([(t, 1.)], dur) for name, (t, k, c) in kicks.items()}


# ------------------------------------------------------------------ the wizard
class Wizard(Hero):
    """An old man with a tall staff (weapon_r, the gem on weapon_tip). Facing +X."""
    def __init__(self, rig):
        super().__init__(rig)
        H = rig.rest_h
        self.target = V((6., 0., 2.6))     # the dais, where the beholder will hang
        self.palm_rest_r = rig.palm['r'][0]

    def staff(self, grip, axis, palm):
        return self.weapon('weapon_r', 'weapon_tip', grip, axis, palm, self.palm_rest_r)

    def idle_state(self, t, breath=True):
        a = self.ank
        br = math.sin(t * 2 * math.pi / 3.6) if breath else 0.
        sw = math.sin(t * 2 * math.pi / 7.2 + .4) if breath else 0.
        return dict(hips=V((.01 * sw, .0, -.025 + .004 * br)), yaw=-8. + 2. * sw, chest=V((-2. * sw, 14. + 1.2 * br, 0.)), pitch=6.,
                    lfoot=V((.08, .13, a)), rfoot=V((-.06, -.15, a)), lfoot_yaw=12., rfoot_yaw=-18.,
                    grip=V((.24, -.3, 1.2 + .004 * br)), axis=V((.06, -.05, 1.)), spalm=V((-.2, 1., 0.)),
                    lwrist=None, look=V((4., .3 * sw, 1.7)))

    def state(self, t, v, extra=None):
        wr, qr = self.staff(v['grip'], v['axis'].normalized(), v['spalm'])
        arms = {'r': {'wrist': wr, 'pole': V((-.4, -1., -.5)), 'hand': qr, 'curl': 1., 'thumb': .8}}
        if v.get('lmode', 0.) < .5:
            # the left hand on the staff above the right
            p = v['grip'] + v['axis'].normalized() * .13
            wl, ql = self.on_shaft('l', p, v['axis'].normalized(), V((.1, -1., 0.)))
            arms['l'] = {'wrist': wl, 'pole': V((-.3, 1., -.6)), 'hand': ql, 'curl': .95, 'thumb': .8}
        else:
            ql = self.hand_q('l', v['lalong'].normalized(), v['lpalm'].normalized())
            arms['l'] = {'wrist': v['lwrist'], 'pole': v.get('lpole', V((-.2, 1., -.8))), 'hand': ql, 'curl': v.get('lcurl', .2), 'thumb': .1}
        b = {'hips': v['hips'], 'yaw': v['yaw'], 'pitch': v.get('pitch', 6.), 'roll': 0., 'chest': (v['chest'].x, v['chest'].y, v['chest'].z),
             'feet': feet_of(v, self), 'look': v['look'], 'arms': arms}
        return b


def W_Idle(Wz):
    """Leaning on his staff, both hands on it, breathing, looking about."""
    n = int(7.2 * FPS); poses = []
    for i in range(n):
        t = i / FPS
        v = Wz.idle_state(t)
        poses.append(body_pose(Wz.R, Wz.state(t, v)))
    return poses, {'loops': True}


def channel_state(Wz, t, tremble=1.):
    a = Wz.ank
    tr = lambda f, ph: (math.sin(t * f + ph) + .6 * math.sin(t * f * 2.3 + ph * 1.7)) * tremble
    br = math.sin(t * 2 * math.pi / 2.4)
    return dict(hips=V((.04, .0, -.07)), yaw=-14., chest=V((8., 4. + br, 0.)), pitch=2.,
                lfoot=V((.2, .14, a)), rfoot=V((-.22, -.17, a)), lfoot_yaw=10., rfoot_yaw=-30.,
                grip=V((.36, -.3, 1.34)) + V((tr(9., .3), tr(11., 1.), tr(8., 2.))) * .003,
                axis=V((.55, -.08, .83)), spalm=V((-.3, .9, .2)),
                lmode=1., lwrist=V((.5, .14, 1.52)) + V((tr(13., .7), tr(10., .1), tr(12., 1.3))) * .004,
                lalong=V((.35, .05, .94)), lpalm=V((1., -.1, -.25)), lcurl=.22 + .04 * math.sin(t * 7.),
                look=Wz.target)


def W_Channel(Wz):
    """Working the binding: braced, the staff thrust up at the dais in his right hand, his left
    reaching out after it, palm open, trembling with the effort (a loop of 4.8 s)."""
    n = int(4.8 * FPS); poses = []
    for i in range(n):
        t = i / FPS
        poses.append(body_pose(Wz.R, Wz.state(t, channel_state(Wz, t))))
    return poses, {'loops': True}


def W_Cast(Wz):
    """A bolt from the binding: the left hand drawn back to his chest, then flung out, palm first, the
    staff jerking forward with it; a recoil, and back to the channel."""
    def st(t, v):
        c = channel_state(Wz, t, tremble=.4)
        for k in ('lwrist', 'lalong', 'lpalm', 'lcurl', 'hips', 'yaw', 'chest', 'grip', 'axis'):
            if k in v: c[k] = v[k]
        return Wz.state(t, c)
    c0 = channel_state(Wz, 0., tremble=0.)
    tracks = {
        'lwrist': [(0., c0['lwrist']), (.22, V((.12, .12, 1.36))), (.34, V((.66, .08, 1.48))), (.42, V((.6, .1, 1.5))), (.75, c0['lwrist']), (1.1, c0['lwrist'])],
        'lalong': [(0., c0['lalong']), (.22, V((.2, -.3, .93))), (.34, V((.3, 0., .95))), (.75, c0['lalong']), (1.1, c0['lalong'])],
        'lpalm': [(0., c0['lpalm']), (.22, V((-.2, -1., 0.))), (.34, V((1., -.05, -.2))), (.75, c0['lpalm']), (1.1, c0['lpalm'])],
        'lcurl': [(0., .22), (.22, .7), (.3, .7), (.34, .05), (.6, .2), (1.1, .22)],
        'hips': [(0., c0['hips']), (.22, c0['hips'] + V((-.04, 0, .01))), (.36, c0['hips'] + V((.06, 0, -.02))), (.8, c0['hips']), (1.1, c0['hips'])],
        'yaw': [(0., c0['yaw']), (.22, -26.), (.36, 0.), (.8, c0['yaw']), (1.1, c0['yaw'])],
        'chest': [(0., c0['chest']), (.22, V((-12., 0., 0.))), (.36, V((14., 10., 0.))), (.46, V((10., 6., 0.))), (.85, c0['chest']), (1.1, c0['chest'])],
        'grip': [(0., c0['grip']), (.22, c0['grip'] + V((-.06, 0, .04))), (.36, c0['grip'] + V((.1, 0, .02))), (.85, c0['grip']), (1.1, c0['grip'])],
        'axis': [(0., c0['axis']), (.36, V((.75, -.05, .66))), (.85, c0['axis']), (1.1, c0['axis'])],
    }
    return hero_clip(Wz, tracks, 1.1, st), {'fire': .34, 'fire_bone': 'hand_l'}


def W_Hit(Wz, heavy):
    """Struck: he staggers back (heavy: two steps), his left arm up across his face, bent over the
    staff, then straightens back into the channel."""
    dur = 1.8 if heavy else 1.0
    P = 1.8 if heavy else .6
    sp = springs(dur, {'x': (.02, 150., 13.), 'y': (.03, 60., 9.), 'h': (.05, 240., 14.)})
    a = Wz.ank
    def st(t, v):
        c = channel_state(Wz, t, tremble=.2)
        x, y, h = sp['x'](t) * 9., sp['y'](t) * 9., sp['h'](t) * 9.
        c['hips'] = c['hips'] + V((-.07 * P * y, 0., -.03 * P * abs(y))) + V((-v['back'], 0., 0.))
        c['chest'] = V((c['chest'].x - 6. * P * x, c['chest'].y - 14. * P * x, 0.))
        c['pitch'] = 2. - 10. * P * x - 6. * P * h
        c['look'] = Wz.target + V((0., 0., -2.5 * P * h))
        guard = clamp(x * 2.)
        c['lwrist'] = c['lwrist'].lerp(V((.2, .02, 1.6)), guard)
        c['lalong'] = c['lalong'].lerp(V((0., -1., .3)), guard); c['lpalm'] = c['lpalm'].lerp(V((1., 0., 0.)), guard)
        c['grip'] = c['grip'] + V((-.12 * P * x, 0., -.04 * P * x))
        if heavy:
            c['rfoot'], c['lfoot'] = v['rfoot'], v['lfoot']
        return Wz.state(t, c)
    c0 = channel_state(Wz, 0., tremble=0.)
    tracks = {'back': [(0., 0.), (.05, 0.), (.6, .25 if heavy else 0.), (1.1, .25 if heavy else 0.), (1.6, 0.), (dur, 0.)],
              'rfoot': [(0., c0['rfoot']), (.12, c0['rfoot']), (.22, c0['rfoot'] + V((-.15, 0, .06))), (.32, c0['rfoot'] + V((-.3, -.02, 0))), (1.05, c0['rfoot'] + V((-.3, -.02, 0))), (1.25, c0['rfoot'] + V((-.15, 0, .05))), (1.4, c0['rfoot']), (dur, c0['rfoot'])],
              'lfoot': [(0., c0['lfoot']), (.32, c0['lfoot']), (.44, c0['lfoot'] + V((-.12, 0, .05))), (.56, c0['lfoot'] + V((-.24, 0, 0))), (1.3, c0['lfoot'] + V((-.24, 0, 0))), (1.45, c0['lfoot'] + V((-.12, 0, .05))), (1.6, c0['lfoot']), (dur, c0['lfoot'])]}
    return hero_clip(Wz, tracks, dur, st), {'hit': 0.}


WIZARD_CLIPS = {'Idle': W_Idle, 'Channel': W_Channel, 'Cast': W_Cast, 'HitLight': lambda W: W_Hit(W, False), 'HitHeavy': lambda W: W_Hit(W, True)}


# ------------------------------------------------------------------ the ranger
class Ranger(Hero):
    """A longbow (weapon_l, its nocking point on bowstring) and arrows (arrow_r in her right fingers;
    the quiver on her back). Facing +X, she draws side-on, her left shoulder to the target."""
    def __init__(self, rig):
        super().__init__(rig)
        H = rig.rest_h
        self.target = V((9., 0., 1.4))
        self.bow_toward_rest = (H['bowstring'] - H['weapon_l']).normalized()
        self.arrow_axis_rest = (H['index_01_r'] - H['pinky_01_r']).normalized()
        q = [o for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith('quiver')]
        top = None
        for o in q:
            for vv in o.data.vertices:
                c = o.matrix_world @ vv.co
                if top is None or c.z > top.z: top = c
        self.quiver_top = top if top is not None else H['spine_03'] + V((-.12, -.08, .25))

    def bow(self, grip, axis, toward):
        return self.weapon('weapon_l', 'bowstring', grip, axis, toward, None)

    def bow_hand(self, grip, axis, toward):
        R = self.R; H = R.rest_h
        palm, along, across = R.palm['l']
        a0 = (across).normalized()                      # the bow's axis at rest (build_temple_heroes: across)
        q = frame_from(axis, toward) @ frame_from(a0, self.bow_toward_rest).inverted()
        wrist = grip - q @ (H['weapon_l'] - H['hand_l'])
        return wrist, q

    def quiver(self, pose):
        """Where the quiver's mouth is now (carried by her chest)."""
        H = pose.H; R = self.R
        return H['spine_03'] + pose.D['spine_03'] @ (self.quiver_top - R.rest_h['spine_03'])

    def state(self, t, v):
        a = self.ank
        wl, ql = self.bow_hand(v['bgrip'], v['baxis'].normalized(), v['btoward'].normalized())
        arms = {'l': {'wrist': wl, 'pole': V((-.2, .5, -1.)), 'hand': ql, 'curl': 1., 'thumb': .7}}
        R = self.R; H0 = R.rest_h
        qr = self.hand_q('r', v['ralong'].normalized(), v['rpalm'].normalized())
        nock_off = H0['arrow_r'] - H0['hand_r']
        if v.get('rquiver', 0.) > 0.:
            k = v['rquiver']
            def wrist_q(pose, qr=qr, k=k, v=v):
                at = self.quiver(pose) + V((0., 0., .06))
                return v['rwrist'].lerp(at - qr @ nock_off, k)
            rw = wrist_q
        else:
            rw = v['rwrist']
        arms['r'] = {'wrist': rw, 'pole': v.get('rpole', V((-.6, -.6, -.2))), 'hand': qr, 'curl': v.get('rcurl', .55), 'thumb': .5}
        drawn, arrow = v.get('drawn', 0.), v.get('arrow', 1.)
        def post(pose, drawn=drawn, arrow=arrow, v=v):
            H = pose.H
            if drawn > 0.:
                pose.loc['bowstring'] = (H['arrow_r'] - H['bowstring']) * drawn
            else:
                pose.loc['bowstring'] = (H['weapon_l'] - H['bowstring']).normalized() * v.get('twang', 0.)
            if v.get('aim', 0.) > 0.:
                now = pose.D['arrow_r'] @ self.arrow_axis_rest
                want = (H['weapon_l'] + (pose.D['weapon_l'] @ V((0., 0., 0.))) - H['arrow_r']).normalized()
                pose.turn('arrow_r', Quaternion().slerp(swing(now, want), v['aim']))
            pose.scale['arrow_r'] = 1. if arrow > .5 else .001
        b = {'hips': v['hips'], 'yaw': v['yaw'], 'pitch': v.get('pitch', 4.), 'roll': 0., 'chest': (v['chest'].x, v['chest'].y, v['chest'].z),
             'feet': feet_of(v, self), 'look': v['look'], 'arms': arms, 'post': post}
        return b


def ranger_ready(Rg, t=0., breath=True):
    a = Rg.ank
    br = math.sin(t * 2 * math.pi / 3.0) if breath else 0.
    return dict(hips=V((0., 0., -.03 + .004 * br)), yaw=-48., chest=V((-6., 6. + br, 0.)), pitch=4.,
                lfoot=V((.12, .16, a)), rfoot=V((-.16, -.14, a)), lfoot_yaw=-20., rfoot_yaw=-80.,
                bgrip=V((.3, .14, 1.04)), baxis=V((.45, .05, .9)), btoward=V((-.6, -.6, .2)),
                rwrist=V((.12, -.04, 1.02)), ralong=V((.6, .3, -.5)), rpalm=V((-.5, .8, .2)), rcurl=.6,
                drawn=1., arrow=1., aim=1., look=Rg.target)


def R_Idle(Rg):
    """Ready: an arrow on the string, the bow low across her, watching (a loop of 6 s)."""
    n = int(6. * FPS); poses = []
    for i in range(n):
        t = i / FPS
        v = ranger_ready(Rg, t)
        v['look'] = Rg.target + V((0., 1.2 * math.sin(t * 2 * math.pi / 6.), 0.))
        v['rwrist'] = None
        poses.append(body_pose(Rg.R, Rg.state(t, ready_hands(Rg, v))))
    return poses, {'loops': True}


def ready_hands(Rg, v):
    """With the arrow nocked, the right hand is wherever the string is: put it there."""
    if v.get('rwrist') is None:
        wl, ql = Rg.bow_hand(v['bgrip'], v['baxis'].normalized(), v['btoward'].normalized())
        nock = v['bgrip'] + ql @ (Rg.R.rest_h['bowstring'] - Rg.R.rest_h['weapon_l'])
        qr = Rg.hand_q('r', v['ralong'].normalized(), v['rpalm'].normalized())
        v['rwrist'] = nock - qr @ (Rg.R.rest_h['arrow_r'] - Rg.R.rest_h['hand_r'])
    return v


def R_Shoot(Rg):
    """One shot and the next arrow: up and draw to the corner of her jaw, hold, loose (the string
    snaps forward, the drawing hand flies back past her ear), then over her shoulder to the quiver,
    the arrow brought round to the bow and nocked, back to ready (2.6 s; the loose at 1.0 s)."""
    a = Rg.ank
    r0 = ranger_ready(Rg, 0., breath=False)
    full = dict(bgrip=V((.62, .04, 1.43)), baxis=V((.0, -.12, 1.)), btoward=V((-1., -.05, 0.)))
    anchor = V((.04, -.1, 1.5))
    tracks = {
        'hips': [(0., r0['hips']), (.3, V((0., 0., -.02))), (2.2, V((0., 0., -.02))), (2.6, r0['hips'])],
        'yaw': [(0., r0['yaw']), (.3, -78.), (2.2, -78.), (2.6, r0['yaw'])],
        'chest': [(0., r0['chest']), (.3, V((-8., 2., 0.))), (.7, V((-10., -2., 0.))), (1.0, V((-10., -2., 0.))), (1.15, V((-12., -3., 0.))), (1.5, V((-14., 6., 0.))), (1.9, V((-6., 4., 0.))), (2.2, V((-8., 2., 0.))), (2.6, r0['chest'])],
        'bgrip': [(0., r0['bgrip']), (.32, full['bgrip']), (1.0, full['bgrip']), (1.06, full['bgrip'] + V((.03, -.01, -.01))), (1.35, full['bgrip'] + V((.0, 0., -.04))), (1.8, V((.44, .08, 1.28))), (2.15, V((.44, .08, 1.28))), (2.6, r0['bgrip'])],
        'baxis': [(0., r0['baxis']), (.32, full['baxis']), (1.35, full['baxis']), (1.8, V((.3, -.1, .95))), (2.15, V((.3, -.1, .95))), (2.6, r0['baxis'])],
        'btoward': [(0., r0['btoward']), (.32, full['btoward']), (1.35, full['btoward']), (1.8, V((-.8, -.5, 0.))), (2.6, r0['btoward'])],
        # the right hand: on the string (drawn), to the anchor, the loose, to the quiver, back to the bow
        'rwrist': [(0., V((.3, -.05, 1.08))), (.32, V((.48, -.06, 1.4))), (.72, anchor), (1.0, anchor), (1.05, anchor + V((-.1, -.04, .0))), (1.3, anchor + V((-.13, -.06, -.02))),
                   (1.5, V((-.05, -.18, 1.6))), (1.75, V((-.05, -.18, 1.6))), (2.1, V((.36, -.06, 1.26))), (2.6, V((.3, -.05, 1.08)))],
        'rquiver': [(0., 0.), (1.3, 0.), (1.55, 1.), (1.72, 1.), (1.95, 0.), (2.6, 0.)],
        'ralong': [(0., r0['ralong']), (.32, V((1., .1, -.1))), (1.0, V((1., .1, -.1))), (1.05, V((.6, .2, .3))), (1.5, V((-.2, .2, .9))), (1.75, V((-.2, .2, .9))), (2.1, V((.8, .2, -.3))), (2.6, r0['ralong'])],
        'rpalm': [(0., r0['rpalm']), (.32, V((0., 1., 0.))), (1.0, V((0., 1., 0.))), (1.5, V((-.3, .9, .3))), (2.1, V((-.4, .9, .2))), (2.6, r0['rpalm'])],
        'rcurl': [(0., .6), (.32, .62), (1.0, .62), (1.03, .15), (1.4, .2), (1.6, .9), (2.1, .8), (2.6, .6)],
        'rpole': [(0., V((-.6, -.6, -.2))), (.6, V((-1., -.3, .3))), (1.2, V((-1., -.3, .3))), (1.5, V((-.3, -.7, .8))), (1.8, V((-.3, -.7, .8))), (2.2, V((-.6, -.6, -.2))), (2.6, V((-.6, -.6, -.2)))],
        'drawn': [(0., 1.), (1.0, 1.), (1.001, 0.), (2.05, 0.), (2.1, 1.), (2.6, 1.)],
        'twang': [(0., 0.), (1.0, 0.), (1.04, .03), (1.08, -.02), (1.12, .012), (1.16, -.006), (1.22, 0.), (2.6, 0.)],
        'arrow': [(0., 1.), (1.0, 1.), (1.001, 0.), (1.6, 0.), (1.601, 1.), (2.6, 1.)],
        'aim': [(0., 1.), (1.0, 1.), (1.001, 0.), (2.05, 0.), (2.12, 1.), (2.6, 1.)],
        'look': [(0., Rg.target), (1.25, Rg.target), (1.55, V((3., -2., 1.6))), (1.8, V((3., -2., 1.6))), (2.15, Rg.target), (2.6, Rg.target)],
    }
    for ch in ('lfoot', 'rfoot', 'lfoot_yaw', 'rfoot_yaw'):
        tracks[ch] = [(0., r0[ch])]
    tracks['lfoot_yaw'] = [(0., r0['lfoot_yaw']), (.3, -10.), (2.2, -10.), (2.6, r0['lfoot_yaw'])]
    def st(t, v):
        if v['drawn'] > .5 and t < .99 or t > 2.09:
            # hooked on the string: the string's nock follows the hand (pose.loc), the hand is free
            pass
        return Rg.state(t, v)
    return hero_clip(Rg, tracks, 2.6, st), {'fire': 1.0, 'fire_bone': 'arrow_r', 'loops': True}


def R_Hit(Rg, heavy):
    dur = 1.6 if heavy else .9
    P = 1.6 if heavy else .6
    sp = springs(dur, {'x': (.02, 150., 13.), 'y': (.03, 60., 9.), 'h': (.05, 240., 14.)})
    def st(t, v):
        c = ready_hands(Rg, ranger_ready(Rg, t))
        x, y, h = sp['x'](t) * 9., sp['y'](t) * 9., sp['h'](t) * 9.
        c['hips'] = c['hips'] + V((-.07 * P * y, .03 * P * y, -.03 * P * abs(y)))
        c['chest'] = V((c['chest'].x + 8. * P * x, c['chest'].y - 14. * P * x, 0.))
        c['pitch'] = 4. - 10. * P * x - 6. * P * h
        c['look'] = Rg.target + V((0., 0., -2. * P * h))
        c['bgrip'] = c['bgrip'] + V((-.1 * P * x, .06 * P * x, .05 * P * x))
        c['rwrist'] = c['rwrist'] + V((-.08 * P * x, -.06 * P * x, .06 * P * x))
        if heavy:
            c['rfoot'] = v['rfoot']; c['lfoot'] = v['lfoot']
        return Rg.state(t, c)
    r0 = ranger_ready(Rg, 0., breath=False)
    tracks = {'rfoot': [(0., r0['rfoot']), (.1, r0['rfoot']), (.2, r0['rfoot'] + V((-.12, -.05, .06))), (.3, r0['rfoot'] + V((-.24, -.08, 0))), (1.0, r0['rfoot'] + V((-.24, -.08, 0))), (1.15, r0['rfoot'] + V((-.12, -.04, .04))), (1.3, r0['rfoot']), (dur, r0['rfoot'])],
              'lfoot': [(0., r0['lfoot']), (dur, r0['lfoot'])]}
    return hero_clip(Rg, tracks, dur, st), {'hit': 0.}


RANGER_CLIPS = {'Idle': R_Idle, 'Shoot': R_Shoot, 'HitLight': lambda R: R_Hit(R, False), 'HitHeavy': lambda R: R_Hit(R, True)}


def run(name):
    rig = Rig(name)
    bpy.context.scene.render.fps, bpy.context.scene.render.fps_base = FPS, 1.
    if name == 'Fighter':
        F = Fighter(rig); clips = {c: (lambda f=f: f(F)) for c, f in FIGHTER_CLIPS.items()}
    elif name == 'Wizard':
        Wz = Wizard(rig); clips = {c: (lambda f=f: f(Wz)) for c, f in WIZARD_CLIPS.items()}
    else:
        Rg = Ranger(rig); clips = {c: (lambda f=f: f(Rg)) for c, f in RANGER_CLIPS.items()}
    path = HEROES / f'Hero{name}/Anim/clips.json'
    manifest = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'generator': 'Tools/build_hero_anims.py', 'clips': {}}
    for clip, make in clips.items():
        if ONLY_CLIPS and clip not in ONLY_CLIPS: continue
        poses, info = make()
        act = key(rig, poses, f'AS_Hero{name}_{clip}')
        m = measure(rig, poses, info.get('contact')) if name == 'Fighter' else {}
        if 'contact_fixed' in info: m.update(contact_s=info['contact_fixed'], contact_frame=int(round(info['contact_fixed'] * FPS)))
        if 'fire' in info:
            f = int(round(info['fire'] * FPS)); bpy.context.scene.frame_set(f + 1)
            pb = rig.obj.pose.bones[info['fire_bone']]; at = rig.obj.matrix_world @ pb.head
            m.update(fire_s=info['fire'], fire_frame=f, fire_bone=info['fire_bone'], fire_cm=[round(at.x * 100, 1), round(-at.y * 100, 1), round(at.z * 100, 1)])
        if 'root_x' in info: m['root_x_cm'] = info['root_x']
        if 'hit' in info: m['hit_s'] = info['hit']
        export(rig, clip)
        if REVIEW:
            review(rig, clip, len(poses))
            if 'contact_frame' in m or 'fire_frame' in m:
                c = m.get('contact_frame', m.get('fire_frame')) + 1
                review_keys(rig, clip, [1, max(1, c - 7), max(1, c - 4), max(1, c - 2), c, c + 3, min(len(poses), c + 8)], view=((1.6, -3.4, 1.35), (86, 0, 22)))
        manifest['clips'][clip] = {'fbx': f'Anim/AS_Hero{name}_{clip}.fbx', 'frames': len(poses), 'fps': FPS, 'loops': info.get('loops', False),
                                   **{k: v for k, v in m.items() if k != 'tip_cm'}}
        print('CHUCK_HERO_CLIP', name, clip, f'frames={len(poses)}', {k: v for k, v in m.items() if k != 'tip_cm'})
        rig.obj.animation_data.action = None
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')


def write_clip_data():
    """Unreal/Chuck3D/Source/Chuck3D/HeroClipData.h from every hero's clips.json: lengths, moments of
    contact and release, contact points (cm, the hero's component frame: +X ahead, +Y his right, +Z up),
    and the steps' travel per frame."""
    L = ['#pragma once', '// Generated by Tools/build_hero_anims.py from SourceAssets/Heroes/*/Anim/clips.json. Do not edit.',
         "// Seconds; cm in the hero's component frame (+X ahead, +Y his right, +Z up from the floor).", 'namespace HeroClipData', '{']
    for name in ('Fighter', 'Wizard', 'Ranger'):
        path = HEROES / f'Hero{name}/Anim/clips.json'
        if not path.exists(): continue
        clips = json.loads(path.read_text(encoding='utf-8'))['clips']
        L.append(f'    namespace {name}')
        L.append('    {')
        for clip, c in clips.items():
            L.append(f'        constexpr float {clip}Length = {(c["frames"] - 1) / c["fps"]:.4f}f;')
            if 'contact_s' in c: L.append(f'        constexpr float {clip}Contact = {c["contact_s"]:.4f}f;')
            if 'contact_point_cm' in c:
                x, y, z = c['contact_point_cm']; L.append(f'        constexpr float {clip}ContactX = {x:.1f}f, {clip}ContactY = {y:.1f}f, {clip}ContactZ = {z:.1f}f;')
                L.append(f'        constexpr float {clip}Speed = {c["contact_speed_m_s"]:.2f}f;   // m/s at the blade tip')
            if 'fire_s' in c:
                x, y, z = c['fire_cm']; L.append(f'        constexpr float {clip}Fire = {c["fire_s"]:.4f}f;   // {c["fire_bone"]} at ({x:.1f}, {y:.1f}, {z:.1f})')
                L.append(f'        constexpr float {clip}FireX = {x:.1f}f, {clip}FireY = {y:.1f}f, {clip}FireZ = {z:.1f}f;')
            if 'root_x_cm' in c:
                L.append(f'        constexpr float {clip}Root[] = {{{", ".join(f"{v:.2f}f" for v in c["root_x_cm"])}}};   // cm ahead, per frame')
        L.append('    }')
    L.append('}')
    out = ROOT / 'Unreal/Chuck3D/Source/Chuck3D/HeroClipData.h'
    out.write_text('\n'.join(L) + '\n', encoding='utf-8')
    print('CHUCK_HERO_CLIPDATA', out)


if __name__ == '__main__':
    if '--data-only' not in argv:
        for who in WHO: run(who)
    write_clip_data()
    print('CHUCK_HERO_ANIMS_READY', WHO)
