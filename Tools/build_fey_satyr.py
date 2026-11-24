"""The maze's marble satyrs (Claude, 2026-11-23): SMK's public-domain scan of the Medici Faun (KAS460; Tools/Fetch-FeySatyr.ps1)
cut down to game meshes. Blender 4.5.14, run in the background:

    blender -b -P Tools/build_fey_satyr.py

The scan's faun laughs (a broad grin, the cheeks raised). The user (2026-11-23): smiling statues along the way through, frowning
ones in the dead ends, real satyrs rather than cartoon heads. So two meshes from the one scan:

  SM_FeySatyr       the faun as he is, grinning.
  SM_FeySatyrFrown  the same faun with his face re-carved to a scowl: the mouth's corners drawn well down and its middle up, the
                    lips pressed toward each other, the inner ends of the brows pulled down and together, the raised cheeks let down.
                    Landmarks were read off an orthographic render of the face (camera below); each is found on the surface along
                    its camera ray, and the vertices round it are moved with a smooth falloff.

Each: stood on the origin, 141.5 cm tall (the cast's height), about 70,000 triangles with the head kept denser (its expression
must read), written to SourceAssets/Props/Feywild/Satyr/<name>.fbx (metres), with review renders of the face beside it.
"""
import math
from pathlib import Path

import bpy
import bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'SourceAssets/Props/Feywild/Satyr'
STL = FOLDER / 'Source/KAS460_small.stl'
HEIGHT = 141.5
TRIS = 70000

# The face camera (scan units, Z up; the scan's own frame): orthographic, 26 units across 800 px.
CAM_LOC = Vector((17.429, -39.994, 83.617))
CAM_RIGHT = Vector((0.9594, -0.2822, 0.0))
CAM_UP = Vector((-0.1278, -0.4344, 0.8916))
CAM_FWD = Vector((0.2516, 0.8554, 0.4528))
PX = 26.0 / 800.0
HEAD = Vector((32.52, 11.33, 110.79))

# (pixel x, pixel y, move along the face's right, up and out, cm in the scan's units, falloff radius)
NOSE = (405, 362, 1.1)   # its tip (pixel x, y) and how far round it is held still
# (the face is about 8 units across: these are fractions of a centimetre)
FROWN = [
    (295, 370, 0.20, -1.05, -0.10, 1.05),    # the mouth's left corner: down, a little in
    (492, 404, -0.10, -0.70, 0.00, 0.70),   # its right corner (the face turns away on this side: smaller)
    (410, 412, 0.00, 0.44, 0.00, 0.88),       # its middle: up, the upper lip pressed down onto it
    (405, 452, 0.00, 0.60, 0.16, 1.00),       # the lower lip and chin: up, pouting
    (340, 335, 0.00, -0.60, -0.30, 1.12),     # the raised left cheek let down
    (495, 352, 0.00, -0.40, -0.20, 0.80),     # and the right
    (385, 258, 0.30, -0.90, 0.16, 0.88),    # the left brow's inner end: down and in
    (455, 276, -0.30, -0.90, 0.16, 0.88),   # the right brow's inner end
    (300, 222, 0.00, 0.30, 0.00, 1.00),       # the left brow's outer end: a little up (the slant of a scowl)
    (550, 292, 0.00, 0.30, 0.00, 1.00),       # the right brow's outer end
]


def load():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.wm.stl_import(filepath=str(STL))
    ob = bpy.context.selected_objects[0]
    bpy.context.view_layer.objects.active = ob
    return ob


def scowl(ob):
    me = ob.data
    bm = bmesh.new(); bm.from_mesh(me); bm.verts.ensure_lookup_table()
    tree = BVHTree.FromBMesh(bm)
    moves = []
    for px, py, dr, du, do, rad in FROWN:
        o = CAM_LOC + CAM_RIGHT * ((px - 400) * PX) + CAM_UP * ((400 - py) * PX)
        hit, nrm, idx, dist = tree.ray_cast(o, CAM_FWD, 200.)
        if hit is None: raise RuntimeError(f'landmark ({px}, {py}) missed the face')
        moves.append((hit, CAM_RIGHT * dr + CAM_UP * du - CAM_FWD * do, rad))
        print('CHUCK_SATYR_LANDMARK (%d,%d) at %s' % (px, py, tuple(round(x, 2) for x in hit)))
    near = [v for v in bm.verts if (v.co - HEAD).length < 16.]
    shift = {v.index: Vector() for v in near}
    for at, mv, rad in moves:
        t0 = (at - CAM_LOC).dot(CAM_FWD)
        for v in near:
            d = (v.co - at).length
            # only the surface at the landmark's own depth (not the face's far edge behind it, which made spikes)
            if d < rad * 2.2: shift[v.index] += mv * math.exp(-(d / rad) ** 2) * math.exp(-(((v.co - CAM_LOC).dot(CAM_FWD) - t0) / .9) ** 2)
    # the nose stays as it is (the cheeks' and mouth's moves dragged its tip into a beak)
    o = CAM_LOC + CAM_RIGHT * ((NOSE[0] - 400) * PX) + CAM_UP * ((400 - NOSE[1]) * PX)
    tip = tree.ray_cast(o, CAM_FWD, 200.)[0]
    if tip is None: raise RuntimeError('the nose missed the face')
    for v in near: v.co += shift[v.index] * (1. - math.exp(-((v.co - tip).length / NOSE[2]) ** 2))
    bm.to_mesh(me); bm.free(); me.update()


def finish(ob, name):
    me = ob.data
    # its feet on the origin, the cast's height
    s = HEIGHT / ob.dimensions.z
    for v in me.vertices: v.co *= s
    vs = [v.co for v in me.vertices]
    lo = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs))); hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    c = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    for v in me.vertices: v.co -= c
    head = HEAD * s - c
    # down to a game mesh, the head kept denser (a vertex group the decimation spares, inverted)
    g = ob.vertex_groups.new(name='Head')
    g.add([v.index for v in me.vertices if (v.co - head).length < 15. * s], 1., 'REPLACE')
    if len(me.polygons) > TRIS:
        m = ob.modifiers.new('Decimate', 'DECIMATE')
        m.ratio = TRIS / len(me.polygons); m.vertex_group = 'Head'; m.invert_vertex_group = True; m.vertex_group_factor = .85
        bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.ops.object.shade_smooth()
    ob.name = name
    print('CHUCK_SATYR_GAME %s tris=%d dims=%s' % (name, len(me.polygons), tuple(round(x, 1) for x in ob.dimensions)))
    # review renders of the face (and the whole)
    sc = bpy.context.scene; sc.render.engine = 'BLENDER_WORKBENCH'; sc.render.resolution_x = sc.render.resolution_y = 700
    sc.display.shading.light = 'STUDIO'; sc.display.shading.show_cavity = True
    bpy.ops.object.camera_add(); cam = bpy.context.object; sc.camera = cam; cam.data.type = 'ORTHO'
    for tag, loc, aim, scale in (('face', head - CAM_FWD * 60., head, 26. * s), ('whole', Vector((0, -400, 75)), Vector((0, 0, 75)), 170.)):
        cam.location = loc; cam.rotation_euler = (aim - loc).to_track_quat('-Z', 'Y').to_euler(); cam.data.ortho_scale = scale
        sc.render.filepath = str(FOLDER / f'{name}_review_{tag}.png'); bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam, do_unlink=True)
    # out in metres (Blender's unit; the importer makes centimetres of them, as for the other props)
    for v in me.vertices: v.co *= .01
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=str(FOLDER / f'{name}.fbx'), use_selection=True, global_scale=1.0, apply_unit_scale=True,
                             axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE', add_leaf_bones=False)


ob = load(); finish(ob, 'SM_FeySatyr')
ob = load(); scowl(ob); finish(ob, 'SM_FeySatyrFrown')
print('CHUCK_SATYR_DONE')
