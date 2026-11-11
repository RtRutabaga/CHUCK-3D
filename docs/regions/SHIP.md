# The pirate ship — 3D adaptation plan

Written 2026-11-01 (Claude), on the user's request: "start building the ship/next part of the game. base your approach on how we've handled the chult adaptation from 2d". **Approved by the user, 2026-11-01**, with milestone 1 to start, and these answers to the questions below: the chest gives 40 cigarettes **and** starts the golden leaf's 45-second stamina lock; the sea and horizon rock round a still hull; the user will supply a ship track and the pirates' voice takes later (until then, sea and creaks only, and subtitles only). Claude builds the setting, as with the temple. Actual work and tests go in HANDOFF.

**Milestone 1 built, 2026-11-01 (Claude):** the escape and below decks. **Milestone 2 built, 2026-11-02 (Claude):** the weather deck, the rig and its climb, and the open sea rocking round the still hull (the sea's tilt rather than the deck's, with no camera roll). **Milestone 3 built, 2026-11-02 (Claude):** the crew (the chef and his chase, the seated pirate, the concertina player, the cheering pirate, the dancer, the two fencers, Jeffries) with their 2D first and repeat lines as subtitles, all keeping to the ship track's beat; voices still to come. **The captain and flag, 2026-11-02 (Claude):** the user moved the captain on deck from the start (in the 2D game he appears only at the trigger; what he does then is to be decided). He is a halfling on tall platform boots, from the user's reference pictures, and stands on the quarterdeck with his fists on his hips, now and then raising his cutlass. The user's mustachioed skull-and-cutlasses flag flies from the main truck. **The concertina version, 2026-11-02 (Claude):** the user's concertina recording of the ship track plays on the weather deck, from the concertina player, and the ship track below. Each takes over at the matching moment of the tune. **Fixes, 2026-11-02 night (Claude):** the flag moved to the spanker gaff's peak, larger; a pile of lumber with a small stone mouth and the oil haze replaces the temple blocks in the arrival compartment; hammocks lower (two no longer poke a lanyard up through the deck); the dancer off the grating; the captain reaches for and grips his cutlass properly. **The chest, the trigger and the storm, 2026-11-03 (Claude):** the user asked for a storm to roll in when the ending is triggered and to last a good while before the plank; the chest opens once and drops the golden carton, the seven conditions are tracked, and when they are met a storm rolls in over a minute and keeps on, with its own sounds (the plank not yet). What is built: [../SHIP.md](../SHIP.md); tests: HANDOFF.

## Direction and sources

Read-only source root: `C:\Users\ashsm\OneDrive\Documents\CHUCK-game`.

- `docs/development/PHASE-6.md` §6: the escape. Chuck crawls a tight stone passage, a light appears ahead, he comes out into a wooden room, and a hole shows the open sea. He has reached a ship without knowing it. The 2D escape starts the ship's music.
- `docs/development/PHASE-7.md`: the ship. Arrival compartment, lower hold, galley, crew quarters, captain's cabin, deck. Exploration, dialogue and lively animation over combat. The captain's accusation, the plank and the fall into the Nine Hells.
- `docs/design/pirate ship.png`: the deck's composition reference. A long, round-ended two-masted hull with a centre hatch, posts along the rail, six gunports on the side and a bowsprit.
- `chuck/chuck/assets/maps/ship_{deck,lower_hold,galley,crew_quarters,captain_cabin,exterior_deck}.txt`: spatial and pacing references (`ship_deck` is the arrival compartment). They are not grids to extrude.
- `chuck/chuck/data/dialogue/ship_{galley,crew_quarters,exterior_deck}.json`: every line aboard. They are quoted exactly below. No line is added or rewritten.

As in Chult, the phase documents are adaptation evidence. Their engine, tile, ladder-prompt and Ashtray details are not 3D instructions.

## Changes from 2D

| 2D | Proposed 3D |
| --- | --- |
| Six separate maps joined by doorways and `Climb ladder?` prompts | One continuous ship at sea, built at human scale: a hold, a lower deck (arrival compartment, galley, crew quarters, captain's cabin) and the weather deck. Ladders and hatches join them, climbed with the existing ladder move. No prompts or loading. |
| Ashtrays | Local respawn: the arrival compartment, the hold and the deck, each with a Dev Checkpoint. |
| Escape cutscene from the crawlspace | An in-engine cutscene from the rubble crevice's crack of lamplit timber (`AtChultTempleCreviceEnd`) into the arrival compartment. Esc skips. It starts the ship's music. |
| Top-down rocking, animated wave tiles | Open sea with moving waves to the horizon. The ship rocks gently (see [Ship motion](#ship-motion)). |
| Pirate sprite animation synced to the shanty | Pirates built with the human NPC pipeline. They move to the music's beat: richer than earlier NPCs, as Phase 7 asks. |
| Captain's chest: Premium Buhetian Halfling Leaf, +40 cigarettes | A one-time golden carton of 40 cigarettes. Whether it also starts the golden leaf's 45-second stamina lock is a question for the user (below). |
| Plank sequence and Hell fall in 2D | In-engine cutscenes after the FallCutscene pattern, reusing the fall's music ("Fall Cutscene theme"), as Phase 7 §9 asks. |
| One-foot Chuck | Our 65 cm Chuck. Bunks, the mess table, the captain's bed and the crew loom over him. |

Kept as in 2D: the order of discovery; the hold as the ship's only real fight (rats); the chef's chase that cannot be fought; first and repeat lines for every important NPC; Jeffries as the one sailor who half sees the collided worlds while the crew explain them away; the ending gate; no explanation of anything.

## The ship

An original two-masted ship after the reference: about 34 m on deck and 9 m in the beam. The bow is forward (the 2D maps' east), with a bowsprit, and the stern is aft with a raised quarterdeck and the helm. Six gunports a side, lidded and dark (no guns are worked). It is built in code like the jungle wreck (`ChultWreck.cpp`), from existing wood, canvas, rope and iron materials. It sails on its own stage, far from Chult, as the fall cutscene does.

Inside, from bottom to top:

| Space | 2D source | What happens in 3D | Traversal |
| --- | --- | --- | --- |
| Arrival compartment | `ship_deck.txt` | A small lamplit sail locker amidships on the lower deck. Portholes look onto open sea (the 2D's "hole reveals the open sea"). Doors fore and aft and a hatch with a ladder down. | Coiled cable and sailcloth to climb over. |
| Lower hold | `ship_lower_hold.txt` | Cargo, sacks, barrels and crates (terrain, never broken), the pantry's shelves with breakable jars, and 16 ship rats that notice and chase him as the sewer's do. | Mantles and jumps between stacked cargo; a raised route over the rats. |
| Galley | `ship_galley.txt` | A working galley, forward. The chef sees him, says his line and chases him with a cleaver. Chuck cannot fight him; he gets away through the lanes between counters and stores, under tables, or out of the galley. | Rat-sized routes under counters; shelves to climb out of reach. |
| Crew quarters | `ship_crew_quarters.txt` | Hammocks slung overhead, a big round mess table, and a seated pirate swaying with his mug. The ladder up to the deck and the passage aft to the cabin. | The table and benches; hammocks as an optional climb. |
| Captain's cabin | `ship_captain_cabin.txt` | Under the quarterdeck, with stern windows. The captain is away. A brass-trimmed bed, a rug, and the chest. Scratching or interacting opens the lid once. | Furniture to climb. |
| Weather deck | `ship_exterior_deck.txt`, `pirate ship.png` | The visual centrepiece, in open sea and sun: the deck cast, Jeffries tied to the mainmast, the helm aft. | Optional: ratlines up the shrouds to the mast tops, a brachiation run along the yards, and a view of the sea. |

Chuck's moves get real use aboard without adding any. Ladders join the decks. Climbing, mantles and jumps work the hold's cargo and the cabin's furniture. Rat routes get him through the galley chase. The rigging gives an optional climb and brachiation route above the deck. No new moves.

## The crew and their lines

Pirates are new humans from the MPFB pipeline (`Tools/build_npc_humans.py`). They need new outfit pieces: tricorns shaped as Phase 7 asks, so a turned head never hides the face, plus headscarves, sashes, waistcoats and a captain's coat. New props: a concertina, tankard, mug, cutlasses and a cleaver. Strand hair and beards come from the existing CC0 pack. Animation comes from CMU motion capture where a take fits (dance, walk, sitting), and otherwise from authored clips like the heroes' (fencing, concertina, the chef's chase). It is paced to the music's beat.

Their lines, exactly as in the 2D data:

- **Chef** (galley), on noticing him: "Damn! Another rat got in, come er' you little bugger!"
- **Seated pirate** (crew quarters): first "Well I'll be. Never seen a rat wearin' a coat before."; then "You seem like a good listener, I bet all of the crew would love to bend your ear"
- **Concertina player:** first "Requests are considered in the order they are shouted."; then "Still considering it."
- **Cheering pirate**, with a tankard he raises from time to time: first "Don't mind Jeffries over there tied to the mast. He's gone mad. Too much sun, I think."; then "You're not smoking the captain's cigarettes, are you!?" / "Don't let the captain catch you sniffing around in his quarters!"
- **Dancing pirate**, dancing a jig: first "Deck's moving. Saves me half the work."; then "Other half's mine."
- **Two fencing pirates**: they wander the deck while they fence. Touching their blades costs Sanity. They have no lines.
- **Jeffries**, tied to the mainmast and always straining at the ropes: first "The sea is wrong, the land is wrong. Open your eyes!" / "Half the crew is gone... lost to the purple. They don't even remember them."; then "Don't go into the purple! DON'T GO INTO THE PURPLE!" / "Why don't they see!?"; on the plank, "It's back! The purple is back!"
- **The ending:** a crewman calls "Captain on deck!", then "...". The captain: "That's the rat who stole my Premium Buhetian Halfling Leaf." / "Put him on the plank." A pirate: "But we've all grown fond of the smoking rat, Cap." Then "..." and the captain's "No."

As with the heroes, the lines are spoken in voice takes the user supplies, with subtitles. Until a take exists, the line shows as a subtitle only. Chuck never replies.

**Voiced, 2026-11-11 (the user's ElevenLabs takes):** every pre-storm line above is spoken: the seated pirate, the concertina player, the cheering pirate, the dancer, Jeffries and the chef (his shout as he first sees the rat in the galley). From the third talk on, each of the four deck and mess pirates only says **"Arrgh!"** (user's direction); Jeffries keeps repeating his repeat lines. **The captain** (on the quarterdeck from the start) can now be talked to before the storm, with the user's own lines: first "Oi! There ye are, cabin boy! What're ye doin' skulkin' about? Get to work!", then "And keep yer grubby paws off the cigarettes in me cabin! Catch ye nickin' even one, and there'll be HELL to pay!", then "Arrgh!". Talking to him is not one of the seven conditions. In the storm he is at the helm and can't be talked to, as with the rest of the crew at their stations. Takes: `SourceAssets/NPCs/Voice/<NPC>/source/`; lines in `SourceAssets/NPCs/dialogue.json` (`PirateSeated`, `PirateConcertina`, `PirateCheering`, `PirateDancer`, `PirateChef`, and the `talk` lists of `Jeffries` and `PirateCaptain`).

## Ship motion

The deck rocks gently with the sea, and the deck cast's movement keeps to the music. Proposed: the hull stays fixed in the world and the sea, sky and horizon roll and pitch around it, by about 1.5°, with a slight camera roll. Hanging lamps, hammocks, ropes and the sails swing with it. To the player this reads as the ship rocking, but nobody's feet, no ladder and no grip ever move, so walking, climbing and NPC footing stay exact. If it reads poorly in the cooked game, the fallback is a truly moving hull carrying Chuck as a moving base, which needs its own traversal tests.

The sea is an open-ocean surface of moving waves out to the horizon, with foam along the hull. Phase 7 asks for more than Waterdeep's moving dots.

## The ending

**Changed by the user, 2026-11-07/08:** no plank walk. The plan that replaces step 2 below (the reef points on the main, the fore rigging carried away, the golden-leaf leap to the foremast, the cutscene on the fore topsail yard and the fall into a Phlegethos rupture) is [SHIP-STORM-ENDING.md](SHIP-STORM-ENDING.md), not built yet. Step 1 and step 2 below are kept only as the history of the first plan; step 3 still stands.

The ending starts only once the 2D's seven conditions are met: the chef has chased him, he has talked to the seated pirate and to the four deck pirates (concertina, cheering, dancing, Jeffries), and he has opened the chest.

1. **Captain on deck** (cutscene, Esc skips): the captain comes up the midship hatch and crosses the deck to the helm, the camera following. A crewman calls out, and then the accusation, the order, the objection and the refusal follow, with "..." beats between them.
2. **The plank:** the crew run out a broad plank through the starboard rail, with the captain and the objecting pirate on either side. Chuck is marched onto it and gets no control back. As he steps onto it, the sea fills with drifting fragments. Some are pieces of the Astral Sea, in the same Astral look as the sewer, temple and wreck ruptures. Others are pieces of Hell seen from above: cracked basalt plates with branching lava channels and molten vents, each laid out differently. They slide past the ship and do not collide with it. Jeffries shouts his warning. Chuck walks to the end. The captain follows him, waits for a Hell fragment to pass below, and kicks him into it.
3. **The fall into the Nine Hells:** a long fall through hot open air on the fall cutscene's music and timing. A great volcano rises into view from below, peak first. He lands on a basalt plain with sparse lava fissures, blinks out into Astral light and is summoned back (the Chult choreography), looks left, right and down, and lights a cigarette. The scene holds without control: the end of Phase 7 and the start of Phase 8. Like the fall at the end of the demo, it ends on a card until Phlegethos exists.

## Milestones and ownership

The brief gives the setting to Codex and the character, creatures, animation, traversal, camera, imports and cutscenes to Claude. In Chult, the user later had Claude build the temple's setting too. Both are open here. The user's request this time was to Claude.

| Milestone | Contents | Acceptance before promotion |
| --- | --- | --- |
| 1: escape and below decks | The crevice-to-ship escape cutscene; the hull; the arrival compartment, hold, galley, crew quarters and cabin; ladders; sea and sky outside the portholes; the hold's rats and jars; respawns and Dev Checkpoints. No pirates yet. | The escape plays and Esc-skips into control. Every space can be reached and left. Collisions, ladders and the interior camera are verified, and the rats are avoidable. |
| 2: the weather deck | The deck, masts, sails and rigging; the open sea and the rocking; the optional rigging route; Jeffries's mast and the helm. | The ladder between decks works both ways. The rigging route and its recovery work. The rocking is reviewed in the cooked game. |
| 3: the crew | All the pirates, their outfits and props, and their animation to the beat; the chef's chase; the fencers' damage; first and repeat lines; the voices. | Every conversation's first and repeat lines are checked. The chase is escapable, the fencers' hits are measured, and nothing passes through the hull. |
| 4: the chest and the ending | The chest; the seven-condition gate; the captain, the plank, the fragments and the kick; the fall into the Hells and the arrival tableau. A `Captain Arrival` Dev Checkpoint sets the gate. | The gate opens only when all seven are done. The ending plays in full and Esc-skips cleanly. |

Each milestone is built, verified with Verify-Package and promoted with `Tools/Promote-Build.ps1`, with the tests recorded in HANDOFF.

## Questions for the user

1. **Approval** of this plan, including one continuous ship rather than separate maps.
2. **Music:** the 2D ship theme was generated (`ship_shanty.py`, which we don't run). Will you supply a ship track, as for Chult and the temple? Until then the ship is quiet except for the sea and the ship's creaks.
3. **Voices:** will you supply takes for the pirates' lines, as for the heroes?
4. **The chest:** 40 cigarettes only, or 40 cigarettes plus the golden leaf's 45-second stamina lock?
5. **Rocking:** sea and horizon rocking around a still hull (proposed), or a truly moving deck?

## Verification during implementation

- The escape hands over from the rubble map with Sanity unchanged, no held input and no replay on respawn. Esc skips it.
- Every space is entered and left, every ladder works both ways, and there are no invisible walls at open-looking doors.
- The camera stays usable in the low rooms (2.1 m between decks) at rat height and from above.
- The rats are avoidable and the jars break into cigarettes. Crates and barrels are never breakable.
- The chef can always be escaped and never traps Chuck in a corner. The fencers hurt only where their blades meet him.
- Each NPC's first line comes once and the repeat lines after it. The gate counts all seven conditions and nothing else.
- The chest rewards once, including across respawns.
- The plank, the fragments and the fall play without player control, and Esc lands at the Phase 8 boundary.
- Measure performance in the deck's busiest view with the sea, sails and crew.
