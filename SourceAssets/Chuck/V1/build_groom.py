"""Blender 4.5 LTS: strand groom for Chuck v1 (Unreal Groom / Alembic).

blender --background SourceAssets/Chuck/V1/Chuck_V1.blend --python SourceAssets/Chuck/V1/build_groom.py

Grows shaggy, clumped rat fur (References/ArtDirection/Chuck-Turnaround.png:
longer coat, spiky crown, short muzzle) on the exposed fur/cream surfaces of
SK_Chuck_Groomed (the v1 mesh without geometric tufts) in its rest pose and
writes SourceAssets/Chuck/V1/Groom/GR_Chuck.abc plus groom_metadata.json.
Surfaces covered by the jacket/sleeves and the eyelids get no strands.

Blender's Alembic exporter keeps only strand positions and widths (custom
attributes such as groom_color are dropped), so colour travels as groom
groups: one Alembic curves object per group, each taking its own hair
material in Unreal. Never saves the .blend.
"""
import json
import math
import random
from pathlib import Path
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree
import sys

V1 = Path(bpy.data.filepath).parent
sys.path.insert(0, str(V1.parents[2] / 'Tools'))
# Source heights below are v1.0 values mapped by the v1.1 shape amendment.
from chuck_v1_shape import Z, X  # noqa: E402
OUT = V1 / 'Groom'; OUT.mkdir(exist_ok=True)
scene = bpy.context.scene
rng = random.Random(4217)
mesh_obj = bpy.data.objects['SK_Chuck_Groomed']
# Grow on the rest pose, which is what Unreal binds the groom against.
bpy.data.objects['SK_Chuck_Rig'].data.pose_position = 'REST'
bpy.context.view_layer.update()
mesh_obj.hide_set(False)
deps = bpy.context.evaluated_depsgraph_get()
ev = mesh_obj.evaluated_get(deps)
mesh = ev.to_mesh()
mesh.calc_loop_triangles()
mat_names = [m.name.split('.')[0] for m in mesh.materials]
tree = BVHTree.FromObject(mesh_obj, deps)
poly_mat = [p.material_index for p in mesh.polygons]
EYES = [Vector((X(7.15), s * 3.8, Z(55.0))) for s in (1, -1)]
# Ear shells (cupped_ear in build_chuck_model.py) stay nearly bare; strands on
# their furred backs would fringe past the rim.
EARS = [Vector((1.1, s * 4.4, Z(61.3))) for s in (1, -1)]  # ear seated on the skull top-side (build_chuck_model.py)

# Group definitions: strands, length range (cm), flow direction chooser.
def flow_for(p):
    if p.z > Z(47.5):                    # head and muzzle: sleek toward the tail
        return Vector((-1., 0., -.25))
    if p.z < Z(19.):                     # legs: straight down
        return Vector((.1, 0., -1.))
    return Vector((-.45, 0., -1.))       # body: down and back

GROUPS = {
    'Fur_Body':  {'count': 60000, 'length': (.9, 1.6)},
    'Fur_Back':  {'count': 32000, 'length': (1.0, 1.8)},
    'Fur_Cream': {'count': 44000, 'length': (.6, 1.15)},
}
ROOT_WIDTH, TIP_WIDTH = .0055, .001  # cm: about 55 um at the root
POINTS = 6
CLUMP_EVERY = 10      # one clump guide per this many strands
CLUMP_PULL = .75      # how far tips converge on their guide (0..1)
GUARD = .08           # share of long, lifted guard hairs (city-rat scruff)

def region_scale(p):
    """Length and lift multipliers. The head coat is short and lies back along
    the skull (user feedback 2026-09-27: a lifted crown read as a human
    haircut); the scruffy city-rat body stays as it is."""
    if p.z > Z(47.5) and p.x > X(7.5):
        return .45, .6                   # muzzle and snout
    if p.z > Z(55.):
        return .85, .85                  # crown: textured, a little tufty, no tall crest
    if p.z > Z(47.5):
        return .8, .8                    # cheeks and nape
    if p.z < Z(19.):
        return 1., 1.1                   # thighs and shins
    return 1., 1.

def group_of(tri, p, n):
    mat = mat_names[tri.material_index]
    if mat == 'Chest':
        if p.z > Z(47.5) and tri.index % 3 == 0:
            return 'Fur_Body'
        return 'Fur_Cream'
    if mat != 'Fur':
        return None
    # Darker coat along the back, crown and upper snout.
    back = n.dot(Vector((-1., 0., .35)).normalized()) > .3 or (p.z > Z(55.5) and n.z > .35)
    return 'Fur_Back' if back else 'Fur_Body'

def covered(p, n):
    """True if the surface is hidden under the jacket/sleeves (or inside
    another part): a short ray along the normal hits another surface."""
    hit, _, index, dist = tree.ray_cast(p + n * .03, n, 2.2)
    return hit is not None

def near_eye(p):
    return any((p - e).length < 2.3 for e in EYES) or any(
        (p - e).length < 4.8 and abs(p.y) > 2.9 and p.z > Z(58.) for e in EARS)

# Area-weighted candidate triangles per group.
cands = {g: [] for g in GROUPS}
for tri in mesh.loop_triangles:
    c = tri.center; n = tri.normal
    g = group_of(tri, c, n)
    if g and not near_eye(c):
        cands[g].append(tri)

strands = {g: [] for g in GROUPS}
rejected = 0
for g, spec in GROUPS.items():
    tris = cands[g]
    if not tris: continue
    # Denser on the head (seen close up, and the skin shows between strands).
    weights = [t.area * (3.2 if t.center.z > Z(47.5) else 1.) for t in tris]
    tries = 0
    while len(strands[g]) < spec['count'] and tries < spec['count'] * 4:
        tries += 1
        t = rng.choices(tris, weights=weights)[0]
        a, b, c = (mesh.vertices[i].co for i in t.vertices)
        u, v = rng.random(), rng.random()
        if u + v > 1: u, v = 1 - u, 1 - v
        root = a * (1 - u - v) + b * u + c * v
        n = t.normal.normalized()
        # Fur stops at the ankle: the paws stay bare pink skin (strands rooted
        # lower hung over the paw tops in the Unreal review).
        # Whisker pads keep sparse fur so their follicle dots show (target image).
        pad = root.x > X(11.8) and Z(49.8) < root.z < Z(51.8) and abs(root.y) > .8
        if root.z < Z(4.8) or near_eye(root) or covered(root, n) or (pad and rng.random() < .7):
            rejected += 1; continue
        flow = flow_for(root)
        flow = flow - n * flow.dot(n)
        if flow.length < 1e-4:
            flow = n.orthogonal()
        flow.normalize()
        jitter = Vector((rng.gauss(0, .25), rng.gauss(0, .25), rng.gauss(0, .15)))
        jitter -= n * jitter.dot(n)
        flow = (flow + jitter).normalized()
        scale, lift_scale = region_scale(root)
        length = rng.uniform(*spec['length']) * scale
        lift = rng.uniform(.45, .95) * lift_scale  # scruffy: about 25-55 degrees off the skin
        head = root.z > Z(47.5)
        guard = g != 'Fur_Cream' and rng.random() < (GUARD * .35 if head else GUARD)
        if guard:
            length *= rng.uniform(1.2, 1.5) if head else rng.uniform(1.6, 2.3); lift *= 1.4
        pts = [root - n * .02]             # root slightly below the skin
        pos = root.copy()
        for k in range(1, POINTS):
            f = k / (POINTS - 1)
            # The strand leaves the skin at `lift`, then lies down along the
            # flow, with a little frizz so the coat is not combed flat.
            d = (n * lift * (1 - f * .8) + flow).normalized()
            spread = .16 if head else .22  # textured head, scruffy body
            frizz = Vector((rng.gauss(0, spread), rng.gauss(0, spread), rng.gauss(0, spread))) * f
            pos = pos + (d + frizz) * (length / (POINTS - 1))
            pts.append(pos.copy())
        strands[g].append(pts)

    # Clumping: every strand's tip converges on the matching point of a nearby
    # guide strand, so the coat reads as wet-looking locks, not a velvet.
    curves = strands[g]
    guides = curves[::CLUMP_EVERY]
    kd = KDTree(len(guides))
    for i, gpts in enumerate(guides):
        kd.insert(gpts[0], i)
    kd.balance()
    for pts in curves:
        _, gi, dist = kd.find(pts[0])
        gpts = guides[gi]
        if dist > 1.2 or gpts is pts:
            continue
        for k in range(1, POINTS):
            f = (k / (POINTS - 1)) ** 1.4 * CLUMP_PULL
            target = pts[0] + (gpts[k] - gpts[0])
            target = target.lerp(gpts[k], .5)
            pts[k] = pts[k].lerp(target, f)

ev.to_mesh_clear()

# Build one hair-curves object per group and export them together.
objs = []
for g, curves in strands.items():
    data = bpy.data.hair_curves.new(g)
    data.add_curves([POINTS] * len(curves))
    flat = [p for pts in curves for p in pts]
    data.points.foreach_set('position', [c for p in flat for c in p])
    radii = [(ROOT_WIDTH * (1 - k / (POINTS - 1)) + TIP_WIDTH * k / (POINTS - 1)) / 2
             for _ in curves for k in range(POINTS)]
    data.points.foreach_set('radius', radii)
    obj = bpy.data.objects.new(g, data)
    scene.collection.objects.link(obj)
    objs.append(obj)

for o in scene.objects: o.select_set(False)
for o in objs: o.select_set(True)
bpy.context.view_layer.objects.active = objs[0]
bpy.ops.wm.alembic_export(filepath=str(OUT / 'GR_Chuck.abc'), selected=True, start=0, end=0,
                          export_hair=True, export_particles=False, uvs=False, normals=False,
                          face_sets=False, global_scale=1.)

lengths = {g: [(pts[-1] - pts[0]).length for pts in c] for g, c in strands.items()}
meta = {
    'file': 'Groom/GR_Chuck.abc', 'source_mesh': 'SK_Chuck_Groomed (rest pose)',
    'skeleton': '/Game/Characters/Chuck/V1/SK_Chuck', 'points_per_strand': POINTS,
    'groups': {g: {'strands': len(c), 'length_cm': [round(min(lengths[g]), 3), round(max(lengths[g]), 3)],
                   'mean_length_cm': round(sum(lengths[g]) / len(lengths[g]), 3)} for g, c in strands.items()},
    'total_strands': sum(len(c) for c in strands.values()),
    'width_cm': {'root': ROOT_WIDTH, 'tip': TIP_WIDTH},
    'rejected_roots_covered_eye_or_ear': rejected,
    # Warm taupe-brown coat, darker back/crown and beige-cream belly after the
    # 2026-09-27 turnaround, calibrated on the Unreal review render against the
    # turnaround's leg fur (sRGB about 89, 71, 62).
    'suggested_hair_colours_linear': {'Fur_Body': [.26, .15, .08], 'Fur_Back': [.18, .11, .062],
                                      'Fur_Cream': [.44, .32, .21]},
    'clumping': {'guide_every': CLUMP_EVERY, 'tip_pull': CLUMP_PULL, 'guard_hair_share': GUARD},
    'coordinates': 'Blender source cm, Z-up; the Alembic exporter writes Y-up (x, z, -y). '
                   'Set the groom import conversion so the result matches SK_Chuck (verify on import).',
    'alembic_limits': 'Only positions and widths are exported; colour is per group (object), not per strand.',
}
(OUT / 'groom_metadata.json').write_text(json.dumps(meta, indent=1) + '\n', encoding='utf-8')
print('CHUCK_GROOM_READY', meta['total_strands'], {g: v['strands'] for g, v in meta['groups'].items()},
      'rejected', rejected)
