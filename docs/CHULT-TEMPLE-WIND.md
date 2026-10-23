# Chult — the Astral wind and the sanctum

User request, October 22, 2026 (Claude): "for the next part of the temple, build a winding narrow section of tunnels with some spike pits and quite a lot of astral ruptures (do these ruptures like the sewer, they should be ruptures in the ground and not read as intentional recessed traps like the spike pits), the challenge of this part will be back & forth wall running to make it across the spike jumps, the astral ruptures can be less challenging it's ok if you can walk around them like in the sewer. after this section we'll have a grand temple hall which will be our staging area for the beholder fight against the three heroes (we'll save this for later, just the empty hall for now)".

It follows the [trap run](CHULT-TEMPLE-RUN.md) from its antechamber, whose choked arch is now open. The 2D direction:
- **Map 6, the Astral wind** (`temple_astral_wind.txt`): a very narrow serpentine connector, its legs alternating direction, Astral cells all along it.
- **Map 9, the final chamber** (`temple_sanctum.txt`): the temple's widest hall, a paved processional between guardian colonnades to a dais, its entrance the only door. The adventurers' battle with the beholder is staged there later. It is their fight, not Chuck's.

The 2D maps 7 and 8 (the shrine hall and the 2D gauntlet) are not built. The trap run took the 2D gauntlet's role.

## Layout

The frame is the interior's: S toward the temple, T across, Z above the plaza. The tunnels are at the trap run's upper level (−860); the sanctum's floor is the halls' (−1460).

| Part | Where | What |
|---|---|---|
| Leg 1 (+T) | S 73560–73740, T −3200 → −860 | The antechamber's old choke (its fall cleared to heaps along the walls), then 1.8 m wide: two ruptures, **pit 1** (4.6 m) |
| Bend 1 (+S) | S 73560–75600, T −860 → −560 | 3 m wide, five ruptures across it in a slalom |
| Leg 2 (−T) | S 75420–75600, T −860 → −3000 | One rupture, **pit 2** (9 m), one more |
| Bend 2 (+S) | S 75420–77300, T −3300 → −3000 | Six ruptures, one lying across the middle |
| Leg 3 (+T) | S 77120–77300, T −3000 → −860 | One rupture, **pit 3** (9 m, broken walls), one more |
| Bend 3 (+S) | S 77120–78800, T −860 → −560 | Five ruptures, a long one down the middle |
| Leg 4 (−T) | S 78620–78800, T −860 → −3400 | One rupture, **pit 4** (15 m), then an arch onto the gallery |
| The sanctum | S 77600–79800, T −8000 → −3400, 10 m high | Gallery, twin stairs, colonnades, idols, braziers, dais |

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

A hall 46 × 22 m and 10 m high, corbel-vaulted, its frieze in the ruins' mossy stone. Leg 4 enters it through an arch with a plaster lintel, onto a **gallery** 6 m above the floor. The gallery:
- runs across the hall's end on four square pillars;
- has a low balustrade between the heads of the twin **stairs**, which run down its side walls (38 risers each, a hidden ramp under visible steps, as in the well).

From the gallery the hall reads end to end:
- **The processional:** a paved way of ruin stone with kerbs.
- **The colonnades:** two rows of four great square pillars, base and capital, each with a torch on its face toward the processional.
- **The guardians:** six serpent idols on plinths between the pillars, facing the way.
- **The braziers:** four by the processional, two before the dais.
- **The dais:** risen 96 cm with five steps, an altar, two great serpent idols and a glyph wall behind.
- **Torches** down both long walls, on the end walls and at the gallery.
- **Pots:** two on the gallery, two in the hall.

The gallery is where Chuck will watch the battle; the floor and dais are its stage. The hall has no other way out, as in 2D, where the scripted Fireball is the way on. It is empty for now: no adventurers, beholder, skeletons, battle or music of its own (the temple interior's score plays).

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
