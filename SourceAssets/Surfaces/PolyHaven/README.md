# Stone and timber sources

Downloaded 2026-09-25 from Poly Haven's official download server. These six 2048 x 2048 JPG maps are asset files, not website preview renders.

| Asset | Author | Physical tile | Source |
| --- | --- | --- | --- |
| Grey Stone Path | Amal Kumar | 1.8 m | https://polyhaven.com/a/grey_stone_path |
| Brown Planks 03 | Rob Tuytel | 1 m | https://polyhaven.com/a/brown_planks_03 |

License: CC0-1.0, permitting use, modification and redistribution. See https://polyhaven.com/license and https://creativecommons.org/publicdomain/zero/1.0/ . Attribution is retained for provenance even though CC0 does not require it. No purchase or account was needed.

`manifest.json` records each official download URL, size and SHA-256. `Tools/Fetch-SurfaceTextures.ps1` retrieves only these maps. `Tools/import_surface_textures.py` verifies hashes and imports diffuse, packed ambient occlusion/roughness/metallic (ARM), and DirectX normal textures. Source JPGs and imported Unreal assets use Git LFS.

World-space projection preserves scale across primitive geometry. Timber samples the interior of a board to avoid painting extra seams across the authored dock planks. Material tint and normal strength are adjusted for the scene. There is no displacement or geometry derived from these images. `Tools/Build-ArtMaterials.ps1` reapplies these surfaces after generating the original procedural material set, without network access.
