# Agent handoff — Claude Code

Per-session log, newest first. Template: [TEMPLATE.md](TEMPLATE.md). Prototype-era entries (character, rig, traversal, camera, sewer, NPCs) are in [../archive/agent-handoffs/CLAUDE-PROTOTYPE.md](../archive/agent-handoffs/CLAUDE-PROTOTYPE.md).

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
