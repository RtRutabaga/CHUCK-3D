"""Blender 4.5.14: the tavern keeper's pewter tankard and polishing rag (user
2026-10-04: "have him polishing a tankard").

blender --background --python Tools/build_keeper_props.py [-- --review <dir>]
  -> SourceAssets/NPCs/Props/SM_Tankard.fbx, SM_Rag.fbx and their entries in
     SourceAssets/NPCs/Props/manifest.json (the other props' entries are kept)

Slots are coloured like the humans' clothing (M_HumanFabric: a Poly Haven
texture's grain, greyed, times a tint).

Tankard: a pint pewter tankard, hollow (the rag goes in), 13 cm tall, a moulded
foot and rim, a strap handle. Origin at the middle of the handle where his fist
closes on it; the tankard stands up +Z, its body toward +X from the handle.
Slot Pewter.

Rag: a bunched linen cloth, its tail hanging down. Origin in the middle of the
bunch (his fist is in it). Slot Cloth.
"""
from pathlib import Path
import json
import math
import random
import sys
import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets/NPCs/Props'
CLOTH = ROOT / 'SourceAssets/Surfaces/Cloth'
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None

HEIGHT, R_FOOT, R_RIM, WALL = .13, .052, .044, .003
GRIP = Vector((-.071, 0., .075))   # the handle's middle, from the base centre (the origin is moved here)
SLOTS = {   # slot: (texture, folder, size of one repeat in cm, tint = linear albedo)
    'Pewter': ('metal_plate_02', CLOTH, 40., (.15, .15, .155)),   # dark, soft-sheened pewter (a light tint mirrors the walls like glass)
    'Cloth': ('rough_linen', CLOTH, 27.1, (.34, .31, .26)),   # a working rag, not fresh linen
}


def gain(name, folder, cache={}):
    if name not in cache:
        import numpy as np
        img = bpy.data.images.load(str(folder / f'{name}_diff_2k.jpg')); img.scale(256, 256)
        a = np.array(img.pixels[:]).reshape(-1, 4)[:, :3]
        lin = np.where(a <= .04045, a / 12.92, ((a + .055) / 1.055) ** 2.4)
        bpy.data.images.remove(img)
        cache[name] = round(1. / float((lin @ np.array([.2126, .7152, .0722])).mean()), 3)
    return cache[name]


def mesh_object(name, build, slot):
    me = bpy.data.meshes.new(name); bm = bmesh.new(); build(bm)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    obj = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(obj)
    obj.data.materials.append(bpy.data.materials.get(slot) or bpy.data.materials.new(slot))
    return obj


def profile_lathe(bm, profile, sides=24):
    """An open solid of revolution about Z through (radius, z) points (the
    profile runs out round the foot, up the outside, over the rim and down
    the inside to the floor), closed at both ends' axis."""
    loops = [[bm.verts.new((r * math.cos(2 * math.pi * k / sides), r * math.sin(2 * math.pi * k / sides), z)) for k in range(sides)] for r, z in profile]
    for a, b in zip(loops, loops[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    bm.faces.new(list(reversed(loops[0]))); bm.faces.new(loops[-1])


def tube(bm, pts, radii, sides=8, flat=1.):
    loops = []
    for i, (p, r) in enumerate(zip(pts, radii)):
        d = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        side = d.cross(Vector((0, 1, 0)))
        if side.length < 1e-4: side = Vector((1, 0, 0))
        side.normalize(); up = side.cross(d).normalized()
        loops.append([bm.verts.new(p + side * (r * flat * math.cos(2 * math.pi * k / sides)) + up * (r * math.sin(2 * math.pi * k / sides))) for k in range(sides)])
    for a, b in zip(loops, loops[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    bm.faces.new(list(reversed(loops[0]))); bm.faces.new(loops[-1])


def box_uv(obj, tile_cm):
    me = obj.data
    while me.uv_layers: me.uv_layers.remove(me.uv_layers[0])
    uv = me.uv_layers.new(name='UVMap')
    s = 100. / tile_cm
    for poly in me.polygons:
        n = poly.normal; ax = max(range(3), key=lambda k: abs(n[k]))
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            uv.data[li].uv = [(co.y * s, co.z * s), (co.x * s, co.z * s), (co.x * s, co.y * s)][ax]


def join(name, objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active; obj.name = name
    return obj


def tankard():
    # Foot moulding, a gently tapering body with two incised rings, a rolled rim, then the inside down to the floor.
    taper = lambda z: R_FOOT - (R_FOOT - R_RIM) * (z / HEIGHT)
    outside = [(.0, .0), (R_FOOT + .004, .0), (R_FOOT + .005, .004), (R_FOOT + .002, .008), (taper(.012), .012),
               (taper(.03), .03), (taper(.032) - .0012, .032), (taper(.035), .035),
               (taper(.1), .1), (taper(.102) - .0012, .102), (taper(.105), .105),
               (R_RIM + .0005, HEIGHT - .006), (R_RIM + .0025, HEIGHT - .002), (R_RIM + .0018, HEIGHT + .001)]
    inside = [(R_RIM - WALL + .0005, HEIGHT + .0005), (R_RIM - WALL, HEIGHT - .006), (taper(.02) - WALL, .02), (taper(.012) - WALL - .004, .009), (.0, .009)]
    body = mesh_object('Body', lambda bm: profile_lathe(bm, outside + inside), 'Pewter')
    # A strap handle: off the upper body, out and down to the lower body, flattened.
    handle_pts = [Vector(p) for p in ((-R_RIM - .001, 0, .112), (-.06, 0, .116), (-.074, 0, .1), (-.075, 0, .07), (-.068, 0, .045), (-.054, 0, .032), (-R_FOOT + .004, 0, .03))]
    handle = mesh_object('Handle', lambda bm: tube(bm, handle_pts, [.0045, .0055, .0055, .005, .005, .0045, .004], 8, 1.8), 'Pewter')
    obj = join('SM_Tankard', [body, handle])
    for p in obj.data.polygons: p.use_smooth = True
    obj.data.set_sharp_from_angle(angle=math.radians(50))
    box_uv(obj, SLOTS['Pewter'][2])
    # The origin at his grip on the handle.
    obj.data.transform(Matrix.Translation(-GRIP))
    return obj


def rag():
    rnd = random.Random(11)
    def build(bm):
        # The bunch: a lumpy ball round his fist.
        bmesh.ops.create_icosphere(bm, subdivisions=3, radius=.042)
        for v in bm.verts:
            n = v.co.normalized()
            v.co = Vector((v.co.x * 1.15, v.co.y * .95, v.co.z * .85)) * (1 + .18 * math.sin(n.x * 9 + 1) * math.sin(n.y * 7) * math.sin(n.z * 11 + .4))
        # The tail: a soft strip falling from the bunch, rippled, with a little thickness.
        rows, cols = 9, 5
        grid = []
        for i in range(rows):
            t = i / (rows - 1)
            row = []
            for j in range(cols):
                u = j / (cols - 1) - .5
                x = u * .075 * (1 + .25 * t) + .006 * math.sin(t * 7 + u * 4)
                y = .028 + .012 * math.sin(u * 6 + t * 5) + .02 * t
                row.append(bm.verts.new((x, y, -.02 - .15 * t)))
            grid.append(row)
        for i in range(rows - 1):
            for j in range(cols - 1):
                bm.faces.new((grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j]))
    obj = mesh_object('Rag', build, 'Cloth')
    sol = obj.modifiers.new('Thick', 'SOLIDIFY'); sol.thickness = .003
    bpy.context.view_layer.objects.active = obj; bpy.ops.object.modifier_apply(modifier=sol.name)
    for p in obj.data.polygons: p.use_smooth = True
    box_uv(obj, SLOTS['Cloth'][2])
    obj.name = 'SM_Rag'
    return obj


def export(obj, name):
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT / f'{name}.fbx'), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
    lo = [round(min(v.co[k] for v in obj.data.vertices) * 100, 1) for k in range(3)]
    hi = [round(max(v.co[k] for v in obj.data.vertices) * 100, 1) for k in range(3)]
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    print('CHUCK_KEEPER_PROP', name, f'tris={tris}', 'bounds_cm=', lo, hi)
    return tris, lo, hi


def build():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / 'manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'props': {}}
    manifest.setdefault('generators', {})
    made = {}
    for name, fn, extra in (('Tankard', tankard, {'grip_cm': [round(-GRIP.x * 100, 1), 0., round(-GRIP.z * 100, 1)], 'height_cm': HEIGHT * 100,
                                                  'mouth_cm': [round(-GRIP.x * 100, 1), 0., round((HEIGHT - GRIP.z) * 100, 1)]}),
                            ('Rag', rag, {})):
        obj = fn()
        tris, lo, hi = export(obj, f'SM_{name}')
        slots = [m.name for m in obj.data.materials]
        manifest['props'][name] = dict(fbx=f'SM_{name}.fbx', tris=tris, bounds_min_cm=lo, bounds_max_cm=hi, **extra, slots={
            s: {'type': 'fabric', 'fabric': SLOTS[s][0], 'folder': str(SLOTS[s][1].relative_to(ROOT)).replace('\\', '/'),
                'tile_cm': SLOTS[s][2], 'tint': list(SLOTS[s][3]), 'gain': gain(SLOTS[s][0], SLOTS[s][1])} for s in slots})
        manifest['generators'][name] = 'Tools/build_keeper_props.py'
        made[name] = obj
    path.write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
    if REVIEW:
        import os
        bpy.data.materials['Pewter'].diffuse_color = (.55, .56, .58, 1.); bpy.data.materials['Cloth'].diffuse_color = (.8, .77, .7, 1.)
        made['Rag'].location = (.13, .0, .1)
        s = bpy.context.scene
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam); s.camera = cam
        s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.color_type = 'MATERIAL'; s.display.shading.light = 'STUDIO'
        s.render.resolution_x, s.render.resolution_y = 800, 600
        cam.location = (.12, -.42, .2); cam.data.lens = 50
        cam.rotation_euler = (Vector((.08, 0, .02)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
        s.render.filepath = os.path.join(REVIEW, 'keeper_props.png'); bpy.ops.render.render(write_still=True)


build()
