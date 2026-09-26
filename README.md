# CHUCK 3D

A separate Windows PC adaptation prototype in Unreal Engine, with Blender for custom assets. The first milestone is one small Waterdeep docks slice built from placeholders.

- [Project brief](docs/PROJECT-BRIEF.md)
- [Character references and art direction](docs/ART-DIRECTION.md)
- [Custom character asset study](SourceAssets/Chuck/README.md)
- [Material and daylight study](docs/GRAPHICS-PASS.md)
- [Hardware and installation assessment](docs/SETUP.md)
- [Prototype and camera test plan](docs/PROTOTYPE.md)
- [Reference provenance](References/PROVENANCE.md)
- [Current handoff](docs/HANDOFF.md)

Status: playable Windows prototype using Unreal 5.7.4, with a Blender 4.5.14 character form study, restrained procedural movement, original surface materials and daylight, and two switchable cameras. Double-click `Launch-Prototype.cmd` for the local build. See [controls and camera comparison route](docs/PLAYTEST.md). All 32 rendered packaged runtime checks passed; physical Xbox testing and the camera decision remain open. Chuck now has a Blender skeletal rig with procedural leg IK, arm motion and tail sway; production animation and finished artwork remain future work. Source and LFS assets are published to the separate public GitHub repository. Packaged builds remain local and excluded from Git.

The original `C:\Users\ashsm\OneDrive\Documents\CHUCK-game` is read-only reference. This repository has its own history and public remote at [RtRutabaga/CHUCK-3D](https://github.com/RtRutabaga/CHUCK-3D). No deployment is configured. Never reuse the original game's remote, Pages workflow, save locations, or release destination.
