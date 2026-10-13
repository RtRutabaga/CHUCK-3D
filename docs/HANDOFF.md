# Handoff — CHUCK 3D full game

Started 2026-10-12, when the Waterdeep prototype was declared done. Newest entries first. Keep entries short: what changed, what actually ran, the evidence, the remaining flaws, launcher status and the next-owner line. Per-agent detail goes in `agent-handoffs/`. The prototype log (2026-09-24 to 2026-10-12) is [archive/HANDOFF-PROTOTYPE.md](archive/HANDOFF-PROTOTYPE.md).

## Current state

- **Scope:** [PROJECT-BRIEF.md](PROJECT-BRIEF.md). Waterdeep is done ([regions/WATERDEEP.md](regions/WATERDEEP.md)). The Chult plan is awaiting the user's review ([regions/CHULT.md](regions/CHULT.md)).
- **Playing and building:** [PLAYING.md](PLAYING.md). The launcher is `Launch-Chuck.cmd`.
- **Open user decisions:** saving, the 3D form of cutscenes, combat depth, and the duration and lifecycle of the golden cigarettes. See the brief.
- **Next work:** user review of the Chult plan. Then Claude does the Phase 3 fall-to-Chult transition and Codex the first jungle setting milestone.

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
