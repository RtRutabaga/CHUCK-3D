# Agent handoff — Claude Code, jacket continuity and neutral chest

## Current instruction for Claude — sequential main workflow

The user now runs only one agent at a time and asks Claude to start working directly on `main` in `C:\Users\ashsm\OneDrive\Documents\GameDev\CHUCK-3D`. Read the v3 workflow in `docs/AGENT-WORKFLOW.md` and the latest `docs/HANDOFF.md` before resuming. Inspect status/branch and preserve unfinished work; do not reset or copy an old worktree over main. If this Claude session is pinned to its old worktree, reopen the root project before implementation. Existing worktrees have not been deleted or reset.

Keep character/traversal ownership, but commit completed work on main and use the root `Launch-Prototype.cmd` as the single player-facing launcher. A playable milestone includes building, verifying and updating that launcher's package and receipt, not just committing source. Update the handoff and publish routine verified changes under existing user permission. The worktree-only instructions in the historical deliveries below are superseded.

- **Owner, task, branch, worktree:** Claude Code (Opus 5.5, Claude desktop app), `docs/agent-tasks/CLAUDE-CHARACTER.md` first bounded assignment. Authored in the app's own worktree `.claude/worktrees/project-orientation-fd7504` on branch `claude/project-orientation-fd7504`, because the app blocks edits into another worktree. `codex/claude-character` was then fast-forwarded to the same commit. `Local/AgentWorktrees/claude-character` was not edited.
- **Base commit and delivered commit(s):** base `d10eba4`. Delivered: the single commit on `codex/claude-character` directly after `d10eba4` ("Rebuild Chuck jacket as one continuous garment").
- **Changed paths; any changes outside initial ownership:** `Tools/build_chuck_model.py`, `SourceAssets/Chuck/{Chuck.blend, SK_ChuckBody.fbx, SM_ChuckBody.fbx, SM_ChuckFoot.fbx, README.md}`, new `SourceAssets/Chuck/{check_model.py, review_renders.py, Review/}`, and this file. No changes outside the owned paths. No Unreal assets or runtime code touched.
- **Summary and comparison with user references:**
  - The jacket shell, collar stand, collar fold and both lapels are now one continuous grid driven by a single front-edge function. Solidify gives 0.4 cm thickness; its rim closes every edge; the inner face is the lining (`Seam` slot).
  - The front edges carry a zipper (teeth `Metal`, tape `Seam`), as in `Chuck-Standing.jpg`. Welt pockets, hem and back stitching sit on the same surface function.
  - Removed: floating lapel quads, `FrontSeam` tubes, fasteners and shoulder seams that hovered off the shell (see `Review/before/close_edge_right.jpg`).
  - Chest: the separate protruding ellipsoid (hard rim, dark torso visible in the opening) is replaced by a cream patch ray-cast onto the evaluated torso, sunk at its border. The opening now shows cream chest top to bottom, like the references.
  - Sleeve folds reduced (less "puffer" banding).
  - Still well short of the references: head, ears, hands, legs and feet are the old primitives. Sleeves remain tubes overlapping the shell at the shoulder.
- **Rig/rest-pose/scale/material/asset-path contract changes:** none. Same 14 bones, parents and head positions (verified). Ear top 65.000 cm. Same material slot names (the jacket now also uses `Metal` for zipper teeth, an existing slot). Same asset names and export settings. All new jacket parts are weighted 100% to `root`, as the old jacket was.
- **Source of truth; generator versus manual edits; safe reproduce/export/import steps:**
  - `Tools/build_chuck_model.py` is the source of truth; `Chuck.blend` and the FBXs are regenerated from it with no manual edits. Git history shows the .blend has only ever changed together with the generator.
  - Reproduce: `blender --background --python Tools/build_chuck_model.py` (writes `SourceAssets/Chuck/*`).
  - Unreal import by the integration owner: `Tools/Build-ChuckAssets.ps1` (runs Blender, then both UE imports). Not run here.
- **Exact tool versions and commands actually run:**
  - Blender 4.5.14 LTS (`%LOCALAPPDATA%\Programs\CHUCK-Tools\blender-4.5.14-windows-x64\blender.exe`).
  - Commands: the generator; `SourceAssets/Chuck/check_model.py` on the old and new .blend; `SourceAssets/Chuck/review_renders.py` on the old and new .blend.
  - Pillow 12.3.0 for PNG→JPG evidence conversion.
- **Tests/measurements passed and failed; evidence paths:**
  - `check_model.py` on the new .blend: all PASS. Bones/parents, 65 cm, slot names, bone-only vertex groups, 0 vertices with weight total ≠ 1, 0 open jacket-shell edges.
  - The open-edge check also passes on the old model, so it does not catch between-part gaps; the renders are the evidence for that.
  - Triangles: body 131,194 → 161,334 (+23%), foot 10,944 unchanged. Body bounds X −51.06..18.9, Y ±19.0, Z 1.62..65.0.
  - Evidence: `SourceAssets/Chuck/Review/after/*.jpg` (front, side, rear, three_quarter, scale_side, close_front_opening, close_edge_left/right, close_collar, close_elbow, close_edge_underside) and `Review/before/*.jpg`.
- **Visual/motion review findings; known gaps; what was not tested:**
  - Not tested: Unreal import, packaging, `Verify-Package.ps1`, any motion or posed bend. The jacket is rigid on `root`, so arm swing will still intersect sleeves with the shell at the shoulder.
  - Remaining flaws: sleeve–shell join is an overlap, not an armhole. Hands are blobs. Legs are separate ellipsoids, detached from feet in the neutral review pose (runtime places the feet). Feet are flat discs, and head/ear/nose are not yet close to the references. No UVs or LODs. The Workbench renders show forms only, not Unreal materials.
  - The collar stand sits near the head's underside; check it for clipping when the head bone turns.
- **Binary files/size/LFS status; generated-file exclusion check:** `.blend` (~36 MB) and FBX (~10 MB) remain in LFS via `.gitattributes`. The 15 review JPGs total ~1.1 MB and are LFS-tracked (`*.jpg`). No generated Unreal output is committed.
- **Heavy tools currently running:** none.
- **Integration order and any coordination required:**
  - Cherry-pick the commit onto main, then run `Tools/Build-ChuckAssets.ps1` with Unreal closed, then the normal package and verification.
  - No rig change, so it can integrate independently of Codex's movement work.
  - Proposed next steps, not started: a jacket that deforms with the arms (sleeves joined to the shell through armholes, graded shoulder weights), then the production-rig proposal from `CHARACTER-PLAN.md` step 3 before remodeling legs, feet and hands.

## Second pass — jacket follows the arms

- **Delivered commit:** the commit after "Rebuild Chuck jacket as one continuous garment" on `codex/claude-character` ("Grade Chuck garment weights and join sleeves through the elbow").
- **Changed paths:** `Tools/build_chuck_model.py`, regenerated `SourceAssets/Chuck/{Chuck.blend, *.fbx}`, `SourceAssets/Chuck/{README.md, check_model.py}`, new `SourceAssets/Chuck/pose_test.py` and `Review/poses/`, updated `Review/after/`. Nothing outside the owned paths.
- **What changed:**
  - Weights: jacket shell, sleeves and stitched details share one spatial field. Sleeves grade root → `arm_*` → `forearm_*`. Armhole shell cloth takes `arm_*` weight, fading out below the top of the upper arm and away from the arm axis, and never takes forearm weight.
  - Sleeves: the two overlapping tubes (`Sleeve` + `SleeveLower`) are now one tube bending through the elbow.
  - Neutral form: torso and jacket sides slimmed about 1 cm per side, sleeves thinned.
- **Contract:** unchanged. Same 14 bones, parents, head positions, 65.000 cm ear top, slot names and asset names. The `SleeveLower` source part name no longer exists, but no runtime code depended on it. What changes is the weight distribution on `arm_*`/`forearm_*`/`root`: garment vertices near the arms now move with them.
- **Measurements:**
  - `check_model.py`: all PASS, including 2,130 (L) / 2,115 (R) vertices with graded arm/forearm weights and 0 weight-total errors.
  - Body triangles 159,524; foot 10,944.
- **Pose evidence** (Blender Workbench, rigid 9e337c3 weights left, graded right): `SourceAssets/Chuck/Review/poses/*_rigid_vs_graded.jpg`.
  - Shoulder: at 35° swing, 80° reach and 45° side raise, the sleeve top no longer cuts into or separates from the shell.
  - Elbow: the pinch between the two sleeve tubes is gone.
- **Not tested / remaining flaws:**
  - No Unreal import or in-game motion. The runtime currently only swings about ±7°, so the in-game difference will be subtle.
  - At large raises the armhole cloth stretches as a smooth web; there is no real armhole seam topology or cloth simulation.
  - Collar/head: a 30° head turn showed no visible collar clipping in the front view. No close-up or pitch test was done.
  - Legs, feet, hands and head are still primitives.
- **Integration:** as before. Cherry-pick both commits in order, then run `Tools/Build-ChuckAssets.ps1`. Codex's current arm-swing code needs no change.

## Third pass — continuous limbs, paws and shoulders (after main 9b9bd46 integration feedback)

- **Input:** main `9b9bd46` and `docs/agent-handoffs/CODEX.md`. Packaged front/rear views showed flat sleeve caps above the shoulders and primitive legs, hands and paws. The Codex leg IK chain and foot origin were read from `ChuckCharacter.cpp` on main and kept exactly.
- **Delivered commit:** the third commit on `codex/claude-character` ("Build continuous legs, paws, hands and domed shoulders"). It is based on `939580a`. `Tools/build_chuck_model.py` on main equals `939580a`, so it should cherry-pick cleanly onto main.
- **Changed paths:** `Tools/build_chuck_model.py`; regenerated `SourceAssets/Chuck/{Chuck.blend, SK_ChuckBody.fbx, SM_ChuckBody.fbx, SM_ChuckFoot.fbx}`; `SourceAssets/Chuck/{README.md, check_model.py, review_renders.py, pose_test.py}`; `Review/after` (regenerated), new `Review/limbs`; this file. Nothing outside the owned paths.
- **What changed:**
  - `chain_tube`: a continuous tube with blended joint frames and domed ends. It replaces the sleeve builder and the separate thigh/shin ellipsoids.
  - Legs: one tube per side along the IK chain, tucked into the heel. Graded root → thigh → shin weights.
  - Lower jacket: blends toward both thighs by distance, so the hem lifts over the haunch.
  - Shoulders: wider dropped shoulders cover domed sleeve heads (no flat caps).
  - Paw: longer foot with heel, five toes and claws. Hands: palm, four curled fingers and a thumb.
- **Contract:** unchanged. Same 14 bones, parents and heads, ear top 65.000 cm, slot names, asset paths and FBX settings. `SM_ChuckFoot` keeps slots Skin/Claw, origin convention and sole near local z=-2 (measured -1.96 vs -2.00 before). The ankle point (-2,0,2.5) sits inside the heel mound. Foot X extent is now -5.9..9.14.
  - New weights: garment vertices now also carry `thigh_*`; legs blend `root`/`thigh_*`/`shin_*`.
  - Source part names `Thigh`/`Shin`/`ThighFur`/`ShinFur` became `Leg`/`LegFur`; no runtime reference was found.
- **Measurements:**
  - `check_model.py`: all PASS (now including the foot sole check).
  - Body 165,625 triangles (+6,101). Foot 17,120 (+6,176 each, ×2 in game).
- **Evidence:** `SourceAssets/Chuck/Review/limbs/*_prev_vs_limbs.jpg`, with the previous delivery on the left.
  - The previous neutral images used approximate foot placement; the new ones use runtime placement.
  - Stride (thigh ±30/20°, knee 25°) and crouch (45°/60°) show a continuous leg, with the hem lifting instead of being pierced.
  - Arm poses remain as in pass two.
- **Not tested / remaining:**
  - No Unreal import or packaged motion capture. The integration owner should re-run `Build-ChuckAssets.ps1` and `Verify-Package.ps1 -MotionCapture`, and watch the ankle/heel join during Codex's stance/swing and turns.
  - Paws still rotate separately from the shin, so extreme foot pitch could open the ankle join.
  - The IK "knee" sits behind the hip, which is the temporary rig's shape, not rat anatomy. A true hock/toe chain needs the coordinated rig milestone.
  - Head and face unchanged. No UVs or LODs.

## Fourth pass — styling toward the references (while Codex is paused)

- **Delivered commit:** the fourth commit on `codex/claude-character` ("Restyle Chuck face, ears, cuffs and yoke toward the references"), on top of `875f579`. It should cherry-pick onto main after `875f579`.
- **Changed paths:** `Tools/build_chuck_model.py`; regenerated `SourceAssets/Chuck/{Chuck.blend, *.fbx}`; `SourceAssets/Chuck/{README.md, review_renders.py}` (head close-ups added); `Review/after` regenerated; new `Review/style`; this file.
- **What changed:**
  - Face: brows removed for a restrained expression; eyelids flatter; cream chin/cheek patch conformed to the head replaces the lip ellipsoid; mouth line on the head surface; slight nose droop.
  - Ears: thin cupped pink ears, still exactly 65 cm at the top.
  - Jacket: fabric band cuffs, slimmer sleeves, front/back yoke stitching.
  - Hands: flatter palm.
- **Contract:** unchanged. Same bones, 65.000 cm ear top, material slot names, asset paths and foot. Head parts stay 100% on `head`. Cuffs keep the graded sleeve weights. The `Brow` and `EarInner` source parts no longer exist; no runtime reference was found.
- **Measurements:** `check_model.py` all PASS. Body 172,648 triangles (+7,023). Foot unchanged at 17,120.
- **Pose evidence:** the 30° head turn, 45° side raise and 35° swing still hold together (`Review/style/*_prev_vs_style.jpg`).
- **Build note:** one generator run failed with `OSError: [Errno 22]` while writing `SM_ChuckBody.fbx`. It looked like a temporary file lock (OneDrive sync); an immediate re-run succeeded. If it recurs during import, re-run the generator.
- **Not tested / remaining:**
  - No Unreal import.
  - Runtime materials override slot colours, so the in-game cream/pink balance must be checked after import.
  - Still missing versus the references: fur groom quality, eye highlights/wetness, whisker pads, woven/worn jacket surface, and the cigarette/mouth socket (planned rig work).

## Fifth pass — production rig proposal (for agreement, not implemented)

- **Delivered commit:** the fifth commit on `codex/claude-character` ("Propose Chuck production rig for Codex agreement"). It contains text, script and review JPGs only. No generator change, no FBX/.blend change, no contract change.
- **Files:**
  - `SourceAssets/Chuck/RIG-PROPOSAL.md`: rationale, skeleton, mesh/asset migration, animation approach, questions, order.
  - `SourceAssets/Chuck/rig_proposal.py`: the bone table as the single source; builds the armature beside the current model and renders overlays; never saves.
  - `SourceAssets/Chuck/rig_proposal.json`: the generated table.
  - `SourceAssets/Chuck/Review/rig_proposal/*.jpg`: overlay renders.
- **Proposal summary:** 41 bones (35 deforming).
  - Core: pelvis/spine/chest/neck; `head` kept but re-pivoted.
  - Face: jaw and ears.
  - Arms: clavicle/upperarm/lowerarm/hand/fingers/thumb. Arm heads match today's `arm_*`/`forearm_*`.
  - Legs: plantigrade thigh/calf/foot/toes with the knee forward.
  - Tail: six bones.
  - Helpers: `ik_foot_*`, `ik_hand_*`, and `socket_cigarette` at the left lip corner.
  - Assets: paws merge into one skinned body (`SM_ChuckFoot` retired); new asset path imported side by side before a single runtime switch.
- **Needs from Codex:** answers to the five questions in `RIG-PROPOSAL.md` (IK tooling / Control Rig enablement, PoseableMesh → AnimBP swap, bone axes, pelvis height and reach, extra sockets). Claude will not build the new rig until the table is agreed.

## Sixth pass — rig v1 first delivery (rig/skin, pose evidence, Idle + WalkLoop)

- **Input:** Codex's acceptance in `docs/RIG-CONTRACT-V1.md` (uncommitted in the main checkout when read; `Tools/Check-RigContract.py` there passes on this branch's unchanged table: 41/35, forward poles, 8.7687/9.3670 cm).
- **Delivered commit:** the sixth commit on `codex/claude-character` ("Deliver Chuck rig v1 skin, pose evidence, Idle and WalkLoop"). It builds on the latest art (`875f579`, `dc52106`), which are still not integrated.
- **Changed paths:**
  - New `Tools/build_chuck_v1.py` (the separate v1 entry point) and `Tools/chuck_v1_pose.py` (posing/IK helpers).
  - New `SourceAssets/Chuck/V1/**`: `Chuck_V1.blend`, `SK_Chuck.fbx`, `Animations/AS_Chuck_{Idle,WalkLoop}.fbx`, `Animations/manifest.json`, `rig_v1_metadata.json`, `check_v1.py`, `review_v1.py`, `README.md`, `Review/`.
  - `Tools/build_chuck_model.py`: behaviour-preserving only. The paw construction is now `paw_parts()`, and the export tail is guarded by `CHUCK_GEOMETRY_ONLY`. Legacy output was verified identical by vertex/weight fingerprint, and the legacy binaries were not regenerated.
- **Contract compliance** (`V1/check_v1.py`, all PASS):
  - Rig: 41 bones and parents equal the table; rest head/tail error 0; 35 deforming; zero roll; helpers exported with no weights; every deforming bone weighted.
  - Mesh: ear top 65.000 cm; soles at Z≈0; nine material slots including Claw; weight totals 1; max 4 influences; no `_L`/`_R` cross-weighting.
  - Clips: root static and root motion off.
  - FBX re-import: one armature with exactly 41 names, one skinned mesh.
- **Delivery facts:**
  - Export settings, per-bone rest matrices and measured sole markers are in `V1/rig_v1_metadata.json`. Heel -5.541 / ball 3.4 / toe 7.305 cm at y=±7, z=0, versus the proposed -4.5 / 3.4 / 8.2.
  - Clip data is in `V1/Animations/manifest.json`. WalkLoop is 9 frames (0.30 s) at 95 cm/s; stance `foot_L` 0–0.18 s, `foot_R` 0.15–0.30 s and 0–0.03 s.
- **Measured on the deformed mesh:** during WalkLoop stance the ball-of-paw sole vertex moves at 95.0 cm/s (world slip ≤ 0.39 cm/s at 95 cm/s capsule speed) and stays 0.03–0.05 cm above the ground.
  - A first build showed 3–8 cm/s slip and 0.7 cm sinking because the ball of the paw took calf weight. The weight rule was fixed (distance falloff) before delivery.
- **Pose evidence (`V1/Review/`):** neutral views, crouch/curl, forward knee flexion, toe roll, overhead grip, cigarette at the left lip corner with the jaw open, WalkLoop frames.
- **Not done / remaining:**
  - Clips not yet delivered: WalkStart, WalkStop, TurnLeft90, TurnRight90, JumpStart, JumpLoop, JumpLand.
  - Overhead grip reaches only about head height.
  - The leg/paw ankle join is overlapping surfaces with shared weights, not merged topology.
  - The jacket stretches at large arm raises.
  - No Unreal import was attempted (integration/Codex own that).
- **LFS:** `.blend` (22 MB) and FBX (6.3 MB + 0.7 MB of clips) are LFS via the existing patterns; review JPGs (~0.7 MB) are LFS; the `.blend1` backup is ignored.

## Seventh pass — rig v1 first clip set complete

- **Delivered commit:** the seventh commit on `codex/claude-character` ("Complete Chuck v1 first clip set"), after `314b3a1`.
- **Added clips:** WalkStart, WalkStop, TurnLeft90, TurnRight90, JumpStart, JumpLoop, JumpLand, joining Idle and WalkLoop. All come from `Tools/build_chuck_v1.py` with a world-space footstep planner, so planted paws stay world-locked while the capsule accelerates, decelerates or turns.
- **Manifest (`V1/Animations/manifest.json`):** stance intervals and events for every clip; speed profiles plus `capsule_travel_cm_per_frame` for start/stop; `capsule_yaw_deg_per_frame` for turns (source +Z; the sign must be verified after import); takeoff/contact/compression events for jumps. `duration_s` for one-shots is now the last-frame time.
- **Checks (`V1/check_v1.py`, 55 PASS):** all earlier contract checks, per-clip frame ranges and static root, and seams (WalkStart end and WalkStop start match WalkLoop frame 0 to 0.000 cm / 0.00°). Peak leg reach is at most 0.892 in all clips.
- **Deformed-mesh stance drift** (`V1/Review/stance_drift_report.json`, mid-toe sole in world space): WalkLoop 0.60, WalkStart/Stop 1.1, turns 1.55, JumpLand 2.5, JumpStart 5.3 cm/s (about 0.5 cm total during the toe push-off).
  - Method note: an earlier measurement on the ball pad overstated turn/start slip at about 5.7 cm/s, because the pad legitimately lifts during toe roll.
  - A trial of pivoting toe roll at ground level made the planted toes slide (about 10 cm/s) and was reverted.
- **Also changed:** `Tools/chuck_v1_pose.py` gained a `heading` parameter (paw turned about Z; knee pole follows half the heading). WalkLoop output is unchanged by it.
- **Not done:** no Unreal import or AnimBP (Codex). Turns are in-place with the body facing forward in component space, so they depend on the runtime applying the yaw profile. The overhead-grip range still needs work.

## Eighth pass — v1 deformation QA while Codex is paused

- **Delivered commit:** the eighth commit on `codex/claude-character` ("Fix v1 paw weights, toe roll, tail ground clearance and overhead reach"), after `6ec0682`. It changes the same V1 files; asset paths and the 41-bone table are unchanged.
- **Found by a new per-frame ground check:**
  - The tail went up to 12.6 cm below the floor in JumpStart, 7.2 cm in JumpLand and 0.4 cm while walking, because it rode down with the pelvis.
  - The back of the heel pad was 100% calf-weighted and swung 0.5 cm under.
  - The toe "droop" at landing contact pushed the toes under.
  - The negative toe pitch during roll lifted the toes (the toes are aimed absolutely).
  - The minimal-rotation aim rolled turned paws, dipping the inner toe.
- **Fixes:**
  - Rigid foot/toes paw weights, with calf only on the heel mound.
  - Toes flat whenever grounded.
  - `Poser.orient`, giving explicit heading-then-pitch paw rotation with no roll.
  - `Poser.clear_ground` for the tail, with armature-space rotation and radius plus 0.9 cm margin.
  - `Poser.arm`, two-bone arm IK, used for the overhead-grip study (wrists at 62.5 cm, elbows bent).
- **Results (`check_v1.py`, 64 PASS):** every clip stays above ground in every frame (min -0.002 cm); seams are still exact; peak leg reach is unchanged. Deformed-mesh mid-toe drift under planted paws is now at most 0.002 cm/s in all clips, down from 0.6–5.3.
- **Still not done:** Unreal import, AnimBP and in-game verification (Codex/integration). No cloth simulation, so the jacket stretches in the overhead pose. The leg/paw ankle is overlapping surfaces with shared weights, not merged topology.

## Ninth pass — v1 ankle continuity and UVs

- **Delivered commit:** the ninth commit on `codex/claude-character` ("Close v1 ankle join and add UVs"), after `6d4c9cd`. Same V1 files and asset paths; the 41-bone table is unchanged.
- **Ankle:** the pose study (toe roll 35°, paw lift, crouch) showed the heel mound splitting and the leg-tube end poking out. The cause was the heel mound's partial calf weight. Now the paw is fully rigid on foot/toes, the leg tube grades calf → foot over about 1.5 cm around the hock, and the tube end inside the heel is fully foot-weighted. Evidence: `SourceAssets/Chuck/V1/Review/ankle_join.jpg`.
- **UVs:** `UVMap`, a Smart UV Project of real surfaces covering about 56% of the square; fur tufts and whiskers are parked on one corner island. A first unwrap that included the tufts packed only 1.2%. Evidence: `Review/uv_checker.jpg`. `check_v1.py` now also checks one UV channel within 0–1: 66 PASS.
- **Measurements unchanged:** ground clearance in every clip; mid-toe stance drift at most 0.002 cm/s; start/stop seams exact; grip reach 0.93.
- **Still not done:** Unreal import/AnimBP/in-game checks (Codex/integration); LODs; hand-authored UV seams; cloth simulation.

## Tenth pass — baked surface textures for v1

- **Delivered commit:** the tenth commit on `codex/claude-character` ("Bake Chuck v1 surface textures"), after `e7299f0`.
- **Files:**
  - New `SourceAssets/Chuck/V1/{bake_textures.py, preview_textured.py}` and `V1/Textures/T_Chuck_{BaseColor,Normal,ORM}.png` (2048², about 8.7 MB, LFS).
  - `Tools/build_chuck_v1.py`: UV parking is now per material, zipper teeth are parked too, and the real islands are scaled into v ≤ 0.985 so the parked strip never overlaps.
  - Regenerated `V1` Blender/FBX/review evidence.
- **What the maps carry:** object-space procedural looks baked to the shared `UVMap`, including worn purple canvas with fading, grime, wrinkles and twill grain, streaked grey-brown fur, cream chest, mottled pink skin with tail rings, glossy eyes, horn claws and nickel zipper. The normal map is DirectX (Unreal); ORM is AO/roughness/metallic. Texel density is 12.8 px/cm at 2048² (25.7 at 4096²).
- **Fixed during review:** zipper teeth rendered black twice. First, self-occluding parked geometry baked AO≈0, so the parked strip's AO is now forced to 1. Second, Cycles bakes no diffuse colour for metallic surfaces, so base colour is baked with metallic off.
- **Evidence:** `V1/Review/textured_{three_quarter,front,rear,close_jacket,close_head,close_paw_tail}.jpg` (EEVEE, maps only).
- **Checks:** `check_v1.py` 66 PASS; stance drift at most 0.002 cm/s; unchanged.
- **Needs integration:** Unreal import of the textures and a textured material for the v1 slots (the runtime `M_Chuck_*` override must select it). Not visible in-game until v1 itself is integrated. The legacy in-game mesh has no UVs and cannot use these maps.

## Eleventh pass — surface fixes from the first Unreal review (main `98f6d61`)

- **Input:** `docs/CHUCK-V1-INTEGRATION.md` and `Local/V1UnrealReview/*.png`. v1 imported side by side and verified; the next runtime gate (AnimBP/contact migration) is Codex's.
- **Delivered commit:** the eleventh commit on `codex/claude-character` ("Fix v1 surface colours seen in the Unreal review"), after `c5d6d25`.
- **Changed:** `SourceAssets/Chuck/V1/bake_textures.py`, `Tools/build_chuck_v1.py`, regenerated `V1` FBX/blend/textures/review, and the V1 README.
- **Fixes:**
  - Jacket: base colour toned down to a deeper, less saturated violet; the engine render read it as neon.
  - Head fur: tufts looked dark and noisy because thousands of differently shaded tufts baked into their few parked texels. Each parked island now gets its material's mean surface colour from an exact material-ID bake (no margin, no pixel filter). Whiskers and zipper, which have no real surface, use defined flat colours (pale, nickel).
  - Whiskers: 0.028 → 0.05 cm radius in v1 only, to stop in-engine dotted aliasing. Legacy output is unchanged.
  - README material note corrected to the integrated wiring: ORM.R as AO, not multiplied into albedo.
- **Checks:** `check_v1.py` 66 PASS; stance drift at most 0.002 cm/s; bones, clips, UV layout and weights unchanged.
- **Integration:** re-run `Tools/Import-ChuckV1.ps1 -Review`. `SK_Chuck.fbx` (whisker geometry) and all three textures changed; the clip FBXs were re-exported from the same actions.
- **Still short of the references:** geometric fur tufts versus a real groom or hair cards, a stylised face, no cloth simulation.

## Twelfth pass — strand groom (user-approved) and rest-pose fix

- **User decision (2026-09-26):** "go ahead and use the groom thing". Unreal Groom replaces the geometric fur tufts; this overrides the earlier "no plugin changes now" default for this one feature.
- **Delivered commit:** the twelfth commit on `codex/claude-character` ("Add Chuck v1 strand groom and tuft-free mesh variant"), after `8363a88`.
- **New:**
  - `SourceAssets/Chuck/V1/{build_groom.py, SK_Chuck_Groomed.fbx}` and `V1/Groom/{GR_Chuck.abc, groom_metadata.json}` (5.5 MB).
  - `Review/groomed_*.jpg`.
  - `preview_textured.py --groom`, and groom checks in `check_v1.py`.
- **Groom:** 68,000 strands in three groups (`Fur_Body`, `Fur_Back`, `Fur_Cream`), sleek and 0.35–1.2 cm long, on exposed fur only (none under the jacket, on eyelids or on ears), with a denser head. Colour is per group because Blender's Alembic export drops per-strand attributes.
- **Tuft-free variant:** `SK_Chuck_Groomed` keeps the same skeleton, UVs and materials (170,736 triangles). `SK_Chuck` remains the fallback.
- **Rest-pose bug found and fixed:** `Chuck_V1.blend` had been saved in the last exported clip's pose, because clearing the action does not reset bones. Scripts evaluating the saved file (texture bakes, groom) therefore saw a posed body; the first groom was up to 3 cm off the rest mesh. The builder now resets before saving, and the groom and bake scripts force `pose_position = 'REST'`. Exported FBXs and the Unreal imports were unaffected (FBX export was always at rest).
- **Texture change:** textures are now baked from the tuft-free mesh (no tuft AO dots under the groom). The parked tuft islands get flat colour, roughness, normal and metallic, which removed glints on the fallback tufts.
- **Shared-file change:** `.gitattributes` gains `*.abc filter=lfs diff=lfs merge=lfs -text` so the 5.5 MB groom is stored in LFS like the FBX/blend files (integration-owned file; one added line).
- **Checks:** `check_v1.py` **72 PASS**, including groom variant on the same 41 bones, three groups with exact strand counts, and roots 0.020 cm under the rest mesh (702 sampled). Stance drift at most 0.002 cm/s; clips unchanged.
- **For Codex / integration (Unreal):**
  - Enable Groom (HairStrands); import `SK_Chuck_Groomed` on `SK_Chuck_Skeleton`.
  - Import `GR_Chuck.abc` (Y-up → match the mesh), bind it to `SK_Chuck_Groomed`, and create three Hair materials from `groom_metadata.json`.
  - Add a GroomComponent and profile on the 8 GB GPU in both cameras. Re-run `Import-ChuckV1.ps1`, since textures and `SK_Chuck.fbx` changed.
  - Details: `SourceAssets/Chuck/V1/README.md`, Groom section.

## Note to Codex — end-of-session ownership line (user request, 2026-09-26)

The user asked both agents to end **every** session with one explicit line saying who can do the next part:

- "Next part of the work can only be done by Codex" / "...only by Claude"
- "Next part of the work could be done either by Claude or Codex — preference: <which, why>"
- "Next part of the work can be done here"

Claude now does this. Please do the same at the end of each Codex session, from your side.

## Thirteenth pass — picked up Codex's groom import; v2 work split

- **Input:** Codex hit its usage limit mid-way through the groom import. Its work was **uncommitted in the main checkout**, with no handoff note:
  - Plugins HairStrands + AlembicHairImporter; `r.SkinCache.CompileShaders`; `HairStrandsCore` in `Chuck3D.Build.cs`.
  - `Tools/{Import-ChuckGroom.ps1, import_chuck_groom.py, prepare_groom_groups.py, validate_groom_roots.py}`.
  - `ChuckReviewLibrary` groom helpers, and the groom/binding/material assets.
  - Its import succeeded (3 grooms, counts exact, roots ≤ 0.020 cm), but its review render failed with "Review pose mismatch: pelvis expected 19.25, actual 19.5".
- **Delivered commit:** `0f09073` on `codex/claude-character` ("Integrate the v1 groom in Unreal and fix the editor pose review"), on top of `28918f6` (AI-dev-notes review).
  - Carries Codex's work over exactly; leftover duplicate imports `GR_Chuck.uasset` and `GB_Chuck_Fur_*1.uasset` are excluded.
  - Adds Claude's fixes, re-imported assets and review evidence.
- **Fixes:**
  - **Review pose.** The single-node animation path does not evaluate in a non-ticking editor world: a second clip, or a `SetAnimation` call, falls back to the reference pose. New `ChuckReviewLibrary::SetEditorComponentPose` writes the evaluated clip pose straight into the `SkeletalMeshComponent` bone buffer, single-buffered so it is what is read and rendered. It keeps a real skeletal mesh component, which groom bindings require. All six groom review views now pass the pose check: Idle pelvis 19.25, walk 18.125, landing 15.0.
  - **Groom colours.** Unreal's hair shading renders darker than a surface BSDF, so colours are tuned after the engine render (body `.3,.245,.19`; back `.2,.165,.13`; cream `.62,.54,.42`, linear).
  - **Paws.** Fur now stops at the ankle (roots below 4.8 cm skipped) so the paws stay bare. Strands had hung over the paw tops.
- **Verified in `.claude/worktrees/project-orientation-fd7504`** (Unreal 5.7.4, Blender 4.5.14; one heavy process at a time):
  - `Chuck3DEditor` build succeeded.
  - `Tools/Import-ChuckV1.ps1` (import step) passed.
  - `import_chuck_groom.py`: `CHUCK_GROOM_IMPORTED 68000`, bindings 30,000/16,000/22,000 render and 3,000/1,600/2,200 guides.
  - `validate_groom_roots.py`: 706 roots, max 0.020 cm.
  - `review_chuck_v1_unreal.py -ChuckGroomReview`: 6/6 views.
  - `check_v1.py`: 72 PASS.
  - Evidence: `SourceAssets/Chuck/V1/Review/unreal_groom_{front,three_quarter,walk_side,land_side,rat_height,elevated}.jpg`.
- **Not done:** no packaged build or `Verify-Package.ps1` with the Groom plugin enabled. That is the first thing the integrator (or Codex, under v2) should run. The playable character is unchanged (still the legacy runtime), and the groom performance on the 8 GB GPU is unmeasured.
- **Main checkout cleanup (integrator, before merging):** `git merge` of this branch will refuse over the uncommitted groom files. They are all superseded by `0f09073`, except the three duplicate `GB_*1` and the unreferenced `GR_Chuck.uasset`, which should be deleted. With Unreal and Blender closed, in the main checkout:

  ```powershell
  git stash push -u -m "codex-groom-wip-superseded-by-0f09073"   # keeps a recoverable copy
  git merge --ff-only codex/claude-character
  ```

  Keep the stash until the merged main is verified, then drop it.
- **v2 work split (user request 2026-09-27):** `docs/AGENT-WORKFLOW.md` now leads with the v2 table. Claude owns the character end to end (including Unreal import/groom) and the v1 runtime migration, then traversal/camera. Codex takes independent packaged verification, regression tests, tooling, data import and isolated fixes (`docs/agent-tasks/CODEX-V2.md` is its starter prompt). `CLAUDE.md` is updated to match. Next-owner lines at every session end are part of the workflow for both agents.

## Fourteenth pass — v1 runtime migration (playable Chuck is now v1)

- **Source commit:** on top of `9daf5cb`, branch `codex/claude-character`.
- **Changed files:**
  - `Unreal/Chuck3D/Source/Chuck3D/ChuckAnimInstance.{h,cpp}` (new)
  - `ChuckCharacter.{h,cpp}`
  - `DockGameMode.{h,cpp}`
  - `Chuck3D.Build.cs` (adds `AnimationCore`)
  - evidence `SourceAssets/Chuck/V1/Review/runtime_{front,motion_view0,motion_view2}.jpg`
- **Character:** `GetMesh()` uses `SK_Chuck_Groomed`, and three `GroomComponent`s (`Groom_Fur_{Body,Back,Cream}`, simulation off, `M_Fur_*`) are attached to it. The legacy `ChuckBody` PoseableMesh, the `FootLeft`/`FootRight` static paws and the `RatVisual` node are removed. `-ChuckNoGroom` removes the grooms and uses `SK_Chuck` (geometric tufts). Capsule and movement tuning are unchanged.
- **Animation:** `UChuckAnimInstance` is native, with no binary AnimBP. Its custom `FAnimInstanceProxy::Evaluate` does the following:
  - Samples two clip layers and cross-fades between them.
  - Idle, WalkStart and WalkLoop are **distance-matched**. WalkStart inverts its authored travel curve (19 cm). WalkLoop advances by distance ÷ 28.5 cm stride.
  - Air uses JumpStart from its extension half, then JumpLoop; landing uses JumpLand.
  - Two-bone IK per leg (`AnimationCore::SolveTwoBoneIK`) keeps the clip's knee plane and its hock→ball vector.
  - Per-paw ground traces set paw height; the pelvis drops up to 4 cm for a lower paw.
  - **Stance locks:** during the manifest stance windows, a paw's ball is held in world space from where it was last drawn. It is released (0.08 s fade) if a turn pulls it more than 6 cm.
  - **Settle steps** when standing: a locked paw steps (0.18 s, 1.5 cm lift) if it drifts more than 3 cm, or hovers more than 0.5 cm, from the Idle pose.
  - Unreal imported the loop clips at their full period (Idle 2.0 / WalkLoop 0.3 / JumpLoop 0.4 s, logged as `CHUCK_CLIP`).
- **Verifier:** the ten checks that read legacy internals were replaced one-for-one, so there are still 43. The new checks cover:
  - the v1 mesh and animation instance, and the v1 leg, arm, tail and cigarette bones;
  - the reference hip at (-2,-6,19.5);
  - v1 materials and 3 grooms;
  - evaluated-paw stance slip and IK shortfall;
  - `upperarm_L` articulation;
  - ball lift in the air;
  - ball-above-ground after landing, equal to the rest height.
- **Verified in `.claude/worktrees/project-orientation-fd7504`** (UE 5.7.4, one heavy process at a time):
  - `Chuck3DEditor` build.
  - `Build-Prototype.ps1 -Package`, then a second BuildCookRun after fixes.
  - `Verify-Package.ps1`: **43 passes, failures=0**.
  - `Verify-Package.ps1 -MotionCapture`: 43 passes, 36 frames per view.
  - Measures (`Local/verify-package-20260926-232536.log`): contact 42 samples, max slip 0.0000 cm/s; IK shortfall 0.0000 cm; ball lift in the air 6.90 cm; landing ball 0.9997 cm above ground vs rest 1.0000.
  - Captures reviewed by eye: the v1 groomed Chuck stands on the quay and walks with paws grounded (front, side and back views).
- **Caveats and remaining flaws:**
  - The slip measure reads the proxy's evaluated ball position, so it proves the lock and IK hold. It does not independently read skinned vertices.
  - `Build-Prototype.ps1` regenerated `Content/Prototype/Materials/*.uasset`. These are world assets outside this task, so they were reverted, not committed.
  - WalkStop and Turn clips are not used yet: stopping cross-fades to Idle, then settle steps; turning uses stance release.
  - Grooms are not simulated.
  - Groom GPU cost on the 8 GB card has not been measured.
  - The `-ChuckNoGroom` path compiles but was not run.

## Fifteenth pass — WalkStop, turn in place, no-groom and GPU-cost checks

- **Source commit:** on top of `3f57bf5`, branch `codex/claude-character`.
- **Changed files:**
  - `ChuckAnimInstance.{h,cpp}`, `ChuckCharacter.{h,cpp}`, `DockGameMode.{h,cpp}`
  - `Chuck3D.Build.cs` (adds `RHI` for GPU frame timing)
  - `Tools/Verify-Package.ps1`: new `-NoGroom` switch; expected passes are now 47, or 46 with `-NoCapture`.
  - evidence `SourceAssets/Chuck/V1/Review/runtime_stop_turn_jump_view2.jpg`
- **WalkStop:**
  - **Braking** is now constant: 237.5 cm/s² = 95² ÷ (2 × 19 cm), with braking friction 0, so a stop from full speed takes the clip's own 19 cm / 0.4 s and is distance-matched. The previous friction 16 + 700 cm/s² stopped in about 3 cm / 0.07 s, which no authored clip could match.
  - **Phase alignment:** WalkStop is authored to begin from WalkLoop frame 0 (left paw planted). When input is released, Chuck coasts (at most half a stride, ≤ 14 cm) to the next half-stride point.
  - **Mirroring:** from the right-planted half the stop plays **mirrored**. `FAnimationRuntime::MirrorPose` on Y uses an `_L/_R` map built at runtime, so no mirror asset is needed.
  - **Entry point:** the stop enters where its remaining authored travel equals v²/2a, and its stance intervals, swapped when mirrored, drive the locks.
  - Before this alignment, uncapped frame rates showed a planted paw dragged at 82 cm/s.
- **Turn in place:** when Chuck is standing and the input points more than 60° from his facing, he plays TurnLeft90/TurnRight90. Movement is disabled during the turn, and the capsule yaw follows the clip profile (scaled for angles other than 90°); walking starts after the last plant (0.55 s). Unreal positive yaw is a right turn; sign verified by 0 releases and 0 slip.
- **Other runtime changes:**
  - WalkStart now also locks stance paws from its manifest intervals.
  - A landing without input brakes at 1000 cm/s² and goes to Idle; it only walks on with input.
- **Test fix:** stage 6's simulated gamepad stick stays deflected after its axis-zero event, and the old test order hid this. Before the new stages, the test flushes keys and disables player input, as the capture branch already did.
- **New checks (43 → 47):**
  - walk stop brakes over its clip with planted paws still;
  - walk stop settles into planted idle stance;
  - turn in place pivots on planted paws (0 releases);
  - turn in place faces input before walking.
  - `CHUCK_PERF_MEASURE` logs frame and GPU time during the steady walk.
- **Verified** (UE 5.7.4, package from BuildCookRun; one heavy process at a time):

  | Run | Passes | Stop | Turn | Landing ball |
  |---|---|---|---|---|
  | `Verify-Package.ps1 -MotionCapture` (groom) | 47/47 | 18.01 cm, 37 locked samples, 0 cm/s, 0 releases | 24 samples, 0 cm/s, yaw −90.000 | 0.9997 cm |
  | `Verify-Package.ps1 -NoGroom` | 47/47 | 18.01 cm, 0 cm/s | 0 cm/s, −90.000 | 0.9997 cm |
  | uncapped (`r.VSync 0, t.MaxFPS 0`), groom | 0 failures | 18.58 cm, 184 samples, 0 cm/s | 114 samples, 0 cm/s | 0.9999 cm |
  | uncapped, `-ChuckNoGroom` | 0 failures | 18.74 cm, 0 cm/s | 187 samples, ≤0.0001 cm/s | 0.9999 cm |

  - **Groom GPU cost** (uncapped, 1280×720, steady walk): 3.02 ms per frame with grooms against 2.03 ms without, **about 1.0 ms for the 68k-strand groom**. Vsync-capped runs read 16.67 ms either way, so they give no cost information.
  - Motion-capture frames were reviewed by eye: walk → right turn while moving → stop → jump → land, with paws grounded.
- **Caveats and remaining flaws:**
  - Coasting to the half stride adds up to 0.15 s before braking begins, which is a deliberate feel trade-off.
  - Turn-in-place delays walking by 0.55 s from standing. Turns beyond 90° scale the authored yaw, so their paws may release; this is untested beyond 90°.
  - The slip and turn measures read the proxy's evaluated paw positions.
  - Grooms are still unsimulated.

## Sixteenth pass — small-character camera feel (both cameras, no choice made)

- **Source commit:** on top of `7ba97dc`, branch `codex/claude-character`.
- **Changed files:**
  - `ChuckCharacter.{h,cpp}` (`UpdateCamera`), `DockGameMode.{h,cpp}`
  - `Tools/Verify-Package.ps1`: expects 49, or 48 with `-NoCapture`.
  - `docs/PROTOTYPE.md` camera bullets
  - evidence `SourceAssets/Chuck/V1/Review/runtime_rat_height_framing.jpg`
- **Camera decision:** stays OPEN (PROTOTYPE.md). Both modes and the toggle are unchanged in behaviour otherwise.
- **Changes:**
  - **Vertical follow (both views):** the boom pivot holds its height through a jump (1.5/s interpolation while falling, clamped to ±40 cm) and settles quickly when grounded (8/s), so a landing on a new level or a fall is still followed.
  - **Rat-height framing:** the pivot is chest-high (+22 cm instead of +30 cm) and the boom is tilted −5°, giving a lens about 76 cm up, just over the ears. Previously the ear-height lens put Chuck's head in the middle of the view, covering the boat and the tavern door. Mouse/stick pitch still rotates only the lens.
- **New checks (47 → 49):**
  - camera holds its height through a jump (< 8 cm; measured 5.49 cm in the elevated view);
  - rat-height lens sits just over Chuck's ears (68–85 cm; measured 75.97 cm).
- **Verified** (UE 5.7.4, one heavy process at a time):
  - `Verify-Package.ps1 -MotionCapture`: **49/49**.
  - `-NoGroom`: 49/49.
  - Uncapped with and without the groom: 0 failures.
  - All earlier measures are unchanged (slip 0, stop 18.0–18.7 cm, turn −90.000, landing 0.9997 cm). The GPU cost of the groom is again about 1.1 ms (3.20 vs 2.06 ms).
  - Rat-height captures were reviewed by eye.
  - The jump camera travel before this change was not measured, so there is no before/after number.
- **Not done:**
  - Lazy auto-follow of the camera behind a moving Chuck. It changes how camera-relative input steers, so it needs the user's feel feedback first.
  - No physical-controller or subjective comfort test.
  - Traversal (run/climb/vault) waits for the user's scope decision (AGENT-WORKFLOW suggested order, step 4).

## Seventeenth pass — new goal images; first look pass toward them

- **Request (2026-09-27):** the user supplied four new goal images and asked to bring Chuck closer to them before any camera or roll/jump work.
- **Source commit:** on top of `00e46d6`, branch `codex/claude-character`.
- **References:** `References/ArtDirection/Chuck-{Turnaround,Standing-Smoking,Run-Profile,Run-Cycle-Sheet}.png` (byte-exact, SHA-256 in `docs/ART-DIRECTION.md`; LFS, 8.6 MB). `docs/ART-DIRECTION.md` has a new "Goal images, 2026-09-27" section with measured targets. `References/PROVENANCE.md` and `docs/CHARACTER-PLAN.md` point to them.
- **Measured against the turnaround** (65 cm scale, front view): the goal head is about 8–9 cm wide against our 14.6 cm. The goal jacket runs hem ≈ 25 cm to collar ≈ 53 cm against our 18.5–49 cm. The goal coat is warm brown and shaggy against our cool grey and sleek.
- **Changes** (all in generators, reproducible):
  - `Tools/build_chuck_model.py`:
    - `HEAD_NARROW = 0.72` on the skull half-widths, with the eyes, ears and whisker roots moved in with it;
    - ears 8.6 cm tall with pink backs (bare skin both sides);
    - jacket cropped to a 23 cm hem, with zipper, pockets, hem stitching and back seams moved up;
    - a broader collar-to-lapel roll; the old narrow strip read as a drawstring.
  - `Tools/build_chuck_v1.py`: slimmer legs (radius 3.7 instead of 4.3 cm at the haunch).
  - `bake_textures.py`:
    - warm taupe-brown coat, beige belly, pinker skin, pale claws;
    - red-violet jacket calibrated on the Unreal render (sRGB 88, 62, 113 against the goal's 86, 46, 113; before this it was 100, 21, 125, near neon);
    - hem grime moved to the new hem.
  - `build_groom.py`:
    - shaggy, clumped coat: 6 points per strand, lengths 0.6–1.8 cm, 20–45° lift, frizz, tips pulled to a guide strand (1 per 14, pull 0.6);
    - region scales: short muzzle, spiky crown, fluffier legs;
    - warm colours; eye and ear exclusions follow the narrower head.
  - `import_chuck_v1.py` / `import_chuck_groom.py`: re-importing over existing materials crashed UE 5.7.4 (`Assertion failed: !IsRooted()` in `DeleteAllMaterialExpressions`). The scripts now keep an existing material and update it in place: `M_Chuck_V1` has its wiring checked, and the `M_Fur_*` colour constants are updated.
- **Rig contract:** unchanged. The 41 bones and all bone positions are the same, and the clips were regenerated from the same table. The mesh is now 208,413 triangles (170,544 for the groom variant).
- **Verified** (Blender 4.5.14, UE 5.7.4, one heavy process at a time):
  - `check_v1.py`: PASS. Groom roots in Unreal: 706 at max 0.020 cm. Groom counts 30,000/16,000/22,000 with bindings.
  - `Import-ChuckGroom.ps1 -Review`: all steps verified.
  - Packaged `Verify-Package.ps1 -MotionCapture` and `-NoGroom`: **49/49** each. Uncapped with and without groom: 0 failures.
  - Motion measures are unchanged (slip 0, stop 18.0–18.7 cm, turn −90.000, landing 0.9997 cm).
  - Groom GPU cost is still about 1.0 ms (3.07 vs 2.09 ms, uncapped at 1280×720), despite longer six-point strands.
- **Evidence:**
  - `Review/goal_compare_turnaround_unreal.jpg` (goal against the Unreal review);
  - refreshed `groomed_*`, `textured_*`, `neutral_*`, `pose_*`, `strip_*`, `ankle_join.jpg`, `unreal_groom_*` and `runtime_front.jpg`.
  - Note: a `-NoGroom` verifier run overwrites the game's `Chuck_Front.png`. The runtime capture was retaken from a groom run.
- **Remaining gaps to the goal images** (next passes, largest first):
  1. **Proportions:** the goal has long, slender legs (crotch ≈ 19 cm against our ≈ 16 cm) and a narrower torso and jacket (about 29 cm against 34 cm across the sleeves). Moving the hips and thighs changes rig bone positions: the contract table, clips, runtime reach constants and the hip test.
  2. **Cigarette prop and smoke:** planned as a separate mesh on `socket_cigarette`, so a future pickup design isn't pre-empted.
  3. **Hands:** larger, with longer fingers and claws.
  4. **Collar:** a pointed shirt collar and a flatter lapel.
  5. **Belly** reads grey and flat in-game when shaded; the colour calibration was done under the editor review lighting.
  6. **Run clip**, from `Chuck-Run-Cycle-Sheet.png`.

## Eighteenth pass — v1.1 proportions (legs, torso and head toward the turnaround)

- **Source commit:** on top of `699efd0`, branch `codex/claude-character`.
- **Contract:** `docs/RIG-CONTRACT-V1.md` has a new "v1.1 shape amendment" section.
  - Same 41 bones, hierarchy, flags and sockets; rest positions move.
  - `Tools/chuck_v1_shape.py` (new, pure Python) maps both the accepted v1.0 table and the legacy study geometry.
- **The shape:**
  - paws unchanged; legs stretched so the hip rises 3 cm (thigh 10.20 cm, calf 10.65 cm, hip 22.5 cm);
  - torso, arms and jacket up 3 cm;
  - head compressed back under the fixed 65 cm ear tip;
  - torso and jacket ×0.88 in width, arm chain ×0.88.
- **Changed files:**
  - `Tools/build_chuck_v1.py`: remaps the geometry and the table; weight bands use `Z()`; asserts the ear tip is 65 cm; metadata records the amendment.
  - `SourceAssets/Chuck/V1/check_v1.py` and `Tools/validate_chuck_v1_import.py`: use the effective table.
  - `build_groom.py`: region heights use `Z()`.
  - `bake_textures.py`: hem grime at 26 cm.
  - `import_chuck_v1.py` / `import_chuck_groom.py`: `update_skeleton_reference_pose` (the first re-import kept the old skeleton rest pose: 3.49 cm error).
  - `DockGameMode.cpp`: hip check now (−2, −6, 22.5).
  - Regenerated FBX, blend, groom, textures, `/Game/Characters/Chuck/V1` assets and Review images.
- **Verified:**
  - `check_v1.py` PASS.
  - `Import-ChuckGroom.ps1 -Review`: all steps, including the rest-pose validator < 0.01 cm; roots 706 at max 0.020 cm.
  - Packaged `Verify-Package.ps1 -MotionCapture` and `-NoGroom`: **49/49**. Uncapped with and without groom: 0 failures.
  - Contact slip 0; landing ball 1.0016 cm against 1.0 rest; stop 18.0–18.7 cm; turn −90.000.
  - Groom about 0.8 ms (2.92 vs 2.09 ms uncapped).
  - Evidence: `Review/goal_compare_turnaround_unreal.jpg` and the refreshed Review set.
- **Remaining gaps to the goal images:**
  - cigarette prop and smoke;
  - larger hands with long fingers and claws;
  - pointed shirt collar and flatter lapels (one lapel end still hangs like a tab in 3/4 view);
  - belly reads grey in-game shade;
  - run clip.
- **Next-owner note for Codex:** the table consumers now read `chuck_v1_shape.load_effective_table()`. `Check-RigContract.py` still validates the unchanged v1.0 base (script/JSON match). It does not check the v1.1 positions, which are covered by `check_v1.py` and the Unreal validator.

## Nineteenth pass — cigarette prop and smoke wisp

- **Source commit:** on top of `270cd37`, branch `codex/claude-character`.
- **Why:** every goal image shows the cigarette in the left mouth corner with a thin smoke wisp. It is a **separate prop** on `socket_cigarette` (not skinned into the body), so the deferred pickup design stays open. `-ChuckNoCigarette` removes it.
- **New files:**
  - `SourceAssets/Chuck/V1/build_cigarette.py`, producing `Cigarette/SM_Cigarette.fbx` (7 cm; Paper, Filter, Ash, Ember), `SM_CigaretteSmoke.fbx` (16 cm crossed curling ribbons) and `T_CigaretteSmoke.png`;
  - `Tools/import_chuck_cigarette.py`, run from `Import-ChuckV1.ps1`;
  - `/Game/Characters/Chuck/V1/Cigarette/*`.
- **Changed files:**
  - `ChuckCharacter.{h,cpp}`: `Cigarette` and `CigaretteSmoke` components. The prop's +X follows the exported bones' local axis, read from the imported rest pose (thigh → knee), so there is no hard-coded FBX axis conversion. The smoke sits at the mesh's lit end (bounds) with absolute, upright rotation.
  - `DockGameMode.cpp`: new check `cigarette held in the left mouth corner with upright smoke` (aim · rest socket direction > 0.97, at the socket < 0.05 cm, smoke up > 0.999).
  - `Verify-Package.ps1`: expects 50, or 49 with `-NoCapture`.
  - `ChuckReviewLibrary.cpp`: `UpdateChildTransforms` after an editor pose, so socket props follow.
  - `review_chuck_v1_unreal.py`: shows the prop and adds a `face` close-up.
- **Materials:** flat paper, filter and ash; an ember with a slow 3 s emissive pulse (restrained, no flicker); translucent unlit smoke panning the mask upward, faded at the base, top and edges.
- **Verified:**
  - `Import-ChuckV1.ps1` (cigarette step `CHUCK_CIGARETTE_IMPORTED`, length 7.000 cm).
  - Unreal groom review including `face`.
  - Packaged `Verify-Package.ps1 -MotionCapture` and `-NoGroom`: **50/50**; uncapped with and without groom: 0 failures.
  - `CHUCK_CIGARETTE_MEASURE aim_dot=0.9996 at_mouth_cm=0.0000 smoke_up=1.0000`.
  - Evidence: `Review/unreal_groom_face.jpg` and `Review/goal_compare_cigarette.jpg`.
- **Gaps:**
  - The goal's cigarette sits a little further forward, nearer the nose; moving `socket_cigarette` would be a contract change.
  - The smoke reads as a thin straight thread from most angles; the goal's curls more.
  - There is no smoking animation (hand to mouth, puff), and the `-ChuckNoCigarette` path is untested in the packaged verifier.
  - Still open from the goal list: larger hands, pointed shirt collar and flat lapels, belly shading in-game, run clip.

## Twentieth pass — hands and shirt collar

- **Source commit:** on top of `7974b61`, branch `codex/claude-character`.
- **Hands** (`Tools/build_chuck_model.py`, after the goal images):
  - slimmer palm;
  - four long relaxed fingers, 2.9–3.9 cm (were 2.0–2.7 cm), and a longer thumb;
  - a pale pointed claw on every digit.
  - New part labels `Thumb`, `FingerClaw` and `ThumbClaw`. The thumb skins to `thumb_*` by part, not by a Y threshold: after the 0.88 arm narrowing the pinky surface already crossed the old 15 cm split. Claws are rigid on their digit. `chuck_v1_shape.ARM_PARTS` includes the new labels.
- **Collar:** the collar fall now hangs to a **pointed shirt-collar tip** on each side, about 3.6 cm below the collar line (`collar_bottom`). Before, it ran 6.2 cm down the chest as a narrow roll that read as a dangling tab or drawstring.
- **Review:** `preview_textured.py` gains `close_hand` and `close_collar` views.
- **Verified:**
  - `check_v1.py` PASS; `Import-ChuckGroom.ps1 -Review` all steps.
  - Packaged `-MotionCapture` and `-NoGroom`: **50/50**; uncapped with and without groom: 0 failures.
  - Grip and curl poses (`Review/pose_overhead_grip_*`) inspected.
- **Remaining look gaps:**
  - belly reads grey in-game shade;
  - cigarette sits slightly back from the goal's position;
  - the smoke curls less than the goal's;
  - run clip (`Chuck-Run-Cycle-Sheet.png`).

## Twenty-first pass — shorter snout and finer, denser fur (user target image)

- **Input:** the user's `References/ArtDirection/Chuck-Snout-Fur-Target.png` (byte-exact, SHA-256 `BF97F4DCDDD58205590298F8D05999DEEBF8036C2499B2A7A949EBB876421258`): "the snout should be shorter like this and overall with more hair detail like this".
- **Source commit:** on top of `5694c73`, branch `codex/claude-character`.
- **Changes:**
  - **Snout:** v1.2 snout amendment, 0.82 length for head parts and bones (see `docs/RIG-CONTRACT-V1.md`). The front muzzle sections are fuller, with a slightly larger nose.
  - **Cream and whisker pads:** the cream muzzle patch wraps up over the whisker pads. The bake adds follicle dots there, and the groom keeps the pads sparse (70% of roots skipped).
  - **Whiskers:** 7 per side (was 4), fanning from the pad, shorter, 0.035 cm radius.
  - **Groom:** 136,000 strands (was 68,000), 55 µm roots, 3.2× head density, lighter back and crown.
  - **Unreal fur materials:** `M_Fur` is a parent hair material with a `HairColour` parameter × Hair Attributes seed variation (0.85–1.15). `M_Fur_{Body,Back,Cream}` are now instances. The old plain materials were deleted and recreated, and re-imports now just set the parameter.
- **Verified:**
  - `check_v1.py` PASS; `Import-ChuckGroom.ps1 -Review` all steps.
  - Packaged `-MotionCapture` and `-NoGroom`: **50/50**; uncapped with and without groom: 0 failures.
  - Groom cost about 0.85 ms (2.82 vs 1.97 ms uncapped) despite twice the strands.
  - Evidence: `Review/goal_compare_snout_fur.jpg` and the refreshed Review set.
- **Gaps:**
  - The follicle dots don't yet read through the fur at preview distance.
  - The nose is still rounder than the target's.
  - The target's fur has lighter tips and more visible individual hairs; Unreal's seed variation adds some of this, but the Blender preview cannot show it.

## Twenty-second pass — face proportions (user target image)

- **Input:** `References/ArtDirection/Chuck-Face-Proportion-Target.png` (byte-exact, SHA-256 `610C3358066A5507B6C8100023E5CF56ECFCA5F9EEF733CE319918CE580A1907`): "I'd like the face/snout proportions more like this".
- **Source commit:** on top of `ccc6459` (main), branch `codex/claude-character`.
- **Changes:**
  - **Snout:** 0.74 of its length (was 0.82), through the v1.2 amendment in `Tools/chuck_v1_shape.py`; the head, jaw and cigarette-socket bones follow. The front sections are conical again, not blunt, and the nose is smaller. The cigarette aim check now uses (3.256, −2.1, −0.5).
  - **Cranium:** `CRANIUM` gives a slight lift (×1.08) for a sloping rat forehead. A first try at ×1.3 read as a round mouse and was dropped.
  - **Eyes:** smaller and set higher.
  - **Whisker-pad mask:** in the bake it now follows `SHAPE.X()`.
- **Verified:**
  - `check_v1.py` PASS; `Import-ChuckGroom.ps1 -Review` all steps.
  - Packaged `-MotionCapture` and `-NoGroom`: **50/50**; uncapped with and without groom: 0 failures.
  - Cigarette aim dot 0.9993; groom about 0.86 ms.
  - Evidence: `Review/goal_compare_face.jpg`.
- **Gaps against the target:**
  - The cream muzzle patch reads as a bright white mask; the target's muzzle is only slightly lighter.
  - The target's head fur is warmer tan and scruffier.
  - The target's eyes have an amber-brown iris.
- **Not integrated into main.** The launcher still shows `4eef511`.

## Twenty-third pass — softer muzzle, scruffy city-rat coat, amber eyes

- **Request:** "Do the softer muzzle, scruffier fur head and body (city rat look), and amber eyes".
- **Source commit:** on top of `b155ea1`, branch `codex/claude-character`.
- **Changes:**
  - **Softer muzzle:** the Chest shader (muzzle and belly) is a warmer tan-cream, and `Fur_Cream` is (.44, .32, .21). One third of the muzzle cream triangles grow body-colour strands, so the patch blends into the coat.
  - **Scruff, head and body:**
    - clumps tighter and stronger (a guide per 10 strands, tip pull 0.75);
    - lift 25–55° and frizz σ 0.22;
    - 8% guard hairs at 1.6–2.3× length and 1.4× lift;
    - slightly warmer coat (body .26, .15, .08).
  - **Amber eyes:** the eye shader bakes a black pupil, an amber-brown iris and a dark rim, gazing sideways and a little forward, centred from `chuck_v1_shape`.
- **Verified:**
  - `check_v1.py` PASS; `Import-ChuckGroom.ps1 -Review` all steps.
  - Packaged `-MotionCapture` and `-NoGroom`: **50/50**; uncapped with and without groom: 0 failures.
  - Groom about 0.75 ms.
  - Evidence: `Review/goal_compare_city_rat.jpg` and `Review/unreal_groom_face.jpg`.
- **Gaps:**
  - The belly reads grey-cream in cool light; the target is warmer.
  - The iris is only about 10 texels across on the 2048 map, so it is soft up close.
- **Not integrated into main.** The launcher still shows `4eef511`.

## Twenty-fourth pass — sleeker head fur (user feedback)

- **Feedback:** the head fur "stands up too much and looks too much like human haircut, but i like the body fur".
- **Source commit:** on top of `7128d41`, branch `codex/claude-character`.
- **Change** (`build_groom.py`, head only, z > `Z(47.5)`; body untouched):
  - crown ×0.6 length / ×0.45 lift (was 1.2 / 1.6), laid back along the skull;
  - cheeks and nape ×0.7 / ×0.5;
  - muzzle lift ×0.6;
  - no guard hairs; frizz σ 0.11 (the body keeps 0.22).
- **Verified:**
  - `check_v1.py` PASS; `Import-ChuckGroom.ps1 -Review` all steps.
  - Packaged `-MotionCapture` and `-NoGroom`: **50/50**; uncapped: 0 failures.
  - Groom about 0.88 ms.
  - Evidence: `Review/goal_compare_head_fur.jpg`.
- **Not integrated into main.** The launcher still shows `4eef511`.

## Twenty-fifth pass — ears seated on the skull; natural finger curl

- **User report:** "his ears float off his head" and "his hands/paws/fingers bend backward in a weird way".
- **Source commit:** on top of `f797938` (main), branch `codex/claude-character`.
- **Ears:**
  - **Cause:** fixed ear positions (y = 4.6 cm) were kept when the head was narrowed to 0.72 (skull about 4.2 cm wide there), so the ears sat beside the head.
  - **Fix** (`cupped_ear` in `build_chuck_model.py`): each ear's pinched base is seated by ray cast on the evaluated skull, 65° from vertical on its side, and sunk 0.25 cm. The 65 cm ear-tip contract is met by scaling about that base, not shifting, so an ear can never lift off.
  - A first try at 50° seated the ears too high and buried half of each disc.
  - The groom ear exclusion follows (y 4.1, |y| > 3.2).
- **Fingers:**
  - **Cause:** the thumb is lateral, so the palm faces forward. The finger chains curled backward, while the clips curl `fingers_*` forward (−Y), which read as hyperextension.
  - **Fix:** fingers now continue the hand's forward-down line and curl gently toward the palm; the thumb opposes slightly.
- **Verified:**
  - `check_v1.py` PASS; `Import-ChuckGroom.ps1 -Review` all steps.
  - Packaged `-MotionCapture` and `-NoGroom`: **50/50**; uncapped: 0 failures.
  - Grip, curl and walk poses inspected.
  - Evidence: `Review/fix_ears_fingers.jpg`.
- **Not integrated into main.** The launcher still shows `37208d3`.

## Twenty-sixth pass — ear placement and textured head fur (user reference)

- **Input:** `References/ArtDirection/Chuck-Ears-HeadFur-Target.jpg` (the user's pick of an earlier render: pass 23 close-ups): ears "positioning more like this just not floating"; head fur "textured / a bit sticking out … just not so much that it looks like a haircut".
- **Source commit:** on top of `c198639`, branch `codex/claude-character`.
- **Ears:**
  - The 25th pass's ray-cast seat (65° down the skull side) put the ears low, near the cheeks. That is replaced.
  - Each ear keeps its original high, set-back placement and orientation, and slides inward only, until its pinched base vertex is 0.5 cm inside the evaluated skull at the base's height (it had been about 2 cm off after the head was narrowed).
  - The top-at-65 cm rule is a plain vertical shift again.
  - Groom ear exclusion: y 3.2, |y| > 2.6, z > Z(57.5).
- **Head fur:**
  - between the city-rat crown (1.2 / 1.6) and the sleek pass (0.6 / 0.45): crown ×0.85 length / ×0.85 lift, cheeks and nape ×0.8 / ×0.8;
  - head frizz σ 0.16;
  - head guard hairs at 35% of the body rate, 1.2–1.5× length.
  - The body is unchanged.
- **Verified:**
  - `check_v1.py` PASS; `Import-ChuckGroom.ps1 -Review` all steps.
  - Packaged `-MotionCapture` and `-NoGroom`: **50/50**; uncapped: 0 failures.
  - Groom about 0.86 ms.
  - Evidence: `Review/fix_ears_headfur.jpg` (reference against close, front, rear and 3/4 views).
- **Not integrated into main.** The launcher still shows `37208d3`.

## Twenty-seventh pass — ears arranged as in the turnaround

- **Request:** "The ears need to look more like this with their positioning", with the turnaround sheet (identical to `References/ArtDirection/Chuck-Turnaround.png`, so no new reference file).
- **Source commit:** on top of `9ed827b`, branch `codex/claude-character`.
- **Changes** (`cupped_ear` in `build_chuck_model.py`):
  - **Orientation:** the cup faces forward (0.85, ±0.45, 0.15); the old one faced sideways (0.5, ±0.84). The ear leans about 12° outward.
  - **Shape:** a taller oval, half-width 3.2 × half-height 4.9 cm, about 1.3:1 after the head's vertical compression.
  - **Seat:** the pinched base is ray-cast onto the upper side of the skull (35° from vertical) at source x 1.5, about 4.7 cm behind the eye, and sunk 0.4 cm.
  - **65 cm tip:** met by shifting down only; if an ear falls short it grows about its base, so it can never lift off.
  - Two intermediate iterations (45° seat with 25° lean; then a 35° seat) read as round, Mickey-like ears and were refined.
  - Groom ear exclusion follows: centre (1.1, ±4.4, Z(61.3)).
- **Verified:**
  - `check_v1.py` PASS; `Import-ChuckGroom.ps1 -Review` all steps.
  - Packaged `-MotionCapture` and `-NoGroom`: **50/50**; uncapped: 0 failures.
  - A bake retry was needed after a transient OneDrive write lock on `T_Chuck_Normal.png`.
  - Evidence: `Review/goal_compare_ears_turnaround.jpg`.
- **Gap:** the turnaround's ears still stand a little taller and closer together.
- **Not integrated into main.** The launcher still shows `37208d3`.

## Twenty-eighth pass — ears wider apart; neutral, human-like hands

- **User feedback:** the ears were "too close together"; the hands "still look unnatural like someone doing the 6 7 meme", and the user wants "more natural human-like hand/finger placement while standing/walking".
- **Source commit:** on top of `1ffbec3` (main), branch `codex/claude-character`.
- **Ears:** seated on the outer top corners of the skull (48° from vertical, was 35°) with a 17° outward lean, as in the turnaround front view. The groom ear exclusion follows.
- **Hands:**
  - **Cause:** the palms faced forward with the thumbs lateral and the fingers curled forward, so the hands read as held out palms-up.
  - **Fix:** the hands are now in a neutral rest. The palm turns toward the thigh; the thumb is at the front, angled down and in; the four fingers hang in a front-to-back row and curl loosely toward the palm (medially).
  - The clips' finger curl now uses a `curl(side, degrees)` helper in `build_chuck_v1.py`: `fingers_*` about X with sign by side (toward the medial palm), replacing the forward −Y bend in Idle, WalkLoop, WalkStop/turns and the jumps.
- **Verified:**
  - `check_v1.py` PASS; `Import-ChuckGroom.ps1 -Review` all steps.
  - Packaged `-MotionCapture` and `-NoGroom`: **50/50**; uncapped: 0 failures.
  - Standing (`neutral_*`), walking (`walk_side_f*`) and close-hand views inspected.
  - Evidence: `Review/fix_ears_wide_hands_neutral.jpg`.
- **Not integrated into main.** The launcher still shows `1057e0f`.

## Twenty-ninth pass — aplomb idle and swagger saunter (body language)

- **User direction (2026-09-27):** "Chuck, this aplomb chain smoking rat in a kick ass jacket saunters down the dock smoking his cigarette, he busts into a roll and a side jump and slashes with agility, then reassumes his cool and collected saunter." Lock the standing and walking aura first (natural hands and overall body language, a saunter with a touch of swagger), using the turnaround stance as the starting point.
- **Push:** main (`43bf1eb`) was pushed to `origin` on the user's instruction. That uploaded about 1.9 GB of LFS objects; check the GitHub LFS quota.
- **Research used:**
  - Walk-cycle guides: hip/shoulder counter-rotation, with peaks offset (overlap). Scaling the hip and shoulder rotation makes a swagger. Arms swing from the shoulder with a lag; elbows bend on the upswing; hands trail.
  - Contrapposto: weight on one leg, that hip up, shoulders tilted the opposite way, free knee soft.
  - Dynamic similarity: Froude number Fr = v²/(gL), with a preferred walk at Fr ≈ 0.25.
  - Chuck's 22.5 cm leg gives about 74 cm/s preferred; a saunter at about 0.7× that is **62 cm/s** (Fr ≈ 0.17). The old 95 cm/s with a 0.3 s cycle was Fr ≈ 0.41 (400 steps/min), a scurry.
- **Changes** (`Tools/build_chuck_v1.py`):
  - **`carriage(k, w, look, breath)`** is shared by Idle (k = 0), WalkLoop (k = 1), start/stop and turns (blending k):
    - pelvis sway over the standing paw, dip at contact, ±6° twist toward the forward leg, 4.5° swing-side drop;
    - chest and spine counter-twist of about +14°, lagged 0.06 cycle, with the head cancelling most of the net yaw (steady gaze);
    - chest up, a slight lean back, chin up;
    - arms swing ±16° from the shoulder with a lag, carried 5° out, elbows bending on the forward swing, hands trailing;
    - a lazy tail.
  - **WalkLoop:** 62 cm/s, 15 frames (0.5 s), 31 cm stride, stance 0.62, pelvis drop 0.6 cm, lift 1.8 cm, paws toed out 7°.
  - **Idle** (now 4 s, 120 frames):
    - contrapposto on the right leg (pelvis over it, hips and shoulders counter-tilted), with the free left paw 1.6 cm forward, 0.9 cm out and turned out 12°;
    - chest and chin up; a slow look drift; one breath per 2 s; a small chin lift at 60%, as if drawing on the cigarette.
  - **Transitions:** WalkStart and WalkStop take 0.5 s each (15.5 cm) and start and end on the idle stance, joining the loop's toe-out. Turns start and end on the idle stance.
  - **Hands** (`build_chuck_model.py`): compact, softly curled fingers (2.2–2.9 cm), slimmer palm.
- **Runtime sync:**
  - `Tools/gen_chuck_clip_data.py` (new, run by the builder) writes `Unreal/Chuck3D/Source/Chuck3D/ChuckClipData.h` from the manifest: walk speed, cycle, stride, stance fraction, start/stop duration and travel, stance windows (including mirrored stop) and the turn yaw table.
  - `ChuckCharacter.cpp` uses it for `MaxWalkSpeed`, braking (v²/2d), distance matching, stance locks and turns; there are no hard-coded clip numbers left.
- **Tests** (`DockGameMode.cpp`):
  - The slip probe gates at 0.9 × walk speed.
  - The three one-second "walks" checks now require X > −200 (40 cm; the saunter covers about 50 cm). They had required X > −175 (65 cm), which assumed 95 cm/s.
  - The stop distance is compared against the manifest's `StopTravel`.
  - The cigarette aim threshold is 0.95 (the idle head turns and lifts up to about 12°).
- **Tooling:** `SourceAssets/Chuck/V1/review_motion.py` (new) renders front, 3/4 and side contact sheets of WalkLoop and Idle: `Review/motion_{WalkLoop,Idle}.png`.
- **Verified:**
  - `check_v1.py` PASS; `Import-ChuckGroom.ps1 -Review` all steps.
  - Packaged `-MotionCapture` and `-NoGroom`: **50/50**; uncapped with and without groom: 0 failures.
  - Contact slip 0 (52 samples at 60 fps); stop 14.6–15.1 cm against 15.5 authored; turn −90.000.
- **Not verified:** how it feels in motion. Stills cannot judge a saunter; the user should play it. The pier gap is still crossed at 62 cm/s in both cameras (checks pass).
- **Not integrated into main.** The launcher still shows `2fa74eb`.

## Thirtieth pass — wider, fuller legs for the saunter

- **User feedback:** the legs "need to be wider especially the thighs", for "a better swagger saunter that looks less like a waddle" (not a drastic change). New reference `References/ArtDirection/Chuck-Dock-Saunter.png` (SHA-256 `CD5B5001D911CD30212617B0A35FA0950234273D23618D1D454E9A56EE8D838B`); the other two images were the existing turnaround and run sheet.
- **Source commit:** on top of `2e7287d`, branch `codex/claude-character`.
- **Measured** on the turnaround: the thighs are about 17 cm across both legs and about 7–8 cm thick, staying full to the knee. Ours were a 7.4 cm haunch tapering quickly, with the hips at ±6 cm. The hip height already matched (crotch about 19 cm), so leg length is unchanged.
- **Changes:**
  - `chuck_v1_shape.LEG_SPREAD = 1.1`: the leg chain (thigh, calf, foot, toes, `ik_foot_*`) spreads in Y, so the hips sit at ±6.6 cm. Paws follow `H('foot_*')`.
  - New `leg_radius`: a full haunch peaking just below the jacket hem (4.6 cm), thick to the knee, then a slimmer shin.
  - Jacket hem flared 0.5 cm so the haunch doesn't cut through.
  - Walk: pelvis sway ±0.45 cm (was 0.7) and swing-side drop 3.2° (was 4.5°); a swagger, not a waddle.
  - Groom: shins (below `Z(11)`) are short and sleek with no guard hairs; the fluffy thighs are kept.
  - `DockGameMode` hip check and `docs/RIG-CONTRACT-V1.md` updated to (−2, −6.6, 22.5).
- **Verified:**
  - `check_v1.py` PASS; `Import-ChuckGroom.ps1 -Review` all steps (rest pose < 0.01 cm).
  - Packaged `-MotionCapture` and `-NoGroom`: **50/50**; uncapped: 0 failures.
  - Contact slip 0; stop 14.6–15.0 cm.
  - A bake retry was needed after the OneDrive write lock on `T_Chuck_ORM.png`.
  - Evidence: `Review/goal_compare_legs.jpg`, `Review/motion_{WalkLoop,Idle}.png`.
- **Not integrated into main.** A worktree launcher receipt was written so the user can play it.

## Thirty-first pass — a more exaggerated, brisk swagger

- **User request:** a swagger that's "a little more exaggerated" and still brisk, like an "im finna whoop your ass" walk.
- **Source commit:** on top of `023986e`, branch `codex/claude-character`.
- **Research used:**
  - [PMC5283505](https://pmc.ncbi.nlm.nih.gov/articles/PMC5283505/): swagger is a large relative thorax–pelvis twist, which correlates with rated aggression.
  - Johnson & Tassinary (2007, via PsyBlog): a masculine walk shows a rolling shoulder dip.
  - Envato Tuts+ walk-cycle notes: chest out, arms carried wide with bent elbows, broad swings and bounce.
  - A Nature Scientific Reports intimidation paper required a login and was not used.
- **Changes** (`Tools/build_chuck_v1.py` `WALK` and `carriage`):
  - Brisker gait: 72 cm/s, a 14-frame (0.467 s) cycle, stance 0.6 and 2 cm lift. `ChuckClipData.h` was regenerated, so the runtime speed, braking and stance follow.
  - Pelvis yaw is 7°, with a counter-twisting chest of 9° and 12° (spine_02 and chest). The swing-side shoulder rolls and dips 5°.
  - Chest out 2° more, with the chin level. The head counter-yaws 11° so the gaze stays down the dock. The bounce is bigger, plus a small nod.
  - Arms swing 22°, carried 11° out from the body, with elbows bent a further 12°.
  - The hands close loosely (curl 16 + 18k). Real fists would need finger joints the v1 rig lacks: one `fingers_*` bone bends all four fingers at the knuckle, and a higher curl read as pointing.
- **Verified:**
  - `check_v1.py` PASS; `Import-ChuckGroom.ps1 -Review` all steps.
  - Packaged `-MotionCapture` and `-NoGroom`: **50/50**; uncapped with and without groom: 0 failures.
  - Contact slip 0. Stop 17.0 cm, or about 17.5 cm uncapped, which matches the new StopTravel.
  - Evidence: `Review/motion_{WalkLoop,Idle}.png`, plus the refreshed textured/Unreal review images.
- **Remaining flaws:**
  - No true fists without extra finger bones (a rig-contract change).
  - Sleeves still hide much of the elbow bend from the side.
  - Not tested with a gamepad by a human.
- **Not integrated into main.** A worktree launcher receipt was written.

## Thirty-second pass — GTA-style continuous orbit camera

- **User request:** make the camera work like GTA, with no button press needed to go between the two views.
- **Source commit:** on top of `20da1f2`, branch `codex/claude-character`.
- **Changes** (`ChuckCharacter.cpp/h`):
  - `bElevated`/`CameraBlend` are replaced by one orbit pitch (`LookPitch`, smoothed at rate 14). Mouse/right-stick Y drives it at all times; the stick runs at 80°/s.
    - −48° is the old elevated view (400 cm boom, pivot 16 cm, FOV 65). Down to −60° climbs a little higher.
    - −5° is the old rat-height view (220 cm, pivot 22 cm, FOV 78, lens about 76 cm).
    - Boom length, pivot and FOV blend linearly in between.
    - Above −5° the boom stays level and only the lens tilts up, to +30°, so it never dips under the pier.
  - Auto-follow: about 1.2 s after the last look input, while grounded and walking away from the camera, the yaw eases behind Chuck. The rate is 1.5/s × alignment × speed fraction; strafing or walking toward the lens is left alone.
  - Recenter and reset keep the chosen height.
  - The `Camera` action and its C/Y mappings are removed from `DefaultInput.ini`. `ToggleCamera()` stays as a preset snap for the smoke-test and capture stages. `IsElevated()` now means the upper half of the orbit.
  - HUD text and docs (`PLAYTEST.md`, `PROTOTYPE.md`) are updated.
- **Tests** (`DockGameMode.cpp`, now **51** checks; `Verify-Package.ps1` expects 51, or 50 with `-NoCapture`):
  - Stage 5 holds right stick up until the orbit reaches rat height, replacing the C key.
  - Stage 6 holds right stick down to elevated, replacing the Y button.
  - The jump test measures the orbit pivot, not the lens, since look pitch moves the lens freely.
  - New stage 53: with the orbit 35° off his heading, walking brings it back behind him (measured −1.1°).
  - Simulated MouseY axis events were not sampled in capped runs, so the orbit tests use the gamepad stick. Real-mouse orbiting is untested by automation.
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **51/51**; uncapped with and without groom: 0 failures.
  - Jump pivot travel about 6 cm; rat lens 75.8 cm; follow offset −1.1° to −1.2°.
- **Remaining:**
  - Not played by a human with a mouse or physical controller.
  - Auto-follow strength and delay, orbit speed and the default start height (elevated) are first guesses to tune by feel.
  - No GTA-style pitch auto-return; the chosen height is kept.
- **Not integrated into main.** A worktree launcher receipt was written.

## Thirty-third pass — roll and side jump (first pass)

- **User request:** "Integrate and begin roll and side jump"; run and running jump will come later.
- **Integration:** main fast-forwarded to `fd78cc8`, then rebuilt in the main checkout: Verify-Package `-MotionCapture` **51/51**; receipt `fd78cc8`. Recorded as `f7407d8` (HANDOFF Update 5).
- **Source commit:** on top of `f7407d8`, branch `codex/claude-character`.
- **Clips** (`Tools/build_chuck_v1.py`, all root-fixed; manifest-driven import, no contract change):
  - **`Roll`** (25 frames, 0.8 s):
    - A dive off both paws (stance 0–0.08 s), a tight tuck and one revolution about the ball centre (0.1–0.54 s). The paws plant at 0.56 s on the planner's world positions, and he rises into the aplomb stance.
    - Speed: smoothstep up to 215 cm/s, then down to rest by 0.62 s. Travel is 101 cm (`capsule_travel_cm_per_frame`).
    - While off the paws, the tucked ball is ground-fitted to the deformed mesh each frame (`body_min_z`), so it rests on the floor. The tail wraps round his left hip.
  - **`SideJumpLeft` / `SideJumpRight`** (25 frames, 0.8 s):
    - Takeoff: he loads onto the far leg, and the runtime launches him at 0.1 s (190 cm/s sideways, 158 cm/s up, 16 cm apex, 0.4 s air, about 77 cm).
    - In the air he leans 20° into the flight; the head stays level, the lead arm is flung out, the lead leg reaches, the trail leg tucks and the tail counter-swings.
    - He lands on both paws in the aplomb stance at 0.5 s. The hips carry on 3 cm, then settle.
    - Each side is authored separately rather than mirrored, so the left-corner cigarette doesn't flip sides.
  - `check_v1.py` PASS, including the no-ground-penetration check for all three clips (min z −0.002 cm).
  - `review_motion.py` now takes clip names.
- **Runtime** (`ChuckCharacter.cpp/h`, `ChuckClipData.h` via `gen_chuck_clip_data.py`):
  - New `Dodge` action: C / Xbox B (`DefaultInput.ini`). `DodgeToward(stick)`: a stick held mostly sideways (camera-relative) gives a side jump that way, squared up down the camera; otherwise he rolls toward the stick, or straight ahead with no stick.
  - Roll:
    - It enters the dive at the point where its speed matches his current pace.
    - Velocity is set closed-loop from distance actually travelled, because movement ticks before the character. Travel is exact even through frame hitches.
    - Braking is off during the dodge, and foot IK is off while tucked.
  - Side jump: `LaunchCharacter` at takeoff; the clip holds just before land while airborne; the capsule stops at touchdown.
  - Both dodges end in the aplomb stance, then Idle, or WalkStart if the stick is held. Movement input is ignored during a dodge but still read.
- **Tests** (`DockGameMode.cpp`, now **57**; `Verify-Package.ps1` expects 57, or 56 with `-NoCapture`):
  - Stage 54 (roll): travel vs authored within 8 cm, paws hold, recovers to Idle.
  - Stage 55 (side jump): lateral vs ballistic within 12 cm, facing kept, paws hold, recovers.
  - Stages 56/57: unmeasured capture replays (`Roll_*.png`, `SideJump_*.png`), because screenshots stall frames.
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **57/57**; uncapped with and without groom: 0 failures.
  - Roll 101.050 cm (authored 101.050), paw slip 0.
  - Side jump 79.6–82.3 cm (ballistic 76.8; touchdown is detected up to a frame late), apex 14.6 cm, yaw 0, paw slip 0.
  - Evidence: `Review/motion_Roll.png`, `Review/motion_SideJumpLeft.png`, `Review/runtime_agility.jpg` (in-game captures, no-groom run).
- **Remaining flaws / next:**
  - Roll direction snaps the facing instantly.
  - The capsule isn't shrunk, so he can't roll under low obstacles.
  - No mid-air roll or side-jump chaining; a side jump off an edge holds the pre-landing pose until he lands.
  - The roll's contact point is the crown of the tucked head at about 90°, not the shoulders.
  - The keyboard/pad Dodge mapping is not exercised by automation (tests call `DodgeToward`).
  - Not played by a human.
  - Run and running jump are not started. Plan: a run gait from `Run-Profile`/`Run-Cycle-Sheet` (hold-to-run), a speed blend in the distance-matched gait, then a running-jump takeoff from the run phase.
- **Not integrated into main.** A worktree launcher receipt was written.

## Thirty-fourth pass — run (first pass)

- **User request:** "Integrate and run".
- **Integration:** main fast-forwarded to `08a425b` (roll and side jump). Main-checkout Verify-Package `-MotionCapture` **57/57**; receipt `08a425b`. Recorded as `68bc4ee` (HANDOFF Update 6).
- **Source commit:** on top of `68bc4ee`, branch `codex/claude-character`.
- **Clip:** `RunLoop` (12 frames, 0.4 s), matched to the two run references.
  - Speed from dynamic similarity: Froude ~1.5 on the 22.5 cm leg gives ~190 cm/s. The cycle is the slower end of the sqrt(leg length) scaling, for the reference's long, leaping stride: 38 cm steps. Duty factor 0.3, so there is a flight phase.
  - Forefoot strike with the heel up; the swing is a Hermite spline that meets the ground at running speed (no contact slip).
  - The trailing leg extends back, then the heel kicks up, the knee drives high and the paw reaches forward.
  - 17° lean from the hips, with the neck and head countering to keep the gaze level. The arms pump ±40° with elbows at about 90° and loose fists, and the tail streams back.
  - Same phase convention as WalkLoop.
  - Max reach 0.934; `check_v1.py` PASS, including ground.
- **Runtime:**
  - `ChuckAnimInstance` gets a run layer (`ClipRun`/`WeightRun`) blended over the A/B result.
  - `ChuckCharacter`:
    - `Run` action: Left Shift / Xbox LB, hold. MaxWalkSpeed switches between WalkSpeed and RunSpeed.
    - In Loop, the phase advances by travel over a stride lerped by the speed blend. The run weight is smoothed at rate 10 and is 0 outside Loop.
    - Stance windows switch to the run's once the weight passes 0.5.
    - Releasing the stick mid-run brakes at 700 cm/s² down to the saunter, then the usual coast and WalkStop.
    - Landing at a run goes straight into the stride.
  - `ChuckClipData.h` gains RunSpeed, RunPeriod, RunStride and RunStanceFraction.
- **Tests:** now **60** (`Verify-Package.ps1` expects 60, or 59 with `-NoCapture`).
  - Stage 58: run speed and blend; running paws hold.
  - Stage 59: a stop from a run reaches Idle within 60 cm.
  - Stage 60: an unmeasured side-view capture (`Run_*.png`).
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **60/60**; uncapped with and without groom: 0 failures.
  - Run 190.000 cm/s, blend 1.0, paw slip 0.
  - Stop from a run: about 50 cm to Idle.
  - Evidence: `Review/motion_RunLoop.png`, `Review/runtime_run.jpg`.
- **Remaining:**
  - No RunStart/RunStop clips: starts go through WalkStart, and stops brake through the blended loop.
  - The running jump is the ordinary jump clip with momentum; a dedicated leap (split legs, reach) and a run-landing clip are next.
  - Dodges entered from a run drop the run layer over about 0.1 s.
  - The jacket has no follow-through at speed.
  - The Shift/LB mapping is not exercised by automation (tests call `SetRunHeld`).
  - Not played by a human.
- **Not integrated into main.** A worktree launcher receipt was written.

## Thirty-fifth pass — roll back into the run; keyboard side jumps

- **User feedback** (played `724ec23`):
  - "When running then rolling i want to be able to quickly resume running after the roll finishes, rn its a long pause."
  - "Can i right side-jump on pc controls? only left side-jump is working."
- **Source commit:** on top of `724ec23`, branch `codex/claude-character`.
- **Roll carry** (`ChuckCharacter.cpp`): with the stick held, the roll keeps at least the current pace (run or saunter) after the dive. At the paw plant (`RollPlant`, 0.56 s, generated) it goes straight into the stride, skipping the 0.25 s rise, with the run blend set to the speed. With no stick held, the full recovery to the aplomb stance is unchanged. Side jump with the stick held: he walks or runs on 0.15 s after touchdown instead of at the clip end.
- **Run layer:** now attached to the WalkLoop sample (A or B) in `ChuckAnimInstance`, so fades into and out of the stride (rolls, landings) fade the whole run pose. That fixes the run pose showing over the start of a dodge.
- **Right side jump on keyboard:** the mappings were correct. Two causes, both fixed:
  - Pressing D from standstill starts a turn in place, and `DodgeToward` ignored dodges during a turn. A dodge now cuts the turn short.
  - Action events dispatch before that frame's axis events, so D and C pressed together read the stick as neutral (a forward roll). `Dodge()` now reads `Right`/`Forward` from the InputComponent directly.
  - `ResetToDock` clears the stored stick.
- **Tests:** now **64** (`Verify-Package.ps1` expects 64, or 63 with `-NoCapture`).
  - Stage 61: roll out of a run with the stick held; back to ≥95% run speed in the stride within 0.75 s; paws hold after.
  - Stages 62/63: the real key path. Input is enabled, D (or A) is held from standing, C is pressed 0.15 s later, and sideways travel along Chuck's right is measured.
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **64/64**; uncapped with and without groom: 0 failures.
  - Back to run 0.50 s after the roll starts, paw slip 0.
  - Keyboard D+C: +79–82 cm (right); A+C: −79–82 cm (left).
  - The existing roll (101.050 cm) and side jump are unchanged.
- **Remaining:** not re-played by the user. The earlier run and running-jump notes still apply. **Not integrated into main.** A worktree launcher receipt was written.

## Thirty-sixth pass — running jump

- **User request:** "Running jump".
- **Source commit:** on top of `020baee`, branch `codex/claude-character`.
- **Clip:** `RunJump` (15 frames), a split leap from the run references.
  - Lead leg reaching with the knee high; trail leg stretched back near horizontal.
  - Opposite arm forward with the elbows opening; chest open, body extended 3 cm, tail up for balance.
  - It is built as RunLoop frame 0 plus the leap offsets, faded in by 0.2 and out from 0.6 of the flight, so its last frame is exactly RunLoop frame 0 (foot_L touchdown).
  - Reach 0.934; `check_v1.py` PASS.
- **Runtime:**
  - A jump with the run blend above 0.5 (and rising, not a fall) becomes a leap. Vertical speed is set to `RunJumpVerticalSpeed` (190 vs 170 for the standing jump).
  - Clip time comes from the flight progress `(vz0 - vz) / 2vz0`, never running backwards, so the pose fits any landing height.
  - On touchdown with the stick held (acceleration or raw stick), he goes straight into Loop at phase 0 with the run blend set to the speed. Otherwise JumpLand, braking at 2500 cm/s² to shed the run before the 0.1 s landing lock.
- **Tests:** now **67** (`Verify-Package.ps1` expects 67, or 66 with `-NoCapture`).
  - Stage 64: running jump with the stick held. Checks: it is a leap, distance vs ballistic within 12 cm, it lands in Loop at ≥90% run speed, and paws hold 0.1–0.6 s after landing.
  - The run capture replay (stage 60) adds `RunJump_*.png`.
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **67/67**; uncapped with and without groom: 0 failures.
  - Leap 94.7–98.2 cm (ballistic 92.1; landing is detected up to a frame late), apex 23.0 cm, lands into the run, paw slip 0.
  - Evidence: `Review/motion_RunJump.png`, `Review/runtime_runjump.jpg`.
- **Remaining:**
  - The takeoff has no anticipation or phase matching; it launches from whatever stride phase he is in, with a 0.08 s fade.
  - No dedicated run-landing absorb beyond the stride's own compression.
  - A leap off an edge into a long fall holds the touchdown pose.
  - Not played by a human.
- **Not integrated into main.** A worktree launcher receipt was written.

## Thirty-seventh pass — run is a tap, so jumping mid-run works on any keyboard

- **User feedback** (played `f2c7d48`): the running jump "doesnt work while im holding down shift, so i have to let off of shift for long enough to jump but not long enough to stop running".
- **Source commit:** on top of `f2c7d48`, branch `codex/claude-character`.
- **Diagnosis:**
  - The engine's legacy chord matching is permissive about Shift (`PlayerInput.cpp` `GetChordsForKeyMapping`: `bShift == false || IsShiftPressed()`).
  - A new real-key test (stage 65: LeftShift + W held, then SpaceBar through `PlayerController::InputKey`) leaps at 190 cm/s.
  - So the game receives the combination. The likely cause is keyboard ghosting/rollover: Shift + a movement key + Space is a combination many keyboards drop. That can't be fixed in software.
- **Change:**
  - Run is now a latch: tap Shift / LB to run and it keeps running with Shift released. Tap again to saunter; coming to a stop (entering Idle) also ends it.
  - The IE_Released binding is removed. HUD and `PLAYTEST.md` updated.
  - Tests that enable live input lock mouse and stick look (`SetLookLocked`) so a real mouse on the machine can't steer them; the key run test starts at X −380.
- **Tests:** now **69** (`Verify-Package.ps1` expects 69, or 68 with `-NoCapture`).
  - Stage 65 through the real keys: Shift tapped and released, W held; Space at 1.2 s leaps at full run speed. A second Shift tap drops back to the saunter (72 cm/s).
- **Verified:** packaged `-MotionCapture` and `-NoGroom`: **69/69**; uncapped with and without groom: 0 failures.
- **Not verified:** the user's physical keyboard. **Not integrated into main.** A worktree launcher receipt was written.

## Thirty-eighth pass — push, then the slash (claw scratch)

- **User request:** "push and do slash".
- **Push:** main `06be7c6` pushed to `origin` (`43bf1eb..06be7c6`, 431 LFS objects, about 971 MB).
- **Source commit:** on top of `06be7c6`, branch `codex/claude-character`.
- **What a slash is:** Chuck's canonical attack is a claw scratch (`References/Original/PHASE-2.md`: "brief forward movement, paw scratch"; PROJECT-BRIEF lists "Scratching" as a later prototype goal), so there is no weapon.
- **Clips:** `SlashRight` / `SlashLeft` (16 frames, 0.5 s).
  - Wind-up to 0.12 s: the striking paw cocks beside the ear and the torso turns away (22°).
  - Strike to 0.2 s: the torso unwinds hard (30°) and the paw rakes on a Hermite wrist path, claws leading (fingers opened), to in front of the chest.
  - Follow-through to 0.3 s: the paw finishes low across the body. Recovered by 0.5 s.
  - The opposite paw steps in 10 cm (a boxer's cross) and the striking side follows. The other arm pulls back as a guard, the head counters the twist to hold the gaze, and the tail counters.
  - The striking arm uses Poser.arm IK blended from the carriage arm.
  - Events: `chain_from` 0.27, after the paw has crossed the midline. `check_v1.py` PASS.
- **Runtime:**
  - `Slash` action: left mouse button / Xbox X. F stays recenter.
  - Standing (Idle/Stop/Land/Turn/Start under 20 cm/s): `EGait::Slash` plays the clip as the base. The capsule follows the 10 cm step-in closed-loop, with stance windows from the manifest.
  - On the move (walk, run, air): an upper-body layer, the spine_01 subtree, via `FChuckAnimProxy::BlendUpper`, over the stride. It fades in over 0.05 s and out over the last 0.15 s, and there are two slots so a chained paw fades the previous one out over 0.08 s.
  - A press during a slash buffers the other paw, which chains at 0.27 s. A dodge can cancel a standing slash.
  - Movement input and braking are owned by the slash while standing (`OwnsCapsule`).
- **Tests:** now **76** (`Verify-Package.ps1` expects 76, or 75 with `-NoCapture`).
  - Stage 66: standing slash plus a buffered second press. Checks: the right paw rakes from its own side across the midline at over 250 cm/s; the left paw chains; the step-in matches the manifest (chained: 17.8 cm expected); it settles to Idle; paws hold.
  - Stage 67: slash while running. The rake crosses, and the stride holds 190 cm/s with paws holding.
  - Stage 68: the left mouse button slashes (real key path, look locked).
  - Stage 69: capture replay (`Slash_*.png`).
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **76/76**; uncapped with and without groom: 0 failures.
  - Standing: right paw y +14.1 → −3.3 to −4.2 cm, peak 650–1010 cm/s. Step 17.9–18.3 cm (expected 17.8). Paw slip 0.
  - Running: rake +16.4 → −4.6 cm, speed stays 190. Paw slip 0.
  - An 11 cm "jump" seen in uncapped runs was the first frame after the test's reset teleport (a stale pose), not a pop. The measurement now skips 0.05 s.
  - Evidence: `Review/motion_SlashRight.png`, `Review/motion_SlashLeft.png`, `Review/runtime_slash.jpg`.
- **Remaining:**
  - No hit detection, targets or breakables (combat encounters remain out of scope unless the user asks).
  - The slash keeps the current facing; there's no aim toward the stick or camera.
  - The layered slash has no pelvis or leg involvement.
  - No claw trail or FX.
  - The in-game capture is small; it needs the user's eye.
- **Not integrated into main.** A worktree launcher receipt was written.

## Thirty-ninth pass — slash as a sword-like arc (Wolverine)

- **User feedback:** "The slash reads too much like a punch, make it more like an almost sword slash, like how Wolverine from the x-men would slash".
- **Source commit:** on top of `c355248`, branch `codex/claude-character`.
- **Diagnosis:** the wrist went almost straight from beside the ear to in front of the chest (a jab path), with a short sweep of about 17 cm.
- **Change** (`slash_clip` in `Tools/build_chuck_v1.py`):
  - The wrist now rides a wide, flat arc at about 0.85 reach around the turning shoulder. Offsets are relative to the posed shoulder: high, back and outside (0.12 s) → out wide (0.17) → straight ahead at full extension (0.22) → across (0.27) → low on the far side (0.34).
  - The elbow pole is down and slightly out so the arm stays long. Fingers are fully straight, so the claws lead like the blade.
  - Wind-up twist 30° and unwind 45°. The striking shoulder drops into the cut (chest X 6°), with a deeper sink (2.8 cm) and lean (10°).
  - The strike runs 0.12–0.26 s (a sweep, not a snap). Clip 0.55 s; step-in 12 cm; the rear paw follows at 0.28–0.42 s (it overreached at 1.06 with a 14 cm step); chain at 0.3 s.
  - Max leg reach 0.918.
- **Verified:**
  - `check_v1.py` PASS. Packaged `-MotionCapture` and `-NoGroom`: **76/76**; uncapped with and without groom: 0 failures.
  - Right paw sweep +23.9 → −8.6 to −10.1 cm across the body (was +14 → −3), peak 7.6–11.6 m/s.
  - Chained step 22.1 cm (expected 22.1). Running slash keeps 190 cm/s. Paw slip 0.
  - Evidence: `Review/motion_SlashRight.png`, `Review/motion_SlashLeft.png`, `Review/runtime_slash.jpg`.
- **Remaining / offered:**
  - A claw-trail streak for the strike window (a Blender-made crescent mesh with a fading material, no plugin) would sell the blade read further.
  - The hand's roll about the forearm is not art-directed (palm-forward on the wind-up can read as a wave).
  - No hit detection.
- **Not integrated into main.** A worktree launcher receipt was written.

## Fortieth pass — faster slash; held flurry at random intervals

- **User request:** "make it faster, but no streak. then make it so he can attack with both paws one then the next, but at random intervals instead of directly back and forth to make it more natural".
- **Source commit:** on top of `a831bb9`, branch `codex/claude-character`.
- **Faster:** `SLASH_RATE` 1.375. The arc is authored on the 0.55 s base timeline and the clip plays it in 0.4 s (13 frames), so the cut takes about 0.1 s. Manifest travel, stance windows and events are scaled to match; chain_from is 0.218 s. No streak or FX.
- **Flurry** (`ChuckCharacter`):
  - The Slash action now has a released binding. Holding it, or pressing again, chains the other paw at `NextChainAt` = `SlashChainAt` plus a random pause of 0–`SlashJitter` (0.16 s), drawn from a `FRandomStream` seeded at BeginPlay.
  - Paws alternate, and the timing varies from strike to strike. A tap is a single slash. The flurry works standing (stepping in) and on the move (upper-body layer).
  - Tests use `Slash()` + `SlashReleased()` for taps.
- **Tests:** now **78** (`Verify-Package.ps1` expects 78, or 77 with `-NoCapture`).
  - Stage 66's chained-step check accepts the whole random-pause window.
  - New stage 70: slash held for 1.6 s. Checks: ≥4 strikes, paws alternate, every gap within [chain − 0.03, chain + jitter + 0.05] and the gaps are not all equal, paws hold, and it settles to Idle after release.
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **78/78**; uncapped with and without groom: 0 failures.
  - Flurry: 6 strikes in 1.6 s; gaps 0.23–0.38 s, varying run to run. Paw peak 9–13 m/s. Paw slip 0.
  - Evidence: `Review/motion_SlashRight.png`, `Review/motion_SlashLeft.png`, `Review/runtime_slash.jpg`.
- **Remaining:**
  - A standing flurry steps forward about 11 cm per strike (an advance).
  - Paws strictly alternate; the order is not randomized.
  - No hit detection.
  - Not played by the user.
- **Not integrated into main.** A worktree launcher receipt was written.

## Forty-first pass — flurry: random paw order, steady timing

- **User request:** "Make the ordering random not the timing".
- **Source commit:** on top of `38207ea`, branch `codex/claude-character`.
- **Change** (`ChuckCharacter`):
  - The random pause is gone: every follow-up chains at `SlashChainAt` (0.218 s).
  - `PickPaw` chooses each strike's paw: the first 50/50; each follow-up repeats the last paw with probability 0.5 unless it just repeated, so there are never three of one paw in a row.
  - `SlashStrikes` counts strikes, and `SetSlashSeed` gives tests a fixed sequence.
  - No clip changes.
- **Tests:** still **78**.
  - Stages 66/67 are paw-agnostic: they follow whichever paw strikes, with outward as +. The second press must chain another raking strike.
  - Stage 68 accepts either paw.
  - Stage 70 (seed 20260928): ≥5 strikes, both paws, at least one repeat, no run over 2, and every gap within [chain − 0.02, chain + 0.05].
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **78/78**; uncapped with and without groom: 0 failures.
  - Flurry order LLRLLRR(L); gaps 0.218–0.233 s. Paw slip 0.
- **Remaining:**
  - When the running slash draws the left paw, the rake only just crosses the midline (outward min −1.0 cm vs −6 for the right). The run pose's arm and twist fight the left layer; worth tuning if it reads weak.
  - No hit detection.
  - Not played by the user.
- **Not integrated into main.** A worktree launcher receipt was written.

## Forty-second pass — parkour phases 1–2: practice yard, wall run, wall jump

- **User request:** start the parkour angle. The vision: "feels like you're doing something difficult but it's not actually that hard". Wall run up about 3 steps, wall jumps back and forth between walls, then hanging, pull-up and shimmy (phases 3–4).
- **Research and plan:** delivered in chat (Sly Cooper, AC Unity, Mirror's Edge, Prince of Persia, Brink SMART, Celeste forgiveness). The user approved the practice yard (dock- or Waterdeep-themed, kept and dressed later) and all recommendations:
  - the stick held toward the wall triggers the wall run;
  - the wall jump goes away from the wall and is steerable;
  - low ledges get an automatic mantle (later).
- **Source commit:** on top of `fdde88a`, branch `codex/claude-character`.
- **Practice yard** (`DockGameMode.cpp`), on the quay's empty south strip, dock-built (the world pause is lifted for this at the user's request):
  - Cargo chimney: two 60×65×240 cm stacks of `SM_DockCrate` (collision box plus four crate props each, slight yaw) at X −390 and −230. The faces at −360 and −260 leave a 100 cm gap.
  - Stone harbour wall: 180×50×115 cm at (−40, −360), with a stone coping course and a rope coil on top.
- **Clips:**
  - `WallRun`: a 10-frame loop, stride 30 cm, stance 0.55. Paws plant on the wall plane at x = 13 (soles on it, toes up); arms reach up alternately, head up, hips in. The review sheet shows the wall.
  - `WallKick`: 7 frames. Paws flat on the wall behind drive him off, then tuck.
  - Reach 0.86/0.87. `review_motion.py` now handles short clips and draws the wall for WallRun.
- **Runtime** (`ChuckCharacter`):
  - `JumpPressed` replaces the direct `ACharacter::Jump` binding. On a wall, or within `WallCoyote` (0.15 s) of leaving one, it wall-jumps. In the air it buffers (`WallBuffer` 0.15 s) for a wall reached just after. On the ground it jumps.
  - `TryEnterWallRun` runs while airborne:
    - a 4 cm sphere sweep reaching `WallReach` (12 cm) past the capsule, along the stick (or the velocity after a wall jump);
    - the surface must be near-vertical (|Nz| < 0.3) and within 60° of head-on;
    - it can't be the last wall (dot > 0.7) until he lands;
    - he must not be falling faster than 250 cm/s.
  - `EnterWallRun`: snugs him to the wall (0.5 cm), faces it and switches to flying movement. The rise is linear-decay, 45 cm over 0.45 s (three 0.15 s steps). The step phase follows the height gained. Paw IK is off and he presses 30 cm/s into the wall.
  - The run ends at the time limit, when the wall ends at chest height, or when the stick is pulled away (not after a wall jump, since the stick then points at the old wall). He peels off at 40 cm/s.
  - `WallJump`: away from the wall plus 0.6 × the stick along the wall, at 260 out and 230 up. He turns to face the jump and plays WallKick then JumpLoop; the next wall is caught without the stick.
  - Landing refreshes all walls. Dodges are blocked on the wall.
  - HUD and `PLAYTEST.md` updated (controls and a yard section).
- **Tests:** now **82** (`Verify-Package.ps1` expects 82, or 81 with `-NoCapture`).
  - Stage 71: jump into the harbour wall pushing toward it. It must run up (rise ≥ 0.8 × 45 cm), last three steps (0.45 ± 0.08 s), drop off and land, with only one run on the same wall.
  - Stage 73: a jump 0.08 s after peeling off still wall-jumps.
  - Stage 72: cargo chimney bounces with no stick after the first wall. Runs must alternate walls (≥3), with ≥3 jumps and a climb of ≥150 cm.
  - Stage 74: chimney capture replay (`Chimney_*.png`).
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **82/82**; uncapped with and without groom: 0 failures.
  - Wall run 45.1–45.7 cm in 0.450–0.453 s; he drops and lands; the coyote wall jump counts.
  - Chimney: walls A→B→A, 3 jumps, 204–205 cm climbed (about 70 cm per bounce, which reaches the 240 cm stack tops).
  - Evidence: `Review/motion_WallRun.png`, `Review/motion_WallKick.png`, `Review/runtime_chimney.jpg`.
- **Remaining / next (phase 3):**
  - At the top of the stacks he can't get onto them. The ledge grab (auto when the paws reach a top edge), hang, pull-up, drop and low-ledge mantle come next; the harbour wall is built for them.
  - Tuning (3 steps, about 70 cm per bounce, kick strength) awaits the user's feel.
  - The camera doesn't frame wall runs specially.
  - Hands don't touch the wall (reaching only).
  - Not played by the user.
- **Not integrated into main.** A worktree launcher receipt was written.

## Forty-third pass — parkour phase 3: ledge grab, hang, pull-up, drop, mantle

- **User request:** "next" (phase 3 of the approved parkour plan).
- **Source commit:** on top of `a093600`, branch `codex/claude-character`.
- **Clips** (`Tools/build_chuck_v1.py`):
  - `Hang`: 36-frame loop. The capsule centre sits `HANG_DROP` 22 cm below the top and snug to the wall (face at x = 15.5); wrists grip the edge by arm IK, fingers curl over, paws scrabble on the wall, gentle sway.
  - `PullUp`: 0.7 s. The capsule follows `pull_path` (rise 54.5 cm to standing, advance 34 cm onto the top). The grip is world-locked until 0.45 s; the left knee comes over the edge and plants at 0.58, the right follows at 0.65. Both end in the aplomb stance.
  - `Mantle`: 0.35 s, authored for a 25 cm step: a two-footed hop with paws on the top, landing standing. The runtime scales the rise to the real step.
  - Leg reach 0.82 / 0.94 / 0.92. `check_v1.py` PASS (a first pass left a paw 1 cm into the top; fixed).
  - `review_motion.py` draws the ledge block, moved against the authored capsule path; this also fixed a variable shadowing.
- **Runtime** (`ChuckCharacter`):
  - `FindLedge` traces down 8 cm past the face for a walkable top in a band relative to his centre. It confirms the face really ends there (nothing just above the top) and tests whether there is room to stand.
  - Grab happens automatically:
    - during a wall run, when the top is 5–45 cm above centre;
    - in the air, when a probe at chest or hips (along the stick, or the velocity after a wall jump) finds a wall whose top is −15 to +45 cm, while he isn't falling faster than 300 cm/s. Any wall counts, including the last one.
  - The hang snaps him in over 0.12 s. Holding toward the wall for `PullUpHold` (0.2 s), or pressing jump, pulls up if there is room; pulling away or dodge lets go (0.4 s re-grab cooldown); jump while pulling away wall-jumps backward.
  - `TryMantle`: walking or standing with input into a ledge 6–40 cm above the feet with room, via a knee-height probe. The capsule follows the clip path scaled to the step, then goes to Idle or Start.
  - Slash, dodge and movement input are owned or blocked while hanging or climbing.
  - Yard: a 60×60×30 cm stone mooring plinth with an iron ring at (150, −330).
- **Tests:** now **87** (`Verify-Package.ps1` expects 87, or 86 with `-NoCapture`).
  - Stages 71/73 moved to crate stack A's north face, a wall too tall to top out.
  - Stage 72 (chimney) additionally catches a stack top and jumps up onto it (≥2 kicks now, since the top catch can come before the third).
  - Stage 75: harbour wall with the stick held: grab, then pull up to stand on the top.
  - Stage 76: stick released at the grab: still hanging and steady 1 s later, then pulling away drops him to the quay.
  - Stage 78: walking into the plinth mantles onto it.
  - Stage 79: side-view capture replay (`Ledge_*.png`).
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **87/87**; uncapped with and without groom: 0 failures.
  - Wall run unchanged (45 cm, 0.45 s).
  - Chimney A→B→A catches a top and ends standing on the 240 cm stack (+2.15 cm, the CMC floor gap).
  - Harbour wall: hang at z 93–95, pull-up to 149.65 (top 115 + 32.5 + 2.15).
  - Hang held steady (93.00 → 93.00) and the drop lands.
  - Mantle onto the plinth ends at 64.65.
  - Evidence: `Review/motion_Hang.png`, `Review/motion_PullUp.png`, `Review/motion_Mantle.png`, `Review/runtime_ledge.jpg`.
- **Remaining / next (phase 4):**
  - Shimmy along the edge; hands currently fixed on the grab point.
  - Corners.
  - Camera framing for hangs.
  - The snap-in can move him up to about 23 cm in 0.12 s.
  - Mantle speed at a run isn't blended (he stops, then hops).
  - The pull-up passes through the corner of the capsule's volume (the location is set without sweeping).
  - Not played by the user.
- **Not integrated into main.** A worktree launcher receipt was written.

## Forty-fourth pass — parkour phase 4: shimmy and hang camera

- **User request:** "Next" (phase 4 of the parkour plan).
- **Source commit:** on top of `e194472`, branch `codex/claude-character`.
- **Clips:** `ShimmyLeft` / `ShimmyRight`, 12-frame loops with a 16 cm stride, authored separately (no mirroring).
  - Each paw grips for half a cycle (world-locked, sliding back in mesh space) and reaches ahead 3 cm up / 2 cm out for the other half. The chest leans into the travel and the feet shuffle on the wall.
  - Leg reach 0.66–0.67. `check_v1.py` PASS.
- **Runtime:**
  - Shimmy: while hanging (after the 0.12 s snap), a stick more sideways than toward the wall (|side| > 0.4) moves the edge along his right vector at `ShimmySpeed` (45 cm/s).
  - Every step, `FindLedge` must find the edge continuing at the next point and 10 cm ahead of the lead hand, within ±8 cm of the hang height, and a slightly shrunk capsule sweep must be clear. Otherwise he stops, still hanging.
  - The loop phase follows the distance moved over `ShimmyStride`. It fades back to `Hang` when he stops. Pull-up and drop work as before from any point along the edge.
  - Camera: while hanging or climbing, after 0.3 s without look input the orbit yaw eases (rate 4/s) to face the wall, so the stick is intuitive: up climbs, left/right shimmies. Not during wall runs, since chimney kicks flip his facing.
- **Tests:** now **90** (`Verify-Package.ps1` expects 90, or 89 with `-NoCapture`). Stage 80: hang on the harbour wall with the camera 30° off, then the stick right:
  - the camera faces the wall within 5° after 1 s;
  - he shimmies 1.2 × 45 cm ± 12 with |dz| < 1;
  - holding on, he stops at the wall end (X 20–50) still hanging.
  - Capture stage 79 now hangs, shimmies right, then pulls up.
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **90/90**; uncapped with and without groom: 0 failures.
  - Camera −89.5° (target −90). Shimmy 54.0 cm in 1.2 s, dz 0.000.
  - Stops at X 39.5–40.0 (wall end 50, the lead-hand probe 10 cm ahead).
  - Evidence: `Review/motion_ShimmyRight.png`, `Review/runtime_shimmy.jpg` (camera swings behind; hang, shimmy, pull-up in game).
- **Remaining (phase 5, tuning):**
  - No corners (outer or inner); he stops at the end.
  - Numbers await the user's feel: 3 steps / 45 cm, about 70 cm per chimney bounce, kick 260/230, shimmy 45 cm/s, grab snap up to 23 cm, pull-up hold 0.2 s.
  - Mantle from a run isn't blended.
  - The yard's look is to be dressed later.
  - Not played by the user.
- **Not integrated into main.** A worktree launcher receipt was written.

## Forty-fifth pass — a bigger obstacle area: the cargo wharf

- **User request:** "Before we get to corners and turning I want a bigger obstacle area, expand the area".
- **Source commit:** on top of `46cf6cf`, branch `codex/claude-character`.
- **Scene** (`DockGameMode.cpp`): a stone wharf slab 800×600 cm (X −500..300, Y −1000..−400, top 0) joins the quay's south edge, with timber edge beams. Collision-true primitives in dock materials, to be dressed later. The course:
  - **Crate staircase** at X −440: `CrateColumn` collision boxes with stacked or scaled `SM_DockCrate` art, 30/60/120/180/230 cm tall, stepping south. That's mantle, mantle, then three jump-and-grab steps (60/60/50 cm up). A 40 cm-wide plank bridge crosses from the 230 column to the warehouse roof, with a rope coil.
  - **Warehouse:** a 300×220×230 plaster box with a flat walkable roof on a 300×40×115 stone plinth jutting from its north face (a 40 cm ledge: standing room for the 30 cm capsule). Visual-only timbers, beam, amber windows, door and dark roof trim.
  - **Sail loft:** 200×260×260 at X 70..270, leaving a 100 cm alley to the warehouse's east wall. Visual-only timbers, hoist beam and door.
  - **Knee-high field:** three 120×30×35 stone harbour walls with coping, four 35 cm bollards, two barrels (collision cylinders plus barrel art) and a 60 cm crate column.
  - **Boats:** two `SM_HarborBoat` props moored off the south and east edges.
  - `AChuckCharacter::SetOrbitPitch` added for captures.
- **Tests:** now **92** (`Verify-Package.ps1` expects 92, or 91 with `-NoCapture`).
  - Stage 82: run up the warehouse plinth and climb onto it (z = 115 + 32.5 ± 3).
  - Stage 83: bounce up the warehouse/sail-loft alley, catch a roof edge, pull up (z > 255).
  - Capture stage 84: `Wharf_080` from the south edge looking north, `Wharf_190` from the warehouse roof.
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **92/92**; uncapped with and without groom: 0 failures.
  - Plinth climb ends at z 149.65; the alley ends on the warehouse roof at z 264.65. All earlier parkour, run and dodge checks unchanged.
  - Evidence: `Review/runtime_wharf.jpg`.
- **Not verified:**
  - The crate staircase and the plank bridge are not traversed by an automated test.
  - The field's mantles are covered only by the generic plinth test.
  - A first overview capture from the yard collapsed onto the paving because the boom hit the harbour wall; it was moved.
  - Not played by the user.
- **Next (the user's queue):** corners and turning on ledges, then tuning. **Not integrated into main.** A worktree launcher receipt was written.

## Forty-sixth pass — expand the area more: Chandlers' Row and the Timber Yard

- **User request:** "Expand the area more".
- **Source commit:** on top of `bc76998`, branch `codex/claude-character`.
- **Scene** (`DockGameMode.cpp`), dock materials and collision-true primitives as before:
  - **Chandlers' Row:** quay slab X −500..300, Y −1800..−1000.
    - A `House` helper builds a box with visual roof trim, corner timbers, flush amber windows and a door on the east face. Three row houses at X −480..−260 are 180 (plaster), 230 (stone) and 280 cm (plaster) with 70 cm gaps, plus a 120×40×100 lean-to shed on the first house's north face.
    - Four 120×60×80 market stalls with visual posts and canopies.
    - A 30×600×120 garden wall with coping, reached from a 60 cm crate column.
    - A 350×180×90 customs terrace with visual rail posts, and a 30° ramp (120 wide) oriented with `FRotationMatrix::MakeFromYZ`.
  - **Timber Yard:** slab X 300..900, Y −1000..−400.
    - Lumber stacks 300×60 at 160, 200 and 90 cm with visual bands; the first two leave a 100 cm chimney.
    - A 120×120×250 crane tower 80 cm from the 200 stack, with visual legs, an arm over the water, a rope and a hanging crate.
    - Two barrels and a hand cart.
  - The wharf's two boats moved to the new outer edges.
- **Tests:** now **94** (`Verify-Package.ps1` expects 94, or 93 with `-NoCapture`).
  - Stage 85: on the 180 roof, run (run latch) toward the gap and jump at its edge. He catches the 230 roof in the air and holding the stick pulls him up (z = 230 + 34.65 ± 3).
  - Stage 86: walk up the ramp onto the terrace (z = 90 + 34.65 ± 3).
  - Capture stage 84 adds `Wharf_310` (the Row from the 280 roof) and `Wharf_430` (the yard from the crane tower). The south-edge view was dropped because the market canopies now block it.
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **94/94**; uncapped with and without groom: 0 failures (see below for the final rebuild).
  - Roof leap ends at z 264.65; ramp at 124.65; wharf routes unchanged.
  - Evidence: `Review/runtime_districts.jpg` (wharf roof, Chandlers' Row, Timber Yard).
- **Not verified:**
  - Not traversed by automated tests: the lean-to route, the garden-wall top walk, the market stalls, the lumber chimney and the leap to the crane tower (80 cm gap, 50 cm up).
  - Not played by the user.
- **Next (the user's queue):** corners and turning, then tuning. **Not integrated into main.** A worktree launcher receipt was written.

## Forty-seventh pass — sleeves that read as arms in a coat; vivid purple

- **User feedback** (with a goal image, saved as `References/ArtDirection/Chuck-Jacket-Sleeves-Target.jpg`, SHA-256 `6CC4B393B47C59714335CA1D7C344CAE12F587E83CA5CCD6D10E737F9D562807`): "The jacket's arms don't read as arms within a normal coat… make it more like the top image… Also… more vibrant purple".
- **Source commit:** on top of `3873171`, branch `codex/claude-character`.
- **Diagnosis:** the sleeves were plain tubes whose inner surface touched the body shell (sleeve centre y 14, radius about 3.5, versus shell half-width about 10.4 at z 30). Arm and torso merged, with no armhole seam, gap or cuff structure. The dye was a dark red-violet.
- **Changes:**
  - `Tools/build_chuck_model.py`:
    - `JACKET_PROFILE` side panels slimmed about 0.8 cm between the hem and armpit (ry 10.4 → 9.6 at z 30, 10.6 → 9.9 at z 37), so the sleeves hang free with a crease between.
    - The sleeve tapers from 3.75 to 2.95 radius with deeper elbow folds (0.2), and has a turned-back cuff band.
    - `seam_on_tube` puts seams on the cloth: a set-in armhole seam, tilted so it rides high over the shoulder and low at the armpit; a cuff edge seam; and a seam down the back of each sleeve. These are labelled Sleeve/Cuff so they skin with the sleeve.
    - A first pass used flat rings that floated off the cloth; replaced.
  - `Tools/build_chuck_v1.py`: at rest the arms are carried 5° off the body (was 1°).
  - `bake_textures.py`:
    - Jacket ramp moved from red-violet to vivid violet, plus a faint lighter Voronoi crackle (scale 1.1) like the goal's distressed suede.
    - Calibrated in Unreal: the first pass measured a median sRGB (74, 29, 130), too blue. The final measures (82, 50, 122) against the goal's (81, 49, 112).
- **Verified:**
  - `rebuild-v1` build/bake/groom (136k strands)/check PASS. `Import-ChuckGroom.ps1 -Review` all steps.
  - Packaged `-MotionCapture` and `-NoGroom`: **94/94**; uncapped with and without groom: 0 failures.
  - Evidence: `Review/goal_compare_jacket_sleeves.jpg` (goal / Blender rear / Unreal three-quarter), plus refreshed textured and Unreal review images.
- **Remaining:**
  - The goal's coat is longer and boxier; Chuck's is the established cropped jacket.
  - The lighter pattern is fainter than the goal's.
  - The sleeves still touch the body in the rest pose itself (the idle carries them off it).
  - Not played by the user.
- **Not integrated into main.** A worktree launcher receipt was written.

## Forty-eighth pass — ledge corners; landing roll from a fall

- **User request:** "Do ledge corners and turning, and also make it so falling for more than a short height results in an auto roll on landing". "Turning" is taken as turning corners on the ledge.
- **Source commit:** on top of `0285878` (main after integration 8), branch `codex/claude-character`.
- **Corners** (`ChuckCharacter`):
  - When a shimmy step fails (no edge ahead, or the capsule sweep is blocked), `TryHangCorner(direction)` runs.
    - **Inside corner:** a chest trace ahead finds a wall facing back (dot > 0.7) with an edge at the same height (`FindLedge` ±8 cm around `HangDrop`).
    - **Outside corner:** it steps 2 cm at a time to where this face's edge ends, then looks for the edge on the side face (normal = travel direction), 23 cm round the corner.
  - `TurnHangCorner` blends position and yaw over 0.3 s (`HangSnapTime`); the grab snap stays 0.12 s.
  - Stick carry: after a turn the camera lags. The raw stick held while shimmying keeps meaning "carry on this way" (`bCornerCarry`), with pull-up/let-go suppressed, until the raw stick changes (dot < 0.7) or is released.
    - Without this the first test run pulled up at the inside corner and let go at the outside one, because the camera-relative stick suddenly read as toward or away from the new wall.
  - If there is no edge round a corner, he still stops.
- **Landing roll:** `AirApexZ` tracks the highest point since leaving ground, wall or ledge. On touchdown, a fall over `RollFallHeight` (80 cm) runs `LandingRoll`: the roll clip from 0.12 s (straight into the tuck), toward the stick, else the velocity, else the facing. The roll's stick carry and exits apply as before.
- **Yard:** an L-return on the harbour wall's east end (50×80×115 with coping at X 50..100, Y −335..−255) gives an inside corner.
- **Tests:** now **97** (`Verify-Package.ps1` expects 97, or 96 with `-NoCapture`).
  - Stage 80's end check is now: shimmying right turns the inside corner, still hanging, height unchanged.
  - Stage 89: shimmy left off the west end goes round the outside corner. Checks: yaw 0 ± 10 0.35 s after the turn, still hanging, height unchanged.
  - Stage 87: dropped from 150 cm, lands in a roll.
  - Stage 88: a 50 cm drop just lands.
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **97/97**; uncapped with and without groom: 0 failures.
  - Inside-corner run: 1 inside and 2 outside corners, round the return, still hanging at z 93.00.
  - Outside-corner run: 2 outside corners, yaw 0.0 after the first, dz 0.000.
  - Falls: 150 cm → 1 roll; 50 cm → none.
- **Remaining:**
  - No in-game capture of corners or landing rolls yet (log evidence only).
  - The 0.3 s corner turn has no dedicated clip (the Hang pose blends round).
  - Landing rolls can carry him off a roof edge.
  - Not played by the user.
- **Not integrated into main.** A worktree launcher receipt was written.

## Forty-ninth pass — chimney camera; side jump onto a wall

- **User feedback:** with the camera-relative stick, climbing a chimney by wall-jumping back and forth leaves Chuck out of view. The user asked for side-jumping onto a wall and bouncing sideways, "or lmk if a different approach would be better".
- **Diagnosis:**
  - Entering the first wall means pushing toward it, so the camera sits behind him facing that wall, and every kick flies toward or past the lens.
  - The camera also held its height through airborne time (rate 1.5), so the climb left the frame.
- **Change** (`ChuckCharacter`):
  - **Chimney camera:** `EnterWallRun` traces 250 cm out from the wall for another wall facing back (dot > 0.8) and sets `bChimney`. During chimney wall runs and wall-jump flights, after 0.3 s without look input, the orbit yaw eases (rate 5) to whichever side-on yaw (wall ± 90°) is nearest, looking along the gap. It's cleared on landing and reset. Single walls are unchanged.
  - **Height follow:** the camera follows at rate 8 during wall-jump flights (and wall runs), holding height only through ordinary jumps.
  - **Side jump onto a wall:** during a side jump's flight, `TryEnterWallRun` probes along its velocity; a hit starts a wall run with the stick not needed (`bWallAuto`), as after a wall jump.
- **Tests:** now **99** (`Verify-Package.ps1` expects 99, or 98 with `-NoCapture`). Stage 90: in the cargo chimney with the camera 20° off the gap, side jump (stick left) at stack A, then bounce with jump alone. Checks:
  - the first wall run is entered from the side jump;
  - the camera yaw at the 2nd and later runs is within 10° of side-on (−90), with ≥3 runs.
- **Verified:**
  - Packaged `-MotionCapture` and `-NoGroom`: **99/99**; uncapped with and without groom: 0 failures.
  - Side-entry chimney: camera yaws at the runs −70 → −88 → −90; he catches the top and pulls up (z 274.65).
  - The original chimney and wharf alley still pass.
  - Evidence: `Review/runtime_chimney.jpg` (side-on bounce in game, camera rising with him).
- **Remaining:**
  - The camera's side-on choice (left or right of the gap) follows whichever is nearer.
  - No special pitch.
  - Not played by the user.
- **Not integrated into main.** A worktree launcher receipt was written.

## Fiftieth pass — strafe, faster run, drop to hang, landing roll height

- **User requests (2026-09-29):**
  - Q/E strafe (Counter-Strike style), with jump while strafing = side jump.
  - The strafe jump longer when running, shorter when not.
  - Running a bit faster.
  - Walking gently off an edge grabs and hangs, like GTA.
  - Double the fall height that triggers the landing roll.
- **Clips** (`Tools/build_chuck_v1.py`, `strafe_clip`):
  - `StrafeLeft`/`StrafeRight`: a step-together sidestep, 55 cm/s, 0.4 s cycle, stance 0.65, trail paw 0.35 of a cycle behind.
  - `StrafeRunLeft`/`StrafeRunRight`: a bounding shuffle with a flight phase, 150 cm/s, 0.33 s cycle, stance 0.3, trail 0.15 behind.
  - Both: athletic crouch (2.5 / 3.5 cm), a slight lean into the travel, head level. The paws never cross (closest about 7.7 cm apart).
  - Leg reach 0.91 / 0.86.
  - Review sheets: `Review/motion_StrafeLeft.png`, `motion_StrafeRunLeft.png`.
- **Run:** 190 → 225 cm/s on a 10-frame cycle (was 12), steps 37.5 cm (was 38). Reach 0.93 (an 11-frame try reached 0.99 and was rejected). `RunBrake` 700 → 1000, so letting go still coasts about the same distance.
- **Side jump:** the manifest has `launch_walking` (150 cm/s, 13 cm apex, about 55 cm) and `launch_running` (265 cm/s, 22 cm apex, about 125 cm). The runtime picks from the run latch when the jump starts. This applies to C + stick side jumps too.
- **Runtime** (`ChuckCharacter`):
  - **Input:** `StrafeKeys` axis (Q −1, E +1) and `StrafeTrigger` (LT axis). Q/E no longer turn the camera; the mouse and right stick do.
  - **Strafe gait:** while strafe is held, facing is fixed to the camera yaw (720°/s) and orient-to-movement is off.
    - Speed: `StrafeSpeed`, or `StrafeRunSpeed` with run latched.
    - Clips: mostly sideways (lateral ≥ 0.8 × forward) plays the strafe clips, phase from sideways travel over the stride; otherwise WalkLoop plus the run layer, played backward for a back-pedal.
    - Paw stance windows come from the manifest.
  - **Strafe jump:** jump while strafing sideways starts a side jump. It uses the axes as of last frame; reading the bindings in `JumpPressed` picked up stale values once test input was switched off (fixed). After the jump, `FinishDodge` goes back into the strafe while it's held.
  - **Drop to hang** (`TryDropHang`): runs on the first airborne frame when he walked off (vertical speed < 10, not running, walking-speed gait).
    - It traces back for the face just under the top, requires no ground within `DropHangMinDrop` (60 cm) below, then finds the ledge and enters Hang.
    - He swings round over 0.35 s. `bHangNeedsRelease` ignores the stick until it's released, so the stick that walked him off doesn't drop him or pull him up.
  - **Landing roll:** `RollFallHeight` 80 → 160.
- **Tests:** now **106** (105 with `-NoCapture`).
  - New stage 91: Q held on real keys → strafe walk facing the camera with paws holding; Space gives the short jump; he strafes on afterwards.
  - New stage 92: run latched and E held → strafe run; Space gives the long jump.
  - New stage 93: walk off the harbour wall's north face with the stick held away: he hangs facing the wall at 93 cm, stays hanging until the stick is released, then jumps back up.
  - Stage 55 now uses the short launch. Keyboard side-jump threshold is 40 cm. Stage 87 falls 220 cm (rolls); stage 88 falls 120 cm (no roll).
- **Verified:**
  - Worktree package `-MotionCapture` and `-NoGroom`: **106/106**; uncapped with and without groom: 0 failures.
  - Strafe 55.00 / 150.00 cm/s, yaw held, paw slip 0.
  - Strafe jumps about 57 / 130 cm (authored 55 / 126).
  - Drop hang yaw −90, z 93.00, held, back on top at 149.65.
  - The first rerun stalled for about 2.5 h on a test waiting for a hang (the stale-axis bug). The pass script now has a 12-minute watchdog.
- **Remaining:**
  - No dedicated diagonal strafe clips; diagonals use whichever clip dominates.
  - The strafe camera doesn't reframe.
  - Drop to hang doesn't trigger when running (by design), and walking off the pier into the water now hangs.
  - Not played by the user.

## Fifty-first pass — movement sound effects

- **User request (2026-09-29):** walking, running, jumping, slashing and rolling sound effects suited to the game's feel, tone, style and volume. The soundtrack already exists (Codex).
- **Source:** `Tools/gen_chuck_sfx.py` synthesizes 36 mono 48 kHz WAVs in pure Python (seeded, so reproducible, with no licence or install) into `SourceAssets/Audio/SFX` with a manifest.
  - Steps: wood and stone × walk and run × 6 variants.
  - Jump ×3, land ×3, slash ×4 (swish peak on the clip's 0.16 s strike), roll ×2 (contacts timed to the Roll clip).
  - Style: a small rat's soft pads with a faint claw tick, the jacket's cloth, air swishes. No vocal efforts; no cartoon sounds.
  - Preview reel sent to the user.
- **Import:** `Tools/import_chuck_sfx.py` → `/Game/Art/Audio/SFX` (36 SoundWaves; cooked by the existing `/Game/Art` rule).
- **Runtime** (`ChuckCharacter`):
  - All sounds are 2D under the 0.45 soundtrack, with a random variant and ±5% pitch.
  - **Steps:** on each paw plant (stance rising edge) in Start/Loop/Stop/Strafe/Turn. Stone or wood comes from the floor material's name under the paw. Run steps play above about 111 cm/s.
  - **Jump:** on takeoff, side-jump launch and wall kick. **Land:** on landing, scaled by the drop, and on side-jump touchdown.
  - **Slash:** every strike, via `PickPaw`. **Roll:** dodge rolls start the sound at the clip's entry offset; landing rolls add a land sound.
  - Volumes: walk 0.3, run 0.4, jump 0.4, land 0.5, slash 0.45, roll 0.45.
- **Tests:** **107** (106 with `-NoCapture`). The final check: all 36 sounds load, and steps (>20), jump, land, slash and roll all fired during the run.
- **Verified:**
  - Worktree `-MotionCapture` and `-NoGroom`: **107/107**; uncapped with and without groom: 0 failures.
  - Fired counts, e.g. steps 243 / jumps 38 / lands 25 / slashes 13 / rolls 4.
- **Not verified:**
  - How they sound on the user's speakers, and the loudness balance against the music.
  - Whether the stone surface is actually detected on Codex's streets (no test walks there).
- **Remaining:** no sounds yet for wall-run steps, hang grab, pull-up, mantle or shimmy.

## Fifty-second pass — strafe key + jump is always a side jump

- **User request (2026-09-30):** holding forward plus strafe moves diagonally (good). Jump pressed while a strafe key is held should always be a side jump (not a new diagonal jump), even with forward held, so they can run forward and side jump by pressing strafe and jump together.
- **Change** (`ChuckCharacter::JumpPressed`):
  - On the ground gaits (now including Turn), a held strafe key gives a side jump that way whatever else is held; the "mostly sideways" condition is gone.
  - Q/E and LT are read from the player controller's live key state (`IsInputKeyDown(Q/E)`, `GetInputAnalogKeyState(LeftTriggerAxis)`). A strafe key pressed on the same frame as jump therefore counts, and stale axis bindings can't fire it after input is switched off. The keys mirror `DefaultInput.ini`.
  - With LT, the stick's sideways push picks the side; LT with the stick straight ahead jumps normally.
  - The side jump is the usual square-up sideways jump (short from a walk, long with run latched), with no forward carry.
- **Tests:** **108** (107 with `-NoCapture`). Stage 94: W held on real keys with run latched from (−240, 0); at 1.0 s Q and Space are pressed on the same frame → side jump at once, long, about 125 cm to the left, under 10 cm forward drift.
- **Verified:**
  - Worktree `-MotionCapture` and `-NoGroom` **108/108**; uncapped with and without groom 0 failures.
  - Stage 94: speed 225 at the press, side jump at once, 119 cm sideways (116 uncapped) against 126 authored, forward 0.00.
- **Remaining:** Q+Space from a standstill is also a side jump now (it was before only when strafing sideways). The C + stick side jump is unchanged. Not played by the user.

## Fifty-third pass — shreddable grass tufts

- **User direction (2026-09-30):** a key loop of the Chuck game is shredding grass tufts and breaking jars to collect the cigarettes inside (Zelda rupee style), and later fighting largish rats. The scratch may need a low variant. Start with shreddable grass.
- **Recommendations given:**
  - Cigarettes as grounded physical pickups with a small counter (no power progression).
  - A shared breakable base for grass, then jars, crates and rats.
  - A generous hit zone now; later an automatic low rake chosen when the target in front is low.
- **Art:** `Tools/build_grass_tuft.py` (Blender) → `SourceAssets/Props/Grass`.
  - Tufts A/B/C: 22–25 cm tall and 33–47 cm across, 760–1080 tris. Curved, tapering, double-sided blades with up-and-out normals. Vertex tint runs olive to straw; vertex alpha is the height fraction for the sway.
  - Matching stubble (same seed, ragged 2–6 cm stalks) and a clipping blade.
  - Review: `Review/runtime_Grass_*.png`.
- **Import:** `Tools/import_grass_tuft.py` via `-ExecutePythonScript` (the commandlet crashed on the FBX importer) → `/Game/Art/Props/Grass`.
  - `M_Grass`: vertex colour squared (the FBX colours arrive a gamma step bright; the first capture was yellow-green), roughness 0.85, specular 0.25. Wind sway in world position offset, about 1 cm at the tips, growing with height².
  - Instanced-static-mesh use is enabled; the verifier caught the default-material fallback. No collision.
- **Sounds:** `SFX_Shred_00..02` added to `gen_chuck_sfx.py`: fibre snaps, a leafy rustle and a light swish. The existing 36 regenerate byte-identical.
- **Runtime:**
  - `AChuckBreakable` (abstract): a registry of live breakables, plus hit radius/height, `Cigarettes` (for the next pass) and a virtual `Break(Swing)`. No collision: the slash tests reach against the list, so grass can't catch paw, ledge or camera traces.
  - `AGrassTuft`: swaps to stubble. 18 clippings (8–12 cm) fly along the swing with drag and spin, settle flat and shrink away by 1.3 s, as instances with ticking only while flying. It plays a shred sound at 0.45.
  - `Plant` grounds a tuft by trace (ground level only). `SpawnDockGrass` plants 9 seeded patches: 51 tufts.
  - `ChuckCharacter`: each strike schedules a hit at the clip's strike (`SlashStrike` 0.16 s, now generated). `SlashHit` breaks every breakable whose edge is within `SlashReach` 40 cm, ahead (dot > 0.3) or touching, from ground to chest. The swing runs across the body away from the striking paw.
- **Tests:** **110** (109 with `-NoCapture`).
  - Stage 95: tufts 30 cm ahead, 60 cm behind and 110 cm ahead. The slash cuts only the first (18 clippings, sound, 1 break); he walks through the far one; 51 tufts are in play.
  - Capture stage 96: a shred among five tufts.
- **Verified:** worktree `-MotionCapture` and `-NoGroom` **110/110**; uncapped with and without groom 0 failures.
- **Remaining:**
  - The slash visibly passes at chest height over the grass (the low rake is next).
  - No cigarette drops or pickups yet, and tufts don't regrow.
  - Stone-and-grass patches were placed by trace only. Not every patch has been looked at in game (the garden and timber yard weren't captured).
  - Not played by the user.

## Fifty-fourth pass — low rake, jars, cigarette pickups and counter

- **User (2026-09-30):** proceed with the suggested next steps: the low rake, cigarette drops/pickups/counter, then jars. Design note: Chuck isn't strong. He breaks grass and jars (urns in later post-prototype maps), never crates or barrels. Recorded in `PROJECT-BRIEF.md` and `CHARACTER-PLAN.md`.
- **Low rake** (`Tools/build_chuck_v1.py` `slash_low_clip`): `SlashLowRight`/`SlashLowLeft`.
  - Same timing, footwork and travel as the slash. The hips sink 7 cm and fold forward (pelvis 14°, spine 16°/8°). The wrist arc is given as directions from the moving shoulder at 0.92 of the arm's reach: out and up, down through the ground about 25 cm ahead, across low. Gaze stays on the ground ahead. Leg reach 0.92 (unchanged).
  - Chosen in `PickPaw` when `LowTargetInReach` (an unbroken breakable ahead within reach + 10 cm, top below feet + 35 cm). Paw order and beat unchanged. Stance maps to the slash's (same steps).
  - Review: `V1/Review/motion_SlashLowRight.png`.
  - Known (also in the normal slash): at the deepest frames the pulled-back arm opens a grey fur gap at the jacket's rear armhole.
- **Jar** (`Tools/build_clay_jar.py`, `import_clay_jar.py` via `-ExecutePythonScript`):
  - 25.5 cm glazed terracotta storage jar: a seamless lathe with a dark shoulder glaze and drips, 5760 tris. `M_ClayJar` uses vertex colour squared, with roughness and specular from glaze alpha.
  - 12 shards: three bands with jittered cut lines, each with its origin at its centre. The rest offsets are generated into `ClayJarData.h`.
  - `AClayJar`: a pawn-only capsule blocker (traces ignore it, so he can't mantle onto it). `Break` hides the jar, rebuilds it from shards at their rest poses, and bursts them outward and along the swing with tumble, one bounce and settling; they sink away after 2.4 s. It plays `SFX_JarBreak` and drops its cigarettes. `SpawnDockJars` places 10 (1–3 cigarettes each, seeded).
- **Cigarettes:**
  - `AChuckBreakable::DropContents` pops each cigarette out as an `ACigarettePickup`: `SM_Cigarette` at 1.6×, with Ash and Ember slots set to paper (fresh, unlit), tumbling, then lying flat.
  - Pocketed within 26 cm once 0.35 s old: `Chuck->AddCigarettes`, plus `SFX_Pickup` (paper tick and crinkle) at 0.35.
  - About one grass tuft in three holds one.
  - HUD: a "CIGARETTES n" box top-right, in the existing panel style.
- **Sounds:** `SFX_JarBreak_00..02` (dull clay crack, low ring, shard knocks, a landing scatter) and `SFX_Pickup_00..02`. The earlier sounds are unchanged. 45 in all; a preview reel was made.
- **Tests:** **113** (112 with `-NoCapture`).
  - Stage 95 now requires the low rake, and that the cut tuft's cigarette comes out.
  - Stage 97: walk over a cigarette → collected, counter +1, none left.
  - Stage 98: jar 40 cm ahead. Walking into it stops at x −224 with no mantle. The low rake breaks it into 12 shards with the sound and 2 cigarettes, then he walks through.
  - Capture 96: a tuft and a jar broken by one rake.
  - Flurry names now match "Right" in either variant.
- **Verified:** worktree `-MotionCapture` and `-NoGroom` **113/113**; uncapped with and without groom 0 failures. Evidence: `Props/Grass/Review/runtime_Loot_{046,060,130}.png` (counter, shatter, shards and a cigarette on the stones) and `Props/Jar/review_jar.png`.
- **Remaining:**
  - The front-on capture doesn't show the rake's depth well.
  - Jars at the market and timber yard weren't looked at in game.
  - No regrowth or respawn. Cigarettes don't persist beyond the session.
  - Not played by the user.

## Fifty-fifth pass — the small rat enemy

- **User request (2026-09-30):** "Do the small rat enemy" (regular largish rats; the low rake was built for them).
- **Model** (`Tools/build_enemy_rat.py`): an ordinary big dock rat on all fours.
  - Overlapping solids voxel-remeshed into one skin and smoothed; separate ears, eyes, nose and a tapering tail. 23-bone skeleton (spine, head, three per leg, six tail), automatic weights on the body, hand weights on the parts. 10k tris.
  - Five material slots (Fur, Belly, Pink, Eye, Tail). The first import used vertex colour, which didn't carry through the skeletal import (it rendered plain grey).
  - Scaled 1.2× in game: about 31 cm plus tail, 18 cm tall.
  - Review: `Enemies/Rat/Review/`.
- **Import** (`Tools/import_enemy_rat.py`, `-ExecutePythonScript`): `/Game/Characters/Rat/SK_Rat` with its own skeleton and five flat `M_Rat*` materials. The slot structs are copies, so each must be written back (the first pass left the default grid material). The script now checks that assignments persist.
- **Sounds:** `SFX_RatChitter` ×3, `RatHiss` ×2, `RatBite` ×2, `RatHurt` ×2, `RatDeath`; 55 sounds in all. Played 3D with attenuation (full to 2 m, silent by 11 m). Preview reel made.
- **Runtime:**
  - `AEnemyRat` (`ACharacter`, no controller) is posed procedurally on a `UPoseableMeshComponent`: component-space rotations on the reference pose, parent first. The pose covers:
    - a diagonal trot tied to ground travel (stride 10 cm + 0.1 × speed);
    - body bob and sway, sniffing and idle looking;
    - a tail wave that lashes in the wind-up;
    - crouch, lunge stretch and hurt twist;
    - the death roll onto its side, then sinking away.
  - States: Roam → Chase → Windup → Lunge → Recover, plus Hurt and Dead.
    - Roam: 60 cm/s round its home, with chitters. It notices Chuck within 350 cm when he's within 60 cm of its level, and loses him at 700 cm.
    - Chase: 170 cm/s.
    - Windup at 50 cm (inside the slash's reach): 0.45 s, facing him, crouched, hissing.
    - Lunge: 0.25 s at 280 cm/s, one bite chance within 45 cm.
    - Recover: backs off for 0.7 s.
    - Two hits (`Health` 2): the first knocks it back 170 cm/s with a squeal; the second kills it, drops its cigarettes, and it is gone by 2.2 s.
  - Chuck:
    - `TakeBite`: knockback 230 cm/s plus a hop, then 1 s immunity. Rolling, side-jumping, hanging, climbing and wall runs avoid bites. No health yet; that's the user's call.
    - `SlashHit` and `LowTargetInReach` include rats (hit radius 16), so they get the low rake.
    - `ACigarettePickup::Burst` is now shared by breakables and rats.
  - `SpawnDockRats`: 5 rats (wharf, timber yard, garden). Not spawned under `-ChuckSmokeTest`; tests place their own.
- **Tests:** **115** (114 with `-NoCapture`).
  - Stage 101: a rat 150 cm away chases, winds up for 0.45 s and bites at 1.2 s; Chuck is knocked back about 94 cm.
  - Stage 102: two low rakes kill a rat in about 0.97 s; one cigarette comes out and the rat is removed.
  - Capture 103: the encounter at rat height from the side.
- **Verified:** worktree `-MotionCapture` and `-NoGroom` **115/115**; uncapped with and without groom 0 failures. Evidence: `Review/runtime_Rat_{105,120,240,340}.png`.
- **Remaining:**
  - The knockback (about 94 cm) is longer than intended; tune on feel.
  - Rats path straight at Chuck (no obstacle avoidance) and don't climb.
  - No hit flash or death particles.
  - Procedural pose only (no authored clips); the pose hasn't been reviewed closely at speed.
  - Rats aren't respawned.
  - Not played by the user.

## Fifty-sixth pass — Sanity, the astral vanish and the summon

- **User (2026-09-30):** "Like the 2d game": health as a cigarette bar that cigarettes refill, and cigarettes beyond it collected like coins. Zero is a "death", but Chuck is a fey summon who can't die: he respawns at the spawn point with a magical summoning animation. Canon, `References/Original/GAME-BIBLE.md`:
  - "Instead of health, Chuck has Sanity. Damage lowers Sanity. Cigarettes restore it. When Sanity reaches zero, Chuck quietly disappears … he returns at the most recent Astral Anchor."
  - The Astral Sea is peaceful, ancient, quiet.
- **Runtime** (`ChuckCharacter`):
  - `MaxSanity` 5, and a bite costs 1. `AddCigarettes` refills Sanity first, then counts; `PickupsCollected` tracks both.
  - At zero, `bPendingVanish` fires once he's down from the knockback. A new `EGait::Astral` (owns the capsule, no input, rats lose interest) runs:
    - **Vanishing** (1.45 s): the `Summon` clip played backward so he sinks; an `AAstralSummon` vanish effect; hidden at 0.65 s; camera fade to indigo from 0.8 s.
    - **Away** (0.35 s): teleport to `StartLocation`, full Sanity, camera reset.
    - **Summoning** (2.6 s): fade in; the summon effect; he appears at 1.0 s, curled, and the clip plays forward.
    - Then Idle and walking; `Respawns` increments.
  - `ResetToDock` restores Sanity and clears the astral state and camera fade.
- **Summon clip** (`build_chuck_v1.py` `summon`): 1.6 s.
  - Curled low (hips down 11.5 cm; 13 put the rump 3 mm into the ground and failed `check_v1`), back rounded, head down, arms folded, tail wrapped. A breath, then an unhurried rise with the head last and a shoulder-roll settle. Paws planted in the idle stance; reach 0.92.
  - Review: `V1/Review/motion_Summon.png`.
- **Effect** (`Tools/build_astral_fx.py`, `import_astral_fx.py`, `AAstralSummon`):
  - A rune-circle disc: a 1024 px generated texture with a double ring, ticks, glyph band and stars, turning slowly.
  - An open column: additive, brightest at the foot, fading upward and toward the silhouette (Fresnel). The first pass read as a solid white tube; the second was brightest at the top, because the FBX import flips V.
  - 26 star motes (instanced) spiralling in and up.
  - Additive, unlit starlight materials with an "Intensity" parameter faded through dynamic instances. The vanish variant is narrower and quicker.
  - Sounds: `SFX_AstralVanish` (a glassy inharmonic shimmer swelling and drifting down, a low hum) and `SFX_AstralSummon` (rising, with a soft glass chime as he appears); 57 sounds in all.
- **HUD:** a "SANITY" row of five cigarette icons (filter, paper, ash; spent slots dark), with "CIGARETTES n" below.
- **Tests:** **117** (116 with `-NoCapture`).
  - Stage 104: bite 5 → 4; a pickup refills to 5 without counting; the next counts +1.
  - Stage 105: at Sanity 1 on the wharf, a bite gives vanish → away → summon (both effects). He's back at the start (0.00 cm) at about 4.85 s with full Sanity, visible, and walks 67 cm.
  - Capture 106: the vanish and summon from the front three-quarter.
  - Pickup-count assertions in stages 95/98/102 now use `PickupsCollected`.
- **Verified:** worktree `-MotionCapture` and `-NoGroom` **117/117**; uncapped with and without groom 0 failures. Evidence: `Props/Astral/Review/runtime_{Summon_070,Summon_115,Summon_190,Vanish_075}.png`.
- **Remaining:**
  - Respawn is always at the dock start (no Astral Anchors yet).
  - No low-Sanity feedback beyond the bar.
  - The summon's column facets slightly (32 sides).
  - The camera turn to his front is only for the capture; in play the camera sits behind him.
  - Not played by the user.

## Fifty-seventh pass — exhaled smoke

- **User (2026-09-30):**
  - "Since Chuck is always smoking can you have him exhale smoke periodically."
  - Also: no Astral Anchors; the map spawn is the respawn point (recorded in `PROJECT-BRIEF.md`).
- **Runtime** (`ChuckCharacter`):
  - Every 7–12 s, only when calm (Idle, Start, Loop, Stop, Land, Strafe or Turn, on the ground, not hidden), otherwise retried each second, `Exhale()` streams puffs for 0.7 s at 16/s from `socket_cigarette` (the mouth corner), forward and slightly down, easing off. They inherit half his velocity.
  - Each puff is world-space. Over 1.7–2.4 s it slows (drag 1.6/s), rises after 0.25 s, drifts in a faint breeze, swells from 3 to 18 cm and fades (peak opacity 0.3; the first try at 0.42 read as a cotton-wool ball).
  - Puffs are instanced `/Engine/BasicShapes/Sphere` with per-instance opacity (custom data 0).
  - `SFX_Exhale` ×2: a soft filtered breath, no voice, at 0.14.
- **Material** (`Tools/import_smoke_puff.py`, `-ExecutePythonScript`): `M_SmokePuff`.
  - Translucent unlit pale grey, with opacity = per-instance fade × (1 − Fresnel)^1.6 × the cigarette's smoke texture panning up. Instancing enabled.
  - No new meshes. 59 sounds in all.
- **Tests:** **118** (117 with `-NoCapture`).
  - Final stage: at least 5 exhales across the smoke test, and at least 8 puffs each. Measured 20/209 capped and 12–13 uncapped (a shorter run).
  - Capture 107: one breath, close in from the front three-quarter.
- **Verified:** worktree `-MotionCapture` and `-NoGroom` **118/118**; uncapped with and without groom 0 failures. Evidence: `V1/Review/runtime_Exhale_{100,140,190}.png`.
- **Remaining:**
  - No inhale (the idle clip's chin lift stays unsynchronised), and the ember doesn't brighten on a draw.
  - Smoke comes from the mouth corner, not the nose.
  - Not played by the user.

## Fifty-eighth pass — the dock worker NPC and talking

- **User (2026-09-30):** start the human NPC by the spawn (more NPCs later); suggestion docs cover free resources.
  - `References/chuck-3d-resource-guide.md` suggests MetaHuman, or MakeHuman + Mixamo, for humans.
  - The user chose my recommendation: build in Blender (no installs or sign-ins; MetaHuman clothing is modern anyway). Also: no dialogue for the worker for now, and talk on F / Y.
  - `References/Original/PHASE-2.md`: every human NPC is sized to the dock worker (the 2D scale reference); the next NPCs are the guard and the market woman.
- **Model** (`Tools/build_dock_npc.py`): 180.4 cm to the top of the cap, facing +X, 20 bones.
  - Lofted solids: a superellipse torso from hips to trapezius, and tapered tubes for arms and legs.
  - Ellipsoids for skull, jaw, nose, brow, ears, cap, palm and fingers, boot feet and heels. Voxel remesh at 0.75 cm plus smoothing, no decimation (even quads keep the clothing edges clean). Automatic weights; the eyes are separate spheres. About 96k tris.
  - Clothing by region into 10 slots: Skin, Shirt (sleeves rolled to the elbow, the jerkin's open V front, the armholes), Jerkin, Belt, Trousers (navy, as the old figure), Boots, Cap, Stubble, Brow, Eye.
  - The first pass was stacked ellipsoids and read as a bead-jointed mannequin; lofting fixed that.
  - Review: `NPCs/DockWorker/Review/`.
- **Import** (`Tools/import_dock_npc.py`, generated from the rat importer): `/Game/Characters/DockWorker/SK_DockWorker` with 10 flat `M_Worker*` materials. Slots are written back and the assignments checked.
- **Runtime:**
  - `ADockNPC` (actor): a pawn-only capsule blocker (radius 24, half height 90), so wall-run, ledge, mantle and camera traces ignore him. The `UPoseableMeshComponent` body is posed in component space:
    - a breath every 4.2 s, weight shift every 11 s, slight arm sway;
    - glances every 2.5–6 s;
    - head and neck (40/60) turning to Chuck within 450 cm and ±110°, clamped to ±70° yaw and −20 to 55° pitch, at rate 4.
  - `SpawnDockWorker` places him at (90, 200) facing −90 (where the old figure stood), tagged `DockWorkerArt`. The old primitive figure and `SM_DockWorker` prop are no longer spawned.
  - Talk: `AChuckCharacter::Interact` (the "Interact" action on F / Gamepad Y) starts a conversation with the nearest NPC that has `Lines`, within 120 cm and in front (dot > 0.2) or within 45 cm. Each press advances a line; the last closes it.
    - While talking, movement, jump, dodge and slash are ignored. Chuck turns to face the speaker; a bite, reset or vanish ends it.
    - HUD: "F / Y  Talk" near the top in reach (as in the 2D game), and a dialogue box above the controls showing the speaker's name and line.
  - Keyboard re-centre moved from F to the middle mouse button (the right-stick click is unchanged).
- **Tests:** **121** (120 with `-NoCapture`).
  - The prop-tag check accepts the worker's skinned body.
  - Stage 108: at 1.3 m in front he watches, looking down 43°; at 8 m he doesn't. Walking and jumping into him stops Chuck at 39 cm, with no wall run and no mantle.
  - Stage 109, on real F keys with a stand-in NPC: prompt in reach, line 1, S held does nothing, line 2 with the speaker's name, closed. The worker is silent.
  - Capture 110: Chuck walks up and the worker looks down at him.
- **Verified:**
  - Worktree `-MotionCapture` and `-NoGroom` **121/121**; uncapped with and without groom 0 failures.
  - The first run failed the watch check: I required Chuck's capsule to be "visible", and player capsules aren't. It now checks that he isn't astral.
  - Evidence: `Review/runtime_Worker_{030,260}.png`.
- **Remaining:**
  - A stylised face with no facial rig, and stiff, straight hanging arms.
  - No body turn (he only watches within ±110°).
  - About 96k tris, too heavy for a crowd without lighter detail levels.
  - No talking NPC in the map yet (the guard and market woman are next).
  - Not played by the user.

## Fifty-ninth pass — MakeHuman humans (the dock worker rebuilt)

- **User (2026-09-30):** NPCs "at least on par with Blade and Sorcery Nomads… not quite as detailed as Chuck", copied with minor changes to save compute. Approved installing MPFB2 and downloading the assets; animation source left to me (CMU mocap, no sign-in, matches the `cmu_mb` rig; not yet done).
- **Source:** `b88134c` + `f7deb68` on main. New: `Tools/build_npc_humans.py`, `Tools/import_npc_humans.py`, `Tools/Fetch-ClothTextures.ps1`, `SourceAssets/NPCs/{humans.json,README.md,Humans/}`, `SourceAssets/Surfaces/Cloth/`, `/Game/Characters/Humans/`. Changed: `DockNPC.cpp/.h` (CMU bone names, per-body arm lowering and palm turn, `GetWiderHandReach`), `DockGameMode.cpp/.h` (`-ChuckNPCCapture`, pose check), `Verify-Package.ps1` (122). Removed: `build_dock_npc.py`, `import_dock_npc.py`, `SourceAssets/NPCs/DockWorker`, `/Game/Characters/DockWorker`.
- **Model:** MPFB body with macro sliders, MakeHuman skin, eyes, brows and lashes. Clothing is cut from the body along signed-distance hems (edges split at the crossing, nearby vertices snapped, hems Taubin-smoothed), offset and solidified, carrying the body's weights. Boot feet are a remeshed, smoothed hull of the foot (vertex projection onto the hull folded the toes and was abandoned). Skin is removed 2 cm inside hems. 54.9k tris, 10 slots, 180 cm.
- **Materials:** fabric = linear luminance of the Poly Haven colour map × `gain` (1/mean luminance, measured at build) × `tint` (the cloth's albedo). Colour textures are forced to sRGB/default compression (import took the linen for a normal map). The eye is masked: the cornea's corner of the texture is clear. The import rebuilds `/Game/Characters/Humans` from scratch (rebuilding masters in place crashed the editor).
- **Contract:** `ADockNPC` API unchanged except the test accessor; tags, blocker, talk and watch behaviour as pass 58.
- **Verified:** root package `-MotionCapture` 122/122 + world, plaza and music checks, `Local/verify-package-20260930-183904.log`; promoted, receipt `f7deb68`. The first run failed my own pose check (wrist height 105 cm: height doesn't separate A-pose from arms down); it now measures lateral hand reach (24.3 cm). Evidence: `SourceAssets/NPCs/Humans/Review/` (runtime front, three-quarter, face; Blender back and boots).
- **Remaining:** no facial animation; the four fingers share one bone (slightly splayed); a small jag on the cap's front hem; no body turn; no mocap clips yet; the guard and market woman are not built. Not played by the user.

## Sixtieth pass — the guard, the market woman, and no zombie arms

- **User (2026-10-01):** resume character work; NPCs shouldn't hold their hands out like zombies.
- **Source:** `66aa87f` (code, assets) on main.
  - Changed: `DockNPC.cpp/.h`, `DockGameMode.cpp`, `build_npc_humans.py`, `import_npc_humans.py`, `Fetch-ClothTextures.ps1`, `Verify-Package.ps1` (123), `humans.json`, `NPCs/README.md`.
  - New: `Guard` and `MarketWoman` sources and assets, `metal_plate_02` (CC0 Poly Haven).
- **Pose:**
  - `ADockNPC::SolveRest` aims each joint in turn and re-measures the posed skeleton after every step, so the pose holds for any proportions:
    - upper arm to (-.03, ±.13, -1);
    - forearm to (.11, ±.03, -1);
    - twist about the forearm until the thumb points forward;
    - wrist aligned;
    - finger base curled 22°, finger 34°, thumb 12° toward the palm.
  - The zombie look came from the old minimal-arc arm lowering plus a forward elbow bend and a guessed twist.
  - Eye height now comes from the head bone, per body.
- **People:**
  - `EDockHuman` kinds; all meshes are hard-referenced in the constructor so they cook. `SpawnHuman` uses deferred spawn to set the kind.
  - `SpawnTownsfolk`: guard at (260, -4060), yaw 90, tag `DockGuard`; woman at (148, -1240), yaw 180, tag `MarketWoman`.
  - NPC blockers ignore Visibility, so Codex's plaza traces are unaffected.
- **Builder:**
  - New pieces: gambeson, cuirass, helmet, kerchief, bodice; skirt and apron cut from MakeHuman's `helper-skirt` (weighted waist-to-ankle shell); legs under the skirt removed.
  - Options: boot `height`, belt `z`, shirt `collar`.
  - Fabric master: metalness from ARM.B.
- **Process note:** this session is pinned to its worktree and a hook blocks writes to root. Some earlier edits had reached root through shell commands; they were moved into the worktree and root restored before the fast-forward. Reopening sessions at root avoids this.
- **Verified:**
  - Root package `-MotionCapture` 123/123 plus world, plaza and music checks: `Local/verify-package-20260930-195245.log`. Pose measures: worker 21.6 cm out / 4.0 ahead, guard 24.2 / 7.5, woman 19.7 / 5.2.
  - Promoted; receipt `66aa87f`.
  - Evidence: `SourceAssets/NPCs/Humans/Review/runtime_{DockWorker,Guard,MarketWoman}_*.png`.
- **Remaining:**
  - No facial animation; one finger bone for four fingers; the kerchief folds a little at the ears; a thin jag at the gambeson collar.
  - No body turn; no mocap yet (CMU clips are the next pass).
  - The sewer prompt ("Jump into the sewer?") and the 2D tutorial text are not done.
  - Not played by the user.

## Sixty-first pass — motion capture for the townsfolk, body turn, talk gestures

- **User (2026-10-01):** proceed with the remaining character work; leave the sewer for later.
- **Source:** `f3965ea`, `68547ba`, `93a586d` on main.
  - New: `Tools/Fetch-CMUMocap.ps1`, `Tools/build_npc_mocap.py`, `Tools/Import-NPCHumans.ps1`, `SourceAssets/Mocap/CMU/` (3 BVH + manifest with SHA-256; `.bvh -text`), `SourceAssets/NPCs/Humans/Anim/`, `/Game/Characters/Humans/Anim/AS_Human_{StandHip,StandLook,Talk}`.
  - Changed: `DockNPC.cpp/.h`, `ChuckCharacter.h` (`GetTalkingTo`), `DockGameMode.cpp`, `build_npc_humans.py`, `import_npc_humans.py`, `Verify-Package.ps1` (124), `NPCs/README.md`.
- **Mocap:**
  - Source: CMU Graphics Lab database, B. Hahne's Motionbuilder-friendly BVH (bone names equal to `cmu_mb`).
  - Takes: 111_28 (StandHip), 77_02 (StandLook), 18_08 (Talk).
  - Tried and dropped: 140_06/07 "Idle" (a crouched ready stance), 113_21 (head thrown back), 141_20 (fidgety).
- **Retarget:**
  - At the take's T-pose frame, only the limbs are aimed from our A-pose. Aiming the spine, neck and collarbones onto CMU's straighter neck tipped heads back.
  - Every bone then follows `take(f) * take(T)^-1`. Facing and drift are removed, and clips are resampled to 30 fps with a 1 s loop blend.
  - Zero-length CMU links: Neck sits on Spine1's joint and FingerBase on the Hand's, so the hand aims at the finger.
  - Clips are exported with the worker mesh so the FBX carries a bind pose.
  - The scene frame rate is set after the rig FBX import, which had reset it to 24.
- **Runtime:**
  - Clips are sampled per bone and applied as rotation changes from the skeleton's rest (so they fit every body); hips offset at the Hips bone.
  - Talk blends in at 2.5/s while Chuck talks to that NPC.
  - Curled fingers are laid back over the mocap from the rest solve; the look-at is on top.
  - Body turn: after 1.2 s with Chuck more than 55° off, the NPC turns at 70°/s until facing him, and returns to its post when he leaves.
  - The procedural pose remains the fallback.
- **Fixes on the way:**
  - Each rig had exported as its own root bone (`SK_Guard_Rig`...), so only the worker could map clips. All rigs are now named `HumanRig`, with a clean rebuild (`Import-NPCHumans.ps1 -Clean`; the editor can't delete assets the game module hard-references).
  - Skirt and apron are re-weighted to Hips / UpLegs so they don't split.
  - The worker turn test first put Chuck on the harbour side (he fell in and was reset) and then stopped 55° short.
- **Verified:**
  - Root package 124/124 plus world, plaza and music checks: `Local/verify-package-20260930-211255.log`. Receipt `93a586d`.
  - Evidence: `SourceAssets/NPCs/Humans/Review/runtime_*`.
- **Remaining:**
  - The feet slide slightly during a turn (no stepping clip).
  - CMU has no real finger motion (fingers stay curled).
  - No facial animation.
  - The sewer prompt is deferred (user's call).
  - Not played by the user.

## Sixty-second pass — head carriage and the scratch reaction

- **User (2026-10-01):** NPCs always look down at Chuck with a craned neck; they should only look down when he's practically touching them. When Chuck scratches a friendly NPC, have them react believably (brace back, or shuffle their feet).
- **Source:** `09273cd`, `0efe501` on main.
  - Changed: `DockNPC.cpp/.h`, `ChuckCharacter.cpp/.h` (`SlashNPCHits`), `DockGameMode.cpp/.h`, `build_npc_mocap.py` (one-shot clips), `Fetch-CMUMocap.ps1`, `Verify-Package.ps1` (125).
  - New: `79_73.bvh`, `AS_Human_React`.
- **Look:**
  - Pitch is capped at `Lerp(6°, 55°, Close)`, where Close ramps from 0 at 95 cm (centre to centre) to 1 at 45 cm. Yaw is unchanged.
- **Scratch:**
  - `SlashHit` also checks NPCs: within 40 cm past their 24 cm capsule, in front of Chuck. It calls `ADockNPC::TakeScratch`, which starts the reaction (ignored if already within the first 0.6 s of one) and sets the body turn going.
  - The React clip is a one-shot (79_73 from 1.4 s, 2 s long) blended over the idle and talk clips: 0.2 s in, 0.5 s out.
  - 76_06 "avoid stepping on something" was tried and dropped: a cartoonish hop.
  - `-ChuckNPCCapture` gains a "Scratched" shot.
- **Tests:**
  - Head nearly level at 1.3 m (< 9°) and looking down when pressed close (> 15°).
  - A scratch makes the worker react.
  - The pose check now measures before the scratch. Its first run caught the worker mid-reaction (hands at his chest, 27 cm ahead).
- **Verified:**
  - Root package 125/125 plus world, plaza and music checks: `Local/verify-package-20260930-220145.log`. Receipt `0efe501`.
  - Evidence: `SourceAssets/NPCs/Humans/Review/runtime_Guard_scratched.png`.
- **Remaining:**
  - The reaction is the same take for everyone, with no sound or line.
  - There's no grudge or flee: they don't avoid Chuck afterwards.
  - The feet slide slightly in body turns.
  - Not played by the user.

## Sixty-third pass — hands and fingers

- **User (2026-10-01):** the hand placement looks weird; preferably add finger capability, keeping in mind the guard will hold a spear.
- **Causes:**
  - `cmu_mb` has one finger bone per hand (paddle fingers).
  - CMU wrist data bent the hands flat against the legs.
  - The capture actors were slighter at the hip, so the hands sank into the thighs.
- **Source:** `fc343f7`, `884c2ff`, `681dd10` on main (rebased onto Codex's Dock Street `2deace2`).
  - Changed: `build_npc_humans.py` (game_engine rig and region names), `build_npc_mocap.py` (TAKE name map, limb AIM table), `DockNPC.cpp/.h`, `DockGameMode.cpp`, the README and docstrings, and all human/clip FBX and Unreal assets (clean re-import).
- **Runtime:**
  - Body bones use mannequin names, with one neck bone (look split 50/50 neck/head).
  - Fingers: the axis for each of the 30 finger joints comes from the palms-down model pose (across the finger, toward -Z). Per frame each joint is set relative to its parent at a Relaxed curl or, by `SetGrip`, a Gripped one.
  - Wrists: 65% slerp to the straight wrist from the rest solve.
  - Clip arms rolled 5° out.
- **Test:** the standing-pose check now requires a finger curl of more than 10°, and it rejects only a straight arm held more than 30° out from hanging.
  - Hand width was dropped as a measure: a natural hanging arm reaches 34 cm, nearly the A-pose width.
  - Two failed runs got it there (a hand on the hip at 39 cm, then a straight arm at 37 cm). The portrait capture now logs these measures per shot.
- **Verified:**
  - Root package 125/125 plus world, plaza and music checks: `Local/verify-package-20261001-101818.log`. Receipt `681dd10`.
  - The package also contains Codex's uncommitted `DockSetting.cpp` edit (see HANDOFF Update 28).
  - Evidence: `SourceAssets/NPCs/Humans/Review/runtime_{Guard,MarketWoman}_front.png`.
- **Remaining:**
  - Spear prop and holding pose: `SetGrip` is ready; the arm pose and the prop attached to `hand_r` are not done.
  - No individual finger animation in the idles (fingers hold their curl).
  - Not played by the user.

## Sixty-fourth pass — the guard's spear

- **User (2026-10-03):** give the guard a spear.
- **Source:** `fd6ffb8` on main.
  - New: `Tools/build_spear.py`, `SourceAssets/NPCs/Props/{SM_Spear.fbx,manifest.json}`, `/Game/Characters/Humans/Props/SM_Spear` with three material instances.
  - Changed: `import_npc_humans.py` (`import_props`, fabric textures from any folder), `DockNPC.cpp/.h`, `DockGameMode.cpp`, `Verify-Package.ps1` (126), `NPCs/README.md`.
- **Model:** 212 cm, origin at the butt, along +Z. Slots:
  - Shaft: brown_planks_03 grain, 1 m tile, ash tint;
  - Head: blade, socket and butt in metal_plate_02 steel;
  - Wrap: brown_leather.
  - The blade first came out needle-thin (a bad leaf curve); it's now a sine leaf, 5 cm wide and 26 cm long.
- **Runtime:**
  - `GiveSpear` shows a `UStaticMeshComponent` on the body (no collision) and sets `Grip[right] = 1`.
  - `PlaceSpear` (from `GiveSpear` or `BeginPlay`, whichever comes after the rest solve): butt at (16, right×30, 0), leaning out 4°; grip point 108 cm up the shaft.
  - `HoldSpear` runs after `PoseHands`, in both the mocap and procedural paths. It's two-bone IK to a wrist target 8 cm back along the hand and 3 cm toward the palm, with the pole back and out; the hand aims across the shaft and twists thumb-up. Descendants follow by local transforms.
  - The worktree's PolyHaven JPGs were LFS pointers; `git lfs pull` fixed it.
- **Verified:**
  - Root package 126/126 plus world, plaza and music checks: `Local/verify-package-20261003-095700.log`. Receipt `fd6ffb8`.
  - Evidence: `SourceAssets/NPCs/Humans/Review/runtime_Guard_spear.png`.
- **Remaining:**
  - The fist only approximately wraps the shaft: the curl is generic, and the shaft can show through the fingers up close.
  - The spear doesn't react to anything (no grounding check on slopes; the guard stands on flat paving).
  - Not played by the user.

## Sixty-fifth pass — two gate guards

- **User (2026-10-03):** add another guard by the existing one, both either side of the gate; same outfit and spear, female Caucasian.
- **Source:** `e82be03` on main.
  - Changed: `humans.json` (GuardWoman), the FBX and textures, `DockNPC.cpp/.h` (`EDockHuman::GuardWoman`, `GiveSpear(Side)`, `SpearSide`), `DockGameMode.cpp`, `NPCs/README.md`.
- **Placement:** guards at (105, -4085) and (415, -4085), yaw 90; tags `DockGuard` and `DockGuardB`.
- **Tests:**
  - The pose check now counts 4 humans.
  - The guard check requires both guards, either side of x = 260, each with an upright, gripped spear.
  - The portrait capture names files by tag; the two shared the display name "Guard" and overwrote each other.
- **Verified:**
  - Root package 126/126 plus world, plaza and music checks: `Local/verify-package-20261003-102630.log`. Receipt `e82be03`.
  - Gate view: `-ChuckPlazaCapture` View3, saved as `runtime_Gate_guards.png`.
- **Remaining:**
  - Both guards play the same idle clip (different start points).
  - Her line duplicates his.
  - Not played by the user.

## Sixty-sixth pass — the side-gate guard

- **User (2026-10-03):** add a Caucasian male guard by the gate next to the sewer grate.
- **Source:** `0396522` on main.
  - Changed: `humans.json` (SideGuard), the FBX and textures, `DockNPC.cpp/.h` (`EDockHuman::SideGuard`; spawn at (-1690, 3470), yaw 0, tag `DockGuardC`, spear right), `DockGameMode.cpp` (pose check counts 5; side-guard check; capture "Wide" shot), `Verify-Package.ps1` (127), `NPCs/README.md`.
- **Placement:** the side gate is at (-1748, 3650) and the hatch at (-1580, 3900) (DockSetting.cpp). He stands south of the gate, clear of the hatch apron and the sewer test's approach along y 3900. NPC blockers ignore Visibility, so Codex's traces are unaffected; they pass.
- **Verified:**
  - Root package 127/127 plus world, plaza and music checks: `Local/verify-package-20261003-105156.log`. Receipt `0396522`.
- **Remaining:**
  - All three guards share one idle clip (different start points) and one line.
  - Not played by the user.

## Sixty-seventh pass — sewer life (sewer plan step 1)

- **User (2026-10-03):** begin the sewer plan (rats and cigarette grass in the sewer; the zombie, the slide exit and the evening return come later).
- **Source:** `7d38b0e`, plus the check fix `192dd14`, on main (from Codex's `ae2e123`).
  - New: `SewerLife.cpp/.h` and `Tools/create_sewer_moss_material.py` with `M_SewerMoss` (LFS).
  - `DockSewer.h/.cpp` (Codex's): read-only accessors and `RouteRight` only.
  - `GrassTuft` (`PlantAt`, `SetMoss`), `EnemyRat::PlaceAt`, `CigarettePickup` (sewer lighting), `DockGameMode` (spawn plus check), `Verify-Package.ps1` (128).
- **Contract:** no rig, animation or controller change.
- **Verified:**
  - Root package 128/128: `Local/verify-package-20261003-115923.log`. Receipt `192dd14`.
  - Editor capture `-ChuckSewerLifeCapture`: rats on the banks past the first gap, moss clumps at the wall base.
- **Remaining:**
  - Silhouette readability in Codex's dim lighting.
  - Steps 2–4 of the plan (`memory: sewer-plan`).
  - Not played by the user.

## Sixty-eighth pass — the sewer's water-slide exit (sewer plan step 2)

- **User (2026-10-03, assigned to Claude):** the end of the sewer becomes a narrow downward water slide. A second of sliding, a fade to black, then Chuck at the end of the pier climbing out, with control returned.
- **Source:** `8d99378` on main.
  - New: `SewerSlide.cpp/.h`.
  - `ChuckCharacter.cpp/.h`:
    - `EAstral::SlideDown/SlideAway`, `BeginSlide`, `FindPierExit`;
    - the held slide camera;
    - `bAutoClimb` in Hang;
    - foot IK off while sliding;
    - a colour for `CameraFade`.
  - `DockSewer.cpp` (Codex's): the end cap swap, the slide counting as sewer, and the last-ring direction fix.
  - `DockGameMode.cpp/.h`: stages 111 and 112, plus `-ChuckSlideTest`.
  - `Verify-Package.ps1`: 129.
- **Contract:** no rig or clip change. Hang, Climb and the Astral respawn are unchanged for their existing callers.
- **Verified:**
  - Root package 129/129: `Local/verify-package-20261003-122052.log`. Receipt `8d99378`.
  - `-ChuckSlideTest` in the editor: `Local/slide-test.log` plus the screenshots.
- **Remaining:**
  - The slide isn't visible once he's below the mouth.
  - No splash.
  - Step 3 (after-sewer evening, closed grate, open tavern; hook `HasExitedDockSewer()`).
  - Step 4 (the zombie).
  - Not played by the user.

## Sixty-ninth pass — the sewer zombie (sewer plan step 4)

- **User (2026-10-03):** "Proceed". The sewer plan: a hostile zombie in the wide section that can just barely be killed and is best avoided.
- **Source:** `58ef707` on main.
  - `humans.json`: Zombie.
  - `build_npc_humans.py`: `ragged` and `skin_tint`.
  - `build_npc_mocap.py`: walk mode and the three Zombie clips.
  - `Fetch-CMUMocap.ps1`: 137_32, 137_33, 113_08.
  - `DockNPC.cpp/.h`: Zombie mode.
  - `ChuckCharacter.cpp/.h`: the `TakeBite` amount.
  - `SewerLife.cpp/.h`: placement.
  - `DockGameMode.cpp/.h`: stages 113–115 and `-ChuckZombieTest`.
  - `Verify-Package.ps1`: 130.
  - Zombie assets (LFS).
- **Contract:** the shared skeleton and the existing clips are unchanged (the reimported copies were restored). Chuck's rig and clips are unchanged.
- **Verified:**
  - Root package 130/130: `Local/verify-package-20261003-132359.log`.
  - Editor `-ChuckZombieTest` and `-ChuckNPCCapture`.
- **Remaining:**
  - Sound for the tell.
  - Silhouette readability.
  - Hole pattern.
  - Collapse direction.
  - Not played by the user.

## Seventieth pass — the side wall run

- **User:** "Pick up from codex". Codex's handoff named the wall run as next. The plan is in `memory: wallrun-plan`.
- **Source:** `c3b0e91` on main.
  - `ChuckCharacter.cpp/.h`: `EGait::WallSide`, `TryWallSideRun`, `ProbeSideWall`, `LeaveWallSide`, the mesh lean.
  - `DockGameMode.cpp/.h`: stages 116 and 117, plus `-ChuckWallSideTest`.
  - `Verify-Package.ps1`: 131 Claude-side; Codex's additions bring the total to 133.
- **Contract:** no rig or clip change. The existing gaits and their tests are unchanged.
- **Verified:**
  - Root package 133/133: `Local/verify-package-20261003-142806.log`.
  - Editor `-ChuckWallSideTest`: `Local/wallside-test.log`.
- **Remaining:**
  - A dedicated wall-run clip.
  - Camera handling on walls.
  - The narrow-tunnel zombie and the steeper arch.
  - Not played by the user.

## Seventy-first pass — zombie decay, flat collapse, harder

- **User (2026-10-03):** more decayed; fully on the ground when it goes down; too easy (hit more frequently, more easily, harder).
- **Source:** `5c3080f` on main.
  - New: `Tools/build_zombie_textures.py` and the two textures.
  - `build_npc_humans.py`: `skin_texture`, `eye_texture` and the hole tweak.
  - `humans.json`, `build_npc_mocap.py` (the fall to 4.5 s), `Fetch-CMUMocap.ps1` (137_32 dropped).
  - `DockNPC.cpp/.h`: tuning, no stagger, stoop.
  - `DockGameMode.cpp`: review lamp, lit zombie camera, kill cadence 0.15 s.
  - Zombie assets.
- **Contract:** the shared skeleton and the other humans' assets are unchanged.
- **Verified:**
  - Root package 133/133: `Local/verify-package-20261003-145522.log`.
  - Editor `-ChuckZombieTest` and `-ChuckNPCCapture`.
- **Remaining:**
  - Readability in the dark.
  - Facial decay.
  - Sound.
  - Not played by the user.

## Seventy-second pass — pantry ladder, broken floor, cheese, fall deaths

- **User (2026-10-03):**
  - Climb the pantry ladder with the climb animation, up and down, without jumping (ropes later).
  - A bigger pantry with 2D-style breakables, Astral ruptures, and cheese on a crate surrounded by sky that looks jumpable but isn't (no Chult cutscene; the sky is an off-map death).
  - Any off-map fall a death with the summon respawn.
- **Source:** `a606b10`, `ebc2b96`, `43e20a9` on main.
  - New: `ChuckClimbable.cpp/.h` and `Tools/create_pantry_materials.py` with three materials.
  - `DockPantry.cpp/.h` (Codex's; rebuilt).
  - `ChuckCharacter.cpp/.h`: the Ladder gait, `TryMountLadder`, `EnterLadder`, `FallToDeath`, the pantry respawn, the drop-hang speed gate, `SetTestStickWorld`.
  - `ClayJar.cpp/.h`: `PlaceAt`.
  - `DockGameMode.cpp/.h`: stages 118–124 and the stage 4 fall wait.
  - `Verify-Package.ps1`: 133/134 and the pantry check string.
  - `docs/TAVERN-PANTRY.md`.
- **Contract:** no rig or clip change; Codex's shaft and ladder coordinates are kept.
- **Verified:**
  - Root package 135/135: `Local/verify-package-20261003-212909.log`.
  - Editor `-ChuckPantryLadderTest` and `-ChuckPantryCapture`.
- **Remaining:**
  - Sky and cloud art.
  - Turn-in when mounting from above.
  - Shaft camera.
  - Ropes.
  - Not played by the user.

## Seventy-third pass — speed vault

- **User (2026-10-04, reference image):** speed vault over short surfaces when running and jumping; mustn't disturb climbs that follow (crate stairs).
- **Source:** `f0b8999` on main.
  - `build_chuck_v1.py` (SpeedVault) and `gen_chuck_clip_data.py` (VaultClear, VaultDuration).
  - The Chuck V1 blend, manifest and `AS_Chuck_SpeedVault` FBX and uasset.
  - `ChuckCharacter.cpp/.h` (`TryVault`, the Vault gait) and `DockGameMode.cpp/.h` (stages 125 and 126, `-ChuckVaultTest`).
  - `Verify-Package.ps1` (134/135).
- **Contract:** rig unchanged; the other clips are byte-identical in content.
- **Verified:**
  - Root package 136/136: `Local/verify-package-20261003-221922.log`.
  - Editor `-ChuckVaultTest`: `Local/vault-test.log`.
- **Remaining:**
  - Paw-plant fit on unusual depths.
  - Camera.
  - Not played by the user.

## Seventy-fourth pass — low crates to vault

- **User (2026-10-04):** add shorter crates the right height for him to speed-vault.
- **Source:** `d141812` and `44a64ce` on main.
  - New: `VaultCrates.cpp/.h`.
  - `DockGameMode.cpp/.h`: spawn after the jars, stages 127 and 128.
  - `ChuckCharacter.cpp/.h`: the landing-sweep start and `VaultRefusal`.
  - `Verify-Package.ps1`: 136.
- **Contract:** setting addition only (the crates); no rig or clip change.
- **Verified:** root package 137/137, `Local/verify-package-20261003-225236.log`.
- **Remaining:**
  - Bespoke low-crate art (Codex could dress them).
  - Not played by the user.

## Seventy-fifth pass — sewer stream water

- **User (2026-10-04):** better water from free resources, start with the sewer stream (Fab is offered if useful; not needed yet).
- **Source:** `c6b9d1c` on main.
  - New: `Tools/create_sewer_water_material.py`, `M_SewerWater` and the three textures.
  - `DockSewer.cpp` (Codex's): the stream material path, with a fallback.
- **Contract:** setting art only; Codex's stream mesh and checks are unchanged.
- **Verified:** root package 137/137, `Local/verify-package-20261004-093145.log`.
- **Remaining:**
  - Fountain and bay.
  - Glancing brightness.
  - Not seen in motion by the user.

## Seventy-sixth pass — the plaza blacksmith and his anvil

- **User (2026-10-04):** a gruff white male blacksmith by the smithy and forge, with an anvil he works at.
- **Source:** `f19b883` on main (base `7ea8010`).
  - `humans.json` Blacksmith; `build_npc_humans.py` (the `bib` piece, apron options).
  - New: `build_smith_props.py`, `gen_anvil_sfx.py`, `import_anvil_sfx.py`.
  - `import_npc_humans.py` and `Import-NPCHumans.ps1` (`-Only`); `build_spear.py` (merges the manifest).
  - `DockNPC.cpp/.h` (`SpawnBlacksmith`, `PlaceHand`, `HandFrame`, `TickSmith`, `PoseSmith`, `Strike`), `DockGameMode.cpp` (the smith check), `DockPlaza.cpp` (Codex's: the box anvil removed).
  - `Verify-Package.ps1` (136/137 and the smith marker).
- **Contract:** the shared human skeleton is unchanged; the new mesh is on `SKEL_Human`. Chuck's rig and clips are untouched.
- **Verified:** root package 138/138, `Local/verify-package-20261004-114823.log`; worst strike gap 2.9 cm over 73 blows. Editor `-game -ChuckNPCCapture`: `Local/smith-npc-capture.log`.
- **Uncommitted:** 80 re-import-churned, content-equivalent human `.uasset`s (restoring them was blocked this session); see HANDOFF Update 70.
- **Remaining:**
  - A smithing mocap take or authored swing.
  - Bib straps across the back.
  - Beard.
  - Spark art.
  - Not played by the user.

## Seventy-seventh pass — the blacksmith's motion and sounds

- **User (2026-10-04):** do his motion and sound effects.
- **Source:** `3477113` on main.
  - `DockNPC.cpp/.h`: the blow/tap/lift timeline, wrist lag, body drive and recoil, halved idle sway, the inspection pause, clinks, the forge loop, `SmithEvents`.
  - `DockGameMode.cpp/.h`: the check additions and `-ChuckSmithCapture`.
  - `gen_anvil_sfx.py` and `import_anvil_sfx.py`: 12 sounds; `Verify-Package.ps1` (`sounds=12`).
- **Contract:** unchanged (shared human skeleton; no Chuck changes).
- **Verified:** root package 138/138, `Local/verify-package-20261004-124923.log`. Worst blow 0.3 cm, worst tap 2.6 cm, tongs 0.0 cm.
- **Remaining:**
  - A captured smithing take.
  - Leg weight shift.
  - Not played or heard by the user.

## Seventy-eighth pass — cigarette HUD, no reset, full screen

- **User (2026-10-04):** take out the text around the game screen (title, camera mode, scale, controls); one single cigarette HP bar as in the 2D game; remove the R return-to-start; make the game full-screen capable.
- **Source:** `21b8004` on main (base `804a03d`).
  - `DockGameMode.cpp` `ADockHUD::DrawHUD`: the title/camera/scale panel and the bottom controls bar are gone. Sanity is one cigarette in the 2D game's 320x180 pixel layout (`CHUCK-game src/ui/hud.py`, read only), scaled by whole pixels (4x at 720p): filter, paper that burns down with Sanity, ember at the burn line, and a faint ash line. The cigarette count is top right as `xN` with a small unlit cigarette. Talk prompt and dialogue box are unchanged.
  - `ChuckCharacter.cpp`, `DefaultInput.ini`: the `Reset` action (R and the controller's View button) is removed. `ResetToDock` remains for falls, astral respawn and tests.
  - `Tools/Launch-Prototype.ps1`: launches with `-fullscreen` (borderless at desktop resolution, r.FullScreenMode 1) instead of a 1280x720 window. F11 / Alt+Enter toggle remains enabled in `DefaultInput.ini`.
  - `docs/PLAYTEST.md`: note and controls table.
- **Contract:** unchanged.
- **Verified:** candidate `Builds/HudCandidate` 138/138 (`Local/verify-package-20261004-130951.log`), promoted to `Builds/Windows`. Previous package: `Builds/Windows-Previous-20261004-Hud`. Receipt `21b8004` written, `Launch-Prototype.ps1 -CheckOnly` passes. Capture `Builds/Windows/Chuck3D/Saved/Screenshots/Windows/Scale_Elevated.png` shows only the full cigarette and `x5`.
- **Remaining:**
  - The full-screen launch and F11 toggle were not run (the verifier runs windowed); not played by the user.
  - Without a reset, a player stuck somewhere can only quit and relaunch.
  - Older PLAYTEST/DOCKS-RETURN notes still mention R / View; the PLAYTEST note marks them historical.

## Seventy-eighth pass — a more natural hammer strike, quieter

- **User (2026-10-04):** make the hammer strike more natural and realistic; reduce its volume.
- **Source:** `fbd246e` on main: `DockNPC.cpp` (schedule, swing, volumes), `gen_anvil_sfx.py` and the 12 sounds.
- **Verified:** root package 138/138, `Local/verify-package-20261004-140834.log`. Worst blow 0.3 cm, worst tap 3.1 cm.
- **Remaining:**
  - Captured smithing.
  - Tap margin.
  - Not heard by the user.

## Sewer wall-run ease, checkpoint, zombies and NPC-only Astral floor (October 4)

- **Source/delivered:** base `ab08f20`, delivered `01aeeb5` on main. Changed: `ChuckCharacter.{cpp,h}`, `DockSewer.{cpp,h}`, `SewerLife.{cpp,h}`, `DockGameMode.{cpp,h}`, `Tools/Verify-Package.ps1` (expected 142), `docs/SEWER-WALLRIFT.md`, HANDOFF Update 77.
- **Contracts:** no rig, clip, material or asset change. New read-only sewer API: checkpoint sample/location/yaw, nearest sample, `DockSewerAstralFloor()`. `GetSewerZombie()` became indexed (`GetSewerZombieCount/GetSewerZombie(i)/GetSewerZombieSample(i)`).
- **Validation:** verifier 143 passes (`Local/verify-package-20261004-161735.log`), route test `Local/sewer-zombies-route.log` failures=0, wall-side-only `Local/wallside-angled-1.log`. Evidence numbers are in HANDOFF Update 77.
- **Flaws/not tested:** zombie placement and chamber fights not visually reviewed or playtested. The wider angle and air catch affect dock parkour globally; feel not checked by a person. Only one checkpoint: falls after the break return before it, so the wall run must be redone. Zombies float visibly over Astral openings when they cross (the intended cosmology, though it may look odd).
- **Launcher:** root `Builds/Windows` = `01aeeb5`, backup `Builds/Windows-Previous-20261004-SewerZombies`.

Next part of the work can be done here.


## The plaza dwarf (October 4)

- **Owner and task:** Claude, on main. User request: "create a dwarf npc by the smithy, wearing dwarven armor and holding a battle axe. Bearded."
- **Commits:** base `0ee3f2b`, runtime `6d48d4f`.
- **Changed:**
  - `Tools/build_npc_humans.py`: targets; mail, mailskirt, pauldron, vambrace and trim pieces; nasal; beard; per-item offset, slot and folder.
  - New `Tools/build_dwarf_textures.py` and `Tools/build_battle_axe.py`.
  - `humans.json`, both manifests and the NPC README.
  - `DockNPC.*`: `EDockHuman::Dwarf`, `GiveAxe`, `SpawnDwarf`, and pole placement moved into members.
  - `DockGameMode.cpp`: the dwarf check and the pose count of 6.
  - `Verify-Package.ps1`: expected passes 142/143.
- **Contract:** shared skeleton unchanged; new slots only on the dwarf; new `/Game/Characters/Humans/Dwarf` and `Props/SM_BattleAxe`.
- **To reproduce:**
  1. `blender -b -P Tools/build_dwarf_textures.py`
  2. `blender -b -P Tools/build_npc_humans.py -- Dwarf`
  3. `blender -b -P Tools/build_battle_axe.py`
  4. `Tools/Import-NPCHumans.ps1 -Only Dwarf,props`
- **Tests:** `Local/verify-package-20261004-171655.log`, 144 passes against 143 expected. Portraits in `Local/dwarf-npccapture2.log` and the Review PNGs. Full details and flaws are in HANDOFF Update 78.
- **Launcher:** `Builds/Windows` = `6d48d4f`; backup `Builds/Windows-Previous-20261004-Dwarf`.

Next part of the work can be done here.

## Dwarf touch-up: sideburns and rolled rims (October 4)

- **What changed** (`9a3f935`):
  - Sideburns run from the beard up to the helmet rim, tucked under it, with short strand tufts down them.
  - Rolled brass tubes replace the cut trim bands, which broke into strips.
  - Plate hems are relaxed further.
  - The Blender review has a profile camera.
- **Files:** `Tools/build_npc_humans.py`, `humans.json`, the Dwarf FBX, uassets and manifest.
- **Tests:** `Local/verify-package-20261004-174118.log`, 144 passes against 143 expected.
- **Launcher:** `Builds/Windows` = `9a3f935`; backup `Builds/Windows-Previous-20261004-DwarfBurns`.

Next part of the work can be done here.

## Dwarf: rigid lamellar pauldrons, beard strands, brows (October 4)

- **What changed** (`2973957`):
  - Pauldrons are three rigid lames with rolled brass rims (`rigid`, trims addressed by `id`).
  - 220 more strand cards lie along the beard's surface.
  - `brow_tint` gives auburn brows.
- **Tests:** `Local/verify-package-20261004-180629.log`, 144 passes against 143 expected.
- **Launcher:** `Builds/Windows` = `2973957`; backup `Builds/Windows-Previous-20261004-DwarfLames`.

Next part of the work can be done here.

## Dwarf: no looking down, chest-anchored beard, axe clearance, nose (October 4)

- **What changed** (`48cdda5`):
  - His look is clamped level, with head yaw at most 25° and the clip's head motion damped.
  - The beard hang is weighted to `spine_03` below the chin.
  - The axe stands 34 cm out and leans 9°.
  - MakeHuman nose targets.
  - New `-ChuckDwarfCapture`, plus a look-pitch check.
- **Tests:** `Local/verify-package-20261004-190150.log`, 144 passes; capture log `Local/dwarf-closeup-capture3.log`. The 38 cm attempt failed the pose check and was reverted.
- **Launcher:** `Builds/Windows` = `48cdda5`; backup `Builds/Windows-Previous-20261004-DwarfNose`.

Next part of the work can be done here.

## Tavern keeper (user 2026-10-04)

- **Request:** a tavern keeper like the blacksmith with a medium beard and a different apron, behind the counter between the hatch and the barrels, polishing a tankard silently.
- **Source:** `c2e8975` on main.
  - `humans.json` TavernKeeper; `build_npc_humans.py` (the `braids` option).
  - New: `build_keeper_props.py`.
  - `import_npc_humans.py` (`prop:<Name>`).
  - `DockNPC.cpp/.h` (`SpawnTavernKeeper`, `PoseKeeper`) and `DockGameMode.cpp/.h` (the check and `-ChuckKeeperCapture`).
  - `Verify-Package.ps1`.
- **Contract:** shared human skeleton; no Chuck, tavern geometry or route changes.
- **Verified:** package 145, `Local/verify-package-20261004-193825.log`. Grip 0.0 cm, rag 0.3 cm.
- **Remaining:**
  - Prop art.
  - Beard detail.
  - Not played by the user.

## Dwarf by the quench barrel, bushy beard (October 4)

- **What changed** (`81fc1d0`):
  - He now stands beside the smith's quench tub, and the check requires him within 140 cm of it.
  - `make_beard` gains a `bushy` option: a broad mass resting on the breastplate, tufts, 320 fluff cards, chest anchoring from above the chin.
  - Head yaw is at most 15°.
  - The axe is at 33 cm out, 7° lean, grip at 78 cm (two other settings failed the arms-down check and were not kept).
- **Tests:** `Local/verify-package-20261004-202408.log`, 145 passes; close-ups in `Local/dwarf-bushy-capture4.log`.
- **Flaky:** the tavern keeper's rag reach check failed once (8.5 cm against a 4 cm limit) in `-201933.log`. It is unfixed.
- **Launcher:** `Builds/Windows` = `81fc1d0`; backup `Builds/Windows-Previous-20261004-DwarfBushy`.

Next part of the work can be done here.
