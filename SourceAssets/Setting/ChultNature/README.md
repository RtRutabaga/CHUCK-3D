# Chult nature kit — Poly Haven CC0

User authorized implementation on October 13, 2026 after the resource shortlist.
Sources are unmodified provider files, acquired through the official API/download
server. `manifest.json` records URLs, provider MD5, SHA-256, byte counts and 2K
resolution. License: https://polyhaven.com/license (CC0-1.0, commercial use and
modification permitted). No account, paid assets or new software installation.

| Source | Authors | Use |
| --- | --- | --- |
| https://polyhaven.com/a/tree_bark_03 | Rob Tuytel | Shared jungle bark; rigid trunks, roots and canopy limbs |
| https://polyhaven.com/a/forest_ground_04 | Rob Tuytel, Rico Cilliers | Exposed soil blended with existing damp litter and moss |
| https://polyhaven.com/a/fern_02 | Rico Cilliers, Rob Tuytel | Four noncolliding understory variants |
| https://polyhaven.com/a/rock_moss_set_01 | Kless Gyzen | Six scanned rocks; visible mesh supplies static collision |
| https://polyhaven.com/a/island_tree_02 | Rob Tuytel, Rico Cilliers | Limited background/mid-story trees, bark-only collision |

Pipeline, run sequentially:

1. `Tools/Fetch-ChultNature.ps1` downloads only manifest files and verifies hashes.
2. Blender **4.5.14 LTS / 62c1db4208e8** runs `Tools/prepare_chult_nature.py`.
   It derives only `Derived/` from the preserved scans. Fern meshes retain their
   geometry; rocks use 45% source triangles; the tree uses 7.5% of the provider's
   LOD1 mesh. UVs/material slots remain intact. Footprints are centred with the
   bottom at Z=0 and converted to centimetres. Exact output counts/dimensions and
   hashes are in `Derived/manifest.json`. The tree has **24,153 triangles**.
3. Unreal **5.7.4 / CL 51494982** runs `Tools/import_chult_nature.py` with
   `-ExecutePythonScript=<absolute script path> -unattended -nullrhi -nosplash
   -NoLiveCoding` (editor Python, not the Python commandlet: Interchange texture
   import attempts Slate access in the latter). Verify the
   `CHUCK_NATURE_IMPORTED` marker and absence of Python errors; exit code alone
   does not establish a successful editor script.
4. Build/cook/test a candidate and promote with `Tools/Promote-Build.ps1`.

Unreal destination: `/Game/Art/ChultNature`. Textures are 2048 square; normal maps
are DirectX, AO/roughness maps and alpha are linear. Foliage is masked/two-sided;
imported plants and trees are stationary, with no foliage collision.
Imported trees have an invisible query mesh extracted from their visible reduced
bark faces; leaves and branch cards never become character/camera blockers.
Rocks use their reduced visible mesh for complex static collision. Ferns are
instanced and distance culled (45–65 m); only 18 background trees are added.
Placement is seeded and excludes the walking/cinematic lanes and canopy tree
reserve. The landing and cutscene share this dressing through `AChultLanding`.

No original-game assets copied, authored Blender sources regenerated, character
materials changed, gameplay grips activated, or new region encounters added.
Existing climb faces, fork surfaces and paw proposals remain as authored.
Tall primary trees, repeated crowns and flat terrain still need refinement;
this does not establish that the supplied art target has been reached.

All JPG/PNG/FBX/BLEND/UASSET files use existing Git LFS rules; engine output and
candidate builds remain ignored. Storage totals and actual verification evidence
are recorded in `docs/HANDOFF.md` when the milestone is accepted.
