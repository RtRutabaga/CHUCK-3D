# Chult ruin scans

Poly Haven CC0-1.0 sources, selected and approved October 13, 2026:
- https://polyhaven.com/a/rock_wall_08 — gray worn masonry base.
- https://polyhaven.com/a/mossy_stone_wall — damp, mossy wall/joint detail.
- https://polyhaven.com/a/rabdentse_ruins_wall — dark, eroded ruin detail.
License: https://polyhaven.com/license (commercial use/modification permitted).

`Tools/Fetch-ChultRuins.ps1` acquires the exact nine 2K JPG diffuse, packed
AO/roughness/metal and DirectX normal maps. `manifest.json` preserves provider
URLs, bytes, MD5 and SHA-256. `Tools/import_chult_ruins.py` verifies/imports them
and creates only `/Game/Art/ChultRuins/M_RuinStone` and `M_RuinMoss` with
world-aligned gray scanned stone, scanned normal relief and irregular moss.
No original assets are overwritten or Blender meshes regenerated. Actual
rounded masonry is native procedural geometry with visible mesh collision.
Uses existing Unreal 5.7.4/Python editor tools; no software installation.

## Reproduction

Run `Tools/Fetch-ChultRuins.ps1`, then launch the existing editor with
`-ExecutePythonScript=C:/GameDev/CHUCK-3D/Tools/import_chult_ruins.py -unattended -nosplash -NoLiveCoding`.
Use the full editor initialization (no Python commandlet): texture import needs
Slate. Compression changes rebuild texture resources asynchronously; the importer
saves/flushes each asset before checking its 2048 x 2048 dimensions.
Import tools: Unreal 5.7.4 / CL 51494982, embedded Python 3.11.8. Independent
source-dimension validation used bundled Python 3.12.14 / Pillow; no installation.

Native masonry uses shared ring vertices, authored smooth normals and simple
perpendicular tangents. The materials use world-space normals, so expensive UV
tangent reconstruction is omitted. Final geometry is 174,080 triangles / 109,888
vertices in 544 blocks. `CHUCK_CHULT_RUINS_BUILD` records measured setup time;
the package verifier requires less than 10 seconds on the test machine.
