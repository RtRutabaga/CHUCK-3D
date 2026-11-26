# The Feywild: the great hare (plan)

**Status: planned 2026-11-26, awaiting the user's approval. Nothing is built.** A rideable mount for the Feywild only, asked for by the user because of the region's size. Part of [FEYWILD.md](FEYWILD.md); written separately so as not to collide with work on that file.

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

## Open for the user

- The bound's speed (550 cm/s planned) and whether the leap should be longer.
- Whether one hare should also graze in the Riverbank, away from the arrival.
- The coat colour: moss-grey, dusk-gold or a mix across the herd.
