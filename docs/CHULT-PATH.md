# Chult — remnants of an ancient jungle road

Authorized by the user's October 15, 2026 request. Setting work within the approved Chult region, owned by Codex.

The old road starts at the escarpment behind the crash clearing, passes through the landing and existing grove/raptor territory, goes around the western bend of the creek, then follows its northern bank to the bridge. Its last approach and first departure align with the bridge's axis. There is an explicit gap across the gorge: the active broken bridge remains the sole crossing. Beyond it the road turns onto the broad river's negative-T bank and follows its meanders to the current upstream limit, leaving future expansion open.

Use Amal Kumar's CC0 Stone Pathway scan from Poly Haven, https://polyhaven.com/a/stone_pathway (license: https://polyhaven.com/license). Import only three 2K JPG maps and one dedicated material. Existing Git LFS rules cover JPG/uasset; the configured GitHub LFS remote is in use, although account quota is not exposed. Record bytes and hashes before committing. No installation or existing asset regeneration.

The road is roughly 2–3 m wide, surviving as separate embedded, chipped slab clusters with missing sections, soil-filled joints and moss. It is not an intact stone ribbon. Ground conformity and sparse coverage preserve the old terrain/collision contracts. Existing vegetation and encounters remain; route planning takes solid obstacles into account. No water crossings, new bridge deck, traversal, characters, dialogue or pickups.

Verify actual terrain support, the entire route's water clearance and real capsule clearance, paving vertices against terrain, bridge alignment and the gorge gap. Review landing, raptor area, approach, departure and river views. Build/cook, run existing package regression suites, promote with the standard tool and record actual evidence in HANDOFF.

## Implementation

`ChultPath.cpp` builds two independent road centre lines after terrain registration.
Authored guide points follow the existing clearing/grove lanes, loop west and
north of the creek, and meet the canyon frame at T=0. A bounded local A* search
detours around existing solid roots and trees; rounded corners and every shortcut
are rechecked against terrain, water and capsule clearance. No vegetation is removed.
The bridge's land ends stop at S=-380 and begin at S=5000; the far road then
follows the negative-T riverbank to S=27600, before the current floor's S=28000 limit.

Irregular chipped flagstones survive in three staggered lanes with long reclaimed
sections and individually missing slabs. The final cooked build has 991 slabs,
7,928 triangles and 305 reclaimed rows. Moss varies across and between fragments.
Their edges follow actual ground triangles, with tops 1.2–2.8 cm above the soil.
These thin visual remnants have no collision; the existing terrain remains the
walking surface and strict earlier floor-height contracts remain valid.

`CheckChultPath` is included in the ordinary Chult test. `-ChuckChultTest
-ChuckPathReview` adds six captures and six actual character walks. The package
verifier requires both the ordinary checks and this focused suite. Final actual
results, promotion and limitations are recorded in HANDOFF rather than implied
by the existence of these checks.

> **2026-10-15 (Claude), upriver:** the far road continues from S=27600 to S=63400 along `ChultUpriverRoadT`, past the river's head, beside the giant trees and through the deep jungle, with three more real walks ([CHULT-UPRIVER.md](CHULT-UPRIVER.md)). Its rules and the near road are unchanged.

> **2026-10-15 (Claude), temple:** the far road now ends at the foot of the temple's stair, straight up the avenue between the skull stakes ([CHULT-TEMPLE.md](CHULT-TEMPLE.md)); one more walk there.
