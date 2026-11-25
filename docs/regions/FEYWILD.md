# The Feywild: 3D adaptation plan

**Status: approved by the user 2026-11-17 with the changes recorded under [Decisions](#decisions). Stage 1, the look slice, is built (2026-11-18, dev checkpoint "Feywild Look"; second pass the same day, after "Fix the flaws first"). The user approved the look on 2026-11-18 ("Approve, start stage 2"). Stage 2, the river cutscene and arrival, is built (2026-11-19, `FeyRiverScene.h`; see [the river cutscene](#the-river-cutscene)). Stage 3, the whole map walkable in rough form, is built (2026-11-19, `FeyMap.h`; see [the rough map as built](#the-rough-map-as-built-stage-3)). Stage 4, the art pass, is built (2026-11-20). Stage 5 is under way: [part one](#stage-5-part-one-the-first-mechanics-and-hazards-2026-11-21), the flowers, pollen, orchids and moths, is built (2026-11-21); [part two](#stage-5-part-two-the-creatures-2026-11-22), the thorn mites, redcaps, displacer beast and pad-raising flower, is built (2026-11-22). Stages 6 to 8 (the maze, the Cloud Staircase to its landing, polish) are built (2026-11-23; see [stages 6 to 8](#stages-6-to-8-the-maze-the-cloud-staircase-and-polish-2026-11-23)), and the maze's [parkour and thorns](#the-mazes-parkour-and-thorns-2026-11-23) after them. The tower beyond the landing is not planned yet.**

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

**One rule for the sky: blue, warming to peach and gold at the horizon, never purple.** Purple belongs to the wisteria, foxgloves and mushroom caps. This is what keeps it from looking like a night-elf forest. (The user, 2026-11-22: the rule is against purple sky and purple light over everything, not against purple things. Purple creatures and features, such as the flying jellyfish, are fine.)

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
- **Hit plants in the maze (the user, 2026-11-22):** a player who reaches the end in good time finishes by scratching a hit plant (the reactive flower) that opens the way into the heart. One who stays lost for a long time finds more hit plants appearing, each opening a new entrance through a hedge, so the maze grows easier the longer he is in it.
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

### The rough map as built (stage 3)

Built 2026-11-19 (`FeyMap.h/.cpp`, test `FeyMapTest.cpp`). The land, planting and blockout are placeholders from the look slice's kit; stage 4 replaces them.

- **Layout.** Each district stands in its own rectangle west of the look slice; every road between two of them is a threshold through the gap. The look slice is the riverbank's east end, and its path now runs on west into the Blooming Path.

  | District | Size | Music | What stands in for now |
  |---|---|---|---|
  | Riverbank and Blooming Path | 198 x 112 m plus the slice | main | Path, oaks, bell-flower lamps, flowers |
  | Orchard and Needle Garden | 245 x 250 m | main | Rows of small trees; lanes between low stone walls with tall foxgloves |
  | Twilight Crossroads | 225 x 220 m | main | The vast old tree on a ring path, standing stones, a rune arch at each of its seven roads |
  | Lily Mere and Moonmoth Fen | 265 x 265 m | open | Six shallow pools with lily pads, reeds |
  | Deepwood with Redcap Hollow | 300 x 300 m | dark | Dense old oaks, ferns, fungi; the hollow's ring of stones and fire pit |
  | Highmeadow and Rapids | 300 x 300 m | open | Open meadow climbing 20 m west; shallow rapids with a chute and a ford; the overlook under a great tree, open to the maze |
  | The Great Hedge Maze | 300 x 300 m | maze | Four rings of 7 m hedge with stone arches, dead ends, statues; the Cloud Staircase's first steps, then a boundary |

- **Thresholds.** Ten, each about 40 m: rock cuttings, root tunnels and the overlook's stone descent (20 m down). Each is an S whose middle hides both ends. The two loops (Orchard, Lily Mere), the Deepwood's two ways and its hidden way to Highmeadow are all there.
- **Loading.** A district is planned on a worker thread when he enters a threshold toward it, then put in place a few milliseconds each frame. The district behind is dropped once he is past the bend and the one ahead is ready. If a district were late, mist would thicken and that end would stay shut. Checkpoint starts build their district at once.
- **Distance.** A low painted land with forest under and between the districts, ridges closing the horizon, Zephyros' tower on its cloud above the maze with the cloud stair winding up to it, and a stand-in maze seen from the overlook.
- **Checkpoints.** Each district has one, in the dev menu's new ninth column. A death returns him to the checkpoint of the district he is in.
- **Music.** Main, dark, open and maze tracks by district, crossfading over 4 s and keeping their place. In a threshold whatever plays goes on.
- **Measured (editor, 1280x720, 60 fps cap).** The whole route walked by the test, 5.4 km through all ten thresholds into the maze: never stuck, never late, at most two districts standing. Each district was ready 0.5 to 1.1 s after he entered its threshold; a checkpoint start takes under 0.5 s. Every district held 16.7 ms a frame (Chult's landing view, the baseline, averaged 23 ms); the worst frame crossing a threshold was 25 ms. The process's memory stayed between 4400 and 4550 MB in the districts against 4477 MB at Chult. That figure includes Chult, which stays loaded behind the Feywild, so it is not a clean per-district number.
- **Walking time.** At his run (2.25 m/s) the whole route is about 40 minutes; a district takes one to two minutes to cross.

### The art pass and the user's changes (2026-11-20)

The user asked for the art pass plus specific changes. As built:

- **Thresholds are solid ground.** The floor and the rock either side are one heightfield measured from the way's middle (offset cross-sections folded over on the S-bends). A cutting has rock walls, broad mossy tops and outer slopes; a root tunnel runs between earth banks under a roof as long as the tube. Both sink into each district past its mouth. The painted distance keeps its trees 48 m clear of every threshold (they stood up through the canyons).
- **Deep water crossed only on lily pads.** A deep half-moat round the Lily Mere's second threshold (from the Crossroads): 13 big pads about 1.2 m apart (a hop each) with collision under each. In the water he is put back on the last ground he stood on, with a splash.
- **The Deepwood.** Its own oaks (`SM_FeyOakDeep0/1`, Blender kit): mossed to the crown, nearly twice the roots, some arching, moss hanging in curtains from the limbs (`M_FeyHangMoss`). About twice as many trees, moss clumps, glowing fungi, small blue lights low among the roots, drifting mist. Its gloom fades in through its thresholds: the sun dimmed and cooled, a bluer fill, less sky light, fog three to four times as thick and blue, lower exposure.
- **Signs.** 13 wooden signposts (`M_FeySign`: weathered wood, nonsense glyphs cut into the arrows) lead from the riverbank through the orchard and Needle Garden, the Crossroads, the Lily Mere's fen and pads, Redcap Hollow and the hidden way, the ford and the overlook. From Highmeadow they start to lie (one points at the sky); the overlook's post points every way; the maze gate's eight arrows go every way, up and down included; at the heart one points straight up and one straight down.
- **The maze at a person's scale.** Corridors 2.6 m wide between leafy hedges 8.4 m high and 1.2 m thick (`M_FeyHedge`), 29 rings and 3,097 cells grown by a randomised Prim's walk with a few loops, round the plaza with the cloud stair's foot. The shortest way through is 195 m. Mossy statues stand in some dead ends; stone arches with a few glowing runes at the gate and the heart. From the overlook the ground now falls away, so the maze shows below.
- **Stone.** Arches, statues and the maze's arches are carved stone (`M_FeyStone`); the Crossroads' rune arches glow faintly. The orchard's rows are small wisteria-hung oaks.
- **Music.** In the main areas Feywild_main and the user's Feywild_ (`SW_FeywildAlt`) take turns, each crossfading into the other near its end.
- **Measured (editor, full route):** 5.0 km, stuck 0, drowned 0, late 0, at most two districts, all checkpoints OK, every district at the 60 fps cap (Deepwood 16.9 ms), worst threshold frame 28 ms.

### The user's fixes (2026-11-21)

- **No more seeing through the canyons.** A threshold's heightfield used to leave out its floor and the feet of its walls inside each district's rectangle, relying on that district's land to cover them. A district behind him is dropped once he is past the bend, and the one ahead is not loaded until he is a third of the way in, so the walls stood over holes. It is now closed everywhere (inside a rectangle it lies 30 cm under the district's land). `-ChuckFeyMapTest=cracks` casts about 33,000 rays from each threshold's floor, with both districts, only the near one and only the far one standing, and counts any that pass under the threshold's own rock without striking something visible: the first canyon had 5,504 such sight-lines before the fix; every threshold now has 0.
- **Spots within the districts.** Ten more checkpoints (`FeyMap::Spot`): Riverbank West (the first canyon's mouth), the Needle Garden, the Lily Mere Fen, the Lily Pads (the landward end, before the deep water), Redcap Hollow, Deepwood North (toward Highmeadow), the Highmeadow Ford, the Maze Overlook, the Maze Middle (halfway along the way through) and the Maze Heart (the stair's foot). A death returns him to the nearest of his district's checkpoint and the spots in it he has reached; the save and the dev menu (a tenth column) know them all.
- **The Tower's cloud and the Cloud Staircase.** The cloud is a heap of noise-billowed lumps (broad flat ones for its body, smaller ones round its rim and on top, soft lobes under it), not a lathe. The stair is one continuous band of cloud from the heart to the cloud, its top stepped (a flat tread per step, a riser in the last tenth), billowing at its sides and under it, so there is no gap between steps. `M_FeyCloud`: white to lavender-blue away from the sun, warm where it catches it, a little light of its own, its edges feathered (dithered where it turns from the eye) so the lumps melt together; the treads stay solid. The first eight steps are walked on their footing (hidden boxes, now covering each step's whole span); the stair beyond is still scenery (stage 7).
- **The hedges.** Each hedge carries a shell of small leaf sprigs (`SM_FeyHedgeLeaves`, Blender kit, the oaks' CC0 leaf atlas, `M_FeyHedgeLeaf`) rooted in its faces with a ragged crown, near only (out to 56 m); the box under it is four layers of the same sprigs on a near-black green (`M_FeyHedge`, each face's own UVs), which is all that shows beyond.
- **The Deepwood.** Its blue lights among the roots are about 1.7 times as bright, reach 14 m and are a little more frequent; the blue fill is a little brighter in the gloom; its fog is denser (density 6.5 times the open woods', was 4.5) with 150 veils of mist (was 90), a third of them along the paths. The hanging moss on its oaks is now beards of separate strands, each a thin tapering ribbon crossed with another, wandering as it falls, a few draped in a sag along the limb (`M_FeyHangMoss`: opaque, edges eaten slowly, no fine detail to shimmer).

### The user's second fixes (2026-11-21)

- **Canyons cut through ridges.** The user still saw the outside from the first canyon (stills with a slit in a wall and the meadow beyond). Behind its crest a way's rock used to slope down to the gap between the districts, and near each mouth it sank into the district with the district's rim (notched for the road): beside the opening the eye went past it into the gap, where the ground simply ends, and to the backs of the rims, which are one-sided and do not show. Now every way except the maze's stair is cut through a ridge: past the crest a broad plateau (13 m over a cutting, 8 m over a root tunnel) runs out 26 m, holds its height inside a district near its edge where it meets the rim, and is never lower than a district's edge just outside it. District rims never fall below 10 m. The see-through check now also counts low sight-lines that end in the gaps, ones that reach a district other than through the way's mouth, and hits on the backs of surfaces; stills walk the first canyon both ways (`Art_Canyon_*`).
- **Root tunnels.** The tube ends where its banks end, in a steep earth face closed round the opening, with a thick lip of root and great roots arching over the mouth and down into the ground either side.
- **The Deepwood's light.** The floor's blue lights are far softer (22-36 cd); light comes instead from the glowing caps: a soft light of their colour over each of the 140 richest clusters, stronger where more grow; the Deepwood's own fungi, shelves and bark veins glow brighter (material parameters on its instances only: elsewhere unchanged); a little more sky light in its gloom.
- **The maze from above.** Its hedges have rounded, lumpy crowns, each a slightly different green, and the overlook's stand-in casts their shadows down the corridors. The ground between the brink and the maze is now its own valley (`BuildBrink`) instead of the painted distance's low ground. No oaks in the overlook's view down onto the maze (neither Highmeadow's nor the maze's side facing it), and no ridge on the stair down the brink. The hedge core is brighter.

## Stage 5, part one: the first mechanics and hazards (2026-11-21)

Built in `FeyLife.h/.cpp` (one `AFeyLife` per loaded district, after PHASE-9.md sections 4, 5 and 7):

- **Reactive flowers.** The bud on broad leaves of `hit-plant-closed/open`, modelled and animated in code. His scratch makes it pulse and shake, then open into a star with curled tips and stamens; only then does the curtain of vines it rules draw back into the ground. Scratched again it closes and the vines grow back, never over him. All back as they began when he respawns; nothing saved. **The first one** stands by the riverbank's road: its vines shut the only gap into a ring of boulders with three cigarettes inside.
- **Slowing pollen.** Beds of small glowing gold flowers with spores drifting over them; on the ground in one he walks at half speed (measured 225 to 120 cm/s), at once and only while there. A strip over the orchard's road a jump clears, a broad bed after it, two more among the rows.
- **Spitting orchids.** Three south of the Needle Garden's road, taller than he is: each swells and glows pink, then spits a hard seed north across the road every 2.4 s, a third of a beat after its neighbour; a seed stops at what it meets and costs a Sanity point. They are hazards to time, not to fight.
- **Lantern moths.** Three over the way out to the Moonmoth Fen: wings wider than he is tall, amber at the root and teal at the edge with an eye, lit from within with a light of their own, weaving across the path at his shoulder; a touch costs a Sanity point; they cannot be scratched.
- **Checked:** `-ChuckFeyMapTest=life` (vines block, the flower opens them, he walks in and takes the cigarettes, pollen halves his speed, an orchid's seed and a moth's touch hurt), also a step of the package's verification (`feyLife`). The route walker runs with them calmed.
- **Part two (next):** thorn mites, redcaps (the camp in Redcap Hollow), the displacer beast in Highmeadow, flowers that raise lily pads, and more flowers through the districts.

## Stage 5, part two: the creatures (2026-11-22)

After PHASE-9.md sections 7.1, 7.2 and 7.5 and the goal image `redcap-camp`. Meshes from `Tools/build_fey_creatures.py` (signed distance fields with the Phlegethos creatures' kit, review stills on request), imported by `Tools/import_fey_creatures.py` into `/Game/Art/Feywild/Creatures` on the Phlegethos creatures' material. Behaviour in `FeyCreatures.h/.cpp` and `TempleSkeleton` (the redcaps); placed, reset on his respawn and dropped with their district by `AFeyLife`.

- **Thorn mites** (smaller than him, 26 cm): a bark-shelled body bristling with thorns, pincers, ember eyes, six legs walking in tripods. Roam about home; notice him at 3.8 m; scuttle straight at him; crouch and hiss (the tell, .38 s), lunge and bite (a Sanity point), back off. One scratch kills one. Three in the orchard after the pollen, three each by two of the Deepwood's paths.
- **Redcaps** (gnome-sized, about 1.2 m with the cap): on the temple skeleton's rig and motion-capture clips (`Post.Kind` 3, scale .55), so they walk, strike, kick, swat and grab as the skeletons do. A long drooping blood-red cap, pointed ears, a long nose, a grey beard, yellow eyes, a grin of teeth, red tatters, iron-banded boots, a rusted sickle. Quick to notice (9.5 m), quicker than his walk, five scratches to put one down; their iron boots are heard before they are seen. Three stand round the camp's fire in Redcap Hollow (a fire, a tripod and cauldron, two lean-tos hung with red cloth). They keep to their camp; outside it they give up. Behind the camp a hollow among boulders has cigarettes in it: its only way in is under a low root (82 cm) he passes and they cannot.
- **The displacer beast** (Highmeadow): a great six-legged blue-black panther, 2.6 m of body, two tentacles from its shoulders ending in spiked pads; its legs reach for the ground (two tripods), its tentacles arch and ripple over it, green eyes. It prowls a round across the way to the maze and the overlook's spur. It sees him at 26 m in the open, stops and roars, runs him down (faster than he runs) and lashes with its tentacles (two Sanity points and a knock back). Four hollow logs beside the road and the spur are refuges it cannot enter: it paces outside, growling, and gives up after 8 s. As in D&D it is never quite where it seems: its image is drawn about a metre to one side of where it truly is, and now and then slips across to the other. Thirty scratches drive it off for good (it is meant to be avoided). Pollen lies across the road to the stair.
- **The flower that raises pads** (Lily Mere): on the land inside the deep water, a reactive flower whose scratch raises two lily pads out of the water to an islet with cigarettes; scratched again they sink.
- **Not spawned** while the route walker runs (the creatures need his reactions); its frame times are without them.
- **Checked:** `-ChuckFeyMapTest=life` now also: a mite bites, one scratch kills one, the pads rise, he crosses onto the islet and takes its cigarettes, a redcap sees him and comes, none gets into the hollow, five scratches put one down, the beast notices him, lashes him in the open, cannot reach him in a log, paces and gives up. With stills of each creature (`Life_MiteClose`, `Life_RedcapClose`, `Life_Camp`, `Life_BeastSide`, `Life_BeastFront`).

The Deepwood's lanterns (2026-11-22, the user: the blue lamps of `deepwood-lantern-path`, putting out deep blue light like the mushrooms, with less light from the sky and the floor): crooked wooden posts along its paths, each with a hooked arm and a hanging iron lantern of blue glass and a deep blue light that scatters in the fog (up to 80); the floor lights fewer and fainter; its glowing caps' lights a deeper blue; the sky and fill light in the Deepwood dimmed.

Overcast and sounds (2026-11-22, the user: the Deepwood overcast, free resources for a better displacer beast, sounds for everything that needs them):

- **The Deepwood overcast.** `M_FeySky` gains an `Overcast` parameter: a low deck of slate-blue cloud, billowing and drifting, with no sun disc; it follows the Deepwood's gloom (so it comes over through its thresholds). Under it the sun is down to 5 %, the sky light at 56 % and the exposure lifted a little so the ground still reads; the lanterns and fungi carry the colour.
- **The displacer beast's fur.** ambientCG Fabric084 (CC0, `Tools/Fetch-FeyFur.ps1`) on a new `M_FeyFur`: the fur projected from three sides in each part's own space, darkened to blue-black, over the parts' own colours (the green eyes glow, the fangs and spikes keep theirs). When its image slips from one side of where it is to the other (now over about half a second, passing over its true place), the fur dithers out in flickering patches with a blue sparkle (`Shimmer`).
- **Sounds** (BigSoundBank CC0 recordings, `Tools/Fetch-FeySounds.ps1`, cut and levelled by `Tools/build_fey_sfx.py`; credited in `SourceAssets/Audio/SFX/Feywild/Source/manifest.json`; not auditioned by Claude): the beast's roar (cat roars slowed under a deep monster roar), growls, whip-crack lash and padded steps; the mites' chitter, click before a bite and twig-snap death; the redcaps' cackle (wild laughter, slowed and roughened) as they notice him and before a blow; the camp's crackling fire and bubbling cauldron (loops); the moths' soft wingbeats (a loop each); the flowers' chime and rustle as they open (the chime reversed as they close); the vines' dragging leaves; the orchids' suck as they swell, cork-pop spit and the seeds' knock; the pads rising with a bubbling and a splash.

## Stages 6 to 8: the maze, the Cloud Staircase and polish (2026-11-23)

The user: "go ahead and do the next 3 stages". Built in `FeyMap` (the maze's shortcuts and the stair) and `AFeyLife` (everything in the maze that moves or reacts).

- **The heart's gate.** The heart is closed by a lattice of vines across its one opening; a hit plant beside it opens it (and closes it again, never over him).
- **Hit plants for the lost (the user's design).** Twelve hedge sections are chosen as shortcuts, where an opening saves the most walking along the way to the heart. After 4 minutes in the maze, and every 2.5 minutes after that, a hidden hit plant appears by the shortcut nearest him. Scratching it sinks that hedge into the ground; scratching it again raises it, but never over him. Plants once revealed stay; the hedges close again when he respawns.
- **The bees.** A swarm lives on the hedge tops: on arrival a warning buzz (`SFX_FeyBeeLoop_00`, three CC0 bumblebee recordings layered), then they knock him back down into the nearest corridor. No Sanity lost.
- **Statues** (hooded stone figures in dead ends) turn to face him while he is looking away.
- **Creatures in the maze:** thorn mites in two dead ends, two redcaps near the middle of the way, orchids down long straight runs, lantern moths near the heart.
- **The Cloud Staircase.** 56 steps of 34 cm wind up from the heart, from 11 m out to 22 m, over the hedges to a broad cloud landing 19 m up. A new respawn spot is there ("Feywild Cloud Landing"). Each tread is walked on a slab under it, not a column, so the maze's corridors under the stair stay open. Invisible rails keep him from stepping off the high part (from step 30). An invisible wall at the landing's far end is the boundary until the tower is planned; the stair beyond it is still scenery.
- **Polish.** The 55 ms stutter on reaching the maze was its hedges: eight meshes of about 43,000 triangles, each taking about 45 ms to cook its collision in one frame. They are now 48 parts (eight sectors by six bands), none over about 9 ms (the maze's worst build step 48 ms before, 9.2 ms after). A one-frame spike of about 47 ms at some threshold remains, outside the build steps (not yet found). Each district's plants and creatures are also built a part at a time over several frames (within about 4 ms a frame). Any build part slower than 8 ms is logged (`CHUCK_FEY_SLOW_PART`). The thorn mites are lighter (they read near black).
- **Checked** (route walk: 0 failures, 0 stuck, every district at the 60 fps cap except the Deepwood at 17.9 ms; `-ChuckFeyMapTest=life` with 0 failures, steps 50 to 57 new): plants appear while he is lost; a revealed plant opens its shortcut and he walks through; the heart's vines stop him, its plant opens them and he walks in; on a hedge top the bees knock him down to the ground; a statue turns; he stands on the landing, the boundary holds him on it, and he climbs the stair from its foot. Stills: `Life_MazeShortcut`, `Life_MazeHeartGate`, `Life_MazeBees`, `Life_CloudLanding`.
- **Known flaws:** the bees were gold specks (modelled the same day, below); the statues' turn is a snap made while unseen; the parkour and thorn hazards were not built here (built the same day, below); the stair's treads are long (about 4.3 m each), so the climb is a long walk; nothing here has been played by hand.

## The maze's parkour and thorns (2026-11-23)

The user: "Do the parkour and thorn hazards". Built in `FeyMap` (where they go, the dressing, his grips, ladders and crawl spaces) and `AFeyLife` (the thorns' effect), with his existing moves: the side wall run, brachiation, the ladder climb and the crawl.

- **Thorns.** Beds of arching bramble canes across the whole corridor, thorned, with a few leaves and dark berries. On the ground among them he walks at 45 % speed and loses a Sanity point at most every 1.6 s, with a stumble back the way he came. Over them (on a trellis, a hoop, the walkway, in a jump) nothing happens. `FeyMap::InMazeThorns`, `AFeyLife::ThornPricks`.
- **Over each bed, a way across:**
  - **Trellis.** A wooden lattice with ivy along one hedge, 2.6 m either side of a 1.8 m bed, with a flat face for the run. Jump while running beside it and he wall-runs past the thorns.
  - **Ivy hoops.** Five bent willow hoops across the corridor, 1.35 m apart, hung with ivy, over a 4.5 m bed. Their crowns (98 cm up) are swing grips: a standing hop catches the first, then he swings hoop to hoop and off the end.
  - **Raised walkway.** A plank walkway 1.6 m up along the outer hedge over a 5.8 m bed, with a ladder at its start (walk into it to climb) and four steps down at its end.
  - **Short beds** (1.2 m) are jumped.
- **Where.** The way through winds, with few straight runs of three cells on one ring. So the way itself has two short beds to jump. The trellises, hoops and walkways (two of each) stand on straight runs off it, longest first. Where one end of such a run is a dead end, three cigarettes wait there past the thorns. None is placed by the orchids' lanes, the redcaps at the middle of the way, the gate, the heart or a shortcut.
- **Root gaps.** Six low holes, 46 cm high, at the foot of hedges between cells that are not linked, each saving 4 to 13 cells. Each is roofed with a slab of root and framed by arching roots. Walking into one, he goes down on all fours and crawls through (a registered crawl space); the redcaps cannot follow.
- **Checked** (`-ChuckFeyMapTest=life` from step 50, 0 failures, `Local/feypk-2.log`; steps 58 to 62 new):
  - The short bed slows him (51 cm/s across it) and pricks him.
  - Run beside the trellis and jump: he wall-runs and lands past the thorns with no prick.
  - He catches the first hoop, swings along all five and lands past the thorns with no prick.
  - He climbs the ladder, walks the walkway 1.6 m up and comes down its steps with no prick.
  - He crawls through a root gap.

  Stills: `Life_MazeThorns`, `Life_MazeTrellisRun`, `Life_MazeHoops`, `Life_MazeWalkway`, `Life_MazeRootGap`.
- **Second round** (the same day; the user: "proceed" on the flaws above):
  - **On the way through.** One two-cell straight run of the way itself gets three ivy hoops over a 1.8 m bed, placed only where the way comes in before the first hoop and goes out after the last. The other such run keeps its short bed to jump. Checked (`=life` step 63): he catches all three and lands past the thorns with no prick. Still `Life_MazeWayHoops`.
  - **The walkway** is weathered timber (`M_FeyPlank`: the bark's shader on the `T_rough_wood` set, mossed only in a few patches, not on every face that looks up). Its steps are boards (treads and risers on two stringers, open beneath), with each step's block kept as collision only.
  - **The bees** are modelled: a dark head, a furred gold thorax, a banded abdomen and two pale wings beating, each flying along its circle. Still `Life_MazeBees`.
  - Checked: `=life` from step 50, 0 failures (`Local/feypk-4.log`).
- **Known flaws:**
  - The brambles are thin canes, with the thorns small at a distance.
  - The ivy leaves are simple cards.
  - The trellis hides part of the view when the camera is beside it.
  - Only one piece (the three hoops) is on the way through; the trellises and walkways stay off it.
  - The walkway's grain follows the world, not each board.
  - The bees are drawn in fresh geometry every frame they show (up to 56).
  - Nothing has been played by hand.

## The start's narrow cut, and the maze's third round (2026-11-23)

The user: the gaps through the rock and the floating petals in the first canyon after the start; hazards and unusual things only on the way he must take through the maze (follow them and you get through), with frowning statues in the dead ends and smiling ones on the way, real satyrs rather than cartoon heads, some detailed marble satyrs, the Deepwood's blue lamps (dimmer), low fog in parts, more decor, and spots narrow enough to wall-jump to the hedge tops (and be knocked off by the bees). The Cloud Staircase needs no more testing: it will become an escalator cutscene.

- **The narrow cut behind the arrival** (not a threshold: the path pressed 6 m into the rise west of the pond, in the look slice and on into the Riverbank).
  - The "gaps" were long dark-blue streaks down its walls. The path's wet sheen (vertex colour) ran up the steep wall triangles and mirrored the sky. Dampness and path colour are now off on steep faces, in the slice and the districts.
  - Plants and petals were placed on and beside the walls, where the true ground and the mesh part most. Nothing is now planted on ground steeper than 45 degrees, or within 80 cm of it. So the petals lie on the floor.
  - The Riverbank's land was 123 cm cells, too coarse for the cut's walls (they smeared, and its grid met the slice's at a seam). It is 41 cm there now, and the slice's land runs a metre under it.
  - Stills: `Local/shots/feywild-startcut/` (`Tools/shots/feywild-startcut.txt`).
- **The maze is laid out from its way.** The way through is laid first: straight runs along every other ring (2 to 4 cells), then a turn inward. The rest grows from it with no loops, so it stays the only way through (458 m, was 195 m).
  - **On the way only:** all the parkour over thorns (on each run, sized to the run between the opening it comes in by and the one it leaves by: 3 trellises, 3 five-hoop runs, 2 walkways, 3 three-hoop runs, 2 short beds) and three root gaps you must crawl through (the redcaps cannot follow, and none is near them).
  - Also on the way only: the smiling satyrs, the statues that turn, the thorn mites, cigarettes, and a squeeze.
  - The hit plants' shortcuts all come out on the way.
- **Statues.**
  - Along the way: marble satyrs on plinths at every smiling mark (17), SMK's public-domain scan of the Medici Faun, laughing as carved. Two more flank the heart's arch.
  - In about twenty dead ends: the same faun re-carved to a scowl (`Tools/build_fey_satyr.py`: mouth corners down, lips pressed, brows drawn down and together). Each is set back from the dead end's way in and faces it.
  - Sources and licences: `Tools/Fetch-FeySatyr.ps1`. The scan is under the Public Domain Mark; the marble is Poly Haven marble_01, CC0.
  - `Tools/import_fey_satyr.py` makes `SM_FeySatyr`, `SM_FeySatyrFrown` and `M_FeyMarble`. The material is projected from three sides (the scan has no UVs), with a little grime and moss low down.
- **Squeezes.** On the way, the hedges bulge in to leave a slot 80 cm wide and 3.6 m long for the hedges' full height, with flat faces to run on. He chimney-jumps to the top (8.5 m in 17 kicks in the test), and the bees knock him down.
- **Decor, spread evenly over the whole maze:**
  - 285 of the Deepwood's blue lanterns on crooked posts at the hedges' feet. Every fourth is lit, 20 cd within 9.5 m (the Deepwood's are 70 cd), 64 lights at most.
  - Ferns, glowing caps, foxgloves and celandine at the hedges' feet.
  - Stone urns of flowers, stone benches, and fallen petals.
  - 39 patches of low fog: veils lying 20 to 40 cm over the floor, shown when near.
- **Checked:** `=life` from step 50 in the editor, 0 failures (`Local/feymaze-18.log`; marks 17, gaps 3, shortcuts 3, squeezes 1). Route walk 0 failures, 0 stuck, worst threshold frame 53.1 ms, maze build parts at most 9.2 ms (`Local/feymaze-route4.log`). Package verified 50/50, 182 passes (`Local/verify-package-20261123-195342.log`), promoted as FeywildMaze3. New steps: 63 (three hoops on the way), 64 (the squeeze climbed and the bees knock), 65 (stills).
  - The thorns, trellis, hoops, walkway and root gap are now all on the way, and all pass.
  - Stills: `Life_MazeSmile`, `Life_MazeSatyr`, `Life_MazeFrown`, `Life_MazeLantern`, `Life_MazeFog`, `Life_MazeSqueeze`, with review renders of both faces in `SourceAssets/Props/Feywild/Satyr/`.
- **Known flaws:**
  - The scowl has small creases at the mouth's corners. From his height (the faun's head is 1.8 m up, looking down) smile and frown read mostly by the mouth's line, at a few metres.
  - The satyr is one pose, repeated.
  - Under the maze's blue lanterns and shade the marble reads dark blue-grey, not white.
  - Only one squeeze is built (the way's runs leave room for one).
  - The way is more than twice as long as before.
  - The route walker needed straight points through each crawl hole (a player lines up by eye).
  - Nothing has been played by hand.

## Aiming orchids, the wonder pass, the hag's cottage and the Crossroads trio (2026-11-23)

The user, with an image (`References/Feywild/crossroads-trio.png`): make the plants that shoot work like the spined devils
and target Chuck; do the wonder pass (the giant flying jellyfish, the unique giant plants and the rest); build a hag's
cottage in the Deepwood just off the trail, so he passes close to it; and make the image's three (a centaur carrying a
satyr, a gnome on the satyr's shoulders) and have them mill around by the Crossroads' giant tree. Dialogue comes later,
from the user. Free resources could be downloaded.

- **Orchids aim** (`FeyLife.cpp`), as the spined devils do.
  - Each sleeps facing its lane. It wakes when he is in front of it (up to about 100 degrees either side), in sight, and
    within 1.25 times its lane's reach.
  - Awake, its bulb turns to follow him. As it swells it takes aim at where he is (no lead) and spits there. Half its
    spits go 30 to 95 cm wide.
  - It does not spit at nothing: it stops when he goes.
  - `=life` step 12: it aims off its lane and turns to him (`Life_OrchidAim`), then hits him in its lane.
- **The wonder pass** (`Tools/build_feywild_kit.py`, `FeyMap.cpp`). The districts' pieces are on their own random stream,
  so the rest of the planting is unchanged.
  - Riverbank and Highmeadow: great bell flowers, up to 9 m, arching from one clump with lit amber bells.
  - Orchard and Needle Garden: three flower spires, the tallest 5.4 m.
  - Crossroads' edges and the redcaps' hollow: fly agarics with white warts, 3.3 m tall.
  - Deepwood: stands of tall glowing blue caps with shelves.
  - Lily Mere: a giant lily on a great pad on each of the two great pools.
  - Highmeadow: the great wisteria, an oak hung all over with wisteria, replacing the plain stand-in.
  - Oaks hung with purple moss by the mere's fen and at the Crossroads' edges (`M_FeyHangMossPurple`).
  - Rounded archway ruins of grey dressed stone (`M_FeyArchStone`: Poly Haven cliff_side, greyed), with ivy and hanging
    moss, mossed on top. They stand in the open districts; two whole arches span the orchard's road, walked under.
  - Floating islands: rock cones with grassy tops, trees (one a wisteria), trailing roots and thin falls. There are seven,
    over Highmeadow, the overlook, the mere and the river, bobbing and turning very slowly.
  - The giant jellyfish (`M_FeyJelly`): 24 m across its bell, glowing violet and pink with cyan canals, see-through only at
    its edge. It drifts round a nine-minute loop high over Highmeadow, the Crossroads and the mere, pulsing, tentacles
    waving (in the material), with one soft light.
  - Gentle motion: the kit's plants sway a few centimetres, more the higher a part stands; the fungi breathe. The map's
    and life's own surfaces stay still.
  - Mirror water at the lily-pad crossing: one cube capture from the great pool's middle when the mere stands, which its
    water mirrors (`M_FeyWater` now takes Probe and Reach).
  - Not built from the plan: the crows.
- **A fix found on the way.** The materials projected from three sides (bark, roots, timber, stone, the marble satyrs)
  took the normal in the whole component's frame but the position in each instance's own. Every turned instance (every
  oak, every satyr) was projected askew. They now use the instance's frame. The arches showed it worst.
- **The hag's cottage** (`hag_cottage` in `Tools/build_feywild_kit.py`; `FeyMap.cpp`, `FeyLife.cpp`).
  - Where: just off the trail from the Crossroads into the Deepwood, on its outer side, its door facing the trail across
    a small fenced garden.
  - Ground and garden: its ground is levelled, and planting is kept off the house and garden.
  - The house: a crooked timber frame with daub walls (Poly Haven worn_mossy_plasterwall, CC0) on a footing of mossy
    stones, under a deep sagging thatch (Poly Haven thatch_roof_angled, CC0; `Tools/Fetch-FeyHag.ps1`) with a bound ridge.
  - Chimney and door: a leaning stone chimney on one gable, and an arched plank door with iron straps and a ring.
  - The front: leaded windows glowing amber, a stag's skull and antlers over the door, herbs and dried flowers hung from
    the eave, a lantern, a broom, a barrel and bucket, and jars on the sill.
  - The garden: pumpkins, a stick fence, and an iron cauldron of glowing green brew on three stones over a fire.
  - Warm window and lantern lights; the cauldron's fire light, its crackle and bubbling, and smoke from the chimney
    (`AChuckFlames`); collision on the house, chimney and cauldron.
- **The Crossroads trio** (`Tools/build_fey_trio.py`, `Tools/build_fey_trio_anims.py`, `Tools/import_fey_trio.py`,
  `FeyTrio.cpp`).
  - How they are built: on the human kit (MPFB 2.0.17), with MakeHuman's expression units baked into the faces, strand
    hair and beards, and the kit's clothes. The temple heroes' surfaces are used for their gear.
  - The centaur: helmet and bracers, a bay horse's body from the hips back (signed distance fields), and a hair tail and
    feathering. He wears a purple velvet blanket with a gilt border, a girth strap and saddlebags.
  - The satyr: laughing. He has goat's legs on bones moved to a goat's (furred, with hooves), ram's horns and a chest
    strap.
  - The gnome: scowling and smirking, in a tall green felt hat, leather jerkin, breeches and boots.
  - One 60-second loop for all three, authored together. The centaur stands, walks half round a circle 4 m across,
    stands, walks back and stands. He walks a four-beat walk with planted hooves (IK), his body bobbing and his tail
    switching.
  - The satyr rides with his hands on the centaur's shoulders, laughing in bursts and looking about. The gnome sits on
    the satyr's shoulders holding a horn and points: ahead while they walk, to the sides while they stand.
  - In the game: three meshes at one place, played from one clock, on the Crossroads' ring road north-west of the tree.
    A collision box follows the centaur's body (not while the route walker runs).
- **Checked:** see `docs/agent-handoffs/CLAUDE.md` (November 24).
- **Known flaws:**
  - Animals' and riders' contact is by IK. The satyr's knees and the gnome's legs bend oddly from some angles.
  - The horse is a smooth, stylised body: no muscle detail beyond its masses, its coat a suede.
  - The flames and smoke of the cauldron and chimney did not show in the stills (the camp's do not either; not checked
    in play).
  - The great bell flowers have no collision.
  - The islands' and the jellyfish's look from the far districts has not been judged.

## The Crossroads trio, third round (2026-11-24)

The user: the satyr's arms should not pass through the centaur's clothes or body; the centaur's helmet more like
Achilles' in Troy (2004), without feathers, in leather; the horse's proportions more accurate (it felt too long), the
front legs without their narrow part; travellers' gear behind the satyr, so that the way they ride makes sense.

- **The horse** (`Tools/build_fey_trio.py`):
  - Shortened: everything behind the man's hips is drawn in along its length by 0.76 (`XS`): the trunk, the hind thighs,
    the hind legs, the tail. From the point of shoulder to the point of buttock it is now about its height at the withers
    (it was 1.3 times).
  - The chest is a little narrower.
  - Each foreleg is one loft down its height (`FORELEG`). The forearm is deep and muscled at the top, with the elbow's
    point behind, and tapers evenly to a knee a little wider than the forearm above it, then the flat cannon. Before, it
    was a tube with lumps on it, pinched above the knee. Black from just below the knee.
  - The tail's root sits lower, with no nub over the croup.
- **The helmet** (`troy_helmet`, made in the kit's hook so the strand hair is pressed under it; the kit's steel cap and
  nasal are gone from `trio.json`):
  - A Corinthian shape in moulded tan leather. The open T of the face, the brow arched over each eye down to a short
    nasal, long cheek guards to the jaw, the ears covered, a neck guard flaring over the nape.
  - A low keel ridge from brow to nape where the crest would stand (no plume). A rolled darker edge. Bronze studs along
    the brow and down the cheek guards.
  - Fitted by casting out from the middle of the head in every direction (past the ears), the radii smoothed and kept
    clear of the skin.
  - Without the kit's helmet the man measures 2.7 cm taller to his crown, so the lift is now 42.6 cm (`FeyTrio.h`).
- **The gear** (`traveller_gear`), lashed behind the satyr's seat and riding the croup:
  - A rolled red wool bedroll across the loin with two straps.
  - A canvas pack on the croup with a leather flap, its straps and a coil of rope on top.
  - Saddlebags on both flanks, with flaps and buckles.
  - A pot hanging off the left bag and a lit lantern off the right. A waterskin.
  - The satyr's shepherd's crook lashed on top of the load, rising behind.
- **The satyr's hands** (`Tools/build_fey_trio_anims.py`):
  - Flat on the centaur's shoulder caps. The wrist sits just behind the top of each shoulder and the knuckles on it, both
    found on the centaur's skin and vest.
  - The hand's roll is the one that lays the palm down with the curled fingers over the front of the shoulder.
  - They are carried by the centaur's upper chest, not his collarbone, which rolls when he gestures.
  - Before, the hand hung as it was, with its fingers in the vest; on the first try the probe hit his jaw and the hands
    held his face.
- **Then, from the user's stills** ("the front horse legs are super narrow at the top"; the satyr's feet should read as
  goat hooves, not too glossy; his legs and hips should look normal for a goat-legged satyr riding a centaur):
  - **The forelegs' pinch was the walk's IK, not the mesh.** Each leg's IK pole angle was a fixed -90°, which for the
    forelegs is half a turn from their rest, so the forearm and cannon stood twisted about their length against the
    shoulder: an hourglass under the chest in every frame. Each leg's pole angle is now fitted so that with its targets
    at rest the leg is at rest (`CHUCK_TRIO_LEG_POLE`: front 90°, hind -80° and -100°, twist under 1.5°).
  - The chest also narrows toward the breastbone between the forelegs. The forearms' tops are broader, flush with the
    point of shoulder. The round triceps lumps that stood out over them are gone, and the breast muscles are smaller.
  - **The satyr's hooves** are cloven: two toes with a cleft, a heel bulb, flat soles. They are a matte dark horn
    (`goat_hoof`, suede-based) instead of the horse's glossy hoof.
  - **His hips** are furred round the front up to the sash; a band of the man's skin had shown between the waist and the
    goat. The thighs are slimmer.
  - **His fur** lies along each leg and follows the bone. The cards had hung straight down from the standing pose and
    stood out from his seated thighs like a skirt. They are shorter and fewer.
  - **His legs as he rides:** the thigh forward and down over the barrel, the shank back from the knee, the long cannon
    down, the hoof hanging under the knee. Before, the hoof reached forward to the horse's elbow.
- **Checked:** see `docs/agent-handoffs/CLAUDE.md` (November 24, third entry).
- **Known flaws:**
  - `CHUCK_TRIO_CLEAR`:
    - The satyr's fingertips go up to 1.7 cm into the centaur's shoulders as he sways (about 90 of 4246 arm and hand
      verts).
    - His legs: 10 verts up to 2.2 cm into the horse (it was 358, up to 4.3 cm).
    - The gnome in the satyr: as in the second round.
  - The chest from the front is still a smooth rounded mass.

## The Crossroads trio, second round (2026-11-24)

The user: the gnome's legs over the satyr's shoulders in a normal piggyback; the satyr's legs round the horse's body
without cutting into it; a proper horse's body, not drawn shapes; clothes fitting a whimsical satyr; a vest on the
centaur. No free full-body horse with a usable licence was found (OpenGameArt refuses automated fetches; Sketchfab's are
CC-BY behind an account; SMK's scans are a rider and heads), so the horse was sculpted again in code.

- **The horse** (`centaur()` in `Tools/build_fey_trio.py`), built as a horse is:
  - The trunk is a loft over a 1.5 m riding horse's lines: top line (withers, a slight dip of the back, loin, croup,
    tail root), under line (breast, sternum, girth, belly rising to the stifle) and widths (`TRUNK`), its section an egg.
  - Each hind thigh is lofted down its height (`THIGH`): the broad, flat-sided wedge from the croup past the stifle to
    the gaskin and hock.
  - Over them: shoulder blade, point of shoulder, triceps, the two breast muscles parted by a groove, and a forearm
    muscled in front. The joints are a horse's: elbow, flat-fronted knee, the hock and its point, fetlock and ergot.
    Slanted hooves stand flat on the floor.
  - Colour: a bay coat with a little sheen and black points below the knees and hocks; the tail and feathering are
    black.
  - Weights blend over about 10 cm between bands, so the swinging hind thigh does not crease.
  - The man's belly runs into the horse up to his belt. The lift is now 45.3 cm (`FeyTrio.h`).
- **Clothes:**
  - A sleeveless `vest` piece added to the kit (`Tools/build_npc_humans.py`), and the heroes' surfaces (velvet, suede,
    leather) usable as outfit fabrics.
  - The centaur wears a brown leather vest with a rolled bronze edge.
  - The satyr wears a cropped emerald velvet waistcoat with a gilt edge, a red neckerchief and a saffron sash, with a
    wreath of ivy and small flowers round his brow and panpipes tucked into the sash. His chest strap went.
  - His horns are now low ram's curls round his ears, as in the image. The tall ones speared the gnome.
- **Riding** (`Tools/build_fey_trio_anims.py`):
  - **The satyr:**
    - Each leg joint is found by searching directions that clear the horse's coat: the thigh over the blanket's curve
      (pressing in no more than a rider's thigh does), the shank close down the flank, then cannon and hoof. IK and
      tracking put the bones there (knee within 1.6 cm).
    - He sits 2 cm higher.
  - **The gnome's piggyback:**
    - He sits on the satyr's shoulders with his crotch against the back of the neck, measured on the satyr's seated
      skin.
    - His thighs go forward over the shoulders and his shins hang down the satyr's chest. His left hand is on the left
      horn; his right arm points.
  - **A bug fixed:** the gnome's targets were parented to the satyr's bones while the satyr still stood at the origin,
    so they rose a second time when he was seated. That is why the gnome perched high.
- **Checked:** see `docs/agent-handoffs/CLAUDE.md` (November 24, second entry).
- **Known flaws:**
  - The posed clearance check (`CHUCK_TRIO_CLEAR`) still finds the satyr's thigh up to 5 cm into the horse's coat at
    the seat (a pressed thigh). The gnome's legs go up to 8 cm into the satyr's skin where the satyr's posed arms and
    shoulders rise.
  - The satyr's fur legs read dark in the evening light.
  - The bay's points end in a hard line at the knees and hocks.

## The wonder pass (planned 2026-11-22)

The user's request: more immersive and visually stunning elements distinct to each district, and more movement for whimsy. Built 2026-11-23 (above, except the crows), each piece judged in stills.

| Element | Where | How (to keep it cheap) |
|---|---|---|
| **Distant floating islands** (as in `open-vista` and `maze-tower-overlook`), some with thin falls and trailing roots | Over Highmeadow and the overlook, glimpsed from the Crossroads and the Lily Mere | Low-detail always-present meshes on the painted distance's material, like the tower's cloud: no collision, a handful of them; a slow bob and drift in the material |
| **A giant flower landmark per flower-heavy district** | Riverbank and Blooming Path, Orchard and Needle Garden, Lily Mere (giant lilies), Highmeadow (the great wisteria) | A few hero models per district (Blender kit, with levels of detail), loaded only with their district; glow from the material |
| **A giant mushroom landmark per mushroom-heavy district** | Deepwood, Redcap Hollow (red toadstools), the Crossroads' edges | As above; some house-sized with shelves he may climb, on purpose, or kept clear of the routes |
| **Gentle motion** | Everywhere | In the material, so the graphics card does it: flowers that slowly open and close, mushroom caps that bob and breathe, swaying fronds, drifting petals. Switched off at a distance (as now). Kept small on anything he can stand on, since collision does not move with it |
| **Crows** | The Deepwood | A small group (about 6 to 12) after Chult's birds: mostly perched and watching, the odd one taking off with a call. Not dozens |
| **Mirror water at the lily-pad crossing** (the user, 2026-11-22) | The Lily Mere's deep water crossed on the pads, and its pools | As the look slice's pond (`AFeywildSetting::BuildProbe`, `M_FeyWater`): a cube capture taken once from the water's middle when the district stands, the water reflecting it carried out to about the trees' distance. One capture per district, not per frame |
| **Purple hanging moss outside the Deepwood** (the user: a purple version of the Deepwood's) | On some old oaks in parts of the other districts: by the Lily Mere's fen, the Crossroads' edges, the riverbank's oldest trees | A variant of the ordinary oak (Blender kit) hung with the Deepwood's strand moss coloured purple (a colour parameter on `M_FeyHangMoss`, so the Deepwood's stays grey-green), mixed in with the plain oaks rather than replacing them |
| **Rounded archway ruins** (the user: separate from and in addition to the present rune arches) | Open areas, as in the goal images: Highmeadow, the Orchard's edges, the Lily Mere's shores | Round-topped stone arches, some broken or half fallen, mossed on top (`M_FeyStone`), with partial ivy hanging from them (strand and leaf cards like the vines); static, a few per district, clear of the routes or walked under on purpose |
| **A flying giant jellyfish** (purple allowed) | High over one open area, Highmeadow or the Lily Mere | One, high and slow, its bell mostly solid and glowing with only its edge see-through, its tentacles waving in the material; one soft light at most |

**Budgets and risks** (measured 2026-11-22: every district at the 60 fps cap except the Deepwood at about 17.9 ms, one district loaded at a time):

- **Cheap:** islands, landmarks and material-driven motion, as above. Material motion costs a little more where its object casts a shadow.
- **Watch:** lights (the Deepwood already has about 160; unshadowed lights near Chuck bleach his fur, so the light audit runs after adding any); shadows from giant pieces (a choice per piece, not the default); animated creatures in numbers.
- **Risky:** large see-through surfaces filling the screen (the jellyfish close to the camera, more mist or light shafts in the Deepwood, which is the slowest district); new textures (memory matters more than drawing speed on this machine: reuse the kit's leaf, petal and bark materials and colour by vertex); building a district's extras in one frame (this round's flowers and creatures already cause a 55 ms stutter when a district finishes loading, so extras are spread over several frames).
- **Tone and play:** motion stays slow and quiet (the brief asks for restrained animation); hero pieces either climbable on purpose or clear of the paths and the camera.

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

Made by the user on 2026-11-22:

8. **A wonder pass** of district landmarks and gentle motion is planned: see [The wonder pass](#the-wonder-pass-planned-2026-11-22).
9. **Purple is allowed on things,** only not as the sky's colour or the light over everything; the jellyfish may be purple.

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
