# Chult — the temple trap run

User request, October 22, 2026 (Claude): "expand the temple dungeon run. for the next part, currently we have some rubble closing in an entrance, keep the rubble there and make it so chuck can climb up the rubble then wall jump climb to get to the next part of the temple. from there i want this to be heavily parkour focused, with a combination of spike pits to wall run over, swinging axe sections to quickly get through, wall flamethrower traps to doge, and overhead circular saw blades to roll under. while creating this, look up how other media have depicted temple trap runs, and how other games have made things like this playable and fun. i want some amount of challenge here, it should be conducive to a feeling of flow getting the timing right with the movements and parkour".

This is the user's direction for the next part of the [temple interior](CHULT-TEMPLE-INTERIOR.md), beyond the [snake chamber](CHULT-TEMPLE-SNAKES.md)'s choked arch. It takes the place the plan held for 2D map 6 (the Astral wind connector): it is an original 3D parkour section, closest in spirit to the 2D map 8 gauntlet (spike bands, darts and Astral cells together). The 2D maps 6–9 remain for later.

## Research: how others have done it

Films:
- **Raiders of the Lost Ark** (1981): the most cited temple trap run. Each trap is shown working (a dead predecessor on the spikes) before it threatens Indy. Critics credit the suspense to the traps' visible mechanics rather than effects. Jordan Mechner took the original *Prince of Persia*'s trap temple from it.
- **Indiana Jones and the Last Crusade** (1989): "Breath of God". Two spinning blades come out of the wall at neck height; only "the penitent man" who kneels and rolls forward passes. That is our saws: a roll under them is the only safe way while they are down.
- **Temple of Doom**'s closing spike room and *The Mummy*'s collapsing tomb: the threat is sustained and visible, and the escape is one continuous movement.

Games:
- **Prince of Persia (1989) and The Sands of Time (2003)**: traps are physical and readable (chompers, slicers, pendulum blades, spikes in the floor) and the acrobatics chain: wall run, jump, wall run. The 2003 team said 3D acrobatics were hard to get right, so they added the rewind to take the sting out of failure.
- **Super Mario 3D Land/World** (Koichi Hayashida's *kishōtenketsu*): each level introduces a mechanic where failure is cheap, develops it, adds a twist, then ends with a test that uses everything learned.
- **Rayman Legends' music levels**: obstacles land on the beat of the music; the player who keeps moving at full speed finds every gap where it is needed. That is the purest "flow" design: the level is tuned for a continuous run.
- **Celeste**: coyote time, jump buffering and an instant respawn at the start of the current room keep a player retrying a hard section without frustration.
- **Mirror's Edge**: flow is momentum. Its weak points, per its critics, are dead ends that stop you, unreadable routes and checkpoints too far apart.
- **Tomb Raider and Indiana Jones and the Great Circle**: film traps don't transfer directly; they have to be rebuilt around the game's own moves.

What we take from them:
1. **Introduce, develop, twist, combine.** Each trap appears alone first, where a mistake costs little. Then a run of them, then a variation. The run ends with a gauntlet of all four.
2. **Telegraph everything.** Every trap shows and sounds its state before it can hurt: the spikes are visible in the pits, the axes swing in plain view, a fire mouth glows and hisses before it fires, a saw lowers its teeth and whines before it drops.
3. **Tuned for a continuous run (the Rayman idea).** Each run of traps is phased so that a player moving at Chuck's run speed meets every gap: the axes' crossings, the fire bursts and the saw drops travel down the hall as a wave at run speed. Stopping between traps is always possible too, so a careful player can still get through.
4. **Short rooms and instant retry (Celeste).** Every section is its own space with its own respawn and Dev Checkpoint. A fall into a pit returns Chuck to that section's start at once; a trap hit costs one Sanity (of five), and a jar at each turn restores it.
5. **"Look hard, be easy"** (the user's standing direction for Chuck's parkour): generous hit tests, a ledge grab on a short jump, buffered inputs.

## Chuck's moves this uses

Measured from `ChuckCharacter`:
- Run 225 cm/s; sprint 380 cm/s (needs full stamina; 2.5 s of it, 10 s to refill).
- Sprint leap: 3.7 m measured here from a 5 m run-up (the sewer rift's notes say about 2.8 m).
- Side wall run: about 1.1 s along a wall beside him at ≥236 cm/s, so 2.6 m, with an arc 72 cm high.
- Chimney: wall jumps back and forth between two walls 100 cm apart, about 70 cm higher per bounce, catching the top edge.
- Roll: 0.8 s, 1 m on its own and about 1.8 m from a run.
- Mantle: a 6–40 cm step walked into.

**New move: the chained wall run.** A jump during a side wall run now kicks off across the corridor, keeping his speed along it. If the opposite wall is close, he catches it in the air and runs along it too: left wall, kick, right wall, the *Prince of Persia* zig-zag. Without a wall within reach it is the old kick off the wall. The head-on climb and the chimney are unchanged.

## Layout

Frame as in the interior (S toward the temple, T across, Z above the plaza). The rubble passage is at the hall floor (−1460). The upper level is 6 m higher at −860; every ceiling stays at least 4 m under the plaza, which is level ground (Z 0) over all of it.

| # | Section | Where | What |
|---|---|---|---|
| 0 | **The collapse** | The snake chamber's +S arch (T −7950), S 67900–68310 | The rubble stays. It is heaped as tumbled blocks that he climbs by six mantles to 2 m. At its top the old passage has fallen into a chimney 1 m wide, a 4 m wall-jump climb to the upper level. |
| 1 | **Pit gallery** | S 68310–68490, T −8000 → −4710 (1.8 m wide) | A landing over the chimney, then three spike pits across the corridor: a 2.2 m pit (one side wall run), then two 4.6 m pits (the chained wall run). The third starts against a broken wall on one side, so he has to begin on the other. |
| 2 | **Axe hall** | S 68310–71470, T −4710 → −4210 (5 m wide) | Its ends are the turns. Between them a 1.6 m causeway between spiked trenches. One great axe alone, then a wave of three that swing across it one after another at run speed. |
| 3 | **Fire hall** | S 71080–71380, T −4710 → −7700 (3 m wide) | One fire mouth alone, then a slalom of four alternating-wall jets that reach two thirds of the way across, then two full-width paired jets. |
| 4 | **Saw passage** | S 71080–73740, T −7940 → −7700 (2.4 m wide, a low 2.4 m ceiling) | Its ends are the turns. One saw row that stays down, then two (roll, run, roll), then three that drop and rise as a wave. |
| 5 | **The gauntlet** | S 73560–73740, T −7700 → −3800 (1.8 m wide; the axes' chamber 5 m) | All four together, on one beat: a pit, two axes, saws, fire, then a pit with the chained run. |
| 6 | **Antechamber** | S 73400–73900, T −3800 → −3200 | Rest, jars, and a choked arch for the next part. |

Each section's ends hold a torch, a jar or urn, and its respawn.

## Rules

- **Spike pits and trenches:** a fall in is an Astral death, back to that section's start (the hall's existing rule for the rift and the well).
- **Axes, fire, saws:** one Sanity a hit, a knock back along the corridor (never into a pit), a second of immunity. A dodge roll doesn't protect from axes or fire. Under a lowered saw, **only a roll** is safe; a side jump doesn't help.
- **Respawns / Dev Checkpoints:** Temple Climb (the rubble's foot), Temple Pits, Temple Axes, Temple Fire, Temple Saws, Temple Gauntlet, in a fourth column on the checkpoint page.
- **Sound:** new synthesised trap sounds in the stone-chamber acoustic, heard only inside.
- **Light:** torches along every section; the fire mouths light the walls when they fire.


## As built (October 22)

Code: `ChultTempleRun.h` (the layout), `ChultTempleRunBuild.inl` (masonry and trap layout, included into `ChultTempleInterior.cpp` to share its lining helpers), `ChultTempleRunTraps.cpp` (`AChultTempleTraps`' axes, fire jets and saws), `ChultTempleRunTest.cpp` (`-ChuckTempleRunTest`), and the chained wall run in `ChuckCharacter`.

**The collapse.** The snake chamber's +S arch stays; behind it the passage is heaped with tumbled blocks. Up the middle runs a line of flat blocks, each exactly 34 cm above the last, so each is one mantle. Either side are rougher blocks at other heights, loose stones, heaps against the walls, a broken lintel leaning on the heap and a fallen drum. At 2 m the old passage has fallen into a chimney 1 m wide and 4 m high, lit by two torches. Its +T wall's top is the lip of the pit gallery's floor.

**The pit gallery** (1.8 m wide, corbel-vaulted). A landing over the chimney (with an urn), then three pits 1.2 m deep with spike beds and old skulls:
- a 2.2 m pit: one side wall run;
- a 4.6 m pit: the chained wall run;
- a 4.6 m pit whose −S wall is broken back into an alcove for its first 2.2 m. He has to start on the +S wall and kick across only once the far wall is there again.

**The axe hall** (5 m wide). The turn rooms at both ends are part of it. In between, a 16 m causeway 1.6 m wide with pale kerbs, between trenches 1.5 m deep full of spikes and skulls. Four great axes hang from iron hubs on beams: one alone, then a wave of three 2 m apart.
- Each axe swings ±36° on a 3.15 m arm every 2.2 s. Its crescent blade is 1.1 m across, with an edge 28 cm off the floor.
- The wave's phases are 0.89 s apart (2 m at run pace). A runner who passes the first just after its blade crosses meets each of the others the same way.

**The fire hall** (3 m wide, 30 m long). Nine carved serpent mouths:
- one alone;
- four in a slalom, alternating walls 2.5 m apart. Each reaches 2 m of the 3 m, and they fire as a wave at run pace. Start as the first flares and weave with the wave, or start as it dies and run straight down the middle behind it;
- two pairs of opposed mouths 1.6 m each, meeting in the middle, 3 m apart and a beat apart.

Each mouth's cycle is 3 s: a 0.7 s tell (a flame licking at the mouth, a hiss, a growing glow), a 1.3 s burst, then 1 s out. The jet is drawn from the torches' flame material: four tongues streaming out and a row of broad flames rising off it, its own light flaring with it.

**The saw passage** (2.4 m wide, a 2.4 m ceiling). Six rows of two discs 1.24 m across, their teeth 30 cm off the floor, spinning on hangers out of a slot in the ceiling, with iron track straps on the walls:
- one row that stays down;
- two more 2.2 m apart (roll, run, roll);
- three that drop and rise as a wave at run pace: up 0.8 s, teeth lowering 12 cm and a whine for 0.4 s (the tell), dropped, up again over 0.3 s, every 2.4 s.

The blades and discs are in a new dark forged-steel material, `M_TempleBlade` (`Tools/create_temple_run_materials.py`). It is on the meshes' own UVs, so it doesn't slide over a moving blade.

**The gauntlet** (1.8 m wide, with a widened chamber for the axes). It runs:
1. a 2.2 m pit;
2. a short causeway between trenches with two axes;
3. a low passage with a fixed and a dropping saw row;
4. two full-width fire mouths, alternating walls;
5. a 4.6 m pit for the chained run.

All of it is on one 2.4 s beat. Each trap's phase is set from when a runner who leaves the first pit's lip on the beat reaches it at run pace. Measured with the real character, the arrivals match within 0.05 s.

**The antechamber.** Two serpent idols on plinths, three jars and urns, and torches. The way on is an arch whose passage has fallen in (rubble over a hidden wall), for the next part.

**Traps in play.**
- Axes and fire: one Sanity a hit, a knock back along the run toward its start (never sideways off a causeway or into a pit), then a second of immunity. A dodge roll doesn't protect.
- Saws: a lowered saw meets his body unless he is rolling (`AChuckCharacter::IsRolling`). A side jump doesn't help.
- The spike pits, trenches and alcove: below 80 cm under the floor he is on the spikes, an Astral death back at that section's start. A short jump can still catch the far lip and hang.
- The hit tests are geometric:
  - the axe: points along its blade and arm against his capsule;
  - the jet: a box 60 cm deep along the route and 70 cm tall, out to how far the flame has reached;
  - the saw: the disc's lowest edge at his position.

**Sounds.** `Tools/gen_temple_run_sfx.py` (stdlib, seeded, in the traps' stone acoustic) makes 15 originals:
- the axe's whoosh through the bottom of its swing and its pivot's creak at either end;
- the fire's hiss (its tell) and roar;
- the saw's whine and its drop clank;
- a hit.

`Tools/import_temple_run_sfx.py` imports them. As with the other traps, they are heard only inside the interior.

**Spaces and respawns.** Ten new spaces (the chimney, the gallery and its alcove, the axe hall, the fire hall, the saw passage, the gauntlet and its axe chamber, the antechamber and its choked passage). The collapse replaced the old `SnakeExit` and is listed before the snake chamber. Six new spawns and Dev Checkpoints, in a fourth column on the checkpoint page (now 1400 wide, 18 pt): Temple Climb, Temple Pits, Temple Axes, Temple Fire, Temple Saws, Temple Gauntlet.

**Interior checks** (`CHUCK_CHULT_TEMPLE_INTERIOR_CHECK`, every Chult run):
- the route goes on from the snake chamber's arch through every section, with gaps where it is climbed or crossed on the walls;
- the rubble's first block stops a walking capsule, and the antechamber's way on is choked;
- the intrusion probes cover the new spaces (28 spaces, 43,003 probes, `intrusions=0`), as does the ground clearance check;
- 12 spawns, deaths in each new space mapping to its own, and swallows in each pit kind (not on the floors or the chimney's);
- the trap counts: 6 axes, 11 jets, 16 saw discs.

## The chained wall run (ChuckCharacter)

A jump during a side wall run (or in its coyote time) now kicks off at `WallChainOut` 230 cm/s across and `WallChainUp` 190 cm/s up, keeping his speed along the wall. Within `WallChainAirTime` (0.7 s) a wall reached at a glancing angle is caught as another side run (`TryWallSideRun(true)`), before the head-on climb is tried. Other wall jumps are unchanged (the chimneys still bounce head-on), and so is a side run with no jump.

Measured in the gallery (1.8 m wide):
- one side run covers 2.63 m along the wall;
- run, kick and run lands 7.35 m from the takeoff, so the 4.6 m pits leave room;
- the kick catches the far wall if it comes 0.35–1.25 s into the first run (estimated from the speeds, not measured trial by trial).

## Tests

`-ChuckChultTest -ChuckTempleRunTest` (`ChultTempleRunTest.cpp`): 16 stills (`Saved/Screenshots/.../Chult/Run*.png`), then the real character, with live input off (the bot drives the test stick):

| Trial | What it does | Uncooked result (`Local/temple-run-test-3.log`) |
|---|---|---|
| climb | from Temple Climb up the rubble (mantles), into the chimney, wall jumps to its lip, the pull-up | 6 mantles, 5 wall jumps, 1 hang, on the gallery floor in 10.4 s |
| pit_single | along the +S wall, jump at the lip: one side run | lands 0.3 m past the far lip (2.7 m from the takeoff) |
| pit_chain | the same, kick once 1.2 m in: the chained run | lands 2.6 m past the far lip |
| pit_twist | the broken-wall pit, kick only once the far wall is back | lands 2.6 m past |
| pit_fall | runs into the first pit | one death, back at Temple Pits |
| leap_short | a sprint leap down the middle at the long pit | falls (3.7 m of 4.6) |
| side_short | one side run, no kick, at the long pit | falls (3.0 m of 4.6) |
| axes | waits at the lone axe until a straight run is clear, runs; then the wave in one go | no hits, 0.6 s of waiting |
| axe_hit | stands in the wave's first axe's path | one hit, sanity 4, still on the causeway |
| fire | the lone mouth, the slalom straight down the middle behind its wave, the pairs | no hits, 1.0 s of waiting |
| fire_strip | stands in the strip a slalom jet doesn't reach, through two bursts | no hits |
| fire_hit | stands in the lone mouth's jet | one hit |
| saws | runs the passage rolling under each row | no hits, 12 discs rolled under |
| saw_hit | walks under the first row | one hit |
| gauntlet | all of it, reading each trap | no hits, no falls, 19.3 s (0.6 s waiting) |
| gauntlet_flow | leaves the first lip on the beat and never stops | no hits, no falls, 15.3 s from the lip; arrivals within 0.05 s of the design |

Verify-Package runs it (and the interior's own review, the snake and skeleton runs and the rest of the suite) on the packaged candidate. The menu suite has 27 points.

## Not done

- No person has played it. Untested by a human:
  - the difficulty;
  - the camera in the chimney and the 1.8 m gallery;
  - whether the timing feels like flow.

  A gamepad has not been tried.
- The sounds have not been listened to.
- The fire is billboarded flame (the torches' material), not a particle effect. It reads as a burst of flame, but its edges are jagged up close.
- The axes and saws are simple procedural shapes.
- The gauntlet's beat is tuned for his run pace. A walking player has to read each trap instead (always possible).
- The golden cigarette's stamina lock would make side runs longer and the pits easier; there are none in the temple.
- The way on from the antechamber is choked: the next part (the 2D maps 6–9) follows from there.
