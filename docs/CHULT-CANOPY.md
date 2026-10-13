# Chult canopy grove — setting contract

User-authorized October 13, 2026. Codex owns this expansion's setting. Claude owns the actual cat-to-cat climb/brachiation integration, camera behavior and traversal tests. This is a setting preview awaiting those tests, not a completed brachiation milestone.

## Location and ground access

The grove connects to the original landing loop at local **(4200,500,0)**, then follows an open ground approach to **(5600,900,0)**. A second ground lane winds beneath the forks and returns to the original loop at **(4600,1350,0)**. All coordinates are centimetres relative to `AChultLanding::FloorOrigin()`, currently **(400000,2500000,0)**. The floor, cinematic clearing, entrance and local recovery point are unchanged.

`ChultCanopy.cpp` owns the authored grove. Random forest trunks and colliding understory palms are excluded from its reserved footprint: X=5000–8800, Y=250–3000. Preserve this exclusion when adding foliage: imported trees carry broad crowns and unpredictable branches that can obscure the authored routes. Noncolliding ferns, shrubs and elephant ears dress the ground away from the open lanes. The existing wet-forest materials and crown mesh are reused; no binary assets or Blender sources are regenerated.

The later October 13 user-authorized Poly Haven art pass adds four scanned fern
variants, scanned rocks and bark/soil textures. Optimized island trees stay
outside this reserve with a 650 cm margin. Only the new imported CC0 sources are
derived; authored Blender jungle sources, climb surfaces, crown meshes and named
proposals remain unchanged.

## Climb and standing surfaces

| Fork marker | Local standing reference | Intended use |
| --- | --- | --- |
| `ChultCanopyFork0` | (5600,900,650) | First cat-to-cat climb and choice of two branch routes |
| `ChultCanopyFork1` | (7200,900,650) | Level first chain, standing pause and onward branch |
| `ChultCanopyFork2` | (8200,1800,760) | Higher overlook and vine return section |
| `ChultCanopyFork3` | (6800,2400,650) | Return loop and another climb/recovery point |

Each fork uses two bevelled trunks at Y ±160 cm from its centre. Their broad inner faces lie at Y ±60, giving **120 cm clear separation**, with **140 cm flat face width** along X. Visible procedural bark also supplies query/physics collision. Trunk tops terminate at the fork reference height (6.5–7.6 m); thinner continuation trunks grow outside the gap. Buttress roots point away from the climbing gap. Broad solid limbs cross and extend beside the tops for resting/dismount surfaces; their round profile is part of the art, not a rectangular hidden platform. The standing marker references the limb's highest surface, not a guaranteed capsule resting transform. Actual climb, mantle and comfortable standing coverage need Claude's tests.

## Branch and vine proposals

`ChultCanopyChains()` returns four named chains, each with **local paw coordinates** and a `bVine` flag. Consecutive proposals are evenly spaced at no more than 150 cm horizontally, with gradual height changes. `BuildChultCanopy()` creates named scene components `<ChainName>_PawNN`; their +X axis is the horizontal chain direction. All carry `ChultCanopyGripProposal`. They **do not call `AddChuckSwingGrip`** and cannot be caught yet.

| Chain | Direction | Surface |
| --- | --- | --- |
| `ChultCanopyChain0` | Fork 0 → 1 | Nearly level branch |
| `ChultCanopyChain1` | Fork 1 → 2 | Gradually rising branch |
| `ChultCanopyChain2` | Fork 2 → 3 | Static vine with 35 cm sag |
| `ChultCanopyChain3` | Fork 3 → 0 | Returning branch |

Proposal endpoints are 300 cm horizontally out from the fork centre, 115 cm above its standing reference. The thin branch's underside or vine's underside meets the proposal's Z. Parent limbs rise away from these corridors. Thin hand branches, parent branches and vine are currently **noncolliding**, while climb trunks and standing limbs collide. This avoids declaring the existing leap arc valid before Claude assesses natural continuous branch contact. The vine is stationary scenery; it implies hand-over-hand traversal, not a swinging rope or physics pendulum.

Claude should integrate world-space grip registration, audit lifetime/respawn/re-entry, and adjust paws/endpoints/collision as needed for actual catches and exits. At a fork, aim and climb/dismount need explicit verification: current no-input chain-end behavior can select the previous grip. The 300 cm transition through each fork is a standing break, not an automatic connection between endpoint grips. No control or movement-code change has been made by Codex.

## Foliage visibility and future encounters

The eight individual crown components are named `ChultCanopyCrown<I>_<Side>` and tagged **`ChultCanopyFoliage`**. They sit to the sides and above the branches to leave open camera space. If traversal play reveals obstruction, Claude can use these components for selective partial fading, with per-component material instances and restoration on exit. No fade is implemented or promised unnecessary: actual orbit, paw visibility and elevated camera behavior must be checked in play.

The grove is provisional procedural setting art: repeated crowns, bevelled climb faces, simple limb junctions and the existing flat forest floor. It does not reach the supplied art references. Refining the exterior tree silhouettes must preserve the paired inner climbing faces and named route contract until the traversal is verified.

This grove offers a future elevated refuge above open ground. No raptors, enemy boundaries, encounter trigger or claim of raptor safety exists yet. Their territory/pacing remains a later approved region step. The isolated buff-only teaching tree is deferred entirely; ordinary paired-trunk climbs are intended to work without the golden cigarette, subject to Claude's traversal verification. Do not place the future pickup above its teaching climb or silently gate this main grove behind it.

## Verification boundaries

Codex's setting checks extend `-ChuckChultTest` with 11 real-capsule ground segment sweeps and metre-spaced floor samples. Captures add `Canopy_Entry`, `Canopy_Overview` and `Canopy_Fork`. These establish setting access and visual evidence only. Actual cat-to-cat climbs, forward/reverse brachiation, fork choice, dismounts, missed catches, empty stamina, physical Xbox and camera/fading tests are Claude's next task. The package verifier's `registered_grips=0` inventory expectation must be updated when Claude activates the chains.

Next part of the work can only be done by Claude: integrate and test canopy traversal on this setting, then decide whether selective crown fading is necessary.
