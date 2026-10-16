"""Blender 4.5.14 (for its numpy): the Chult dinosaurs' shared skin detail (Claude, 2026-10-16).

blender --background --factory-startup --python Tools/build_dino_skin_textures.py
  -> SourceAssets/Enemies/Shared/T_DinoMottle.png   R: broad blotches, G: mottling, B: fine speckle, A: crease cavity (tileable)
     SourceAssets/Enemies/Shared/T_DinoFolds_N.png  skin folds across the body (tileable normal map)

Generated here (no downloads). M_DinoSkin (Tools/build_dino_skin.py) lays
these over each creature's own pattern and scales so the hide reads as dry,
weathered skin rather than smooth plastic: uneven colour, creases running
round the body (its UVs run u along the spine, v round it), dull crevices.
"""
from pathlib import Path
import math

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets/Enemies/Shared'
OUT.mkdir(parents=True, exist_ok=True)
rng = np.random.default_rng(20261016)


def save_png(name, rgba):
    h, w, _ = rgba.shape
    img = bpy.data.images.new(name, w, h, alpha=True)
    img.pixels.foreach_set(np.clip(rgba, 0, 1).astype(np.float32).ravel())
    img.filepath_raw = str(OUT / f'{name}.png'); img.file_format = 'PNG'; img.save()
    print('CHUCK_DINO_SKIN_TEXTURE', name, w, h)


def noise(n, lo, hi, beta=1.0, aniso=(1.0, 1.0)):
    """Tileable band-passed noise (FFT-filtered white noise), normalised to 0..1."""
    f = np.fft.fftfreq(n)
    fx, fy = np.meshgrid(f * aniso[0], f * aniso[1])
    r = np.sqrt(fx * fx + fy * fy) * n
    amp = np.where((r >= lo) & (r <= hi), 1.0 / np.maximum(r, 1.0) ** beta, 0.0)
    field = np.real(np.fft.ifft2(np.fft.fft2(rng.standard_normal((n, n))) * amp))
    field -= field.min(); field /= max(field.max(), 1e-6)
    return field


N = 1024
blotch = noise(N, 1, 6, 1.2)
blotch = np.clip((blotch - .5) * 1.8 + .5, 0, 1)
mottle = noise(N, 6, 40, 1.0)
speck = noise(N, 60, 300, .4)
speck = np.clip((speck - .5) * 2.2 + .5, 0, 1)

# Folds: a network of creases between irregular skin plates (Voronoi cell
# edges, the cells stretched round the body), plus a few broad folds round
# the body that come and go in patches.
y, x = np.mgrid[0:N, 0:N] / N          # x: u (along the body), y: v (round it)
CELLS = 30
jit = rng.uniform(.1, .9, (CELLS, CELLS, 2))
f1 = np.full((N, N), 9.); f2 = np.full((N, N), 9.)
warp = (noise(N, 2, 10, 1.0) - .5) * .5 / CELLS
for cy in range(CELLS):
    for cx in range(CELLS):
        px = (cx + jit[cy, cx, 0]) / CELLS; py = (cy + jit[cy, cx, 1]) / CELLS
        dx = (x + warp - px + .5) % 1 - .5; dy = ((y - warp - py + .5) % 1 - .5) * .7
        d = np.sqrt(dx * dx + dy * dy) * CELLS
        closer = d < f1
        f2 = np.where(closer, f1, np.minimum(f2, d)); f1 = np.where(closer, d, f1)
cracks = np.clip(1 - (f2 - f1) / .12, 0, 1) ** 2 * (.5 + .5 * noise(N, 2, 9, 1.0))
patch = np.clip((noise(N, 1, 5, 1.0) - .45) * 3, 0, 1)
fwarp = (noise(N, 1, 4, 1.0) - .5) * 2
creases = (1 - np.abs(np.sin(math.pi * (x * 11 + fwarp * .7)))) ** 8 * patch * .8
fine = (1 - np.abs(np.sin(math.pi * (y * 40 + (noise(N, 2, 10, 1.0) - .5) * 4 + x * 2)))) ** 12 * .25 * noise(N, 3, 12, 1.0)
height = -(cracks * .8 + creases + fine)   # creases cut in
gx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 3.0
gy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 3.0
nrm = np.dstack([-gx, -gy, np.ones_like(height)])
nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
save_png('T_DinoFolds_N', np.dstack([nrm * .5 + .5, np.ones_like(height)]))
# The crease depth as a cavity mask (dirt and dull skin in the creases): mottle's alpha.
cavity = np.clip(cracks * .8 + creases + fine, 0, 1)
save_png('T_DinoMottle', np.dstack([blotch, mottle, speck, cavity]))
print('CHUCK_DINO_SKIN_READY')
