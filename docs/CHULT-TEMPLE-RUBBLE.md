# Chult — the rubble map and the crevice

User request (Claude), October 26, 2026: "finish the beholder encounter, ending on the 'rubble map' with crawlable crevice, we'll do the entry to ship later".

This is the 2D game's Phase 6 rubble map (`docs/development/PHASE-6.md` §5, `assets/maps/temple_rubble.txt`, `tools/generate_temple_rubble.py`): where the wizard's Fireball throws Chuck. Here it is the sanctum after the Fireball: its vault partly caved in, its gallery and stairs broken, its fall heaped under the holes and torn through by the Astral Sea. One clear lane, lit by the sanctum's own fires, winds from where he comes to the only way out, a crawlspace under the stair. The escape through it onto the ship is in [SHIP.md](SHIP.md). The Fireball that leads here is in [CHULT-TEMPLE-BEHOLDER.md](CHULT-TEMPLE-BEHOLDER.md#the-cutscenes).

## The hall fallen in (`ChultTempleRubble.h/.cpp`, `ChultTempleRubbleBuild.inl`)

User request, November 4, 2026: "make the rubble map look more convincingly like the temple sanctum had partially collapsed in. there should be rubble from the balcony, blocked off stair cases, and only torches where there were torches in the temple sanctum map".

Follow-up the same day: "make sure all the rubble reads as having fallen from somewhere discernible such as a roof collapse or structure collapse. I don't want unexplained piles of rubble anywhere, and make sure there's no unrealistically stacked piles or floating rubble ... If you encounter a difficult time closing access to some parts of the map while following these rules you can close access with new Astral sea areas instead".

Second follow-up (November 4): "Add more damage from the fireball, burn singe on rubble and the snake statues, cracks running through the walls, pillar collapse below roof breaks, and some of the water world astral rip that the wizard was trying to seal in the beholder encounter", then "knock some of the torches over" and "don't have all of the torches lit, some would have gone out in the explosion". See [The Fireball's damage](#the-fireballs-damage).

Until then the rubble map was a separate 24 × 36 m chamber beside the sanctum, with its own walls, braziers and torches. Now it is **the sanctum itself after the Fireball**:

- **Built by the sanctum's own builder.** `TempleWindBuild::Sanctum` takes an `FSanctumFall` (what came down). The rubble map builds the hall with it, then sets it 40 m under the real sanctum (`TempleRubble::DropZ`); the white-out carries him there. The battle's hall is never touched, and the two cannot drift apart: the same walls, gallery, stairs, colonnades, guardian idols, dais, colossi, carved disc, glyphs and floor. With no `FSanctumFall`, the sanctum builds exactly as before (its architraves are now made in spans between piers, the same shape).
- **Torches:** only the sanctum's, and since the second follow-up only some still burn ([below](#the-fireballs-damage)). The rubble map's own wall torches and lane braziers are gone. What is lit is what the sanctum lights, where the sanctum has it: wall torches down both long walls and along both stairs, the gallery's ends, the end wall, a torch on each standing pier, the processional's braziers and the four great fire bowls. The torches on the two fallen piers and on the gallery's back wall over the fallen stretch went down with them.
- **The balcony (gallery):** its middle 16 m (S 77900 to 79500) fell with the vault over it into the Astral pool 13 m below.
  - Only its runs at the stairs' heads still stand. Each broken end has a length of slab snapped and sagging from the break, courses and spalls jutting from the faces, and the balustrade's top course cracked off. The four gallery piers under the fallen stretch are broken off at 2.6 to 6.8 m on their islands.
  - The arch he came in by is now high in the wall over nothing, choked with blocks to its lintel.
  - Most of what fell is gone into the Astral (the pit reads as open void, so nothing is stood in it). Chunks and finials lie on the islands round the piers' feet. What was flung across the pool lies at its far rim: slabs teetering over the edge, a block or two, a balustrade length and a finial on the floor (`GalleryFall`).
- **The stairs broken:** the middle 3.1 m of each flight, from 4.9 to 6.5 m up (T −5444 to −5135), fell with the masonry under it into a new Astral rupture torn in the floor beneath (`TempleRubble::StairBreak0/1`, `FSanctumFall::StairBreaks`). The flight below ends at the break, where the snapped tread hangs a little over it. The flight above is out of reach: 1.6 m higher and 3.1 m away. Both broken ends are faced in ragged courses and spalls. What fell went into the Astral; a few pieces of tread and masonry lie on the floor by the rupture's rim, one tipped over its edge (`StairBreakDebris`). A hidden wall across each flight at the far side of its break, from the floor to 4.5 m over the gallery's floor, keeps a wall run from crossing. (A first version heaped rubble up each flight instead. It read as stacked towers of blocks with nothing above to drop them, and was replaced on the user's follow-up.)
- **The piers:** two are brought down (the −S row's fourth, the +S row's second; `TempleRubble::FallenPiers`), each broken off over his reach. Its shaft lies in three lengths and its capital, out from the stump, the way that keeps them clear of the lane and the fittings. The architrave and the upper beam stand only between piers still standing.
- **The broken vault:** four ragged holes (`TempleRubble::Caves`), each with its 40 cm edge, broken courses at the rim, a cavity of earth whose top is lost in black, and roots hanging through. They are over the gallery's middle and the pool before it (what brought it down), the nave over the sea tear, the crevice's pile, and before the dais. None reaches the colonnades' beams. The heaps lie thickest under them.
- **The Astral:** the sanctum's pool before the gallery and three of its aisle ruptures are still open. The sea tear is open too (since the second follow-up); the −S aisle's rupture lies buried under the crevice's pile (`TempleRubble::OpenSanctumRifts`). The fall tore four more (`TempleRubble::Rifts`): two in the aisles and the two under the stairs' breaks. Falling 40 cm into any is an Astral death, and he respawns where he came to.
- **The rubble, and where it came from:** every piece lies where something came down (`ChultTempleRubbleSourced`), and the rubble test fails on any that does not (`unsourced=0`). The sources are:
  - under or beside a hole in the vault: heaps of the vault's blocks and slabs, only under the holes, no more than 1.4 m up, kept off the lane, the ruptures, the hall's fittings (piers, wall pilasters, idol plinths, braziers, the altar and the colossi's plinths, the dais's steps, the stairs, the pool's end) and where he comes to;
  - round a fallen pier's stump, where it lies;
  - the gallery's fall, out to the pool's rim;
  - a stair's break;
  - the crevice's pile, under its own vault hole.
  
  There are about 90 blocks in all. Every piece rests on something: the floor, the dais, the treads, or the blocks under it (`ChultTempleRubbleBaseAt`, `ChultTempleRubbleFloating`). Gravel lies only where something came down.
- **The lane:** 2.2 m wide, about 43 m. It runs from where he comes to (the +S aisle beside the dais, facing the gallery end, past a great fire bowl), up the aisle, across the hall between the piers, and up the −S aisle to the crevice's mouth.
- **Sound:** nothing of the trap run or of the sanctum's battle is heard here (user, October 26), as before.
- **Pots:** two by the lane, two cigarettes each (the Fireball leaves him at half Sanity). Old skulls lie in the rubble.
- **Music:** the temple interior's score, after the Fireball's silence.
- **Checkpoint:** **Temple Rubble** (`ETempleInteriorSpawn::Rubble`, the last entry in the dev menu's fifth column). Deaths anywhere here return him to it.

## The Fireball's damage

Measured from where it burst: the beholder's place before the dais (`TempleRubble::BlastS/BlastT`).

- **The fires** (`FSanctumFall::Fate`):
  - The torches within about 17 m of the blast were thrown off their mounts: 7 (the two piers nearest the dais, the +S row's third, the long walls by the dais, the end wall). Each mount is left with its plate and a snapped arm stub. The torch lies on the floor below it, half of them still smouldering with a small flame (`FallenTorch`).
  - The torches between 17 and 19.5 m were blown out: 4, their cups black.
  - The rest still burn: the processional, the stairs, the gallery, the piers toward the pool.
  - Of the great fire bowls before the dais, the −S one was knocked over: its plinth stands, its stem and bowl lie off it toward the nave, and its embers burn on the floor beyond with ash round them (`TippedBowl`). The +S one went cold. The outer two still burn, and the processional's four braziers.
- **Soot** (`M_TempleScorch`, a deferred decal from `Tools/create_temple_scorch.py`; about 50 placed by `BuildRubble`):
  - on the floor and the dais under the blast;
  - up the end wall behind it;
  - on the serpent colossi (their sides toward the blast and their coils' tops) and lightly on the guardian serpents along the processional;
  - on the 26 largest pieces of rubble within 20 m, fainter further off;
  - under every knocked torch, and over its mount where the flame was torn off.
  - The soot is near-black at the core, with a heat-tinted brown fringe, smoke-broken edges and streaks thrown outward. It is fully rough with no specular, because on this dark stone the loss of sheen is most of what reads. No decal box comes near the lane, since Chuck's mesh takes decals.
- **Cracks:** thin black lines that wander, taper and sometimes fork. They run down every wall from under the corbels, up from the floor near the blast (the end wall and the long walls' dais end), up and down the walls from both stair breaks, and across the vault from each hole's rim. They stay off the crevice's opening and the walled arch.
- **Piers under roof breaks:** two more vault holes, over each fallen pier, so each stump has its break above it, with earth and roots. The vault and the beam it carried came down with the pier. The nave's hole and the crevice's moved to make room.
- **The sea tear:** the rip into the water world that the wizard was sealing is open, as in the sanctum, with a vault slab fallen half over its +T side. A fall into it is an Astral death. Its sea is its own mesh (`ChultTempleInteriorSeaLow`), its plane set 15 m under this hall's floor (`SeaZ`, `Gain` 0.55).

## The crevice

- **The mouth:** since November 4 it is under the −S stair, where the flight is 2.2 m overhead (T −5950). It is a gap at the foot of a pile of fallen blocks heaped 1.7 m out from the stair's masonry (`TempleRubble::MouthS`, `PileS1`), 80 cm wide and 44 cm high (he is 65 cm standing).
  - The blocks either side are set square to the passage, turned a little about the vertical, so their faces are its walls. A vault slab across the gap and a broken lip at the front are its roof.
  - The rest of the fall is heaped on and round them by the same resting rule as the rubble; any piece that would reach into the passage is left out.
  - Small stones spill out of it.
  - **It glows faintly gold, as the golden cigarettes do** (`AChultTempleRubbleGlow`, the cigarette's `M_GoldenLeafAura`). There is a low haze in the gap, a faint wisp rising off the pile (seen from across the hall), 14 motes drifting up out of it, a warm light breathing on its stones (the cigarette's 2.6 s breath) and a fainter one a metre inside. It has been about half as bright since October 27 (user: "dimmer").
- **The passage:** through the pile (1.7 m), then two straight runs lined in stone. The first runs 7.4 m along −S: under the stair (the stair's masonry face and plinth are opened for it), through the hall's −S wall and into the rock. Then it bends and runs about 4 m along −T. The faces he crawls along are straight. The lumps and spalls on its walls and roof and the pebbles on its floor are visual only.
- **The end:** a crack in the end wall, 13 × 32 cm, with a hollow of old lamplit timber behind it. `AtChultTempleCreviceEnd` starts the escape onto the ship ([SHIP.md](SHIP.md)).

## Crawling (Chuck: `ChuckCharacter`, `Tools/build_chuck_v1.py`)

A new move, but not a new control. In a crawl space Chuck goes down on all fours by himself, and he stands again outside it once there is room.

- **Crawl spaces:** registered by the setting (`AChuckCharacter::RegisterCrawlSpace`, oriented world boxes): the run through the pile and the crevice's two runs, plus an apron 80 cm deep and 1.8 m wide before the mouth, so he is down before the gap's roof however he comes at it.
- **The gait:** `EGait::Crawl`. He crouches with the engine's crouch: the capsule is 34 cm tall instead of 65 (`SetCrouchedHalfHeight(17)`), at the clip's own pace (40 cm/s). No jump, sprint, roll or slash. The paws follow the clip (no foot IK). Leaving the space he un-crouches, and the movement component keeps trying while there is no room.
- **The camera:** close behind him and low (95 cm boom, 9 cm pivot, 84° FOV, lens level). It turns to his heading, and he stays visible down to 30 cm from the lens.
- **The clip:** `Crawl` (20 frames, 30 fps, looping), authored in `build_chuck_v1.py` after the sprint's quadruped pose. It is a rat's lateral-sequence walk (left hind, left fore, right hind, right fore, a quarter cycle apart), each paw down 72% of the cycle. The back is long and level and bends toward the planted forepaw. The head is low and forward, the ears back, the tail trailing low with a slow wave. He measures 32.7 cm at his highest (`Tools/review_chuck_clip.py`, `Local/CrawlReview/r2`). Imported alone with `Tools/Import-ChuckV1.ps1 -Clips Crawl`. `ChuckClipData.h` gains `CrawlSpeed/Period/Stride/StanceFraction`.

## Tests

`-ChuckChultTest -ChuckTempleRubbleTest` (`ChultTempleRubbleTest.cpp`; `-ChuckRubbleTrials=a+b`, `-ChuckTempleRubbleViewsOnly`):
- **Stills:** 20 views: since the second follow-up also the sea tear, a scorched colossus, a knocked torch, the tipped bowl and a cracked wall. The first 15 are (`Saved/Screenshots/.../Chult/Rubble*.png`): arrival, the hall from high, the fallen gallery from below and above, both broken stairs, the nave to the dais, a vault hole, both fallen piers, the lane, the mouth near and low, the walled arch, the dais.
- **lane:** the real character walks the lane's corners from the Rubble spawn to the mouth, standing, with no death.
- **crawl:** first a trace low along the passage from the apron to the bend must meet nothing (`CHUCK_TEMPLE_RUBBLE_PASSAGE_HIT` otherwise). Then in at the mouth, to the end, and back out. It passes when he is on all fours with the low capsule in every frame inside, reaches the end, and stands again in the hall.
- **rift_fall:** from the lane's first corner into the sanctum's +S aisle rupture: one Astral death, then back at the Rubble spawn.
- The layout logs `in_passage=0` (no block in the passage or on the apron). The test fails on any floating block (`floating=0`), any block away from where something came down (`unsourced=0`) or any trap or battle sound heard (`heard=0`).
- The temple interior's check covers the spawn (on floor, room to stand, deaths return to it) and that the ground is at least 4 m over the hall.

Verify-Package runs it and requires `CHUCK_TEMPLE_RUBBLE_TEST failures=0 floating=0 heard=0 lane=1 crawl=1 rift_fall=1` and the interior check with `spawns=21`. Its interior line expects `wind_glows=55` since November 4 (the fallen hall's Astral lights, the sea tear's daylight and the spilled embers' glow).

## Not done, and known flaws

- **The sea tear's sea** reads paler and more sky-like than the sanctum's own, even at `Gain` 0.55; why is not understood.
- **Soot** is decals only: no charred colour in the stone itself, no smoke and no embers glowing in the soot.
- **Cracks** are flat black lines (no depth) and cross the wall pilasters' backs unseen.
- **Rubble shapes:** the rubble is boxes in the temple's stone. There are no sculpted break faces, and close up they read as blocks.
- **The stairs' breaks** are straight cuts through the flight with spalls on their faces, not a ragged collapse.
- **The gallery's sagging ends** are single slabs; one reads as a thin dark plane from below.
- **Reachability:** the gallery's standing runs and the stairs above their breaks cannot be reached. The heaps elsewhere can be climbed (mantles, wall jumps); nothing requires it, and climbing onto the pool's debris ends in the Astral.
- **Cost:** the hall is the sanctum's geometry again (about 400,000 interior triangles in all, cracks included), fewer fire lights than the sanctum now, and about 50 decals. Not measured for frame time in the packaged game.
- **Unplayed:** the crawl and the hall have been played only by the test driver, not by a person, with keyboard or controller.
- **The crawl's look:** no paw contact sounds, and the paws can slide a little at speeds off its 40 cm/s.
