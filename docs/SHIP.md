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
- **Sound:** a slow wash of the sea against the hull loops, and now and then a timber creaks near him. Both are synthesised: `Tools/gen_ship_sfx.py`, imported by `Tools/import_ship_sfx.py`. There is no music, as the user asked, until their ship track comes. Every score fades out aboard.
- **The sea:** seen through every opening, it is the sanctum's sea-tear material (`M_TempleRiftSea`), which draws an open sea under a pale sky from the view ray alone. It sits on a 3 km box round the ship, its sea at the waterline. The material gained a `Gain` parameter (default 1, so the sanctum is unchanged); the ship sets 0.22, because its interior exposure would otherwise blow it out (`-ChuckShipSeaGain=` overrides it for tuning). Seen from inside, that is all that is needed. The weather deck (milestone 2) will need a real water surface where the sea meets the hull.
- **Hiding the other stages:** fog, sky lights and atmospheres belong to `AInfo` actors, which the hiding pass otherwise leaves running (the game mode is one). Their parts are switched off too: Waterdeep's harbour fog had turned every opening white.
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

### Not yet (later milestones)

- **Milestone 2, the weather deck:** its ladders, the deck furniture, the sails and rigging, a real sea surface and the sea's rocking.
- **Milestone 3, the crew:** pirates, the chef's chase, voices.
- **Milestone 4, the ending:** the chest, the plank, the fall into the Hells.

The ship's track and the pirates' voices are the user's to supply.
