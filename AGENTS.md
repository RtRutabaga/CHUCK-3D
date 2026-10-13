# CHUCK 3D working rules

Read docs/PROJECT-BRIEF.md and docs/HANDOFF.md before implementation. Read docs/CHARACTER-PLAN.md before character changes, and docs/AGENT-WORKFLOW.md for the Codex/Claude Code workflow and the ownership/rig contract.

**Scope (2026-10-12):** the Waterdeep prototype is done and the full game has started. CHUCK 3D adapts the completed 2D campaign region by region, in the order and with the rules set out in docs/PROJECT-BRIEF.md. This supersedes every earlier "prototype only / do not expand into the campaign" instruction, including those in archived documents. Build a region only after the user has approved its plan in `docs/regions/`. Changes to Chuck's identity, controls, scale, tone or the brief's rules are the user's decision.

The original CHUCK-game repository (`C:\Users\ashsm\OneDrive\Documents\CHUCK-game`) is strictly read-only. It is the direction for each region: phase documents, dialogue data, maps and the game bible. Do not run its generators, builds, installs, formatters, Git mutations or deployment commands. Access it with absolute paths so that no command resolves inside it by accident.

Chuck is a roughly 65 cm tall gray rat (above the human NPC kneecap) in an oversized purple open jacket. No dialogue, inner monologue, cartoon reactions or power progression. Exploration leads; animation and writing stay restrained. NPC lines come from the 2D game or the user, never AI-written. The supplied character images are the art target; do not claim the current graphics have reached it.

Explain dependencies before installation. Record exact engine/tool versions. Keep generated Unreal output out of Git. Assess LFS and remote storage before committing binary assets. Never put engine installations in this repository.

Update the handoff with what actually runs and what has actually been tested. Do not claim planned tests or packaging checks have passed.

The user runs one agent at a time, and both agents work directly on `main` in `C:\GameDev\CHUCK-3D`. Separate worktrees are optional, for risky experiments or future parallel work. Before editing, inspect branch/status and the latest handoff, and preserve any unfinished work. The root `Launch-Prototype.cmd` is the single player-facing launcher; update its verified package with `Tools/Promote-Build.ps1` when completing a playable milestone. Do not run simultaneous Unreal builds/imports on this 16 GB machine. Do not overwrite another agent's binary assets or regenerate manually improved Blender sources with an obsolete generator.
