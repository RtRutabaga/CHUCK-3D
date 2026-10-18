"""Blender 4.5.14: breakable clay jar (user 2026-09-30: Chuck breaks grass and
jars for the cigarettes inside - never crates or barrels; urns in later maps).

blender --background --python Tools/build_clay_jar.py [-- --variant jar|tan|urn]
  jar (default) -> SourceAssets/Props/Jar/SM_ClayJar.fbx, SM_JarShard_00..11.fbx, shards.json
                -> Unreal/Chuck3D/Source/Chuck3D/ClayJarData.h (shard rest offsets)
  tan           -> SourceAssets/Props/TempleJar/SM_TempleJar.fbx, SM_TempleJarShard_*, TempleJarData.h
  urn           -> SourceAssets/Props/Urn/SM_ClayUrn.fbx, SM_UrnShard_*, ClayUrnData.h

A small dockside storage jar, 25.5 cm tall (knee to hip on 65 cm Chuck):
terracotta body with a dark glaze over the shoulder and neck, ending in an
uneven drip line. Lathe profile with a real wall thickness and an open mouth.
The shards are the same wall cut into 12 pieces (three bands, cut lines
jittered so they read as breaks, not tiles); each exports with its origin at
its own centre, and its rest offset from the jar origin goes to
ClayJarData.h so the runtime can put the pieces exactly where the jar stood
before they fly. Vertex colour carries the tint (M_Grass's gamma-squared
convention); alpha marks glaze (glossier). Centimetres, origin at the base.

Chult temple variants (Claude, 2026-10-18; docs/CHULT-TEMPLE.md): 'tan' is the
same jar in tan ceramic under a slightly deeper glossy tan glaze; 'urn' is a 44 cm temple
urn (to Chuck's chest), buff slip painted with red bands and a black stepped
fret round the shoulder, in 19 shards. The default 'jar' output is unchanged.
"""
from pathlib import Path
import json
import math
import random
import sys
import bpy
from mathutils import Vector

ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
VARIANT = ARGS[ARGS.index('--variant') + 1] if '--variant' in ARGS else 'jar'
ROOT = Path(__file__).resolve().parents[1]
VARIANTS = {
    'jar': dict(folder='Jar', body='SM_ClayJar', shard='SM_JarShard_', header='ClayJarData', seed=5150),
    'tan': dict(folder='TempleJar', body='SM_TempleJar', shard='SM_TempleJarShard_', header='TempleJarData', seed=5150),
    'urn': dict(folder='Urn', body='SM_ClayUrn', shard='SM_UrnShard_', header='ClayUrnData', seed=181018),
}[VARIANT]
OUT = ROOT / 'SourceAssets' / 'Props' / VARIANTS['folder']
OUT.mkdir(parents=True, exist_ok=True)
HEADER = ROOT / 'Unreal' / 'Chuck3D' / 'Source' / 'Chuck3D' / f"{VARIANTS['header']}.h"
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = .01
MAT = bpy.data.materials.new('Clay')

# (height, outer radius): foot, belly, shoulder, neck, flared rim.
PROFILE = [(0., 5.6), (.8, 6.2), (4., 8.4), (9., 9.6), (13., 9.3), (17., 7.6), (20., 5.2), (22., 4.3), (24.2, 4.6), (25.5, 5.1)]
WALL = .7
SEGMENTS = 36
TERRACOTTA = (.30, .115, .05)
GLAZE = (.055, .06, .035)
INSIDE = (.12, .05, .025)
if VARIANT == 'tan':
    TERRACOTTA = (.5, .38, .22)
    GLAZE = (.4, .28, .145)
    INSIDE = (.2, .14, .075)
if VARIANT == 'urn':
    PROFILE = [(0., 7.5), (1.2, 8.2), (6., 12.), (14., 15.), (22., 15.2), (28., 13.5), (33., 9.5), (36., 6.8), (39., 6.2), (42., 7.), (44., 8.)]
    WALL = 1.
    SEGMENTS = 96
    TERRACOTTA = (.36, .2, .09)    # buff-orange slip
    INSIDE = (.16, .08, .035)
HEIGHT = PROFILE[-1][0]
FOOT = PROFILE[1][0]
MAX_RADIUS = max(r for _, r in PROFILE)
URN_CREAM, URN_RED, URN_BLACK = (.5, .4, .26), (.24, .06, .03), (.035, .03, .025)


def radius_at(z):
    for (z0, r0), (z1, r1) in zip(PROFILE, PROFILE[1:]):
        if z <= z1:
            u = (z - z0) / (z1 - z0)
            u = u * u * (3 - 2 * u)
            return r0 + (r1 - r0) * u
    return PROFILE[-1][1]


def glaze_line(a):
    """Height where the glaze ends at angle a: uneven drips."""
    return 15.2 + 1.1 * math.sin(3 * a + .7) + .7 * math.sin(7 * a + 2.1) + .9 * max(0., math.sin(11 * a)) ** 6 * -3


def urn_colour(z, a):
    """Painted bands: red rules, a cream shoulder band with a black stepped
    fret (the step-pyramid motif) twelve times round, a red rim; burnished."""
    h = z / HEIGHT
    m = 1 + .06 * math.sin(a * 5 + z * .5) + .04 * math.sin(a * 11 - z * .9)
    def c(base, alpha=.25): return (*(x * m for x in base), alpha)
    if h > .93 or .2 < h < .235 or .515 < h < .545 or .735 < h < .765: return c(URN_RED)
    if .545 <= h <= .735:
        u = (h - .545) / .19
        x = (a * 12 / math.tau) % 1.
        step = math.floor(min(x, 1. - x) * 2 * 4) / 4   # 0 .. .75 in four stairs
        if u < .1 or u > .9 or (u < .1 + .8 * step and .08 < x < .92): return c(URN_BLACK)
        return c(URN_CREAM)
    return c(TERRACOTTA)


def colour(z, a, outer, rng=None):
    if not outer:
        return (*INSIDE, 0.)
    if VARIANT == 'urn':
        return urn_colour(z, a)
    glazed = z >= glaze_line(a)
    base = GLAZE if glazed else TERRACOTTA
    # A little mottling in the fired clay.
    m = 1 + .08 * math.sin(a * 5 + z * .9) + .05 * math.sin(a * 13 - z * 1.7)
    return (*(c * m for c in base), 1. if glazed else 0.)


def point(z, a, inner=False):
    r = radius_at(z) - (WALL if inner else 0.)
    return Vector((r * math.cos(a), r * math.sin(a), z if not inner or z > FOOT else FOOT))


def build_patch(name, z0, z1, a0, a1, jitter, rows=6, cols=6):
    """A piece of the wall (outer and inner skins plus its broken edges) between
    heights z0..z1 and angles a0..a1; jitter(z or a) roughens the cut lines."""
    verts, faces, cols_ = [], [], []
    grid = {}
    for skin in (0, 1):
        for i in range(rows + 1):
            for j in range(cols + 1):
                u, v = i / rows, j / cols
                a = a0 + (a1 - a0) * v
                z = z0 + (z1 - z0) * u
                # Roughen the boundary rows/columns (the breaks), not the interior.
                if i in (0, rows) and 0 < z < HEIGHT: z += jitter('z', a, i == 0)
                if j in (0, cols): a += jitter('a', z, j == 0)
                p = point(z, a, inner=skin == 1)
                grid[(skin, i, j)] = len(verts); verts.append(p); cols_.append(colour(z, a, skin == 0))
    for i in range(rows):
        for j in range(cols):
            o = [grid[(0, i + di, j + dj)] for di, dj in ((0, 0), (0, 1), (1, 1), (1, 0))]
            n = [grid[(1, i + di, j + dj)] for di, dj in ((0, 0), (0, 1), (1, 1), (1, 0))]
            faces.append(tuple(o)); faces.append(tuple(reversed(n)))
    # Broken edges: bands of bare clay joining the skins round the patch border.
    border = [(0, j) for j in range(cols)] + [(i, cols) for i in range(rows)] + \
             [(rows, cols - j) for j in range(cols)] + [(rows - i, 0) for i in range(rows)]
    for k, (i, j) in enumerate(border):
        i2, j2 = border[(k + 1) % len(border)]
        a, b = grid[(0, i, j)], grid[(0, i2, j2)]
        c, d = grid[(1, i2, j2)], grid[(1, i, j)]
        base = len(verts)
        for idx in (a, b, c, d):
            verts.append(verts[idx]); cols_.append((*INSIDE, 0.))
        faces.append((base, base + 3, base + 2, base + 1))
    return verts, faces, cols_


def base_disc(name):
    """The jar's floor with its foot ring: the one piece that stays put."""
    verts, faces, cols_ = [], [], []
    (_, r0), (zf, rf) = PROFILE[0], PROFILE[1]
    rings = [(0., 0.), (0., r0), (zf, rf), (zf, rf - WALL), (zf, 0.)]
    for z, r in rings:
        for j in range(SEGMENTS):
            a = math.tau * j / SEGMENTS
            verts.append(Vector((r * math.cos(a), r * math.sin(a), z))); cols_.append((*TERRACOTTA, 0.) if r > 3 else (*INSIDE, 0.))
    for k in range(len(rings) - 1):
        for j in range(SEGMENTS):
            j2 = (j + 1) % SEGMENTS
            faces.append((k * SEGMENTS + j2, k * SEGMENTS + j, (k + 1) * SEGMENTS + j, (k + 1) * SEGMENTS + j2))
    return verts, faces, cols_


def make(name, verts, faces, cols_, origin=Vector((0, 0, 0))):
    data = bpy.data.meshes.new(name)
    data.from_pydata([tuple(v - origin) for v in verts], [], faces)
    data.update()
    data.validate()
    attr = data.color_attributes.new('Col', 'FLOAT_COLOR', 'CORNER')
    for poly in data.polygons:
        for li in poly.loop_indices:
            attr.data[li].color = cols_[data.loops[li].vertex_index]
    for poly in data.polygons: poly.use_smooth = True
    obj = bpy.data.objects.new(name, data); bpy.context.collection.objects.link(obj)
    obj.data.materials.append(MAT)
    return obj


def export(obj):
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT / f'{obj.name}.fbx'), use_selection=True, object_types={'MESH'},
                             apply_unit_scale=True, axis_forward='-Y', axis_up='Z', bake_anim=False,
                             mesh_smooth_type='FACE', colors_type='LINEAR')


rng = random.Random(VARIANTS['seed'])
cut_z = {k: [rng.uniform(-1.2, 1.2) for _ in range(8)] for k in (('b1', 'b2') if VARIANT != 'urn' else ('b1', 'b2', 'b3'))}
cut_a = [rng.uniform(-.12, .12) for _ in range(16)]


def make_jitter(band_lo, band_hi):
    def jitter(kind, x, low_side):
        if kind == 'z':   # a wavy cut line round the jar at this band boundary
            key = band_lo if low_side else band_hi
            if key not in cut_z: return 0.
            w = cut_z[key]
            return .6 * w[0] * math.sin(3 * x + w[1]) + .4 * w[2] * math.sin(7 * x + w[3])
        # a crooked vertical crack
        return .08 * math.sin(x * .9 + cut_a[int(abs(x)) % 16] * 20)
    return jitter


# The whole jar: the three bands' patches spanning the full circle, plus the floor.
BANDS = [(.8, 8., 4, 'b0', 'b1'), (8., 16.5, 5, 'b1', 'b2'), (16.5, HEIGHT, 3, 'b2', 'top')]
if VARIANT == 'urn':
    BANDS = [(FOOT, 12., 5, 'b0', 'b1'), (12., 24., 6, 'b1', 'b2'), (24., 34., 5, 'b2', 'b3'), (34., HEIGHT, 3, 'b3', 'top')]
jar_v, jar_f, jar_c = base_disc('floor')
shards = []
index = 0
for z0, z1, count, lo, hi in BANDS:
    offset = rng.uniform(0, math.tau)
    for k in range(count):
        a0 = offset + math.tau * k / count
        a1 = offset + math.tau * (k + 1) / count
        name = f"{VARIANTS['shard']}{index:02d}"
        rows, cols = (5, max(4, 24 // count)) if VARIANT != 'urn' else (12, 96 // count)
        v, f, c = build_patch(name, z0, z1, a0, a1, make_jitter(lo, hi), rows=rows, cols=cols)
        base = len(jar_v)
        jar_v += v; jar_c += c; jar_f += [tuple(i + base for i in face) for face in f]
        centre = sum(v, Vector()) / len(v)
        obj = make(name, v, f, c, origin=centre)
        export(obj)
        shards.append({'name': obj.name, 'offset_cm': [round(centre.x, 3), round(centre.y, 3), round(centre.z, 3)],
                       'outward': [round(x, 4) for x in Vector((centre.x, centre.y, 0)).normalized()]})
        index += 1
# The intact jar is its own seamless lathe (patches would show their cuts).
def lathe():
    verts, faces, cols_ = [], [], []
    rows = 40 if VARIANT != 'urn' else 120
    zs = [HEIGHT * i / rows for i in range(rows + 1)]
    outer = [(z, radius_at(z), True) for z in zs]
    inner = [(z, radius_at(z) - WALL, False) for z in reversed(zs) if z >= FOOT]
    profile = [(0., 0., True)] + outer + inner + [(FOOT, 0., False)]
    for z, r, out in profile:
        for j in range(SEGMENTS):
            a = math.tau * j / SEGMENTS
            verts.append(Vector((r * math.cos(a), r * math.sin(a), z))); cols_.append(colour(z, a, out))
    for k in range(len(profile) - 1):
        for j in range(SEGMENTS):
            j2 = (j + 1) % SEGMENTS
            faces.append((k * SEGMENTS + j, k * SEGMENTS + j2, (k + 1) * SEGMENTS + j2, (k + 1) * SEGMENTS + j))
    return verts, faces, cols_
jar = make(VARIANTS['body'], *lathe())
bpy.ops.object.select_all(action='DESELECT'); jar.select_set(True); bpy.context.view_layer.objects.active = jar
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.remove_doubles(threshold=.001)
bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.object.mode_set(mode='OBJECT')
export(jar)
(OUT / 'shards.json').write_text(json.dumps({'height_cm': HEIGHT, 'max_radius_cm': MAX_RADIUS, 'shards': shards}, indent=1) + '\n', encoding='utf-8')
lines = ['// Generated by Tools/build_clay_jar.py. Do not edit by hand.', '#pragma once', '',
         f"namespace {VARIANTS['header']}", '{',
         f'    constexpr float Height = {HEIGHT:.4f}f;',
         f'    constexpr float Radius = {MAX_RADIUS:.4f}f;   // widest (the belly)',
         f'    constexpr int ShardCount = {len(shards)};',
         '    // Each shard mesh\'s rest offset from the jar origin (its centre), cm.',
         '    constexpr float ShardOffset[][3] = {' + ', '.join('{' + ', '.join(f'{x:.4f}f' for x in s['offset_cm']) + '}' for s in shards) + '};',
         '}', '']
text = '\n'.join(lines)
if not HEADER.exists() or HEADER.read_text(encoding='utf-8') != text:
    HEADER.write_text(text, encoding='utf-8', newline='\n')
print('CHUCK_JAR_READY', VARIANT, f'tris={sum(len(p.vertices) - 2 for p in jar.data.polygons)}', f'shards={len(shards)}', f'size=({jar.dimensions.x:.1f},{jar.dimensions.y:.1f},{jar.dimensions.z:.1f})')

if '--review' in sys.argv:
    review = sys.argv[sys.argv.index('--review') + 1]
    for s in bpy.data.objects:
        if s.name.startswith(VARIANTS['shard']):
            i = int(s.name[-2:]); d = Vector(shards[i]['offset_cm'])
            s.location = Vector((0, 30, 0)) + d + Vector((d.x, d.y, 0)).normalized() * 6
    bpy.ops.mesh.primitive_plane_add(size=400)
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); scene.collection.objects.link(cam)
    cam.location = (110, 15, 45); cam.rotation_euler = (math.radians(72), 0, math.radians(90))
    cam.data.lens = 50; scene.camera = cam
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.color_type = 'VERTEX'; scene.display.shading.light = 'STUDIO'
    scene.render.resolution_x, scene.render.resolution_y = 1200, 700
    scene.render.filepath = review; bpy.ops.render.render(write_still=True)
    print('CHUCK_JAR_REVIEW', review)
