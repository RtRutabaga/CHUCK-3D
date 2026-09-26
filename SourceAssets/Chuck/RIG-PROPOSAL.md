# Chuck production rig — proposal for agreement (not implemented)

Status: **proposal only**, written by Claude (character art) on 2026-09-26 for Codex (runtime) and the integration owner. It is step 3 of docs/CHARACTER-PLAN.md and answers the "next coordinated rig milestone" in docs/agent-handoffs/CODEX.md. Nothing here changes the exported assets or the current 14-bone contract. Both owners should agree before either side depends on it.

- Bone table: [`rig_proposal.json`](rig_proposal.json), generated from `PROPOSAL` in [`rig_proposal.py`](rig_proposal.py), the single source of truth.
- Overlay renders: `Review/rig_proposal/rig_{front,side,three_quarter,head_side,leg_side}.jpg`. The current body is ghosted; orange marks deforming joints, cyan marks helpers.
- Reproduce: `blender --background SourceAssets/Chuck/Chuck.blend --python SourceAssets/Chuck/rig_proposal.py -- <out_dir>`

## Why change

The temporary rig (root, head, thigh/shin, arm/forearm ×2, tail_0..3) cannot support the eventual actions:

- There is no pelvis or spine, so roll, lean and the pelvis contact correction are faked on `root`.
- The knee sits *behind* the hip, the reverse of rat anatomy.
- The feet are separate static meshes, so the ankle can open and there is no toe roll.
- There are no hands for climbing and no jaw or mouth socket for the cigarette.

## Proposed skeleton (41 bones, 35 deforming)

Centimetres, Z up, nose +X, feet at Z=0, ear top 65 cm. Blender +Y is the runtime `_L` side (FBX reflects Y), unchanged from today.

| Chain | Bones (parent →) | Notes |
| --- | --- | --- |
| Core | `root` → `pelvis` (-2,0,19.5) → `spine_01` → `spine_02` → `chest` → `neck` → `head` (0.8,0,47.5) | `root` stays at ground under the capsule. Pelvis carries height/tilt correction. The `head` name is kept, but its pivot moves up from (0,0,46) to the top of the neck. |
| Face | `head` → `jaw` (2,0,49.6); `head` → `ear_L/R` | Jaw hinge under the cheek, for restrained lip/jaw motion only. Ears for subtle secondary motion. |
| Arms | `chest` → `clavicle_L` → `upperarm_L` (0,10.4,41) → `lowerarm_L` (-1,14,29) → `hand_L` (3,14,22) → `fingers_L`, `thumb_L` | The upper/lower arm heads match today's `arm_L`/`forearm_L`, so current arm swing maps directly. One finger curl bone plus a thumb is enough for climb grips. |
| Legs | `pelvis` → `thigh_L` (-2,6,19.5) → `calf_L` knee (1.5,6.8,11.5) → `foot_L` hock (-3.2,7,3.4) → `toes_L` ball (3.4,7,1.0) | Plantigrade rat hind leg: knee forward, hock back, heel down in stance, toe roll at push-off. **This replaces the reversed knee.** |
| Tail | `pelvis` → `tail_0..tail_5` | Six bones instead of four, following the current tail curve. |
| Helpers (non-deforming) | `ik_foot_L/R`, `ik_hand_L/R` under `root`; `socket_cigarette` under `jaw` | IK goals for the AnimBP or Control Rig. The socket head is the filter at the left lip corner (12.6,2.5,49.3); its tail is the lit end and smoke origin. |

`_R` bones mirror `_L` with Y negated.

## Mesh and asset changes that come with it

1. **One deforming mesh.** The paws become part of the skinned body, and `SM_ChuckFoot` plus the two static-mesh foot components are retired. Codex's world-locked stance and swing then drive `ik_foot_*` instead of moving foot components. This is the biggest runtime change; the contact logic itself carries over.
2. **Legs remodelled** along the new chain (knee forward), reusing the `chain_tube` construction and graded weights. The current legs follow the old chain and would be replaced.
3. **Graded weights** everywhere, as the jacket, sleeves and legs already have. The jacket keeps its garment field, rebased onto `chest`, `spine_*`, `upperarm_*` and `thigh_*`.
4. **New asset path, side by side.** Import as `/Game/Characters/Chuck/SK_Chuck` with a new skeleton, next to the current `SK_ChuckBody`. Switch the runtime in one commit, then delete the old assets after verification, so main never has half a rig.
5. Material slot names stay the same (Fur, Chest, Jacket, Seam, Skin, Eye, Claw, Metal, Whisker). Claw moves into the body, since the paws are no longer separate.

## Animation approach (for discussion)

- Authored clips in an Animation Blueprint: idle, walk start/loop/stop, turn-in-place, run, jump start/loop/land, side-jump L/R, roll, climb reach/pull-up, and a restrained smoke idle (jaw/lip plus a small head tilt, cigarette held in the mouth, smoke from the socket tail).
- Keep capsule-driven locomotion. Add foot IK with Codex's contact locking on top of the clips, plus pelvis height correction.
- Root motion: not for walk/run; evaluate it explicitly for roll and climb only.
- Clip authoring would be Blender actions exported with the skeleton. Who authors which clips is a question for the user.

## Questions for Codex / integration

1. **IK tooling.** Two-bone IK in the Animation Blueprint needs no plugin. Control Rig ships with UE 5.7 but would need enabling in `Chuck3D.uproject`, which currently enables only PythonScriptPlugin. Is that acceptable, or should IK stay in C++/AnimBP nodes?
2. **Replacing PoseableMeshComponent.** Can it be swapped for a SkeletalMeshComponent plus an AnimBP in the same commit as the asset switch? Can the contact telemetry and `Verify-Package` checks be re-pointed at `ik_foot_*` / `foot_*` bones?
3. **Bone axes.** Is Blender's bone-Y-down-the-chain, with default FBX primary Y / secondary X, fine for your solver? If not, name the convention you want.
4. **Pelvis height.** At rest the pelvis sits at 19.5 cm. Your reach checks use hip (-2,±6,21) today; thigh heads move to 19.5. The new chain is thigh 8.77 + calf 9.37 cm to the hock, then foot 7.02 cm to the ball (the current thigh + shin reach is about 10.3 + 8.5 cm).
5. **Sockets.** Any other sockets needed, e.g. a hand-hold socket for climbing or a camera anchor on `head`?

## Proposed order once agreed

1. Codex confirms or amends this table. The final table is committed first, as its own commit.
2. Claude builds the rig, rebuilds the legs and paws on it, skins the whole mesh, and delivers new FBXs under new asset names with pose-test evidence. The current assets stay untouched.
3. Codex builds the AnimBP/IK against the new skeleton in its branch, using a simple test clip set.
4. The integration owner imports both side by side, switches the runtime in one commit, re-runs `Verify-Package.ps1 -MotionCapture`, and only then deletes the old assets.
