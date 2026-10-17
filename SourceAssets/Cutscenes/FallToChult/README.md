# Fall to Chult — cutscene set (Claude, 2026-10-12)

The jungle seen in the fall-to-Chult cutscene (`Unreal/Chuck3D/Source/Chuck3D/FallCutscene.cpp`): the canopy he crashes through and the clearing where he lands, dies his quiet astral death, returns and takes a drag. It is the cutscene's own set dressing on a stage 25 km from the docks, **not** the playable Chult setting, which is Codex's (docs/regions/CHULT.md). Since 2026-10-13 the cutscene's clearing **is** Codex's playable `AChultLanding` set: the same trees, understory, wet dressing and floor, built when the sky stage is built, with its lighting brought in at the canopy crash. Only the sky, the cloud deck and puffs, and the far canopy beyond the landing's 90 m forest remain the cutscene's own.

Everything here is procedural and seeded; no downloads or third-party art.

- `Tools/build_fall_jungle.py` (Blender 4.5.14, `blender --background --python`) writes the FBX meshes and PNG textures here:
  - **Meshes:** `SM_FallTree_A/B` (40 m and 32 m buttressed trees with crowns and lianas), `SM_FallPalm`, `SM_FallFern`, `SM_FallElephantEar`, `SM_FallShrub`, `SM_FallLog`, `SM_FallCrown` (a canopy crown for mass instancing) and `SM_FallLeafCard` (falling leaves).
  - **Textures:** `T_FallLeaves` (2×2 leaf atlas), `T_FallBark` (+`_N`, alpha = moss mask) and `T_FallGround` (+`_N`, leaf litter).
- `Tools/import_fall_cutscene.py` (Unreal 5.7.4 Python commandlet with `-AllowCommandletAudio`) imports them into `/Game/Art/Cutscenes/FallToChult`, together with the music and the fall sounds. It **rebuilds** its own materials each run (meshes keep their references):
  - `M_FallLeaves`: masked two-sided foliage.
  - `M_FallCanopy`: the opaque crown core.
  - `M_FallBark`, `M_FallGround` and `M_FallJungleFar` (the canopy from the air).
  - `M_FallCloudPuff`: mist puffs in the cloud deck.
  - `MI_FallClouds`: a project instance of the engine's `m_SimpleVolumetricCloud_Inst`, so the cooker takes it. The base material's textures are black placeholders, so it must parent the `_Inst`.

**Trees only (2026-10-13):** the trees' buttresses are now lobes of the trunk surface itself (`buttressed_trunk`): broad where they join the trunk, a thin rounded plank further out, fading into the bark at the top, with no seam or spike. The random draws are unchanged, so branches, crowns, lianas and `ChultRootProfiles.h` (the landing's root collision, `Tools/export_chult_root_profiles.py`, whose `FinEdge` mirrors the outer edge) still match. To rebuild just the trees without touching the textures, the other meshes or the materials (Codex's wet-forest finish lives in them):

```
blender --background --python Tools/build_fall_jungle.py -- --only SM_FallTree_A,SM_FallTree_B
UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_fall_trees.py -unattended -nosplash -NoLiveCoding
```

Material slots come from the Blender material names (`M_FallBark`, `M_FallLeaves`, `M_FallCanopy`). Vertex colour r is brightness, g is the deep/yellow-green blend and a is the wind weight. The generator still writes a dryness value in b, but it arrived non-zero on some leaves after FBX import (flat cream leaves), so the leaf material ignores it.

**Crowns (2026-10-17, user: the tree tops looked "fake overly round" and "glossy like plastic"):** a crown was one lumpy opaque ball inside an even shell of square procedural-leaf cards. `crown()` now builds five to eight irregular clumps over a broad, flattish umbrella, each a small dark core under leaf-spray cards that lie mostly up and out, with sprays hanging at the rims. The sprays are `T_FallCrownLeaves` (+`_N`), composited by `Tools/build_crown_leaves.py` (Blender, numpy) from the real leaves of Poly Haven's CC0 `island_tree_02` scan already in `SourceAssets/Setting/ChultNature` (nothing downloaded). They use a fourth slot, `M_FallCrownLeaves` (matte two-sided foliage, roughness .8). `M_FallCanopy` is darker and matte (.95), and the wet-forest `M_FallLeaves` (ferns, shrubs, palms, lianas) went from roughness .28–.55 / specular .42 (it glinted white in sun) to .78–.9 / .22. Each crown takes one draw from the tree's stream and uses its own, so the buttresses and `ChultRootProfiles.h` still match; the lianas' draws changed. Rebuild:

```
blender --background --python Tools/build_crown_leaves.py
blender --background --python Tools/build_fall_jungle.py -- --only SM_FallTree_A,SM_FallTree_B,SM_FallCrown
UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_fall_crowns.py -unattended -nosplash -NoLiveCoding
```

Size: about 5 MB of FBX/PNG here, plus the imported assets; covered by the LFS rules for `.fbx`, `.png` and `.uasset`.
