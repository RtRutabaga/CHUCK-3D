# CHUCK 3D

A Windows PC 3D adaptation of the completed 2D game CHUCK, built in Unreal Engine with Blender for custom assets. The Waterdeep prototype is done (2026-10-12), and the full game now follows the 2D campaign region by region; see the project brief.

- [Project brief](docs/PROJECT-BRIEF.md): scope, rules and the region list
- [Playing and building](docs/PLAYING.md): launcher, controls, build/verify/promote
- [Current handoff](docs/HANDOFF.md)
- [Region plans and summaries](docs/regions/): Waterdeep (done), Chult (plan)
- [Codex + Claude Code workflow](docs/AGENT-WORKFLOW.md)
- [Character plan](docs/CHARACTER-PLAN.md) and [rig contract](docs/RIG-CONTRACT-V1.md)
- [Art direction](docs/ART-DIRECTION.md)
- [Hardware and installation](docs/SETUP.md)
- [Custom character assets](SourceAssets/Chuck/README.md), [dock props](SourceAssets/Docks/README.md), [reference provenance](References/PROVENANCE.md)
- [Prototype-era archive](docs/archive/)

Status: the Waterdeep docks, plaza, sewer, tavern and pantry are playable in Unreal 5.7.4 (Blender 4.5.14 assets). Double-click `Launch-Chuck.cmd` for the local build. Chult is the next region. The supplied character images remain the visual target and have not been reached. Source and LFS assets are published to the separate public GitHub repository; packaged builds stay local and excluded from Git.

The original `C:\Users\ashsm\OneDrive\Documents\CHUCK-game` is read-only reference. This repository has its own history and public remote at [RtRutabaga/CHUCK-3D](https://github.com/RtRutabaga/CHUCK-3D). No deployment is configured. Never reuse the original game's remote, Pages workflow, save locations, or release destination.
