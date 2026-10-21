"""Blender 4.5.14: the temple snake (Claude, 2026-10-21; docs/CHULT-TEMPLE-SNAKES.md).

User: "Create and add the snakes enemies". The 2D game's Phase 6 snake chamber:
twenty small, quick snakes that one scratch kills. Built here, no downloads: a
1.8 m jungle pit viper (after the fer-de-lance: a heavy body, a thin neck, a broad
triangular head), big enough to take a rat - Chuck is 65 cm.

blender --background --factory-startup --python Tools/build_temple_snake.py [-- --review DIR]
  -> SourceAssets/Enemies/Snake/SK_TempleSnake.fbx    skinned, no clips (the runtime poses it along its path)
     SourceAssets/Enemies/Snake/T_SnakePattern.png     colour: olive-brown with dark, pale-edged saddles, a cream belly
     SourceAssets/Enemies/Snake/T_SnakeScales_N.png    keeled dorsal scales and wide belly scutes (normal map; tiles along the body)
     SourceAssets/Enemies/Snake/T_SnakeScales_C.png    their crevices (cavity)
     SourceAssets/Enemies/Snake/T_SnakeEye.png         an amber eye with a vertical pupil

Conventions as the dock rat (Tools/build_enemy_rat.py): centimetres (scene unit
0.01), facing +X, the same FBX settings. The snake lies straight along +X, its
tail tip at x=0 and its snout at x=L, belly on z=0.

- Body: a tube of rings every centimetre, its section a squarish loaf with a flat
  belly; the radius grows from the tail tip, swells mid-body, narrows to a neck
  and widens again into the head.
- Mouth: in front of the mouth corner the rings are cut along the mouth line on
  both sides, the lower part riding the jaw, so a gape opens a real mouth: a pink
  palate closes the upper part, a mouth floor the lower. Two fangs fold back along
  the palate; a forked tongue lies on the floor of the mouth.
- UVs: u along the body in centimetres / 200, v round it (0 on the spine, 0.5 the
  belly's middle); the materials tile the scale detail on top of the body pattern.
- Bones: 28 along the spine (s00 at the neck .. s27 near the tail tip), head, jaw
  (hinged at the mouth corner) and tongue; each body vertex blends between the two
  nearest spine bones.
"""
from pathlib import Path
import math
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets/Enemies/Snake'
OUT.mkdir(parents=True, exist_ok=True)
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
REVIEW = argv[argv.index('--review') + 1] if '--review' in argv else None

L = 180.0                     # cm, tail tip to snout
NECK, CORNER, SNOUT = 170.0, 173.6, 179.6   # the neck's narrowest, the mouth corner, the last ring
SPINE = 28                    # spine bones
SEG = 28                      # vertices round a ring (even)
MOUTH_J = 9                   # the mouth line on each side: this many segments up from the belly's middle (of SEG/2 per side)


def half_width(u):
    """Half the body's width at u (cm from the tail tip)."""
    if u < 30: return .25 + 1.6 * (u / 30) ** .8
    if u < 150: return 1.85 + 1.35 * math.sin(math.pi * (u - 30) / 130) + .3 * (u - 30) / 120
    if u < NECK: return FMath(2.4, 1.55, (u - 150) / (NECK - 150))
    if u < 174.5: return FMath(1.55, 2.75, smooth((u - NECK) / (174.5 - NECK)))   # the jaws' angle: the viper's broad head
    t = (u - 174.5) / (SNOUT - 174.5)
    return FMath(2.75, 1.0, t ** 1.4)


def half_height(u):
    w = half_width(u)
    if u < NECK: return .82 * w
    if u < 174.5: return FMath(.82 * 1.55, 1.55, smooth((u - NECK) / (174.5 - NECK)))
    return FMath(1.55, .75, (u - 174.5) / (SNOUT - 174.5))   # the head is flat, the snout lower still


def FMath(a, b, t): return a + (b - a) * max(0., min(1., t))
def smooth(t): t = max(0., min(1., t)); return t * t * (3 - 2 * t)


def centre(u):
    """Height of the body's axis: the belly is flattened (60 % of the half-height below the axis)."""
    return .6 * half_height(u)


def section(u, j):
    """Ring point j (0 = spine, SEG/2 = belly middle), relative to the axis: (y, z)."""
    th = 2 * math.pi * j / SEG
    w, h = half_width(u), half_height(u)
    s, c = math.sin(th), math.cos(th)
    n = 2.6   # squarish loaf
    y = w * math.copysign(abs(s) ** (2 / n), s)
    z = h * math.copysign(abs(c) ** (2 / n), c)
    if z < 0: z *= .6   # flat belly
    if u > 174.5 and z > 0: z *= FMath(1., .8, (u - 174.5) / 5)   # the snout slopes down
    return y, z


# -------------------------------------------------------------------- geometry
def build_mesh():
    us = [0.6] + [float(u) for u in range(1, int(SNOUT))] + [SNOUT]
    verts, uvs_v = [], []
    faces, face_uv, face_mat = [], [], []
    ring = []   # per ring: dict j -> vertex index (upper copy), and lower copies for the mouth lines
    jL, jR = SEG // 2 - MOUTH_J, SEG // 2 + MOUTH_J     # the mouth lines (left and right of the belly)
    lower = set(range(jL + 1, jR))                       # segments' vertices below the mouth line
    for u in us:
        mouth = u >= CORNER
        idx = {}
        for j in range(SEG):
            y, z = section(u, j)
            idx[j] = len(verts); verts.append(Vector((u, y, centre(u) + z)))
        if mouth:   # duplicate the mouth-line vertices: the copies ride the jaw
            for j in (jL, jR):
                idx[('low', j)] = len(verts); verts.append(verts[idx[j]].copy())
        ring.append(idx)
    jaw_verts = set()
    for u, r in zip(us, ring):
        if u < CORNER: continue
        for k, vi in r.items():
            if isinstance(k, tuple) or k in lower: jaw_verts.add(vi)

    def vid(r, j, low):
        if low and ('low', j) in ring[r]: return ring[r][('low', j)]
        return ring[r][j]
    for r in range(len(us) - 1):
        u0, u1 = us[r], us[r + 1]
        for j in range(SEG):
            j1 = (j + 1) % SEG
            # Segments under the mouth line (between jL and jR, belly side) use the low copies at the lines.
            low = jL <= j < jR
            a, b = vid(r, j, low), vid(r, j1, low)
            c, d = vid(r + 1, j1, low), vid(r + 1, j, low)
            faces.append((a, d, c, b))
            face_uv.append([(u0, j), (u1, j), (u1, j + 1), (u0, j + 1)])
            face_mat.append(0)
    # Palate and mouth floor across the mouth, from the corner to the snout.
    for r in range(len(us) - 1):
        if us[r] < CORNER: continue
        u0, u1 = us[r], us[r + 1]
        faces.append((ring[r][jL], ring[r][jR], ring[r + 1][jR], ring[r + 1][jL]))   # palate, facing down
        face_uv.append([(u0, 0), (u0, 1), (u1, 1), (u1, 0)]); face_mat.append(1)
        faces.append((ring[r][('low', jL)], ring[r + 1][('low', jL)], ring[r + 1][('low', jR)], ring[r][('low', jR)]))   # floor, facing up
        face_uv.append([(u0, 0), (u1, 0), (u1, 1), (u0, 1)]); face_mat.append(1)
    # Snout cap: the upper half to a tip, the lower half (the jaw's chin) to its own.
    last = len(us) - 1
    up_tip = len(verts); verts.append(Vector((SNOUT + .45, 0, centre(SNOUT) + .15)))
    low_tip = len(verts); verts.append(Vector((SNOUT + .3, 0, centre(SNOUT) - .45)))
    jaw_verts.add(low_tip)
    for j in range(SEG):
        j1 = (j + 1) % SEG
        low = jL <= j < jR
        a, b = vid(last, j, low), vid(last, j1, low)
        faces.append((a, b, low_tip if low else up_tip)); face_uv.append([(SNOUT, j), (SNOUT, j + 1), (L, j + .5)]); face_mat.append(0)
    # Tail tip cap.
    tail_tip = len(verts); verts.append(Vector((0., 0., centre(.6))))
    for j in range(SEG):
        j1 = (j + 1) % SEG
        faces.append((ring[0][j1], ring[0][j], tail_tip)); face_uv.append([(.6, j + 1), (.6, j), (0., j + .5)]); face_mat.append(0)
    # The palate closes the upper half between the mouth lines: the cap must not fan across it.
    me = bpy.data.meshes.new('SK_TempleSnake')
    me.from_pydata([tuple(v) for v in verts], [], faces)
    uv = me.uv_layers.new(name='UVMap')
    li = 0
    for p, fu, fm in zip(me.polygons, face_uv, face_mat):
        p.material_index = fm
        for k in range(p.loop_total):
            uu, jj = fu[k]
            uv.data[p.loop_start + k].uv = (uu / 200., jj / SEG)
    me.validate(); me.update()
    obj = bpy.data.objects.new('SK_TempleSnake', me)
    bpy.context.scene.collection.objects.link(obj)
    # Outward normals (the palate down, the floor up come from their winding).
    bpy.context.view_layer.objects.active = obj
    for p in me.polygons: p.use_smooth = True
    return obj, jaw_verts


def parts():
    """Eyes, fangs and the tongue: separate pieces, joined later."""
    pieces = []
    # Eyes: amber with a slit pupil (their own UV: a sphere map), set into the head's sides under a brow.
    for side in (1, -1):
        u = 176.0; w = half_width(u); c = centre(u) + .35 * half_height(u)
        bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=10, radius=.48, location=(u, side * (w - .4), c))
        e = bpy.context.object; e.rotation_euler = (0, 0, math.radians(90 * side)); e.name = f'Eye_{side}'
        e.scale = (1, 1, .85)
        pieces.append((e, 'Eye', 'head'))
        # The supraocular scale: a flattened lid over the eye, the viper's scowl.
        bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=6, radius=.6, location=(u - .05, side * (w - .5), c + .38))
        b = bpy.context.object; b.scale = (1.2, .7, .28); b.name = f'Brow_{side}'
        pieces.append((b, 'Skin', 'head'))
    # Fangs: folded back along the palate, swung down by the open jaw's pose in the runtime (head bone).
    for side in (1, -1):
        bm = bmesh.new()
        base = Vector((178.6, side * .7, centre(178.6) - .05))
        ring_n = 6
        pts = []
        for k in range(5):
            t = k / 4
            pts.append(base + Vector((-.75 * t, 0, -.25 * t - .25 * t * t)))
        rings = []
        for k, p in enumerate(pts):
            r = .14 * (1 - k / 4.5)
            rings.append([bm.verts.new(p + Vector((0, r * math.cos(2 * math.pi * i / ring_n), r * math.sin(2 * math.pi * i / ring_n) * .7 + 0))) for i in range(ring_n)])
        for k in range(len(rings) - 1):
            for i in range(ring_n):
                bm.faces.new((rings[k][i], rings[k][(i + 1) % ring_n], rings[k + 1][(i + 1) % ring_n], rings[k + 1][i]))
        tip = bm.verts.new(pts[-1] + Vector((-.08, 0, -.05)))
        for i in range(ring_n): bm.faces.new((rings[-1][i], rings[-1][(i + 1) % ring_n], tip))
        me = bpy.data.meshes.new(f'Fang_{side}'); bm.to_mesh(me); bm.free()
        f = bpy.data.objects.new(f'Fang_{side}', me); bpy.context.scene.collection.objects.link(f)
        pieces.append((f, 'Fang', 'head'))
    # Tongue: a thin black strip, forked, lying in the floor of the mouth (the runtime slides it out).
    bm = bmesh.new()
    x0, x1, fork = 171.5, 178.4, 176.9
    z = centre(176) - .55
    # The stem: rows of three (left, middle, right) so each prong can leave from its half.
    stem = []
    for k in range(9):
        x = x0 + (fork - x0) * k / 8
        stem.append([bm.verts.new((x, -.17, z)), bm.verts.new((x, 0., z)), bm.verts.new((x, .17, z))])
    for k in range(8):
        a0, a1 = stem[k], stem[k + 1]
        bm.faces.new((a0[0], a1[0], a1[1], a0[1])); bm.faces.new((a0[1], a1[1], a1[2], a0[2]))
    # The prongs: each from its half of the stem's end, spreading and thinning to a point.
    for sgn in (-1, 1):
        prev = [stem[-1][0], stem[-1][1]] if sgn < 0 else [stem[-1][1], stem[-1][2]]
        for m in range(1, 5):
            t = m / 4; x = fork + (x1 - fork) * t
            mid = sgn * (.085 + .28 * t); w = .085 * (1 - .7 * t)
            row = [bm.verts.new((x, mid - w, z)), bm.verts.new((x, mid + w, z))]
            bm.faces.new((prev[0], row[0], row[1], prev[1]))
            prev = row
    bmesh.ops.solidify(bm, geom=bm.faces[:], thickness=.06)
    me = bpy.data.meshes.new('Tongue'); bm.to_mesh(me); bm.free()
    t = bpy.data.objects.new('Tongue', me); bpy.context.scene.collection.objects.link(t)
    pieces.append((t, 'Tongue', 'tongue'))
    return pieces


# -------------------------------------------------------------------- textures
def save_png(name, rgba):
    h, w, _ = rgba.shape
    img = bpy.data.images.new(name, w, h, alpha=True)
    img.pixels.foreach_set(np.clip(rgba, 0, 1).astype(np.float32).ravel())
    img.filepath_raw = str(OUT / f'{name}.png'); img.file_format = 'PNG'; img.save(); bpy.data.images.remove(img)
    print('CHUCK_SNAKE_TEXTURE', name, w, h)


def textures():
    rng = np.random.default_rng(20261021)

    def noise(W, H, lo, hi, beta=1.):
        fx = np.fft.fftfreq(W)[None, :] * W; fy = np.fft.fftfreq(H)[:, None] * H
        r = np.sqrt(fx * fx + fy * fy)
        amp = np.where((r >= lo) & (r <= hi), 1. / np.maximum(r, 1.) ** beta, 0.)
        f = np.real(np.fft.ifft2(np.fft.fft2(rng.standard_normal((H, W))) * amp))
        f -= f.min(); return f / max(f.max(), 1e-6)

    # Body pattern (u: 0..200 cm across the width, v: round the body; v=0/1 the spine, .5 the belly).
    W, H = 2048, 512
    u = np.linspace(0, 200, W)[None, :].repeat(H, 0)
    v = np.linspace(0, 1, H)[:, None].repeat(W, 1)
    side = np.abs(((v + .5) % 1.) - .5) * 2          # 0 on the spine, 1 on the belly's middle
    base = np.stack([.30, .25, .15]) [:, None, None] * (0.85 + .3 * noise(W, H, 2, 30))[None]
    belly = np.stack([.62, .55, .34])[:, None, None] * (.9 + .15 * noise(W, H, 4, 40))[None]
    # Saddles: dark triangles from the spine down each side, their points meeting their mirror (the X/diamond of
    # the fer-de-lance), about every 9 cm, edged in a pale line; small dark spots between.
    period = 9.0
    ph = (u / period + .25 * noise(W, H, 1, 4)) % 1.
    dist = np.abs(ph - .5) * 2                         # 0 at a saddle's centre line
    reach = (1 - side / .62)                           # the saddle narrows toward the belly
    saddle = np.clip((reach * .9 - dist) * 6, 0, 1) * (side < .62)
    edge = np.clip(1 - np.abs(reach * .9 - dist - .06) * 14, 0, 1) * (side < .64) * .8
    spots = np.clip((noise(W, H, 40, 120) - .78) * 6, 0, 1) * (1 - saddle) * (side < .7)
    col = base * (1 - .62 * saddle - .35 * spots)[None] + np.stack([.2, .19, .12])[:, None, None] * edge[None]
    # The belly (between the ventral lines), with a smudge of the side's pattern at its edges.
    bmask = np.clip((side - .66) * 12, 0, 1)
    col = col * (1 - bmask)[None] + belly * bmask[None]
    # The head (last 9 cm): dark post-ocular stripe, pale lips, a plain crown.
    head = u > 171
    stripe = head & (np.abs(side - .36) < .07) & (u > 174.5)
    col[:, stripe] = np.array([.07, .055, .04])[:, None]
    lips = head & (side > .46) & (side < .66)
    col[:, lips] = np.array([.6, .55, .38])[:, None] * .95
    # Fade to darker at the tail tip.
    col *= (.7 + .3 * np.clip(u / 25, 0, 1))[None]
    rgb = np.moveaxis(col, 0, -1)
    save_png('T_SnakePattern', np.dstack([rgb, np.ones((H, W))]))

    # Scale relief, tiling along the body every 12 cm (u) and once round it (v):
    # 24 rows of keeled, overlapping dorsal scales, and wide belly scutes across the ventral band.
    W2, H2 = 1024, 512
    x = np.linspace(0, 1, W2, endpoint=False)[None, :].repeat(H2, 0)
    y = np.linspace(0, 1, H2, endpoint=False)[:, None].repeat(W2, 1)
    sideb = np.abs(((y + .5) % 1.) - .5) * 2
    rows, along = 26, 11                              # rows round, scales per tile along
    ry = y * rows; rx = x * along + (np.floor(ry) % 2) * .5   # staggered
    fx, fy = rx % 1., ry % 1.
    # Each scale: a rounded diamond rising toward its tip (the free edge points tail-ward... i.e. -u), a keel down its middle.
    dx = (fx - .5) * 2; dy = (fy - .5) * 2
    shape = 1 - (np.abs(dx) ** 1.6 + np.abs(dy) ** 1.6)
    hgt = np.clip(shape, 0, 1) ** .6 * (.55 + .45 * (1 - fx))   # overlapping: higher at the head-ward edge
    keel = np.clip(1 - np.abs(dy) * 7, 0, 1) * np.clip(shape * 3, 0, 1) * .35
    dorsal = hgt + keel
    # Belly scutes: one wide plate per 1.1 cm across the ventral band.
    sc = (x * 12 / 1.1) % 1.
    scute = np.clip(1 - np.abs(sc - .55) * 2.2, 0, 1) ** .5 * .9
    vb = np.clip((sideb - .7) * 14, 0, 1)
    height = dorsal * (1 - vb) + scute * vb
    height += .08 * noise(W2, H2, 20, 200)
    gx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 3.2
    gy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 3.2
    nrm = np.dstack([-gx, -gy, np.ones_like(height)]); nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
    save_png('T_SnakeScales_N', np.dstack([nrm * .5 + .5, np.ones_like(height)]))
    cavity = np.clip(1 - height * 1.4, 0, 1) ** 1.5
    save_png('T_SnakeScales_C', np.dstack([cavity, cavity, cavity, np.ones_like(height)]))

    # Eye: amber iris flecked darker, a vertical black slit, a dark rim (sphere UV: u round, v pole to pole).
    E = 256
    eu = np.linspace(0, 1, E)[None, :].repeat(E, 0); ev = np.linspace(0, 1, E)[:, None].repeat(E, 1)
    # The sphere faces +-Y (rotated); its front is near u=.75 on the default UV sphere.
    du = np.minimum(np.abs(eu - .75), 1 - np.abs(eu - .75)) * 2 * math.pi
    dv = (ev - .5) * math.pi
    r = np.sqrt(du * du + dv * dv)
    iris = np.stack([.62, .42, .1])[:, None, None] * (.75 + .5 * noise(E, E, 8, 60))[None]
    eye = iris * (r < 1.1)[None] + np.stack([.05, .04, .03])[:, None, None] * (r >= 1.1)[None]
    slit = (np.abs(du) < .12 * np.clip(1 - np.abs(dv) / .9, 0, 1) + .02) & (r < 1.0)
    eye[:, slit] = .01
    save_png('T_SnakeEye', np.dstack([np.moveaxis(eye, 0, -1), np.ones((E, E))]))


# -------------------------------------------------------------------- rig
def build():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
    for block in (bpy.data.meshes, bpy.data.armatures, bpy.data.materials):
        for item in list(block): block.remove(item)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = .01
    body, jaw_verts = build_mesh()
    mats = {}
    for n in ('Skin', 'Mouth', 'Eye', 'Fang', 'Tongue'):
        mats[n] = bpy.data.materials.new(n)
    body.data.materials.append(mats['Skin']); body.data.materials.append(mats['Mouth'])
    # Spine bones: s00 at the neck (u=166) back to s27 (u=6).
    spine_u = [166 - k * (160 / (SPINE - 1)) for k in range(SPINE)]
    arm_data = bpy.data.armatures.new('SnakeRig')
    rig = bpy.data.objects.new('SnakeRig', arm_data)
    scene.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    eb = {}
    root = arm_data.edit_bones.new('root'); root.head = (0, 0, 0); root.tail = (0, 4, 0); eb['root'] = root
    prev = root
    for k, u in enumerate(spine_u):
        b = arm_data.edit_bones.new(f's{k:02d}')
        b.head = (u, 0, centre(u)); b.tail = (u, 3, centre(u)); b.parent = prev; eb[b.name] = b; prev = b
    head = arm_data.edit_bones.new('head'); head.head = (NECK + .5, 0, centre(NECK)); head.tail = (NECK + .5, 3, centre(NECK)); head.parent = eb['s00']
    jaw = arm_data.edit_bones.new('jaw'); jaw.head = (CORNER - .4, 0, centre(CORNER) - .25 * half_height(CORNER)); jaw.tail = (CORNER - .4, 3, jaw.head.z); jaw.parent = head
    tongue = arm_data.edit_bones.new('tongue'); tongue.head = (171.5, 0, centre(176) - .55); tongue.tail = (171.5, 3, centre(176) - .55); tongue.parent = jaw
    bpy.ops.object.mode_set(mode='OBJECT')
    # Weights: body by u between the two nearest spine bones; the head and jaw; the parts on their bone.
    groups = {n: body.vertex_groups.new(name=n) for n in ['root'] + [f's{k:02d}' for k in range(SPINE)] + ['head', 'jaw', 'tongue']}
    for v in body.data.vertices:
        u = v.co.x
        if v.index in jaw_verts:
            groups['jaw'].add([v.index], 1., 'REPLACE'); continue
        hw = smooth((u - (NECK - 2)) / 4)        # the neck's last spine bone hands over to the head
        if hw > 0: groups['head'].add([v.index], hw, 'REPLACE')
        if hw >= 1: continue
        if u >= spine_u[0]:
            groups['s00'].add([v.index], 1 - hw, 'ADD'); continue
        if u <= spine_u[-1]:
            groups[f's{SPINE - 1:02d}'].add([v.index], 1 - hw, 'REPLACE'); continue
        step = spine_u[0] - spine_u[1]
        k = int((spine_u[0] - u) // step); t = (spine_u[k] - u) / step
        groups[f's{k:02d}'].add([v.index], (1 - t) * (1 - hw), 'ADD'); groups[f's{k + 1:02d}'].add([v.index], t * (1 - hw), 'ADD')
    # The lower head behind the mouth corner follows the jaw a little, so the gape opens smoothly.
    for v in body.data.vertices:
        if v.index in jaw_verts: continue
        if CORNER - 2.5 < v.co.x < CORNER and v.co.z < centre(v.co.x) - .2 * half_height(v.co.x):
            t = (v.co.x - (CORNER - 2.5)) / 2.5 * .7
            groups['jaw'].add([v.index], t, 'REPLACE'); groups['head'].add([v.index], 1 - t, 'REPLACE')
    for piece, mat, bone in parts():
        piece.data.materials.clear(); piece.data.materials.append(mats[mat])
        for p in piece.data.polygons: p.use_smooth = mat != 'Fang' or True
        vg = piece.vertex_groups.new(name=bone); vg.add([v.index for v in piece.data.vertices], 1., 'REPLACE')
        bpy.context.view_layer.objects.active = piece
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in bpy.context.selected_objects: o.select_set(False)
    for o in bpy.data.objects:
        if o.type == 'MESH': o.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()
    body.parent = rig
    m = body.modifiers.new('Armature', 'ARMATURE'); m.object = rig
    # Normals outward (the tube and caps), recalculated by mesh shape.
    bm = bmesh.new(); bm.from_mesh(body.data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(body.data); bm.free()
    tris = sum(len(p.vertices) - 2 for p in body.data.polygons)
    print('CHUCK_SNAKE_MESH', f'tris={tris}', f'verts={len(body.data.vertices)}', f'bones={len(arm_data.bones)}', f'length_cm={L}',
          f'max_width_cm={2 * max(half_width(u) for u in range(0, 180)):.1f}', f'jaw_verts={len(jaw_verts)}')
    return rig, body


FBX = dict(apply_unit_scale=True, axis_forward='-Y', axis_up='Z', add_leaf_bones=False,
           primary_bone_axis='Y', secondary_bone_axis='X', use_armature_deform_only=False,
           mesh_smooth_type='FACE')


def review(rig, body):
    import os
    s = bpy.context.scene
    s.render.engine = 'BLENDER_WORKBENCH'; s.display.shading.light = 'STUDIO'; s.display.shading.color_type = 'MATERIAL'
    s.render.resolution_x, s.render.resolution_y = 900, 500
    for n, c in (('Skin', (.35, .3, .18, 1)), ('Mouth', (.8, .45, .45, 1)), ('Eye', (.7, .5, .1, 1)), ('Fang', (.95, .95, .9, 1)), ('Tongue', (.05, .05, .05, 1))):
        bpy.data.materials[n].diffuse_color = c
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); s.collection.objects.link(cam); s.camera = cam
    for name, loc, look in (('side', (90, -260, 30), (90, 0, 3)), ('head', (186, -14, 9), (175, 0, 3)), ('top', (90, 0, 300), (90, 0, 0))):
        cam.location = loc
        d = Vector(look) - Vector(loc); cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
        cam.data.lens = 50 if name != 'head' else 40
        s.render.filepath = os.path.join(REVIEW, f'Snake_{name}.png'); bpy.ops.render.render(write_still=True)
    # The gape: open the jaw 60 degrees.
    bpy.context.view_layer.objects.active = rig; bpy.ops.object.mode_set(mode='POSE')
    rig.pose.bones['jaw'].rotation_mode = 'XYZ'; rig.pose.bones['jaw'].rotation_euler = (0, math.radians(45), 0)   # about the bone's own axis: the head's left-right
    bpy.ops.object.mode_set(mode='OBJECT')
    cam.location = (188, -12, 10); d = Vector((175, 0, 2)) - cam.location; cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    s.render.filepath = os.path.join(REVIEW, 'Snake_gape.png'); bpy.ops.render.render(write_still=True)
    rig.pose.bones['jaw'].rotation_euler = (0, 0, 0)


if __name__ == '__main__':
    rig, body = build()
    textures()
    if REVIEW: review(rig, body)
    for o in bpy.context.selected_objects: o.select_set(False)
    rig.select_set(True); body.select_set(True); bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SK_TempleSnake.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'}, bake_anim=False, **FBX)
    print('CHUCK_SNAKE_FBX', OUT / 'SK_TempleSnake.fbx')
