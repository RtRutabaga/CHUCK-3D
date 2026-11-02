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
| Arrival compartment | −170 to 330 | A sail locker amidships. Folded spare sails, rope coils, a spare spar and a barrel. The main hatch with a ladder down. The temple masonry and its crawlway in the starboard-aft corner. The broken planking in the port side. |
| Galley | 330 to 1150 | A brick hearth under an iron top with a great pot and its pipe up through the deck, and a fire glowing in the firebox. A work table under a rail of hanging pots. A counter along the port side, open underneath, with a low board of jars. A rack of shelves to starboard. Water casks and sacks. |
| Crew quarters | −1080 to −170 | Eight hammocks slung fore and aft from the beams, 1.5 m up, over his head. A 2 m round mess table on a pedestal with five stools (stool 41 cm, table 76 cm: he can mantle up) and tankards. Iron-bound sea chests along both sides. The main mast. |
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
2. The ship's stage is built, and he is on all fours in a stone passage: the crawlway through a mass of the temple's masonry that has come through the ship's timbers in the arrival compartment's starboard-aft corner. The blocks are jammed against the hull and up into the deck beams, the planking is split round them, and grit has spilled from the mouth. Behind him, his crawl camera looks along the passage toward the lamplight at its mouth.
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

  The main course is furled on its yard. The square sails' feet are cut in a curve (a roach), so the topsails sweep clear over the tops. The sails breathe with the swell and flutter a little at their free edges; their meshes are rebuilt every frame (`UpdateSails`). A black flag streams from the main truck.
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

- **Milestone 3, the crew:** pirates, the chef's chase, voices.
- **Milestone 4, the ending:** the chest, the plank, the fall into the Hells.

## The ship's score (November 2)

The user supplied the soundtrack while milestone 2 was being built: "here is the soundtrack to add".

- **The file:** `C:/Users/ashsm/Downloads/ship.wav`, copied byte for byte to `SourceAssets/Audio/Ship.wav` (stereo 16-bit, 48 kHz, 207.7 s).
- **Import:** `Tools/import_ship_music.py` brings it in as `/Game/Art/Audio/SW_Ship`, looping the whole recording.
- **In play:** `DockGameMode`'s ship branch fades every other score out and brings this one in over 3 s at 45%, the level of the other scores.
- **When it starts:** the escape fades the old music to black, and the track rises as he crawls through the masonry once the ship is staged. This follows the 2D escape, which starts the ship's music.
- **The test:** under `-ChuckShipTest` it starts 2 s before its end, and `CHUCK_SHIP_MUSIC_CHECK` confirms it loops. Verify-Package requires that check.
- **Not checked:** I have not heard it on speakers, and I haven't checked the mix against the sea wash or the loop's seam.

The pirates' voices are still the user's to supply.
