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

The two fence in eight-beat phrases on their ground, forward of the fore mast. Until November 3 each phrase was one lunge and parry each way; since then each is a run of four to six blows ("The fight made harder", below).

Whoever leads, and the ground each blow gains, keeps both on their ground and inside the rail. They stand side-on in a crouch, on feet that are planted and stepped one at a time, each with a cutlass. The other fist rests on the hip, and is thrown back for balance in a lunge.

Their blades fence mostly at a man's chest height, over the rat's head, but a low cut or parry brings the point down near the deck and can catch him. What hurts him is their blades, their feet (the rat in a stance, or in the way of an advance or lunge) and walking into them. Each costs one Sanity, as the 2D swordsmen's 15 of 100.

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

## The fight made harder (November 3)

User request: "Make the sword fighter's fight more intense and complex while being smooth realistic good looking movements".

- **What they fight** (`ShipCrew.cpp`, `AShipCrew::PlanPhrase`): each eight-beat phrase is planned afresh (from the phrase's number, so the same every run) as a run of blows. The one who parried mostly ripostes; now and then the attacker renews. There are seven kinds of blow:
  - **head:** a cut from over the shoulder down at the head, parried overhead with the blade across;
  - **flank:** a cut from the right at the other's left side, parried with the blade upright on that side;
  - **low:** a backhand cut at the leg, parried point down;
  - **lunge:** a lunging thrust at the chest, parried to the outside;
  - **feint:** a half thrust to the inside that draws the parry, the point dipped under the blade and thrust home on the outside, parried at the last moment;
  - **beat:** a sharp blow knocks the other's blade aside, then a lunge into the opening, which he escapes by leaping back and leaning away;
  - **bind:** the blades crossed upright near the hilts, the two close and shoving for over a beat, then springing apart (at most once a phrase).
- Every fifth phrase they circle: the pair turns up to 30° on the deck, stepping in and out, before engaging again.
- **How it moves** (`DockNPCPirate.inl`):
  - Each blow is a few sword positions in time (wind-up, the blow, a held moment, back to guard; the parry, held, back to guard): 20 positions in all (`EFencePose`, `DockNPC.h`).
  - Between positions the fist, the blade, the edge and the elbow run on a cubic. The sword sweeps on through a wind-up without stopping. It stops dead where a blow lands or a parry is held.
  - Each blow is aimed at the other's blade where it is, crossing it as near his hilt as the arm reaches, never through his head. So steel meets steel at every blow.
  - The body goes with it: the shoulders wind back for a cut and drive through it, the hips drop and carry forward in a lunge, the knees bend for a low parry, they lean away from a blade, and they bounce lightly on the beat while on guard. Each blow has its own steps in and out, the front foot leads an advance and the back foot a retreat, and the steps are quicker (0.15 s). The free arm is thrown back for balance in a lunge.
  - They stand closer (1.5 m between them; it was 1.7 m), so blows reach the blade.
- **Hurting the rat** is as before: their feet and bodies, one Sanity each.
- **Tests:**
  - The `fencers` trial now also needs at least four blows that should meet the other's blade, 90% of them touching (within 4 cm). Each blow is logged (`CHUCK_FENCE_BLOW kind gap`). Blows seen only on frames slower than 50 ms are not counted.
  - `-ChuckFenceFilm` takes 90 stills of the fight from the side, 0.1 s apart (`Saved/Screenshots/.../Ship/Film/`), for reviewing it frame by frame.
- **Not checked:**
  - Nobody has played it by hand or watched it in motion; it was reviewed from still sequences.
  - The sound of steel came later the same day ("The ship heard", below).
  - The fingers don't change grip between cuts and thrusts.
  - In a bind the two stand close, but their bodies are not checked against each other.

## The ship heard (November 3)

User request: "Find and download sword fight sounds and also other immersive sounds, such as the boat going through the water. Maybe also some feet on deck sounds for the fighting and the dancing. Make it so you can only hear the rats in the hull and nowhere else on the ship. Decrease the volume of the creaking sound that we currently have."

- **The recordings** (all CC0; cut by `Tools/make_ship_sounds.py`, imported by `Tools/import_ship_recorded_sfx.py`; sources, pages and hashes in `SourceAssets/Audio/SFX/ship-recorded-manifest.json`, details in `SourceAssets/Audio/README.md`):
  - **Steel:** ten sabre-on-sword hits and two hilt-on-blade hits from Still North Media's weapon-impact library (real swords struck together), a blade sliding along a blade and a sword through the air (BigSoundBank).
  - **Feet:** boots on a wooden pontoon, and two-footed jumps and hard steps on wood (BigSoundBank).
  - **The sea and the wind:** small waves and froth (the sea along the hull), water lapping round moored hulls (below decks), wind whistling in masts (the rigging), all BigSoundBank. The ferry recordings there were left out: under the water they are mostly engine hum.
- **The fight** (`ShipCrew.cpp`): every blow's blade is heard through the air just before it lands, and steel rings where the blades cross, louder for a head cut or a beat than for a parried thrust. A bind locks with a heavier hilt-on-blade hit, scrapes as they shove and scrapes again as they spring apart. These carry across the deck (heard to about 36 m).
- **Feet on the deck** (`DockNPCPirate.inl`): a fencer's boot is heard when a step lands, by how far it went (shuffles under 16 cm are silent), and a lunge's front foot lands hard. The dancer's hop lands on every beat, lighter on the heel-taps, and the jump's landing is a two-footed stamp.
- **Where you hear what** (`ShipSetting.cpp`, `ShipSound`): sounds from the weather deck are about a third as loud below, and the lower decks' as quiet from on deck.
  - On deck: the sea rushing along the hull (0.6), the wind in the rigging (0.35, rising to 0.75 up the rig), and the old wash under them (0.2).
  - Below: the wash (0.5), the water lapping at the planking (0.7, 1.0 in the hold), and the sea along the hull and the rigging faint (0.14, 0.04).
  - The levels follow him over about a second.
- **The rats** (`EnemyRat.cpp`) are heard only when he is in the hold, nowhere else aboard.
- **The creaks** are about two fifths as loud as before (0.14-0.22, from 0.35-0.55).
- **Tests:** the new `sounds` trial (16 trials now) stands on deck for 10 s, then in the hold for 10 s. It checks the clashes and feet play, the deck and hold mixes, that no rat is heard on deck while some are hushed, that rats are heard in the hold, and that creaks keep coming. These are play counts and levels; nobody has listened to them.
- **Not checked:** heard on speakers by nobody. The balance against the score, how busy the fencers' feet sound (about three steps a second each) and whether the surf recording reads as a moving hull are for the user's ears. The steps sound the same in boots and bare feet, and through a deck sounds are only quieter, not muffled.

## The chest, the trigger and the storm (November 3)

User request: "when the ship trigger takes effect, i want a storm to roll in and for the ship going through a storm to be a thing for a good while before the captain kicks chuck off the plank. start working on this (the chest, the trigger, the storm). implement sounds for the storm too and really make it look and feel cool. don't do the plank part yet". The code is `ShipStorm.h/.cpp`; the materials come from `Tools/create_ship_storm.py`, the sea's storm from `Tools/create_ship_sea.py`.

### The chest (`AShipChest`)

- The captain's chest in his cabin (against the port side) now has a lid on a hinge. Scratching it, or interacting with it while facing it within about a metre, opens it once: the lid swings back past upright (1.1 s, the hinges creaking, a knock as it comes against its stay) and a warm gold light comes up inside.
- A golden carton (22 × 12 × 9 cm, in the golden leaf's aura) pops out over the front and lands before the chest. Walking over it gives 40 cigarettes and starts the golden leaf's 45 s stamina lock (the user's decision of 2026-11-01). It is taken once and never comes back, through deaths too; the chest stays open.
- The 2D game drops it "with no dialogue box"; there is none here either.

### The trigger (`ShipGate`)

- The 2D game's seven conditions: the chef has seen him (noticed, chasing or swinging), he has talked once to the seated pirate, the concertina player, the cheering pirate, the dancer and Jeffries, and he has opened the chest. They are kept for the session, through deaths aboard, and logged as each is met (`CHUCK_SHIP_GATE`).
- When the seventh is met, the storm begins. Later it will also start the captain's sequence (not built).
- A fresh start aboard from the checkpoint menu forgets them (the chest shuts, the storm ends). The new **Ship Storm (dev)** checkpoint, in the ship's column, starts on deck with all seven met, so the storm rolls in at once.

### The storm (`AShipStorm`)

It rolls in over about a minute and then keeps on: there is no end to it yet (the plank will come out of it later). Seconds after the gate opens:

| | from | full by | then |
| --- | --- | --- | --- |
| Wind (sails, flag, the gale's sound) | 0 | 45 s | gusts every few seconds (0.82–1.0) |
| Dark (clouds, light, haze) | 4 s | 55 s | |
| Sea (swell, rolling, spray, slams) | 8 s | 70 s | |
| Rain | 22 s | 65 s | squalls over most of a minute (0.55–1.0) |
| Lightning | 18 s | | far off first, then closer and more often |

- **The sky** (`AShipSetting::ApplyStorm`): the fair-weather clouds (the engine's volumetric cloud material) thicken, come down to 750 m and turn to storm cloud (its own `StormClouds` setting). The sun dims under them to a grey light; the sky's fill rises instead, so the deck stays readable in the overcast. The haze thickens with the rain, the colour drains (saturation about 0.63) and a vignette closes in.
- **The sea** (`M_ShipSea`'s new `Storm` setting): a long storm swell (90, 56 and 33 m waves, up to 0.6 m, running down the wind from starboard), the wind sea under it nearly twice as high, darker slate water, foam on far more of the crests and spume streaked down the wind. Against the hull the swell is lower. The sea mesh has rings 2 m apart out to 150 m now, so the swell keeps its shape there; its slope still shades the sea to 800 m.
- **The ship's motion:** she rolls about 6.5° each way on a slightly quicker swell (6.2 s), pitches 3° (5.8 s), a 3.1 s chop rides on both, and the gale heels her up to 3.5° to port. (The hull stays still; the sea, sky and sun move, as before.) Up to 11° of the horizon's tilt was measured. The lamps and hammocks below swing about three times as far.
- **The sails and flag** (`ShipDeck.inl`): fuller, their free edges thrumming faster and harder; the flag streams out flatter and cracks.
- **Rain:** 2,600 streaks in a 30 m box round the view, falling slanted down the wind, lit by the light round them (a flash lights them) with a little glow of their own. They stop at the deck or the sea, so none falls below decks, and those landing on deck near the view throw tiny splashes.
- **Spray:** every 1–4 s at full sea a sea comes over the bow or the starboard (weather) rail: white water thrown up, blown across the deck and falling, with the slam of a wave (heard muffled below).
- **Lightning:** a jagged channel from the cloud down to the sea, with forks, a white core and a faint glow round it, lit for the flash (two to four strokes in a few tenths of a second; it is not fogged, so it cuts through the haze). Its light on the ship is a second, hard white light from the strike's side, casting the hull's and the rig's shadows (so below decks it comes only through the openings); the sun brightens more slowly, to light the clouds (a sudden change there speckled every edge against the sky). Thunder follows after about a third of the real delay: a crack and its roll for a strike within 1.2 km, rolling thunder for one further off.

### The storm heard

All CC0 recordings by Joseph Sardin (BigSoundBank), cut by `Tools/make_storm_sounds.py` (sources, URLs and hashes in `SourceAssets/Audio/SFX/storm-recorded-manifest.json`; six that the site would serve only as MP3 are decoded by `Tools/decode_storm_mp3.py` with Blender's audaspace), imported by `Tools/import_storm_sfx.py`:

- **Loops**, mixed by where he is (`AShipStorm::TickSound`): heavy rain on a terrace (the rain on deck); rain on a tent heard from inside, low-passed (the rain on the deck overhead, below decks); strong wind with whistling wind under it (the gale, louder up the rig); wind heard from indoors (below decks); a flag flogging in a strong wind (the canvas); heavy waves (the sea in the storm). The ship's own wash, water along the hull and rigging loops are turned up with the storm, and the creaks come about three times as often and louder.
- **Thunder:** four close strikes (a sharp crack and its roll) and seven distant rolls, heard everywhere aboard, low-passed and quieter below decks.
- **The sea's slams:** six wave hits on a cliff, one at each spray.
- **The music** sinks to about half under the full gale.

### Tests (the chest and the storm)

- `chest` (trial 17): before the chest, facing it, he interacts; the lid opens, the carton drops; he walks over it. It checks the lid's angle, 40 cigarettes, the stamina lock, that a second interact does nothing, that no carton is left, and that the gate opens with the chest only if the other six were met.
- `storm` (trial 18): 19 stills as it rolls in (at 5, 25, 45 and 70 s) and in full, from the deck, the rig, the rat's eye, outside, below, and with a strike and a spray (`Saved/Screenshots/WindowsEditor/Ship/Storm/`; `-ChuckStormStills=a+b` takes only those); then he walks forward along the deck in the full storm (a strike near by on the way) and goes below. It checks rain and splashes shown, a spray, strikes and their thunder, the roll, the mix on deck (rain, gale, canvas, sea; no rain overhead) and below (rain overhead and wind through the hull; no deck rain).
- `-ChuckShipTrials=chest+storm` runs just these two.

### Not yet / not checked

- The plank, the fragments and the fall (the user: not yet). Nothing ends the storm except the menu.
- Heard on speakers by nobody: the balance of rain, gale, sea and thunder against the music and each other is measured, not listened to.
- The crew go on as before in the storm (the dancer dances, the fencers fence). Whether they should react (or the sails be reefed, as a real crew would in a gale) is the user's call. (November 4: they now work the ship in the storm; see below.)
- The flash still leaves a faint speckle on edges against the clouds for a frame or two.
- No wet look on the deck or the crew; no rain comes below through the gratings; drops pass through the sails and tops.
- Not played by hand.

## The storm developed (November 4)

User request: "fix the concertina players hands (currently they're backwards on the concertina). then, develop the storm sequence: when the storm starts and player is above deck, have an astral rupture form at ladder locking player to staying above deck. add this Ship_storm soundtrack for the storm starting, after the song plays have the storm intensify and switch soundtrack to Ship_storm_intense. no concertina transition during storm/ following the trigger. during storm have the NPC's responding to the storm, doing things sailors would in a storm, the captain at the helm. no concertina held by the concertina player as he is now manning the ship as with the other crewmen. when the storm intensity increases have more wave influence on ship, more wind, waves crashing on deck at times, make it look intense and cool. download and implement whatever free resources you need".

### The concertina player's hands

His palms faced out, the fingers curling away from the instrument (in this rig, fingers up with the palm in means the thumb's side back toward him; `DockNPCPirate.inl`). Now each hand lies over its end, palm in, fingers on the buttons, the back of the hand out.

### The rupture over the companion (`AShipStorm::TickRupture`)

- Once the storm has started and he is on the weather deck (at least 1.7 m from the companion hatch, so it never opens under him as he climbs out), the Astral tears open in the companion hatch: a slit along it first, widening over 2.4 s until the whole opening is the Astral's starry glow 8 cm below the deck, with the oil-film veils standing round it as the sewer's and the temple's ruptures have, a purple light that flares as it tears, and the Astral summoning sound.
- The companion ladder can no longer be taken hold of (`SetChuckClimbableBlocked`, a new flag on `FChuckClimbable`). Stepping into the hatch is an Astral death (`ShipRuptureSwallows`), and while the rupture is open every death aboard returns him to the deck (`ShipRespawn`).
- If he is below when the storm starts (the chest is in the cabin, so the seventh condition is often met there), it forms as soon as he comes up and walks clear of the hatch.
- The checkpoint menu's fresh start aboard closes it and frees the ladder.

### The storm's music (`DockGameMode`)

From the storm's start, wherever he is aboard: the user's storm track (once), and from the storm's second part their intense track, looping (`SW_ShipStorm`, `SW_ShipStormIntense`; `SourceAssets/Audio/README.md`). The ship track and the concertina version fade out and there is no switching between them by deck in the storm. Each storm track is kept at the storm's clock (a checkpoint jumping it brings the music along). The storm tracks sink only 12% under the gale.

### The second part (`AShipStorm::Levels`)

When the storm track has played through (202 s, its length read from the asset), the storm intensifies over 25 s and stays so:
- **Sea and ship:** she rolls about 10° each way (6.5° before) on a quicker swell, pitches about 5°, the chop rides harder and the heel deepens: about 15° of horizon tilt was measured at the peaks. `M_ShipSea`'s Storm goes on to 1.7 (its swell and wind sea higher, more foam and spume).
- **Wind:** the gusts 30% stronger (the sails thrum and fill harder, the flag streams flatter); the gale's loop 25% louder, the sea's 35%.
- **Rain:** heavier (up to 3,400 streaks).
- **Spray:** about twice as often and larger, with billows of spume (below).
- **Lightning:** strikes nearly twice as often and more of them close.

### Seas breaking aboard (`AShipStorm::DeckWaveNow`)

In the second part, every 7–13 s a sea breaks over the weather (starboard) rail into the waist, aft of the main mast or forward of the companion (never over the rupture):
- White water bursts up along 5–7 m of the rail, with billows of spume over it, and the sea's slam, heavy, twice.
- A sheet of dark green water with a foaming front runs across the deck to the lee rail in about two seconds, rippling and foaming as it goes (`M_ShipDeckWash`, using the sea's own normal and foam textures), then drains away (4.4 s in all).
- If its front catches him on the deck, it shoves him across toward the lee rail (he keeps his feet; the rail holds him).

The spume (also over every spray now) is drawn from Unity Labs' CC0 smoke flipbook (`WispySmoke01`, already imported for the temple's jets as `T_JetSmoke`) as white water: `M_ShipSpume`, lit, on camera-facing sprites that billow up, blow down the wind and thin away. No new download was needed.

### The crew in the storm (`AShipCrew::BeginStorm`, `DockNPCPirate.inl`)

At the storm's start the deck's crew stow what they hold (the concertina, the tankard, the cutlasses), stop the music, dancing and fencing, and walk briskly to their stations (round the companion and the skylight, never through them):
- **On the main's line to starboard:** the concertina player and the cheering pirate, one either side of it, facing its lead block. They haul together: both hands draw back as they throw their weight back, then the forward hand lets go and reaches on along the line and the other follows. The line runs aft along the deck at hand height from a lead block, up to the mast, and its tail lies coiled behind them.
- **On the fore's line to port:** the two fencers, the same way, half a heave out of step with the first pair.
- **At the main mast's port fife rail:** the dancer sweats up a halyard, reaching high on it and throwing his weight down, hand over hand.
- **At the helm:** the captain, behind the wheel facing forward, his hands on two spokes, steering it to and fro against the sea (a long sweep with quicker corrections over it, wider in the full gale and wider still in the second part). The wheel now turns: its rim and spokes are their own part, and it stands on the aft side of its posts (it was on the fore side, with no room for a helmsman behind it).
- They work faster as the storm builds. Jeffries stays tied to the mast. The chef and the seated pirate below go on as before (he cannot go below in the storm).
- Their talk stops while they work (they can't be talked to), and they don't turn to the rat.
- Every grip is kept within the man's reach along its line or spoke, and the hand takes the line the way round that keeps the wrist natural.

### Checkpoints

**Ship Storm Intense (dev)** (new, after Ship Storm (dev)): the storm already running, 8 s before its second part.

### Tests

- `storm_work` (trial 19): the full storm's first part until the six deck crew are at their stations; 16 stills (`Saved/Screenshots/WindowsEditor/Ship/Storm/Work_*.png`: both hauling pairs, the halyard, the helm from before and behind, the rupture, the whole deck, the second part from the bow, the quarter and the horizon, and a sea breaking aboard from the deck and at the rat's eye); then he walks aft at the companion's lip into the rupture (an Astral death, no ladder taken, back on deck), and a sea breaks aboard over him. It checks all six at work, every hand within 8 cm of its line or spoke, the wheel's swing over 12°, the rupture open, the death and the deck return, the shove, more than 8° of roll in the second part, spume shown, seas aboard, and both storm tracks heard (music regions 31 and 32).
- `-ChuckShipTrials=storm_work` runs it (with `storm`, whose name it contains).

### Not checked / still open

- Heard on speakers by nobody: the storm tracks against the gale, the sea's slams and the thunder.
- The plank, the captain's sequence and the storm's end (the user: not yet).
- The haulers' feet stay planted (they don't walk the line back); the line itself doesn't run through their hands.
- The sea washing aboard passes through deck fittings (the capstan, the coamings, the crew's feet) rather than round them, and the crew don't react to it.
- The rupture's veils follow the sewer's style (oil film); from close by, the nearest can read as a flat sheet.
- Not played by hand.
