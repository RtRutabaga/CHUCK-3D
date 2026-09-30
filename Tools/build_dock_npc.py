"""Blender 4.5.14: the dock worker NPC by the spawn (user 2026-09-30: "start work
on the human NPC near spawn point, eventually we'll have more NPCs").

blender --background --python Tools/build_dock_npc.py [-- --review out.png]
  -> SourceAssets/NPCs/DockWorker/SK_DockWorker.fbx (skinned; the runtime poses it)

The 2D game's human-scale reference (References/Original/PHASE-2.md: every
human NPC matches the dock worker): 180 cm to the top of his knit cap, a
working man's build. Linen shirt with the sleeves rolled to the elbow, a
leather jerkin, belt, navy canvas trousers, boots, a rust knit cap and a few
days' stubble. Built like the rat (Tools/build_enemy_rat.py): overlapping
solids voxel-remeshed into one skin and smoothed, eyes as separate pieces,
clothing and skin as material slots by region, a plain 22-bone skeleton with
automatic weights. No authored clips: ADockNPC poses him procedurally
(breathing, weight shift, glances, turning his head to watch Chuck).
Centimetres, facing +X (Blender +Y is his left), origin between his feet.
"""
from pathlib import Path
import math
import sys
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets' / 'NPCs' / 'DockWorker'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
for block in (bpy.data.meshes, bpy.data.armatures):
    for item in list(block): block.remove(item)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = .01
FBX = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False,
           primary_bone_axis='Y', secondary_bone_axis='X', use_armature_deform_only=False, mesh_smooth_type='FACE')
SLOTS = ['Skin', 'Shirt', 'Jerkin', 'Belt', 'Trousers', 'Boots', 'Cap', 'Stubble', 'Brow', 'Eye']

# ------------------------------------------------------------ skeleton
BONES = {
    'pelvis': ((0, 0, 92), (0, 0, 100), None),
    'spine_01': ((0, 0, 100), (0, 0, 114), 'pelvis'),
    'spine_02': ((0, 0, 114), (0, 0, 128), 'spine_01'),
    'chest': ((0, 0, 128), (0, 0, 146), 'spine_02'),
    'neck': ((0, 0, 147), (1, 0, 158), 'chest'),
    'head': ((1, 0, 158), (1, 0, 178), 'neck'),
}
for side, y in (('L', 1), ('R', -1)):
    BONES[f'clavicle_{side}'] = ((1, 3 * y, 145), (-1, 18 * y, 145), 'chest')
    BONES[f'upperarm_{side}'] = ((-1, 21 * y, 145), (-2, 24 * y, 117), f'clavicle_{side}')
    BONES[f'lowerarm_{side}'] = ((-2, 24 * y, 117), (2, 25 * y, 93), f'upperarm_{side}')
    BONES[f'hand_{side}'] = ((2, 25 * y, 93), (4, 25.5 * y, 80), f'lowerarm_{side}')
    BONES[f'thigh_{side}'] = ((0, 9.5 * y, 91), (1, 10 * y, 50), 'pelvis')
    BONES[f'calf_{side}'] = ((1, 10 * y, 50), (-.5, 10 * y, 9), f'thigh_{side}')
    BONES[f'foot_{side}'] = ((-.5, 10 * y, 9), (15, 10 * y, 2), f'calf_{side}')

arm = bpy.data.armatures.new('DockWorker')
rig = bpy.data.objects.new('SK_DockWorker_Rig', arm); scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig; rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
for name, (h, t, _) in BONES.items():
    eb = arm.edit_bones.new(name); eb.head, eb.tail, eb.roll = h, t, 0.
for name, (_, _, parent) in BONES.items():
    if parent: arm.edit_bones[name].parent = arm.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT')

# ------------------------------------------------------------ body and clothes
# Lofted solids (continuous cross-sections) rather than stacked blobs: a real
# torso taper and limbs that taper smoothly through the joints.
blobs = []


def solid(name, rings):
    """rings: list of lists of points (closed loops, same count); capped ends."""
    verts, faces = [], []
    n = len(rings[0])
    for ring in rings: verts += [tuple(v) for v in ring]
    for k in range(len(rings) - 1):
        for j in range(n):
            a, b = k * n + j, k * n + (j + 1) % n
            faces.append((a, b, b + n, a + n))
    c0 = len(verts); verts.append(tuple(sum((Vector(v) for v in rings[0]), Vector()) / n))
    c1 = len(verts); verts.append(tuple(sum((Vector(v) for v in rings[-1]), Vector()) / n))
    last = (len(rings) - 1) * n
    for j in range(n):
        faces.append((c0, (j + 1) % n, j)); faces.append((c1, last + j, last + (j + 1) % n))
    data = bpy.data.meshes.new(name); data.from_pydata(verts, [], faces); data.update()
    obj = bpy.data.objects.new(name, data); scene.collection.objects.link(obj)
    blobs.append(obj)


def superellipse(cx, cy, z, rx, ry, e=2.6, n=36):
    out = []
    for j in range(n):
        a = math.tau * j / n
        c, s_ = math.cos(a), math.sin(a)
        out.append(Vector((cx + rx * math.copysign(abs(c) ** (2 / e), c), cy + ry * math.copysign(abs(s_) ** (2 / e), s_), z)))
    return out


def loft_z(name, profile, e=2.6):
    """profile: (z, centre x, half depth x, half width y)."""
    solid(name, [superellipse(cx, 0, z, rx, ry, e) for z, cx, rx, ry in profile])


def tube(name, path, n=20):
    """path: (point, radius); a smooth tapered tube with rounded ends."""
    pts = [Vector(p) for p, _ in path]
    rings = []
    for k, (p, r) in enumerate(path):
        d = (pts[min(k + 1, len(pts) - 1)] - pts[max(k - 1, 0)]).normalized()
        u = d.orthogonal().normalized(); v = d.cross(u)
        rings.append([Vector(p) + (u * math.cos(math.tau * j / n) + v * math.sin(math.tau * j / n)) * r for j in range(n)])
    # Rounded ends: shrink toward the tips.
    d0 = (pts[0] - pts[1]).normalized(); d1 = (pts[-1] - pts[-2]).normalized()
    r0, r1 = path[0][1], path[-1][1]
    start = [[pts[0] + d0 * r0 * .5 + (q - pts[0]) * .7 for q in rings[0]], [pts[0] + d0 * r0 * .85 + (q - pts[0]) * .3 for q in rings[0]]]
    end = [[pts[-1] + d1 * r1 * .5 + (q - pts[-1]) * .7 for q in rings[-1]], [pts[-1] + d1 * r1 * .85 + (q - pts[-1]) * .3 for q in rings[-1]]]
    solid(name, list(reversed(start)) + rings + end)


def ellipsoid(center, radii):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=28, ring_count=16, radius=1., location=center)
    o = bpy.context.object; o.scale = radii
    blobs.append(o)


# Torso: hips to shoulders (z, centre x, half depth, half width).
loft_z('Torso', [(82, 0, 9.5, 15), (86, 0, 11, 17.2), (92, 0, 11.6, 18), (99, .5, 10.8, 16.8), (106, 1, 10.4, 15.6),
                 (114, 1.5, 11.4, 16.4), (124, 2, 12.6, 18.4), (134, 1.5, 12.6, 19.6), (141, .5, 11.4, 20.2),
                 (145, -.3, 10, 18.2), (148.5, 0, 8.2, 13), (151.5, .3, 6.8, 8.2), (153, .5, 5, 5)])   # trapezius slope to the neck
loft_z('Belt', [(101.2, .6, 11.4, 17.2), (105.2, .9, 11.1, 16.6)], e=2.4)
tube('Neck', [((0, 0, 146), 7.6), ((.6, 0, 152), 7.0), ((1.4, 0, 159), 6.4)])
ellipsoid((.5, 0, 168.5), (10, 8.2, 10.5))             # skull
ellipsoid((4, 0, 160.8), (7, 6.6, 5))                  # jaw
tube('Nose', [((8.9, 0, 167.2), 1.3), ((11.1, 0, 164.2), 1.5)], n=12)
ellipsoid((8, 0, 169.8), (2.2, 6.2, 1.5))              # brow ridge
for y in (1, -1):
    ellipsoid((0, 8.4 * y, 166), (1.8, 1., 3.))        # ears
ellipsoid((-.5, 0, 173.5), (10.6, 9.2, 6.8))           # knit cap
ellipsoid((0, 0, 171.2), (11.2, 9.6, 2.3))             # its turned-up rim
for y in (1, -1):
    tube(f'Arm{y}', [((-.5, 17.5 * y, 143), 6.4), ((-1.2, 21.5 * y, 136), 5.6), ((-1.6, 23.2 * y, 127), 5.1),
                     ((-2, 24 * y, 118), 4.4), ((-.8, 24.4 * y, 108), 4.4), ((1, 24.8 * y, 99), 3.6), ((2, 25 * y, 94), 3.0)])
    tube(f'Cuff{y}', [((-2.1, 23.9 * y, 117.5), 5.5), ((-1.6, 24 * y, 112.5), 5.3)])
    ellipsoid((3, 25.3 * y, 87.5), (4.1, 2.1, 5.4))           # palm
    ellipsoid((3.8, 25.2 * y, 81.5), (3.7, 1.9, 3.8))         # fingers, loosely curled
    tube(f'Thumb{y}', [((4, 23.4 * y, 89), 1.2), ((6.5, 23 * y, 84.5), 1.0)], n=10)
    tube(f'Leg{y}', [((0, 9.6 * y, 90), 8.8), ((.8, 10 * y, 72), 7.6), ((1.5, 10 * y, 52), 5.8), ((-.4, 10 * y, 38), 5.9),
                     ((-.5, 10 * y, 24), 4.6), ((-.5, 10 * y, 17), 4.3)])
    tube(f'Boot{y}', [((-.5, 10 * y, 21), 5.5), ((-.5, 10 * y, 12), 5.7), ((-.3, 10 * y, 5), 5.9)])
    ellipsoid((6, 10 * y, 4), (13.5, 5.4, 4.2))               # boot foot
    ellipsoid((-4.5, 10 * y, 3), (4.4, 5, 3.2))               # heel
bpy.ops.object.select_all(action='DESELECT')
for o in blobs: o.select_set(True)
bpy.context.view_layer.objects.active = blobs[0]
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.ops.object.join()
body = bpy.context.view_layer.objects.active; body.name = 'SK_DockWorker'
body.data.remesh_voxel_size = .75   # even quads: clean clothing edges, no decimation needed
bpy.ops.object.voxel_remesh()
smooth = body.modifiers.new('Blend joins', 'SMOOTH'); smooth.factor = .5; smooth.iterations = 5
bpy.ops.object.modifier_apply(modifier=smooth.name)
bpy.ops.object.shade_smooth()
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.parent_set(type='ARMATURE_AUTO')

# Eyes: separate small dark spheres on the head.
def sphere(center, r, segs=12, rings=8):
    verts, faces = [], []
    for i in range(rings + 1):
        th = math.pi * i / rings
        for j in range(segs):
            ph = math.tau * j / segs
            verts.append((center[0] + r * math.sin(th) * math.cos(ph), center[1] + r * math.sin(th) * math.sin(ph), center[2] + r * math.cos(th)))
    for i in range(rings):
        for j in range(segs):
            a, b = i * segs + j, i * segs + (j + 1) % segs
            faces.append((a, b, b + segs, a + segs))
    return verts, faces
eyes = []
for y in (1, -1):
    v, f = sphere((8.9, 3.3 * y, 167.4), .75)
    data = bpy.data.meshes.new('Eye'); data.from_pydata(v, [], f); data.update()
    obj = bpy.data.objects.new('Eye', data); scene.collection.objects.link(obj)
    obj.vertex_groups.new(name='head').add(list(range(len(v))), 1., 'REPLACE')
    eyes.append(obj)
bpy.ops.object.select_all(action='DESELECT')
for e in eyes: e.select_set(True)
body.select_set(True); bpy.context.view_layer.objects.active = body
bpy.ops.object.join()
body = bpy.context.view_layer.objects.active

# ------------------------------------------------------------ clothing by region
body.data.materials.clear()
for n in SLOTS: body.data.materials.append(bpy.data.materials.get(n) or bpy.data.materials.new(n))
def region(c, n):
    x, y, z = c
    ay = abs(y)
    if z < 18: return 'Boots'
    if ay > 20.8 and z < 146:                       # arms
        if z > 115.5: return 'Shirt'                # sleeve
        if z > 110: return 'Shirt'                  # the rolled cuff
        return 'Skin'                               # forearm and hand
    if z < 100.5: return 'Trousers'
    if z < 105.6: return 'Belt'
    if z < 149:
        if x > 5 and ay < 2.2 + (z - 110) * .13 and z > 110: return 'Shirt'   # the jerkin's open V front
        if ay > 17.6 and z > 129: return 'Shirt'                          # the armholes: shirt at the shoulder
        return 'Jerkin'
    # Neck and head.
    if z > 171.3: return 'Cap'
    if x > 7.2 and z > 168.9 and ay > 1.2: return 'Brow'
    if x > 1.5 and 154.5 < z < 163.2 and not (x > 9 and z > 162.5) and n.z < .6: return 'Stubble'
    return 'Skin'
eye_verts = set()
for poly in body.data.polygons:
    c = poly.center
    eye = abs(c.x - 8.9) < 1.1 and abs(abs(c.y) - 3.3) < 1.1 and abs(c.z - 167.4) < 1.1 and c.x > 8.4
    poly.material_index = SLOTS.index('Eye' if eye else region(c, poly.normal))

# ------------------------------------------------------------ export
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); body.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.export_scene.fbx(filepath=str(OUT / 'SK_DockWorker.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'}, bake_anim=False, **FBX)
dims = body.dimensions
counts = {n: 0 for n in SLOTS}
for poly in body.data.polygons: counts[SLOTS[poly.material_index]] += 1
print('CHUCK_WORKER_READY', f'tris={sum(len(p.vertices) - 2 for p in body.data.polygons)}', f'size=({dims.x:.1f},{dims.y:.1f},{dims.z:.1f})',
      f'bones={len(BONES)}', f'groups={len(body.vertex_groups)}', counts)

if '--review' in sys.argv:
    review = sys.argv[sys.argv.index('--review') + 1]
    colours = {'Skin': (.36, .2, .15), 'Shirt': (.55, .5, .4), 'Jerkin': (.12, .07, .035), 'Belt': (.07, .04, .02), 'Trousers': (.035, .067, .085),
               'Boots': (.05, .03, .02), 'Cap': (.16, .06, .04), 'Stubble': (.12, .08, .06), 'Brow': (.05, .035, .025), 'Eye': (.01, .01, .01)}
    for n, c in colours.items(): bpy.data.materials[n].diffuse_color = (*c, 1)
    bpy.ops.mesh.primitive_plane_add(size=600)
    bpy.ops.mesh.primitive_cylinder_add(radius=1.5, depth=65, location=(40, -30, 32.5))   # Chuck's height
    for name, loc, rot in (('front', (230, -110, 110), (82, 0, 65)), ('side', (0, -260, 100), (84, 0, 0))):
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); scene.collection.objects.link(cam)
        cam.location = loc; cam.rotation_euler = tuple(math.radians(a) for a in rot); cam.data.lens = 40; scene.camera = cam
        scene.render.engine = 'BLENDER_WORKBENCH'; scene.display.shading.color_type = 'MATERIAL'; scene.display.shading.light = 'STUDIO'
        scene.render.resolution_x, scene.render.resolution_y = 900, 1000
        scene.render.filepath = review.replace('.png', f'_{name}.png'); bpy.ops.render.render(write_still=True)
    print('CHUCK_WORKER_REVIEW', review)
