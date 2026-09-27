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
| `Textures/T_Chuck_{BaseColor,Normal,ORM}.png` | Baked 2048² surface textures for the shared `UVMap` (see Textures). |
| `bake_textures.py` / `preview_textured.py` | Texture bake from procedural 3D shaders; lit EEVEE preview using only the baked maps (`--groom` previews the groom variant). |
| `SK_Chuck_Groomed.fbx` | The same skinned mesh, skeleton, UVs and materials **without** the geometric fur tufts, for use with the strand groom. |
| `Groom/GR_Chuck.abc`, `Groom/groom_metadata.json`, `build_groom.py` | Strand fur groom for Unreal Groom (see Groom). |
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

- One skinned mesh, 208,614 triangles (170,544 for the groom variant); v1.1 proportions (`Tools/chuck_v1_shape.py`, docs/RIG-CONTRACT-V1.md) (whiskers 0.05 cm radius so they don't alias into dotted lines in-engine), one UV channel `UVMap`, nine material slots (Fur, Chest, Jacket, Seam, Skin, Eye, Claw, Metal, Whisker). Ear top at exactly 65 cm; paw soles at Z = -0.002…0.01 cm.
- **Legs:** rebuilt knee-forward along thigh → calf → hock. Their domed ends sink into the paw heel mound.
- **Paws:** now part of the body (the legacy `paw_parts` at origin (-0.8, ±7, 1.96)).
- **Continuity:** leg and paw are overlapping surfaces, not one merged manifold. The paw, heel mound included, is rigid on foot/toes. The ankle bend happens on the fur leg tube, which grades calf → foot over about 1.5 cm around the hock. The tube's end sits inside the heel mound and is fully foot-weighted, so the join stays closed under toe roll, paw lift and crouch (`Review/ankle_join.jpg`).
- **Weights:** graded fields per source part in `build_chuck_v1.py`.
  - Spine chain for the torso, with clavicle share at the shoulders and thigh share at the hips.
  - Jacket shell follows the upper arm near the armhole and lifts with the thighs at the hem.
  - Sleeves: clavicle/chest → upper arm → lower arm. Hands: hand → fingers/thumb.
  - Head, with a neck blend at the back, jaw on the chin/lower lip/lower muzzle, and ears.
  - Legs: pelvis → thigh → calf, then calf → foot around the hock. Paws are rigid on foot/toes. Tail: tail_0 → tail_5.
  - Every deforming bone carries weight; helpers carry none. At most 4 influences per vertex. No `_L`/`_R` cross-weighting.

## UVs

`UVMap` is an automatic Smart UV Project (66°, margin 0.003) of the real surfaces, scaled into v ≤ 0.985 (`Review/uv_checker.jpg`). The top strip (v > 0.985) holds one tiny flat island per material for sub-texel geometry: fur tufts, whiskers and zipper teeth (38,211 faces). Real surfaces are about 10,080 cm² at **12.8 px/cm with 2048² maps** (0.8 mm per texel), or 25.7 px/cm at 4096². The seams are automatic, not hand-placed.

## Textures

Rebuild after any mesh change:

```powershell
& $B --background SourceAssets\Chuck\V1\Chuck_V1.blend --python SourceAssets\Chuck\V1\bake_textures.py -- 2048
& $B --background SourceAssets\Chuck\V1\Chuck_V1.blend --python SourceAssets\Chuck\V1\preview_textured.py -- <out_dir>
```

`bake_textures.py` gives each of the nine materials a procedural shader in **object space** (centimetres), so the automatic UV seams do not break the pattern. It then bakes with Cycles:

| Map | Contents |
| --- | --- |
| `T_Chuck_BaseColor.png` (sRGB) | Albedo, baked with metallic off because Cycles returns no diffuse colour for metals. Each parked strand island (tufts, whiskers, zipper teeth) is filled with its material's mean surface colour, measured through an exact material-ID bake, or with a defined flat colour for whiskers and zipper, which have no surface. |
| `T_Chuck_Normal.png` (linear) | Tangent space, **DirectX convention (green flipped) for Unreal**. |
| `T_Chuck_ORM.png` (linear) | R = ambient occlusion (forced to 1 on the parked-island strip), G = roughness, B = metallic. |

Surface design, following `References/ArtDirection`:
- **Jacket:** worn red-violet suede/brushed canvas (2026-09-27 turnaround; more saturated than the first study's blue-purple, still below the neon of the first Unreal review). Cropped: the hem sits at 23 cm, so the hips and thighs show. Blotchy dye variation; sun-faded raised folds and edges (pointiness); grime toward the hem; crumple wrinkles, diagonal twill and fibre grain in the normal.
- **Lining/stitching:** darker purple with fine grain.
- **Fur:** warm taupe-brown (2026-09-27 turnaround), darker along the back, with vertically stretched streaks following the hair.
- **Chest:** beige cream with streaks.
- **Skin (ears, nose, hands, paws, tail):** pink-brown mottling, ring scales on the tail, darker sole pads, and a little subsurface in the preview only.
- **Eyes:** glossy near-black (roughness 0.06).
- **Claws:** horn with streaks.
- **Zipper:** worn nickel/steel (metallic).
- **Whiskers:** pale.

`preview_textured.py` rebuilds every material in memory from the three maps alone, the way an Unreal material would sample them, and renders warm-daylight views (`Review/textured_*.jpg`).

**Unreal wiring (done by the integration owner in `M_Chuck_V1`):** BaseColor as Base Color, ORM.R as Ambient Occlusion (not multiplied into albedo), ORM.G roughness, ORM.B metallic, and the DirectX normal with no extra flip. It is assigned to all nine `SK_Chuck` slots. After any rebake, re-run `Tools/Import-ChuckV1.ps1`. The geometric fur tufts are separate geometry and take their flat colour from the parked islands.

## Groom

Unreal strand fur, requested by the user in place of the spiky geometric tufts (`Review/groomed_*.jpg`).

```powershell
& $B --background SourceAssets\Chuck\V1\Chuck_V1.blend --python SourceAssets\Chuck\V1\build_groom.py
```

- **Where the fur grows:** strands grow on `SK_Chuck_Groomed` **in the rest pose** (the script forces the armature to rest), on exposed Fur and Chest (cream) surfaces. Surfaces covered by the jacket, sleeves or chest patch get none; a short ray along the normal must be clear. Eyelids and the cupped ears stay bare.
- **Strands:** 68,000 in total, 6 points each, 75 µm root / 15 µm tip width. The coat is shaggy and clumped, after the 2026-09-27 turnaround:
  - Lengths: body 0.9–1.6 cm, back 1.0–1.8 cm, cream 0.6–1.15 cm, scaled by region (muzzle ×0.45, crown ×1.2 and more lifted for a spiky look, cheeks and nape ×0.9).
  - Strands leave the skin at about 20–45° with slight frizz. Their tips converge on the nearest of one guide strand per 14 (`clumping` in the metadata).
  - Flow is toward the tail on the head and muzzle, down and back on the body, and down on the legs and chest. The head has 2.2× density.
- **Groups** (one Alembic curves object each; Blender's Alembic export drops custom per-strand attributes such as `groom_color`, so colour is per group): `Fur_Body` 30,000, `Fur_Back` 16,000 (back, crown and upper snout, darker), `Fur_Cream` 22,000 (chest, cheeks, chin). Suggested linear colours are in `groom_metadata.json`.
- **Coordinates:** source cm, Z-up. The Alembic is written Y-up (x, z, −y), so the Unreal groom import conversion must be set so it lands on `SK_Chuck_Groomed`. `check_v1.py` verifies the roots lie 0.020 cm under that mesh's rest surface (in source space).
- **Texture bake:** textures are baked from the tuft-free mesh, so no tuft AO dots show under the groom. The parked tuft islands get flat material colour, roughness, normal and metallic, so the maps also stay correct for `SK_Chuck` (the no-groom fallback).

**Unreal side (Codex / integration; not done here):**
1. Enable the engine's Groom (HairStrands) plugin. This is a `Chuck3D.uproject` plugin-list change that the rig contract had deferred; the user approved using groom on 2026-09-26.
2. Import `SK_Chuck_Groomed.fbx` onto the existing `SK_Chuck_Skeleton`.
3. Import `GR_Chuck.abc` as a Groom asset with the conversion set to match; create a Groom Binding to `SK_Chuck_Groomed`.
4. Add three hair materials (`Hair` shading model) using the suggested colours.
5. Attach a GroomComponent to the skeletal mesh component, with the binding.
6. Profile on this PC's 8 GB GPU: strand count, LOD or hair cards, and both cameras. Keep `SK_Chuck` without groom as the fallback.

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

**Measured on the deformed mesh** (`Review/stance_drift_report.json`): world-space drift of the mid-toe sole under each planted paw, using the manifest's capsule travel/yaw. The maximum is 0.002 cm/s in every clip; toes stay flat and planted through toe roll. `Review/walk_contact_report.json` tracks the ball pad instead. It moves at exactly 95.0 cm/s during flat stance and lifts during toe roll (up to 4.5 cm/s on the roll frame), which is intended.

**Ground:** `check_v1.py` checks every frame of every clip, and no deformed vertex goes below Z = -0.002 cm (the resting sole). A tail ground-clearance pass (`Poser.clear_ground`) lifts any tail joint that would dip, allowing for the tail's radius plus 0.9 cm margin.

## Pose evidence (`Review/`)

- Neutral: front, side, rear, three-quarter, scale.
- Pose studies: crouch/curl, forward knee flexion, toe roll, overhead grip (two-bone arm IK `Poser.arm`: wrists at 62.5 cm height (ear top 65 cm), elbows bent outward, reach 0.93, clavicles shrugged, fingers/thumbs curled), and the cigarette held at the left lip corner with the jaw opened 6°. The cigarette is a temporary render-only stick on the socket, not an asset.
- Clips: `strip_<Clip>.jpg` (side view; turns from above-front).

## Known limits

- **Overhead grip:** reaches above the head, but the jacket armhole and hem stretch visibly in that pose (no cloth simulation).
- **Jacket:** a separate shell skinned to the body, with no cloth simulation. The armhole stretches at large raises.
- **Hands:** one curl bone for four fingers, as accepted for v1.
- **Ankle:** the leg/paw seam at the ankle is covered by the heel mound, not merged topology.
- **Not done:** no LODs (the Unreal importer can generate them); no Unreal import. The UV seams are automatic.

## Cigarette

Every goal image shows the cigarette held in the left mouth corner with a thin smoke wisp. It is a **separate prop**, not part of the skinned body, so a future pickup design can show or hide it (the `-ChuckNoCigarette` launch flag removes it).

```powershell
& $B --background --factory-startup --python SourceAssets\Chuck\V1\build_cigarette.py
```

- **Assets:** `Cigarette/SM_Cigarette.fbx` is 7 cm, radius 0.3 cm, filter end at the origin, lit end at +X. Slots: Paper, Filter, Ash, Ember. `Cigarette/SM_CigaretteSmoke.fbx` is two crossed ribbons, 16 cm, curling and widening as they rise (slot Smoke). `T_CigaretteSmoke.png` is a vertically tileable wisp mask.
- **Unreal:** `Tools/import_chuck_cigarette.py` runs from `Import-ChuckV1.ps1`.
  - Flat paper, filter and ash materials.
  - A slow 3 s emissive pulse on the ember.
  - Translucent unlit smoke that pans the mask upward and fades at the base, the top and the edges.
  - Existing materials are kept (delete an asset to rebuild its graph).
- **Runtime:** `AChuckCharacter` attaches the prop to `socket_cigarette`. Its +X follows the exported bones' local axis, read from the imported rest pose, and the smoke sits at the mesh's lit end, upright in world space. The packaged check `cigarette held in the left mouth corner with upright smoke` measures aim, mouth distance and smoke orientation.
- **Review:** `review_chuck_v1_unreal.py` shows the prop and adds a close `face` view (`Review/unreal_groom_face.jpg`, `Review/goal_compare_cigarette.jpg`).

