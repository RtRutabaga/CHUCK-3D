# Waterdeep movement and camera milestone

Current scope follows the user's request to test fundamentals before adding interactions. One Windows PC scene: primitive docks, tavern frontage, stationary human scale reference, barrel, crate, low step and bench. No combat, dialogue, pickups, additional maps or finished art.

Chuck is a 30.48 cm gray rat with an oversized purple open jacket built from primitives. His capsule is 30.48 cm tall and 14 cm wide; the human is 180 cm tall and tavern door 210 cm tall. Use real scale rather than shrinking a human template visually.

Unreal 5.7.4 with a small C++ runtime module replaces the initial Blueprint-only suggestion. Geometry is generated at play time; a Python editor script creates only the level and flat-color material assets. This makes the placeholder setup reproducible from source. Blender remains deferred until custom asset work.

## Camera comparison

- Elevated: perspective, 300 cm collision-tested boom, 55-degree downward angle, 60-degree FOV.
- Rat-height follow: 90 cm horizontal collision-tested boom, lens about 29 cm above the floor, 75-degree FOV; limited look up/down rotates the camera without lowering the boom beneath the floor.
- C / Xbox Y switches instantly at the same character position. Both modes use the same movement, jumping and scene. No camera lag or motion blur.

Follow the same route in [PLAYTEST.md](PLAYTEST.md) in both modes. Compare visibility near the barrel/crate, human scale, view of the tavern, landing readability at the pier gap, and comfort. Reset preserves the selected camera. Repeat with keyboard and Xbox controller. No final camera choice until the user plays and gives feedback.

## Acceptance

- Walk/turn/stop at rat scale using keyboard and Xbox mappings.
- Jump and land, collide with the world, and recover locally after falling.
- Read Chuck's gray body and purple jacket against human-scale placeholders.
- Both switchable camera modes run in the same Windows build.
- Controls and exact launch instructions are documented.
- Runtime checks and visual inspection are recorded honestly, with physical-controller verification distinguished from simulated input tests.
- Commit locally only; public GitHub push remains separately blocked pending reference-publication approval.

Camera decision: OPEN. This milestone exists for the user's comparison, not a commitment to either view.
