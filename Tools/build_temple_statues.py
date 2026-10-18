"""Blender 4.5.14: the Chult temple's skull stakes and serpent statues
(Claude, 2026-10-18; docs/CHULT-TEMPLE.md). User: "Find and implement free
resources to make the skull stakes and the serpent statues look better, if you
can't find a good resource for serpent statue feel free to change it".

blender --background --python Tools/build_temple_statues.py [-- --review <dir>]
  -> SourceAssets/Props/TempleStatues/SM_TempleSkull.fbx      the skull (CC0, below)
     SM_SkullStake.fbx      a hewn stake with lashings and a few stones at its foot
     SM_TempleSerpentHead.fbx  the stair balustrades' carved serpent head
     SM_TempleSerpentIdol.fbx  the rooms' coiled, reared serpent idol
     T_TempleSkull_D.png, T_TempleSkull_N.png  the skull's aged texture and normal map

The skull is "human skull 0" by CDmir, CC0, from
https://opengameart.org/content/human-skull-0 (skull-fbx.zip, downloaded with
the user's approval on 2026-10-18); its files are kept in Skull/source. Its
painted bone texture is aged here (darker, yellower, grimed in its crevices
from its own AO map, stained) and both maps are reduced to 1024.

No free serpent statue of usable quality and licence was found, so both are
modelled here: overlapping primitives and tubes, joined by a voxel remesh into
one carved surface, smoothed, weathered by displacement and decimated. All
four face +X with their origin on the ground (the skull's at the base of its
cranium, where the stake enters it). Centimetres. The statues' stone and
plaster materials in Unreal are world-aligned, so their UVs are only boxed.
"""
from pathlib import Path
import math
import random
import sys
import bpy
import numpy as np
from mathutils import Vector, Matrix, Euler

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets' / 'Props' / 'TempleStatues'
SKULL = OUT / 'Skull' / 'source'
ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = ARGS[ARGS.index('--review') + 1] if '--review' in ARGS else None

bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = .01
MATS = {}


def mat(name):
    if name not in MATS: MATS[name] = bpy.data.materials.new(name)
    return MATS[name]


def link(obj):
    bpy.context.collection.objects.link(obj); return obj


def apply_all(obj):
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj
    for m in list(obj.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def ell(c, r, rot=(0, 0, 0), seg=24):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=seg // 2, radius=1, location=c)
    o = bpy.context.active_object; o.scale = r; o.rotation_euler = Euler([math.radians(a) for a in rot]); apply_all(o); return o


def rbox(c, h, bevel, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=2, location=(0, 0, 0))
    o = bpy.context.active_object; o.scale = h; apply_all(o)
    b = o.modifiers.new('B', 'BEVEL'); b.width = min(bevel, min(h) * .95); b.segments = 4; apply_all(o)
    o.rotation_euler = Euler([math.radians(a) for a in rot]); o.location = c; apply_all(o); return o


def tube(points, r0, r1, res=6):
    cu = bpy.data.curves.new('T', 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = 1; cu.bevel_resolution = res
    cu.use_fill_caps = True; cu.resolution_u = 6
    sp = cu.splines.new('NURBS'); sp.points.add(len(points) - 1); sp.use_endpoint_u = True; sp.order_u = min(4, len(points))
    for i, p in enumerate(points):
        sp.points[i].co = (*p, 1); sp.points[i].radius = r0 + (r1 - r0) * i / max(len(points) - 1, 1)
    o = link(bpy.data.objects.new('T', cu))
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.convert(target='MESH'); return bpy.context.active_object


def unite(parts, name, voxel):
    """Join closed parts and voxel-remesh them into one surface (their union)."""
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts: p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]; bpy.ops.object.join()
    o = bpy.context.active_object; o.name = name; o.data.name = name
    r = o.modifiers.new('R', 'REMESH'); r.mode = 'VOXEL'; r.voxel_size = voxel; r.use_smooth_shade = True
    apply_all(o); return o


def weather(o, smooth, cloud_scale, cloud, pit_scale, pit, tris, seed):
    s = o.modifiers.new('S', 'SMOOTH'); s.factor = .6; s.iterations = smooth
    for scale, strength, kind in ((cloud_scale, cloud, 'CLOUDS'), (pit_scale, pit, 'VORONOI')):
        t = bpy.data.textures.new(f'W{seed}{kind}', kind)
        if kind == 'CLOUDS': t.noise_scale = scale; t.noise_depth = 3
        else: t.noise_scale = scale; t.distance_metric = 'DISTANCE'; t.noise_intensity = 1
        d = o.modifiers.new('D' + kind, 'DISPLACE'); d.texture = t; d.strength = strength; d.mid_level = .5; d.texture_coords = 'GLOBAL'
    apply_all(o)
    faces = len(o.data.polygons)
    if faces > tris:
        dm = o.modifiers.new('X', 'DECIMATE'); dm.ratio = tris / faces; apply_all(o)
    for p in o.data.polygons: p.use_smooth = True
    return o


def box_uv(o, scale):
    """UVs boxed from position by each face's main axis (scale cm per unit)."""
    me = o.data; uv = me.uv_layers.new(name='UV') if not me.uv_layers else me.uv_layers[0]
    for p in me.polygons:
        n = p.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        for li in p.loop_indices:
            v = me.vertices[me.loops[li].vertex_index].co
            a, b = [(v.y, v.z), (v.x, v.z), (v.x, v.y)][ax]
            uv.data[li].uv = (a / scale, b / scale)


def slots(o, names, pick):
    o.data.materials.clear()
    for n in names: o.data.materials.append(mat(n))
    for p in o.data.polygons: p.material_index = pick(p)


def export(o):
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=str(OUT / f'{o.name}.fbx'), use_selection=True, object_types={'MESH'},
                             apply_unit_scale=True, axis_forward='-Y', axis_up='Z', bake_anim=False, mesh_smooth_type='FACE')
    print('CHUCK_STATUE', o.name, f'tris={sum(len(p.vertices) - 2 for p in o.data.polygons)}',
          'size=(%.0f,%.0f,%.0f)' % tuple(o.dimensions))


OUT.mkdir(parents=True, exist_ok=True)

# --- The skull: the CC0 model, real size (19 cm long), facing +X. ---------
bpy.ops.import_scene.fbx(filepath=str(SKULL / 'skull-Low4K.fbx'))
skull = [o for o in bpy.context.selected_objects if o.type == 'MESH'][0]
bpy.context.view_layer.objects.active = skull; apply_all(skull)
k = 19. / skull.dimensions.y
skull.data.transform(Matrix.Rotation(math.radians(90), 4, 'Z') @ Matrix.Scale(k, 4))   # its face looked down -Y
vs = [v.co for v in skull.data.vertices]
xmid = (max(v.x for v in vs) + min(v.x for v in vs)) * .5
back = [v for v in vs if v.x < xmid]   # the back half: the cranium's base is its lowest point there
base = Vector((min(v.x for v in vs) + (max(v.x for v in vs) - min(v.x for v in vs)) * .38, 0, min(v.z for v in back) + 1.2))
skull.data.transform(Matrix.Translation(-base))
skull.name = skull.data.name = 'SM_TempleSkull'
skull.data.materials.clear(); skull.data.materials.append(mat('Bone'))
for p in skull.data.polygons: p.use_smooth = True
export(skull)


def pixels(path, size):
    img = bpy.data.images.load(str(path)); img.scale(size, size)
    a = np.array(img.pixels[:], dtype=np.float32).reshape(size, size, 4); return img, a


def save(img, a, path):
    img.pixels[:] = a.ravel(); img.filepath_raw = str(path); img.file_format = 'PNG'; img.save()


N = 1024
rng = np.random.default_rng(1810)
d_img, d = pixels(SKULL / 'Skull-Low.png', N)
_, ao = pixels(SKULL / 'Skull-AO.png', N)
occ = ao[..., :1]


def field(cells):
    f = rng.random((cells, cells)).astype(np.float32)
    f = np.kron(f, np.ones((N // cells, N // cells), np.float32))
    for _ in range(3):   # box blur
        f = (np.roll(f, 1, 0) + f + np.roll(f, -1, 0) + np.roll(f, 1, 1) + np.roll(f, -1, 1)) / 5
    return f[..., None]


stain = .75 + .25 * field(16) * field(64)
bone = d[..., :3]
grey = bone.mean(axis=2, keepdims=True)
aged = (bone * .55 + grey * .45) * np.array([.8, .74, .6], np.float32) * (.4 + .6 * occ) * stain
d[..., :3] = np.clip(aged, 0, 1)
save(d_img, d, OUT / 'T_TempleSkull_D.png')
n_img, n = pixels(SKULL / 'Skull-Low-normal.png', N)
save(n_img, n, OUT / 'T_TempleSkull_N.png')

# --- The stake: hewn, eight-faced, tapering, slightly bent; three lashings
#     under the skull, two hanging cords, stones round its foot. -------------
r = random.Random(1813)
ring = 8
verts, faces = [], []
zs = [-25 + i * 207 / 24 for i in range(25)]
jit = [r.uniform(.85, 1.12) for _ in range(ring)]
for z in zs:
    u = (z + 25) / 207
    rad = 6.5 - 2.3 * u
    if z > 176: rad *= max(.05, 1 - (z - 176) / 6)   # the point goes up into the skull
    bend = .00004 * (z + 25) ** 2
    for j in range(ring):
        a = j * math.tau / ring
        verts.append(Vector((bend + rad * jit[j] * math.cos(a), rad * jit[j] * math.sin(a), z)))
for i in range(len(zs) - 1):
    for j in range(ring):
        j2 = (j + 1) % ring
        faces.append((i * ring + j, i * ring + j2, (i + 1) * ring + j2, (i + 1) * ring + j))
faces.append(tuple(range(ring - 1, -1, -1))); faces.append(tuple((len(zs) - 1) * ring + j for j in range(ring)))
me = bpy.data.meshes.new('SM_SkullStake'); me.from_pydata(verts, [], faces); me.update()
stake = link(bpy.data.objects.new('SM_SkullStake', me))
uv = me.uv_layers.new(name='UV')
for p in me.polygons:
    for li in p.loop_indices:
        v = me.vertices[me.loops[li].vertex_index].co
        uv.data[li].uv = ((math.atan2(v.y, v.x) / math.tau + .5) * .45, v.z / 90)
stake.data.materials.append(mat('Wood'))
parts = [stake]
for z in (140, 145.5, 151):
    pts = [(.00004 * (z + 25) ** 2 + 4.9 * math.cos(a), 4.9 * math.sin(a), z + 1.4 * a / math.tau) for a in [k * math.tau / 10 for k in range(11)]]
    parts.append(tube(pts, .9, .9, 2))
for y, ln in ((1.5, 44), (-2, 31)):
    parts.append(tube([(5.2, y, 147), (6.2, y * 1.2, 147 - ln * .5), (5.6, y * 1.4, 147 - ln)], .55, .45, 2))
cord = parts[1:]
bpy.ops.object.select_all(action='DESELECT')
for p in cord: p.select_set(True)
bpy.context.view_layer.objects.active = cord[0]; bpy.ops.object.join(); cord = bpy.context.active_object
cord.data.materials.clear(); cord.data.materials.append(mat('Cord'))
stones = []
for k in range(5):
    a = k * math.tau / 5 + r.uniform(-.4, .4); d_ = r.uniform(10, 15)
    stones.append(rbox((d_ * math.cos(a), d_ * math.sin(a), 2), (r.uniform(4, 7), r.uniform(3.5, 6), r.uniform(3, 5)), 1.5, (r.uniform(-15, 15), r.uniform(-15, 15), r.uniform(0, 90))))
bpy.ops.object.select_all(action='DESELECT')
for p in stones: p.select_set(True)
bpy.context.view_layer.objects.active = stones[0]; bpy.ops.object.join(); stone = bpy.context.active_object
stone.data.materials.clear(); stone.data.materials.append(mat('Stone')); box_uv(stone, 200)
bpy.ops.object.select_all(action='DESELECT')
for p in (stake, cord, stone): p.select_set(True)
bpy.context.view_layer.objects.active = stake; bpy.ops.object.join()
stake = bpy.context.active_object; stake.name = stake.data.name = 'SM_SkullStake'
export(stake)

# --- The balustrade's serpent head (after Chichen Itza's Kukulkan heads):
#     back at x=0 where the balustrade begins, snout at x~220, its broad forked
#     tongue on the ground to x~295, 1.3 m wide, the feather crest 1.9 m up. ---
P = [rbox((30, 0, 60), (32, 60, 60), 22), ell((75, 0, 112), (62, 62, 46)), ell((150, 0, 104), (68, 54, 30)),
     ell((190, 0, 100), (32, 46, 26)), ell((130, 0, 24), (80, 52, 22)), rbox((110, 0, 10), (70, 50, 10), 6),
     ell((55, 0, 55), (30, 56, 36))]
for s in (-1, 1):
    # Bulging lidded eyes under brows that scroll down at the front, nostrils.
    P.append(ell((122, s * 50, 128), (16, 12, 15)))
    P.append(tube([(122 + 14 * math.cos(a), s * 58, 128 + 14 * math.sin(a)) for a in [k * math.tau / 12 for k in range(13)]], 3.5, 3.5, 3))
    P.append(tube([(92, s * 45, 136), (120, s * 49, 143), (146, s * 47, 139), (156, s * 44, 130), (152, s * 41, 124)], 7.5, 4.5))
    P.append(ell((211, s * 16, 112), (7, 7, 6)))
    # The great curved fangs at the mouth's corners, smaller ones below.
    P.append(tube([(201, s * 34, 82), (208, s * 34, 68), (206, s * 32, 57), (199, s * 30, 51)], 12, 1.4))
    P.append(tube([(197, s * 21, 40), (202, s * 21, 50), (200, s * 20, 56)], 7, 1))
    # Scale plates in rows along the lower jaw and up the neck.
    for z in (14, 28):
        for x in range(64, 192, 18):
            q = 1 - ((x - 130) / 80) ** 2 - ((z - 24) / 22) ** 2
            if q > .05: P.append(rbox((x, s * (52 * math.sqrt(q) + .5), z), (7, 3, 5), 2))
    for z in range(30, 111, 20):
        for x in range(10, 55, 16): P.append(rbox((x, s * 60, z), (7, 2.5, 8), 2))
    # The forked tongue's tips.
    P.append(ell((283, s * 9, 4.5), (14, 5, 3.5), (0, 0, s * 25)))
P.append(tube([(190, 0, 122), (207, 0, 126), (216, 0, 118), (211, 0, 111)], 5, 3.5))
for y in range(-18, 19, 9):
    P.append(tube([(211, y, 77), (212, y, 69), (211.5, y, 66)], 3.6, .8))
# The tongue: broad and flat, lolling out of the mouth onto the plaza.
for i in range(15):
    u = i / 14; x = 120 + 148 * u
    P.append(ell((x, 0, 44 - 39 * u ** 1.4), (16, 16 - 6 * u, 6 - u)))
# The feather crest behind the head: two tiers of broad plumes fanning back.
for i in range(7):
    y = -54 + 18 * i
    P.append(ell((12 - abs(y) * .1, y, 138 - abs(y) * .3), (7, 11, 36), (y * .35, -24, 0)))
for i in range(6):
    y = -45 + 18 * i
    P.append(ell((-8, y, 150 - abs(y) * .3), (6, 10, 42), (y * .4, -38, 0)))
head = unite(P, 'SM_TempleSerpentHead', 1.4)
weather(head, 4, 22, 2.0, 5, 1.0, 12000, 1)
box_uv(head, 200)
slots(head, ['Stone', 'Moss'], lambda p: 1 if p.normal.z > .8 and p.center.z > 60 else 0)
export(head)

# --- The coiled serpent idol (rooms): R = 25 cm, 2.6 turns rising from a thin
#     tail, the neck rearing to a fanged head in a feathered hood, facing +X. --
R = 25.; T = R * .34
coil = []
turns = 2.6
for i in range(81):
    u = i / 80; a = -turns * math.tau * (1 - u); rr = R * (1 - .5 * u)
    coil.append((rr * math.cos(a), rr * math.sin(a), T * 1.05 + u * R * 1.35))
zE = coil[-1][2]
neck = [coil[-1], (R * .5, 0, zE + R * .35), (R * .42, 0, zE + R * .75), (R * .6, 0, zE + R * 1.0)]
P = [tube(coil[:12], T * .25, T * .95), tube(coil[10:], T, T * .85), tube(neck, T * .85, T * .75)]
H = Vector((R * .85, 0, zE + R * 1.08))
P += [ell(H, (R * .38, R * .3, R * .24)), ell(H + Vector((R * .32, 0, R * .02)), (R * .32, R * .22, R * .13)),
      ell(H + Vector((R * .26, 0, -R * .17)), (R * .28, R * .18, R * .06), (0, 18, 0)),
      ell((0, 0, T * .25), (R * 1.18, R * 1.18, T * .32), seg=48)]
for s in (-1, 1):
    P.append(ell(H + Vector((R * .18, s * R * .22, R * .1)), (R * .08, R * .05, R * .08)))
    P.append(tube([tuple(H + Vector((-R * .05, s * R * .2, R * .2))), tuple(H + Vector((R * .2, s * R * .22, R * .22))), tuple(H + Vector((R * .34, s * R * .17, R * .14)))], R * .065, R * .035))
    P.append(tube([tuple(H + Vector((R * .55, s * R * .13, -R * .04))), tuple(H + Vector((R * .59, s * R * .13, -R * .14))), tuple(H + Vector((R * .55, s * R * .12, -R * .22)))], R * .055, R * .01))
# The feathered hood: plumes fanned round the back of the head.
for i in range(9):
    a = math.radians(-80 + i * 20)
    c = H + Vector((-R * .2, math.sin(a) * R * .3, math.cos(a) * R * .25 + R * .05))
    P.append(ell(c, (R * .035, R * .09, R * .3), (math.degrees(a) * -.9, -20, 0)))
idol = unite(P, 'SM_TempleSerpentIdol', .5)
weather(idol, 3, 6, .45, 1.3, .3, 8000, 2)
# Wrapped round its axis (one seam, at the back): boxed UVs showed the plaster's seams.
uv = idol.data.uv_layers.new(name='UV')
for p in idol.data.polygons:
    for li in p.loop_indices:
        v = idol.data.vertices[idol.data.loops[li].vertex_index].co
        uv.data[li].uv = ((math.atan2(v.y, v.x) / math.tau + .5) * 2 * math.pi * R / 200, v.z / 200)
slots(idol, ['Plaster'], lambda p: 0)
export(idol)

if REVIEW:
    Path(REVIEW).mkdir(parents=True, exist_ok=True)
    sc = scene; sc.render.engine = 'BLENDER_WORKBENCH'; sc.display.shading.light = 'STUDIO'; sc.display.shading.color_type = 'MATERIAL'
    sc.render.resolution_x, sc.render.resolution_y = 900, 700
    cam = link(bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))); sc.camera = cam
    for o in bpy.data.objects:
        if o.type == 'MESH': o.hide_render = True
    for o, eye, at in ((skull, (40, -25, 12), (0, 0, 8)), (stake, (220, -150, 150), (0, 0, 110)),
                       (head, (420, -260, 170), (140, 0, 80)), (head, (520, 0, 60), (150, 0, 70)), (idol, (120, -80, 70), (5, 0, 40))):
        for p in bpy.data.objects:
            if p.type == 'MESH': p.hide_render = p is not o
        cam.location = eye; cam.rotation_euler = (Vector(at) - Vector(eye)).to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = str(Path(REVIEW) / f'{o.name}_{eye[0]}.png'); bpy.ops.render.render(write_still=True)
print('CHUCK_STATUES_READY')
