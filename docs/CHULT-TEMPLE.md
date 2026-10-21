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

The interior (Phase 6) begins at the sanctuary vestibule's dark back wall. Since October 20 it is built: the cella is lined and lit, a doorway in its back wall leads down a hidden stair into the temple interior ([CHULT-TEMPLE-INTERIOR.md](CHULT-TEMPLE-INTERIOR.md)); `Box()` leaves the masonry's horizontal faces open where the interior passes through them. (The pyramids' summit vestibules, once suggested as side ways in, are now closed ceremonial chambers: see October 18.) The 2D map also returns the player onto the stair from the interior (`arrival:from_temple_interior`): a stair spawn is the natural return point when the interior exists.

## The temple score; closed undersides (October 17)

User request, October 17, 2026 (Claude): the user's `Chult 3.wav` "for sound track on approach to temple and around temple exterior", timed "so that a character running straight towards temple will arrive in view of the temple as the music swells"; then "clean up the temple some": spots "partially invisible when you're next to them like the side of the main stair case when looked down on from the top", and "spots that jut out but have no floors".

**The score.** The track (204.9 s) builds from near-silence; its swell is the full arrangement's entry at **10.7 s** (the low band jumps about 11 dB between 10.6 and 11.0 s; analysis in `SourceAssets/Audio/README.md`). Running up the road (225 cm/s), the temple glimpses through the trunks from about S=63000 and the central spire comes clear in the jungle's opening at about **S=63500** (`ChultTempleRevealS`, judged from a frame every 0.5 s of the real character's run, `-ChuckTempleApproachShots`). `ChultTempleMusicFromS` walks back along the road's line by 10.7 s of running: the score starts at **S=61250** and plays over the rest of the road and the whole precinct (`InChultTempleMusicZone`, 15 m hysteresis). It cross-fades with the bridge score ("Chult 2") over 2.5 s, which takes over again back down the road.

- Arriving on the road (within 10 m of the start), it plays from the top with a 0.25 s fade. Arriving any other way (the **Chult Temple** checkpoint, a respawn after a long absence), it fades in over 1.5 s from 1.5 s before the swell.
- Leaving and coming back within a minute, it carries on where it was; after longer it starts again.
- Measured: the run reaches the reveal 10.67–10.73 s after the score starts (`CHUCK_CHULT_TEMPLE_APPROACH lead=…`, required within 1 s of 10.7). The swell itself was not heard by the agent.

**Hollow masonry.** Two gaps in the builder (`Box`, the balustrades), both confirmed by a before/after pair (`Local/temple-faces/`) and a new camera-sweep check:
- Every box was open underneath, so each band, cornice, roof overhang, spire band, plinth and serpent snout was hollow seen from below (Chuck, 65 cm tall, mostly looks up at them), and the camera could rise into them. All boxes are now closed below.
- Each stair's balustrades had no top end, and that end stands a metre proud of the platform: from the top of a stair the balustrade vanished, and the camera could slip inside it. The ends are now closed.
- `CHUCK_CHULT_TEMPLE_CHECK` now sweeps a 12 cm camera sphere into all six balustrade ends and up into both summit-house roofs (24 sweeps in all). With the old faces 8 of them got in; now none.

The review adds three views (`TempleStairSideCentral`, a balustrade's end from the platform; `TempleStairSidePyramid`; `TempleRoofBelow`) and the approach run before the climb.

## The rooms; tan jars and clay urns (October 18)

User request, October 18, 2026 (Claude): leave the central stair's doorway alone (it will be the temple interior's entrance), but give "the other side temple entrances and other parts of the temple that look like they should allow access inside" small, limited, ceremonial interiors, with torches and setting objects; the Waterdeep clay jars in tan ceramic; and a new breakable clay urn, lootable for cigarettes.

**Eight rooms** (`Rooms()` in `ChultTemple.cpp`), each frame-aligned, door centred in its near wall:

| Room | Where | Inside (m) | What is in it |
| --- | --- | --- | --- |
| Summit chamber ×2 | each pyramid's summit house (its old 3 m dark vestibule) | 9.8 × 11.8, 3.4 high | a dais with an altar, a coiled serpent idol and two braziers; four piers carrying a corbelled ceiling, one fallen with its drums on the floor; glyph panels between four wall torches; a glyph frieze and two dark niches in the back wall; 8 tan jars, 2 urns |
| Gate shrine ×2 | the gate pavilions either side of the central stair's foot, door facing the avenue | 4.3 square, 3.0 high | a pedestal with a serpent idol, a glyph frieze, two wall torches; 3 jars, 1 urn |
| Corner shrine ×4 | the central temple's top-terrace corner towers, doors facing the middle | 2.9 square, 2.3 high | an altar with a small serpent idol, a glyph frieze, one wall torch; 2 jars, 1 urn |

Since October 20 the corner shrines stand 3.77 m in from the top terrace's edges (`ShrineIn`; they were 2.4 m in): their 4.5 m plinths had overhung the ledge by about 1 m on two sides and the tablero face beneath by 1.1 m. They now stand on the ledge, 40 cm clear of its moss band, with walkable terrace round their outer sides. The rooms, steps, growth clearances and the walk-in test all follow the constant.

The second terrace's towers (out of reach) and the sanctuary's side false doors stay false. The rest of the sanctuary is untouched.

- **Dressing** follows the 2D temple's kit (PHASE-6: coiled serpent idols, glyph stelae, terracotta urns, fallen column drums, recurring wall torches). The walls are plastered over a 55 cm stone dado; the glyphs are carved stone cartouches, each with a number in bars and dots cut dark into it; the idols are pale stucco.
- **Light:** the insides (`ChultTempleRooms`, `ChultTempleRoomsDark`) and the jars are on lighting channel 1 only, so the sun and the unshadowed bounce fill cannot reach them and the 20 fires (unshadowed point lights, channel 1, flickering) cannot leak onto the outside masonry. While the camera is in a room the sky light fades to 15% over 0.4 s and Chuck's components join channel 1 (the sewer's pattern); the histogram auto exposure does the rest.
- **Ways in:** the summit chambers open off the summit platform. Each prasat door has three 15 cm steps up its 45 cm plinth over a hidden ramp (26.6°, through the treads' middles). Growth is kept off the doorways and away from the room walls (leaves poked through the gate's walls) by a `Suppress` flag that keeps every random draw, so the rest of the precinct's growth is unchanged.

**The pots** (`Tools/build_clay_jar.py --variant tan|urn`, imported by `import_clay_jar.py` with `CHUCK_JAR_VARIANTS=tan,urn` onto the existing `M_ClayJar`):
- `ATempleJar` (`/Game/Art/Props/TempleJar`): the docks' jar, same shape and 12 shards, in tan ceramic under a slightly deeper glossy tan glaze. 1–2 cigarettes each.
- `AClayUrn` (`/Game/Art/Props/Urn`): 44 cm (to Chuck's chest), 30 cm across, buff slip with red bands and a black stepped fret round the shoulder; 19 shards and a lower-pitched break. 2–4 cigarettes each.
- `AClayJar` is now configurable (`Configure`, `PlaceKind`, `SetIndoors`); the docks' jar and its generated `ClayJarData.h` are unchanged (re-running the default variant reproduces them).

**Checks:** `CHUCK_CHULT_TEMPLE_ROOMS_CHECK failures=0 rooms=8 jars=30 urns=10 breaks=2`: standing room in each room, its doorway open from outside, four probes of each prasat's step ramp, all 40 pots present and whole, and one tan jar and one urn broken on the plaza (12 and 19 shards, sound played, cigarettes released). `-ChuckTempleReview` adds five views (`TempleRoomSummit`, `TempleRoomGate`, `TempleRoomShrine`, `TempleGateDoor`, `TempleRoomJars`), and the real character, after the sanctuary, walks back out across the top terrace, up a corner shrine's steps and into it (`CHUCK_CHULT_TEMPLE_TEST failures=0 climb=1 legs=6`).

## Modelled skull stakes and serpent statues (October 18)

User request, October 18, 2026 (Claude): "Find and implement free resources to make the skull stakes and the serpent statues look better, if you can't find a good resource for serpent statue feel free to change it", the serpent statues including the rooms' idols.

- **Skull:** "human skull 0" by CDmir, **CC0**, from [OpenGameArt](https://opengameart.org/content/human-skull-0) (`skull-fbx.zip`, 9.7 MB, downloaded with the user's approval; credits and the original files in `SourceAssets/Props/TempleStatues/Skull/`). 4,548 triangles, real size (19 cm long); its painted bone texture is aged (desaturated, yellowed, grimed in its crevices from its own AO map, stained) and reduced to 1K with its normal map (`M_TempleSkull`).
- **Stakes:** a modelled pole 2 m tall, eight-faced, tapering and slightly bent, in the jungle's bark (`M_ChultIslandBark`), with three leather lashings and two hanging cords under its point and a ring of stones at its foot. The skull sits on the point, its face across the avenue, tilted a little at random. The stakes keep their places and their two lean draws, so nothing else in the precinct moved.
- **Serpent heads:** no free serpent statue of usable quality and licence was found (the good ones are paid; the free ones are AI-generated with unclear terms), so the head is modelled after Chichen Itza's Kukulkan heads: rounded jaws open down the avenue, two great curved fangs and a row of teeth, smaller lower fangs, lidded eyes under brows that scroll down at the front, a curled snout, scale plates along the lower jaw and neck, a broad forked tongue lolling out onto the plaza and two tiers of plumes behind. 3.3 m long with the tongue, 1.4 m wide, 1.8 m high; stone with moss on its uppermost faces. It replaces the box heads at all six balustrade feet; growth is kept off the tongues without changing any draw.
- **Idols:** a modelled coil of 2.6 turns rising from a thin tail on a low base, the neck rearing to a fanged, open-jawed head with brows, eyes and a feathered hood; stucco (`M_WeatheredPlaster`), lit by the rooms' channel only. One mesh, scaled to each room (coil radius 25, 15 and 12 cm).
- **How they are made:** `Tools/build_temple_statues.py` (Blender 4.5.14) builds all four: primitives and tubes joined by a voxel remesh, smoothed, weathered by displacement and decimated (head 24,000, idol 16,000 triangles). `Tools/import_temple_statues.py` imports them into `/Game/Art/ChultTemple`; the stake, head and idol collide as their own shape and the skull does not. `ChultTemple.cpp` places 30 statues (`CHUCK_CHULT_TEMPLE ... statues=30`).
- **Review:** `-ChuckTempleReview` adds `TempleSkullClose`, `TempleSerpentClose` and `TempleIdolClose`.
