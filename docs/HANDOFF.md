# Handoff — 2026-09-26

## Current priority

**Character first. World/setting work is paused.** The user says Chuck remains far from the supplied goal: primitive appearance, cartoony movement, unnatural leg placement and unintended gaps along the open jacket/zipper edges. The latest checks do not establish acceptable character quality. Read CHARACTER-PLAN.md before more implementation and AGENT-WORKFLOW.md for the prepared Codex/Claude Code split.

The eventual rig must support walking, running, rolling, side-jumping, climbing and smoking with a cigarette kept in the mouth. These are design requirements, not a claim that all actions are playable. Keep Chuck silent, restrained and about 65 cm tall; oversized open purple jacket, gray-brown rat anatomy. The supplied reference images take precedence over the current procedural study.

## What runs

Last gameplay milestone: 7104609, published to https://github.com/RtRutabaga/CHUCK-3D. Double-click Launch-Prototype.cmd for the existing local Windows package. See PLAYTEST.md for controls and the same-route camera comparison. Unreal 5.7.4 CL 51494982, Blender 4.5.14 LTS 62c1db4208e8; hardware/toolchain details in SETUP.md. No new engine, Blender or development dependency was installed in these graphics passes.

One runtime-generated Waterdeep dock scene: quay, pier with a jumpable missing board, closed tavern frontage, stationary 180 cm worker, barrel, crate, bench and low step. Custom Blender prop/worker/boat meshes replace visible blockout shapes; original hidden collision preserves the route. No new map, interior, conversation, combat or pickup system. The editor level is intentionally empty until Play constructs it.

Chuck uses a Blender body with a 14-bone authored rig plus any imported armature root. Procedural leg IK, sleeve motion, tail sway, independent feet and sine-driven gait remain a prototype. Body source: 131,194 triangles; reusable foot: 10,944 triangles. Directional geometric fur covers portions of head, chest, belly and legs. Open jacket has seams/lapels/folds but construction defects remain. Runtime now preserves Skin and Claw foot material slots. No production groom, cloth simulation, authored motion clips, terrain-aware planting or LODs.

Twenty-two art materials include original procedural character/world surfaces and CC0 Poly Haven stone/timber maps with recorded source hashes/licenses. DX11 SM5 uses screen-space reflections, 35 cm AO and 8 cm contact shadows; no Lumen/ray tracing/motion blur. Water is opaque and shoreline/reflection quality remains limited. SourceAssets readmes and GRAPHICS-PASS.md describe reproduction and limits. Do not expand these world studies while character work is the priority.

## Cameras and controls

Both cameras remain, with no selection: elevated boom 400 cm, pitch -48 degrees, FOV 65; rat-height boom 220 cm, lens about 65 cm high, FOV 78. Switching/orbit blends and positional lag is capped at 8 cm. Camera collision hides Chuck within 70 cm of the lens; foreground obstruction and comfort still need user feedback.

WASD/arrows/left stick walk; Space/A jump; C/Y switch camera; mouse/QE/right stick turn; F/right-stick click recenter; R/View reset; Esc/Menu exit. Mouse/right-stick Y adjusts rat-height pitch. See PLAYTEST.md for exact launch and verification commands.

## Verified latest gameplay milestone

- Windows Development BuildCookRun completed successfully; final log Local/tavern-final-build.log contains no material compile failure, invalid shader map or remaining variable-shadow warning.
- Tools/Verify-Package.ps1 completed with **42 passing rendered checks**, zero failures: Local/verify-package-20260926-114439.log. It requires the success marker, not merely Unreal's exit code. Without captures it expects 41 checks; that is not a claim of a separate physical-controller run.
- Checks include loaded character/prop assets, scale, required bones/materials, sampled pose behavior, walking/jumping/landing, wall/prop collision, fall reset, simulated keyboard/Xbox input, camera transitions/recentering and the pier gap in both views.
- Front/walking/scale/harbor and Tavern_RatHeight captures were inspected across the passes. Evidence is ignored under Local and Builds/Windows/Chuck3D/Saved/Screenshots/Windows. Still captures do not establish natural animation quality; the user specifically finds the current motion unacceptable.
- Physical Xbox hardware, sustained frame-time performance, production animation and subjective camera comfort remain unverified. Existing tests tied to the old lean/bob implementation must not force preservation of unwanted motion.
- Published LFS assets through d233879 were independently downloaded, fsck-checked and sampled source/import hashes matched. Later fur/tavern pushes and local LFS fsck succeeded; final round-trip status should be recorded when performed. Generated Unreal output and Blender backups remain excluded.

## Two-agent preparation

CLAUDE.md imports shared AGENTS.md. CHARACTER-PLAN.md records defects, action requirements and review evidence. AGENT-WORKFLOW.md defines separate branches/worktrees, file ownership, initial rig contract, handoffs and integration. Ready-to-paste assignments are in docs/agent-tasks. Tools/New-AgentWorktrees.ps1 creates/reuses ignored Local/AgentWorktrees/codex-movement and claude-character without resetting existing work. The initial roles are Codex movement/runtime and Claude Blender character forms/jacket; the main integration owner imports accepted assets, verifies and publishes.

Claude Code was not found on PATH or in the usual native/npm launcher locations. Git Bash is installed. No Claude installation, login, permission bypass or session was started; the user requested groundwork for their setup. Official Windows installation/sign-in steps are linked in AGENT-WORKFLOW.md. Run only one Unreal build/editor/import at a time on this 16 GB machine. Keep binary ownership explicit and don't run obsolete generators over manual art changes.

## Repository boundaries and publication

The original CHUCK-game is strictly read-only. Reference HEAD: 87585dd6efb3d9fb0a44dd33549fa521f17b6701. Only selected historical documents/excerpts were copied; References/PROVENANCE.md records origins/hashes. No original code/assets were copied. Missing geography/player-progression supplements were not found; do not invent them.

CHUCK-3D has a separate public remote and no deployment. Standing user permission covers future verified project code/docs/assets commits and pushes, including supplied JPG references and Blender/FBX/Unreal assets. Do not ask again for routine publication. Paid services, sensitive data, destructive changes and the original game are outside that authorization. All authored binary types use LFS; remaining remote account allowance is unknown. No paid storage was purchased. Builds, Local evidence/worktrees, caches, Binaries, Intermediate, Saved and Blender backups remain ignored.

The user previously requested sustained graphics work, then specifically directed finishing/publishing the in-flight tavern pass and preparing this two-agent workflow. Do not resume world work or launch both agents automatically from that earlier request. Next implementation is character-first under the prepared assignments.
