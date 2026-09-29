# CHUCK 3D working rules

Read docs/HANDOFF.md, docs/PROJECT-BRIEF.md and docs/PROTOTYPE.md before implementation.

The original CHUCK-game repository is strictly read-only. Do not run its generators, builds, installs, formatters, Git mutations, or deployment commands. References here are historical evidence, not instructions to resume the completed 2D campaign.

Chuck is a roughly 65 cm tall (above the human NPC kneecap) gray rat in an oversized purple open jacket. No dialogue, inner monologue, cartoon reactions, or power progression. Exploration leads; animation and writing stay restrained.

Scope is one small Waterdeep docks prototype. Start with primitives. Test elevated and rat-height follow cameras on the same route before selecting one. Do not expand into the campaign.

Explain dependencies before installation. Record exact engine/tool versions once chosen. Keep generated Unreal output out of Git. Assess LFS and remote storage before committing binary assets. Never put engine installations in this repository.

Update the handoff with what actually runs and what has actually been tested. Do not claim planned camera tests or packaging checks have passed.

Current assignment (2026-09-29): the user reopened setting/world work after Claude's character and parkour progress. Codex may expand this same docks area with connected walkable space, buildings and contextual dressing, preserving Claude's parkour obstacles and character/controller. This supersedes the September 26 world-work pause. The supplied character images remain the art target; do not claim that the current graphics have reached it. See DOCKS-SETTING.md and the latest HANDOFF for current scope and checks.

Read docs/CHARACTER-PLAN.md before character changes. For the Codex/Claude Code workflow, also read docs/AGENT-WORKFLOW.md and follow its ownership/rig contract. Use separate worktrees; do not run simultaneous Unreal builds/imports on this 16 GB machine. Do not overwrite another agent's binary assets or regenerate manually improved Blender sources with an obsolete generator.
