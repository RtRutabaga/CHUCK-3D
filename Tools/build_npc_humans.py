"""Blender 4.5.14 + MPFB 2.0.17: build the human NPCs from SourceAssets/NPCs/humans.json
(user 2026-09-30: NPCs at about Blade & Sorcery: Nomad quality, copied with minor changes).

blender --background --python Tools/build_npc_humans.py [-- Name ...] [--review dir]
  -> SourceAssets/NPCs/Humans/<Name>/SK_<Name>.fbx, SourceAssets/NPCs/Humans/manifest.json,
     SourceAssets/NPCs/Humans/Textures/* (the MakeHuman textures used, CC0)

Each NPC is data: MakeHuman macro sliders, skin, eyes, brows, lashes, hair and an
outfit from the clothing kit below. MPFB builds the body (CC0 MakeHuman base mesh
and targets) with MPFB's game engine rig ("game_engine": Unreal mannequin bone
names, three bones per finger; CMU motion capture is mapped onto it by name in
build_npc_mocap.py. It replaced "cmu_mb", whose one finger bone per hand made
stiff paddle hands), and fits the eyes, brows,
lashes and hair. Clothing is made from the body's own surface for each region,
pushed out and thickened into cloth with clean hems, so it fits, carries the
body's skin weights and bends with it; body faces fully under cloth are removed
so nothing pokes through. Fabric UVs are a box projection at a fixed real-world
scale (the fabric's real size per texture repeat), so a weave reads true on
every garment. The fabric texture supplies weave and grain only: the material
greys it and multiplies by `gain` (1 / its mean linear luminance, measured
here) and the NPC's tint, so tint is the cloth's actual colour. Output faces +X, centimetres via FBX unit conversion (metres in Blender).
"""
from pathlib import Path
import json
import math
import shutil
import sys
import bmesh
import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from bl_ext.user_default.mpfb.services import HumanService, TargetService, LocationService

ROOT = Path(__file__).resolve().parents[1]
DATA = Path(LocationService.get_user_data())
SPEC = json.loads((ROOT / 'SourceAssets/NPCs/humans.json').read_text(encoding='utf-8'))['npcs']
OUT = ROOT / 'SourceAssets/NPCs/Humans'
TEX = OUT / 'Textures'
CLOTH = ROOT / 'SourceAssets/Surfaces/Cloth'
ARMOR = ROOT / 'SourceAssets/Surfaces/Armor'   # generated mail (Tools/build_dwarf_textures.py); an outfit item names it with `folder`
# Real-world size of one repeat of each fabric (Poly Haven's dimensions; the mail's own).
TILE_CM = {r['asset']: r['size_cm'] for d in (CLOTH, ARMOR) if (d / 'manifest.json').exists()
           for r in json.loads((d / 'manifest.json').read_text(encoding='utf-8'))['assets']}
FBX = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False, primary_bone_axis='Y',
           secondary_bone_axis='X', use_armature_deform_only=True, mesh_smooth_type='FACE', bake_anim=False)
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None
ONLY = [a for a in argv if a in SPEC]


def reset_scene():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    for block in (bpy.data.meshes, bpy.data.armatures, bpy.data.materials, bpy.data.images):
        for item in list(block):
            if item.users == 0: block.remove(item)
    s = bpy.context.scene
    s.unit_settings.system = 'METRIC'; s.unit_settings.scale_length = 1.


def activate(obj):
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj


def apply_modifier(obj, kind):
    activate(obj)
    for m in list(obj.modifiers):
        if m.type == kind: bpy.ops.object.modifier_apply(modifier=m.name)


# ------------------------------------------------------------ body regions
def dominant_bones(obj, rig):
    names = {b.name for b in rig.data.bones}
    index = {g.index: g.name for g in obj.vertex_groups if g.name in names}
    out = []
    for v in obj.data.vertices:
        best, w = None, 0.
        for g in v.groups:
            if g.group in index and g.weight > w: best, w = index[g.group], g.weight
        out.append(best)
    return out


class Body:
    """Joint heights and a classifier for each vertex (metres, Blender: front -Y)."""
    def __init__(self, obj, rig, eye_z):
        self.obj, self.rig = obj, rig
        self.bone = dominant_bones(obj, rig)
        self.co = [obj.matrix_world @ v.co for v in obj.data.vertices]
        J = lambda b, tail=False: rig.matrix_world @ (rig.data.bones[b].tail_local if tail else rig.data.bones[b].head_local)
        self.elbow, self.wrist = J('lowerarm_l'), J('hand_l')
        self.knee, self.ankle = J('calf_l'), J('foot_l')
        self.hips, self.neck, self.head = J('pelvis'), J('neck_01'), J('head')
        self.shoulder = J('upperarm_l')
        self.waist_z = self.hips.z + .085
        zs = [c.z for c in self.co]
        self.top, self.floor = max(zs), min(zs)
        self.brow_z = eye_z + .028
        self.eye_z = eye_z

    # MPFB's game_engine rig (Unreal mannequin names, three bones per finger).
    ARMS = {'clavicle_l', 'upperarm_l', 'clavicle_r', 'upperarm_r'}
    FOREARMS = {'lowerarm_l', 'lowerarm_r'}
    TORSO = {'spine_01', 'spine_02', 'spine_03', 'pelvis'}
    LEGS = {'thigh_l', 'thigh_r', 'calf_l', 'calf_r'}
    FEET = {'foot_l', 'foot_r', 'ball_l', 'ball_r'}
    NECK, HEAD = 'neck_01', 'head'

    def forearm_u(self, c):
        """0 at the elbow, 1 at the wrist (by height)."""
        return (self.elbow.z - c.z) / max(1e-4, self.elbow.z - self.wrist.z)

    def boot_top(self, opts=None):
        """Top of a boot: `height` above the ankle (default a calf boot; a shoe is ~.06)."""
        return self.ankle.z + (opts or {}).get('height', .17)

    def margin(self, i, piece, opts):
        """edge_margin, torn if the garment is `ragged` (the sewer zombie's rags)."""
        m = self.edge_margin(i, piece, opts)
        if opts.get('ragged') and m > -1.:
            m = min(m - tatter_hem(self.co[i], opts['ragged']), tatter_holes(self.co[i], opts['ragged']))
        return m

    def edge_margin(self, i, piece, opts):
        """Signed distance (metres, + inside) from vertex i to the edge of a
        garment. Cloth is cut exactly where this crosses zero, so hems follow
        a clean line rather than the body's quads; -1 means "never this
        garment" (wrong part of the body)."""
        b, c = self.bone[i], self.co[i]
        OUT = -1.
        clamp = lambda x: min(1., max(0., x))
        # Cut from the skirt helper surface (see skirt_source), not the body:
        if piece == 'skirt':
            return c.z - (self.floor + .07)
        if piece == 'apron':   # a front panel over the skirt (the smith's: longer `bottom`, wider `width`)
            return min(c.z - (self.floor + opts.get('bottom', .3)), opts.get('width', .15) - abs(c.x), -c.y - .01)
        if b is None: return OUT
        if piece == 'shirt':
            if b in self.FOREARMS:
                cut = .3 if opts.get('sleeves') == 'rolled' else .94
                return c.z - (self.elbow.z - cut * (self.elbow.z - self.wrist.z))
            # One neckline across neck and torso bones (`collar`: + higher, - lower).
            collar = self.neck.z + .03 + opts.get('collar', 0.) - c.z
            if b in self.ARMS: return collar
            if b in self.TORSO: return min(collar, c.z - (self.waist_z - .07))
            if b == self.NECK: return collar
            return OUT
        if piece == 'jerkin':
            collar = self.neck.z + .015 - c.z
            if b in self.TORSO: m = collar
            elif b in self.ARMS: m = min(collar, .19 - abs(c.x))
            elif b == self.NECK: m = collar
            else: return OUT
            v_bottom = self.waist_z + .2
            if c.y < 0:   # the open V front
                m = min(m, max(abs(c.x) - (.012 + max(0., c.z - v_bottom) * .26), v_bottom - c.z))
            return min(m, c.z - (self.waist_z - .11))
        if piece == 'belt':
            if b not in self.TORSO and b not in self.LEGS: return OUT
            return .026 - abs(c.z - (self.waist_z - .02 + opts.get('z', 0.)))
        if piece == 'trousers':
            if b in self.LEGS | self.TORSO:   # waistband to the boots, tucked in
                return min(self.waist_z + .01 - c.z, c.z - (self.boot_top() - .07))
            return OUT
        if piece == 'boots':   # the shaft; boot_feet makes the feet
            if b not in self.FEET and b not in self.LEGS: return OUT
            return min(self.boot_top(opts) - c.z, c.z - (self.ankle.z + .045))
        if piece == 'cap':
            if b != self.HEAD: return OUT
            # A knit cap pulled down: just above the brows at the front,
            # over the tops of the ears at the sides, to the nape at the back.
            t = min(1., max(0., (c.y - self.head.y + .05) / .1))
            return c.z - (self.brow_z + .01 - .055 * t)
        if piece == 'helmet':   # a plain steel skull cap, a little higher at the brow
            if b != self.HEAD: return OUT
            t = clamp((c.y - self.head.y + .05) / .1)
            return c.z - (self.brow_z + .014 - .065 * t)
        if piece == 'kerchief':   # a headscarf: hairline at the front, over the ears, to the nape
            if b not in (self.HEAD, self.NECK): return OUT
            t = clamp((c.y - self.head.y + .04) / .1)
            side = clamp((abs(c.x) - .06) / .03)
            return c.z - (self.brow_z + .015 - .025 * side * (1. - t) - .1 * t)
        if piece == 'cuirass':   # a steel breastplate and backplate over the coat
            if b in self.ARMS: m = .15 - abs(c.x)
            elif b in self.TORSO or b == self.NECK: m = 1.
            else: return OUT
            return min(m, self.neck.z - .025 - c.z, c.z - (self.waist_z - .05))
        if piece == 'gambeson':   # a quilted coat: sleeves to the wrist, closed high collar, to the upper thigh
            collar = self.neck.z + .035 - c.z
            if b in self.FOREARMS: return min(collar, c.z - (self.elbow.z - .9 * (self.elbow.z - self.wrist.z)))
            if b in self.ARMS or b == self.NECK: return collar
            if b in self.TORSO | self.LEGS: return min(collar, c.z - (self.waist_z - .16))
            return OUT
        if piece == 'bib':   # a leather apron's bib: the chest front, from below the collarbones down over the apron's top,
            if b not in self.TORSO and b != self.NECK and b not in self.ARMS: return OUT
            top = self.neck.z - .1
            bib = min(top - c.z, .135 - abs(c.x), -c.y + .01, c.z - (self.waist_z - .1))
            # and a strap from each top corner up over the shoulder, down to the shoulder blades behind.
            strap = min(.02 - abs(abs(c.x) - .105), c.z - (top - .04 if c.y < 0 else self.neck.z - .2))
            return max(bib, strap)
        if piece == 'mail':   # a mail shirt: sleeves just past the elbow, a high collar, to the hips (mailskirt hangs below)
            collar = self.neck.z + .02 - c.z
            if b in self.FOREARMS: return c.z - (self.elbow.z - opts.get('sleeve', .22) * (self.elbow.z - self.wrist.z))
            if b in self.ARMS or b == self.NECK: return collar
            if b in self.TORSO | self.LEGS: return min(collar, c.z - (self.waist_z - .1))
            return OUT
        if piece == 'mailskirt':   # cut from the skirt helper: from the waist to `bottom` below it, bridging the legs
            return min(self.waist_z + .02 - c.z, c.z - (self.waist_z - opts.get('bottom', .3)))
        if piece == 'pauldron':   # a plate over each shoulder cap, a `drop` down the upper arm
            if b not in self.ARMS and b not in self.TORSO: return OUT
            return min(c.z - (self.shoulder.z - opts.get('drop', .12)), abs(c.x) - (abs(self.shoulder.x) - opts.get('inner', .05)))
        if piece == 'vambrace':   # a plate round the forearm, below the elbow to above the wrist
            if b not in self.FOREARMS: return OUT
            u = self.forearm_u(c); L = self.elbow.z - self.wrist.z
            return min(u - .18, .9 - u) * L
        if piece == 'trim':   # a band along the edges of another piece (`of`), `width` wide: brass edging on plate
            m = self.margin(i, opts['of_opts']['piece'], opts['of_opts'])
            return -1. if m <= -1. else min(m, opts.get('width', .02) - m)
        if piece == 'bodice':   # sleeveless, square-ish neckline lower at the front, to the waist
            front = clamp((-c.y - .02) / .04) * clamp((.095 - abs(c.x)) / .03)   # square front, straps beside it
            m = self.neck.z - .01 - .1 * front - c.z
            if b in self.ARMS: m = min(m, .17 - abs(c.x))
            elif b not in self.TORSO and b != self.NECK: return OUT
            return min(m, c.z - (self.waist_z - .06))
        raise ValueError(piece)

    def covers(self, i, piece, opts):
        """Whether the garment hides the skin at vertex i (boots: the whole foot).
        Skin stays 2 cm in under each hem, so looking up under a brim or cuff
        shows skin, not the inside of the body."""
        if piece == 'boots' and self.bone[i] in self.FEET | self.LEGS and self.co[i].z < self.boot_top(opts) - .02: return True
        return self.margin(i, piece, opts) > .02


def tatter_hem(c, amount):
    """Torn cloth: every edge pulled back a jagged 0.5..4 cm (smooth enough to cut cleanly)."""
    n = (math.sin(c.x * 61 + c.z * 23) + math.sin(c.y * 47 - c.z * 71 + 1.3) + math.sin((c.x + c.y) * 113 + 2.1)) / 3.
    return amount * (.022 + .017 * n)


def tatter_holes(c, amount):
    """And worn through in a few places: negative (no cloth) inside small blobs, a few cm across."""
    n = math.sin(c.x * 41 + c.z * 23 + .4) * math.sin(c.y * 43 - c.z * 17 + .7) * math.sin(c.z * 29 + c.x * 11)
    n *= .55 + .45 * math.sin(c.x * 9 - c.y * 13 + c.z * 7 + 1.1)   # unevenly: some places worn more than others
    return (.62 * (2. - min(amount, 1.2)) - n) * .06


# Per piece: distance out from the skin, cloth thickness, and whether the skin under it goes.
PIECES = {
    'shirt':    dict(offset=.006, thickness=.0035, hides=True),
    'trousers': dict(offset=.007, thickness=.004, hides=True),
    'boots':    dict(offset=.011, thickness=.005, hides=True),
    'jerkin':   dict(offset=.015, thickness=.005, hides=False),
    'belt':     dict(offset=.027, thickness=.006, hides=False),
    'cap':      dict(offset=.012, thickness=.006, hides=True),
    'helmet':   dict(offset=.018, thickness=.004, hides=True),
    'kerchief': dict(offset=.01, thickness=.003, hides=True),
    'gambeson': dict(offset=.016, thickness=.012, hides=True),
    'cuirass':  dict(offset=.034, thickness=.006, hides=False),
    'bodice':   dict(offset=.013, thickness=.004, hides=False),
    'skirt':    dict(offset=.005, thickness=.004, hides=False),   # its legs are hidden in build()
    'apron':    dict(offset=.011, thickness=.003, hides=False),
    'bib':      dict(offset=.016, thickness=.004, hides=False),
    'mail':     dict(offset=.02, thickness=.004, hides=True),
    'mailskirt': dict(offset=.024, thickness=.004, hides=False),
    'pauldron': dict(offset=.052, thickness=.007, hides=False),
    'vambrace': dict(offset=.03, thickness=.005, hides=False),
    'trim':     dict(offset=0., thickness=.006, hides=False),   # offset: just over the piece it edges (make_piece)
}


def fabric_gain(name, folder=CLOTH, cache={}):
    """1 / mean linear luminance of a fabric's colour map."""
    if name not in cache:
        import numpy as np
        img = bpy.data.images.load(str(folder / f'{name}_diff_2k.jpg')); img.scale(256, 256)
        a = np.array(img.pixels[:]).reshape(-1, 4)[:, :3]
        lin = np.where(a <= .04045, a / 12.92, ((a + .055) / 1.055) ** 2.4)
        cache[name] = round(1. / float((lin @ np.array([.2126, .7152, .0722])).mean()), 3)
        bpy.data.images.remove(img)
    return cache[name]


def box_uv(obj, tile_cm):
    """Fabric UVs: per face, project on its dominant axis, one repeat per tile_cm."""
    me = obj.data
    while me.uv_layers: me.uv_layers.remove(me.uv_layers[0])
    uv = me.uv_layers.new(name='UVMap')
    s = 100. / tile_cm
    for poly in me.polygons:
        n = poly.normal; ax = max(range(3), key=lambda k: abs(n[k]))
        for li in poly.loop_indices:
            co = obj.matrix_world @ me.vertices[me.loops[li].vertex_index].co
            u, v = [(co.y, co.z), (co.x, co.z), (co.x, co.y)][ax]
            uv.data[li].uv = (u * s, v * s)


def relax_hems(bm, iterations=8):
    """Region edges follow the body's quads and zig-zag; walk each hem toward
    the line through its neighbours so it reads as a cut, sewn edge."""
    ring = {}
    for e in bm.edges:
        if e.is_boundary:
            a, b = e.verts; ring.setdefault(a, []).append(b); ring.setdefault(b, []).append(a)
    ring = {v: n for v, n in ring.items() if len(n) == 2}
    for _ in range(iterations):
        for k in (.5, -.53):   # Taubin: smooth without shrinking the loop
            new = {v: v.co + k * ((n[0].co + n[1].co) * .5 - v.co) for v, n in ring.items()}
            for v, co in new.items(): v.co = co


def cut_along(bm, margin):
    """Keep the part of the surface where margin > 0, cut exactly on margin = 0.
    A vertex that lies close to the line is slid onto it (no sliver faces);
    other crossing edges are split at the crossing (skin weights interpolated)
    and each face crossed is split between its two points on the line."""
    m = bm.verts.layers.float.new('margin')
    bm.verts.ensure_lookup_table()
    for v in bm.verts: v[m] = margin[v.index]
    crossing = lambda e: (e.verts[0][m] > 0 and e.verts[1][m] < 0) or (e.verts[0][m] < 0 and e.verts[1][m] > 0)
    snap = {}
    for e in [e for e in bm.edges if crossing(e)]:
        a, b = e.verts; t = a[m] / (a[m] - b[m])
        if t < .25: snap.setdefault(a, []).append(a.co.lerp(b.co, t))
        elif t > .75: snap.setdefault(b, []).append(a.co.lerp(b.co, t))
    for v, points in snap.items():
        v.co = sum(points, Vector()) / len(points); v[m] = 0.
    for e in [e for e in bm.edges if crossing(e)]:
        a, b = e.verts; t = a[m] / (a[m] - b[m])
        _, v = bmesh.utils.edge_split(e, a, t)
        v[m] = 0.
    for f in list(bm.faces):
        if not (any(v[m] > 0 for v in f.verts) and any(v[m] < 0 for v in f.verts)): continue
        on = [v for v in f.verts if v[m] == 0.]
        if len(on) == 2 and not bm.edges.get(on): bmesh.utils.face_split(f, on[0], on[1])
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if any(v[m] < 0 for v in f.verts)], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.verts.layers.float.remove(m)


def boot_feet(body, boots):
    """No toes in a boot: each boot foot is new geometry, the convex hull of
    that foot (flat sole, rounded toe box, heel) remeshed, smoothed and pushed
    out like the rest of the cloth, skinned from the body and joined to the
    shaft cut from the leg."""
    cut = body.ankle.z + .045
    for side in (1., -1.):
        hull = bmesh.new()
        for i, c in enumerate(body.co):
            if c.x * side > 0 and c.z < cut + .02 and body.bone[i] in Body.FEET | Body.LEGS: hull.verts.new(c)
        bmesh.ops.convex_hull(hull, input=hull.verts)
        bmesh.ops.delete(hull, geom=[v for v in hull.verts if not v.link_faces], context='VERTS')
        me = bpy.data.meshes.new('boot_foot'); hull.to_mesh(me); hull.free()
        foot = bpy.data.objects.new('boot_foot', me); bpy.context.collection.objects.link(foot)
        activate(foot)
        rm = foot.modifiers.new('Even', 'REMESH'); rm.mode = 'VOXEL'; rm.voxel_size = .006
        sm = foot.modifiers.new('Round', 'SMOOTH'); sm.factor = .8; sm.iterations = 12
        for m in list(foot.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)
        bm = bmesh.new(); bm.from_mesh(me); bm.normal_update()
        for v in bm.verts: v.co += v.normal * (PIECES['boots']['offset'] + PIECES['boots']['thickness'])
        bm.to_mesh(me); bm.free()
        # The body's skin weights onto the new foot.
        dt = foot.modifiers.new('Weights', 'DATA_TRANSFER'); dt.object = body.obj
        dt.use_vert_data = True; dt.data_types_verts = {'VGROUP_WEIGHTS'}; dt.vert_mapping = 'POLYINTERP_NEAREST'
        dt.layers_vgroup_select_src = 'ALL'; dt.layers_vgroup_select_dst = 'NAME'
        bpy.ops.object.datalayout_transfer(modifier=dt.name); bpy.ops.object.modifier_apply(modifier=dt.name)
        foot.modifiers.new('Armature', 'ARMATURE').object = body.rig
        foot.data.materials.append(boots.data.materials[0])
        bpy.ops.object.select_all(action='DESELECT'); foot.select_set(True); boots.select_set(True)
        bpy.context.view_layer.objects.active = boots; bpy.ops.object.join()


PLATE = {'helmet', 'cuirass', 'pauldron', 'vambrace'}   # hard edges: their hems are relaxed further


def hem_iterations(piece):
    return 24 if piece in PLATE else 30 if piece == 'trim' else 8


def rolled_trim(body, opts, material):
    """A `rolled` trim: a brass tube of `radius` swept along every edge of the
    piece it trims (`of`), wrapped round the plate's edge, so it reads as a clean
    rolled rim however coarse the body under it."""
    base = opts['of_opts']; of = base['piece']
    obj = body.obj.copy(); obj.data = body.obj.data.copy(); obj.name = 'trim_path'
    bpy.context.collection.objects.link(obj)
    bm = bmesh.new(); bm.from_mesh(obj.data)
    cut_along(bm, [body.margin(i, of, base) for i in range(len(body.co))])
    relax_hems(bm, hem_iterations(of))
    bm.normal_update()
    out = base.get('offset', PIECES[of]['offset']) + PIECES[of]['thickness'] * .5
    for v in bm.verts: v.co += v.normal * out
    loops, seen = [], set()
    nxt = {}
    for e in bm.edges:
        if e.is_boundary:
            a, b = e.verts; nxt.setdefault(a, []).append(b); nxt.setdefault(b, []).append(a)
    for start in list(nxt):
        if start in seen or len(nxt[start]) != 2: continue
        loop, prev, cur = [start], None, start
        seen.add(start)
        while True:
            step = next((n for n in nxt[cur] if n is not prev and len(nxt[n]) == 2), None)
            if step is None or step is start or step in seen: break
            loop.append(step); seen.add(step); prev, cur = cur, step
        if len(loop) > 8: loops.append([v.co.copy() for v in loop])
    bm.free(); bpy.data.objects.remove(obj)
    r, sides = opts.get('radius', .005), 8
    tb = bmesh.new()
    for pts in loops:
        n = len(pts)
        rings = []
        for k in range(n):
            d = (pts[(k + 1) % n] - pts[k - 1]).normalized()
            side = d.cross(Vector((0, 0, 1)))
            if side.length < 1e-4: side = Vector((1, 0, 0))
            side.normalize(); up = side.cross(d).normalized()
            rings.append([tb.verts.new(pts[k] + (side * math.cos(2 * math.pi * j / sides) + up * math.sin(2 * math.pi * j / sides)) * r) for j in range(sides)])
        for k in range(n):
            a, b = rings[k], rings[(k + 1) % n]
            for j in range(sides): tb.faces.new((a[j], a[(j + 1) % sides], b[(j + 1) % sides], b[j]))
    trim = new_object('trim', tb)
    trim.data.materials.append(material)
    for poly in trim.data.polygons: poly.use_smooth = True
    skin_from_body(trim, body)
    box_uv(trim, TILE_CM[opts['fabric']])
    return trim, [False] * len(body.co)


def rigidify(obj, info, weights):
    """Plate that keeps its shape: every vertex on the same few bones, by side
    (`rigid` = {bone stem: weight}, e.g. upperarm 0.75, clavicle 0.25), so a
    pauldron turns with the shoulder instead of bending like a sleeve."""
    left = 1. if info.shoulder.x > 0 else -1.
    for g in list(obj.vertex_groups): obj.vertex_groups.remove(g)
    for v in obj.data.vertices:
        sx = '_l' if v.co.x * left > 0 else '_r'
        for stem, w in weights.items():
            g = obj.vertex_groups.get(stem + sx) or obj.vertex_groups.new(name=stem + sx)
            g.add([v.index], w, 'REPLACE')


def make_piece(body, piece, opts, material):
    if piece == 'trim' and opts.get('rolled'): return rolled_trim(body, opts, material)
    p = dict(PIECES[piece])
    if 'offset' in opts: p['offset'] = opts['offset']   # layered armour: each layer over the last
    if piece == 'trim':   # just proud of the outer face of the piece it edges
        base = opts['of_opts']
        p['offset'] = base.get('offset', PIECES[base['piece']]['offset']) + PIECES[base['piece']]['thickness'] + .005
    keep = [body.covers(i, piece, opts) for i in range(len(body.co))]
    obj = body.obj.copy(); obj.data = body.obj.data.copy(); obj.name = f'{piece}'
    bpy.context.collection.objects.link(obj)
    bm = bmesh.new(); bm.from_mesh(obj.data)
    cut_along(bm, [body.margin(i, piece, opts) for i in range(len(body.co))])
    relax_hems(bm, hem_iterations(piece))   # plates and narrow bands show every zig-zag
    bm.normal_update()
    for v in bm.verts: v.co += v.normal * p['offset']
    bm.to_mesh(obj.data); bm.free()
    activate(obj)
    sm = obj.modifiers.new('Ease', 'CORRECTIVE_SMOOTH'); sm.factor = .5; sm.iterations = 4; sm.use_only_smooth = True; sm.use_pin_boundary = True
    bpy.ops.object.modifier_move_to_index(modifier=sm.name, index=0); bpy.ops.object.modifier_apply(modifier=sm.name)
    sol = obj.modifiers.new('Cloth', 'SOLIDIFY'); sol.thickness = p['thickness']; sol.offset = 1.; sol.use_rim = True
    bpy.ops.object.modifier_move_to_index(modifier=sol.name, index=0); bpy.ops.object.modifier_apply(modifier=sol.name)
    obj.data.materials.clear(); obj.data.materials.append(material)
    if piece == 'boots': boot_feet(body, obj)
    for poly in obj.data.polygons: poly.material_index = 0; poly.use_smooth = True
    box_uv(obj, TILE_CM[opts['fabric']])
    return obj, keep


def soften_skirt(obj, info):
    """A skirt mustn't split between the legs: the helper's weights follow each
    leg. Re-weight it to the hips, easing onto the thighs toward the hem (up
    to half), blended across the middle, so it sways with the stride."""
    left_x = info.rig.data.bones['thigh_l'].head_local.x   # which side is her left
    sign = 1. if left_x > 0 else -1.
    names = ('pelvis', 'thigh_l', 'thigh_r')
    for n in names:
        if n not in obj.vertex_groups: obj.vertex_groups.new(name=n)
    groups = {n: obj.vertex_groups[n] for n in names}
    keep = {g.index for g in groups.values()}
    rigged = {b.name for b in info.rig.data.bones}
    for v in obj.data.vertices:
        c = obj.matrix_world @ v.co
        leg = .5 * min(1., max(0., (info.waist_z - .05 - c.z) / max(.1, info.waist_z - .05 - info.floor)))
        left = min(1., max(0., .5 + sign * c.x / .2))
        for g in list(v.groups):
            if g.group not in keep and obj.vertex_groups[g.group].name in rigged:
                obj.vertex_groups[g.group].remove([v.index])
        groups['pelvis'].add([v.index], 1. - leg, 'REPLACE')
        groups['thigh_l'].add([v.index], leg * left, 'REPLACE')
        groups['thigh_r'].add([v.index], leg * (1. - left), 'REPLACE')


def skirt_source(body):
    """MakeHuman's skirt helper (a weighted shell from waist to ankle that
    bridges the legs), as its own mesh to cut skirts and aprons from."""
    src = body.copy(); src.data = body.data.copy(); src.name = 'skirt_source'
    bpy.context.collection.objects.link(src)
    for m in list(src.modifiers):
        if m.type != 'ARMATURE': src.modifiers.remove(m)
    g = src.vertex_groups['helper-skirt'].index
    inside = [any(x.group == g and x.weight > .5 for x in v.groups) for v in src.data.vertices]
    bm = bmesh.new(); bm.from_mesh(src.data); bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if not all(inside[v.index] for v in f.verts)], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(src.data); bm.free()
    return src


SKIRTED = ('skirt', 'apron', 'mailskirt')   # cut from the skirt helper, not the body


def skin_to(obj, rig, weights):
    """Vertex groups from a per-vertex {bone: weight} and an armature modifier on rig."""
    for v, w in zip(obj.data.vertices, weights):
        for bone, x in w.items():
            g = obj.vertex_groups.get(bone) or obj.vertex_groups.new(name=bone)
            g.add([v.index], x, 'REPLACE')
    obj.modifiers.new('Armature', 'ARMATURE').object = rig


def skin_from_body(obj, info, below=None):
    """The body's own skin weights onto obj (nearest surface), so it moves exactly
    with the face under it; `below` = (z, fade, toward) eases vertices under z onto
    one bone (a beard's hang resting on the chest)."""
    activate(obj)
    dt = obj.modifiers.new('Weights', 'DATA_TRANSFER'); dt.object = info.obj
    dt.use_vert_data = True; dt.data_types_verts = {'VGROUP_WEIGHTS'}; dt.vert_mapping = 'POLYINTERP_NEAREST'
    dt.layers_vgroup_select_src = 'ALL'; dt.layers_vgroup_select_dst = 'NAME'
    bpy.ops.object.datalayout_transfer(modifier=dt.name); bpy.ops.object.modifier_apply(modifier=dt.name)
    if below:
        z0, fade, bone = below
        g = obj.vertex_groups.get(bone) or obj.vertex_groups.new(name=bone)
        for v in obj.data.vertices:
            t = min(1., max(0., (z0 - v.co.z) / fade))
            if t <= 0: continue
            for e in v.groups: e.weight *= 1 - t
            g.add([v.index], t + sum(e.weight for e in v.groups if e.group == g.index), 'REPLACE')
    obj.modifiers.new('Armature', 'ARMATURE').object = info.rig


def face_marks(info):
    """Nose tip, chin and the head's front-to-back middle, from the head's own vertices."""
    head = [c for i, c in enumerate(info.co) if info.bone[i] == Body.HEAD]
    mid = [c for c in head if abs(c.x) < .008]
    nose = min((c for c in mid if info.eye_z - .065 < c.z < info.eye_z - .015), key=lambda c: c.y)
    chin = min((c for c in mid if c.y < nose.y + .06), key=lambda c: c.z)
    back = max(c.y for c in head)
    return nose, chin, (nose.y + back) / 2


def nasal_guard(info, material):
    """A helmet's nasal: a tapering iron bar from the brow rim down over the nose."""
    nose, _, _ = face_marks(info)
    top_z, bot_z = info.brow_z + .03, nose.z + .006
    mid = [c for i, c in enumerate(info.co) if info.bone[i] == Body.HEAD and abs(c.x) < .008]
    front = lambda z: min(c.y for c in mid if abs(c.z - z) < .008)   # the face's front at that height
    bm = bmesh.new()
    rings = []
    for z, half, y in ((top_z, .01, front(top_z) - .022), (bot_z + .015, .008, nose.y - .006), (bot_z, .006, nose.y - .006)):
        rings.append([bm.verts.new((x, y + dy, z)) for x, dy in ((-half, 0), (half, 0), (half, .005), (-half, .005))])
    for a, b in zip(rings, rings[1:]):
        for k in range(4): bm.faces.new((a[k], a[(k + 1) % 4], b[(k + 1) % 4], b[k]))
    bm.faces.new(list(reversed(rings[0]))); bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new('nasal'); bm.to_mesh(me); bm.free()
    obj = bpy.data.objects.new('nasal', me); bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    skin_from_body(obj, info)
    box_uv(obj, 60.)
    return obj


def tube(bm, centres, radii, sides=10):
    """Capped rings along a path (each perpendicular to it)."""
    loops = []
    for i, (p, r) in enumerate(zip(centres, radii)):
        d = (centres[min(i + 1, len(centres) - 1)] - centres[max(i - 1, 0)]).normalized()
        side = d.cross(Vector((0, 0, 1)))
        if side.length < 1e-4: side = Vector((1, 0, 0))
        side.normalize(); up = side.cross(d).normalized()
        loops.append([bm.verts.new(p + (side * math.cos(2 * math.pi * k / sides) + up * math.sin(2 * math.pi * k / sides)) * r) for k in range(sides)])
    for a, b in zip(loops, loops[1:]):
        for k in range(sides): bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    bm.faces.new(list(reversed(loops[0]))); bm.faces.new(loops[-1])


def voxel_merge(obj, size, smooth=0):
    activate(obj)
    rm = obj.modifiers.new('Merge', 'REMESH'); rm.mode = 'VOXEL'; rm.voxel_size = size
    if smooth:
        sm = obj.modifiers.new('Round', 'SMOOTH'); sm.factor = .7; sm.iterations = smooth
    for m in list(obj.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)


def new_object(name, bm):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    obj = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(obj)
    return obj


def sideburns(info, cheek_z, spec):
    """Points (and blob sizes) for sideburns: the side of the face in front of
    each ear, from the cheek line up to `sideburn_tuck` above the helmet's rim
    (so they run up under it), thinning toward the top."""
    out = []
    head = [i for i in range(len(info.co)) if info.bone[i] == Body.HEAD]
    for sx in (-1, 1):
        side = [i for i in head if info.co[i].x * sx > 0 and abs(info.co[i].z - info.eye_z) < .03]
        if not side: continue
        ear_x = max(abs(info.co[i].x) for i in side)
        ear_front = min(info.co[i].y for i in side if abs(info.co[i].x) > ear_x - .012)
        width = spec.get('sideburn_width', .035)
        helmet = next((it for it in spec.get('_outfit', []) if it['piece'] == 'helmet'), None)
        for i in head:
            c = info.co[i]
            if c.x * sx <= .045 or not (ear_front - width < c.y < ear_front + .004) or c.z < cheek_z - .015: continue
            if helmet:
                rim = -info.edge_margin(i, 'helmet', helmet)    # metres below the rim (negative: under the helmet)
                if rim < -spec.get('sideburn_tuck', .012): continue
            elif c.z > info.eye_z + .02: continue
            up = min(1., max(0., (c.z - cheek_z) / max(.01, info.eye_z + .03 - cheek_z)))
            out.append((c, .01 - .004 * up))
    return out


def make_beard(info, spec, slots):
    """A full beard (MakeHuman has none): one mass over the jaw, cheeks and upper
    lip, hanging `length` down the chest (`clear` of the armour on it), a
    moustache and two braids ending in brass rings, voxel-merged into one smooth
    body with clumped strands, then loose strand cards over it for a frayed edge.
    Skinned to the head, easing onto the chest toward the tip so it rests there
    as he looks about. Slots Beard and BeardStrands (card material, tinted) and
    BeardRing."""
    import random
    rnd = random.Random(7)
    nose, chin, head_y = face_marks(info)
    L, W = spec.get('length', .26), spec.get('width', .085)
    clear = spec.get('clear', .05)
    torso = [c for i, c in enumerate(info.co) if info.bone[i] in Body.TORSO | {Body.NECK} and abs(c.x) < .06]
    def chest_front(z):
        near = [c.y for c in torso if abs(c.z - z) < .02]
        return min(near) if near else chin.y + .06
    bm = bmesh.new()
    # 1. The jaw: face below the cheekbones, in front of the ears, clear of the nose: a blob per vertex, united by the remesh.
    cheek_z = info.eye_z - spec.get('cheek', .045)
    jaw = [c for i, c in enumerate(info.co) if info.bone[i] in (Body.HEAD, Body.NECK) and c.z < cheek_z and c.y < head_y - .005
           and c.z > chin.z - .05 and not (abs(c.x) < .03 and c.z > nose.z - .016)]
    for c in jaw:
        n = Vector((c.x, c.y - head_y, 0)).normalized()
        depth = .007 + .016 * min(1., max(0., (cheek_z - c.z) / .06))
        bmesh.ops.create_icosphere(bm, subdivisions=1, radius=depth, matrix=Matrix.Translation(c + n * depth * .55))
    # Sideburns: a band in front of each ear from the jaw up under the helmet's rim (or to the temple bare-headed).
    burns = sideburns(info, cheek_z, spec)
    for c, depth in burns:
        n = Vector((c.x, (c.y - head_y) * .5, 0)).normalized()
        bmesh.ops.create_icosphere(bm, subdivisions=1, radius=depth, matrix=Matrix.Translation(c + n * depth * .55))
    # 2. The hang: a rounded spade down the chest, narrowing to the tip.
    steps = 9
    for k in range(steps + 1):
        t = k / steps
        z = chin.z + .02 - (L + .02) * t
        back = min(chest_front(z) - clear, chin.y + .03) if t > .15 else chin.y + .045
        depth = .05 * (1 - .55 * t)
        half = W * (1 - .6 * t ** 1.6)
        for sx in (-1, 0, 1):
            bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=6, radius=1.,
                matrix=Matrix.Translation((sx * half * .45, back - depth, z)) @ Matrix.Diagonal((half * (.6 if sx else .75), depth, .045, 1.)))
    # 3. The moustache: from under the nose, out and down past the corners of the mouth.
    for sx in (-1, 1):
        pts = [Vector((sx * x, nose.y + .012 + y, nose.z - .018 - zz)) for x, y, zz in ((.004, 0, 0), (.022, .006, .008), (.036, .016, .028), (.042, .022, .05))]
        tube(bm, pts, [.009, .011, .009, .006], 8)
    beard = new_object('beard', bm)
    voxel_merge(beard, .0055, 14)
    # Clumped strands: grooves running down, deeper toward the tip.
    for v in beard.data.vertices:
        a = math.atan2(v.co.x, -(v.co.y - head_y))
        fall = min(1., max(0., (chin.z - v.co.z) / L))
        groove = math.sin(a * 46 + v.co.z * 9) * .5 + math.sin(a * 97 - v.co.z * 23 + 1.7) * .3
        v.co += v.normal * groove * (.0025 + .005 * fall)
    beard.data.update()
    # Braids from the tip, each ending in a brass ring (unless `braids` is false: the tavern keeper's plain beard).
    bm, rings_bm = bmesh.new(), bmesh.new()
    tip_z = chin.z - L
    tip_back = chest_front(tip_z + .03) - clear
    length, lobes = spec.get('braid', .1), 7
    for sx in ((-1, 1) if spec.get('braids', True) else ()):
        x0, y0 = sx * W * .2, tip_back - .022
        for k in range(lobes):
            t = k / lobes
            r = .013 * (1 - .35 * t)
            bmesh.ops.create_uvsphere(bm, u_segments=8, v_segments=5, radius=1.,
                matrix=Matrix.Translation((x0 + sx * (.004 if k % 2 else -.004), y0 - .004 * t, tip_z + .025 - length * t))
                @ Matrix.Diagonal((r, r * .9, length / lobes * .75, 1.)))
        rz = tip_z + .025 - length * .92
        tube(rings_bm, [Vector((x0, y0 - .004, rz + .008)), Vector((x0, y0 - .004, rz - .008))], [.011, .011], 12)
    rings = None
    if spec.get('braids', True):
        braids = new_object('braids', bm)
        voxel_merge(braids, .003)
        bpy.ops.object.select_all(action='DESELECT'); braids.select_set(True); beard.select_set(True)
        bpy.context.view_layer.objects.active = beard; bpy.ops.object.join()
        rings = new_object('beard_rings', rings_bm)
    # 4. Loose strands: cards hanging from points on the lower beard, a little proud, past its edge.
    bm, uv_cards = bmesh.new(), []
    pts = [(v.co.copy(), v.normal.copy()) for v in beard.data.vertices if v.co.z < chin.z + .015 and v.normal.y < .2]
    rnd.shuffle(pts)
    pts = pts[:spec.get('cards', 110)]
    # And short tufts down the sideburns, so their edges are hair, not a rim.
    burn_top = max((c.z for c, _ in burns), default=chin.z)
    side_pts = [(v.co.copy(), v.normal.copy()) for v in beard.data.vertices
                if chin.z + .03 < v.co.z < burn_top and abs(v.co.x) > .045 and abs(v.normal.x) > .5]
    rnd.shuffle(side_pts)
    pts += [(co, n, True) for co, n in side_pts[:spec.get('sideburn_cards', 50)]]
    # Strands lying over the whole beard, following its surface, so it reads as hair rather than a sculpted mass.
    bvh = BVHTree.FromObject(beard, bpy.context.evaluated_depsgraph_get())
    lying = [(v.co.copy(), v.normal.copy()) for v in beard.data.vertices if v.normal.y < .3 and v.co.z < info.eye_z - .05]
    rnd.shuffle(lying)
    for co, n in lying[:spec.get('lying_cards', 220)]:
        flat = Vector((n.x, n.y, 0))
        side = Vector((n.y, -n.x, 0)).normalized() if flat.length > .1 else Vector((1, 0, 0))
        width, length, u0 = rnd.uniform(.01, .018), rnd.uniform(.03, .07), rnd.uniform(0, .85)
        col, p = [], co.copy()
        for k in range(5):
            t = k / 4
            if k:
                p = p - Vector((0, 0, length / 4))
                hit, normal, _, _ = bvh.find_nearest(p)
                if hit is not None: p, n = hit.copy(), normal.copy()
            q = p + n * (.0025 + .002 * t)
            col.append((bm.verts.new(q - side * width / 2), bm.verts.new(q + side * width / 2), 1 - t))
        for a, b in zip(col, col[1:]):
            f = bm.faces.new((a[0], a[1], b[1], b[0]))
            uv_cards.append((f, ((u0, a[2]), (u0 + .15, a[2]), (u0 + .15, b[2]), (u0, b[2]))))
    for item in pts:
        co, n = item[0], item[1]
        short = len(item) > 2
        flat = Vector((n.x, n.y, 0))
        side = Vector((n.y, -n.x, 0)).normalized() if flat.length > .1 else Vector((1, 0, 0))
        width, length = (rnd.uniform(.008, .014), rnd.uniform(.022, .04)) if short else (rnd.uniform(.012, .022), rnd.uniform(.05, .12))
        u0 = rnd.uniform(0, .85)
        col = []
        for k in range(5):
            t = k / 4
            p = co + n * (.002 + .004 * t + .01 * t * t) - Vector((0, 0, length * t))
            col.append((bm.verts.new(p - side * width / 2), bm.verts.new(p + side * width / 2), 1 - t))
        for a, b in zip(col, col[1:]):
            f = bm.faces.new((a[0], a[1], b[1], b[0]))
            uv_cards.append((f, ((u0, a[2]), (u0 + .15, a[2]), (u0 + .15, b[2]), (u0, b[2]))))
    uvl = bm.loops.layers.uv.new('UVMap')
    for f, uvs in uv_cards:
        for loop, uv in zip(f.loops, uvs): loop[uvl].uv = uv
    cards = new_object('beard_strands', bm)
    # UVs on the mass: around the head's vertical axis, strands running down (8 cm across, 18 cm down per repeat).
    me = beard.data
    while me.uv_layers: me.uv_layers.remove(me.uv_layers[0])
    uv = me.uv_layers.new(name='UVMap')
    for poly in me.polygons:
        for li in poly.loop_indices:
            c = me.vertices[me.loops[li].vertex_index].co
            uv.data[li].uv = (math.atan2(c.x, -(c.y - head_y)) * .09 / .08, c.z / .18)
    if rings: box_uv(rings, 60.)
    # Skin: the face's own weights under it, easing onto the chest down the hang.
    for obj, slot in ((beard, 'Beard'), (cards, 'BeardStrands')) + (((rings, 'BeardRing'),) if rings else ()):
        obj.data.materials.append(slot_material(slot))
        for p in obj.data.polygons: p.use_smooth = obj is not cards
        skin_from_body(obj, info, (chin.z, spec.get('chest_fade', .07), 'spine_03'))   # the hang rests on the chest, not swinging with the head
    slots['Beard'] = {'type': 'card', 'texture': 'Textures/beard_mass.png', 'tint': spec['tint']}
    slots['BeardStrands'] = {'type': 'card', 'texture': 'Textures/beard_strands.png', 'tint': spec.get('strand_tint', spec['tint'])}
    if rings:
        ring = spec.get('ring', {'fabric': 'metal_plate_02', 'tint': [.42, .27, .09]})
        slots['BeardRing'] = {'type': 'fabric', 'fabric': ring['fabric'], 'tint': ring['tint'], 'tile_cm': TILE_CM[ring['fabric']], 'gain': fabric_gain(ring['fabric'])}
    print('CHUCK_BEARD', f'length_cm={L * 100:.0f}', f'mass_verts={len(beard.data.vertices)}', f'cards={len(uv_cards) // 4}')
    return [beard, cards] + ([rings] if rings else [])


def slot_material(name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    return m


def build(name, spec):
    reset_scene()
    body = HumanService.create_human(scale=.1, macro_detail_dict=spec['macro'])
    # Proportion targets on top of the macro sliders (the dwarf: short legs, broad torso, big hands and head),
    # before the rig, so the rig is fitted to the shape.
    for target, weight in spec.get('targets', {}).items():
        path = TargetService.target_full_path(target)
        if not path: raise ValueError(f'No MakeHuman target {target}')
        TargetService.load_target(body, path, weight=weight, name=target)
    HumanService.add_builtin_rig(body, 'game_engine')
    rig = body.parent
    parts = {}
    def asset(kind, path):
        obj = HumanService.add_mhclo_asset(str(path), body, asset_type=kind, subdiv_levels=0)
        parts[kind] = obj
    asset('Eyes', DATA / 'eyes/high-poly/high-poly.mhclo')
    asset('Eyebrows', DATA / f"eyebrows/{spec['eyebrows']}/{spec['eyebrows']}.mhclo")
    asset('Eyelashes', DATA / f"eyelashes/{spec['eyelashes']}/{spec['eyelashes']}.mhclo")
    if spec.get('hair'): asset('Hair', DATA / f"hair/{spec['hair']}/{spec['hair']}.mhclo")
    # Bake the shape; keep the skirt helper if the outfit needs it, then drop the helpers.
    TargetService.bake_targets(body)
    skirt_src = skirt_source(body) if any(it['piece'] in SKIRTED for it in spec['outfit']) else None
    apply_modifier(body, 'MASK')
    for obj in parts.values():
        if obj and obj.data.shape_keys:
            activate(obj); bpy.ops.object.shape_key_remove(all=True, apply_mix=True)
        for m in list(obj.modifiers):
            if m.type not in ('ARMATURE',): activate(obj); bpy.ops.object.modifier_apply(modifier=m.name)
    # Clothing from the outfit.
    eye_z = sum((parts['Eyes'].matrix_world @ v.co).z for v in parts['Eyes'].data.vertices) / len(parts['Eyes'].data.vertices)
    info = Body(body, rig, eye_z)
    hide = [False] * len(info.co)
    skirt = None
    if skirt_src:
        skirt = Body(skirt_src, rig, eye_z); skirt.floor, skirt.top = info.floor, info.top
        # Legs under a long skirt can't be seen: the skin goes, as under other cloth.
        if any(it['piece'] in ('skirt', 'apron') for it in spec['outfit']):
            hide = [info.bone[i] in Body.LEGS | {'pelvis'} and info.floor + .16 < c.z < info.waist_z - .1 for i, c in enumerate(info.co)]
    meshes = [body] + [o for o in parts.values() if o]
    slots = {}
    for item in spec['outfit']:
        piece = item['piece']; slot = item.get('slot', piece.capitalize())
        if piece == 'trim':
            item = dict(item, of_opts=next(it for it in spec['outfit'] if it.get('id', it['piece']) == item['of']))
        obj, keep = make_piece(skirt if piece in SKIRTED else info, piece, item, slot_material(slot))
        if piece in SKIRTED: soften_skirt(obj, info)
        meshes.append(obj)
        folder = ROOT / item['folder'] if 'folder' in item else CLOTH
        slots[slot] = {'type': 'fabric', 'fabric': item['fabric'], 'tint': item['tint'], 'tile_cm': TILE_CM[item['fabric']], 'gain': fabric_gain(item['fabric'], folder)}
        if 'folder' in item: slots[slot]['folder'] = item['folder']
        if piece == 'helmet' and item.get('nasal'): meshes.append(nasal_guard(info, slot_material(slot)))
        rigid = item.get('rigid') or item.get('of_opts', {}).get('rigid')
        if rigid: rigidify(obj, info, rigid)
        if PIECES[piece]['hides']: hide = [h or k for h, k in zip(hide, keep)]
    if skirt_src: bpy.data.objects.remove(skirt_src)
    if spec.get('beard'):
        meshes += make_beard(info, dict(spec['beard'], _outfit=spec['outfit']), slots)
    # Skin fully under cloth goes (a face survives if any corner shows).
    bm = bmesh.new(); bm.from_mesh(body.data); bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if all(hide[v.index] for v in f.verts)], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(body.data); bm.free()
    # Materials and the textures they use.
    TEX.mkdir(parents=True, exist_ok=True)
    def keep_texture(src):
        src = Path(src); dst = TEX / src.name
        if not dst.exists() or dst.stat().st_size != src.stat().st_size: shutil.copy2(src, dst)
        return f'Textures/{src.name}'
    skin_dir = DATA / 'skins' / spec['skin']
    skin_png = next(p for p in skin_dir.glob('*.png') if 'diffuse' in p.name or p.name.endswith('.png'))
    if spec.get('skin_texture'): skin_png = ROOT / spec['skin_texture']   # the zombie's decayed skin (Tools/build_zombie_textures.py)
    def assign(obj, slot):
        obj.data.materials.clear(); obj.data.materials.append(slot_material(slot))
        for poly in obj.data.polygons: poly.material_index = 0
    assign(body, 'Skin'); slots['Skin'] = {'type': 'skin', 'texture': keep_texture(skin_png)}
    if spec.get('skin_tint'): slots['Skin']['tint'] = spec['skin_tint']   # the zombie's dead grey
    assign(parts['Eyes'], 'Eye'); slots['Eye'] = {'type': 'eye', 'texture': keep_texture(ROOT / spec['eye_texture'] if spec.get('eye_texture') else DATA / f"eyes/materials/{spec['eyes']}_eye.png")}
    assign(parts['Eyebrows'], 'Brow'); slots['Brow'] = {'type': 'card', 'texture': keep_texture(next((DATA / f"eyebrows/{spec['eyebrows']}").glob('*.png')))}
    if spec.get('brow_tint'): slots['Brow']['tint'] = spec['brow_tint']   # the dwarf's auburn brows to match his beard
    lash_dir = DATA / f"eyelashes/{spec['eyelashes']}"
    assign(parts['Eyelashes'], 'Lash'); slots['Lash'] = {'type': 'card', 'texture': keep_texture(next(lash_dir.glob('*.png')))}
    if 'Hair' in parts:
        hair_dir = DATA / f"hair/{spec['hair']}"
        assign(parts['Hair'], 'Hair'); slots['Hair'] = {'type': 'card', 'texture': keep_texture(next(hair_dir.glob('*diffuse*.png')))}
    # Face +X and stand exactly spec height tall (cap included), feet on the ground.
    lo = min((o.matrix_world @ v.co).z for o in meshes for v in o.data.vertices)
    hi = max((o.matrix_world @ v.co).z for o in meshes for v in o.data.vertices)
    scale = spec['height_cm'] / 100. / (hi - lo)
    M = Matrix.Translation((0, 0, 0)) @ Matrix.Rotation(math.radians(90), 4, 'Z') @ Matrix.Scale(scale, 4) @ Matrix.Translation((0, 0, -lo))
    for o in meshes:
        activate(o); bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
    for o in [rig] + meshes:
        o.matrix_world = M @ o.matrix_world
    bpy.ops.object.select_all(action='DESELECT')
    for o in [rig] + meshes: o.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in meshes:
        o.parent = rig
        for m in o.modifiers:
            if m.type == 'ARMATURE': m.object = rig
    # One name for every human's rig: Unreal keeps the armature as the root
    # bone, and the shared skeleton (and the clips) need the same root.
    rig.name = 'HumanRig'
    # Export.
    out = OUT / name; out.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    for o in [rig] + meshes: o.select_set(True)
    bpy.context.view_layer.objects.active = rig
    fbx = out / f'SK_{name}.fbx'
    bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={'ARMATURE', 'MESH'}, **FBX)
    tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in meshes)
    lo2 = min((o.matrix_world @ v.co).z for o in meshes for v in o.data.vertices)
    hi2 = max((o.matrix_world @ v.co).z for o in meshes for v in o.data.vertices)
    print('CHUCK_HUMAN', name, f'tris={tris}', f'height_cm={(hi2 - lo2) * 100:.1f}', f'bones={len(rig.data.bones)}', sorted(slots))
    if REVIEW:
        render_review(name, meshes, slots, spec['height_cm'] / 180.)
    return {'fbx': f'{name}/SK_{name}.fbx', 'height_cm': spec['height_cm'], 'tris': tris, 'slots': slots}


def render_review(name, meshes, slots, k=1.):
    import os
    preview = {'Mail': (.1, .11, .14), 'Skin': (.6, .42, .34), 'Eye': (.1, .08, .06), 'Brow': (.12, .08, .05), 'Lash': (.05, .04, .03), 'Hair': (.15, .1, .06)}
    k3 = lambda v: tuple(x * k for x in v)   # cameras scaled to the body (the dwarf is short)
    for slot, info in slots.items():
        m = bpy.data.materials.get(slot)
        if not m: continue
        c = preview.get(slot) or tuple(min(1., t * .55) for t in info.get('tint', (1, 1, 1)))
        m.diffuse_color = (*c, 1.)
    bpy.ops.mesh.primitive_plane_add(size=6)
    s = bpy.context.scene
    for tag, loc, rot in (('front', (2.6, -1.1, 1.15), (82, 0, 67)), ('back', (-2.6, 1.1, 1.15), (82, 0, 247)),
                          ('feet', (.9, -.5, .45), (60, 0, 60)), ('head', (.75, -.35, 1.68), (88, 0, 65)),
                          ('profile', (.03, -.62, 1.66), (88, 0, 0))):
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam)
        cam.location = k3(loc); cam.rotation_euler = tuple(math.radians(a) for a in rot); cam.data.lens = 45; s.camera = cam
        s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.color_type = 'MATERIAL'; s.display.shading.light = 'STUDIO'
        s.render.resolution_x, s.render.resolution_y = 800, 1000
        s.render.filepath = os.path.join(REVIEW, f'{name}_{tag}.png'); bpy.ops.render.render(write_still=True)


manifest_path = OUT / 'manifest.json'
manifest = json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else {'npcs': {}}
manifest['generator'] = 'Tools/build_npc_humans.py'
manifest['mpfb'] = '2.0.17'
manifest['skeleton'] = 'game_engine (MPFB): 53 bones, Unreal mannequin names, three bones per finger'
for name, spec in SPEC.items():
    if ONLY and name not in ONLY: continue
    manifest['npcs'][name] = build(name, spec)
OUT.mkdir(parents=True, exist_ok=True)
manifest_path.write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
print('CHUCK_HUMANS_READY', sorted(manifest['npcs']))
