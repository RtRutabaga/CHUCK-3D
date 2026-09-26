# Chuck character form study

Authored in Blender 4.5.14 LTS (build 62c1db4208e8), from the user's supplied references. No external mesh, texture pack or original-game asset was used.

`Chuck.blend` is the editable source. `SK_ChuckBody.fbx` is the skinned body import source; `SM_ChuckFoot.fbx` supplies the two foot targets. `SM_ChuckBody.fbx` preserves the static form study. Centimetres, Z up, nose toward +X, feet at ground Z=0. The body reaches Z=65 at the top of the ears. FBX reflects Y on import, so left/right bone names are assigned for Unreal coordinates.

The form study contains a continuous tapered trunk and muzzle, inset eyes and ears, cheek tufts, whiskers, a curved tapered tail, separate paws, and an open purple jacket built as one continuous garment (see below), sleeves, cuffs, welt pockets, stitching and front zippers. Imported slots retain the blockout materials; runtime overrides now select the original fur, chest, jacket, skin, eye and metal art materials. See docs/GRAPHICS-PASS.md. Authored vertex colors are not currently used by those materials.

The body is 161,334 triangles; each foot is 10,944 triangles. This is unoptimized prototype topology. A 14-bone authored rig includes a root, head, paired thighs/shins and arms/forearms, and four tail bones. Source part membership supplies limb weights; neighboring tail weights blend smoothly. Unreal may add an armature root during import.

Runtime procedural posing drives the skeleton through a PoseableMeshComponent. Legs use two-bone inverse kinematics to follow the feet through walking, body lean and landing compression. Sleeves counter-swing and the tail follows a restrained, phase-delayed sway. No animation clips, retargeting dependency or Animation Blueprint is required. The feet are independent static meshes, and this is not terrain-aware foot planting. Shoulder and knee joins still use the form study's overlapping surfaces. Full production topology, authored animation clips, groom/hair cards, cloth, texture maps and LODs remain future work. This does not reach the final BG3-quality target.

## Rebuild

Close Unreal first, then run from the repository root:

```powershell
powershell -NoProfile -File .\Tools\Build-ChuckAssets.ps1
powershell -NoProfile -File .\Tools\Build-Prototype.ps1 -Package
```

The first script generates the named Blender/FBX files and imports static and skeletal assets with full Unreal editor scripting. UE 5.7.4's Interchange importer needs editor UI services; Python commandlet-only import crashed and is not the supported route here. Completion markers are required for both imports. Skeletal material assignments are explicitly saved after import; package tests check them again after loading from disk.

Generation replaces the named assets. Preserve any future manual sculpt edits in a separate source file or update the generator before rerunning it. The ordinary prototype build uses the committed Unreal character assets and does not require Blender.

Source .blend/.fbx and Unreal .uasset files use Git LFS. The rig adds roughly 13 MB of FBX/Unreal assets and enlarges the editable Blender source to roughly 16 MB. Remaining GitHub LFS account allowance is unknown; no paid storage purchase is authorized. Generated Blender backups, logs, caches and packaged output stay ignored. Blender remains installed outside the repository at the previously recorded version.

The sleeve geometry now uses tapered cross-sections and shallow gathered folds instead of ellipsoids. Shoulder/elbow joins remain overlapping prototype surfaces; they are not finished garment topology.

The head now uses a continuous tapered skull/muzzle surface, smaller nasal pad and outward-angled ears. Short tapered fur clusters sampled across the skull provide silhouette detail and inherit the head bone. The eyes, nose and mouth remain clear; shorter tufts now extend over the muzzle. Exposed chest, belly and legs also carry directional geometric tufts, and leg tufts inherit the matching thigh/shin bones. This is not a hair groom or finished face sculpt; ear height remains 65 cm.

## Jacket construction (2026-09-26)

The jacket shell, collar stand, collar fold and lapels are one grid generated from a single front-edge function (`jacket_edge_angle`/`jacket_point` in `Tools/build_chuck_model.py`). A Solidify modifier gives 0.4 cm cloth thickness; its rim closes every boundary, and the inner face uses the `Seam` slot as lining. Zipper teeth/tape, welt pockets, hem and back stitching are placed on that same surface function, so they cannot drift off the edge. The former floating lapel quads, front-seam tubes and fasteners were removed. The cream chest is a patch ray-cast onto the evaluated torso, raised at its centre and sunk below the surface at its border, instead of a separate protruding ellipsoid. Sleeves are still separate tubes overlapping the shell at the shoulder.

## Review and checks

```powershell
$B="$env:LOCALAPPDATA\Programs\CHUCK-Tools\blender-4.5.14-windows-x64\blender.exe"
& $B --background SourceAssets\Chuck\Chuck.blend --python SourceAssets\Chuck\check_model.py
& $B --background SourceAssets\Chuck\Chuck.blend --python SourceAssets\Chuck\review_renders.py -- <out_dir>
```

`check_model.py` verifies the 14 bone names/parents, 65 cm ear top, material slot names, bone-only vertex groups, per-vertex weight totals of 1 and a closed jacket shell. The closed-shell check does not detect gaps between separate parts; use the renders for that. `review_renders.py` renders Workbench front/side/rear/three-quarter/scale and jacket close-ups with an approximate neutral foot placement; runtime owns the real foot placement. `Review/before` and `Review/after` hold the 2026-09-26 comparison.
