# Codex + Claude Code: character-first workflow

Two independent sessions, two Git worktrees, one integration step. This is repository groundwork, not an automated agent orchestrator.

## Current split — v2 (2026-09-27, user request)

The user asked to allocate more of the work to Claude. Codex's usage limits interrupt long tasks, and `References/ai-dev-notes.md` suggests Claude for open-ended, multi-file and feel/visual work and Codex for narrow, well-specified tasks. This replaces the original ownership table below. Suggestions in the notes stay suggestions; `AGENTS.md` scope rules still govern.

| Area | Owner (v2) |
| --- | --- |
| Character end to end: Blender source, rig, clips, textures, groom, **and their Unreal import/material/binding/review** (`Tools/*chuck_v1*`, `Tools/*groom*`, `/Game/Characters/Chuck/V1`) | **Claude** |
| **v1 runtime migration**: SkeletalMesh + native AnimInstance/thin AnimBP, capsule-synchronised clips, foot contact/IK on the v1 skeleton, switching the default character (`ChuckCharacter.*`, character checks in `DockGameMode.cpp`) | **Claude** (was Codex) |
| Later: traversal/parkour controller, small-character camera feel, tail/secondary motion | **Claude** |
| Independent verification: packaged `Verify-Package.ps1 [-MotionCapture]`, reviewing Claude's branches with fresh eyes, contact/regression telemetry and tests | **Codex** |
| Narrow tooling: validators, build/packaging scripts, data import/conversion (e.g. future 2D-game dialogue/items → Ink), save/load, isolated bug fixes given an exact file/symptom | **Codex** |
| Integration to `main` (cherry-pick/merge verified commits, import, package, verify, update `HANDOFF.md`) | Whichever agent is active; one at a time |
| Publishing (`git push` to the public remote) | Only after the user confirms in that session |

Working rules:

- **One heavy tool at a time.** Only one Unreal editor/build/import (or Blender batch) at a time on this 16 GB machine. Check `tasklist` before starting; never kill another session's process.
- **Branches and worktrees.** Claude keeps `codex/claude-character` (`Local/AgentWorktrees/claude-character`); Codex keeps `codex/movement-foundation` (`Local/AgentWorktrees/codex-movement`). Refresh a branch from `main` only when its worktree is clean. Neither agent edits the other's worktree. Work left uncommitted in the main checkout is carried into a branch by whoever picks it up, then the main checkout is cleaned by the integrator.
- **Handoffs.** Each agent keeps `docs/agent-handoffs/<AGENT>.md`: commits, changed paths, what actually ran, evidence, known defects.
- **Next-owner line.** Each agent ends **every** session with one line: "Next part of the work can only be done by Codex / Claude", "could be done by either — preference: …", or "can be done here".
- **Contracts.** A new rig/material-slot/socket contract is still agreed in a committed document (`docs/RIG-CONTRACT-V1.md` pattern) before code depends on it.
- **Original repository.** The original `CHUCK-game` stays read-only for both agents.

Suggested order now:

1. **Integration.** Integrate `0f09073` and later; see `docs/agent-handoffs/CLAUDE.md`, thirteenth pass.
2. **Claude:** v1 runtime migration.
3. **Codex:** independent packaged verification of that migration, plus regression tests for contact slip and reach.
4. **Then**, only if the user adopts the notes' vertical slice: traversal controller and camera (Claude); Ink dialogue/data port and one lootable item (Codex).

## Original preparation (2026-09-26, historical)

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

## Ownership (original split — superseded by v2 above)

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
4. Import only the accepted source using the documented scripts, build/package, then run `Tools/Verify-Package.ps1`. Current baseline: 43 checks with captures, 42 without. Use -MotionCapture for the character review sequences. Also inspect the requested visual/motion evidence; current tests do not prove natural motion.
5. Update HANDOFF with what actually passed, inspect LFS/exclusions, commit imported assets and publish verified main under existing permission. Refresh task branches from that integration point only when their working trees are clean and their owners are ready.

The original `CHUCK-game` remains strictly read-only. Public CHUCK-3D publication permission continues; it does not authorize new paid assets, credentials, engine installs or changes to the original game.

Preparation verified: both worktrees were created from the same workflow base, remained clean, and a second setup run preserved them. Sampled Blender source, imported skeletal asset and CLAUDE.md hashes match the integration checkout. Worktree/generated directories are not tracked. Agent sessions and Claude installation/authentication remain unperformed.


2026-09-26 integration update: Claude delivered character source commits 9e337c3 and 939580a from its desktop-managed worktree; the prepared claude-character branch points to that delivery. Codex integrated those sources and imported the committed FBXs, then developed contact/stride changes on codex/movement-foundation. See both agent handoffs and HANDOFF.md for actual verification. Earlier installation/session notes describe the preparation stage, not current delivery status. Do not reset or refresh Claude's checkout while its owner may be using it.
