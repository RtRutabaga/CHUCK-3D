"""Blender 4.5.14: Bobert's barrel (user 2026-10-05: "Bobert in his barrel asleep
like in the 2d game, positioned by starting point").

blender --background --python Tools/build_bobert_barrel.py [-- --review <dir>]
  -> SourceAssets/NPCs/Props/SM_BobertBarrel.fbx and its entry in
     SourceAssets/NPCs/Props/manifest.json (the other props' entries are kept)

A big old cask (86 cm long, 76 cm across the belly) lying on its side on two
wooden chocks, its front head gone so it's a shelter: Chuck's home and
Bobert's (GAME-BIBLE.md). Twenty-two bowed staves with real thickness and
hairline gaps, one at the mouth broken short; the back head set in its croze;
four iron hoops; an old sack spread over the floor inside to sleep on and a
rolled blanket against the back head that he leans on.

Origin on the ground under the middle of the barrel; the axis runs along X with
the open mouth toward +X (ADockNPC::SpawnBobert faces the barrel and Bobert out
of it). Centimetres via FBX unit conversion. Slots are coloured like the humans'
clothing (M_HumanFabric: a Poly Haven texture's grain, greyed, times a tint):
Staves, Hoops, Sacking, Blanket.

The numbers ADockNPC needs to seat him (inner floor, back head) are written to
the manifest entry; keep BobertBarrel* in DockNPC.cpp in step with them.
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
WOOD = ROOT / 'SourceAssets/Surfaces/PolyHaven'
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None

LENGTH, R_END, R_BELLY, STAVE = .86, .31, .38, .026   # metres: end to end, radii (outside), stave thickness
STAVES, GAP = 22, .0035                                # staves round, the gap between neighbours (m at the belly)
CHIME = .035                                           # stave ends past the head (the croze)
HOOP_W, HOOP_T = .035, .006
BEDROLL_R = .1                                         # the rolled blanket's radius
AXIS_Z = R_BELLY + HOOP_T                              # lying on its hoops and chocks
SLOTS = {   # slot: (texture, folder, size of one repeat in cm, tint = linear albedo)
    'Staves': ('brown_planks_03', WOOD, 70., (.2, .13, .075)),     # old weathered oak
    'Hoops': ('metal_plate_02', CLOTH, 60., (.085, .06, .045)),    # rusted iron
    'Sacking': ('rough_linen', CLOTH, 20., (.2, .16, .1)),         # a grubby old sack
    'Blanket': ('wool_boucle', CLOTH, 35.2, (.11, .08, .07)),      # a moth-eaten brown wool blanket
}


def gain(name, folder, cache={}):
    """1 / mean linear luminance of a texture's colour map (as build_npc_humans measures fabrics)."""
    if name not in cache:
        import numpy as np
        img = bpy.data.images.load(str(folder / f'{name}_diff_2k.jpg')); img.scale(256, 256)
        a = np.array(img.pixels[:]).reshape(-1, 4)[:, :3]
        lin = np.where(a <= .04045, a / 12.92, ((a + .055) / 1.055) ** 2.4)
        bpy.data.images.remove(img)
        cache[name] = round(1. / float((lin @ np.array([.2126, .7152, .0722])).mean()), 3)
    return cache[name]


def radius(x):
    """Outside radius at x along the axis (0 at the middle): a cask's bilge."""
    t = min(1., abs(x) / (LENGTH / 2))
    return R_BELLY - (R_BELLY - R_END) * t * t


def inner_floor(x):
    """Height of the inside floor (top of the bottom stave) at x."""
    return AXIS_Z - (radius(x) - STAVE)


def mesh_object(name, bm, slot):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    obj = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(obj)
    obj.data.materials.append(bpy.data.materials.get(slot) or bpy.data.materials.new(slot))
    return obj


def staves(rnd):
    bm = bmesh.new()
    rows = 13
    half = math.pi / STAVES
    for k in range(STAVES):
        a = 2 * math.pi * k / STAVES
        trim = GAP / 2 / R_BELLY
        # The broken one: the top stave at the mouth snapped off short, a jagged end.
        broken = k == STAVES // 4
        x_hi = LENGTH / 2 - (.11 if broken else 0.)
        x_lo = -LENGTH / 2
        sag = rnd.uniform(-.002, .002)   # staves never sit perfectly true
        grid = []
        for i in range(rows):
            x = x_lo + (x_hi - x_lo) * i / (rows - 1)
            r = radius(x) + sag
            ring = []
            for a_off, rr in ((-half + trim, r), (half - trim, r), (half - trim, r - STAVE), (-half + trim, r - STAVE)):
                ang = a + a_off
                xx = x
                if broken and i == rows - 1: xx += .02 * math.sin(a_off * 37 + rr * 50)   # splintered
                ring.append(bm.verts.new((xx, rr * math.cos(ang), AXIS_Z + rr * math.sin(ang))))
            grid.append(ring)
        for g0, g1 in zip(grid, grid[1:]):
            for e in range(4):
                bm.faces.new((g0[e], g0[(e + 1) % 4], g1[(e + 1) % 4], g1[e]))
        bm.faces.new(list(reversed(grid[0]))); bm.faces.new(grid[-1])
    obj = mesh_object('staves', bm, 'Staves')
    bev = obj.modifiers.new('Edges', 'BEVEL'); bev.width = .002; bev.segments = 1
    bpy.context.view_layer.objects.active = obj; bpy.ops.object.modifier_apply(modifier=bev.name)
    return obj


def back_head():
    """The closed head: boards across a disc set in the croze."""
    bm = bmesh.new()
    x = -LENGTH / 2 + CHIME
    r = radius(x) - STAVE + .004
    boards = 6
    for b in range(boards):
        y0 = -r + 2 * r * b / boards + .002; y1 = -r + 2 * r * (b + 1) / boards - .002
        ys = [y0 + (y1 - y0) * i / 4 for i in range(5)]
        top = [(y, math.sqrt(max(0., r * r - y * y))) for y in ys]
        outline = [(y, z) for y, z in top] + [(y, -z) for y, z in reversed(top)]
        front = [bm.verts.new((x + .022, y, AXIS_Z + z)) for y, z in outline]
        back = [bm.verts.new((x, y, AXIS_Z + z)) for y, z in outline]
        bm.faces.new(front); bm.faces.new(list(reversed(back)))
        n = len(outline)
        for i in range(n):
            bm.faces.new((front[i], back[i], back[(i + 1) % n], front[(i + 1) % n]))
    return mesh_object('head', bm, 'Staves')


def hoops():
    bm = bmesh.new()
    sides = 48
    for xc in (-LENGTH / 2 + .06, -LENGTH / 2 + .19, LENGTH / 2 - .19, LENGTH / 2 - .06):
        loops = []
        for x in (xc - HOOP_W / 2, xc + HOOP_W / 2):
            r_out = radius(x) + HOOP_T; r_in = radius(x) - .0005
            loops.append([bm.verts.new((x, r * math.cos(2 * math.pi * s / sides), AXIS_Z + r * math.sin(2 * math.pi * s / sides)))
                          for r in (r_in, r_out) for s in range(sides)])
        (a, b) = loops
        for s in range(sides):
            n = (s + 1) % sides
            bm.faces.new((a[sides + s], a[sides + n], b[sides + n], b[sides + s]))   # outside
            bm.faces.new((a[s], b[s], b[n], a[n]))                                   # inside
            bm.faces.new((a[s], a[n], a[sides + n], a[sides + s]))                   # edges
            bm.faces.new((b[s], b[sides + s], b[sides + n], b[n]))
    return mesh_object('hoops', bm, 'Hoops')


def chocks():
    """Two wedges each side under the belly so it can't roll."""
    bm = bmesh.new()
    for xc in (-.2, .2):
        for side in (-1, 1):
            y_in = side * .2; y_out = side * .36
            z_in = AXIS_Z - math.sqrt(max(0., radius(xc) ** 2 - .2 ** 2)) + .004
            pts = [(y_out, 0.), (y_in, 0.), (y_in, z_in), (y_out, .015)]
            near = [bm.verts.new((xc - .04, y, z)) for y, z in pts]
            far = [bm.verts.new((xc + .04, y, z)) for y, z in pts]
            bm.faces.new(near); bm.faces.new(list(reversed(far)))
            for i in range(4):
                bm.faces.new((near[i], far[i], far[(i + 1) % 4], near[(i + 1) % 4]))
    return mesh_object('chocks', bm, 'Staves')


def sacking(rnd):
    """An old sack spread on the floor from the back head to near the mouth, rumpled."""
    bm = bmesh.new()
    nx, ny = 22, 12
    x0, x1 = -LENGTH / 2 + CHIME + .03, LENGTH / 2 - .14
    grid = []
    for i in range(nx + 1):
        x = x0 + (x1 - x0) * i / nx
        r_in = radius(x) - STAVE - .006
        row = []
        for j in range(ny + 1):
            u = j / ny - .5
            ang = -math.pi / 2 + u * 1.5                     # across the bottom of the inside, up the sides a little
            ripple = .006 * math.sin(x * 31 + u * 7) * math.sin(u * 13 + x * 9) + rnd.uniform(-.0015, .0015)
            rr = r_in - max(0., ripple)
            row.append(bm.verts.new((x + .01 * math.sin(u * 9 + i), rr * math.cos(ang), AXIS_Z + rr * math.sin(ang))))
        grid.append(row)
    for i in range(nx):
        for j in range(ny):
            bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
    obj = mesh_object('sacking', bm, 'Sacking')
    sol = obj.modifiers.new('Thick', 'SOLIDIFY'); sol.thickness = .004; sol.offset = 1.
    bpy.context.view_layer.objects.active = obj; bpy.ops.object.modifier_apply(modifier=sol.name)
    return obj


def bedroll(rnd):
    """A blanket rolled into a bolster across the back of the barrel, squashed
    where he leans on it (BEDROLL_R thick, against the back head)."""
    bm = bmesh.new()
    x_back = -LENGTH / 2 + CHIME + .022
    xc = x_back + BEDROLL_R
    z0 = inner_floor(xc) + BEDROLL_R * .8
    rings, sides = 14, 16
    width = 2 * (radius(xc) - STAVE) * .8
    grid = []
    for i in range(rings + 1):
        y = -width / 2 + width * i / rings
        taper = math.sqrt(max(.15, 1 - (2 * y / width) ** 6))
        row = []
        for k in range(sides):
            a = 2 * math.pi * k / sides
            wob = 1 + .06 * math.sin(a * 3 + y * 17) + rnd.uniform(-.02, .02)
            r = BEDROLL_R * taper * wob
            row.append(bm.verts.new((xc + r * math.cos(a) * .85, y, z0 + r * math.sin(a) * .8)))
        grid.append(row)
    for i in range(rings):
        for k in range(sides):
            bm.faces.new((grid[i][k], grid[i][(k + 1) % sides], grid[i + 1][(k + 1) % sides], grid[i + 1][k]))
    bm.faces.new(list(reversed(grid[0]))); bm.faces.new(grid[-1])
    return mesh_object('bedroll', bm, 'Blanket')


def box_uv(obj, tile_cm_for_slot):
    me = obj.data
    while me.uv_layers: me.uv_layers.remove(me.uv_layers[0])
    uv = me.uv_layers.new(name='UVMap')
    for poly in me.polygons:
        s = 100. / tile_cm_for_slot[me.materials[poly.material_index].name]
        n = poly.normal; ax = max(range(3), key=lambda k: abs(n[k]))
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            uv.data[li].uv = [(co.y * s, co.z * s), (co.x * s, co.z * s), (co.x * s, co.y * s)][ax]


def build():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    rnd = random.Random(1005)
    parts = [staves(rnd), back_head(), hoops(), chocks(), sacking(rnd), bedroll(rnd)]
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts: o.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active; obj.name = 'SM_BobertBarrel'
    for p in obj.data.polygons: p.use_smooth = True
    obj.data.set_sharp_from_angle(angle=math.radians(40))
    box_uv(obj, {s: v[2] for s, v in SLOTS.items()})
    OUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SM_BobertBarrel.fbx'), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
    lo = [round(min(v.co[k] for v in obj.data.vertices) * 100, 1) for k in range(3)]
    hi = [round(max(v.co[k] for v in obj.data.vertices) * 100, 1) for k in range(3)]
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    print('CHUCK_BOBERT_BARREL', f'tris={tris}', 'bounds_cm=', lo, hi)
    path = OUT / 'manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'props': {}}
    manifest.setdefault('generators', {})
    cm = lambda v: round(v * 100, 1)
    manifest['props']['BobertBarrel'] = dict(
        fbx='SM_BobertBarrel.fbx', tris=tris, bounds_min_cm=lo, bounds_max_cm=hi,
        length_cm=cm(LENGTH), belly_radius_cm=cm(R_BELLY), end_radius_cm=cm(R_END), stave_cm=cm(STAVE), axis_z_cm=cm(AXIS_Z),
        back_head_inner_x_cm=cm(-LENGTH / 2 + CHIME + .022), mouth_x_cm=cm(LENGTH / 2), bedroll_front_x_cm=cm(-LENGTH / 2 + CHIME + .022 + BEDROLL_R * 1.85),
        inner_floor_cm={str(x): cm(inner_floor(x / 100.)) for x in (-40, -30, -20, -10, 0, 10, 20, 30, 40)},
        slots={s: {'type': 'fabric', 'fabric': SLOTS[s][0], 'folder': str(SLOTS[s][1].relative_to(ROOT)).replace('\\', '/'),
                   'tile_cm': SLOTS[s][2], 'tint': list(SLOTS[s][3]), 'gain': gain(SLOTS[s][0], SLOTS[s][1])} for s in SLOTS})
    manifest['generators']['BobertBarrel'] = 'Tools/build_bobert_barrel.py'
    path.write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
    if REVIEW:
        import os
        colours = {'Staves': (.42, .3, .2, 1.), 'Hoops': (.2, .17, .15, 1.), 'Sacking': (.45, .38, .28, 1.), 'Blanket': (.3, .22, .18, 1.)}
        for s, c in colours.items(): bpy.data.materials[s].diffuse_color = c
        s = bpy.context.scene
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam); s.camera = cam
        s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.color_type = 'MATERIAL'; s.display.shading.light = 'STUDIO'
        s.render.resolution_x, s.render.resolution_y = 900, 700
        for tag, loc in (('front', (1.9, -.9, .55)), ('back', (-1.6, 1.2, 1.1))):
            cam.location = loc; cam.data.lens = 40
            cam.rotation_euler = (Vector((0, 0, .35)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
            s.render.filepath = os.path.join(REVIEW, f'bobert_barrel_{tag}.png'); bpy.ops.render.render(write_still=True)


build()
