# Handoff — 2026-09-27

## Current launcher and integration status

**Update 13 (Codex, September 29 — supplied soundtrack):** runtime `18e6144` adds the user's `waterdeep_docks.wav` as looping non-spatial music, 45% volume with a 1.5-second fade-in. It starts with the level and continues through position resets. The unmodified 158.4-second stereo 48 kHz source, SHA-256 provenance and import notes are in `SourceAssets/Audio`. Source WAV and imported SoundWave use LFS (roughly 49 MB total); no new dependency or original-game access. Import commandlets require `-AllowCommandletAudio`; the initial decoder ensure was resolved by enabling it.

Import and Windows build succeeded (`Local/music-import-audio.log`, `Local/music-build.log`). Packaged rendered verifier passed **99/99**, **15 route checks**, and `CHUCK_MUSIC_CHECK failures=0 looping=1 playing_after_boundary=1` (`Local/verify-package-20260929-205701.log`). The audio smoke run starts two seconds before the source end and checks six seconds later; normal play starts at the beginning. Playback state and active audio device were checked, not subjective loudness or an audible seamless transition. The track's original ending is preserved; no crossfade edit. LFS fsck passed; generated output remains untracked.

Root launcher now uses the verified music package, runtime `18e6144`; previous detail build retained at `Builds/Windows-Previous-20260929-Music`. No movement, camera or setting geometry changes. Next work can be done here.

**Update 17 (Claude, 2026-09-30, user request: low rake, cigarettes, jars):** runtime `5a869d8` adds:
- an automatic low rake for low targets;
- 10 breakable clay jars (solid; they shatter into shards and drop 1–3 cigarettes);
- cigarette pickups (about one grass tuft in three holds one) and a top-right counter.

Design rule (user): Chuck breaks grass and jars (urns in later maps), never crates or barrels; recorded in `PROJECT-BRIEF.md`. Details are in `agent-handoffs/CLAUDE.md` pass 54.

The root candidate passed `-MotionCapture` **113/113**, the world check and the music check (`Local/verify-package-20260930-092918.log`). It was promoted to `Builds/Windows` with receipt `5a869d8`; the previous package is kept as `Builds/Windows-Previous-20260930-Loot`. Not yet played by the user.

**Update 16 (Claude, 2026-09-30, user request: shreddable grass tufts):** runtime `da4eea5` adds 51 generated weed tufts around the docks and a shared breakable base (`ChuckBreakable`) for grass, then jars and small enemies. A slash shreds any tuft in reach to stubble, with a clipping spray and a rustle, and Chuck walks through grass. Details are in `agent-handoffs/CLAUDE.md` pass 53 and `PLAYTEST.md`.

The root candidate passed `-MotionCapture` **110/110**, the world check and the music check (`Local/verify-package-20260930-085524.log`). It was promoted to `Builds/Windows` with receipt `da4eea5`; the previous package is kept as `Builds/Windows-Previous-20260930-Grass`. Not yet played by the user. Next suggested: the low rake for low targets, then cigarette drops and pickups.

**Update 15 (Claude, 2026-09-30, user request: strafe + jump always side jumps):** jump with a strafe key (Q/E, or LT with the stick sideways) held is now always a side jump, even while running forward; the keys are read live, so pressing strafe and jump together works. Details are in `agent-handoffs/CLAUDE.md` pass 52. The root candidate's first `-MotionCapture` run failed one check: flurry paw slip 6.7 cm/s after a single slow frame (a 0.248 s strike gap). The slash code was unchanged, and the check had passed in all four worktree runs. Two reruns of the same package passed **108/108** plus the world and music checks (`Local/verify-package-20260930-081436.log`). Promoted to `Builds/Windows` with receipt `de4b81f`; the previous package is kept as `Builds/Windows-Previous-20260930-SideJump`. The flurry slip check can fail on a slow frame.

**Update 14 (Claude, 2026-09-29, user request: movement sound effects):** runtime `cf0a70e` adds generated SFX (`Tools/gen_chuck_sfx.py`, `SourceAssets/Audio/SFX`, imported to `/Game/Art/Audio/SFX`). They play 2D under the soundtrack: paw steps on wood and stone (walk and run), jump, land, claw slash and roll. Details are in `agent-handoffs/CLAUDE.md` pass 51 and `SourceAssets/Audio/README.md`.

The root candidate passed `-MotionCapture` **107/107**, the world check and the music check (`Local/verify-package-20260929-221028.log`). It was promoted to `Builds/Windows` with receipt `cf0a70e`; the previous package is kept as `Builds/Windows-Previous-20260929-Sfx`. Loudness against the music and how they sound haven't been judged by ear yet.

**Update 13 (Claude, 2026-09-29, user requests: strafe, faster run, drop to hang, landing roll height):** runtime `c0154fd` adds:
- strafe on Q/E or held LT, with jump while strafing = side jump (short from a walk, long from a run);
- a faster run (225 cm/s);
- drop to hang when walking gently off an edge;
- the landing roll only above a 160 cm fall.

Q/E no longer turn the camera. Details are in `agent-handoffs/CLAUDE.md` pass 50 and `PLAYTEST.md`.

The candidate built in the root checkout passed `Verify-Package.ps1 -MotionCapture` **106/106**, the world check and the music check (`Local/verify-package-20260929-211245.log`). It was promoted to root `Builds/Windows` with receipt `c0154fd`; the previous package is kept as `Builds/Windows-Previous-20260929-Strafe`. Worktree runs also passed `-NoGroom` and uncapped with and without groom. Not yet played by the user.

This pass was authored in Claude's old app worktree and fast-forwarded onto main (that session was pinned there). Later Claude sessions should open the root project.

**Workflow update:** the user now runs Codex and Claude sequentially and explicitly requests direct work on root `main`. Read v3 in `AGENT-WORKFLOW.md` and the updated `CLAUDE.md`. Preserve unfinished changes; use one root launcher and update its verified package for playable milestones. Existing worktrees remain intact as optional development copies. This documentation-only update does not change the game or require rebuilding it.

**Update 12 (Codex, September 29 — near-field dock details):** runtime `42f33d7` reuses existing beveled crate/rope meshes in the loading court and adds platform boards, loading-door joinery, high barred warehouse windows, a net-drying frame, tavern rear windows/timber bays and continuous drain grates. Existing cargo proxy dimensions are preserved, with proxies hidden when the visual mesh is available. Added dressing is noncolliding. No character/controller, binary source or material changes; no installs or asset regeneration.

Built successfully (`Local/dock-detail-build.log`). Packaged rendered checks passed **99/99** plus **9 ground / 6 capsule-route checks**, no material fallback failures (`Local/verify-package-20260929-133159.log`). Four setting captures were visually reviewed (`Local/dock-detail-capture.log`; package `Saved/Screenshots/Windows/Setting`). MotionCapture and physical-controller testing were not repeated. Repeated architecture, basic roof/material treatment and simplified water remain unfinished; this does not meet the reference graphics target.

Normal root launcher uses this verified package, runtime `42f33d7`; previous skyline package is retained at `Builds/Windows-Previous-20260929-Detail`. Generated output remains untracked. Next setting refinement can be done here; character/traversal remain Claude-owned.

**Update 11 (Codex, September 29 — skyline and working facades):** runtime `6aa739b` adds stepped city land, uphill roof rows, a distant civic hall/towers, a northern continuation and opposite-bank buildings. Canopies, brackets, shutters and trade signs distinguish Dock Street; shallow details dress the parkour workshop faces. This is visual scenery only: playable collision, character/controller, obstacle dimensions and materials are unchanged. No new binary assets or dependencies.

Build succeeded (`Local/skyline-build.log`). Packaged verifier with rendered captures passed **99/99** plus **9 ground samples / 6 capsule routes**, with no material fallback failures (`Local/verify-package-20260929-131527.log`). Four setting captures were visually reviewed (`Local/skyline-capture.log`; package `Saved/Screenshots/Windows/Setting`). MotionCapture was not rerun for this scenery-only pass. Repeated architecture, basic materials, distant boundaries and simple water remain visible limitations; background land is nonplayable scenery.

Normal launcher package now carries runtime `6aa739b`; previous connected-docks build retained at `Builds/Windows-Previous-20260929-Skyline`. Use root `Launch-Prototype.cmd`. Generated files remain untracked. Next setting refinement can be done here; Claude retains character/traversal ownership.

**Update 10 (Codex, September 29 — connected docks setting):** reviewed Claude's integrated work through `c2f26c3` and preserved its parkour geometry, character assets and controller. The user explicitly reopened setting work. Runtime `ab885d8` adds Dock Street, a cargo court, a service quay and tavern rear court; seven solid buildings, fuller old frontage shells, contextual obstacle dressing and 22 grounded background buildings. See `DOCKS-SETTING.md` for routes, ownership and limits.

Packaged with existing Unreal 5.7.4, no installs or character regeneration. Final build: `Local/setting-build-final.log`. `Verify-Package.ps1 -MotionCapture -PackageRoot Builds/SettingCandidate/Windows` passed **99/99**, plus **9 ground samples and 6 capsule-clearance routes**, with no material fallback/compile failures: `Local/verify-package-20260929-124955.log`. The first candidate passed traversal but exposed missing instancing flags on world materials; those flags were corrected and the full suite rerun. Seven existing LFS material assets total about 191 KB; no new character binaries. LFS integrity passed and generated files remain untracked.

Four packaged setting views were reviewed at overview and street level. The expanded district remains repeated blockout architecture with closed doors and simple water; the harbor horizon and distant edges still need development. These checks do not establish physical Xbox behavior, subjective camera comfort, natural traversal on every new facade, or the reference graphics target. Claude retains character/traversal ownership. Codex setting code is isolated in `DockSetting.cpp/.h` with one scene-construction call.

The verified candidate is the normal `Builds/Windows` launcher package; the previous version is retained as `Builds/Windows-Previous-20260929-Setting`. Always use the repository-root `Launch-Prototype.cmd`, whose receipt identifies runtime `ab885d8`. No original CHUCK-game files were touched.

**Update 9 (Claude, 2026-09-29, user request "Integrate and push to GitHub"):** main fast-forwarded to `c2f26c3`, adding:
- ledge corners (outside and inside, with the stick carried round the turn);
- an automatic landing roll after falls over 80 cm;
- the side-on chimney camera, which follows the climb's height;
- side jumps into a wall starting a wall run.

Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **99/99** (`Local/verify-package-20260929-120607.log`); receipt `c2f26c3`. Pushed main to `origin` at the user's request. Next: Codex takes the setting/world work.
**Update 8 (Claude, 2026-09-29, user request "Integrate"):** main fast-forwarded to `7324500`, adding:
- the running jump and tap-to-run;
- the claw slash: a wide, fast arc and a held flurry with a random paw order;
- parkour: wall run, wall jump, ledge grab/hang/pull-up/drop, knee-high mantle, shimmy and the hang camera;
- the practice yard, cargo wharf, Chandlers' Row and Timber Yard;
- sleeves that read as arms in a coat, and a vivid violet jacket.

Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **94/94** (`Local/verify-package-20260929-100457.log`); receipt `7324500`. Nothing pushed.

**Update 7 (Claude, 2026-09-27, user request "Integrate"):** main fast-forwarded to `f0b296c`, adding:
- the run (tap Shift / LB);
- rolling straight back into the run with the stick held;
- keyboard side-jump fixes (a dodge cuts a turn in place short; the stick is read directly);
- the running jump (a split leap that lands into the stride).

Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **69/69** (`Local/verify-package-20260927-215040.log`); receipt `f0b296c`. Nothing pushed.

**Update 6 (Claude, 2026-09-27, user request "Integrate and run"):** main fast-forwarded to `08a425b`, adding the first roll and side jump: C / Xbox B dodge; the stick held sideways (camera-relative) gives a side jump. Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **57/57** (`Local/verify-package-20260927-192523.log`); receipt `08a425b`. Nothing pushed.

**Update 5 (Claude, 2026-09-27, user request "Integrate and begin roll and side jump"):** main fast-forwarded to `fd78cc8`, adding:
- the aplomb idle and swagger saunter;
- wider, fuller legs;
- a more exaggerated, brisk swagger (72 cm/s);
- the GTA-style continuous orbit camera, which replaces the C/Y camera switch.

Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **51/51** (`Local/verify-package-20260927-184911.log`); receipt `fd78cc8`. Nothing pushed.

**Update 4 (Claude, 2026-09-27, user request "Integrate"):** main fast-forwarded to `2fa74eb`: ears wider apart on the outer top skull corners; neutral human-like hands (palm to thigh, thumb forward, fingers curling inward, clips curl medially). Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **50/50** (`Local/verify-package-20260927-150737.log`); receipt `2fa74eb`; content paks cooked at 15:07. Nothing pushed.

**Update 3 (Claude, 2026-09-27, user request "Integrate"):** main fast-forwarded to `1057e0f`, adding:
- ears arranged as in the turnaround: forward cups on the upper skull corners, seated so they cannot float;
- natural forward finger curl;
- textured head fur between sleek and the haircut crown.

Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **50/50** (`Local/verify-package-20260927-135522.log`); receipt `1057e0f`. The executable was not relinked (no C++ change since `37208d3`); the cooked content paks are from 13:55. Nothing pushed.

**Update 2 (Claude, 2026-09-27, user request "Integrate"):** main fast-forwarded to `37208d3`, adding four passes:
- the face-proportion pass: a short conical rat snout (0.74, v1.2 amendment), smaller high eyes;
- a scruffy city-rat body coat with guard hairs;
- a softer tan-cream muzzle and amber eyes;
- sleek, laid-back head fur (the lifted crown had read as a human haircut).

Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **50 passes, 0 failures** (`Local/verify-package-20260927-130138.log`); receipt `37208d3`; `Launch-Prototype.ps1 -CheckOnly` shows no newer-source warning. Nothing pushed.

**Update (Claude, 2026-09-27, user request "integrate"):** Claude merged `codex/claude-character` into main as `4eef511` (merge of `699efd0..c2e31d5` over Codex's `a4b00b2`; one clean automatic merge). That brings in:
- the four 2026-09-27 goal images and the user's snout/fur target (`References/ArtDirection/`);
- the look passes: warm shaggy groom, narrow head, cropped red-violet jacket;
- the v1.1 proportions and the v1.2 snout amendment (`docs/RIG-CONTRACT-V1.md`, `Tools/chuck_v1_shape.py`);
- the cigarette prop and smoke (`-ChuckNoCigarette` removes it);
- long clawed hands and a pointed shirt collar;
- a 136,000-strand groom with per-strand colour variation.

The package was rebuilt **in the main checkout**: editor module, then BuildCookRun into `Builds/Windows`. Then:
- `Verify-Package.ps1 -MotionCapture`: **50 passes, 0 failures** (`Local/verify-package-20260927-121313.log`).
- `Write-PrototypeReceipt.ps1` recorded build `4eef511`.
- `Launch-Prototype.ps1 -CheckOnly` identified it with no newer-source warning.

`Launch-Prototype.cmd` now opens this Chuck. Not verified by this integration: a physical controller, subjective camera comfort, and whether the look matches the user's taste. Remaining look gaps are listed in `docs/agent-handoffs/CLAUDE.md`, 21st pass. Nothing was pushed to the public remote.

Claude's delivery through `00e46d6` is now integrated on main: v1 skeletal character, native AnimInstance/contact IK, authored walk/start/stop/turn/jump clips, three bound groom groups, and the revised jump/follow cameras. Character/art ownership follows the v2 split in AGENT-WORKFLOW.md. World work remains paused.

The user reported that the normal launcher showed none of this. Cause: `Launch-Prototype.cmd` still opened main's September 26 13:03 executable, while Claude packaged the new game in `.claude/worktrees/project-orientation-fd7504/Builds/Windows`. Git commits and asset imports do not replace a packaged executable or its cooked content.

Codex copied Claude's package into an isolated candidate (without changing Claude's worktree), ran `Verify-Package.ps1 -MotionCapture`, and obtained **49 passes, zero failures** in `Local/verify-package-20260927-093257.log`. The fresh front capture was visually inspected and shows the v1 character. That exact candidate was promoted to main's `Builds/Windows`; the old package is retained at `Builds/Windows-Previous-20260926`. Captures now live under `Builds/Windows/Chuck3D/Saved/Screenshots/Windows`.

`Launch-Prototype.cmd` now calls `Tools/Launch-Prototype.ps1`, which checks the executable against a verification receipt, identifies build `00e46d6`, and warns when newer committed Unreal changes are not packaged. `Build-Prototype.ps1 -Package` now runs package verification and writes the receipt before declaring the build ready. `Verify-Package.ps1 -PackageRoot <candidate>` supports testing a copied build before promotion. Normal launch still uses main's package, never an arbitrary agent worktree.

The actual `.cmd` launcher was executed after promotion and the running process path confirmed as main's `Builds/Windows/Chuck3D/Binaries/Win64/Chuck3D.exe`. The initial launcher test exposed an unavailable `Get-FileHash` command in that shell environment; hashing now uses .NET directly and the retest passed. The game was left open for the user. Script syntax and Git whitespace checks passed; no new engine build was necessary because the exact copied package passed independent verification.

Interrupted Codex groom work was preserved in Git stash `e3164da437321014069a0c5bb31e14a3b1431c7a` before the fast-forward; do not reapply it over Claude's replacement implementation. No new character or world design was done during this launcher repair. Physical controller and subjective camera comfort remain unverified; the reference-quality art target is still unmet.

## Historical handoff below — superseded by the launcher status above

The following records the previous 14-bone package and earlier integration stages. Its statements that v1 is not playable are historical, not the current launcher state.

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
