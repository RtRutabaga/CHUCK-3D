# Codex movement handoff — 2026-09-26

## September 30 — plaza detail pass

Root launcher keeps Claude's newer combined `f7deb68` package and original receipt, which includes this plaza pass. An older candidate was briefly promoted before concurrent changes were detected; the combined package was restored and launcher check passed. Plaza-only package retained at `Builds/Windows-PlazaDetail-Verified-d38d505`. Next part of the work can be done here, coordinated with Claude.

Runtime `d38d505` / `8422cd4` changes only DockPlaza.cpp: fountain coping/thinner animated jets, shop gables/hoods/door hardware, anchor banners, gate/sewer detail and approach signage. New details are noncolliding; no character/controller, existing obstacle or binary asset changes. Final build `Local/plaza-detail-build-final.log` succeeded. Five packaged plaza views reviewed; verifier passed 121 gameplay + 15 world + 20 plaza checks and music (`Local/verify-package-20260930-140609.log`). No new motion-capture or physical-controller validation. Visual quality remains provisional. User reports Claude is also running a task; avoid overlapping heavy tools or overwriting its work.

## September 30 — additive original-map adaptation

Worked on root main from `dd02680`, retaining Claude's worker NPC and all gameplay through `9b5b46e`. User authorized fountain plaza despite historical exclusion. Runtime `9e527bf` adds `DockPlaza.cpp/.h`, one construction call and dawn lighting; original setting geometry and character code unchanged. Added connected plaza, fountain, shop shells, market, walls and two closed gates. Three new material assets (18 KB) plus isolated generator; no original asset/code import or character regeneration. Text-map reference copies and hashes in References/PROVENANCE.md.

Packaged build `Local/plaza-build.log` succeeded. Verifier passed 121 gameplay + 15 existing world + 20 new plaza checks and music-loop check (`Local/verify-package-20260930-135125.log`). Five plaza/spawn captures reviewed; no new physical-controller or motion-capture claims. Normal launcher updated with receipt; old package kept at `Builds/Windows-Previous-20260930-Plaza`. Remaining limits: prototype VFX/materials, no shop/gate interactions, arbitrary wall escape climbs not comprehensively tested. Details and route in WATERDEEP-PLAZA.md. Next setting work can be done here.

## September 29 — user soundtrack

Worked directly on main per v3 workflow. `18e6144` copies/imports the supplied WAV and starts looping 2D music at 45% with a 1.5-second fade-in. Import helper needs `-AllowCommandletAudio`; source provenance and exact format are in `SourceAssets/Audio/README.md`. No generators or character assets changed. WAV and SoundWave add roughly 49 MB of LFS data; integrity passed.

Packaged verifier: 99 gameplay checks, 15 floor/clearance checks and loop-boundary playback check passed (`Local/verify-package-20260929-205701.log`); build `Local/music-build.log`. Music smoke test starts near the end; normal game starts at zero. User listening feedback is still needed for loudness/loop transition. Normal launcher package updated with receipt; prior build retained as `Builds/Windows-Previous-20260929-Music`. Next work can be done here.

## September 29 — near-field detail follow-up

Continued from `046a35c`; Claude branches still at `03344ac`. Runtime `42f33d7` changes only setting construction: existing crate/rope meshes, platform boards, warehouse door joinery/windows/net frame, tavern rear articulation and drain grates. Collision proxy sizes and all Claude traversal geometry are preserved. No binary changes or asset generation.

Build `Local/dock-detail-build.log` succeeded. Four packaged setting captures reviewed. Rendered verifier passed 99/99 plus 15 ground/clearance checks (`Local/verify-package-20260929-133159.log`); no material fallback failures. No new motion or hardware-controller validation. Normal launcher updated with receipt; skyline package retained as `Builds/Windows-Previous-20260929-Detail`. Main remaining setting defects: repeated forms and basic roof/material treatment. Next setting work can be done here.

## September 29 — skyline follow-up

Continued from `26df615`; no newer Claude branch commits were present. `6aa739b` changes only `DockSetting.cpp`: stepped nonplayable city ground, uphill and opposite-bank buildings, civic hall/towers, and street/workshop facade detail. All added geometry has no collision; existing parkour and character code are untouched. No binary changes or new dependencies.

Windows package built (`Local/skyline-build.log`), four setting views inspected, and rendered `Verify-Package.ps1` passed 99/99 plus 15 floor/clearance checks (`Local/verify-package-20260929-131527.log`). Did not rerun motion capture or test physical controller hardware. Promoted candidate to normal launcher with receipt, retaining the previous package under `Builds/Windows-Previous-20260929-Skyline`. Repetition/material quality and distant boundaries remain unfinished. Next setting work can be done here.

## September 29 — connected docks setting

Picked up clean main `03344ac` after reviewing Claude's latest integrated `c2f26c3` traversal/camera work. User authorized world expansion, superseding the character-first world pause. Worked on `codex/docks-setting` in the existing Codex worktree, integrated by fast-forward. Claude's worktrees, character assets and controller were untouched.

Source: `16d38ca` setting construction and route checks; `4d011c6` docs/material configuration/verifier; `09eaa9b` current scope rules; `3d917dc` seven world material usage flags; `ab885d8` reviewed facade/sign corrections. `DockGameMode.cpp` only adds the setting include and construction call. No existing obstacle dimensions changed.

Final Windows package passed 99 gameplay checks with motion capture plus 15 new floor/clearance checks (`Local/verify-package-20260929-124955.log`). Build log: `Local/setting-build-final.log`; visual captures: package `Saved/Screenshots/Windows/Setting/View0..3.png`. The normal launcher package is updated with a verified receipt; old package is retained. No generated output tracked; LFS fsck passed. Material usage flags were necessary to avoid default-material substitution on instanced geometry; `Tools/configure_setting_materials.py` preserves existing graphs.

Keep refinement within the same docks map. New roof traversal and camera comfort need player feedback; buildings are shells and background still repeats. Details/routes are in `DOCKS-SETTING.md`. Character and parkour improvements remain Claude's part. Next setting work can be done here.

## September 27 — launcher repair and independent verification

Reviewed Claude's delivery through `00e46d6` and the v2 ownership split. Preserved interrupted main edits in stash `e3164da437321014069a0c5bb31e14a3b1431c7a`, then fast-forwarded main. Claude's worktrees were not modified.

The user was seeing the old character because main's launcher still ran the September 26 13:03 package; Claude's current package existed only in its worktree. Copied that package into a candidate, independently passed 49/49 checks with motion capture (`Local/verify-package-20260927-093257.log`), visually checked the front capture, and promoted it to main's `Builds/Windows`. Old package retained in `Builds/Windows-Previous-20260926`.

Tooling changes: verifier accepts a candidate package path; launcher checks/displays a build receipt; normal packaging verifies and stamps the package. No character systems or world design changed. Earlier pending-runtime notes below are historical. Next character/design work belongs to Claude; narrow launcher/tooling verification can be done here.

## Rig proposal response

Latest pickup: Claude delivered through `c5d6d25` while this review was active. Its source is now integrated, including the legacy-art dependencies; v1 FBXs/textures/clips have been imported beside the old character. Source QA passes 66 checks; Unreal rest-pose and raw animation checks pass, and import tooling restores missing loop endpoint intervals. See `docs/CHUCK-V1-INTEGRATION.md`. The next runtime task has real assets available now; do not wait for another Claude delivery or regenerate its source. Earlier pending-source notes below are historical.

Claude's proposal `51ff219` is accepted for the first v1 rig/skin delivery under `docs/RIG-CONTRACT-V1.md`. That document answers its five questions and defines exports, clip ownership, foot/sole metadata, cigarette tip handling and the atomic runtime migration. `Tools/Check-RigContract.py` validates the accepted 41-bone source table and derives runtime-space review metadata; it does not validate a future FBX or create an AnimBP. Claude owns new geometry/rig/Blender clips; Codex owns native animation data, AnimBP/contact logic, imports and packaged verification. No new gameplay actions or plugin installations are needed for this agreement.

The proposal was initially recorded separately from the preceding legacy-rig art passes (`875f579`, `dc52106`). Those source dependencies are now integrated with v1, but the current packaged model must not be described as including them. Do not change Claude's checkout automatically. The editor review helper explicitly refreshes the applied poses; walking and landing renders have been visually checked. Runtime migration remains the next task.

## Previous completed movement delivery

Owner: Codex, `codex/movement-foundation`, `Local/AgentWorktrees/codex-movement`. Base: integrated Claude source at `bc7d711`. Baseline telemetry: `d9fce07`; stance/swing implementation: `3a35bf7`. Runtime changes stay in ChuckCharacter and character checks/captures in DockGameMode. Main integration separately owns imports, shared verifier and publication.

No bone, rest-pose, scale, material, collision, input or camera contract changes. No world edits. Claude's original deliveries `9e337c3` and `939580a` were imported from their committed FBXs, without regenerating Blender source. Integrated equivalents: `483bbaf` and `bc7d711`.

Walking now alternates world-locked stance and predictive swing targets, traces ground support, adjusts feet after stopping/turning, and clears contact state on jump/reset. Short initial steps avoid stretching the stationary leg at startup. The ankle offset follows each foot's retained heading. Body lean, roll and arm swing are reduced; the old sinusoidal foot translation is gone. This remains procedural animation on a temporary rig, not finished natural locomotion.

Subsequent motion review found overextension: stride corrections `5943883`/`e5fb31e`, bounded pelvis lowering `c38284b`, and sharp-turn contact release `723518f` address it (integration equivalents may have different hashes). A contact that becomes unreachable during a sharp turn is released, its ankle is constrained to leg reach, and its next support step is prioritized. This recovery is provisional; fast turns can still show a corrective shuffle. Two intermediate reach checks failed (0.6195 and 0.7253 cm excess); the threshold was not loosened. The pelvis-corrected package passed all 43 checks with 51 steady stance samples, zero measured slip and zero reach excess (`Local/verify-package-20260926-125719.log`). Final turn-recovery verification is recorded in HANDOFF.md.

Baseline packaged measurement at steady 95 cm/s walking: 43 flat-height foot samples, mean world slip **70.5968 cm/s**, maximum **100.0720 cm/s** (`Local/verify-package-20260926-124027.log`). The probe measures foot-component origins with unchanged height, excludes acceleration, and is specific to the flat test route. New regression requires at least ten samples and maximum slip below 1 cm/s. Landing checks sole clearance against an independent ground trace instead of an old actor-relative constant. The game-mode probe runs after character visual updates.

Motion reproduction after packaging: `powershell -NoProfile -File Tools/Verify-Package.ps1 -MotionCapture`. This runs the rendered checks, then records rear/front/side rat-height and elevated image sequences covering start, walk, turn, stop, jump and landing. Images go to ignored `Builds/Windows/Chuck3D/Saved/Screenshots/Windows/Motion/View0..3`. Tests are scripted; physical Xbox hardware is not verified.

Known limits: separate static feet, no toe roll, fixed knee bend plane, rigid segmented legs and limited pelvis motion. Traces support the existing small static route, not a general slope/stair/moving-platform solution. When no suitable support exists, a neutral visual target is used; this does not grant collision support. Jacket front continuity improves, but shoulder overlap/stretching and simplified anatomy remain Claude's next art concerns. Do not certify reference-quality art or animation from passing tests.

## Next coordinated rig milestone (proposal only)

Agree the centimetre rest pose, joint axes, names and socket transforms before changing assets. Add pelvis/spine/neck, proper deforming shoulder/elbow/wrist and hip/knee/ankle/toe chains with continuous weighted topology. Use authored idle/walk/start/stop/turn/jump clips in an Animation Blueprint, then limited terrain contact correction and pelvis adjustment. Keep capsule-driven locomotion initially; evaluate root motion explicitly for roll/climb rather than mixing conventions accidentally. Plan run and lateral takeoff/landing clips, hand contacts for climbing, and a mouth/head cigarette socket with restrained jaw/lip animation. No such actions or replacement rig are implemented by this pass.

Final verification and publication results are recorded in the main `docs/HANDOFF.md`; no heavy tools should be started by the next art session until the current integration run has ended.
