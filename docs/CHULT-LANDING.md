# Chult landing setting

The user authorized this first setting pass on October 12, 2026: match the supplied fall-cutscene contact sheet, with a better brachiation layout farther into the jungle. The starting clearing is ground exploration. No swing grips, enemies, pickups or new character moves are part of this pass.

`ChultLanding.h/.cpp` reuses the already imported fall-set meshes and materials. It does not regenerate Blender sources or change binary assets. The near tree and palm placements, lower overhead crowns, floor material, sun direction and warm under-canopy light follow `FallCutscene.cpp`. The perimeter vegetation is reinterpreted to leave a winding walking loop. This is a first setting pass using the existing procedural art, not a claim that the art target has been reached.

## Integration contract for Claude

- `AChultLanding::FloorOrigin()` returns the existing `AFallCutscene::LandingSpot()`: **(400000, 2500000, 0) cm**. Floor top is **Z=0**, Chuck faces **+X / yaw 0**.
- `AChultLanding::Entrance()` returns **(400000, 2500000, 34.65) cm**, matching the current capsule's resting height. Keep the cinematic actor at its existing floor transform rather than moving the landing to a different region origin.
- `AChultLanding::Build()` creates collision, foliage and its own jungle lighting. The actor owns its components; it registers no static traversal grips. Do not overlay its global lighting with the fall stage's lights. For a shared cinematic set, consolidate the duplicated near scenery and lighting first.
- `EnterPreview(World)` deactivates Waterdeep actors' rendering/collision/environment and constructs the playable setting. It serves checkpoint entry and the completed cutscene handoff; call it only after the sequence has finished, since it hides the cinematic stage. The completed stage and cinematic camera are then destroyed.
- The 12 segment ground loop runs about 46 m into the jungle and returns through a second lane. The initial clearing and +X cinematic pullback lane remain open. Brachiation belongs in the next setting section, after this grounded introduction.
- The existing area-reset path now records Chult entry and returns deaths/resets to this entrance. It uses existing movement and animation. `ResetAtLocation` refills sanity and stamina, so it must not be used unchanged for the final cinematic handoff if state must carry through.
- The normal pantry fall and Escape-skip now enter playable Chult. The handoff restores collision, movement, input and Chuck's follow camera, retains sanity/stamina/cigarettes, and records the local entrance for respawn. The landing can also be explored through **Dev Checkpoints → Chult Landing**. Actual package verification is recorded in HANDOFF.

## Verification

`-ChuckChultTest` checks asset presence, floor traces every metre along the loop, sweeps Chuck's real capsule along its 12 straight route segments and checks local recovery after moving into the jungle. It captures `Chult/Landing_Rat.png`, `Landing_Elevated.png` and `Jungle_Loop.png`. `Verify-Package.ps1` requires these checks; `Verify-Menu.ps1` includes the real Chult checkpoint button.

These geometry checks do not establish controller comfort or complete physical walking of the loop. Actual results, capture review, build versions and launcher state are recorded in HANDOFF after testing.

## Current limits

The forest floor is flat and the inherited tree forms remain procedural. Trunks, understory palms and additional fallen timber have simple collision; solid buttress fins and flared trunk bases have a shared procedural collision mesh based on the original seeded root dimensions. Lianas and the cutscene's original fallen log remain visual. Broad foliage stays noncolliding so the camera can pass through it. Leaves/canopy sway at one quarter their earlier amplitude; bark has zero wind displacement. Lower foliage brightness/skylight and cooler volumetric depth begin adapting the October 12 reference direction in ART-DIRECTION.md; this is not reference-quality finished art. This first pass has no authored outer boundary or onward transition, ambience, brachiation, expedition props, enemies or golden cigarettes. Later spaces remain subject to the region plan.
