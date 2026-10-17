# Chult — the temple exterior

User request, October 15, 2026 (Claude): "build the temple exterior, similar to 2d game". Only the exterior: the interior (the 2D game's Phase 6 dungeon) is not built.

## The 2D direction

Read-only sources: `docs/development/PHASE-5.md` §11–12 (Chult Map 5, temple exterior) and `chuck/chuck/assets/maps/chult_temple.txt` with its legend in `src/world/tilemap.py`. The map is a Mesoamerican stepped pyramid: four talud-tablero terraces, a broad stair walled in by sloping balustrades that end in serpent heads at plaza level, a house with a dark doorway on the summit (the Phase 6 boundary) and a roof comb over it, eight skull stakes lining the approach avenue, four stelae, fallen ruin fragments, ferns and leaf litter, the plaza closed in by dense jungle. The phase notes ask for it to feel very large relative to Chuck, weathered and overgrown, with an obvious, accessible dark entrance, and a drop in pressure: no combat there.

## Layout (`ChultTemple.cpp`)

At the end of the upriver valley ([CHULT-UPRIVER.md](CHULT-UPRIVER.md)); the far bank's floor (`ChultFarFloorEnd`) now runs to S=92000. Canyon frame S toward the pyramid, T across.

| S (cm) | What is there |
| --- | --- |
| 64500 | The deep jungle opens onto a level plaza (S 64500–91500, |T| ≤ 56 m, blended over 15 m into the ground round it). The **Chult Temple** spawn stands at S=65200 on the avenue, facing the pyramid. |
| 66500–71900 | The avenue: the old road runs up its middle; four pairs of skull stakes 10.4 m apart, the skulls looking across it; two pairs of carved stelae with altars 15 m off it. |
| 72000–76800 | Plaza with fallen carved blocks and ferns thickening toward the jungle. |
| 74550–74750 | The two serpent heads at the balustrades' feet, jaws open toward the avenue. |
| 74812–77312 | The stair: 125 steps (16 cm risers, 20 cm treads, 6 m wide, 39°) between balustrades 1 m above its noses. |
| 76000–80400 | The pyramid: 44 m square, four terraces of 5 m (a 3.5 m sloping talud, then a 1.5 m tablero: a plastered panel divided into framed bays by stone ribs between two stone bands; the band's mossy top is a 2.9 m walkable ledge). The summit platform is 17.8 m across, 20 m up. |
| 77750–78850 | The summit house: a dark doorway (2.4 × 2.6 m) into a dark vestibule, a corniced roof with a plaster frieze, a pierced roof comb behind; glyphs over the lintel. Top of the comb about 30 m above the plaza. |

Growth: ferns and shrubs on every ledge (the summit mostly bare), small trees clinging to two upper corners, hanging moss over the tablero edges, mist round the foot. The jungle round the plaza and behind the pyramid is `ChultUpriver.cpp`'s, from its own random stream (the earlier jungle keeps its draws); `InChultTempleReserve` keeps it off the avenue, plaza and pyramid.

Materials, all existing: the ruins' world-aligned `M_RuinStone`/`M_RuinMoss`, `M_WeatheredPlaster` (panels, frieze, glyphs, fangs), `M_Plaster` (skulls), `M_Dark` (doorway, throats, eyes), `M_FallBark` (stakes), `M_ChultHangingMoss`. No new assets or installations.

## Walking

The pyramid, balustrades, house, stakes, stelae and blocks are solid. Chuck steps only 4 cm, so the visible steps have no collision; a hidden ramp through the middle of each tread is the walking surface (39°, within his 45° limit), flush with the plaza at the foot and the platform at the top. The terraces' ledges and the balustrades' tops are walkable too. The doorway admits him into the vestibule (3 m deep); nothing lies beyond yet.

## Spawn, deaths, checkpoint

`EChultUpriverSpawn::Temple` / `ChultTempleSpawn()`. A death on the far bank from S≈64400 on returns him to it (before it, the deep jungle's). Dev Checkpoints has **Chult Temple**; `Verify-Menu.ps1` drives all 15 points.

## Verification

- `-ChuckChultTest`: `CHUCK_CHULT_TEMPLE` (build: 4 tiers, 125 steps, 8 stakes, 2 serpents, 4 stelae, missing assets 0) and `CHUCK_CHULT_TEMPLE_CHECK`: the plaza level at 207 probes, the stair's surface under each tread line and a capsule sweep up it (69 probes), the vestibule's standing room, an open avenue and summit-to-door path, the spawn.
- `-ChuckChultTest -ChuckTempleReview`: six cooked views (`Chult/Temple*.png`), then the real character runs from the plaza up the stair to the summit and into the doorway (`CHUCK_CHULT_TEMPLE_TEST failures=0 climb=1 legs=3`).
- The road gains a walk along the avenue (10 walks).

## The mega temple (October 16)

User request, October 16, 2026 (Claude), with a reference image: "double the current pyramid temple and have both connected to this more SE Asian styled temple to create one mega temple like this reference image. Have the area with the temple being reclaimed by the jungle, the back of the temple completely inaccessible due to vegetation, but still a clear path remnant leading to the temple".

This supersedes the single-pyramid layout above where they differ; the avenue, stakes, stelae, spawn and Phase 6 notes still hold.

- **Twin pyramids:** the October 15 pyramid, unchanged in form, now stands twice, centred at T=±52 m, fronts in line at S=76000. Each keeps its 125-step stair, serpent balustrades, summit house, dark vestibule and roof comb; both stairs are climbable.
- **Central temple (Khmer style):** three talud-tablero terraces (5 m each, 15 m total) between them. The lowest (65 m wide) runs into both pyramids' sides so that its top meets their first ledges: one joined, walkable mass. A 94-step stair, 8 m wide with serpent (naga) balustrades, climbs the middle to the sanctuary: a 9 m square cella whose real doorway opens into a dark vestibule (the Phase 6 way in is now this one) under a nine-storey stepped conical spire, 44 m above the plaza. Four corner towers stand on the top terrace and four on the second; two squat gate pavilions flank the stair's foot. Every spire storey has a pale plaster band and corner/face antefixes; there are false doors on the towers and the sanctuary's sides.
- **Wider precinct:** from S=62000 the far floor widens to |T|≤105 m (`ChultTemplePrecinctFrom`/`ChultTemplePrecinctHalf` in `ChultRiver.h`; `InChultRiverExtension`, the gorge's floor reservation and the upriver floor chunks follow). The level plaza is now |T|≤90 m.
- **Reclaimed by the jungle:** trees (with trunk collision) on the plaza, thickening toward the sides and back, ferns, elephant ears and shrubs over most of the ground; growth on every ledge, heavier on the backs; trees clinging to upper corners; hanging moss over 210 tablero edges; 26 fallen carved blocks.
- **Tall trees (October 17):** the landing's tall trees (`SM_FallTree_A`/`B`, about 27 m at scale 1), from their own random stream: a ring of giants down both sides of the precinct, 14 out on the plaza clear of the ways in, 34 of the tallest (to about 46 m) rising behind the temple, and six rooted on the back terraces (two on each pyramid, two on the central temple's second terrace): 79 in all, visible from the avenue's start (cull 300–420 m). Where he can reach them their trunks have hidden capsule collision, as at the landing.
- **The ways in:** the old road stays clear up the avenue to the central stair (no plants within 8 m of its line), with the forecourt between the gates and a tree-free corridor from the road to each pyramid's stair.
- **The closed back:** from S=79500 to 82500 a hidden wall (ground to 52 m, camera passes through it) spans the precinct, so nothing behind the front halves of the ledges, the summit platforms or the plaza can be reached; it is dressed with a dense band of trees and heavy growth, and the existing upriver jungle continues beyond it. The top terraces and both summits stay walkable up to it.

Checks (`CHUCK_CHULT_TEMPLE_CHECK`): plaza level at 345 probes; all three stairs' surfaces and capsule sweeps (192 probes); the sanctuary and both pyramid vestibules have standing room and open doorways; the central first ledge joins both pyramids' first ledges; the plaza reaches both pyramid stairs; the avenue is open; the back is closed at five places across the precinct; the spawn; the widened floor counts as the far bank. `-ChuckTempleReview` writes nine views (adds `TempleLedgeJoin`, `TempleBackJungle`, `TempleApproach`) and the real character now climbs the central stair into the sanctuary (`CHUCK_CHULT_TEMPLE_TEST failures=0 climb=1 legs=3`).

## Later

The interior (Phase 6) begins at the sanctuary vestibule's dark back wall (the pyramids' summit vestibules could serve as side ways in). The 2D map also returns the player onto the stair from the interior (`arrival:from_temple_interior`): a stair spawn is the natural return point when the interior exists.
