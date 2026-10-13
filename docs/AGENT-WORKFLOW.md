# Codex + Claude Code workflow

## Current workflow — v3: sequential work on main

The user explicitly switched to this workflow after confirming that only one agent works at a time. This section supersedes the worktree-only, separate-integration and per-session publication instructions in v2 and the historical task files below.

- Both agents use `main` in `C:\GameDev\CHUCK-3D`. Check the actual checkout/branch, `git status` and latest handoff before edits. Never discard another session's unfinished changes. Review and continue them where appropriate; ask only if ownership or intent is unclear.
- Claude retains character, animation, rig/import and traversal/camera ownership; Codex handles region setting work and verification/tooling. The active user assignment takes precedence.
- Only one agent edits this checkout at a time. Keep one heavy Unreal/Blender process at a time on the 16 GB machine; do not kill an unknown session's process. If simultaneous work resumes, agree on isolated branches/worktrees first.
- Use the root `Launch-Chuck.cmd` as the single player-facing launcher. Completed playable milestones must be built, verified and promoted to root `Builds/Windows` with `Tools/Promote-Build.ps1 -Candidate <dir> -Label <Name> -VerificationLog <log>`. It writes the receipt on the candidate (a failing log stops it before anything moves), refuses while the game is running from the launcher build, keeps the old package as `Windows-Previous-<date>-<Label>`, runs the launcher check and deletes all but the newest two backups (`-Keep`). Delete leftover candidate folders it warns about once they are no longer needed. Committing or pushing source alone does not update the game.
- Update HANDOFF and the active agent's handoff with changes, actual test evidence, remaining flaws and launcher status. Commit completed work on main and push routine verified project changes under the user's standing publication permission. Document partial/uncommitted work honestly if interrupted.
- Existing worktrees are retained, not deleted or reset. Before moving any unfinished work out of one, inspect its status and commits and preserve it. Old launchers/task files are historical development copies, not the default workflow. If a Claude app session is pinned to a worktree and cannot access the root checkout, ask the user to reopen the root project.
- Separate worktrees remain optional for risky experiments. Rig contracts, binary-asset care, original-game read-only boundaries and the next-owner line still apply.
- **Next-owner line.** End every session with one line: "Next part of the work can only be done by Codex / Claude", "could be done by either — preference: …", or "can be done here".
- **Contracts.** A new rig/material-slot/socket contract is agreed in a committed document (the `RIG-CONTRACT-V1.md` pattern) before code depends on it.
- **Regions.** Before building a region, write `docs/regions/<REGION>.md` from the 2D phase documents and get the user's approval (see PROJECT-BRIEF.md).

Older workflows (v1 worktrees, v2 split) are in [archive/AGENT-WORKFLOW-HISTORY.md](archive/AGENT-WORKFLOW-HISTORY.md). Per-agent prototype handoff logs are in [archive/agent-handoffs/](archive/agent-handoffs/).
