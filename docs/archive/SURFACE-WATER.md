# Fountain and harbor water — October 4

Continues Claude's sewer-water pass (HANDOFF Update 68). That material and its generator stay unchanged. Unreal Engine 5.7.4, DX11/SM5 and the existing installed tools are retained; no installation or paid asset is needed.

## Assets and ownership

`Tools/create_surface_water_materials.py` owns exactly three new graphs: `/Game/Art/Materials/M_FountainBasinWater`, `M_FountainJetWater` and `M_HarborWater`. It reuses the three engine Water-plugin textures already copied by Claude into `/Game/Art/Textures/Water`; it does not copy them again or enable the Water plugin. Those textures are Unreal Engine content for use in Unreal projects, not public-domain art. The generated material packages use the existing Git LFS rules.

The basin and harbor use world-position XY coordinates with two animated wave normals in world space. This keeps the enormous harbor tiles from stretching their texture UVs and keeps their waves aligned across sheet boundaries. Harbor sheets are native surface grids with triangles at most 20m wide, instead of enormous scaled engine primitives; global texture streaming remains enabled. Lit translucency, shallow-edge fade and restrained intersection foam replace the old surfaces. Basin landing rings are anchored to the existing four jet endpoints. Existing thin jet paths retain their shape, with 24 traveling beads and 48 small staggered ballistic impact droplets. These are native instanced meshes, not Niagara emitters or an imported VFX pack. They have no collision. The two basin disks also use single surfaces to avoid stacked refraction from their former closed cylinders. Basin masonry and all playable routes are unchanged.

The harbor retains its seven noncolliding single sheets, including both genuine openings below the sewer grate and tavern pantry hatch. Waves are surface-normal detail, not displaced geometry, buoyancy or swimming. There is no new underwater gameplay. Initial closed slabs showed a visible band; huge engine planes then produced flat patches and a detailed narrow strip. Disabling global texture streaming did not fix that. Native surface grids removed the visible band and restored consistent waves in the reviewed views; a specific renderer/driver cause has not been established. No global streaming override is shipped.

## Ocean integration decision

`Tools/test_ocean_water_body.py` ran an isolated editor-only WaterBodyOcean/WaterZone configuration trial with `-EnablePlugins=Water -nullrhi`. It created a four-point ocean spline with the engine's ocean material, disabled landscape deformation, and saved `/Game/Review/WaterOceanTrial`. That map was moved to `Local/WaterOceanTrial.umap` after the trial and is ignored and not part of the explicit packaged map list. The project plugin config and playable map were not changed.

This was a configuration trial, **not a rendered ocean comparison or performance test**. Loading Water also reported the missing WaterBodyCollision profile. A continuous WaterBody ocean and its zone/spline material data would require integration around the two shaft exclusions. For this prototype the independent harbor material on existing cut surfaces is the lower-risk choice. It shares Water textures; it does not use the engine ocean parent shader or its simulation.

## Review and checks

`-ChuckWaterCapture` records eight fixed views in `Saved/Screenshots/Windows/SurfaceWater`, then exits: two low fountain views, two overhead basin views, two low harbor views and two elevated bay views. Each pair is four seconds apart to expose animated surface and splash changes. Do not combine this flag with other camera-capture modes.

Add `-ChuckWaterNight` to review the same views in the post-sewer night state.

The package verifier additionally requires `CHUCK_SURFACE_WATER_CHECK` (three loaded graphs, basin/jet batches without collision, 24 jet plus 48 splash droplets) and `CHUCK_HARBOR_WATER_CHECK` (seven correctly assigned noncolliding sheets, two uncovered shafts and six probes covered by exactly one sheet). Existing actual shaft, route, sewer, pantry, character and music checks remain mandatory. See the latest HANDOFF for completed build and visual evidence; code or an exit status alone is not proof of a rendered result.

Translucent reflections depend on the existing sky and screen-space rendering. Foam only appears where opaque geometry intersects the surface; it is not a simulated shoreline. Jet geometry and splash particles remain primitive, and reference-quality graphics are still unmet.

## Completed visual evidence

Dawn review: `Local/surface-water-grid-capture.log`, eight views retained in `Local/SurfaceWaterDawn`; committed stills are `SourceAssets/Setting/Review/runtime_FountainWater.png` (basin overhead) and `runtime_HarborWater.png` (bay elevated). Fixed water-only crops in the four-second pairs changed by 0.7067/255 RGB mean absolute difference for the basin, 12.4228 for the low harbor and 7.1543 for the elevated bay, supporting visible animated surfaces rather than frozen screenshots. These are image differences, not physical simulation or performance measurements.

The final review-only night transition was deferred until after scene construction, since sewer initialization resets its exit state. `Local/surface-water-publish-night.log` confirms `CHUCK_RETURN_CHECK failures=0 evening=1 hatch_closed=1 tavern_open=1 sun=0.378 sky=0.682`; the final fountain and bay night views were inspected. Night captures stay in the package's `SurfaceWater` screenshot folder. The dawn fountain remains pale at grazing angles because of sky reflection, and the thin segmented jets still read as primitive geometry. These remain visual limitations. See HANDOFF for exact final regression counts and publication.

The first rendered `-MotionCapture` run passed 137 checks (`Local/verify-package-20261004-103326.log`). After the review-only night timing correction, the abbreviated `-NoCapture` run ended with zero gameplay failures but only 128 passes, correctly rejected against its required minimum 135 (`Local/verify-package-20261004-104105.log`). Comparing the runs showed the no-capture dispatch omits the newer slide/return, zombie/wall-side, pantry and vault test block. Thresholds were not reduced. Use the full/default rendered verifier; the legacy abbreviated dispatch needs a separate tooling repair. The latest HANDOFF records the final full rendered verification of the published executable.
