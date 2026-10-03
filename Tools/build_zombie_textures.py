"""Blender 4.5.14 (its bundled numpy): the sewer zombie's decayed skin and clouded eyes.

blender --background --python Tools/build_zombie_textures.py
  -> SourceAssets/NPCs/Humans/Textures/zombie_skin_diffuse.png, zombie_eye.png

From the CC0 MakeHuman textures it is built on (MPFB's old_caucasian_male skin,
the grey eye): user 2026-10-03, "he should look more decayed like a zombie
that's been around for awhile". The skin is drained to a grey-green, mottled
with large dark patches of rot and bruising, laced with dark veins, and broken
by a scatter of open sores; the eye sockets (located on this UV layout's face)
are sunken dark; the mouth parts go a dead purple-grey. The eyes cloud over
milky. Seeded, so it rebuilds the same. humans.json points the Zombie at these
(skin_texture / eye_texture); Tools/build_npc_humans.py copies them like any
other texture.
"""
from pathlib import Path
import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
DATA = Path(bpy.utils.user_resource('EXTENSIONS')) / '.user/user_default/mpfb/data'
OUT = ROOT / 'SourceAssets/NPCs/Humans/Textures'
SKIN = DATA / 'skins/old_caucasian_male'
EYE = DATA / 'eyes/materials/grey_eye.png'
RNG = np.random.default_rng(20261003)


def load(path):
    img = bpy.data.images.load(str(path))
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32); img.pixels.foreach_get(px)
    return img, px.reshape(h, w, 4)


def save(px, path):
    h, w, _ = px.shape
    img = bpy.data.images.new(path.stem, w, h, alpha=True)
    img.pixels.foreach_set(np.clip(px, 0, 1).astype(np.float32).ravel())
    img.filepath_raw = str(path); img.file_format = 'PNG'; img.save()


def noise(h, w, cells, octaves=4):
    """Smooth value noise (fBm) in 0..1: random grids upsampled bilinearly."""
    out = np.zeros((h, w), np.float32); amp, total = 1., 0.
    for o in range(octaves):
        n = cells * 2 ** o
        g = RNG.random((n + 1, n + 1)).astype(np.float32)
        y = np.linspace(0, n, h, dtype=np.float32); x = np.linspace(0, n, w, dtype=np.float32)
        y0 = np.minimum(y.astype(int), n - 1); x0 = np.minimum(x.astype(int), n - 1)
        fy = (y - y0)[:, None]; fx = (x - x0)[None, :]
        fy = fy * fy * (3 - 2 * fy); fx = fx * fx * (3 - 2 * fx)
        a = g[y0][:, x0]; b = g[y0][:, x0 + 1]; c = g[y0 + 1][:, x0]; d = g[y0 + 1][:, x0 + 1]
        out += amp * (a * (1 - fx) * (1 - fy) + b * fx * (1 - fy) + c * (1 - fx) * fy + d * fx * fy)
        total += amp; amp *= .5
    return out / total


def smooth(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def blend(px, colour, weight):
    w = weight[..., None]
    px[..., :3] = px[..., :3] * (1 - w) + np.asarray(colour, np.float32) * w


skin_png = next(p for p in SKIN.glob('*.png') if 'diffuse' in p.name or p.name.endswith('.png'))
_, px = load(skin_png)
h, w, _ = px.shape
rgb = px[..., :3]
lum = rgb @ np.array([.2126, .7152, .0722], np.float32)
# Mouth interior and lips (the saturated red parts of the layout) found before draining.
red = smooth(rgb[..., 0] - .5 * (rgb[..., 1] + rgb[..., 2]), .18, .35)
# Drained: mostly grey, a cold sickly green-yellow left in it, a little darker.
grey = lum[..., None] * np.array([.86, .92, .80], np.float32)
px[..., :3] = rgb * .25 + grey * .75
px[..., :3] *= .82
# Rot and bruising: broad dark green-brown patches, a few purple.
rot = smooth(noise(h, w, 5), .55, .8)
blend(px, lum[..., None] * np.array([.36, .38, .24], np.float32), rot * .75)
bruise = smooth(noise(h, w, 7), .62, .85)
blend(px, lum[..., None] * np.array([.42, .30, .40], np.float32), bruise * .6)
# Fine mottling everywhere.
px[..., :3] *= (.82 + .3 * noise(h, w, 40, 3))[..., None]
# Veins: thin dark lines where a warped noise crosses its middle.
v = noise(h, w, 18, 3) + .35 * noise(h, w, 60, 2)
veins = (1 - smooth(np.abs(v - .68), .0, .012)) * smooth(noise(h, w, 4), .45, .7)
blend(px, np.array([.10, .12, .13], np.float32), veins * .55)
# Open sores: irregular, clustered in a few places, a raw reddish rim round a dark wet middle.
yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
where = smooth(noise(h, w, 4), .55, .68)
grain = noise(h, w, 26, 3)
rim = smooth(grain, .55, .61) * where
blend(px, np.array([.30, .13, .10], np.float32), rim * .7)
blend(px, np.array([.07, .05, .04], np.float32), smooth(grain, .61, .67) * where * .9)
# Sunken eyes: the face sits on the right of this layout, chin to the right;
# its two eye sockets at about (0.83, 0.46) and (0.83, 0.55) of the image.
for ex, ey in ((.835, .455), (.835, .552)):
    d = np.sqrt(((xx / w - ex) / .028) ** 2 + ((yy / h - (1 - ey)) / .022) ** 2)   # Blender pixels run bottom-up
    blend(px, np.array([.10, .08, .09], np.float32), (1 - smooth(d, .4, 1.6)) * .7)
# Mouth parts: dead purple-grey.
blend(px, np.array([.24, .17, .20], np.float32), red * .8)
OUT.mkdir(parents=True, exist_ok=True)
save(px, OUT / 'zombie_skin_diffuse.png')

# Clouded eyes: the iris and pupil fogged over, a yellowed white.
_, ep = load(EYE)
el = ep[..., :3] @ np.array([.2126, .7152, .0722], np.float32)
ep[..., :3] = ep[..., :3] * .25 + el[..., None] * .2 + np.array([.62, .62, .55], np.float32) * .55
save(ep, OUT / 'zombie_eye.png')
print('CHUCK_ZOMBIE_TEXTURES_READY', skin_png.name, px.shape)
