# Chult — jungle river beyond the bridge

Authorized by the user's October 15, 2026 request: expand beyond the bridge with dense jungle, a larger faster meandering river and a bigger waterfall into the bridge canyon. This is an addition to the approved Chult setting, using the Phase 4/5 exploration direction and falls map as read-only references. It does not advance the wreck, encounters or temple milestones.

The existing canyon frame is retained. Extend the far bank from S=11500 to 28000, T=±6500 (165 m farther upstream), also widening the existing far bank to the same T limits. A roughly 7.4–11 m wide river winds from the upstream forest to S=4500, T=2200, descending about 5 m before a broad roughly 49 m waterfall into the existing gorge. Retain the smaller opposite-bank tributary. Keep the bridge's T=0 crossing, grips, entry and dismount intact.

Layer scanned trees and palms over fern, shrub and broad-leaf understory. Reserve continuous walking space along both riverbanks and connect the bridge exit to the near riverbank. Water is shallow visual flow over a solid channel; use faster downstream UV motion, without introducing current forces or swimming. No new characters, dialogue, controls, pickups or transitions. Reuse existing assets and materials; no installations or binary regeneration.

Verification: sampled channel/riverbank floors, capsule sweeps along both banks, actual character walks across the old/new terrain seam and at a bend and canyon approach, cooked river/falls captures, existing bridge/canopy/Chult/package regressions. Promote only a verified candidate through Tools/Promote-Build.ps1; record actual results and limitations in HANDOFF.

Implementation: `ChultRiver.cpp/.h` shares the canyon ground-height function, carves the riverbed into both the inherited far bank and new floor, and reserves bank paths before scattering solid trees. The new river uses the existing stream material with 480 cm longitudinal UV units versus the creek's 230 cm: the same 0.12 UV/s scroll gives approximately 58 cm/s ripple travel, about 2.1 times the creek. River V decreases downstream for this shader; waterfall V increases downstream for its separate shader. This is visible surface motion, not a physical current simulation.

The broad waterfall samples the folded cliff profile independently across its width. Twenty-five component traces check that the actual canyon face remains behind the water. Terrain checks cover 693 channel/bank floors and 462 capsule sweeps; three real-character walks cover the inherited/new seam, an upstream bend and the downstream bank. Static checks and all three walks are required by `Verify-Package.ps1`. Cooked review views are `FarBankRiver.png`, `RiverMeanders.png` and `BroadWaterfall.png` under the package's Chult screenshot folder.

> **2026-10-15 (Claude):** the gorge walls, the plunge of this water into the gorge and the gorge mist are now [CHULT-GORGE.md](CHULT-GORGE.md). Floors, bridge and checks of the banks are unchanged.

> **2026-10-15 (Claude), upriver:** the far bank now runs on to S=64000 and the river no longer ends at the floor's edge: from S=28000 it swings toward +T and rises at a pool under a spring-fed fall at S=33800 ([CHULT-UPRIVER.md](CHULT-UPRIVER.md)). Below S=28000 its course, width, water and checks are unchanged.
