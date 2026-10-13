# Playing and building CHUCK 3D

How to play the current build, what the controls are and how to build, verify and promote a new one. Region content is described in [regions/](regions/). Older prototype playtest notes are in [archive/PLAYTEST.md](archive/PLAYTEST.md).

## Launch

Double-click `Launch-Chuck.cmd` at the repository root. It checks the executable against the receipt in `Builds/Windows/chuck-build.json`, prints the verified source revision and opens the game full screen. F11 or Alt+Enter switches between full screen and a window. No Unreal editor is needed.

Only a build promoted with `Tools/Promote-Build.ps1` reaches the launcher. Committing source, importing assets or building in a worktree does not update it. The launcher warns when newer Unreal source exists than its build contains.

## Title menu

**New Game** starts the morning docks. **Dev Checkpoints** offers Waterdeep, Sewer, Sewer Jump, Sewer Long Jump, Waterdeep Night, Tavern Pantry and **Chult Landing**. Each starts a fresh world; there is no persistent save yet. Esc exits.

**Chult Landing** is the first jungle setting pass: the cutscene's landing clearing, tall trees and a winding ground loop. Brachiation starts in a later setting section. The normal fall's connection to this playable setting is still Claude's next task; use the checkpoint to explore it now. See [CHULT-LANDING.md](CHULT-LANDING.md).

Falling through the sky hole in the tavern cellar plays the fall-to-Chult cutscene (about 48 s, letterboxed, no control). Esc skips it. It ends on the "End of demo" card, where Esc exits.

## HUD

One burning cigarette at the top left is Sanity. The cigarette count is at the top right, with the stamina ring beneath it. The talk prompt, dialogue subtitles and first-time tutorial hints appear only when needed.

## Controls — keyboard and mouse

| Action | Keyboard / mouse |
| --- | --- |
| Walk relative to camera | WASD or arrow keys |
| Run (225 cm/s): tap to start, tap again or stop to saunter (no need to hold it while jumping) | Left Shift |
| Sprint: while moving with the stamina ring (top right, under the cigarette count) full, a burst on all fours at 380 cm/s that drains the ring in 2.5 s, then he is back up running. The ring refills whatever he's doing, in 10 s from empty, and shines purple for a moment when full; he can only start a sprint with it full. Dodging, strafing, letting go or a real fall ends a sprint early and keeps the stamina left (half used refills in 5 s); small step-downs don't end it | Left Ctrl |
| Sprint leap: jump while sprinting, after at least a metre of straight sprinting run-up, for a long leap on all fours (about 2.8 m, two and a half times the running jump, 40 cm high) that lands back into the sprint and costs no stamina; it also works just after a small step down. Without the run-up a sprint jump is the ordinary running jump. The pantry's cheese can be reached with a leap from the cellar floor, but it's a one-way trip: the island has no room for a run-up and the sky hole's rim can't be caught. It clears the 1.4 m gap between the Bonded Stores and Chandler's roofs either way: down by landing, up by scrambling over the edge. Vaults and side wall runs out of a sprint stay at run speed | Space while sprinting |
| Strafe: hold to sidestep facing the camera (Counter-Strike style); forward/back still work while held. With run latched it is a bounding shuffle. Jump with a strafe key held = side jump that way, even while running forward (press strafe and jump together to hop sideways out of a run; it is always sideways, never diagonal): short (~55 cm) from a walk, long (~125 cm) from a run | Hold Q (left) / E (right); A/D also strafe while Q or E is held |
| Jump (while running: a leap that lands into the stride) | Space |
| Wall run: jump into a wall while pushing toward it, or side jump into it (three steps up). Between two facing walls the camera turns side-on and follows the climb | Space + WASD toward the wall, or C + WASD sideways |
| Wall jump: jump while on a wall, or just after leaving it; the movement keys angles it; he catches the next wall on his own | Space |
| Ledge: grabbed automatically when his paws reach a top edge, flat or pitched (a house eave too; he hangs clear of the overhang). Hold the movement keys roughly toward the wall (or jump) to pull up, pull away (or dodge) to let go, jump + pull away to kick off backward. Arriving with the edge already at his hips, he scrambles straight over it | WASD / Space / C |
| Shimmy: while hanging, movement keys left/right moves hand over hand along the edge. He goes round outside corners and turns into inside corners; keep holding the same way to carry on round (the camera catches up). He stops only where the edge really ends. The camera swings round behind him to face the wall | WASD |
| Drop to hang: walk gently (not running, not jumping) off an edge with a drop of more than 60 cm and he turns round and grabs it, GTA-style. The movement keys that walked him off is ignored until you let go of it; then pull up, shimmy or let go as usual | automatic |
| Landing roll: a fall of more than about 160 cm (about 2.5 times his height) ends in a roll, toward the movement keys or the way he was going | automatic |
| Mantle: walk into a knee-high ledge (6–40 cm) and he hops up onto it | WASD |
| Slash (claw scratch); tap again or hold for a flurry on a steady beat with a random paw order (never three of one paw in a row); works while moving | Left mouse button |
| Roll toward the movement keys (straight ahead if none); with the movement keys held left or right, side jump that way (short from a walk, long with run latched). Keep holding the movement keys to come out of the roll at your pace (running stays running) | C |
| Turn view | Mouse left/right |
| Orbit camera height (up = rat height, down = high) | Mouse up/down |
| Center camera behind Chuck | Middle mouse button |
| Talk (to an NPC with something to say; press again for the next line) | F |
| Exit | Escape |
| Toggle full screen / window (the launcher opens full screen) | F11 or Alt+Enter |

Brachiation on lantern rings and eave grabs are described in [BRACHIATION.md](BRACHIATION.md).

## Controls — Xbox controller

| Action | Controller |
| --- | --- |
| Move / camera | Left stick / right stick |
| Jump | A |
| Roll (dodge) | B |
| Slash | X |
| Talk | Y |
| Run (latched) | LB |
| Sprint | Left stick click |
| Strafe | LT |
| Recenter camera | Right stick click |
| Exit | Menu |

These are the bindings in `Unreal/Chuck3D/Config/DefaultInput.ini`. Physical Xbox play, including menu navigation, has not been verified.

## Build, verify and promote

Close Unreal and the game first. Run only one Unreal or Blender heavy process at a time. Requirements and exact versions are in [SETUP.md](SETUP.md).

```powershell
powershell -NoProfile -File .\Tools\Build-Game.ps1 -Package -ArchiveDirectory .\Builds\<Name>Candidate
powershell -NoProfile -File .\Tools\Promote-Build.ps1 -Candidate .\Builds\<Name>Candidate -Label <Name> -VerificationLog .\Local\verify-package-<timestamp>.log
```

`Build-Game.ps1` compiles the editor module, runs `create_base_assets.py` (the base materials in `/Game/Common/Materials` and the empty `/Game/Maps/WaterdeepDocks` map; the world is generated by the game mode at play time), packages Windows and runs `Verify-Package.ps1` on the candidate. `Promote-Build.ps1` writes the receipt, moves the candidate into `Builds/Windows`, keeps the previous build as `Windows-Previous-<date>-<Label>` and prunes older backups to two.

For source-only changes with the base assets already present, add `-SkipBaseAssets` to retain the existing binary materials/map. Builds use `-NoUBTMakefiles` to avoid restoring stale checkout paths and expired sandbox environment variables from Unreal's build cache.

- `Tools/Test-Game.ps1`: editor smoke test with no packaging.
- `Tools/Verify-Package.ps1 [-PackageRoot <dir>] [-MotionCapture]`: the packaged test suite. Logs go to `Local/`.
- `Tools/Verify-Menu.ps1 [-PackageRoot <dir>]`: drives the real title-menu buttons and checkpoints. Run it after menu or startup changes.

Automated checks do not establish visual quality, camera comfort or physical controller behaviour.
