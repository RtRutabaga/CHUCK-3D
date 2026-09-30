# Movement and camera playtest

Scope: one connected Waterdeep docks scene with Claude's parkour areas, a new Dock Street loop, cargo court, service quay, buildings and a stationary human scale reference. Chuck is 65 cm tall with an oversized open purple jacket. Dialogue, pickups, finished art and additional maps remain deferred. See DOCKS-SETTING.md for the new route.

## Launch

Double-click `Launch-Prototype.cmd` at the repository root. It opens the verified executable under `Builds/Windows/Chuck3D/Binaries/Win64` in a 1280 x 720 window. Click the game window to capture input. No Unreal editor is needed. The launcher identifies its build and checks its executable hash against `Builds/Windows/prototype-build.json`.

The launcher prints its verified revision before opening. See the latest HANDOFF entry for the current packaged milestone; source commits alone do not update the playable build. The package retains Claude's v1 character, groom, authored clips, traversal and continuous orbit camera. Physical Xbox hardware and subjective movement/camera comfort still need user testing.

If recent changes are missing, check where they were packaged: an agent worktree has its own `Builds` directory. Importing assets or committing/pushing source does not update this launcher. Integrate the delivery into main, then package and verify in main, or verify a copied candidate with `Tools/Verify-Package.ps1 -PackageRoot <path>` before promoting it to main's `Builds/Windows`. Keep the previous package until the replacement passes. `Build-Prototype.ps1 -Package` now verifies and records the local build automatically. A source-only delivery must not be described as ready in this launcher.

## Controls

The supplied `waterdeep_docks.wav` now plays automatically as non-spatial background music, with a 1.5-second fade-in at 45% volume. It repeats the full 2:38.4 track; resetting Chuck does not restart the music. The source ending is preserved, so assess the repeat transition and loudness during play. No separate music controls have been added yet.

| Action | Keyboard / mouse | Xbox controller |
| --- | --- | --- |
| Walk relative to camera | WASD or arrow keys | Left stick |
| Run (225 cm/s): tap to start, tap again or stop to saunter (no need to hold it while jumping) | Left Shift | LB |
| Strafe: hold to sidestep facing the camera (Counter-Strike style); forward/back still work while held. With run latched it is a bounding shuffle. Jump with a strafe key held = side jump that way, even while running forward (press strafe and jump together to hop sideways out of a run; it is always sideways, never diagonal): short (~55 cm) from a walk, long (~125 cm) from a run | Hold Q (left) / E (right); A/D also strafe while Q or E is held | Hold LT, left stick |
| Jump (while running: a leap that lands into the stride) | Space | A |
| Wall run: jump into a wall while pushing toward it, or side jump into it (three steps up). Between two facing walls the camera turns side-on and follows the climb | Space + stick toward the wall, or C + stick sideways | A + stick, or B + stick sideways |
| Wall jump: jump while on a wall, or just after leaving it; the stick angles it; he catches the next wall on his own | Space | A |
| Ledge: grabbed automatically when his paws reach a top edge. Hold toward the wall (or jump) to pull up, pull away (or dodge) to let go, jump + pull away to kick off backward | stick / Space / C | stick / A / B |
| Shimmy: while hanging, stick left/right moves hand over hand along the edge. He goes round outside corners and turns into inside corners; keep holding the same way to carry on round (the camera catches up). He stops only where the edge really ends. The camera swings round behind him to face the wall | stick | stick |
| Drop to hang: walk gently (not running, not jumping) off an edge with a drop of more than 60 cm and he turns round and grabs it, GTA-style. The stick that walked him off is ignored until you let go of it; then pull up, shimmy or let go as usual | automatic | automatic |
| Landing roll: a fall of more than about 160 cm (about 2.5 times his height) ends in a roll, toward the stick or the way he was going | automatic | automatic |
| Mantle: walk into a knee-high ledge (6–40 cm) and he hops up onto it | stick | stick |
| Slash (claw scratch); tap again or hold for a flurry on a steady beat with a random paw order (never three of one paw in a row); works while moving | Left mouse button | X |
| Roll toward the stick (straight ahead if none); with the stick held left or right, side jump that way (short from a walk, long with run latched). Keep holding the stick to come out of the roll at your pace (running stays running) | C | B |
| Turn view | Mouse left/right | Right stick left/right |
| Orbit camera height (up = rat height, down = high) | Mouse up/down | Right stick up/down |
| Center camera behind Chuck | F | Right-stick click |
| Reset position and view direction | R | View button |
| Exit prototype | Escape | Menu button |

Sound (2026-09-29): Chuck's paws tap on planks and cobbles (a different sound for each), his jacket flaps on jumps and settles on landings, the claw slash swishes and the roll tumbles, all under the soundtrack. The effects are generated in code (`SourceAssets/Audio/README.md`); tell us if any are too loud, too quiet or the wrong feel.

The camera is one GTA-style orbit with no switch button (since 2026-09-27). Look down (mouse/right stick) and it climbs to the elevated view: 400 cm boom, 48 degrees down, or a little higher. Look up and it sweeps down and in to rat height: 220 cm boom, lens about 76 cm above the floor. Past that it keeps the lens low and tilts it up. Boom length, pivot and field of view blend continuously in between. About 1.2 s after your last look input, the camera eases back behind Chuck while he walks away from it. Strafing or walking toward the camera leaves it alone. It uses camera collision and capped follow smoothing. Controller sticks have a 20% dead zone. A fall below the dock returns Chuck to the start. Reset keeps your chosen camera height.

## Compare the cameras

1. Start in elevated view. Walk to the barrel and crate; check that they block Chuck. Jump onto the 10 cm low step. Walk around the bench; at the revised size Chuck cannot fit beneath its 41 cm clearance.
2. Approach the dock worker and tavern door. Judge whether they feel human-sized beside Chuck. They have no interactions.
3. Walk out along the pier. Jump the missing board near its middle. Falling resets you nearby on the quay.
4. Reset with R / View, look up until the camera sits at rat height, and repeat the route. Look up at the worker and frontage. Round the crate and barrel to check camera collision and visibility.
5. Repeat using the other input device. Report which view feels better, where Chuck disappears or distances become hard to judge, and whether the jump or movement feels too fast or too slow.

Neither camera is selected as the final direction. The user authorized further presentation and animation work; the camera decision still requires playtest feedback.

## Rebuild

Close Unreal before rebuilding. Requires Unreal 5.7.4, Visual Studio Build Tools 2022 with MSVC v143, Windows SDK 10.0.26100.0 and .NET Framework 4.8 SDK/targeting pack. Exact installed versions are in SETUP.md. Run from this repository in PowerShell:

```powershell
powershell -NoProfile -File .\Tools\Build-Prototype.ps1 -Package
powershell -NoProfile -File .\Tools\Test-Prototype.ps1
```

Pass `-EngineRoot` if Unreal is elsewhere. The build script compiles the editor module, creates the small prototype materials and empty map, then packages Windows. Level geometry is generated by the game mode at play time, so the editor map itself is empty until Play. Subsequent material generation updates only named prototype materials and leaves an existing map intact.

The smoke test checks runtime spawn, scale, walking, jump/landing, blocking collision, camera state switching, falling reset and pier gap collision. It does not establish visual quality or physical controller behavior. Those require the actual window and controller playtest.

To repeat the rendered packaged checks and capture both views:

```powershell
& .\Builds\Windows\Chuck3D\Binaries\Win64\Chuck3D.exe -ChuckSmokeTest -ChuckCapture -windowed -ResX=1280 -ResY=720
```

The test exits automatically. Logs and screenshots are under Builds/Windows/Chuck3D/Saved. This package is local generated output and is intentionally excluded from Git; another checkout must rebuild it.

When an obstacle forces the camera within 70 cm of Chuck, his proxy is hidden temporarily to keep the lens clear. It reappears when the camera has room. Tight-space framing still needs user feedback.

The normal smoke run performs 42 checks; adding -ChuckCapture adds the settled recenter check for 43 and captures both views plus walking poses. These automated checks do not establish subjective camera comfort or validate a physical controller.

For this rig pass, walk, turn, stop and jump on clear quay ground in both views. Watch whether the legs remain connected to the feet, the small sleeve/tail movements read naturally, and stops settle without distracting motion. Joint surfaces and foot planting are still prototype quality.

For the graphics pass, compare fur and purple-jacket readability in sun and shade, surface detail near the crate and pier, and the distant harbor silhouette in both views. Water is an opaque procedural study; shoreline transitions and final props are still unfinished.

The scanned surface pass replaces the paving overlay with irregular stone joints and weathered timber maps. Compare detail and repetition at the barrel, crate and pier in both cameras. There is no physical displacement; walking collision remains smooth.

At the far end of the pier, face the moored boat in rat-height mode, then switch to elevated. Compare harbor context with near-foot visibility. The boat and rope coil are scenery, not destinations or interactable objects.

The human scale reference now wears a shirt, vest, trousers and boots. Stand beside the visible knees to compare Chuck's 65 cm height with the 180 cm worker. The worker is still stationary and noninteractive.

For a checked verification run from PowerShell at the repository root:

```powershell
powershell -NoProfile -File .\Tools\Verify-Package.ps1
```

This opens the existing packaged game, exercises the movement/camera checks, captures both views and exits automatically. The wrapper requires the success marker and at least 43 passing checks; it also fails on reported test/material/fatal errors even if Unreal returns exit code zero. Use `-NoCapture` for the 42-check run without screenshots. Timestamped logs go to the ignored Local directory. This command verifies the existing package; it does not rebuild stale source changes.


For the character contact review, run `powershell -NoProfile -File .\Tools\Verify-Package.ps1 -MotionCapture`. It adds rear/front/side rat-height and elevated image sequences under `Builds/Windows/Chuck3D/Saved/Screenshots/Windows/Motion/View0..3`. These cover start, straight walk, 90-degree turn, stop, jump and landing. Contact telemetry is printed in the test log. Keep reviewing ankle connections and jacket shoulder joins; passing contact checks is not finished animation approval.

## Parkour practice yard

On the quay's south strip:

- **Cargo chimney:** two 240 cm crate stacks 100 cm apart. Jump at one pushing toward it, then press jump on each wall to bounce up between them (about 70 cm gained per bounce).
- **Stone harbour wall:** 115 cm high with a walkable top. Run up it and Chuck catches the top edge, then pull up or let go.
- **Mooring plinth:** a 30 cm stone block. Walk into it and he mantles up.

The chimney bounce ends with a catch on a stack top; jump to pull up onto it.

A wall gives one run until Chuck lands or reaches a different wall.

## Cargo wharf (bigger parkour course)

South of the practice yard, a stone wharf extension (8 × 6 m) laid out as one course:

- **Crate staircase** along the west edge: columns 30, 60, 120, 180 and 230 cm tall. Mantle the low ones; jump and catch the tall ones. A plank bridge from the top leads onto the warehouse roof.
- **Warehouse** (230 cm, flat roof) on a stone plinth that juts 40 cm at 115 cm. Run up and catch it, shimmy along it, or stand on it.
- **Alley** (100 cm) between the warehouse and a 260 cm sail loft: bounce up it to either roof.
- **Knee-high field** on the south edge: low harbour walls, bollards, barrels and a crate.
- **Boats** moored off the wharf.

## Chandlers' Row and the Timber Yard

Two more districts, on new quay slabs:

- **Chandlers' Row** (south of the wharf, 8 × 8 m):
  - Three row houses stepping up (180, 230 and 280 cm) with 70 cm gaps between the roofs. Leap across and the wall catch takes you onto the next roof. A 100 cm lean-to shed at the north end is the way up from the street.
  - Four market stalls (80 cm tables) to hop onto.
  - A 6 m garden wall, 120 cm high with a narrow walkable top, reached from a crate.
  - A 90 cm customs terrace with a 30° ramp up to it.
- **Timber Yard** (east of the wharf, 6 × 6 m):
  - Lumber stacks 160, 200 and 90 cm tall. The first two are 100 cm apart: another chimney.
  - A 250 cm crane tower, 80 cm across from the tallest stack, with its arm over the water and a hanging crate.
  - Barrels and a hand cart.
