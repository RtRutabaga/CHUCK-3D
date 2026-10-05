"""Blender 4.5.14 (its bundled numpy): the old elf woman's grey brows.

blender --background --python Tools/build_elf_textures.py
  -> SourceAssets/NPCs/Humans/Textures/eyebrow010_grey.png

User 2026-10-05, "an elderly elf woman ... long braided gray hair": her hair
(build_npc_humans.make_hair) is tinted silver, but MakeHuman's eyebrow010 is
near-black and the card material can only multiply it by a tint, so it is
re-coloured here: each texel's own lightness (its hair-to-hair shading,
kept) re-mapped from dark steel to a warm ivory grey. Alpha is untouched.
humans.json points the ElfElder at it (brow_texture);
Tools/build_npc_humans.py copies it like any other texture.
"""
from pathlib import Path
import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
DATA = Path(bpy.utils.user_resource('EXTENSIONS')) / '.user/user_default/mpfb/data'
OUT = ROOT / 'SourceAssets/NPCs/Humans/Textures'


def load(path):
    img = bpy.data.images.load(str(path))
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32); img.pixels.foreach_get(px)
    return px.reshape(h, w, 4)


def save(px, path):
    h, w, _ = px.shape
    img = bpy.data.images.new(path.stem, w, h, alpha=True)
    img.pixels.foreach_set(np.clip(px, 0, 1).astype(np.float32).ravel())
    img.filepath_raw = str(path); img.file_format = 'PNG'; img.save()


def silver(px, dark, light):
    """Each opaque texel's lightness, normalised over the opaque texels, onto dark..light (display values)."""
    rgb, a = px[..., :3], px[..., 3]
    lum = rgb @ np.array([.2126, .7152, .0722], np.float32)
    seen = lum[a > .5]
    lo, hi = np.percentile(seen, 2), np.percentile(seen, 98)
    t = np.clip((lum - lo) / max(1e-4, hi - lo), 0, 1)[..., None]
    out = np.array(dark, np.float32) * (1 - t) + np.array(light, np.float32) * t
    return np.concatenate([np.clip(out, 0, 1), a[..., None]], axis=-1)


OUT.mkdir(parents=True, exist_ok=True)
brow = load(DATA / 'eyebrows/eyebrow010/eyebrow010.png')
save(silver(brow, (.36, .36, .36), (.74, .73, .71)), OUT / 'eyebrow010_grey.png')
print('CHUCK_ELF_TEXTURES_READY')
