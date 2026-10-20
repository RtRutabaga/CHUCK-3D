"""Blender 4.5.14: Chult's flying jungle birds (Claude, 2026-10-19; docs/CHULT-BIRDS.md).
User: "add some flying jungle birds to chult, in particular i want some birds to
fly past player while crossing bridge and i want birds flying above temple in a
cinematic wonderous way".

blender --background --python Tools/build_chult_birds.py [-- --review <dir>]
  -> SourceAssets/Props/ChultBirds/SM_<Kind>_Body.fbx       head, beak, eyes, body and tail
     SM_<Kind>_WingInL/R.fbx   the inner wing (shoulder to wrist), origin at the shoulder
     SM_<Kind>_WingOutL/R.fbx  the hand (wrist to the splayed primaries), origin at the wrist
  for Kind = Scarlet (scarlet macaw) and BlueGold (blue-and-gold macaw).

Modelled here as lofts (no downloads). Centimetres, facing +X, +Z up, the
left wing along +Y; the body's origin is between the shoulders. A macaw:
about 86 cm beak to tail tip, 1.1 m across the wings. The plumage is vertex
colour only (M_ChultBird in Unreal multiplies it); every part shares one slot,
Feather. The wings are closed airfoils, so they need no two-sided material.
Unreal flaps the wings by rolling each part about its origin (ChultBirds.cpp).
"""
from pathlib import Path
import math
import sys
import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets' / 'Props' / 'ChultBirds'
ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = ARGS[ARGS.index('--review') + 1] if '--review' in ARGS else None

bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = .01
FEATHER = bpy.data.materials.new('Feather')

# Wing joints, shared with ChultBirds.cpp (Shoulder, Wrist).
SHOULDER = Vector((3.0, 4.5, 2.5))
INNER_SPAN, OUTER_SPAN = 21.0, 30.0


def srgb(r, g, b): return (r, g, b)


PAL = {
    'Scarlet': dict(back=srgb(.78, .07, .05), belly=srgb(.70, .06, .05), rump=srgb(.20, .45, .85), face=srgb(.94, .90, .86),
                    crown=srgb(.85, .12, .06), throat=srgb(.70, .07, .05), upper_beak=srgb(.90, .86, .76), lower_beak=srgb(.09, .08, .08),
                    tail_root=srgb(.78, .08, .05), tail_tip=srgb(.12, .30, .80),
                    cov=srgb(.80, .08, .05), band=srgb(.98, .78, .10), band_tip=srgb(.35, .60, .18), flight=srgb(.10, .28, .78),
                    under_cov=srgb(.62, .07, .05), under_flight=srgb(.55, .18, .10), eye=srgb(.95, .92, .70)),
    'BlueGold': dict(back=srgb(.10, .42, .80), belly=srgb(.98, .68, .08), rump=srgb(.25, .62, .92), face=srgb(.94, .92, .88),
                     crown=srgb(.32, .66, .22), throat=srgb(.06, .06, .07), upper_beak=srgb(.08, .07, .07), lower_beak=srgb(.08, .07, .07),
                     tail_root=srgb(.12, .45, .82), tail_tip=srgb(.06, .25, .62),
                     cov=srgb(.12, .46, .82), band=srgb(.10, .40, .80), band_tip=srgb(.14, .52, .78), flight=srgb(.06, .24, .66),
                     under_cov=srgb(.92, .66, .12), under_flight=srgb(.55, .48, .20), eye=srgb(.95, .93, .80)),
}


class Builder:
    """Collects vertices with colours and faces, then makes one smooth mesh."""
    def __init__(self): self.v, self.c, self.f = [], [], []

    def vert(self, p, col): self.v.append(Vector(p)); self.c.append(col); return len(self.v) - 1

    def face(self, *idx): self.f.append(idx)

    def mesh(self, name, mirror=False):
        me = bpy.data.meshes.new(name); bm = bmesh.new()
        verts = [bm.verts.new((p.x, -p.y if mirror else p.y, p.z)) for p in self.v]
        bm.verts.index_update()
        layer = bm.loops.layers.color.new('Col'); uv = bm.loops.layers.uv.new('UV')
        for idx in self.f:
            vs = [verts[i] for i in (reversed(idx) if mirror else idx)]
            try: f = bm.faces.new(vs)
            except ValueError: continue
            f.smooth = True
            for loop in f.loops:
                col = self.c[loop.vert.index]; loop[layer] = (col[0], col[1], col[2], 1.0)
                loop[uv].uv = (loop.vert.co.x / 100, loop.vert.co.y / 100)
        bm.normal_update(); bm.to_mesh(me); bm.free()
        me.color_attributes.active_color_name = 'Col'; me.color_attributes.render_color_index = me.color_attributes.active_color_index
        obj = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(obj)
        obj.data.materials.append(FEATHER)
        return obj


def ring_loft(b, stations, colour, segs=14, cap_front=True, cap_back=True):
    """stations: (x, cy, cz, ry, rz). colour(x, angle, station index) -> rgb (angle 0 = top)."""
    rings = []
    for si, (x, cy, cz, ry, rz) in enumerate(stations):
        ring = []
        for k in range(segs):
            a = 2 * math.pi * k / segs
            p = (x, cy + ry * math.sin(a), cz + rz * math.cos(a))
            ring.append(b.vert(p, colour(x, a, si)))
        rings.append(ring)
    for r0, r1 in zip(rings, rings[1:]):
        for k in range(segs):
            k1 = (k + 1) % segs
            b.face(r0[k], r1[k], r1[k1], r0[k1])
    if cap_back:
        x, cy, cz, *_ = stations[0]; c = b.vert((x - .4, cy, cz), colour(x, math.pi / 2, 0))
        for k in range(segs): b.face(rings[0][(k + 1) % segs], c, rings[0][k])
    if cap_front:
        x, cy, cz, *_ = stations[-1]; c = b.vert((x + .4, cy, cz), colour(x, math.pi / 2, len(stations) - 1))
        for k in range(segs): b.face(rings[-1][k], c, rings[-1][(k + 1) % segs])


def lerp(a, b, t): return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def smooth(t): t = min(max(t, 0), 1); return t * t * (3 - 2 * t)


def body(kind):
    P = PAL[kind]; b = Builder()
    # Body and head: vent at the back, the head's face just behind the beak.
    stations = [(-14, 0, .5, 3.0, 3.0), (-11, 0, .6, 5.0, 4.8), (-6, 0, .4, 6.3, 6.6), (0, 0, .4, 6.8, 7.2), (5, 0, .9, 6.2, 6.6),
                (9, 0, 1.8, 4.9, 5.2), (12, 0, 2.9, 3.9, 4.3), (15, 0, 3.8, 3.9, 4.5), (18.5, 0, 4.3, 4.2, 4.6), (21.5, 0, 4.2, 3.7, 4.1),
                (23.6, 0, 3.9, 2.6, 3.1), (24.6, 0, 3.7, 1.6, 2.2)]

    def colour(x, a, si):
        top = math.cos(a)  # 1 on the back, -1 on the belly
        side = abs(math.sin(a))
        if x > 19 and top > -.35 and side > .55: return P['face']           # the bare white face patch
        if x > 15 and top > .3: return P['crown']
        if 9 < x < 16 and top < -.2: return P['throat']
        if x < -7 and top > .2: return lerp(P['back'], P['rump'], smooth((-7 - x) / 5))
        return lerp(P['belly'], P['back'], smooth((top + .3) / .9))
    ring_loft(b, stations, colour, segs=16, cap_front=True)
    # Upper beak: a deep hooked bill curving down over the lower.
    beak = []
    for i in range(9):
        t = i / 8
        x = 23.8 + 6.0 * math.sin(t * math.pi * .45)
        cz = 3.9 + 1.2 * math.sin(t * math.pi * .5) - 5.0 * t ** 2.2
        r = 2.2 * (1 - t) ** .8 + .15
        beak.append((x, 0, cz, r * .9, r * 1.05))
    ring_loft(b, beak, lambda x, a, si: P['upper_beak'], segs=10, cap_back=False)
    # Lower mandible: a short blunt scoop under the hook.
    lower = [(23.4, 0, 1.6, 1.9, 1.3), (25.3, 0, 1.0, 1.6, 1.1), (27.0, 0, .9, 1.0, .8), (27.8, 0, 1.1, .3, .3)]
    ring_loft(b, lower, lambda x, a, si: P['lower_beak'], segs=10)
    # Eyes: a pale iris round a dark pupil, either side of the face.
    for s in (1, -1):
        # A small dome across the face (along Y): a pale iris ring round a dark pupil.
        rings = []
        for depth, r, col in ((0, .95, P['eye']), (.35, .9, P['eye']), (.6, .55, srgb(.03, .03, .03))):
            rings.append([b.vert((19.9 + r * math.sin(2 * math.pi * k / 10), s * (3.2 + depth), 5.0 + r * math.cos(2 * math.pi * k / 10)), col) for k in range(10)])
        tip = b.vert((19.9, s * 4.0, 5.0), srgb(.02, .02, .02))
        for r0, r1 in zip(rings, rings[1:]):
            for k in range(10):
                q = (r0[k], r0[(k + 1) % 10], r1[(k + 1) % 10], r1[k])
                b.face(*(q if s > 0 else tuple(reversed(q))))
        for k in range(10):
            t = (rings[-1][k], rings[-1][(k + 1) % 10], tip)
            b.face(*(t if s > 0 else tuple(reversed(t))))
    # Tail: two long pointed central feathers over a fan of shorter ones.
    def tail(z, length, width, spread, colour_root, colour_tip):
        start = b.vert((-12, 0, z), colour_root)
        rows = []
        n = 10
        for i in range(1, n + 1):
            t = i / n; x = -12 - length * t
            w = width * (1 - t ** 1.6) * (1 + .4 * math.sin(t * math.pi)) + .3
            th = .7 * (1 - t) + .25
            y0 = spread * t
            col = lerp(colour_root, colour_tip, smooth((t - .2) / .7))
            rows.append([b.vert((x, y0 - w, z - .1), col), b.vert((x, y0, z + th), col), b.vert((x, y0 + w, z - .1), col), b.vert((x, y0, z - th), col)])
        tip = b.vert((-12 - length - 2.5, spread * 1.04, z), colour_tip)
        for k in range(4): b.face(start, rows[0][(k + 1) % 4], rows[0][k])
        for r0, r1 in zip(rows, rows[1:]):
            for k in range(4): b.face(r0[k], r1[k], r1[(k + 1) % 4], r0[(k + 1) % 4])
        for k in range(4): b.face(rows[-1][k], tip, rows[-1][(k + 1) % 4])
    tail(1.2, 46, 2.6, -.8, P['tail_root'], P['tail_tip'])
    tail(1.4, 44, 2.4, .8, P['tail_root'], P['tail_tip'])
    for s in (1, -1):
        tail(.6, 37, 2.4, s * 1.7, P['tail_root'], P['tail_tip'])
        tail(.1, 28, 2.5, s * 2.6, P['tail_root'], lerp(P['tail_root'], P['tail_tip'], .7))
    return b


def airfoil(b, span_pts, chord_fn, colour, n_c=7, root_closed=True, tip_closed=True):
    """span_pts: spanwise stations (y). chord_fn(y) -> (x_lead, x_trail, z_camber, thickness).
    colour(u_span, c_chord, top) -> rgb. Top and bottom share the leading and trailing edges."""
    rows = []
    for j, y in enumerate(span_pts):
        lead, trail, camber, thick = chord_fn(y)
        u = j / (len(span_pts) - 1)
        row_top, row_bot = [], []
        for i in range(n_c + 1):
            c = i / n_c; x = lead + (trail - lead) * c
            hump = math.sin(math.pi * min(1, c * 1.15)) if c < .87 else 0
            th = thick * (1 - c) ** .9 * (.35 + .65 * hump) * .5 + .08
            zc = camber * math.sin(math.pi * c)
            if i == 0 or i == n_c:
                v = b.vert((x, y, zc), colour(u, c, True)); row_top.append(v); row_bot.append(v)
            else:
                row_top.append(b.vert((x, y, zc + th), colour(u, c, True)))
                row_bot.append(b.vert((x, y, zc - th * .55), colour(u, c, False)))
        rows.append((row_top, row_bot))
    for (t0, b0), (t1, b1) in zip(rows, rows[1:]):
        for i in range(n_c):
            b.face(t0[i], t0[i + 1], t1[i + 1], t1[i])
            b.face(b0[i], b1[i], b1[i + 1], b0[i + 1])
    for (top, bot), flip in ((rows[0], False), (rows[-1], True)):
        if (flip and not tip_closed) or (not flip and not root_closed): continue
        for i in range(1, n_c - 1):
            q = (top[i], top[i + 1], bot[i + 1], bot[i])
            b.face(*(q if flip else tuple(reversed(q))))


def wing_inner(kind):
    P = PAL[kind]; b = Builder()
    # Shoulder to wrist: a broad arm wing, its trailing edge the secondaries.
    ys = [0, 2, 5, 9, 13, 17, 21]
    def chord(y):
        u = y / INNER_SPAN
        serr = 1.2 * abs(math.sin(y * math.pi / 3.4))   # secondary feather tips
        root = 3.0 * (1 - smooth(y / 5))                 # the trailing edge rounds into the flank
        return (5.5 - .8 * u, -17 + 1.5 * u - serr + root, .9, 2.6 - .8 * u)
    def colour(u, c, top):
        if not top: return P['under_cov'] if c < .45 else P['under_flight']
        if c < .3: return P['cov']
        if c < .5: return lerp(P['band'], P['band_tip'], smooth((c - .38) / .12))
        return P['flight']
    airfoil(b, ys, chord, colour, n_c=8, root_closed=True)
    return b


def wing_outer(kind):
    P = PAL[kind]; b = Builder()
    # Wrist to hand: a short swept hand panel, then six overlapping primaries
    # lengthening outward to the macaw's long pointed tip, slotted only at the end.
    ys = [0, 3, 6, 9, 11]
    def chord(y):
        u = y / 11
        return (5.0 - 3.0 * u, -14 + 5.0 * u, .7, 2.0 - .8 * u)
    def colour(u, c, top):
        if not top: return P['under_flight'] if c > .3 else P['under_cov']
        return P['cov'] if (c < .25 and u < .7) else P['flight']
    airfoil(b, ys, chord, colour, n_c=7)
    for k in range(6):
        t = k / 5
        root = Vector((-9 + 10.5 * t, 3 + 7.5 * t, .1))
        ang = math.radians(-42 + 36 * t)
        length = 13 + 10 * t ** 1.2
        d = Vector((math.sin(ang), math.cos(ang), 0))
        start = len(b.v)
        def fchord(y, w0=6.2 - 1.4 * t):
            w = w0 * (1 - y ** 2.4) + .35
            return (w * .5, -w * .5, .25 - .2 * y, .8 * (1 - y) + .2)
        def fcol(u, c, top, t=t):
            if not top: return P['under_flight']
            return lerp(P['flight'], srgb(.03, .08, .25), .2 * u + .15 * t)
        tmp = Builder(); airfoil(tmp, [0, .2, .4, .6, .8, 1.0], fchord, fcol, n_c=4)
        # Map the unit blade (x across, y along 0..1) onto its line from the hand.
        across = Vector((d.y, -d.x, 0))
        for p, col in zip(tmp.v, tmp.c):
            at = root + d * (p.y * length) + across * p.x
            b.vert((at.x, at.y, at.z + p.z + .18 * (5 - k)), col)
        for f in tmp.f: b.face(*(i + start for i in f))
    return b


def export(obj, name):
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj
    obj.name = name
    bpy.ops.export_scene.fbx(filepath=str(OUT / f'{name}.fbx'), use_selection=True, object_types={'MESH'},
                             apply_unit_scale=True, axis_forward='-Y', axis_up='Z', bake_anim=False,
                             mesh_smooth_type='FACE', colors_type='SRGB')


OUT.mkdir(parents=True, exist_ok=True)
made = {}
for kind in PAL:
    bo = body(kind).mesh(f'SM_{kind}_Body'); export(bo, f'SM_{kind}_Body')
    wi = wing_inner(kind); wo = wing_outer(kind)
    parts = {'WingInL': wi.mesh(f'SM_{kind}_WingInL'), 'WingInR': wi.mesh(f'SM_{kind}_WingInR', mirror=True),
             'WingOutL': wo.mesh(f'SM_{kind}_WingOutL'), 'WingOutR': wo.mesh(f'SM_{kind}_WingOutR', mirror=True)}
    for part, obj in parts.items(): export(obj, f'SM_{kind}_{part}')
    made[kind] = (bo, parts)
    print(f'CHUCK_BIRD_BUILT kind={kind} body_tris={sum(len(p.vertices) - 2 for p in bo.data.polygons)} '
          f'wing_in={len(wi.f)} wing_out={len(wo.f)}')

if REVIEW:
    # Assemble each bird with its wings raised mid-stroke and render top and side stills.
    review = Path(REVIEW); review.mkdir(parents=True, exist_ok=True)
    for n, (kind, (bo, parts)) in enumerate(made.items()):
        off = Vector((0, n * 140, 0)); bo.location = off
        for side, s in (('L', 1), ('R', -1)):
            inner, outer = parts['WingIn' + side], parts['WingOut' + side]
            inner.location = off + Vector((SHOULDER.x, s * SHOULDER.y, SHOULDER.z)); inner.rotation_euler = (s * math.radians(-20), 0, 0)
            outer.parent = inner; outer.location = (0, s * INNER_SPAN, 0); outer.rotation_euler = (s * math.radians(-12), 0, math.radians(s * 4))
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.color_type = 'VERTEX'; scene.display.shading.light = 'STUDIO'
    scene.render.resolution_x, scene.render.resolution_y = 1400, 900
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); scene.collection.objects.link(cam); scene.camera = cam
    cam.data.lens = 50
    for name, loc in (('top', (10, 70, 330)), ('side', (40, -260, 40)), ('front', (300, 70, 60)), ('three_quarter', (190, -170, 120))):
        cam.location = loc
        direction = Vector((0, 70, 0)) - Vector(loc); cam.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(review / f'birds_{name}.png'); bpy.ops.render.render(write_still=True)
print('CHUCK_BIRDS_DONE ' + ' '.join(made))
