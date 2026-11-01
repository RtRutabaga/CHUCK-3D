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
from mathutils.noise import noise as pnoise
import sys

V1 = Path(bpy.data.filepath).parent
sys.path.insert(0, str(V1.parents[2] / 'Tools'))
# Source heights below are v1.0 values mapped by the v1.1 shape amendment.
from chuck_v1_shape import Z, X  # noqa: E402


def smoothstep(x, lo, hi):
    t = min(1., max(0., (x - lo) / (hi - lo)))
    return t * t * (3 - 2 * t)
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
EYES = [Vector((X(7.1), s * 3.86, Z(55.0))) for s in (1, -1)]
# Ear shells (cupped_ear in build_chuck_model.py) stay nearly bare; strands on
# their furred backs would fringe past the rim.
EARS = [Vector((1.1, s * 5.2, Z(60.8))) for s in (1, -1)]  # ear on the outer top corner of the skull (build_chuck_model.py)

# Group definitions: strands, length range (cm), flow direction chooser.
def flow_for(p):
    if p.z < Z(19.):                     # legs: straight down
        return Vector((.1, 0., -1.))
    # Head and muzzle sleek toward the tail, the body down and back. The change used to happen at one
    # height (47.5 cm), a line of parted fur round the neck that drew the head as a separate ball
    # (2026-10-31); now it turns over the jowls and neck, the cheek fur sweeping back and down onto it.
    head = smoothstep(p.z, Z(46.), Z(51.))
    return Vector((-1., 0., -.25)) * head + Vector((-.45, 0., -1.)) * (1 - head)

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
    if p.z > Z(45.):
        # Cheeks down the neck into the body coat: length and lift change gradually over the neck (2026-10-31),
        # so the coat runs on from the head to the shoulders and chest with no step or ruff.
        f = smoothstep(p.z, Z(46.), Z(52.))
        return 1. - .15 * f, 1. - .2 * f
    if p.z < Z(11.):
        return .55, .6                   # shins: short, sleek (the goal's slim lower legs)
    if p.z < Z(19.):
        return 1., 1.1                   # thighs
    return 1., 1.

# Cream underside (2026-10-31, user: the light fur should continue "from neck to stomach in a natural way
# similar to coloration on actual rats and mice", not stop under the throat and restart as an oval on the
# belly). On the trunk and neck (faces tagged chuck_ventral by Tools/build_chuck_v1.py) a hair is cream
# when the surface faces less than VENTRAL_WIDTH off straight ahead: one band from the chin down the
# throat, broad over chest and belly, narrowing between the thighs. The edge is a soft, slightly wavy
# mix of cream and coat hairs about a centimetre wide, as on a real rat, not a cut line.
VENTRAL_TOP = Z(50.5)                    # above this the jowls/chin keep their material colours
_vent = mesh.attributes.get('chuck_ventral')
VENTRAL_FACE = [d.value for d in _vent.data] if _vent else [False] * len(mesh.polygons)
_VW = [(Z(z), w) for z, w in ((12.5, .5), (16, .75), (19, 1.), (24, 1.18), (30, 1.22), (36, 1.18),
                               (41, 1.1), (44, 1.02), (47, .98), (50.5, .98))]

def ventral_width(z):
    if z <= _VW[0][0]: return _VW[0][1]
    for (z0, w0), (z1, w1) in zip(_VW, _VW[1:]):
        if z <= z1: return w0 + (w1 - w0) * (z - z0) / (z1 - z0)
    return _VW[-1][1]

def facing(n, p=None):
    """Angle (rad) the surface faces off straight ahead in plan. Where it faces up or down (the top of the
    trunk round the neck, under the jaw) the direction from the body's axis stands in for it."""
    h = math.hypot(n.x, n.y)
    if h > .3: return math.atan2(abs(n.y), n.x)
    if p is None: return None
    return math.atan2(abs(p.y), p.x - .5)

_leg = mesh.attributes.get('chuck_leg')
LEG_FACE = [d.value for d in _leg.data] if _leg else [False] * len(mesh.polygons)

def ventral(tri, p):
    return (VENTRAL_FACE[tri.polygon_index] and p.z < VENTRAL_TOP) or LEG_FACE[tri.polygon_index]

def cream_share(p, n, tri=None):
    wobble = .07 * pnoise(p * .45) + .03 * pnoise(p * 1.7)
    if tri is not None and LEG_FACE[tri.polygon_index]:
        # The belly's cream carries on down the front-inner thigh and fades out above the knee, as on a
        # real rat, so it does not stop in a hard U where the thighs overlap the belly.
        inner = Vector((.45, -.89 if p.y > 0 else .89, 0.))
        h = Vector((n.x, n.y, 0.))
        if h.length < .3: return 0.
        a = h.normalized().angle(inner)
        w = (.95 + wobble) * smoothstep(p.z, Z(10.5), Z(17.5))
        return 1. - smoothstep(a, w - .14, w + .14) if w > .05 else 0.
    a = facing(n, p)
    w = ventral_width(p.z) + wobble
    return 1. - smoothstep(a, w - .11, w + .11)

def group_of(tri, p, n):
    mat = mat_names[tri.material_index]
    if ventral(tri, p):
        c = cream_share(p, n, tri)
        if c is not None:
            # Edge triangles go to the cream pool; each root there is then cream or coat by its share.
            return 'Fur_Cream' if c > .01 else coat_of(p, n)
    if mat == 'Chest':
        # A few coat-coloured hairs in the cream on the face only: the throat stays one cream band from the
        # chin down into the chest bib (2026-10-31).
        if p.z > Z(51.) and tri.index % 3 == 0:
            return 'Fur_Body'
        return 'Fur_Cream'
    if mat != 'Fur':
        return None
    return coat_of(p, n)

def coat_of(p, n):
    # Darker coat along the back, crown and upper snout.
    back = n.dot(Vector((-1., 0., .35)).normalized()) > .3 or (p.z > Z(55.5) and n.z > .35)
    return 'Fur_Back' if back else 'Fur_Body'

def covered(p, n, reach=2.2):
    """True if the surface is hidden under the jacket/sleeves (or inside
    another part): a short ray along the normal hits another surface."""
    hit, _, index, dist = tree.ray_cast(p + n * .03, n, reach)
    return hit is not None

def near_eye(p):
    # Clear of the eye: 2.3 cm ahead of it (head fur lies back, over it), 1.85 cm elsewhere, so the coat
    # meets the dark lid rim instead of leaving a ring of bare skin round it (2026-10-31).
    return any((p - e).length < (2.3 if p.x > e.x + .3 else 1.85) for e in EYES) or any(
        (p - e).length < 4.8 and abs(p.y) > 3.3 and p.z > Z(57.5) for e in EARS)

# Area-weighted candidate triangles per group.
cands = {g: [] for g in GROUPS}
for tri in mesh.loop_triangles:
    c = tri.center; n = tri.normal
    g = group_of(tri, c, n)
    if g and not near_eye(c):
        cands[g].append(tri)

def weight_of(t, g=None):
    # Denser on the head (seen close up, and the skin shows between strands). The cream on the trunk
    # (the underside below the neck) at three quarters of the face cream's density: still about 1.5x
    # the coat's, without adding strands for the larger cream area (2026-10-31).
    # The head's extra density rises gradually over the neck: a step at one height showed as a line across
    # the throat where the denser head cream met the thinner chest cream.
    body = .75 if g == 'Fur_Cream' and ventral(t, t.center) else 1.
    return t.area * (body + (3.2 - body) * smoothstep(t.center.z, Z(45.5), Z(49.5)))

# The cream pool now covers the whole underside: every group's strand count follows its new area at the
# density it had before the mask (cream twice the coat's: short hair, the skin must not show).
_area = {g: sum(weight_of(t, g) for t in cands[g]) for g in GROUPS}
# Counts were set when every Fur triangle was coat and every Chest triangle cream: keep those densities.
_old = {g: 0. for g in GROUPS}
for t in mesh.loop_triangles:
    if mat_names[t.material_index] == 'Fur' and not near_eye(t.center):
        _old[coat_of(t.center, t.normal)] += weight_of(t)
    elif mat_names[t.material_index] == 'Chest' and not near_eye(t.center):
        _old['Fur_Cream'] += weight_of(t)
for g in GROUPS:
    GROUPS[g]['count'] = round(GROUPS[g]['count'] * _area[g] / _old[g])
print('CHUCK_GROOM_AREAS', {g: round(a, 1) for g, a in _area.items()}, {g: GROUPS[g]['count'] for g in GROUPS})

strands = {g: [] for g in GROUPS}
rejected = 0
for g, spec in GROUPS.items():
    tris = cands[g]
    if not tris: continue
    weights = [weight_of(t, g) for t in tris]
    tries = made = 0
    while made < spec['count'] and tries < spec['count'] * 4:
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
        # The top of the trunk round the front of the neck faces up at the chin 2-3 cm above; with the full
        # reach it counted as covered and showed as a bare band across the throat (2026-10-31).
        reach = .7 if g == 'Fur_Cream' and n.z > .45 and ventral(t, root) else 2.2
        if root.z < Z(4.8) or near_eye(root) or covered(root, n, reach) or (pad and rng.random() < .7):
            rejected += 1; continue
        target = g
        if g == 'Fur_Cream' and ventral(t, root):
            c = cream_share(root, n, t)
            if c is not None and rng.random() > c:
                target = coat_of(root, n)   # a coat hair in the soft edge of the cream
        made += 1
        flow = flow_for(root)
        flow = flow - n * flow.dot(n)
        if flow.length < 1e-4:
            flow = n.orthogonal()
        flow.normalize()
        jitter = Vector((rng.gauss(0, .25), rng.gauss(0, .25), rng.gauss(0, .15)))
        jitter -= n * jitter.dot(n)
        flow = (flow + jitter).normalized()
        scale, lift_scale = region_scale(root)
        length = rng.uniform(*GROUPS[target]['length']) * scale
        lift = rng.uniform(.45, .95) * lift_scale  # scruffy: about 25-55 degrees off the skin
        head = root.z > Z(47.5)
        guard = target != 'Fur_Cream' and root.z > Z(11.) and rng.random() < (GUARD * .35 if head else GUARD)
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
        strands[target].append(pts)

for g in GROUPS:
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
