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

Material slots come from the Blender material names (`M_FallBark`, `M_FallLeaves`, `M_FallCanopy`). Vertex colour r is brightness, g is the deep/yellow-green blend and a is the wind weight. The generator still writes a dryness value in b, but it arrived non-zero on some leaves after FBX import (flat cream leaves), so the leaf material ignores it.

Size: about 5 MB of FBX/PNG here, plus the imported assets; covered by the LFS rules for `.fbx`, `.png` and `.uasset`.
