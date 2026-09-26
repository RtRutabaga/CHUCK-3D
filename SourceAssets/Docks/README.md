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
