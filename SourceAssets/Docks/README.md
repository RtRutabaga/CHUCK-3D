# Dock prop geometry study

Original meshes authored by this project's Blender 4.5.14 generator. No asset pack or original CHUCK-game content was used. `DockProps.blend` contains editable geometry; the three FBXs are individual Unreal import sources. All use centimetres, Z up and origins at their collision centres.

| Mesh | Intended size (cm) | Features |
| --- | --- | --- |
| SM_DockBarrel | 62 × 62 × 90 | 20 bowed staves, inset plank lids, four iron hoops, rivets and lid rim |
| SM_DockCrate | 60 × 65 × 60 | Separate side/lid planks, corner battens, rails, diagonal braces, nails and bevelled edges |
| SM_DockPlank | 23 × 180 × 12 | Bevelled edges and two recessed nail heads |

Imported bounds measured approximately 61.97 × 61.97 × 90, 60 × 65.05 × 60, and 23 × 180 × 12 cm respectively. Tiny differences come from facets, bevels and nail heads. The importer checks dimensions within 0.6 cm.

The meshes render without collision. Hidden original primitive actors retain the established barrel/crate blocking shapes and pier-board collision, including the missing-board gap. This keeps the camera comparison and movement route consistent. The barrel remains a conservative cylindrical collider; its bowed visual sides narrow near the lid and base. These props have no interaction or breakable system.

## Regenerate

Close Unreal and run from the repository root:

```powershell
powershell -NoProfile -File .\Tools\Build-DockProps.ps1
```

The script uses the installed Blender and Unreal versions, generates only the named source assets, imports to `/Game/Art/Props`, assigns the authored art materials and checks bounds/completion. Preserve manual sculpt edits separately before regeneration. Ordinary game builds use the committed `.uasset` files and do not require Blender.

Files use Git LFS; Blender backups, logs and packaged output remain ignored. These are geometry studies, not production assets with authored UV textures, damage variants or LODs. Material grain currently follows the procedural surface mapping rather than individual stave UVs.

## Moored working boat

`HarborBoat.blend` and `SM_HarborBoat.fbx` contain an original six-metre open cargo boat: eight strakes per side, gunwales, rubbing strips, internal ribs, floor boards, thwarts, keel, rudder/tiller, mast, yard, furled canvas and standing rigging. Origin is at the waterline, placed at world Z -60. Imported complete bounds are about 628.1 x 222.4 x 494.2 cm, including rigging and underwater hull. The source mesh has 5,482 polygons before export triangulation. It replaces the old flattened sphere and mast primitives; it is nonplayable scenery, without buoyancy or sail simulation.

`SM_RopeCoil` is a low decorative coil beside a pier bollard, approximately 54.8 x 41 x 2.6 cm, with no collision. The source has 2,898 polygons. Regenerate both with `powershell -NoProfile -File .\Tools\Build-HarborBoat.ps1`. This uses the existing Blender and Unreal installations and performs no downloads. The importer checks centimetre bounds and assigns existing art materials. Timber uses the sourced world-projected texture rather than plank-specific UVs; canvas and rope currently share the procedural plaster material and remain a surface-study limitation.

## Stationary dock worker

`DockWorker.blend` / `SM_DockWorker.fbx` are an original static clothed human study, generated with the installed Blender 4.5.14. Boots, shaped trousers, shirt sleeves/cuffs/collar, vest fronts, belt, hands, neutral face and wool cap replace the primitive visible worker. Front is +X in the source and rotates -90 degrees around Z at runtime. Feet are at Z=0; imported height is exactly 180 cm, with bounds about 29.94 x 70.88 x 180 cm. Source has 55,188 polygons before FBX triangulation.

The worker remains stationary and noninteractive. Hidden original human primitives preserve collision; the new visible mesh has none. No dialogue, rig, idle animation, face animation or additional NPC was added. This is a geometry study with overlapping clothing pieces, not a production character or a claim of realistic facial fidelity.

Regenerate with `powershell -NoProfile -File .\Tools\Build-DockWorker.ps1`. The committed Canvas and Leather materials are required; `Tools/Build-ArtMaterials.ps1` can recreate them. The material generator's optional `CHUCK_ART_ONLY` environment variable accepts a comma-separated subset for incremental development. Without it, the full set is generated. No new tools or downloads are required.
