"""Blender 4.5.14: the plaza dwarf's battle axe (user 2026-10-04: "a dwarf NPC by the
smithy, wearing dwarven armor and holding a battle axe. Bearded.").

blender --background --python Tools/build_battle_axe.py [-- --review <dir>]
  -> SourceAssets/NPCs/Props/SM_BattleAxe.fbx and its entry in SourceAssets/NPCs/Props/manifest.json
     (the other props' entries are kept)

A double-bitted (two-bladed) axe sized to a dwarf: 127 cm overall, an oak haft
with an iron pommel, a leather grip wrap where his fist closes (ADockNPC holds
it as the guards hold their spears: butt on the ground beside his foot), and an
iron head of two bearded crescent blades either side of the socket, brass bands
round the socket and a short top spike. Origin at the butt on the ground, the
haft up +Z, the blades along +/-X. Slots Haft (wood), Head (steel), Wrap
(leather), Trim (brass), coloured like the humans' clothing (M_HumanFabric).
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

HAFT = 1.16                  # m: butt to the top of the haft
GRIP = (.66, .86)           # m: the leather wrap; ADockNPC's fist closes on its middle
HEAD_Z = 1.12                # m: the middle of the head
SLOTS = {   # slot: (texture, folder, size of one repeat in cm, tint = linear albedo)
    'Haft': ('brown_planks_03', WOOD, 80., (.3, .19, .1)),
    'Head': ('metal_plate_02', CLOTH, 60., (.3, .3, .31)),
    'Wrap': ('brown_leather', CLOTH, 40., (.045, .03, .02)),
    'Trim': ('metal_plate_02', CLOTH, 60., (.42, .27, .09)),
}


def gain(name, folder, cache={}):
    """1 / mean linear luminance of a colour map (as build_npc_humans.fabric_gain)."""
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


def lathe(bm, rings, sides=14):
    """A capped solid of revolution about Z from (z, radius) rings."""
    loops = [[bm.verts.new((r * math.cos(2 * math.pi * k / sides), r * math.sin(2 * math.pi * k / sides), z)) for k in range(sides)] for z, r in rings]
    for a, b in zip(loops, loops[1:]):
        for k in range(sides): bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    bm.faces.new(list(reversed(loops[0]))); bm.faces.new(loops[-1])


def blade(bm, side):
    """One bearded crescent: a grid from the socket (u = 0) to the edge (u = 1),
    bottom to top (v), thick at the socket, ground thin at the edge."""
    nu, nv = 8, 10
    grid = {}
    for i in range(nu + 1):
        u = i / nu
        # Height of the blade at this distance out: a narrow neck flaring to the crescent edge, the beard hanging lower.
        flare = u ** 1.6
        lo = HEAD_Z - .045 - flare * .13
        hi = HEAD_Z + .045 + flare * .09
        for j in range(nv + 1):
            v = j / nv
            z = lo + (hi - lo) * v
            x = .024 + u * .17 + flare * .03 * math.sin(math.pi * v)          # the edge bows out
            t = .011 * (1 - u) ** 1.3 + .0012                                  # half thickness
            for s in (-1, 1):
                grid[i, j, s] = bm.verts.new((side * x, s * t, z))
    for i in range(nu):
        for j in range(nv):
            for s in (-1, 1):
                q = (grid[i, j, s], grid[i + 1, j, s], grid[i + 1, j + 1, s], grid[i, j + 1, s])
                bm.faces.new(q if s * side > 0 else tuple(reversed(q)))
    for i in range(nu):   # top and bottom
        for j in (0, nv):
            bm.faces.new((grid[i, j, -1], grid[i + 1, j, -1], grid[i + 1, j, 1], grid[i, j, 1]))
    for j in range(nv):   # the edge and the root
        for i in (0, nu):
            bm.faces.new((grid[i, j, -1], grid[i, j + 1, -1], grid[i, j + 1, 1], grid[i, j, 1]))


def box_uv(obj, tile_cm):
    me = obj.data
    while me.uv_layers: me.uv_layers.remove(me.uv_layers[0])
    uv = me.uv_layers.new(name='UVMap')
    s = 100. / tile_cm
    for poly in me.polygons:
        n = poly.normal; ax = max(range(3), key=lambda k: abs(n[k]))
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            u, v = [(co.y, co.z), (co.x, co.z), (co.x, co.y)][ax]
            uv.data[li].uv = (u * s, v * s)


def axe():
    haft = mesh_object('Haft', lambda bm: lathe(bm, [(.05, .016), (.2, .0175), (.6, .017), (HAFT, .0155)]), 'Haft')
    for v in haft.data.vertices: v.co.y *= .85   # oval in section, the long axis along the blades
    wrap = mesh_object('Wrap', lambda bm: lathe(bm, [(GRIP[0], .0185), (GRIP[0] + .005, .0195), (GRIP[1] - .005, .0195), (GRIP[1], .0185)]), 'Wrap')
    for v in wrap.data.vertices:   # the wound leather: a slight spiral ridge
        a = math.atan2(v.co.y, v.co.x)
        v.co.x *= 1 + .06 * math.sin(a + v.co.z * 260); v.co.y *= .87 + .06 * math.sin(a + v.co.z * 260)
    pommel = mesh_object('Pommel', lambda bm: lathe(bm, [(0., .006), (.01, .019), (.035, .021), (.05, .0172), (.058, .0165)]), 'Head')
    def socket(bm):
        lathe(bm, [(HEAD_Z - .07, .021), (HEAD_Z - .06, .027), (HEAD_Z + .06, .027), (HEAD_Z + .07, .021)], 8)
        lathe(bm, [(HEAD_Z + .07, .016), (HEAD_Z + .1, .01), (HEAD_Z + .15, .002)], 8)   # the top spike
        for side in (-1, 1): blade(bm, side)
    head = mesh_object('HeadIron', socket, 'Head')
    bands = mesh_object('Bands', lambda bm: [lathe(bm, [(z, .0295), (z + .012, .0295)], 16) for z in (HEAD_Z - .072, HEAD_Z + .06)], 'Trim')
    parts = [(haft, True), (wrap, True), (pommel, True), (head, False), (bands, True)]
    for o, smooth in parts:
        box_uv(o, SLOTS[o.data.materials[0].name][2])
        for p in o.data.polygons: p.use_smooth = smooth
    bpy.ops.object.select_all(action='DESELECT')
    for o, _ in parts: o.select_set(True)
    bpy.context.view_layer.objects.active = haft
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = 'SM_BattleAxe'
    obj.data.set_sharp_from_angle(angle=math.radians(40))
    return obj


def build():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    obj = axe()
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SM_BattleAxe.fbx'), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
    lo = [round(min(v.co[k] for v in obj.data.vertices) * 100, 1) for k in range(3)]
    hi = [round(max(v.co[k] for v in obj.data.vertices) * 100, 1) for k in range(3)]
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    manifest_path = OUT / 'manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    manifest['props']['BattleAxe'] = dict(fbx='SM_BattleAxe.fbx', length_cm=hi[2], grip_cm=[GRIP[0] * 100, GRIP[1] * 100], tris=tris,
        bounds_min_cm=lo, bounds_max_cm=hi, slots={s: {'type': 'fabric', 'fabric': SLOTS[s][0], 'folder': str(SLOTS[s][1].relative_to(ROOT)).replace('\\', '/'),
        'tile_cm': SLOTS[s][2], 'tint': list(SLOTS[s][3]), 'gain': gain(SLOTS[s][0], SLOTS[s][1])} for s in [m.name for m in obj.data.materials]})
    manifest.setdefault('generators', {})['BattleAxe'] = 'Tools/build_battle_axe.py'
    manifest_path.write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
    print('CHUCK_AXE', f'tris={tris}', 'bounds_cm=' + ' '.join(f'{a}..{b}' for a, b in zip(lo, hi)))
    if REVIEW: review(obj)


def review(obj):
    import os
    for slot, c in (('Haft', (.45, .3, .16)), ('Head', (.45, .45, .47)), ('Wrap', (.12, .08, .05)), ('Trim', (.7, .5, .15))):
        bpy.data.materials[slot].diffuse_color = (*c, 1.)
    s = bpy.context.scene
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam); s.camera = cam
    s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.color_type = 'MATERIAL'; s.display.shading.light = 'STUDIO'
    s.render.resolution_x, s.render.resolution_y = 700, 900
    for tag, loc, aim in (('side', (.3, -1.6, .7), (0, 0, .6)), ('head', (.25, -.55, 1.05), (0, 0, .95))):
        cam.location = loc; cam.data.lens = 40
        cam.rotation_euler = (Vector(aim) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        s.render.filepath = os.path.join(REVIEW, f'axe_{tag}.png'); bpy.ops.render.render(write_still=True)


build()
