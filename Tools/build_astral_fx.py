"""Blender 4.5.14: the astral summoning effect's meshes and rune texture
(user 2026-09-30: Chuck is a fey summon who can't die - at zero Sanity he
quietly disappears and is summoned back; References/Original/GAME-BIBLE.md:
the Astral Sea is peaceful, ancient, quiet, melancholy).

blender --background --python Tools/build_astral_fx.py
  -> SourceAssets/Props/Astral/SM_AstralCircle.fbx   flat disc, 100 cm across, UV 0..1
     SM_AstralColumn.fbx   open tube 50 cm across, 100 cm tall, UV v = height (0 at the foot)
     SM_AstralMote.fbx     a small four-point star (a double-sided cross of diamonds), 2.4 cm
     T_AstralCircle.png    1024 px rune circle: a faint double ring, fine ticks, a
                           band of small glyphs and a few stars - white on black,
                           tinted and made additive by the material
Centimetres, origins at the base centre.
"""
from pathlib import Path
import math
import random
import bpy
from mathutils import Vector

OUT = Path(__file__).resolve().parents[1] / 'SourceAssets' / 'Props' / 'Astral'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = .01
MAT = bpy.data.materials.new('Astral')


def make(name, verts, faces, uvs=None):
    data = bpy.data.meshes.new(name); data.from_pydata(verts, [], faces); data.update()
    if uvs:
        layer = data.uv_layers.new(name='UVMap')
        for poly in data.polygons:
            for li in poly.loop_indices:
                layer.data[li].uv = uvs[data.loops[li].vertex_index]
    obj = bpy.data.objects.new(name, data); bpy.context.collection.objects.link(obj)
    obj.data.materials.append(MAT)
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT / f'{name}.fbx'), use_selection=True, object_types={'MESH'},
                             apply_unit_scale=True, axis_forward='-Y', axis_up='Z', bake_anim=False, mesh_smooth_type='FACE')
    return obj


# Circle: a flat disc just off the ground.
seg = 64
verts = [(0., 0., .3)] + [(50 * math.cos(math.tau * i / seg), 50 * math.sin(math.tau * i / seg), .3) for i in range(seg)]
uvs = [(.5, .5)] + [(.5 + .5 * math.cos(math.tau * i / seg), .5 + .5 * math.sin(math.tau * i / seg)) for i in range(seg)]
make('SM_AstralCircle', verts, [(0, 1 + i, 1 + (i + 1) % seg) for i in range(seg)], uvs)

# Column: an open tube, double-sided (inner faces too), UV v = height.
rings, seg = 8, 32
verts, faces, uvs = [], [], []
for i in range(rings + 1):
    z = 100. * i / rings
    for j in range(seg + 1):
        a = math.tau * j / seg
        verts.append((25 * math.cos(a), 25 * math.sin(a), z)); uvs.append((j / seg, i / rings))
for i in range(rings):
    for j in range(seg):
        a, b = i * (seg + 1) + j, i * (seg + 1) + j + 1
        c, d = b + seg + 1, a + seg + 1
        faces.append((a, b, c, d)); faces.append((d, c, b, a))
make('SM_AstralColumn', verts, faces, uvs)

# Mote: a four-point star of crossed diamonds (reads as a point of starlight from any side).
verts, faces = [], []
for axis in range(3):
    base = len(verts)
    for k in range(4):
        a = math.tau * k / 4
        r = 1.2 if k % 2 == 0 else .35
        p = [0., 0., 0.]
        p[(axis + 1) % 3] = r * math.cos(a); p[(axis + 2) % 3] = r * math.sin(a)
        verts.append(tuple(p))
    faces.append((base, base + 1, base + 2, base + 3)); faces.append((base + 3, base + 2, base + 1, base))
make('SM_AstralMote', verts, faces, [(.5, .5)] * len(verts))

# Rune circle texture.
N = 1024
px = [0.] * (N * N)
def stamp(x, y, v):
    ix, iy = int(x), int(y)
    if 0 <= ix < N and 0 <= iy < N: px[iy * N + ix] = max(px[iy * N + ix], v)
def ring(r, width, v):
    steps = int(math.tau * r * 3)
    for k in range(steps):
        a = math.tau * k / steps
        for w in range(-int(width), int(width) + 1):
            fall = 1 - abs(w) / (width + 1)
            stamp(N / 2 + (r + w) * math.cos(a), N / 2 + (r + w) * math.sin(a), v * fall)
def line(a, r0, r1, v, width=1):
    steps = int(abs(r1 - r0) * 2) + 1
    for k in range(steps):
        r = r0 + (r1 - r0) * k / steps
        for w in range(-width, width + 1):
            stamp(N / 2 + r * math.cos(a) - w * math.sin(a), N / 2 + r * math.sin(a) + w * math.cos(a), v * (1 - abs(w) / (width + 1)))
rng = random.Random(1123)
ring(488, 3, 1.); ring(470, 1.5, .7); ring(360, 2, .8); ring(348, 1, .45)
for k in range(96):                                   # fine ticks between the outer rings
    a = math.tau * k / 96
    line(a, 472, 486 if k % 4 else 460, .8)
for k in range(12):                                   # a band of small glyphs: arcs and strokes
    a0 = math.tau * k / 12 + .12
    for s in range(rng.randint(2, 4)):
        kind = rng.random(); a = a0 + rng.uniform(-.1, .1)
        if kind < .5:
            line(a, 385 + rng.uniform(0, 20), 440 - rng.uniform(0, 20), .9, 2)
        else:
            rr = rng.uniform(395, 430); span = rng.uniform(.05, .12)
            for q in range(40):
                aa = a + span * (q / 39 - .5)
                stamp(N / 2 + rr * math.cos(aa), N / 2 + rr * math.sin(aa), .9)
                stamp(N / 2 + (rr + 1) * math.cos(aa), N / 2 + (rr + 1) * math.sin(aa), .7)
for k in range(7):                                    # a few stars inside
    a = rng.uniform(0, math.tau); r = rng.uniform(60, 320); cx, cy = N / 2 + r * math.cos(a), N / 2 + r * math.sin(a)
    for d in range(-9, 10):
        v = (1 - abs(d) / 10) ** 2
        stamp(cx + d, cy, v); stamp(cx, cy + d, v)
img = bpy.data.images.new('T_AstralCircle', N, N, alpha=False)
pixels = []
for v in px: pixels += [v, v, v, 1.]
img.pixels = pixels
img.filepath_raw = str(OUT / 'T_AstralCircle.png'); img.file_format = 'PNG'; img.save()
print('CHUCK_ASTRAL_FX_READY', [p.name for p in OUT.iterdir()])
