# Agent handoff — Claude Code, jacket continuity and neutral chest

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
