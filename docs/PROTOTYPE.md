# Waterdeep movement and camera milestone

Current scope follows the user's request to test fundamentals before adding interactions. One Windows PC scene: detailed primitive docks, tavern frontage, stationary human scale reference, barrel, crate, low step and bench. The presentation pass adds paving, plank nails/grain, timber and door details, roof tiles and distant nonplayable harbor silhouettes. Chuck uses an imported Blender form study with restrained procedural movement. No combat, dialogue, pickups, additional maps or finished art.

Chuck is a 65 cm gray rat with an oversized purple open jacket. His capsule is 65 cm tall and 30 cm wide; the human is 180 cm tall and tavern door 210 cm tall. Speed-matched footsteps, slight body lean, airborne foot tuck and landing compression affect visual components only. The rigid body still needs a skeletal rig for articulated limbs and tail. Use real scale rather than shrinking a human template visually.

Unreal 5.7.4 with a small C++ runtime module replaces the initial Blueprint-only suggestion. Dock geometry is generated at play time; Python scripts create the level/materials and import the Blender 4.5.14 character study. See SourceAssets/Chuck/README.md for the asset pipeline. No new dependencies are required for procedural animation.

## Camera comparison

- Elevated: perspective, 400 cm collision-tested boom, 48-degree downward angle, 65-degree FOV.
- Rat-height follow: 220 cm horizontal collision-tested boom, lens about 65 cm above the floor, 78-degree FOV; limited look up/down rotates the camera without lowering the boom beneath the floor.
- C / Xbox Y blends between views at the same character position. Both modes use the same movement, jumping and scene. Follow smoothing is capped at 8 cm; orbit rotation is damped. No motion blur. F / right-stick click recenters behind Chuck. Mouse sensitivity is reduced.

Follow the same route in [PLAYTEST.md](PLAYTEST.md) in both modes. Compare visibility near the barrel/crate, human scale, view of the tavern, landing readability at the pier gap, and comfort. Reset preserves the selected camera. Repeat with keyboard and Xbox controller. No final camera choice until the user plays and gives feedback.

## Acceptance

- Walk/turn/stop at rat scale using keyboard and Xbox mappings.
- Jump and land, collide with the world, and recover locally after falling.
- Read Chuck's gray body and purple jacket against human-scale placeholders.
- Both switchable camera modes run in the same Windows build.
- Controls and exact launch instructions are documented.
- Runtime checks and visual inspection are recorded honestly, with physical-controller verification distinguished from simulated input tests.
- Keep source and LFS assets in the separate public CHUCK-3D repository; public publication was authorized on 2026-09-24.

Camera decision: OPEN. This milestone exists for the user's comparison, not a commitment to either view.

When an obstacle forces the camera within 70 cm of Chuck, his proxy is hidden temporarily to keep the lens clear. It reappears when the camera has room. Tight-space framing still needs user feedback.
