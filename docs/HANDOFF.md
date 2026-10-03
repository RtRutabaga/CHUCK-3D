# Handoff — 2026-09-27

## Current launcher and integration status

**Update 40 (Codex, October 2 — starting court ruins and supported planks):** runtime `f07f43d` / `59e67a5`. Four stored plank bundles slope from the ground to their barrel rims. The mixed plaster/stone store beside spawn is now entirely stone and roofless, with broken stepped upper courses and solid rubble. The nearby L foundation loses its coping and has broken end stones/rubble while retaining a clear central climb landing. Two isolated tall crate stacks are absent in normal play; historical wall-run/chimney/camera regression fixtures exist only under `-ChuckSmokeTest`. These regression passes do not establish a playable tower route. All 15 chimneys on solid setting buildings now block collision; distant vista stays noncolliding. No assets/dependencies or character/controller edits. Ruin shapes remain blocky prototype geometry.

Initial package built and captured, but verifier found two failed pull-up checks due to loose stones blocking the central landing (`Local/verify-package-20261002-202605.log`); cleared that landing. Final build `Local/ruins-build-final.log` succeeded. All three initial Ruins views and final View0 inspected (`Local/ruins-capture.log`, `Local/ruins-capture-final.log`). Final verifier `Local/verify-package-20261002-203203.log` passed 125 gameplay checks, all existing setting/pier/prop/boundary/music checks and 15 new required chimney traces. No manual traversal, physical-controller or MotionCapture repeat. Root launcher promoted and receipt checked at `59e67a5`; backup `Builds/Windows-Previous-20261002-Ruins`. Generated files untracked. Next part of the work can be done here.

**Update 39 (Codex, October 2 — obsolete layout cleanup):** runtime `1d3bf0d`. Removed three isolated low wharf wall stubs, the redundant garden divider and northern tavern-court cross-street wall. The court's waterside edge retains a low kerb. Replaced the railed narrow plaza approach and enclosed boat pocket with continuous paving; relocated that boat to open harbor water at (1550,-1860). Useful ledge/corner and rooftop parkour routes remain. No character/controller changes, assets or dependencies.

Build `Local/setting-cleanup-build.log` succeeded. Packaged Setting View0 and Plaza View0 inspected; captures `Local/setting-cleanup-setting-capture.log` and `Local/setting-cleanup-plaza-capture.log`. Verifier `Local/verify-package-20261002-200618.log` passed 125 gameplay checks and all setting, pier, prop, boundary and music checks, including expanded plaza checks (14 floors, 11 capsule routes, closed sewer). No manual traversal or physical-controller/MotionCapture repeat. Root launcher promoted and receipt checked at `1d3bf0d`; backup `Builds/Windows-Previous-20261002-SettingCleanup`. Generated output untracked. Next part of the work can be done here.

**Update 38 (Codex, October 2 — coastal mountain):** runtime `b764556` / `dd2a2f1`. Added a prominent western summit near (-32000,6500), with two shoulders, uneven spurs/gullies and gray-brown high rock slopes blended into existing distant terrain. Trees restricted below 55 m. Reuses existing mesh/material, no binary assets/dependencies or playable collision changes. First roof review found the summit too close/tall and cropped; moved it back and reduced its height. Low-detail silhouettes remain provisional. Vista review now has six views, including rat-height from the new dock.

Final build `Local/mountain-build-final.log` succeeded; rooftop View0 and dock View5 inspected (`Local/mountain-capture-final.log`). Full summit visible above the city from the roof; visible through the building/wall gap from dock. Final verifier `Local/verify-package-20261002-195119.log` passed 125 gameplay checks and all setting/pier/prop/boundary/music checks. No physical-controller, MotionCapture or formal performance repeat. Root launcher promoted and receipt checked at `b764556`; backup `Builds/Windows-Previous-20261002-Mountain`. Generated files untracked. Next part of the work can be done here.

**Update 37 (Codex, October 2 — dock boundary and solid stores):** runtime `84ac3a9` / `49465dc`. Northern court boundary now returns along X890 to Y3740, then reaches X1845 at the breakwater/water, closing the shore-side vista shortcut. Five added barrels use cylindrical collision matching the original barrel (scaled for the four small ones). Four stored plank bundles retain their visible boards with collision enabled. Dock entrance stores shifted from Y3100 to Y3260 after tests caught an obstruction. No new assets/dependencies.

Final build `Local/dock-boundary-build-final.log` succeeded; packaged overview View11 inspected (`Local/dock-boundary-capture-final.log`). Verifier `Local/verify-package-20261002-193059.log` passed 125 gameplay checks, all previous world/plaza/street/grate/music/pier checks, and eleven new required checks (five barrels, four plank bundles, two boundary crossings). Initial verifier failed one approach and one prop trace before the relocation; final rerun passed. No manual traversal, physical-controller or MotionCapture repeat. Root launcher promoted and receipt checked at `84ac3a9`; backup `Builds/Windows-Previous-20261002-DockBoundary`. Generated output untracked. Next part of the work can be done here.

**Update 36 (Codex, October 2 — ivy and weathered plaster):** runtime `bbf66d3` / `ae5756b` / `e723888`. Solid setting/plaza plaster bodies use a dedicated world-space material with muted lime-wash variation, damp foundation stains, relief and sparse cracks. Six irregular exposed-masonry patches with chipped render rims and six localized ivy growths (2,304 lobed leaves and branching stems) add visible age. Noncolliding; parkour and character unchanged. Two new LFS materials total 18,882 bytes; LFS fsck passed, no install/download or paid storage purchase. Remote allowance unknown. Existing material graphs untouched. This remains procedural prototype art; no formal performance benchmark.

Initial cook exposed a missing ivy vertex-color connection; corrected before review. First close-ups then exposed single-sided stone patches, corrected in `bbf66d3`. Final build `Local/weathering-build-verified.log` succeeded without material fallback warnings. Four close-ups captured (`Local/weathering-capture-verified.log`); initial ivy views 1/2 and final patch views 0/3 inspected. Final verifier `Local/verify-package-20261002-185821.log` passed 125 gameplay checks plus all world/plaza/Dock Street/side-gate/pier/music checks. No physical-controller or MotionCapture repeat. Root launcher promoted and receipt checked at `bbf66d3`; backup `Builds/Windows-Previous-20261002-Weathering`. Generated output untracked. Next part of the work can be done here.

**Update 35 (Codex, October 2 — sewer court waterfront dock):** runtime `eff5b8c`. Removed the eastern/waterside wall of northern Dock Street, retaining the west/north walls and sewer gate/grate. Added a flush 6.2 x 10 m landing with two finger piers (8.4 x 3 m and 6.6 x 2.6 m), continuous deck collision, timber piles/bracing, patched boards/nails, mooring coils, lamp, bench and small cargo nook. Existing harbor breakwater and shipping channel retained. No new binary assets/dependencies. Setting capture now has thirteen views; views 11/12 inspect the dock.

Build `Local/court-pier-build.log` succeeded. Packaged views 11/12 inspected (`Local/court-pier-capture.log`). Verifier `Local/verify-package-20261002-183401.log` passed 125 gameplay checks, all prior world/plaza/Dock Street/side-gate/music checks, and 19 new pier checks (10 floor samples, nine capsule routes), now required by Verify-Package. No physical-controller or MotionCapture repeat; routes checked with traces/sweeps rather than a manual traversal session. Root launcher promoted and receipt checked at `eff5b8c`; backup `Builds/Windows-Previous-20261002-CourtPier`. Generated output untracked. Next part of the work can be done here.

**Update 34 (Codex, October 2 — aged working docks):** runtime `4443077`. User direction: rough, longstanding medieval dockside, maintained rather than absolute squalor. Added facade repair masonry/timber braces, shutter leaves/drip caps, door boards/straps/rivets/latches, flush roof patches, wall-side barrels/rope/stored planks and ladder, muted upper-floor laundry, repaired bench slats, shop fuel/herbs and patched market canvas/counters. Reused materials and prop meshes; no binary assets, installation or new dependencies. Dressing has no collision; existing controller and parkour geometry retained. Art remains provisional; dedicated surface weathering still needed.

Build `Local/aged-docks-build.log` succeeded. Packaged Setting views 1/7 and Plaza view1 inspected; captures `Local/aged-docks-setting-capture.log` and `Local/aged-docks-plaza-capture.log`. Verifier `Local/verify-package-20261002-181315.log` passed 125 gameplay checks plus all world/plaza/Dock Street/side-gate/music checks. No physical-controller or MotionCapture repeat. Root launcher promoted and receipt checked at `4443077`; backup `Builds/Windows-Previous-20261002-AgedDocks`. Generated output untracked. Next part of the work can be done here.

**Update 33 (Codex, October 2 — plaza connections and town backdrop):** runtime `7c28408` / `6770cda`. Removed the crosswise 48 cm approach lip at (-680,-2270). Added a 4.8 m stone return at (-1585,-2300), with battlements/coping/torch, joining the existing western district boundary to the plaza's western wall. Added 38 noncolliding buildings and supporting scenery behind the closed plaza gate, connecting into the western vista. The initial review exposed the far platform edge down the lane; final roof rows close that view. No binary assets or dependencies added. Graphics remain provisional.

Final build `Local/plaza-connection-build-final.log` succeeded. Packaged plaza views 3/5/6 reviewed across the two captures; final backdrop inspected in `Local/plaza-connection-capture-final.log`, View6. Final verifier `Local/verify-package-20261002-175913.log` passed 125 gameplay checks plus world/plaza/Dock Street/side-gate/music checks. No physical-controller or MotionCapture repeat. Root launcher promoted and receipt checked at `7c28408`; backup `Builds/Windows-Previous-20261002-PlazaConnection`. Generated output remains untracked. Next part of the work can be done here.

**Update 32 (Codex, October 2 — grate beside gate):** runtime `cafa272`. Moved the grate to (-1580,3900), north of the gate and opposite the bench at Y3300; doorway paving remains clear. Updated collision probes and close-up capture positions. No new assets or dependencies.

Build `Local/grate-side-build.log` succeeded; packaged views 9/10 visually inspected (`Local/grate-side-capture.log`). Verifier `Local/verify-package-20261002-173443.log` passed 125 gameplay checks plus world, plaza, Dock Street, side-gate and music checks. No physical-controller or MotionCapture repeat. Promoted to root launcher and checked receipt at `cafa272`; previous package retained at `Builds/Windows-Previous-20261002-GrateSide`. Generated files remain untracked. Next part of the work can be done here.

**Update 31 (Codex, October 1 — side gate and sewer setup):** runtime `1577561` / `cb6e7ed`. A small closed timber gate with simple stone framing is on the city-facing western wall at the far end of Dock Street (-1748,3650). A flush iron grate is directly in front (-1580,3650), over retained solid ground. This is the future sewer entrance location; both props are noninteractive. Existing plaza sewer arch remains decorative drainage. No new assets/dependencies, map or dialogue.

Final build `Local/side-gate-build-final.log` succeeded. Packaged verifier passed 125 gameplay checks, existing world/plaza/Dock Street/music checks and three new checks for gate blocking, ground beneath the grate and clear capsule approach (`Local/verify-package-20261001-191507.log`). Close-ups caught an existing bench overlapping the grate; it was moved along the wall, then rebuilt, captured and retested. Final captures `Local/side-gate-capture-final.log`, package `Setting/View9..10.png`, show a clear apron. No physical-controller or MotionCapture repeat. Setting review now has eleven views. Root launcher promoted and receipt checked at `1577561`; backup `Builds/Windows-Previous-20261001-SideGate`. Next part of the work can be done here.

**Update 30 (Codex, October 1 — coastal city vista):** runtime `4544034` / `650bec6`, retaining the latest Claude hands work and completing the earlier wall-style pass. Added continuous noncolliding countryside around west/north/east, connected northern harbor land and opposite-bank backing, 235 simple building silhouettes plus four towers and sparse woodland. Open sea remains to the south. Sea/sky expanded to contain the new horizon. Roof views are taken at 1150 cm above the highest Dock Street roof. Existing playable collision and traversal unchanged.

Uses Unreal 5.7's bundled ProceduralMeshComponent plugin, 23,254 terrain vertices and two new LFS materials totaling 10,176 bytes. No installation/download or paid storage; LFS fsck passed. Earlier interrupted box-terrain draft was replaced before building. First rooftop review led to closed gables, simple windows/bands, terrain winding and ridge refinements. Final build `Local/coastal-vista-build-final.log` succeeded. Five packaged rooftop views inspected (`Local/coastal-vista-capture-final.log`, package `Saved/Screenshots/Windows/Vista/View0..4.png`); no material errors. Verifier passed **125/125**, 15 world + 20 plaza + 28 Dock Street checks and music (`Local/verify-package-20261001-183738.log`). No physical-controller/MotionCapture repeat or formal performance benchmark. Hills/trees/buildings remain low-detail and repetitive; target art fidelity is not achieved.

Root launcher now uses the verified combined package `4544034`, receipt checked; prior hands/walls package retained at `Builds/Windows-Previous-20261001-CoastalVista`. This also finishes publication of the pending wall milestone below. Generated output is untracked. See COASTAL-VISTA.md. Next part of the work can be done here.

**Update 29 (Codex, October 1 — perimeter wall styling):** runtime `cfc7ce2`, including Claude's latest hands work through `681dd10`. The user's red-marked perimeter walls now follow the plaza's stone thickness, battlements and warm interior torches, with a coping course. Existing wall heights and entrance retained; thicker solid wall bodies still pass route checks. No new assets or dependencies. This commits the wall edit that Claude's earlier hands package had included while uncommitted.

Build `Local/wall-build.log` succeeded. Packaged verifier passed 125 gameplay checks, 15 world, 20 plaza, 28 Dock Street and music (`Local/verify-package-20261001-112429.log`). No physical-controller or MotionCapture repeat for this wall pass. Graphics remain provisional.

**Update 28 (Claude, 2026-10-01, user request: fix the weird hand placement, with finger capability; the guard will hold a spear later):** runtime `681dd10`.
- The humans are rebuilt on MPFB's `game_engine` rig: Unreal mannequin names and three bones in every finger, instead of `cmu_mb`'s single finger bone per hand (paddle hands). The CMU motion capture is mapped onto it by name.
- Hands now rest beside the thighs:
  - wrists mostly straightened (the capture's wrist data was poor);
  - arms eased 5° out so the hands clear wider hips;
  - every finger joint curls to a relaxed rest (little finger most, index least).
- `ADockNPC::SetGrip(side, amount)` closes a hand into a fist round a shaft, ready for the guard's spear. The spear prop and his holding pose are not done.

The root candidate passed `-MotionCapture` **125/125**, plus the world, plaza and music checks (`Local/verify-package-20261001-101818.log`).
- Fingers curl 26–27°.
- Straight arms hang at most 16° out; the A-pose is about 45°.
- Promoted to `Builds/Windows` with receipt `681dd10`; the previous package is kept as `Builds/Windows-Previous-20261001-Hands`.

**Caveat:** Codex was working in root at the same time. Its uncommitted edit to `Unreal/Chuck3D/Source/Chuck3D/DockSetting.cpp` (10:07, city-wall masonry, battlements and torches) was compiled into this package. So the launcher is `681dd10` plus that uncommitted change; it passed all checks. I did not touch that file. Codex should commit or revert it and re-promote so the receipt matches. Not yet played by the user.

**Update 28 (Codex, October 1 — playable Dock Street):** runtime `749367b` / `6568704`, retaining Claude's `0efe501` character/NPC work. The northern town beyond the DOCK STREET sign now has continuous solid ground, an open entrance, eight existing houses with solid bodies/roofs, connected lanes/courts, shop awnings/signs/lamps, benches and cargo. The western larger-city boundary stays closed. Houses are exterior shells; no new dialogue or interiors. The interrupted draft's duplicate overlapping buildings and incomplete floor were replaced before packaging. No binary assets/dependencies added.

Root launcher promoted and receipt checked at `749367b`; previous package retained at `Builds/Windows-Previous-20261001-DockStreet`. Walk directly through the DOCK STREET sign opening, then between the houses into the court. Next part of the work can be done here.

Final build succeeded (`Local/dock-street-build-final.log`). Packaged verifier passed **125/125 gameplay checks**, 15 world + 20 plaza checks, music loop, and **28 new Dock Street checks** (10 floors, 9 capsule routes, 8 facade/roof traces, closed western boundary): `Local/verify-package-20261001-082916.log`. Initial two route failures were cargo in the lane; moved cargo to the edge and reran successfully. Entrance, court and overhead views reviewed; final lighting capture `Local/dock-street-capture-final.log`. No physical-controller or MotionCapture repeat. Selected routes/roofs are checked; this is not exhaustive testing of all parkour escape attempts. Graphics remain provisional.

**Update 27 (Claude, 2026-10-01, user request: no craned necks; react when Chuck scratches a friendly NPC):** runtime `0efe501`.
- NPCs still turn their heads to follow Chuck, but tip down at most 6° until he's within about a metre. They only look properly down once he's at their feet (full at 45 cm).
- Chuck's scratch now catches a friendly NPC in reach. They start back: hands to the chest, a lean and half-turn away, a small shift of the feet (a 2 s slice of CMU's "scared" take, 79_73). Then they settle and turn to the rat.

The root candidate passed `-MotionCapture` **125/125** and the world, plaza and music checks (`Local/verify-package-20260930-220145.log`).
- Look-down 6.0° at 1.3 m and 52.7° when Chuck is pressed close.
- The scratch set the reaction off; the worker turned 82.8° to a rat at his side.

Promoted to `Builds/Windows` with receipt `0efe501`; the previous package is kept as `Builds/Windows-Previous-20261001-React`. Details in `agent-handoffs/CLAUDE.md` pass 62. Not yet played by the user.

**Update 26 (Claude, 2026-10-01, user request: proceed with character work, sewer later):** runtime `93a586d`.
- The worker, guard and market woman now move with motion capture: three takes from Carnegie Mellon's free mocap library (any use), retargeted onto the shared skeleton.
  - The guard keeps looking about.
  - The worker and the woman shift their weight and set a hand on a hip now and then. Each plays from its own point in the clip.
  - Head look-at and curled fingers are layered on top.
- When Chuck talks to one of them, it gestures (a hand-gesturing "explaining" clip) and turns to face him.
- If Chuck stays off to one side, the NPC turns its body to face him, then back to its post when he goes.
- The woman's skirt and apron move with her hips and no longer split between her legs.

The root candidate passed `-MotionCapture` **124/124**, plus the world, plaza and music checks (`Local/verify-package-20260930-211255.log`). New check: all three play motion capture, and the worker turns 82.8° to a rat at his side.
- Hands measure 24–29 cm out and 9–14 cm ahead.
- Promoted to `Builds/Windows` with receipt `93a586d`; the previous package is kept as `Builds/Windows-Previous-20261001-Mocap`.
- Details: `agent-handoffs/CLAUDE.md` pass 61. Not yet played by the user.

**Update 25 (Claude, 2026-10-01, user request: resume character work; NPCs must not hold their hands out like zombies):** runtime `66aa87f`.
- Every human now stands naturally. A standing pose is solved per body from the model's A-pose: arms hanging just clear of the hips, soft elbows, palms turned to the thighs, wrists straight, fingers curled. Breathing, weight shift and glances play on top.
- **Guard** (183 cm; quilted green gambeson, steel breastplate and helmet, hip belt, tall boots) stands before the closed city gate in the plaza. He blocks Chuck; F / Y: "Stick to the docks, rat."
- **Market woman** (166 cm; chemise, square-necked bodice, long skirt, apron, kerchief) stands at the end of the aisle between the red-canopied market stalls: "No handouts here. If you're hungry, you should check the sewer for scraps."
- Both lines and placements are from `References/Original/PHASE-2.md`. The sewer stays non-interactive, as Codex built it.

The root candidate passed `-MotionCapture` **123/123**, the world, plaza and music checks (`Local/verify-package-20260930-195245.log`). Measured hands are 20–24 cm out and 4–8 cm ahead of the body line. Promoted to `Builds/Windows` with receipt `66aa87f`; the previous package is kept as `Builds/Windows-Previous-20261001-Townsfolk`. Details are in `agent-handoffs/CLAUDE.md` pass 60. Not yet played by the user.

**Update 24 (Codex, September 30 — opposite waterfront):** runtime `bb2526a` / `c053469`, based on Claude's combined worker build `f7deb68`. Added quay coping, masonry supports and timber fenders, three landing fingers with moored boats/derricks, loading fronts on the rear warehouses, and a harbor entrance with two breakwater heads and continued land/roofs. Scenery only: existing playable collision, character, parkour, plaza and dawn lighting preserved. Existing meshes/materials reused; no binary assets or dependencies added.

Windows build succeeded (`Local/harbor-bank-build-final.log`). Six setting views reviewed, with final opposite-quay inspection after moving crane posts clear of warehouse faces; capture log `Local/harbor-bank-capture-final.log` has no material errors. Rendered verifier passed **122/122**, **15 world checks**, **20 plaza checks** and music loop (`Local/verify-package-20260930-192046.log`). MotionCapture and physical-controller tests were not repeated. Architecture still repeats and water remains basic; this is not the reference graphics target.

Root launcher verified at `bb2526a`; previous combined worker/plaza package retained at `Builds/Windows-Previous-20260930-Harbor`. Source revision and installed receipt were checked immediately before promotion to avoid replacing concurrent work. Generated files remain untracked. Next part of the work can be done here.

**Update 23 (Claude, 2026-10-01, user request: NPCs at about Blade & Sorcery: Nomad quality, cheap to copy):** runtime `f7deb68`.
- The dock worker is rebuilt as a MakeHuman human (MPFB 2.0.17 and the CC0 MakeHuman assets, installed and downloaded with the user's approval; versions and hashes in `SETUP.md`). He has a textured face and eyes, and wears a linen shirt with rolled sleeves, a leather jerkin, a belt, wool trousers tucked into leather boots and a red cap (CC0 Poly Haven fabrics at real size). He stands with his arms down from the model's A-pose and keeps the old behaviour (breathing, glances, watching Chuck, solid but not climbable).
- New NPCs are data: an entry in `SourceAssets/NPCs/humans.json`, one Blender build (`Tools/build_npc_humans.py`), one import (`Tools/import_npc_humans.py`). All humans share one skeleton (MPFB `cmu_mb`, CMU mocap bone names) and four master materials. `-ChuckNPCCapture` writes portraits of every NPC. See `SourceAssets/NPCs/README.md`.
- The superseded lofted worker (builder, importer, assets) is removed.

The root candidate passed `-MotionCapture` **122/122** (new: the worker's arms are by his sides, hand 24.3 cm out), the world, plaza and music checks (`Local/verify-package-20260930-183904.log`). Promoted to `Builds/Windows` with receipt `f7deb68`; previous package kept as `Builds/Windows-Previous-20261001-Humans`. This also includes Codex's plaza refinements `8422cd4`/`d38d505`. Codex's uncommitted two-line note in `docs/WATERDEEP-PLAZA.md` was left untouched. Details: `agent-handoffs/CLAUDE.md` pass 59. Not yet played by the user.

**Update 23 (Codex, September 30 — plaza detail pass):** runtime `d38d505` (with `8422cd4`) refines the fountain coping and thinner jets with moving water beads, closes shop gables, adds window hoods and door hardware, harbor anchor banners, gate rivets, sewer latch/drain dressing and a two-sided approach sign. All additions are noncolliding; existing routes, parkour, character and controller remain unchanged. No binary assets or dependencies added.

Concurrent-work correction: Claude completed and promoted `f7deb68` while this pass waited; it already includes these plaza changes. Codex briefly promoted the older candidate, detected the newer receipt, and restored Claude's combined package with its original verified receipt. Root launcher check passed at `f7deb68`. The older plaza-only package is retained at `Builds/Windows-PlazaDetail-Verified-d38d505` with corrected revision metadata. Claude's combined verification log is `Local/verify-package-20260930-183904.log`. Next part of the work can be done here, coordinating with Claude.

Final Windows build succeeded (`Local/plaza-detail-build-final.log`). Five final packaged plaza/spawn views were inspected (`Local/plaza-detail-capture-final.log`); no material fallback errors. Rendered verifier passed **121/121**, **15 world checks**, **20 plaza checks** and the music-loop check (`Local/verify-package-20260930-140609.log`). MotionCapture and physical-controller tests were not repeated. Fountain effects and architecture remain prototype quality, below the supplied art target.

**Update 22 (Codex, September 30 — additive Waterdeep plaza and dawn):** runtime `9e527bf`, based on Claude's current rigged-worker milestone `dd02680`/`9b5b46e`. Added adjoining land with two ground-level approaches, tiered fountain, smithy/alchemist exteriors, stalls, benches, ruin fragment, battlement walls, closed city gate and barred sewer arch. Existing docks, skyline, parkour, character/controller/NPCs and gameplay assets are retained. Low warm light, cool ambient sky, lit lamps and wall torches replace midday presentation. Sewer has solid backing and no interaction or destination. See WATERDEEP-PLAZA.md for route and scope.

Original game was read-only: two small text maps copied with source SHA-256 hashes and byte-preserving Git attributes; no original code/assets/generator copied or executed. Three new LFS world materials total 18,048 bytes; LFS fsck passed, no new dependencies or paid storage. New material script initially needed a Python property-access correction, then succeeded. Build `Local/plaza-build.log` succeeded. Packaged verifier passed **121/121**, existing **15 world checks**, new **20 plaza checks** (11 floors, 8 clear capsule routes, closed sewer trace), and music-loop check: `Local/verify-package-20260930-135125.log`. Five packaged plaza/spawn views were visually inspected (`Local/plaza-capture.log`, package `Saved/Screenshots/Windows/Plaza/View0..4.png`). MotionCapture and physical controller tests were not repeated for this setting pass.

Root launcher now uses the verified plaza package, runtime `9e527bf`; previous worker package retained at `Builds/Windows-Previous-20260930-Plaza`. Both approaches and the fountain loop are checked, not every possible wall climb. Shops/gates are noninteractive, architecture and fountain/flame effects remain prototype quality, and reference graphics fidelity is not achieved. Next setting work can be done here; Claude retains character/traversal ownership.

**Update 13 (Codex, September 29 — supplied soundtrack):** runtime `18e6144` adds the user's `waterdeep_docks.wav` as looping non-spatial music, 45% volume with a 1.5-second fade-in. It starts with the level and continues through position resets. The unmodified 158.4-second stereo 48 kHz source, SHA-256 provenance and import notes are in `SourceAssets/Audio`. Source WAV and imported SoundWave use LFS (roughly 49 MB total); no new dependency or original-game access. Import commandlets require `-AllowCommandletAudio`; the initial decoder ensure was resolved by enabling it.

Import and Windows build succeeded (`Local/music-import-audio.log`, `Local/music-build.log`). Packaged rendered verifier passed **99/99**, **15 route checks**, and `CHUCK_MUSIC_CHECK failures=0 looping=1 playing_after_boundary=1` (`Local/verify-package-20260929-205701.log`). The audio smoke run starts two seconds before the source end and checks six seconds later; normal play starts at the beginning. Playback state and active audio device were checked, not subjective loudness or an audible seamless transition. The track's original ending is preserved; no crossfade edit. LFS fsck passed; generated output remains untracked.

Root launcher now uses the verified music package, runtime `18e6144`; previous detail build retained at `Builds/Windows-Previous-20260929-Music`. No movement, camera or setting geometry changes. Next work can be done here.

**Update 21 (Claude, 2026-09-30, user request: the human NPC by the spawn):** runtime `9b5b46e`.
- The dock worker is now a rigged NPC, built in Blender on the user's go-ahead for my recommendation: a 180 cm dockhand, the 2D game's human-scale reference, on the old figure's spot.
- He breathes, shifts his weight, glances about, and turns his head to watch Chuck, looking down at the rat. He's solid but can't be climbed.
- New `ADockNPC` base with F / Y talk: a prompt in reach and a plain dialogue box. The worker has no dialogue yet (user's call); the guard and market woman are next.
- Keyboard re-centre moved to the middle mouse button.

Details are in `agent-handoffs/CLAUDE.md` pass 58.

The root candidate passed `-MotionCapture` **121/121**, the world check and the music check (`Local/verify-package-20260930-130741.log`). It was promoted to `Builds/Windows` with receipt `9b5b46e`; the previous package is kept as `Builds/Windows-Previous-20260930-Worker`. Not yet played by the user.

**Update 20 (Claude, 2026-09-30, user request: exhaled smoke):** runtime `3a5faf8`. Every 7–12 s, when calm, Chuck breathes out a stream of soft smoke from the cigarette corner of his mouth, which drifts, swells, rises and thins. The breath is barely audible. The user doesn't want Astral Anchors: the map spawn stays the respawn point. Details are in `agent-handoffs/CLAUDE.md` pass 57.

The root candidate passed `-MotionCapture` **118/118**, the world check and the music check (`Local/verify-package-20260930-121520.log`). It was promoted to `Builds/Windows` with receipt `3a5faf8`; the previous package is kept as `Builds/Windows-Previous-20260930-Exhale`. Not yet played by the user.

**Update 19 (Claude, 2026-09-30, user request: Sanity and the astral respawn):** runtime `d934fee` adds Sanity, as in the 2D game:
- a bar of five cigarettes (top right); a bite burns one;
- picked-up cigarettes refill it, then count up like coins;
- at zero Chuck, a fey summon who can't die, quietly sinks into starlight, the view fades to astral indigo, and he's summoned back at the start;
- the summon: a rune circle, spiralling motes and a soft column, with him appearing curled in the light and rising (the new `Summon` clip);
- vanish and summon sounds.

Details are in `agent-handoffs/CLAUDE.md` pass 56 and `PLAYTEST.md`.

The root candidate passed `-MotionCapture` **117/117**, the world check and the music check (`Local/verify-package-20260930-114301.log`). It was promoted to `Builds/Windows` with receipt `d934fee`; the previous package is kept as `Builds/Windows-Previous-20260930-Sanity`. Not yet played by the user.

**Update 18 (Claude, 2026-09-30, user request: the small rat enemy):** runtime `5ae941e` adds five big dock rats (cargo wharf, timber yard, garden):
- a generated model, posed procedurally;
- they roam, notice and chase Chuck, then give a clear tell (a crouch and a hiss) before a lunging bite;
- a bite knocks Chuck back, and he has 1 s of immunity (no health yet);
- two slashes (the low rake is automatic) kill a rat, which drops a cigarette;
- natural rat sounds, heard only nearby.

Details are in `agent-handoffs/CLAUDE.md` pass 55 and `PLAYTEST.md`.

The root candidate passed `-MotionCapture` **115/115**, the world check and the music check (`Local/verify-package-20260930-103417.log`). It was promoted to `Builds/Windows` with receipt `5ae941e`; the previous package is kept as `Builds/Windows-Previous-20260930-Rat`. Not yet played by the user. Open question for the user: should Chuck have health (and what happens when it runs out)?

**Update 17 (Claude, 2026-09-30, user request: low rake, cigarettes, jars):** runtime `5a869d8` adds:
- an automatic low rake for low targets;
- 10 breakable clay jars (solid; they shatter into shards and drop 1–3 cigarettes);
- cigarette pickups (about one grass tuft in three holds one) and a top-right counter.

Design rule (user): Chuck breaks grass and jars (urns in later maps), never crates or barrels; recorded in `PROJECT-BRIEF.md`. Details are in `agent-handoffs/CLAUDE.md` pass 54.

The root candidate passed `-MotionCapture` **113/113**, the world check and the music check (`Local/verify-package-20260930-092918.log`). It was promoted to `Builds/Windows` with receipt `5a869d8`; the previous package is kept as `Builds/Windows-Previous-20260930-Loot`. Not yet played by the user.

**Update 16 (Claude, 2026-09-30, user request: shreddable grass tufts):** runtime `da4eea5` adds 51 generated weed tufts around the docks and a shared breakable base (`ChuckBreakable`) for grass, then jars and small enemies. A slash shreds any tuft in reach to stubble, with a clipping spray and a rustle, and Chuck walks through grass. Details are in `agent-handoffs/CLAUDE.md` pass 53 and `PLAYTEST.md`.

The root candidate passed `-MotionCapture` **110/110**, the world check and the music check (`Local/verify-package-20260930-085524.log`). It was promoted to `Builds/Windows` with receipt `da4eea5`; the previous package is kept as `Builds/Windows-Previous-20260930-Grass`. Not yet played by the user. Next suggested: the low rake for low targets, then cigarette drops and pickups.

**Update 15 (Claude, 2026-09-30, user request: strafe + jump always side jumps):** jump with a strafe key (Q/E, or LT with the stick sideways) held is now always a side jump, even while running forward; the keys are read live, so pressing strafe and jump together works. Details are in `agent-handoffs/CLAUDE.md` pass 52. The root candidate's first `-MotionCapture` run failed one check: flurry paw slip 6.7 cm/s after a single slow frame (a 0.248 s strike gap). The slash code was unchanged, and the check had passed in all four worktree runs. Two reruns of the same package passed **108/108** plus the world and music checks (`Local/verify-package-20260930-081436.log`). Promoted to `Builds/Windows` with receipt `de4b81f`; the previous package is kept as `Builds/Windows-Previous-20260930-SideJump`. The flurry slip check can fail on a slow frame.

**Update 14 (Claude, 2026-09-29, user request: movement sound effects):** runtime `cf0a70e` adds generated SFX (`Tools/gen_chuck_sfx.py`, `SourceAssets/Audio/SFX`, imported to `/Game/Art/Audio/SFX`). They play 2D under the soundtrack: paw steps on wood and stone (walk and run), jump, land, claw slash and roll. Details are in `agent-handoffs/CLAUDE.md` pass 51 and `SourceAssets/Audio/README.md`.

The root candidate passed `-MotionCapture` **107/107**, the world check and the music check (`Local/verify-package-20260929-221028.log`). It was promoted to `Builds/Windows` with receipt `cf0a70e`; the previous package is kept as `Builds/Windows-Previous-20260929-Sfx`. Loudness against the music and how they sound haven't been judged by ear yet.

**Update 13 (Claude, 2026-09-29, user requests: strafe, faster run, drop to hang, landing roll height):** runtime `c0154fd` adds:
- strafe on Q/E or held LT, with jump while strafing = side jump (short from a walk, long from a run);
- a faster run (225 cm/s);
- drop to hang when walking gently off an edge;
- the landing roll only above a 160 cm fall.

Q/E no longer turn the camera. Details are in `agent-handoffs/CLAUDE.md` pass 50 and `PLAYTEST.md`.

The candidate built in the root checkout passed `Verify-Package.ps1 -MotionCapture` **106/106**, the world check and the music check (`Local/verify-package-20260929-211245.log`). It was promoted to root `Builds/Windows` with receipt `c0154fd`; the previous package is kept as `Builds/Windows-Previous-20260929-Strafe`. Worktree runs also passed `-NoGroom` and uncapped with and without groom. Not yet played by the user.

This pass was authored in Claude's old app worktree and fast-forwarded onto main (that session was pinned there). Later Claude sessions should open the root project.

**Workflow update:** the user now runs Codex and Claude sequentially and explicitly requests direct work on root `main`. Read v3 in `AGENT-WORKFLOW.md` and the updated `CLAUDE.md`. Preserve unfinished changes; use one root launcher and update its verified package for playable milestones. Existing worktrees remain intact as optional development copies. This documentation-only update does not change the game or require rebuilding it.

**Update 12 (Codex, September 29 — near-field dock details):** runtime `42f33d7` reuses existing beveled crate/rope meshes in the loading court and adds platform boards, loading-door joinery, high barred warehouse windows, a net-drying frame, tavern rear windows/timber bays and continuous drain grates. Existing cargo proxy dimensions are preserved, with proxies hidden when the visual mesh is available. Added dressing is noncolliding. No character/controller, binary source or material changes; no installs or asset regeneration.

Built successfully (`Local/dock-detail-build.log`). Packaged rendered checks passed **99/99** plus **9 ground / 6 capsule-route checks**, no material fallback failures (`Local/verify-package-20260929-133159.log`). Four setting captures were visually reviewed (`Local/dock-detail-capture.log`; package `Saved/Screenshots/Windows/Setting`). MotionCapture and physical-controller testing were not repeated. Repeated architecture, basic roof/material treatment and simplified water remain unfinished; this does not meet the reference graphics target.

Normal root launcher uses this verified package, runtime `42f33d7`; previous skyline package is retained at `Builds/Windows-Previous-20260929-Detail`. Generated output remains untracked. Next setting refinement can be done here; character/traversal remain Claude-owned.

**Update 11 (Codex, September 29 — skyline and working facades):** runtime `6aa739b` adds stepped city land, uphill roof rows, a distant civic hall/towers, a northern continuation and opposite-bank buildings. Canopies, brackets, shutters and trade signs distinguish Dock Street; shallow details dress the parkour workshop faces. This is visual scenery only: playable collision, character/controller, obstacle dimensions and materials are unchanged. No new binary assets or dependencies.

Build succeeded (`Local/skyline-build.log`). Packaged verifier with rendered captures passed **99/99** plus **9 ground samples / 6 capsule routes**, with no material fallback failures (`Local/verify-package-20260929-131527.log`). Four setting captures were visually reviewed (`Local/skyline-capture.log`; package `Saved/Screenshots/Windows/Setting`). MotionCapture was not rerun for this scenery-only pass. Repeated architecture, basic materials, distant boundaries and simple water remain visible limitations; background land is nonplayable scenery.

Normal launcher package now carries runtime `6aa739b`; previous connected-docks build retained at `Builds/Windows-Previous-20260929-Skyline`. Use root `Launch-Prototype.cmd`. Generated files remain untracked. Next setting refinement can be done here; Claude retains character/traversal ownership.

**Update 10 (Codex, September 29 — connected docks setting):** reviewed Claude's integrated work through `c2f26c3` and preserved its parkour geometry, character assets and controller. The user explicitly reopened setting work. Runtime `ab885d8` adds Dock Street, a cargo court, a service quay and tavern rear court; seven solid buildings, fuller old frontage shells, contextual obstacle dressing and 22 grounded background buildings. See `DOCKS-SETTING.md` for routes, ownership and limits.

Packaged with existing Unreal 5.7.4, no installs or character regeneration. Final build: `Local/setting-build-final.log`. `Verify-Package.ps1 -MotionCapture -PackageRoot Builds/SettingCandidate/Windows` passed **99/99**, plus **9 ground samples and 6 capsule-clearance routes**, with no material fallback/compile failures: `Local/verify-package-20260929-124955.log`. The first candidate passed traversal but exposed missing instancing flags on world materials; those flags were corrected and the full suite rerun. Seven existing LFS material assets total about 191 KB; no new character binaries. LFS integrity passed and generated files remain untracked.

Four packaged setting views were reviewed at overview and street level. The expanded district remains repeated blockout architecture with closed doors and simple water; the harbor horizon and distant edges still need development. These checks do not establish physical Xbox behavior, subjective camera comfort, natural traversal on every new facade, or the reference graphics target. Claude retains character/traversal ownership. Codex setting code is isolated in `DockSetting.cpp/.h` with one scene-construction call.

The verified candidate is the normal `Builds/Windows` launcher package; the previous version is retained as `Builds/Windows-Previous-20260929-Setting`. Always use the repository-root `Launch-Prototype.cmd`, whose receipt identifies runtime `ab885d8`. No original CHUCK-game files were touched.

**Update 9 (Claude, 2026-09-29, user request "Integrate and push to GitHub"):** main fast-forwarded to `c2f26c3`, adding:
- ledge corners (outside and inside, with the stick carried round the turn);
- an automatic landing roll after falls over 80 cm;
- the side-on chimney camera, which follows the climb's height;
- side jumps into a wall starting a wall run.

Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **99/99** (`Local/verify-package-20260929-120607.log`); receipt `c2f26c3`. Pushed main to `origin` at the user's request. Next: Codex takes the setting/world work.
**Update 8 (Claude, 2026-09-29, user request "Integrate"):** main fast-forwarded to `7324500`, adding:
- the running jump and tap-to-run;
- the claw slash: a wide, fast arc and a held flurry with a random paw order;
- parkour: wall run, wall jump, ledge grab/hang/pull-up/drop, knee-high mantle, shimmy and the hang camera;
- the practice yard, cargo wharf, Chandlers' Row and Timber Yard;
- sleeves that read as arms in a coat, and a vivid violet jacket.

Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **94/94** (`Local/verify-package-20260929-100457.log`); receipt `7324500`. Nothing pushed.

**Update 7 (Claude, 2026-09-27, user request "Integrate"):** main fast-forwarded to `f0b296c`, adding:
- the run (tap Shift / LB);
- rolling straight back into the run with the stick held;
- keyboard side-jump fixes (a dodge cuts a turn in place short; the stick is read directly);
- the running jump (a split leap that lands into the stride).

Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **69/69** (`Local/verify-package-20260927-215040.log`); receipt `f0b296c`. Nothing pushed.

**Update 6 (Claude, 2026-09-27, user request "Integrate and run"):** main fast-forwarded to `08a425b`, adding the first roll and side jump: C / Xbox B dodge; the stick held sideways (camera-relative) gives a side jump. Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **57/57** (`Local/verify-package-20260927-192523.log`); receipt `08a425b`. Nothing pushed.

**Update 5 (Claude, 2026-09-27, user request "Integrate and begin roll and side jump"):** main fast-forwarded to `fd78cc8`, adding:
- the aplomb idle and swagger saunter;
- wider, fuller legs;
- a more exaggerated, brisk swagger (72 cm/s);
- the GTA-style continuous orbit camera, which replaces the C/Y camera switch.

Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **51/51** (`Local/verify-package-20260927-184911.log`); receipt `fd78cc8`. Nothing pushed.

**Update 4 (Claude, 2026-09-27, user request "Integrate"):** main fast-forwarded to `2fa74eb`: ears wider apart on the outer top skull corners; neutral human-like hands (palm to thigh, thumb forward, fingers curling inward, clips curl medially). Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **50/50** (`Local/verify-package-20260927-150737.log`); receipt `2fa74eb`; content paks cooked at 15:07. Nothing pushed.

**Update 3 (Claude, 2026-09-27, user request "Integrate"):** main fast-forwarded to `1057e0f`, adding:
- ears arranged as in the turnaround: forward cups on the upper skull corners, seated so they cannot float;
- natural forward finger curl;
- textured head fur between sleek and the haircut crown.

Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **50/50** (`Local/verify-package-20260927-135522.log`); receipt `1057e0f`. The executable was not relinked (no C++ change since `37208d3`); the cooked content paks are from 13:55. Nothing pushed.

**Update 2 (Claude, 2026-09-27, user request "Integrate"):** main fast-forwarded to `37208d3`, adding four passes:
- the face-proportion pass: a short conical rat snout (0.74, v1.2 amendment), smaller high eyes;
- a scruffy city-rat body coat with guard hairs;
- a softer tan-cream muzzle and amber eyes;
- sleek, laid-back head fur (the lifted crown had read as a human haircut).

Rebuilt in the main checkout: `Verify-Package.ps1 -MotionCapture` **50 passes, 0 failures** (`Local/verify-package-20260927-130138.log`); receipt `37208d3`; `Launch-Prototype.ps1 -CheckOnly` shows no newer-source warning. Nothing pushed.

**Update (Claude, 2026-09-27, user request "integrate"):** Claude merged `codex/claude-character` into main as `4eef511` (merge of `699efd0..c2e31d5` over Codex's `a4b00b2`; one clean automatic merge). That brings in:
- the four 2026-09-27 goal images and the user's snout/fur target (`References/ArtDirection/`);
- the look passes: warm shaggy groom, narrow head, cropped red-violet jacket;
- the v1.1 proportions and the v1.2 snout amendment (`docs/RIG-CONTRACT-V1.md`, `Tools/chuck_v1_shape.py`);
- the cigarette prop and smoke (`-ChuckNoCigarette` removes it);
- long clawed hands and a pointed shirt collar;
- a 136,000-strand groom with per-strand colour variation.

The package was rebuilt **in the main checkout**: editor module, then BuildCookRun into `Builds/Windows`. Then:
- `Verify-Package.ps1 -MotionCapture`: **50 passes, 0 failures** (`Local/verify-package-20260927-121313.log`).
- `Write-PrototypeReceipt.ps1` recorded build `4eef511`.
- `Launch-Prototype.ps1 -CheckOnly` identified it with no newer-source warning.

`Launch-Prototype.cmd` now opens this Chuck. Not verified by this integration: a physical controller, subjective camera comfort, and whether the look matches the user's taste. Remaining look gaps are listed in `docs/agent-handoffs/CLAUDE.md`, 21st pass. Nothing was pushed to the public remote.

Claude's delivery through `00e46d6` is now integrated on main: v1 skeletal character, native AnimInstance/contact IK, authored walk/start/stop/turn/jump clips, three bound groom groups, and the revised jump/follow cameras. Character/art ownership follows the v2 split in AGENT-WORKFLOW.md. World work remains paused.

The user reported that the normal launcher showed none of this. Cause: `Launch-Prototype.cmd` still opened main's September 26 13:03 executable, while Claude packaged the new game in `.claude/worktrees/project-orientation-fd7504/Builds/Windows`. Git commits and asset imports do not replace a packaged executable or its cooked content.

Codex copied Claude's package into an isolated candidate (without changing Claude's worktree), ran `Verify-Package.ps1 -MotionCapture`, and obtained **49 passes, zero failures** in `Local/verify-package-20260927-093257.log`. The fresh front capture was visually inspected and shows the v1 character. That exact candidate was promoted to main's `Builds/Windows`; the old package is retained at `Builds/Windows-Previous-20260926`. Captures now live under `Builds/Windows/Chuck3D/Saved/Screenshots/Windows`.

`Launch-Prototype.cmd` now calls `Tools/Launch-Prototype.ps1`, which checks the executable against a verification receipt, identifies build `00e46d6`, and warns when newer committed Unreal changes are not packaged. `Build-Prototype.ps1 -Package` now runs package verification and writes the receipt before declaring the build ready. `Verify-Package.ps1 -PackageRoot <candidate>` supports testing a copied build before promotion. Normal launch still uses main's package, never an arbitrary agent worktree.

The actual `.cmd` launcher was executed after promotion and the running process path confirmed as main's `Builds/Windows/Chuck3D/Binaries/Win64/Chuck3D.exe`. The initial launcher test exposed an unavailable `Get-FileHash` command in that shell environment; hashing now uses .NET directly and the retest passed. The game was left open for the user. Script syntax and Git whitespace checks passed; no new engine build was necessary because the exact copied package passed independent verification.

Interrupted Codex groom work was preserved in Git stash `e3164da437321014069a0c5bb31e14a3b1431c7a` before the fast-forward; do not reapply it over Claude's replacement implementation. No new character or world design was done during this launcher repair. Physical controller and subjective camera comfort remain unverified; the reference-quality art target is still unmet.

## Historical handoff below — superseded by the launcher status above

The following records the previous 14-bone package and earlier integration stages. Its statements that v1 is not playable are historical, not the current launcher state.

## Current priority

**Latest source pickup:** Claude has now delivered v1 through `c5d6d25`; all source commits are integrated and the mesh, nine clips and baked textures are imported side by side under `/Game/Characters/Chuck/V1`. Read `CHUCK-V1-INTEGRATION.md` for verified source/import results and reproduction. This supersedes the earlier “awaiting source delivery” status below. The normal playable build still uses the old runtime; AnimInstance/AnimBP and contact migration is the next gate before switching it.

The user's resource guide has been copied unchanged with its SHA-256 provenance. Read `RESOURCE-GUIDE-NOTES.md` alongside it: reuse existing art/animation where practical, verify current resource terms/hardware fit, preserve the user's reference target and keep this character-first scope.

**Rig proposal reviewed and accepted by Codex.** Read `docs/RIG-CONTRACT-V1.md` before changing bones or starting new runtime animation. Claude's proposal files from `51ff219` were recorded separately; subsequent art/rig deliveries are now integrated as source. The current playable package remains the verified 9b9bd46 integration described below. V1 clips run in source/import review, but no new Animation Blueprint or game features are running in the playable build yet.

Codex answered all five rig questions: SkeletalMesh + AnimInstance/AnimBP Two Bone IK, no Control Rig dependency now; accepted 41-bone hierarchy/rest coordinates; preserve and verify axes/helpers on import; derive new reach/poles from rest pose; fit sole markers and a real cigarette-tip attachment. Claude followed this agreement in its delivered v1 rig/skin/clips. Its worktrees were not changed.

Source contract validation passed with `Tools/Check-RigContract.py --output Local/rig-v1-review.json`: 41 bones / 35 deforming, script/JSON match, valid hierarchy, mirrored forward knee poles, upper/lower leg lengths 8.7687/9.3670 cm. Subsequent source QA passed 66 checks, and Unreal import validation confirmed rest positions, clip motion and repaired loop timing. Editor captures now explicitly refresh and validate the applied poses; walking and landing images were visually inspected. A small C++ editor-review helper was added and built; gameplay/controller code is unchanged. Packaged v1 controller/deformation verification remains outstanding.

**Character first. World/setting work is paused.** The user says Chuck remains far from the supplied goal: primitive appearance, cartoony movement, unnatural leg placement and unintended gaps along the open jacket/zipper edges. The latest checks do not establish acceptable character quality. Read CHARACTER-PLAN.md before more implementation and AGENT-WORKFLOW.md for the prepared Codex/Claude Code split.

The eventual rig must support walking, running, rolling, side-jumping, climbing and smoking with a cigarette kept in the mouth. These are design requirements, not a claim that all actions are playable. Keep Chuck silent, restrained and about 65 cm tall; oversized open purple jacket, gray-brown rat anatomy. The supplied reference images take precedence over the current procedural study.

## What runs

Current character integration: Claude source deliveries 9e337c3/939580a integrated as 483bbaf/bc7d711, plus Codex contact/stride/turn-recovery code through 3bcbc71. Public remote: https://github.com/RtRutabaga/CHUCK-3D. Double-click Launch-Prototype.cmd for the existing local Windows package. See PLAYTEST.md for controls and the same-route camera comparison. Unreal 5.7.4 CL 51494982, Blender 4.5.14 LTS 62c1db4208e8; hardware/toolchain details in SETUP.md. No new engine, Blender or development dependency was installed in these graphics passes.

One runtime-generated Waterdeep dock scene: quay, pier with a jumpable missing board, closed tavern frontage, stationary 180 cm worker, barrel, crate, bench and low step. Custom Blender prop/worker/boat meshes replace visible blockout shapes; original hidden collision preserves the route. No new map, interior, conversation, combat or pickup system. The editor level is intentionally empty until Play constructs it.

Chuck uses a Blender body with a 14-bone authored rig plus any imported armature root. Procedural leg IK, sleeve motion, tail sway and independent feet remain a prototype. Feet now alternate world-locked stance and short predictive swings with ground traces on the existing static route. Body source: 159,524 triangles; reusable foot: 10,944 triangles. Directional geometric fur covers portions of head, chest, belly and legs. Claude rebuilt continuous jacket fronts/collar/lapels, lining and zipper edges, then graded sleeve weights through the elbow. Visible shoulder/armhole joins and simplified anatomy still need work. Runtime now preserves Skin and Claw foot material slots. No production groom, cloth simulation, authored motion clips, general slope/stair/platform support or LODs.

Twenty-two art materials include original procedural character/world surfaces and CC0 Poly Haven stone/timber maps with recorded source hashes/licenses. DX11 SM5 uses screen-space reflections, 35 cm AO and 8 cm contact shadows; no Lumen/ray tracing/motion blur. Water is opaque and shoreline/reflection quality remains limited. SourceAssets readmes and GRAPHICS-PASS.md describe reproduction and limits. Do not expand these world studies while character work is the priority.

## Cameras and controls

Both cameras remain, with no selection: elevated boom 400 cm, pitch -48 degrees, FOV 65; rat-height boom 220 cm, lens about 65 cm high, FOV 78. Switching/orbit blends and positional lag is capped at 8 cm. Camera collision hides Chuck within 70 cm of the lens; foreground obstruction and comfort still need user feedback.

WASD/arrows/left stick walk; Space/A jump; C/Y switch camera; mouse/QE/right stick turn; F/right-stick click recenter; R/View reset; Esc/Menu exit. Mouse/right-stick Y adjusts rat-height pitch. See PLAYTEST.md for exact launch and verification commands.

## Verified latest gameplay milestone

- Claude's committed Blender source passed check_model.py: 65 cm bounds, 14 named bones/parents, material slots, normalized weights, closed jacket shell and graded sleeve weights. Log: Local/claude-review-check.log. Source FBXs were imported directly; no generator rerun overwrote the accepted art.
- Windows Development BuildCookRun completed successfully: Local/character-integration-final-build.log. No C++ warning/error or material compile/invalid shader-map failure was found in that log.
- Tools/Verify-Package.ps1 -MotionCapture passed **43 rendered checks**, zero failures: Local/verify-package-20260926-130525.log. The wrapper requires the success marker, not just exit code. Without captures it expects 42 checks; physical Xbox hardware remains unverified.
- Baseline flat-route stance-foot slip: mean 70.5968 cm/s, maximum 100.0720 cm/s across 43 samples. Final: mean/max 0.0000 cm/s across 51 samples; maximum ankle-target reach excess 0.0000 cm. These are measurements on the scripted straight walk, not a guarantee for every terrain or input sequence.
- Motion review caught long-stride ankle separation, then sharp-turn unreachable targets. Shortened steps, bounded pelvis lowering and release/recovery of unreachable contacts address those specific faults. Two intermediate reach runs failed; the threshold was retained. Abrupt turn recovery can still shuffle and is not finished authored animation.
- Rear/front/side rat-height and elevated sequences cover start, walk, 90-degree turn, stop, jump and landing. PNGs: Builds/Windows/Chuck3D/Saved/Screenshots/Windows/Motion/View0..3. Local/MotionReview contains GIFs and sampled-frame contact sheets. The verifier clears only previous generated frame PNGs before a new motion run so stale frames cannot mix into evidence.
- Loaded art/scale/bones/materials, walk/jump/landing, wall/prop collision, reset, simulated keyboard/Xbox mappings, camera transitions/recentering and the pier gap in both views remain covered. Physical controller, sustained performance, arbitrary terrain and subjective camera comfort are not established.
- Visible shoulder/armhole joins, primitive head/hands/limbs, rigid foot/toe behavior and segmented leg surfaces remain defects. The character is still far from the supplied reference quality. World work stays paused.
- Local Git LFS fsck passed. Generated Unreal output, local motion evidence/worktrees and Blender backups are excluded. A new independent remote asset download was not performed in this integration.

## Two-agent preparation

CLAUDE.md imports shared AGENTS.md. CHARACTER-PLAN.md records defects, action requirements and review evidence. AGENT-WORKFLOW.md defines separate branches/worktrees, file ownership, initial rig contract, handoffs and integration. Ready-to-paste assignments are in docs/agent-tasks. Tools/New-AgentWorktrees.ps1 creates/reuses ignored Local/AgentWorktrees/codex-movement and claude-character without resetting existing work. The initial roles are Codex movement/runtime and Claude Blender character forms/jacket; the main integration owner imports accepted assets, verifies and publishes.

Claude has now delivered two source commits from its desktop-managed worktree. Both are integrated; its working branch has been left intact. The initial installation notes in AGENT-WORKFLOW.md are historical preparation guidance. No dependency installation or login was performed by Codex during this integration. Run only one Unreal build/editor/import at a time on this 16 GB machine. Keep binary ownership explicit and don't run obsolete generators over manual art changes.

## Repository boundaries and publication

The original CHUCK-game is strictly read-only. Reference HEAD: 87585dd6efb3d9fb0a44dd33549fa521f17b6701. Only selected historical documents/excerpts were copied; References/PROVENANCE.md records origins/hashes. No original code/assets were copied. Missing geography/player-progression supplements were not found; do not invent them.

CHUCK-3D has a separate public remote and no deployment. Standing user permission covers future verified project code/docs/assets commits and pushes, including supplied JPG references and Blender/FBX/Unreal assets. Do not ask again for routine publication. Paid services, sensitive data, destructive changes and the original game are outside that authorization. All authored binary types use LFS; remaining remote account allowance is unknown. No paid storage was purchased. Builds, Local evidence/worktrees, caches, Binaries, Intermediate, Saved and Blender backups remain ignored.

The user previously requested sustained graphics work, then specifically directed finishing/publishing the in-flight tavern pass and preparing this two-agent workflow. Do not resume world work or launch both agents automatically from that earlier request. Next implementation is character-first under the prepared assignments.

Codex movement was developed and committed in its prepared worktree and cherry-picked onto main for packaging with shared caches. Claude owns source art; Codex owns runtime; integration owns Unreal imports. Do not reset Claude's checkout or regenerate its accepted source with an older generator. See docs/agent-handoffs/CODEX.md and CLAUDE.md for specific deliverables and remaining work.
