# Chuck rig v1 source (docs/RIG-CONTRACT-V1.md)

Delivery under the accepted rig contract: the 41-bone rig with one continuously skinned mesh, pose evidence, and the complete first clip set: Idle, WalkStart, WalkLoop, WalkStop, TurnLeft90, TurnRight90, JumpStart, JumpLoop, JumpLand. None of this is used by the running game yet. The legacy `SourceAssets/Chuck` exports and 14-bone rig are unchanged.

| File | Contents |
| --- | --- |
| `Chuck_V1.blend` | Editable source: `SK_Chuck_Rig` armature (41 bones, zero roll) with `SK_Chuck` mesh (armature modifier) and one `AS_Chuck_<Clip>` action per clip (fake user). |
| `SK_Chuck.fbx` | Rest-pose armature and skinned mesh. Import to `/Game/Characters/Chuck/V1/SK_Chuck` with a new skeleton. |
| `Animations/AS_Chuck_<Clip>.fbx` | One clip per FBX, armature only, baked every frame at 30 fps. |
| `Animations/manifest.json` | Per clip: frame range, duration, loop flag, stance intervals, events (seconds), peak leg reach. Walk: reference speed. Start/stop: speed profile and per-frame capsule travel. Turns: per-frame capsule yaw. |
| `rig_v1_metadata.json` | Bone and material counts, bounds, measured sole markers (source and Unreal component space), FBX settings, rest matrices for every bone. |
| `check_v1.py` / `review_v1.py` | Contract checks and pose evidence (see below). |
| `Review/` | Neutral views, pose studies, `strip_<Clip>.jpg` frame strips, `walk_contact_report.json`, `stance_drift_report.json`. |

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

All clips are 30 fps with `root` static (root motion off); the runtime moves or turns the capsule. Loops have no duplicate endpoint sample. For one-shots, `duration_s` is the time of the last frame.

| Clip | Frames / s | Loop | Content |
| --- | --- | --- | --- |
| Idle | 60 / 2.0 | yes | Breathing through spine/chest/neck, small head/ear/tail motion, paws planted. |
| WalkLoop | 9 / 0.30 | yes | 95 cm/s reference; stride cycle 28.5 cm; stance 0.6. `foot_L` stance 0–0.18 s, `foot_R` 0.15–0.30 s and 0–0.03 s. Toe roll over the last 30% of stance; 2.4 cm swing lift. |
| WalkStart | 13 / 0.40 | no | Smoothstep 0→95 cm/s (`capsule_travel_cm_per_frame`, 19 cm). R steps first; ends **exactly** on WalkLoop frame 0. |
| WalkStop | 16 / 0.50 | no | Starts **exactly** on WalkLoop frame 0; smoothstep 95→0 cm/s over 0.4 s (19 cm), then a 0.1 s settle into the neutral stance. |
| TurnLeft90 / TurnRight90 | 21 / 0.67 | no | Turn in place. `capsule_yaw_deg_per_frame` rises 0→±90° (smoothstep 0.02–0.5 s). Inside paw steps twice, outside once; paws turn with their steps; head/chest lead. The sign is source +Z (toward +Y = runtime left); verify after import. |
| JumpStart | 9 / 0.27 | no | Anticipation crouch (bottom 0.16 s), then extension onto the toes; `takeoff` event at 0.25 s. |
| JumpLoop | 12 / 0.40 | yes | Airborne: tucked paws with toes angled for landing, balancing arms/tail. |
| JumpLand | 13 / 0.40 | no | `contact` at frame 0; pelvis absorbs 4.5 cm (max at 0.09 s) and recovers to neutral by 0.4 s. |

Peak thigh+calf reach ratio is at most 0.892 in every clip, so no leg is hyperextended. `ik_foot_*` follow the solved hock and `ik_hand_*` the posed wrist in every frame; `socket_cigarette` rides the jaw. `check_v1.py` confirms the start/stop seams match WalkLoop frame 0 to 0.000 cm / 0.00°.

**Measured on the deformed mesh** (`Review/stance_drift_report.json`): world-space drift of the mid-toe sole under each planted paw, using the manifest's capsule travel/yaw.

| Clip | Max drift (cm/s) |
| --- | --- |
| WalkLoop | 0.60 |
| WalkStart / WalkStop | 1.1 |
| Turns | 1.55 |
| JumpLand | 2.5 |
| JumpStart | 5.3 |

The JumpStart figure comes from the 0.09 s push onto the toes, about 0.5 cm in total. `Review/walk_contact_report.json` also shows the ball-of-paw sole moving at 95.0 cm/s through WalkLoop stance, 0.03–0.05 cm above the ground. The mid-toe sole is the right contact to measure: in toe roll the ball pad lifts while the toes stay planted.

## Pose evidence (`Review/`)

- Neutral: front, side, rear, three-quarter, scale.
- Pose studies: crouch/curl, forward knee flexion, toe roll, overhead grip, and the cigarette held at the left lip corner with the jaw opened 6°. The cigarette is a temporary render-only stick on the socket, not an asset.
- Clips: `strip_<Clip>.jpg` (side view; turns from above-front).

## Known limits

- **Overhead grip:** reads as a hand at head height rather than a full overhead reach. The clavicle/upper-arm range for climbing needs a follow-up study.
- **Jacket:** a separate shell skinned to the body, with no cloth simulation. The armhole stretches at large raises.
- **Hands:** one curl bone for four fingers, as accepted for v1.
- **Ankle:** the leg/paw seam at the ankle is covered by the heel mound, not merged topology.
- **Not tested:** no UVs or LODs; no Unreal import.
