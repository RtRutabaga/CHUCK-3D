# Chult — the rubble map and the crevice

User request (Claude), October 26, 2026: "finish the beholder encounter, ending on the 'rubble map' with crawlable crevice, we'll do the entry to ship later".

This is the 2D game's Phase 6 rubble map (`docs/development/PHASE-6.md` §5, `assets/maps/temple_rubble.txt`, `tools/generate_temple_rubble.py`): where the wizard's Fireball throws Chuck. The temple's vault has caved in. The chamber is choked with fallen masonry and torn through by blocks of the Astral Sea. One clear, torch-lit lane winds from where he comes to, past a waypoint, to the only way out: a crawlspace in the far wall. The escape cutscene through it onto the ship is not built. The Fireball that leads here is in [CHULT-TEMPLE-BEHOLDER.md](CHULT-TEMPLE-BEHOLDER.md#the-cutscenes).

## The chamber (`ChultTempleRubble.h/.cpp`, `ChultTempleRubbleBuild.inl`)

- **Where:** beside the sanctum, at its floor's depth: frame S 74300–76700, T −8700 to −5100 (24 × 36 m), floor at the sanctum's (`TempleWind::HZ`). The vault is 10 m high, and the plaza is 11 m above it. It is not connected to the sanctum: the Fireball's white-out carries him there.
- **Walls:** the temple's masonry lining, with a moss frieze, a five-course corbel vault and pilasters (some broken off short).
- **The broken vault:** four ragged holes where the ceiling has fallen in (`TempleRubble::Caves`; the largest about 10 × 14 m, three more since the user's October 26 follow-up, "more ceiling collapses"). Each has its 40 cm edge and broken courses at the rim, a cavity of earth above (the mossy stone) whose top is lost in unlit black, and roots hanging through. Slabs of the vault lie tipped steeply under them.
- **The Astral:** nine ruptures in the floor, placed by hand off the lane. They are the sanctum's kind: a ragged hole stepped in 10 cm cells, the Astral's well under it, the oil haze, chips round the rim and a purple glow. Falling 40 cm into one is an Astral death, and he respawns where he came to.
- **The rubble:** 100 heaps scattered by a seeded stream, the first tries under the collapses where the most fell: about 160 turned blocks with collision (mossy where they were the vault's), vault slabs and 20 column-drum pieces. Gravel lies off the lane.
- **Every piece rests on something** (October 26 follow-up: "some of the rubble is impossible levitating blocks, make it natural"). Before, a heap's later pieces were set at the heap's height wherever they landed, so many hung in the air. Now:
  - Drums go down first, on the floor. Each block is tipped to lie along what is under it (pitch and roll fitted to the support at its ends and sides, with a little jitter). It is then set down so its corners, mid-edges and middle are on or above the support under them, the highest touching.
  - Where it would still hang more than 22 cm somewhere, it settles into the debris by up to two fifths of its height. Failing that it slides on, out from the heap's middle, until it rests (scree at a heap's foot), or is not placed.
  - Vault slabs lie from what is under one end to what is under the other. Past a wall, what is against the wall carries on (blocks lean on it).
  - The rubble test fails if any block's whole underside is more than 8 cm above what is under it (`ChultTempleRubbleFloating`).
  - There are fewer blocks than before the fix (162 against 221), because what cannot rest is no longer placed. Every piece keeps clear of the lane plus 35 cm, the ruptures' rims, the braziers and pots, the crevice's mouth and where he comes to. The heaps can be climbed (mantles, wall jumps); nothing requires it.
- **The lane:** 2.2 m wide, eight corners, about 56 m long. It runs from where he comes to (76250, −5600), down past the broken vault, and back across. It ends before the crevice's pile at the −S wall. Four braziers stand along it, and wall torches line it.
- **Sound:** nothing of the trap run (its gauntlet is only metres away through the rock) or of the sanctum's battle is heard here (user, October 26): the traps' and the battle's sounds are not played while he is in the rubble map, and the wizard's hum is silenced. The rubble test fails if any is.
- **Pots:** two by the lane, two cigarettes each (the Fireball leaves him at half Sanity). Old skulls lie in the rubble.
- **Music:** the temple interior's score, after the Fireball's silence.
- **Checkpoint:** **Temple Rubble** (`ETempleInteriorSpawn::Rubble`, the last entry in the dev menu's fifth column). Deaths anywhere here return him to it.

## The crevice

- **The mouth** (since the October 26 follow-up: "less like a constructed narrow entrance and more like an opening in rubble"): a gap at the foot of a pile of fallen blocks heaped 1.7 m out from the −S wall (`TempleRubble::PileS1`), 80 cm wide and 44 cm high (he is 65 cm standing).
  - The blocks either side are set square to the passage, turned a little about the vertical, so their faces are its walls. A vault slab across the gap and a broken lip at the front are its roof.
  - The rest of the fall is heaped on and round them by the same resting rule as the rubble; any piece that would reach into the passage is left out.
  - Small stones spill out of it. The old lintel, jambs, glyph panel and the two braziers beside it are gone.
  - **It glows faintly gold, as the golden cigarettes do** (`AChultTempleRubbleGlow`, the cigarette's `M_GoldenLeafAura`): a low haze in the gap, a faint wisp rising off the pile (seen from across the chamber), 14 motes drifting up out of it, a warm light breathing on its stones (the cigarette's 2.6 s breath) and a fainter one a metre inside.
- **The passage:** through the pile (1.7 m), then two straight runs lined in stone. One runs 5.4 m along −S, then it bends and runs about 4 m along −T. The faces he crawls along are straight. The lumps and spalls on its walls and roof and the pebbles on its floor are visual only.
- **The end:** a crack in the end wall, 13 × 32 cm. Behind it is a hollow of old timber, lit warm: the ship, for later (the 2D escape has "a light appears ahead", then a wooden room). `AtChultTempleCreviceEnd` marks the handoff point.

## Crawling (Chuck: `ChuckCharacter`, `Tools/build_chuck_v1.py`)

A new move, but not a new control. In a crawl space Chuck goes down on all fours by himself, and he stands again outside it once there is room.

- **Crawl spaces:** registered by the setting (`AChuckCharacter::RegisterCrawlSpace`, oriented world boxes): the run through the pile and the crevice's two runs, plus an apron 80 cm deep and 1.8 m wide before the mouth, so he is down before the gap's roof however he comes at it.
- **The gait:** `EGait::Crawl`. He crouches with the engine's crouch: the capsule is 34 cm tall instead of 65 (`SetCrouchedHalfHeight(17)`), at the clip's own pace (40 cm/s). No jump, sprint, roll or slash. The paws follow the clip (no foot IK). Leaving the space he un-crouches, and the movement component keeps trying while there is no room.
- **The camera:** close behind him and low (95 cm boom, 9 cm pivot, 84° FOV, lens level). It turns to his heading, and he stays visible down to 30 cm from the lens.
- **The clip:** `Crawl` (20 frames, 30 fps, looping), authored in `build_chuck_v1.py` after the sprint's quadruped pose. It is a rat's lateral-sequence walk (left hind, left fore, right hind, right fore, a quarter cycle apart), each paw down 72% of the cycle. The back is long and level and bends toward the planted forepaw. The head is low and forward, the ears back, the tail trailing low with a slow wave. He measures 32.7 cm at his highest (`Tools/review_chuck_clip.py`, `Local/CrawlReview/r2`). Imported alone with `Tools/Import-ChuckV1.ps1 -Clips Crawl`. `ChuckClipData.h` gains `CrawlSpeed/Period/Stride/StanceFraction`.

## Tests

`-ChuckChultTest -ChuckTempleRubbleTest` (`ChultTempleRubbleTest.cpp`; `-ChuckRubbleTrials=a+b`, `-ChuckTempleRubbleViewsOnly`):
- **Stills:** nine views (`Saved/Screenshots/.../Chult/Rubble*.png`).
- **lane:** the real character walks the lane's corners from the Rubble spawn to the mouth, standing, with no death.
- **crawl:** in at the mouth, to the end, then back out. It passes when he is on all fours with the low capsule in every frame inside, reaches the end, and stands again in the chamber.
- **rift_fall:** into a rupture: one Astral death, then back at the Rubble spawn.
- The temple interior's check covers the new spawn (on floor, room to stand, deaths return to it) and that the ground is at least 4 m over the chamber.

Verify-Package runs it and requires `CHUCK_TEMPLE_RUBBLE_TEST failures=0 lane=1 crawl=1 rift_fall=1` and the interior check with `spawns=21`.

## Not done, and known flaws

- **The escape onto the ship** (the 2D escape cutscene and Phase 7): the crevice ends at the crack.
- **Rubble shapes:** the rubble is boxes and cylinders in the temple's stone. There are no sculpted break faces, and close up they read as blocks.
- **Unplayed:** the crawl and the chamber have been played only by the test driver, not by a person, with keyboard or controller. Turning in the crevice's bend is untried by hand.
- **Lane walk:** at a walk it takes 76 s.
- **The crawl's look:** no paw contact sounds, and the paws can slide a little at speeds off its 40 cm/s.
