"""Blender 4.5.14: the tavern's rubbish bin, out back in the alley (user 2026-11-10: the keeper's night
line has Bobert "rootin' about in me bin out back", so there ought to be one).

blender --background --python Tools/build_tavern_bin.py [-- --review <dir>]
  -> SourceAssets/NPCs/Props/SM_TavernBin.fbx and its entry in SourceAssets/NPCs/Props/manifest.json
     (the other props' entries are kept); import with Tools/Import-NPCHumans.ps1 -Only prop:TavernBin

A period refuse tub: an old cask sawn down and stood on end (62 cm tall, 44 cm across the belly), eighteen
staves with hairline gaps, two of them left long as lugs to lift it by, one cracked short at the rim; three
iron hoops; a boarded bottom. Its plank lid has been pushed off and leans against its side, and the kitchen's
refuse is heaped inside: wet scraps, cabbage leaves, bones and a fish's backbone. A few scraps lie spilled at
its foot where something has been rooting in it.

Origin on the ground under the middle of the bin. It stands against a wall on its -Y side: everything stays
within the belly radius in Y (the lid and the spill lie along X), so it keeps clear of the alley route.
Slots coloured like the humans' props (M_HumanFabric: a texture's grain, greyed, times a tint):
Staves, Hoops, Refuse, Leaves, Bone. Centimetres via FBX unit conversion.
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

HEIGHT, R_END, R_BELLY, STAVE = .62, .195, .22, .022   # metres: rim height, radii (outside), stave thickness
STAVES, GAP = 18, .003
BOTTOM = .035                                           # the bottom head sits this far up inside the chime
HOOP_W, HOOP_T = .03, .005
FILL = HEIGHT - .05                                     # the refuse surface at the rim
SLOTS = {   # slot: (texture, folder, size of one repeat in cm, tint = linear albedo)
    'Staves': ('brown_planks_03', WOOD, 60., (.15, .105, .065)),     # old oak, greyed and grimy
    'Hoops': ('metal_plate_02', CLOTH, 50., (.07, .05, .038)),      # rusted iron
    'Refuse': ('brown_leather', CLOTH, 25., (.075, .056, .034)),    # wet kitchen scraps
    'Leaves': ('rough_linen', CLOTH, 12., (.13, .17, .065)),        # limp cabbage leaves
    'Bone': ('rough_linen', CLOTH, 10., (.33, .29, .21)),           # gnawed bones, a fish's spine
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


def radius(z):
    """Outside radius at height z: a cask's bilge, widest a little below half way."""
    t = min(1., abs(z - HEIGHT * .46) / (HEIGHT * .54))
    return R_BELLY - (R_BELLY - R_END) * t * t


def mesh_object(name, bm, slot):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    obj = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(obj)
    obj.data.materials.append(bpy.data.materials.get(slot) or bpy.data.materials.new(slot))
    return obj


def apply(obj, mod):
    bpy.context.view_layer.objects.active = obj; bpy.ops.object.modifier_apply(modifier=mod.name)


def staves(rnd):
    bm = bmesh.new()
    rows = 11
    half = math.pi / STAVES
    for k in range(STAVES):
        a = 2 * math.pi * k / STAVES + .09
        trim = GAP / 2 / R_BELLY
        lug = k in (0, STAVES // 2)                     # two long staves to lift it by, along X
        cracked = k == 5                                 # one split short at the rim
        z_hi = HEIGHT + (.065 if lug else -.05 if cracked else rnd.uniform(-.004, .004))
        sag = rnd.uniform(-.0015, .0015)
        grid = []
        for i in range(rows):
            z = z_hi * i / (rows - 1)
            r = radius(min(z, HEIGHT)) + sag
            ring = []
            for a_off, rr in ((-half + trim, r), (half - trim, r), (half - trim, r - STAVE), (-half + trim, r - STAVE)):
                zz = z
                if cracked and i == rows - 1: zz += .018 * math.sin(a_off * 41 + rr * 60)   # splintered
                if lug and i == rows - 1: zz -= .012 * abs(a_off) / half                      # rounded ear
                ang = a + a_off
                ring.append(bm.verts.new((rr * math.cos(ang), rr * math.sin(ang), zz)))
            grid.append(ring)
        for g0, g1 in zip(grid, grid[1:]):
            for e in range(4):
                bm.faces.new((g0[e], g0[(e + 1) % 4], g1[(e + 1) % 4], g1[e]))
        bm.faces.new(list(reversed(grid[0]))); bm.faces.new(grid[-1])
    obj = mesh_object('staves', bm, 'Staves')
    bev = obj.modifiers.new('Edges', 'BEVEL'); bev.width = .002; bev.segments = 1; apply(obj, bev)
    return obj


def bottom():
    bm = bmesh.new()
    r = radius(BOTTOM) - STAVE + .003
    bmesh.ops.create_circle(bm, cap_ends=True, radius=r, segments=32)
    for v in bm.verts: v.co.z = BOTTOM
    obj = mesh_object('bottom', bm, 'Staves')
    sol = obj.modifiers.new('Thick', 'SOLIDIFY'); sol.thickness = .02; apply(obj, sol)
    return obj


def hoops():
    bm = bmesh.new()
    sides = 48
    for zc in (.05, .2, HEIGHT - .16, HEIGHT - .045):
        loops = []
        for z in (zc - HOOP_W / 2, zc + HOOP_W / 2):
            r_out = radius(z) + HOOP_T; r_in = radius(z) - .0005
            loops.append([bm.verts.new((r * math.cos(2 * math.pi * s / sides), r * math.sin(2 * math.pi * s / sides), z))
                          for r in (r_in, r_out) for s in range(sides)])
        (a, b) = loops
        for s in range(sides):
            n = (s + 1) % sides
            bm.faces.new((a[sides + s], a[sides + n], b[sides + n], b[sides + s]))
            bm.faces.new((a[s], b[s], b[n], a[n]))
            bm.faces.new((a[s], a[n], a[sides + n], a[sides + s]))
            bm.faces.new((b[s], b[sides + s], b[sides + n], b[n]))
    return mesh_object('hoops', bm, 'Hoops')


def lid(rnd):
    """Three boards on two battens, pushed off and leaning against the +X side, its foot on the ground."""
    bm = bmesh.new()
    r = R_END + .01
    boards = 3
    for b in range(boards):
        y0 = -r + 2 * r * b / boards + .002; y1 = -r + 2 * r * (b + 1) / boards - .002
        ys = [y0 + (y1 - y0) * i / 4 for i in range(5)]
        top = [(math.sqrt(max(0., r * r - y * y)), y) for y in ys]
        outline = [(x, y) for x, y in top] + [(-x, y) for x, y in reversed(top)]
        lo = [bm.verts.new((x, y, 0.)) for x, y in outline]
        hi = [bm.verts.new((x, y, .02)) for x, y in outline]
        bm.faces.new(hi); bm.faces.new(list(reversed(lo)))
        n = len(outline)
        for i in range(n):
            bm.faces.new((lo[i], lo[(i + 1) % n], hi[(i + 1) % n], hi[i]))
    for x in (-.09, .09):   # battens underneath, across the boards
        bmesh.ops.create_cube(bm, size=1., matrix=Matrix.Translation((x, 0, -.012)) @ Matrix.Diagonal((.045, 2 * r * .82, .024, 1)))
    obj = mesh_object('lid', bm, 'Staves')
    # Stand it on its edge leaning back against the bin: tilt about Y, foot out along +X.
    tilt = math.radians(68)
    foot = R_BELLY + .14
    obj.matrix_world = Matrix.Translation((foot - r * math.cos(tilt) * .98, 0, r * math.sin(tilt) + .005)) @ Matrix.Rotation(-tilt, 4, 'Y') @ Matrix.Rotation(.06, 4, 'Z')
    return obj


def blob(bm, centre, size, rnd, segs=8, rings=5, squash=1.):
    """A lumpy ellipsoid (a scrap of food)."""
    cx, cy, cz = centre
    grid = []
    for i in range(rings + 1):
        th = math.pi * i / rings
        row = []
        for j in range(segs):
            ph = 2 * math.pi * j / segs
            w = 1 + rnd.uniform(-.25, .25)
            row.append(bm.verts.new((cx + size[0] * w * math.sin(th) * math.cos(ph), cy + size[1] * w * math.sin(th) * math.sin(ph),
                                     cz + size[2] * squash * w * math.cos(th))))
        grid.append(row)
    for i in range(rings):
        for j in range(segs):
            bm.faces.new((grid[i][j], grid[i][(j + 1) % segs], grid[i + 1][(j + 1) % segs], grid[i + 1][j]))


def refuse(rnd):
    """The heap inside: a domed, lumpy surface at the rim, with scraps piled on it."""
    bm = bmesh.new()
    rings, segs = 7, 28
    r_in = radius(FILL) - STAVE - .002
    centre = bm.verts.new((0, 0, FILL + .06))
    grid = []
    for i in range(1, rings + 1):
        t = i / rings
        row = []
        for j in range(segs):
            a = 2 * math.pi * j / segs
            rr = r_in * t
            z = FILL + .06 * (1 - t * t) + rnd.uniform(-.012, .012) * (1 - t * .7)
            row.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a), z)))
        grid.append(row)
    for j in range(segs):
        bm.faces.new((centre, grid[0][j], grid[0][(j + 1) % segs]))
    for i in range(rings - 1):
        for j in range(segs):
            bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][(j + 1) % segs], grid[i][(j + 1) % segs]))
    for _ in range(14):   # crusts, peel and lumps on top
        a = rnd.uniform(0, 2 * math.pi); d = rnd.uniform(0, r_in * .75)
        x, y = d * math.cos(a), d * math.sin(a)
        z = FILL + .06 * (1 - (d / r_in) ** 2)
        blob(bm, (x, y, z), (rnd.uniform(.018, .035), rnd.uniform(.014, .028), rnd.uniform(.01, .02)), rnd)
    # A few scraps spilled at its foot along the wall, on both sides (Bobert has been at it).
    for x, y in ((-.33, -.05), (-.29, .08), (.42, -.1), (-.38, .02)):
        blob(bm, (x, y, .008), (rnd.uniform(.02, .035), rnd.uniform(.015, .028), .012), rnd)
    return mesh_object('refuse', bm, 'Refuse')


def leaf(bm, centre, length, yaw, droop, rnd, lift=0.):
    """A limp cabbage leaf: a cupped, curling sheet."""
    nu, nv = 6, 5
    cx, cy, cz = centre
    grid = []
    for i in range(nu + 1):
        u = i / nu
        w = length * .42 * math.sin(math.pi * min(1., u * 1.1 + .05))
        row = []
        for j in range(nv + 1):
            v = j / nv - .5
            lx = u * length
            ly = v * 2 * w
            lz = lift * u - droop * u * u + .02 * (v * v) * length * 4 + rnd.uniform(-.002, .002)
            x = cx + lx * math.cos(yaw) - ly * math.sin(yaw)
            y = cy + lx * math.sin(yaw) + ly * math.cos(yaw)
            row.append(bm.verts.new((x, y, cz + lz)))
        grid.append(row)
    for i in range(nu):
        for j in range(nv):
            bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))


def leaves(rnd):
    bm = bmesh.new()
    r_in = radius(FILL) - STAVE
    # On the heap.
    for k in range(6):
        a = rnd.uniform(0, 2 * math.pi); d = rnd.uniform(0, r_in * .55)
        leaf(bm, (d * math.cos(a), d * math.sin(a), FILL + .06 * (1 - (d / r_in) ** 2) + .006), rnd.uniform(.1, .14), rnd.uniform(0, 6.3), .03, rnd)
    # Spilled at the foot.
    for x, y, yaw in ((-.36, .1, 2.6), (.5, -.02, .3)):
        leaf(bm, (x, y, .006), .12, yaw, -.01, rnd)
    obj = mesh_object('leaves', bm, 'Leaves')
    sol = obj.modifiers.new('Thick', 'SOLIDIFY'); sol.thickness = .003; apply(obj, sol)
    return obj


def bone(bm, a, b, r, knob):
    a, b = Vector(a), Vector(b)
    d = b - a
    rot = d.to_track_quat('Z', 'Y').to_matrix().to_4x4()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=8, radius1=r, radius2=r, depth=d.length,
                          matrix=Matrix.Translation((a + b) / 2) @ rot)
    for p in (a, b):
        bmesh.ops.create_uvsphere(bm, u_segments=8, v_segments=5, radius=knob, matrix=Matrix.Translation(p) @ Matrix.Diagonal((1.25, .9, .8, 1)))


def bones(rnd):
    bm = bmesh.new()
    z = FILL + .045
    bone(bm, (-.09, -.04, z + .01), (.07, .03, z + .045), .009, .02)        # a mutton shank on the heap
    bone(bm, (.03, -.11, z - .01), (.12, -.05, z + .005), .006, .013)
    # A fish's backbone across the heap: spine, ribs, the head end.
    s0, s1 = Vector((-.12, .07, z + .01)), Vector((.08, .1, z + .03))
    bmesh.ops.create_cone(bm, cap_ends=True, segments=6, radius1=.004, radius2=.0025, depth=(s1 - s0).length,
                          matrix=Matrix.Translation((s0 + s1) / 2) @ (s1 - s0).to_track_quat('Z', 'Y').to_matrix().to_4x4())
    side = Vector((-(s1 - s0).y, (s1 - s0).x, 0)).normalized()
    for i in range(1, 12):
        p = s0.lerp(s1, i / 12)
        for sgn in (-1, 1):
            q = p + side * sgn * .03 * (1 - abs(i - 5) / 9) + Vector((0, 0, -.006))
            d = q - p
            bmesh.ops.create_cone(bm, cap_ends=False, segments=4, radius1=.0015, radius2=.0006, depth=d.length,
                                  matrix=Matrix.Translation((p + q) / 2) @ d.to_track_quat('Z', 'Y').to_matrix().to_4x4())
    blob(bm, tuple(s0 - (s1 - s0).normalized() * .025), (.03, .018, .016), rnd)   # the head
    # A gnawed bone dropped on the ground beside it.
    bone(bm, (.33, .05, .012), (.47, .1, .016), .008, .016)
    return mesh_object('bones', bm, 'Bone')


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
    rnd = random.Random(1110)
    parts = [staves(rnd), bottom(), hoops(), lid(rnd), refuse(rnd), leaves(rnd), bones(rnd)]
    for o in parts:   # bake each part's placement into its mesh before joining
        o.data.transform(o.matrix_world); o.matrix_world = Matrix.Identity(4)
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts: o.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active; obj.name = 'SM_TavernBin'
    for p in obj.data.polygons: p.use_smooth = True
    obj.data.set_sharp_from_angle(angle=math.radians(40))
    box_uv(obj, {s: v[2] for s, v in SLOTS.items()})
    OUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SM_TavernBin.fbx'), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
    lo = [round(min(v.co[k] for v in obj.data.vertices) * 100, 1) for k in range(3)]
    hi = [round(max(v.co[k] for v in obj.data.vertices) * 100, 1) for k in range(3)]
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    print('CHUCK_TAVERN_BIN', f'tris={tris}', 'bounds_cm=', lo, hi)
    path = OUT / 'manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'props': {}}
    manifest.setdefault('generators', {})
    cm = lambda v: round(v * 100, 1)
    manifest['props']['TavernBin'] = dict(
        fbx='SM_TavernBin.fbx', tris=tris, bounds_min_cm=lo, bounds_max_cm=hi,
        height_cm=cm(HEIGHT), belly_radius_cm=cm(R_BELLY + HOOP_T),
        slots={s: {'type': 'fabric', 'fabric': SLOTS[s][0], 'folder': str(SLOTS[s][1].relative_to(ROOT)).replace('\\', '/'),
                   'tile_cm': SLOTS[s][2], 'tint': list(SLOTS[s][3]), 'gain': gain(SLOTS[s][0], SLOTS[s][1])} for s in SLOTS})
    manifest['generators']['TavernBin'] = 'Tools/build_tavern_bin.py'
    path.write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8', newline='\n')
    if REVIEW:
        import os
        colours = {'Staves': (.36, .27, .19, 1.), 'Hoops': (.2, .16, .13, 1.), 'Refuse': (.17, .14, .1, 1.),
                   'Leaves': (.4, .48, .25, 1.), 'Bone': (.78, .73, .62, 1.)}
        for s, c in colours.items(): bpy.data.materials[s].diffuse_color = c
        s = bpy.context.scene
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam); s.camera = cam
        s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.color_type = 'MATERIAL'; s.display.shading.light = 'STUDIO'
        s.render.resolution_x, s.render.resolution_y = 900, 700
        for tag, loc, target in (('front', (.2, 1.5, .75), (0, 0, .35)), ('top', (.35, .55, 1.25), (0, 0, .55)), ('side', (1.4, .7, .45), (.1, 0, .3))):
            cam.location = loc; cam.data.lens = 40
            cam.rotation_euler = (Vector(target) - cam.location).to_track_quat('-Z', 'Y').to_euler()
            s.render.filepath = os.path.join(REVIEW, f'tavern_bin_{tag}.png'); bpy.ops.render.render(write_still=True)


build()
