"""Blender 4.5.14 + MPFB 2.0.17: the three adventurers of the temple sanctum (user 2026-10-22:
"create the three heroes from the 2d game ... much more detailed and visually impressive than our
NPCs so far"; docs/CHULT-TEMPLE-HEROES.md).

blender --background --python Tools/build_temple_heroes.py [-- Fighter Wizard Ranger] [--review <dir>]
  -> SourceAssets/Heroes/<Name>/SK_Hero<Name>.fbx, SourceAssets/Heroes/manifest.json

The body, face rig, eyes, brows, lashes, hair, beard and the cloth layers next to the skin come from
the human kit (Tools/build_npc_humans.py, imported as a module; SourceAssets/Heroes/heroes.json in
its format). Everything a hero wears or carries over that is built here, from the body's own
surface where it fits the body:
  * plate: a shell of the body region pushed out over the layers under it, smoothed until it is a
    plate's form (no anatomy) while staying clear of the body, given thickness, a rolled edge
    (a tube swept round its rim) and rivets; held rigidly by a bone or a fixed blend of two, as
    plate moves (pauldron and fauld lames each on their own blend, so they slide over each other);
  * the wizard's hat, the ranger's mantle and hood, belts, straps, pouches, quiver;
  * the props, skinned into the same mesh on extra bones: weapon_r (sword / staff, in the right
    fist), weapon_tip (the blade's point: the game reads the cut from it), shield_l (on the left
    forearm), weapon_l (the bow in the left fist), bowstring (its nocking point, drawn by the
    clips) and arrow_r (the nocked arrow, in the right fingers).
Each hero has its own skeleton (the kit's 53 bones + face bones + these), so the clips authored
on it (Tools/build_hero_anims.py) place the hands, sword and shield exactly.

Surfaces: CC0 (Tools/Fetch-HeroSurfaces.ps1: ambientCG Metal054A / Metal038, Poly Haven velvet,
herringbone, leather, suede, hessian; the kit's Poly Haven cloth and generated mail). Metal, wood,
leather and gem slots are type 'pbr' (the importer's M_HeroSurface: colour x tint, roughness and
metal maps, normal, real-size UVs, and the baked vertex colour: R ambient occlusion, G edge wear,
B grime). Faces +X, centimetres via FBX, like the humans.
"""
from pathlib import Path
import importlib.util
import json
import math
import random
import sys
import bmesh
import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[1]
spec_ = importlib.util.spec_from_file_location('npc_kit', ROOT / 'Tools/build_npc_humans.py')
kit = importlib.util.module_from_spec(spec_); spec_.loader.exec_module(kit)
Body = kit.Body

HEROES = json.loads((ROOT / 'SourceAssets/Heroes/heroes.json').read_text(encoding='utf-8'))['heroes']
OUT = ROOT / 'SourceAssets/Heroes'
SURF = ROOT / 'SourceAssets/Surfaces/Heroes'
SURF_REL = 'SourceAssets/Surfaces/Heroes'
kit.OUT, kit.TEX = OUT, OUT / 'Textures'
for rec in json.loads((SURF / 'manifest.json').read_text(encoding='utf-8'))['assets']:
    kit.TILE_CM[rec['asset']] = rec['size_cm']
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None
kit.REVIEW = None   # the heroes' own review below
ONLY = [a for a in argv if a in HEROES]

clamp = lambda x: min(1., max(0., x))
step = lambda a, b, x: (lambda t: t * t * (3 - 2 * t))(clamp((x - a) / (b - a)))

# ------------------------------------------------------------------ surfaces
# A 'pbr' slot: a texture set, its tint (linear albedo multiplier), the real size of one repeat.
PBR = {
    'steel':      dict(set='metal054a', folder=SURF_REL, tint=(.34, .335, .33), rough=1.4, metal=1.0, tile_cm=60.),   # 2026-10-23 in-game: 1.0 read chalk-white under the torches
    'steel_dark': dict(set='metal038', folder=SURF_REL, tint=(.7, .7, .7), rough=1.15, metal=1.0, tile_cm=50.),
    'gilt':       dict(set='metal054a', folder=SURF_REL, tint=(1.0, .72, .32), rough=.8, metal=1.0, tile_cm=40.),
    'leather':    dict(set='fabric_leather_02', folder=SURF_REL, tint=(.09, .055, .032), rough=1.0, metal=0., tile_cm=None),
    'leather_dk': dict(set='fabric_leather_02', folder=SURF_REL, tint=(.035, .024, .016), rough=1.0, metal=0., tile_cm=None),
    'suede':      dict(set='scuba_suede', folder=SURF_REL, tint=(.11, .075, .045), rough=1.0, metal=0., tile_cm=None),
    'wood':       dict(set='rough_wood', folder='SourceAssets/Surfaces/WeatheredTimber', tint=(.5, .38, .26), rough=1.0, metal=0., tile_cm=None),
    'wood_dark':  dict(set='rough_wood', folder='SourceAssets/Surfaces/WeatheredTimber', tint=(.28, .2, .14), rough=1.0, metal=0., tile_cm=None),
    'paint':      dict(set='fabric_leather_02', folder=SURF_REL, tint=(.05, .1, .2), rough=1.0, metal=0., tile_cm=None),
    'gem':        dict(set=None, tint=(.12, .75, .58), rough=.08, metal=0., tile_cm=None, emissive=(.2, 2.2, 1.6)),
    'wool_green': dict(set='poly_wool_herringbone', folder=SURF_REL, tint=(.045, .07, .04), rough=1.0, metal=0., tile_cm=None),
    'velvet':     dict(set='velour_velvet', folder=SURF_REL, tint=(.02, .028, .09), rough=1.0, metal=0., tile_cm=None),
    'gold_cloth': dict(set='poly_wool_herringbone', folder=SURF_REL, tint=(.6, .38, .08), rough=.7, metal=0., tile_cm=None),
    'rope':       dict(set='hessian_230', folder=SURF_REL, tint=(.32, .25, .16), rough=1.0, metal=0., tile_cm=None),
    'string':     dict(set=None, tint=(.6, .58, .52), rough=.7, metal=0., tile_cm=None),
    'feather':    dict(set=None, tint=(.42, .38, .33), rough=.8, metal=0., tile_cm=None),
}
WRAPPED = set()   # slots whose objects already carry UVs (a wrap along a grip, a bow's limbs)


def tile_cm(kind):
    spec = PBR[kind]
    if spec.get('tile_cm'): return spec['tile_cm']
    if spec.get('set') in kit.TILE_CM: return kit.TILE_CM[spec['set']]
    return 50.


def pbr_slot(slots, kind):
    """The hero slot 'Hero_<kind>' and its manifest entry (the importer's M_HeroSurface)."""
    name = f'Hero_{kind}'
    s = PBR[kind]
    slots[name] = {'type': 'pbr', 'set': s.get('set'), 'folder': s.get('folder'), 'tint': list(s['tint']),
                   'rough': s['rough'], 'metal': s['metal'], 'tile_cm': tile_cm(kind)}
    if s.get('emissive'): slots[name]['emissive'] = list(s['emissive'])
    if s.get('set') and s['metal'] < .5:   # cloth/leather sets: weave only, brightness-normalised like the kit's fabric
        slots[name]['gain'] = kit.fabric_gain(s['set'], ROOT / s['folder'])
    return kit.slot_material(name)


# ------------------------------------------------------------------ geometry helpers
def link(obj):
    bpy.context.collection.objects.link(obj); return obj


def body_bvh(info):
    bm = bmesh.new(); bm.from_mesh(info.obj.data); bm.transform(info.obj.matrix_world)
    tree = BVHTree.FromBMesh(bm); bm.free()
    return tree


class Frame:
    """Joint positions of the rig for either side (s = +1 his left, -1 his right) and the body's BVH."""
    def __init__(self, info):
        self.info = info
        self.L = 1. if info.shoulder.x > 0 else -1.
        rig = info.rig
        self.J = lambda b, tail=False: rig.matrix_world @ (rig.data.bones[b].tail_local if tail else rig.data.bones[b].head_local)
        self.tree = body_bvh(info)
        R3 = info.obj.matrix_world.to_3x3()
        self.normal = [(R3 @ v.normal).normalized() for v in info.obj.data.vertices]
        self.i = 0   # the vertex a margin function is being asked about (shell sets it)
        nose, chin, mid_y = kit.face_marks(info)
        self.nose, self.chin, self.head_mid_y = nose, chin, mid_y

    def side_of(self, c):
        return 'l' if c.x * self.L > 0 else 'r'

    def j(self, name, s, tail=False):
        """Joint of bone `name_<side>` (s = 'l' / 'r')."""
        return self.J(f'{name}_{s}', tail)


def boundary_loops(bm, min_len=6):
    nxt = {}
    for e in bm.edges:
        if e.is_boundary:
            a, b = e.verts; nxt.setdefault(a, []).append(b); nxt.setdefault(b, []).append(a)
    loops, seen = [], set()
    for start in list(nxt):
        if start in seen or len(nxt[start]) != 2: continue
        loop, prev, cur = [start], None, start
        seen.add(start)
        while True:
            nb = next((n for n in nxt[cur] if n is not prev and len(nxt[n]) == 2), None)
            if nb is None or nb is start or nb in seen: break
            loop.append(nb); seen.add(nb); prev, cur = cur, nb
        if len(loop) >= min_len: loops.append([(v.co.copy(), v.normal.copy()) for v in loop])
    return loops


def resample(loop, spacing):
    """Points every `spacing` m along a closed loop of (co, normal)."""
    pts = [p for p, _ in loop]; ns = [n for _, n in loop]
    out, carry = [], 0.
    for k in range(len(pts)):
        a, b = pts[k], pts[(k + 1) % len(pts)]
        seg = (b - a).length
        t = carry
        while t < seg:
            f = t / seg if seg > 1e-9 else 0.
            out.append((a.lerp(b, f), ns[k].lerp(ns[(k + 1) % len(pts)], f).normalized()))
            t += spacing
        carry = t - seg
    return out


def smooth_loop(loop, iterations=6):
    pts = [p for p, _ in loop]
    for _ in range(iterations):
        for k_ in (.5, -.52):
            pts = [p + k_ * ((pts[k - 1] + pts[(k + 1) % len(pts)]) * .5 - p) for k, p in enumerate(pts)]
    return [(p, n) for p, (_, n) in zip(pts, loop)]


def sweep(bm, loop, radius, sides=8, closed=True, flatten=1.):
    """A tube along a loop of (co, normal); its section flattened toward the normal (a rolled rim lies flat on the plate)."""
    n = len(loop)
    rings = []
    for k in range(n):
        p, nor = loop[k]
        d = (loop[(k + 1) % n][0] - loop[k - 1][0]) if closed else (loop[min(k + 1, n - 1)][0] - loop[max(k - 1, 0)][0])
        if d.length < 1e-9: d = Vector((0, 0, 1))
        d.normalize()
        up = (nor - d * nor.dot(d))
        if up.length < 1e-6: up = d.orthogonal()
        up.normalize(); side = d.cross(up).normalized()
        rings.append([bm.verts.new(p + up * math.sin(2 * math.pi * j / sides) * radius * flatten + side * math.cos(2 * math.pi * j / sides) * radius) for j in range(sides)])
    for k in range(n if closed else n - 1):
        a, b = rings[k], rings[(k + 1) % n]
        for j in range(sides): bm.faces.new((a[j], a[(j + 1) % sides], b[(j + 1) % sides], b[j]))
    if not closed:
        bm.faces.new(list(reversed(rings[0]))); bm.faces.new(rings[-1])


def dome(bm, p, nor, r, h, seg=6):
    """A rivet head: a low dome on a surface."""
    nor = nor.normalized(); a = nor.orthogonal().normalized(); b = nor.cross(a)
    top = bm.verts.new(p + nor * h)
    rim = [bm.verts.new(p + (a * math.cos(2 * math.pi * k / seg) + b * math.sin(2 * math.pi * k / seg)) * r - nor * .001) for k in range(seg)]
    mid = [bm.verts.new(p + nor * h * .7 + (a * math.cos(2 * math.pi * k / seg) + b * math.sin(2 * math.pi * k / seg)) * r * .7) for k in range(seg)]
    for k in range(seg):
        bm.faces.new((rim[k], rim[(k + 1) % seg], mid[(k + 1) % seg], mid[k]))
        bm.faces.new((mid[k], mid[(k + 1) % seg], top))


def keep_clear(bm, frame, clear, iterations=10, smooth=.5, pin_boundary=True, taubin=False):
    """Smooth a shell toward a plate's simple form while every vertex stays at least `clear` off the body.
    Taubin steps (no shrinkage) alternate with pushing any vertex that came too close back out along
    the body's normal there."""
    tree = frame.tree
    verts = [v for v in bm.verts if not (pin_boundary and v.is_boundary)]
    nbr = {v: [e.other_vert(v) for e in v.link_edges] for v in verts}
    for _ in range(iterations):
        # Plain Laplacian steps: hollows fill outward, swellings would sink, but the clearance below
        # holds them out, so the shell settles toward a smooth, convex plate over the body.
        for k_ in ((smooth, -smooth * 1.04) if taubin else (smooth,)):
            new = {v: v.co + k_ * (sum((n.co for n in nbr[v]), Vector()) / len(nbr[v]) - v.co) for v in verts if nbr[v]}
            for v, co in new.items(): v.co = co
        bm.normal_update()
        for v in bm.verts:
            loc, nor, _, dist = tree.find_nearest(v.co)
            if loc is None or dist > clear * 3: continue
            # Only against a body face turned the shell's way (not the far side of an ear, a fold, a crease),
            # and never more than a little at a time.
            if nor.dot(v.normal) < .35: continue
            off = (v.co - loc).dot(nor)
            if off < clear: v.co += nor * min(clear - off, clear * .5)
    bm.normal_update()
    # A last guard: nothing may stand further out than twice the offset from the body (a stray vertex is a spike).
    for v in bm.verts:
        loc, nor, _, dist = tree.find_nearest(v.co)
        if loc is not None and dist > clear * 2.6:
            v.co = loc + (v.co - loc).normalized() * clear * 1.2


def shell(frame, margin, offset, name, mat, smooth=0, clear=None, hem=24, keep_boundary=False):
    """The body's surface where margin(c, bone) > 0, cut on its zero line, hems relaxed, pushed `offset`
    out and (smooth > 0) smoothed into a plate's form staying `clear` off the body. Keeps the body's
    skin weights. Returns (object, boundary loops of its outer surface before thickness)."""
    info = frame.info
    obj = info.obj.copy(); obj.data = info.obj.data.copy(); obj.name = name; link(obj)
    for m in list(obj.modifiers):
        if m.type != 'ARMATURE': obj.modifiers.remove(m)
    bm = bmesh.new(); bm.from_mesh(obj.data)
    margins = []
    for i in range(len(info.co)):
        frame.i = i
        margins.append(margin(info.co[i], info.bone[i]))
    kit.cut_along(bm, margins)
    if not bm.faces: raise RuntimeError(f'{name}: empty region')
    kit.relax_hems(bm, hem)
    bm.normal_update()
    for v in bm.verts: v.co += v.normal * offset
    if smooth: keep_clear(bm, frame, clear if clear is not None else offset * .85, smooth, pin_boundary=keep_boundary)
    loops = boundary_loops(bm)
    bm.to_mesh(obj.data); bm.free()
    obj.data.materials.clear(); obj.data.materials.append(mat)
    for p in obj.data.polygons: p.use_smooth = True
    return obj, loops


def thicken(obj, thickness, offset=1.):
    kit.activate(obj)
    sol = obj.modifiers.new('Plate', 'SOLIDIFY'); sol.thickness = thickness; sol.offset = offset; sol.use_rim = True; sol.use_even_offset = False
    bpy.ops.object.modifier_move_to_index(modifier=sol.name, index=0); bpy.ops.object.modifier_apply(modifier=sol.name)
    for p in obj.data.polygons: p.use_smooth = True


def subdivide(obj, levels=1):
    kit.activate(obj)
    m = obj.modifiers.new('Sub', 'SUBSURF'); m.levels = levels; m.render_levels = levels
    bpy.ops.object.modifier_move_to_index(modifier=m.name, index=0); bpy.ops.object.modifier_apply(modifier=m.name)


def rigid(obj, weights):
    """Every vertex on the same bones: weights = {bone: w} or a function(world co) -> {bone: w}."""
    for g in list(obj.vertex_groups): obj.vertex_groups.remove(g)
    for v in obj.data.vertices:
        w = weights(obj.matrix_world @ v.co) if callable(weights) else weights
        for bone, x in w.items():
            if x <= 0: continue
            g = obj.vertex_groups.get(bone) or obj.vertex_groups.new(name=bone)
            g.add([v.index], x, 'REPLACE')
    if not any(m.type == 'ARMATURE' for m in obj.modifiers):
        obj.modifiers.new('Armature', 'ARMATURE').object = HERO['rig']


def rim_and_rivets(frame, loops, mat_rim, mat_rivet, name, thickness, radius=.0042, rivet_every=None, inset=.014, rivet_r=.0045, skip=None):
    """A rolled rim along each loop (on the plate's outer face) and rivets `inset` in from it."""
    bm = bmesh.new()
    rbm = bmesh.new()
    for loop in loops:
        loop = smooth_loop(loop)
        pts = [(p + n * thickness * .5, n) for p, n in loop]
        sweep(bm, pts, radius, sides=8, flatten=.8)
        if rivet_every:
            centre = sum((p for p, _ in loop), Vector()) / len(loop)
            for p, n in resample(loop, rivet_every):
                if skip and skip(p): continue
                inward = (centre - p); inward -= n * inward.dot(n)
                if inward.length < 1e-6: continue
                dome(rbm, p + inward.normalized() * inset + n * thickness, n, rivet_r, rivet_r * .7)
    rim = kit.new_object(name + '_rim', bm); rim.data.materials.append(mat_rim)
    for p in rim.data.polygons: p.use_smooth = True
    out = [rim]
    if len(rbm.verts):
        riv = kit.new_object(name + '_rivets', rbm); riv.data.materials.append(mat_rivet)
        for p in riv.data.polygons: p.use_smooth = True
        out.append(riv)
    else: rbm.free()
    return out


HERO = {}   # the hero being built: rig, frame, slots


def plate(frame, name, margin, offset, weights, thickness=.0024, smooth=12, rim=True, rivet_every=None, subdiv=0, shape=None, kind='steel', clear=None, rim_r=.0042):
    """A steel plate: shell, form, optional shape(bm-less co edit), thickness, rim, rivets, rigid weights."""
    slots = HERO['slots']
    mat = pbr_slot(slots, kind); dark = pbr_slot(slots, 'steel_dark')
    obj, loops = shell(frame, margin, offset, name, mat, smooth=smooth, clear=clear)
    if shape:
        for v in obj.data.vertices: v.co = shape(v.co.copy())
        loops = [[(shape(p.copy()), n) for p, n in loop] for loop in loops]
    if subdiv: subdivide(obj, subdiv)
    thicken(obj, thickness)
    parts = [obj]
    if rim: parts += rim_and_rivets(frame, loops, mat, dark, name, thickness, radius=rim_r, rivet_every=rivet_every)
    for p in parts: rigid(p, weights)
    return parts


def mirror_x(c, s, L):
    """For a side: whether c is on side s ('l'/'r')."""
    return (c.x * L > 0) == (s == 'l')


# ------------------------------------------------------------------ the extra bones
def add_bone(rig, name, parent, head, tail, roll_toward=None):
    kit.activate(rig); bpy.ops.object.mode_set(mode='EDIT')
    eb = rig.data.edit_bones.new(name)
    inv = rig.matrix_world.inverted()
    eb.head, eb.tail = inv @ head, inv @ tail
    eb.parent = rig.data.edit_bones[parent]
    if roll_toward is not None: eb.align_roll(inv.to_3x3() @ roll_toward)
    eb.use_deform = True
    bpy.ops.object.mode_set(mode='OBJECT')


def grip_frame(frame, s):
    """Where a fist on side s holds a shaft: the palm's centre and the shaft's axis (from the little
    finger's side out past the thumb's), in the rest pose."""
    hand, knuckles = frame.j('hand', s), frame.j('middle_01', s)
    index, pinky = frame.j('index_01', s), frame.j('pinky_01', s)
    along = (knuckles - hand).normalized()                  # wrist to knuckles
    across = (index - pinky).normalized()                   # little finger to index
    palm = along.cross(across).normalized()
    # The palm faces the thumb's side of `palm` or the other: the thumb's base lies to the palm side.
    thumb = frame.j('thumb_01', s)
    if (thumb - hand).dot(palm) < 0: palm = -palm
    centre = hand + along * (knuckles - hand).length * .72 + palm * .024
    return centre, across, palm, along


# ------------------------------------------------------------------ props
def lathe(bm, profile, axis_o, axis_d, ref, seg=16, scale_xy=(1., 1.)):
    """Revolve [(t along axis, radius)] round an axis; ref fixes the angle 0 side. scale_xy squashes the section."""
    axis_d = axis_d.normalized(); a = (ref - axis_d * ref.dot(axis_d)).normalized(); b = axis_d.cross(a)
    rings = []
    for t, r in profile:
        rings.append([bm.verts.new(axis_o + axis_d * t + (a * math.cos(2 * math.pi * k / seg) * scale_xy[0] + b * math.sin(2 * math.pi * k / seg) * scale_xy[1]) * r) for k in range(seg)])
    for ra, rb in zip(rings, rings[1:]):
        for k in range(seg): bm.faces.new((ra[k], ra[(k + 1) % seg], rb[(k + 1) % seg], rb[k]))
    if profile[0][1] > 0: bm.faces.new(list(reversed(rings[0])))
    if profile[-1][1] > 0: bm.faces.new(rings[-1])
    return rings


def wrap_uv(obj, axis_o, axis_d, ref, u_len, v_len):
    """Cylindrical UVs round an axis: u round, v along (a grip's wrap, a bow's limb)."""
    axis_d = axis_d.normalized(); a = (ref - axis_d * ref.dot(axis_d)).normalized(); b = axis_d.cross(a)
    me = obj.data
    while me.uv_layers: me.uv_layers.remove(me.uv_layers[0])
    uv = me.uv_layers.new(name='UVMap')
    for poly in me.polygons:
        us = []
        for li in poly.loop_indices:
            co = obj.matrix_world @ me.vertices[me.loops[li].vertex_index].co - axis_o
            us.append(((math.atan2(co.dot(b), co.dot(a)) / (2 * math.pi)) % 1., co.dot(axis_d)))
        if max(u for u, _ in us) - min(u for u, _ in us) > .5: us = [((u + .5) % 1. - .5, v) for u, v in us]
        for li, (u, v) in zip(poly.loop_indices, us): uv.data[li].uv = (u * u_len, v / v_len)


def make_sword(frame, slots, centre, axis, flat):
    """An arming sword: a 79 cm blade (diamond section, a fuller down two thirds, a little distal taper),
    a gilt cruciform guard, a leather-bound grip and a gilt wheel pommel. axis: the blade's way,
    flat: the blade's flat (the cutting edges lie across it)."""
    axis = axis.normalized(); flat = (flat - axis * flat.dot(axis)).normalized(); edge = axis.cross(flat)
    steel, gilt, leather = pbr_slot(slots, 'steel'), pbr_slot(slots, 'gilt'), pbr_slot(slots, 'leather_dk')
    out = []
    guard_t = .062            # the guard's middle, this far up the axis from the grip's centre
    L = .79
    # The blade: rows of a diamond section with a fuller, from the guard to the point.
    bm = bmesh.new()
    rows = []
    n = 40
    for k in range(n + 1):
        t = k / n
        w = .026 * (1 - t) + .0105 * t                      # half width: 5.2 cm at the guard to 2.1 at the start of the point
        if t > .86: w *= math.sqrt(max(0., 1 - ((t - .86) / .14) ** 1.6))   # the point
        th = .0032 * (1 - t) + .0018 * t                     # half thickness: distal taper
        if t > .86: th *= max(.15, 1 - (t - .86) / .14 * .85)
        fuller = .0012 if t < .62 else .0012 * max(0., 1 - (t - .62) / .1)
        fw = .006 * (1 - t * .6)
        p = centre + axis * (guard_t + .01 + L * t)
        # Section: edge, bevel to the ridge, the fuller's lip, the fuller's bottom, mirrored; both faces.
        sec = [(w, 0.), (w * .55, th * .85), (fw * 1.4, th), (fw, th - fuller), (0., th - fuller * 1.2),
               (-fw, th - fuller), (-fw * 1.4, th), (-w * .55, th * .85), (-w, 0.),
               (-w * .55, -th * .85), (-fw * 1.4, -th), (-fw, -th + fuller), (0., -th + fuller * 1.2),
               (fw, -th + fuller), (fw * 1.4, -th), (w * .55, -th * .85)]
        rows.append([bm.verts.new(p + edge * x + flat * y) for x, y in sec])
    m = len(rows[0])
    for a, b in zip(rows, rows[1:]):
        for j in range(m): bm.faces.new((a[j], a[(j + 1) % m], b[(j + 1) % m], b[j]))
    bm.faces.new(list(reversed(rows[0])))
    tip = bm.verts.new(centre + axis * (guard_t + .01 + L * 1.002))
    for j in range(m): bm.faces.new((rows[-1][j], rows[-1][(j + 1) % m], tip))
    blade = kit.new_object('sword_blade', bm); blade.data.materials.append(steel)
    for p in blade.data.polygons: p.use_smooth = False
    kit.activate(blade)
    ws = blade.modifiers.new('Sharp', 'WEIGHTED_NORMAL') if hasattr(blade.data, 'use_auto_smooth') else None
    out.append(blade)
    # The guard: a bar 20 cm across the edges, swelling to rounded terminals, its quillons a little curved toward the point.
    bm = bmesh.new()
    prof = []
    for k in range(25):
        u = -1 + 2 * k / 24
        prof.append((u * .1, .0075 * (1 + .55 * abs(u) ** 6) - .0018 * (1 - abs(u))))
    for k, (x, r) in enumerate(prof):
        pass
    # swept as a tube along the edge axis
    pts = [(centre + axis * (guard_t + .012 * (x / .1) ** 2) + edge * x, flat) for x, _ in prof]
    rings = []
    for (p, _), (_, r) in zip(pts, prof):
        rings.append([bm.verts.new(p + flat * math.cos(2 * math.pi * j / 10) * r * 1.15 + axis * math.sin(2 * math.pi * j / 10) * r) for j in range(10)])
    for a, b in zip(rings, rings[1:]):
        for j in range(10): bm.faces.new((a[j], a[(j + 1) % 10], b[(j + 1) % 10], b[j]))
    bm.faces.new(list(reversed(rings[0]))); bm.faces.new(rings[-1])
    # its block (the écusson) over the blade's root
    lathe(bm, [(guard_t - .014, 0.), (guard_t - .014, .011), (guard_t + .016, .011), (guard_t + .016, 0.)], centre, axis, edge, seg=8, scale_xy=(1.6, .75))
    guard = kit.new_object('sword_guard', bm); guard.data.materials.append(gilt)
    for p in guard.data.polygons: p.use_smooth = True
    out.append(guard)
    # The grip: 10.5 cm of cord-bound leather, swelling a little at its middle, ridged by the binding.
    bm = bmesh.new()
    prof = []
    for k in range(29):
        t = -.05 + .104 * k / 28
        r = .0135 + .0017 * math.cos(t / .052 * math.pi * .5) + .0005 * math.cos(k * math.pi)
        prof.append((t, r))
    lathe(bm, prof, centre, axis, edge, seg=14, scale_xy=(1., .82))
    grip = kit.new_object('sword_grip', bm); grip.data.materials.append(leather)
    for p in grip.data.polygons: p.use_smooth = True
    wrap_uv(grip, centre, axis, edge, 1., .1); WRAPPED.add(grip.name)
    out.append(grip)
    # The pommel: a wheel, its faces dished, a peen block beyond it.
    bm = bmesh.new()
    pc = centre - axis * .072
    lathe(bm, [(-.012, 0.), (-.012, .012), (-.0085, .021), (-.004, .024), (0., .0245), (.004, .024), (.0085, .021), (.012, .012), (.012, 0.)],
          pc, edge, axis, seg=20)
    lathe(bm, [(-.06 - .012, 0.), (-.06 - .012, .007), (-.06 + .0, .007), (-.06, 0.)], centre - axis * .012, axis, edge, seg=8)
    pommel = kit.new_object('sword_pommel', bm); pommel.data.materials.append(gilt)
    for p in pommel.data.polygons: p.use_smooth = True
    out.append(pommel)
    tip_at = centre + axis * (guard_t + .01 + L)
    return out, tip_at


def make_shield(frame, slots, centre, out_dir, up, width=.6, height=.78, curve=.11):
    """A heater shield: lime-wood boards under painted leather, curved across, a steel rim, and
    the enarmes (forearm strap and grip) behind. centre: its middle, out_dir: the face's way, up: its top."""
    out_dir = out_dir.normalized(); up = (up - out_dir * up.dot(out_dir)).normalized(); across = up.cross(out_dir)
    paint, wood, steel, leather = pbr_slot(slots, 'paint'), pbr_slot(slots, 'wood_dark'), pbr_slot(slots, 'steel'), pbr_slot(slots, 'leather_dk')
    def outline(v):   # half width at height v (-1 bottom point .. 1 top)
        if v > .55: return width / 2
        return width / 2 * math.sqrt(max(0., 1 - ((.55 - v) / 1.55) ** 2.2))
    nu, nv = 24, 30
    bm = bmesh.new(); back = bmesh.new()
    def pos(u, v, depth):
        hw = outline(v)
        x = u * hw
        bow = curve * (1 - (x / (width / 2)) ** 2) - curve   # curved across (its middle stands out)
        vb = -.02 * (v - .2) ** 2                             # and a little from top to bottom
        return centre + across * x + up * v * height / 2 + out_dir * (bow + vb + depth)
    front, rear = [], []
    for j in range(nv + 1):
        v = -1 + 2 * j / nv
        front.append([bm.verts.new(pos(-1 + 2 * i / nu, v, 0.)) for i in range(nu + 1)])
    for j in range(nv):
        for i in range(nu):
            bm.faces.new((front[j][i], front[j][i + 1], front[j + 1][i + 1], front[j + 1][i]))
    face = kit.new_object('shield_face', bm); face.data.materials.append(paint)
    thicken(face, .016, offset=-1.)
    # The rim: a steel band round the edge.
    loop = []
    for i in range(nu + 1): loop.append((pos(-1 + 2 * i / nu, 1., -.008), out_dir))
    for j in range(nv, -1, -1):
        v = -1 + 2 * j / nv
        loop.append((pos(1., v, -.008), out_dir))
    for i in range(nu, -1, -1): pass
    for j in range(0, nv + 1):
        v = -1 + 2 * j / nv
        loop.append((pos(-1., v, -.008), out_dir))
    # de-duplicate the corners, order: top edge left->right, right side top->bottom, left side bottom->top
    clean = []
    for p, n in loop:
        if not clean or (clean[-1][0] - p).length > 1e-4: clean.append((p, n))
    if (clean[0][0] - clean[-1][0]).length < 1e-4: clean.pop()
    rb = bmesh.new(); sweep(rb, clean, .0095, sides=10, flatten=1.)
    rim = kit.new_object('shield_rim', rb); rim.data.materials.append(steel)
    for p in rim.data.polygons: p.use_smooth = True
    # Rim nails, every 6 cm.
    nb = bmesh.new()
    for p, n in resample(clean, .06): dome(nb, p + out_dir * .0095, out_dir, .005, .004)
    nails = kit.new_object('shield_nails', nb); nails.data.materials.append(steel)
    # Enarmes: two straps on the back, across the forearm.
    eb = bmesh.new()
    for v0, length in ((.18, .2), (-.18, .13)):
        c = pos(0., v0, -.03)
        for k in range(2):
            pass
        verts = [eb.verts.new(c + across * x + up * y - out_dir * z) for x, y, z in (( -length / 2, -.02, 0.), (length / 2, -.02, 0.), (length / 2, .02, 0.), (-length / 2, .02, 0.))]
        eb.faces.new(verts)
    straps = kit.new_object('shield_straps', eb); straps.data.materials.append(leather)
    thicken(straps, .006)
    return [face, rim, nails, straps]


def make_staff(frame, slots, centre, axis, side):
    """The wizard's crooked staff: 1.85 m of twisted wood, knotted, its head a claw of roots round a sea-green gem."""
    wood, gem = pbr_slot(slots, 'wood'), pbr_slot(slots, 'gem')
    axis = axis.normalized(); side = (side - axis * side.dot(axis)).normalized(); third = axis.cross(side)
    rnd = random.Random(5)
    bm = bmesh.new()
    pts, radii = [], []
    bottom, top = -1.22, .62   # held about two thirds of the way up: planted, the gem stands above his head
    n = 70
    for k in range(n + 1):
        t = bottom + (top - bottom) * k / n
        wob = side * (.012 * math.sin(t * 5.1) + .006 * math.sin(t * 13.)) + third * (.01 * math.sin(t * 4.3 + 1.) + .005 * math.sin(t * 11. + 2.))
        crook = side * (.05 * step(.28, .62, t) ** 2)
        pts.append(centre + axis * t + wob + crook)
        r = .0175 - .003 * (t - bottom) / (top - bottom)
        r += .004 * max(0., math.cos((t - .25) * 9.)) ** 8 + .003 * max(0., math.cos((t + .45) * 7.)) ** 10   # knots
        radii.append(r if t > bottom + .02 else r * .8)
    kit.tube(bm, pts, radii, sides=10)
    staff = kit.new_object('staff', bm); staff.data.materials.append(wood)
    for p in staff.data.polygons: p.use_smooth = True
    wrap_uv(staff, centre, axis, side, 1., .3); WRAPPED.add(staff.name)
    # The head: four roots curling up round the gem.
    head = pts[-1] + axis * .01
    gem_c = head + axis * .075 + side * .01
    rb = bmesh.new()
    for k in range(4):
        ang = k * math.pi / 2 + .4
        d = side * math.cos(ang) + third * math.sin(ang)
        path, rr = [], []
        for j in range(12):
            u = j / 11
            p = head + axis * (.13 * u) + d * (.04 * math.sin(u * math.pi * .9) + .008)
            p += (gem_c - p) * .35 * u * u
            path.append(p); rr.append(.0075 * (1 - u * .75))
        kit.tube(rb, path, rr, sides=6)
    roots = kit.new_object('staff_roots', rb); roots.data.materials.append(wood)
    for p in roots.data.polygons: p.use_smooth = True
    wrap_uv(roots, head, axis, side, 1., .3); WRAPPED.add(roots.name)
    gb = bmesh.new()
    bmesh.ops.create_icosphere(gb, subdivisions=1, radius=.034)
    for v in gb.verts:
        v.co = Vector((v.co.x * .8, v.co.y * .8, v.co.z * 1.35))   # an elongated crystal
    M = Matrix((side, third, axis)).transposed().to_4x4(); M.translation = gem_c
    bmesh.ops.transform(gb, matrix=M, verts=gb.verts)
    gemo = kit.new_object('staff_gem', gb); gemo.data.materials.append(gem)
    return [staff, roots, gemo], gem_c


def make_bow(frame, slots, centre, axis, toward):
    """A self longbow, 1.78 m tip to tip, D-section limbs tapering to horn nocks, a leather-bound grip, braced:
    the string 15 cm from the grip on `toward` (the archer's side). Returns the bow, the string (its
    middle on the 'bowstring' bone) and the string's ends and nocking point."""
    wood, leather, string = pbr_slot(slots, 'wood'), pbr_slot(slots, 'leather_dk'), pbr_slot(slots, 'string')
    axis = axis.normalized(); toward = (toward - axis * toward.dot(axis)).normalized(); side = axis.cross(toward)
    half = .89
    bm = bmesh.new()
    rows = []
    n = 60
    for k in range(n + 1):
        t = -half + 2 * half * k / n
        u = abs(t) / half
        bend = -.14 * u ** 1.7        # braced: the limbs curve away from the string
        w = .0165 * (1 - .55 * u ** 1.3) if abs(t) > .07 else .019
        d = .019 * (1 - .5 * u ** 1.2) if abs(t) > .07 else .022
        c = centre + axis * t + toward * bend
        sec = []
        for j in range(12):
            a = 2 * math.pi * j / 12
            x, y = math.cos(a) * w, math.sin(a) * d
            if y > 0: y *= .45          # the D: the back (away from the string) flatter
            sec.append(c + side * x - toward * y)
        rows.append([bm.verts.new(p) for p in sec])
    for a, b in zip(rows, rows[1:]):
        for j in range(12): bm.faces.new((a[j], a[(j + 1) % 12], b[(j + 1) % 12], b[j]))
    bm.faces.new(list(reversed(rows[0]))); bm.faces.new(rows[-1])
    bow = kit.new_object('bow', bm); bow.data.materials.append(wood)
    for p in bow.data.polygons: p.use_smooth = True
    wrap_uv(bow, centre, axis, side, 1., .6); WRAPPED.add(bow.name)
    gb = bmesh.new()
    lathe(gb, [(-.075, .021), (-.07, .0235), (.07, .0235), (.075, .021)], centre + toward * 0., axis, side, seg=14, scale_xy=(.9, 1.2))
    grip = kit.new_object('bow_grip', gb); grip.data.materials.append(leather)
    wrap_uv(grip, centre, axis, side, 1., .05); WRAPPED.add(grip.name)
    ends = [centre + axis * s * (half - .025) + toward * (-.14 * ((half - .025) / half) ** 1.7) for s in (1, -1)]
    nock = centre + toward * .15
    sb = bmesh.new()
    path = [ends[0], nock, ends[1]]
    for a, b in zip(path, path[1:]):
        kit.tube(sb, [a.lerp(b, k / 8) for k in range(9)], [.0018] * 9, sides=5)
    strg = kit.new_object('bowstring', sb); strg.data.materials.append(string)
    return [bow, grip], strg, ends, nock


def make_arrow(slots, nock, axis, side, length=.76):
    """An ash shaft, a bodkin head, three grey goose fletchings."""
    wood, steel, feather = pbr_slot(slots, 'wood'), pbr_slot(slots, 'steel_dark'), pbr_slot(slots, 'feather')
    axis = axis.normalized(); side = (side - axis * side.dot(axis)).normalized(); third = axis.cross(side)
    bm = bmesh.new(); kit.tube(bm, [nock + axis * (length * k / 10) for k in range(11)], [.0042] * 11, sides=6)
    shaft = kit.new_object('arrow_shaft', bm); shaft.data.materials.append(wood)
    hb = bmesh.new()
    lathe(hb, [(length - .005, .0052), (length + .015, .005), (length + .05, 0.)], nock, axis, side, seg=4)
    head = kit.new_object('arrow_head', hb); head.data.materials.append(steel)
    fb = bmesh.new()
    for k in range(3):
        a = 2 * math.pi * k / 3
        d = side * math.cos(a) + third * math.sin(a)
        v = [fb.verts.new(nock + axis * t + d * r) for t, r in ((.025, .004), (.14, .004), (.12, .018), (.05, .016))]
        fb.faces.new(v)
    fl = kit.new_object('arrow_fletch', fb); fl.data.materials.append(feather)
    return [shaft, head, fl]


# ------------------------------------------------------------------ vertex colour: occlusion, edge wear, grime
def bake_colours(objs):
    """R: ambient occlusion (Cycles, 5 cm), G: edge wear (convexity), B: grime (low noise in hollows).
    All in one colour attribute 'Col', the rest pose, every object together so they shade each other."""
    s = bpy.context.scene
    s.render.engine = 'CYCLES'; s.cycles.device = 'CPU'; s.cycles.samples = 48
    s.world = s.world or bpy.data.worlds.new('World')
    s.world.light_settings.distance = .05
    s.render.bake.target = 'VERTEX_COLORS'
    for o in objs:
        if 'Col' not in o.data.color_attributes: o.data.color_attributes.new('Col', 'BYTE_COLOR', 'POINT')
        o.data.color_attributes.active_color = o.data.color_attributes['Col']
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.bake(type='AO')
    for o in objs:
        bm = bmesh.new(); bm.from_mesh(o.data); bm.verts.ensure_lookup_table(); bm.normal_update()
        conv = []
        for v in bm.verts:
            if not v.link_edges: conv.append(0.); continue
            c = sum(((e.other_vert(v).co - v.co).normalized().dot(v.normal) for e in v.link_edges), 0.) / len(v.link_edges)
            conv.append(clamp(-c * 3.5))          # neighbours below the tangent plane: a convex edge
        bm.free()
        attr = o.data.color_attributes['Col']
        for i, d in enumerate(attr.data):
            ao = d.color[0]
            co = o.matrix_world @ o.data.vertices[i].co
            n = .5 + .5 * math.sin(co.x * 23 + co.z * 17) * math.sin(co.y * 19 - co.z * 29 + 1.)
            grime = clamp((1 - ao) * 1.6 + n * .25)
            d.color = (ao, conv[i], grime, 1.)


# ------------------------------------------------------------------ the fighter
def fighter_gear(info, slots, hide):
    F = Frame(info); L = F.L
    HERO.update(rig=info.rig, frame=F, slots=slots)
    neck, hips, head = info.neck, info.hips, info.head
    sh = {s: F.j('upperarm', s) for s in 'lr'}
    el = {s: F.j('lowerarm', s) for s in 'lr'}
    wr = {s: F.j('hand', s) for s in 'lr'}
    th = {s: F.j('thigh', s) for s in 'lr'}
    kn = {s: F.j('calf', s) for s in 'lr'}
    an = {s: F.j('foot', s) for s in 'lr'}
    waist = info.waist_z
    parts = []
    T = Body.TORSO; A = Body.ARMS; FA = Body.FOREARMS; LG = Body.LEGS
    sx = lambda c: F.side_of(c)
    # 1. The breastplate and backplate: from the collarbones to the waist, round the arms, a keel down the front.
    def cuirass_m(c, b):
        if b in A: m = abs(sh['l'].x) - .055 - abs(c.x)
        elif b in T or b == Body.NECK: m = 1.
        else: return -1.
        return min(m, neck.z - .035 - c.z + .02 * clamp((abs(c.x) - .05) / .06), c.z - (waist - .035))
    def keel(c):
        if c.y < hips.y - .02 and c.z > waist - .02:
            c = c.copy(); c.y -= .009 * clamp(1 - abs(c.x) / .07) * step(waist - .02, waist + .12, c.z)
        return c
    parts += plate(F, 'cuirass', cuirass_m, .042, {'spine_03': .55, 'spine_02': .45}, thickness=.003, smooth=16, rivet_every=.055, shape=keel)
    # 2. Faulds: four hooped lames below it, each over the next's top edge, flaring out over the hips.
    for k in range(4):
        z0 = waist - .02 - k * .042
        lame = lambda c, b, z0=z0: (min(z0 - c.z, c.z - (z0 - .052), (abs(c.x) < .2) * 1. - .5) if b in T | LG else -1.)
        parts += plate(F, f'fauld{k}', lame, .046 + .004 * k, {'spine_01': max(0., .6 - .2 * k), 'pelvis': min(1., .4 + .2 * k)}, smooth=18, rivet_every=.09)
    # 3. Tassets: a plate of three lames hanging from the faulds over each thigh's front.
    for s in 'lr':
        for k in range(3):
            z0 = waist - .165 - k * .06
            def tasset(c, b, z0=z0, s=s):
                if b not in LG | T or sx(c) != s or c.y > th[s].y + .02: return -1.
                return min(z0 - c.z, c.z - (z0 - .072), .1 - abs(c.x - th[s].x * 1.05))
            parts += plate(F, f'tasset{k}_{s}', tasset, .044 + .004 * k, {'pelvis': .55 - .1 * k, f'thigh_{s}': .45 + .1 * k}, smooth=14)
    # 4. Pauldrons: a cap over each shoulder and three lames down the upper arm, the lower ones over the upper.
    for s in 'lr':
        S = sh[s]
        def cap(c, b, S=S, s=s):
            if b not in A | T or sx(c) != s: return -1.
            return min(c.z - (S.z - .02), abs(c.x) - (abs(S.x) - .085), .13 - (c - S).length)
        parts += plate(F, f'pauldron_{s}', cap, .05, {f'clavicle_{s}': .6, f'upperarm_{s}': .4}, smooth=14, rivet_every=.05, thickness=.0032)
        for k in range(3):
            z1 = S.z - .02 - k * .038
            def lame(c, b, z1=z1, S=S, s=s):
                if b not in A or sx(c) != s or abs(c.x) < abs(S.x) - .03: return -1.
                return min(z1 + .008 - c.z, c.z - (z1 - .045))
            parts += plate(F, f'pauldron{k}_{s}', lame, .046 + .005 * k, {f'clavicle_{s}': .3 - .1 * k, f'upperarm_{s}': .7 + .1 * k}, smooth=8)
        # 5. Rerebrace (upper-arm tube under the pauldron), couter (elbow cop with its wing), vambrace.
        E, W = el[s], wr[s]
        def rere(c, b, S=S, E=E, s=s):
            if b not in A or sx(c) != s: return -1.
            u = (S - c).dot((S - E).normalized()) / (S - E).length
            return min(u - .45, .88 - u) * .3
        parts += plate(F, f'rerebrace_{s}', rere, .03, {f'upperarm_{s}': 1.}, smooth=6)
        def couter(c, b, E=E, s=s):
            if b not in A | FA or sx(c) != s: return -1.
            return .068 - (c - E).length
        parts += plate(F, f'couter_{s}', couter, .036, {f'upperarm_{s}': .5, f'lowerarm_{s}': .5}, smooth=10, rivet_every=.05, thickness=.003)
        def vamb(c, b, E=E, W=W, s=s):
            if b not in FA or sx(c) != s: return -1.
            u = (c - E).dot((W - E).normalized()) / (W - E).length
            return min(u - .2, .86 - u) * .25
        parts += plate(F, f'vambrace_{s}', vamb, .024, {f'lowerarm_{s}': 1.}, smooth=6, rivet_every=.07)
    # 6. Legs: cuisses down the thighs' fronts, poleyns over the knees, greaves round the shins.
    for s in 'lr':
        H, K, Ak = th[s], kn[s], an[s]
        def cuisse(c, b, H=H, K=K, s=s):
            if b not in LG or sx(c) != s or c.y > H.y + .045: return -1.
            return min(H.z - .1 - c.z, c.z - (K.z + .025))
        parts += plate(F, f'cuisse_{s}', cuisse, .026, {f'thigh_{s}': 1.}, smooth=8, rivet_every=.08)
        def poleyn(c, b, K=K, s=s):
            if b not in LG or sx(c) != s or c.y > K.y + .035: return -1.
            d = c - (K + Vector((0, -.02, 0)))
            d.x *= .8   # wider than tall: the wing round the outside of the knee
            return .088 - d.length
        parts += plate(F, f'poleyn_{s}', poleyn, .03, {f'thigh_{s}': .5, f'calf_{s}': .5}, smooth=10, rivet_every=.045, thickness=.003)
        def greave(c, b, K=K, Ak=Ak, s=s):
            if b not in LG | Body.FEET or sx(c) != s: return -1.
            return min(K.z - .075 - c.z, c.z - (Ak.z + .02))
        parts += plate(F, f'greave_{s}', greave, .018, {f'calf_{s}': 1.}, smooth=8)
    # 7. Gauntlets: suede gloves on the hands, a flared steel cuff and a plate over the back of each hand.
    for s in 'lr':
        W = wr[s]
        def glove(c, b, W=W, s=s):
            if not b or not b.startswith(('hand_', 'thumb_', 'index_', 'middle_', 'ring_', 'pinky_')) or sx(c) != s: return -1.
            return 1.
        g, _ = shell(F, glove, .0028, f'glove_{s}', pbr_slot(slots, 'suede'), smooth=0)
        thicken(g, .002); parts.append(g)
        E = el[s]
        def cuff(c, b, W=W, E=E, s=s):
            if b not in FA | {f'hand_{s}'} or sx(c) != s: return -1.
            u = (c - W).dot((E - W).normalized())
            return min(u + .01, .075 - u) * .5
        cf = plate(F, f'cuff_{s}', cuff, .022, {f'hand_{s}': .7, f'lowerarm_{s}': .3}, smooth=4, thickness=.0025)
        # flare toward the wrist end
        for o in cf:
            ax = (E - W).normalized()
            for v in o.data.vertices:
                co = o.matrix_world @ v.co; u = (co - W).dot(ax)
                radial = (co - W) - ax * u
                if radial.length > 1e-5: v.co += o.matrix_world.inverted().to_3x3() @ (radial.normalized() * .012 * clamp(1 - (u + .01) / .085))
        parts += cf
        K1 = F.j('middle_01', s)
        def back_plate(c, b, W=W, K1=K1, s=s):
            if b != f'hand_{s}' or sx(c) != s: return -1.
            palm_side = F.j('thumb_01', s)
            return min(.03 - abs((c - (W + K1) * .5).dot((K1 - W).normalized())), 1.)
        # (the back-of-hand plate is cut by the palm test below)
        def back_m(c, b, W=W, K1=K1, s=s):
            if b != f'hand_{s}' or sx(c) != s: return -1.
            _, across, palm, along = grip_frame(F, s)
            return min((W - c).dot(palm) + .002, .032 - abs((c - W).dot(along) - .05))
        parts += plate(F, f'handplate_{s}', back_m, .009, {f'hand_{s}': 1.}, smooth=4, thickness=.0022, rim_r=.0028)
    # 8. Helmet: an open-faced bascinet, its crown drawn up to a blunt point behind, its face opening rolled,
    #    and a mail aventail from its lower edge over the neck and the tops of the shoulders.
    nose, chin = F.nose, F.chin
    head_c = Vector((0, F.head_mid_y, info.eye_z + .01))
    def basc(c, b):
        if b not in (Body.HEAD, Body.NECK): return -1.
        if F.normal[F.i].dot((c - head_c).normalized()) < .2: return -1.   # the outside of the head only (not the mouth, the ears' hollows)
        back = clamp((c.y - F.head_mid_y + .02) / .06)
        low = (info.eye_z - .075) * (1 - back) + (neck.z + .03) * back
        m = c.z - low
        if c.y < F.head_mid_y:   # the face opening
            open_ = min(.06 - abs(c.x), info.brow_z + .012 - c.z, c.z - (chin.z - .01))
            m = min(m, -open_)
        return m
    def apex(c):
        a = Vector((0, F.head_mid_y + .02, info.top))
        t = clamp(1 - Vector((c.x, c.y - a.y, 0)).length / .1)
        return c + Vector((0, .012 * t * t, .035 * t * t))
    parts += plate(F, 'helmet', basc, .024, {'head': 1.}, smooth=18, rivet_every=.045, shape=apex, thickness=.0032, rim_r=.005)
    def aventail(c, b):
        if b not in (Body.HEAD, Body.NECK) and b not in T | A: return -1.
        back = clamp((c.y - F.head_mid_y + .02) / .06)
        top = (info.eye_z - .06) * (1 - back) + (neck.z + .05) * back
        m = min(top - c.z, c.z - (neck.z - .085), .2 - abs(c.x))
        if c.y < F.head_mid_y and c.z > chin.z - .02: m = min(m, abs(c.x) - .058)
        return m
    av, _ = shell(F, aventail, .036, 'aventail', kit.slot_material('Mail'), smooth=8)
    thicken(av, .004); kit.box_uv(av, kit.TILE_CM['chainmail']); parts.append(av)
    kit.skin_from_body(av, info) if not av.vertex_groups else None
    # 9. A sword belt round the hips with a scabbard hanging on his left, and the guard's belt over the faulds.
    belt_mat = pbr_slot(slots, 'leather')
    def belt(c, b):
        if b not in T | LG: return -1.
        z = waist - .12 + .03 * (c.x * L)    # slung low, dropping to his left
        return .021 - abs(c.z - z)
    bo, bl = shell(F, belt, .07, 'swordbelt', belt_mat, smooth=8, clear=.064)
    thicken(bo, .005); rigid(bo, {'pelvis': 1.}); parts.append(bo)
    sb = bmesh.new()
    top = Vector((L * .17, hips.y + .02, waist - .1))
    tipp = top + Vector((L * .03, .33, -.72)).normalized() * .84
    kit.tube(sb, [top.lerp(tipp, k / 12) for k in range(13)], [.022 - .01 * (k / 12) ** 2 for k in range(13)], sides=10)
    scab = kit.new_object('scabbard', sb); scab.data.materials.append(pbr_slot(slots, 'leather_dk'))
    for p in scab.data.polygons: p.use_smooth = True
    cb = bmesh.new(); kit.tube(cb, [tipp - (tipp - top).normalized() * .06 * k / 4 for k in range(5)], [.0135, .016, .016, .016, .0135], sides=10)
    chape = kit.new_object('scabbard_chape', cb); chape.data.materials.append(pbr_slot(slots, 'gilt'))
    for o in (scab, chape): rigid(o, {'pelvis': 1.}); parts.append(o)
    # 10. The sword in his right fist, the shield on his left forearm (extra bones).
    rig = info.rig
    gc, across, palm, along = grip_frame(F, 'r')
    blade_dir = (across * .92 + along * .38).normalized()
    sw, tip = make_sword(F, slots, gc, blade_dir, palm)
    add_bone(rig, 'weapon_r', 'hand_r', gc, gc + blade_dir * .2, roll_toward=palm)
    add_bone(rig, 'weapon_tip', 'weapon_r', tip, tip + blade_dir * .05, roll_toward=palm)
    for o in sw: rigid(o, {'weapon_r': 1.}); parts.append(o)
    E, W = el['l'], wr['l']
    fa = (W - E).normalized()
    out_dir = Vector((L, 0, 0)) - fa * fa.dot(Vector((L, 0, 0)))   # out from his left forearm (A-pose: the arm hangs outward)
    out_dir.normalize()
    sc = E.lerp(W, .5) + out_dir * .085
    sh_parts = make_shield(F, slots, sc, out_dir, -fa)
    add_bone(rig, 'shield_l', 'lowerarm_l', sc, sc - fa * .2, roll_toward=out_dir)
    for o in sh_parts: rigid(o, {'shield_l': 1.}); parts.append(o)
    HERO['props'] = {'sword_grip': list(gc), 'sword_tip': list(tip), 'shield_centre': list(sc)}
    return parts


# ------------------------------------------------------------------ the wizard
def wizard_gear(info, slots, hide):
    F = Frame(info); L = F.L
    HERO.update(rig=info.rig, frame=F, slots=slots)
    neck, waist = info.neck, info.waist_z
    T, A = Body.TORSO, Body.ARMS
    parts = []
    velvet, gold = pbr_slot(slots, 'velvet'), pbr_slot(slots, 'gold_cloth')
    # A mantle: a short cape over the shoulders, longer behind, with a broad gilt-thread band at its edge.
    def mantle(c, b):
        if b not in T | A and b != Body.NECK: return -1.
        back = clamp((c.y - info.hips.y) / .06 + .5)
        bottom = (neck.z - .2) * (1 - back) + (neck.z - .3) * back
        m = min(c.z - bottom, neck.z + .03 - c.z)
        if b in A: m = min(m, abs(info.shoulder.x) + .07 - abs(c.x))
        if c.y < info.hips.y: m = min(m, abs(c.x) - .035 - max(0., neck.z - .02 - c.z) * .35)   # open at the front
        return m
    mo, loops = shell(F, mantle, .026, 'mantle', velvet, smooth=6, clear=.022)
    thicken(mo, .006); kit.skin_from_body(mo, info); kit.box_uv(mo, kit.TILE_CM['velour_velvet']); parts.append(mo)
    bm = bmesh.new()
    for loop in loops: sweep(bm, [(p + n * .004, n) for p, n in smooth_loop(loop)], .011, sides=8, flatten=.35)
    band = kit.new_object('mantle_band', bm); band.data.materials.append(gold); kit.skin_from_body(band, info); parts.append(band)
    # A rope girdle with tasselled ends, a leather pouch and a scroll case.
    rope = pbr_slot(slots, 'rope')
    ring = []
    torso = [c for i, c in enumerate(info.co) if info.bone[i] in T and abs(c.z - (waist - .02)) < .012]
    for k in range(48):
        a = 2 * math.pi * k / 48
        d = Vector((math.cos(a), math.sin(a), 0))
        far = max((c for c in torso), key=lambda c: (c - Vector((0, info.hips.y, c.z))).dot(d))
        r = (far - Vector((0, info.hips.y, far.z))).dot(d) + .03
        ring.append((Vector((0, info.hips.y, waist - .02)) + d * r, d))
    rb = bmesh.new(); sweep(rb, ring, .009, sides=8)
    front = ring[int(48 * .75)][0]   # -Y
    for sxn in (-.04, .03):
        path = [front + Vector((sxn, -.01, -k * .045)) for k in range(9)]
        kit.tube(rb, path, [.008] * 8 + [.012], sides=8)
    girdle = kit.new_object('girdle', rb); girdle.data.materials.append(rope)
    for p in girdle.data.polygons: p.use_smooth = True
    rigid(girdle, lambda c: {'pelvis': 1.} if c.z > waist - .06 else {'pelvis': .6, 'thigh_l' if c.x * L > 0 else 'thigh_r': .4})
    kit.box_uv(girdle, 20.); parts.append(girdle)
    pb = bmesh.new()
    pc = ring[int(48 * .62)][0] + Vector((0, 0, -.07))
    bmesh.ops.create_cube(pb, size=1.)
    for v in pb.verts: v.co = pc + Vector((v.co.x * .1, v.co.y * .045, v.co.z * .12))
    pouch = kit.new_object('pouch', pb); pouch.data.materials.append(pbr_slot(slots, 'leather'))
    kit.activate(pouch); bv = pouch.modifiers.new('Round', 'BEVEL'); bv.width = .012; bv.segments = 3; bpy.ops.object.modifier_apply(modifier=bv.name)
    rigid(pouch, {'pelvis': .7, 'thigh_l' if pc.x * L > 0 else 'thigh_r': .3}); kit.box_uv(pouch, 40.); parts.append(pouch)
    # The hat: a wide drooping brim, a tall crown leaning back with its tip bent over, a band.
    top = info.top
    hz = info.brow_z + .02
    hc = Vector((0, F.head_mid_y + .005, hz))
    hb = bmesh.new()
    rings = []
    for j in range(7):   # brim, from the crown's foot out
        r = .105 + .16 * j / 6
        droop = -.035 * (j / 6) ** 1.6
        rings.append([(hc + Vector((math.cos(2 * math.pi * k / 32) * r, math.sin(2 * math.pi * k / 32) * r * .96, droop + .006 * math.sin(k * 1.7 + j))), j) for k in range(32)])
    for j in range(1, 16):   # the crown
        u = j / 15
        r = .105 * (1 - u) ** 1.15 + .006
        lean = Vector((0, .07 * u ** 1.5, 0)) + Vector((0, .1 * step(.6, 1., u) ** 2, -.11 * step(.7, 1., u) ** 1.5))
        rings.append([(hc + lean + Vector((math.cos(2 * math.pi * k / 32) * r * (1 + .05 * math.sin(k * 3 + j)), math.sin(2 * math.pi * k / 32) * r, .43 * u ** .92 - .07 * step(.75, 1., u))), 7 + j) for k in range(32)])
    order = list(reversed(rings[:7])) + rings[7:]
    vs = [[hb.verts.new(p) for p, _ in ring] for ring in order]
    for a, b in zip(vs, vs[1:]):
        for k in range(32): hb.faces.new((a[k], a[(k + 1) % 32], b[(k + 1) % 32], b[k]))
    hat = kit.new_object('hat', hb); hat.data.materials.append(velvet)
    thicken(hat, .005, offset=0.); subdivide(hat, 1)
    rigid(hat, {'head': 1.}); kit.box_uv(hat, kit.TILE_CM['velour_velvet']); parts.append(hat)
    bandb = bmesh.new()
    sweep(bandb, [(hc + Vector((math.cos(2 * math.pi * k / 32) * .108, math.sin(2 * math.pi * k / 32) * .108, .022)), Vector((math.cos(2 * math.pi * k / 32), math.sin(2 * math.pi * k / 32), 0))) for k in range(32)], .018, sides=6, flatten=.25)
    hband = kit.new_object('hat_band', bandb); hband.data.materials.append(gold); rigid(hband, {'head': 1.}); kit.box_uv(hband, 20.); parts.append(hband)
    # Hair under the hat: everything above the brim's foot goes (MakeHuman's long01 is on the head).
    # The staff in his right fist.
    rig = info.rig
    gc, across, palm, along = grip_frame(F, 'r')
    st, gem = make_staff(F, slots, gc, across, palm)
    add_bone(rig, 'weapon_r', 'hand_r', gc, gc + across * .2, roll_toward=palm)
    add_bone(rig, 'weapon_tip', 'weapon_r', gem, gem + across * .05, roll_toward=palm)
    for o in st: rigid(o, {'weapon_r': 1.}); parts.append(o)
    HERO['props'] = {'staff_grip': list(gc), 'gem': list(gem)}
    HERO['hat_z'] = hz
    return parts


# ------------------------------------------------------------------ the ranger
def ranger_gear(info, slots, hide):
    F = Frame(info); L = F.L
    HERO.update(rig=info.rig, frame=F, slots=slots)
    neck, waist, hips = info.neck, info.waist_z, info.hips
    T, A, FA = Body.TORSO, Body.ARMS, Body.FOREARMS
    parts = []
    wool, leather, dark = pbr_slot(slots, 'wool_green'), pbr_slot(slots, 'leather'), pbr_slot(slots, 'leather_dk')
    # The mantle: a short hooded cape, to the shoulder blades behind, the hood lying back on it.
    def mantle(c, b):
        if b not in T | A and b != Body.NECK: return -1.
        back = clamp((c.y - hips.y) / .05 + .5)
        bottom = (neck.z - .13) * (1 - back) + (neck.z - .26) * back
        m = min(c.z - bottom, neck.z + .035 - c.z)
        if b in A: m = min(m, abs(info.shoulder.x) + .05 - abs(c.x))
        if c.y < hips.y: m = min(m, abs(c.x) - .03 - max(0., neck.z - c.z) * .3)
        return m
    mo, loops = shell(F, mantle, .022, 'mantle', wool, smooth=6, clear=.018)
    thicken(mo, .005); kit.skin_from_body(mo, info); kit.box_uv(mo, kit.TILE_CM['poly_wool_herringbone']); parts.append(mo)
    # The hood, down: a soft fold of cloth lying behind the neck on the cape.
    hb = bmesh.new()
    base = Vector((0, max(c.y for i, c in enumerate(info.co) if info.bone[i] == Body.NECK) + .03, neck.z - .01))
    for j in range(9):
        pass
    path = [base + Vector((0, .012 * math.sin(k / 8 * math.pi), -.15 * k / 8)) for k in range(9)]
    rr = [.075 * (1 - .4 * (k / 8) ** 2) for k in range(9)]
    kit.tube(hb, path, rr, sides=14)
    hood = kit.new_object('hood', hb); hood.data.materials.append(wool)
    for v in hood.data.vertices:
        if v.co.y < base.y - .02: v.co.y = base.y - .02 + (v.co.y - base.y + .02) * .25   # flattened against her back
        v.co.x *= 1.35
    rigid(hood, {'spine_03': .7, 'neck_01': .3}); kit.box_uv(hood, kit.TILE_CM['poly_wool_herringbone']); parts.append(hood)
    # A baldric from her right shoulder to her left hip, the quiver on her back on it.
    def baldric(c, b):
        if b not in T | A: return -1.
        # a band along the line from (right shoulder top) to (left hip), front and back
        k = (-L) * c.x * .9 + (c.z - waist)   # the band's line: z rises toward her right
        return .024 - abs(k - .17)
    bo, _ = shell(F, baldric, .03, 'baldric', dark, smooth=6, clear=.026)
    thicken(bo, .004); rigid(bo, {'spine_03': .6, 'spine_02': .4}); parts.append(bo)
    qb = bmesh.new()
    back_y = max(c.y for i, c in enumerate(info.co) if info.bone[i] in T and abs(c.z - (waist + .2)) < .02)
    qtop = Vector((-L * .09, back_y + .06, neck.z + .03))
    qbot = Vector((L * .05, back_y + .05, waist - .12))
    kit.tube(qb, [qbot.lerp(qtop, k / 10) for k in range(11)], [.05 - .012 * (1 - k / 10) for k in range(11)], sides=14)
    quiver = kit.new_object('quiver', qb); quiver.data.materials.append(leather)
    for p in quiver.data.polygons: p.use_smooth = True
    rigid(quiver, {'spine_03': .8, 'spine_02': .2}); kit.box_uv(quiver, 40.); parts.append(quiver)
    ab = []
    axis = (qtop - qbot).normalized()
    rnd = random.Random(3)
    for k in range(9):
        off = Vector((rnd.uniform(-.026, .026), rnd.uniform(-.02, .02), 0))
        a_ = make_arrow(slots, qtop - axis * .55 + off, (axis + Vector((rnd.uniform(-.06, .06), rnd.uniform(-.06, .06), 0))).normalized(), Vector((1, 0, 0)), length=.66)
        for o in a_:
            if o.name.startswith('arrow_head'): bpy.data.objects.remove(o); continue
            rigid(o, {'spine_03': .8, 'spine_02': .2}); ab.append(o)
    parts += ab
    # A belt with a knife on her right hip.
    def belt(c, b):
        if b not in T | Body.LEGS: return -1.
        return .017 - abs(c.z - (waist - .07))
    bt, _ = shell(F, belt, .02, 'belt', dark, smooth=6, clear=.018)
    thicken(bt, .004); rigid(bt, {'pelvis': 1.}); parts.append(bt)
    kb = bmesh.new()
    kt = Vector((-L * .16, hips.y - .02, waist - .08)); kbm = kt + Vector((0, .04, -.24))
    kit.tube(kb, [kt.lerp(kbm, k / 6) for k in range(7)], [.018, .019, .018, .016, .013, .009, .004], sides=8)
    knife = kit.new_object('knife_sheath', kb); knife.data.materials.append(dark)
    rigid(knife, {'pelvis': .6, 'thigh_r' if L > 0 else 'thigh_l': .4}); kit.box_uv(knife, 40.); parts.append(knife)
    # Bracer on her bow arm (left), gloves.
    E, W = F.j('lowerarm', 'l'), F.j('hand', 'l')
    def bracer(c, b):
        if b not in FA or F.side_of(c) != 'l': return -1.
        u = (c - E).dot((W - E).normalized()) / (W - E).length
        return min(u - .3, .92 - u) * .25
    br, bloops = shell(F, bracer, .012, 'bracer', leather, smooth=4, clear=.01)
    thicken(br, .004); rigid(br, {'lowerarm_l': 1.}); parts.append(br)
    for s in 'lr':
        def glove(c, b, s=s):
            if not b or F.side_of(c) != s: return -1.
            if b == f'hand_{s}': return 1.
            if b.startswith(('thumb_', 'index_', 'middle_', 'ring_', 'pinky_')) and b.endswith(('_01_' + s, '_02_' + s)): return 1.
            return -1.
        g, _ = shell(F, glove, .0022, f'glove_{s}', pbr_slot(slots, 'suede'), smooth=0)
        thicken(g, .0018); parts.append(g)
    # The bow in her left fist, an arrow at her right fingers.
    rig = info.rig
    gc, across, palm, along = grip_frame(F, 'l')
    bow_axis = across                   # held upright when she draws: the clips turn the fist
    toward = palm                       # the string toward her palm side
    bowp, strg, ends, nock = make_bow(F, slots, gc, bow_axis, toward)
    add_bone(rig, 'weapon_l', 'hand_l', gc, gc + bow_axis * .2, roll_toward=toward)
    add_bone(rig, 'bowstring', 'weapon_l', nock, nock + toward * .05, roll_toward=bow_axis)
    for o in bowp: rigid(o, {'weapon_l': 1.}); parts.append(o)
    # the string: its ends on the bow, its middle on the bowstring bone, linearly between
    for g in list(strg.vertex_groups): strg.vertex_groups.remove(g)
    gb_, gs_ = strg.vertex_groups.new(name='weapon_l'), strg.vertex_groups.new(name='bowstring')
    for v in strg.data.vertices:
        c = strg.matrix_world @ v.co
        d0 = min((c - e).length for e in ends); d = (ends[0] - nock).length
        t = clamp(1 - d0 / d)
        gb_.add([v.index], 1 - t, 'REPLACE'); gs_.add([v.index], t, 'REPLACE')
    strg.modifiers.new('Armature', 'ARMATURE').object = rig
    parts.append(strg)
    rc, racross, rpalm, ralong = grip_frame(F, 'r')
    arrow_nock = rc + ralong * .02
    ar = make_arrow(slots, arrow_nock, racross, rpalm)
    add_bone(rig, 'arrow_r', 'hand_r', arrow_nock, arrow_nock + racross * .2, roll_toward=rpalm)
    for o in ar: rigid(o, {'arrow_r': 1.}); parts.append(o)
    HERO['props'] = {'bow_grip': list(gc), 'nock': list(nock), 'arrow_nock': list(arrow_nock)}
    return parts


GEAR = {'fighter': fighter_gear, 'wizard': wizard_gear, 'ranger': ranger_gear}


def finish(parts):
    """UVs for the hero slots (box projection at the material's real size unless already wrapped)."""
    for o in parts:
        if o.name in WRAPPED or o.data.uv_layers: continue
        slot = o.data.materials[0].name if o.data.materials else ''
        kind = slot[5:] if slot.startswith('Hero_') else None
        kit.box_uv(o, tile_cm(kind) if kind in PBR else 50.)
    for o in parts:
        if not o.data.uv_layers: kit.box_uv(o, 50.)


def build(name, spec):
    WRAPPED.clear(); HERO.clear()
    made = []
    def extra(info, slots, hide):
        parts = GEAR[spec['gear']](info, slots, hide)
        finish(parts)
        bake_colours(parts)
        made.extend(parts)
        return parts
    result = kit.build(f'Hero{name}', spec, extra=extra)
    if spec.get('hair_tint') and 'Hair' in result['slots']:
        result['slots']['Hair']['tint'] = spec['hair_tint']
        if spec.get('hair_grey'): result['slots']['Hair']['texture'] = grey_texture(result['slots']['Hair']['texture'], spec['hair_grey'])
    result['props'] = HERO.get('props', {})
    result['bones_extra'] = [b.name for b in bpy.data.objects['HumanRig'].data.bones if b.name in ('weapon_r', 'weapon_tip', 'shield_l', 'weapon_l', 'bowstring', 'arrow_r')]
    print('CHUCK_HERO', name, f"tris={result['tris']}", result['bones_extra'])
    if spec['gear'] == 'ranger': result['arrow_fbx'] = export_arrow()
    if REVIEW: review(name)
    return result


def grey_texture(rel, lift):
    """A greyed copy of a hair texture (the old wizard's: MakeHuman's hair is brown; a tint cannot make it
    grey): its luminance only, brightened toward `lift`, its alpha kept. Returns the new relative path."""
    import numpy as np
    src = OUT / rel
    img = bpy.data.images.load(str(src))
    a = np.array(img.pixels[:]).reshape(-1, 4)
    lum = a[:, :3] @ np.array([.3, .59, .11])
    lum = lum / max(1e-4, float(lum[a[:, 3] > .5].mean() if (a[:, 3] > .5).any() else lum.mean())) * lift
    a[:, 0] = a[:, 1] = a[:, 2] = np.clip(lum, 0, 1)
    img.pixels = a.ravel()
    dst = src.with_name(src.stem + '_grey.png')
    img.filepath_raw = str(dst); img.file_format = 'PNG'; img.save()
    bpy.data.images.remove(img)
    return dst.relative_to(OUT).as_posix()


def export_arrow():
    """The loosed arrow, on its own (Props/SM_HeroArrow.fbx): the nock at the origin, flying along +X
    (Unreal), its slots the ranger's (Hero_wood, Hero_steel_dark, Hero_feather)."""
    slots = {}
    parts = make_arrow(slots, Vector((0, 0, 0)), Vector((1, 0, 0)), Vector((0, 0, 1)), length=.76)   # Blender +X is Unreal +X (as the heroes face)
    for o in parts:
        kit.box_uv(o, tile_cm('wood'))
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts: o.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    arrow = bpy.context.view_layer.objects.active; arrow.name = 'SM_HeroArrow'
    out = OUT / 'Props'; out.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(out / 'SM_HeroArrow.fbx'), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE', colors_type='LINEAR')
    bpy.data.objects.remove(arrow)
    print('CHUCK_HERO_ARROW', out / 'SM_HeroArrow.fbx')
    return 'Props/SM_HeroArrow.fbx'


def review(name):
    import os
    s = bpy.context.scene
    s.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items] else 'BLENDER_EEVEE'
    for m in bpy.data.materials:
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get('Principled BSDF')
        if not bsdf: continue
        kind = m.name[5:] if m.name.startswith('Hero_') else None
        if kind in PBR:
            spec = PBR[kind]; t = spec['tint']
            bsdf.inputs['Base Color'].default_value = (min(1, t[0] * (.62 if spec['metal'] else 1.)), min(1, t[1] * (.62 if spec['metal'] else 1.)), min(1, t[2] * (.62 if spec['metal'] else 1.)), 1)
            bsdf.inputs['Metallic'].default_value = spec['metal']
            bsdf.inputs['Roughness'].default_value = .32 if spec['metal'] else .7
        elif m.name in ('Skin',):
            bsdf.inputs['Base Color'].default_value = (.62, .42, .33, 1)
        elif m.name in HERO.get('slots', {}) and 'tint' in HERO['slots'][m.name]:
            t = HERO['slots'][m.name]['tint']
            k = .45 if HERO['slots'][m.name].get('type') in ('card', 'hair') else 1.
            bsdf.inputs['Base Color'].default_value = (min(1, t[0] * k), min(1, t[1] * k), min(1, t[2] * k), 1)
            bsdf.inputs['Roughness'].default_value = .8
            if 'chainmail' == HERO['slots'][m.name].get('fabric'):
                bsdf.inputs['Metallic'].default_value = 1.; bsdf.inputs['Roughness'].default_value = .45
        else:
            bsdf.inputs['Base Color'].default_value = (.15, .15, .17, 1)
    world = s.world or bpy.data.worlds.new('World'); s.world = world; world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (.25, .25, .28, 1)
    sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN')); s.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(50), 0, math.radians(30)); sun.data.energy = 3.5
    h = HEROES[name]['height_cm'] / 100.
    for tag, loc, rot, lens in (('front', (3.4, -1.1, h * .62), (84, 0, 72), 50), ('back', (-3.4, 1.1, h * .62), (84, 0, 252), 50),
                                ('head', (.9, -.35, h * .93), (88, 0, 69), 50), ('three', (2.2, 2.2, h * .7), (82, 0, 135), 50)):
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam)
        cam.location = loc; cam.rotation_euler = tuple(math.radians(a) for a in rot); cam.data.lens = lens; s.camera = cam
        s.render.resolution_x, s.render.resolution_y = 900, 1100
        s.render.filepath = os.path.join(REVIEW, f'Hero{name}_{tag}.png'); bpy.ops.render.render(write_still=True)


if __name__ == '__main__':
    manifest_path = OUT / 'manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else {'heroes': {}}
    manifest['generator'] = 'Tools/build_temple_heroes.py'
    manifest['mpfb'] = '2.0.17'
    for name, spec in HEROES.items():
        if ONLY and name not in ONLY: continue
        manifest['heroes'][name] = build(name, spec)
    manifest_path.write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
    print('CHUCK_HEROES_READY', sorted(manifest['heroes']))
