# Handoff — CHUCK 3D full game

Started 2026-10-12, when the Waterdeep prototype was declared done. Newest entries first. Keep entries short: what changed, what actually ran, the evidence, the remaining flaws, launcher status and the next-owner line. Per-agent detail goes in `agent-handoffs/`. The prototype log (2026-09-24 to 2026-10-12) is [archive/HANDOFF-PROTOTYPE.md](archive/HANDOFF-PROTOTYPE.md).

## Current state

- **Scope:** [PROJECT-BRIEF.md](PROJECT-BRIEF.md). Waterdeep is done ([regions/WATERDEEP.md](regions/WATERDEEP.md)). The user authorized Chult's first landing setting pass; later milestone detail remains proposed ([regions/CHULT.md](regions/CHULT.md)).
- **Playing and building:** [PLAYING.md](PLAYING.md). The launcher is `Launch-Chuck.cmd`.
- **Open user decisions:** saving, combat depth, and the duration and lifecycle of the golden cigarettes. See the brief. Cutscenes are decided: 2D beats, authored in 3D, on the user's music.
- **Next work:** refine Chult's natural tree/leaf/rock forms and ground relief; extend the jungle with brachiation farther in under the region plan. Claude can address the inherited alchemist pose/check separately.

## October 13 — Chult hanging moss and wet-forest look (Codex)

- **Built:** shared `ChultDressing.cpp` gives the cinematic floor and playable landing identical seeded low branches/hanging moss, nine shallow puddles, 27 mossy colliding rocks, 108 embedded gravel pieces and a broad-leaf foreground plant. Moss, foliage, low decorative branches, gravel and puddles are noncolliding. Landing transform and the walking loop remain intact. Added dressing: **54,444 triangles**.
- **Materials:** subdued olive foliage with shader-sampled vein relief, fine mottling and varied wet roughness; patchy moss on bark/rocks; damp leaf litter and moss on the ground; soft-edged reflective water. Neutralized the earlier colour gain and warmed/corrected the sun/fog balance. Narrow `create_chult_wet_materials.py` owns three new graphs plus leaf/bark/ground updates; no meshes, source images or Blender assets regenerated. The old wind updater delegates to this current finish. Two reference JPGs preserved under `References/ArtDirection/Chult/Moss-WetForest-*`.
- **Tested:** UE **5.7.4**, MSVC **14.44.35229**, SDK **10.0.26100.0**; single-worker build/cook succeeded (`Local/chult-wet-final-build.log`, cook/package **156.13 s**). Editor and cooked Chult: **127** floor samples, **12** real-capsule route sweeps, **5** root checks, local recovery and dressing/material inventory passed. Reviewed four cooked Chult captures, including close leaf detail. Full suite rerun **182/0** (`verify-package-20261013-092019.log`, console `chult-wet-verification-rerun.log`); real Chult menu passed (`chult-wet-menu.log`). Full cutscene/skip, pantry, leap and lamp-swing checks passed. First full smoke run was **181/1** (`verify-package-20261013-075621.log`): unchanged alchemist wrist-gap snapshot measured **8.3 cm** against **<8 cm**. His idle phase is randomized; prior verified baseline measured **6.4 cm**, rerun **4.4 cm**. No NPC code/assets or threshold changed; this remains an intermittent check, not a resolved pose issue. Cutscene floor averaged **57.8 fps** at 720p, worst frame **139.8 ms**, in the first full-cutscene run; this is not a gameplay FPS profile.
- **Limits:** this remains a first look pass with flat terrain, repeated provisional trees/cards and simple boulder forms. Hanging moss and ground moss are procedural; puddles are thin surface dressing over the solid floor. No new traversal/encounters, ambience or onward route. Physical Xbox and a manual whole-loop playtest remain untested. Alchemist snapshot check needs an animation/test follow-up, independent of Chult.
- **Storage/launcher:** eight small JPG/material assets total **788,114 bytes / 0.75 MiB**, existing LFS rules/remote retained; postcommit LFS fsck passed. Promoted **e5603eb** with `Promote-Build.ps1 -Label ChultWetForest` (`Local/chult-wet-promotion.log`); receipt/hash and launcher CheckOnly passed. Previous package retained as `Windows-Previous-20261013-ChultWetForest`, two backups kept, older ChultLanding backup pruned; no candidate remains. Play through the fall or use **Dev Checkpoints → Chult Landing** in the root launcher.

Next part of the work can be done here. Claude can address the inherited alchemist pose/check separately.

## October 12 — playable Chult handoff, rigid bark and root collision (Codex)

- **Built (`c527d68`):** full fall playback and Escape-skip enter playable Chult instead of the demo card. Restore collision, movement/input and follow camera; preserve sanity/stamina/cigarettes, set local respawn, destroy the completed stage/camera. Leaves/canopy use one quarter of their former wind amplitude; bark has zero displacement. Added solid buttress/base collision from original seeded profiles and understory palm trunks. No meshes/Blender sources regenerated; the narrow material updater changes four materials without importing meshes/audio.
- **Art:** darker/greener bark and damp ground, subdued exposure/fill and cooler atmospheric depth begin adapting the four user reference JPGs saved under `References/ArtDirection/Chult`. Art direction, landing contract and brief updated. Repeated procedural tree forms and flat terrain remain below the target; this is an initial palette/lighting pass.
- **Tested:** UE **5.7.4**, VS **14.44.35229**, SDK **10.0.26100.0**, single-worker cooked package (`Local/chult-handoff-final-build.log`, UAT **62.29 s**). Full suite **182 passes / 0 failures**, `Local/verify-package-20261012-221535.log`; Chult **127 floor samples / 12 capsule route sweeps / five outer-root ray-and-real-capsule checks**, local respawn; real menu **8/8**, `Local/chult-handoff-menu.log`. Full playback and skip both report playable landing; skip preserves damaged sanity and tests actual simulated movement/local reset. Reviewed final Chult and post-cutscene captures. No physical Xbox or manual whole-loop playtest. First compile's shadowed variable corrected; an earlier superseded smoke run was deliberately stopped at 175 passes for the final art pass and is not acceptance evidence.
- **Storage:** four material assets and four reference JPGs total about **0.47 MiB** in the existing GitHub LFS endpoint; `git lfs fsck` passed. No generated engine/build output committed, installations or new dependencies.
- **Launcher:** promoted through `Promote-Build.ps1 -Label ChultHandoff`, `Local/chult-handoff-promotion.log`. Root `Builds/Windows` is **c527d68**; receipt/hash and CheckOnly pass. Prior package retained as `Windows-Previous-20261012-ChultHandoff`, two backups kept; empty candidate folder removed. Landing still has no onward route, ambience, brachiation, encounters or golden pickups; these remain later region work.

Next part of the work can be done here.

Publication completed October 13 after the user enabled full access and instructed Codex to proceed. Pushed the Chult fixes and handoff commits to `origin/main`; all eight LFS objects uploaded successfully (**496 KB**). The earlier automatic approval block is resolved. Launcher runtime remains **c527d68**; no rebuild or gameplay tests were repeated for publication.

## October 12 — Chult landing preview (Codex)

- **Built:** `ChultLanding.h/.cpp` reuses the fall-set art and exact near tree/palm placements, floor origin and material. A winding ground loop runs about 46 m into the jungle and returns to the clearing. No landing grips; brachiation moves farther in under the user's new instruction. `Dev Checkpoints → Chult Landing` enters this setting with local respawn. Production fall-to-control handoff remains Claude's work; the ordinary pantry fall still ends on the demo card.
- **Tested:** editor geometry/captures; corrected packaged Chult run `Local/verify-chult-20261012-213439.log`: assets present, 127 floor samples, 12 real-capsule segment sweeps, local recovery, clean exit. Low/elevated landing and farther-jungle captures reviewed. Full package suite **182 passes / 0 failures** (`verify-package-20261012-213439.log`); real menu **8/8** (`verify-menu-20261012-214404-*.log`), including Chult's actual input, floor, possession and local recovery point.
- **Tooling:** `Build-Game.ps1 -SkipBaseAssets` preserves existing binary assets, disables stale UBT makefile environment restoration and avoids recompiling the editor during packaging. Standard corrected build/package/verification command succeeded (`Local/chult-final-build.log`). The first packaged Chult test crashed during timer cleanup; its executing lambda no longer deletes itself, and the corrected run exits cleanly.
- **Limits:** flat ground, procedural/repetitive foliage, simple trunk/timber collision; visual buttress roots/lianas and the original scenic log have no collision. No outer boundary, onward transition, Chult ambience/music, grips, encounters or pickups yet. No physical Xbox or complete walking-loop playtest.
- **Launcher:** promoted with `Promote-Build.ps1 -Label ChultLanding` (`Local/chult-promotion.log`). `Builds/Windows` is **fb51039**, and launcher `-CheckOnly` passed. Select **Dev Checkpoints → Chult Landing**. The previous package is retained as `Windows-Previous-20261012-ChultLanding`; two backups kept, Rename pruned, no candidate folder left. The first test's crash evidence moved to ignored `Local/chult-first-test-crash` before promotion. No binary assets changed.

Next part of the work could be done by either — preference: Claude connects the cutscene to the landing under CHULT-LANDING.md; Codex then builds brachiation farther into Chult.

## October 12 — the fall-to-Chult cutscene (Claude)

- **Built:** falling through the cellar's sky hole plays a 47.5 s cutscene on the user's "Fall Cutscene theme" instead of the instant end card:
  - It runs from open sky over a volumetric cloud deck, through the deck, out over the jungle, a crash down through the canopy, his astral death and return on the jungle floor, a look round and a drag, to a pull-back.
  - The music is cut at the end of its second section, fading over the last two bars (43.8–47.5 s), and the shots cut on the bars.
  - Letterboxed, no control; Esc skips. It still ends on the "End of demo" card.
  - Code is in `FallCutscene.h/.cpp`, the set in `SourceAssets/Cutscenes/FallToChult` (procedural), and the audio in `SourceAssets/Audio/README.md`. Details are in `agent-handoffs/CLAUDE.md`.
- **Tested:**
  - Packaged candidate: `Verify-Package` **182 passes** plus the new skip and full-cutscene checks (`Local/verify-package-20261012-203529.log`, `verify-demo-end-*`, `verify-fall-cutscene-*`).
  - About 58 fps at 720p in every part of the cutscene.
  - Cooked captures reviewed.
- **Changed shared tooling:** `Tools/Verify-Package.ps1` now runs `-ChuckFallCutsceneTest` and checks the skip. `-ChuckDemoEndTest` expects the cutscene.
- **Gotcha:** a stale editor `Makefile.bin` made editor builds compile the old OneDrive copy (deleted; see the Claude handoff).
- **Flaws:** the procedural jungle is flat and bright up close; the distant trunks are repetitive; there are no visible light shafts; the haze under the deck hides the jungle; the fall uses the jump loop pose. Not tested: Xbox, speakers, other resolutions.
- **Launcher:** promoted with `Promote-Build.ps1 -Label FallCutscene`. `Builds/Windows` is **c6f2631** and `Launch-Chuck.ps1 -CheckOnly` passes. The previous build is kept as `Windows-Previous-20261012-FallCutscene` (plus `-Rename`), and `-Flow` was pruned. No candidate folder is left.

Next part of the work can only be done by Codex: the Chult landing setting, after the user approves regions/CHULT.md. Claude then connects the cutscene to it.

## October 12 — prototype cleanup: docs archived, "Prototype" renamed, stale assets resolved (Claude)

- **Assets (`e06c6bc`):** the 38 long-unstaged Human/Zombie/base-material uassets were later re-imports, and every launcher build since October 4 was cooked from them, so they were committed. The 2 untracked, unreferenced MakeHuman PNGs moved to the ignored `Local/UnusedSourceTextures/`.
- **Codex's Chult plan:** committed for it as `4ada88c`.
- **Renames:**
  - The launcher is `Launch-Chuck.cmd` (with `Tools/Launch-Chuck.ps1`), and the scripts are `Build-Game.ps1`, `Test-Game.ps1`, `Write-BuildReceipt.ps1` and `create_base_assets.py`. The receipt file is `chuck-build.json`.
  - `Build-Game.ps1 -ArchiveDirectory <dir>` now packages and verifies a candidate instead of overwriting `Builds/Windows`.
- **Unreal content:**
  - The base materials moved from `/Game/Prototype/Materials` to `/Game/Common/Materials` through an editor rename (`Local/migrate_prototype_content.py`, log `Local/migrate-prototype-content.log`), which also re-saved the referencing legacy `SK_/SM_ChuckBody` and `SM_ChuckFoot`.
  - The editor's rename of the map was refused by a confirmation dialog. The map was a never-edited blank level from the generator, so it was deleted and regenerated at `/Game/Maps/WaterdeepDocks` by the same generator.
  - C++ material paths (4 files), the ini files and the import scripts were updated.
- **Docs:** prototype docs, the old HANDOFF, both agent logs, agent-tasks and the old workflow history moved to `docs/archive/`, which has an index README. New `PLAYING.md` and `regions/WATERDEEP.md`. README, AGENTS, CLAUDE and AGENT-WORKFLOW updated; the next-owner and region-plan rules are now in the v3 workflow.
- **Tested:**
  - `Build-Game.ps1 -Package -ArchiveDirectory Builds/RenameCandidate` succeeded (`Local/rename-build.log`).
  - `Verify-Package` gave **182 passes, 0 failures**, identical to the previous launcher build's run (`Local/verify-package-20261012-174110.log` vs `-102556.log`).
  - `Verify-Menu` passed **7/7** destinations (`Local/verify-menu-20261012-174835-*.log`).
  - The `Harbor_Elevated` capture is identical to the previous build's.
- **Launcher:** promoted with `Promote-Build.ps1 -Label Rename`. `Builds/Windows` is now **a0ea6c6**, with the receipt `chuck-build.json`, and `Launch-Chuck.ps1 -CheckOnly` passes. The previous build is kept as `Windows-Previous-20261012-Rename`, and `KeeperRag` was pruned. The launcher opened the game once during checking (I closed it).
- **Not tested:** physical Xbox play. The legacy `SK_ChuckBody` meshes are no longer used by the game; they were only re-saved.

Next part of the work could be done by either — preference: the user reviews regions/CHULT.md first, then Claude does the fall to Chult.
