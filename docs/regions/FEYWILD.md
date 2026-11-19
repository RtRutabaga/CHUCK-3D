# The Feywild: 3D adaptation plan

**Status: approved by the user 2026-11-17 with the changes recorded under [Decisions](#decisions). Stage 1, the look slice, is built (2026-11-18, dev checkpoint "Feywild Look"; second pass the same day, after "Fix the flaws first"). The user approved the look on 2026-11-18 ("Approve, start stage 2"). Stage 2, the river cutscene and arrival, is built (2026-11-19, `FeyRiverScene.h`; see [the river cutscene](#the-river-cutscene)). Stage 3 is next.**

Sources: the user's Feywild vision and ten goal images (2026-11-17), the 2D game's `docs/development/PHASE-8.md` section 6 and 9 (river cutscene) and `PHASE-9.md` (thirteen maps), and the research below. Where the user's vision and the 2D game differ, the vision wins.

## The aim

The Feywild is the most memorable, immersive and beautiful part of the game. It is a very large, explorable twilight country that is lovely, slightly wrong and quietly dangerous. Chuck stays what he is: a 65 cm rat with no lines, no powers and no explanation of what is happening.

## What the D&D setting gives us

| Setting fact | What it means in the game |
|---|---|
| The Feywild is an echo of the Material Plane, more vivid and exaggerated | Familiar things, too large and too saturated: oak, fern, foxglove, lily and mushroom, not alien plants |
| Eternal twilight, the sun always just setting or rising, lit further by lanterns, fireflies and glimmering beings | The sun never moves. It sits on the horizon, and small magical lights do the rest |
| Distance and direction cannot be trusted | Paths that come back somewhere unexpected. A loop may return to the hub by a different road than it left on |
| Its most beautiful things are the most dangerous | Attractive hazards: the orchid that spits, the pollen that slows, the glowing moth that hurts |
| Fey crossings, where the veil is thin | Standing stone arches with faint runes through the country. Scenery only, never a fast-travel system |
| Fey courts, bargains, and food that traps those who eat it | Told only through objects left in the landscape, such as abandoned feasts of fruit gone strange |
| Redcaps: small, vicious, iron-booted, sickle-wielding, caps soaked in blood | The camp in the goal image. Their heavy boots are heard before they are seen |

Research sources: [Roll20 Feywild guide](https://pages.roll20.net/dnd/feywild-guide), [Bell of Lost Souls](https://belloflostsouls.net/?p=387905), [A Little Bit Human](https://alittlebithuman.com/dungeons-and-dragons-feywild/), [Black Citadel on redcaps](https://blackcitadelrpg.com/redcap-5e/). The rulebooks themselves were not available to read.

## Look and light

**One rule for the sky: blue, warming to peach and gold at the horizon, never purple.** Purple belongs to the wisteria, foxgloves and mushroom caps. This is what keeps it from looking like a night-elf forest.

- **Key light:** a low, warm, unmoving sun, giving gold rims on bark, moss and water.
- **Fill:** cool blue from the sky, so shadows are blue and still readable. It is never truly dark.
- **Accents:** cyan-blue and warm amber fungi, blue lanterns, bell-flower lamps and fireflies.
- **Air:** low mist on water and hollows, light shafts through canopy, drifting petals and spores.
- **Wet surfaces:** dew on leaves, puddles on paths, and still water holding the sky.

| Space | Goal image | Mood |
|---|---|---|
| General Feywild | `general-mushroom-forest`, `general-waterfall-glade` | Gold and blue, lush, inviting |
| Open vistas | `open-vista` | Brightest. Long views, waterfalls, giant wisteria trees, a floating island |
| Deep woods | `deepwood-lantern-path`, `deepwood-root-tunnels` | Darkest. Blue-teal, tunnels of branch and root, moss, light fog |
| Lily mere | `lily-pad-mere` | Mirror water, gold on mist, a mossy bridge |
| Maze and tower from above | `maze-tower-overlook` | The region's great reveal: the whole maze, the cloud stair and the tower on its cloud, under a blue sky with peach clouds. **Unlike the image, the stair does not glow** |
| Hedge maze | `maze-interior` | Dusk, mist, wet paving, blue lanterns, pan-pipe statues |
| Redcap camp | `redcap-camp` | Firelight and dried blood, red toadstools, wrongness in the beauty |
| Reactive flower | `hit-plant-closed`, `hit-plant-open` | Exact model target for the scratchable flower |

The goal images are paintings and the renderer has limits (no Lumen, no virtual shadows, screen-space reflections, DX11). We will match their composition, colour, light and density, and judge each area side by side against its image. The mirror water is the hardest part. It may need a dedicated reflection for the mere alone.

### Flora and fungi kit

- **Trees:** ancient twisted oaks with huge roots, giant purple wisteria trees, mushroom trees, ordinary oaks and birches for contrast.
- **Hanging:** wisteria, ivy curtains, moss beards, lianas.
- **Ground:** ferns, foxgloves, lupins, daisies, violets, bluebells, broad dewy leaves, crimson-veined leaves, moss, grass.
- **Water:** giant ribbed lily pads, small pads, pink water lilies, reeds, glowing fish.
- **Fungi, glowing:** blue caps, amber-gilled giants, violet caps, bracket shelves on trunks, house-sized mushroom trees.
- **Fungi, ordinary:** brown field mushrooms and the redcaps' blood-spotted toadstools.
- **Magical:** bell-flower lamps, reactive flowers, spitting orchids, pollen beds.
- **Life:** fireflies in chosen places, moths, butterflies, fat bumblebees, glowing fish, drifting motes.

## The map

About **900 m by 900 m**, with roughly **50 hectares** of walkable country. Chult's main valley is about 1 km long and 130 m wide, so this is around three times its size and far wider. The sizes below are targets to be tested by walking the blockout.

```text
                          [ CLOUD STAIRCASE ]
                                  |
                    +-------------------------+
                    |    THE GREAT HEDGE MAZE |
                    +-----------^-------------+
                                |
   +------------+        +------+--------+
   |  DEEPWOOD  |=======>|   HIGHMEADOW  |  displacer meadow,
   |  roots and |  hidden|   AND RAPIDS  |  rapids, waterfalls
   | underways  |  way   +------^--------+
   | [redcaps]  |               |  (the way forward)
   +--+-----+---+               |
      |     ^                   |
 out  |     | back by a         |
      v     | root tunnel       |
   +--+-----+-------------------+--+        +--------------+
   |      THE TWILIGHT CROSSROADS  |<======>|   LILY MERE  |
   |      (the central space)      |<======>| MOONMOTH FEN |
   +--+-------------+-----------+--+  loop  +--------------+
      ^             ^           |
 loop |             |           |
   +--+-------------+--+        |
   |  POLLEN ORCHARD   |<-------+
   |  NEEDLE GARDEN    |
   +-------------------+
                    ^
                    |
          RIVERBANK AND BLOOMING PATH
             (Chuck washes ashore)
```

| Area | Size | 2D maps it adapts | What happens there | Music |
|---|---|---|---|---|
| Riverbank and Blooming Path | 6 ha | 1, 2 | Quiet arrival, first reactive flower, no enemies | main |
| Twilight Crossroads | 5 ha | 13 | The central space: a vast old tree, rune arches, roads in every direction | main |
| Lily Mere and Moonmoth Fen | 7 ha | 7, 12 | Loop. Lily-pad jumping, flowers that raise pads, lantern moths | open / main |
| Pollen Orchard and Needle Garden | 6 ha | 3, 6 | Loop. Slowing pollen among blossom trees, then lanes covered by spitting orchids | main |
| Deepwood | 9 ha | 4, 11 | Loop. Root tunnels, moss, fog, brachiation, slide-jumps over ground roots, thorn mites | dark |
| Redcap Hollow | 3 ha | 8 | Optional camp within the Deepwood: avoid it, sneak past it or fight through it | dark |
| Highmeadow and Rapids | 9 ha | 10, 12 | The way forward. The great vistas, the displacer beast, Chuck-sized refuges, waterfalls | open |
| The Great Hedge Maze | 9 ha | 9 | See below | maze |
| Cloud Staircase | rises from the maze's heart | cloud staircase map | A winding stair of natural, unlit cloud, catching only the low sun, climbing out of the maze to the cloud under Zephyros' tower | maze, until his tracks arrive |

Two loops and the Deepwood return to the Crossroads. Highmeadow leads onward. The player is never given a marker or minimap. Landmarks do that job. **Zephyros' tower is the main one:** it hangs on its cloud above the maze, high enough to be seen from every open part of the region, so the player always has a far-off place to wonder about. The Crossroads tree and the floating islands support it.

### The Maze Overlook

Highmeadow ends at a high mossy rise under a great wisteria tree, composed to match `maze-tower-overlook`. The road comes out of the trees and the whole view opens at once: the maze spread below, waterfalls and mist around it, floating islands, the low sun, the cloud stair and the tower above. **This is the Feywild's signature moment.** Control stays with the player, with no cutscene. A stone path then winds down to the maze's main arch.

### The Great Hedge Maze

- **Shape:** curving, organic rings of hedge around the centre, as in `maze-tower-overlook`, not a square grid. Stone arches with blue lanterns mark the openings, and mossy statues, flowering wisteria, glowing mushrooms and low mist fill it.
- **The goal is always visible.** The stair and tower can be seen above the walls from inside, so the player knows where to go but not how to get there.
- Living walls 6 to 8 m high, about 300 m across, with arches, lanterns, statues and wet paving as in the goal images.
- **Parkour inside it:** wall runs along trellised sections, brachiation under ivy arches, climbs to raised walkways and Chuck-sized gaps through the roots.
- **Danger:** redcap patrols, spitting orchids covering long lanes, thorn mites, lantern moths and thorn hazards.
- **Whimsy:** flowers that shift the hedge gates (always reversible and never a trap), and statues that seem to have turned when you look back.
- **No cheating over the top:** a bumblebee swarm lives on the hedge tops. A warning buzz sounds on arrival, then the swarm knocks Chuck back down into the maze. It sends him back rather than killing him.

### Carried over from the 2D game

Reactive flowers scratched to change routes, slowing pollen, Chuck-sized passages big enemies cannot enter, thorn mites, redcaps, spitting orchids, lantern moths, the displacer beast, and cigarette tufts and breakables as rewards. The 2D game's Giant Tea Table is left out at the user's request. No inventory, keys or upgrades. Any talking NPCs wait for the user's decision and the user's lines.

## The river cutscene

It replaces the current placeholder, where the sweep returns Chuck to the Forecourt. It runs about 37 seconds.

1. **Sound only, about 7 s.** Near darkness around Chuck's head. Rushing water, muffled when he goes under, sharp and loud when he surfaces, in an irregular rhythm.
2. **Music enters** at the first glimpse of light: `FallToChult` from its last section at **48.5 s**, played through to the song's own ending (29.5 s).
3. **The Feywild opens around the river.** Dark banks give way to moss, roots, wisteria, glowing fungi and low gold sun.
4. **A short waterfall**, a plunge, and the current calms.
5. **He washes onto the bank**, lies still for a beat, pushes himself up, and control returns. `Feywild_main` begins.

**As built (2026-11-19).** The sweep's end at Phlegethos starts it (`APhlegBleed::TickSweep` → `AChuckCharacter::BeginFeyRiver`). It runs 36.5 s, from the sweep's end to the song's own end. The cuts fall on the score's bars.

| Time | What happens |
|---|---|
| 0–7 s | Dark in a root tunnel. Rushing water is muffled under a low-pass filter on his four dunks and returns sharp at each surfacing. |
| 7.0 s | The score enters at the first glimpse of the mouth's light. |
| 7.9 s (bar 26) | Out of the mouth. |
| 13.5 s / 17.3 s | Close on him, then behind him. |
| 21.05 s (bar 33) | The cut to the slice's stream above the two-step falls. |
| 26.7 s (bar 36) | The plunge into the pond. |
| 30.45 s (bar 38) | He washes onto the south shore (`CrashLand`: still, then up). |
| 34.2 s | The camera rises behind him. The game's camera takes over at 36.5 s. |

- **The river set:** its own set about 300 m south of the slice, out of its sight (`AFeywildSetting::BuildRiver`). It is built when the cutscene starts and hidden at the cut to the slice. The banks run from bare rock and root to moss, fungi, flowers and wisteria oaks.
- **Materials:** `M_FeyRiver` is running water with screen-space reflections, now also used for the slice's two streams. `M_FeyRoot` is the tunnel's bark, dark inside. `M_FeyGround` takes light from vertex alpha.
- **Checkpoint:** "Feywild Riverbank" (`FeyArrival`) is the save point and the Feywild's respawn. The dev menu entry "Feywild River (dev)" plays the cutscene.

## Music

Tracks change by area with slow crossfades, never restarting when Chuck crosses back and forth over a boundary.

| Track | File supplied | Length | Used for |
|---|---|---|---|
| Feywild main | `Feywild_main.wav` | 147.8 s | Arrival, Crossroads, orchard, general country |
| Feywild dark | `feywild_dark.wav` | 177.9 s | Deepwood, Redcap Hollow |
| Feywild open | `Feywild_open.wav` | 194.4 s | Highmeadow, Rapids, Lily Mere |
| Feywild maze | `Feywild_maze.wav` | 159.6 s | The hedge maze |
| Zephyros | Later, from the user | | The tower |

## Build order

Each stage ends with something the user can see and judge.

1. **The look.** One small finished piece of riverbank and forest, with the sky, light, water, first plants and fungi, compared against the goal images. **The user approves the look before the world is built out.** Everything depends on this stage.
2. **The river cutscene and arrival**, joined to the Phlegethos sweep, with music and checkpoints.
3. **The whole map walkable in rough form**, with the thresholds, district loading, all routes, checkpoints and music zones, to prove its size, flow and performance.
4. **The plant, fungus and rock kit**, then the art pass area by area: Crossroads, Lily Mere, Highmeadow, Deepwood, Orchard and Needle Garden.
5. **Mechanics and creatures:** reactive flowers, pollen, lily pads, thorn mites, orchids, moths, redcaps and the displacer beast.
6. **The Great Hedge Maze** with its parkour, enemies and bees.
7. **The Cloud Staircase**, ending at a stable boundary until the tower is planned.
8. **Polish and full playthrough.**

## Keeping it light on the machine

The player experiences one huge country, but the machine only ever holds a small part of it. **The Crossroads entrances do the loading**, as the user suggested.

### Thresholds

Every road out of the Crossroads passes through a **threshold**: a short winding passage, about 40 m long, such as a root tunnel, a mossy rock cutting or a bend through a rune arch. Its bends block the view both ways.

1. Chuck enters the threshold. The district ahead starts loading in the background.
2. Halfway through, neither side can be seen. When the new district is ready, the one behind is removed from memory.
3. If loading runs late, mist thickens and the far end stays hidden until it is ready. There is no loading screen and Chuck never stops walking. A brief fade is the last resort.

**At most one district and its threshold are loaded during normal play, and two districts only while crossing.** The same applies to the Deepwood's hidden way to Highmeadow and to the maze gate.

This also serves the setting. Districts no longer have to fit together physically, so a road can leave east and return from the south, which is exactly the Feywild's untrustworthy geography.

### Inside each district

- **Built ahead of time.** Chult generates its meshes while loading. Feywild districts will be baked into ready-made assets in advance, which removes most of that load cost.
- **One shared kit.** All districts reuse the same plants, rocks and materials, so texture memory stays nearly flat as the world grows.
- **Instanced plants with detail levels.** Thousands of copies draw cheaply, simplify with distance and disappear beyond a set range. Wind movement runs only near Chuck.
- **Glow without real lights.** Mushrooms, lanterns and flowers glow through their materials. Each view gets a few real lights, and very few of those cast shadows.
- **Effects near Chuck only.** Fireflies, petals and spores use the existing lightweight sprite system and exist only around him.
- **Painted distance.** Far hills, giant trees and the floating islands are a single simple backdrop for each district.
- **One cheap tower, seen everywhere.** The tower, its cloud and the stair exist as one simple far-view model that stays loaded across the region.
- **A stand-in maze for the overlook.** From the overlook the maze is a single simplified model of its real layout. The real maze loads in quadrants as Chuck walks down the winding path, which works as this view's threshold.
- **Layouts that hide things.** The Deepwood's tunnels and the maze's walls block long views. The maze is also split into quadrants that load separately. Highmeadow is the most demanding area, so it gets the strictest plant limits.

### Budgets

Stage 3 measures memory, frame rate and threshold load time in a packaged build on this machine, with Chult as the baseline. No district may use more memory or run slower than Chult. Art passes must stay inside those limits.

## Technical risks

- **Late loading.** If baked districts still load too slowly on this machine, thresholds get longer or use the fade. Stage 3 tests this before any art depends on it.
- **Density against frame rate.** The goal images are extremely dense, so the budgets above may limit how closely some views can match them.
- **Reaching the goal images.** The CC0 scans will carry bark, rock, moss and ordinary plants, while the fantasy plants still have to be custom built to match them.

## Out of scope for this plan

Zephyros' tower beyond its distant exterior. `maze-tower-overlook` sets that exterior (pale stone spires on a cloud, under a blue, starred wizard's-hat roof), and the user will supply references for the explorable tower. Also out of scope: talking NPCs (the user will decide), and anything explaining the collided worlds or Chuck's origin.

## Decisions

Made by the user on 2026-11-17:

1. **No Giant Tea Table section.**
2. **The cutscene's water-sounds-only opening is under 12 seconds** (planned at about 7).
3. **`feywild_dark.wav` is the dark track.** `Feywild_.wav` is ignored for now.
4. **Free CC0 scanned assets are approved** for bark, rock, moss and plants. Licences are recorded per asset.
5. **The cutscene's river is the Feywild river.**
6. **Make the large area less taxing,** with loading at the Crossroads entrances considered: see [Keeping it light on the machine](#keeping-it-light-on-the-machine).
7. **The Cloud Staircase does not glow,** unlike `maze-tower-overlook`.

## Goal images

Copied unchanged from the user's files to `References/Feywild/`.

| File | Original name | SHA-256 |
|---|---|---|
| `general-waterfall-glade.png` | `Feywild_1.png` | `b0def65a0cfdd92c91f9a647e9bd2fcbbe9d25d5647b556af1e6f070e73f7150` |
| `general-mushroom-forest.png` | `Feywild_3.png` | `5c29275c355153800308c73a98e7720469756710d01686949b117e6902f29013` |
| `deepwood-lantern-path.png` | `Feywild_deepwod_2.png` | `507b5e71efb950f9294d444d0a096196addb5549af7a4e9bb8243801873db51f` |
| `deepwood-root-tunnels.png` | `Feywild_deepwood_1.png` | `66b83eb15b997fa08cacd94770d528f9396337a5487941c16df501fa1711f451` |
| `lily-pad-mere.png` | `Feywild_lillypads.png` | `47426416416124f75130be5263a3a2ab3453a76f810d2127de9a8b10d4ca4cf5` |
| `maze-tower-overlook.png` | supplied in chat, 2026-11-17 | `a292c53fa12916a256d75cc44c6db76cfc97a23e424915102f3b79836f5907c8` |
| `maze-interior.png` | `Feywild_maze_interior.png` | `f280dfd655f0a4c14da6f47333963e4b3f5cab4f17b22c1e331a4cdde0154921` |
| `open-vista.png` | `Feywild_open.png` | `58e4c42658fe35472cadbcd877ccc02c07abf56005b0609eb642d1ea0efb27ce` |
| `redcap-camp.png` | `Feywild_redcap_camp.png` | `935307f6e267c6083d75f62800de3ddcd851de3b5b98fac6e8be9c8d9a9b4c25` |
| `hit-plant-closed.png` | `feywild_hit-plant_closed.png` | `964b09a91141acc5a5a3217d367ea66d2e1c674cf445a48ee96cfba24c2363d7` |
| `hit-plant-open.png` | `feywild_hit-plant_open.png` | `b8eb8cb9d8cfb9d8e35789523339e137ea144d5a49888c6cb8a8506ecffa4ed6` |
