# Weathered timber sources

Downloaded October 5, 2026 from Poly Haven. Six **2048 × 2048 JPG** maps: diffuse, packed ambient occlusion / roughness / metallic, and DirectX normal for each scan.

- [Weathered Brown Planks](https://polyhaven.com/a/weathered_brown_planks): exposed siding on the existing timber workshops.
- [Rough Wood](https://polyhaven.com/a/rough_wood): existing beams, braces, piers, doors, barrels and individual crate boards.
- [Poly Haven license](https://polyhaven.com/license): **CC0-1.0**, including commercial use and redistribution. Credit is recorded here although CC0 does not require it. No paid assets, account, plugin or new installation.

`manifest.json` records exact URLs, file sizes and SHA-256 hashes. `Tools/Fetch-WeatheredTimber.ps1` can recover just these selected maps and verifies their hashes. Source images total **8,003,680 bytes** and are stored through the project's existing Git LFS rules. The metallic channel is supplied for provenance but deliberately unused: bare wood remains nonmetallic.

`SM_WeatheredDockCrate.fbx` is a board-mapped derivative of this project's `SourceAssets/Docks/SM_DockCrate.fbx`. `crate-derivation.json` records the original and derived hashes. Existing bevels, gaps, nails and topology remain. The October 6 shimmer fix recesses panels/lids 0.6 cm, moves rails inward and away from their top/bottom edges by 0.25 cm, and shortens front rails 1 cm to tuck their ends behind the corner battens. Outer bounds are unchanged. It removes exposed coplanar surfaces (84 overlapping face pairs before, zero after); the original world-aligned wood concealed this depth conflict, while independent board UVs made it visible. PCA identifies each connected board's long direction, so the grain follows individual boards and horizontal rails rather than the crate as a whole. The method also supports rotated boards if future crate sources include them. The original FBX and Blender source are preserved. Hidden collision proxies remain authoritative.

Rebuild only this derivative with existing Blender 4.5.14:

```powershell
& "$env:LOCALAPPDATA/Programs/CHUCK-Tools/blender-4.5.14-windows-x64/blender.exe" --background --python Tools/build_weathered_crate.py
```

Then run `Tools/import_weathered_timber.py` through the existing Unreal 5.7.4 editor's `-ExecutePythonScript` option. This script targets only the six timber textures, `M_Wood`, `M_WoodLight`, `M_AgedDockTimber`, the two crate materials and the derived crate mesh. It does not run the broad original material/prop generators or touch character/NPC assets. A full cook is required to update the packaged game.

For a crate-only repair, run `Tools/import_weathered_crate.py` through the editor instead of the broader texture/material importer. The Blender derivation now checks bounds and exposed same-facing coplanar intersections before exporting.
