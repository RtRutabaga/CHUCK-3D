"""Blender 4.5.14 (its bundled numpy): Bobert's closed eyes (he's asleep, user
2026-10-05; GAME-BIBLE.md: he never wakes).

blender --background --python Tools/build_bobert_textures.py
  -> SourceAssets/NPCs/Humans/Textures/bobert_eye_closed.png

MakeHuman's eyes are balls under the lids with no lid rig to close, so the eye
texture becomes lid: every colour of the grey eye's layout is replaced with the
skin round his eye sockets (sampled from the old_caucasian_male skin he wears,
at the sockets Tools/build_zombie_textures.py located on this UV layout), with a
little shading so it reads as a rounded lid; the alpha (the cornea's clear
corner) is kept. humans.json points Bobert at it (eye_texture);
Tools/build_npc_humans.py copies it like any other texture.
"""
from pathlib import Path
import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
DATA = Path(bpy.utils.user_resource('EXTENSIONS')) / '.user/user_default/mpfb/data'
OUT = ROOT / 'SourceAssets/NPCs/Humans/Textures'
SKIN = DATA / 'skins/old_caucasian_male'
EYE = DATA / 'eyes/materials/grey_eye.png'


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


skin_png = next(p for p in SKIN.glob('*.png') if 'diffuse' in p.name or p.name.endswith('.png'))
sp = load(skin_png)
h, w, _ = sp.shape
yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
# The lids: a ring round each socket (the face on the right of the layout; Blender pixels run bottom-up).
ring = np.zeros((h, w), bool)
for ex, ey in ((.835, .455), (.835, .552)):
    d = np.sqrt(((xx / w - ex) / .028) ** 2 + ((yy / h - (1 - ey)) / .022) ** 2)
    ring |= (d > .7) & (d < 1.5)
lid = np.median(sp[ring][:, :3], axis=0)
ep = load(EYE)
eh, ew, _ = ep.shape
v = np.mgrid[0:eh, 0:ew][0].astype(np.float32) / eh
# Faint shading across the layout (a lid is a little darker toward its fold) and a skin-like grain.
rng = np.random.default_rng(20261005)
grain = 1. + .035 * (rng.random((eh, ew)).astype(np.float32) - .5)
shade = (.9 + .1 * np.sin(v * np.pi)) * grain
ep[..., :3] = lid[None, None, :] * shade[..., None] * .92
OUT.mkdir(parents=True, exist_ok=True)
save(ep, OUT / 'bobert_eye_closed.png')
print('CHUCK_BOBERT_TEXTURES_READY', skin_png.name, [round(float(c), 3) for c in lid])
