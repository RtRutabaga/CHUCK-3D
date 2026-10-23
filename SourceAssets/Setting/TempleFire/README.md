# Temple fire jet flipbooks

Three CC0 Houdini-simulated flipbooks from Unity Labs' free VFX image sequences
(https://unity.com/blog/engine-platform/free-vfx-image-sequences-flipbooks; license
https://creativecommons.org/publicdomain/zero/1.0/), selected October 22, 2026 for the
temple trap run's wall flamethrowers:
- `FireBall01_8x8.tga`: 64 frames of a rolling fireball, RGB on black (additive). The jet's puffs.
- `Flame02_16x4.tga`: 64 frames of a licking flame, RGBA. The pilot flame in each jet's tell.
- `WispySmoke01_8x8.tga`: 64 frames of smoke, RGBA. The smoke the burnt-out puffs leave.

They are unmodified. `manifest.json` records each zip's URL and SHA-256 and each file's SHA-256.

## Reproduction

1. `Tools/Fetch-TempleFire.ps1` downloads the three zips into `Local/UnityLabsFlipbooks/zips`,
   checks their hashes and extracts only these files here.
2. `Tools/import_temple_fire.py` (UnrealEditor-Cmd) imports them into `/Game/Art/ChultTemple/Fire`
   (`T_JetFireBall`, `T_JetPilotFlame`, `T_JetSmoke`) and builds `M_TempleJetFire`, `M_TempleJetPilot`
   and `M_TempleJetSmoke`.

The motion is code, not the textures: `ChultTempleJetFire.cpp` simulates the puffs and draws them on
camera-facing instanced quads. See docs/CHULT-TEMPLE-RUN.md, "The fire jets' flame".
