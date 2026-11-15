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
- ambientCG `Lava004` (the molten surface, driven by a flowing emissive material).

The fire and smoke flipbooks already in the project (Unity Labs, CC0) are reused for the geysers and bombs.

## Out of scope for this milestone

The trio and the Pit Fiend, the Astral escalation and the Feywild river, the river cutscene, and every Phlegethos creature.
