# Starter prompt for Codex (v2 split, 2026-09-27)

Work in the `codex/movement-foundation` worktree (`Local/AgentWorktrees/codex-movement`). Refresh it from `main` first if it is clean. Read AGENTS.md, docs/AGENT-WORKFLOW.md (the **v2 split** at the top is current), docs/HANDOFF.md, docs/agent-handoffs/CLAUDE.md and docs/agent-handoffs/CODEX.md.

Under v2, Claude now owns the character end to end, including Unreal character import/groom and the v1 runtime migration (AnimInstance/AnimBP, contact/IK, switching the default character). Codex takes narrow, well-specified work that protects its usage limit:

1. **Independent verification.** When Claude hands over a branch, build/package it and run `Tools/Verify-Package.ps1 [-MotionCapture]` and the character reviews. Report concrete defects with file/frame/value evidence rather than rewriting Claude's systems.
2. **Regression tests and tooling.** Automated contact-slip, reach and ground-clearance regressions for the v1 character; keep validators and build/packaging scripts working.
3. **Data and isolated fixes.** Future 2D-game data import (dialogue → Ink, the two items) when the user starts it; isolated bug fixes the user or Claude assigns with an exact file and symptom.

Do not edit `ChuckCharacter.*`, the v1 import/groom tools or `/Game/Characters/Chuck/V1` while Claude owns them. Ask for a handoff instead. Run only one Unreal/Blender heavy process at a time and check `tasklist` first. Keep the original CHUCK-game read-only. Record results in docs/agent-handoffs/CODEX.md, and end every session with one line: "Next part of the work can only be done by Claude / Codex", "could be done by either — preference: …", or "can be done here".
