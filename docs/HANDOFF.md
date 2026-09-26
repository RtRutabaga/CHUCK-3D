# Handoff — 2026-09-25

## Current build

Launch `Launch-Prototype.cmd` from the repository root for the packaged Windows prototype. See docs/PLAYTEST.md for controls and the same-route camera comparison. Unreal 5.7.4 (CL 51494982), Blender 4.5.14 LTS (62c1db4208e8); exact toolchain and hardware details are in docs/SETUP.md. No new dependencies were installed for the material and daylight pass.

One Waterdeep dock scene includes a quay, pier with a jumpable missing board, tavern frontage, 180 cm stationary human, barrel, crate, low step and bench. Timber framing, roof slates and distant harbor silhouettes remain procedural primitives. Scanned stone and timber now supply surface detail. Barrel, crate and pier boards now render custom Blender meshes over the original hidden collision shapes. The editor map is intentionally empty until Play constructs the scene.

Chuck is a 65 cm gray rat in an oversized purple open jacket, using the imported Blender form study. This supersedes the original one-foot target. The capsule is 65 cm tall and 30 cm wide. He remains silent. No dialogue, combat, pickups or additional maps have been added.

## Latest skeletal motion pass

Speed-matched alternating footsteps now lift and roll; the body leans slightly around the hips, feet tuck in the air, and landing produces a small compression that eases out. Movement against a wall does not advance gait phase. Reset clears the pose. Animation affects visual components only, leaving collision and camera anchors unchanged.

Chuck now uses a skinned body with a 14-bone authored Blender rig (plus any imported armature root). Two-bone leg IK follows the foot targets while arms counter-swing and the four-bone tail sways with delayed gait phases. The head has a reserved bone but no facial animation. Motion is procedural, not a clip-based animation set. Feet remain independent static meshes; terrain-aware foot planting and production joint topology remain future work. No groom, cloth simulation, authored texture maps or LODs yet. Runtime now uses original procedural art materials for fur, chest, open purple jacket, skin, eyes and metal; imported vertex colors are not used. See SourceAssets/Chuck/README.md for reproducible generation and import.

## Material and daylight pass

Nineteen original procedural materials distinguish fur/cloth/skin from stone, plaster, timber, slate, metal and animated opaque water. Quay paving uses a consistent stone palette; sky gradient/clouds and the direct/ambient lighting balance replace the flat look. Existing distant warehouses gain pitched roofs, windows, chimneys and a harbor wall. No playable area or collision changes. See docs/GRAPHICS-PASS.md for the generator, limits and next art priorities. This is a step toward the supplied references, not BG3-level fidelity.

## Custom prop and sleeve pass

The barrel now has 20 bowed staves, inset lids, iron hoops and rivets; the crate has separate planks, rails, braces and bevelled edges; pier boards have bevels and recessed nails. Imported bounds were checked in centimetres. Hidden primitive collisions preserve the existing route and pier gap. Chuck's sleeves use tapered rings with shallow gathered folds, retaining the existing rig and scale. See SourceAssets/Docks/README.md. No new dependencies, interactions or maps.

## Rat anatomy and fur silhouette pass

Chuck now has a continuous tapered skull/muzzle, a smaller nasal pad, smaller outward-angled ears, dark fur eyelids and short tapered fur clusters over the back/sides of the skull. The 65 cm ear height and existing bones remain unchanged. This is geometric fur detail rather than a groom; no facial expression or dialogue system was added. The Windows package passed all 39 checks, and front/side captures were inspected.

## Cameras

Both views remain available, with no final selection. Elevated: 400 cm boom, -48 degree pitch, 65 degree FOV. Rat-height: 220 cm boom, lens about 65 cm above the floor, 78 degree FOV. Switching blends, orbit is damped and positional lag is capped at 8 cm. F / Xbox right-stick click recenters. No camera shake or motion blur. Camera collision hides Chuck if the lens comes within 70 cm. Foreground props can still occupy much of the rat-height view; framing and comfort need user feedback.

## Verified in this pass

- Editor and game targets compiled; Windows Development BuildCookRun completed with exit code 0. The final cook log was checked for material compilation failures and invalid shader maps; none remained.
- Packaged DirectX 11 game ran at 1280 x 720 with all 40 rendered runtime checks passing (zero failures).
- Checks cover three imported visual prop types, hidden barrel/crate collision, imported skeletal mesh/65 cm scale, required bones and hip axes, authored art material assignments including the purple jacket, articulated sleeve motion, walking lean, airborne foot tuck and settling after landing, jumping/landing, wall collision, camera retraction, fall reset, keyboard and simulated Xbox input, settled camera transitions, recentering and crossing the same pier gap in both cameras.
- Elevated and rat-height still captures inspected. Walking captures use the same clear quay route in both modes. Still captures confirm pose and framing, not subjective animation quality.
- Physical Xbox hardware, long-session performance and user comfort remain unverified. No final camera selection.
- Evidence is ignored: Local/worker-build.log, Local/worker-smoke.log and Builds/Windows/Chuck3D/Saved/Screenshots/Windows/{Scale_*,Rig_Walk_*,Props_Barrel}.png.

## Repository boundaries and publication

Original CHUCK-game remains strictly read-only. Reference HEAD: 87585dd6efb3d9fb0a44dd33549fa521f17b6701. Only selected reference documents/excerpts were copied; References/PROVENANCE.md records hashes and origins. No original code or assets were copied. Missing geography/player-progression supplements were not found; do not invent their contents.

Separate public remote: https://github.com/RtRutabaga/CHUCK-3D. No deployment is configured. The user explicitly authorized public publication of the reference documents, source paths, supplied JPG references and Blender/FBX/Unreal assets. The character study was published as 48a1573; a separate verification checkout downloaded LFS objects, passed git lfs fsck and matched source asset hashes. Earlier approval holds are resolved.

Authored binary assets use Git LFS; the earlier static character asset set was approximately 11.4 MB. Remaining remote account allowance is unknown. The skeletal update adds about 13 MB of FBX/Unreal assets and enlarges the editable Blender source to about 16 MB. All new binary types are covered by LFS; remaining account allowance is unknown. Generated Unreal output, packaged Builds, Local evidence and Blender backups stay excluded. Close Unreal before building to avoid Live Coding conflicts; build scripts use one compile action without UBA for this 16 GB machine.

Standing publication permission: the user explicitly authorized future verified CHUCK-3D code, documentation and project-asset commits/pushes to this public repository. Do not ask again for routine updates within that scope. Paid services, destructive changes, sensitive data publication and the original CHUCK repository are outside that authorization.

The graphics pass adds 19 small material .uassets under Content/Art, covered by Git LFS. /Game/Art is explicitly included in cooking. The user-authorized original procedural source requires no external asset license or paid storage purchase; remaining LFS account allowance is still unknown.

The user requested continued graphics work until a usage limit blocks progress. Publish verified milestones under standing permission and continue within the existing scene/art scope. No recurring automation was requested.

## Scanned surface pass

Six 2K CC0 Poly Haven texture maps now drive stone and timber color, roughness, occlusion and normals. Provenance and hashes are in SourceAssets/Surfaces/PolyHaven. The decorative rectangular paving overlay was removed without changing quay collision. BuildCookRun passed with no material compile failures; the packaged game passed all 40 rendered checks. Elevated, rat-height and barrel captures were inspected. No new dependencies or purchases. Remaining remote LFS account allowance is unknown.

## Moored boat pass

The old hull sphere and mast shapes are replaced by an original Blender working boat with strakes, ribs, thwarts, furled canvas and rigging. A low rope coil sits beside a pier bollard. Both are noncolliding scenery; no new map, interaction or buoyancy. BuildCookRun passed and all 40 rendered checks passed. Harbor_RatHeight and Harbor_Elevated captures were inspected: the follow view reads the boat silhouette; the elevated view emphasizes nearby walking surfaces and does not frame the whole boat. Source/reproduction details are in SourceAssets/Docks/README.md. Published surface assets through 31be3fb were downloaded in Local/remote-verification, passed LFS fsck and matched sampled source/imported hashes.

## Clothed dock worker pass

An original static Blender human replaces the visible worker primitives: rounded boots, shaped trousers, work shirt/cuffs/collar, vest, belt, hands, cap and neutral face. Imported height is exactly 180 cm. Hidden old human geometry preserves collision. Canvas and Leather are two new original procedural materials. No NPC behavior, dialogue or animation system. BuildCookRun passed without material compile errors; all 40 rendered checks passed. Scale_RatHeight and Chuck_Front were inspected. The face and overlapping clothing topology remain a form study, not finished realism. Reproduce with Tools/Build-DockWorker.ps1 using committed art materials.
