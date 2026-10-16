# Chult dinosaur sounds: download shortlist

**User request, 2026-10-16:** "look up and paste links for download sounds for the dinosaur effects to use, i'll download these and give them to you next pass."

The raptor and T-rex use synthetic placeholder sounds (`Tools/gen_raptor_sfx.py`, `Tools/gen_trex_sfx.py`). These candidates replace them. Every one was listed under **CC0** (no attribution required) by the site's licence filter on 2026-10-16. Confirm the licence on each page before downloading; Freesound needs a free account to download. None has been listened to yet; the user picks. I left out clips marked as AI-generated.

Next pass: put the files in `SourceAssets/Audio/SFX/Incoming/` (any names), say which slot each is for, and Claude cuts, levels and imports them into the existing slots (no code change needed for the slots below).

## Slots in the game

| Creature | Slot (asset stem) | Count used | When it plays |
|---|---|---|---|
| Raptor | `SFX_RaptorStep` | 3 | every half stride |
| Raptor | `SFX_RaptorCall` | 1 | it notices Chuck |
| Raptor | `SFX_RaptorHiss` | 1 | the bite's tell |
| Raptor | `SFX_RaptorSnap` | 2 | the bite, and leaping at him up a tree |
| Raptor | `SFX_RaptorDeath` | 1 | the tenth scratch |
| T-rex | `SFX_TRexStep` | 3 | every half stride (ground-shaking) |
| T-rex | `SFX_TRexRoar` | 2 | it notices him |
| T-rex | `SFX_TRexGrowl` | 1 | stalking under him in the giants |
| T-rex | `SFX_TRexSnap` | 1 | the bite, and snapping up at him |

## Candidates

**T-rex roars and growls**
- [t-rex calls — CaveboyTup (OpenGameArt)](https://opengameart.org/content/t-rex-calls): growls, huffs, bellows and roars built from CC0 alligator, lion and elk recordings. One 740 KB MP3. Best single source for roar + growl.
- [trexroar.wav — Opticreep](https://freesound.org/people/Opticreep/sounds/255405/): Allosaurus/T-rex roar.
- [Dinosaur Roars and Growls V2 — GojiS.O.S.](https://freesound.org/people/GojiS.O.S./sounds/641743/)
- [Dinosaur Breath, Moan, and Roar — 2create](https://freesound.org/people/2create/sounds/670510/)
- [Dinosaur call — Alessandrobraz](https://freesound.org/people/Alessandrobraz/sounds/708641/): two versions.
- [Dinosaur Snarl — LaurenPonder](https://freesound.org/people/LaurenPonder/sounds/639448/)
- [G12-01 Alligator Growl — craigsmith](https://freesound.org/people/craigsmith/sounds/437933/): low, reverberant; good for the stalking growl.
- [Alligator Growls 02 — D.jones](https://freesound.org/people/D.jones/sounds/527844/): real alligator; slowed down it suits the T-rex's growl.
- [SciFi Processed Alligator — CHallSmith](https://freesound.org/people/CHallSmith/sounds/870610/): slow heavy breathing, for close-up menace.

**Raptor calls, hiss and death cry**
- [Raptor / Small Dragon cry — HowesenbergFilms](https://freesound.org/people/HowesenbergFilms/sounds/828228/)
- [raptor.wav — taure](https://freesound.org/people/taure/sounds/340272/): several raptor noises, some deeper.
- [raptorshreak — Nerdwizard78](https://freesound.org/people/Nerdwizard78/sounds/643850/): velociraptor shriek.
- [Utahraptor Hiss — NaturesTemper](https://freesound.org/people/NaturesTemper/sounds/622809/): for the tell.
- [Crocodile scream (Amazonian Rainforest) — felix.blume](https://freesound.org/people/felix.blume/sounds/403373/): real young crocodile, close-miked; good raw material for the call or death cry.
- [Tiny Croc Chirp — iwanPlays](https://freesound.org/people/iwanPlays/sounds/511430/): pitched down, a raptor chatter.

**Jaw snaps (both)**
- [Jaw or teeth bite at air — Wigglesworth](https://freesound.org/people/Wigglesworth/sounds/830093/): made for monster bites.
- [Teeth Snapping — LucasDuff](https://freesound.org/people/LucasDuff/sounds/527489/): close-miked chomps.

**Footsteps**
- T-rex: [Giant step — DasWaff](https://freesound.org/people/DasWaff/sounds/721601/) (a huge step breaking branches), [Giant Approaches in Forest — rwschenk](https://freesound.org/people/rwschenk/sounds/588655/) (crunchy forest stomps), [GiantStompFootsteps — RobHorror240](https://freesound.org/people/RobHorror240/sounds/622737/).
- Raptor: [CC0 sounds library (OpenGameArt)](https://opengameart.org/content/cc0-sounds-library) has steps on leaves, gravel and mud; pitched down and layered they suit a 4 m raptor on the jungle floor.

**Larger bundles (optional):** [Monster Sound Effects Pack — Ogrebane](https://opengameart.org/content/monster-sound-effects-pack) and [Monster snarls — Darsycho](https://opengameart.org/content/monster-snarls) on OpenGameArt are listed as CC0: growls, grunts and death sounds.
