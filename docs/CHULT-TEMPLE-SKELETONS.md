# Chult — the temple skeletons

User request, October 21, 2026 (Claude): "create the skeleton enemies in temple, use free resources where helpful, make them look good and have realistic attack motions". This is the next step of [CHULT-TEMPLE-INTERIOR.md](CHULT-TEMPLE-INTERIOR.md) (its "Next" item 1). Claude owns the creature (model, rig, animation, import, behaviour).

## The 2D direction

Read-only sources: `docs/development/PHASE-4.md` and `PHASE-6.md`, `src/entities/undead.py`, `src/core/config.py`, `tools/generate_undead_sprites.py` and the maps `temple_spikes` and `temple_skeletons`.

- Skeletons are the temple's primary enemy: five in the spike corridor (map 2) and twelve "avoidable, durable" ones round the monumental piers of the skeleton hall (map 3). Later, twelve more in the shrine hall (map 7) and some in the final chamber.
- An unarmed human skeleton (no weapons, no equipment, PHASE-4).
- Against the zombie: faster (25 vs 18 px/s; Chuck walks 80), fewer scratches (6 vs 8), a slightly weaker hit (15 vs 20 sanity). Avoiding or outrunning them is the intended play; fighting is possible.

## Asset

- **Model:** the realistic skeleton of Blender Studio's **Human Base Meshes** bundle v1.4.1 (CC0; the model by 1d_inc, cleaned and UV-unwrapped by Julien Kaspar). Downloaded with the user's approval (`human-base-meshes-bundle-v1.4.1.zip`, 50.6 MB, from blender.org's mirror). Only its "Skeleton - Realistic" collection is kept, unmodified: `SourceAssets/Enemies/Skeleton/Source/skeleton_realistic.blend` (1.1 MB, `manifest.json` with the URL, licence and hashes). Credited in `docs/DEMO-CREDITS.txt`.
- **Motion:** CMU motion-capture takes (B. Hahne's BVH release, as the townsfolk's), six downloaded with the user's approval into `SourceAssets/Mocap/CMU` and listed in `Tools/Fetch-CMUMocap.ps1` (sixteen candidates were retargeted and compared on contact sheets), plus the zombie's fall already in the project:

| Clip | Take | What it is |
| --- | --- | --- |
| Idle | 140_06 "Idle" | crouched and ready, hands open in front |
| Walk | 74_01 "stiff walk" | upright and rigid, head forward: purposeful, not a shamble (87 cm/s at its own pace) |
| Kick | 74_03 "kick" | weight back, a short front kick low along the floor that stamps down after it |
| Grab | 139_06 "Poking Around On The Ground" | drops into a crouch, a hand clawing along the floor ahead |
| Swat | 76_05 "swatting at pesky bug" | bent double, a hand slapped down at its own feet |
| Rise | 140_08 "Get Up From Ground Laying on Back" | from lying flat, onto an elbow, a knee, up |
| Collapse | 113_08 "Lay down and get up" (already in the project: the zombie's fall) | knees go, down onto the floor, flat on its back |
| Strike | 02_05 "punch/strike" (added October 23 for the sanctum: [CHULT-TEMPLE-HEROES.md](CHULT-TEMPLE-HEROES.md)) | weight onto the front foot, a fist driven out at chest height; its fingertips 63 cm ahead and 157 cm up at 0.47 s. Only the sanctum's skeletons use it, against the heroes |

  Dropped: "Sneaking" (staggered), "Walk Crouched" (too low), punches (they pass over a 65 cm rat), shorter kicks, "smash against wall", a second get-up.

### Build (`Tools/build_temple_skeleton.py`, Blender 4.5.14)

- The source is 135 separate bone pieces in a parent hierarchy whose pivots sit on the joints. The rig is built on those pivots with the humans' bone names (pelvis, spine_01-03, neck_01, head, clavicles, arms, hands with three bones a finger, thighs, calves, feet, balls) plus a **jaw**: 54 bones. `Tools/build_npc_mocap.py`'s retargeting therefore works on it unchanged (it gained module settings `PREFIX`, `GROUND`, `AFTER` and a `__main__` guard; the humans' output is unchanged).
- **Rigid binding:** every piece is weighted entirely to one bone, as real bones move. Vertebrae go to the nearest spine bone, the ribcage and sternum to spine_03, the scapula with the clavicle, the patella and fibula with the calf, the toes with the ball. The skull is split by loose parts: the mandible and lower teeth ride the jaw.
- **Budget:** the pieces' mirror modifiers applied, one subdivision level on the larger bones, then a collapse decimation (40 % of those, 55 % of the skull): **104,032 triangles**, 175.8 cm tall. Unreal makes three more LODs (vertices 69k / 21k / 12k / 7k).
- **Surface:** ambient occlusion baked into the vertex colour (Cycles, 6 cm distance: crevices, the ribcage's inside, the eye sockets); a generated, tileable bone texture `T_SkeletonBone` (broad staining, grain running along the bones, pits and hairline cracks, wear) and its normal map.
- **Clips:** the seven takes retargeted (30 fps). Takes whose reference pose stood higher are put back on the floor (the lowest ball joint over the clip at its rest height; the get-up moved down 96 cm). Each attack's moment of contact is measured from its bones and written to `Unreal/Chuck3D/Source/Chuck3D/SkeletonClipData.h`: the kick's toe 78 cm ahead and 53 cm up at 0.47 s; the grab's fingertips 77 cm ahead, 23 cm up and 17° to the right at 1.0 s; the swat's fingertips by its feet at 1.2 s.

### Import

- `Tools/import_temple_skeleton.py` (UnrealEditor-Cmd): `/Game/Characters/Skeleton` — `SK_TempleSkeleton` with its own skeleton asset and generated LODs, seven `AS_Skeleton_*` clips (no root motion), the textures and `M_SkeletonBone` (aged ivory stained in broad patches, darker and dirtier in its crevices by the baked occlusion, pitted, paler where worn; rough, low specular) with `MI_SkeletonBone` and `MI_SkeletonTeeth`.
- **Sounds:** `Tools/gen_skeleton_sfx.py` (stdlib, seeded, the traps' stone-chamber acoustic) makes 16 originals: footfalls, teeth chattering, a swung limb, a scratch on bone, the collapse, the get-up. `Tools/import_skeleton_sfx.py` imports them to `/Game/Art/Audio/SFX`.

## Behaviour (`ATempleSkeleton`, TempleSkeleton.h/.cpp; animation SkeletonAnim.h/.cpp)

- **Posts** (`ChultTempleSkeletonPosts` in ChultTempleInterior.cpp): seventeen, as in the 2D maps.
  - *Spike corridor:* one lying in each gap after the first band (the entry gap holds the respawn), alternating walls; the fifth stands in the last gap by the door to the pier hall.
  - *Pier hall:* seven before the rift (two by the rings' wall, the rest among the piers, none within 5 m of the respawn), five beyond it (one where he steps off the last ring, one before the dart gallery). Eleven lie on the floor, six stand.
- **Its own space:** each is kept to a rectangle of the canyon frame: its gap between two spike bands, or its side of the rift (60 cm short of the lip). It follows him to its edge and stands there watching; it never walks into a band or the rift.
- **Waking and noticing:** a lying one gets up (the whole motion-captured get-up, about 3 s) when he comes within 3.8 m, or when scratched. A standing one notices him in view within 7.5 m, or within 3 m all round, if it can see him.
- **Pursuit:** the stiff walk at 82 cm/s (a little over his walk; his run leaves it far behind), the stride's phase driven by the ground it covers so the planted foot stays planted. It steers round piers, jars and drums.
- **Attacks**, chosen by how far he is: under 58 cm the swat; to 98 cm the kick (60 %) or the grab. While it winds up it turns to bring the striking foot or hand onto him, with its teeth chattering (the tell, a rattle); about 0.2 s before contact it is committed. The blow lands only if the striking bone actually meets his capsule (within 12 cm) in a short window round the contact frame. So stepping back, rolling, or being behind it beats it.
  - Kick: 2 sanity and the human-sized knock-back (as the zombie's blow). Grab and swat: 1 sanity.
- **Out of reach:** he cannot be struck hanging from the rings, or off its floor. After 6 s out of reach, or 2.5 s out of its space or out of sight, it walks back to its post. A lying one lies down again (the get-up played backwards).
- **Pauses:** after each attack it stands off and watches for 0.7-1.5 s before the next: the opening to scratch it. (Without the pause it struck back to back and could not be fought.)
- **Scratches:** ten put one down (2D 6 against the zombie's 8; the 3D zombie takes 14). Each makes it flinch, chest thrown back. The tenth: a clatter, its knees go and it drops flat on its back (the fall clip at 1.8×), then lies still with the skull rolled aside and the jaw hanging open. It stays down; Chuck can walk over it.
- **Layers on the clips:** the head and neck turn to follow him; the jaw rests a little open, chatters in the tell and drops at the strike; a flinch springs the chest back.
- **Light:** on lighting channel 1, lit by the interior's torches only.
- **Automated runs:** skeletons spawn in play and in `-ChuckSkeletonTest` only. Every other `-Chuck…Test/Capture/Review` run keeps the world its checks were written against, including the interior review.

## Tests

`-ChuckSkeletonTest` (`TempleSkeletonTest.cpp`), with the real character, one skeleton at a time (the rest held):

1. *placement:* 17 skeletons (5 corridor, 12 pier hall, 11 lying), each on its floor, inside its space, at its post;
2. *wake_rise_chase:* he comes near a lying one: it gets up and comes for him faster than his walk;
3. *kick_lands*, *grab_lands*, *swat_lands:* standing where each reaches: it lands by its bones, for its damage;
4. *step_back_misses:* a step back as the kick winds up: the toe passes short of him;
5. *roll_dodges:* a roll as the kick winds up: no hit;
6. *kept_to_its_gap:* a corridor skeleton follows him to its band's edge, gives up, walks back and lies down again, never more than 8 cm out of its gap;
7. *rings_out_of_reach:* he hangs from the first ring: it comes to the lip, cannot strike, stays in its space;
8. *ten_scratches_fall:* ten scratches: it falls and lies flat (pelvis down over 60 cm, every limb between the floor and 40 cm above it).

`-ChuckSkeletonCapture` also saves frames to `Saved/Screenshots/.../Skeleton`.

### Placement and test numbers

`-ChuckSkeletonTest` adds two kinds of number to the trial list above. 17 skeletons: 5 in the corridor and 12 in the pier hall; 11 lie on the floor. The capsule is 34 cm round. Its body is wider than its bones at rest, so a crouch or a kick does not swallow the rat.

## Evidence (October 21)

- Uncooked `-ChuckSkeletonCapture`: all ten trials pass (`Local/skeleton-test.log`).
  - **Contact:** the kick strikes 3.4 cm from his body, the grab −2.8 cm (inside it) and the swat 0.7 cm.
  - **Misses:** after a step back the toe passes 71 cm short, and a roll takes him 29 cm clear.
  - **Wake:** a lying skeleton gets up and comes on at its 82 cm/s.
  - **Corridor:** the corridor skeleton never leaves its gap (0 cm out of it), returns and lies down again.
  - **Rings:** no blow reaches him on the rings.
  - **Fall:** at the tenth scratch the pelvis drops 79 cm; every limb ends 2-20 cm off the floor, the jaw at 20 degrees.
- Frames: `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/Skeleton/` (portraits, the hall, every trial; generated, not in Git).

## What was tried and dropped

- **A ragdoll death.** The fall was first a physics ragdoll. The editor's physics-asset generator made it trouble:
  - Unreal puts a ×100 scale on the Blender armature node above Root. Changing the Blender export did not move it, and Interchange ignores `convert_scene_unit`.
  - So bone space is in metres, and the generator's minimum body size blew every capsule up to half a metre.
  - Even with refitted capsules, the rendered mesh hung about 40 cm above its own bodies in some runs.
  - The interior's one-sided floors also let small bodies through, which needed a hidden slab.

  The motion-captured fall is deterministic and lies flat every time. No physics asset is made.
- **Takes:** "Sneaking" (staggered), "Walk Crouched" (too low), punches (they pass over a 65 cm rat), shorter kicks, "smash against wall", a second get-up.

## Not done

- No person has played against them. Difficulty, how readable the tells are and camera comfort in the 4 m corridor are untested by a human.
- The sounds are synthetic and were checked only as files and as played events, not listened to.
- No navmesh: it steers round obstacles with short sweeps inside its rectangle.
- Seventeen 104k-triangle skeletons are on screen at most a dozen at a time; LODs drop to 7k vertices. Cooked frame cost in the pier hall has not been measured separately.
- The mesh is the realistic CC0 model with generated surface detail, not a hand-painted texture. Close up its joints are clean, rigid pieces with no cartilage or wear at the contacts.
- Skeletons of the shrine hall (map 7) and the final chamber come with those maps.
