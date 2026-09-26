# Codex + Claude Code: character-first workflow

Two independent sessions, two Git worktrees, one integration owner. This is prepared repository groundwork, not an automated agent orchestrator. No Claude session, login or installation has been performed.

## Prepared layout

Run `powershell -NoProfile -File .\Tools\New-AgentWorktrees.ps1` from the main repository to create/reuse:

| Role | Branch | Folder relative to main repository |
| --- | --- | --- |
| Codex movement/runtime | `codex/movement-foundation` | `Local/AgentWorktrees/codex-movement` |
| Claude character art | `codex/claude-character` | `Local/AgentWorktrees/claude-character` |
| Integration/publication owner | `main` | Main CHUCK-3D repository |

The worktree folders, builds and logs are ignored. Source/LFS data is checked out; Unreal/Blender installations are shared outside the repository. Each worktree has its own generated Unreal files. Do not share Binaries/Intermediate/Saved via symlinks. The script does not reset existing branches, delete directories or launch an agent. Keep main as the verified integration checkout while both task branches are active.

## Local setup status (2026-09-26)

Claude Code was not found on PATH, at `%USERPROFILE%\.local\bin\claude.exe`, or in the npm global launcher location. This does not exclude an unrelated desktop/WSL installation. Git for Windows and `C:\Program Files\Git\bin\bash.exe` are present; Unreal 5.7.4, Blender 4.5.14 LTS and the existing C++ toolchain are sufficient for project work. C: had about 274 GiB free at inspection. No engine, Blender, Node or WSL installation is required for this workflow.

The remaining new dependency is Claude Code if you want its CLI. Use the official [Windows setup instructions](https://code.claude.com/docs/en/setup); their native PowerShell command is:

```powershell
irm https://claude.ai/install.ps1 | iex
```

This installs Claude Code outside this repository. After installation, open a new terminal, run `claude --version` and `claude doctor`, then launch `claude` and complete its browser sign-in with your own supported account. No credentials belong in Git. Record the actual installed version in your handoff. Installation/sign-in has intentionally been left for the user, who requested setup groundwork.

Launch from the **Claude worktree**, not main:

```powershell
Set-Location 'C:\Users\ashsm\OneDrive\Documents\GameDev\CHUCK-3D\Local\AgentWorktrees\claude-character'
claude
```

Paste the assignment in `docs/agent-tasks/CLAUDE-CHARACTER.md`. Give Codex the `codex-movement` worktree and `docs/agent-tasks/CODEX-MOVEMENT.md`. Do not additionally use Claude's `--worktree` flag inside the already prepared worktree. CLAUDE.md imports the shared AGENTS.md using the official [project-memory mechanism](https://code.claude.com/docs/en/memory). Separate checkouts follow the documented [worktree workflow](https://code.claude.com/docs/en/common-workflows#run-parallel-sessions-with-worktrees).

## Ownership

| Owner | Initially writable scope |
| --- | --- |
| Claude | `Tools/build_chuck_model.py`, `SourceAssets/Chuck/`, `docs/agent-handoffs/CLAUDE.md` |
| Codex movement | `ChuckCharacter.cpp/.h`, character-focused runtime checks in `DockGameMode.cpp`, `docs/agent-handoffs/CODEX.md` |
| Integration owner | Unreal character `.uasset` imports, shared materials/import scripts, verifier counts, shared docs, `main`, packaging and publication |

Read other files freely within CHUCK-3D; don't write across ownership without coordinating a concrete handoff. Unreal binaries cannot be meaningfully line-merged. Claude delivers source and reproducible export instructions; the integrator imports after review. Do not regenerate manually improved `.blend` files from an older script. A new rig or material-slot contract must be coordinated before either side depends on it.

Use only one Unreal editor/build/import at a time on this 16 GB machine. Text/code review can proceed while the other session uses Blender. Announce heavy-tool use in the handoff to the user/other session; there is no automatic cross-product lock or mailbox. Never kill another session's processes blindly. Do not copy engine installations, original CHUCK-game assets or an entire original repository.

## Initial compatibility contract

- Centimetres, Z up, source forward +X, feet at Z=0, ear top at 65 cm. FBX reflects Y: current runtime left has negative Y.
- Preserve current root/head, thigh_L/R, shin_L/R, arm_L/R, forearm_L/R and tail_0..3 names, parents and rest transforms for the first independent tasks. Do not treat this temporary 14-bone rig as production-ready.
- Keep current `/Game/Characters/Chuck/SK_ChuckBody` and `SM_ChuckFoot` paths/material slot names, 65 cm collision height, both camera modes and inputs. Export part weight totals must remain 1.
- Capsule movement is runtime-owned. Model/garment edits do not change capsule or camera anchors. No new actions or world improvements during this initial split.
- For the later rig migration, first commit a proposed bone/rest-pose/attachment/clip contract, agree it with the other owner, then merge assets and runtime as one verified milestone.

## Integration loop

1. Both agents start from the same recorded base commit and confirm `git status --short`, branch and worktree path. Do not run pull/rebase while the other session uses that checkout.
2. Work and commit only on the assigned branch. Keep generated output ignored and binary source in LFS. Fill the handoff template with commit(s), changed paths, contract changes, evidence and known defects.
3. Give the completed branch/commit and handoff to the integration owner. The owner reviews diffs and source art, then cherry-picks focused commits onto clean main one branch at a time. Resolve source conflicts deliberately; re-import assets rather than combining binary histories blindly. No force-push or history rewriting is needed.
4. Import only the accepted source using the documented scripts, build/package, then run `Tools/Verify-Package.ps1`. Current baseline: 42 checks with captures, 41 without. Also inspect the requested visual/motion evidence; current tests do not prove natural motion.
5. Update HANDOFF with what actually passed, inspect LFS/exclusions, commit imported assets and publish verified main under existing permission. Refresh task branches from that integration point only when their working trees are clean and their owners are ready.

The original `CHUCK-game` remains strictly read-only. Public CHUCK-3D publication permission continues; it does not authorize new paid assets, credentials, engine installs or changes to the original game.

Preparation verified: both worktrees were created from the same workflow base, remained clean, and a second setup run preserved them. Sampled Blender source, imported skeletal asset and CLAUDE.md hashes match the integration checkout. Worktree/generated directories are not tracked. Agent sessions and Claude installation/authentication remain unperformed.
