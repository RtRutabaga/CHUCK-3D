@AGENTS.md

Read docs/AGENT-WORKFLOW.md (the **v2 split** at the top is current) and docs/CHARACTER-PLAN.md. Follow the user's actual assignment when it differs from any task file.

Work only in your own worktree/branch (`codex/claude-character`). Under v2, Claude owns the character end to end: Blender source, rig, clips, textures, groom, and their Unreal import, materials, bindings and review. Claude also owns the v1 runtime migration (SkeletalMesh + AnimInstance/AnimBP, contact/IK, switching the default character) and, later, traversal/camera feel. World/environment work stays paused unless the user resumes it. Use the supplied reference JPGs as the visual target.

Do not start generators until you have inspected the source and understood what they replace. Keep edits reproducible without destroying manual art improvements. Run only one Unreal editor/build/import or Blender batch at a time on this 16 GB machine. Report source commit, changed files, contract compatibility, actual validation, evidence paths and remaining flaws in docs/agent-handoffs/CLAUDE.md (template: TEMPLATE.md). Commit to your branch. Integrate into `main` only when asked, and push to the public remote only after the user confirms in that session. End every session with the next-owner line described in AGENT-WORKFLOW.md. No plugin, engine or tool installation without the user's approval.
