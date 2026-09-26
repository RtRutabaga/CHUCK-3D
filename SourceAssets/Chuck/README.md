# Chuck character form study

Authored in Blender 4.5.14 LTS (build 62c1db4208e8), from the user's supplied references. No external mesh, texture pack or original-game asset was used.

`Chuck.blend` is the editable source. `SM_ChuckBody.fbx` and `SM_ChuckFoot.fbx` are the Unreal import sources. Centimetres, Z up, nose toward +X, feet at ground Z=0. The body mesh reaches Z=65 at the top of the ears. The reusable foot is centered locally and placed/animated by the character code.

The form study contains a continuous tapered trunk and muzzle, inset eyes and ears, cheek tufts, whiskers, a curved tapered tail, separate paws, and an open purple jacket with lapels, cuffs, pockets, seams and fasteners. Runtime material slots use the stable gray, purple, dark and pink prototype materials. Authored vertex colors are not currently used by those materials.

Current geometry: body 87,064 triangles; foot 10,944 triangles (two instances in game). This is an unoptimized prototype, not final production topology. Procedural motion adds speed-matched feet, a small hip-pivot body lean, airborne foot tuck and landing compression. The body remains a rigid mesh: knees, arms and tail do not articulate. Full skinning, a skeletal animation set, groom/hair cards, cloth simulation, authored texture maps and LODs are future work. Do not claim this reaches the final BG3-quality target.

## Rebuild

Close Unreal first, then run from the repository root:

```powershell
powershell -NoProfile -File .\Tools\Build-ChuckAssets.ps1
powershell -NoProfile -File .\Tools\Build-Prototype.ps1 -Package
```

The first script generates the named Blender/FBX files and imports them with full Unreal editor scripting. UE 5.7.4's Interchange importer needs editor UI services; Python commandlet-only import crashed and is not the supported route here. Completion is checked in the import log rather than relying only on the editor exit code.

Generation replaces the named assets. Preserve any future manual sculpt edits in a separate source file or update the generator before rerunning it. The ordinary prototype build uses the committed Unreal character assets and does not require Blender.

Source .blend/.fbx and Unreal .uasset files use Git LFS. The new character asset set totals approximately 11.4 MB before LFS compression. Generated Blender backups, logs, caches and packaged output stay ignored. Blender is installed outside the repository.
