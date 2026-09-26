# Handoff — 2026-09-25

## Current build

Launch `Launch-Prototype.cmd` from the repository root for the packaged Windows prototype. See docs/PLAYTEST.md for controls and the same-route camera comparison. Unreal 5.7.4 (CL 51494982), Blender 4.5.14 LTS (62c1db4208e8); exact toolchain and hardware details are in docs/SETUP.md. No new dependencies were installed for this motion pass.

One Waterdeep dock scene includes a quay, pier with a jumpable missing board, tavern frontage, 180 cm stationary human, barrel, crate, low step and bench. Paving, plank details, timber, roof slates and distant harbor silhouettes remain procedural primitives. The editor map is intentionally empty until Play constructs the scene.

Chuck is a 65 cm gray rat in an oversized purple open jacket, using the imported Blender form study. This supersedes the original one-foot target. The capsule is 65 cm tall and 30 cm wide. He remains silent. No dialogue, combat, pickups or additional maps have been added.

## Latest motion pass

Speed-matched alternating footsteps now lift and roll; the body leans slightly around the hips, feet tuck in the air, and landing produces a small compression that eases out. Movement against a wall does not advance gait phase. Reset clears the pose. Animation affects visual components only, leaving collision and camera anchors unchanged.

This is an interim procedural pass on static meshes, not a skeletal animation set. Knees, arms and tail remain rigid. Rigging and articulated locomotion are the next substantial character improvement. No groom, cloth simulation, texture maps or LODs yet. Runtime uses stable flat prototype materials; imported vertex colors are not used. See SourceAssets/Chuck/README.md for reproducible generation and import.

## Cameras

Both views remain available, with no final selection. Elevated: 400 cm boom, -48 degree pitch, 65 degree FOV. Rat-height: 220 cm boom, lens about 65 cm above the floor, 78 degree FOV. Switching blends, orbit is damped and positional lag is capped at 8 cm. F / Xbox right-stick click recenters. No camera shake or motion blur. Camera collision hides Chuck if the lens comes within 70 cm. Foreground props can still occupy much of the rat-height view; framing and comfort need user feedback.

## Verified in this pass

- Editor and game targets compiled; Windows Development BuildCookRun completed with exit code 0.
- Packaged DirectX 11 game ran at 1280 x 720 with all 28 rendered runtime checks passing (zero failures).
- Checks cover imported mesh/65 cm scale, walking lean, airborne foot tuck and settling after landing, jumping/landing, wall collision, camera retraction, fall reset, keyboard and simulated Xbox input, settled camera transitions, recentering and crossing the same pier gap in both cameras.
- Elevated and rat-height still captures inspected. These confirm framing, not subjective animation quality.
- Physical Xbox hardware, long-session performance and user comfort remain unverified. No final camera selection.
- Evidence is ignored: Local/motion-build.log, Local/motion-smoke.log and Builds/Windows/Chuck3D/Saved/Screenshots/Windows/Scale_*.png.

## Repository boundaries and publication

Original CHUCK-game remains strictly read-only. Reference HEAD: 87585dd6efb3d9fb0a44dd33549fa521f17b6701. Only selected reference documents/excerpts were copied; References/PROVENANCE.md records hashes and origins. No original code or assets were copied. Missing geography/player-progression supplements were not found; do not invent their contents.

Separate public remote: https://github.com/RtRutabaga/CHUCK-3D. No deployment is configured. The user explicitly authorized public publication of the reference documents, source paths, supplied JPG references and Blender/FBX/Unreal assets. The character study was published as 48a1573; a separate verification checkout downloaded LFS objects, passed git lfs fsck and matched source asset hashes. Earlier approval holds are resolved.

Authored binary assets use Git LFS; the character asset set is approximately 11.4 MB. Remaining remote account allowance is unknown. This procedural motion update introduces no binary assets. Generated Unreal output, packaged Builds, Local evidence and Blender backups stay excluded. Close Unreal before building to avoid Live Coding conflicts; build scripts use one compile action without UBA for this 16 GB machine.
