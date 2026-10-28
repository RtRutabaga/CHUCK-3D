"""Blender 4.5.14 (its bundled numpy): a tiling skin-detail normal map for the
humans' and heroes' skin (user 2026-10-28: "make all huminoid npc's look better,
in particular faces").

blender --background --python Tools/build_skin_detail.py
  -> SourceAssets/NPCs/Humans/Textures/skin_detail_normal.png

MakeHuman's skins are a colour map only, so close up a face is smooth plastic.
This is the fine relief the colour map lacks, tiled over the body's UVs many
times (M_HumanSkin / M_HeroSkin: DetailTile, DetailStrength): pores (small
dimples, scattered, a few larger), a criss-cross of fine lines and a faint
lumpy undulation. Tileable (built on a torus: every filter wraps). DirectX
normal (green down), as Unreal expects. Seeded: the same file every run.
"""
from pathlib import Path
import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets/NPCs/Humans/Textures/skin_detail_normal.png'
N = 1024
RNG = np.random.default_rng(20261028)


def blur(a, sigma):
    """Gaussian blur on the torus (FFT): wraps at the edges, so it tiles."""
    f = np.fft.fftfreq(N)
    k = np.exp(-2 * (np.pi * sigma) ** 2 * (f[:, None] ** 2 + f[None, :] ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(a) * k))


def pores(count, radius, depth):
    """Dimples: a sparse field of points blurred to the pore's size, normalised to `depth`."""
    field = np.zeros((N, N), np.float64)
    ys, xs = RNG.integers(0, N, count), RNG.integers(0, N, count)
    np.add.at(field, (ys, xs), RNG.uniform(.6, 1.4, count))
    d = blur(field, radius)
    return -d / d.max() * depth


def lines(angle, spacing, depth):
    """Fine lines at an angle, broken up so they read as skin, not a weave."""
    y, x = np.mgrid[0:N, 0:N] / N
    # Integer wave counts across the tile on both axes keep it tiling.
    kx, ky = np.round(np.cos(angle) * N / spacing), np.round(np.sin(angle) * N / spacing)
    phase = 2 * np.pi * (kx * x + ky * y) + 3. * blur(RNG.normal(0, 1, (N, N)), 18) * 8
    wave = np.abs(np.sin(phase)) ** 6
    breakup = np.clip(blur(RNG.normal(0, 1, (N, N)), 10) * 6 + .3, 0, 1)
    return -wave * breakup * depth


height = (pores(9000, 1.6, 1.) + pores(1400, 2.8, .8)
          + lines(.5, 26, .35) + lines(2.2, 31, .3)
          + blur(RNG.normal(0, 1, (N, N)), 22) * 40 * .25
          + blur(RNG.normal(0, 1, (N, N)), 1.2) * 1.5 * .12)
height -= height.mean()
# Normal from the wrapped gradient; `bump` sets how steep (DetailStrength scales it down further in the material).
bump = 3.2
gx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * .5 * bump
gy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * .5 * bump
n = np.stack([-gx, -gy, np.ones_like(gx)], -1)
n /= np.linalg.norm(n, axis=-1, keepdims=True)
# Image rows run bottom-up in Blender's pixel buffer; DirectX green is the opposite of OpenGL's.
rgb = n * .5 + .5
rgb[..., 1] = 1. - rgb[..., 1]
px = np.concatenate([rgb, np.ones((N, N, 1))], -1).astype(np.float32)
img = bpy.data.images.new(OUT.stem, N, N, alpha=False)
img.colorspace_settings.name = 'Non-Color'
img.pixels.foreach_set(px.ravel())
img.filepath_raw = str(OUT); img.file_format = 'PNG'; img.save()
flat = float((n[..., 2] > .995).mean())
print('CHUCK_SKIN_DETAIL', OUT.name, f'size={N}', f'flat={flat:.2f}', f'min_nz={float(n[..., 2].min()):.2f}')
