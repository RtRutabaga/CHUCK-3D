"""Blender 4.5.14: do the heroes' arms pass through their bodies, and do their wrists bend past what a wrist can?
(Claude, 2026-10-25; user: "The archer's arm goes through herself often ... the fighter often holds his sword in a way
where his hand/wrist is strangely bent"; docs/CHULT-TEMPLE-HEROES.md#arms-and-wrists.)

blender --background --python Tools/check_hero_limbs.py [-- Fighter|Wizard|Ranger ...] [--only Clip,Clip] [--every N] [--review DIR]
  -> CHUCK_LIMB_CLIP <hero> <clip> frames=<n> arm_frames=<k> arm_worst_cm=<d>@<frame> (<side>:<region>)
                                   wrist_frames=<k> flex=<min..max> dev=<min..max> worst=<frame>
     CHUCK_LIMB_CHECK failures=<clips over a limit>

Each clip is solved exactly as build_hero_anims.py solves it.
 * Arms: every frame's skinned meshes are evaluated. Each vertex belongs to the bone that weighs it most. The forearm,
   hand and fingers (each side) are tested against the torso, hips, thighs, neck and head and what is worn on them (the
   clothes, the quiver): the depth of each arm vertex inside that surface, from the nearest point and its normal. Deeper
   than ARM_TOL is a failure (contact, a hand on the cheek at full draw, is allowed).
 * Wrists: the hand's long axis (wrist to the middle knuckle) in the forearm's frame: flexion toward the palm (+) or
   extension (-), and deviation toward the thumb (radial, +) or the little finger (ulnar, -). A wrist reaches about 70
   degrees of flexion, 60 of extension, 20 radial and 35 ulnar; gripping, less looks natural. Past WRIST limits is a
   failure.
"""
import importlib.util
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('hero_anims', ROOT / 'Tools/build_hero_anims.py')
ha = importlib.util.module_from_spec(spec); spec.loader.exec_module(ha)
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
EVERY = int(argv[argv.index('--every') + 1]) if '--every' in argv else 1
ONLY = argv[argv.index('--only') + 1].split(',') if '--only' in argv else None
WHO = [a for a in argv if a in ('Fighter', 'Wizard', 'Ranger')] or ['Fighter', 'Wizard', 'Ranger']
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None
ARM_PAIRS = 6                       # triangle pairs crossing before it counts (a touch is a few)
FLEX, EXT, RAD, ULN = 62., -55., 20., -32.
ARM = ('lowerarm', 'hand', 'index', 'middle', 'ring', 'pinky', 'thumb')
TORSO = ('pelvis', 'spine', 'neck', 'head', 'clavicle', 'thigh', 'breast', 'jaw', 'quiver')


def side_of(name):
    return 'l' if name.endswith('_l') else 'r' if name.endswith('_r') else None


def region(name):
    base = name.rsplit('_', 1)[0] if side_of(name) else name
    if any(base.startswith(a) for a in ARM): return 'arm'
    if any(base.startswith(t) for t in TORSO): return 'torso'
    return None


def dominant(obj):
    """Each vertex's heaviest bone."""
    names = {g.index: g.name for g in obj.vertex_groups}
    out = []
    for v in obj.data.vertices:
        best, w = None, 0.
        for g in v.groups:
            if g.weight > w and g.group in names: best, w = names[g.group], g.weight
        out.append(best)
    return out


def wrist_angles(pose, R, s):
    """The hand's turn relative to its forearm (identity at rest), as flexion and deviation of its long axis in the
    rest hand's frame (twist about the forearm is the forearm's, by the solver)."""
    rel = pose.D[f'lowerarm_{s}'].inverted() @ pose.D[f'hand_{s}']
    palm0, along0, across0 = R.palm[s]
    x = along0.normalized(); y = (palm0 - x * palm0.dot(x)).normalized(); z = x.cross(y)
    if z.dot(across0) < 0: z = -z      # + toward the thumb (radial)
    a = rel @ x
    return math.degrees(math.atan2(a.dot(y), a.dot(x))), math.degrees(math.atan2(a.dot(z), a.dot(x)))


def check(name):
    rig = ha.Rig(name)
    bpy.context.scene.render.fps = ha.FPS
    H = None
    if name == 'Fighter':
        who = ha.Fighter(rig); clips = ha.FIGHTER_CLIPS; hands = 'r'
    elif name == 'Wizard':
        who = ha.Wizard(rig); clips = ha.WIZARD_CLIPS; hands = 'rl'
    else:
        who = ha.Ranger(rig); clips = ha.RANGER_CLIPS; hands = 'rl'
    meshes = [c for c in rig.obj.children if c.type == 'MESH' and c.vertex_groups]
    groups = {m.name: dominant(m) for m in meshes}
    fails = 0
    for clip, make in clips.items():
        if ONLY and clip not in ONLY: continue
        poses, _ = make(who)
        # Wrists from the solved poses.
        per = {}
        wf, flex_lo, flex_hi, dev_lo, dev_hi, w_worst, w_score = 0, 1e9, -1e9, 1e9, -1e9, -1, 0.
        for i, p in enumerate(poses):
            for s in hands:
                if name == 'Fighter' or s == 'r' or True:
                    fl, dv = wrist_angles(p, rig, s)
                    if s == 'r' or name != 'Fighter':
                        flex_lo, flex_hi, dev_lo, dev_hi = min(flex_lo, fl), max(flex_hi, fl), min(dev_lo, dv), max(dev_hi, dv)
                        per.setdefault(s, [1e9, -1e9, 1e9, -1e9]); r = per[s]; r[0], r[1], r[2], r[3] = min(r[0], fl), max(r[1], fl), min(r[2], dv), max(r[3], dv)
                    over = max(fl - FLEX, EXT - fl, 0.) + max(dv - RAD, ULN - dv, 0.)
                    if over > 0: wf += 1
                    if over > w_score: w_score, w_worst = over, i + 1
        # Arms against the body, on the skinned meshes.
        ha.key(rig, poses, f'limbcheck_{clip}')
        af, a_worst, a_frame, a_where = 0, 0, -1, ''
        for f in range(1, len(poses) + 1, EVERY):
            bpy.context.scene.frame_set(f)
            dg = bpy.context.evaluated_depsgraph_get()
            torso_v, torso_p, torso_names, torso_meta = [], [], [], []
            arm_tris = {'l': ([], []), 'r': ([], [])}; arm_names = {'l': [], 'r': []}; arm_meta = {'l': [], 'r': []}
            for m in meshes:
                ev = m.evaluated_get(dg); me = ev.to_mesh(); mw = m.matrix_world
                dom = groups[m.name]
                co = [mw @ v.co for v in me.vertices]
                base = len(torso_v)
                keep = {}
                for poly in me.polygons:
                    vs = poly.vertices
                    if all(dom[k] and region(dom[k]) == 'torso' for k in vs):
                        torso_p.append(tuple(base + keep.setdefault(k, len(keep)) for k in vs)); torso_names.append(f'{m.name}/{dom[vs[0]]}'); torso_meta.append((m.name, set(vs)))
                for k, i in sorted(keep.items(), key=lambda kv: kv[1]): torso_v.append(co[k])
                for poly in me.polygons:
                    vs = poly.vertices
                    d0 = dom[vs[0]]
                    if d0 and region(d0) == 'arm' and all(dom[k] and region(dom[k]) == 'arm' and side_of(dom[k]) == side_of(d0) for k in vs):
                        sd = side_of(d0); av, ap = arm_tris[sd]
                        ap.append(tuple(len(av) + j for j in range(len(vs)))); av.extend(co[k] for k in vs); arm_names[sd].append(d0); arm_meta[sd].append((m.name, set(vs)))
                ev.to_mesh_clear()
            if not torso_p: continue
            torso = BVHTree.FromPolygons(torso_v, torso_p, all_triangles=False)
            deep, where = 0, ''
            for sd in 'lr':
                av, ap = arm_tris[sd]
                if not ap: continue
                pairs = BVHTree.FromPolygons(av, ap, all_triangles=False).overlap(torso)
                # Neighbours in one garment (a sleeve meeting its body) touch at their shared edge: not a crossing.
                pairs = [(ia, it) for ia, it in pairs if not (arm_meta[sd][ia][0] == torso_meta[it][0] and arm_meta[sd][ia][1] & torso_meta[it][1])]
                if len(pairs) > deep:
                    deep = len(pairs)
                    bones = {}
                    for ia, it in pairs: bones[arm_names[sd][ia]] = bones.get(arm_names[sd][ia], 0) + 1
                    against = {}
                    for ia, it in pairs: against[torso_names[it]] = against.get(torso_names[it], 0) + 1
                    tri = ap[pairs[0][0]] if pairs else None
                    cen = sum((av[i] for ia, _ in pairs for i in ap[ia]), Vector()) / max(1, 3 * len(pairs))
                    ebone = rig.obj.pose.bones[f'lowerarm_{sd}']; elbow = rig.obj.matrix_world @ ebone.head
                    wbone = rig.obj.pose.bones[f'hand_{sd}']; wristp = rig.obj.matrix_world @ wbone.head
                    near = f' @elbow+{(cen - elbow).length * 100:.0f}cm/wrist+{(cen - wristp).length * 100:.0f}cm'
                    where = sd + ':' + ','.join(f'{k}' for k, _ in sorted(bones.items(), key=lambda kv: -kv[1])[:3]) + near + ' vs ' + ','.join(f'{k}' for k, _ in sorted(against.items(), key=lambda kv: -kv[1])[:3])
            if deep > ARM_PAIRS: af += 1
            if deep > a_worst: a_worst, a_frame, a_where = deep, f, where
        bad = af > 0 or wf > 0
        fails += bad
        print(f'CHUCK_LIMB_CLIP {name} {clip} frames={len(poses)} arm_frames={af} arm_worst_pairs={a_worst}@{a_frame} ({a_where}) '
              f'wrist_frames={wf} ' + ' '.join(f'{k}:flex={v[0]:.0f}..{v[1]:.0f},dev={v[2]:.0f}..{v[3]:.0f}' for k, v in sorted(per.items())) + f' worst={w_worst}', flush=True)
        if REVIEW and (af or wf):
            ha.REVIEW = REVIEW
            fr = sorted({max(1, a_frame), max(1, w_worst)} - {-1})
            ha.review_keys(rig, f'{clip}_limb', fr, view=((1.6, -3.4, 1.35), (86, 0, 22)))
            ha.review_keys(rig, f'{clip}_limb', fr, tag='front', view=((3.6, .4, 1.4), (88, 0, 96)))
        rig.obj.animation_data.action = None
    return fails


total = sum(check(w) for w in WHO)
print(f'CHUCK_LIMB_CHECK failures={total}')
