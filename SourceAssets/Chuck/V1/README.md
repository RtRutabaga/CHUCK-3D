# Chuck rig v1 source (docs/RIG-CONTRACT-V1.md)

First delivery under the accepted rig contract: the 41-bone rig with one continuously skinned mesh, pose evidence, and the first two test clips (**Idle**, **WalkLoop**). The remaining first-set clips (WalkStart, WalkStop, TurnLeft90, TurnRight90, JumpStart, JumpLoop, JumpLand) are not in this delivery. None of this is used by the running game yet. The legacy `SourceAssets/Chuck` exports and 14-bone rig are unchanged.

| File | Contents |
| --- | --- |
| `Chuck_V1.blend` | Editable source: `SK_Chuck_Rig` armature (41 bones, zero roll) with `SK_Chuck` mesh (armature modifier) and actions `AS_Chuck_Idle`, `AS_Chuck_WalkLoop` (fake user). |
| `SK_Chuck.fbx` | Rest-pose armature and skinned mesh. Import to `/Game/Characters/Chuck/V1/SK_Chuck` with a new skeleton. |
| `Animations/AS_Chuck_<Clip>.fbx` | One clip per FBX, armature only, baked every frame at 30 fps. |
| `Animations/manifest.json` | Frame ranges, loop flags, reference speed, stance intervals and events in seconds. |
| `rig_v1_metadata.json` | Bone and material counts, bounds, measured sole markers (source and Unreal component space), FBX settings, rest matrices for every bone. |
| `check_v1.py` / `review_v1.py` | Contract checks and pose evidence (see below). |
| `Review/` | Neutral views, pose studies, WalkLoop frames, `walk_contact_report.json`. |

## Build and export

Generated only by `Tools/build_chuck_v1.py`. Blender 4.5.14 LTS:

```powershell
$B="$env:LOCALAPPDATA\Programs\CHUCK-Tools\blender-4.5.14-windows-x64\blender.exe"
& $B --background --python Tools\build_chuck_v1.py
& $B --background SourceAssets\Chuck\V1\Chuck_V1.blend --python SourceAssets\Chuck\V1\check_v1.py
& $B --background SourceAssets\Chuck\V1\Chuck_V1.blend --python SourceAssets\Chuck\V1\review_v1.py -- <out_dir>
```

The builder runs the legacy generator (`Tools/build_chuck_model.py`) with `CHUCK_GEOMETRY_ONLY=True`. That reuses the latest authored head, jacket, sleeves, hands and tail and skips every legacy export; legacy output was verified identical by vertex/weight fingerprint. The builder then replaces the legs and paws and applies the v1 weights and clips. Posing helpers are in `Tools/chuck_v1_pose.py`.

FBX settings (both mesh and clips):
- `apply_unit_scale=True`, `axis_forward='-Y'`, `axis_up='Z'`
- `primary_bone_axis='Y'`, `secondary_bone_axis='X'`
- `add_leaf_bones=False`, `use_armature_deform_only=False` (the helper bones are exported), `mesh_smooth_type='FACE'`

Clip exports additionally use:
- `bake_anim=True`, `bake_anim_use_all_bones=True`, `bake_anim_step=1`, `bake_anim_simplify_factor=0`
- `bake_anim_force_startend_keying=True`, NLA/all-actions off

Scene unit scale is 0.01 (centimetres). Re-importing `SK_Chuck.fbx` in Blender gives exactly one armature (object scale 0.01, i.e. metres to centimetres) with all 41 names and no extra bones, plus one skinned mesh. Unreal may add an armature container node above `root`; normalise it by name during import.

## Mesh and weights

- One skinned mesh, 207,771 triangles, nine material slots (Fur, Chest, Jacket, Seam, Skin, Eye, Claw, Metal, Whisker). Ear top at exactly 65 cm; paw soles at Z = -0.002…0.01 cm.
- **Legs:** rebuilt knee-forward along thigh → calf → hock. Their domed ends sink into the paw heel mound.
- **Paws:** now part of the body (the legacy `paw_parts` at origin (-0.8, ±7, 1.96)).
- **Continuity:** leg and paw are overlapping surfaces with shared graded weights, not one merged manifold. They deform together, but a strongly bent ankle can still show the seam.
- **Weights:** graded fields per source part in `build_chuck_v1.py`.
  - Spine chain for the torso, with clavicle share at the shoulders and thigh share at the hips.
  - Jacket shell follows the upper arm near the armhole and lifts with the thighs at the hem.
  - Sleeves: clavicle/chest → upper arm → lower arm. Hands: hand → fingers/thumb.
  - Head, with a neck blend at the back, jaw on the chin/lower lip/lower muzzle, and ears.
  - Legs: pelvis → thigh → calf → foot. Paws: calf → foot → toes. Tail: tail_0 → tail_5.
  - Every deforming bone carries weight; helpers carry none. At most 4 influences per vertex. No `_L`/`_R` cross-weighting.

## Sole markers (measured)

Heel, ball and toe on each paw centreline at ground height, in source cm (Unreal component space negates Y):
- **L:** heel (-5.541, 7, 0), ball (3.4, 7, 0), toe (7.305, 7, 0).
- **R:** mirrored.

Versus the proposed (-4.5 / 3.4 / 8.2): the heel is 1.0 cm further back and the toe tip 0.9 cm shorter, measured on the flesh (claws excluded). The ball equals the `toes_*` head.

## Clips

Both clips loop with no duplicate endpoint sample and keep `root` static (root motion off).
- **Idle** (60 frames, 2 s): breathing through spine/chest/neck, small head yaw/pitch, ear and tail motion. Paws planted at rest; knees re-solved for a 0.25–0.5 cm pelvis settle.
- **WalkLoop** (9 frames, 0.30 s, reference 95 cm/s):
  - Stride cycle 28.5 cm; stance fraction 0.6 (17.1 cm of travel per stance).
  - Pelvis lowered 1.6 cm with a small bob, sway and yaw; counter-rotating chest; arms swing opposite the legs; tail follows.
  - Stance: `foot_L` 0.00–0.18 s; `foot_R` 0.15–0.30 s and 0.00–0.03 s.
  - Heel lifts around the ball over the last 30% of stance (toe roll). Swing lift is 2.4 cm.
  - Peak thigh+calf reach ratio 0.877.
  - Measured on the **deformed mesh** (`Review/walk_contact_report.json`): the ball-of-paw sole vertex moves at 95.0 cm/s through stance, so with the capsule moving at 95 cm/s the world slip is ≤ 0.39 cm/s (worst sample is the start of toe roll). Sole height stays 0.03–0.05 cm.
- **IK goals:** `ik_foot_*` follow the solved hock and `ik_hand_*` the posed wrist in every frame. `socket_cigarette` rides the jaw.

## Pose evidence (`Review/`)

- Neutral: front, side, rear, three-quarter, scale.
- Pose studies: crouch/curl, forward knee flexion, toe roll, overhead grip, and the cigarette held at the left lip corner with the jaw opened 6°. The cigarette is a temporary render-only stick on the socket, not an asset.
- WalkLoop: `walkloop_side_frames.jpg`.

## Known limits

- **Overhead grip:** reads as a hand at head height rather than a full overhead reach. The clavicle/upper-arm range for climbing needs a follow-up study.
- **Jacket:** a separate shell skinned to the body, with no cloth simulation. The armhole stretches at large raises.
- **Hands:** one curl bone for four fingers, as accepted for v1.
- **Ankle:** the leg/paw seam at the ankle is covered by the heel mound, not merged topology.
- **Not tested:** no UVs or LODs; no Unreal import.
