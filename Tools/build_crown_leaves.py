"""Leaf-spray atlas for the jungle trees' crowns (Claude, 2026-10-17).

blender --background --python Tools/build_crown_leaves.py   (Blender 4.5.14: numpy and its image IO)

Composites the real leaves of Poly Haven's CC0 island_tree_02 scan (already in
SourceAssets/Setting/ChultNature, manifest-verified) into four leaf sprays and
writes SourceAssets/Cutscenes/FallToChult/T_FallCrownLeaves.png (+ _N, DirectX
normal) as a 2x2 atlas, tile order as T_FallLeaves (u right, v up):
  0 bottom-left, 1 bottom-right, 2 top-left: sprays on a twig, stem at the
    tile's bottom middle, tip up
  3 top-right: a rosette of leaves seen from above
Used by build_fall_jungle.py's crowns (M_FallCrownLeaves). Seeded: the same
output every run.
"""
import math
import random
import sys
from pathlib import Path

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'SourceAssets/Setting/ChultNature'
OUT = ROOT / 'SourceAssets/Cutscenes/FallToChult'
N = 1024  # tile size


def load(path):
    """Image as (rows, cols, 4) float, row 0 at the top."""
    im = bpy.data.images.load(str(path)); im.colorspace_settings.name = 'Non-Color'
    w, h = im.size; px = np.empty(w * h * 4, np.float32); im.pixels.foreach_get(px)
    bpy.data.images.remove(im)
    return px.reshape(h, w, 4)[::-1].copy()


def save(path, arr):
    h, w = arr.shape[:2]
    rgba = arr if arr.shape[2] == 4 else np.dstack([arr, np.ones((h, w), np.float32)])
    im = bpy.data.images.new(path.stem, w, h, alpha=True, float_buffer=False)
    im.colorspace_settings.name = 'Non-Color'
    im.pixels.foreach_set(np.ascontiguousarray(rgba[::-1]).ravel())
    im.filepath_raw = str(path); im.file_format = 'PNG'; im.save()
    bpy.data.images.remove(im)


diff = load(SRC / 'island_tree_02_leaves_diff_2k.jpg')[..., :3]
alpha = load(SRC / 'island_tree_02_leaves_alpha_2k.png')[..., 0]
alpha = alpha / alpha.max()
nrm = load(SRC / 'island_tree_02_leaves_nor_dx_2k.jpg')[..., :3]


def spans(mask):
    """Runs of True in a 1D mask, as (start, end)."""
    out = []; start = None
    for i, v in enumerate(mask):
        if v and start is None: start = i
        if not v and start is not None:
            if i - start > 40: out.append((start, i))
            start = None
    if start is not None: out.append((start, len(mask)))
    return out


# The scan lays its leaves out in rows, tip up: find each leaf's box.
leaves = []
solid = alpha > .5
for r0, r1 in spans(solid.any(1)):
    for c0, c1 in spans(solid[r0:r1].any(0)):
        a = alpha[r0:r1, c0:c1]
        rows = np.where((a > .5).any(1))[0]
        y0, y1, x0, x1 = r0 + rows[0], r0 + rows[-1] + 1, c0, c1
        leaves.append((np.dstack([diff[y0:y1, x0:x1], alpha[y0:y1, x0:x1]]), nrm[y0:y1, x0:x1]))
if len(leaves) < 6: raise RuntimeError(f'Found {len(leaves)} leaves in the scan, expected 8')
print(f'CHUCK_CROWN_LEAVES source_leaves={len(leaves)}')


def bilinear(img, x, y):
    h, w = img.shape[:2]
    x = np.clip(x, 0, w - 1.001); y = np.clip(y, 0, h - 1.001)
    x0 = x.astype(int); y0 = y.astype(int); fx = (x - x0)[..., None]; fy = (y - y0)[..., None]
    return (img[y0, x0] * (1 - fx) * (1 - fy) + img[y0, x0 + 1] * fx * (1 - fy) +
            img[y0 + 1, x0] * (1 - fx) * fy + img[y0 + 1, x0 + 1] * fx * fy)


def place(colour, normal, leaf, at, angle, height, shade, rng):
    """Composite one leaf: its stem at `at` (pixels, y down), pointing `angle`
    degrees clockwise from straight up, `height` pixels tall."""
    rgba, nm = leaf
    h, w = rgba.shape[:2]; s = height / h
    a = math.radians(angle); ca, sa = math.cos(a), math.sin(a)
    # Leaf-local (x right of the midrib, up toward the tip) to tile pixels.
    corners = [(-w / 2, 0), (w / 2, 0), (-w / 2, h), (w / 2, h)]
    pts = [(at[0] + (lx * ca + ly * sa) * s, at[1] + (lx * sa - ly * ca) * s) for lx, ly in corners]
    xa = max(0, int(min(p[0] for p in pts)) - 1); xb = min(N, int(max(p[0] for p in pts)) + 2)
    ya = max(0, int(min(p[1] for p in pts)) - 1); yb = min(N, int(max(p[1] for p in pts)) + 2)
    if xa >= xb or ya >= yb: return
    yy, xx = np.mgrid[ya:yb, xa:xb].astype(np.float32)
    dx = (xx - at[0]) / s; dy = (yy - at[1]) / s
    lx = dx * ca + dy * sa; ly = dx * sa - dy * ca
    sx = lx + w / 2; sy = (h - 1) - ly
    inside = ((sx >= 0) & (sx <= w - 1) & (sy >= 0) & (sy <= h - 1))[..., None]
    src = bilinear(rgba, sx, sy) * inside; nsrc = bilinear(nm, sx, sy)
    # Turn the normals' xy with the leaf (DirectX: green is image-down).
    nx = nsrc[..., 0] * 2 - 1; ny = -(nsrc[..., 1] * 2 - 1); r = -a
    rx = nx * math.cos(r) - ny * math.sin(r); ry = nx * math.sin(r) + ny * math.cos(r)
    nsrc = np.dstack([rx * .5 + .5, -ry * .5 + .5, nsrc[..., 2]])
    k = src[..., 3:4]
    tint = np.array([rng.uniform(.9, 1.05), rng.uniform(.95, 1.08), rng.uniform(.85, 1.)], np.float32) * shade
    colour[ya:yb, xa:xb, :3] = colour[ya:yb, xa:xb, :3] * (1 - k) + np.clip(src[..., :3] * tint, 0, 1) * k
    colour[ya:yb, xa:xb, 3:] = np.maximum(colour[ya:yb, xa:xb, 3:], k)
    normal[ya:yb, xa:xb] = normal[ya:yb, xa:xb] * (1 - k) + nsrc * k


def twig(colour, pts, width):
    """A tapering brown twig along pts, drawn as overlapping discs."""
    yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
    for i in range(len(pts) - 1):
        r = max(1.5, width * (1 - .6 * i / len(pts)) / 2)
        for t in np.linspace(0, 1, 8):
            cx = pts[i][0] + (pts[i + 1][0] - pts[i][0]) * t; cy = pts[i][1] + (pts[i + 1][1] - pts[i][1]) * t
            x0, x1 = int(max(0, cx - r - 1)), int(min(N, cx + r + 2)); y0, y1 = int(max(0, cy - r - 1)), int(min(N, cy + r + 2))
            if x0 >= x1 or y0 >= y1: continue
            m = (((xx[y0:y1, x0:x1] - cx) ** 2 + (yy[y0:y1, x0:x1] - cy) ** 2) <= r * r)[..., None]
            colour[y0:y1, x0:x1] = np.where(m, np.array([.23, .17, .12, 1.], np.float32), colour[y0:y1, x0:x1])


def spray(seed):
    rng = random.Random(seed)
    colour = np.zeros((N, N, 4), np.float32); normal = np.zeros((N, N, 3), np.float32); normal[..., :] = (.5, .5, 1)
    bend = rng.uniform(-90, 90)
    path = [(N / 2 + bend * math.sin(t * math.pi * .5) + 30 * math.sin(t * 5), N - 10 - t * (N * .78)) for t in (i / 24 for i in range(25))]
    side_twigs = []
    for k in range(rng.randint(2, 3)):
        i = rng.randint(6, 14); sg = -1 if k % 2 else 1
        base = path[i]; L = rng.uniform(.25, .38) * N; ang = math.radians(sg * rng.uniform(35, 55))
        side_twigs.append([(base[0] + math.sin(ang) * L * t, base[1] - math.cos(ang) * L * t - 40 * t * t) for t in (j / 10 for j in range(11))])
    twig(colour, path, 9)
    for p in side_twigs: twig(colour, p, 6)
    # Leaves along every twig, alternating sides, back ones darker: deeper inside the spray.
    jobs = []
    for p, main in [(path, True)] + [(p, False) for p in side_twigs]:
        n = len(p)
        for j in range(3 if main else 2, n - 1, 2 if main else 2):
            t = j / (n - 1); sg = -1 if j % 4 < 2 else 1
            dx, dy = p[min(j + 1, n - 1)][0] - p[j][0], p[min(j + 1, n - 1)][1] - p[j][1]
            along = math.degrees(math.atan2(dx, -dy))
            jobs.append((p[j], along + sg * rng.uniform(30, 68), rng.uniform(.16, .25) * N * (1.05 - .3 * abs(t - .5)), rng.random()))
        tip = p[-1]; dx, dy = p[-1][0] - p[-2][0], p[-1][1] - p[-2][1]
        jobs.append((tip, math.degrees(math.atan2(dx, -dy)) + rng.uniform(-12, 12), rng.uniform(.17, .22) * N, rng.random()))
    jobs.sort(key=lambda j: j[3])
    for at, angle, height, depth in jobs:
        place(colour, normal, leaves[rng.randrange(len(leaves))], at, angle, height, .62 + .38 * depth, rng)
    return colour, normal


def rosette(seed):
    rng = random.Random(seed)
    colour = np.zeros((N, N, 4), np.float32); normal = np.zeros((N, N, 3), np.float32); normal[..., :] = (.5, .5, 1)
    c = (N / 2, N / 2)
    for ring, (count, size, shade) in enumerate([(9, .30, .7), (8, .25, .85), (5, .19, 1.)]):
        for k in range(count):
            a = k / count * 360 + rng.uniform(-15, 15) + ring * 20
            r = rng.uniform(0, .04) * N
            at = (c[0] + math.sin(math.radians(a)) * r, c[1] - math.cos(math.radians(a)) * r)
            place(colour, normal, leaves[rng.randrange(len(leaves))], at, a, size * N * rng.uniform(.9, 1.1), shade * rng.uniform(.9, 1.05), rng)
    return colour, normal


tiles = [spray(31), spray(47), spray(59), rosette(71)]
atlas = np.zeros((2 * N, 2 * N, 4), np.float32); natlas = np.zeros((2 * N, 2 * N, 3), np.float32)
# Image rows run down; atlas v runs up: tile 0 (u0,v0) is the image's bottom-left.
for i, (cl, nm) in enumerate(tiles):
    u, v = i % 2, i // 2
    r0 = (1 - v) * N; c0 = u * N
    atlas[r0:r0 + N, c0:c0 + N] = cl; natlas[r0:r0 + N, c0:c0 + N] = nm
# Bleed colour into the transparent texels so mips don't fringe dark.
a = atlas[..., 3:4]
mean = (atlas[..., :3] * a).sum((0, 1)) / max(1., float(a.sum()))
atlas[..., :3] = np.where(a > .02, atlas[..., :3], mean)
natlas = np.where(a > .02, natlas, np.array([.5, .5, 1.], np.float32))
save(OUT / 'T_FallCrownLeaves.png', np.clip(atlas, 0, 1))
save(OUT / 'T_FallCrownLeaves_N.png', np.clip(natlas, 0, 1))
print(f'CHUCK_CROWN_LEAVES_READY coverage={float((a > .4).mean()):.2f}')
