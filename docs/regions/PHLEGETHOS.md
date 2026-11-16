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
| Lava falls, lava rivers, narrow basalt paths, jump challenges | **Built** as parkour: a narrowing basalt spine, a lava-fall gorge crossed along its wall and on hanging chains, a wall-jump chimney, crumbling steps, lava bombs |
| Lemures, fire snakes, spined devils, flameskulls, horned devils | **Not in this milestone** (creatures are a later Claude milestone). Lava geysers and bombs are this milestone's moving hazards |
| Infernal idols, fortress exterior | **Built**: the processional and the shut gate |
| The trio battling the Pit Fiend, Astral escalation, the Feywild river, the river cutscene | **Not built** (user: "don't do the trio of heros pit fiend encounter yet"). The fortress yard ends at the shut gate |
| Breakable urns with cigarettes | **Built**: clay jars on the safe ground off the routes |

## Chuck

Lava is a fall hazard exactly like the Astral Sea: falling in is an Astral death, and he is summoned back at the start of the stretch he fell from. Geyser spray and lava bombs cost Sanity, as a zombie's bite does. Nothing here asks for a new move: every challenge uses one he already has.

## The stage

One continuous stage, built in code on its own site (`PhlegethosSetting.h`), 8 km beyond the ship's. Frame: X along the route (north, toward the fortress), Y across, Z up. Lava lies at Z 0. The walkable basalt stands about 3 m above it, so every fall is a long one.

| Stretch | Checkpoint | Traversal |
| --- | --- | --- |
| 0. The arrival plain | Phlegethos Arrival | The fall's landing spot. Broad cracked basalt with glowing fissures; boulders to vault and mantle; the worn path north. Jars. The volcano, the lava rivers and the fortress on its mesa are in view |
| 1. The lava road | Phlegethos Road | The road narrows to a basalt spine over the lava: running jumps over breaks, then a sprint-leap gap with a straight run-up. Lava geysers beside the spine erupt on a beat, with a bubbling warning |
| 2. The column lake | Phlegethos Lake | Hexagonal basalt columns standing out of a lava lake at different heights: hops, mantles, ledge pull-ups. Cracked columns sink when he stands on them. Two long basalt slabs either side of a wide lava gap: a chained side wall run carries him across |
| 3. The lava-fall gorge | Phlegethos Gorge | A shelf along the gorge's basalt wall, with lava falls pouring past. A break in the shelf is crossed by a side wall run. Then iron chains hanging from a broken infernal arch over the lava river: brachiation |
| 4. The ascent (Phlegethos_2) | Phlegethos Ascent | The volcano erupts. A wall-jump chimney up the mesa's face; crumbling basalt steps over the lava that fall away behind him; lava bombs falling on glowing marks; a last sprint leap onto the approach |
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

The fire and smoke flipbooks already in the project (Unity Labs, CC0) are reused for the geysers and bombs.

## Out of scope for this milestone

The trio and the Pit Fiend, the Astral escalation and the Feywild river, the river cutscene, and every Phlegethos creature.

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

### Known gaps

- No creatures (lemures, fire snakes, spined devils, flameskulls, horned devils): a later milestone.
- The world, motion, sound and playtest audits do not cover Phlegethos yet (no baselines).
- Toward the goal art:
  - The fortress walls and the plinths are still plain boxes, as are the mesa and the lake's shores.
  - The idols are blocky.
  - The volcano's plume reads as one dark blob at 12 km.
  - The lava falls are flat sheets (now streaming, but with no thickness or spray).
  - The far hills are smooth and greyish at a distance.
- The wastes are dark between the braziers; the walk is 70 s at a run.
- The flagstones are plain slabs, not the goal art's carved blocks.
- The geysers hit hard: running straight through without timing them cost 3 Sanity in the tests.
- Not played by hand.
