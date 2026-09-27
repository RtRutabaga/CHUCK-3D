# Handoff — 2026-09-26

## Current priority

**Latest source pickup:** Claude has now delivered v1 through `c5d6d25`; all source commits are integrated and the mesh, nine clips and baked textures are imported side by side under `/Game/Characters/Chuck/V1`. Read `CHUCK-V1-INTEGRATION.md` for verified source/import results and reproduction. This supersedes the earlier “awaiting source delivery” status below. The normal playable build still uses the old runtime; AnimInstance/AnimBP and contact migration is the next gate before switching it.

The user's resource guide has been copied unchanged with its SHA-256 provenance. Read `RESOURCE-GUIDE-NOTES.md` alongside it: reuse existing art/animation where practical, verify current resource terms/hardware fit, preserve the user's reference target and keep this character-first scope.

**Rig proposal reviewed and accepted by Codex.** Read `docs/RIG-CONTRACT-V1.md` before changing bones or starting new runtime animation. Claude's proposal files from `51ff219` were recorded separately; subsequent art/rig deliveries are now integrated as source. The current playable package remains the verified 9b9bd46 integration described below. V1 clips run in source/import review, but no new Animation Blueprint or game features are running in the playable build yet.

Codex answered all five rig questions: SkeletalMesh + AnimInstance/AnimBP Two Bone IK, no Control Rig dependency now; accepted 41-bone hierarchy/rest coordinates; preserve and verify axes/helpers on import; derive new reach/poles from rest pose; fit sole markers and a real cigarette-tip attachment. Claude followed this agreement in its delivered v1 rig/skin/clips. Its worktrees were not changed.

Source contract validation passed with `Tools/Check-RigContract.py --output Local/rig-v1-review.json`: 41 bones / 35 deforming, script/JSON match, valid hierarchy, mirrored forward knee poles, upper/lower leg lengths 8.7687/9.3670 cm. Subsequent source QA passed 66 checks, and Unreal import validation confirmed rest positions, clip motion and repaired loop timing. Editor captures now explicitly refresh and validate the applied poses; walking and landing images were visually inspected. A small C++ editor-review helper was added and built; gameplay/controller code is unchanged. Packaged v1 controller/deformation verification remains outstanding.

**Character first. World/setting work is paused.** The user says Chuck remains far from the supplied goal: primitive appearance, cartoony movement, unnatural leg placement and unintended gaps along the open jacket/zipper edges. The latest checks do not establish acceptable character quality. Read CHARACTER-PLAN.md before more implementation and AGENT-WORKFLOW.md for the prepared Codex/Claude Code split.

The eventual rig must support walking, running, rolling, side-jumping, climbing and smoking with a cigarette kept in the mouth. These are design requirements, not a claim that all actions are playable. Keep Chuck silent, restrained and about 65 cm tall; oversized open purple jacket, gray-brown rat anatomy. The supplied reference images take precedence over the current procedural study.

## What runs

Current character integration: Claude source deliveries 9e337c3/939580a integrated as 483bbaf/bc7d711, plus Codex contact/stride/turn-recovery code through 3bcbc71. Public remote: https://github.com/RtRutabaga/CHUCK-3D. Double-click Launch-Prototype.cmd for the existing local Windows package. See PLAYTEST.md for controls and the same-route camera comparison. Unreal 5.7.4 CL 51494982, Blender 4.5.14 LTS 62c1db4208e8; hardware/toolchain details in SETUP.md. No new engine, Blender or development dependency was installed in these graphics passes.

One runtime-generated Waterdeep dock scene: quay, pier with a jumpable missing board, closed tavern frontage, stationary 180 cm worker, barrel, crate, bench and low step. Custom Blender prop/worker/boat meshes replace visible blockout shapes; original hidden collision preserves the route. No new map, interior, conversation, combat or pickup system. The editor level is intentionally empty until Play constructs it.

Chuck uses a Blender body with a 14-bone authored rig plus any imported armature root. Procedural leg IK, sleeve motion, tail sway and independent feet remain a prototype. Feet now alternate world-locked stance and short predictive swings with ground traces on the existing static route. Body source: 159,524 triangles; reusable foot: 10,944 triangles. Directional geometric fur covers portions of head, chest, belly and legs. Claude rebuilt continuous jacket fronts/collar/lapels, lining and zipper edges, then graded sleeve weights through the elbow. Visible shoulder/armhole joins and simplified anatomy still need work. Runtime now preserves Skin and Claw foot material slots. No production groom, cloth simulation, authored motion clips, general slope/stair/platform support or LODs.

Twenty-two art materials include original procedural character/world surfaces and CC0 Poly Haven stone/timber maps with recorded source hashes/licenses. DX11 SM5 uses screen-space reflections, 35 cm AO and 8 cm contact shadows; no Lumen/ray tracing/motion blur. Water is opaque and shoreline/reflection quality remains limited. SourceAssets readmes and GRAPHICS-PASS.md describe reproduction and limits. Do not expand these world studies while character work is the priority.

## Cameras and controls

Both cameras remain, with no selection: elevated boom 400 cm, pitch -48 degrees, FOV 65; rat-height boom 220 cm, lens about 65 cm high, FOV 78. Switching/orbit blends and positional lag is capped at 8 cm. Camera collision hides Chuck within 70 cm of the lens; foreground obstruction and comfort still need user feedback.

WASD/arrows/left stick walk; Space/A jump; C/Y switch camera; mouse/QE/right stick turn; F/right-stick click recenter; R/View reset; Esc/Menu exit. Mouse/right-stick Y adjusts rat-height pitch. See PLAYTEST.md for exact launch and verification commands.

## Verified latest gameplay milestone

- Claude's committed Blender source passed check_model.py: 65 cm bounds, 14 named bones/parents, material slots, normalized weights, closed jacket shell and graded sleeve weights. Log: Local/claude-review-check.log. Source FBXs were imported directly; no generator rerun overwrote the accepted art.
- Windows Development BuildCookRun completed successfully: Local/character-integration-final-build.log. No C++ warning/error or material compile/invalid shader-map failure was found in that log.
- Tools/Verify-Package.ps1 -MotionCapture passed **43 rendered checks**, zero failures: Local/verify-package-20260926-130525.log. The wrapper requires the success marker, not just exit code. Without captures it expects 42 checks; physical Xbox hardware remains unverified.
- Baseline flat-route stance-foot slip: mean 70.5968 cm/s, maximum 100.0720 cm/s across 43 samples. Final: mean/max 0.0000 cm/s across 51 samples; maximum ankle-target reach excess 0.0000 cm. These are measurements on the scripted straight walk, not a guarantee for every terrain or input sequence.
- Motion review caught long-stride ankle separation, then sharp-turn unreachable targets. Shortened steps, bounded pelvis lowering and release/recovery of unreachable contacts address those specific faults. Two intermediate reach runs failed; the threshold was retained. Abrupt turn recovery can still shuffle and is not finished authored animation.
- Rear/front/side rat-height and elevated sequences cover start, walk, 90-degree turn, stop, jump and landing. PNGs: Builds/Windows/Chuck3D/Saved/Screenshots/Windows/Motion/View0..3. Local/MotionReview contains GIFs and sampled-frame contact sheets. The verifier clears only previous generated frame PNGs before a new motion run so stale frames cannot mix into evidence.
- Loaded art/scale/bones/materials, walk/jump/landing, wall/prop collision, reset, simulated keyboard/Xbox mappings, camera transitions/recentering and the pier gap in both views remain covered. Physical controller, sustained performance, arbitrary terrain and subjective camera comfort are not established.
- Visible shoulder/armhole joins, primitive head/hands/limbs, rigid foot/toe behavior and segmented leg surfaces remain defects. The character is still far from the supplied reference quality. World work stays paused.
- Local Git LFS fsck passed. Generated Unreal output, local motion evidence/worktrees and Blender backups are excluded. A new independent remote asset download was not performed in this integration.

## Two-agent preparation

CLAUDE.md imports shared AGENTS.md. CHARACTER-PLAN.md records defects, action requirements and review evidence. AGENT-WORKFLOW.md defines separate branches/worktrees, file ownership, initial rig contract, handoffs and integration. Ready-to-paste assignments are in docs/agent-tasks. Tools/New-AgentWorktrees.ps1 creates/reuses ignored Local/AgentWorktrees/codex-movement and claude-character without resetting existing work. The initial roles are Codex movement/runtime and Claude Blender character forms/jacket; the main integration owner imports accepted assets, verifies and publishes.

Claude has now delivered two source commits from its desktop-managed worktree. Both are integrated; its working branch has been left intact. The initial installation notes in AGENT-WORKFLOW.md are historical preparation guidance. No dependency installation or login was performed by Codex during this integration. Run only one Unreal build/editor/import at a time on this 16 GB machine. Keep binary ownership explicit and don't run obsolete generators over manual art changes.

## Repository boundaries and publication

The original CHUCK-game is strictly read-only. Reference HEAD: 87585dd6efb3d9fb0a44dd33549fa521f17b6701. Only selected historical documents/excerpts were copied; References/PROVENANCE.md records origins/hashes. No original code/assets were copied. Missing geography/player-progression supplements were not found; do not invent them.

CHUCK-3D has a separate public remote and no deployment. Standing user permission covers future verified project code/docs/assets commits and pushes, including supplied JPG references and Blender/FBX/Unreal assets. Do not ask again for routine publication. Paid services, sensitive data, destructive changes and the original game are outside that authorization. All authored binary types use LFS; remaining remote account allowance is unknown. No paid storage was purchased. Builds, Local evidence/worktrees, caches, Binaries, Intermediate, Saved and Blender backups remain ignored.

The user previously requested sustained graphics work, then specifically directed finishing/publishing the in-flight tavern pass and preparing this two-agent workflow. Do not resume world work or launch both agents automatically from that earlier request. Next implementation is character-first under the prepared assignments.

Codex movement was developed and committed in its prepared worktree and cherry-picked onto main for packaging with shared caches. Claude owns source art; Codex owns runtime; integration owns Unreal imports. Do not reset Claude's checkout or regenerate its accepted source with an older generator. See docs/agent-handoffs/CODEX.md and CLAUDE.md for specific deliverables and remaining work.
