"""Blender 4.5 LTS: Chuck's cigarette prop and smoke wisp (static meshes).

blender --background --factory-startup --python SourceAssets/Chuck/V1/build_cigarette.py

Writes SourceAssets/Chuck/V1/Cigarette/{SM_Cigarette.fbx, SM_CigaretteSmoke.fbx,
T_CigaretteSmoke.png}. Every goal image (References/ArtDirection) shows the
cigarette held in the left mouth corner with a thin smoke wisp. It is a
separate prop on socket_cigarette (docs/RIG-CONTRACT-V1.md), not part of the
skinned body, so a future pickup design can show or hide it.

Local frames (source cm, same FBX axis settings as SK_Chuck: Blender +X stays
Unreal +X, +Z up):
  SM_Cigarette: filter end at the origin (on socket_cigarette), lit end at +X,
    7 cm long: the goal images show it reaching well past the lips; the
    runtime reads the length from the mesh bounds.
  SM_CigaretteSmoke: rises along +Z from the origin (placed at the lit end and
    kept upright in world space by the runtime).
Material slots: Paper, Filter, Ash, Ember (cigarette); Smoke (wisp).
"""
import math
import random
from pathlib import Path
import bpy
from mathutils import Vector

OUT = Path(__file__).resolve().parent / 'Cigarette'
OUT.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = .01
LENGTH, RADIUS, FILTER, ASH = 7.0, .3, 1.6, .4
rng = random.Random(1729)

def material(name, colour):
    m = bpy.data.materials.new(name); m.diffuse_color = (*colour, 1); return m

def ring_mesh(name, stations, sides=16, mats=()):
    """Closed tube along +X: stations = [(x, radius, material index)]."""
    verts, faces, face_mat = [], [], []
    for x, r, _ in stations:
        for j in range(sides):
            a = 2 * math.pi * j / sides
            verts.append((x, r * math.cos(a), r * math.sin(a)))
    for i in range(len(stations) - 1):
        for j in range(sides):
            k, n = i * sides + j, i * sides + (j + 1) % sides
            faces.append((k, n, n + sides, k + sides)); face_mat.append(stations[i + 1][2])
    cap0 = len(verts); verts.append((stations[0][0], 0, 0))
    cap1 = len(verts); verts.append((stations[-1][0], 0, 0))
    last = (len(stations) - 1) * sides
    for j in range(sides):
        n = (j + 1) % sides
        faces.append((cap0, n, j)); face_mat.append(stations[0][2])
        faces.append((cap1, last + j, last + n)); face_mat.append(stations[-1][2])
    data = bpy.data.meshes.new(name); data.from_pydata(verts, [], faces); data.update()
    for m in mats: data.materials.append(m)
    for f, mi in zip(data.polygons, face_mat):
        f.material_index = mi; f.use_smooth = True
    obj = bpy.data.objects.new(name, data); scene.collection.objects.link(obj)
    return obj

paper, filt = material('Paper', (.86, .84, .8)), material('Filter', (.78, .62, .42))
ash, ember = material('Ash', (.35, .34, .33)), material('Ember', (1., .32, .06))
# Filter (0..1.3), paper, a thin grey ash band, then the glowing ember cap.
L = LENGTH
stations = [(0., RADIUS * .96, 1), (.05, RADIUS, 1), (FILTER, RADIUS, 1), (FILTER + .02, RADIUS, 0),
            (L - ASH - .06, RADIUS, 0), (L - ASH, RADIUS * .99, 2), (L - .08, RADIUS * .9, 2),
            (L, RADIUS * .55, 3)]
cig = ring_mesh('SM_Cigarette', stations, mats=(paper, filt, ash, ember))
# Flat per-slot materials: the cylinder needs only a trivial UV channel.
cig_uv = cig.data.uv_layers.new(name='UVMap')
for loop in cig.data.loops:
    co = cig.data.vertices[loop.vertex_index].co
    cig_uv.data[loop.index].uv = (co.x / LENGTH, .5 + math.atan2(co.z, co.y) / (2 * math.pi))

# Smoke: two crossed ribbons along a gently curling path, widening as they
# rise; UV u across, v up (the material pans a wisp texture along v and
# fades at the base, the top and both edges).
HEIGHT, SEGMENTS = 16., 36
def path(t):
    # A thin rising thread that curls more as it climbs.
    return Vector((1.8 * math.sin(t * 6.3) * t ** .8, 1.4 * math.sin(t * 4.4 + 1.) * t ** .8, HEIGHT * t))
verts, faces, uvs = [], [], []
for ribbon in range(2):
    side = Vector((1, 0, 0)) if ribbon == 0 else Vector((0, 1, 0))
    base = len(verts)
    for i in range(SEGMENTS + 1):
        t = i / SEGMENTS
        half = .08 + 1.1 * t ** 1.5
        c = path(t)
        verts += [tuple(c - side * half), tuple(c + side * half)]
        uvs += [(0., t), (1., t)]
    for i in range(SEGMENTS):
        k = base + 2 * i
        faces.append((k, k + 1, k + 3, k + 2))
data = bpy.data.meshes.new('SM_CigaretteSmoke'); data.from_pydata(verts, [], faces); data.update()
data.materials.append(material('Smoke', (.8, .8, .82)))
uv = data.uv_layers.new(name='UVMap')
for poly in data.polygons:
    for li in poly.loop_indices:
        uv.data[li].uv = uvs[data.loops[li].vertex_index]
smoke = bpy.data.objects.new('SM_CigaretteSmoke', data); scene.collection.objects.link(smoke)

FBX = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE',
           object_types={'MESH'}, use_selection=True, bake_anim=False)
for obj in (cig, smoke):
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT / f'{obj.name}.fbx'), **FBX)

# Wisp texture (grey = opacity): soft, vertically tileable streaks. Pure bpy
# pixels so no image library is needed.
W, H = 64, 256
px = [0.] * (W * H)
for _ in range(9):
    x0, amp, freq, phase = rng.uniform(16, 48), rng.uniform(3, 9), rng.choice((1, 2)), rng.uniform(0, 6.3)
    width, strength = rng.uniform(2., 5.), rng.uniform(.35, .8)
    for y in range(H):
        cx = x0 + amp * math.sin(2 * math.pi * freq * y / H + phase)
        for x in range(W):
            d = (x - cx) / width
            px[y * W + x] += strength * math.exp(-d * d)
peak = max(px)
img = bpy.data.images.new('T_CigaretteSmoke', W, H, alpha=False)
img.pixels = [c for v in px for c in (min(1., v / peak),) * 3 + (1.,)]
img.filepath_raw = str(OUT / 'T_CigaretteSmoke.png'); img.file_format = 'PNG'; img.save()
print('CHUCK_CIGARETTE_READY', LENGTH, 'cm; smoke', HEIGHT, 'cm', sorted(p.name for p in OUT.iterdir()))
