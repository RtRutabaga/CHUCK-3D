# Chult — the Astral wind and the sanctum

User request, October 22, 2026 (Claude): "for the next part of the temple, build a winding narrow section of tunnels with some spike pits and quite a lot of astral ruptures (do these ruptures like the sewer, they should be ruptures in the ground and not read as intentional recessed traps like the spike pits), the challenge of this part will be back & forth wall running to make it across the spike jumps, the astral ruptures can be less challenging it's ok if you can walk around them like in the sewer. after this section we'll have a grand temple hall which will be our staging area for the beholder fight against the three heroes (we'll save this for later, just the empty hall for now)".

It follows the [trap run](CHULT-TEMPLE-RUN.md) from its antechamber, whose choked arch is now open. The 2D direction:
- **Map 6, the Astral wind** (`temple_astral_wind.txt`): a very narrow serpentine connector, its legs alternating direction, Astral cells all along it.
- **Map 9, the final chamber** (`temple_sanctum.txt`): the temple's widest hall, a paved processional between guardian colonnades to a dais, its entrance the only door. The adventurers' battle with the beholder is staged there later. It is their fight, not Chuck's.

The 2D maps 7 and 8 (the shrine hall and the 2D gauntlet) are not built. The trap run took the 2D gauntlet's role.

## Layout

The frame is the interior's: S toward the temple, T across, Z above the plaza. The tunnels are at the trap run's upper level (−860). The sanctum's floor was the halls' (−1460); since October 23 it is at −2160.

| Part | Where | What |
|---|---|---|
| Leg 1 (+T) | S 73560–73740, T −3200 → −860 | The antechamber's old choke (its fall cleared to heaps along the walls), then 1.8 m wide: two ruptures, **pit 1** (4.6 m) |
| Bend 1 (+S) | S 73560–75600, T −860 → −560 | 3 m wide, five ruptures across it in a slalom |
| Leg 2 (−T) | S 75420–75600, T −860 → −3000 | One rupture, **pit 2** (9 m), one more |
| Bend 2 (+S) | S 75420–77300, T −3300 → −3000 | Six ruptures, one lying across the middle |
| Leg 3 (+T) | S 77120–77300, T −3000 → −860 | One rupture, **pit 3** (9 m, broken walls), one more |
| Bend 3 (+S) | S 77120–78800, T −860 → −560 | Five ruptures, a long one down the middle |
| Leg 4 (−T) | S 78620–78800, T −860 → −3400 | One rupture, **pit 4** (15 m), then an arch onto the gallery |
| The sanctum | S 77000–80400, T −8800 → −3400, 17.5 m high (October 23; it was S 77600–79800, T −8000, 10 m) | Gallery, twin stairs, colonnades, idols, colossi, fire bowls, dais |

The legs are 1.8 m wide, like the trap run's pit gallery, so the far wall is in reach of the chained wall run. They are corbel-vaulted, 4 m high, with mossy lower courses: older and rougher than the trap run. The bends are 3 m wide. Every ceiling is at least 4.6 m under the plaza.

## The pits: back and forth on the walls

Each pit is 1.2 m deep with a bed of spikes and old skulls, across the full width of its leg. The only way over is a side wall run, kicked across to the other wall and back (the [chained wall run](CHULT-TEMPLE-RUN.md#the-chained-wall-run-chuckcharacter)).

The lengths come from Chuck's reach, measured with the real character in leg 4 (`-ChuckWindMeasure`, `Local/wind-test-1.log`). Distances run from the takeoff to where he goes below the floor; a landing needs a little less.

| Kicks | Kicked 1.2 m into each side run | Kicked 2 m in |
|---|---|---|
| 0 | 2.45 m | — |
| 1 | 7.7 m | 8.4 m |
| 2 | 13.4 m | 13.7 m |
| 3 | 15.7 m (against the far wall of a 15 m test pit; he lands about 17 m on) | 14.3 m |

So:

| Pit | Length | It asks for | Fewer kicks |
|---|---|---|---|
| 1 | 4.6 m | One kick: a reminder of the trap run | — |
| 2 | 9 m | Two kicks | One kick falls short (8.6 m of 9) |
| 3 | 9 m | Two kicks, timed to its broken walls (below) | Starting on the −S wall: no side run at all |
| 4 | 15 m | Three kicks, the finale | Two kicks fall short (14.96 m of 15) |

**Pit 3's broken walls.** Its −S wall is broken back over the pit's first 2 m, so the run has to start on the +S wall. Its +S wall is broken over 2 m in the middle. The line is therefore +S, −S, +S. Each break punishes a kick made too early, because the catch would come in the gap:
- Kicked at once off the +S wall, he reaches the −S side while it is still broken.
- Kicked at once off the −S wall, he reaches the +S side in its gap.

Kicked 1.2–2 m into each run, he meets a wall both times. As in the trap run's twist, each break opens onto an alcove over the spikes, with the wall's broken ends.

**Landings.** He lands 2–4 m beyond a pit's far lip, by the wall of his last run. The 5 m past every pit are kept free of ruptures, and the next one lies by the other wall. An earlier layout put ruptures in those landing zones; the full-run trial showed it.

## The ruptures

There are 24: two to three per leg, five or six per bend (`ChultTempleWind.cpp`). They use the sewer's natural form, as the [jungle wreck](CHULT-WRECK.md) does: a pointed tear, ragged and a little bent. In stone, it is drawn as:
- the broken flag edge lapping over the cut and breaking down inside;
- the Astral glowing in it (`M_AstralDepth`);
- two oil-film veils rising off its long edges (`M_AstralOilMist`);
- a purple light.

To read as the floor torn open rather than as traps:
- each lies at its own angle, off the middle;
- cracks run out from its two points into the flags;
- a few flakes of the floor round it are lifted and tipped (visual only, so they never trip him).

The spike pits are the opposite: square, dressed and lined.

**The floor is really open.** Each tunnel's floor is laid in whole flags, and in 10 cm cells round the tears with the tear cut out. Walking in is an Astral death, back at the start of that leg or bend: `ChultTempleInteriorSwallows` applies 40 cm down inside the outline.

**There is always a way round.** In the legs a rupture lies by one wall and leaves about 80 cm by the other. Each pit's run-up wall is kept clear for 2.5 m. The bends' ruptures are a slalom: walked round, never jumped. The interior check proves it on a 10 cm grid: from each start to the next goal, his capsule (18 cm round) stands clear of every rupture and pit.

## The sanctum

**Made bigger and grander on October 23** (user: "Make the grand hall bigger and grander"). It was 46 × 22 m and 10 m high. It is now 54 × 34 m and 17.5 m high, corbel-vaulted (six courses), its frieze in the ruins' mossy stone.
- **The floor went down, not the roof up:** the plaza is 4.6 m above the old ceiling and the interior check keeps every ceiling 4 m under the ground (its trees' roots). The floor is 7 m below the halls', at −2160, and the ceiling is at −410.
- **The gallery:** leg 4 still enters through an arch with a plaster lintel onto it, now **13 m** above the floor. It runs across the hall's end on six square piers, with a balustrade and finials between the stairs' heads.
- **The stairs:** two 2 m wide flights down the side walls, 25 m long with 81 risers each, a hidden ramp under visible steps as in the well. Torches follow their line.

From the gallery the hall reads end to end:
- **The processional:** a paved way of ruin stone, 3.8 m wide, with kerbs.
- **The colonnades:** two rows of five great square piers either side of the nave, 9 m off its axis. Each pier has a base, a moss band, a banded capital and a torch on its nave face.
  - The piers carry an architrave along each row at 11 m, below the gallery, so nothing crosses the view from the gallery to the dais.
  - An upper colonnade of pilasters rises over them to a beam under the ceiling, and glyph friezes run on the architraves.
  - A lane is left between the third and fourth piers at the stairs' foot.
- **The guardians:** four serpent idols on plinths along the processional's first half.
- **The fires:** four braziers by the processional and four great fire bowls (2 m, on stepped plinths), two flanking the dais and two in the aisles before it.
- **The dais:** 22 m wide and 12 m deep, risen 96 cm with five steps. On it:
  - an altar on a stepped base;
  - two **colossal serpent idols**, about 5 m, on 2.5 m plinths carved with glyphs;
  - behind them, a 6.6 m **carved disc** with sixteen rays in the end wall, over an 18 m glyph wall between stone bands.
- **Torches** down both long walls, on the end wall, on the gallery and its back wall, and pilasters on the side walls beyond the stairs.
- **Pots:** two on the gallery, two in the hall's far corners.

**Moved with it:**
- The battle's heroes, skeleton posts and leash are placed relative to the dais's front (the reserve posts avoid the piers and fire bowls).
- The battle's sounds carry 36 m.
- The interior check's route and the stairs trial cross to the processional through the colonnade's lane.

The gallery is where Chuck will watch the battle; the floor and dais are its stage. The hall has no other way out, as in 2D, where the scripted Fireball is the way on. Since October 23 its music is the user's "boss_battle", and the tunnels before it play their "temple 2" from the trap run's pits on (SourceAssets/Audio/README.md). The three adventurers stand on its floor before the dais, fighting skeletons that rise from the bones round the dais end ([CHULT-TEMPLE-HEROES.md](CHULT-TEMPLE-HEROES.md)); the beholder is still to come.

## The sanctum's ruptures

User request, October 24, 2026 (Claude): fill in under the staircases with stone, "like a real stone staircase of this height would be"; add Astral ruptures to the hall, "in particular a rectangle of astral rupture at the base of the staircases", so Chuck can't jump from the balcony or the very top of a stair without an Astral death and has to walk down at least part of the way; make them look like the pantry cellar's ruptures, not the sewer's; and to the wizard's left an other-world rupture like the cellar's fall to Chult, smaller, showing waves of the open sea with the cellar's oil slick round it: the tear the wizard is trying to seal, a fall death for now.

**Under the stairs.** Each stair now rests on solid masonry (`StairMass` in `ChultTempleWindBuild.inl`).
- Its open side is a stone wall from the floor up to the soffit, continued under the gallery to the back wall.
- A string course (5 cm proud) runs along the stair's line under the fascia, and a plinth course runs along the foot where it stands on floor.
- The wall has collision, so nothing walks in under a stair. The interior check sweeps in from each aisle (`stair_open_beneath`).

**The pool.** The whole floor between the stairs is broken through from under the gallery to 15 m out (T −3400 to about −4990, its far edge ragged by 25 cm).
- **How far it reaches.** The longest jump off the high end is a sprint leap: 420 cm/s with 250 cm/s lift. From the stair's head (13 m up) that is about 2.2 s in the air, so about 9 m. The pool's edge is set beyond that.
- **The gallery's balustrade** allows no run-up for a sprint leap. A standing jump off it falls shorter.
- **About 5 m down the stair** (about 9.5 m up) the same leap clears the pool. The `mid_leap` trial shows it.
- **The piers inside it** (the gallery's six and the colonnades' first pair) stand on ragged islands of floor round their footings, each with its own haze.
  - Landing on an island's ledge is a dead end: there is no run-up to leap off it. The only way off is into the Astral.
- **Moved out of it:**
  - the processional's paving and kerbs now start at its far edge;
  - the first two braziers stand at that edge (T −5080; they were at −4300);
  - the four guardian idols moved back to T −5250 and −5650 (they were at −4875 and −5425).

**Four smaller ruptures** in the aisles: two beside the stairs, two beyond the dais' corners.

**How they look.** All of these are the cellar's kind (`DockPantry.cpp`):
- a ragged hole stepped in 10 cm cells;
- the floor's 30 cm stone edge;
- the Astral's well under it (`M_AstralDepth`);
- the cellar's oil haze rising round the whole rim (`M_AstralOilMist`, in lengths of about 4 m along long rims so the pattern keeps its scale);
- chips of flag round it;
- purple light from below.

The Astral wind's tunnels keep the sewer's slits.

**The sea tear.** It lies to the wizard's left as he faces the beholder: centre S 78083, T −6710, radius about 1.1 m (the cellar's sky hole is 2.65 m).
- The cellar's ragged round hole and its 1 m oil slick.
- Its 8 m well is lined with `M_TempleRiftSea` (`Tools/create_temple_rift_sea.py`). Like the cellar's sky, that material ignores which face a pixel is on and colours it by the view ray alone. The ray meets a sea plane 15 m below the floor, where moderate travelling waves from one wind (3.8 m down to 0.25 m long) are shaded with sky reflection, light through the crests, a little glint and foam on the steepest crests.
- Pale daylight comes up out of it.
- It is a fall death, not a portal.
- It is about 1.4 m from the wizard's ground. The skeleton post that lay beside it moved 0.9 m away.

**Deaths and the battle.**
- Feet 40 cm down in any of these holes is a death (`ChultTempleInteriorSwallows`), back on the gallery.
- The skeletons' leash now stops 3 m short of the pool (T −5300; it reached T −4200).
- In the sanctum the skeletons don't walk off ledges, and their steering refuses a heading into a hole.
- A hero's step never ends on one.
- `CHUCK_SANCTUM_BATTLE` reports `near_holes`: heroes' grounds or posts within 1 m of a hole (0).

**Checks.**
- Every hole is open to 2 m and swallows. Its rim holds him and doesn't swallow.
- Each stair's underside is closed.
- The interior route stays 40 cm clear of every hole (`CHUCK_SANCTUM_HOLES_CHECK`).
- Wind-test trials:
  - `top_leap`: a sprint leap off the −S stair's head dies in the pool.
  - `mid_leap`: the same leap from 5 m down lands beyond it.
  - `sea_fall`: walking into the sea tear dies, back on the gallery.
- New stills: `SanctumPool`, `SanctumPoolEdge`, `SanctumRift`, `SanctumSea`, `SanctumSeaDown`.

## Respawns and checkpoints

There are eight new spawns, each a Dev Checkpoint in a fifth column on the checkpoint page (1700 wide):
- **Astral Wind 1, 3, 5, 7:** each leg's start (1 in the old choke).
- **Astral Wind 2, 4, 6:** each bend's start corner.
- **Temple Sanctum:** on the gallery.

A death in a leg, its pit or leg 3's alcoves returns him to that leg's start. A death in a bend returns him to that bend's start. A death in the hall returns him to the gallery.

## Checks and tests

Every Chult run runs the interior check (`CHUCK_CHULT_TEMPLE_INTERIOR_CHECK`), which now covers:
- the route on through the old choke, along every leg and bend round the ruptures, over the pits only on the walls (gaps), onto the gallery, down the −S stair, along the aisle, across between a pillar and an idol, and up the processional onto the dais;
- every rupture: open to 2 m, swallowing at its middle, its rim floor that does not swallow;
- the grid search for a way round (`CHUCK_TEMPLE_WIND_CHECK lanes=8`), and each pit's run-up wall clear;
- deaths in each new part mapping to its own respawn, the pits and alcoves swallowing, their lips and the hall not;
- the intrusion and ground-clearance probes on the 37 spaces (65,335 probes, `intrusions=0`).

`-ChuckChultTest -ChuckTempleWindTest` (`ChultTempleWindTest.cpp`) takes 16 stills (`Saved/Screenshots/.../Chult/Wind*.png`, `Sanctum*.png`). Then the real character runs these trials, with live input off:

| Trial | What it does | Uncooked result (`Local/wind-test-6.log`) |
|---|---|---|
| pit1 | −S wall, one kick | lands 2.3 m past the far lip |
| pit2 | −S wall, two kicks | lands 3.3 m past (4.2 m in the full run) |
| pit2_one_kick | one kick | falls (8.6 m of 9) |
| pit3 | +S wall, two kicks past the broken walls | lands 4.2 m past |
| pit3_wrong_wall | starts on the broken −S wall | no side run; falls |
| pit4 | +S wall, three kicks | lands 3.9 m past (rolls) |
| pit4_short | two kicks | falls (14.96 m of 15) |
| tear_fall | walks into bend 1's first rupture | one death, back at Astral Wind 2 |
| bends | walks each bend from its start to the next leg round its ruptures (the searched path) | no deaths, 73.8 s |
| stairs | gallery, the stair down, the aisle, the processional, onto the dais | on the dais, 26.7 s |
| full | from the old choke to the gallery: each leg, its pit on the walls, each bend | no deaths, 128 s |

Verify-Package runs it on the packaged candidate.

The trap run's test is unchanged at 16/16 (`Local/wind-regress-run.log`).

**A fix found on the way (ChuckCharacter).** A long frame (a hitch) ended a side wall run as "stalled". The frame's travel exceeds the teleport guard (50 cm), is dropped as a teleport, and so reads as no movement. The stall test now ignores frames over 0.1 s.

## Not done

- No person has played it. Untested by a human:
  - the difficulty, above all pit 4's three kicks;
  - the camera in the 1.8 m legs;
  - whether the ruptures read as torn ground rather than traps.

  A gamepad has not been tried.
- The beholder, the three adventurers, the skeletons, the battle, the Fireball and the boss music are all for later. The hall is empty.
- The ruptures make no sound of their own.
- The veils are the sewer's oil film. Close up in torchlight they read as glassy panels.
- The 2D maps 7 (shrine hall) and 8 (gauntlet) are not built.
- The sanctum's ruptures (October 24): seen low from the floor, the sea tear reads pale, more like sky than water (seen from above it shows rows of waves and whitecaps). Landing on a pier island in the pool is a dead end. A wall run along a stair's wall has not been tried as a way past the pool. No person has played them.
