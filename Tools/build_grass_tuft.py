"""Blender 4.5.14: shreddable dock grass tufts (user request 2026-09-30).

blender --background --python Tools/build_grass_tuft.py
  -> SourceAssets/Props/Grass/SM_GrassTuft_{A,B,C}.fbx, SM_GrassStub_{A,B,C}.fbx,
     SM_GrassClipping.fbx

Scruffy weeds growing between the planks and cobbles, 20-32 cm tall (knee to
hip on 65 cm Chuck), dry olive green to straw tips. Each blade is a curved,
tapering strip, built double-sided with normals leaning up and out of the clump
so both sides shade softly (a one-sided material). Vertex colour carries the
tint; its alpha is the height fraction (0 root, 1 tip) for the wind sway in
M_Grass. The stub is the same clump cut to ragged 2-6 cm stalks (what is left
after a slash), from the same seed so it sits exactly where the tuft was; the
clipping is a short blade piece for the shred spray. Centimetres, origin at the
clump's root centre on the ground.
"""
from pathlib import Path
import math
import random
import bpy
from mathutils import Vector

OUT = Path(__file__).resolve().parents[1] / 'SourceAssets' / 'Props' / 'Grass'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = .01
MAT = bpy.data.materials.new('Grass')

# Linear tints: root (shaded, damp), body (dusty olive), tip (sun-dried straw).
ROOT_C = (.05, .065, .022)
BODY_C = (.14, .18, .05)
TIP_C = (.40, .34, .13)


def lerp3(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def tint(u, rng_shift):
    c = lerp3(ROOT_C, BODY_C, min(1., u / .45)) if u < .45 else lerp3(BODY_C, TIP_C, (u - .45) / .55)
    return tuple(max(0., v * rng_shift[0] + rng_shift[1]) for v in c)


def blade_points(rng, root, height, cut=None):
    """Centre line and widths of one blade. cut: stalk height if shredded."""
    angle = math.atan2(root.y, root.x) + rng.uniform(-.6, .6) if root.length > .5 else rng.uniform(0, math.tau)
    out = Vector((math.cos(angle), math.sin(angle), 0))
    lean = math.radians(rng.uniform(3, 12) + 22 * min(1., root.length / 6.))   # edge blades splay
    droop = rng.uniform(.1, .4)                                               # tips arch over
    width = rng.uniform(.9, 1.5)
    top = height if cut is None else min(height, cut)
    n = 5 if cut is None else 2
    pts, widths, us = [], [], []
    for i in range(n + 1):
        s = top * i / n
        u = s / height                                                       # fraction of the full blade
        tilt = lean + droop * u * u
        p = root + out * (math.sin(tilt) * s * .9) + Vector((0, 0, math.cos(min(tilt, 1.3)) * s))
        pts.append(p)
        widths.append(width * (1 - u) ** .7 if cut is None else width * (1 - u) ** .7)
        us.append(u)
    if cut is None:
        widths[-1] = 0.
    return pts, widths, us, out


def add_blade(verts, faces, cols, norms, rng, root, height, cut=None):
    pts, widths, us, out = blade_points(rng, root, height, cut)
    side = Vector((-out.y, out.x, 0)).normalized()
    twist = rng.uniform(-.5, .5)
    shift = (rng.uniform(.85, 1.2), rng.uniform(-.01, .015))
    normal = (Vector((0, 0, 1)) * .75 + out * .35).normalized()
    rows = []
    for i, (p, w, u) in enumerate(zip(pts, widths, us)):
        s = (side * math.cos(twist * u) + out * math.sin(twist * u)) * (w / 2)
        left, right = p - s, p + s
        if cut is not None and i == len(pts) - 1:
            # Ragged cut: one corner torn higher than the other.
            left = left + Vector((0, 0, rng.uniform(-.8, .4))); right = right + Vector((0, 0, rng.uniform(-.4, .8)))
        rows.append((left, right, u))
    base = len(verts)
    for left, right, u in rows:
        for v in (left, right):
            verts.append(v)
            cols.append((*tint(u, shift), u))
            norms.append(normal)
    for i in range(len(rows) - 1):
        a, b, c, d = base + 2 * i, base + 2 * i + 1, base + 2 * i + 3, base + 2 * i + 2
        faces.append((a, b, c, d))       # front
        faces.append((d, c, b, a))       # back (same up-leaning normals)


def build(name, seed, blades, radius, heights, cut=None):
    rng = random.Random(seed)
    verts, faces, cols, norms = [], [], [], []
    for _ in range(blades):
        r = radius * math.sqrt(rng.random()); a = rng.uniform(0, math.tau)
        root = Vector((r * math.cos(a), r * math.sin(a), -.3))   # just below the ground: no floating roots
        h = rng.uniform(*heights) * (1.05 - .25 * r / radius)    # taller in the middle
        stalk = None if cut is None else rng.uniform(*cut)
        add_blade(verts, faces, cols, norms, rng, root, h, stalk)
    return make(name, verts, faces, cols, norms)


def make(name, verts, faces, cols, norms):
    data = bpy.data.meshes.new(name)
    data.from_pydata([tuple(v) for v in verts], [], faces)
    data.update()
    attr = data.color_attributes.new('Col', 'FLOAT_COLOR', 'CORNER')
    loop_normals = []
    for poly in data.polygons:
        for li in poly.loop_indices:
            vi = data.loops[li].vertex_index
            attr.data[li].color = cols[vi]
            loop_normals.append(tuple(norms[vi]))
    data.normals_split_custom_set(loop_normals)
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(MAT)
    return obj


def export(obj):
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT / f'{obj.name}.fbx'), use_selection=True, object_types={'MESH'},
                             apply_unit_scale=True, axis_forward='-Y', axis_up='Z', bake_anim=False,
                             mesh_smooth_type='OFF', colors_type='LINEAR', use_tspace=False)


VARIANTS = {'A': (4011, 46, 6.5, (22, 32)), 'B': (4012, 38, 5.5, (18, 27)), 'C': (4013, 54, 8., (20, 30))}
report = []
for key, (seed, blades, radius, heights) in VARIANTS.items():
    tuft = build(f'SM_GrassTuft_{key}', seed, blades, radius, heights)
    stub = build(f'SM_GrassStub_{key}', seed, blades, radius, heights, cut=(2., 6.))
    for obj in (tuft, stub):
        export(obj)
        dims = obj.dimensions
        report.append(f'{obj.name} tris={sum(len(p.vertices) - 2 for p in obj.data.polygons)} size=({dims.x:.1f},{dims.y:.1f},{dims.z:.1f})')

# Clipping: a 5 cm blade piece, lying along +X, centred on its origin.
rng = random.Random(4020)
verts, faces, cols, norms = [], [], [], []
add_blade(verts, faces, cols, norms, rng, Vector((0, 0, 0)), 16., cut=5.)
clip = make('SM_GrassClipping', [Vector((v.z - 2.5, v.y, v.x)) for v in verts], faces,
            [(*tint(.55 + .35 * (i % 4) / 3, (1., 0.)), .6) for i, c in enumerate(cols)], [Vector((0, 0, 1)) for _ in norms])
export(clip)
report.append(f'SM_GrassClipping tris={sum(len(p.vertices) - 2 for p in clip.data.polygons)}')
print('CHUCK_GRASS_READY', '; '.join(report))

# Review render (not exported): the three tufts and a stub beside a 65 cm marker.
import sys
if '--review' in sys.argv:
    review = sys.argv[sys.argv.index('--review') + 1]
    for i, key in enumerate('ABC'):
        bpy.data.objects[f'SM_GrassTuft_{key}'].location = (0, i * 45 - 45, 0)
    bpy.data.objects['SM_GrassStub_A'].location = (0, 90, 0)
    for obj in bpy.data.objects:
        if obj.name.startswith('SM_GrassStub_') and obj.name != 'SM_GrassStub_A': obj.hide_render = True
    clip.hide_render = True
    bpy.ops.mesh.primitive_cylinder_add(radius=1.5, depth=65, location=(0, -95, 32.5))
    bpy.ops.mesh.primitive_plane_add(size=600, location=(0, 0, 0))
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); scene.collection.objects.link(cam)
    cam.location = (190, 10, 45); cam.rotation_euler = (math.radians(80), 0, math.radians(90))
    cam.data.lens = 40; scene.camera = cam
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.color_type = 'VERTEX'; scene.display.shading.light = 'STUDIO'
    scene.render.resolution_x, scene.render.resolution_y = 1400, 700
    scene.render.filepath = review; bpy.ops.render.render(write_still=True)
    print('CHUCK_GRASS_REVIEW', review)
