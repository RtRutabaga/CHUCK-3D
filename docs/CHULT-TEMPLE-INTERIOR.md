# Chult — the temple interior

User request, October 20, 2026 (Claude): "start creating the temple interior, accessible from the central staircase door. this is going to be a large trap-laden fantasy temple dungeon, so the player will likely need to travel in and down into an inferred underground for it's scale to make sense with the temple exterior, but overall handle this as you see best fit". The user left the layout to the agent; this document is the plan and its first milestone. It is milestone 5 of [regions/CHULT.md](regions/CHULT.md).

## The 2D direction

Read-only sources: `docs/development/PHASE-6.md` and the temple maps in `chuck/chuck/assets/maps/` (`temple_entrance`, `temple_spikes`, `temple_skeletons`, `temple_darts`, `temple_snakes`, `temple_astral_wind`, `temple_shrine`, `temple_gauntlet`, `temple_sanctum`, `temple_rubble`). The 2D temple is a run of connected maps, broad rooms alternating with long narrow connectors, wall torches in every one, the route turning through the structure rather than running in one line:

| 2D map | What it is | 3D adaptation |
| --- | --- | --- |
| 1 Entrance hall | Monumental, torch-lit, safe | The way down (a hidden stair and a great well), then the Great Hall |
| 2 Spike corridor | Long connector, five spike bands, five skeletons | The spike corridor: five timed spike bands |
| 3 Skeleton hall | Broad, twelve durable skeletons round monumental piers | The pier hall, with an Astral rift crossed by collapsing slabs or hanging rings |
| 4 Dart corridor | Long connector turning west, eight dart walls | The dart gallery, turning west: eight serpent-mouth launchers |
| 5 Snake chamber | Twenty one-hit snakes | Built October 21: the snake chamber ([CHULT-TEMPLE-SNAKES.md](CHULT-TEMPLE-SNAKES.md)) |
| 6 Astral wind | Very narrow, winding, Astral fall cuts needing committed jumps | Built October 22 at the user's direction ([CHULT-TEMPLE-WIND.md](CHULT-TEMPLE-WIND.md)): four narrow legs and three bends beyond the trap run, a spike pit in each leg crossed by wall runs kicked back and forth, 24 Astral ruptures walked round |
| 7 Shrine hall | Twelve skeletons, guardian monument rows | Later |
| 8 Gauntlet | Spike bands, darts and Astral cells together | Later |
| 9 Final chamber | Fighter, wizard, ranger, beholder, skeletons; scripted Fireball | The hall is built, empty (October 22: the sanctum, [CHULT-TEMPLE-WIND.md](CHULT-TEMPLE-WIND.md)). The battle is later, staged as the 2D beats (not a boss fight for Chuck) |
| Rubble map, crawl, escape | Astral rubble, a crawlspace, the cutscene onto the ship | The rubble map and its crawlable crevice are built (October 26: [CHULT-TEMPLE-RUBBLE.md](CHULT-TEMPLE-RUBBLE.md)), reached by the Fireball cutscene. The escape onto the ship is later |

Skeletons first appear in the temple (PROJECT-BRIEF). They are creatures (Claude) and are the next step after this milestone: the pier hall and the spike corridor are laid out for them.

## Why it goes down

Seen from the plaza the central temple is three 5 m terraces with a 9 m sanctuary and a 44 m spire. A dungeon of the 2D game's size cannot fit inside that. The sanctuary's doorway therefore leads to a hidden stair inside the masonry (after the stair inside Palenque's Temple of the Inscriptions), which comes out at the top of a great square well sunk under the temple. A stair winds down the well's walls to a hall 14.6 m below the plaza, under the forecourt; from there the dungeon spreads out under the precinct. The player goes down about 30 m from the sanctuary floor.

## Layout (first milestone)

Canyon frame as in [CHULT-TEMPLE.md](CHULT-TEMPLE.md): S toward the temple, T across, Z above the plaza (`ChultTemplePlazaZ()`, about 17.6 m above world zero). The central temple's centre is S=78200.

| Space | Where (S, T in cm; Z above the plaza) | What is there |
| --- | --- | --- |
| Sanctuary cella | S 77810–78110, T ±260, Z 1500 | The old dark vestibule, now lined and lit by two torches either side of a doorway in its back wall |
| Hidden stair | Flight A S 78200→78800 (+S), Z 1500→1100; landing; flight B S 78800→77960 (−S), Z 1100→540 | Narrow (2 m) corbel-vaulted stairs inside the masonry, torch at each landing |
| The well | Inside S 77420–78620, T 500–1700 (12 m square), Z 960 down to the Astral at −2300 | A gallery along one wall at Z 540; a stair winds down the walls (five flights, corner landings, no rail) to a ring walkway at −1460 round an open pit; the Astral glows far below |
| Great Hall (map 1) | S 73400–77360, T 300–1900 (40 × 16 m), floor −1460, ceiling −560 (9 m) | Two rows of six piers, a processional way, idols, stelae, braziers, jars and urns; safe |
| Spike corridor (map 2) | S 69400–73400, T 900–1300 (40 × 4 m) | Five iron spike bands across the floor, each thrusting up for about a second in a 3 s cycle, alternating so the player stops between them |
| Pier hall (map 3) | S 66340–69340, T −300–2500 (30 × 28 m), ceiling −660 (8 m) | Eight piers; an Astral rift (5.5 m since October 20) across the whole hall, crossed only by five hanging iron rings (brachiation) |
| Dart gallery (map 4) | S 66720–67080, T −360 → −5160 (48 × 3.6 m), turning west | First a 2.7 m pit bridged by three cracked slabs that fall 0.6 s after he steps on each (run across); then eight serpent-mouth launchers alternating walls, each firing a dart across on its own rhythm with a click first; a dodge roll goes through a dart |
| The way on | T −5160 | Since October 21 an open arch into the snake chamber (S 65900–67900, T −5560 → −8360); its own way on, an arch in its +S wall, is choked ([CHULT-TEMPLE-SNAKES.md](CHULT-TEMPLE-SNAKES.md)) |

All of it is under the precinct or inside the temple's masonry. Underground ceilings stay at least 5.6 m below the plaza, under the trees' hidden trunk capsules and the plaza floor.

## Rules

- **Way in:** walk through the sanctuary's doorway. No fade, no prompt. The way back out is the same route.
- **Hazards** use existing Sanity damage (`TakeBite`, one Sanity per hit, a knock back out of the trap). Spikes cannot be rolled through; darts can (the dodge's immunity). Falling into the well's pit or the rift is an Astral death, as in the 2D Astral cuts.
- **Respawn:** a death returns Chuck to the start of the space he died in: the well's gallery, the Great Hall, the spike corridor, the pier hall or the dart gallery. Each has a Dev Checkpoint.
- **Light:** interiors are on lighting channel 1, lit only by their torches, as the exterior rooms are. While the camera is inside, the sky light dims and the jungle's height fog thins, so the dungeon's air is clear and dark.
- **Music:** since October 20 the user's "temple" plays inside, from the cella down (music region 6); "Chult 3" stays the exterior's.
- **Creatures:** since October 21 the skeletons: five in the spike corridor and twelve in the pier hall ([CHULT-TEMPLE-SKELETONS.md](CHULT-TEMPLE-SKELETONS.md)); and the snake chamber's twenty snakes ([CHULT-TEMPLE-SNAKES.md](CHULT-TEMPLE-SNAKES.md)).

## Integration with the exterior

- The masonry's horizontal faces inside the hidden stair and the well are cut open (`ChultTempleInteriorCutsAt`), and the precinct floor is left out over the well (`ChultTempleInteriorCutsGround`). Both are inside the solid temple, out of sight.
- The sanctuary's back wall gets the doorway; its old dark lining is gone.
- The bird swoops do not fire underground, and a fall below the world's kill height is still caught by it.

## As built (October 20)

Code: `ChultTempleInterior.h/.cpp` (layout, building, checks, review) and `ChultTempleTraps.h/.cpp` (`AChultTempleTraps`, the moving parts). `BuildChultTemple` calls `BuildChultTempleInterior` last.

**Construction.** Every space is lined from inside with faces looking inward (`Line`: floor, ceiling, walls with openings; stone to a dado, then the space's upper material, then a frieze band). Corridors and the hidden stair have corbel vaults (`Corbel`, `Flight`: three or four courses stepping in). Flights and the well's runs have visible steps without collision over a hidden ramp through the treads' middles, as on the exterior stairs (Chuck steps only 4 cm). The well's five runs are solid flights with a soffit and a fascia on the pit side; corner landings join them; at the bottom a 2 m ring walkway runs round an 8 m pit whose shaft drops to an Astral surface (`M_AstralDepth`) with a purple glow. Each stair end has a two-sided lintel closing its corbels' ends.

**Light.** 82 torches (iron wall brackets with the docks' flame and an unshadowed point light on lighting channel 1, flickering), four braziers among them, and four purple Astral glows (the pit and the rift). In `ChultTemple.cpp`'s room timer the interior counts as a room (sky light down to 15%, Chuck joins channel 1); deep inside it the sky drops further (to about 4%), the height fog and the temple's mist pools fade out, and the auto exposure may not lift the dark above EV −0.5 (`-ChuckInteriorMinEV=` overrides it for look tests). The timer now looks again for the sky and fog until they exist (the fall cutscene builds them after the set).

**Materials.** `Tools/create_temple_interior_materials.py` makes two new assets in `/Game/Art/ChultTemple`: `M_TempleDungeonStone`, the ruins' three CC0 scans projected as `M_RuinStone` does but as dry, darker, warm-grey stone with faint damp patches and rare moss on floors (the ruin material grows moss below 5 m of world height, where all of the interior lies), and `M_TempleVoid`, unlit black for grate holes, cracks, glyph cuts and throats (`M_Dark` is lit and reads grey by torchlight). `M_RuinStone` is kept for mossy accents in the rubble; `M_WeatheredPlaster` for the cella, the halls' frieze bands, cartouches and idols.

**Dressing.** Glyph panels and cartouches (the exterior rooms' bar-and-dot numbers), four serpent idols on plinths in the Great Hall (the exterior's modelled idol), four glyph stelae, braziers, fallen column drums, 17 tan jars and 8 clay urns (breakable, cigarettes as in the exterior rooms), three skulls (the CC0 skull) just short of the spike bands.

**Traps** (`AChultTempleTraps`):
- *Spike bands:* five iron grates, 1.6 m deep, across the 4 m corridor, 96 spikes each. Their points show 7 cm at rest. Each band's 3 s cycle: 1.55 s at rest, a 0.45 s rattle (sound), a thrust to 48 cm in 0.08 s (sound), held 0.67 s, drawn back in 0.25 s. Phases 0, 1.5, 0.4, 1.9 and 0.8 s, so running straight through meets a raised band; the player waits between them. Above 12 cm they hurt: one Sanity and a knock out of the band toward its nearer edge, also during a dodge roll (`TakeBite`'s new `bThroughDodge`).
- *Dart launchers:* eight carved serpent mouths, alternating walls along the 3.6 m gallery, each on a 3.2 s cycle (phases 1.15 s apart): a click, then two darts 0.3 s apart at 15 m/s across the gallery at Chuck's chest height (36 cm). A dart that meets his capsule costs one Sanity; a dodge roll goes through. Darts stick in the far wall for 1.2 s. Twenty pooled darts.
- *Cracked slabs:* four 1 m slabs bridging the 4 m rift. Each cracks when he first stands on it and falls 0.6 s later; running across works, stopping does not. They come back 5 s after falling once he is 7 m from the rift.
- *Rings:* four iron rings on chains over the rift by the +T wall, 1.4 m apart, 1 m above the floor (caught by a standing jump), registered with `AddChuckSwingGrip`; the last hangs 10 cm short of the far lip for the step-down.

**Sounds.** `Tools/gen_temple_trap_sfx.py` (stdlib, seeded) synthesises 17 originals with a stone-chamber reverb: spike rattle ×2 and thrust ×3, dart click ×2, shot ×3 and hit ×3, slab crack ×2 and fall ×2. `Tools/import_temple_trap_sfx.py` imports them to `/Game/Art/Audio/SFX`. Played spatialised (2.2 m falloff start, 22 m reach), only within earshot.

**Recorded trap sounds (2026-10-27).** User request: free sounds for the traps, a bit quieter. `Tools/build_temple_trap_sfx.py` (Blender 4.5.14 for its decoder and numpy) cuts all 32 trap sounds, the trap run's included, from CC0 recordings and writes them under the old names, so no slot changed. It supersedes `gen_temple_trap_sfx.py` and `gen_temple_run_sfx.py`. The sources are in `SourceAssets/Audio/SFX/Temple/Source/` (36 unmodified clips, about 28 MB, LFS), with URL, author and SHA-256 for each in its `manifest.json`:
- spike rattle: small chains shaken (BigSoundBank), a light iron tick (Kenney); spike thrust: a knife blade scraped on stone, an iron clank, a low thud;
- dart click: two dry wood knocks; shot: a bow's release and swishes (artisticdude); hit: an arrow into a target;
- slab crack: a tombstone pushed open (stone on stone) with grit; slab fall: a falling stone, slowed for weight;
- axe: a whoosh slowed to 72%, a creaking metal door for the pivot; fire: LEGIT Audio's fire staff hiss and roar; saw: an empty table saw's whine, a heavy chain and an iron clank for the drop; hit: a heavy punch and metal.

Each slot is levelled to the loudness of the synthetic sound it replaces (a 2 ms-block limiter keeps the peaks at -1 dBFS). The saw's whine is a further 5 dB down, the fire's hiss 3 dB, the axe's creak 3 dB, its roar and the slab fall 2 dB. `AChultTempleTraps::TrapVolume` (0.55, about -5 dB) then scales every trap sound. Each gets the same seeded stone-chamber reverb. Claude chose the cuts from envelope scans and cannot hear them, so none has been auditioned; change a cut or level in the script, re-run it, then run `Tools/import_temple_trap_sfx.py` (it imports every `SFX_Temple*.wav`) with `-AllowCommandletAudio`.

**Death and respawn.** `ChultTempleInteriorSwallows` (below the ring walkway in the pit, below the hall floor in the rift) is an Astral death in `AChuckCharacter::Tick`. `GetAreaStartLocation` asks `ChultTempleInteriorRespawn` first: the start of the space he died in. The interior lies above the world's kill height (the hall floor is about 3 m above world zero), so nothing else changes.

**Dev Checkpoints.** A third column: Temple Well, Temple Hall, Temple Spikes, Temple Piers, Temple Darts.

## Checks

- `-ChuckChultTest` (every run): `CHUCK_CHULT_TEMPLE_INTERIOR` (the build: 15 spaces, 185 steps, 82 torches, 5 bands, 8 launchers, 4 slabs, 4 grips, missing assets and sounds 0) and `CHUCK_CHULT_TEMPLE_INTERIOR_CHECK`:
  - 210 route points from the cella to the choked arch: a floor under each where the build says (on stairs, the ramp's height) and the real capsule swept clear between them; the choke closed;
  - 30,651 sphere probes on an 80 cm grid through every space: nothing of the exterior (masonry, floor, trunks) inside;
  - 544 probes: the plaza ground at least 4 m over every underground ceiling;
  - each spawn on its floor with room to stand, inside Chult, returning a death there to itself; deaths in the pit, the hall, the corridor, the rift and the gallery map to their spaces; deaths outside do not; the pit and the rift swallow, their lips and the slabs do not;
  - the rings where they hang, a standing jump's reach to the first, the body clear between them;
  - the traps' counts and each band's danger time (about a third of its cycle); every interior mesh on lighting channel 1 only.
- `-ChuckChultTest -ChuckTempleInteriorReview`: 17 stills (`Chult/Interior*.png`, each logging its average frame time with v-sync off; the spike and dart stills wait for a thrust and a dart in flight), then the real character:
  1. *descent:* from the top terrace in through the doorway, down both flights, along the gallery, down all five runs and the ring walkway into the Great Hall (16 legs, about 61 s at run speed);
  2. *spikes:* waits short of each band until it has just drawn back, runs through (no hits), then stands in the last band: exactly one hit;
  3. *slab_run:* runs across the slabs (all four fall behind him), walks away, all four come back;
  4. *slab_fall:* walks onto the first slab and stops: it falls, he is swallowed and returns to the pier hall's door;
  5. *rings:* jumps to the first ring, three leaps, steps down beyond the rift;
  6. *darts:* waits short of each dart line until the next volley is far off, runs across (no hits), stands in the last line (hit), then pushes into the choked arch (held);
  7. *well_fall:* runs off the gallery into the pit: swallowed, back on the gallery.
- `Verify-Package.ps1` requires the build and check lines in the Chult run and a separate interior review run with `CHUCK_CHULT_TEMPLE_INTERIOR_TEST failures=0` and every trial passed; `Verify-Menu.ps1` drives the five new checkpoints.

## Next

1. ~~Skeletons~~: done October 21 ([CHULT-TEMPLE-SKELETONS.md](CHULT-TEMPLE-SKELETONS.md)).
2. ~~The snake chamber (map 5)~~: done October 21.
3. ~~The trap run~~: done October 22 at the user's direction ([CHULT-TEMPLE-RUN.md](CHULT-TEMPLE-RUN.md)). The chamber's choked arch is now climbed (rubble, then a chimney) to an upper level of spike pits, axes, fire and saws, ending in a gauntlet and an antechamber.
4. ~~The Astral wind and the sanctum~~: done October 22 at the user's direction ([CHULT-TEMPLE-WIND.md](CHULT-TEMPLE-WIND.md)). The antechamber's arch opens on the winding tunnels (wall-run spike pits, Astral ruptures), which end on the gallery of the empty sanctum. Next: the adventurers' battle with the beholder there (staging, characters, the Fireball). The shrine hall (map 7) and the 2D gauntlet (map 8) remain unbuilt; whether they come is the user's call.
4. The final chamber's tableau, Fireball, rubble map and crawl: done October 26 ([CHULT-TEMPLE-BEHOLDER.md](CHULT-TEMPLE-BEHOLDER.md#the-cutscenes), [CHULT-TEMPLE-RUBBLE.md](CHULT-TEMPLE-RUBBLE.md)). Next: the escape cutscene onto the ship, from the 2D beats and the user's music.
5. ~~A temple interior score~~: the user's, since October 20.

## October 20 follow-up: score, sounds, the rift

User: "add this as temple soundtrack, then fix how when you're walking towards temple exterior you can hear the grinding of either darts or spikes. then, make it so the chain brachiation in the temple in the part its in is the only way to cross that part don't make it so you can walk around".

- **Score:** the user's `temple (1).wav` (stereo 16-bit 48 kHz, 132.0 s, natural fade at the end) is copied unmodified to `SourceAssets/Audio/TempleInterior.wav` and imported looping as `SW_ChultTempleInterior` (`Tools/import_chult_temple_interior_audio.py`). `DockGameMode` plays it while Chuck is inside the interior (`InChultTempleInterior`, 1.5 m hysteresis), cross-fading with "Chult 3" at the sanctuary doorway. `-ChuckTempleInteriorMusicTest` starts it 2 s before its end; `CHUCK_CHULT_TEMPLE_INTERIOR_MUSIC_CHECK` confirms the loop.
- **Trap sounds:** they played to anyone within about 25 m, and the spike corridor runs 14.6 m under the avenue. `AChultTempleTraps` now plays them only while Chuck is inside the interior. The review's `hearing` trial stands him on the avenue over the corridor (no trap sound may start) and then in it (they must).
- **The rift:** it had a cracked-slab bridge beside the rings. The rift is now 5.5 m (S 67450–68000; too far for a sprint leap of about 2.8 m or a side wall run) with five rings 1.4 m apart, and nothing else across it; a check sweeps for any floor over it. The cracked slabs moved to a 2.7 m pit at the start of the dart gallery (T −560 to −830, three slabs), and the launchers moved on to T −1100 − 540k. The Temple Darts spawn is now just inside the gallery.
