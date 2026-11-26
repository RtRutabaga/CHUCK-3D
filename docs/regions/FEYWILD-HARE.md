# The Feywild: the great hare (plan)

**Status: approved by the user 2026-11-25 with the plan's recommendations for its open questions; built 2026-11-25 (see [What was built](#what-was-built-2026-11-25) at the end).** A rideable mount for the Feywild only, asked for by the user because of the region's size. Part of [FEYWILD.md](FEYWILD.md); written separately so as not to collide with work on that file.

## The idea

Great hares graze in herds in the open districts. Chuck can climb onto one and ride it across the country at speed, well past his run and sprint. Each hare stays within its own district: at a threshold it stops and won't go on, and on the way to the maze it throws him and bolts. Lose one and another grazes not far off.

Why a hare: hares are classic fey animals (D&D's Feywild has the harengon, rabbit folk, so a large plain hare belongs there without being a person), it is the right size for a 65 cm rat, its bounding gait reads well at speed, and a skittish animal that throws its rider and bolts needs no explanation.

## The user's decisions (2026-11-26)

1. **The mount is a great hare**, for the Feywild only.
2. **It roams in herds**, so a lost hare can be replaced by another.
3. **It throws him and runs off near the maze**, so it never enters the maze.
4. **The displacer beasts do not spook it.** He may ride to outrun them.

## The animal

| Item | Target |
|---|---|
| Size | About 1.3 m nose to tail, 75 to 85 cm at the shoulder. Chuck sits behind its shoulders like a child on a pony, his head about 1.3 m up |
| Look | An exaggerated brown hare, as the Feywild exaggerates the familiar: long black-tipped ears, a moss-grey to dusk-gold coat (vertex colour on the shared fur material, no new texture), a pale belly, large amber eyes. No saddle or tack: he holds on by the scruff |
| Feywild touch | Faint pale-gold flecks in the coat that catch the low sun, and a few seed heads caught in the fur. No glow |
| Build | Signed-distance sculpt in the creatures' kit (`Tools/build_fey_creatures.py` or its own script), on its own small skeleton (spine, four legs, ears, a scut); weighted the way the trio's horse is, blended over the joints |
| Sound | Soft thumps for the bound, a thump of a hind foot as a warning, a short squeal when it throws him (CC0 recordings where they exist, credited in the manifest; otherwise synthesised, as the crows' are) |

## Behaviour

**Grazing (unridden).** A herd of two to four in each chosen district, each hare kept within about 25 m of its home ground. They nibble, sit up with their ears turning, hop a few metres and settle again. When Chuck runs or sprints straight at one within about 6 m it lollops a little way off. A slow approach (walking) lets him reach it.

**Mounting.** Within about 1.2 m of a calm hare the talk prompt shows (F / Y, the existing talk key). Pressing it plays a short scramble up its side and he is on.

**Riding.**

| Control | On the hare |
|---|---|
| Move | Steers the hare, relative to the camera as his walking is. A light push is a hop-along at about 150 cm/s |
| Run (Shift / LB) | The bound, about 550 cm/s (his run is 225 and his sprint 380). The displacer beasts chase at 250, so he outruns them |
| Jump (Space / A) | A long hare leap, about 3.5 m and 70 cm high, over logs, the pollen strip and ditches |
| Roll (B) or Talk again | He slides off sideways and the hare stays where it is, calm, so he can climb back on |
| Slash, climbing, wall runs, ledges, ladders, brachiation, crawling | Not available while riding |

The camera keeps its orbit, raised by the hare's height and drawn back a little at the bound's speed.

**Where it stops and throws him.** The hare keeps to its own district's open ground:

- **A threshold mouth:** at about 8 m from a threshold it slows, turns aside and will not go on. Pressing on makes it thump its foot and turn away. He gets off to go through on foot.
- **The way to the maze:** from the overlook's brink and on the stone path down to the maze's main arch, it rears, throws him and bolts back toward its herd.
- **Also throws him** in deep water (before he would be put back), at thorns, on entering Redcap Hollow's ground, and when an orchid's seed or a lantern moth hits him.
- **Pollen** slows it as it slows him (to half).
- **A displacer beast** does not spook it (decision 4). If a beast does reach him, its lash costs Sanity as usual and he stays on.

**After a throw** he lands in the existing `CrashLand` (still, then up), with no Sanity lost from the fall. The hare bolts home and grazes again. Nothing is saved: on a death or checkpoint start every hare is back at its home ground, unridden.

## Where the herds live

| District | Hares | Why |
|---|---|---|
| Highmeadow and Rapids | 4 in two herds | The largest open district, the way forward, and where the beasts are |
| Twilight Crossroads | 2 on the ring road's open ground | The hub, so a ride can start almost anywhere |
| Pollen Orchard and Needle Garden | 2 by the orchard's edges | Long roads between the rows; kept clear of the orchids' lanes |
| Lily Mere and Moonmoth Fen | 2 on the dry ground | Long shores; the deep water throws him |
| Riverbank and Blooming Path | None | The arrival stays quiet, as planned |
| Deepwood and Redcap Hollow | None | The slowest district, narrow roots and tunnels |
| The maze | None | Decision 3 |

Each herd is built with its district (`AFeyLife`, a part per frame) and gone with it, like the other creatures. No hare is carried across a threshold, so the one-district loading is untouched and speed cannot outrun it.

## Chuck

- **A riding pose:** seated behind the shoulders, forepaws in the scruff, the jacket and tail following the bound. Played as a pose with the hare's bound, not physics.
- **Clips:** mount scramble (about 0.8 s), sit and lean, a lean into turns, the slide-off, and a throw (lifted and tossed forward), landing into the existing `CrashLand`.
- Attached to a saddle socket on the hare; his movement is switched off while he rides, and his capsule rides with the hare's.

## Building it

1. The hare: sculpt, rig, its clips (graze, sit up, hop, bound, leap, foot thump, rear and throw, bolt), review stills against reference photos of brown hares.
2. Chuck's riding clips (above).
3. The behaviour (`FeyHare.h/.cpp`): grazing, approach and shy, mount and steer, the stop at thresholds, the throw zones, the bolt home, the reset.
4. Herds placed in the four districts, on open, level, dry ground, kept off the orchids' lanes, thorns, deep water and the overlook's path.
5. Tests: new `-ChuckFeyMapTest=life` steps (below), the route walker run without hares as it runs without the other creatures, package verification, then the user's hand play.

## Checks planned

- A hare grazes and shies from a sprint; a walking approach reaches it; the prompt shows and he mounts.
- Riding: the bound reaches about 550 cm/s; the leap clears a log; pollen halves it.
- It stops before each threshold it can reach and will not go through.
- On the overlook's path it throws him, he lands in `CrashLand` with no Sanity lost, and the hare runs home.
- Deep water and thorns throw him; a seed or a moth throws him.
- A displacer beast chasing him is outrun.
- The slide-off leaves the hare calm and he remounts.
- A death resets every hare.
- Frame time: each herd district stays at the 60 fps cap with the hares (the riverbank and Deepwood are unchanged).

## Risks

- **Skipping the traversal.** The brief asks for each region's moves to matter. The hare stays on open ground; every climb, the pads, the Deepwood and the maze are still done on foot.
- **Collision at speed.** At 5.5 m/s it can catch on roots, rocks and planted kit. Its movement uses a low capsule with step-up, and the tests ride the herds' ground at the bound.
- **The bound's animation** is the hardest clip. A poor one will read at once at speed.
- **Rider contact.** As with the trio, his paws and legs against its coat need a clearance check.
- **Camera** at the bound in tight places, such as the orchard's rows.

## Open for the user (answered 2026-11-25: "go with its recommendations")

- The bound's speed (550 cm/s planned) and whether the leap should be longer.
- Whether one hare should also graze in the Riverbank, away from the arrival.
- The coat colour: moss-grey, dusk-gold or a mix across the herd.

The answers taken: the bound at 550 cm/s and the leap as planned (about 3.5 m, 70 cm high); no hare in the Riverbank;
the coats a mix across each herd, from moss-grey to dusk-gold.

## What was built (2026-11-25)

- **The hare:** `Tools/build_fey_hare.py` (Blender 4.5.14, the creatures' SDF kit): about 1.2 m from nose to scut, its
  back about 80 cm up, on a skeleton of its own (spine, chest, neck, head, two-bone ears, hips, scut, three-bone
  forelegs, four-bone hind legs, and a `saddle` bone with no weights). Coat by vertex colour: tawny agouti back, warmer
  flanks, pale belly, ginger chest, black ear tips, white scut black on top, amber eyes, pale-gold flecks, seven seed
  heads in the fur. `Tools/import_fey_hare.py` makes `/Game/Art/Feywild/Creatures/Hare/SK_FeyHare` and `M_FeyHareFur`:
  the displacer's CC0 fur (ambientCG Fabric084) projected in the rest pose over the vertex colours, a `Tint` per hare.
  (To rebuild the material, delete its `.uasset` first: clearing a loaded material's graph crashes UE 5.7.)
- **Its motion:** `FeyHare.h/.cpp` (`AFeyHare`, a character with a poseable mesh). The gait is procedural, not clips:
  foot paths by phase and speed, two-bone reaches for each leg, and the back flexing and extending through the bound.
  Moods are graze (head down, nibbling), sitting up with its ears turning, the hop, the bound, the leap, the thump of
  a hind foot, the rear and the bolt.
  The pose is solved in component-space positions and turns about each joint, because the imported rest transforms
  carry the armature's scale. A first version that composed rotations through them sent the legs astray.
- **Chuck:** three new clips in `Tools/build_chuck_v1.py`, imported with `Import-ChuckV1.ps1 -Clips`:
  - `RideSit`: astride behind its shoulders, leaning forward, forepaws in its scruff, legs down its flanks.
  - `RideBound`: one bound cycle, played at the hare's own phase over the sit, by its speed.
  - `Mount`: 0.8 s up its side.

  There is a new gait, `EGait::Ride`. The hare moves him (`UpdateRide`) and the camera draws back at the bound. Thrown,
  he tumbles into his `CrashLand`, with no wall slam and no Sanity lost.
- **Herds:** `AFeyLife::SpawnHares`, built with each district.
  - Highmeadow has two herds of two; the Crossroads, the orchard and the Lily Mere have one herd of two each.
  - Each herd's home is searched for near its planned spot. It must be open, level, dry ground, clear of thresholds'
    mouths, orchid lanes, pollen, the overlook and FeyLife's clearings, with nothing standing on it.
  - The homes found: Highmeadow (-100500, 7000) and (-95692, 2076), the Crossroads (-62500, -5000), the orchard
    (-31156, 8290), the Lily Mere (-63000, 34000).
- **Sounds:**
  - The thump and the bound's thuds are cut from BigSoundBank's CC0 "Bunny hutch" (rabbits stamping their feet; bsb_1376,
    fetched by `Tools/Fetch-FeySounds.ps1`), with "Feet in leaves" (bsb_0137) under the thuds.
  - The squeal is synthesised.
  - Built by `Tools/build_fey_hare_sfx.py`.
- **Controls as planned:** F / Y (the talk key) mounts and slides him off; so does Roll. Run toggles the bound, and it
  ends when the stick is let go. Jump is the leap. The HUD shows "F   Ride" by a calm hare.
- **Checks:** `-ChuckFeyMapTest=life` steps 92 and 82 to 90 (`hares_crossroads`, `hares_spawned`, `hare_shies`,
  `hare_prompt`, `hare_mount`, `hare_bound`, `hare_leap`, `hare_pollen`, `hare_refuses`, `hare_throws_maze`,
  `hare_throw_crashland`, `hare_throw_no_sanity`, `hare_bolts`, `hare_slide_off`, `hare_stays_calm`, `hare_remount`,
  `hare_reset`).
  - `CHUCK_FEY_HARE_POSES=1` with `CHUCK_FEY_LIFE_FROM=82` adds step 91: stills of each pose from the side
    (`Life_HarePose_*`).
- **Not built or not tested:**
  - The deep-water, seed and moth throws are in the code but have no test. The throw zone uses `InDeepWater` or
    `InRiver`.
  - Displacer lashes are told apart from seeds and moths by the counters (`LashHits`).
  - The plan's thorns throw does not apply: no hare reaches thorns (there are none in its districts).
  - Redcap Hollow is the Deepwood's, where no hare goes.

## Second pass (2026-11-26, after the user's play)

The user found the forelegs wrong, too few hares where they come in, and many throws without a clear cause.

- **Legs.** The leg solver found no child joint for any limb bone. The import's 1/100 scale on local translations hid
  them below the cutoff, so every segment was aimed as if it pointed forward.
  - The forelegs folded up into the chest. The hind legs were right only by chance, with the hock trailing far behind.
  - Children are now found by component-space distance (`AFeyHare::BeginPlay`).
  - The poses follow a real hare:
    - **Forelegs:** slim, nearly straight under the shoulder, with the elbow back.
    - **Hind legs:** the long foot runs to a raised heel. In the bound the hind feet swing forward past the forefeet
      (the hare's overreach in the gallop).
    - **Grazing:** on straight forelegs, with the crouch in the hind legs and the neck reaching down.
    - **Sitting up:** the forelegs hang straight down against the chest.
  - Evidence: `Local/hare2-poses-1.png` (all poses) and `Local/hare2-poses-2.png` (graze, sit, hop, bound, rear after
    tuning).
- **Hares at the ways in.** One more herd of two just inside each way the player arrives by on the route through:
  - the Riverbank into the orchard;
  - the orchard into the Crossroads (the second mouth already had the Crossroads herd within 40 m);
  - the Crossroads into the Lily Mere (two mouths) and into Highmeadow;
  - the Deepwood into Highmeadow.

  Each herd is placed 27 m in from the mouth and 7 m beside the way in. Its home search is limited to 5 rings
  (about 22 m).
  - Measured in Highmeadow: the nearest hare is 22.6 m from the Crossroads mouth and 26.4 m from the Deepwood mouth
    (test `hare_near_entrance`). Highmeadow has 8 hares, the Crossroads 4.
- **Throws only where the game needs them.** The causes of the throws he saw:
  - the overlook and the way down to the maze, each counted from 14 m away;
  - any wade through Highmeadow's stream;
  - any orchid seed or moth hit.

  Now:
  - The hare refuses (thump, turn away) at every threshold mouth, including the way down to the maze. It also refuses
    within 17 m of Highmeadow's western brink (the overlook's drop) and at deep water.
  - It wades streams.
  - Bites, seeds and moths never throw him.
  - It throws him only if it is on its way out of its district all the same (on a threshold's way, or off its land).
    Deep water slides him off without a throw.
  - Tests:
    - `hare_refuses_brink` (new step 93): ridden at a run toward the overlook, it refused 16.7 m from the edge with no
      throw.
    - `hare_refuses`: it now also requires no throw.
    - `hare_throws_leaving` (step 88, replacing `hare_throws_maze`): put on the maze stair itself, it throws him.
