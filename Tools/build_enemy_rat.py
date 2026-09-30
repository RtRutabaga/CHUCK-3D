"""Blender 4.5.14: the small rat enemy (user 2026-09-30: "Chuck will also
eventually fight small enemies like regular largish rats").

blender --background --python Tools/build_enemy_rat.py [-- --review out.png]
  -> SourceAssets/Enemies/Rat/SK_Rat.fbx (skinned, no clips: the runtime poses it)

An ordinary big dock rat on all fours: 26 cm nose to rump plus a 22 cm tail,
about 11 cm at the shoulder - knee-high on 65 cm Chuck, low enough to hide in
the grass. No clothes; dark grey-brown fur, a lighter belly, pink ears, paws,
nose and tail. Chuck's conventions: centimetres, facing +X (Blender +Y is his
left, Unreal +Y right), bones along +Y, same FBX settings.

The body is blended metaball shapes (torso, shoulders, haunches, head, snout,
legs) converted to one smooth skin and weighted automatically; ears, eyes,
nose and the tail are separate pieces weighted to their bone by hand, then
joined. The skeleton is small and plain because the runtime animates it
procedurally (component-space bone deltas: scurry, sniff, lunge, flinch,
death) - no authored clips yet.
"""
from pathlib import Path
import math
import sys
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets' / 'Enemies' / 'Rat'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
for block in (bpy.data.meshes, bpy.data.metaballs, bpy.data.armatures):
    for item in list(block): block.remove(item)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = .01
MAT = bpy.data.materials.new('RatFur')
FBX = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False,
           primary_bone_axis='Y', secondary_bone_axis='X', use_armature_deform_only=False,
           mesh_smooth_type='FACE')

# ------------------------------------------------------------ skeleton
BONES = {   # name: (head, tail, parent)
    'pelvis': ((-7., 0, 7.2), (-2., 0, 7.8), None),
    'spine': ((-2., 0, 7.8), (4., 0, 8.), 'pelvis'),
    'chest': ((4., 0, 8.), (8., 0, 8.1), 'spine'),
    'neck': ((8., 0, 8.1), (10.5, 0, 8.5), 'chest'),
    'head': ((10.5, 0, 8.5), (16., 0, 7.1), 'neck'),
}
for side, y in (('L', 1.), ('R', -1.)):
    BONES[f'arm_{side}'] = ((6., 2.6 * y, 6.3), (6.8, 2.8 * y, 3.2), 'chest')
    BONES[f'forearm_{side}'] = ((6.8, 2.8 * y, 3.2), (7.3, 2.8 * y, .9), f'arm_{side}')
    BONES[f'hand_{side}'] = ((7.3, 2.8 * y, .9), (8.7, 2.8 * y, .45), f'forearm_{side}')
    BONES[f'thigh_{side}'] = ((-6.5, 3.2 * y, 6.4), (-4.6, 3.4 * y, 4.), 'pelvis')
    BONES[f'shin_{side}'] = ((-4.6, 3.4 * y, 4.), (-7.6, 3.4 * y, 1.2), f'thigh_{side}')
    BONES[f'foot_{side}'] = ((-7.6, 3.4 * y, 1.2), (-3., 3.4 * y, .45), f'shin_{side}')
TAIL = [f'tail_{i}' for i in range(6)]
tail_pts = [Vector((-11.5 - 3.7 * i, 0, 6.2 - .9 * i - .08 * i * i)) for i in range(7)]
for i, name in enumerate(TAIL):
    BONES[name] = (tuple(tail_pts[i]), tuple(tail_pts[i + 1]), 'pelvis' if i == 0 else TAIL[i - 1])

arm = bpy.data.armatures.new('Rat')
rig = bpy.data.objects.new('SK_Rat_Rig', arm); scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig; rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
for name, (h, t, _) in BONES.items():
    eb = arm.edit_bones.new(name); eb.head, eb.tail, eb.roll = h, t, 0.
for name, (_, _, parent) in BONES.items():
    if parent: arm.edit_bones[name].parent = arm.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT')

# ------------------------------------------------------------ body
# Overlapping solids, united by a voxel remesh and smoothed so the joins blend.
blobs = []


def ellipsoid(center, radii, along=None):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=14, radius=1., location=center)
    o = bpy.context.object; o.scale = radii
    if along is not None:
        o.rotation_mode = 'QUATERNION'; o.rotation_quaternion = Vector((1, 0, 0)).rotation_difference(along)
    blobs.append(o)


def limb(a, b, r):
    a, b = Vector(a), Vector(b)
    ellipsoid((a + b) / 2, ((b - a).length / 2 + r * .8, r, r), (b - a).normalized())


ellipsoid((-2.5, 0, 7.4), (8.5, 4.3, 3.9))           # barrel of the body
ellipsoid((4.5, 0, 7.6), (5., 3.6, 3.5))             # shoulders
ellipsoid((-7.5, 0, 6.9), (4.6, 4.2, 3.8))           # rump
for y in (3., -3.):
    ellipsoid((-6.3, y, 5.4), (3.8, 2.1, 3.3))       # haunches
ellipsoid((10.6, 0, 8.3), (3.9, 2.8, 2.8))           # skull
limb((11.5, 0, 8.), (15.9, 0, 7.05), 1.5)           # snout
for side in 'LR':
    for bone, r in (('arm', 1.45), ('forearm', .95), ('hand', .62), ('thigh', 1.6), ('shin', 1.05), ('foot', .62)):
        limb(BONES[f'{bone}_{side}'][0], BONES[f'{bone}_{side}'][1], r)
bpy.ops.object.select_all(action='DESELECT')
for o in blobs: o.select_set(True)
bpy.context.view_layer.objects.active = blobs[0]
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.ops.object.join()
body = bpy.context.view_layer.objects.active; body.name = 'SK_Rat'
body.data.remesh_voxel_size = .3
bpy.ops.object.voxel_remesh()
smooth = body.modifiers.new('Blend joins', 'SMOOTH'); smooth.factor = .6; smooth.iterations = 10
bpy.ops.object.modifier_apply(modifier=smooth.name)
dec = body.modifiers.new('Budget', 'DECIMATE'); dec.ratio = .35
bpy.ops.object.modifier_apply(modifier=dec.name)
bpy.ops.object.shade_smooth()
# Automatic weights for the body.
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.parent_set(type='ARMATURE_AUTO')


# ------------------------------------------------------------ parts
def part(name, verts, faces, bone_weights):
    """A separate piece weighted by bone_weights(vertex) -> {bone: w}."""
    data = bpy.data.meshes.new(name); data.from_pydata(verts, [], faces); data.update()
    obj = bpy.data.objects.new(name, data); scene.collection.objects.link(obj)
    for i, v in enumerate(verts):
        for bone, w in bone_weights(Vector(v)).items():
            group = obj.vertex_groups.get(bone) or obj.vertex_groups.new(name=bone)
            group.add([i], w, 'REPLACE')
    for p in data.polygons: p.use_smooth = True
    return obj


def sphere(center, radius, rings=8, segs=12, scale=(1, 1, 1)):
    verts, faces = [], []
    for i in range(rings + 1):
        th = math.pi * i / rings
        for j in range(segs):
            ph = math.tau * j / segs
            verts.append((center[0] + radius * scale[0] * math.sin(th) * math.cos(ph),
                          center[1] + radius * scale[1] * math.sin(th) * math.sin(ph),
                          center[2] + radius * scale[2] * math.cos(th)))
    for i in range(rings):
        for j in range(segs):
            a, b = i * segs + j, i * segs + (j + 1) % segs
            faces.append((a, b, b + segs, a + segs))
    return verts, faces


head_only = lambda v: {'head': 1.}
pieces = []
for side, y in (('L', 1.), ('R', -1.)):
    # Ear: a thin rounded cup standing up and out behind the eye.
    base = Vector((9.6, 1.9 * y, 10.2))
    verts, faces = [], []
    for i in range(7):
        for j in range(9):
            u, v = i / 6, j / 8
            a = math.pi * (v - .5)
            r = 1.55 * math.sin(math.pi * (.15 + .85 * u)) ** .6
            p = base + Vector((-.25 * u, .55 * u * y, 2.3 * u)) + Vector((r * math.sin(a) * .9, .35 * y * (1 - math.cos(a)) * r, r * math.cos(a) * .25))
            verts.append(tuple(p))
    for i in range(6):
        for j in range(8):
            a, b = i * 9 + j, i * 9 + j + 1
            faces.append((a, b, b + 9, a + 9)); faces.append((a + 9, b + 9, b, a))
    pieces.append(part(f'Ear_{side}', verts, faces, head_only))
    v, f = sphere((12.9, 1.75 * y, 8.7), .62)
    pieces.append(part(f'Eye_{side}', v, f, head_only))
v, f = sphere((16.05, 0, 7.05), .55, scale=(1, 1.1, .85))
pieces.append(part('Nose', v, f, head_only))
# Tail: a tapering tube along the tail bones.
verts, faces = [], []
rings, segs = 24, 8
for i in range(rings + 1):
    u = i / rings
    k = min(5, int(u * 6)); w = u * 6 - k
    c = tail_pts[k].lerp(tail_pts[k + 1], w) if k < 6 else tail_pts[6]
    r = 1.05 * (1 - u) + .18 * u
    for j in range(segs):
        a = math.tau * j / segs
        verts.append((c.x, c.y + r * math.cos(a), c.z + r * math.sin(a)))
for i in range(rings):
    for j in range(segs):
        a, b = i * segs + j, i * segs + (j + 1) % segs
        faces.append((a, b, b + segs, a + segs))
def tail_weights(v):
    t = min(5.999, max(0., (-11.5 - v.x) / 3.7))
    k = int(t); w = t - k
    out = {TAIL[k]: 1 - w}
    if k + 1 < 6: out[TAIL[k + 1]] = w
    elif v.x > -12.5: out = {'pelvis': 1.}
    return out
pieces.append(part('Tail', verts, faces, tail_weights))

bpy.ops.object.select_all(action='DESELECT')
for p in pieces: p.select_set(True)
body.select_set(True); bpy.context.view_layer.objects.active = body
bpy.ops.object.join()
body = bpy.context.view_layer.objects.active

# ------------------------------------------------------------ fur colour
FUR = (.040, .034, .028); BACK = (.028, .024, .020); BELLY = (.10, .088, .072)
PINK = (.34, .17, .16); EYE = (.008, .007, .007); TAILC = (.20, .13, .12)
col = body.data.color_attributes.new('Col', 'FLOAT_COLOR', 'CORNER')
vgroups = {g.index: g.name for g in body.vertex_groups}
for poly in body.data.polygons:
    for li in poly.loop_indices:
        vi = body.data.loops[li].vertex_index
        v = body.data.vertices[vi]
        p, n = v.co, v.normal
        names = {vgroups[g.group]: g.weight for g in v.groups if g.weight > .3}
        if any(k.startswith('tail') for k in names) and p.x < -12.: c = TAILC
        elif p.x > 15.3 and p.z < 7.8: c = PINK                                         # nose
        elif abs(p.x - 12.9) < .8 and abs(abs(p.y) - 1.75) < .8 and abs(p.z - 8.7) < .8 and n.x > -.2 and abs(n.y) > .3: c = EYE
        elif p.z > 10.1 and 8.3 < p.x < 11.: c = PINK if n.x > -.1 else FUR          # ears: pink inside
        elif p.z < 1.35: c = PINK                                                      # paws
        elif n.z < -.45 and p.z < 6.8: c = BELLY
        else: c = BACK if n.z > .6 else FUR
        col.data[li].color = (*c, 1.)
# Material slots per region (each face takes its first corner's class): the
# skeletal import doesn't carry vertex colour reliably, slots always import.
SLOTS = {FUR: 'Fur', BACK: 'Fur', BELLY: 'Belly', PINK: 'Pink', EYE: 'Eye', TAILC: 'Tail'}
names = ['Fur', 'Belly', 'Pink', 'Eye', 'Tail']
body.data.materials.clear()
for n in names: body.data.materials.append(bpy.data.materials.get(n) or bpy.data.materials.new(n))
for poly in body.data.polygons:
    c = tuple(round(x, 4) for x in col.data[poly.loop_indices[0]].color[:3])
    key = next((k for k in SLOTS if tuple(round(x, 4) for x in k) == c), FUR)
    poly.material_index = names.index(SLOTS[key])

# ------------------------------------------------------------ export
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); body.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.export_scene.fbx(filepath=str(OUT / 'SK_Rat.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'},
                         bake_anim=False, colors_type='LINEAR', **FBX)
dims = body.dimensions
print('CHUCK_RAT_READY', f'verts={len(body.data.vertices)}', f'tris={sum(len(p.vertices) - 2 for p in body.data.polygons)}',
      f'size=({dims.x:.1f},{dims.y:.1f},{dims.z:.1f})', f'bones={len(BONES)}',
      f'groups={len(body.vertex_groups)}')

if '--review' in sys.argv:
    review = sys.argv[sys.argv.index('--review') + 1]
    bpy.ops.mesh.primitive_plane_add(size=400)
    # Chuck-height marker (65 cm) and a knee marker (20 cm) for scale.
    bpy.ops.mesh.primitive_cylinder_add(radius=.6, depth=65, location=(0, 30, 32.5))
    bpy.ops.mesh.primitive_cylinder_add(radius=.6, depth=20, location=(0, 26, 10))
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); scene.collection.objects.link(cam)
    cam.location = (38, -62, 26); cam.rotation_euler = (math.radians(76), 0, math.radians(30))
    cam.data.lens = 45; scene.camera = cam
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.color_type = 'VERTEX'; scene.display.shading.light = 'STUDIO'
    scene.render.resolution_x, scene.render.resolution_y = 1200, 800
    scene.render.filepath = review; bpy.ops.render.render(write_still=True)
    print('CHUCK_RAT_REVIEW', review)
