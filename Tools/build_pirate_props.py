"""Blender 4.5.14: the pirate crew's hand props (Claude, 2026-11-02; docs/SHIP.md, milestone 3).

blender --background --python Tools/build_pirate_props.py [-- --review <dir>]
  -> SourceAssets/NPCs/Props/SM_<Name>.fbx and their entries in SourceAssets/NPCs/Props/manifest.json
     (the other props' entries are kept); imported by Tools/Import-NPCHumans.ps1 -Only prop:<Name>

Slots are coloured like the humans' clothing (M_HumanFabric: a Poly Haven texture's grain, greyed, times a tint).

Every hand prop shares one frame, its origin in the middle of the fist that holds it:
  +X  along the grip, toward the blade (in the hand: the thumb's side of the fist);
  -Z  toward the edge (in the hand: the knuckles' side; a guard's knuckle bow is there).

  Cutlass       a sailor's cutlass: a 64 cm curved single-edged blade widening to a clipped point, an
                iron shell guard with a knuckle bow, a cord-bound wooden grip and a cap pommel.
  Cleaver       the chef's cleaver: a 21 x 9.5 cm blade on a riveted wooden handle.
  ConcertinaEnd one hexagonal end of the concertina (16 cm across): a wooden box with a fretted
                grille, ivory buttons and a leather hand strap. The origin is the player's fist under
                the strap; the box runs out along +X to the face the bellows join (`face_cm`).
  ConcertinaBellows the bellows between the ends: seven hexagonal folds over 10 cm of +X from the origin
                (the runtime stretches it along X between the two ends' faces).
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
PLANKS = ROOT / 'SourceAssets/Surfaces/PolyHaven'
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None
SLOTS = {   # slot: (texture, folder, size of one repeat in cm, tint = linear albedo)
    'Steel': ('metal_plate_02', CLOTH, 60., (.32, .32, .33)),
    'Iron': ('metal_plate_02', CLOTH, 40., (.09, .09, .095)),
    'Brass': ('metal_plate_02', CLOTH, 40., (.42, .3, .1)),
    'Wood': ('brown_planks_03', PLANKS, 60., (.2, .12, .07)),
    'Cord': ('rough_linen', CLOTH, 12., (.07, .05, .035)),
    'Leather': ('brown_leather', CLOTH, 30., (.06, .03, .02)),
    'Bellows': ('brown_leather', CLOTH, 30., (.13, .02, .018)),
    'Ivory': ('rough_linen', CLOTH, 50., (.62, .58, .48)),
    'Grille': ('metal_plate_02', CLOTH, 40., (.035, .03, .025)),
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


def material(slot):
    return bpy.data.materials.get(slot) or bpy.data.materials.new(slot)


def mesh_object(name, build, slot, smooth=True):
    me = bpy.data.meshes.new(name); bm = bmesh.new(); build(bm)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    obj = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material(slot))
    for p in obj.data.polygons: p.use_smooth = smooth
    box_uv(obj, SLOTS[slot][2])
    return obj


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


def tube(bm, pts, radii, sides=8, flat=1., up_hint=Vector((0, 1, 0))):
    loops = []
    for i, (p, r) in enumerate(zip(pts, radii)):
        d = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        side = d.cross(up_hint)
        if side.length < 1e-4: side = d.cross(Vector((1, 0, 0)))
        side.normalize(); up = side.cross(d).normalized()
        loops.append([bm.verts.new(p + side * (r * flat * math.cos(2 * math.pi * k / sides)) + up * (r * math.sin(2 * math.pi * k / sides))) for k in range(sides)])
    for a, b in zip(loops, loops[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    bm.faces.new(list(reversed(loops[0]))); bm.faces.new(loops[-1])


def lathe_x(bm, profile, sides=16, x0=0.):
    """A solid of revolution about X through (x, radius) points, closed at both ends."""
    loops = [[bm.verts.new((x0 + x, r * math.cos(2 * math.pi * k / sides), r * math.sin(2 * math.pi * k / sides))) for k in range(sides)] for x, r in profile]
    for a, b in zip(loops, loops[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    bm.faces.new(list(reversed(loops[0]))); bm.faces.new(loops[-1])


def slab(bm, outline, half):
    """A flat plate in XZ from a closed outline [(x, z, half_thickness_scale)], bevelled to a thin edge
    where the scale is small (a blade's edge)."""
    n = len(outline)
    mid = [bm.verts.new((x, 0., z)) for x, z, _ in outline]
    front = [bm.verts.new((x, -half * t, z)) for x, z, t in outline]
    back = [bm.verts.new((x, half * t, z)) for x, z, t in outline]
    # Each half a fan from the plate's centre line: simple and closed.
    cx = sum(x for x, _, _ in outline) / n; cz = sum(z for _, z, _ in outline) / n
    cf = bm.verts.new((cx, -half, cz)); cb = bm.verts.new((cx, half, cz))
    for k in range(n):
        a, b = k, (k + 1) % n
        bm.faces.new((front[a], front[b], cf)); bm.faces.new((back[b], back[a], cb))
        bm.faces.new((mid[a], mid[b], front[b], front[a])); bm.faces.new((mid[b], mid[a], back[a], back[b]))


def join(name, objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active; obj.name = name
    return obj


# ------------------------------------------------------------------ the cutlass
GRIP_HALF = .055   # the grip runs -5.5 .. +5.5 cm along X; the guard's shell at +X
BLADE = .64


def cutlass():
    parts = []
    # The blade: from the shell along +X, curving gently toward its back (+Z), widening a little,
    # the back clipped near the point. Edge on -Z, thin; the back thick.
    def blade(bm):
        pts = []
        N = 26
        for k in range(N + 1):
            u = k / N
            x = GRIP_HALF + .012 + BLADE * u
            bow = .045 * u * u             # the curve, toward the back
            w = .032 + .01 * math.sin(math.pi * min(1., u * 1.15))   # width, a touch wider toward the point
            if u > .86: w *= max(.05, 1 - ((u - .86) / .14) ** 1.3)   # the clipped point
            pts.append((x, bow, w))
        outline = [(x, z + w * .5, .8 if k < len(pts) - 1 else .2) for k, (x, z, w) in enumerate(pts)]                  # the back
        edge = [(x, z - w * .5, .06) for x, z, w in reversed(pts)]                                                      # the edge
        slab(bm, outline + edge, .0035)
    parts.append(mesh_object('blade', blade, 'Steel'))
    # The guard: a dished oval shell at the blade's root, and a knuckle bow from it round the
    # knuckles' side (-Z) back to the pommel.
    def shell(bm):
        ring = []
        for k in range(20):
            a = 2 * math.pi * k / 20
            ring.append(Vector((GRIP_HALF + .006, .045 * math.sin(a), .006 - .05 * max(0., -math.cos(a)) - .022 * max(0., math.cos(a)))))
        c0 = bm.verts.new((GRIP_HALF + .014, 0, -.008))
        outer = [bm.verts.new(p) for p in ring]
        inner = [bm.verts.new(p + Vector((.004, 0, 0))) for p in ring]
        c1 = bm.verts.new((GRIP_HALF + .018, 0, -.008))
        for k in range(20):
            a, b = k, (k + 1) % 20
            bm.faces.new((outer[b], outer[a], c0)); bm.faces.new((inner[a], inner[b], c1))
            bm.faces.new((outer[a], outer[b], inner[b], inner[a]))
    parts.append(mesh_object('shell', shell, 'Iron', smooth=False))
    bow = [Vector((GRIP_HALF + .01, 0, -.048)), Vector((.03, 0, -.062)), Vector((-.01, 0, -.066)), Vector((-.045, 0, -.052)), Vector((-GRIP_HALF - .006, 0, -.02))]
    parts.append(mesh_object('bow', lambda bm: tube(bm, bow, [.0045] * len(bow), 8, 1.5), 'Iron'))
    # The grip: wood bound in cord, swelling in the middle; the pommel cap.
    parts.append(mesh_object('grip', lambda bm: lathe_x(bm, [(-GRIP_HALF, .0115), (-.03, .0135), (0., .0145), (.03, .0135), (GRIP_HALF, .012)], 12), 'Cord'))
    parts.append(mesh_object('pommel', lambda bm: lathe_x(bm, [(-GRIP_HALF - .022, .0), (-GRIP_HALF - .02, .009), (-GRIP_HALF - .012, .014), (-GRIP_HALF, .013), (-GRIP_HALF + .002, .0)], 12), 'Iron'))
    return join('SM_Cutlass', parts)


# ------------------------------------------------------------------ the cleaver
def cleaver():
    parts = []
    H = .06   # the handle's half length
    parts.append(mesh_object('handle', lambda bm: lathe_x(bm, [(-H - .004, .0), (-H, .013), (-.02, .0145), (.02, .0142), (H, .0135), (H + .004, .012)], 10), 'Wood'))
    def rivets(bm):   # two brass rivets through the handle, a head each side
        for x in (-.025, .02):
            for y in (-.0148, .0148):
                bmesh.ops.create_uvsphere(bm, u_segments=8, v_segments=4, radius=.0035)
                for v in list(bm.verts)[-26:]: v.co = Vector((x + v.co.x, y + v.co.y * .5, v.co.z))
    parts.append(mesh_object('rivets', rivets, 'Brass'))
    # The blade: a deep rectangle out past the handle's +X end, its spine level with the handle's
    # top, the edge 9.5 cm below (-Z) with a slight belly; a hole in the far corner.
    x0, x1 = H + .004, H + .215
    def blade(bm):
        outline = []
        for k in range(9):   # the spine, from the bolster out
            outline.append((x0 + (x1 - x0) * k / 8, .016, .9))
        outline.append((x1 + .004, -.004, .5))
        for k in range(13):   # the edge, back toward the handle, a slight belly
            u = 1 - k / 12
            outline.append((x0 + .01 + (x1 - x0 - .01) * u, -.079 - .01 * math.sin(math.pi * u), .05))
        outline.append((x0, -.02, .6))
        slab(bm, outline, .0035)
    parts.append(mesh_object('blade', blade, 'Steel', smooth=False))
    return join('SM_Cleaver', parts)


# ------------------------------------------------------------------ the concertina
HEX_R = .08       # centre to corner of an end's hexagon
END_X0, END_X1 = .035, .1   # the end box from its strap face to the face the bellows join (from the fist)


def hexagon(x, r, z_off=0.):
    return [Vector((x, r * math.cos(math.pi / 6 + k * math.pi / 3), z_off + r * math.sin(math.pi / 6 + k * math.pi / 3))) for k in range(6)]


def prism(bm, a, b):
    va = [bm.verts.new(p) for p in a]; vb = [bm.verts.new(p) for p in b]
    n = len(a)
    for k in range(n): bm.faces.new((va[k], va[(k + 1) % n], vb[(k + 1) % n], vb[k]))
    bm.faces.new(list(reversed(va))); bm.faces.new(vb)


def concertina_end():
    parts = []
    # The box, a little bevelled: the strap face (x0) and the bellows face (x1).
    def box(bm):
        prism(bm, hexagon(END_X0 + .006, HEX_R - .006), hexagon(END_X0 + .012, HEX_R))
        prism(bm, hexagon(END_X0 + .012, HEX_R), hexagon(END_X1, HEX_R))
    b = mesh_object('box', box, 'Wood', smooth=False); parts.append(b)
    # The fretted grille on the strap face (a dark inset hexagon), and the buttons through it in three rows.
    parts.append(mesh_object('grille', lambda bm: prism(bm, hexagon(END_X0 + .002, HEX_R - .016), hexagon(END_X0 + .0065, HEX_R - .016)), 'Grille', smooth=False))
    def buttons(bm):
        for row, (z, n) in enumerate(((.03, 4), (.0, 5), (-.03, 4))):
            for k in range(n):
                y = (k - (n - 1) / 2) * .022
                lathe_x(bm, [(0., .0055), (.006, .0055), (.0075, .004), (.008, .0)], 8, x0=END_X0 - .006)
                for v in list(bm.verts)[-8 * 4:]: v.co += Vector((0, y, z))   # this button into its place
    bt = mesh_object('buttons', buttons, 'Ivory'); parts.append(bt)
    # The hand strap: a leather band over the back of the fist, its ends screwed to the box's rim.
    strap = [Vector((END_X0 - .002, -.07, .0)), Vector((.0, -.062, .0)), Vector((-.03, -.035, .0)), Vector((-.038, .0, .0)),
             Vector((-.03, .035, .0)), Vector((.0, .062, .0)), Vector((END_X0 - .002, .07, .0))]   # over the back of the hand
    parts.append(mesh_object('strap', lambda bm: tube(bm, strap, [.004] * len(strap), 6, 4., Vector((0, 0, 1))), 'Leather'))
    # A brass corner on each of the six corners of the bellows face.
    def corners(bm):
        for p in hexagon(END_X1 - .006, HEX_R + .001):
            bmesh.ops.create_cube(bm, size=1.)
            for v in list(bm.verts)[-8:]: v.co = p + Vector((v.co.x * .012, v.co.y * .012, v.co.z * .012))
    parts.append(mesh_object('corners', corners, 'Brass', smooth=False))
    return join('SM_ConcertinaEnd', parts)


def concertina_bellows():
    def build(bm):
        folds = 7
        rings = []
        for k in range(folds * 2 + 1):
            x = .1 * k / (folds * 2)
            r = HEX_R + (.004 if k % 2 else -.004)
            rings.append(hexagon(x, r))
        vs = [[bm.verts.new(p) for p in ring] for ring in rings]
        for a, b in zip(vs, vs[1:]):
            for k in range(6): bm.faces.new((a[k], a[(k + 1) % 6], b[(k + 1) % 6], b[k]))
        bm.faces.new(list(reversed(vs[0]))); bm.faces.new(vs[-1])
    obj = mesh_object('SM_ConcertinaBellows', build, 'Bellows', smooth=False)
    return obj


def export(obj, name):
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT / f'{name}.fbx'), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
    lo = [round(min(v.co[k] for v in obj.data.vertices) * 100, 1) for k in range(3)]
    hi = [round(max(v.co[k] for v in obj.data.vertices) * 100, 1) for k in range(3)]
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    print('CHUCK_PIRATE_PROP', name, f'tris={tris}', 'bounds_cm=', lo, hi)
    return tris, lo, hi


def build():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / 'manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'props': {}}
    manifest.setdefault('generators', {})
    made = {}
    for name, fn, extra in (('Cutlass', cutlass, {'blade_cm': [round((GRIP_HALF + .012) * 100, 1), round((GRIP_HALF + .012 + BLADE) * 100, 1)]}),
                            ('Cleaver', cleaver, {'blade_cm': [6.4, 27.5]}),
                            ('ConcertinaEnd', concertina_end, {'face_cm': round(END_X1 * 100, 1), 'hex_r_cm': HEX_R * 100}),
                            ('ConcertinaBellows', concertina_bellows, {'length_cm': 10.})):
        obj = fn()
        tris, lo, hi = export(obj, f'SM_{name}')
        slots = [m.name for m in obj.data.materials]
        manifest['props'][name] = dict(fbx=f'SM_{name}.fbx', tris=tris, bounds_min_cm=lo, bounds_max_cm=hi, **extra, slots={
            s: {'type': 'fabric', 'fabric': SLOTS[s][0], 'folder': str(SLOTS[s][1].relative_to(ROOT)).replace('\\', '/'),
                'tile_cm': SLOTS[s][2], 'tint': list(SLOTS[s][3]), 'gain': gain(SLOTS[s][0], SLOTS[s][1])} for s in slots})
        manifest['generators'][name] = 'Tools/build_pirate_props.py'
        made[name] = obj
    path.write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
    if REVIEW:
        import os
        for s, (_, _, _, t) in SLOTS.items():
            if s in bpy.data.materials: bpy.data.materials[s].diffuse_color = (min(1, t[0] * 2.2), min(1, t[1] * 2.2), min(1, t[2] * 2.2), 1.)
        made['Cutlass'].location = (0, 0, .25); made['Cleaver'].location = (0, 0, .05)
        made['ConcertinaEnd'].location = (.1, 0, -.2)
        made['ConcertinaBellows'].location = (.2, 0, -.2)
        e2 = made['ConcertinaEnd'].copy(); bpy.context.collection.objects.link(e2)
        e2.rotation_euler = (0, 0, math.pi); e2.location = (.2 + .1 + .1, 0, -.2)
        s = bpy.context.scene
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam); s.camera = cam
        s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.color_type = 'MATERIAL'; s.display.shading.light = 'STUDIO'
        s.render.resolution_x, s.render.resolution_y = 1000, 800
        for name, loc, target in (('pirate_props.png', (.35, -1.1, .2), (.33, 0, 0.)), ('pirate_props_side.png', (.9, -.6, .5), (.3, 0, 0.))):
            cam.location = loc; cam.data.lens = 40
            cam.rotation_euler = (Vector(target) - cam.location).to_track_quat('-Z', 'Y').to_euler()
            s.render.filepath = os.path.join(REVIEW, name); bpy.ops.render.render(write_still=True)


build()
