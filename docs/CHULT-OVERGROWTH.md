# Chult additive overgrowth and low mist

User authorized October 14, 2026: add vegetation/dressing toward
[Fantasy Tropical Forest — Muhammed Haroon Neyyathoor](https://kunjonart.artstation.com/projects/1Ne1JX),
remove no existing elements, add low fog and assess the light for yellow cast.
Reference images inspected directly from the artist's page; they are visual
direction only, not assets to redistribute or import into the game.

`ChultOvergrowth.cpp` owns a separate seeded additive layer. Existing tree,
fern, palm, puddle, stream, cliff, ruin and canopy builders retain their original
placements and random seeds. New colonies reuse all four scanned fern variants,
elephant ears, shrubs and six scanned rock variants. Foliage and outer gateway
creepers are noncolliding; added outcrops use their scanned mesh collision.
Additional scanned island trees supply irregular mid-story branching, using
their existing hidden trunk collision meshes.
Cull distances bound the new instanced vegetation's rendering cost.

Walking/cinematic lanes, liana feet, ruin traversal and stream channel are
reserved. Rocks and new trees stay outside the entire canopy reserve; short
ferns can grow underneath it, away from trunks, feet and walking lanes.
Rocks require a larger clearance than plants. Only authored plants and creepers
enter the outer gateway faces and ground pockets, away
from the stairwell, causeway, fork entry and jump platforms. No dialogue,
encounter, traversal or character changes are part of this pass.

The existing height/volumetric fog gains a ground layer; twelve bounded local
height fog pools follow the clearing, understory, ruins and stream. Uniform
volume scale follows UE's local-fog contract. The runtime local-fog near distance
is 30 cm, overriding UE's 20 m default; no other current setting uses local fog
volumes and this changes no saved user setting. Sunlight and inscattering become
cooler. These are runtime components, without new material or binary assets.

Builds use installed UE 5.7.4, a single compiler worker and `-SkipBaseAssets`.
The package verifier requires the added asset inventory and mist markers,
then retains the existing actual route/collision, ruin movement, canopy,
raptor, cutscene and general smoke checks. HANDOFF records actual results and
visual review; construction/log inventory alone does not establish art quality.
