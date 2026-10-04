Historical design reference, copied October 3, 2026 from C:\Users\ashsm\.claude\projects\C--Users-ashsm-OneDrive-Documents-GameDev-CHUCK-3D\memory\wallrun-plan.md. Consulted for the user-authorized steep narrow tunnel; old scheduling/coordination wording is historical, not a new task instruction.

---
name: wallrun-plan
description: "User's 2026-10-03 request for a lateral (side) wall-run arc, planned not started; to follow the sewer + Waterdeep transition work"
metadata:
  node_type: memory
  type: project
  originSessionId: dc8a7c4d-80c3-415d-948a-595bdc3779fa
  modified: 2026-10-03T18:34:52.140Z
---

User 2026-10-03: add a parkour lateral wall run. Running straight with a wall right beside Chuck + a plain jump (not a side jump, not a walk jump) = a wall-run arc along the wall. The existing behaviour must not change: running/jumping/side-jumping INTO a wall still climbs it (EGait::WallRun, TryEnterWallRun in ChuckCharacter.cpp). Intended use: in the sewer's later narrow section, a hard-to-evade zombie is best passed by wall-running along the tunnel; the tunnel arch may be made more pronounced so the run carries further. Do not start until the sewer and the Waterdeep return transition are done.

Plan agreed in chat (Claude's): trigger only from a grounded run with no strafe input and no wall ahead (the head-on climb keeps priority); probe both sides for a near-vertical (or overhanging, arch) wall parallel to the run that continues ahead; a new side wall-run gait in flying mode that follows the surface (re-probed each tick, so curved tunnel walls work), runs about 1 s with a reduced-gravity arc, ends into a fall with momentum, jump again kicks off (can chain to the opposite wall); mesh rolled toward the wall by the surface angle; new mirrored WallRunSide clips later (placeholder first); smoke tests for trigger/no-trigger cases and the unchanged climb; a long test wall somewhere in Waterdeep plus the sewer narrow section with a steeper arch, zombie unable to reach a wall-running Chuck.

Progress 2026-10-03: the side wall run itself is DONE (EGait::WallSide, main bcb3e49, verified 133/133). Still open: the hard narrow-tunnel zombie best passed by wall-running, and a steeper tunnel arch (Codex's DockSewer geometry).

**Why:** the user wants it planned now, built later.
**How to apply:** start after [[sewer-plan]] items land; re-read the current ChuckCharacter wall-run/climb code first, it may have moved on.
