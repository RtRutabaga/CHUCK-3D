# Weathered dock timber — October 5, 2026

Use free scanned surfaces to make the existing timber feel old, repaired and exposed to coastal weather. Preserve the current layout, parkour routes and collision. This is a material/UV pass, not new buildings or a character change.

## Assets and appearance

- Poly Haven **Weathered Brown Planks**, 2K: washed brown/grey siding, knots, scraped surface and board seams on the existing aging-timber buildings (`M_AgedDockTimber`). Vertical wall boards; grain follows the board length.
- Poly Haven **Rough Wood**, 2K: rough, split grain on individual structural beams, door boards, braces, pier boards and barrel staves (`M_Wood`, `M_WoodLight`). Existing light/dark board choices retain a restrained difference.
- Crates use a board-mapped derivative of the existing mesh with separate long-axis mapping for each connected crate board/rail (`M_WeatheredCrate`, `M_WeatheredCrateLight`). Existing board gaps, bevels and nail heads remain geometry. The October 6 correction recesses the panels and rail joints to eliminate exposed coplanar faces while retaining the outer bounds. No change to hidden blockers or vault dimensions.
- Matte roughness, shallow normal detail, subtle colour variation and modest darkening within the lowest metre of outdoor timber. No shiny varnish, bright painted wood or wholesale green moss coverage.

The projected materials use local mesh positions and **vertex-interpolated instance transforms**. This aligns grain to a beam even when it is rotated or scaled in an instanced batch; it avoids a world-axis grain direction cutting across diagonal braces. Texture scale is in centimetres rather than stretching one image to cover a building. Siding uses 130 cm per scan tile; rough grain uses 90 cm. Normals are projected into world space; the UV-authored crates use tangent-space normals. The wood remains nonmetallic.

## Reproduction and ownership

Sources, CC0 license links, sizes, hashes and crate derivation: `SourceAssets/Surfaces/WeatheredTimber/README.md`, `manifest.json`, `crate-derivation.json`.

1. Recover/verify selected source maps with `Tools/Fetch-WeatheredTimber.ps1` if needed.
2. Rebuild only the derivative crate with `Tools/build_weathered_crate.py` in existing Blender 4.5.14 if needed. The original dock FBX/Blender files are read-only inputs for this step.
3. Run `Tools/import_weathered_timber.py` via Unreal 5.7.4 `-ExecutePythonScript` to import only this pass's textures/materials/mesh. Do not run the old broad art-material generator, which would overwrite these graph changes.
4. Full cook, package verification, visual review and promotion are required before the root launcher reflects the change. Latest actual results are in `HANDOFF.md`.

New files continue using the existing Git LFS JPG/FBX/uasset rules. Generated Unreal output, review captures and Windows packages remain excluded from Git. Keep Claude's unstaged human imports intact; this pass does not reimport or save those assets.

## Review

Launch the root `Launch-Prototype.cmd`, choose **New Game** / **Waterdeep**, and inspect the starting crates, pier, storehouse doors and aging timber workshops. Compare close rat-height and elevated views on the same route. **Waterdeep Night** offers the warm lamp-lit comparison and the tavern's wood.

For a developer capture, run the verified game with `-ChuckTimberCapture -windowed -ResX=1280 -ResY=720`. Four fixed views are written under `Chuck3D/Saved/Screenshots/Windows/Timber` and the process exits. The existing `-ChuckWorkshopCapture` is also useful for the storehouse and timber storefront.

Remaining limits: building bodies are still simple blockout shapes; their scanned cracks are surface detail, not carved openings. Most individual beams still have square geometric edges. This pass does not claim final asset quality or add destructible wood.

## October 6 — crate shimmer correction

The original crate has rails, panels and corner battens with coincident outward planes. World-aligned colour previously concealed their depth-buffer conflict; the new independent board UVs made it visible as flickering grain. The derivative now separates those surfaces. Its geometry check finds **84 exposed coplanar face intersections before, zero after**, with unchanged outer bounds. Texture resolution, normals, roughness, buildings and collision are retained.

`Tools/import_weathered_crate.py` reimports only the derivative mesh. `-ChuckCrateMotionCapture` captures 20 stationary views, then 30 near and 30 farther orbit views around the starting crate, under `Saved/Screenshots/Windows/CrateMotion`. Some requests can be dropped if frame timing is slow; compare matching frame names. Actual before/after evidence and final package checks are in the latest handoff.
