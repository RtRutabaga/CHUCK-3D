"""Blender 4.5.14: the plaza blacksmith's anvil, hammer and tongs (user 2026-10-04:
"a blacksmith character by the smithy and forge, make him an anvil that he'll be working at").

blender --background --python Tools/build_smith_props.py [-- --review <dir>]
  -> SourceAssets/NPCs/Props/SM_Anvil.fbx, SM_SmithHammer.fbx, SM_Tongs.fbx,
     their entries in SourceAssets/NPCs/Props/manifest.json (the spear's entry is kept)

Material slots are coloured like the humans' clothing (Tools/import_npc_humans.py,
M_HumanFabric: a Poly Haven texture's grain, greyed, times a tint).

Anvil: a London-pattern anvil, 30 cm tall and 62 cm horn to heel, on an elm
stump bound with two iron hoops; the face 80 cm off the ground (the smith's
knuckle height). Origin at the foot of the stump; the horn points +X. Slots:
Stump (wood), Iron (the dark body and hoops), Face (the polished working face).

Hammer: a cross-peen smith's hammer. Origin in the middle of his fist on the
handle; the handle runs up +Z to the head (centre 29 cm up), the striking face
points +X and the peen -X. Slots Handle (wood), Head (steel).

Tongs: flat-jaw tongs closed on a bar of hot iron. Origin in the middle of his
fist on the reins; the reins run +X to the boss at 31.5 cm, where the jaws
bend up 20 degrees, closed on a bar whose middle is at (46.1, 0, 5.3) cm
(ADockNPC's TongsBar). Slots Iron (the tongs) and Hot (the bar: the runtime
gives it the forge's glowing ember material).
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

FACE_Z = .80          # m: the anvil's working face above the ground
STUMP_Z = .50         # m: top of the stump (the anvil's feet)
PIVOT = .315          # m: the tongs' boss along the reins
JAW_BEND = 20.        # deg: the jaws (and the bar in them) bent up from the reins
SLOTS = {   # slot: (texture, folder, size of one repeat in cm, tint = linear albedo)
    'Stump': ('brown_planks_03', WOOD, 60., (.2, .13, .08)),
    'Iron': ('metal_plate_02', CLOTH, 120., (.075, .074, .078)),
    'Face': ('metal_plate_02', CLOTH, 60., (.32, .32, .33)),
    'Handle': ('brown_planks_03', WOOD, 50., (.36, .25, .15)),
    'Head': ('metal_plate_02', CLOTH, 60., (.13, .13, .135)),
    'Hot': ('metal_plate_02', CLOTH, 60., (.6, .15, .02)),
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


def material(slot):
    return bpy.data.materials.get(slot) or bpy.data.materials.new(slot)


def mesh_object(name, build, slot):
    me = bpy.data.meshes.new(name); bm = bmesh.new(); build(bm)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    obj = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material(slot))
    return obj


def lathe(bm, rings, sides=16, wobble=0., axis=Matrix.Identity(4)):
    """A capped solid of revolution about local Z from (z, radius) rings,
    optionally uneven (a stump's bark), placed by `axis`."""
    loops = []
    for z, r in rings:
        ring = []
        for k in range(sides):
            a = 2 * math.pi * k / sides
            rr = r * (1. + wobble * (math.sin(3 * a + z * 9) * .6 + math.sin(7 * a + 1.3) * .4))
            ring.append(bm.verts.new(axis @ Vector((rr * math.cos(a), rr * math.sin(a), z))))
        loops.append(ring)
    for a, b in zip(loops, loops[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    bm.faces.new(list(reversed(loops[0]))); bm.faces.new(loops[-1])


def loft(bm, sections, chamfer=.15):
    """A capped solid lofted through rectangular sections (z, x0, x1, half width),
    each an octagon with chamfered corners, so the anvil's edges catch light."""
    loops = []
    for z, x0, x1, w in sections:
        c = min(x1 - x0, 2 * w) * chamfer
        pts = [(x0 + c, -w), (x1 - c, -w), (x1, -w + c), (x1, w - c), (x1 - c, w), (x0 + c, w), (x0, w - c), (x0, -w + c)]
        loops.append([bm.verts.new((x, y, z)) for x, y in pts])
    n = 8
    for a, b in zip(loops, loops[1:]):
        for k in range(n):
            bm.faces.new((a[k], a[(k + 1) % n], b[(k + 1) % n], b[k]))
    bm.faces.new(list(reversed(loops[0]))); bm.faces.new(loops[-1])


def tube_along(bm, centres, radii, sides=12, flat=1.):
    """Rings around a path of centres (each ring perpendicular to the path),
    capped; `flat` squashes the ring's height (a horn, a rein)."""
    loops = []
    for i, (p, r) in enumerate(zip(centres, radii)):
        p = Vector(p)
        d = (Vector(centres[min(i + 1, len(centres) - 1)]) - Vector(centres[max(i - 1, 0)])).normalized()
        side = d.cross(Vector((0, 0, 1)))
        if side.length < 1e-4: side = Vector((0, 1, 0))
        side.normalize(); up = side.cross(d).normalized()
        loops.append([bm.verts.new(p + side * (r * math.cos(2 * math.pi * k / sides)) + up * (r * flat * math.sin(2 * math.pi * k / sides)))
                      for k in range(sides)])
    for a, b in zip(loops, loops[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    bm.faces.new(list(reversed(loops[0]))); bm.faces.new(loops[-1])


def box(bm, lo, hi):
    bmesh.ops.create_cube(bm, size=1., matrix=Matrix.Translation((Vector(lo) + Vector(hi)) / 2) @ Matrix.Diagonal((*(Vector(hi) - Vector(lo)), 1.)))


def box_uv(obj, tile_cm):
    """Per face, project on its dominant axis, one texture repeat per tile_cm."""
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


def join(name, parts):
    """One object, a material slot per part's slot, smooth where the part asks."""
    for o, smooth in parts:
        box_uv(o, SLOTS[o.data.materials[0].name][2])
        for p in o.data.polygons: p.use_smooth = smooth
    bpy.ops.object.select_all(action='DESELECT')
    for o, _ in parts: o.select_set(True)
    bpy.context.view_layer.objects.active = parts[0][0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    # Sharp creases stay sharp under smooth shading.
    obj.data.set_sharp_from_angle(angle=math.radians(40))
    return obj


def anvil():
    # The stump: elm, a little uneven, two iron hoops.
    stump = mesh_object('Stump', lambda bm: lathe(bm, [(0., .245), (.04, .235), (.25, .225), (.46, .22), (STUMP_Z, .215)], 20, .03), 'Stump')
    # Proud of the bark's widest bulge (the stump's radius there plus its 3% wobble).
    hoops = mesh_object('Hoops', lambda bm: [lathe(bm, [(z, r), (z + .035, r)], 20) for z, r in ((.07, .248), (.4, .233))], 'Iron')
    # The body: feet, a narrow waist, flaring to the top under the face.
    z0 = STUMP_Z
    body = mesh_object('Body', lambda bm: loft(bm, [
        (z0, -.17, .17, .125), (z0 + .035, -.17, .17, .125), (z0 + .06, -.12, .12, .09),
        (z0 + .14, -.07, .08, .055), (z0 + .2, -.15, .19, .057), (z0 + .225, -.2, .23, .058),
        (FACE_Z - .004, -.2, .23, .058)], .18), 'Iron')
    # The polished face plate, a hair proud of the body.
    face = mesh_object('FacePlate', lambda bm: loft(bm, [(FACE_Z - .012, -.198, .2, .0575), (FACE_Z, -.198, .2, .0575)], .06), 'Face')
    # The horn: a cone off the face end, its top just under the face, curving down to a blunt point.
    horn_c, horn_r = [], []
    for i in range(10):
        t = i / 9
        horn_c.append((.22 + .2 * t, 0., FACE_Z - .034 - .028 * t * t))
        horn_r.append(.036 * (1 - t) ** .85 + .004)
    horn = mesh_object('Horn', lambda bm: tube_along(bm, horn_c, horn_r, 14), 'Iron')
    # Hardy and pritchel holes as dark recesses on the heel (a slab of iron, inset).
    holes = mesh_object('Holes', lambda bm: (box(bm, (-.17, -.012, FACE_Z - .004), (-.146, .012, FACE_Z + .0005)),
                                              box(bm, (-.125, -.006, FACE_Z - .004), (-.113, .006, FACE_Z + .0005))), 'Iron')
    obj = join('SM_Anvil', [(stump, True), (hoops, True), (body, True), (face, True), (horn, True), (holes, False)])
    return obj


def hammer():
    # Handle: oval ash, swelling at the butt, slimming under the head; the fist is at the origin.
    handle = mesh_object('Handle', lambda bm: lathe(bm, [(-.075, .013), (-.065, .0165), (-.02, .0155), (.12, .013), (.27, .0115), (.31, .011)], 12), 'Handle')
    for v in handle.data.vertices: v.co.y *= .78   # oval in section, the long axis along the swing
    # Head at 29 cm: a square-ish striking face (+X), a horizontal cross peen (-X).
    hz = .29
    def build_head(bm):
        axis = Matrix.Translation((0, 0, hz)) @ Matrix.Rotation(math.radians(90), 4, 'Y')   # lathe Z -> world +X
        lathe(bm, [(-.075, .006), (-.07, .016), (-.045, .019), (.0, .021), (.05, .022), (.062, .022), (.066, .019)], 8, 0., axis)
    head = mesh_object('HeadIron', build_head, 'Head')
    # Flatten the peen into a wedge (thin in Z at the -X end).
    for v in head.data.vertices:
        if v.co.x < 0:
            t = min(1., -v.co.x / .075)
            v.co.z = hz + (v.co.z - hz) * (1 - .7 * t)
    return join('SM_SmithHammer', [(handle, True), (head, True)])


def tongs():
    # Reins from the fist (origin) to the boss at 32 cm, then the jaws to 43 cm, closed on a bar.
    def build(bm):
        for s in (-1., 1.):
            path = [(-.13, s * .013, 0.), (-.05, s * .011, 0.), (.1, s * .008, 0.), (.25, s * .006, 0.), (.31, s * .004, 0.)]
            tube_along(bm, path, [.0045, .005, .0055, .006, .007], 8, .7)
            jaw = [(.31, s * .004, 0.), (.36, s * .007, s * .004), (.43, s * .006, s * .009)]
            tube_along(bm, jaw, [.008, .007, .006], 8, .6)
        lathe(bm, [(-.006, .011), (.006, .011)], 10, 0., Matrix.Translation((.315, 0, 0)) @ Matrix.Rotation(math.radians(90), 4, 'Z') @ Matrix.Rotation(math.radians(90), 4, 'X'))
    iron = mesh_object('TongsIron', build, 'Iron')
    bar = mesh_object('Bar', lambda bm: box(bm, (.38, -.009, -.008), (.56, .009, .008)), 'Hot')
    # The jaws are bent up JAW_BEND at the boss, so with the reins sloping down
    # from his fist at the waist the bar lies flat on the face.
    bend = Matrix.Translation((PIVOT, 0, 0)) @ Matrix.Rotation(math.radians(-JAW_BEND), 4, 'Y') @ Matrix.Translation((-PIVOT, 0, 0))
    for o in (iron, bar):
        for v in o.data.vertices:
            if v.co.x > PIVOT: v.co = bend @ v.co
    return join('SM_Tongs', [(iron, True), (bar, False)])


def export(obj, name):
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT / f'{name}.fbx'), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
    lo = [min(v.co[k] for v in obj.data.vertices) for k in range(3)]
    hi = [max(v.co[k] for v in obj.data.vertices) for k in range(3)]
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    print('CHUCK_SMITH_PROP', name, f'tris={tris}', 'bounds_cm=' + ' '.join(f'{a * 100:.1f}..{b * 100:.1f}' for a, b in zip(lo, hi)))
    return tris, [round(a * 100, 1) for a in lo], [round(b * 100, 1) for b in hi]


def build():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    OUT.mkdir(parents=True, exist_ok=True)
    manifest_path = OUT / 'manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else {'props': {}}
    manifest.setdefault('generators', {})
    made = {}
    for name, fn, extra in (('Anvil', anvil, {'face_cm': FACE_Z * 100}), ('SmithHammer', hammer, {'head_cm': 29.}), ('Tongs', tongs, {'bar_centre_cm': [round((PIVOT + .155 * math.cos(math.radians(JAW_BEND))) * 100, 2), 0., round(.155 * math.sin(math.radians(JAW_BEND)) * 100, 2)], 'jaw_bend_deg': JAW_BEND})):
        obj = fn()
        tris, lo, hi = export(obj, f'SM_{name}')
        slots = [m.name for m in obj.data.materials]
        manifest['props'][name] = dict(fbx=f'SM_{name}.fbx', tris=tris, bounds_min_cm=lo, bounds_max_cm=hi, **extra, slots={
            s: {'type': 'fabric', 'fabric': SLOTS[s][0], 'folder': str(SLOTS[s][1].relative_to(ROOT)).replace('\\', '/'),
                'tile_cm': SLOTS[s][2], 'tint': list(SLOTS[s][3]), 'gain': gain(SLOTS[s][0], SLOTS[s][1])} for s in slots})
        manifest['generators'][name] = 'Tools/build_smith_props.py'
        made[name] = obj
    manifest_path.write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
    if REVIEW: review(made)


def review(made):
    import os
    for slot, c in (('Stump', (.35, .24, .15)), ('Iron', (.12, .12, .13)), ('Face', (.55, .55, .57)), ('Handle', (.5, .36, .22)),
                    ('Head', (.3, .3, .32)), ('Hot', (1., .35, .05))):
        if slot in bpy.data.materials: bpy.data.materials[slot].diffuse_color = (*c, 1.)
    made['SmithHammer'].location = (.1, -.35, FACE_Z + .02); made['SmithHammer'].rotation_euler = (0, math.radians(90), math.radians(20))
    made['Tongs'].location = (-.05, .4, FACE_Z + .01); made['Tongs'].rotation_euler = (0, 0, math.radians(-100))
    s = bpy.context.scene
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam); s.camera = cam
    s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.color_type = 'MATERIAL'; s.display.shading.light = 'STUDIO'
    s.render.resolution_x, s.render.resolution_y = 900, 700
    for tag, loc, aim in (('three_quarter', (1.3, -1.1, 1.25), (.05, 0, .62)), ('side', (0, -1.7, .75), (.05, 0, .6)), ('top', (.05, -.35, 1.5), (.05, 0, .8))):
        cam.location = loc; cam.data.lens = 40
        cam.rotation_euler = (Vector(aim) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        s.render.filepath = os.path.join(REVIEW, f'smith_props_{tag}.png'); bpy.ops.render.render(write_still=True)


build()
