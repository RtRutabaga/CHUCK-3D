# Handoff — 2026-09-24

The first playable Waterdeep movement/camera milestone is complete locally. Launch `Launch-Prototype.cmd` from the repository root; it opens the packaged Windows build without the editor. See docs/PLAYTEST.md for controls and the comparison route. User feedback: the blockout is too rudimentary for a useful camera comparison. The next pass should improve visual presentation and natural game-camera feel while keeping the same small docks scope; no camera has been selected.

## What runs

Unreal 5.7.4 (CL 51494982), C++ runtime scene built from primitives: quay and pier with a jumpable missing board, tavern frontage, 180 cm stationary human, barrel, crate, low step and bench. Chuck is a 30.48 cm gray rat with a purple open-jacket proxy. Walking, jumping, blocking collision, fall reset, elevated and rat-height cameras, keyboard/mouse and Xbox mappings are implemented. No dialogue, combat, pickups, finished artwork or additional maps.

The editor map is intentionally empty until Play: the game mode constructs the scene. Content contains one small map and 14 flat materials. Blender is deferred until custom assets. Exact installed compiler/SDK versions and the original hardware assessment are in docs/SETUP.md. Build scripts use one compilation action without UBA because this 16 GB machine encountered memory pressure. Close Unreal before building to avoid Live Coding conflicts.

## Verified

- Editor and game C++ targets compiled; Windows Development BuildCookRun completed with exit code 0.
- Final packaged game ran with DirectX 11 rendering at 1280 x 720. All 19 runtime checks passed: spawn/scale, walking, jumping/landing, wall collision, falling reset, camera switching, pier collision, simulated keyboard/Xbox input, and crossing the same pier gap in both cameras.
- Both rendered camera captures were inspected. Ambient lighting was corrected so the shaded human and tavern stay readable. Purple jacket and rat/human scale are visible. Earlier manual keyboard C switching also worked in the packaged window.
- Physical Xbox hardware, long-session performance, and the user's full comfort/visibility route have NOT been verified. No final camera selection.
- Evidence stays in ignored Local/package-console.txt, Local/packaged-rendered-smoke.log and Builds/Windows/Chuck3D/Saved/Screenshots/Windows/Scale_*.png.

## Repository boundaries

Original CHUCK-game remains strictly read-only. Its reference HEAD was 87585dd6efb3d9fb0a44dd33549fa521f17b6701. Only selected reference documents/excerpts were copied; References/PROVENANCE.md records hashes and origins. No original code or assets were copied. Historical 2D campaign instructions do not expand this prototype.

The separate public remote is https://github.com/RtRutabaga/CHUCK-3D. No deployment is configured. After being explicitly informed that the history contains copied reference documents and local source paths, the user authorized proceeding with the public GitHub push on 2026-09-24. That resolves the earlier publication approval hold.

Unreal generated folders and packaged Builds are excluded. The 15 authored map/material assets total 78,711 bytes and use Git LFS; remote account allowance and upload/download round-trip remain unverified. No GitHub binary storage has been consumed by these local assets. See docs/SETUP.md for the LFS assessment.

Missing original geography/player-progression supplements were not found; do not invent their contents.
