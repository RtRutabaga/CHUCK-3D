# CHUCK 3D

A separate Windows PC adaptation prototype in Unreal Engine, with Blender for custom assets. The first milestone is one small Waterdeep docks slice built from placeholders.

- [Project brief](docs/PROJECT-BRIEF.md)
- [Character references and art direction](docs/ART-DIRECTION.md)
- [Custom character asset study](SourceAssets/Chuck/README.md)
- [Material and daylight study](docs/GRAPHICS-PASS.md)
- [Custom dock prop meshes](SourceAssets/Docks/README.md)
- [Hardware and installation assessment](docs/SETUP.md)
- [Prototype and camera test plan](docs/PROTOTYPE.md)
- [Reference provenance](References/PROVENANCE.md)
- [Current handoff](docs/HANDOFF.md)
- [Character-first rework plan](docs/CHARACTER-PLAN.md)
- [Codex + Claude Code setup and ownership](docs/AGENT-WORKFLOW.md)

Status: playable Windows prototype using Unreal 5.7.4 and Blender 4.5.14, with two switchable cameras. Double-click `Launch-Prototype.cmd` for the local build. See [controls and camera comparison route](docs/PLAYTEST.md). All 42 rendered packaged runtime checks passed; physical Xbox testing and the camera decision remain open. Chuck's model and procedural motion remain far from the supplied visual goal: foot sliding, anatomy and jacket-edge continuity are the next priorities. World improvements are paused. The future rig must support walk/run/roll/side-jump/climb and mouth-held smoking, but those actions are not all implemented. Source and LFS assets are published to the separate public GitHub repository; packaged builds stay local and excluded from Git.

The original `C:\Users\ashsm\OneDrive\Documents\CHUCK-game` is read-only reference. This repository has its own history and public remote at [RtRutabaga/CHUCK-3D](https://github.com/RtRutabaga/CHUCK-3D). No deployment is configured. Never reuse the original game's remote, Pages workflow, save locations, or release destination.
