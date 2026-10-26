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


# ------------------------------------------------------------------ the elbow
def choose_elbow(rig, H, s, wrist, hand_q, pole, remember=True, cost_out=None):
    """Where the elbow goes (2026-10-25; user: "The archer's arm goes through herself often ... the fighter often holds his
    sword in a way where his hand/wrist is strangely bent"). With the shoulder and the wrist fixed the elbow can only be on
    a circle; a fixed pole put it where the forearm ran into the body or met the hand at a broken angle. Each candidate
    on the circle is scored by:
      * the wrist's bend: the forearm against the hand's own long axis (the forearm's way for a straight wrist);
      * the upper arm and forearm going into the torso, hips or head (capsules from this pose's joints, the hero's girth);
      * a little for straying from the keyed pole (the animator's intent), and for jumping from the last frame's choice.
    Returns a point to bend toward (two_bone projects it onto the circle)."""
    R = rig
    S = H[f'upperarm_{s}']
    l1 = (R.rest_h[f'lowerarm_{s}'] - R.rest_h[f'upperarm_{s}']).length
    l2 = (R.rest_h[f'hand_{s}'] - R.rest_h[f'lowerarm_{s}']).length
    d = wrist - S; dist = clamp(d.length, abs(l1 - l2) + 1e-3, l1 + l2 - 1e-4); dn = d.normalized()
    a = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist); h = math.sqrt(max(0., l1 * l1 - a * a))
    C = S + dn * a
    u = (pole - dn * pole.dot(dn)); u = u.normalized() if u.length > 1e-6 else dn.orthogonal().normalized()
    v = dn.cross(u)
    want = (hand_q @ (R.rest_h[f'hand_{s}'] - R.rest_h[f'lowerarm_{s}'])).normalized() if hand_q is not None else None
    girth = getattr(R, 'girth', .16)
    head_up = (H['head'] - H['neck_01']).normalized()
    caps = [(H['spine_01'], H['neck_01'], girth), (H['pelvis'], H['spine_01'], girth * 1.05),
            (H['head'] + head_up * .09, H['head'] + head_up * .09, .12)]
    last = getattr(R, '_elbow_last', {}).get(s)
    best, best_k = None, 0.
    for k in range(48):
        ang = k / 48. * 2 * math.pi
        E = C + (u * math.cos(ang) + v * math.sin(ang)) * h
        cost = 0.
        if want is not None: cost += math.degrees((wrist - E).angle(want, 0.))
        for t0, t1, A, B in ((.35, 1., S, E), (0., .85, E, wrist)):   # (the elbow itself is the end of the first)
            for j in range(5):
                q = A.lerp(B, t0 + (t1 - t0) * j / 4)
                for c0, c1, r in caps:
                    ab = c1 - c0; w = 0. if ab.length < 1e-6 else max(0., min(1., (q - c0).dot(ab) / ab.length_squared))
                    deficit = r + .05 - (q - (c0 + ab * w)).length
                    if deficit > 0: cost += deficit * 2500.
        cost += math.degrees(min(ang, 2 * math.pi - ang)) * .15                      # the keyed pole
        if last is not None: cost += math.degrees(min(abs(ang - last), 2 * math.pi - abs(ang - last))) * .35
        if best is None or cost < best:
            best, best_k = cost, ang
    if remember:
        if not hasattr(R, '_elbow_last'): R._elbow_last = {}
        R._elbow_last[s] = best_k
    if cost_out is not None: cost_out.append(best)
    return C + (u * math.cos(best_k) + v * math.sin(best_k)) * h


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
        pole = arm['pole']
        if arm.get('auto', True):
            # The elbow placed for a natural wrist and clear of the body (choose_elbow), unless the arm's elbow is exact
            # (the fighter's shield arm).
            H = pose.fk()
            mid = (H[f'upperarm_{s}'] + arm['wrist']) * .5
            pole = choose_elbow(rig, H, s, arm['wrist'], arm.get('hand'), pole) - mid
        solve_limb(pose, f'upperarm_{s}', f'lowerarm_{s}', f'hand_{s}', arm['wrist'], pole, end_delta=arm.get('hand'))
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
        rig.girth = .2                         # his torso in plate over a gambeson (choose_elbow)
        # the sword's rest frame (weapon_r: grip -> tip), and where the grip sits on the hand
        self.sw_rest = frame_from(H['weapon_tip'] - H['weapon_r'], self._edge_rest())
        self.grip_off = H['weapon_r'] - H['hand_r']
        self.sh_rest_c = H['shield_l']
        # The shield's mount (2026-10-23): build_temple_heroes carries it with the forearm across its back,
        # rising 12 degrees to the fist, its top on the thumb's side. The same rule on this rest skeleton
        # (his left is +Y here) gives its face's and top's ways; it must put the centre on the bone.
        E, W = H['lowerarm_l'], H['hand_l']
        fa = (W - E).normalized()
        out = (V((0, 1, 0)) - fa * fa.y).normalized()
        fp = out.cross(fa).normalized()
        if fp.dot(H['thumb_01_l'] - W) < 0: fp = -fp
        up = (fa * math.sin(math.radians(12.)) + fp * math.cos(math.radians(12.))).normalized()
        # the bone carries it: its tail along the top, its roll axis out of the face (checked against the rule)
        bone = rig.obj.data.bones['shield_l']; m3 = rig.obj.matrix_world.to_3x3() @ bone.matrix_local.to_3x3()
        b_up, b_out = (m3 @ V((0, 1, 0))).normalized(), (m3 @ V((0, 0, 1))).normalized()
        assert b_up.angle(up) < math.radians(1.) and b_out.angle(out) < math.radians(1.), ('shield mount mismatch', b_up, up, b_out, out)
        up, out = b_up, b_out
        self.sh_up, self.sh_out = up, out
        self.upper_len = (H['lowerarm_l'] - H['upperarm_l']).length
        fore_len = (H['hand_l'] - H['lowerarm_l']).length
        self.fold_min = math.sqrt(self.upper_len ** 2 + fore_len ** 2 - 2 * self.upper_len * fore_len * math.cos(math.radians(60.)))

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
        # the shield's rest frame as built: its face out, its top up, the forearm across its back
        q = frame_from(face, top) @ frame_from(self.sh_out, self.sh_up).inverted()
        elbow = centre - q @ (H['shield_l'] - H['lowerarm_l'])
        wrist = elbow + q @ (H['hand_l'] - H['lowerarm_l'])
        return wrist, q, elbow

    # The shield as a slab: the boards' extent across and up, and their depth behind the face (they bow back to
    # the rim), with 2 cm of margin all round.
    SH_HALF_U, SH_HALF_V, SH_DEPTH = .32, .41, .08

    def keep_out(self, H, centre, q, blade=None):
        """How far to move the shield so its slab keeps clear of his body: capsules round the torso (spine to neck),
        hips, thighs, the left shoulder and the head, from this pose's joints, and round his own sword's blade. Each sample point is pushed off along
        the line to its nearest point on the slab (or straight out of the face if it is inside); the deepest wins."""
        ac = q @ self.sh_up.cross(self.sh_out); up = q @ self.sh_up; out = q @ self.sh_out
        head_up = (H['head'] - H['neck_01']).normalized()
        parts = [(H['spine_01'], H['neck_01'], .2), (H['pelvis'], H['spine_01'], .21),
                 (H['thigh_l'], H['calf_l'], .13), (H['thigh_r'], H['calf_r'], .13),
                 (H['upperarm_l'], H['upperarm_l'], .12), (H['head'] + head_up * .1, H['head'] + head_up * .1, .17)]
        if blade: parts.append((blade[0], blade[1], .08))
        best, push = 0., V()
        for a, b, r in parts:
            for k in range(9):
                p = a.lerp(b, k / 8); rel = p - centre
                u = max(-self.SH_HALF_U, min(self.SH_HALF_U, rel.dot(ac))); v = max(-self.SH_HALF_V, min(self.SH_HALF_V, rel.dot(up)))
                d = max(-self.SH_DEPTH, min(0., rel.dot(out)))
                near = centre + ac * u + up * v + out * d
                gap = near - p
                inside = gap.length < 1e-4
                deficit = r - (0. if inside else gap.length)
                if deficit > best:
                    best = deficit
                    push = (out if rel.dot(out) < -self.SH_DEPTH * .5 else -out) * deficit if inside else gap.normalized() * deficit
        return push

    def best_roll(self, t, H0, grip, d, edge, look, limit=180., weight=.12):
        e0 = (edge - d * edge.dot(d)).normalized()
        last = getattr(self, '_roll_last', None) if t > 0 else None
        fa = self.R.rest_h['hand_r'] - self.R.rest_h['lowerarm_r']
        best, best_e, best_phi = None, e0, 0.
        for k in range(-12, 12):
            phi = k * 15.
            # (Either edge may lead: the sword is double-edged. The roll's distance from the keyed edge or its opposite.)
            off = min(abs(phi), 180. - abs(phi))
            if off > limit: continue
            e = rot(d, phi) @ e0
            wrist, q = self.sword_hand(grip, d, e)
            costs = []
            E = choose_elbow(self.R, H0, 'r', wrist, q, V((-.3, -1., -.6)), remember=False, cost_out=costs)
            cost = costs[0] + off * weight + (3. if abs(phi) > 90. else 0.)
            if last is not None: cost += min(abs(phi - last), 360. - abs(phi - last)) * .3
            if best is None or cost < best: best, best_e, best_phi = cost, e, phi
        self._roll_last = best_phi
        return best_e

    def stance(self):
        """The guard's feet: left foot leading, the back foot turned out."""
        a = self.ank
        return {'l': V((.2, .13, a)), 'r': V((-.24, -.16, a))}

    def clear_grip(self, H, grip):
        """The sword hand kept far enough from his torso, hips and head that the crossguard (20 cm wide) and the
        blade's root do not go into his plate and mail when he draws the sword in."""
        head_up = (H['head'] - H['neck_01']).normalized()
        caps = [(H['spine_01'], H['neck_01'], .3), (H['pelvis'], H['spine_01'], .3), (H['head'] + head_up * .1, H['head'] + head_up * .1, .27)]
        for it in range(4):
            moved = False
            for a, b, r in caps:
                ab = b - a; w = 0. if ab.length < 1e-6 else max(0., min(1., (grip - a).dot(ab) / ab.length_squared))
                away = grip - (a + ab * w)
                if away.length < r:
                    grip = grip + (away.normalized() if away.length > 1e-4 else V((1, 0, 0))) * (r - away.length); moved = True
            if not moved: break
        return grip

    def clear_blade(self, H, grip, d):
        """The blade turned about the grip, as little as will do, until it is clear of his torso, hips, thighs and head
        (2026-10-23: an intersection check found it through his chest plate, thigh and helmet in wind-ups and
        recoveries)."""
        head_up = (H['head'] - H['neck_01']).normalized()
        caps = [(H['spine_01'], H['neck_01'], .19), (H['pelvis'], H['spine_01'], .2), (H['thigh_l'], H['calf_l'], .15),
                (H['thigh_r'], H['calf_r'], .15), (H['head'] + head_up * .1, H['head'] + head_up * .1, .17)]
        for it in range(8):
            worst, fix = 0., None
            for k in range(10):
                l = .18 + .86 * k / 9; p = grip + d * l
                for a, b, r in caps:
                    ab = b - a; w = 0. if ab.length < 1e-6 else max(0., min(1., (p - a).dot(ab) / ab.length_squared))
                    q = a + ab * w; away = p - q
                    deficit = r - away.length
                    if deficit > worst:
                        side = away - d * away.dot(d)
                        if side.length < 1e-4: side = d.orthogonal()
                        worst, fix = deficit, (side.normalized(), math.atan2(deficit + .01, l))
            if not fix: break
            side, ang = fix
            d = (d * math.cos(ang) + side * math.sin(ang)).normalized()
        return d

    def body(self, t, hips, yaw, chest, feet, grip, blade, edge, shield_c, shield_face, shield_top, look=None, curl_l=.85, pitch=6., roll=0., rpole=None, clear_sword=True):
        # his joints in this frame (the arms do not move them), for the blade's clearance and the elbow
        b0 = {'hips': hips, 'yaw': yaw, 'pitch': pitch, 'roll': roll, 'chest': chest, 'feet': feet, 'look': look or self.target,
              'arms': {s: {'wrist': V((0, .3 if s == 'l' else -.3, 1.)), 'pole': V((0, 0, -1)), 'auto': False} for s in 'lr'}}
        H0 = body_pose(self.R, b0).H
        if clear_sword:
            grip = self.clear_grip(H0, grip)
            blade = self.clear_blade(H0, grip, blade.normalized())
        if True:
            # The sword's roll about its blade (which way its flat faces) chosen with the elbow for a natural wrist
            # (2026-10-25: the grip leaves the blade 60 degrees off the forearm, so a keyed edge could leave no elbow
            # that kept the wrist straight: in his guard it was bent 90 degrees). The blade points where it was keyed;
            # through each cut's contact window (protect) the edge is kept exactly, so cuts still land edge first.
            # (Inside the window it may turn at most 45 degrees off the leading edge, and is held to it.)
            edge = self.best_roll(t, H0, grip, blade.normalized(), edge, look, limit=180. if clear_sword else 45., weight=.12 if clear_sword else .7)
        wrist_r, q_r = self.sword_hand(grip, blade, edge)
        # a first pose for the shoulder positions
        b = {'hips': hips, 'yaw': yaw, 'pitch': pitch, 'roll': roll, 'chest': chest, 'feet': feet, 'look': look or self.target, 'arms': {}}
        # The keyed facing turned left by SHIELD_YAW where it faces his foe (less as it swings aside).
        turn = rot(Z, SHIELD_YAW * clamp(shield_face.x))
        shield_face, shield_top = turn @ shield_face, turn @ shield_top
        b['arms']['r'] = {'wrist': wrist_r, 'pole': rpole if rpole is not None else V((-.3, -1., -.6)), 'hand': q_r, 'curl': 1., 'thumb': .9}
        b['arms']['l'] = {'pole': V((-.2, .6, -1.)), 'curl': curl_l, 'thumb': .6, 'auto': False}
        # The shield goes where the keys say, then is moved (its facing unchanged) until the arm can hold it and it
        # is clear of his body (2026-10-23):
        #  * the elbow exactly the upper arm's length from the shoulder (else the forearm, and the shield on it, would
        #    come out turned from its keyed facing);
        #  * the wrist no nearer the shoulder than a 60 degree bend allows (the arm never folds shut);
        #  * the shield's boards clear of his torso, head, left shoulder, hips and thighs (keep_out).
        # Each frame starts from most of the last frame's move (smooth in time, easing back to the keys once clear), the
        # keep-out pushes are damped, and the
        # last passes apply only the arm's limits, so the shield is always where the forearm can really hold it.
        off = self._off * .8 if t > 0 and getattr(self, '_off', None) is not None else V()   # easing back to the keys once clear
        for it in range(14):
            wrist_l, q_l, elbow_l = self.shield_arm(None, shield_c + off, shield_face, shield_top)
            b['arms']['l'].update(wrist=wrist_l, hand=q_l)
            pose = body_pose(self.R, b); H = pose.H
            corr = V()
            reach = elbow_l - H['upperarm_l']
            corr -= reach.normalized() * (reach.length - self.upper_len)   # the upper arm is rigid: exactly its length
            span = wrist_l - H['upperarm_l']
            if span.length < self.fold_min: corr += (span.normalized() if span.length > 1e-4 else V((1, 0, 0))) * (self.fold_min - span.length)
            if it < 11: corr += self.keep_out(H, shield_c + off, q_l, blade=(grip + blade.normalized() * .12, grip + blade.normalized() * 1.02)) * .6
            if corr.length < .001 and it >= 11: break
            off += corr
        self._off = off.copy()
        # the left elbow: re-aim the pole at where the shield needs it
        pole = elbow_l - (H['upperarm_l'] + wrist_l) * .5
        b['arms']['l']['pole'] = pole
        return body_pose(self.R, b)


# 2026-10-23: the shield's keys were laid out for the old mount (the forearm up its length, the elbow at its top
# edge). With the forearm across its back they are all moved by this, toward his middle: in guard the elbow is
# forward of his ribs, bent about 75 degrees, the forearm across in front of his chest, the shield's top at his eyes.
SHIELD_SHIFT = V((.02, -.15, .01))
SHIELD_YAW = 30.   # and every facing turned this far to his left: the forearm then points forward across his body


def fighter_guard_state(F, t, breath=True):
    """The guard, as values (used by every clip as its start and end)."""
    a = F.ank
    br = math.sin(t * 2 * math.pi / 3.2) if breath else 0.
    sway = math.sin(t * 2 * math.pi / 6.4 + .7) if breath else 0.
    return dict(
        hips=V((-.02 + .006 * sway, .01, -.07 + .006 * br)), yaw=-27. + 1.5 * sway, chest=(-6. + 1. * sway, 8. + .8 * br, 0.),
        feet={'l': (V((.21, .12, a)), 8., 0., 0.), 'r': (V((-.25, -.14, a)), -42., 0., 0.)},
        grip=V((.16, -.34, 1.42 + .006 * br)), blade=V((.32, .02, .95)).normalized(), edge=V((1., 0., -.3)),   # (2026-10-25: 8 cm forward, off his pauldron)
        shield_c=V((.46, .07, 1.25 + .008 * br)) + SHIELD_SHIFT, shield_face=V((1., -.25, .05)).normalized(), shield_top=V((.05, .1, 1.)),
    )


def fighter_clip(F, keys, dur, extra=None, protect=None):
    """keys: {channel: [(t, value)]} -> poses for every frame. Channels: hips, yaw, chest(3 floats as Vector),
    lfoot, rfoot (Vector ankle), lfoot_yaw, rfoot_yaw, lfoot_pitch..., grip, blade, shield_c, shield_face, shield_top.
    protect: (t0, t1) of the cut around its contact, where the blade keeps exactly its keyed path (no clearing)."""
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
                      roll=tr['roll'](t) if 'roll' in tr else 0., rpole=tr['rpole'](t) if 'rpole' in tr else None,
                      clear_sword=not (protect and protect[0] <= t <= protect[1]))
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
    add(k, 'grip', (.26, V((-.06, -.3, 1.72))), (.34, V((.04, -.34, 1.8))), (.40, V((.32, -.2, 1.6))), (.45, V((.53, .03, 1.3))), (.50, V((.55, .17, 1.03))), (.6, V((.38, .24, .92))), (.75, V((.34, .2, .98))), (.92, V((.3, -.08, 1.08))), (1.1, V((.1, -.3, 1.38))))
    add(k, 'blade', (.26, V((-.8, .05, .6))), (.34, V((-.35, -.5, .8))), (.40, V((.6, -.42, .62))), (.45, V((.86, .14, -.36))), (.50, V((.48, .6, -.62))), (.6, V((-.2, .7, -.7))), (.75, V((-.1, .55, -.8))), (.92, V((.5, -.4, -.77))), (1.1, V((.32, .02, .95))))
    add(k, 'rpole', (0., V((-.3, -1., -.6))), (.26, V((-.4, -1., .5))), (.38, V((-.2, -1., .4))), (.46, V((.1, -1., -.3))), (.6, V((-.2, -.6, -1.))), (1.1, V((-.3, -1., -.6))), (1.45, V((-.3, -1., -.6))))
    # 2026-10-23 (an intersection check): the shield rises high on his left, over his head's side, while the blade cuts
    # down to low left, and only comes back to guard once the sword is back on his right.
    # The sword's recovery comes back low across his front to his right before it rises (not up through his left).
    add(k, 'shield_c', (.26, V((.4, .1, 1.22)) + SHIELD_SHIFT), (.40, V((.32, .34, 1.3))), (.56, V((.24, .48, 1.32))), (.98, V((.24, .48, 1.32))), (1.28, V((.42, .18, 1.22)) + SHIELD_SHIFT))
    add(k, 'shield_face', (.26, V((1., -.15, .05))), (.40, V((.55, .83, .1))), (.56, V((.4, .92, .1))), (.98, V((.4, .92, .1))), (1.28, V((1., 0., .05))))
    return fighter_clip(F, k, 1.45, protect=(.33, .56)), {'contact': (.38, .52, 1.3)}


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
    add(k, 'grip', (.24, V((-.06, -.42, 1.62))), (.32, V((.03, -.47, 1.61))), (.38, V((.28, -.4, 1.59))), (.42, V((.47, -.15, 1.57))), (.47, V((.46, .14, 1.55))), (.56, V((.4, .3, 1.46))), (.72, V((.36, .28, 1.42))), (.88, V((.3, .0, 1.56))), (1.05, V((.16, -.34, 1.42))))
    add(k, 'blade', (.24, V((-.7, -.55, .45))), (.32, V((-.6, -.78, .12))), (.38, V((.15, -.99, 0.))), (.42, V((.97, .2, -.04))), (.47, V((.42, .9, -.08))), (.56, V((-.3, .85, .42))), (.72, V((-.38, .78, .5))), (.88, V((.05, .3, .95))), (1.05, V((.32, .02, .95))))
    add(k, 'rpole', (0., V((-.3, -1., -.6))), (.24, V((-.5, -1., .1))), (.38, V((-.3, -1., -.2))), (.44, V((-.1, -.5, -1.))), (.56, V((.2, .2, -1.))), (.75, V((.2, .1, -1.))), (1.05, V((-.3, -1., -.6))), (1.4, V((-.3, -1., -.6))))
    # 2026-10-23 (an intersection check found the blade through the shield): the follow-through rises over his left
    # shoulder into a recovery rather than wrapping round at shoulder height, and the shield drops low by his left hip,
    # its top tipped out, under the blade's sweep.
    add(k, 'shield_c', (.24, V((.4, .1, 1.22)) + SHIELD_SHIFT), (.38, V((.26, .3, 1.06))), (.46, V((.16, .38, .98))), (.72, V((.16, .38, .98))), (1.0, V((.4, .18, 1.22)) + SHIELD_SHIFT))
    add(k, 'shield_face', (.24, V((1., -.15, .05))), (.38, V((.3, .95, -.1))), (.46, V((.05, 1., -.25))), (.72, V((.05, 1., -.25))), (1.0, V((1., 0., .05))))
    add(k, 'shield_top', (.24, V((.05, .1, 1.))), (.38, V((.05, .25, 1.))), (.46, V((.05, .45, 1.))), (.72, V((.05, .45, 1.))), (1.0, V((.05, .1, 1.))))
    return fighter_clip(F, k, 1.4, protect=(.33, .56)), {'contact': (.34, .5, 1.55)}


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
    add(k, 'grip', (.11, V((.02, -.46, 1.22))), (.22, V((-.08, -.4, 1.0))), (.30, V((.02, -.44, 1.02))), (.36, V((.28, -.34, 1.36))), (.41, V((.46, -.06, 1.74))), (.47, V((.5, .16, 1.82))), (.58, V((.42, .3, 1.76))), (.8, V((.38, .26, 1.7))), (1.15, V((.16, -.34, 1.42))))
    add(k, 'blade', (.11, V((-.1, -.98, .15))), (.22, V((-.6, -.4, -.7))), (.30, V((-.15, -.62, -.77))), (.36, V((.75, -.55, -.05))), (.41, V((.6, .28, .75))), (.47, V((.0, .55, .84))), (.58, V((-.55, .45, .7))), (.8, V((-.45, .4, .78))), (1.15, V((.32, .02, .95))))
    add(k, 'rpole', (0., V((-.3, -1., -.6))), (.22, V((-.4, -1., -.3))), (.36, V((-.2, -1., -.6))), (.42, V((.1, -1., -.5))), (.55, V((.4, -.3, -1.))), (.85, V((.3, -.3, -1.))), (1.15, V((-.3, -1., -.6))), (1.5, V((-.3, -1., -.6))))
    add(k, 'shield_c', (.22, V((.36, .14, 1.12)) + SHIELD_SHIFT), (.42, V((.28, .3, 1.06)) + SHIELD_SHIFT), (.6, V((.24, .34, 1.08)) + SHIELD_SHIFT), (.95, V((.24, .34, 1.08)) + SHIELD_SHIFT), (1.3, V((.42, .14, 1.2)) + SHIELD_SHIFT))
    add(k, 'shield_face', (.22, V((1., .1, .05))), (.42, V((.5, .86, -.1))), (.6, V((.4, .9, -.1))), (.95, V((.4, .9, -.1))), (1.3, V((1., -.1, .05))))
    return fighter_clip(F, k, 1.5, protect=(.3, .52)), {'contact': (.32, .48, 2.1)}


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
    add(k, 'grip', (.20, V((-.04, -.26, 1.76))), (.34, V((-.1, -.22, 1.86))), (.43, V((.0, -.24, 1.96))), (.475, V((.3, -.18, 1.8))), (.51, V((.5, -.12, 1.45))), (.56, V((.52, -.06, 1.1))), (.66, V((.5, .0, .9))), (.95, V((.48, .0, .94))), (1.32, V((.16, -.34, 1.42))))
    add(k, 'blade', (.20, V((-.6, .0, .8))), (.34, V((-.88, .1, -.3))), (.43, V((-.3, -.05, .95))), (.475, V((.85, -.03, .5))), (.51, V((.95, 0., -.3))), (.56, V((.42, .05, -.9))), (.66, V((.12, .06, -.99))), (.95, V((.15, .05, -.98))), (1.32, V((.32, .02, .95))))
    add(k, 'rpole', (0., V((-.3, -1., -.6))), (.34, V((-.2, -.8, 1.))), (.45, V((-.1, -.9, .6))), (.52, V((-.1, -1., -.4))), (.66, V((-.2, -.6, -1.))), (1.32, V((-.3, -1., -.6))), (1.75, V((-.3, -1., -.6))))
    add(k, 'shield_c', (.34, V((.36, .16, 1.24)) + SHIELD_SHIFT), (.52, V((.24, .34, 1.08)) + SHIELD_SHIFT), (.7, V((.22, .36, 1.02)) + SHIELD_SHIFT), (1.15, V((.22, .36, 1.02)) + SHIELD_SHIFT), (1.5, V((.42, .14, 1.2)) + SHIELD_SHIFT))
    add(k, 'shield_face', (.34, V((1., .1, .05))), (.52, V((.45, .9, -.1))), (.7, V((.35, .93, -.1))), (1.15, V((.35, .93, -.1))), (1.5, V((1., -.1, .05))))
    return fighter_clip(F, k, 1.75, protect=(.4, .62)), {'contact': (.44, .6, 1.55)}


def F_Bash(F):
    """The shield driven into a foe: drawn in to his chest, then punched out with a step and his
    shoulder behind it."""
    a = F.ank
    k = guard_keys(F, (0., 1.1))
    add(k, 'hips', (.18, V((-.07, .0, -.1))), (.3, V((.12, .03, -.12))), (.45, V((.1, .03, -.11))), (.8, V((.0, .01, -.07))))
    add(k, 'yaw', (.18, -40.), (.3, -20.), (.5, -24.), (.85, -27.))
    add(k, 'chest', (.18, V((-10., 6., 0.))), (.3, V((-14., 14., 0.))), (.55, V((-8., 11., 0.))), (.85, V((-6., 8., 0.))))
    add(k, 'lfoot', (.16, V((.21, .12, a))), (.22, V((.3, .12, a + .06))), (.29, V((.38, .11, a))), (.6, V((.38, .11, a))), (.7, V((.3, .12, a + .04))), (.8, V((.21, .12, a))))
    add(k, 'shield_c', (.18, V((.3, .08, 1.24)) + SHIELD_SHIFT), (.3, V((.76, .02, 1.26)) + SHIELD_SHIFT), (.4, V((.7, .03, 1.26)) + SHIELD_SHIFT), (.75, V((.46, .07, 1.25)) + SHIELD_SHIFT))
    add(k, 'shield_face', (.18, V((1., -.3, .05))), (.3, V((1., 0., .02))), (.75, V((1., -.25, .05))))
    # (2026-10-25) The sword hand carried forward with his shoulder as he drives in (it stayed put, the arm folded back on it).
    add(k, 'grip', (.18, V((.04, -.34, 1.5))), (.3, V((.2, -.32, 1.48))), (.45, V((.2, -.32, 1.48))), (.8, V((.16, -.34, 1.42))))
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
        rig.girth = .17                    # in his robes
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
            ax = v['axis'].normalized()
            def fist(pose, p=p, ax=ax):
                # (2026-10-25) Turned about the shaft so the hand continues the forearm, from where the elbow sits; a fixed
                # palm bent his wrist back about 115 degrees.
                w0, q0 = self.on_shaft('l', p, ax, V((.1, -1., 0.)))
                E = choose_elbow(self.R, pose.H, 'l', w0, None, V((-.3, 1., -.6)), remember=False)
                want = (p - E); want = (want - ax * want.dot(ax)).normalized()
                best = None
                for k in range(24):
                    palm = rot(ax, k * 15.) @ (V((.1, -1., 0.)) - ax * V((.1, -1., 0.)).dot(ax))
                    w, q = self.on_shaft('l', p, ax, palm)
                    al = q @ self.R.palm['l'][1]
                    score = al.dot(want)
                    if best is None or score > best[0]: best = (score, w, q)
                return best[1], best[2]
            arms['l'] = {'wrist': lambda pose: fist(pose)[0], 'pole': V((-.3, 1., -.6)), 'hand': lambda pose: fist(pose)[1], 'curl': .95, 'thumb': .8}
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
        'lwrist': [(0., c0['lwrist']), (.22, V((.32, .2, 1.34))), (.34, V((.66, .08, 1.48))), (.42, V((.6, .1, 1.5))), (.75, c0['lwrist']), (1.1, c0['lwrist'])],
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
        c['lwrist'] = c['lwrist'].lerp(V((.34, .04, 1.56)), guard)
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


# The sea tear he is sealing (user 2026-10-25: "Do the animation for the wizard attempting to seal the sea world
# rupture"; docs/CHULT-TEMPLE-HEROES.md#sealing-the-sea-tear). In game he turns to face it: its centre is 2.5 m ahead on
# the floor, its rim (1.1 m) about 1.4 m ahead.
TEAR = V((2.5, 0., 0.))


def seal_state(Wz, t, tremble=1., weave=1.):
    """Sealing: braced in a wide stance leaning over the tear, the staff thrust out over it in his right hand (the gem
    above its rim), the left arm out, palm down, fingers spread, weaving slow circles as if knitting threads of light,
    trembling with the strain. Twice a loop (6 s) he grips the threads, draws them up and drives them down."""
    a = Wz.ank
    tr = lambda f, ph: (math.sin(t * f + ph) + .6 * math.sin(t * f * 2.3 + ph * 1.7)) * tremble
    br = math.sin(t * 2 * math.pi / 3.)
    # The press: twice a loop, a gather (up, the fist closing) and a drive (down, the palm opening). 0..1 envelopes.
    ph = (t % 3.) / 3.
    gather = smooth((ph - .45) / .17) * (1. - smooth((ph - .62) / .06))
    drive = smooth((ph - .62) / .06) * (1. - smooth((ph - .74) / .2))
    # The weave: the hand circling about its reach, 0.5 Hz, the circle tilted to the floor beneath it.
    w = 2 * math.pi * t / 2. if weave else 0.
    circle = V((.05 * math.sin(w), .09 * math.cos(w), .05 * math.sin(w))) * weave * (1. - gather - drive)
    hips = V((.07, .0, -.09 - .045 * drive + .015 * gather))
    lw = V((.6, .24, 1.24)) + circle + V((-.12 * gather, 0., .14 * gather)) + V((.12 * drive, 0., -.14 * drive))
    lw += V((tr(13., .7), tr(10., .1), tr(12., 1.3))) * .004
    return dict(hips=hips, yaw=4., chest=V((-6., 10. + 1.2 * br + 7. * drive - 4. * gather, 0.)), pitch=8. + 4. * drive,
                lfoot=V((.27, .2, a)), rfoot=V((-.22, -.2, a)), lfoot_yaw=8., rfoot_yaw=-34.,
                grip=V((.4, -.37, 1.28)) + V((.08 * drive - .03 * gather, 0., -.04 * drive + .04 * gather))
                     + V((tr(9., .3), tr(11., 1.), tr(8., 2.))) * .003,
                # The staff held up and out, the gem high and leaning over the tear; it stabs toward it in the drive.
                axis=V((.42 + .3 * drive - .1 * gather, .04, .9 - .2 * drive)), spalm=V((-.3, .9, .2)),
                lmode=1., lwrist=lw,
                # The left palm pushed toward the tear, fingers up and spread; it turns down to grip in the gather.
                lalong=V((.64 + .2 * gather, .06 + .08 * math.cos(w) * weave, .76 - .4 * gather)),
                lpalm=V((.76 - .5 * gather, -.05, -.65 - .3 * gather)),
                lcurl=.06 + .05 * math.sin(t * 3.1) + .75 * gather - .06 * drive,
                look=TEAR + V((-.4, .1, .0)))


def W_Seal(Wz):
    """Sealing the sea tear: the loop he works in while nothing comes at him (6 s, two gathers and drives)."""
    n = int(6. * FPS); poses = []
    for i in range(n):
        t = i / FPS
        poses.append(body_pose(Wz.R, Wz.state(t, seal_state(Wz, t))))
    return poses, {'loops': True}


def W_SealStrain(Wz):
    """The tear surges: he is wrenched toward it a stumbling step, the left hand dragged down, the staff dipping;
    then he heaves back, the fist clenched to his chest, leaning away, the back foot braced, and works back into
    the seal (1.8 s)."""
    def st(t, v):
        c = seal_state(Wz, t, tremble=1.6, weave=0.)
        for k in v: c[k] = v[k]
        return Wz.state(t, c)
    c0 = seal_state(Wz, 0., tremble=0., weave=0.)
    c1 = seal_state(Wz, 1.8, tremble=0., weave=0.)
    lf = c0['lfoot']
    tracks = {
        'hips': [(0., c0['hips']), (.18, c0['hips'] + V((.13, .02, -.05))), (.34, c0['hips'] + V((.16, .02, -.06))), (.7, c0['hips'] + V((-.1, 0., -.04))),
                 (1.05, c0['hips'] + V((-.12, 0., -.05))), (1.5, c0['hips'] + V((-.02, 0., -.01))), (1.8, c1['hips'])],
        'chest': [(0., c0['chest']), (.18, V((-2., 24., 4.))), (.34, V((0., 26., 3.))), (.7, V((-14., -4., -3.))), (1.05, V((-16., -6., -2.))), (1.5, V((-8., 6., 0.))), (1.8, c1['chest'])],
        'pitch': [(0., c0['pitch']), (.2, 18.), (.36, 19.), (.72, -3.), (1.05, -4.), (1.5, 5.), (1.8, c1['pitch'])],
        'lwrist': [(0., c0['lwrist']), (.18, V((.86, .24, .86))), (.34, V((.9, .22, .8))), (.62, V((.42, .2, 1.18))), (.8, V((.24, .18, 1.3))),
                   (1.05, V((.22, .18, 1.32))), (1.5, V((.5, .24, 1.16))), (1.8, c1['lwrist'])],
        'lalong': [(0., c0['lalong']), (.18, V((.8, .1, -.6))), (.34, V((.75, .1, -.65))), (.7, V((.3, -.5, .3))), (1.05, V((.25, -.55, .3))), (1.5, V((.75, .05, -.4))), (1.8, c1['lalong'])],
        'lpalm': [(0., c0['lpalm']), (.34, V((.3, 0., -1.))), (.7, V((-.2, -1., 0.))), (1.05, V((-.2, -1., 0.))), (1.5, V((.25, 0., -1.))), (1.8, c1['lpalm'])],
        'lcurl': [(0., .15), (.16, .2), (.34, .3), (.55, .95), (1.1, .95), (1.4, .4), (1.8, c1['lcurl'])],
        'grip': [(0., c0['grip']), (.2, c0['grip'] + V((.1, 0., -.12))), (.36, c0['grip'] + V((.12, 0., -.14))), (.75, c0['grip'] + V((-.12, 0., .06))),
                 (1.05, c0['grip'] + V((-.14, 0., .07))), (1.5, c0['grip'] + V((-.03, 0., .01))), (1.8, c1['grip'])],
        'axis': [(0., c0['axis']), (.3, V((.95, .05, .3))), (.75, V((.5, .05, .86))), (1.05, V((.48, .05, .88))), (1.8, c1['axis'])],
        # The stumble: the left foot goes forward a short step, then back.
        'lfoot': [(0., lf), (.14, lf), (.24, lf + V((.07, 0., .05))), (.32, lf + V((.13, 0., 0.))), (.85, lf + V((.13, 0., 0.))), (1.0, lf + V((.06, 0., .045))), (1.12, lf), (1.8, lf)],
    }
    return hero_clip(Wz, tracks, 1.8, st), {'hit': .18}


WIZARD_CLIPS = {'Idle': W_Idle, 'Channel': W_Channel, 'Cast': W_Cast, 'HitLight': lambda W: W_Hit(W, False), 'HitHeavy': lambda W: W_Hit(W, True),
                'Seal': W_Seal, 'SealStrain': W_SealStrain}


# ------------------------------------------------------------------ the ranger
class Ranger(Hero):
    """A longbow (weapon_l, its nocking point on bowstring) and arrows (arrow_r in her right fingers;
    the quiver on her back). Facing +X, she draws side-on, her left shoulder to the target."""
    def __init__(self, rig):
        super().__init__(rig)
        H = rig.rest_h
        self.target = V((9., 0., 1.4))
        rig.girth = .19                 # her jerkin, shirt and baldric over her chest
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

    def bow_hand(self, grip, axis, toward, shoulder=None):
        """The bow hand and the bow (2026-10-25). The bow is mounted with its string out of the palm, so turning the hand to
        set the bow left her fingers pointing across her and the wrist bent about 100 degrees at full draw. Now the hand
        holds it as an archer does (the grip across the knuckles, the hand continuing the forearm toward the string's
        far side: its long axis away from the string), and the bow is turned on its grip to stand as keyed: the wrist,
        the hand's delta, and the bow's own delta (weapon_l, set after the arm is solved)."""
        R = self.R; H = R.rest_h
        palm, along, across = R.palm['l']
        a0 = (across).normalized()                      # the bow's axis at rest (build_temple_heroes: across)
        qb = frame_from(axis, toward) @ frame_from(a0, self.bow_toward_rest).inverted()
        # The fist round the grip may turn freely about it (the bow is posed on its own): its long axis follows the arm,
        # from her shoulder (as near as it can, square to the bow), else away from the string.
        out = (grip - shoulder) if shoulder is not None else -toward
        n = out.length
        out = out - axis * out.dot(axis)
        # Where the arm runs nearly along the bow the square part is small and wanders: lean it on "away from the string".
        aw = -toward - axis * (-toward).dot(axis)
        out = out + aw.normalized() * .12 * n
        if out.length < 1e-4: out = -toward
        qh = frame_from(axis, out.normalized()) @ frame_from(a0, along).inverted()
        wrist = grip - qh @ (H['weapon_l'] - H['hand_l'])
        return wrist, qh, qb

    def bow_axis(self, v):
        """The bow's keyed tilt squared to her bow arm (the arm from her shoulder to the grip), so the fist round it holds
        a straight wrist (2026-10-25)."""
        a = v['baxis'].normalized(); f = (v['bgrip'] - self.shoulder_l(v)).normalized()
        a = a - f * a.dot(f)
        return a.normalized() if a.length > 1e-4 else v['baxis'].normalized()

    def shoulder_l(self, v, s='l'):
        """About where her shoulder is for these hips and this yaw (the chest's turn is small beside them)."""
        H = self.R.rest_h
        return H['pelvis'] + v['hips'] + rot(Z, v['yaw'] + v['chest'].x) @ (H[f'upperarm_{s}'] - H['pelvis'])

    def quiver(self, pose):
        """Where the quiver's mouth is now (carried by her chest)."""
        H = pose.H; R = self.R
        return H['spine_03'] + pose.D['spine_03'] @ (self.quiver_top - R.rest_h['spine_03'])

    def state(self, t, v):
        a = self.ank
        wl, ql, qb = self.bow_hand(v['bgrip'], self.bow_axis(v), v['btoward'].normalized(), self.shoulder_l(v))
        # The fist's turn about the grip from her real shoulder in this pose (the estimate above sets the bow's tilt).
        def bow_fist(pose, v=v):
            # Where the elbow sits for the grip (clear of her, toward the keyed pole), then the fist along that forearm.
            ax, tw = self.bow_axis(v), v['btoward'].normalized()
            w0, q0, _ = self.bow_hand(v['bgrip'], ax, tw, pose.H['upperarm_l'])
            E = choose_elbow(self.R, pose.H, 'l', w0, None, V((-.2, .5, -1.)), remember=False)
            # and the bow's tilt squared to that forearm (the fist round it then needs no sideways bend)
            f = (v['bgrip'] - E).normalized(); ax2 = ax - f * ax.dot(f)
            ax = ax2.normalized() if ax2.length > .2 else ax
            pose._bow = self.bow_hand(v['bgrip'], ax, tw, E)
            return pose._bow
        arms = {'l': {'wrist': lambda pose: bow_fist(pose)[0], 'pole': V((-.2, .5, -1.)), 'hand': lambda pose: bow_fist(pose)[1], 'curl': 1., 'thumb': .7}}
        R = self.R; H0 = R.rest_h
        qr = self.hand_q('r', v['ralong'].normalized(), v['rpalm'].normalized())
        nock_off = H0['arrow_r'] - H0['hand_r']
        hq = qr
        if v.get('rquiver', 0.) > 0.:
            k = v['rquiver']
            # (2026-10-25) The quiver's mouth is behind her head: she takes an arrow by its fletching, which stands above
            # it over her right shoulder, the hand continuing her arm (it used to go through her neck and ponytail).
            def reach(pose, k=k, v=v):
                at = self.quiver(pose) + pose.D['spine_03'] @ V((0., -.1, .14))
                al = v['ralong'].normalized().lerp((at - pose.H['upperarm_r']).normalized(), k).normalized()
                q = self.hand_q('r', al, v['rpalm'].normalized())
                return at, q
            def wrist_q(pose, k=k, v=v):
                at, q = reach(pose)
                return v['rwrist'].lerp(at - q @ nock_off, k)
            def hand_q(pose):
                return reach(pose)[1]
            rw, hq = wrist_q, hand_q
        else:
            rw = v['rwrist']
        arms['r'] = {'wrist': rw, 'pole': v.get('rpole', V((-.6, -.6, -.2))), 'hand': hq, 'curl': v.get('rcurl', .55), 'thumb': .5}
        drawn, arrow = v.get('drawn', 0.), v.get('arrow', 1.)
        def post(pose, drawn=drawn, arrow=arrow, v=v, qb=qb):
            # The bow turned on its grip to stand as keyed (bow_hand), at the tilt the fist was set for.
            qb = getattr(pose, '_bow', (None, None, qb))[2]
            pose.turn('weapon_l', qb @ pose.D['weapon_l'].inverted()); pose.fk()
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
                bgrip=V((.34, -.34, 1.1)), baxis=V((.45, .05, .9)), btoward=V((-.6, -.6, .2)),   # out before her middle: both arms clear of her jerkin
                rwrist=None, ralong=V((.6, .3, -.5)), rpalm=V((-.5, .8, .2)), rcurl=.6,   # rwrist on the string: ready_hands puts it at the nock
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
        wl, ql, qb = Rg.bow_hand(v['bgrip'], Rg.bow_axis(v), v['btoward'].normalized(), Rg.shoulder_l(v))
        nock = v['bgrip'] + qb @ (Rg.R.rest_h['bowstring'] - Rg.R.rest_h['weapon_l'])
        if v.get('ralong_auto', True):
            # The hand on the string continues her arm from the shoulder (2026-10-25).
            v['ralong'] = (nock - Rg.shoulder_l(v, 'r')).normalized()
        qr = Rg.hand_q('r', v['ralong'].normalized(), v['rpalm'].normalized())
        v['rwrist'] = nock - qr @ (Rg.R.rest_h['arrow_r'] - Rg.R.rest_h['hand_r'])
    return v


def R_Shoot(Rg):
    """One shot and the next arrow: up and draw to the corner of her jaw, hold, loose (the string
    snaps forward, the drawing hand flies back past her ear), then over her shoulder to the quiver,
    the arrow brought round to the bow and nocked, back to ready (2.6 s; the loose at 1.0 s)."""
    a = Rg.ank
    r0 = ready_hands(Rg, ranger_ready(Rg, 0., breath=False))
    nock0 = r0['rwrist']
    full = dict(bgrip=V((.62, .04, 1.43)), baxis=V((.0, -.12, 1.)), btoward=V((-1., -.05, 0.)))
    anchor = V((-.01, -.19, 1.47))   # (2026-10-25: out from under her jaw; the hand had sunk into it)
    tracks = {
        'hips': [(0., r0['hips']), (.3, V((0., 0., -.02))), (2.2, V((0., 0., -.02))), (2.6, r0['hips'])],
        'yaw': [(0., r0['yaw']), (.3, -78.), (2.2, -78.), (2.6, r0['yaw'])],
        'chest': [(0., r0['chest']), (.3, V((-8., 2., 0.))), (.7, V((-10., -2., 0.))), (1.0, V((-10., -2., 0.))), (1.15, V((-12., -3., 0.))), (1.5, V((-14., 6., 0.))), (1.9, V((-6., 4., 0.))), (2.2, V((-8., 2., 0.))), (2.6, r0['chest'])],
        'bgrip': [(0., r0['bgrip']), (.32, full['bgrip']), (1.0, full['bgrip']), (1.06, full['bgrip'] + V((.03, -.01, -.01))), (1.35, full['bgrip'] + V((.0, 0., -.04))), (1.8, V((.56, .06, 1.3))), (2.15, V((.54, .06, 1.28))), (2.6, r0['bgrip'])],
        'baxis': [(0., r0['baxis']), (.32, full['baxis']), (1.35, full['baxis']), (1.8, V((.3, -.1, .95))), (2.15, V((.3, -.1, .95))), (2.6, r0['baxis'])],
        'btoward': [(0., r0['btoward']), (.32, full['btoward']), (1.35, full['btoward']), (1.8, V((-.8, -.5, 0.))), (2.6, r0['btoward'])],
        # the right hand: on the string (drawn), to the anchor, the loose, to the quiver, back to the bow
        'rwrist': [(0., nock0), (.16, V((.4, -.38, 1.3))), (.32, V((.48, -.06, 1.4))), (.72, anchor), (1.0, anchor), (1.05, anchor + V((-.1, -.04, .0))), (1.3, anchor + V((-.13, -.06, -.02))),
                   # out past her right shoulder to the quiver and back (2026-10-25: it went through her head and ponytail)
                   # (side-on, her right is -X and her back +Y)
                   (1.42, V((-.3, -.18, 1.56))), (1.5, V((-.28, -.1, 1.66))), (1.75, V((-.28, -.1, 1.66))), (1.9, V((-.26, -.3, 1.48))), (2.1, V((.26, -.44, 1.26))), (2.6, nock0)],
        'rquiver': [(0., 0.), (1.3, 0.), (1.55, 1.), (1.72, 1.), (1.95, 0.), (2.6, 0.)],
        'ralong': [(0., r0['ralong']), (.32, V((1., .1, -.1))), (1.0, V((1., .1, -.1))), (1.05, V((.6, .2, .3))), (1.3, V((.1, .1, .99))), (1.5, V((-.25, .45, .85))), (1.75, V((-.25, .45, .85))), (1.95, V((.2, .1, .97))), (2.1, V((.8, .2, -.3))), (2.6, r0['ralong'])],
        'rpalm': [(0., r0['rpalm']), (.32, V((0., 1., 0.))), (1.0, V((0., 1., 0.))), (1.5, V((-.95, .1, .2))), (1.75, V((-.95, .1, .2))), (2.1, V((-.4, .9, .2))), (2.6, r0['rpalm'])],
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
            else:
                # Seven frames through it, larger, from his left front (where the sea tear is, ahead of him).
                n = len(poses); fr = [1 + int(i * (n - 1) / 6) for i in range(7)]
                review_keys(rig, clip, fr, view=((1.6, -3.4, 1.35), (86, 0, 22)))
                review_keys(rig, clip, fr, tag='keys_left', view=((2.2, 3.2, 1.6), (80, 0, 146)))
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
