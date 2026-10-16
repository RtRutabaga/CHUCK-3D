# Chult — upriver beyond the bridge: the giant trees and the deep jungle

User request, October 15, 2026 (Claude): expand the playable area beyond where the path ended on the far side of the bridge, extend the ruined ancient road through it and push back the distant landscape as needed. Add brachiation trees taller than the existing ones: a T-rex will later make the high canopy the only way past it, and the golden cigarette's time-limited stamina lock will later be the way up, so for now nothing needs to reach them. Beyond the T-rex canopy zone, dense jungle, then the ancient temple (not built yet). New areas get new spawn points, and every spawn point gets a Dev Checkpoint on the title screen.

## Layout

Canyon frame (`ChultCanyonFrame`): S runs away from the gorge, T across it. The far bank's floor used to end at S=28000; it now runs to `ChultFarFloorEnd` = 64000 (360 m farther), T=±6500 as before. `InChultRiverExtension`, the gorge's floor reservation (`ChultGorge.cpp` `FloorEdge`) and so `AChultLanding::Contains` and the bridge's music zone all cover it.

| S (cm) | Stretch | What is there |
| --- | --- | --- |
| 28000–35500 | River head | The river swings toward +T and rises at `ChultRiverHead` (S=33800) in a rounded pool under the **source fall**: water over a flat bedrock shelf out of a rock face on the valley's +T side, 13.7 m into the pool. Riverside jungle continues. |
| 35500–52000 | Giant trees | Twelve giants (below) over open ground: thinned understory, taller scanned trees only out at the valley sides, mossy outcrops. The later T-rex territory. |
| 52000–64000 | Deep jungle | Dense scanned trees and palms, heavy understory, low mist. The road ends near S=63400, where the temple will stand. |

Ground is `ChultCanyonGroundZ` everywhere (the old base terrain, now `ChultCanyonBaseZ`, carried on), built by `ChultUpriver.cpp` in 18 chunks of 20 m with the same 1 m grid; its first row is the river valley's last, so the seam at S=28000 is exact. Under each giant it is level for 5.2 m round the trunk, blending back over 2.8 m (`ChultUpriverGroundZ`): trunks, buttresses and roots are built for flat ground at their foot.

## The giant trees (`ChultCanopy.cpp`, grove 1)

`ChultCanopy.cpp` now builds "groves". Grove 0 is the canopy by the ruins and its line, unchanged (same data, draws, names, logs and tests). Grove 1, the giants, uses the same code for trunks, buttresses, crotches, walking limbs, leaders, crowns, hand boughs, chains, spurs, checks and trials, selected by `ActiveGrove`; the public grove-0 API is unchanged and the giants have their own entry points (`BuildChultGiantCanopy`, `CheckChultGiantCanopy`, `StartChultGiantCanopyReview`, `ChultGiantTrees`). Each tree's trunk now stands at `Trunk.Z` (0 for grove 0, the far bank's ground for the giants); every height test was made relative to it.

- 12 trees at S=36600+1300·I, alternating 3.8 m either side of a gently winding line (13.5–17 m apart), crotches **15.1–16.9 m** up (the old grove: 6.5–7.6 m), girth 1.24–1.40, leaders 11.3–13.7 m above the crotch, buttresses 1.8 times as tall.
- 11 chains tree to tree (94 grips, the same 98 cm grip height over the limbs, 1.35 m spacing and 1.8 m branch gaps), 36 spurs (146 grips): 240 registered grips.
- Only the **last** tree has a liana (the way down at the far end). The first has none and the trunks are far beyond a wall run's three steps: the golden cigarette now provides the way up ([CHULT-GOLDEN-LEAF.md](CHULT-GOLDEN-LEAF.md)). The canopy can be tested now from the **Chult Giant Canopy (dev)** checkpoint.
- `InChultGiantReserve` is the line's 10 m corridor: tall trees and outcrops keep out of it, and understory keeps 6.5 m from every trunk.

## The road (`ChultPath.cpp`)

The far road's guide now continues from S=27600 along `ChultUpriverRoadT` to S=63400: easing off the river's swing toward its head, wandering up the valley, and among the giants keeping to the −T side of their line, 12.5 m off it, clear of every buttress and root, under their outer boughs. The route search, dry-ground, support and capsule rules are unchanged. The road walk test has three more real walks (across the old floor's end, among the giants, in the deep jungle).

## The vista

- The far side's detailed rim land (`ChultVista.cpp`) now reaches 120 m past the floor's end (was 24000 from the lip, now 71500), with collision within 50 m of a floor as before, and the coarse far sheet starts beyond that.
- On the far side, the plateau's rise and its ranges are measured from the playable floor rather than the gorge lip (`Ur` in `ChultGorge.cpp`), so the hills and mountains stand back from the new valley instead of rising out of it. At the gorge lip nothing changes: the gorge walls, rims, falls and the views from the bridge are as before (compared against the committed `GorgeFromBridgeNorth.png`).
- A separate random stream adds plateau forest and crowns beside and beyond the valley, so every earlier vista draw is kept.

## Spawn points, deaths and Dev Checkpoints

| Checkpoint | Point | A death there returns to |
| --- | --- | --- |
| Chult Landing | `AChultLanding::Entrance` | the landing |
| Chult Canopy Ruins | `ChultCanopyRespawn` (ruin slab) | the same |
| Chult Bridge | near-bank gorge return (`ChultBridgeSpawn(false)`) | the landing (as before) |
| Chult Far Bank | far-bank gorge return (`ChultBridgeSpawn(true)`) | the same |
| Chult Giant Trees | on the road at S=34600 (moved from 35400 with the T-rex, 10 m short of its stretch), facing up it | the same |
| Chult Giant Canopy (dev) | the first giant's crotch | Chult Giant Trees |
| Chult Deep Jungle | on the road at S=52900, facing up it | the same |

`ChultUpriverRespawn` (asked after the gorge rule): a death anywhere on the far bank, other than a fall into the gorge, now returns him to the spawn point of that stretch: the far bank by the bridge up to S≈34800, the giant trees up to S≈52300, then the deep jungle. Before, any far-bank death returned him to the landing across the gorge. The checkpoint page now has two columns (Waterdeep, Chult); `Verify-Menu.ps1` drives all 14 points.

## Verification

- `-ChuckChultTest`: `CHUCK_CHULT_UPRIVER` (build) and `CHUCK_CHULT_UPRIVER_CHECK`: floor heights and no hole at the seam, level ground round all 12 giants, both ground spawns on the floor with room to stand and their own death recovery, the death-to-spawn map (4 cases, including the gorge staying the bridge's), the source fall in front of its rock and never inside it, and the giants' static check (`CHUCK_CHULT_GIANTS_CHECK`: grip clearance, leap sweeps, standing limbs and crotches, the liana).
- `-ChuckChultTest -ChuckPathReview`: road check plus 9 real walks (`CHUCK_CHULT_PATH_TEST failures=0 trials=9`).
- `-ChuckChultTest -ChuckUpriverReview`: six cooked views to `Chult/Upriver*.png`.
- `-ChuckGiantCanopyTest`: 72 trials with the real character: catch, swing and step down at all 11 chains in order, down the last liana, death up a tree and under the line (both back at Chult Giant Trees, facing up the road), and out to the tip of each of the 36 spurs and back.
- `Verify-Package.ps1` requires all of these; `Verify-Menu.ps1` the 14 menu points.

## Later

- The T-rex: added the same day ([CHULT-TREX.md](CHULT-TREX.md)); its stretch is S 35600–50400, ending before the last giant's liana.
- The golden cigarette now provides the way up the first giant: [CHULT-GOLDEN-LEAF.md](CHULT-GOLDEN-LEAF.md).
- The temple: built the same day beyond S=64500 ([CHULT-TEMPLE.md](CHULT-TEMPLE.md)); the floor now ends at S=92000 and the road runs up its avenue.
