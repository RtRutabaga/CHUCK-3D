"""The jungle set for the fall-to-Chult cutscene (Claude, 2026-10-12).

blender --background --python Tools/build_fall_jungle.py

Writes SourceAssets/Cutscenes/FallToChult/: FBX meshes and PNG textures, all
procedural and seeded (no downloads, no third-party art). Import with
Tools/import_fall_cutscene.py. This is the cutscene's own set dressing for the
canopy crash and the landing tableau, not the playable Chult setting (Codex).

Units: 1 Blender unit = 1 cm (scene scale 0.01), Z up, X forward, as the other
generators. Vertex colour carries the material variation:
  leaves  r brightness, g deep (1) or yellow (0) green, b dryness, a wind weight
  bark    r brightness, g moss, b unused, a wind weight (0 on trunks)
Material slots: M_FallBark, M_FallLeaves (masked atlas), M_FallCanopy (the
opaque core of a crown, so it reads as a solid mass from above) and, since
2026-10-17, M_FallCrownLeaves (the crowns' leaf sprays: T_FallCrownLeaves from
Tools/build_crown_leaves.py).

Leaf atlas T_FallLeaves (2x2 tiles, u right, v up):
  0 bottom-left  broad tropical leaf, tip up      1 bottom-right pinnate frond
  2 top-left     elephant-ear (heart) leaf        3 top-right    small-leaf cluster
"""
import math
import random
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets/Cutscenes/FallToChult'
OUT.mkdir(parents=True, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = .01


# ---------------------------------------------------------------------------
# Textures
# ---------------------------------------------------------------------------
def save_png(name, rgba):
    """rgba: float array (h, w, 4) in 0..1, row 0 at the bottom (Blender's order)."""
    h, w, _ = rgba.shape
    img = bpy.data.images.new(name, w, h, alpha=True)
    img.pixels.foreach_set(np.clip(rgba, 0, 1).astype(np.float32).ravel())
    img.filepath_raw = str(OUT / f'{name}.png')
    img.file_format = 'PNG'
    img.save()
    print(f'CHUCK_FALL_TEXTURE {name} {w}x{h}')


def tile_noise(n, freqs, rng, aniso=(1., 1.)):
    """Tileable noise: random-phase sines on integer frequencies."""
    y, x = np.mgrid[0:n, 0:n] / n
    out = np.zeros((n, n))
    for f, amp in freqs:
        for _ in range(6):
            a = rng.uniform(0, 2 * math.pi)
            fx = int(round(f * math.cos(a) * aniso[0])); fy = int(round(f * math.sin(a) * aniso[1]))
            out += amp * np.sin(2 * math.pi * (fx * x + fy * y) + rng.uniform(0, 2 * math.pi))
    return out / max(1e-6, np.abs(out).max())


def normal_from_height(h, strength):
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * strength
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * strength
    n = np.dstack([-gx, -gy, np.ones_like(h)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return np.dstack([n * .5 + .5, np.ones_like(h)])


def leaf_tile(kind, n, rng):
    """One atlas tile: (n, n, 4). Local coords x in [-1, 1], y in [0, 1] (stem at y=0)."""
    yy, xx = np.mgrid[0:n, 0:n] + .5
    x = xx / n * 2 - 1; y = yy / n
    alpha = np.zeros((n, n)); vein = np.zeros((n, n)); shade = np.zeros((n, n))

    def broad(cx, cy, ang, length, width):
        # A leaf from (cx, cy) along angle ang: local u along, v across.
        dx = x - cx; dy = y - cy
        u = (dx * math.sin(ang) + dy * math.cos(ang)) / length
        v = (dx * math.cos(ang) - dy * math.sin(ang)) / length
        inside_u = (u > 0) & (u < 1)
        half = width * np.power(np.clip(np.sin(math.pi * np.clip(u, 0, 1) ** .85), 0, 1), .8) * (1 - .25 * u)
        m = inside_u & (np.abs(v) < half)
        mid = m & (np.abs(v) < .012 + .01 * (1 - u))
        lat = m & (np.mod((u - np.abs(v) * .9) * 13, 1) < .07)
        edge = np.where(m, 1 - np.clip(np.abs(v) / np.maximum(half, 1e-4), 0, 1), 0)
        return m, mid | lat, edge

    if kind == 0:
        m, vv, e = broad(0, .02, 0, .96, .40)
        alpha = m * 1.; vein = vv * 1.; shade = e
    elif kind == 1:
        # Pinnate frond: rachis up the middle, leaflets angled up on both sides.
        rach = (np.abs(x) < .014) & (y < .98)
        d = np.abs(x); s = .045
        k = np.round((y - d * .5 - .04) / s)
        yk = .04 + k * s
        length = .95 * np.sin(math.pi * np.clip(yk * .92 + .06, 0, 1)) ** .7
        t = np.clip(d / np.maximum(length, 1e-3), 0, 1)
        half = s * .46 * np.sin(math.pi * np.clip(t, 0, 1)) ** .5 + .004
        leaflet = (np.abs(y - (yk + d * .5)) < half) & (d < length) & (yk > .03) & (yk < .97)
        alpha = (rach | leaflet) * 1.
        vein = rach * 1. + (leaflet & (np.abs(y - (yk + d * .5)) < .004)) * .6
        shade = np.where(leaflet, 1 - np.abs(y - (yk + d * .5)) / np.maximum(half, 1e-4), .5)
    elif kind == 2:
        # Elephant ear: point at the tip, two round lobes by the stalk.
        xs = x / .78; ys = (y - .50) / .48
        yh = -ys * 1.15 + .15
        heart = (xs ** 2 + yh ** 2 - 1) ** 3 - xs ** 2 * yh ** 3 < 0
        notch = (np.abs(x) < .05 * (1 - np.clip((y - .2) / .25, 0, 1))) & (y < .45)
        m = heart & ~notch & (y > .03)
        ang = np.arctan2(x, y - .42)
        rad = np.hypot(x, y - .42)
        vv = m & ((np.mod(ang * 4.5 / math.pi, 1) < .06) | ((np.abs(x) < .012) & (y > .42)))
        alpha = m * 1.; vein = vv * 1.; shade = np.where(m, np.clip(1 - rad, 0, 1), 0)
    else:
        for i in range(9):
            a = i / 9 * 2 * math.pi + rng.uniform(-.2, .2)
            r0 = rng.uniform(.0, .1)
            m, vv, e = broad(r0 * math.sin(a), .5 + r0 * math.cos(a) * .5, a, rng.uniform(.4, .5), .30)
            alpha = np.maximum(alpha, m * 1.)
            vein = np.where(m, vv * 1., vein)
            shade = np.where(m, e * (.75 + .25 * rng.random()), shade)
        stem = (np.abs(x) < .012) & (y < .5)
        alpha = np.maximum(alpha, stem * 1.)
    noise = tile_noise(n, [(3, 1.), (9, .5), (27, .25)], rng)
    base = np.array([.10, .32, .07]); light = np.array([.24, .48, .12])
    col = base + (light - base) * np.clip(shade[..., None] * .7 + noise[..., None] * .15 + .2, 0, 1)
    col = col * (1 - .35 * vein[..., None]) + np.array([.42, .55, .22]) * .35 * vein[..., None]
    return np.dstack([col, alpha])


def leaf_atlas():
    rng = np.random.default_rng(11)
    n = 1024
    tiles = [leaf_tile(k, n, rng) for k in range(4)]
    atlas = np.zeros((2 * n, 2 * n, 4))
    atlas[0:n, 0:n] = tiles[0]; atlas[0:n, n:] = tiles[1]
    atlas[n:, 0:n] = tiles[2]; atlas[n:, n:] = tiles[3]
    # Bleed colour into the transparent texels so mips don't fringe dark.
    a = atlas[..., 3:4]
    mean = (atlas[..., :3] * a).sum((0, 1)) / max(1, a.sum())
    atlas[..., :3] = np.where(a > 0, atlas[..., :3], mean)
    save_png('T_FallLeaves', atlas)


def bark_texture():
    rng = np.random.default_rng(23)
    n = 1024
    fib = tile_noise(n, [(24, 1.), (60, .6), (140, .3)], rng, aniso=(1., .08))
    blot = tile_noise(n, [(2, 1.), (5, .6), (11, .3)], rng)
    cracks = np.abs(tile_noise(n, [(10, 1.), (22, .4)], rng, aniso=(1., .2)))
    height = .5 + .3 * fib - .35 * np.exp(-cracks * 9)
    col = np.array([.23, .19, .14])[None, None] * (.65 + .55 * height[..., None])
    col = col * (1 + .15 * blot[..., None])
    moss = np.clip((blot + .2 * fib) * 1.6 - .1, 0, 1)
    save_png('T_FallBark', np.dstack([col, moss]))
    save_png('T_FallBark_N', normal_from_height(height, 6.))


def ground_texture():
    """Leaf litter over dark soil, with twigs; tileable."""
    rng = np.random.default_rng(37)
    n = 1024
    soil = tile_noise(n, [(6, 1.), (20, .5), (70, .3)], rng)
    col = np.array([.11, .085, .06])[None, None] * (1 + .35 * soil[..., None])
    col = np.repeat(col, 1, 0)
    height = .2 + .05 * soil
    litter = [np.array(c) for c in ([.30, .17, .07], [.38, .25, .10], [.22, .12, .06], [.16, .19, .07], [.44, .32, .14], [.12, .14, .05])]
    for _ in range(4200):
        cx, cy = rng.uniform(0, n, 2)
        L = rng.uniform(22, 70); W = L * rng.uniform(.28, .45); a = rng.uniform(0, math.pi)
        r = int(L) + 2
        ys = (np.arange(int(cy) - r, int(cy) + r)) % n
        xs = (np.arange(int(cx) - r, int(cx) + r)) % n
        gy, gx = np.meshgrid(np.arange(int(cy) - r, int(cy) + r) - cy, np.arange(int(cx) - r, int(cx) + r) - cx, indexing='ij')
        u = (gx * math.cos(a) + gy * math.sin(a)) / L
        v = (-gx * math.sin(a) + gy * math.cos(a)) / W
        m = (u * u + v * v) < .25
        if not m.any(): continue
        c = litter[rng.integers(len(litter))] * rng.uniform(.75, 1.15)
        vein = np.abs(v) < .05
        patch = col[np.ix_(ys, xs)]
        patch[m] = c * np.where(vein[m], .8, 1.)[:, None]
        col[np.ix_(ys, xs)] = patch
        hp = height[np.ix_(ys, xs)]
        hp[m] = hp[m] + .06 + .04 * (1 - 4 * (u[m] ** 2 + v[m] ** 2))
        height[np.ix_(ys, xs)] = hp
    for _ in range(60):
        x0, y0 = rng.uniform(0, n, 2); a = rng.uniform(0, math.pi); L = rng.uniform(60, 200)
        for t in np.linspace(0, 1, int(L)):
            px = int(x0 + math.cos(a) * L * t) % n; py = int(y0 + math.sin(a) * L * t) % n
            col[py, px] = [.14, .10, .06]; col[(py + 1) % n, px] = [.11, .08, .05]
            height[py, px] += .1
    save_png('T_FallGround', np.dstack([col, np.ones((n, n))]))
    save_png('T_FallGround_N', normal_from_height(height, 4.))


# ---------------------------------------------------------------------------
# Mesh builder
# ---------------------------------------------------------------------------
TILE = {0: (0., 0.), 1: (.5, 0.), 2: (0., .5), 3: (.5, .5)}
SLOTS = ['M_FallBark', 'M_FallLeaves', 'M_FallCanopy', 'M_FallCrownLeaves']


class Builder:
    def __init__(self):
        self.v = []; self.f = []; self.uv = []; self.col = []; self.mat = []

    def face(self, idx, uvs, cols, mat):
        self.f.append(idx); self.uv.append(uvs); self.col.append(cols); self.mat.append(mat)

    def add(self, p):
        self.v.append(Vector(p)); return len(self.v) - 1

    def tube(self, pts, radii, sides=10, vscale=200., colour=(1, 0, 0, 0), rng=None, rough=.0, wind=None):
        """A tube along pts, radius per point; caps omitted (ends are buried or tiny)."""
        rings = []
        for i, p in enumerate(pts):
            t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
            side = t.cross(Vector((0, 0, 1)))
            if side.length < .1: side = t.cross(Vector((1, 0, 0)))
            side.normalize(); up = side.cross(t).normalized()
            ring = []
            for s in range(sides + 1):
                a = s / sides * 2 * math.pi
                r = radii[i] * (1 + (rng.uniform(-rough, rough) if rng and s < sides else 0))
                ring.append(self.add(p + (side * math.cos(a) + up * math.sin(a)) * r))
            if rough and rng:
                self.v[ring[-1]] = self.v[ring[0]].copy()
            rings.append(ring)
        length = [0.]
        for i in range(1, len(pts)): length.append(length[-1] + (pts[i] - pts[i - 1]).length)
        for i in range(len(pts) - 1):
            for s in range(sides):
                a, b, c, d = rings[i][s], rings[i][s + 1], rings[i + 1][s + 1], rings[i + 1][s]
                u0, u1 = s / sides, (s + 1) / sides
                v0, v1 = length[i] / vscale, length[i + 1] / vscale
                w0 = wind[i] if wind else colour[3]; w1 = wind[i + 1] if wind else colour[3]
                c0 = (*colour[:3], w0); c1 = (*colour[:3], w1)
                # Wound so the faces point out of the tube.
                self.face([a, d, c, b], [(u0 * 2, v0), (u0 * 2, v1), (u1 * 2, v1), (u1 * 2, v0)], [c0, c1, c1, c0], 0)
        return rings

    def strip(self, pts, widths, normals, tile, colour, mat=1, wind=None, u_span=(0., 1.)):
        """A bent card along pts (leaf base first), across = widths, card faces along normals."""
        du, dv = TILE[tile]
        left = []; right = []
        for i, p in enumerate(pts):
            t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
            across = normals[i].cross(t).normalized()
            left.append(self.add(p - across * widths[i])); right.append(self.add(p + across * widths[i]))
        n = len(pts) - 1
        for i in range(n):
            v0, v1 = i / n, (i + 1) / n
            w0 = wind[i] if wind else colour[3]; w1 = wind[i + 1] if wind else colour[3]
            c0 = (*colour[:3], w0); c1 = (*colour[:3], w1)
            uv = [(du + .5 * u_span[0], dv + .5 * v0), (du + .5 * u_span[1], dv + .5 * v0),
                  (du + .5 * u_span[1], dv + .5 * v1), (du + .5 * u_span[0], dv + .5 * v1)]
            self.face([left[i], right[i], right[i + 1], left[i + 1]], uv, [c0, c0, c1, c1], mat)

    def card(self, centre, normal, up, w, h, tile, colour, cup=0., mat=1):
        """A flat (or slightly cupped) leaf card centred on centre, its stem toward -up."""
        normal = Vector(normal).normalized(); up = (Vector(up) - normal * Vector(up).dot(normal)).normalized()
        pts = [Vector(centre) + up * (h * (i / 3 - .5)) - normal * (cup * h * (i / 3 - .5) ** 2) for i in range(4)]
        self.strip(pts, [w * .5] * 4, [normal] * 4, tile, colour, mat=mat)

    def blob(self, centre, radius, rng, colour, squash=.75, subdiv=2):
        """The opaque core of a canopy crown: a lumpy, flattened icosphere."""
        import bmesh
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.)
        base = len(self.v)
        lumps = [(Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-.3, 1))).normalized(), rng.uniform(.15, .35)) for _ in range(7)]
        for vert in bm.verts:
            d = vert.co.normalized()
            r = 1 + sum(a * max(0., d.dot(l)) ** 3 for l, a in lumps) + rng.uniform(-.06, .06)
            p = d * r
            self.v.append(Vector(centre) + Vector((p.x * radius, p.y * radius, p.z * radius * squash)))
        for face in bm.faces:
            idx = [base + v.index for v in face.verts]
            self.face(idx, [(.75, .75)] * len(idx), [colour] * len(idx), 2)
        bm.free()

    def write(self, name):
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata([tuple(v) for v in self.v], [], self.f)
        mesh.update()
        uv = mesh.uv_layers.new(name='UVMap')
        col = mesh.color_attributes.new('Col', 'FLOAT_COLOR', 'CORNER')
        li = 0
        for fi, poly in enumerate(mesh.polygons):
            for k, loop in enumerate(poly.loop_indices):
                uv.data[loop].uv = self.uv[fi][k]
                col.data[loop].color = self.col[fi][k]
            poly.material_index = self.mat[fi]
            poly.use_smooth = self.mat[fi] != 1
        used = sorted(set(self.mat))
        for slot in range(max(used) + 1):
            m = bpy.data.materials.get(SLOTS[slot]) or bpy.data.materials.new(SLOTS[slot])
            mesh.materials.append(m)
        obj = bpy.data.objects.new(name, mesh)
        scene.collection.objects.link(obj)
        for o in scene.objects: o.select_set(o == obj)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.fbx(filepath=str(OUT / f'{name}.fbx'), use_selection=True, object_types={'MESH'},
                                 apply_unit_scale=True, axis_forward='-Y', axis_up='Z', bake_anim=False,
                                 mesh_smooth_type='FACE', add_leaf_bones=False)
        lo = Vector((min(v.x for v in self.v), min(v.y for v in self.v), min(v.z for v in self.v)))
        hi = Vector((max(v.x for v in self.v), max(v.y for v in self.v), max(v.z for v in self.v)))
        print(f'CHUCK_FALL_MESH {name} tris={sum(len(f) - 2 for f in self.f)} size_cm={tuple(round(c) for c in (hi - lo))}')


def leaf_colour(rng, deep=None, dry=0.):
    return (rng.uniform(.3, .9), deep if deep is not None else rng.uniform(.4, 1.), dry, 1.)


def bezier(p0, p1, p2, n):
    return [p0 * (1 - t) ** 2 + p1 * 2 * t * (1 - t) + p2 * t * t for t in (i / n for i in range(n + 1))]


def frond(b, base, direction, length, droop, width, tile, rng, segments=6, colour=None):
    """An arching leaf strip from base: out along direction, sagging by droop."""
    direction = Vector(direction).normalized()
    flat = Vector((direction.x, direction.y, 0)).normalized()
    p1 = base + direction * length * .55
    p2 = base + flat * length * .95 + Vector((0, 0, direction.z * length * .45 - droop))
    pts = bezier(base, p1, p2, segments)
    side = flat.cross(Vector((0, 0, 1)))
    normals = []
    for i in range(len(pts)):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        normals.append(side.cross(t).normalized() * -1 if side.cross(t).z < 0 else side.cross(t).normalized())
    widths = [width * (.35 + .65 * math.sin(math.pi * min(.999, .08 + .92 * i / segments)) ** .6) for i in range(segments + 1)]
    winds = [(i / segments) ** 1.5 for i in range(segments + 1)]
    b.strip(pts, widths, normals, tile, colour or leaf_colour(rng), wind=winds)


# ---------------------------------------------------------------------------
# The set
# ---------------------------------------------------------------------------
def crown(b, centre, radius, rng, cards=70, squash=.7, card_size=(120, 170), subdiv=3):
    """A tropical crown (2026-10-17; it was one round lumpy ball in an even
    shell of square cards): five to eight irregular clumps over a broad,
    flattish umbrella, each a small dark core under leaf-spray cards
    (T_FallCrownLeaves, built from scanned leaves by build_crown_leaves.py)
    lying mostly up and out, with sprays hanging at the rims. It takes one
    draw from rng and uses its own stream, so the caller's later draws keep
    their count. subdiv is kept for the callers' signature."""
    r = random.Random(rng.random())
    clumps = r.randint(5, 8)
    per = max(18, int(cards * 3.3 / clumps))
    centres = []
    for k in range(clumps):
        a = k / clumps * 2 * math.pi + r.uniform(-.45, .45)
        d = 0. if k == 0 else r.uniform(.3, .78)
        c = Vector(centre) + Vector((math.cos(a) * d * radius, math.sin(a) * d * radius,
                                     (r.uniform(-.2, .35) - .35 * d * d) * radius * squash))
        centres.append((c, radius * r.uniform(.36, .52) * (1.2 if k == 0 else 1.)))
    for c, cr in centres:
        b.blob(c, cr * .5, r, (r.uniform(.4, .65), r.uniform(.5, 1.), 0., .15), squash=.55, subdiv=1)
        scale = cr / (radius * .44)
        for _ in range(per):
            d = Vector((r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(-.45, 1.))).normalized()
            p = c + Vector((d.x * cr, d.y * cr, d.z * cr * .85)) * r.uniform(.45, 1.08)
            flat = Vector((d.x, d.y, 0))
            outward = flat.normalized() if flat.length > .05 else Vector((math.cos(r.uniform(0, 6.3)), math.sin(r.uniform(0, 6.3)), 0))
            # The spray runs out from the clump (stem in, tip out), rising on
            # top and drooping at the rim; it faces the sky more than the side.
            up = (outward * r.uniform(.6, 1.) + Vector((0, 0, .9 * d.z + r.uniform(-.35, .3)))).normalized()
            w = r.uniform(.2, .7)
            normal = (Vector((0, 0, 1)) * w + d * (1 - w) + Vector((r.uniform(-.25, .25), r.uniform(-.25, .25), 0))).normalized()
            s = r.uniform(*card_size) * scale * 1.1
            tile = 3 if d.z > .7 and r.random() < .45 else r.randint(0, 2)
            b.card(p + up * s * .35, normal, up, s, s, tile, leaf_colour(r), cup=.22, mat=3)
        for _ in range(max(3, per // 5)):
            a = r.uniform(0, 2 * math.pi); out = Vector((math.cos(a), math.sin(a), 0))
            p = c + out * cr * r.uniform(.75, 1.05) - Vector((0, 0, cr * .15))
            up = (out * r.uniform(.4, .7) + Vector((0, 0, -1))).normalized()
            s = r.uniform(*card_size) * scale * r.uniform(.8, 1.)
            b.card(p + up * s * .4, (out + Vector((0, 0, .25))).normalized(), up, s, s, r.randint(0, 2), leaf_colour(r), cup=.18, mat=3)


def trunk_radius(base_radius, height, z):
    """The trunk's radius at height z: tapering, flaring to 2.1x at the ground."""
    t = max(z, 0.) / (height * .86)
    return base_radius * (1 - .62 * t) * (1 + 1.1 * max(0., .12 - t) / .12)


def buttress_extent(base_radius, height, reach, top, z):
    """How far a buttress stands out from the trunk at height z (0 above its top).
    Its outer edge leaves the bark tangentially at the top and swings out to
    reach at the ground. Mirrored by ChultRoots::FinEdge."""
    if z >= top: return 0.
    t = 1 - (max(z, 0.) / top) ** (1 / 2.2)
    return (reach - trunk_radius(base_radius, height, z)) * t ** 1.6


def buttressed_trunk(b, pts, height, base_radius, buttresses, seed):
    """The trunk and its buttresses as one surface (user 2026-10-13: no sharp
    corners where the roots meet the trunk). Each buttress is a smooth lobe of
    the trunk's own radius - broad and filleted where it joins, a thin rounded
    plank further out, fading into the bark at its top - so there is no seam,
    crease or spike where root meets trunk. Angles are sampled densely across
    each lobe and sparsely elsewhere."""
    angles = [k / 20 * 2 * math.pi for k in range(20)]
    for a, reach, top, th in buttresses:
        w = th * 1.25 / (trunk_radius(base_radius, height, 0) + .5 * buttress_extent(base_radius, height, reach, top, 0))
        angles += [a + o * w for o in (-4, -2.6, -1.7, -1.1, -.65, -.3, 0, .3, .65, 1.1, 1.7, 2.6, 4)]
    angles = sorted(x % (2 * math.pi) for x in angles)
    keep = [angles[0]]
    for x in angles[1:]:
        if x - keep[-1] > .004: keep.append(x)
    if keep[-1] - keep[0] > 2 * math.pi - .004: keep.pop()
    angles = keep
    top_z = max(t for _, _, t, _ in buttresses) + 40
    zs = [-12 + (top_z + 12) * (i / 15) ** 1.5 for i in range(16)] + [top_z + (pts[-1].z - top_z) * i / 10 for i in range(1, 11)]
    span = height * .86

    def centre(z):
        f = min(max(z, 0.) / span * 14, 14.)
        i = min(int(f), 13)
        c = pts[i].lerp(pts[i + 1], f - i)
        return Vector((c.x, c.y, z))

    rings = []
    for z in zs:
        c = centre(z); R = trunk_radius(base_radius, height, z)
        ring = []
        for th_ in angles:
            r = R * (1 + .025 * math.sin(5 * th_ + z / 70 + seed) + .015 * math.sin(11 * th_ - z / 45))
            moss = 0.
            for k, (a, reach, top, th) in enumerate(buttresses):
                B = buttress_extent(base_radius, height, reach, top, z)
                if B <= 0: continue
                w = th * 1.25 / (R + .5 * B)
                d = (th_ - a + math.pi) % (2 * math.pi) - math.pi
                lobe = B * math.exp(-(d / w) ** 2)
                r += lobe; moss = max(moss, min(1., lobe / 40))
            ring.append((b.add(c + Vector((math.cos(th_), math.sin(th_), 0)) * r), (z + .6 * (r - R)) / 260, moss))
        rings.append(ring)
    n = len(angles)
    for i in range(len(rings) - 1):
        for q in range(n):
            q1 = (q + 1) % n
            u0 = angles[q] / math.pi; u1 = (angles[q1] if q1 else 2 * math.pi) / math.pi
            (a0, v00, m00), (b0, v01, m01) = rings[i][q], rings[i][q1]
            (a1, v10, m10), (b1, v11, m11) = rings[i + 1][q], rings[i + 1][q1]
            col = lambda m: (.8, .25 + .12 * m, 0, 0)
            b.face([a0, b0, b1, a1], [(u0, v00), (u1, v01), (u1, v11), (u0, v10)], [col(m00), col(m01), col(m11), col(m10)], 0)


def tree(name, seed, height, base_radius, lianas):
    rng = random.Random(seed)
    b = Builder()
    # Trunk: tall, nearly straight, flaring into its buttresses.
    lean = Vector((rng.uniform(-.04, .04), rng.uniform(-.04, .04), 1))
    pts = []; radii = []
    for i in range(15):
        t = i / 14
        z = height * .86 * t
        wob = Vector((math.sin(t * 5 + seed) * 18, math.cos(t * 4 + seed) * 18, 0))
        pts.append(Vector((lean.x * z, lean.y * z, z)) + wob * t)
        radii.append(base_radius * (1 - .62 * t) * (1 + 1.1 * max(0, .12 - t) / .12))
    # (The trunk used to be a rough tube: keep its random draws so every later
    # draw - buttresses, branches, crowns, lianas - is unchanged.)
    for _ in range(15 * 14): rng.uniform(-.05, .05)
    # Buttress roots: thin tall fins running out from the trunk to the ground,
    # built with the trunk as one surface (buttressed_trunk).
    fins = rng.randint(5, 6)
    buttresses = []
    for k in range(fins):
        a = k / fins * 2 * math.pi + rng.uniform(-.25, .25)
        reach = base_radius * rng.uniform(3.2, 4.4); top = base_radius * rng.uniform(4, 6.5); th = rng.uniform(9, 14)
        buttresses.append((a, reach, top, th))
    buttressed_trunk(b, pts, height, base_radius, buttresses, seed)
    # Branches and crowns near the top.
    top = pts[-1]
    branch_ends = []
    for k in range(rng.randint(5, 7)):
        a = k * 2.4 + rng.uniform(-.3, .3)
        start = pts[rng.randint(11, 14)]
        out = Vector((math.cos(a), math.sin(a), rng.uniform(.5, 1.1))).normalized()
        L = height * rng.uniform(.17, .26)
        mid = start + out * L * .5 + Vector((0, 0, L * .12))
        end = start + out * L
        bp = bezier(start, mid, end, 5)
        b.tube(bp, [radii[-1] * .55 * (1 - .7 * i / 5) for i in range(6)], sides=8, vscale=200, colour=(.8, .2, 0, 0), wind=[.0, .02, .04, .07, .1, .14])
        branch_ends.append(end)
        crown(b, end + Vector((0, 0, 40)), height * rng.uniform(.10, .14), rng)
    crown(b, top + Vector((0, 0, 90)), height * .13, rng)
    # Lianas: hanging from branch ends nearly to the ground, a few leaves on them.
    for k in range(lianas):
        end = branch_ends[k % len(branch_ends)] + Vector((rng.uniform(-60, 60), rng.uniform(-60, 60), -40))
        foot = Vector((end.x * rng.uniform(.6, 1.1), end.y * rng.uniform(.6, 1.1), rng.uniform(40, height * .25)))
        sag = (end + foot) * .5 + Vector((rng.uniform(-150, 150), rng.uniform(-150, 150), -rng.uniform(0, 200)))
        lp = bezier(end, sag, foot, 16)
        b.tube(lp, [3.2] * len(lp), sides=6, vscale=120, colour=(.7, .6, 0, 0), wind=[.1 + .3 * (i / 16) for i in range(17)])
        for i in range(2, 16, 2):
            p = lp[i]
            d = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-.3, .3))).normalized()
            b.card(p + d * 14, d, Vector((0, 0, -1)) + d, rng.uniform(28, 40), rng.uniform(36, 50), 0, leaf_colour(rng), cup=.2)
    b.write(name)


def palm():
    rng = random.Random(5)
    b = Builder()
    pts = bezier(Vector((0, 0, -10)), Vector((60, 0, 450)), Vector((140, 20, 880)), 12)
    b.tube(pts, [26 - 10 * i / 12 for i in range(13)], sides=10, vscale=60, colour=(.95, .1, 0, 0), wind=[.0] * 9 + [.02, .04, .06, .08])
    head = pts[-1]
    for k in range(14):
        a = k / 14 * 2 * math.pi + rng.uniform(-.15, .15)
        rise = rng.uniform(-.35, .55)
        frond(b, head, (math.cos(a), math.sin(a), rise), rng.uniform(380, 470), rng.uniform(140, 260), 65, 1, rng, segments=7)
    b.write('SM_FallPalm')


def fern():
    rng = random.Random(7)
    b = Builder()
    for k in range(16):
        a = k / 16 * 2 * math.pi + rng.uniform(-.2, .2)
        rise = rng.uniform(.6, 1.3)
        frond(b, Vector((0, 0, 0)), (math.cos(a), math.sin(a), rise), rng.uniform(110, 175), rng.uniform(50, 110), 26, 1, rng, segments=6)
    b.write('SM_FallFern')


def elephant_ear():
    rng = random.Random(9)
    b = Builder()
    for k in range(7):
        a = k / 7 * 2 * math.pi + rng.uniform(-.3, .3)
        out = Vector((math.cos(a), math.sin(a), 0))
        h = rng.uniform(70, 150)
        tip = out * rng.uniform(25, 55) + Vector((0, 0, h))
        stalk = bezier(Vector((0, 0, -5)), out * 5 + Vector((0, 0, h * .6)), tip, 6)
        b.tube(stalk, [3.2 - 1.6 * i / 6 for i in range(7)], sides=6, vscale=80, colour=(.8, .9, 0, 0), wind=[(i / 6) ** 2 * .5 for i in range(7)])
        size = rng.uniform(70, 105)
        normal = (Vector((0, 0, 1)) + out * rng.uniform(.6, 1.2)).normalized()
        up = (out + Vector((0, 0, -rng.uniform(.2, .6)))).normalized()
        b.card(tip + up * size * .45, normal, up, size * .95, size, 2, leaf_colour(rng, deep=rng.uniform(.7, 1.)), cup=.25)
    b.write('SM_FallElephantEar')


def shrub():
    rng = random.Random(13)
    b = Builder()
    for k in range(26):
        a = rng.uniform(0, 2 * math.pi)
        out = Vector((math.cos(a), math.sin(a), 0))
        h = rng.uniform(25, 110)
        p = out * rng.uniform(8, 50) + Vector((0, 0, h))
        normal = (Vector((0, 0, 1)) + out * rng.uniform(.2, .9)).normalized()
        up = (out + Vector((0, 0, rng.uniform(.1, .6)))).normalized()
        b.card(p, normal, up, rng.uniform(22, 32), rng.uniform(40, 58), 0, leaf_colour(rng), cup=.2)
        b.tube(bezier(Vector((0, 0, -4)), out * 4 + Vector((0, 0, h * .5)), p - up * 20, 3), [1.4, 1.2, 1., .8], sides=5, vscale=60, colour=(.7, .7, 0, 0), wind=[0, .1, .2, .3])
    b.write('SM_FallShrub')


def log_end(b, ring, centre, out, radius, rng):
    """Closes a log's end (2026-10-27: the open tube could be seen through): a ragged broken rim, then the
    weathered end grain sinking to a rotten, dark heart, all closed."""
    rim = [b.v[i].copy() for i in ring[:-1]]
    n = len(rim)
    inner = []
    for k, p in enumerate(rim):
        d = (p - centre)
        # The rim breaks unevenly: some splinters stand proud, some are bitten back.
        inner.append(b.add(centre + d * rng.uniform(.62, .78) + out * rng.uniform(-9, 5)))
    heart = b.add(centre - out * rng.uniform(10, 16))
    grain = (.42, .05, 0, 0); dark = (.22, 0, 0, 0)

    def put(idx, uvs, cols):
        a, c, d = (b.v[i] for i in idx[:3])
        if (c - a).cross(d - a).dot(out) < 0:
            idx = list(reversed(idx)); uvs = list(reversed(uvs)); cols = list(reversed(cols))
        b.face(idx, uvs, cols, 0)

    def uv(p):
        d = p - centre
        return (.5 + d.dot(Vector((0, 1, 0))) / (radius * 2.2), .5 + d.dot(Vector((0, 0, 1))) / (radius * 2.2))
    for k in range(n):
        a, c = ring[k], ring[k + 1]
        ia, ic = inner[k], inner[(k + 1) % n]
        put([a, c, ic, ia], [uv(b.v[a]), uv(b.v[c]), uv(b.v[ic]), uv(b.v[ia])], [grain, grain, grain, grain])
        put([ia, ic, heart], [uv(b.v[ia]), uv(b.v[ic]), uv(b.v[heart])], [grain, grain, dark])


def log():
    rng = random.Random(17)
    b = Builder()
    pts = [Vector((-350 + 700 * i / 10, math.sin(i * .7) * 12, 48 + math.sin(i * .5) * 5)) for i in range(11)]
    radii = [55 * (1 - .25 * i / 10) for i in range(11)]
    # Moss on the top, more toward the old end.
    rings = b.tube(pts, radii, sides=16, vscale=240, colour=(.65, .9, 0, 0), rng=rng, rough=.06)
    log_end(b, rings[0], pts[0], (pts[0] - pts[1]).normalized(), radii[0], rng)
    log_end(b, rings[-1], pts[-1], (pts[-1] - pts[-2]).normalized(), radii[-1], rng)
    for _ in range(10):
        p = pts[rng.randint(1, 9)] + Vector((0, rng.uniform(-40, 40), 45))
        b.card(p, (0, 0, 1), (rng.uniform(-1, 1), rng.uniform(-1, 1), 0), rng.uniform(20, 32), rng.uniform(30, 44), 0, leaf_colour(rng, dry=.4), cup=-.1)
    b.write('SM_FallLog')


def far_crown():
    rng = random.Random(19)
    b = Builder()
    crown(b, Vector((0, 0, 0)), 420, rng, cards=44, squash=.6, card_size=(180, 260), subdiv=2)
    b.write('SM_FallCrown')


def leaf_card():
    b = Builder()
    b.card(Vector((0, 0, 0)), (0, 0, 1), (1, 0, 0), 16, 22, 0, (.7, .8, .15, 1.), cup=.2)
    b.write('SM_FallLeafCard')


# blender --background --python Tools/build_fall_jungle.py -- --only SM_FallTree_A,SM_FallTree_B
# writes just those meshes (their seeds and draws are independent of the rest).
ONLY = set(sys.argv[sys.argv.index('--only') + 1].split(',')) if '--only' in sys.argv else None


def want(name):
    return ONLY is None or name in ONLY


if ONLY is None:
    leaf_atlas()
    bark_texture()
    ground_texture()
if want('SM_FallTree_A'): tree('SM_FallTree_A', 3, 3600, 78, lianas=4)
if want('SM_FallTree_B'): tree('SM_FallTree_B', 8, 2900, 62, lianas=6)
if want('SM_FallPalm'): palm()
if want('SM_FallFern'): fern()
if want('SM_FallElephantEar'): elephant_ear()
if want('SM_FallShrub'): shrub()
if want('SM_FallLog'): log()
if want('SM_FallCrown'): far_crown()
if want('SM_FallLeafCard'): leaf_card()
print('CHUCK_FALL_JUNGLE_READY')
