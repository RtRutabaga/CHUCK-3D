# Ancient jungle road source

Amal Kumar, **Stone Pathway**, Poly Haven: https://polyhaven.com/a/stone_pathway.
CC0-1.0, including commercial use and redistribution: https://polyhaven.com/license.
Original scan covers 2 metres. Three 2048px JPG maps, 8,955,064 bytes total.
`manifest.json` records exact provider URLs, byte counts, provider MD5 and local SHA-256.

Fetch: `Tools/Fetch-ChultPath.ps1`. Narrow Unreal import: `Tools/create_chult_path_material.py`.
Uses the existing Unreal 5.7.4 Python 3.11.8 environment; no installations.
Imported textures and the dedicated material live in `/Game/Art/ChultPath`.

JPG and uasset are covered by the repository's Git LFS rules. The configured
GitHub LFS endpoint is already used by this project; quota is not exposed.
Only these three source maps, their imported counterparts and one material are new binaries.
Road geometry is runtime source code: broken, embedded slab fragments with moss,
dry-bank routing and explicit bridge alignment. No continuous paved ribbon.
