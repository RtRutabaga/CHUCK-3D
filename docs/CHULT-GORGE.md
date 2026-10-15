# Chult — the bridge gorge, its rims and the softer light

Authorized by the user's October 15, 2026 request: soften the single-sun lighting without losing its contrast; rebuild Codex's cliffs, waterfalls and landscape vista on the starting side so the canyon mimics real topography and is a show stopper from the bridge; the far canyon wall may be done now, and the far side's bulk is left for later walkable expansion. Free resources were allowed.

Owner: Claude. Supersedes the geometry of [CHULT-VISTA.md](CHULT-VISTA.md) and the canyon wall faces of [CHULT-CANYON.md](CHULT-CANYON.md). Every existing floor, the bridge, its posts/grips/respawn, the creek, the river, canopy, ruins and encounters are unchanged.

## Lighting (ChultLanding.cpp)

The project runs without dynamic global illumination (`r.DynamicGlobalIlluminationMethod=0`), so anything out of the sun received only the sky term. Games without GI fake the missing bounce with a hemispherical sky term plus artist-placed fill lights opposite the sun, and compress shade with local tone mapping. Chult now does the same:

- Sky light 0.45 → 1.0, with a dim ground-coloured lower hemisphere.
- One unshadowed, specular-free directional fill (0.7, near-neutral) travelling away from the sun and upward, standing in for light thrown back off sunlit rock and canopy. The sun keeps forward-shading priority.
- Local exposure: shadow contrast 0.66, highlight contrast 0.85. The sun/shade pattern stays; the darkest shade lifts.
- The low height-fog layer keeps the jungle floor's haze (same density at the floor) but thickens half as fast with depth, so gorge walls read for ~50 m before the mist.

## Gorge shape (ChultGorge.cpp/.h)

Canyon frame as before: S across the gorge, T along it. Inside the crossing (|T|≤4500 near, ≤6500 far) the lips are exactly the existing banks' edges, S=0 and S=4500; beyond, the gorge widens to 75–100 m and winds, so a view along it ends in overlapping spurs. Shared functions: `ChultGorgeLip`, `ChultGorgeTop`, `ChultGorgeLandZ`, `ChultGorgeWallS`, `ChultGorgeBench`, `ChultGorgeFall`.

Wall profile, from the rim down:

- Soil and roots roll over the rim (radius 0.8 m at the crossing, up to 6.5 m away from it), vegetated.
- A 7 m caprock stands vertical everywhere, keeping the crossing's open air clear.
- Below it a shared stratigraphy (dipping 1.1 % down the gorge, gently undulating) alternates hard beds standing as cliffs with soft beds weathered back into ~48° vegetated benches, with undercuts beneath hard beds. Some stretches are massive sheer walls, others many-benched.
- Buttresses between gullies deepen below the rim; rock roughness; fern/moss mats in patches.
- Behind every fall the face is cut back into a deepening plunge alcove, guaranteed to stay behind the falling water.
- Walls continue to 260 m below their rims, hidden by mist from about 55 m down (`ChultGorgeMistZ`).

Rims: land on both sides is laid from the lips outward, so land and walls share each edge vertex. Plateaus climb gently away from the rim into forested ridged ranges. Ten tributary valleys (five each side) cut V-shaped, round-bottomed valleys that hang above the gorge and notch the rim into amphitheatres. Existing floors are reserved exactly as before: new land starts at their edges with the 11 m boundary rise; vertices inside a floor are buried 12 cm under it. Far-side land fills only outside the far bank/river floor.

## Water, mist, dressing

- 18 plunging falls leave their lips at their streams' speed (air-slowed), spread as they fall and fade into the mist: the creek outfall (same lip point as before), the broad river fall, ten hanging-valley falls (with their streams), and six springs seeping from bedding planes in sight of the bridge. Each is a glassy crest breaking into lace, a thinner sheet behind and a spray veil (`M_ChultPlunge`). No collision.
- Mist sea: ~340 billows along the gorge, spray at every fall's foot, 17 local fog volumes.
- Rim trees lean out over the drop; dense undergrowth spills over both rims near the bridge (noncolliding inside the old 8.5 m floor margin, which trees still keep). Plateau forest and canopy crowns, plants and clinging trees on the wall benches, ~600 clustered lianas. The canyon's six lip fog pools were removed: from the bridge they read as white spheres.

## Materials and sources

`M_ChultGorge`: one opaque material for every gorge and rim surface. Vertex colour R is the rock weight, G pushes stratified rock toward mossy crag, B is spray wetness; the rock/jungle edge follows the textures' own relief, so no triangle edge ever switches material (this replaces the per-quad material split that produced green triangles on the hills). Triplanar on the meshes' local position; damp grime, streaks, sky-facing moss; jungle becomes mottled canopy with distance. `M_ChultPlunge`: translucent falling water.

New CC0 textures from Poly Haven, fetched by `Tools/Fetch-ChultGorge.ps1` (manifest with MD5/SHA-256 in `SourceAssets/Setting/ChultGorge`): Cliff Side, Aerial Rocks 02, Aerial Grass Rock, 2K diffuse/normal/ARM. `Tools/create_chult_gorge_materials.py` imports them into `/Game/Art/ChultGorge` and builds both materials. No other asset, shared material or mesh is changed.

## Checks

`CheckChultVista` (in `-ChuckChultTest`) now checks: no land inside the gorge or over a floor, real-triangle collision probes on solid rim chunks, the six reveal rays, and `CheckChultGorge`: walls stand where the functions say (56 traces), the crossing's open air (289 traces), every fall in front of its wall to the mist (324 probes), and the creek outfall's lip point. Canyon faces and the river fall's visibility trace the new walls. `-ChuckVistaReview` captures `GorgeFromBridgeSouth`, `GorgeFromBridgeNorth`, `GorgeCreekFall`, `GorgeBroadFall`, `GorgeApproach`, `GorgeOverview`, `GorgeRanges`.
