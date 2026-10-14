# Chult raptor — the fast raptor of the canopy

**User request, 2026-10-14:** create the fast raptor, patrolling around the brachiation trees and encouraging the player to climb and evade to get through the area by brachiation; use free resources if they make it easier or better. This places one of the 2D game's two raptors (regions/CHULT.md planned them beyond the wreck) in the canopy area now, at the user's direction. Claude owns the creature (behaviour, animation, import).

## Asset

- **Source:** "Velociraptor" by Quaternius, CC0, from poly.pizza (also in Quaternius's "Animated LowPoly Dinosaurs"). Downloaded with the user's approval. It is kept unmodified in `SourceAssets/Enemies/Raptor/Source/` (`manifest.json` holds the URL, licence, size and SHA-256). Credited in `docs/DEMO-CREDITS.txt`.
- **Build:** `Tools/build_raptor.py` (Blender 4.5.14):
  - Scale and orientation: 30 cm per source unit, facing +X. The raptor is 4.1 m nose to tail, hips at 98 cm, head at 1.56 m (Chuck is 65 cm). The clips' bone translations are rescaled with the rig, checked against the source jump.
  - Mesh repair: the source is flat-shaded with every face's corners split apart. The build welds them and turns inward faces out by ray parity, then subdivides once. The result is 7,488 triangles, smooth-shaded.
  - Skin: body-following UVs (u along the spine, v round it) carry a generated pattern (dark dorsal bands, a pale belly) and a tiled scale texture and normal map. Nothing is downloaded for these.
  - Output: `SK_Raptor.fbx` plus six clips (Idle, Walk, Run, Attack, Jump, Death), and `RaptorClipData.h` with measured natural speeds (walk 86 cm/s, run 469 cm/s) and the bite's reach moment (0.375 s).
- **Import:** `Tools/import_raptor.py` brings these into `/Game/Characters/Raptor`. `M_Raptor` is two-sided, with one instance per source region.
- **Sounds:** `Tools/gen_raptor_sfx.py` synthesizes original sounds (footfalls, the tell's hiss, jaw snaps, a call, a death cry). `Tools/import_raptor_sfx.py` imports them into `/Game/Art/Audio/SFX`.

## Behaviour (`AChultRaptor`, ChultRaptor.h/.cpp)

- **Spawn:** one raptor appears when Chult becomes playable (`AChultLanding::Build`, after the fall or from the checkpoint). There is none in the cutscene. It is kept out of `-ChuckChultTest` and `-ChuckCanopyTest`, whose traversal trials assume nothing hunts Chuck.
- **Patrol:** a ping-pong round of 9 points between the loop's trees and out along the staggered line. Every leg stays at least 5.2 m from every trunk. It walks at 115 cm/s and pauses at each point.
- **Notice:** it notices Chuck within 16 m in view, or within 5.5 m all round, with a call. It then chases at 330 cm/s: faster than his run (225 cm/s), slower than his sprint (380 cm/s).
- **Bite:** at 2.3 m it gives a 0.32 s tell (a hiss, crouching into the strike), then lunges. The bite costs 2 of his 5 sanity. A roll or side jump through the tell beats it.
- **Out of reach:** it cannot reach him when his feet are more than 1.1 m above its own. That covers a tree limb, the branches, the ruin top and high on a liana; low on a liana it can still bite. Then it prowls beneath him, leaping and snapping up every 2.4 s. After 9 s it gives up and returns to its round, not noticing him again for 5 s.
- **Territory:** the canopy reserve plus 15 m. If he leaves it, or is out of sight for 3 s, or beyond 32 m, it goes back to its round.
- **Scratches:** ten kill it, as in the 2D game. Each one makes it flinch back. It never has to be fought.
- **Death:** dying to it returns him to the ruin top (CHULT-CANOPY.md), which it cannot reach.

## Tests

`-ChuckRaptorTest` (also run by `Verify-Package`) runs 8 trials with the real character:
- patrol;
- notice, chase and bite;
- dodge the tell;
- escape up a tree;
- brachiate over it;
- leave its territory;
- killed, returns to the ruin top;
- ten scratches.

It also does a static sweep of the patrol route (8 segments) and requires at least ten sounds played. `-ChuckRaptorCapture` saves frames to `Saved/Screenshots/.../Raptor`.

## Not done

- No hand-made animation, blending or foot IK: clips switch directly. Play rates follow the measured clip speeds.
- The source's low-poly forms remain under the smoothing, below the art target.
- The sounds are synthetic and have not been heard on speakers.
- No navmesh: it steers round obstacles with sweeps.
- Only one raptor; the second 2D raptor is not placed.
- Not played by a person; no Xbox test.
