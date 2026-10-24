"""Blender 4.5.14: does the fighter's shield pass through him? (Claude, 2026-10-23; docs/CHULT-TEMPLE-HEROES.md)

blender --background --python Tools/check_hero_shield.py [-- Clip,Clip] [--every N]
  -> prints CHUCK_SHIELD_CLIP <clip> frames=<n> hit_frames=<k> worst=<frame> pieces=<piece:frames ...>
     and its smoothness: the largest frame-to-frame move of the shield's centre (cm) and turn (degrees)
     and CHUCK_SHIELD_CHECK failures=<clips with any hit>

Each clip is solved exactly as build_hero_anims.py solves it, keyed on the imported rig, and every frame's skinned
meshes are tested triangle against triangle: the shield's face and rim against everything else he wears or holds
(body, plate, mail, gambeson, beard, helmet, the left forearm under it, the sword). The straps are left out: they
wrap the forearm by design. The sword (blade, guard, pommel) is tested the same way against him, less the fist holding
it (CHUCK_SWORD_CLIP).
"""
import importlib.util
import math
import sys
from pathlib import Path
import bpy
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('hero_anims', ROOT / 'Tools/build_hero_anims.py')
ha = importlib.util.module_from_spec(spec); spec.loader.exec_module(ha)
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
EVERY = int(argv[argv.index('--every') + 1]) if '--every' in argv else 1
ONLY = next((a.split(',') for a in argv if a in ha.FIGHTER_CLIPS or ',' in a), None)

SHIELD = ('shield_face', 'shield_rim')
SWORD = ('sword_blade', 'sword_guard', 'sword_pommel')
SWORD_SKIP = ('sword_', 'glove_r', 'handplate_r', 'cuff_r')   # the sword itself and the fist holding it
SKIP = ('shield_', 'Human.eye', 'Human.high-poly', '_rivets')   # the shield itself, eyebrows/lashes, rivets (inside their rims)


def tree(obj, dg, keep=False):
    ev = obj.evaluated_get(dg); me = ev.to_mesh(); mw = obj.matrix_world
    verts = [mw @ v.co for v in me.vertices]; polys = [tuple(p.vertices) for p in me.polygons]
    t = BVHTree.FromPolygons(verts, polys, all_triangles=False)
    ev.to_mesh_clear()
    return (t, verts, polys) if keep else t


rig = ha.Rig('Fighter'); F = ha.Fighter(rig)
bpy.context.scene.render.fps = ha.FPS
meshes = [c for c in rig.obj.children if c.type == 'MESH']
shield = [c for c in meshes if c.name in SHIELD]
others = [c for c in meshes if not any(k in c.name for k in SKIP)]
sword = [c for c in meshes if c.name in SWORD]
body_for_sword = [c for c in others if not any(k in c.name for k in SWORD_SKIP)]
failures = sword_failures = 0
for clip, make in ha.FIGHTER_CLIPS.items():
    if ONLY and clip not in ONLY: continue
    poses, _ = make(F)
    ha.key(rig, poses, f'check_{clip}')
    hits, worst, worst_n = {}, -1, 0
    sword_hits = {}
    prev, step_cm, step_deg = None, 0., 0.
    frames = list(range(1, len(poses) + 1, EVERY))
    for f in frames:
        bpy.context.scene.frame_set(f)
        m = rig.obj.matrix_world @ rig.obj.pose.bones['shield_l'].matrix
        if prev is not None and EVERY == 1:
            step_cm = max(step_cm, (m.translation - prev.translation).length * 100)
            a = math.degrees(m.to_quaternion().rotation_difference(prev.to_quaternion()).angle); step_deg = max(step_deg, min(a, 360 - a))
        prev = m.copy()
        dg = bpy.context.evaluated_depsgraph_get()
        st = [tree(o, dg) for o in shield]
        n_frame = 0
        for o in others:
            t = tree(o, dg)
            n = sum(len(s.overlap(t)) for s in st)
            if n: hits.setdefault(o.name, []).append(f); n_frame += n
        if n_frame > worst_n: worst, worst_n = f, n_frame
        sw = [tree(o, dg) for o in sword]
        fist = rig.obj.matrix_world @ rig.obj.pose.bones['hand_r'].head
        for o in body_for_sword:
            t, verts, polys = tree(o, dg, keep=True)
            # contacts within 15 cm of the sword hand are the grip (pommel and guard at the wrist), not a crossing
            far = [j for x in sw for _, j in x.overlap(t) if (sum((verts[k] for k in polys[j]), verts[polys[j][0]] * 0) / len(polys[j]) - fist).length > .15]
            if far: sword_hits.setdefault(o.name, []).append(f)
    hit_frames = sorted({f for fl in hits.values() for f in fl})
    failures += bool(hit_frames)
    pieces = ' '.join(f'{k}:{len(v)}' for k, v in sorted(hits.items(), key=lambda kv: -len(kv[1])))
    print(f'CHUCK_SHIELD_CLIP {clip} frames={len(frames)} hit_frames={len(hit_frames)} worst={worst} tris={worst_n} pieces={pieces or "-"} step_cm={step_cm:.1f} step_deg={step_deg:.1f} at={hit_frames[:40]}')
    sword_frames = sorted({f for fl in sword_hits.values() for f in fl})
    spieces = ' '.join(f'{k}:{len(v)}' for k, v in sorted(sword_hits.items(), key=lambda kv: -len(kv[1])))
    print(f'CHUCK_SWORD_CLIP {clip} hit_frames={len(sword_frames)} pieces={spieces or "-"} at={sword_frames[:40]}')
    sword_failures += bool(sword_frames)
    rig.obj.animation_data.action = None
print(f'CHUCK_SHIELD_CHECK failures={failures} sword_failures={sword_failures}')
