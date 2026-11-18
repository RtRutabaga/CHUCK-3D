# Feywild kit: sources and licences

The user approved free CC0 scans for the Feywild on 2026-11-17 (docs/regions/FEYWILD.md, decision 4).
`manifest.json` records every file's URL, provider MD5, SHA-256 and size. Licence: https://polyhaven.com/license (CC0-1.0).

| Source | Authors | Use |
| --- | --- | --- |
| https://polyhaven.com/a/grass_medium_02 | Rico Cilliers | Grass along the path and shore |
| https://polyhaven.com/a/celandine_01 | Rob Tuytel, Rico Cilliers | Small ground flowers |
| https://polyhaven.com/a/moss_01 | Rob Tuytel | Fetched and imported; its clump is not planted in the look slice |
| https://polyhaven.com/a/brown_mud_leaves_01 | Rob Tuytel | The path's ground |

Shared with Chult (SourceAssets/Setting/ChultNature, ChultGorge): tree_bark_03, forest_ground_04, fern_02, rock_moss_set_01, the island_tree_02 leaves and cliff_side.

Everything else in `Derived/` is modelled by `Tools/build_feywild_kit.py` with no downloads: the fungi, bell-flower lamps, foxgloves, leaf plants, lily pads, lilies, petals, wisteria and the two oaks.

Pipeline, run one at a time:

1. `Tools/Fetch-Feywild.ps1` downloads the manifest's files and checks their hashes.
2. Blender 4.5.14 runs `Tools/build_feywild_kit.py`, which writes `Derived/`.
3. Unreal 5.7 runs `Tools/create_feywild_assets.py` with `-ExecutePythonScript`, which imports the kit and builds the materials in `/Game/Art/Feywild`.
