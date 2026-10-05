"""Blender 4.5.14 (its bundled numpy): the plaza dwarf's riveted mail and beard textures
(user 2026-10-04: "a dwarf NPC by the smithy, wearing dwarven armor and holding a battle axe. Bearded.").

blender --background --python Tools/build_dwarf_textures.py
  -> SourceAssets/Surfaces/Armor/chainmail_{diff,arm,nor_dx}_2k.jpg (+ manifest.json)
     SourceAssets/NPCs/Humans/Textures/beard_mass.png, beard_strands.png

Mail: European 4-in-1, every ring through four others, seen from the front:
rings in rows half a ring apart, each row's rings tilted the other way so they
lie over and under their neighbours. One tile is 16 rings across (14.4 cm: 9 mm
rings of 1.6 mm wire); it repeats seamlessly. Laid out like the Poly Haven cloth
sets the humans' fabric material reads (Tools/import_npc_humans.py): a colour
map (only its brightness is used: weave), ARM (R ambient occlusion, G roughness,
B metallic) and a DirectX normal map.

Beard: greyscale hair, tinted per NPC by the card material. beard_mass is
opaque (the beard's body: dense strands along V, darker between), beard_strands
has alpha (cards of loose hair over it: dense at the root, V = 1, thinning to
frayed, uneven tips toward V = 0). Seeded, so it rebuilds the same.
"""
from pathlib import Path
import json
import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
ARMOR = ROOT / 'SourceAssets/Surfaces/Armor'
HAIR = ROOT / 'SourceAssets/NPCs/Humans/Textures'
RNG = np.random.default_rng(20261004)
MAIL_RINGS = 16            # rings across one tile
MAIL_TILE_CM = 14.4


def save(px, path, fmt):
    h, w, _ = px.shape
    img = bpy.data.images.new(path.stem, w, h, alpha=fmt == 'PNG')
    img.pixels.foreach_set(np.clip(px, 0, 1).astype(np.float32).ravel())
    img.filepath_raw = str(path); img.file_format = fmt
    bpy.context.scene.render.image_settings.quality = 92
    img.save()
    bpy.data.images.remove(img)


def mail(size=2048):
    """Height (0..1), and whether each pixel is on a ring."""
    s = size / MAIL_RINGS                       # ring pitch along a row (px)
    R, W = .56 * s, .1 * s                    # ring centre-line radius, wire radius
    y, x = np.mgrid[0:size, 0:size].astype(np.float32) + .5
    best = np.full((size, size), -1., np.float32)
    row_pitch = s / 2
    j0 = np.round(y / row_pitch)
    for dj in (-1, 0, 1):
        j = j0 + dj
        cy = j * row_pitch
        shift = np.where(j % 2 == 0, 0., s / 2)
        i0 = np.round((x - shift) / s)
        tilt = np.where(j % 2 == 0, 1., -1.)   # alternate rows lean the other way: over, under
        for di in (-1, 0, 1):
            cx = (i0 + di) * s + shift
            dx, dy = x - cx, (y - cy) / .62      # seen at a slant: a little squashed
            d = np.abs(np.sqrt(dx * dx + dy * dy) - R)
            on = d < W
            h = np.where(on, np.sqrt(np.clip(1 - (d / W) ** 2, 0, 1)) * .75 + .25 + tilt * dx / s * .22, -1.)
            best = np.maximum(best, h)
    ring = best > -.5
    return np.where(ring, np.clip(best, 0, 1.2) / 1.2, 0.), ring


def build_mail():
    ARMOR.mkdir(parents=True, exist_ok=True)
    h, ring = mail()
    size = h.shape[0]
    grain = RNG.normal(0, 1, (size // 8, size // 8)).astype(np.float32)
    grain = np.kron(grain, np.ones((8, 8), np.float32)) * .04   # forged unevenness, ring to ring
    lum = np.where(ring, .3 + .55 * h + grain, .02)
    lum += np.where(ring & (h > .82), (h - .82) * 1.4, 0.)    # worn bright on the crowns
    diff = np.stack([lum, lum, lum, np.ones_like(lum)], -1)
    ao = np.where(ring, .45 + .55 * h, .15)
    rough = np.where(ring, .42 - .12 * h + grain, .85)
    metal = np.where(ring, 1., 0.)
    arm = np.stack([ao, rough, metal, np.ones_like(ao)], -1)
    # Normal from the height field (pixel rows run bottom-up here: axis 0 is +V); DirectX: green is -V.
    k = 6.
    gy, gx = np.gradient(h * k)
    n = np.stack([-gx, -gy, np.ones_like(h)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    nor = np.stack([.5 + .5 * n[..., 0], .5 - .5 * n[..., 1], .5 + .5 * n[..., 2], np.ones_like(h)], -1)
    for name, px in (('diff', diff), ('arm', arm), ('nor_dx', nor)):
        save(px, ARMOR / f'chainmail_{name}_2k.jpg', 'JPEG')
    manifest = {'provider': 'generated', 'generator': 'Tools/build_dwarf_textures.py', 'license': 'project-original',
                'assets': [{'file': f'chainmail_{m}_2k.jpg', 'asset': 'chainmail', 'map': m, 'resolution': size,
                            'size_cm': MAIL_TILE_CM} for m in ('diff', 'arm', 'nor_dx')]}
    (ARMOR / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print('CHUCK_DWARF_TEXTURE chainmail', size, f'tile_cm={MAIL_TILE_CM}', f'ring_cover={ring.mean():.2f}')


def strands(w=512, h=1024, count=900, frayed=True):
    """Hair along V (rows bottom-up: row 0 is the tip end, V = 0)."""
    lum = np.zeros((h, w), np.float32)
    cover = np.zeros((h, w), np.float32)
    v = np.arange(h, dtype=np.float32) / h
    for _ in range(count):
        x0 = RNG.uniform(0, w)
        width = RNG.uniform(.7, 2.2)
        bright = RNG.uniform(.55, 1.)
        wave_a, wave_f, wave_p = RNG.uniform(1, 6), RNG.uniform(1.5, 4.), RNG.uniform(0, 6.28)
        tip = RNG.uniform(0., .35) if frayed else 0.            # where this strand ends (V)
        xs = x0 + wave_a * np.sin(v * wave_f * 6.28 + wave_p) + (1 - v) * RNG.uniform(-8, 8)
        taper = np.clip((v - tip) / .12, 0, 1) if frayed else np.ones_like(v)
        rows = np.nonzero(v >= tip)[0]
        for off in range(-4, 5):
            col = np.floor(xs[rows]).astype(int) + off
            a = np.clip(1. - np.abs(col + .5 - xs[rows]) / (width * np.maximum(.35, taper[rows])), 0, 1) * (.6 + .4 * taper[rows])
            cc = col % w
            better = a > cover[rows, cc]
            r, c2 = rows[better], cc[better]
            cover[r, c2] = a[better]
            lum[r, c2] = bright * (.8 + .2 * v[r])
    return lum, cover


def build_beard():
    HAIR.mkdir(parents=True, exist_ok=True)
    lum, cover = strands(count=1100, frayed=False)
    mass = np.clip(.32 + lum * cover * .68, 0, 1)                  # dark between strands, never see-through
    save(np.stack([mass, mass, mass, np.ones_like(mass)], -1), HAIR / 'beard_mass.png', 'PNG')
    lum, cover = strands(count=700, frayed=True)
    alpha = np.clip(cover * 1.6, 0, 1)
    save(np.stack([lum, lum, lum, alpha], -1), HAIR / 'beard_strands.png', 'PNG')
    print('CHUCK_DWARF_TEXTURE beard', f'strand_cover={float((alpha > .5).mean()):.2f}')


build_mail()
build_beard()
print('CHUCK_DWARF_TEXTURES_READY')
