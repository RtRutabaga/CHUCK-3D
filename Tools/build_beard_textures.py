"""Blender 4.5.14 (its bundled numpy): hair-lock textures for the beards' strand
cards and grey brows for the old sailor (user 2026-10-06: "for all npc's with
beards make the beards look better (also for the sailor make his hair color
match his beard)").

blender --background --python Tools/build_beard_textures.py
  -> SourceAssets/NPCs/Humans/Textures/beard_locks.png, eyebrow009_grey.png

beard_strands.png (Tools/build_dwarf_textures.py) is nearly opaque edge to
edge, so every card cut from it rendered as a solid rectangle. beard_locks is
LOCKS columns side by side, one lock each (rows bottom-up: V = 0 is the tip,
V = 1 the root): a clump of fine strands, widest at the root, gathering and
thinning to a few frayed hairs at the tip, fully clear at the column's sides.
A card takes exactly one column (build_npc_humans.lock_uv), so its outline is
the lock's, not the card's. Greyscale: the card material tints it. Seeded.

eyebrow009_grey re-maps MakeHuman's near-black eyebrow009 onto the sailor's
grey (the card material can only multiply, and black times any tint stays
black), as Tools/build_elf_textures.py does for the old elf.
"""
from pathlib import Path
import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
DATA = Path(bpy.utils.user_resource('EXTENSIONS')) / '.user/user_default/mpfb/data'
OUT = ROOT / 'SourceAssets/NPCs/Humans/Textures'
RNG = np.random.default_rng(20261006)
LOCKS = 8          # columns; build_npc_humans.LOCKS must match


def save(px, path):
    h, w, _ = px.shape
    img = bpy.data.images.new(path.stem, w, h, alpha=True)
    img.pixels.foreach_set(np.clip(px, 0, 1).astype(np.float32).ravel())
    img.filepath_raw = str(path); img.file_format = 'PNG'; img.save()
    bpy.data.images.remove(img)


def lock(w, h, strands):
    """One lock in a w x h column: (lightness, cover)."""
    lum = np.zeros((h, w), np.float32)
    cover = np.zeros((h, w), np.float32)
    v = np.arange(h, dtype=np.float32) / h
    centre = w / 2 + RNG.uniform(-.04, .04) * w
    sway_a, sway_p = RNG.uniform(.02, .06) * w, RNG.uniform(0, 6.28)
    for _ in range(strands):
        # Spread across the root, gathering toward the tip; ends scattered, most low.
        root = np.clip(RNG.normal(0, .21), -.44, .44) * w
        gather = RNG.uniform(.08, .35)
        tip = RNG.uniform(0, .75) ** 1.4
        width = RNG.uniform(.6, 1.4)
        bright = RNG.uniform(.35, 1.)
        wave_a, wave_f, wave_p = RNG.uniform(.3, 1.6), RNG.uniform(1., 3.), RNG.uniform(0, 6.28)
        rows = np.nonzero(v >= tip)[0]
        vv = v[rows]
        shape = gather + (1 - gather) * vv                      # 1 at the root, `gather` at the tip
        xs = centre + root * shape + sway_a * np.sin(vv * 2.4 + sway_p) * (1 - vv) + wave_a * np.sin(vv * wave_f * 6.28 + wave_p)
        fade = np.clip((vv - tip) / .1, 0, 1)                   # each strand fades out at its own end
        for off in range(-2, 3):
            col = np.floor(xs).astype(int) + off
            a = np.clip(1. - np.abs(col + .5 - xs) / width, 0, 1) * fade
            ok = (col >= 1) & (col < w - 1)                     # never to the column's edge
            r, c, a, rv = rows[ok], col[ok], a[ok], vv[ok]
            better = a > cover[r, c]
            cover[r[better], c[better]] = a[better]
            lum[r[better], c[better]] = bright * (.75 + .25 * rv[better])
    return lum, cover


def build_locks(w=64, h=512):
    lum = np.zeros((h, w * LOCKS), np.float32)
    alpha = np.zeros_like(lum)
    for k in range(LOCKS):
        l, c = lock(w, h, int(RNG.integers(70, 110)))
        lum[:, k * w:(k + 1) * w] = l
        # The root fades in too (V 0.82..1), so a card lying on the beard has no hard top edge (in game they read as scales).
        root = np.clip((1 - np.arange(h, dtype=np.float32) / h) / .18, 0, 1)[:, None] ** .8
        alpha[:, k * w:(k + 1) * w] = np.clip(c * 1.3, 0, 1) * root
    save(np.stack([lum, lum, lum, alpha], -1), OUT / 'beard_locks.png')
    print('CHUCK_BEARD_TEXTURE locks', f'cover={float((alpha > .33).mean()):.2f}')


def load(path):
    img = bpy.data.images.load(str(path))
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32); img.pixels.foreach_get(px)
    return px.reshape(h, w, 4)


def silver(px, dark, light):
    rgb, a = px[..., :3], px[..., 3]
    lum = rgb @ np.array([.2126, .7152, .0722], np.float32)
    seen = lum[a > .5]
    lo, hi = np.percentile(seen, 2), np.percentile(seen, 98)
    t = np.clip((lum - lo) / max(1e-4, hi - lo), 0, 1)[..., None]
    out = np.array(dark, np.float32) * (1 - t) + np.array(light, np.float32) * t
    return np.concatenate([np.clip(out, 0, 1), a[..., None]], axis=-1)


OUT.mkdir(parents=True, exist_ok=True)
build_locks()
save(silver(load(DATA / 'eyebrows/eyebrow009/eyebrow009.png'), (.3, .3, .29), (.62, .61, .58)), OUT / 'eyebrow009_grey.png')
print('CHUCK_BEARD_TEXTURES_READY')
