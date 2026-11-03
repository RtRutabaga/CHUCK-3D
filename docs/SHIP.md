# The pirate ship

User request, November 1, 2026 (Claude): "start building the ship/next part of the game. base your approach on how we've handled the chult adaptation from 2d". As with Chult, a region plan came first: [regions/SHIP.md](regions/SHIP.md), approved the same day with milestone 1 to start. This document records what is built. Actual tests are in [HANDOFF.md](HANDOFF.md) and [agent-handoffs/CLAUDE.md](agent-handoffs/CLAUDE.md).

The 2D direction is Phase 6's escape cutscene and Phase 7's ship (`docs/development/PHASE-6.md` §6, `PHASE-7.md`, `assets/maps/ship_*.txt`, all read-only).

## Milestone 1: the escape and below decks

### Where it is

The ship sails on its own stage, 8 km from Chult's landing (`ShipOrigin()`: the landing spot − 800,000 cm in Y). Its frame has X to the bow, Y to port and Z up from the keel, in cm, with the world's axes. Arriving there hides whatever stage was standing (Waterdeep or Chult), stops its sound and stills its actors, as entering Chult does to Waterdeep. There is no way back.

### The ship (`ShipSetting.cpp`)

An original two-masted ship after the 2D deck reference (`docs/design/pirate ship.png`), built in code on procedural meshes (one per material), as the jungle wreck is:

- **Hull:** 33.5 m from the transom to the stem and 9 m in the beam. A parallel middle body, a full round bow with its stem and bowsprit, and a flat transom stern. The section is a full, flat-floored merchant's: about 6.4 m across at the hold's floor and 9 m at the deck. The planking is 10 cm thick, with two wales outside, frames every 60 cm inside, and a clamp under the deck beams. The waterline is 3 m above the keel.
- **Decks (heights above the keel):**
  - The hold's floor is at 70 cm, under 2.3 m of headroom.
  - The lower deck is at 3.3 m, with 2.25 m clear to the beams (human scale: the pirates must stand in it later).
  - The weather deck is at 5.8 m.
  - A quarterdeck at 7 m covers the captain's cabin, so the cabin has 3.3 m of headroom.
- **Masts:** the main and fore masts are stepped on the keel and run up through both decks into the sky, with iron bands, partners at each deck, a fighting top and a bare yard. The sails come in milestone 2.
- **Light from outside:** the weather deck is closed for now (milestone 2). Its three gratings let the sun down: over the main hatch, over the galley and over the crew quarters.
- **Openings:**
  - Gunports with their lids propped open (two a side in the galley and the crew quarters).
  - Small iron-rimmed scuttles in the arrival compartment.
  - Side lights in the cabin.
  - Four glazed stern windows in the transom.
  - The broken planking in the arrival compartment (below).
- **Rooms** along the lower deck, joined by 1.04 × 2 m doorways:

| Space | Where (X) | What is in it |
| --- | --- | --- |
| Arrival compartment | −170 to 330 | A sail locker amidships. Folded spare sails, rope coils, a spare spar and a barrel. The main hatch with a ladder down. A pile of spare lumber in the starboard-aft corner, the crawlway through it ending in a small piece of the temple's masonry ringed with the oil haze (November 2, below). The broken planking in the port side. |
| Galley | 330 to 1150 | A brick hearth under an iron top with a great pot and its pipe up through the deck, and a fire glowing in the firebox. A work table under a rail of hanging pots. A counter along the port side, open underneath, with a low board of jars. A rack of shelves to starboard. Water casks and sacks. |
| Crew quarters | −1080 to −170 | Eight hammocks slung fore and aft from the beams, their beds about 1–1.45 m off the floor (lowered November 2; before, 1.5–1.95 m). A 2 m round mess table on a pedestal with five stools (stool 41 cm, table 76 cm: he can mantle up) and tankards. Iron-bound sea chests along both sides. The main mast. |
| Captain's cabin | transom to −1080 | A bed with brass posts, a blanket and pillow. A desk with a chart and a lamp, and his chair. A rug. The iron-bound chest (closed: milestone 4). A bench under the stern windows (he can see out from it). |
| Hold | −1300 to 1150, below | Cargo stowed in tiers (the docks' crates, barrels and rope coils, and canvas sacks). Three shelving units along the sides. Stanchions. A raised way of crates along the starboard side, aft from the ladder: a sack to step onto, then crates one and two high. |

- **Lanterns:** 11 iron-framed lanterns hang from the beams, each with a shadowed warm point light. They and the hammocks swing on a slow 7.3 s roll and the flames flicker.
- **Sound:** a slow wash of the sea against the hull loops, and now and then a timber creaks near him. Both are synthesised: `Tools/gen_ship_sfx.py`, imported by `Tools/import_ship_sfx.py`. Aboard, every other score fades out. Since November 2 the user's ship track (`SW_Ship`, below) plays under these sounds.
- **The sea:** seen through every opening, it is the sanctum's sea-tear material (`M_TempleRiftSea`), which draws an open sea under a pale sky from the view ray alone. It sits on a 3 km box round the ship, its sea at the waterline. The material gained a `Gain` parameter (default 1, so the sanctum is unchanged); the ship sets 0.22, because its interior exposure would otherwise blow it out (`-ChuckShipSeaGain=` overrides it for tuning). Seen from inside, that is all that is needed. The weather deck (milestone 2) will need a real water surface where the sea meets the hull.
- **Hiding the other stages:** fog, sky lights and atmospheres belong to `AInfo` actors, which the hiding pass otherwise leaves running (the game mode is one). Their parts are switched off too: Waterdeep's harbour fog had turned every opening white.
- **(Milestone 2 replaced the sea box with a real sea surface and the open sky: see below.)**
- **Lighting:** an afternoon sun high on the starboard quarter, with volumetric fog so its light shows in shafts through the ports and gratings. The sky light is faint below decks: with no dynamic GI it would otherwise light the inside as if there were no hull. The exposure is set for an interior.

### Getting about

- **Ladder:** the main hatch (1.2 × 1.2 m, with a coaming on three sides) has a ladder on its fore edge, registered as a `ChuckClimbable` like the pantry's. He climbs it from the hold, or walks to its lip from forward and lowers himself onto it.
- **Climbing:** the crates, stools, table, sacks and window bench give mantles and pull-ups. Crates and barrels are never breakable.
- **Over the side:** through the broken planking (a ledge to pull up onto), he falls into the sea. That is an Astral death, as off the edge of the world, and he comes back at the start of the space he went over from. Gunports and scuttles are too small to pass.
- **Respawns:** each space has a spawn point and a Dev Checkpoint, in a sixth column: Ship Arrival, Ship Hold, Ship Quarters, Ship Galley, Ship Cabin. A death aboard returns him to the start of the space he died in.

### The hold's life

- **Rats:** 16 ordinary rats (`AEnemyRat`), spread through the hold. As in the 2D game, the hold is the ship's one real fight. They notice him on their level and chase. The crates are out of their reach.
- **Jars:** 22 clay jars on the hold's shelves (their lower two boards) and floor, and on the galley's counter board and rack. Each breaks into one cigarette.

### The escape (`ShipEscape.cpp`)

The rubble map's crevice ends at a crack of lamplit timber (`AtChultTempleCreviceEnd`). Crawling to it starts the 2D Phase 6 escape, letterboxed with no control. Esc skips it.

1. It goes to black, and the music fades.
2. The ship's stage is built, and he is on all fours in a stone passage: the crawlway through the pile of lumber in the arrival compartment's starboard-aft corner (until November 2 a mass of the temple's masonry). Its mouth is a small piece of the temple's stone set in the boards, ringed with the oil haze, and grit has spilled from it. Behind him, his crawl camera looks along the passage toward the lamplight at its mouth.
3. He crawls out among the sailcloth, seen from low in the compartment, and gets up.
4. He walks to the broken planking in the port side. The view eases in past him to the open sea through it.
5. Control returns where he stands, facing the hole.

It plays once a session. A respawn aboard never replays it. Skipping puts him at the same place at once. Sanity carries over unchanged.

### Tests

- **`-ChuckChultTest -ChuckShipTest`** (`ShipTest.cpp`) runs in this order:
  1. The real character crawls to the crevice's end, and the escape plays through, with pictures of its three shots.
  2. The ship's checks: every spawn on its floor with room, every doorway open to a 180 cm human, and the hull solid below the waterline.
  3. 18 stills.
  4. Six trials with the real character:
     - through the doors round to the galley's hearth;
     - along the crew quarters past the mast into the cabin, to the stern windows;
     - down the ladder from the arrival compartment;
     - up it from the hold;
     - over the side into the sea and back;
     - back into the crawlway to its blind end and out again.
- **Options:** `-ChuckShipViewsOnly` stops after the stills, and `-ChuckShipTrials=a+b` runs some of the trials.
- **Where they run:** Verify-Package runs the whole test, and Verify-Menu now includes the six new checkpoints (and Temple Rubble).

## Milestone 2: the weather deck, the rig and the sea (November 2)

User request: "Do the next part of the ship" (after milestone 1). The code is in `ShipDeck.inl` (the deck, its fittings, the rig, the sails' shapes and the climbs) and `ShipSea.inl` (the sea's mesh), both included in `ShipSetting.cpp`. The materials come from `Tools/create_ship_sea.py`: `M_ShipSea` and `M_ShipSail` in `/Game/Art/Ship`.

### The weather deck

- **Open to the sky.** The hull's planking now stops 28 cm over the deck (the waist) and over the quarterdeck. Above it, posts every 55 cm carry a cap rail (1 m over the deck) and a middle rail, after the reference's posts along the rail. The sea shows between the posts. The gaps are too low for him to pass or stand in; the ship's check tests 20 places along both sides.
- **The quarterdeck** has a rail along its fore edge, open at its two ladders. The transom stands to the quarterdeck's rail height (the taffrail), with the stern lantern on a bracket over it.
- **Hatches:** the three gratings stay, since they still light the lower deck. The companion hatch (1.2 × 1.2 m) over the crew quarters has a coaming on three sides, its cover leaning against the aft side, and a ladder down on its fore edge.
- **Fittings:**
  - **Helm:** the wheel (10 spokes with handles, about 1.8 m across) faces forward between two posts, with the binnacle and its brass hood before it.
  - **Skylight:** a glazed ridge roof over the cabin. It is solid to stand on.
  - **Capstan:** between the main hatch and the fore mast.
  - **At the bow:** the bitts at the bowsprit's heel, the windlass with its cables to the hawse holes, two catheads with their anchors, and the bobstay.
  - **Rails and pins:** fife rails with belaying pins at the masts' feet. The main mast's rails are open fore (the rope ladder's foot) and aft (room for Jeffries in milestone 3). Pin rails inside the bulwark by the shrouds, with coils hung on the pins.
  - **The bell:** the ship's bell in a small belfry on the fore fife rail.
  - **Stores:** crates lashed by the port rail forward (two high, so he can climb them and see over the rail), water casks aft of the main mast and rope coils.
- **Ladders** (all `ChuckClimbable`, climbed like the hold's): the companion ladder from the crew quarters, and one up each side of the quarterdeck's break. Chuck's step height is 4 cm, so stairs would be a mantle at every step. These are ladders.

### The rig

- **Masts:** each lower mast now stands solid above the deck, so he walks round it, up to a fighting top: the main's at 10.2 m over the deck, the fore's at 9.2 m. Above them are the masthead and cap, the topmast forward of it with crosstrees, and the topgallant mast with its truck.
- **Yards:** three a mast. The fore yards are braced round 12°.
- **Sails:** eight canvas sails, on their own two-sided cloth material that lets the sun through, as thin canvas does, seamed in 62 cm panels:
  - the fore course, both topsails and both topgallants, set and drawing;
  - two staysails on the fore topmast and jib stays;
  - the spanker on its gaff and boom over the quarterdeck (the boom clears a standing man at the helm).

  The main course is furled on its yard. The square sails' feet are cut in a curve (a roach), so the topsails sweep clear over the tops. The sails breathe with the swell and flutter a little at their free edges; their meshes are rebuilt every frame (`UpdateSails`). A flag streams from the spanker gaff's peak over the quarterdeck: since November 2 the user's mustachioed skull over crossed cutlasses (below; at the main truck until the evening of November 2).
- **Standing rigging** (tarred, dark):
  - shrouds from deadeyes on the channels to each masthead, with ratlines;
  - futtock shrouds;
  - topmast shrouds from the tops' rims, with ratlines;
  - backstays;
  - the fore and main stays, and the topmast and topgallant stays.
- **Running rigging** (hemp): lifts, braces, halyards, and the spanker's sheet and peak halyard. No rigging has collision.

### The climb up the rig (optional)

1. A rope ladder hangs from the main top's lubber's hole, its foot made fast on the deck just forward of the main mast. He walks into its foot and climbs it like any ladder: 10.2 m, about 14 seconds.
2. At the top he pulls himself out onto the platform forward of the hole. The lookout's clay jar stands on the aft port corner, with a cigarette inside.
3. The main yard is slung 55 cm under the top, just forward of the mast. A jump off either side of the top, or a drop from its edge, catches it. He then goes hand over hand out along it. There are 12 grips (`AddChuckSwingGrip`), six a side and 90 cm apart (easy leaps, no pumping), from 1.7 m to 6.2 m out; the last is over the sea.
4. **Recovery:** let go over the deck and he falls the 10 m and rolls, unhurt (there is no fall damage). Let go over the sea and it is an Astral death; he comes back at the Ship Deck spawn. There is no way back up from the yard to the top.

### The sea (`ShipSea.inl`, `M_ShipSea`)

- **The mesh:** a real surface replaces milestone 1's view-ray box.
  - About 100 rings round the hull's waterline: the first on the planking itself, then about 1 m apart out to 60 m, then ever wider to 400 km, past any horizon.
  - Each ring is the waterline pushed straight out along its normals, with fans at the transom's corners, so no rings cross.
  - A hidden curtain runs down inside the planking.
- **The waves:** six travelling deep-water waves from about one wind, 42 m down to 4.5 m long, with crests a little sharper than a sine. They are a little lower against the hull and gone by 60 m out.
- **Meeting the hull:** the ring on the hull moves in or out with the planking's flare as the water rises and falls, so the sea always meets the hull and never shows inside it.
- **She makes way:** the sea runs aft past her at 1.1 m/s (`-ChuckShipSeaSpeed=`).
- **Foam and finish:** foam breaks along the hull, at the bow and in the wake astern. Fine ripples (the UE Water tiling normal) fade out by 80 m. The surface is opaque and lit, so the sky and sun reflect in it.
- **Rocking** (`Rock`): the hull stays still, as the user chose. The sea, its horizon, the sky and the sun roll about 1.5° on the lamps' 7.3 s swell and pitch 0.6° on a 9.1 s one.
  - The sea's plane tilts, the waterline climbs and drops on the hull, and the horizon tilts with it.
  - The sun turns with it, so shadows sway on deck and below.
  - The sky's planet is centred under the waterline and moved with the tilt, so the sky's horizon tilts with the sea's.
  - Nobody's feet, no ladder and no grip ever moves. There is no camera roll.
- **The sky:** the open sky (the sky atmosphere) and far fair-weather clouds. These use the fall cutscene's cloud material, thinner and higher; `-ChuckShipNoClouds` turns them off. The haze is as pale as the horizon.
- **Light below decks** is unchanged: the sky light is faint (0.22) while the view is below decks and fuller (0.75) once it is out on deck or outboard.

### Spaces and checkpoints

- A sixth spawn and Dev Checkpoint, **Ship Deck**, stands on the weather deck by the companion hatch.
- Everything above the weather deck or the quarterdeck, including the rig, is the Deck space.
- Over the side (from the deck, the yard or the arrival compartment's broken planking), he comes back at the start of the space he was last in.

### Tests (milestone 2)

- **The ship's check** (`AShipSetting::Check`) now also covers:
  - the six spawns;
  - the rail at 20 places (no room for him on the planking between the posts);
  - the main top (floor and room where the rope ladder lets him off);
  - 5 ladders, 12 grips, 8 sails, the flag and the sea material.
- **Six new trials, 12 in all:**
  - `deck_up`: the companion ladder up from the crew quarters.
  - `deck_down`: down it from the deck.
  - `quarterdeck`: up the port break ladder, past the helm, down the starboard one.
  - `rigging`: up the rope ladder, off the top's side onto the main yard, five leaps out to the yardarm, let go into the sea and back on deck.
  - `yard_drop`: onto the yard and let go over the deck, landing unhurt.
  - `rail`: walking hard at the rail for 3 s keeps him aboard.
- **18 new stills:**
  - On deck: the deck from the bow, the quarterdeck and rat height; the rail; the companion; the helm; the break ladders; the sails.
  - In the rig: the rope ladder; the main top; down from the top; the yardarm.
  - Outboard: the waterline; the bow; the stern; the ship from outside, high and low; the horizon.

  The view log records the sea's tilt for each.

### Not yet (later milestones)

- **Milestone 3, the crew:** built on November 2 (below), except the voices.
- **Milestone 4, the ending:** the chest, the plank, the fall into the Hells.

## Milestone 3: the crew (November 2)

User request: "Do the next part" (after milestone 2). The code is in `ShipCrew.cpp` (`AShipCrew`: who stands where, their lines, the beat, the fencing, the hits) and `DockNPCPirate.inl` (their performances, as `ADockNPC` members; included at the end of `DockNPC.cpp`). The bodies are in the human NPC kit, and the props come from `Tools/build_pirate_props.py`.

### Who is aboard

| Pirate (2D credits name) | Where | What he does |
| --- | --- | --- |
| The Pirate Chef | The galley, at the work table's starboard side | Chops on the board on every other beat. When he sees the rat in the galley he calls out, then chases it with his cleaver (below). |
| The Seated Pirate | The crew quarters, on the mess-table stool that faces the door from the arrival compartment | Sways on the beat with his tankard and drinks from it (the tavern drinker's pose). |
| The Concertina Pirate | Deck, amidships to starboard | Squeezes the concertina open and shut over two bars, bobbing on the beat and nodding. |
| The Cheering Pirate | Deck, amidships to port | Sways, bobbing, his tankard at his side. Every eight bars he raises it high and pumps it on the beat, and every other time drinks instead. |
| The Dancing Pirate | Deck, on the main hatch's grating | Dances a jig: a hop on every beat, kicks out and back on each leg, heel taps on each, a jump with the heels together to close the phrase. Hands on his hips. |
| The Swordsmen (two) | Deck, the open ground forward of the fore mast | Fence (below). No lines. |
| Jeffries | Deck, his back to the main mast's aft side | Tied to it by the chest and waist, wrists bound behind him round it. Heaves at the ropes in bursts of three, more often with the rat near; between them he sags and twists. |

- **Mustachioed (user, November 2):** "These are the mustachioed pirates ... make all the pirates have mustaches and not full beard". Every pirate has a strand moustache and no beard. The kit's moustache volume gains `moustache_size`, `moustache_droop` and `moustache_out`, so each is his own: a ginger handlebar on the cheering pirate, a grey walrus on the seated one, horseshoes on the concertina player and a fencer, drooping ones on the chef and Jeffries, trim ones on the dancer and the other fencer. All eight stay men for now (the user's choice). A mustache pirate flag for the ship comes later.
- **Bodies:** eight new humans in `SourceAssets/NPCs/humans.json, built with `Tools/build_npc_humans.py` on the shared skeleton, with strand hair and beards from the CC0 pack. They are loaded only when the ship spawns them; the Waterdeep NPCs don't reference them.
- **New kit pieces:**
  - `tricorn`: a felt hat built on the head, its brim cocked up on three sides into a triangle, the point over the face. As Phase 7 asks, a turned head never hides the face. `trim` adds braid edging.
  - `sash`: a broad band round the waist, its end hanging down one hip.
- **Skin:** the crew's skin has a milder subsurface colour (`skin_subsurface` 0.62, 0.36, 0.28). The default (1.2, 0.45, 0.3) turned dark and young skin orange-red in the ship's strong sun.
- **Props** (`Tools/build_pirate_props.py`, `SourceAssets/NPCs/Props`): a cutlass (shell guard, knuckle bow), the chef's cleaver, and the concertina's ends and bellows (the bellows stretch between the hands). The tankards are the tavern keeper's.
- **Motion:** each performance is laid over a motion-capture idle and placed by IK.
  - New CMU clips (`Tools/build_npc_mocap.py`): `DrunkSway` (137_15 "Drunk Wait") for the deck hands, and `Jog` (16_35, one stride in place) for the chef.
  - Tried and dropped: 137_13, 137_16, the 02_07-09 swordplay (flailing, not fencing), 93_06 (a swing dance, not a jig), 142_08, 79_69, 09_01, 13_09.
  - Legs that dip, hop or step are placed by two-bone IK on the clip's own feet.

### Keeping time

The performances keep to the ship score: 136.12 beats a minute, the first beat 0.107 s in. I measured this from the track itself by spectral flux and autocorrelation, and checked it holds across all 207.7 s. `DockGameMode` tells `ShipScore` when the track starts, and `AShipCrew` hands each pirate the beat every frame. Since the concertina version (below), the grid is 135.725 beats a minute, the first beat 0.091 s in. This is a sharper fit, and the 136.12 grid slipped about a beat over the track. On deck the crew follow the concertina's own grid, and the count runs on through loops and changes of track.

### Their lines

Each line is the 2D game's (`ship_crew_quarters.json`, `ship_exterior_deck.json`, `ship_galley.json`), word for word. Each talking pirate gives his first lines until one conversation is talked through, then his repeat lines from then on.

| Pirate | First | Repeat |
| --- | --- | --- |
| Seated Pirate | "Well I'll be. Never seen a rat wearin' a coat before." | "You seem like a good listener, I bet all of the crew would love to bend your ear" |
| Concertina Pirate | "Requests are considered in the order they are shouted." | "Still considering it." |
| Cheering Pirate | "Don't mind Jeffries over there tied to the mast. He's gone mad. Too much sun, I think." | "You're not smoking the captain's cigarettes, are you!?" / "Don't let the captain catch you sniffing around in his quarters!" |
| Dancing Pirate | "Deck's moving. Saves me half the work." | "Other half's mine." |
| Jeffries | "The sea is wrong, the land is wrong. Open your eyes!" / "Half the crew is gone... lost to the purple. They don't even remember them." | "Don't go into the purple! DON'T GO INTO THE PURPLE!" / "Why don't they see!?" |

The chef's "Damn! Another rat got in, come er' you little bugger!" is called out when he first sees the rat, and again after a death aboard (the 2D notice re-arms on death). It shows as a subtitle while he is within 15 m.

All of it is subtitles only: no voice takes yet. The performers turn to the rat while he talks to them, and their performances settle without stopping.

### The chef's chase

The 2D galley is an escape, not a fight, and so is this one.

- **Who he chases:** the rat in the galley that he can see. He stays inside the galley. A rat that has just appeared there (a respawn or the Ship Galley checkpoint) goes unseen until it moves, so arriving never costs Sanity before the player can act.
- **His speed:** 205 cm/s at most, so the rat outruns him slowly at a run (225) and quickly at a sprint (380). He steers round the furniture.
- **His blow:** in reach (95 cm) he raises the cleaver and brings it down where the rat was. If it lands, it costs one of Chuck's five Sanity (the 2D 20 of 100). After a blow that lands he stands over it 1.4 s longer.
- **Safe places:** he can't reach under the work table or the counter, or anywhere a man's reach is blocked overhead. After 5 s unable to reach, or once the rat leaves the galley, he shakes the cleaver after it, then walks back to his board.

### The fencing

The two fence in eight-beat phrases on their ground, forward of the fore mast:

1. One advances and lunges, and the other retreats and parries.
2. Then either the other answers, so they end where they began, or the first presses again, so they move a step along the deck.
3. They turn a little in each recovery.

Whoever leads, and which way they turn, keeps both on their ground and inside the rail. They stand side-on in a crouch, on feet that are planted and stepped one at a time, each with a cutlass. The other fist rests on the hip.

Their blades fence at a man's chest height, over the rat's head. What hurts him is their feet (the rat in a stance, or in the way of an advance or lunge) and walking into them. Each costs one Sanity, as the 2D swordsmen's 15 of 100.

### Tests (milestone 3)

- **Three new trials, 15 in all:**
  - `crew_talk`: each pirate with lines, talked to twice. The first conversation must be his first lines exactly, the second his repeat lines.
  - `chef_chase`: into the galley. The chef must see the rat and chase it. Under the work table nothing may reach the rat, and the chef must give up. The rat then gets out of the galley alive.
  - `fencers`: in front of a fencer's front foot the rat must be hurt. Well off their line, nothing may reach him. The fencers must stay on their ground, and no crew body may go into the hull's planking.
- **Stills:** 15 crew stills (`Crew*`) and the trials' (`ShipPlay_ChefChase`, `ShipPlay_ChefUnderTable`, `ShipPlay_Fencers`, `ShipPlay_CrewTalk`).
- **Logs:** `CHUCK_SHIP_PIRATE` gives each pirate's state after the stills (props, grip error, foot sink). `CHUCK_SHIP_VIEW` now logs the frame time.
- **Verify-Package** expects 15 trials.

## The captain and the flag (November 2)

User request: "Make the captain. Here are reference images. In the 2d game the captain only makes his appearance after the trigger but in this game I want him on deck from the start, we'll go over later what he does with the trigger. The captain is a short Halfling but he wears very tall platform boots. Also add this pirate flag to the ship".

### The captain

- **References:** the user's two pictures, `SourceAssets/NPCs/Humans/PirateCaptain/Reference/` (fists on his hips; his cutlass raised).
- **Body** (`PirateCaptain` in `SourceAssets/NPCs/humans.json`): a halfling man of about 99 cm with short legs, a square jaw and a dimpled chin. He stands on platform soles of about 13 cm, so his whole height is 112 cm (116 cm with the hat). Chuck, at 65 cm, comes up to his waist.
- **Hair:** dark curls to the shoulder (the `hair_to_shoulder` style, curled) and a waxed handlebar moustache whose ends curl up. There is no beard.
- **Clothes:**
  - a burgundy frock coat to the knee, with gold braid along its edges, collar and big turned-back cuffs, and brass buttons;
  - a near-black waistcoat buttoned in brass, over an open cream shirt;
  - a belt with a big brass buckle over a red sash;
  - dark breeches;
  - knee boots on platform soles and block heels, with three buckled straps each;
  - a black tricorn edged in gold.
- **New kit options:** see `SourceAssets/NPCs/README.md`, "The captain".
- **Where he stands:** on the quarterdeck, just forward of the skylight, looking forward over the waist at his crew. The way across the quarterdeck passes in front of him.
- **What he does:**
  - He stands with his fists on his hips (the first picture). His cutlass stands planted point-down in the deck before his right foot, its tip 4 cm into the planks (by his right hip until November 2, below).
  - Every 28 s he reaches for it, pulls it out, and holds it up and out to his right a while, the blade turning slowly (the second picture). Then he drives it back into the deck and puts his fist on his hip again.
  - He watches the rat when the rat comes near, as the other humans do.
- **Lines:** none yet. The 2D game gives the captain none before the trigger, and what he does at the trigger is still to be decided with the user (milestone 4).
- **Not on board in the 2D game:** there he appears only at the trigger. On deck from the start is the user's change.

### The flag

The user's picture, `SourceAssets/Setting/Ship/ShipFlag_Moustache.jpg` (a skull with a curled moustache over two crossed cutlasses, on black), is now the ship's flag (at the main truck at first; at the spanker gaff's peak, larger, since November 2, below). `Tools/create_ship_flag.py` imports it as `T_ShipFlag` and makes `M_ShipFlag`: two-sided foliage shading like the sails', so the sun shows through the cloth. The picture fills the 150 × 95 cm sheet with the hoist at its left edge, and from the far side it reads mirrored, as a real flag does.

### Tests

- **Stills:** `CaptainHips`, `CaptainHipsSide`, `CaptainSword`, `CaptainSwordSide` (each sets his round to that stance), `CaptainFace`, `CaptainBoots`, `CaptainFromDeck`; `Flag`, `FlagStarboard`, `FlagFromDeck`.
- **Check:** `CHUCK_SHIP_CAPTAIN`, a failure unless:
  - he is aboard with his motion capture and his cutlass;
  - its planted tip is 1–8 cm into the deck;
  - his fist was within 8 cm of its grip while he held it.
- **The ship check** now needs `M_ShipFlag`.

## The ship's score (November 2)

The user supplied the soundtrack while milestone 2 was being built: "here is the soundtrack to add".

- **The file:** `C:/Users/ashsm/Downloads/ship.wav`, copied byte for byte to `SourceAssets/Audio/Ship.wav` (stereo 16-bit, 48 kHz, 207.7 s).
- **Import:** `Tools/import_ship_music.py` brings it in as `/Game/Art/Audio/SW_Ship`, looping the whole recording.
- **In play:** `DockGameMode`'s ship branch fades every other score out and brings this one in over 3 s at 45%, the level of the other scores.
- **When it starts:** the escape fades the old music to black, and the track rises as he crawls through the masonry once the ship is staged. This follows the 2D escape, which starts the ship's music.
- **The test:** under `-ChuckShipTest` it starts 2 s before its end, and `CHUCK_SHIP_MUSIC_CHECK` confirms it loops. Verify-Package requires that check.
- **Not checked:** I have not heard it on speakers, and I haven't checked the mix against the sea wash or the loop's seam.

The pirates' voices are still the user's to supply. Until then their lines are subtitles only (milestone 3).

## Below decks and above: the concertina version (November 2)

User request: "here is the ship soundtrack but done on a concertina. If possible, I would like it so when you're belowdeck on the ship the main/current theme plays, then when you go above deck the concertina version starts playing BUT i want it to match the current part of the song as close as possible ... I'd also like this to feel like it's coming from the concertina player if it can get louder when you're closer to the concertina player (though not dramatically, i want to hear it from all points of the weather deck)".

- **The file:** `C:/Users/ashsm/Downloads/Ship_concertina.wav`, copied byte for byte to `SourceAssets/Audio/ShipConcertina.wav` (stereo 16-bit, 48 kHz, 187.6 s). `Tools/import_ship_music.py` brings it in as `/Game/Art/Audio/SW_ShipConcertina`, looping. The script now leaves an already-imported track alone unless `CHUCK_REIMPORT_SHIP_MUSIC=1` is set.
- **Where each plays:** below decks the ship track. On the weather deck the concertina version: that is the waist, the quarterdeck, the rig, and over the side from there (`ShipSpaceAt` = Deck, the same space the checkpoints use). The change waits until he has been on the new side of the deck for 0.4 s, so it doesn't flicker at a hatch. The two cross in 1.5 s (in) and 2 s (out).
- **Matching the tune** (`Tools/align_ship_music.py`, generating `ShipMusicMap.inl`):
  - The two are the same tune, but the concertina's is played about 5% slower (129.0 against 135.7 beats a minute), so a percentage or an offset would be up to a second out.
  - Its opening phrase is played once where the ship track plays it twice (the ship track's 0–18.75 s and 18.75–37.5 s both match the concertina's opening). It also drops a beat or two at some section joins.
  - The tool compares the harmony of each moment of one track (4 s either side) with every moment of the other. It keeps to a continuous run, jumping only where another place matches clearly better for many seconds. It then lines up the note onsets within a frame.
  - The result is two maps, ship → concertina and concertina → ship, each of about 220 points.
  - The new track starts at the mapped moment of the old one. Examples: ship 60 s → concertina 40.6 s; 150 s → 131.1 s; concertina 100 s → ship 120.4 s.
- **The crew's beat:** each track has its own grid (the ship track 135.725 beats a minute from 0.091 s, the concertina 128.985 from 0.417 s). At a change, the crew's beat count goes on from where it was, so no fencing phrase is cut short; only the part of a beat follows the new music. The test logs that step: 0.04–0.26 of a beat in the trials. The count also runs on through each track's loop, which before made the fencers jump mid-phrase.
- **From the concertina player:**
  - The concertina version plays from his position, 1.1 m above the deck. It is up to about 2.3 dB louder within 1.5 m of him, and about 2.5 dB quieter from 14 m on, so it is clearly heard everywhere on deck (`UpdateMusicDuck`, multiplied with the speech duck).
  - It is spatialised only a little: fully 2D within 2 m, blending towards 3D only out to 60 m, so at the deck's far end it is about a fifth directional and keeps its stereo width.
- **Tests:**
  - The deck_up trial now waits 1.5 s on deck and requires the concertina version to have taken over. Deck_down requires the ship track back (`CHUCK_SHIP_MUSIC_FOLLOWS`; each change is logged as `CHUCK_SHIP_MUSIC_SWITCH` with its positions and beat step).
  - `CHUCK_SHIP_MUSIC_CHECK` now also needs the concertina version to loop.
  - `Tools/align_ship_music.py --demo` writes sample crossings to `Local/ShipMusicSwitch/` to listen to.
- **Not checked:** I haven't heard it in the game. The offline crossings were checked only by measurement: the tunes' harmony and onsets line up, and on the two tracks' beat grids the beats agree to about 0.2 of a beat at the median.
- **Not done:** the concertina player's bellows keep to the beat, but his hands aren't matched to the notes.

## Fixes on deck and below (November 2, night)

User request: "Make the pirate flag more prominent while still being in a spot consistent with real flag placement on a ship like this. Move the awkward floating mop on the deck so that it's not propping itself up impossibly. Instead of temple stones in the ship have this be a pile of lumber with just small section of stone where you climb out, with our fade out oil slick around the small stone opening, indicating collided worlds. Lower the hammocks to be more realistic placement. Move the dancing pirate forward some so that he's dancing on the deck and not on the grate. Fix the captains hand for how he picks up his sword".

- **The flag** (`ShipDeck.inl`, `FlagPoint`):
  - It now flies where a ship of this kind wore its colours under sail: from the spanker gaff's peak, over the quarterdeck, about 10 m above it. Before, it was at the main truck, 24 m above the deck.
  - It is 2.6 × 1.65 m (it was 1.5 × 0.95 m), about the size of a big ensign.
  - It is hoisted on a halyard just aft of the peak, with the halyard's tail hanging below it. It streams to leeward (to port, the way the spanker fills), clear of the sail's leech.
  - The main truck is bare.
- **The "mop"** was not a mop. Two crew-quarters hammocks hung at x = −960, across the quarterdeck's break. Each one's fore lanyard ran up through the main deck and ended in the air about 90 cm over the waist's planking, beside a break ladder. These two now hang forward of the break: one inboard of the port row, the other outboard of the mess table on the starboard side.
- **The arrival compartment's corner** (`ShipSetting.cpp`):
  - In place of the temple blocks, a pile of the ship's spare lumber fills the corner:
    - stacked boards (`ShipLumber`, the light deck planks) laid fore and aft either side of the crawlway, with boards laid across over it;
    - heavy timbers crossed on top, two of them leant against the hull up under the deck beams;
    - off-cuts on the floor.
  - Where the crawlway comes out, a small piece of the temple's masonry sits in the boards: two jambs and a lintel, about 1.4 m across and 70 cm high, with three broken pieces proud of the edge and a little grit.
  - The cellar's oil haze (`M_AstralOilMist`) stands off the stone's edge across the stack's face, as a 46 cm band that fades away from the stone.
  - What he can and can't pass is unchanged: the crawlway's sides, roof and blind end are invisible boxes, and the boards facing it are square, so its walls are straight inside.
- **The hammocks:** all eight beds hang 50 cm lower, about 1–1.45 m off the floor, low enough for a man to roll into. The lanyards still run to the beams.
- **The dancer** now dances on the planking just forward of the main hatch's grating, at (215, 92), clear of the capstan. The cheering pirate moved a little aft and outboard to (100, 215) to keep room between them. The concertina player is unchanged.
- **The captain's hand** (`DockNPCPirate.inl`):
  - **Where the cutlass stands:** before his right foot. Before, it stood by his right hip, so his fist on his hip and his fist on the grip were almost the same place and he never seemed to reach for it.
  - **The reach:** it now takes 1.1 s (it was 0.7 s). His hand opens on the way, closes round the grip as it arrives, stays shut while he holds the cutlass, and opens again as he lets go.
  - **The grip:** his thumb lies down the grip toward the blade, his knuckles face forward and his elbow is out, so when he draws the cutlass, the blade comes up out of his fist on the thumb side.
  - **The fist's size:** the hand code places a fist for a grown man's hand, so on the halfling the grip sat 3–4 cm past his fingers. His fist is now scaled to his hand: 7.6 cm against the fencer's 11.1 cm, a scale of 0.68, measured when the crew is placed (`CHUCK_CAPTAIN_FIST`). The other NPCs are unchanged.
- **Tests:**
  - New stills:
    - flag: `Flag`, `FlagAft`, `FlagFromDeck`, `FlagFromWaist`;
    - arrival corner: `ArrivalPile`, `ArrivalMouth`;
    - where the lanyard stood: `BreakStarboard`; hammocks: `QuartersAft`;
    - captain's draw: `CaptainReach`, `CaptainGrip`, `CaptainDraw`, `CaptainLift`, `CaptainGripClose` and `CaptainGripBack`, each holding him at that moment of his round;
    - his hand close up: `CaptainHand*`, a camera that follows the hand.
  - The dancer's and cheering pirate's stills follow them.
  - `SetCaptainTime` can now hold him at a moment for a still.
- **Not checked:**
  - Nobody has played it by hand.
  - The haze's colours are the cellar's; they have not been tuned for the lamplit sail locker.
  - The flag is mirrored on its far side, as before.
