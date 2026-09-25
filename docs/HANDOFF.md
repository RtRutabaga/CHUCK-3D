# Handoff — 2026-09-24

The first playable Waterdeep movement/camera milestone is complete locally. A second presentation/camera pass is now implemented. Launch `Launch-Prototype.cmd` from the repository root; it opens the packaged Windows build without the editor. See docs/PLAYTEST.md for controls and the comparison route. User feedback: the blockout is too rudimentary for a useful camera comparison. This pass improves visual presentation and camera behavior while keeping the same small docks scope; no camera has been selected.

## Presentation and camera pass

Added stone paving, plank grain and nails, mooring hardware, window/door timber details, roof slates, crate/barrel details and a distant harbor/boat silhouette with haze. These are procedural engine primitives, not finished artwork or Blender assets. Rounded jacket pieces replace the boxy shell; restrained foot movement gives walking a basic visual cue. Chuck remains 30.48 cm tall and silent. No new interactions or playable areas.

Both views remain available. Elevated boom is 340 cm at -48 degrees, 65-degree FOV. Rat-height boom is 145 cm, lens about 31 cm above the floor, 78-degree FOV. View changes blend, orbit is damped, positional follow lag is capped at 8 cm, and mouse sensitivity is reduced. F / Xbox right-stick click recenters behind Chuck. No motion blur or camera shake. Chuck is temporarily hidden when camera collision pulls the lens within 38 cm, preventing the head/jacket from blocking the view. Tight-space framing and subjective comfort still need playtesting.

## What runs

Unreal 5.7.4 (CL 51494982), C++ runtime scene built from primitives: quay and pier with a jumpable missing board, tavern frontage, 180 cm stationary human, barrel, crate, low step and bench. Chuck is a 30.48 cm gray rat with a purple open-jacket proxy. Walking, jumping, blocking collision, fall reset, elevated and rat-height cameras, keyboard/mouse and Xbox mappings are implemented. No dialogue, combat, pickups, finished artwork or additional maps.

The editor map is intentionally empty until Play: the game mode constructs the scene. Content contains one small map and 14 flat materials. Blender is deferred until custom assets. Exact installed compiler/SDK versions and the original hardware assessment are in docs/SETUP.md. Build scripts use one compilation action without UBA because this 16 GB machine encountered memory pressure. Close Unreal before building to avoid Live Coding conflicts.

## Verified

- Editor and game C++ targets compiled; Windows Development BuildCookRun completed with exit code 0.
- Final packaged game ran with DirectX 11 rendering at 1280 x 720. All 23 rendered runtime checks passed: spawn/scale, walking, jumping/landing, wall collision, falling reset, camera switching, pier collision, simulated keyboard/Xbox input, crossing the same pier gap in both cameras, camera retraction at the warehouse wall, settled camera blends, and recentering.
- Final presentation-pass captures were inspected in both camera modes. Paving and facade details render, Chuck remains visible in the clear comparison position, and the low view emphasizes the human-scale door. Foreground props can still occupy much of the rat-height view; this is an unresolved framing/comfort consideration, not a selected final camera. Earlier manual keyboard C switching worked before this pass.
- Physical Xbox hardware, long-session performance, and the user's full comfort/visibility route have NOT been verified. No final camera selection.
- Evidence stays in ignored Local/presentation-build.log, Local/presentation-smoke.log and Builds/Windows/Chuck3D/Saved/Screenshots/Windows/Scale_*.png.

## Repository boundaries

Original CHUCK-game remains strictly read-only. Its reference HEAD was 87585dd6efb3d9fb0a44dd33549fa521f17b6701. Only selected reference documents/excerpts were copied; References/PROVENANCE.md records hashes and origins. No original code or assets were copied. Historical 2D campaign instructions do not expand this prototype.

The separate public remote is https://github.com/RtRutabaga/CHUCK-3D. No deployment is configured. After being explicitly informed that the history contains copied reference documents and local source paths, the user authorized proceeding with the public GitHub push on 2026-09-24. That resolves the earlier publication approval hold. The main branch and all 15 LFS assets were pushed successfully. A fresh clone in ignored Local/remote-verification downloaded the assets, passed git lfs fsck, and matched the source map hash.

Unreal generated folders and packaged Builds are excluded. The 15 authored map/material assets total 78,711 bytes and use Git LFS; upload/download round-trip and integrity checks passed. Approximately 79 KB of assets were uploaded; remaining account allowance is still unknown. See docs/SETUP.md for the LFS assessment.

Missing original geography/player-progression supplements were not found; do not invent their contents.
