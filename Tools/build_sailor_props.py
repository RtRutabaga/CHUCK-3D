"""Blender 4.5.14: the old sailor's pipe (user 2026-10-05: "an older sailor NPC
on the pier, smoking a pipe").

blender --background --python Tools/build_sailor_props.py [-- --review <dir>]
  -> SourceAssets/NPCs/Props/SM_Pipe.fbx and its entry in
     SourceAssets/NPCs/Props/manifest.json (the other props' entries are kept)

A half-bent briar pipe: a dark vulcanite bit and stem running forward and
down from the mouth, a briar shank, and the bowl rising at the end with
glowing tobacco in it. Origin at the bit's tip (it sits in the corner of
his mouth); the stem runs +X, the bowl's mouth opens +Z at BOWL. Slots Briar
and Bit (M_HumanFabric, tinted grain) and Hot (the runtime gives it the
forge's ember material).
"""
from pathlib import Path
import json
import math
import sys
import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets/NPCs/Props'
CLOTH = ROOT / 'SourceAssets/Surfaces/Cloth'
WOOD = ROOT / 'SourceAssets/Surfaces/PolyHaven'
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None

BOWL = Vector((.118, 0., -.018))   # the top centre of the bowl, from the bit's tip (m)
SLOTS = {   # slot: (texture, folder, size of one repeat in cm, tint = linear albedo)
    'Briar': ('brown_planks_03', WOOD, 12., (.16, .065, .03)),
    'Bit': ('brown_leather', CLOTH, 20., (.012, .011, .01)),
    'Hot': ('metal_plate_02', CLOTH, 20., (.6, .15, .02)),
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


def tube(bm, pts, radii, sides=10, flat=1.):
    loops = []
    for i, (p, r) in enumerate(zip(pts, radii)):
        d = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        side = d.cross(Vector((0, 0, 1)))
        if side.length < 1e-4: side = Vector((0, 1, 0))
        side.normalize(); up = side.cross(d).normalized()
        loops.append([bm.verts.new(p + side * (r * math.cos(2 * math.pi * k / sides)) + up * (r * flat * math.sin(2 * math.pi * k / sides))) for k in range(sides)])
    for a, b in zip(loops, loops[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    bm.faces.new(list(reversed(loops[0]))); bm.faces.new(loops[-1])


def lathe(bm, profile, at, sides=16):
    """Open profile (radius, z) about Z through `at`, both ends capped."""
    loops = [[bm.verts.new(at + Vector((r * math.cos(2 * math.pi * k / sides), r * math.sin(2 * math.pi * k / sides), z))) for k in range(sides)] for r, z in profile]
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


def pipe():
    # The bit and stem: flattened where the teeth hold it, rounding and sloping down to the shank.
    bit = mesh_object('Bit', lambda bm: tube(bm, [Vector(p) for p in ((0, 0, 0), (.012, 0, -.001), (.03, 0, -.004), (.05, 0, -.009), (.062, 0, -.012))],
                                             [.0035, .0045, .0048, .0052, .0055], 10, .6), 'Bit')
    # The briar shank, bending up into the bowl's heel.
    shank = mesh_object('Shank', lambda bm: tube(bm, [Vector(p) for p in ((.061, 0, -.012), (.08, 0, -.018), (.095, 0, -.026), (.105, 0, -.034))],
                                                 [.0062, .0068, .0075, .0085], 12), 'Briar')
    # The bowl: a rounded briar cup, hollow at the top; the tobacco a little below the rim.
    c = Vector((BOWL.x, 0, 0))
    top = BOWL.z
    bowl = mesh_object('Bowl', lambda bm: lathe(bm, [(.0, top - .046), (.009, top - .045), (.014, top - .04), (.0165, top - .03), (.0172, top - .015),
                                                     (.0165, top - .003), (.0155, top), (.0105, top), (.0098, top - .006), (.0, top - .009)], c), 'Briar')
    ember = mesh_object('Ember', lambda bm: lathe(bm, [(.0, top - .0085), (.0096, top - .0085), (.0096, top - .0075), (.0, top - .0072)], c, 12), 'Hot')
    objs = [bit, shank, bowl, ember]
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        box_uv(o, SLOTS[o.data.materials[0].name][2])
        for p in o.data.polygons: p.use_smooth = True
        o.select_set(True)
    bpy.context.view_layer.objects.active = bit
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active; obj.name = 'SM_Pipe'
    obj.data.set_sharp_from_angle(angle=math.radians(55))
    return obj


def build():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    obj = pipe()
    OUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SM_Pipe.fbx'), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    path = OUT / 'manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'props': {}}
    manifest.setdefault('generators', {})['Pipe'] = 'Tools/build_sailor_props.py'
    manifest['props']['Pipe'] = dict(fbx='SM_Pipe.fbx', tris=tris, bowl_cm=[round(BOWL.x * 100, 1), 0., round(BOWL.z * 100, 1)], slots={
        s: {'type': 'fabric', 'fabric': SLOTS[s][0], 'folder': str(SLOTS[s][1].relative_to(ROOT)).replace('\\', '/'),
            'tile_cm': SLOTS[s][2], 'tint': list(SLOTS[s][3]), 'gain': gain(SLOTS[s][0], SLOTS[s][1])} for s in [m.name for m in obj.data.materials]})
    path.write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
    print('CHUCK_SAILOR_PROP SM_Pipe', f'tris={tris}', 'bowl_cm', manifest['props']['Pipe']['bowl_cm'])
    if REVIEW:
        import os
        for slot, col in (('Briar', (.45, .2, .1)), ('Bit', (.05, .05, .05)), ('Hot', (1., .4, .05))):
            bpy.data.materials[slot].diffuse_color = (*col, 1.)
        s = bpy.context.scene
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam); s.camera = cam
        s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.color_type = 'MATERIAL'; s.display.shading.light = 'STUDIO'
        s.render.resolution_x, s.render.resolution_y = 800, 500
        cam.location = (.07, -.2, .05); cam.data.lens = 50
        cam.rotation_euler = (Vector((.065, 0, -.02)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
        s.render.filepath = os.path.join(REVIEW, 'pipe.png'); bpy.ops.render.render(write_still=True)


build()
