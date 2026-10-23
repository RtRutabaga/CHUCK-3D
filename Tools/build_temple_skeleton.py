"""Blender 4.5.14: the temple skeletons (Claude, 2026-10-21; docs/CHULT-TEMPLE-SKELETONS.md).

User: "create the skeleton enemies in temple, use free resources where helpful,
make them look good and have realistic attack motions".

Mesh: the realistic skeleton from Blender Studio's "Human Base Meshes" bundle
v1.4.1 (CC0; the skeleton is 1d_inc's model, cleaned and UV-unwrapped by
Julien Kaspar; downloaded with the user's approval on 2026-10-21). Its
"Skeleton - Realistic" collection is kept unmodified in
SourceAssets/Enemies/Skeleton/Source/skeleton_realistic.blend (--extract).
It is 136 separate bone pieces in an FK hierarchy whose pivots sit on the
joints, so:
- the rig is built on those pivots with the humans' bone names (MPFB
  game_engine / Unreal mannequin), so Tools/build_npc_mocap.py's retargeting
  works on it unchanged;
- every piece is bound rigidly to one bone (bones do not bend); the mandible
  and lower teeth get a jaw bone;
- the pieces' mirror and subdivision modifiers are applied at a budget
  (LEVELS), then the joined mesh's ambient occlusion is baked into a colour
  attribute (crevices, the ribcage's inside, the eye sockets).

blender --background --factory-startup --python Tools/build_temple_skeleton.py -- [--extract BUNDLE.blend] [--takes DIR] [--review DIR] [--no-clips]
  -> SourceAssets/Enemies/Skeleton/SK_TempleSkeleton.fbx
     SourceAssets/Enemies/Skeleton/Anim/AS_Skeleton_<Clip>.fbx (CMU motion capture, CLIPS)
     SourceAssets/Enemies/Skeleton/skeleton-clips.json
"""
from pathlib import Path
import importlib.util
import json
import math
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets/Enemies/Skeleton'
SRC = OUT / 'Source/skeleton_realistic.blend'
COLLECTION = 'Skeleton - Realistic'
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
def arg(name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default
REVIEW = arg('--review')
TAKES = Path(arg('--takes', ROOT / 'SourceAssets/Mocap/CMU'))
CM = .01

# Subdivision applied per piece (the source's viewport levels are 1-2). The
# skull's cage is already dense; small hand and foot bones keep their cage.
SMALL = ('distal.', 'middle.', 'proximal.', 'metacarpal.', 'metatarsal.', 'coccygeal', 'leg_patella', 'foot_calcaneus', 'foot_talus')
def level(name):
    if name == 'skull': return 0
    if name.startswith(SMALL): return 0
    return 1
# Then a collapse decimation (keeps the smoothed silhouette): about 90k triangles in all.
def ratio(name):
    if name == 'skull': return .55
    if name.startswith(SMALL): return 1.
    return .4

# Piece -> rig bone. Vertebrae go to the nearest spine bone; the ribcage and
# sternum ride on spine_03, the scapula on the clavicle.
def bone_for(name):
    side = name[-1].lower() if name.endswith(('.L', '.R')) else ''
    base = name[:-2] if side else name
    if base in ('hip', 'sacral', 'coccygeal', 'spine_lumbar_l5', 'spine_lumbar_l4.5'): return 'pelvis'
    if base in ('spine_lumbar_l4', 'spine_lumbar_l3.5', 'spine_lumbar_l3'): return 'spine_01'
    if base.startswith('spine_lumbar'): return 'spine_02'
    if base.startswith('spine_thoracic') or base == 'ripcage': return 'spine_03'
    if base == 'spine_cervical_c1': return 'head'
    if base.startswith('spine_cervical'): return 'neck_01'
    if base == 'skull': return 'head'   # the mandible and lower teeth are split off to 'jaw'
    if base in ('clavicle', 'shoulder_scapula'): return f'clavicle_{side}'
    if base == 'arm_humerus': return f'upperarm_{side}'
    if base in ('arm_radius', 'arm_ulna'): return f'lowerarm_{side}'
    if base == 'hand_center' or (base.startswith('metacarpal.') and base != 'metacarpal.thumb'): return f'hand_{side}'
    if base == 'leg_femur': return f'thigh_{side}'
    if base in ('leg_tibula', 'leg_fibula', 'leg_patella'): return f'calf_{side}'
    if base in ('foot_talus', 'foot_calcaneus', 'foot_center') or base.startswith('metatarsal.'): return f'foot_{side}'
    seg, digit = base.split('.')
    if digit.isdigit(): return f'ball_{side}'   # toes
    finger = {'thumb': 'thumb', 'index': 'index', 'middle': 'middle', 'ring': 'ring', 'little': 'pinky'}[digit]
    if digit == 'thumb':
        return f'thumb_{ {"metacarpal": "01", "proximal": "02", "distal": "03"}[seg] }_{side}'
    return f'{finger}_{ {"proximal": "01", "middle": "02", "distal": "03"}[seg] }_{side}'

PARENT = {'pelvis': 'Root', 'spine_01': 'pelvis', 'spine_02': 'spine_01', 'spine_03': 'spine_02', 'neck_01': 'spine_03',
          'head': 'neck_01', 'jaw': 'head'}
for s in 'lr':
    PARENT.update({f'clavicle_{s}': 'spine_03', f'upperarm_{s}': f'clavicle_{s}', f'lowerarm_{s}': f'upperarm_{s}',
                   f'hand_{s}': f'lowerarm_{s}', f'thigh_{s}': 'pelvis', f'calf_{s}': f'thigh_{s}', f'foot_{s}': f'calf_{s}',
                   f'ball_{s}': f'foot_{s}'})
    for f in ('thumb', 'index', 'middle', 'ring', 'pinky'):
        PARENT.update({f'{f}_01_{s}': f'hand_{s}', f'{f}_02_{s}': f'{f}_01_{s}', f'{f}_03_{s}': f'{f}_02_{s}'})

# Motion capture (CMU, B. Hahne's BVH release; SourceAssets/Mocap/CMU, Tools/Fetch-CMUMocap.ps1):
# clip: (take, seconds to skip after the T-pose, seconds to keep or None, loops).
CLIPS = {
    'Idle': ('140_06', 4.0, 6.0, True),      # "Idle": crouched and ready, hands open in front (dropped for the townsfolk; right for this)
    'Walk': ('74_01', .5, None, 'walk'),     # "stiff walk": upright, rigid, head forward - purposeful, not a shamble
    'Kick': ('74_03', 1.45, 1.15, False),    # "kick": weight back, a short front kick low along the floor
    'Grab': ('139_06', 1.0, 1.9, False),     # "Poking Around On The Ground": drops into a crouch, both hands down on the floor ahead
    'Swat': ('76_05', 3.4, 1.8, False),      # "swatting at pesky bug": bent double, a hand sweeps down at the ground
    'Rise': ('140_08', 1.6, 4.2, False),     # "Get Up From Ground Laying on Back": from lying flat to standing
    'Collapse': ('113_08', 1.0, 4.5, False), # "Lay down and get up" (the zombie's fall): knees go, down onto the floor, flat on its back; played fast
    # The sanctum's skeletons fight the heroes (2026-10-22, docs/CHULT-TEMPLE-HEROES.md): a blow at a standing man.
    'Strike': ('02_05', .9, 1.3, False),     # "punch/strike": weight onto the front foot, an arm driven out at chest height
}
# Each attack's striking end (tips of the middle fingers and of the toes) and
# how each one's moment of contact is found (measure).
EFFECTORS = {'Kick': ('ball_l', 'ball_r'), 'Grab': ('middle_03_l', 'middle_03_r'), 'Swat': ('middle_03_l', 'middle_03_r'), 'Strike': ('middle_03_l', 'middle_03_r')}
STRIKE = {'Kick': 'reach', 'Grab': 'low', 'Swat': 'low', 'Strike': 'far'}
MEASURED = {}


def measure(clip, rig, frames):
    scene = bpy.context.scene; mw = rig.matrix_world
    MEASURED[clip] = {'seconds': frames / 30.}
    if clip not in EFFECTORS: return
    # The strike, measured from the striking ends (STRIKE): 'reach' = the frame
    # furthest ahead below hip height; 'low' = the first frame within 4 cm of
    # the lowest it gets in front of the hips.
    track = []
    for f in range(1, frames + 1):
        scene.frame_set(f)
        for b in EFFECTORS[clip]:
            tip = (mw @ rig.pose.bones[b].tail).copy()
            if (tip.z < .6 or STRIKE[clip] == 'far') and tip.x > .05: track.append((f, b, tip))
    if STRIKE[clip] in ('reach', 'far'):
        f, b, tip = max(track, key=lambda t: t[2].x)
    else:
        low = min(t[2].z for t in track)
        f, b, tip = min((t for t in track if t[2].z < low + .04), key=lambda t: t[0])
    reach, z, side = tip.x, tip.z, tip.y
    MEASURED[clip].update({'strike': (f - 1) / 30., 'reach_cm': reach * 100, 'height_cm': z * 100, 'side_cm': side * 100, 'aim_deg': math.degrees(math.atan2(side, reach)), 'effector': b})
    print('CHUCK_SKELETON_STRIKE', clip, f'strike_s={(f - 1) / 30:.3f}', f'reach_cm={reach * 100:.1f}', f'height_cm={z * 100:.1f}', f'side_cm={side * 100:.1f}', b)


def write_clip_data(manifest):
    path = ROOT / 'Unreal/Chuck3D/Source/Chuck3D/SkeletonClipData.h'
    L = ['#pragma once', '// Generated by Tools/build_temple_skeleton.py (Blender, from the exported clips). Do not edit.',
         '// Seconds, and cm in the skeleton frame (forward +X from the mean hip position of the clip).', 'namespace SkeletonClipData', '{']
    for clip, m in MEASURED.items():
        L.append(f'    constexpr float {clip}Length = {m["seconds"]:.4f}f;')
        if 'strike' in m:
            L.append(f'    constexpr float {clip}Strike = {m["strike"]:.4f}f;   // {m["effector"]} at the strike')
            L.append(f'    constexpr float {clip}Reach = {m["reach_cm"]:.2f}f;')
            L.append(f'    constexpr float {clip}Height = {m["height_cm"]:.2f}f;')
            L.append(f'    constexpr float {clip}Aim = {m["aim_deg"]:.2f}f;   // deg: the striking end is this far round to the left (+) at the strike')
    walk = manifest['clips'].get('Walk')
    if walk: L.append(f'    constexpr float WalkSpeed = {walk["speed_cm_s"]:.2f}f;   // the ground speed of the clip')
    L.append('}')
    path.write_text('\n'.join(L) + '\n', encoding='utf-8')
    print('CHUCK_SKELETON_CLIPDATA', path)


def clear():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    for block in (bpy.data.actions, bpy.data.armatures, bpy.data.meshes, bpy.data.collections, bpy.data.materials):
        for item in list(block): block.remove(item)


def extract(bundle):
    """Keep the bundle's skeleton collection, unmodified, as the project's source."""
    with bpy.data.libraries.load(bundle, link=False) as (src, dst):
        dst.collections = [c for c in src.collections if c == COLLECTION]
    col = dst.collections[0]
    SRC.parent.mkdir(parents=True, exist_ok=True)
    bpy.data.libraries.write(str(SRC), {col}, fake_user=True, compress=True)
    print('CHUCK_SKELETON_EXTRACTED', SRC, len(col.all_objects))
    for o in list(col.all_objects): bpy.data.objects.remove(o)
    bpy.data.collections.remove(col)
    for me in list(bpy.data.meshes): bpy.data.meshes.remove(me)


def load_pieces():
    with bpy.data.libraries.load(str(SRC), link=False) as (src, dst):
        dst.collections = [c for c in src.collections if c == COLLECTION]
    col = dst.collections[0]
    bpy.context.scene.collection.children.link(col)
    bpy.context.view_layer.update()
    # Our frame: facing +X (the source faces -Y), centred on the spine, feet on z=0.
    centre = col.all_objects['GEO-skeletion.hip'].matrix_world.translation.copy()
    frame = Matrix.Rotation(math.radians(90), 4, 'Z') @ Matrix.Translation(Vector((-centre.x, -centre.y, 0)))
    pieces = {}
    for o in col.all_objects:
        if o.type != 'MESH': continue
        name = o.name.replace('GEO-skeletion.', '')
        for m in o.modifiers:
            if m.type == 'SUBSURF': m.levels = level(name)
        if ratio(name) < 1:
            d = o.modifiers.new('Budget', 'DECIMATE'); d.ratio = ratio(name); d.use_collapse_triangulate = False
        dg = bpy.context.evaluated_depsgraph_get()
        me = bpy.data.meshes.new_from_object(o.evaluated_get(dg))
        M = frame @ o.matrix_world
        me.transform(M)
        if M.determinant() < 0: me.flip_normals()
        pieces[name] = {'mesh': me, 'pivot': M.translation.copy(), 'M': M}
    feet =[v.co.z for n, p in pieces.items() if any(k in n for k in ('foot_', 'metatarsal', 'proximal.1', 'proximal.5')) for v in p['mesh'].vertices]
    drop = Matrix.Translation(Vector((0, 0, -min(feet))))
    for p in pieces.values():
        p['mesh'].transform(drop); p['pivot'] = drop @ p['pivot']
    bpy.data.collections.remove(col)
    return pieces


def loose_parts(me):
    bm = bmesh.new(); bm.from_mesh(me); bm.verts.ensure_lookup_table()
    seen, parts = set(), []
    for v in bm.verts:
        if v.index in seen: continue
        stack, comp = [v], []
        seen.add(v.index)
        while stack:
            x = stack.pop(); comp.append(x.index)
            for e in x.link_edges:
                y = e.other_vert(x)
                if y.index not in seen: seen.add(y.index); stack.append(y)
        parts.append(comp)
    bm.free()
    return sorted(parts, key=len, reverse=True)


def far_point(me, origin, direction):
    """The piece's extreme along direction from origin (a tip for a bone's tail)."""
    return max((v.co for v in me.vertices), key=lambda c: (c - origin).dot(direction))


def build():
    pieces = load_pieces()
    P = lambda n: pieces[n]['pivot']
    # --- Skull: cranium + upper teeth (head), mandible + lower teeth (jaw). ---
    skull = pieces['skull']['mesh']
    parts = loose_parts(skull)
    cranium, mandible = parts[0], parts[1]
    kc, km = KDTree(len(cranium)), KDTree(len(mandible))
    for i, vi in enumerate(cranium): kc.insert(skull.vertices[vi].co, i)
    for i, vi in enumerate(mandible): km.insert(skull.vertices[vi].co, i)
    kc.balance(); km.balance()
    jaw_verts, teeth = set(mandible), set()
    for part in parts[2:]:
        c = sum((skull.vertices[i].co for i in part), Vector()) / len(part)
        teeth.update(part)
        if km.find(c)[2] < kc.find(c)[2]: jaw_verts.update(part)
    # The jaw's hinge: the top of each condyle (the mandible's rearmost upper points).
    mv = [skull.vertices[i].co for i in mandible]
    back = min(c.x for c in mv); front = max(c.x for c in mv)
    rear = [c for c in mv if c.x < back + (front - back) * .18]
    cond = [max((c for c in rear if (c.y > 0) == side), key=lambda c: c.z) for side in (True, False)]
    hinge = (cond[0] + cond[1]) * .5; hinge.y = 0
    chin = max(mv, key=lambda c: c.x - c.z * .3)

    # --- Joints. ---
    J = {'Root': Vector((0, 0, 0)), 'pelvis': P('hip'), 'spine_01': P('spine_lumbar_l4'), 'spine_02': P('spine_lumbar_l2'),
         'spine_03': P('spine_thoracic_t12'), 'neck_01': P('spine_cervical_c7'), 'head': P('spine_cervical_c1'), 'jaw': hinge}
    T = {'Root': Vector((-.3, 0, 0)), 'head': P('spine_cervical_c1') + Vector((0, 0, .12)), 'jaw': Vector((chin.x, 0, chin.z))}
    for S, s in (('L', 'l'), ('R', 'r')):
        clav = pieces[f'clavicle.{S}']['mesh']
        J[f'clavicle_{s}'] = min((v.co for v in clav.vertices), key=lambda c: abs(c.y))   # the sternal end
        J[f'upperarm_{s}'] = P(f'arm_humerus.{S}'); J[f'lowerarm_{s}'] = P(f'arm_radius.{S}'); J[f'hand_{s}'] = P(f'hand_center.{S}')
        J[f'thigh_{s}'] = P(f'leg_femur.{S}'); J[f'calf_{s}'] = P(f'leg_tibula.{S}'); J[f'foot_{s}'] = P(f'foot_talus.{S}')
        toes = [P(f'proximal.{k}.{S}') for k in range(1, 6)]
        J[f'ball_{s}'] = sum(toes, Vector()) / 5
        tip = far_point(pieces[f'distal.2.{S}']['mesh'], J[f'ball_{s}'], Vector((1, 0, 0)))
        T[f'ball_{s}'] = Vector((tip.x, J[f'ball_{s}'].y, J[f'ball_{s}'].z))
        J[f'thumb_01_{s}'] = P(f'metacarpal.thumb.{S}'); J[f'thumb_02_{s}'] = P(f'proximal.thumb.{S}'); J[f'thumb_03_{s}'] = P(f'distal.thumb.{S}')
        for f, d in (('index', 'index'), ('middle', 'middle'), ('ring', 'ring'), ('pinky', 'little')):
            J[f'{f}_01_{s}'] = P(f'proximal.{d}.{S}'); J[f'{f}_02_{s}'] = P(f'middle.{d}.{S}'); J[f'{f}_03_{s}'] = P(f'distal.{d}.{S}')
        for f, d in (('thumb', 'thumb'), ('index', 'index'), ('middle', 'middle'), ('ring', 'ring'), ('pinky', 'little')):
            a, b = J[f'{f}_02_{s}'], J[f'{f}_03_{s}']
            T[f'{f}_03_{s}'] = far_point(pieces[f'distal.{d}.{S}']['mesh'], b, (b - a).normalized())
    children = {}
    for b, p in PARENT.items(): children.setdefault(p, []).append(b)
    MAIN = {'pelvis': 'spine_01', 'spine_01': 'spine_02', 'spine_02': 'spine_03', 'spine_03': 'neck_01', 'neck_01': 'head'}
    for s in 'lr':
        MAIN.update({f'clavicle_{s}': f'upperarm_{s}', f'upperarm_{s}': f'lowerarm_{s}', f'lowerarm_{s}': f'hand_{s}',
                     f'hand_{s}': f'middle_01_{s}', f'thigh_{s}': f'calf_{s}', f'calf_{s}': f'foot_{s}', f'foot_{s}': f'ball_{s}'})
        for f in ('thumb', 'index', 'middle', 'ring', 'pinky'):
            MAIN.update({f'{f}_01_{s}': f'{f}_02_{s}', f'{f}_02_{s}': f'{f}_03_{s}'})
    for b in PARENT:
        if b not in T: T[b] = J[MAIN[b]]
    T['Root'] = Vector((-.3, 0, 0))

    # --- Armature. ---
    arm_data = bpy.data.armatures.new('SK_TempleSkeleton_Rig')
    rig = bpy.data.objects.new('SK_TempleSkeleton_Rig', arm_data)
    bpy.context.scene.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    eb = {}
    for b in ['Root'] + [k for k in PARENT]:
        e = arm_data.edit_bones.new(b)
        e.head, e.tail = J[b], T[b]
        if (e.tail - e.head).length < .004: e.tail = e.head + Vector((0, 0, .02))
        eb[b] = e
    for b, p in PARENT.items():
        eb[b].parent = eb[p]; eb[b].use_connect = False
    # Rolls: limbs and spine with their X axis toward the body's front or side, consistently.
    for b, e in eb.items():
        e.align_roll(Vector((0, 0, 1)) if abs((e.tail - e.head).normalized().z) < .7 else Vector((1, 0, 0)))
    bpy.ops.object.mode_set(mode='OBJECT')

    # --- One mesh, each piece rigid on its bone; teeth on their own slot. ---
    bm = bmesh.new()
    groups = {}
    color_slot = []
    for name, p in sorted(pieces.items()):
        me = p['mesh']
        start = len(bm.verts)
        if name == 'skull':
            bone_of = ['jaw' if i in jaw_verts else 'head' for i in range(len(me.vertices))]
            tooth = [i in teeth for i in range(len(me.vertices))]
        else:
            bone_of = [bone_for(name)] * len(me.vertices); tooth = [False] * len(me.vertices)
        tmp = bmesh.new(); tmp.from_mesh(me)
        for f in tmp.faces: f.material_index = 1 if all(tooth[v.index] for v in f.verts) else 0
        tmp_me = bpy.data.meshes.new('tmp'); tmp.to_mesh(tmp_me); tmp.free()
        bm.from_mesh(tmp_me); bpy.data.meshes.remove(tmp_me)
        for i in range(len(me.vertices)): groups.setdefault(bone_of[i], []).append(start + i)
    body = bpy.data.meshes.new('SK_TempleSkeleton'); bm.to_mesh(body); bm.free()
    for p in pieces.values(): bpy.data.meshes.remove(p['mesh'])
    for poly in body.polygons: poly.use_smooth = True
    obj = bpy.data.objects.new('SK_TempleSkeleton', body)
    bpy.context.scene.collection.objects.link(obj)
    for b, idx in groups.items(): obj.vertex_groups.new(name=b).add(idx, 1., 'REPLACE')
    obj.parent = rig
    mod = obj.modifiers.new('Armature', 'ARMATURE'); mod.object = rig
    for n, (r, g, b) in (('Bone', (.75, .7, .58)), ('Teeth', (.8, .74, .58))):
        m = bpy.data.materials.new(n); m.use_nodes = True
        m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (r, g, b, 1)
        body.materials.append(m)
    bake_ao(obj)
    tris = sum(len(p.vertices) - 2 for p in body.polygons)
    print('CHUCK_SKELETON_MESH', f'tris={tris}', f'verts={len(body.vertices)}', f'bones={len(arm_data.bones)}',
          f'height_cm={max(v.co.z for v in body.vertices) / CM:.1f}', f'jaw_verts={len(jaw_verts)}', f'teeth_verts={len(teeth)}')
    return rig, obj


def bake_ao(obj):
    """Ambient occlusion into the colour attribute 'AO' (Cycles, CPU), in the rest pose."""
    s = bpy.context.scene
    s.render.engine = 'CYCLES'; s.cycles.device = 'CPU'; s.cycles.samples = 96
    s.world = s.world or bpy.data.worlds.new('World')
    obj.data.color_attributes.new('AO', 'BYTE_COLOR', 'POINT')
    obj.data.color_attributes.active_color = obj.data.color_attributes['AO']
    for o in bpy.context.selected_objects: o.select_set(False)
    obj.select_set(True); bpy.context.view_layer.objects.active = obj
    s.render.bake.target = 'VERTEX_COLORS'
    s.world.light_settings.distance = .06   # crevices and the inside of the ribcage and skull, not the whole body
    bpy.ops.object.bake(type='AO')
    vals = [c.color[0] for c in obj.data.color_attributes['AO'].data]
    print('CHUCK_SKELETON_AO', f'mean={sum(vals) / len(vals):.3f}', f'min={min(vals):.3f}')


def bone_textures():
    """Tileable bone surface detail (generated, seeded; no downloads):
    T_SkeletonBone.png  R: broad staining, G: fine grain, B: pits and hairline cracks, A: wear (paler, smoother)
    T_SkeletonBone_N.png  the grain, pits and cracks as a normal map."""
    import numpy as np
    rng = np.random.default_rng(20261021)
    N = 1024
    def noise(lo, hi, beta=1., aniso=(1., 1.)):
        f = np.fft.fftfreq(N)
        fx, fy = np.meshgrid(f * aniso[0], f * aniso[1])
        r = np.sqrt(fx * fx + fy * fy) * N
        amp = np.where((r >= lo) & (r <= hi), 1. / np.maximum(r, 1.) ** beta, 0.)
        field = np.real(np.fft.ifft2(np.fft.fft2(rng.standard_normal((N, N))) * amp))
        field -= field.min(); return field / max(field.max(), 1e-6)
    stain = np.clip((noise(1, 7, 1.3) - .5) * 2.2 + .5, 0, 1)
    grain = noise(30, 200, .5, (1., .25))           # fibres run along the bone (v)
    pores = np.clip((noise(80, 400, .2) - .62) * 4, 0, 1)
    # Hairline cracks: thin ridges of a warped low-frequency field, in patches.
    w = noise(2, 8, 1.)
    ridge = 1 - np.abs(noise(3, 14, 1.) * 2 - 1 + (w - .5) * .3)
    cracks = np.clip((ridge - .965) / .035, 0, 1) * np.clip((noise(1, 4, 1.) - .45) * 3, 0, 1)
    wear = np.clip((noise(2, 10, 1.) - .55) * 3, 0, 1)
    height = grain * .35 - pores * .6 - cracks * 1.
    gx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 2.5
    gy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 2.5
    nrm = np.dstack([-gx, -gy, np.ones_like(height)]); nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
    for name, rgba in (('T_SkeletonBone', np.dstack([stain, grain, np.clip(pores + cracks, 0, 1), wear])),
                       ('T_SkeletonBone_N', np.dstack([nrm * .5 + .5, np.ones_like(height)]))):
        img = bpy.data.images.new(name, N, N, alpha=True)
        img.pixels.foreach_set(np.clip(rgba, 0, 1).astype(np.float32).ravel())
        img.filepath_raw = str(OUT / f'{name}.png'); img.file_format = 'PNG'; img.save(); bpy.data.images.remove(img)
    print('CHUCK_SKELETON_TEXTURES', N)


FBX = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False, primary_bone_axis='Y',
           secondary_bone_axis='X', use_armature_deform_only=False, mesh_smooth_type='FACE', colors_type='LINEAR')


def export_mesh(rig, obj):
    OUT.mkdir(parents=True, exist_ok=True)
    for o in bpy.context.selected_objects: o.select_set(False)
    rig.select_set(True); obj.select_set(True); bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SK_TempleSkeleton.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'},
                             bake_anim=False, **FBX)
    print('CHUCK_SKELETON_FBX', OUT / 'SK_TempleSkeleton.fbx')


def review_still(rig, name):
    import os
    s = bpy.context.scene
    s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.light = 'STUDIO'; s.display.shading.color_type = 'VERTEX'
    cam = bpy.data.objects.get('Review') or bpy.data.objects.new('Review', bpy.data.cameras.new('Review'))
    if cam.name not in s.collection.objects: s.collection.objects.link(cam)
    cam.location = (4.2, -1.6, 1.0); cam.rotation_euler = (math.radians(88), 0, math.radians(69)); cam.data.lens = 50
    s.camera = cam; s.render.resolution_x, s.render.resolution_y = 500, 700
    s.render.filepath = os.path.join(REVIEW, f'{name}.png'); bpy.ops.render.render(write_still=True)


def clips(rig):
    spec = importlib.util.spec_from_file_location('npc_mocap', ROOT / 'Tools/build_npc_mocap.py')
    mocap = importlib.util.module_from_spec(spec); spec.loader.exec_module(mocap)
    mocap.TAKES, mocap.OUT, mocap.PREFIX, mocap.REVIEW = TAKES, OUT / 'Anim', 'AS_Skeleton_', REVIEW
    mocap.GROUND, mocap.AFTER = ('ball_l', 'ball_r'), measure
    bpy.context.scene.render.fps, bpy.context.scene.render.fps_base = mocap.FPS, 1.
    if REVIEW:
        bpy.context.scene.render.engine = 'BLENDER_WORKBENCH'; bpy.context.scene.display.shading.color_type = 'VERTEX'
    manifest = {'generator': 'Tools/build_temple_skeleton.py', 'source': 'SourceAssets/Mocap/CMU (CMU Graphics Lab Motion Capture Database)', 'clips': {}}
    only = arg('--only')
    for clip, (take, skip, keep, loops) in CLIPS.items():
        if only and clip not in only.split(','): continue
        manifest['clips'][clip] = mocap.retarget(rig, clip, take, skip, keep, loops)
    if not only:
        for clip, m in MEASURED.items():
            manifest['clips'][clip].update({k: (round(v, 4) if isinstance(v, float) else v) for k, v in m.items()})
        (OUT / 'skeleton-clips.json').write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
        write_clip_data(manifest)
    print('CHUCK_SKELETON_CLIPS', sorted(manifest['clips']))


if __name__ == '__main__':
    clear()
    if arg('--extract'):
        extract(arg('--extract'))
    rig, obj = build()
    export_mesh(rig, obj)
    bone_textures()
    if REVIEW: review_still(rig, 'Rest')
    if '--no-clips' not in argv: clips(rig)
