"""Blender 4.5.14: the guard's spear (user 2026-10-03: "give the guard a spear").

blender --background --python Tools/build_spear.py [-- --review <dir>]
  -> SourceAssets/NPCs/Props/SM_Spear.fbx, SourceAssets/NPCs/Props/manifest.json

A plain town-watch spear, 212 cm: an ash shaft (3.2 cm, slightly thicker at the
grip), a forged leaf blade with a midrib on a socket, a leather wrap where he
holds it, and an iron butt cap. Origin at the butt, shaft along +Z. Three
material slots coloured like the humans' clothing (Tools/import_npc_humans.py,
M_HumanFabric): Shaft (Poly Haven brown_planks_03 grain, greyed and tinted
ash), Head and Butt (metal_plate_02 steel), Wrap (brown_leather).
"""
from pathlib import Path
import json
import math
import sys
import bmesh
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets/NPCs/Props'
CLOTH = ROOT / 'SourceAssets/Surfaces/Cloth'
WOOD = ROOT / 'SourceAssets/Surfaces/PolyHaven'
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None

LENGTH = 2.12          # m, butt to tip
SHAFT_R = .016         # m
GRIP = (1.0, 1.24)     # m above the butt: the leather wrap (his hand is at ~1.08)
SLOTS = {   # slot: (fabric, folder, size of one texture repeat in cm, tint = linear albedo)
    'Shaft': ('brown_planks_03', WOOD, 100., (.32, .22, .13)),
    'Head': ('metal_plate_02', CLOTH, 200., (.3, .3, .31)),
    'Wrap': ('brown_leather', CLOTH, 40., (.05, .035, .024)),
}


def gain(name, folder):
    """1 / mean linear luminance of a colour map (as build_npc_humans.fabric_gain)."""
    import numpy as np
    img = bpy.data.images.load(str(folder / f'{name}_diff_2k.jpg')); img.scale(256, 256)
    a = np.array(img.pixels[:]).reshape(-1, 4)[:, :3]
    lin = np.where(a <= .04045, a / 12.92, ((a + .055) / 1.055) ** 2.4)
    bpy.data.images.remove(img)
    return round(1. / float((lin @ np.array([.2126, .7152, .0722])).mean()), 3)


def lathe(bm, rings, sides=12):
    """A solid of revolution about Z from (z, radius) rings, capped at both ends."""
    loops = []
    for z, r in rings:
        loops.append([bm.verts.new((r * math.cos(2 * math.pi * k / sides), r * math.sin(2 * math.pi * k / sides), z)) for k in range(sides)])
    for a, b in zip(loops, loops[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    bm.faces.new(list(reversed(loops[0]))); bm.faces.new(loops[-1])


def part(name, build, slot):
    me = bpy.data.meshes.new(name); bm = bmesh.new(); build(bm)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    obj = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(obj)
    mat = bpy.data.materials.get(slot) or bpy.data.materials.new(slot)
    obj.data.materials.append(mat)
    return obj


def blade(bm):
    """A leaf blade with a midrib: widest a third of the way up, flat with bevelled edges."""
    base, length, width, thick = LENGTH - .26, .26, .05, .011
    rows = 12
    prev = None
    for i in range(rows + 1):
        t = i / rows
        z = base + t * length
        w = max(width * .5 * math.sin(math.pi * t ** .7) * (1. - t) ** .15, .0008)   # a leaf: widest about a third up
        d = thick * .5 * (1. - .7 * t)
        ring = [bm.verts.new(p) for p in ((w, 0, z), (0, d, z), (-w, 0, z), (0, -d, z))]
        if prev:
            for k in range(4):
                bm.faces.new((prev[k], prev[(k + 1) % 4], ring[(k + 1) % 4], ring[k]))
        else:
            bm.faces.new(list(reversed(ring)))
        prev = ring
    bm.faces.new(prev)


def box_uv(obj, tile_cm, along_z=True):
    """Fabric-style UVs: around the shaft and along it, one repeat per tile_cm."""
    me = obj.data
    uv = me.uv_layers.new(name='UVMap')
    s = 100. / tile_cm
    for poly in me.polygons:
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            u = (math.atan2(co.y, co.x) * SHAFT_R) if along_z else co.x
            uv.data[li].uv = (u * s, co.z * s)


def build():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    r, g0, g1 = SHAFT_R, GRIP[0], GRIP[1]
    shaft = part('Shaft', lambda bm: lathe(bm, [(.03, r * .95), (g0, r * 1.05), (g1, r * 1.05), (LENGTH - .32, r * .92), (LENGTH - .28, r * .9)]), 'Shaft')
    wrap = part('Wrap', lambda bm: lathe(bm, [(g0, r * 1.2), (g0 + .01, r * 1.32), (g1 - .01, r * 1.32), (g1, r * 1.2)]), 'Wrap')
    socket = part('Socket', lambda bm: lathe(bm, [(LENGTH - .34, r * 1.18), (LENGTH - .32, r * 1.25), (LENGTH - .27, r * .95), (LENGTH - .26, r * .6)]), 'Head')
    head = part('Blade', blade, 'Head')
    butt = part('Butt', lambda bm: lathe(bm, [(0., r * .55), (.006, r * 1.05), (.03, r * 1.15), (.036, r * .98)]), 'Head')
    for o, slot in ((shaft, 'Shaft'), (wrap, 'Wrap'), (socket, 'Head'), (head, 'Head'), (butt, 'Head')):
        box_uv(o, SLOTS[slot][2])
        for p in o.data.polygons: p.use_smooth = o is not head
    bpy.ops.object.select_all(action='DESELECT')
    for o in (shaft, wrap, socket, head, butt): o.select_set(True)
    bpy.context.view_layer.objects.active = shaft
    bpy.ops.object.join()
    shaft.name = 'SM_Spear'
    OUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SM_Spear.fbx'), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
    tris = sum(len(p.vertices) - 2 for p in shaft.data.polygons)
    # Other props (Tools/build_smith_props.py) share the manifest: replace only the spear's entry.
    path = OUT / 'manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'props': {}}
    manifest['generator'] = 'Tools/build_spear.py'
    manifest['props']['Spear'] = {
        'fbx': 'SM_Spear.fbx', 'length_cm': LENGTH * 100, 'grip_cm': [GRIP[0] * 100, GRIP[1] * 100], 'tris': tris,
        'slots': {slot: {'type': 'fabric', 'fabric': f, 'folder': str(folder.relative_to(ROOT)).replace('\\', '/'), 'tile_cm': tile,
                         'tint': list(tint), 'gain': gain(f, folder)} for slot, (f, folder, tile, tint) in SLOTS.items()}}
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
    print('CHUCK_SPEAR', f'tris={tris}', f'length_cm={LENGTH * 100:.0f}')
    if REVIEW:
        import os
        for slot, c in (('Shaft', (.45, .3, .18)), ('Wrap', (.12, .08, .05)), ('Head', (.6, .6, .62))):
            bpy.data.materials[slot].diffuse_color = (*c, 1.)
        s = bpy.context.scene
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam); s.camera = cam
        s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.color_type = 'MATERIAL'; s.display.shading.light = 'STUDIO'
        for tag, loc, aim, lens, res in (('whole', (2.6, -.4, 1.1), (0, 0, 1.06), 40, (500, 1000)), ('head', (.08, -.5, 1.98), (0, 0, 1.96), 50, (600, 600))):
            cam.location = loc; cam.data.lens = lens
            cam.rotation_euler = (Vector(aim) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
            s.render.resolution_x, s.render.resolution_y = res
            s.render.filepath = os.path.join(REVIEW, f'spear_{tag}.png'); bpy.ops.render.render(write_still=True)


build()
