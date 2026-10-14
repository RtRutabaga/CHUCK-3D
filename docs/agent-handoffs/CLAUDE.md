# Agent handoff — Claude Code

Per-session log, newest first. Template: [TEMPLATE.md](TEMPLATE.md). Prototype-era entries (character, rig, traversal, camera, sewer, NPCs) are in [../archive/agent-handoffs/CLAUDE-PROTOTYPE.md](../archive/agent-handoffs/CLAUDE-PROTOTYPE.md).

## October 14 — respawn on the ruin top after dying near the trees (Claude)

- **Request:** if Chuck dies in the area around the brachiation trees, his spawn point is the top of the ruins where the trees are reached from. Base `d5d66f8`.
- **Changed:**
  - `ChuckCharacter.cpp`: `GetAreaStartLocation()` in Chult returns `ChultCanopyRespawn()` when he dies inside `InChultCanopyDeathArea()`, and the landing entrance otherwise. `AreaStartYaw` turns him to face tree 0 there. This covers fall and astral deaths, which both read the area start at the moment of death.
  - `ChultCanopy.h/.cpp`: the death area is the canopy reserve (the loop's rectangle and the line's corridor) plus 5 m. The spot is Codex's landing slab top at (5600,885,670), a checked standing top. A static check asserts it is stone with room to stand.
  - Three death trials were added to `-ChuckCanopyTest` (48 in all). `Verify-Package` is updated. No rig, animation or tuning change.
- **Tests (uncooked):**
  - `Local/canopy-respawn-1.log`: dying on a line tree and on the ground under the line both return him to (5600,885,704.65) facing tree 0, standing; dying on the landing loop returns him to the entrance. 48/48 pass, 0 static failures.
  - `Local/chult-respawn.log`: Codex's landing respawn check still passes (entrance=1).
  - Cooked: full suite **182/182** on the first run (`Local/verify-package-20261014-085721.log`); canopy 48/48 including the three deaths (`verify-canopy-20261014-090308.log`); Chult respawn and ruins checks pass.
- **Notes:** nothing in Chult can kill him yet except an empty sanity bar or leaving the floor; the trials kill him with a bite. Dying on the ruins' own north end (within 5 m of the reserve) also returns to the slab, since it is part of the margin.
- **Launcher:** promoted as `CanopyRespawn` (`Local/respawn-promotion.log`); older backups pruned to two, no candidate left.

Next part of the work can be done here (a play review would help most).

## October 14 — a staggered line of canopy trees into the open ground (Claude)

- **Request:** more brachiation trees in a staggered line leading away into the blank part of the map. Base `15826c5`.
- **Where:** past 90 m from the landing the forest ring stops and the floor is bare out to its edge at 130 m. An oblique overview (`Review_8`) showed the empty strip running north-east from the grove past tree 2.
- **Built (`ChultCanopy.cpp/.h`):**
  - Trees 4–7 at (9050,3250), (10350,4200), (10650,5750) and (11700,6600). They alternate either side of the line, 13.5–16 m apart, with crotches at 6.9–7.6 m and varied girth and leader height.
  - Chains 4–7 (2→4→5→6→7) work like the loop's.
  - Each new tree has a liana and spurs; the end tree has only one chain limb, so it gets four spur tries and its standing point sits at that limb's base.
  - Totals: 8 trees, 8 chains, 14 spurs, 127 grips.
  - Trees take limbs from a `Neighbours` list, and chains record `LimbA/LimbB`. The loop is unchanged: same limb order and random draws. Tree 2 gains a limb, so its spurs moved.
  - `InChultCanopyReserve` covers a 10 m corridor along the line (the landing keeps its random placements out of it). Spur grips stay within 132 m of the landing.
- **Test changes:** `-ChuckCanopyTest` adds 13 line trials: reset at tree 2's crotch, then catch, chain and step down for chains 4–7, then the end tree's liana down. Spur trials cover the new spurs, for 45 trials in all. The capture's final still no longer switches the camera in the frame it renders (it had been capturing Chuck's view). `Verify-Package` counts are updated.
- **Tests:** uncooked canopy 45/45 with 0 static failures (`Local/canopy-line-2.log`). Uncooked Chult: grove ground 11/11, root check, ruins 5/5, landing counts unchanged (`Local/chult-line.log`). Cooked: full suite **182/182** on the first run (`Local/verify-package-20261014-075851.log`); canopy 45/45 with 0 static failures (`verify-canopy-20261014-080440.log`); Chult grove ground 11/11 and ruins 5/5; fall cutscene and lamp swing 8/8 pass.
- **Reviewed:** the overview and a view along the line, kept as `SourceAssets/Setting/Review/Canopy_LineOverview.png` and `Canopy_LineView.png`.
- **Known flaws:**
  - The ground under the line is bare: no understory, roots or forest beyond the trees themselves.
  - The canopy now has about 232k procedural triangles and 15.9k leaves; frame rate not measured.
  - No crown fading, no hands-on play, no Xbox test.
- **Launcher:** promoted **6bfa5a6** as `CanopyLine` (`Local/line-promotion.log`; receipt and launcher check pass); older backups pruned to two, no candidate left.

Next part of the work can be done here (a play review would help most); Codex could dress the open ground along the line (understory, relief) as setting work.

## October 13 — canopy spurs in all directions; smooth buttresses on every tree (Claude)

- **Request:** branches out in all directions like real trees, all brachiatable the same way, with natural variety in size and position; and fix the sharp angles where root structures go up trunks, on these trees, all trees and in the cutscene. Base `5d2b753`.
- **Canopy (`ChultCanopy.cpp/.h`):**
  - Nine **spurs**: side walking limbs in every free direction, each with its own length, girth, pitch, bend and droop. Each carries a hand bough with 3–5 registered grips (33 new; 71 in all). They are used like a chain: walk out, jump, swing to the tip, come back, step down.
  - Placement rules keep lines from stealing each other's grips (at least 52° between limbs, first grips more than 2.4 m apart) and keep walking room in the crotch.
  - Per-tree girth and leader height; a main leader plus a second branching above head height; crown boughs spread all round (golden angle); per-limb lengths.
  - `FChultCanopyTree` gains `TrunkScale` and per-limb `LimbLength/LimbRise/LimbScale/LimbAway`; new `FChultCanopySpur`/`ChultCanopySpurs()`.
  - Lianas moved 30 cm further out (s=240) to clear the thicker trunks.
- **Buttresses, everywhere:**
  - The old fins were separate planks with a hard 90° join and a spike up the trunk. They are now lobes of the trunk's own surface: Gaussian bumps in its radius, broad and filleted at the join, a thin rounded plank further out, fading tangentially into the bark at their top. The C++ is `ButtressedTrunk`; the Blender equivalent is `buttressed_trunk` in `Tools/build_fall_jungle.py`.
  - `SM_FallTree_A/B` were regenerated with `-- --only` (new), keeping their random draws, so branches, crowns, lianas and collision profiles are unchanged. They were reimported with the new narrow `Tools/import_fall_trees.py`; no material, texture or other mesh changed, so Codex's wet finish is intact.
  - Triangle count is 9,976 for A (was 7,608) and 12,134 for B (was 9,716).
  - The landing's hidden root collision (Codex's `ChultLanding.cpp`) and its root check now follow `ChultRoots::FinEdge`, which `export_chult_root_profiles.py` writes into `ChultRootProfiles.h`; the profile values are unchanged.
  - These trees fill the landing forest and the cutscene's clearing.
- **Bug found on the way:** `sin(π)` is a hair below zero, so a crest vertex became NaN. The poisoned collision made phantom ground-lane blocks 7 m from any trunk. Clamped, and the canopy check now logs the hit component and point.
- **Tests:** uncooked `-ChuckCanopyTest` 27/27 (`Local/canopy-lobes-2.log`), static check 0 failures (71 grips, 58 leaps, 142 limb samples). `-ChuckChultTest` passes: grove ground 11/11, root check 5/5, ruins 12 tops and 5/5 trials. `-ChuckFallCutsceneTest` passes (`Local/fall-cutscene-roots.log`), and its pull-back shot shows the smooth buttresses. Cooked results: full suite **182/182** on the first run (`Local/verify-package-20261013-221047.log`); canopy 27/27 with 0 static failures (`verify-canopy-20261013-221522.log`); Chult grove ground 11/11, root check 5/5 and ruins 5/5 (`verify-chult-20261013-221047.log`); fall cutscene and lamp swing 8/8 pass.
- **Reviewed:** overview, tree profile, landing tree base and canopy tree base, player view, cutscene pull-back. Four stills replaced or added in `SourceAssets/Setting/Review/` (`Canopy_Overview`, `Canopy_RootsLanding`, `Canopy_RootsCanopy`, `Canopy_SpurWalk`).
- **Known flaws:** procedural trees below the art target; limbs seen side-on can read as a "T"; lobe UVs stretch a little on the flattest ground runs; no crown fading; a person has not played it. No Xbox test. The extra trunk triangles (about 30% per landing tree) were not profiled for frame rate.
- **Launcher:** promoted **b96c37a** as `CanopySpurs` (`Local/spurs-promotion.log`; receipt and launcher check pass); previous build kept as `Windows-Previous-20261013-CanopySpurs`, older backups pruned to two, no candidate left.

Next part of the work can be done here (a play review by the user would help most); Codex can continue Chult setting art.

## October 13 — Chult canopy brachiation made to work; natural trees (Claude)

- **Owner, task, branch:** Claude, the user's "make Codex's brachiation tree section work; change the trees as needed, more natural if possible", on `main` in `C:\GameDev\CHUCK-3D`. Base `5ab3919`.
- **What was wrong:** Codex's grove had 36 inactive paw proposals (no grips registered), first grips 115 cm above the forks (a standing hop reaches about 112 cm, so they could not be caught from the forks), paired twin trunks with flat chimney strips (the region plan says not to copy the docks' chimney entry), and at a chain's end a jump with the stick released swung back along the chain.
- **Built (`ChultCanopy.cpp/.h` rewritten):**
  - Four single-trunk canopy giants: fluted flared trunks, plank buttresses and surface roots kept off the ground lanes, a crotch bowl and saddle, a walking limb toward each neighbour (tree 0 also under the ruin slab, which now rests on it), two leaders into a leafy crown, and hand boughs from a leader out over each limb toward the gap. About 83.5k triangles, 7,155 leaf instances.
  - 38 registered grips in four chains, end grips 98 cm over the limbs, 122–129 cm spacing and a 180 cm branch gap.
  - A climbable liana on every tree (`AddChuckClimbable`).
  - Layout and contract: [CHULT-CANOPY.md](../CHULT-CANOPY.md), rewritten.
- **Character (`ChuckCharacter.cpp`, `SwingJump`):** at a line's end with the stick released, if there is something to stand on within 170 cm below just ahead, he hops down onto it (`CHUCK_SWING_STEP_DOWN`); otherwise the old reverse. No rig, clip, control-mapping or tuning change.
- **Tests:** `CheckChultCanopy` (static: grips, all 34 leap sweeps, end-grip bark, 72 limb samples, forks, lianas) runs in `-ChuckChultTest`; new `-ChuckCanopyTest` (18 real-character trials) and `-ChuckCanopyCapture`; `DockGameMode` hook; `ChultLanding.cpp` capture phases moved onto the new fork/gap; `Verify-Package` patterns updated and the canopy test added.
- **Outside my usual ownership:** Codex's `ChultCanopy.cpp` setting (rewritten at the user's request), two capture phases in `ChultLanding.cpp`, and `Tools/Verify-Package.ps1`. Ruins code untouched.
- **Commands run:** `Build.bat Chuck3DEditor` (about 8 times); uncooked `-game -ChuckCanopyTest [-ChuckCanopyCapture]` 7 runs (`Local/canopy-test-1..7.log`, all passing; the shapes changed between runs); uncooked `-ChuckLampSwingTest` (8/8) and `-ChuckChultTest` (all Chult/ruins checks and 5 ruin trials pass); `Build-Game.ps1 -Package -SkipBaseAssets -ArchiveDirectory Builds/CanopyCandidate` (`Local/canopy-build.log`).
- **Cooked results:** `verify-canopy-20261013-201123.log` 18/18; `verify-chult-20261013-200710.log` canopy check 0 failures, grove ground 11/11, Chult check, ruins 5/5; `verify-lamp-swing-20261013-201038.log` 8/8. Full suite: first run 181/182 (`verify-package-20261013-200710.log`); the one failure was the known intermittent alchemist wrist-gap snapshot (8.2 cm against <8 cm, idle phase randomized; no NPC code touched). The unchanged candidate then passed **182/182** (`verify-package-20261013-201906.log`).
- **Reviewed visually:** cooked-equivalent editor captures; four kept in `SourceAssets/Setting/Review/Canopy_*.png` (overview, tree profile, player view on the first limb, swinging past the branch gap). The trees read as spreading jungle trees now, not twin pillars with stubs.
- **Known flaws:** procedural trees with the shared bark and leaf card, below the art target; hand boughs still a little straight and pole-like from some angles; the crotch from directly above shows the limb junction; hand-bough roots arch over the crotch; no crown fading, and camera comfort in the canopy not judged by a person; leaves lit from below can look pale; Xbox controller not tested. Swinging is kinematic: nothing stops a body clipping a twig.
- **Binaries:** four review PNGs, about 6.4 MB, LFS. No Unreal assets changed.
- **Launcher:** promoted **d0fa3c5** as `CanopySwing` (receipt and launcher check pass; `-MossyRuins` backup pruned; no candidate left).
- **Heavy tools running:** none at hand-off.

Next part of the work can be done here (a play review by the user would help most); Codex can continue Chult setting art around the new trees.

## October 13 — fall sounds recorded, Chult score, ground thud (Claude)

- **Built:**
  - **Fall sounds:** the user downloaded five CC0 BigSoundBank recordings (Joseph Sardin). They are kept unmodified in `SourceAssets/Audio/SFX/BigSoundBank/` with hashes in `manifest.json`. `Tools/make_fall_sounds.py` cuts four sounds from them:
    - `SFX_FallRush_00`: a seamless rush loop, which replaces the generated wind.
    - `SFX_FallFlap_00`: a loop for his jacket.
    - `SFX_FallWhistle_00`: a whistle loop.
    - `SFX_FallThud_00`: a body fall slowed and low-passed into a soft-earth thud, under a dirt impact.

    In the cutscene the air loops follow his speed and closeness (loudest in the sound-only pre-roll) and stop at the canopy. The thud lands with the impact, with a lighter copy on the bounce.
  - **Chult score:** the user's `chult.wav` is copied unmodified to `SourceAssets/Audio/Chult.wav` and imported as `SW_Chult` (looping). `DockGameMode` gives Chult its own music region: in Chult, after the fall or from the checkpoint, the Waterdeep scores fade out and Chult's fades in at 45%.
  - **Import:** the narrow `Tools/import_fall_chult_audio.py` imports only these sounds (no meshes or materials).
- **Tests adjusted:**
  - The NPC ducking check now expects 4 score tracks (it asserted 3, the only regression found).
  - The sewer score's loop check skips when the run has already left the sewer, the same guard the night score uses. The first candidate run failed on exactly that (`verify-package-20261013-180621.log`, 181/182).
  - `Verify-Package` additionally requires `flap=1 whistle=1 thud=1` and `CHUCK_CHULT_MUSIC_CHECK failures=0 looping=1 playing_after_boundary=1`.
- **Tested:** `Builds/FallSoundCandidate` passed **182** (`Local/verify-package-20261013-181829.log`), including the duck check with `tracks=4` and three clean sewer checks. The fall test ran end to end into playable Chult with `MUSIC_REGION chult=1`, and the packaged Chult test confirmed the Chult loop.
- **Mix:** measured, not listened to. The scores are about −24 dB RMS, and the pre-roll rush is −18.6 dB, falling to about −30 under music. Not heard on speakers; Xbox and other resolutions not tested.
- **Launcher:** promoted with `Promote-Build.ps1 -Label FallSound`. `Builds/Windows` is **ce62b9e**, with the receipt and launcher check passing. The previous build is kept as `Windows-Previous-20261013-FallSound` (plus `-MossyRuins`), and `-NaturalCanopy` was pruned. No candidate is left.

Next part of the work can be done here (or by Codex for Chult setting work).

## October 13 — fall cutscene follow-ups

- **Base:** `360fa4c` (after Codex's Chult work). Details are in the HANDOFF entry of the same date.
- **Changed:**
  - `FallCutscene.h/.cpp`: pre-roll (`Lead`, `Reveal`), the landing set and environment handover, the prone crash, the summon from prone, a `Flap` slot, and the sentinel fixes.
  - `ChultLanding.h/.cpp` (Codex's file): Build split into `BuildSet`/`BuildEnvironment`; `EnterPreview` completes an existing landing.
  - `DockPantry.cpp`: puffs removed; the test timers moved +5 s.
  - `Tools/create_pantry_deep_sky.py` (new) and `M_PantrySky.uasset`.
  - `Tools/Verify-Package.ps1`: score-start check.
- **Contracts:** the cutscene now depends on `AChultLanding::BuildSet()`/`BuildEnvironment()` and on the landing keeping the cutscene camera lane. No rig or socket change.
- **Commands run:** the deep-sky script three times, about 7 uncooked `-ChuckFallCutsceneTest` capture runs, and `Build-Game.ps1 -Package -SkipBaseAssets -ArchiveDirectory Builds/FallCrashCandidate`.
- **Downloads:** the user approved three BigSoundBank CC0 files. The download step uses an anti-bot token and Cloudflare check, so I did not script it. Two blocked form POSTs returned HTML only; those pages were deleted. The user will download the files by hand.

## October 12 — the fall-to-Chult cutscene

- **Owner, task, branch:** Claude, the Phase 3 fall-to-Chult cutscene on the user's "Fall Cutscene theme", on `main` at `C:\GameDev\CHUCK-3D`. Base `c589b8f`.
- **What it is:** falling through the cellar's sky hole now plays an authored 47.5 s cutscene instead of the instant "End of demo" card. It follows the 2D `falling_cutscene_scene.py` beats:
  1. The cellar view watches him drop away, and a white fade opens on open sky.
  2. A wide shot of a small rat over a sea of volumetric cloud under a teal sky.
  3. His face, from below.
  4. Looking down past him at the deck, with mist rushing up through it.
  5. Out under the deck over the jungle.
  6. Up from the canopy as he comes out of the clouds.
  7. The crash through three branches, timed to the score's breakdown, and the impact.
  8. His quiet astral death and summon on the jungle floor, during the rebuild.
  9. He looks off to his left and back.
  10. A close-up drag, with two exhales.
  11. A low pull-back to a small rat at the foot of buttressed giants, with the music fading over the last two bars of the second section (43.76–47.52 s) and the picture going black.
  12. The end card.
  - Letterboxed to about 2.2:1, no HUD and no control; Esc skips to the card, where Esc exits.
- **Music analysis:** stdlib Python found about 128 bpm and the section boundaries. The details are in `SourceAssets/Audio/README.md`, and the shot cuts sit on the bar lines (`FallCutscene.h`). The WAV is unedited; the fade is at runtime.
- **Changed paths:**
  - **New:** `Source/Chuck3D/FallCutscene.h/.cpp`; `Tools/build_fall_jungle.py`, `gen_fall_sfx.py` and `import_fall_cutscene.py`; `SourceAssets/Cutscenes/FallToChult/*`; `SourceAssets/Audio/FallToChult.wav` and `SFX/SFX_FallWind_00`, `SFX_LeafCrash_00..02` with `fall-manifest.json`; `/Game/Art/Cutscenes/FallToChult/*`, `/Game/Art/Audio/SW_FallToChult` and the four SFX assets.
  - **Edited:**
    - `ChuckCharacter.h/.cpp`: the sky-hole trigger starts the cutscene; added cutscene mode, `EndFallCutscene` and the pose puppet.
    - `DockGameMode.h/.cpp`: `FadeOutMusic`, a music-region guard and the HUD letterbox.
    - `DockPantry.cpp`: `-ChuckDemoEndTest` now expects the cutscene and Esc skip; new `-ChuckFallCutsceneTest`.
    - Docs.
  - **Outside my usual ownership:** `Tools/Verify-Package.ps1` (Codex's verification tooling) now also requires `CHUCK_DEMO_SKIP_TEST` and runs the full cutscene test.
- **Contracts:** no rig, socket or slot change. Chuck is puppeted through his existing clips: JumpLoop, Summon, Idle, TurnLeft90/TurnRight90 with the `ChuckClipData::TurnYaw` profile, `Exhale()` and `SetAstralHidden`. `AFallCutscene` is a `friend` of `AChuckCharacter`. Sanity and cigarettes are untouched. Hand-off to Chult: replace the `EndFallCutscene()` call in `AFallCutscene::Finish()` (see regions/CHULT.md).
- **Stage:** 25 km from the docks, at (4 km, 25 km, 0), placed under the world-anchored cloud deck. At the white cut every dock actor is hidden, its lights, fog, post-process and sounds switched off, and the stage is built:
  - Sky atmosphere with teal Rayleigh scattering.
  - A volumetric cloud deck at 2.0–3.1 km (`MI_FallClouds`, coverage 0.22, density 0.035).
  - A sun with cloud shadows, a real-time sky light, height fog with volumetric fog and auto exposure.
  - About 13,600 instances: 11,900 canopy crowns, 1,240 trunks, the clearing's trees and understory, and 150 mist puffs.
  - A 4,000 km far canopy plane.
- **Commands actually run:**
  - `blender --background --python Tools/build_fall_jungle.py`.
  - `python -I Tools/gen_fall_sfx.py`.
  - `UnrealEditor-Cmd ... -ExecutePythonScript=Tools/import_fall_cutscene.py -AllowCommandletAudio`, which logs `Local/fall-cutscene-import.log`.
  - `Build.bat Chuck3DEditor`.
  - About 14 uncooked `-game -ChuckFallCutsceneTest` capture runs, iterating on the look from contact sheets.
  - One `-ChuckDemoEndTest` run.
  - `Build-Game.ps1 -Package -ArchiveDirectory Builds/FallCutsceneCandidate`.
- **Gotcha found:** the editor target's `Intermediate/Build/Win64/x64/Chuck3DEditor/Development/Makefile.bin` dated from 2026-10-09 and pointed at the old OneDrive copy. My first two editor builds "succeeded" but compiled and linked `C:\Users\ashsm\OneDrive\Documents\GameDev\CHUCK-3D` instead; only its generated Binaries and Intermediate changed, and no sources were touched. Deleting the stale makefile fixed it. After a build, check the DLL timestamp.
- **Tests (uncooked, final):**
  - `CHUCK_FALL_CUTSCENE_END at=47.53 skipped=0 landed=1 vanished=1 summoned=1 exhales=2` and `CHUCK_FALL_CUTSCENE_TEST failures=0 card=1`.
  - The music plays at 10 s.
  - Frame rate at 1280×720 on the RTX 4060: about 58 fps average in sky, canopy and floor. Worst frames were 128, 99 and 109 ms; the largest hitch is the stage build under the white.
  - `-ChuckDemoEndTest`: `CHUCK_DEMO_END_TEST failures=0 sky_ends=1 ...`, `CHUCK_DEMO_SKIP_TEST failures=0 card=1` and `CHUCK_DEMO_EXIT escape=1`.
  - Evidence: `Local/fall-cutscene-test.log`, `Local/demo-end-test.log`, and captures in `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/FallCutscene/`.
- **Package:** `Build-Game.ps1 -Package -ArchiveDirectory Builds/FallCutsceneCandidate` succeeded (`Local/fall-cutscene-build.log`). `Verify-Package` gave **182 passes**, with the new checks passing: `Local/verify-demo-end-20261012-203529.log` (`CHUCK_DEMO_SKIP_TEST failures=0 card=1`) and `Local/verify-fall-cutscene-20261012-203545.log` (`CHUCK_FALL_CUTSCENE_TEST failures=0 card=1`; about 58/57/58 fps cooked). The cooked captures in `Builds/FallCutsceneCandidate/Windows/Chuck3D/Saved/Screenshots/Windows/FallCutscene/` match the editor captures, clouds and foliage included.
- **Known flaws:**
  - The jungle set is procedural cards and primitives: bright, flat green leaves; repetitive pole-like distant trunks; no visible volumetric light shafts.
  - The far canopy reads flat from altitude.
  - In the shot right under the deck (about 17.5–19 s) the jungle is mostly lost in haze.
  - The exhale is the game's puff effect, a soft white ball in close-up.
  - The fall pose is the JumpLoop clip with body rotation, not a dedicated skydive or tumble clip; there is no jacket flap.
  - Leaf vertex blue (dryness) arrives wrong after FBX import, so it is ignored.
  - The art target images have not been reached.
  - Not tested: physical Xbox play, listening on speakers, full-screen or other resolutions.
- **Binaries:** about 28 MB new, all under the LFS rules (`.wav`, `.png`, `.fbx`, `.uasset`). The 14 `Common/Materials` re-saves from `create_base_assets.py` during the build were restored rather than committed.
- **Heavy tools running:** none at hand-off.

Next part of the work can only be done by Codex: the Chult landing setting (milestone 1), after the user approves regions/CHULT.md. Claude then connects the cutscene to it.

## October 12 — full-game cleanup (steps 1–4)

- **Commits:**
  - `3fb1a1b`: full-game brief and AGENTS scope.
  - `e06c6bc`: the 38 regenerated assets.
  - `4ada88c`: Codex's Chult plan, committed on its behalf.
  - Then the rename, the docs archive and this handoff.
- **Contracts:** no rig, socket or material-slot change. Material asset paths changed from `/Game/Prototype/Materials/M_*` to `/Game/Common/Materials/M_*`, and the map is now `/Game/Maps/WaterdeepDocks`. Any new script or code must use the new paths.
- **Commands run:**
  - `UnrealEditor-Cmd -run=pythonscript Local/migrate_prototype_content.py`. The materials moved; the map rename was cancelled by a dialog in unattended mode.
  - `Build-Game.ps1 -Package -ArchiveDirectory Builds/RenameCandidate`, which runs `Verify-Package` (182/0).
  - `Verify-Menu` (7/7).
  - `Promote-Build.ps1 -Label Rename`. The first attempt failed safely because my shell's working directory was inside the candidate; the previous build was restored automatically.
- **Lesson:** don't leave a shell inside `Builds/<Candidate>` before promoting.
- **2D game access:** read with absolute paths only; one relative `mkdir` was blocked by the deny rule and nothing was written there.

Next part of the work could be done by either — preference: the user reviews regions/CHULT.md first, then Claude does the fall to Chult.
