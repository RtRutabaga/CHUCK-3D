# Phlegethos (plan and build, Phase 8)

Written 2026-11-14 (Claude) on the user's instruction: "build the fall to plethegos cutscene (using fall to chult soundtrack but a bit more of the song creating a slightly longer cut scene) and build plethegos. don't do the trio of heros pit fiend encounter yet. for plethegos, this should look really cool and it's challenges should draw on Chuck's parkour in ways that are fun to play. download and implement any free resources you think would help". The user also supplied two tracks: `Phlegethos.wav` and `Phlegethos_2.wav` ("for when things get more intense in this part").

The user asked for the build directly, so this document records the plan and what was built, not a proposal awaiting approval. Anything here can be redirected.

Direction: the 2D game's `docs/development/PHASE-8.md` and `hell_falling_cutscene_scene.py` (read-only).

## What the 2D game has, and what this milestone takes from it

| 2D (Phase 8) | 3D, this milestone |
| --- | --- |
| The fall into the Nine Hells: open heated air, a distant volcano climbing into view, a basalt plain with sparse lava fissures, the quiet death, the look round, the cigarette | **Built**: the fall cutscene, on the fall-to-Chult score cut one phrase later (about 15 s longer) |
| Six connected maps: arrival, lava road, lava lake islands, rubble pass with a lava fall, the fractured way, the fortress approach | **Built** as one continuous stage in six stretches, with a checkpoint each |
| The lava lake island crossing ("Include a sequence where Chuck jumps across small islands over a lava lake") | **Built**: a basalt column causeway, some columns sinking under him |
| Lava falls, lava rivers, narrow basalt paths, jump challenges | **Built** as parkour: a narrowing basalt spine, a lava-fall gorge crossed along its wall and on hanging chains, a wall-jump chimney, crumbling steps (the lava geysers and bombs were removed on 2026-11-16) |
| Lemures, fire snakes, spined devils, flameskulls, horned devils | **All built** (2026-11-16), with the Pit Fiend as the colossus; see Creatures below. **No horned devils** (the user, 2026-11-16: "in 2d we had both horned devils and pit fiend but i dont want the separate horned devils in our game") |
| Infernal idols, fortress exterior | **Built**: the processional and the shut gate |
| The trio battling the Pit Fiend, Astral escalation, the Feywild river, the river cutscene | **Not built** (user: "don't do the trio of heros pit fiend encounter yet"). The fortress yard ends at the shut gate |
| Breakable urns with cigarettes | **Built**: clay jars on the safe ground off the routes |

## Chuck

Lava is a fall hazard exactly like the Astral Sea: falling in is an Astral death, and he is summoned back at the start of the stretch he fell from. A flameskull's touch and a fire snake's bite cost Sanity, as a zombie's bite does. Nothing here asks for a new move: every challenge uses one he already has.

## The stage

One continuous stage, built in code on its own site (`PhlegethosSetting.h`), 8 km beyond the ship's. Frame: X along the route (north, toward the fortress), Y across, Z up. Lava lies at Z 0. The walkable basalt stands about 3 m above it, so every fall is a long one.

| Stretch | Checkpoint | Traversal |
| --- | --- | --- |
| 0. The arrival plain | Phlegethos Arrival | The fall's landing spot. Broad cracked basalt with glowing fissures; boulders to vault and mantle; the worn path north. Jars. The volcano, the lava rivers and the fortress on its mesa are in view |
| 1. The lava road | Phlegethos Road | The road narrows to a basalt spine over the lava: running jumps over breaks, then a sprint-leap gap with a straight run-up. Flameskulls weave across the spine (the geysers until 2026-11-16) |
| 2. The column lake | Phlegethos Lake | Hexagonal basalt columns standing out of a lava lake at different heights: hops, mantles, ledge pull-ups. Cracked columns sink when he stands on them. Two long basalt slabs either side of a wide lava gap: a chained side wall run carries him across |
| 3. The lava-fall gorge | Phlegethos Gorge | A shelf along the gorge's basalt wall, with lava falls pouring past. A break in the shelf is crossed by a side wall run. Then iron chains hanging from a broken infernal arch over the lava river: brachiation |
| 4. The ascent (Phlegethos_2) | Phlegethos Ascent | The volcano erupts. A wall-jump chimney up the mesa's face; crumbling basalt steps over the lava that fall away behind him; flameskulls weaving across the ramp (the lava bombs until 2026-11-16); a last sprint leap onto the approach |
| 5. The fortress approach | Phlegethos Fortress | A processional of infernal idols with lit eyes to the iron-black fortress wall and its shut gate. The end of this milestone |

## Music

- The fall: `SW_FallToChult`, bars 0 to 33 (to 62.6 s), fading over the last two bars (was 47.5 s for Chult).
- Stretches 0 to 3: `SW_Phlegethos`, looping.
- From the ascent's foot to the fortress: `SW_Phlegethos2`, crossfading in, and back out if he returns.

## Free resources

CC0 textures, fetched and hash-checked by `Tools/Fetch-Phlegethos.ps1` into `SourceAssets/Setting/Phlegethos`:

- Poly Haven `dark_rock` and `dark_rock_02` (basalt), `volcanic_rock_tiles` (the worn path and the fortress paving). 2K diffuse, normal, ARM.
- ambientCG `Lava004` (the first molten surface; still imported, no longer used by the lava).
- ambientCG `Lava001` (the lake: dark crust torn by molten streaks) and `Lava005` (the flows' streaming molten lines), 2K colour, emission and normal (2026-11-16).

The fire and smoke flipbooks already in the project (Unity Labs, CC0) are reused for the flameskulls' fire and the falls' smoke. The flameskull is the temple's skull (CDmir's CC0 "human skull 0"); the fire snake is the temple snake (Tools/build_temple_snake.py) with a burning skin.

## Out of scope for this milestone

The trio and the Pit Fiend's battle, the Astral escalation and the Feywild river, the river cutscene.

## What was built (2026-11-14)

Code: `PhlegethosSetting.h/.cpp` (the stage, its hazards, light and embers), `PhlegethosFall.h/.cpp` (the cutscene), `PhlegethosTest.cpp` (`-ChuckChultTest -ChuckPhlegethosTest[=<trial>]`, `-ChuckChultTest -ChuckHellFallTest`). Small changes: `ChuckCharacter` (`BeginHellFall`/`EndHellFall`, lava as an Astral death, respawn at the stretch's start), `ShipEndingScene.cpp` (its end leads into the fall, no longer the end card), `DockMenu.cpp` (six checkpoints in a seventh column, saveable), `DockGameMode.cpp` (the two scores), `Tools/Verify-Package.ps1` and `Tools/Verify-Menu.ps1`. Assets: `Tools/Fetch-Phlegethos.ps1` then `Tools/create_phlegethos_assets.py` (run with `-ExecutePythonScript`; `-run=pythonscript` crashes in the content browser when importing).

### The fall (about 67 s with the 5 s lead)

Song-time shots on the bars: tumbling out of the dark; small in a vast red sky (bar 0); his face (4); over his shoulder down at the burning lake through the haze (7); level with him, the volcano climbing into view as he drops below its height (9); down past him at the lake, the road and the columns (11); from the plain, up at him (13, the breakdown); beside him, the ground rushing up (15); the impact (16); flat on the basalt, still; the summons light (18); he looks left, right and back (20); the drag, two exhales (22); a crane up and back over the lake toward the fortress while the volcano erupts (24 to 31); the music fades over bars 31 to 33 and the picture goes to black at 62.6 s. Esc skips. Both end on the plain, playable, the region live.

A volumetric ash deck was tried and removed: its layer sat under him (placed by another stage's hidden atmosphere) and read as dunes from above.

### The challenges (as measured by the test)

| Trial | What the real character does |
| --- | --- |
| road_hops | runs the spine, jumping two breaks (80 and 95 cm) |
| road_leap | runs past the geysers (they knock him back along the spine, never across it) and sprint-leaps the 230 cm break |
| lake_hops | all 21 column-to-column hops, 32 to 65 cm gaps, rises up to 40 cm, one 110 cm rise taken by a wall run and pull-up; seven cracked columns sink 0.55 s after he lands |
| lake_slabs | a side wall run on one slab and a kick to the other across 4.4 m of lava |
| gorge_wall | a side wall run along the cliff over a 2.4 m break in the shelf |
| gorge_drop | a running jump down to the lower shelf |
| chains | off the shelf into the first ring, eight leaps along nine rings, the step down at the end |
| bombs | up the ramp under falling bombs (each marked as it comes; hits cost Sanity) |
| steps | across seven crumbling steps (cracking 0.45 s and falling 0.95 s after he lands) without stopping |
| chimney | wall jumps up a 1 m by 4 m slot to the mesa |
| leap | a sprint leap over the last 2.3 m gap |
| approach | (2026-11-16) along the wastes' winding path from the landing to the forecourt, 155 m, at a run, never more than 3 m off it (47 cm in the editor) |
| gate | the processional to the shut gate, where he stops |

Everything that sinks or crumbles comes back after a death, and when he is 15 m away.

### First look pass toward the user's goal art (2026-11-14)

The user's three goal images are in `References/Phlegethos/` (a smoky sky lit red from below, charcoal basalt plates with glowing cracks, jagged rock, ruined arches and pillars with rusted chains, lava rivers and falls, steam, an erupting volcano with a plume). Matched so far:

- A sky dome (`M_PhlegSky`): billowing smoke, black overhead, red at the horizon, orange toward the volcano. Thinner, darker fog; a dim sun so the light comes from the lava.
- The walkable basalt broken into plates with lava glowing in the cracks (Voronoi edges in `M_PhlegBasalt`, on the flat tops only). The crease glow reads the scan's AO, not its colour.
- Jagged rock everywhere, crags along the gorge's top, 34 spires on the skyline, a ridged volcano with a smoke plume that thickens when it erupts, steam off the lava beside the route.
- A ruined gate where the road leaves the plain (broken arch, hanging chains, a drape), a fallen carved block, four toppled pillars in the lava, one still chained.

The lake's tall column is eased: a wider, non-sinking run-up column before it and a 100 cm rise (it failed 2 of 9 runs, once packaged, at 110 cm).

### Second look pass and hell lightning (2026-11-15)

The user: "start making plethegos look closer to goal" and "add some hell lightning too, overall more immersive and cool".

- **The backdrop was the wrong size.** The ranges stood from 600 m out to 800 m high and the volcano 3.5 km off and 2.2 km high, so from Chuck's eye they walled in nearly everything under 30 degrees: the "sky" in every still was mostly mountainside, and the sky dome barely showed. Now the ranges rise from 3 km out to about 550 m with sharp crests, and the volcano stands 12 km off, 1.5 km high (about 7 degrees, as in the goal art), its crater lit and its flows wider. The fog is thinner (.0045) and stops short of the sky dome.
- **Sky:** dark smoke lit red from below, orange toward the volcano, black overhead, with a glowing haze at and under the horizon.
- **Exposure:** no longer lifts a dark view by more than a little (the lava and the sky burned out).
- **Lava:** a dark crust in drifting plates, thin seams molten in places only, deep red to orange; the falls dimmer and streaked; far off a darker glow with broad brighter patches.
- **Basalt:** darker charcoal with a sheen. Thin cracks glow along some runs between the plates (the ash lies in drifts, not over everything).
- **Bright crosses on the road** came from the crack pattern's sin hash breaking down along its cell lines. It is now a sin-free hash, and the rock scan's dark tile edges no longer glow on the tops.
- **Lava lights:** the lights that throw the lava's glow on Chuck fade out when they would sit under or on rock (they made lamp-like pools). The geyser lights moved above the road.
- **Broken cliff faces:** the gorge cliffs, the far ridge, the cleft, the slabs, the chimney walls and the mesa are faceted crags. Each sits over a plain hidden collision box and is only cut in, a few cm where Chuck wall-runs or wall-jumps, up to 1.5 m where he never touches.
- **Hell lightning:** bolts across the smoke sky 4.5 to 12 km away (to the land or cloud to cloud), and in the volcano's plume. Each has two to four return strokes, lights the clouds round it, flashes a red light across the land, and its thunder follows after a delay set by distance. A strike every 6 to 13 s, every 3.5 to 8 s from the ascent on, every 1 to 3.5 s while the volcano erupts. (`APhlegethosSetting::Strike`, `M_PhlegBolt`; the sky's `Flash`/`FlashDir` parameters.)
- **Plume:** many smaller puffs that keep rising and lean downwind.

### The geysers, the badlands, the wastes (2026-11-16)

The user: the first challenge looked like "a weird flat at the top orange funnel that springs up" and "invisible things hit me"; "add more inaccessible volcanic terrain ... less giant sections of lava", "more cinematic in the cutscene intro"; then "don't add the swirling fire things", the staging area before the fortress "too close", "a longer section of volcanic terrain before the fortress ... more like open natural land ... less contrived block feel", "a setting appropriate path winding its way to the fortress".

- **Geysers (the bug):** the spout was a short cone at the vent while the burn landed 1.5 m across on the spine, where nothing was drawn. Now the vent bubbles and a glowing mark spreads on the spine where it will come down. Then it throws an arc of molten blobs over the edge onto the mark, and the burn is only there (the mark's size), only while the arc reaches it. The blobs, the bombs, their marks and the fire rain use a new unlit `M_PhlegMolten` (on the lava material they came out half black, half white).
- **Badlands:** past the route's lava, from 10 m off it out to 3.5 km and with no collision (never in reach), a heightfield of black crust in stepped flows and pressure ridges, climbing to jagged hills toward the ranges. The lava is left in rivers threading it (great ones, and nearer smaller ones) and in pools. 46 burning flows run down the hills into the rivers, some falling off a mesa 2 km out where a city of fire stands (black towers, a 440 m citadel spire, lit windows, a wall round the rim: Phlegethos's Abriymoch in look only). The crust near the route and along the banks cracks and glows. The shores follow the flatter diagonal, so they do not staircase from the fall. The old near-lake mesas, which read as boxes, are gone.
- **Fire rain:** a burning rock out of the smoke every 2 to 5 s far off (more while the volcano erupts), trailing hot smoke and bursting where it lands; never on the route.
- **The wastes:** the gate moved from X 287 m to 440 m. The last leap now lands on a promontory of open, broken ground: a shallow valley with hummocks, sharp-crested rock, pressure ridges framing it, lava standing in hollows off the path, a ragged coastline. Columnar basalt stands in clusters, with tors, boulders, glowing fissures and steam. Worn flagstones wind about 150 m along the valley floor (`Phleg::ApproachPathY`, `ApproachGround`), marked by iron stakes every 17 m and a brazier at every other one, to the level forecourt where the idols' processional (now 5 pairs) leads to the gate. A heightfield with collision replaced the flat box plateau.
- **Cutscene crane:** rises to 56 m and frames the badlands, the wastes and the fortress, the volcano on the left and the city on the right.
- **Not built (the user withdrew it):** the fire whirls.

### The lava from the CC0 scans (2026-11-16)

The user: "Download and implement free resource(s) to make all the lava look better both lava sea and flowing lava things". With the user's yes, ambientCG `Lava001_2K-JPG.zip` (34 MB) and `Lava005_2K-JPG.zip` (35 MB); their colour, emission and normal maps are kept (about 43 MB). `M_PhlegLava` was rewritten around them:

- **The sea, toward the user's goal image** (`References/Phlegethos/phlegethos-goal-lava-columns.png`, given mid-task): rounded crust plates about 1.5 m across, varied in size, dark red-brown and smouldering faintly. They drift apart on a branching net of thin bright veins that widen in places and pool at the junctions, and the plate rims glow from the veins. Lava001's fine cracks craze the plate tops. The rounding comes from a smooth minimum of the cell-border distances, warped twice; the old hard-edged Voronoi read as a hex net. The veins fade into Lava001's crust scan (two scales) from about 60 to 200 m, before they shimmer. A first pass with only the streaky scan on the lake was replaced after the goal image.
- **Open pools, toward the user's second goal image** (`References/Phlegethos/phlegethos-goal-lava-fall.png`): in places (about a sixth of the lake, 100 to 300 m patches) the plates shrink to rafts and give way to bright open melt crossed by thin swirling lines (Lava005, drifting).
- **Rivers, flows and falls:** Lava005's streaks run down the flow at the flow's speed, two layers sliding past each other. After the second goal image they are nearly all molten, orange to yellow, the darker red streaks running down them. The falls are still flat sheets, with no thickness or spray, and no steam where they land.
- **Far off and from the fall:** two coarse layers of the crust scan (about 400 m and 1 km) keep the lake broken into rafts and glowing rifts; finer detail fades out before it shimmers. The horizon no longer shows a grid of tiles.
- **Two old faults fixed:** the throb was a sine of the UVs and drew 18 m diagonal stripes seen from the fall; and the bright base colour no longer catches the light (the glow is emissive).
- The glowing cracks on the walkable basalt are the basalt material and were not changed.

### The landing hills, hot cracks, fall splash, more light and spawns (2026-11-16)

The user (with a goal image, `References/Phlegethos/phlegethos-goal-hot-cracks.png`): "Make the hot cracks look better like this goal image. Remove the lava spigot hazards, keep these areas, just remove this hazard and the visuals that go with it (later we'll add flame skull hostile npcs here as the hazard). Expand the overall playable area by creating a natural land mass looking hilly section with pathway similar in design to the last part of the map but have it before the current first part of the map, make it so this is where Chuck lands. ... Give the lava waterfalls natural splash effect ... Slightly increase lighting for the whole region. Add additional spawn points ..."

- **The hills** (`Phleg::HillsGround`, `HillsPathY`; X -13 800 to the plain):
  - The fall now lands at X -11 000 in a level clearing about 13 m across, its ground the goal image's cracked plates.
  - A flagstone path winds about 85 m east over two long rises (to about 3.3 m) and a dip, to the plain's neck, where it joins the plain's own path. The plain's path now runs across where the old landing was.
  - Off the path: rolling hills, broken crests and knolls, low near the clearing and rising away from it; ridges framing the valley; basalt column clusters, tors and boulders; glowing fissures; a few iron stakes; lava-filled hollows well off the path with steam.
  - The land mass goes down into the lake 40 to 55 m out on either side and behind the landing.
  - The badlands keep clear of it, and the meteors fall elsewhere.
  - No hazards: hostile NPCs come later.
- **The geysers** are gone: their vents, fountains, warning marks, lights, sounds and burns. The road is otherwise as it was.
- **The hot cracks** (`M_PhlegBasalt`, wherever the vertex colour marks the ground hot):
  - Broader plates (about 2.5 m), warped twice so they are irregular.
  - The crack edges are ragged at small scale and bevelled down into the crack (the normal tilts on the lip).
  - The cracks are 3 to 8 cm wide and glow almost all along: a yellow core, fire flickering along them, the plates' lips lit orange.
  - Dark hairline cracks run across the plates.
  - The first pass glowed too strongly: the cracks bloomed into bands and the exposure turned the plates black.
- **The lava falls' splash** (`TickSplash`, within 90 m):
  - Molten spray thrown up and out where each of the gorge's six falls meets the lake, in surges. The drops are stretched along their flight and fall back in arcs, with bigger gobbets among them.
  - Low boiling swells on the melt; steam and smoke rolling up; a flickering unshadowed light; a hiss now and then.
- **Light:**
  - The sun goes from .9 to 1.2 and the sky light from .9 to 1.15.
  - The exposure bias goes from -.2 to -.05.
  - The lava's light on him is a fifth stronger.
- **Spawns:** 13 instead of 6. A death returns him to the last one he passed:
  - the hills' landing;
  - the plain;
  - the road, and its last long segment before the sprint leap;
  - the lake, and the column beyond the slabs;
  - the gorge, the shelf past its first break, and the low shelf before the chains;
  - the ascent, the ramp's top before the crumbling steps, and the chimney's foot;
  - the fortress.

  The dev checkpoint page lists all 13 over two columns.
- **Tests:**
  - New trial `hills`: from the landing along the path to the plain at a run, never more than 3 m off it (85 cm). That makes 15 trials.
  - The stills now cover all 13 spawns, and the fall-splash shots replace the geyser shots.
  - Verify-Package expects `passed=15`.

### The second geysers, a quieter splash, flameskulls and fire snakes (2026-11-16)

The user: "you took out the first flame geyser section but left the second. also take out the 2nds flame geysers. reduce the intense large splash pattern from the lava falls and tone done the sound effects for them. then do the npc's for the region (one change from the 2d game, in 2d we had both horned devils and pit fiend but i dont want the separate horned devils in our game)".

- **The second geysers** were the ascent ramp's lava bombs: balls arcing onto glowing marks, which read as the road's geysers again. The bombs, their marks, their slams and their burns are gone. The volcano still erupts in the sky while he is on the ramp (every 6 to 9 s). The `bombs` trial is now `ramp` (up it at a run).
- **The falls' splash**, toned down:
  - About a third as many drops, thrown half as high and far.
  - The big gobbets are rarer (7 %) and smaller.
  - The boiling swells are smaller, the smoke thinner, the light a quarter dimmer.
  - The hiss: a third as loud, fading with distance, a third as often, only within 20 m.

### Creatures

From the 2D game's Phase 8 bestiary (`docs/development/PHASE-8.md`, `src/entities/`), less the horned devils. Each keeps to its own ground and never follows him far.

| 2D | 3D | Where | State |
| --- | --- | --- | --- |
| Flameskulls: weave fast round a fixed haunt, bee-like, never pursue; cannot be scratched; float over lava | `APhlegFlameskull` (PhlegethosCreatures.h) | The lava road (4, where the geysers were), the ascent's ramp (3, where the bombs fell), and 5 out over the lava as the setting's life | **Built** |
| Fire snakes: the temple snake, only its look changed; on the shores and in the yard | `ATempleSnake` with `bFire` | The hills beside the path (4), the wastes (4), before the gate (3) | **Built** |
| Lemures: the zombie/skeleton lifecycle, the slowest and most durable | `ATempleSkeleton` with `Post.Kind = 1`, body `SK_Lemure` | The hills (3), the wastes (3), before the gate (2); 3 lie in the ash | **Built** |
| Spined devils: never move; flick burning tail spines across a lane on a staggered beat; scratching one defeats Chuck at once | `APhlegSpinedDevil` (PhlegethosCreatures.h) | On columns in the river beside the gorge's shelf (3), on low columns beside the wastes' path (3) | **Built** |
| Horned devils: the Chult colossus's gameplay | **Not built** (the user) | | |
| Pit Fiend: the colossus, wholly avoidable, beyond notice range of the route (the fractured way); the trio's battle at the fortress | `ATempleSkeleton` with `Post.Kind = 2`, body `SK_PitFiend`. Its battle with the trio stays out of scope | Its round through the wastes | **Built** |

**Flameskulls.**
- The model: the temple skull at 1.2 times real size, embers in its sockets, and flipbook fire that moves with it, so its sparks trail behind. Its own light flickers on its face and on the ground under it.
- Its motion (2D: a sine run with a weave 3.1 times faster): runs of 2.6 m each way across the spine and out over the lava (about 4.8 s there and back), a 45 cm weave and a bob, chest high on him. It looks where it goes and banks into the weave.
- A touch burns: a Sanity point, a hiss. He is thrown a step back along the way, never off the spine's side.
- A soft rush of fire as it passes within 2 m.
- It cannot be scratched. Asleep (hidden, still) beyond 90 m.

**Fire snakes.**
- The temple's pit viper and all its behaviour (the crawl, the tell, the strike, one scratch kills), with:
  - a leash in the stage's frame;
  - its track following the ground under it (traced point by point), and turning back from drops, boulders and the lava's edge;
  - lit by the stage's light, not the temple's torch channel;
  - a burning skin, `M_PhlegFireSnake`: the snake's scale textures as a charred crust with the heat glowing through every gap, running and flickering down its length. Its own light flickers over its middle. Dying, it cools.

**The models (lemures, Pit Fiend, spined devils).** Sculpted as distance fields by `Tools/build_phleg_creatures.py` (Blender 4.5.14, the beholder's `sdf_kit.py`; no downloads), imported by `Tools/import_phleg_creatures.py` into `/Game/Art/Phlegethos/Creatures`.
- The lemure and the Pit Fiend are skinned to the temple skeleton's own rig (the script rebuilds it, unchanged), so the skeleton's CMU motion-capture clips drive them. Each primitive belongs to a bone; a vertex is weighted to the bones of the primitives nearest it.
- Colour: vertex colour (the albedo with its occlusion baked in; alpha the glow) and a tiling detail normal, in one material, `M_PhlegCreature`. Unreal stores the vertex colours gamma-encoded, so each instance raises them to the power 2.2.
- Review renders of each model: `Local/creatures/` (not in Git).

**Lemures.**
- A melting mass of flesh in a man's shape: hunched, the head run down into the shoulders, the face like wax with two ember eye pits and drips at the chin, flesh running down the body in sags and rolls, drips hanging from the belly, back and arms, the deepest folds glowing with the heat inside.
- The skeleton's whole lifecycle at 0.93 times its size: it stands guard or lies in the ash and gets up when he is near; it shambles after him at 50 cm/s (below his walk; 2D: 15 to the skeleton's 25); kicks and swats (never picks him up), a longer pause between blows. Seventeen scratches put it down (2D: 10 to the skeleton's 6); it collapses and melts into a puddle where it lies.
- Wet groans (the zombie's sounds, lower); its feet are silent. Each keeps to its own patch beside the path.

**Spined devils.**
- A small devil (56 cm crouched) like a gargoyle on a basalt column: bat ears, swept horns, a fanged snout, embers for eyes, spines all over its back and arms, small bat wings, a long spined tail ending in a tuft of smouldering spines.
- Every 2 s (staggered, 2D's beat) its tail winds back and whips over its shoulder, and a burning spine (flipbook fire) flies across the lane at 9.5 m/s at his chest height. A spine that meets him burns a Sanity point and shoves him on along its flight; the rest strike the stone and burn out. Its wings beat as it throws; its head follows him.
- Brushing against it costs a point; scratching it costs all of them (2D: melee is fatal).
- On the gorge's shelf the spines fly from the river across the shelf at the cliff: wait for the beat, or wall run past. In the wastes they cross the path.

**The Pit Fiend.**
- Modelled after the user's goal image (`References/Phlegethos/pit-fiend-goal.jpg`, 2026-11-16): a winged devil 4.2 m tall (2.4 times the rig), its horns rising another metre. Smooth crimson hide over heavy muscle, dark blotches on the shoulders; long horns from the crown sweeping out, then up and in; a snarling fanged muzzle on the rig's jaw bone, burning eyes; charcoal armour (a collar, layered spiked pauldrons, spiked bracers, a belt hung with three skulls, plates down the front and outside of each thigh, knee guards with a spike, greaves, armoured clawed feet); great bat wings; spines down its back; a long barbed tail, its last half trailing on the ground behind it.
- A spiked, flanged mace in its right fist, part of the skinned mesh (weighted wholly to the right hand, so it swings with the walk and the swats). Moving flipbook fire rises from the head's crown, with a flickering light above it. The build script gives the head's position as an affine combination of four joints (`creatures.json` "mace_head"), which the game turns into the right hand's bone space from its own reference pose (no socket, whatever the import's axes and units).
- The mace's iron has no glow of its own: at every vertex-alpha glow tried (down to .015 of the eyes' 30) Phlegethos's dark exposure burned it out pale.
- It walks a slow round through the wastes (95 cm/s) along the path itself, weaving across it within 3 m of it (2026-11-16, third pass: 15 m off, the user could not find it among the boulders; the boulders now are cover from it). It notices him 22 m away in front of it, or 8 m all round; it roars, then strides after him at 170 cm/s (faster than his walk, slower than his run). It cannot be scratched (a dull thud). It gives him up at the edge of its ground or when it has lost sight of him for 4 s, and goes back to its round.
- A warm fill light moves with it, from 3.4 m in front and 2.6 m up (dark crimson in the dark wastes, it did not read).

**The Pit Fiend's motion** (2026-11-16, third pass; the user: "more realistic leg movement and placement and overall more immersive body movements such as wings beating etc ... look formidable in battle, not just a lumbering skeleton with dressing"). The skeleton's motion capture still moves the body; `PitFiendMotion.cpp` sets layers over it each tick (`FFiendPose`, applied in `SkeletonAnim.cpp`):
- Its own bones: each wing's arm, forearm and three fingers (with a leaf at each finger's end) and eight tail bones, built by `Tools/build_phleg_creatures.py` and merged by the import into the temple skeleton's shared skeleton asset (the skeleton's clips leave them at rest). The membranes are weighted across the bones round them.
- Feet: each foot is set onto the ground actually under it (a trace), the sole laid on the slope (up to 22 degrees), by two-bone IK; the hips come down into a heavy, bent-kneed stance and lower still where a foot's ground is lower; each footfall (an ankle coming down onto its ground) dips the hips, jolts the wings, plays the tread, throws a puff of ash and shakes the view a little near it. It steps round as it turns where it stands, instead of sliding on its feet.
- It leans into its going (up to 12 degrees as it charges), rolls into its turns, and breathes.
- Wings: folded on its back on its round, half open as it comes for him, spread in the roar and over a swing. A beat is up slow and part folded, down hard and spread; the downstroke throws up the ash under it and lifts the body a little. Two in the roar, one every 2.6-4.2 s as it charges.
- Tail: sways with its stride, trails its turns, lifts off the ground as it charges and lies along it at rest, now and then flicks; it is kept out of the ground by traces under its joints.
- The roar: reared back 22 degrees, head up, jaws wide, wings spread and beating twice, the view shaken; 2.6 s, then it comes.
- The mace (a new attack, `Smash`, its most common, chosen from 2.2 to 3 m): swung up over its right shoulder in 0.95 s, the chest turning away, the left arm thrown forward, the wings flaring up; held a moment; brought down in an arc gathering speed into the ground in front of it (0.22 s), the body bending 50 degrees into it and crouching; ash, embers, a flare of the mace's light, the view shaken hard; left in the ground half a second, then dragged up. The arm is two-bone IK on a planned path of the head; the haft is turned along it. Under the head (within 38 cm of him) he is dead, a roll at the right moment still saves him; within 4.2 m the blast knocks him off his feet (`AChuckCharacter::Blasted`: flung tumbling, crash-lands, gets up; no harm of itself). It still swats at its feet and stamps (the stamp shakes the ground; its blast within 2.3 m knocks him down).
- The hit is judged at the head's planned landing place: the bones read on the game thread lag a swing this fast by a frame or two.

**Lemures** pick him up by the leg and throw him as the temple skeletons do (the user, 2026-11-16), with their kicks and swats.

**Tests.**
- Two new trials, `skulls` and `fire_snakes`, making 17; then three more (below), making 20. Verify-Package expects `passed=20`.
- `skulls`: he stands in a road skull's lane. It burns him (2 burns), throws him back along the spine and does not kill him.
- `fire_snakes`: a hills snake sees him, comes and strikes; one scratch kills it.
- `lemures`: a hills lemure sees him, comes and - its first attack - picks him up by the leg and throws him; he goes back and scratches it seventeen times (his sanity held up); it falls and melts. (In the trials a throw goes only where there is ground, as in the skeleton's test.)
- `devils`: in the gorge shelf's first lane its spines burn him (2 of 6 thrown); beside a wastes devil's column one scratch takes all his sanity.
- `fiend`: on the path before it, it notices him, roars (two wing beats), comes and kills him (third pass: by its mace; footfalls and beats counted, the planted feet within 13 cm of their height on flat ground).
- `fiend_show` (by name only, not in the 20): the fiend on the open forecourt filmed whole by a camera of its own off its right: its round, the roar, coming on, the mace brought down (`Show_*` stills).
- The other trials run with the creatures calm (they test the moves, and do not dodge).

### Known gaps

- The creatures were tested only by the trials, not played by hand: how hard the devils' lanes and the Pit Fiend's round are is untried.
- The lemures share the skeleton's motion capture: a stiff, upright walk, not a shamble.
- The Pit Fiend's legs are still the skeleton's motion-capture walk under the IK and the heavy stance, not a gait of its own; its swats and stamp are the skeleton's clips at 2.4 times; the wings' membranes stretch rather than crease when folded; the thigh plates and the mace may cut into the body in some poses.
- The world, motion, sound and playtest audits do not cover Phlegethos yet (no baselines).
- Toward the goal art:
  - The fortress walls and the plinths are still plain boxes, as are the mesa and the lake's shores.
  - The idols are blocky.
  - The volcano's plume reads as one dark blob at 12 km.
  - The lava falls are flat sheets with no thickness (their feet splash, now softly).
  - The far hills are smooth and greyish at a distance.
- The wastes are dark between the braziers; the walk is 70 s at a run.
- The flagstones are plain slabs, not the goal art's carved blocks.
- The hills are a smooth heightfield (1.2 m grid): from low down their slopes read soft, like clay, more than broken rock. Between the clearing and the plain they are dark.
- Not played by hand.
