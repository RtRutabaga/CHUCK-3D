# Agent handoff — Claude Code

Per-session log, newest first. Template: [TEMPLATE.md](TEMPLATE.md). Prototype-era entries (character, rig, traversal, camera, sewer, NPCs) are in [../archive/agent-handoffs/CLAUDE-PROTOTYPE.md](../archive/agent-handoffs/CLAUDE-PROTOTYPE.md). October 2026 is in [../archive/agent-handoffs/CLAUDE-2026-10.md](../archive/agent-handoffs/CLAUDE-2026-10.md).

## November 24 (fourth) — Feywild: the Crossroads trio was not in the package (Claude)

- **Asked:** the user walked the whole ring path round the Crossroads tree in the launcher build and found no trio.
- **Cause:** `/Game/Characters/FeyTrio` was never in `DirectoriesToAlwaysCook`. `AFeyTrio` loads its meshes and clips
  by path, so nothing referenced them and the cook left them out.
  - Every package since November 23 had no trio. The player's log has `CHUCK_FEY_TRIO_MISSING ... mesh=0 clip=0`.
  - The stills were taken in the editor, and no verify step builds the Crossroads life.
- **Source:** `main` at cfc3bcbf; this commit. The package's receipt names cfc3bcb.
- **Changed:**
  - `Config/DefaultGame.ini`: the FeyTrio folder is always cooked.
  - `FeyTrio.h/.cpp`: new `AFeyTrio::CountMissing()`.
  - `FeyMap.cpp`: adds that count to the Feywild map's `missing`, which the feyMap verify step requires to be 0. A
    package without them now fails verification.
- **Contract:** unchanged.
- **Validation (actual):**
  - Package FeyTrioCookCandidate: 50/50 steps, 182 passes, no retries
    (`Local/verify-package-20261124-211943.log`). The feyMap step's `CHUCK_FEY_MAP_BUILT missing=0` includes the trio.
  - Shot list `feywild-trio.txt` run in the packaged candidate: `CHUCK_FEY_TRIO_SPAWNED parts=3 grooms=9`; stills in
    `Local/shots/feywild-trio-packaged`.
  - Promoted as **FeywildTrioCook**; the candidate folder is removed.
- **Known flaws:** as in the third entry below.
- **Lesson:** a new `/Game/...` folder whose assets are only loaded by path needs a cook entry. When the user cannot
  find something, read their own log first (`Builds/Windows/Chuck3D/Saved/Logs`).

Next part of the work can be done here (the trio's dialogue once the user supplies it, or further trio polish).

## November 24 (third) — Feywild: the Crossroads trio, third round (Claude)

- **Asked:**
  - The satyr's arms not through the centaur's clothes or body.
  - A leather helmet like Achilles' in Troy, without plume.
  - A horse less long, the forelegs without their narrow part.
  - Travellers' gear behind the satyr.
  - Then, from the user's stills: normal horse forelegs (still narrow at the top); matte goat hooves; normal satyr hips
    and legs.
- **Source:** `main` at 0a863857; this commit. The package was built from the working tree on 0a86385, so its receipt
  names 0a86385.
- **Changed:** see `docs/regions/FEYWILD.md`, "The Crossroads trio, third round (2026-11-24)".
  - `Tools/build_fey_trio.py`:
    - The horse is shortened behind the hips (`XS` 0.76), with lofted forelegs (`FORELEG`) and a chest keel.
    - The leather `troy_helmet` comes from the kit's hook.
    - The `traveller_gear` load (bedroll, pack, saddlebags, pot, lantern, waterskin, crook).
    - The satyr gets cloven matte hooves (`goat_hoof`), furred hips and fur along the limb.
  - `Tools/build_fey_trio_anims.py`:
    - Each leg's IK pole is fitted at rest (`CHUCK_TRIO_LEG_POLE`).
    - The satyr's hands are flat on the centaur's shoulders, carried by `spine_03`.
    - The satyr's leg scores hang the hoof under the knee.
    - `CHUCK_TRIO_CLEAR` reports the arms.
  - `SourceAssets/NPCs/Fey/trio.json`: the kit helmet is removed.
  - `FeyTrio.h`: Lift is 42.6.
  - Regenerated the trio's SK/AS/GR sources and `/Game/Characters/FeyTrio` assets. There are new `MI_FeyCentaur_Hero_*` MIs and
    `MI_FeySatyr_Hero_goat_hoof`; `MI_FeyCentaur_Helmet` and `MI_FeyCentaur_Trim` are removed.
- **Contract:** Chuck unchanged. The trio's skeletons are rebuilt; manifest `lift_cm` 42.6 matches `FeyTrio.h`. Objects
  named `helmet*` are groom covers, as before.
- **Validation (actual):**
  - **Blender:**
    - `CHUCK_TRIO_LEG_POLE`: front 90°, hind -80° and -100°, twist under 1.5°.
    - `CHUCK_TRIO_CLEAR`:
      - Satyr into horse: 10 verts, deepest 2.2 cm (it was 441, 5.0 cm).
      - Satyr arms into centaur: about 90 of 4246, deepest 1.7 cm.
      - Gnome into satyr: about 400–500 verts, deepest 7.4–8.4 cm (unchanged).
  - **In-game stills:** `Local/shots/feywild-trio`, `Local/shots/feywild-trio-walk`.
  - **Package FeyTrio3Candidate:**
    - The first full verify failed one step. Phlegethos `lake_slabs` died after the second wall kick, both on the run
      and on its retry (`Local/verify-phlegethos-20261124-164850.log`, `-174118.log`). The temple heroes passed on
      their retry.
    - The same step alone passed on the launcher package (`-175245.log`) and on the candidate (`-180350.log`).
    - A second full run on the same candidate passed 50/50 steps, 182 passes, no retries
      (`Local/verify-package-20261124-181409.log`).
    - Promoted as **FeywildTrio3**; the candidate folder is removed.
    - Not played by hand.
- **Known flaws:**
  - The penetration above.
  - The chest seen from the front is a smooth rounded mass.
  - `lake_slabs` failed twice late in one long run and passed in every other run. The cause was not found: watch it, and
    fix it at its source if it comes back.
  - The trio has no lines yet.

Next part of the work can be done here (the trio's dialogue once the user supplies it, or further trio polish).

## November 24 (second) — Feywild: the Crossroads trio, second round (Claude)

- **Asked:** the gnome's legs over the satyr's shoulders in a normal piggyback; the satyr's legs round the horse's body
  without cutting into it; a proper horse's body rather than drawn shapes; whimsical satyr clothes; a vest on the centaur.
- **Source:** `main` at 1cf0df4a; this commit. The package was built from the working tree on 1cf0df4, so its receipt
  names 1cf0df4.
- **Changed:** see `docs/regions/FEYWILD.md`, "The Crossroads trio, second round (2026-11-24)".
  - `Tools/build_fey_trio.py` (horse re-sculpted as lofted trunk and hind thighs with muscles, bay with black points; ram's
    curls; satyr wreath and panpipes), `Tools/build_fey_trio_anims.py` (riders' legs solved by a clearance search on the
    body under them; `CHUCK_TRIO_CLEAR` report), `Tools/build_npc_humans.py` (new `vest` piece; hero fabrics usable),
    `SourceAssets/NPCs/Fey/trio.json` (vests, neckerchief, sash), `FeyTrio.h` (Lift 45.3).
  - Regenerated: the trio's SK/AS/GR sources and their `/Game/Characters/FeyTrio` assets.
- **Contract:** Chuck unchanged. The trio's skeletons are rebuilt (the centaur's horse bones changed); manifest `lift_cm`
  45.3 matches `FeyTrio.h`. The human kit gains a `vest` outfit piece; existing outfits are unaffected.
- **Validation (actual):**
  - Blender: satyr knee within 1.6 cm of its solved point, gnome 0.1 cm. `CHUCK_TRIO_CLEAR`: satyr into horse 441 verts
    deeper than 2 cm, deepest 5.0 cm (the pressed thigh at the seat); gnome into satyr about 600 verts, deepest 7–8 cm.
  - In-game stills: `Local/shots/feywild-trio*` (`Tools/shots/feywild-trio.txt`, `feywild-trio-walk.txt`).
  - Package FeyTrioCandidate: Verify-Package 50/50 steps, 182 passes, no retries
    (`Local/verify-package-20261124-103124.log`). Promoted as **FeywildTrio2**; the candidate folder is removed. Not
    played by hand.
- **Known flaws:**
  - The penetration above. The satyr's fur legs read dark in the evening light; the bay's points end in a hard line.
  - No free full-body horse with a usable licence was found, so the horse is still code-sculpted.
  - The trio has no lines yet; dialogue will need a stop/talk state on top of the shared loop.

Next part of the work can be done here (the trio's dialogue once the user supplies it, or any further trio polish).

## November 24 — Feywild: aiming orchids, wonder pass, hag's cottage, Crossroads trio (Claude)

- **Asked:**
  - Orchids that target Chuck like the spined devils.
  - The wonder pass (giant jellyfish, unique giant plants, the rest).
  - A hag's cottage just off the Deepwood trail.
  - The image's centaur, satyr and gnome milling by the Crossroads tree; their dialogue comes later from the user.
- **Source:** `main` at 21d8fc71; this commit. The package was built from the working tree on 21d8fc7, so its receipt names 21d8fc7.
- **Changed:** see `docs/regions/FEYWILD.md`, "Aiming orchids, the wonder pass, the hag's cottage and the Crossroads trio".
  - `FeyLife.cpp/.h` (orchids aim; the cottage's fire, light and sounds; the trio spawned), `FeyMap.cpp/.h` (wonder pieces,
    islands, jellyfish, mere mirror, cottage spot), `FeyMapTest.cpp` (step 12 aim checks), new `FeyTrio.cpp/.h`.
  - Tools: `build_feywild_kit.py`, `create_feywild_assets.py`, `Fetch-FeyHag.ps1` (Poly Haven CC0 thatch and plaster),
    `build_fey_trio.py`, `build_fey_trio_anims.py`, `import_fey_trio.py`, `import_npc_grooms.py` (`CHUCK_GROOMS_SET=fey`).
  - Triplanar materials on instanced meshes now project in the instance's frame (bark, roots, timber, stone, marble).
- **Contract:** Chuck's rig, controls and scale unchanged. The trio are three new skeletons under `/Game/Characters/FeyTrio`.
- **Validation (actual):**
  - Editor `=life` (`Local/feywonder-life2.log`): 0 failures, 8 orchid shots, 2 hits; new checks `orchid_aims_off_lane`, `orchid_turns_to_him`.
  - Editor route (`Local/feywonder-route.log`): 1 failure, a stuck at the Lily Mere moat (leg LilyMere_3_4) after one missed pad hop, the known walker flake; the nearest new piece is about 50 m away. `missing=0`; the trio spawned with 3 parts and 9 grooms.
  - Stills: `Tools/shots/feywild-wonders*.txt`, `feywild-arches.txt`, `feywild-hag.txt`, `feywild-trio*.txt`.
  - Package FeyWonderCandidate: Verify-Package 50/50 steps, 182 passes (`Local/verify-package-20261124-074138.log`; sound-Chult passed on its retry). Promoted as **FeywildWonders**; the candidate folder is removed. Not played by hand.
- **Known flaws:**
  - The trio: contact by IK; the satyr's legs cut into the horse's body, the gnome sits rather than riding piggyback, and the horse is a smooth stylised body. The user has asked for all of this next, with clothes for the satyr and a vest for the centaur.
  - The cauldron and chimney flames and smoke did not show in stills.
  - Satyr hair has 65k strands (heavy). The great bell flowers have no collision. Crows not built.
  - Downloaded JPGs and STLs are left out of Git, as before (`Tools/Fetch-FeyHag.ps1` fetches the hag textures again).

Next part of the work can be done here (the trio improvement the user asked for: piggyback gnome, the satyr's legs round the horse, a real horse body, satyr clothes, centaur vest).

## November 23 — Feywild: the start's narrow cut, the maze laid from its way, marble satyrs, squeeze, decor (Claude)

- **Asked:**
  - Fix the gaps through the rock and the floating petals in the first narrow canyon after the start. This turned out to be the cut through the rise beside the arrival, not threshold 0.
  - Put hazards and unusual things only on the way through the maze.
  - Frowning statues in dead ends and smiling ones on the way, as real satyrs (some detailed marble). Downloading free resources was allowed.
  - Dimmer Deepwood lamps scattered through, low fog in parts, more decor.
  - Spots narrow enough to wall-jump to the top and be knocked down by the bees.
  - Not much testing of the Cloud Staircase (it will become a cutscene).
- **Source:** `main` at dcaca93a; this commit. The package was built from the working tree on dcaca93, so its receipt names dcaca93.
- **Changed:**
  - `FeywildSetting.cpp`:
    - No damp sheen or path colour on steep faces.
    - Nothing is planted on ground steeper than 45° or within 80 cm of it. The RNG order is kept.
    - The land runs a metre under the Riverbank.
  - `FeyMap.cpp/.h`:
    - The Riverbank's land is 41 cm cells near the slice. `Put` has the same steepness check.
    - The maze is laid route-first (runs on alternate rings, then inward; no loops).
    - All parkour, three crawl gaps, the marks (smiling satyrs, turning statues, mites, cigarettes) and the squeeze are on the way. Shortcuts come out on it.
    - Frowning satyrs in about 20 dead ends.
    - Spread evenly: 285 lanterns (every fourth lit, 20 cd), plants, urns, benches, petals and 39 low fog patches.
    - The maze's veils are switched with the Deepwood's (`VeilsOf`).
  - `FeyLife.cpp`: statues, mites and cigarettes from `MazeMarks()`.
  - `FeyMapTest.cpp`:
    - Steps 55 and 60 to 63 adapted to the way.
    - Step 64: squeeze climb and bee knock.
    - Step 65: stills.
    - The route walker comes close at gaps.
  - New tools:
    - `Tools/Fetch-FeySatyr.ps1`: SMK KAS460, KAS350 and KAS208 STLs (Public Domain Mark); Poly Haven marble_01 2K (CC0). The STLs are not in Git.
    - `Tools/build_fey_satyr.py`: Blender 4.5.14 makes the laughing and scowling meshes (70k tris, 141.5 cm).
    - `Tools/import_fey_satyr.py`: `M_FeyMarble`, both meshes, 4 LODs.
  - Assets and shot lists:
    - `Content/Art/Feywild/Satyr/`.
    - `SourceAssets/Props/Feywild/Satyr/` (FBX, review renders, manifest).
    - `Tools/shots/feywild-startcut.txt` (and canyon1/start lists from the wrong first look).
  - `docs/regions/FEYWILD.md`: the third-round section.
- **Tested:**
  - `=life` from step 50 (`Local/feymaze-18.log`): 0 failures.
    - Marks 17, gaps 3, shortcuts 3, squeezes 1.
    - Squeeze: 849 cm reached in 17 wall jumps; 1 bee knock.
  - Route (`Local/feymaze-route4.log`): 0 failures, 0 stuck.
    - Maze way 460 m (was 195 m).
    - Worst threshold frame 53.1 ms; maze build parts at most 9.2 ms.
    - The satyr import overlapped an earlier route run, which was discarded.
  - Start-cut stills (`Local/shots/feywild-startcut/`): no streaks, petals on the floor.
  - Satyr stills looked at: both face their way in.
- **Package:** `FeyMaze3Candidate` verified: `CHUCK_VERIFY_SUMMARY steps=50 failed=0 retried=0 partial=0`, `CHUCK_PACKAGE_VERIFIED passes=182 log=Local/verify-package-20261123-195342.log`.
- **Not tested:**
  - By hand.
  - Other squeezes (only one fits).
  - The hit-plant shortcuts walked.
  - The fog's cost on a slow machine.
- **Remaining flaws:**
  - The scowl has small creases at the mouth corners. Smile and frown read only within a few metres (the head is 1.8 m up).
  - The satyr is one pose.
  - The marble reads dark blue-grey under the maze's light.
  - The way is more than twice as long.
  - The walker needs straight points through gaps.
- **Contract compatibility:** unchanged. Chuck's rig, scale, moves and controls are as they were; the squeeze uses the existing chimney wall jump.
- **Launcher:** promoted as FeywildMaze3 (`CHUCK_PROMOTED FeywildMaze3 previous=Windows-Previous-20261123-FeywildMaze3 kept=2`). The candidate folder was removed.

Next part of the work can be done here (more satyr poses, brighter marble under the lanterns, or the tower once it is planned; the tower needs the user's plan first).

## November 23 — Feywild: parkour on the way through, walkway timber, modelled bees (Claude)

- **Asked:** "proceed" (after the parkour entry below, whose next-owner line offered modelled bees, a parkour piece on the way through and the walkway's material).
- **Source:** `main` at 55335eb8; this commit. The package was built from the working tree on 55335eb, so its receipt names 55335eb.
- **Changed:**
  - `FeyMap.cpp/.h`: `FMazeParkour::NHoop`. One two-cell run of the way itself gets three hoops over a 1.8 m bed, only where the way enters before the first hoop and leaves after the last; the other keeps a short bed. The walkway, ladder and steps are on `M_FeyPlank`; the steps are boards (treads, risers, stringers) with collision boxes (`Plan.Walls`).
  - `Tools/create_feywild_assets.py`: `M_FeyPlank` (the bark shader on `WeatheredTimber/T_rough_wood`, moss only from vertex R and in big patches, lighter, faint veins). Built alone with `CHUCK_FEY_ONLY=M_FeyPlank` (`Local/feyplank-mat.log`); `Content/Art/Feywild/M_FeyPlank.uasset` added.
  - `FeyLife.cpp`: `Blob` and `Bee`. Each bee is a head, a thorax, three abdomen bands and two beating wings, facing along its flight.
  - `FeyMapTest.cpp`: `=life` step 63 (the way's hoops; `way_hoops_*` checks; still `Life_MazeWayHoops`). Step 60 now takes the off-way hoops.
  - `docs/regions/FEYWILD.md`: the second round.
- **Tested:**
  - `=life` from step 50: `Local/feypk-3.log` failed one check, because the way's hoops took both short beds (no jump left to test). Capped at one; then `Local/feypk-4.log` passed with 0 failures. Plan: way_hoops 1, jumps 1, off-way trellis, hoops and walkways 2 each. Way hoops: 3 catches, past the bed, 0 pricks. The other pieces are as before.
  - Route (`Local/feypk-route2.log`): failures 0, stuck 0, void 0. Maze 16.68 ms, Deepwood 17.93 ms. The worst threshold frame was 43.7 ms.
  - Stills `Life_MazeWalkway`, `Life_MazeBees` and `Life_MazeWayHoops` were looked at: grey timber with board seams; striped, winged bees.
- **Package:** `FeyParkour2Candidate` packaged and verified: `CHUCK_VERIFY_SUMMARY steps=50 failed=0 retried=0 partial=0`, `CHUCK_PACKAGE_VERIFIED passes=182 log=Local/verify-package-20261123-115640.log`.
- **Not tested:**
  - By hand.
  - Walking up the new steps from the floor (the test only comes down them).
  - The bees' cost on a slow machine.
- **Remaining flaws:**
  - Only the three hoops are on the way through.
  - The plank grain is world-aligned, not per board.
  - The bees are rebuilt as geometry every frame they show.
  - The brambles are thin.
  - The ivy is cards.
  - The trellis can block the camera.
- **Contract compatibility:** unchanged. The rig, scale, moves and controls are as they were.
- **Launcher:** promoted as FeywildMazeParkour2 (`CHUCK_PROMOTED FeywildMazeParkour2 previous=Windows-Previous-20261123-FeywildMazeParkour2 kept=2`). The candidate folder was removed.

Next part of the work can be done here (thicker brambles and real ivy leaves, or the tower once it is planned; the tower needs the user's plan first).

## November 23 — Feywild: the maze's parkour and thorn hazards (Claude)

- **Asked:** "Do the parkour and thorn hazards" (the Great Hedge Maze's parkour from `docs/regions/FEYWILD.md`: trellis wall runs, ivy brachiation, raised walkways, root gaps, plus thorns).
- **Source:** `main` at 89013771; this commit. The package was built from the working tree on 8901377, so its receipt names 8901377.
- **Changed:**
  - `FeyMap.cpp/.h`: where the parkour goes (`FMazeParkour`, `MazeParkour()`, `MazeParkourAt`). Two short bramble beds lie on straight runs of the way through, to jump. The way winds, so two trellises, two hoop runs and two walkways stand on straight runs off it, longest first, away from the orchids' lanes, the redcaps, the gate, the heart and the shortcuts; where a run ends in a dead end it is marked for a reward. Six root gaps (`FMazeGap`, `MazeGaps()`) at the foot of unlinked hedges saving 4 to 13 cells, their hedges split around a 46 cm hole with a lintel. The dressing: bramble canes with thorns, leaves and berries; the trellis (posts, rails, laths, ivy) with a flat collision face; five willow hoops with ivy; the walkway (planks, stringers, posts, a ladder, four steps down); a root slab and arching roots over each gap. `RegisterMazeParkour` (district stage 2) registers the hoop grips, the walkway ladders and the gaps as crawl spaces. `InMazeThorns`.
  - `FeyLife.cpp/.h`: on the ground in a bed he walks at 45 % speed and loses a Sanity point at most every 1.6 s, with a stumble back (`ThornPricks`, `CHUCK_FEY_THORN_PRICK`); three cigarettes past each rewarded piece.
  - `FeyMapTest.cpp`: `=life` steps 58 to 62 (a short bed slows and pricks; trellis run; hoops; walkway; root gap) with stills.
  - `docs/regions/FEYWILD.md`: the section "The maze's parkour and thorns".
- **Tested:** `=life` from step 50 in the editor (`Local/feypk-2.log`): failures 0. Plan: trellis 2, hoops 2, walkways 2, jumps 2 (6 of them off the way); registered grips 10, ladders 2, gaps 6. Thorns: 2 pricks crossing at 51 cm/s. Trellis: jump at s=-137, wall run, lands at 212 (bed ends 90), 0 pricks. Hoops: 5 catches, lands at 449 (bed ends 225), 0 pricks. Walkway: feet 160 cm up, ends at 360 (bed ends 220), 0 pricks. Gap: crawled through. The first run (`Local/feypk-1.log`) built only the two short beds (the way through has no runs long enough), so 3 checks failed: fixed by placing the rest off the way. Route walk (`Local/feypk-route.log`): failures 0, stuck 0, void 0; maze 16.7 ms, Deepwood 17.9 ms; no build part new or slower (maze parts 8 to 9.6 ms), but the worst threshold frame was 59.5 ms (47.1 on the previous run; the same unexplained crossing spike). Stills `Life_MazeThorns`, `Life_MazeTrellisRun`, `Life_MazeHoops`, `Life_MazeWalkway`, `Life_MazeRootGap` in `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/FeyMap/`.
- **Package:** `FeyParkourCandidate` (8901377 + these changes) packaged and verified: `CHUCK_VERIFY_SUMMARY steps=50 failed=0 retried=0 partial=0`, `CHUCK_PACKAGE_VERIFIED passes=182 log=Local/verify-package-20261123-074514.log`.
- **Not tested:** by hand; the thorn sound by ear; the cigarettes picked up; the second trellis, hoop run and walkway (only the first of each kind is driven); gaps other than the first; redcaps or mites near the parkour.
- **Remaining flaws:** the walkway's planks and steps read as mossy green blocks (the bark material mosses every upward face); brambles are thin and the thorns small at a distance; ivy leaves are simple cards; the trellis hides part of the view when the camera is beside it; the main parkour is off the way through, not on it; the threshold spike (now about 59.5 ms) is still not found; the November 23 stages 6 to 8 flaws below otherwise stand (modelled bees, statues' snap turn, long stair treads).
- **Contract compatibility:** Chuck's rig, scale, moves and controls unchanged; the parkour uses his existing wall run, swing grips, ladder and crawl spaces.
- **Launcher:** promoted with `Tools/Promote-Build.ps1` as FeywildMazeParkour (`CHUCK_PROMOTED FeywildMazeParkour previous=Windows-Previous-20261123-FeywildMazeParkour kept=2`); `Launch-Chuck.cmd` runs it. Candidate folder removed.

Next part of the work can be done here (modelled bees, a parkour piece on the way through itself, the walkway's material, or the tower once it is planned; the tower needs the user's plan first).

## November 23 — Feywild stages 6 to 8: the Great Hedge Maze, the Cloud Staircase to its landing, polish (Claude)

- **Asked:** "When you finish this go ahead and do the next 3 stages" (after the November 22 overcast/fur/sounds package, promoted as FeywildStage5b first).
- **Source:** `main` at 8f4152ae; this commit. The November 22 work (entry below) was promoted from the working tree before it was committed, and is committed here together with stages 6 to 8.
- **Changed:**
  - `FeyMap.cpp/.h`: twelve maze shortcuts chosen where an opening saves the most walking (`FMazeShortcut`, `MazeShortcuts()`), their hedges left to `AFeyLife`; exports of the maze's dead ends, lanes, way, rings and heart angle; the stair: 56 walked steps from 11 to 22 m out, then a landing 19 m up (R 24 m, 15 m long), each tread on a slab (not a column through the maze), invisible rails from step 30, the boundary wall at the landing's end, the spot "Feywild Cloud Landing"; the maze's hedges in 48 parts (was 8; each cooked in about 45 ms); `CHUCK_FEY_SLOW_PART` logs any build part over 8 ms.
  - `FeyLife.cpp/.h`: the heart's vine gate and its hit plant; hidden hit plants revealed for the lost (after 4 min, then every 2.5 min) whose scratch sinks or raises a shortcut hedge (never over him; closed again on respawn); the bees on the hedge tops (warning buzz, then a knock back down to the nearest corridor, no Sanity); statues that turn while unseen; mites, redcaps, orchids and moths in the maze; everything after the first part built over later frames (within about 4 ms a frame).
  - `FeyCreatures.cpp`: the mites lightened (a tint on their own material instances).
  - Sounds: bumblebees 1000/1001 added to `Tools/Fetch-FeySounds.ps1`; `SFX_FeyBeeLoop_00` from `Tools/build_fey_sfx.py`; `Tools/import_fey_sfx.py` takes `CHUCK_FEY_SFX_ONLY` so the others are not reimported.
  - `FeyMapTest.cpp`: `=life` steps 50 to 57 (reveals, a shortcut opened and walked through, the heart gate, bees, a statue, the landing, the boundary, the climb); `CHUCK_FEY_LIFE_FROM` starts at a step (developer use only).
  - `docs/regions/FEYWILD.md`: stages 6 to 8 as built.
- **Tested:** `=life` in the editor, full (`Local/feylife6-4.log`): failures 0 across its 33 distinct checks, the new ones included. Maze steps alone after each change (`Local/feylife6-3.log`, `-5.log`, `-6.log`): 0 failures. The first full run (`Local/feylife6-1.log`) failed 8 checks: the test switched plants mid-check (fixed in the test), the boundary wall stood 1.5 m past the landing so he fell through the gap (fixed in the map), and the climb test spent its time walking round the heart (fixed in the test). Route walk (`Local/feystage6-route2.log`): failures 0, stuck 0, void 0, all spots; Deepwood 17.92 ms, the rest at the cap; the maze's worst build step 9.2 ms (48 before); worst threshold frame 47.1 ms (55.1 before). Stills `Life_MazeShortcut`, `Life_MazeHeartGate`, `Life_MazeBees`, `Life_CloudLanding` in `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/FeyMap/`.
- **Package:** `FeyStage6Candidate` (8f4152a + the November 22 and 23 changes) packaged and verified: `CHUCK_VERIFY_SUMMARY steps=50 failed=0 retried=0`, `CHUCK_PACKAGE_VERIFIED passes=182 log=Local/verify-package-20261123-015223.log`.
- **Not tested:** by hand; the bee sound by ear; the full 4-minute wait for the first plant at its real timing (the test shortens it); redcaps and mites inside the maze against him; how long the stair feels to climb.
- **Remaining flaws:** the bees are gold specks, not modelled; the statues turn in one snap while unseen; the maze's parkour (trellis wall runs, ivy brachiation, raised walkways, root gaps) and thorn hazards are not built; the stair's treads are long (about 4.3 m), so the climb is a long walk; one threshold frame still spikes to about 47 ms, outside the build steps (not found); the November 22 flaws below stand (the flowers, orchids, moths and pads now have sounds).
- **Contract compatibility:** Chuck's rig, scale and controls unchanged. Respawn spots gain "Feywild Cloud Landing".
- **Launcher:** promoted with `Tools/Promote-Build.ps1` as FeywildStages6to8 (`CHUCK_PROMOTED FeywildStages6to8 previous=Windows-Previous-20261123-FeywildStages6to8 kept=2`); `Launch-Chuck.cmd` runs it. Candidate folder removed.

Next part of the work can be done here (the maze's parkour and thorn hazards, modelled bees, or the tower once it is planned; the tower needs the user's plan first).

## November 22 — Feywild: the Deepwood's blue lanterns and overcast, stage 5 part two (the creatures), their fur and sounds (Claude)

- **Asked:** "add our blue lamps from reference image to deep woods, have them put out deep blue lighting like the mushrooms and decrease both sky light here and ground orb light ... resulting in more magical blue lighting. Then, do the next stage" (stage 5 part two: thorn mites, redcaps, the displacer beast, the flowers that raise lily pads). Then: "Make the deep wood area overcast ... adjust lighting ... Also free resources to make displaced beast look better. Download and implement sounds for things that need sounds".
- **Source:** `main` at 8f4152ae; this commit.
- **Changed:**
  - `FeyMap.cpp`: Deepwood lanterns along its paths (crooked post, hooked arm, hanging iron lantern of blue glass on `M_FeyPlant`, a deep blue unshadowed light each, up to 80); floor lights fewer (30 %) and fainter (8-14 cd); the caps' cluster lights a deeper blue and a little stronger; `M_FeyPlant` added to the district materials. `FeywildSetting.cpp`: in the gloom the sky light to 36 % (was 62 %), the fill to 80 % (was 110 %), the sun to 20 %.
  - Overcast: `Tools/create_feywild_assets.py` `M_FeySky` `Overcast` parameter (rebuilt alone with `CHUCK_FEY_ONLY=M_FeySky`); `FeywildSetting.cpp/.h` drives it from the gloom (sky MID kept), sun 5 %, sky light 56 %, exposure bias -.18.
  - Sounds: `Tools/Fetch-FeySounds.ps1` (27 BigSoundBank CC0 MP3s, 7 MB, `SourceAssets/Audio/SFX/Feywild/Source/`), `Tools/build_fey_sfx.py` (27 `SFX_Fey*.wav`), `Tools/import_fey_sfx.py`; `FeyCreatures.cpp` sound sets with takes, `FeyLife.cpp/.h` one-shots and loops (`UAudioComponent`), the redcaps' cackle in `TempleSkeleton.cpp`.
  - Fur: `Tools/Fetch-FeyFur.ps1` (ambientCG Fabric084 2K, CC0, `SourceAssets/Enemies/Feywild/Fur/`), `M_FeyFur` built by `Tools/import_fey_creatures.py`; `AFeyDisplacer` gives every part a MID and drives `Shimmer` as its image slips (now smooth).
  - `Tools/build_fey_creatures.py` (new, Blender 4.5.14, reuses `build_phleg_creatures.py`'s SDF kit): `SK_Redcap` on the temple skeleton's rig, `SM_Displacer{Body,Head,Upper,Lower,Tail,Tentacle,Pad}`, `SM_ThornMite{Body,Leg}` -> `SourceAssets/Enemies/Feywild/`. `Tools/import_fey_creatures.py` (new) -> `/Game/Art/Feywild/Creatures/` (instances of `M_PhlegCreature`).
  - `FeyCreatures.h/.cpp` (new): `AFeyMite`, `AFeyDisplacer` (two-bone legs in tripods, tentacle chains, displaced image, refuges), `FeyCreaturesScratch`. `TempleSkeleton.h/.cpp`: `Post.Kind` 3, the redcap (scale .55, speed 112, notice 950, 5 scratches, iron-boot steps, Fey frame, `AFeyLife::bCalm`). `FeyLife.h/.cpp`: places, resets and drops them; the redcaps' camp (fire on `AChuckFlames`, firelight, tripod, cauldron, lean-tos with red cloth) and the hollow behind it (boulders, a root gate 82 cm high, a blocker over it, cigarettes); four hollow logs in Highmeadow and pollen on its road; the pad-raising flower and islet at the Lily Mere. `FeyMapTest.cpp`: `=life` gains mites, pads and islet, redcaps (chase, kept out, die), beast (notices, lashes, kept out, paces) and creature stills on a free camera.
- **Tested:** `=life` in the editor (`Local/feylife2-1.log`, `Local/feylife2-2.log`): failures 0 both times; mite bites 1, kills 1; pads up, he walks onto the islet and takes its cigarettes (drowned 0); a redcap chases, the nearest came 6.6 m from the hollow's middle, five scratches put one down; the beast notices, lashes (1 of 1), comes no nearer than 4.7 m to the log he is in, paces, gives up. Full route (`Local/feystage5b-route.log`, creatures not spawned there): stuck 1 at the moat's main pads after a missed hop and a drowning (the same walker flake as the 21st; 12 m from the new pads), otherwise void 0, late 0, all spots OK; Deepwood 17.93 ms with the lanterns (17.91 before), the rest at the 60 fps cap. `=life` a third time after the stills fix (`Local/feylife2-3.log`): failures 0. Stills `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/FeyMap/Life_*.png`, Deepwood lanterns `Local/shots/feywild-deepwood/`, creature review stills from Blender (scratchpad only).
- **Package:** `FeyStage5bCandidate` (8f4152a + these changes) packaged and verified: `CHUCK_VERIFY_SUMMARY steps=50 failed=0 retried=1`, `CHUCK_PACKAGE_VERIFIED passes=182 log=Local/verify-package-20261122-224220.log retried=hero` (the temple heroes step failed once and passed on retry).
- **Overcast, fur, sounds tested:** Deepwood stills under the overcast (`Local/shots/feywild-deepwood/`); `=life` five more times (`Local/feylife3-1.log` to `-5.log`), failures 0; the fur first fell back to the default material (`SP.w` on a two-component screen position: fixed), then checked in `Life_BeastSide`/`Life_BeastFront`. Sounds load (no missing objects after the redcap's swish count was fixed); none listened to.
- **Not tested:** by hand; any of the new sounds by ear; how fair the beast feels (its speed 250 cm/s against his run 225; refuges 4); redcaps grabbing and throwing him (the skeleton's grab is on for them, not tried); mites in the Deepwood's dark; the lanterns' cost on the Deepwood's frame beyond the route's figure.
- **Remaining flaws:** the creatures are first-pass art (the redcap's long human legs under its gnome proportions; the beast's glossy, untextured fur; the mite's legs only swing); the beast's image flicks across when it slips sides (no shimmer effect yet); creatures have no death or lash effects beyond sound; the redcaps stand round a fire but do not cook, talk or sit; Chuck's purple jacket reads blue under the Deepwood's light; no sounds for the flowers, orchids, moths or pads.
- **Contract compatibility:** Chuck's rig, scale and controls unchanged. The temple skeleton's code gains a kind; its other kinds unchanged.
- **Launcher:** promoted with `Tools/Promote-Build.ps1` as FeywildStage5b (`CHUCK_PROMOTED FeywildStage5b previous=Windows-Previous-20261123-FeywildStage5b kept=2`); `Launch-Chuck.cmd` runs it. Candidate folder removed.

Next part of the work can be done here (stage 6, the maze: its hit plants and the entrances that open the longer he is lost).

## November 21-22 — Feywild: canyons through ridges, tunnel mouths, Deepwood glow, the maze from above, stage 5 part one (Claude)

- **Asked:** "im still seeing portions of canyon where the area outside the canyon in visible, fix these gaps" (with two stills); make the root tunnels' outsides look like what they are (they looked like impossibly thin tubes); in the Deepwood, softer floor lights but more light overall from the blue mushrooms and glowing bits (only there); make the high view of the maze look better; then proceed with the next stage (stage 5).
- **Source:** `main` at 2f9e3419; this commit.
- **Changed:**
  - `FeyMap.cpp/.h`: ways cut through ridges (`RidgeReach`, `OuterMask`, plateau past the crest, never lower than a district's edge just outside it; none on the maze's stair; `-ChuckFeyNoRidge` for before/after checks); district rims never under 10 m; root tunnels end in their banks' face with a closed face, a root lip and arching roots (`TubeRise`, `FMapSurf::Pipe`); see-through check counts escapes, backs and side views; Deepwood floor lights softer, cluster lights over its glowing caps (`LightCols`), its fungi/plant/bark glow raised on its own instances; rounded lumpy hedges (`FMapSurf::Hedge`) for the maze and its shadow-casting stand-in; `BuildBrink` (the valley between the overlook and the maze); oaks kept out of the overlook's view; the maze's rim open toward it; `RoadOf/SpurOf/LandZ` exported; `AFeyDistrict` owns an `AFeyLife`.
  - `FeyMesh.h` (new, `FMapSurf` moved out of FeyMap.cpp). `FeyLife.h/.cpp` (new): reactive flower and vines, pollen, spitting orchids, lantern moths. `ChuckCharacter.cpp`: pollen speed scale, scratch at flowers. `FeywildSetting.cpp`: more sky light in the gloom. `FeyMapTest.cpp`: `=life` mode, canyon/tunnel/life stills, the walker with the hazards calmed. `Tools/Verify-Package.ps1`: `feyLife` step.
  - `Tools/create_feywild_assets.py`: `GlowGain`/`VeinGain` parameters (defaults unchanged), hedge tint and brightness; `M_FeyFungus`, `M_FeyFungusBig`, `M_FeyPlant`, `M_FeyBark`, `M_FeyRoot`, `M_FeyHedge` rebuilt. Shot list `Tools/shots/feywild-maze.txt`.
- **Tested:** `=cracks` with the ridge off and on (`Local/feycracks-noridge.log`, `Local/feycracks-ridge.log`): every canyon and tunnel 0 leaks with the ridge; the only remaining counts were long sight-lines over low rims (rims raised since) and over the overlook's brink at the stair (by design: the overlook's view). Stills along the first canyon both ways (`Art_Canyon_*`), the tunnel mouths, the Deepwood, the maze from the overlook (`Local/shots/feywild-maze/`). `=life` passed (`Local/feylife-3.log`): vines block, flower opens them, walks in, takes the cigarettes, pollen 225 to 120 cm/s, orchid hits, moth touches. Full route (`Local/feyfix2-route-1.log`): failures 0, stuck 0, void 0, late 0, drowned 0, all 17 spots OK; Deepwood 17.9 ms, others at the 60 fps cap; worst threshold frame 55 ms (was 28).
- **Package:** `Builds/FeyStage5Candidate` built (after fixing a namespace-scope static that read the command line before main and killed the first package with 777006). Verify-Package -RetryFailed (`Local/feystage5-package-2.log`): 48 of 50 steps passed, including feyRiver, feyMap and the new feyLife. The temple trap run (`gauntlet_flow`) and the Astral wind (`full`) failed twice. On the launcher package the same checks then went run 1, wind 0 (`Local/baseline-runwind.log`), although they passed when that package was promoted, so this is the slow-machine flake (memory note traversal-tests-fps) and not this change. Neither the pollen nor the scratch hook runs outside the Feywild map. A complete re-verification then passed (`Local/feystage5-verify-3.log`, `Local/verify-package-20261122-181625.log`): 50/50 steps, 182 passes, run and wind passing first time; only the smoke step needed its retry. Its first failure read "180 passes; expected at least 180", so the pass count was right and something else made the step fail (not investigated).
- **Not tested:** by hand; the slits in the user's stills reproduced only by eye along the canyon (the check did not flag them before the ridge, so it cannot prove they are gone); flowers in motion; the orchids' and moths' feel.
- **Remaining flaws:** the 55 ms frame when a district with life finishes loading (the flower, vines and orchids are built in one frame); the Deepwood about 1 ms over a 60 fps frame; the brink valley plain close to; the moths have no trail; no sounds yet for flowers, orchids or moths.
- **Contract compatibility:** Chuck's rig, scale and controls unchanged; his walking speed is halved in pollen.
- **Launcher:** Promoted as FeywildStage5 (receipt a7a74cd9, launch target checked); kept Windows-Previous-20261122-FeywildStage5 (the prior FeywildFixes package) and Windows-Previous-20261120-FeywildFixes; pruned FeywildArt.

Next part of the work can be done here (stage 5 part two: thorn mites, redcaps, the displacer beast, pad-raising flowers).

## November 21 — Feywild: canyon cracks, spots within districts, cloud stair, hedges, Deepwood (Claude)

- **Asked:** fix the cracks you can see through in the first canyon; more spawn points for sub-regions of the big districts; a cloud platform and cloud stair that read as cloud (no gaps between steps, no straight lines and perfect angles); better hedge leaves; the Deepwood's blue ground light a bit brighter, more fog, much better hanging moss (it read as straight-edged rectangles that shimmered); free assets allowed.
- **Source:** `main` at 9f7998aa; this commit.
- **Changed:**
  - `FeyMap.cpp/.h`: the threshold heightfield made once per threshold (`WayField`) and closed inside district rectangles (30 cm under the land) — it had left out the floor and wall feet there, so once the district behind was dropped (or before the one ahead loaded) the walls stood over holes; `FeyMap::SeeThrough` and `AFeyMap::LoadPair` for the check; spots (`FSpot`, `SpotAt/Yaw/Of`, `FeySpotWorld`, `FeyReachSpots`, `FeyForgetSpots`; respawn to the nearest reached spot in the district); the Cloud Staircase as data (`Stair()`), its walked footing covering each step's span; `FeyCloud` (icosphere puffs billowed by 3D noise, `FMapSurf::Closed`); `BuildTower` rebuilt (puff-heap cloud, one continuous stepped sweep for the stair with side and under billows, the foot as its own shadowed mesh); the hedge leaf shell instanced along every hedge (`KHedge`); the Deepwood's lights (80-130 cd, 14 m), 150 veils.
  - `FeywildSetting.cpp`: gloom fog density x6.5 (was x4.5), extinction 2.1, veil alpha .15, fill a little brighter in the gloom.
  - `DockMenu.cpp`: spots start, save and continue; a tenth column. `FeyMapTest.cpp`: `=cracks` mode, every spot checked in the full run, stills `MazeHedge`, `StairFoot`, `StairAlong`.
  - `Tools/build_feywild_kit.py`: Deepwood oaks' moss as beards of crossed tapering strand ribbons and drapes; `SM_FeyHedgeLeaves` (sprig shell, the oaks' CC0 island_tree_02 leaf atlas). `Tools/create_feywild_assets.py`: `M_FeyCloud`, `M_FeyHedgeLeaf`, new `M_FeyHedge` and `M_FeyHangMoss`. Shot lists `Tools/shots/feywild-cloud.txt`, `feywild-deepwood.txt`.
- **Tested:** `=cracks` (rays from every threshold's floor with both, near-only and far-only districts): before the fix 5,504 leaking sight-lines in the first canyon with the far district unloaded (`Local/feycracks-1.log`); after, 0 everywhere (`Local/feycracks-3.log`; run 2's remaining ~90 per rock cut were the check stepping badly through hidden bound boxes, fixed). Full editor route twice: run 1 one stuck after a missed pad hop (`Local/feyfix-route-1.log`), run 2 failures 0, stuck 0, void 0, late 0, all 17 spots OK (`Local/feyfix-route-2.log`); every district at the 60 fps cap except the Deepwood at 17.8-17.9 ms (was 16.9). Stills `Local/shots/feywild-cloud/`, `Local/shots/feywild-deepwood/`, `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/FeyMap/Art_*.png`.
- **Package:** `Tools/Build-Game.ps1 -Package -SkipBaseAssets` (`Local/feyfix-package-1.log`), verified first time: 49 steps, 182 passes, no retries (`Local/verify-package-20261120-164026.log`).
- **Not tested:** by hand; the dithered cloud edges in motion; the new spots by dying there (respawn logic only by code and the spot starts).
- **Remaining flaws:** both route runs drowned once at the first lily pad (0 the day before; the walker recovers); the Deepwood is about 1 ms over the 60 fps frame; the stair's steps close to are smooth and a little faceted; the hedge shell shows faint seams where sections meet in sunlight; corridors still dark.
- **Contract compatibility:** Chuck's rig, scale and controls unchanged. Save points gain the ten spot names; old saves still load.
- **Launcher:** `FeywildFixes` (receipt 9f7998a, built from the working tree before this commit); FeywildArt kept as previous, FeywildMap pruned; no candidate folder left.

Next part of the work can be done here (Feywild stage 5, after the user has walked these fixes).

## November 20 — Feywild: the art pass and the user's changes (Claude)

- **Asked:** "do the art pass plus these items" (canyon ends filled out, the distant vista intersecting the canyons, deep water passable only by hopping lily pads with collision, a much deeper and darker bluer Deepwood with dense rooty mossy trees, more fog and hanging moss, wooden arrow signs with nonsense glyphs leading through every element and turning silly at the end, a maze at Goblet of Fire scale for a person), then "have main sound track areas alternate between the Main track and the Feywild_ track".
- **Source:** `main` at 6581798c; this commit.
- **Changed:**
  - `FeyMap.cpp/.h`: threshold floor and masses as one heightfield from the true distance to the way (tube style: banks plus an earth roof the tube's length); distance trees kept 48 m from thresholds; the polar Prim's maze (`MazeData`, 29 rings, 3,097 cells, 8.4 m hedges, 2.6 m corridors) in eight hedge meshes plus stone, and its stand-in; the deep moat, pads with collision discs, drowning back to the last safe ground; signposts (`SignPosts`, `SignBoard`); the Deepwood's oaks, density, moss, lights and veils; the gloom by district; the overlook's knoll and brink; stone arches with runes; orchard rows as wisteria oaks; `IsPad`, `InDeepWater`, maze stats.
  - `FeywildSetting.h/.cpp`: `SetGloom`, `ApplyFog`, `SetExtraVeils`. `DockGameMode.cpp`: `SW_FeywildAlt` and the main/alt handover, the smoke test's track count 21. `FeyMapTest.cpp`: pad hops, `=stills` mode and the art stills.
  - `Tools/build_feywild_kit.py` (`oak(..., deep=True)`, `SM_FeyOakDeep0/1` and collisions), `Tools/create_feywild_assets.py` (`M_FeyStone`, `M_FeyHedge`, `M_FeySign`, `M_FeyHangMoss`, the alt track); the new assets; `SourceAssets/Audio/Feywild_alt.wav` (the user's `Feywild_.wav`).
- **Tested:** three full editor routes: the first found tunnel ends and the maze road blocked, the second rock masses folding across the S-bends and pads too far apart; the third passed (`Local/feyart-route-3.log`). Package: 49/49 first time.
- **Not tested:** by hand; the alternation by ear (it was seen switching in the logs).
- **Remaining flaws:** dark maze corridors; sky over the Deepwood's paths; the rest of the kit not yet art-passed.
- **Contract compatibility:** Chuck's rig, scale and controls unchanged. The Maze checkpoint moved beside the gate.
- **Launcher:** `FeywildArt` (receipt 6581798, built from the working tree before this commit); FeywildMap kept as previous; no candidate folder left.

Next part of the work can be done here (Feywild stage 5, mechanics and creatures, after the user has walked it).

## November 20 — Verification: named steps, retries, a launch time limit (Claude)

- **Asked:** the cloud session "Game verification efficiency" sent a plan (rerun only what failed, fix flakes at the source, a manifest runner, fewer launches, one summary, archive old handoffs); the user: "Yes please do so".
- **Source:** `main` at abaa8e25; the commit after the Feywild map's.
- **Changed:** `Tools/Verify-Package.ps1` (each check wrapped as a named step by a one-off transform; checks themselves unchanged except the new `feyMap` step), `Tools/Build-Game.ps1` (`-RetryFailed`), `Tools/Write-BuildReceipt.ps1` (requires `CHUCK_PACKAGE_VERIFIED_ALL`).
- **Tested:** `-Only impact` (partial, `CHUCK_PACKAGE_PARTIAL`); three full runs on the Feywild candidate: the first exposed the fixed step breaking the temple heroes (now opt-in) and the map test's exit freeze; the second the smoke test's music-track count (17 → 20); the third passed with one retry (`hero`, wizard_seal 0.29 then pass). The receipt accepted the marker on promotion.
- **Not done:** the manifest-driven runner, batching trials into fewer launches, the per-run summary with contact sheets, archiving this file's old entries. The fixed step needs a step-by-step trial before any step uses it.
- **Flake ledger so far:** `hero` passed on retry twice (11-19 22:13, 11-20 00:27). One more and it is reported as a flake to fix (wizard_seal coverage).

Next part of the work can be done by either agent (the rest of the verification plan, Codex's area by AGENT-WORKFLOW); preference: Codex.

## November 20 — Feywild: the whole map walkable in rough form, stage 3 (Claude)

- **Asked:** "Proceed with feywild" (stage 3 of `docs/regions/FEYWILD.md`).
- **Source:** `main` at abaa8e25; this commit.
- **Changed:**
  - `FeyMap.h/.cpp` (new): the district and threshold tables, each district's land, water, planting, blockout and bounds planned on a worker thread and put in place a few ms a frame (`AFeyDistrict`); `AFeyMap` (loading and dropping by threshold, late walls and mist, the painted distance and its per-district trees, the far tower and cloud stair, the maze's stand-in); checkpoints, respawn, music zones, `Walk` routes.
  - `FeyMapTest.cpp` (new): `-ChuckFeyMapTest[=short]`, the walker, Chult baseline, per-district frame/GPU time and memory, checkpoint starts and stills.
  - `FeywildSetting.h/.cpp`: the slice as the riverbank's east end (`LoadSlice`/`UnloadSlice`), the path on west, its west wall removed, `SetMistBoost`, motes on the map's ground, `InFeywild` over the map, the map started after the river.
  - `ChuckCharacter.cpp` (respawn at the district's checkpoint), `DockMenu.cpp` (seven district checkpoints in a ninth column, font 16, save point by district), `DockGameMode.cpp` (dark/open/maze tracks by zone; the smoke test's track count 20), `ChultLanding.cpp` (dispatch).
  - `Tools/create_feywild_assets.py` (`M_FeyBlock`, the three tracks with `CHUCK_FEY_MUSIC=1`); new `SW_FeywildDark/Open/Maze`, `M_FeyBlock`; the user's tracks copied to `SourceAssets/Audio/Feywild_dark/open/maze.wav` (LFS, about 100 MB).
  - `docs/regions/FEYWILD.md` (as built).
- **Tested:** editor full route (`Local/feymap-test-3.log`): 5.4 km, stuck 0, void 0, late 0, max loaded 2, districts ready in 0.5-1.1 s after entering their threshold, checkpoint starts under 0.5 s, all districts 16.7 ms (capped), worst threshold frame 25 ms, memory 4400-4550 MB against Chult's 4477 (includes Chult). Two earlier full runs found root tunnels without floors, kinked threshold mouths, the vast tree and big plants crowding paths (fixed). Package: `feyMap` short walk passes in verification.
- **Package:** `Local/feymap-package-3.log`; verified `Local/verify-package-20261119-222217.log` (49 steps, 182 passes, `hero` retried); promoted `FeywildMap`. The overnight run before it hung on a GPU device hang in the upriver step (not reproduced).
- **Not tested:** by hand; the music crossfades by ear; frame headroom above the cap.
- **Remaining flaws:** all blockout; long walks; memory not isolated from Chult; the maze's hedge tops not yet guarded (bees are stage 6).
- **Contract compatibility:** Chuck's rig, scale and controls unchanged. Save points gain the district names; `FeyArrival` unchanged.
- **Launcher:** `FeywildMap` (receipt abaa8e2, built from the working tree before this commit); FeywildRiver kept as previous; no candidate folder left.

Next part of the work can be done here (Feywild stage 4, the kit and art pass, after the user has walked the rough map).

## November 19 — Feywild: the river cutscene and the arrival, stage 2 (Claude)

- **Asked:** "Approve, start stage 2" (the river cutscene and arrival in `docs/regions/FEYWILD.md`, with checkpoints and music); waited on the other session's Phlegethos work (8ab3f2b9) before building.
- **Source:** `main` at 8ab3f2b9; this commit.
- **Changed:**
  - `FeyRiverScene.h/.cpp` (new): the cutscene. Beats on the song's bars (`FeyRiver::Bar`), dunks and surfacings, shots (dark, glimpse, mouth, face, ahead, lip, plunge, drift, shore, glade), the tunnel shade by clock, rush/roar loops low-passed under water, splashes, the landing, `Finish` blending to Chuck's camera.
  - `FeywildSetting.h/.cpp`: `Fey::RiverAt/RiverSide/RiverHalf/RiverGround`, `ShoreSpot/ShoreYaw`, `FeywildArrival`, `InRiverBox`; `BuildRiver`, `ShowRiver`, `SetShade`, `Splash`, `GetMissing`; the streams as their own mesh with `M_FeyRiver`; motes over the river ground; the sun's forward-shading priority 1; `-ChuckFeyRiverSet`.
  - `ChuckCharacter.h/.cpp`: `BeginFeyRiver/EndFeyRiver`, Esc skips, the area start and yaw at the arrival, `CHUCK_FEYWILD_HANDOFF`.
  - `PhlegethosBleed.h/.cpp`: the sweep's end starts the cutscene (the Forecourt placeholder removed). `PhlegethosTest.cpp`: `bleed` waits for the scene, skips it and checks him on the shore; new `-ChuckFeyRiverTest`. `ChultLanding.cpp`: its dispatch. `DockMenu.cpp`: "Feywild Riverbank", "Feywild River (dev)", the save point.
  - `Tools/create_feywild_assets.py`: `M_FeyRiver`, `M_FeyRoot` (new assets), `M_FeyGround` darkened by vertex alpha (re-saved). `Tools/Take-Shots.ps1` `-Extra`; `Tools/shots/feyriver-set.txt` (new); `Tools/Verify-Package.ps1`: the fey-river check (music, rush, roar, splashes, score at 7 s, end with five surfacings, washed, rose, the handoff, nothing missing).
  - `docs/regions/FEYWILD.md` (as built), `docs/regions/PHLEGETHOS.md` (the placeholder notes).
- **Tested (editor):** `-ChuckFeyRiverTest` passed in its last runs (`Local/feyriver-test-6.log`: score at 7.01, end at 36.5, surfacings 5, washed, rose, handoff at (4196, 288), `missing=0`, about 56 fps at 720p). Stills `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/FeyRiver/`.
- **Package:** built (`Local/feyriver-package.log`). Verification: the first run failed on the temple `wizard_seal` trial (max coverage .28); the launcher package failed it the same way (.29, `Local/heroes-launcher-1.log`) and the candidate passed it alone (.34, `Local/heroes-candidate-1.log`). The second failed once on the docks' gnome alchemist (wrist gap 9.3 cm, the flake recorded on 11-17). The third passed, 182 (`Local/verify-package-20261119-040325.log`; fey-river `Local/verify-fey-river-20261119-051244.log`, 56-57 fps). Baselines unchanged.
- **Not tested:** by hand; the sound levels by ear; the full Phlegethos `bleed` trial into the scene on the package beyond the verification's run.
- **Remaining flaws:** the tunnel mouth reads as darkness more than an arch; the upper lip edge looks blocky for about 1 s; he looks as if standing while he drifts in the shallow pond; the river is only the cutscene's corridor (the rest of the map is stage 3).
- **Contract compatibility:** Chuck's rig, scale and controls unchanged; the clips used are existing (FallFlail, Tumble, CrashLand, Idle). Verify-Package gains one check.
- **Launcher:** `Launch-Chuck.cmd` runs `FeywildRiver` (`Tools/Promote-Build.ps1`: receipt, launcher check; PhlegethosRiverSurge kept as previous, FeywildLook2 pruned). The receipt names 8ab3f2b because it was built from the working tree before this commit. No candidate folder left.

Next part of the work can be done here (Feywild stage 3, the whole map walkable in rough form).

## November 18 — Phlegethos: the encounter voiced, the fiend's Infernal in its script, its face, the unavoidable river timed to the track (Claude)

- **Asked:** the five takes (`Fiend_00/01`, `Fighter_00`, `Wizard_00`, `Ranger_00` in Downloads): "I also added Fiend_00 and Fiend_01 for both subtitles use unintelligible Infernal text, dont decrease music volume as much when Fiend is talking. After you do this, do the unavoidable river (time it so the encounter soundtrack lines up with this end)"; then "have the fiends first line be the first line in the scene and his last line be the last line spoken"; "I want the fiends lines to be in infernal script like this" (a glyph strip); "And give the fiends face more detail like this" (a face image).
- **Source:** `main` at 314c3542 (another session's Feywild look on top of a258f11e); this commit.
- **Changed:**
  - Voices: `SourceAssets/NPCs/dialogue.json` (`fortress_00` for the three heroes, a `PitFiend` speaker with `fiend_00/01`; the Infernal text is made up, no language), the takes unmodified under `Voice/<NPC>/source/ElevenLabs_2026-11-18_*`, built WAVs, `manifest.json`, `NPCVoiceData.h` (Blender 4.5.14, `Local/fortress-voice-build.log`; the existing lines' WAVs unchanged), five new `VO_*` assets (the import also re-saved the eight existing hero assets: restored from Git).
  - `FortressBattle.cpp`, `TempleSanctumBattle.h`: the entrance's five lines in order on their takes' lengths, a shot per speaker (two on the fiend), voices from the speakers' heads, jaws by loudness (`TickFortressLines`, `FortSay`), `IsFiendSpeaking`, `GetEncounterTime`; the fiend's mace on the fighter and throw at the ranger with their lines.
  - `TempleSkeleton.h/.cpp`, `FortressFiend.cpp`: `SetFortSpeech` (its jaw over whatever it does; while speaking it stands and strikes at nothing).
  - `DockGameMode.h/.cpp`: the encounter track's place from its playback percent (`GetEncounterMusicTime`), `SeekEncounterMusic`, the track pausing with the game (`bIsUISound` off), a 0.72 duck under the fiend.
  - `PhlegethosBleed.h/.cpp`: the schedule fitted to the track at the start (`CHUCK_PHLEG_BLEED_PLAN`), the clock following the track's place in its loop, the surge (`TickSurge`, `UpdateFloodMesh`, `SurgeFrontAt`), `BeginSweep` (the ride to the lip paced to the track's fall, or 7 s with the track brought to its end), the sweep's end with the track's.
  - The script: `Tools/build_infernal_script.py` (new, Blender), `Tools/import_infernal_script.py` (new), `SourceAssets/UI/InfernalScript/` (the user's strip as given, the cut PNG, `glyphs.json`), `InfernalScriptData.h` (generated), `Content/Art/UI/T_InfernalScript.uasset`; `DockGameMode.h/.cpp` `ADockHUD::DrawInfernalLine` for the speaker "Pit Fiend".
  - The face: `Tools/build_phleg_creatures.py` (`fiend_head` at 0.22 cm joined to the body, the face's parts out of `fiend_body`, `Body(tag_reach)`, missing tags as zero, the eyes, mouth glow and dark spots in `fiend_look`, face close-ups in the review), `SK_PitFiend.uasset`, `MI_PitFiend.uasset` (the import re-saved the other creature materials: restored from Git); `References/Phlegethos/pit-fiend-face-goal.jpg`; `FortressBattle.cpp` the face light and the last shot away from the mace.
  - `PhlegethosTest.cpp`: `bleed` waits on the kept ground for the surge and checks the timing against the plan and the track; `trio` five lines, two fiend stills; the `bleed` time limit 180 s.
- **Tested (editor):** `bleed` pass (`Local/phleg-bleed-surge-4.log`: plan start 26.5 s into the track, surge at 46.5, taken by the surge on the kept ground at 57.7, sweep end at clock 60.63 against 60.75, the track at 87.16 of 87.28 s); two earlier runs were knocked into the river by the battle and took the early path, the track ending with them (87.22, 87.25: `Local/phleg-bleed-surge-1.log`, `-2.log`; the second also confirmed the line order). The third run's sweep worked, but its own death count failed (moving Chuck resets it; it now uses the bleed's). `trio` pass (`Local/phleg-trio-surge-4.log`: five lines, 25.7 s; again with the script and the face, `Local/phleg-trio-face-4.log`); `fiend` pass (`Local/phleg-fiend-face-1.log`); `fiend_show` fails (`no_fiend=1`: it is for the wastes' fiend removed on 11-17; not one of the 23). Stills `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/Phlegethos/Bleed_Surge_*`, `Bleed_Swept_0`, `Trio_Entrance_Fiend*`.
- **Package:** The package (`Tools/Build-Game.ps1 -Package -SkipBaseAssets`, `Local/phleg-face-package.log`) passed its full verification, 182 checks (`Local/verify-package-20261118-221406.log`), and is promoted as PhlegethosRiverSurge. Built from the working tree before this commit (its receipt names 314c3542).
- **Not tested:** by hand; the takes' levels and the ducks by ear; the face light at 35 cd (judged at 60); the script at other resolutions than 720p.
- **Remaining flaws:** the surge is one procedural sheet and runs through the trio, the fiend and the lemures; from the island's far end its front reads as a pale band; caught at the far end, the ride is about a second; the early path's jump in the track is heard as a dip; lingering on the forecourt makes the encounter wait for a later end of the track; the end is still the placeholder return to the Forecourt; the fiend speaks only with its jaw; its head alone is about 124k triangles; in play its face reads mostly as a dark shape with glowing eyes (lit from behind); the glyphs are soft (cut from a JPEG) and the tallest are clipped at the top by the strip's black band; letters map onto 21 glyphs (some share).
- **Contract compatibility:** no changes to Chuck's rig, scale or controls. The encounter track is no longer a UI sound (it pauses with the menu). Verify-Package still expects 23 Phlegethos trials.
- **Launcher:** `Launch-Chuck.cmd` runs PhlegethosRiverSurge (`Tools/Promote-Build.ps1`: receipt, launcher check, FeywildLook2 kept as the previous package, FeywildLook pruned); no candidate folder left.

Next part of the work can be done by Claude only (the river cutscene and the Feywild arrival, Feywild stage 2, which the user approved on 2026-11-18 and another Claude session has in progress; it should start from this surge ending at the track's end); preference: that Feywild session.

## November 18 — Feywild: the look slice, second pass (Claude)

- **Asked:** "Fix the flaws first" (the first pass's list below), then "I also want the oak trees to have some slow fade to different colors changing lights in parts of the leaves and bark, maybe like subtle veins through parts, not too much".
- **Source:** `main` at 6bcc3525; code in 556d1d33.
- **Changed:**
  - `FeywildSetting.h/.cpp`: `BuildProbe` (one-off `USceneCaptureComponentCube`, 1024 float, captured at 1 s and 6 s); `BuildFarLand` (valley, ridges to 5.2 km, 4200 far crowns, 26 valley oaks, the great oak, amber giants); the overlook (`Vista`, `CliffX`, walls on the cliff); `TickWater` (wading rings, drops, splash sound, falls-foot rings); `BuildSound`; `-ChuckFeyPerf`.
  - `ChuckCharacter.cpp`: splash steps when `FeyWading`.
  - `Tools/build_feywild_kit.py`: UV-encoded fungus parts, blade UVs, lily pad, `SM_FeyFarTree`, slot remap. `Tools/create_feywild_assets.py`: `M_FeyFungus/Big`, `M_FeyBlade/Red`, `M_FeyPetal`, `M_FeyPad`, `M_FeyFar`, `M_FeyRipple`; parallax mirror in `M_FeyWater`; bark veins and leaf wash (`Fade`); bluer sky; finer moss. New `Tools/gen_feywild_sfx.py` (`SFX_FeyFalls_00`), `Tools/shots/feywild-perf.txt`.
- **Tested:** editor stills `missing=0`, light audit `lights=3 grooms=20 risky=0`. Package `CHUCK_PACKAGE_VERIFIED passes=182` (`Local/verify-package-20261118-181323.log`); baselines unchanged. Two earlier runs failed on a Waterdeep playtest fault (Cube inside at -235,211,27) and ShipArrival jitter 2/0+1; neither reproduced in three candidate runs, and the launcher package showed jitter 2 in one of three runs, so both are flakes. Robot playtester at FeyLook: `cells=102 travel_m=193 inside=0 stuck=0 void=0 falls=0` (`Local/feylook-playtest-2.log`). Frame time at 1080p windowed: 16.7 ms at every view (vsync-bound, so headroom unknown), ~18.6 ms for the second after each cut (`Local/feywild-perf.log`).
- **Not tested:** by hand; memory use; headroom above vsync.
- **Remaining flaws:** far crowns are simple shapes; the sky is cream round the sun; the bark veins are faint in stills (by request, adjustable); the robot does not reach the overlook edge; Oak0 is heavy; the Phlegethos dev-menu route flaw stands.
- **Contract compatibility:** Chuck's rig, scale and controls unchanged.
- **Launcher:** promoted `FeywildLook2` (receipt 556d1d3); previous kept as `Builds/Windows-Previous-20261118-FeywildLook2`, older pruned to two, no candidate folder left.
- **Not mine, left untouched:** `SourceAssets/NPCs/dialogue.json` changes and new fortress/PitFiend voice sources in the working tree.

Next part of the work can be done here, once the user has judged the look.

## November 18 — Feywild: the look slice, stage 1 (Claude)

- **Asked:** build the Feywild once the Phlegethos session finished, from the plan approved in `docs/regions/FEYWILD.md` ("Proceed"). Stage 1 is one small finished piece, and the user approves the look before the world is built out.
- **Source:** `main` at 9e650ae1.
- **Changed:**
  - New: `Tools/Fetch-Feywild.ps1`, `Tools/build_feywild_kit.py` (Blender 4.5.14), `Tools/create_feywild_assets.py`, `Tools/shots/feywild-look.txt`, `SourceAssets/Setting/Feywild/` (CC0 Poly Haven sources, README, manifest), `Content/Art/Feywild/`, `SW_FeywildMain`, `FeywildSetting.h/.cpp`.
  - `DockMenu.cpp`: dev checkpoint `FeyLook`. `DockGameMode.cpp`: music mode 50 (17 tracks). `ChuckShots.cpp/.h`: `origin feywild`.
- **Tested:** editor stills, `missing=0` (`Local/shots/feywild-look/`); purple-hue samples in the sky 5 of 6004 (`sky`), 0 of 10636 (`skyback`). Robot playtester on the package: `region=FeyLook seconds=90 cells=112 travel_m=217 jumps=29 inside=0 stuck=0 void=0 falls=0` (`Local/feylook-playtest.log`).
- **Package:** `CHUCK_CANDIDATE_VERIFIED` (`Local/verify-package-20261118-074013.log`); the world audit and playtest baselines unchanged.
- **Not tested:** by hand; frame rate; the light audit (the blue fill light is unshadowed, a risk to fur); the region's memory use.
- **Remaining flaws:** the pond reflects the sky but not the scene; modelled mushrooms, leaves and foxgloves look smoother than the paintings; the rock ledge closes the view with no layered distance; Chuck wades chest-deep with no splash; no water sound; Oak0 is heavy (over 134k triangles before its fuller crown, with LODs); the moss clump is imported but not planted; the dev-menu route Phlegethos, Feywild, Phlegethos does not unhide the Phlegethos stage.
- **Contract compatibility:** no changes to Chuck's rig, scale or controls. The Feywild has its own site, away from every other region.
- **Launcher:** `Launch-Chuck.cmd` runs the promoted `FeywildLook` (receipt 9e650ae). The previous package is kept as `Builds/Windows-Previous-20261118-FeywildLook`, older ones pruned to two, no candidate folder left.

Next part of the work can be done here, once the user has judged the look.

## November 17 — Phlegethos: trapped on the battle's side, a fiercer fight, one fiend, the encounter's track (Claude)

- **Asked:** "make the spined devils only start shooting when they target on proximity dont have them just shooting nothing beforehand. reduce brightness of the bus stop. remove the earlier pit fiend, only one pit fiend in the encounter. light the fire in the braisures in this section so that its brighter also I dont see any fire coming from the fiends mace fix that too. I want chuck to be trapped on the side that the trio and fiend are on, not trapped before this section. make the fight more intense, more animations especially from the fiend making this a cinematic fight between the trio and this formidable foe. here is the encounter soundtrack. when chuck dies keep going don't reset the astral fall away" (with `C:\Users\ashsm\Downloads\Ritual of the Broken Sun.wav`).
- **Source:** `main` at cf0aaee5; this commit.
- **Changed:**
  - `PhlegethosSetting.h/.cpp`: the slab field at X 41000-43600 under the battle (12 x 24; moats behind, at the sides and before the wall; river on the island's first two columns); `FieldKept` (cells touching the breach's lip are ground), `OverSlab`, `FieldColAt/RowAt`; the heightfield keeps quads touching kept ground; `PhlegethosRespawn` uses `APhlegBleed::ArenaRespawn` in the Forecourt stretch while the bleed runs; `ResetHazards` no longer re-arms the bleed; idols moved to the wall ledge (facing the battle); `PhlegBrazierFire` (larger flames, a light each) for the path's and the idols' braziers.
  - `PhlegethosBleed.h/.cpp`: kept cells skipped (19), walls round them, `CellSlab` lookup, the processional rows in `M_PhlegPath`, the respawn point, `Held` (the battle's feet hold slabs: waves skip them, the collapse waits, a cracking one waits), no re-arm on a death (`DeathsIn`).
  - `FortressBattle.cpp`, `TempleSanctumBattle.h`: positions on the island; lemure leash the island; `FortWantUp` 3; `FiendSmashComing` with a lead and a chance; `PhlegethosBattleMusic`.
  - `TempleSkeleton.h/.cpp`, `FortressFiend.cpp`, `PitFiendMotion.cpp`: `EAttack::Leap/Sweep/Buffet` (procedural, as the mace), their choice by turns, faster pacing, `TickFortressAttack`; the mace's two larger flames from its crown, its light 1500.
  - `TempleHero.cpp`: the wizard's fiend bolts every 3.5-6 s; the fighter's pause after a cut at the fiend .4-.9 s.
  - `PhlegethosCreatures.h/.cpp`: the wastes' Pit Fiend removed (`PhlegethosPitFiend` returns the fortress's); devils `HasTarget` (throw only at him).
  - `PhlegethosBusStop.inl`, `Tools/create_feywild_bleed_assets.py`: dimmer lights and lightbox, darker concrete and markings (all five materials rebuilt, `Local/fey-materials-5.log`).
  - `DockGameMode.cpp`: music mode 42, `SW_PhlegFortressBattle`. `Tools/import_phleg_battle_music.py` (new); `SourceAssets/Audio/RitualOfTheBrokenSun.wav` (the user's, unmodified; README entry); `Content/Art/Audio/SW_PhlegFortressBattle.uasset`.
  - `PhlegethosTest.cpp`: `fiend` on the fortress fiend; `bleed` with a moat death that keeps it going; `trio` 60 s with leap/sweep/buffet stills.
- **Tested (editor):** `bleed` pass (`Local/phleg-bleed-arena-5.log`: moat 48/32/48, death kept, respawn (41888, -1685), swept, back at the checkpoint); `trio` pass (`Local/phleg-trio-arena-5.log`: a leap, 3 smashes, 5 hurls); `fiend` pass (`Local/phleg-fiend-arena-5.log`); all 23 (`Local/phleg-all-arena.log`: `bleed` failed once, knocked about crossing to the moat, now placed at its edge; `Local/phleg-all-arena-2.log`: `devils` failed once, the devil now shot him as he stood before it to scratch it, now beside it; then `devils` twice, `Local/phleg-devils-side-1/2.log`). Sheets `Local/arena-sheet-1.png`, `Local/arena-sheet-2.png`.
- **Package:** `CHUCK_PACKAGE_VERIFIED passes=182` (`Local/verify-package-20261117-232919.log`), Phlegethos 23/23. Three earlier verifications of the first package failed: `lake_slabs` (a fall at the first hop, X 10798; 23/23 on the next two runs), `wizard_seal` (seal coverage 0.28; also flaked on 11-15; the changes to the wizard apply only with a fiend present), and the smoke test's "music ducks under NPC speech", a real miss of mine: the test expected 15 music tracks and the encounter's makes 16 (fixed, repackaged). `lake_slabs` and `wizard_seal` were not changed and their cause was not found.
- **Not tested:** by hand: how hard the island is with the lemures and the fiend's reach; the music's crossfade by ear; the sweep and the buffet were seen in the logs, the leap's stills were not caught.
- **Remaining flaws:** the new moves are procedural over the idle (no tucked legs in the leap, no foot turn in the sweep); the kept ground by the breach is a safe place to wait; the `trio` watching frames mostly show the wall; the entrance's wide shot now sits close on the fiend; the processional's slabs fall with the rest.
- **Contract compatibility:** no changes to Chuck's rig, scale or controls. `EAttack` gains three before `None` (arrays sized 9). Phlegethos spawns unchanged (14); the Forecourt respawn goes to the arena while the bleed runs. Verify-Package still expects 23.
- **Launcher:** `Launch-Chuck.cmd` runs the promoted `PhlegethosArenaFight` (exe built 2026-11-18 07:28 UTC, runtime source cf0aaee5 plus this commit's working tree; the receipt's source revision 88d3121c is `main` with another session's Feywild plan documents on top). Previous package kept; older pruned to two; no candidate folder left.

Next part of the work can be done by Claude only (the river cutscene and the Feywild riverbank; voice takes when supplied); preference: Claude, here.

## November 17 — Phlegethos: spined devils aim (poorly) and quieter; stake bars, ascent lava fall out; dimmer hot cracks (Claude)

- **Asked:** "Have the spined devils intentionally  target chuck (poorly). Reduce spined devil volume. The path is lined with spikes that have a weird side bar sticking out of the top of them, remove the side bar. There's lava fall that directly and impossibly lands on stone close to where player walks, remove this lava fall. Decrease stone hot crack brightness"
- **Source:** `main` at 2e9ff8d0; this commit.
- **Changed:**
  - `PhlegethosCreatures.h/.cpp`: `APhlegSpinedDevil::TakeAim` at each wind-up (in front, line of sight, `AimRange` 26 m; his chest then, no lead; `MissChance` .6 of throws 35-110 cm off, the rest within 12 cm); `Aimed` counter; every devil sound quieter.
  - `PhlegethosSetting.cpp`: the stakes' cross bar removed (hills and wastes); the ascent's fall (`AscentX + 400`) removed.
  - `Tools/create_phlegethos_assets.py`: `M_PhlegBasalt` plates' crack glow x.5, face seams 22 to 13; `CHUCK_PHLEG_ONLY` env var rebuilds only the named materials from the imported textures (run with `CHUCK_PHLEG_ONLY=M_PhlegBasalt`; only `M_PhlegBasalt.uasset` changed).
  - `Tools/create_feywild_bleed_assets.py`: vertex-colour alpha by its own `A` pin in `M_FeyTear` and `M_CityPaint` (masking the RGB output for A failed to compile: both had shown the default material since they were made); all five materials rebuilt (`Local/fey-materials-4.log`).
  - `PhlegethosTest.cpp`: `devils` waits up to 14 s for a hit and requires aimed throws.
- **Tested (editor):** all 23 Phlegethos trials, twice (`Local/phleg-all-devils.log`, `Local/phleg-all-devils-2.log`; devils thrown 9, aimed 10, hits 3 then 2); no material compile failures in the second. Sheets `Local/devils-sheet-1.png` (ascent without its fall, dimmer cracks, bus stop, tear, gorge devil), `Local/devils-sheet-2.png` (plain stakes).
- **Package:** Package: built and verified first time, 182 passes (`Local/verify-package-20261117-141739.log`, Phlegethos 23/23).
- **Not tested:** by hand: how the devils' aim feels on the move; the sound levels by ear.
- **Remaining flaws:** as the entry below; the cracks may still read bright at the hills' landing (halved, not restyled).
- **Contract compatibility:** no changes to Chuck, controls or spawns.
- **Launcher:** promoted as `PhlegethosDevilAim` (receipt names 2e9ff8d0, the source before this commit; previous kept as `Builds/Windows-Previous-20261117-PhlegethosDevilAim`, the TrioBattle backup pruned); `Launch-Chuck.cmd` is current.

Next part of the work can be done by Claude only (the river cutscene and the Feywild riverbank; voice takes when supplied); preference: Claude, here.

## November 17 — Phlegethos: the bus stop and the Feywild bleedthrough to the river's sweep (Claude)

- **Asked:** "Add the modern city bus stop with business man Npc with small astral ruptures to a sensible place along the plethegos path, similar to 2d game. No umbrella for the business man. Then, in the encounter, add the river appearance and sweep into feywild river (don't start the cutscene or feywild yet). We'll need a way to keep the player in the staging area during the encounter ... maybe ... similar with how the pressure plate fall away to astral rupture section worked in the temple?"
- **Source:** `main` at cdb04867; this commit. Direction: 2D `docs/development/PHASE-8.md` (fractured way, Feywild bleedthrough) and `data/dialogue/city.json` (read-only).
- **Changed:**
  - `SourceAssets/NPCs/humans.json`: `Businessman` (suit, shirt, tie, low shoes, short groom; no umbrella). `Tools/build_npc_humans.py`: a `tie` piece and a trouser `hem` option. Built with Blender 4.5.14/MPFB 2.0.17 (`SourceAssets/NPCs/Humans/Businessman/SK_Businessman.fbx`, `GR_Businessman_Hair.abc`, manifest), imported with `Import-NPCHumans.ps1 -Only Businessman` and `Import-NPCGrooms.ps1 -Only Businessman` (`Content/Characters/Humans/Businessman/*`).
  - `DockNPC.h/.cpp`: `EDockHuman::Businessman` (after the pirates; loaded when spawned; `IsPirate` now ends at the captain); looking-about idle.
  - `Tools/create_feywild_bleed_assets.py` (new): `M_FeyRiver`, `M_FeyTear`, `M_CityPaint`, `M_CityConcrete`, `M_CityAsphalt` in `/Game/Art/Phlegethos` (reuses the Water textures already copied for the sewer).
  - `PhlegethosBusStop.inl` (new, included by `PhlegethosSetting.cpp`): the city fragment, shelter, lights. `PhlegethosSetting.h/.cpp`: bus stop frame and five small ruptures (`Rifts`, `RiftInside`; the breach's lip/pit builder generalised), their Astral death; the slab field's constants; the forecourt level from 20 m further back; the heightfield cut under the field and the ruptures; path, stakes, rocks kept off them; processional and idols start past the field; spawns the bleed.
  - `PhlegethosBleed.h/.cpp` (new, `APhlegBleed`): 240 slab meshes (jittered grid, crack glow section), the Astral void, the moat/waves/collapse schedule with the temple's stand-on rule, the river (course, per-frame mesh, tears, lights, sound), the sweep (camera, fade) and re-arm.
  - `ChuckCharacter.h/.cpp`: `BeginCarried`/`SetCarried`/`EndCarried` (`EAstral::Carried`: flailing, moved by its owner, then an Astral return).
  - `FortressBattle.cpp`, `TempleSanctumBattle.h`: wizard and ranger moved to X 42180/42200, lemure posts off the field, `HoleAt` sees fallen slabs, `HasFortressSceneRun`, `IsFortressFighting`. `PhlegethosCreatures.cpp`: spawns the businessman (`Lines` "Ah! A rat!").
  - `PhlegethosTest.cpp`: trials `bus_stop`, `bleed`; `approach` goes round the path's ruptures; `APhlegBleed::bHold` outside `bleed`; screenshot cap 90. `Tools/Verify-Package.ps1`: Phlegethos `passed=23`.
- **Tested (editor):** `bus_stop` pass (`Local/phleg-bus_stop-2.log`: watching, mocap, talkable, one rupture death); `bleed` pass (`Local/phleg-bleed-2.log`, rerun `Local/phleg-bleed-3.log`: moat 48/24/48 down by 5.6 s, 53 island slabs in the waves, swept 6 s, back on the island, re-armed, no death); all 23 (`Local/phleg-all-bleed.log`, 0 failures); `trio` again with the moved heroes (`Local/phleg-trio-3.log`). Sheets `Local/busstop-sheet-1.png`, `Local/bleed-sheet-1a/b/c.png`, `Local/bleed-sheet-3.png`; Blender review renders of the businessman were written to `C:\Local\bm-review` (outside the repo, by a relative path; copies in `Local/bm-review`).
- **Package:** Package: built (`Local/phleg-bleed-package.log`); its first verification failed once on the docks' gnome alchemist (wrist gap 9.5 cm, a timing flake: 4.2-4.6 cm in every other run, no code reaches him); the rerun passed with 182 (`Local/verify-package-20261117-112254.log`, Phlegethos 23/23).
- **Not tested:** anything by hand; how hard the island is to survive before the river; the businessman's talk prompt in play.
- **Remaining flaws:** no voice take for "Ah! A rat!"; the businessman's idle is the guards'; the ad panel is blank; the river reads as a glowing teal current, the tears as pale shapes at a distance; the moat is hard to read from rat height until near; no music change; the slabs read as a grid of broken flags; the sweep's camera trails him a little; borrowed sounds; the cutscene and the Feywild are not built (he returns to the Forecourt checkpoint).
- **Contract compatibility:** no rig, scale or control changes to Chuck; `EAstral` gains `Carried` at its end. `EDockHuman` gains `Businessman` before `Count` (no save stores it). Phlegethos spawns unchanged (14). The battle's hero posts and lemure posts moved.
- **Launcher:** promoted as `PhlegethosBleed` with `Tools/Promote-Build.ps1` (receipt names cdb0486, the source before this commit; previous kept as `Builds/Windows-Previous-20261117-PhlegethosBleed`, the FiendMotion backup pruned); `Launch-Chuck.cmd` is current.

Next part of the work can be done by Claude only (the river cutscene and the Feywild riverbank; a voice take for the businessman's line and the trio's three when the user supplies them); preference: Claude, here.

## November 16 — Phlegethos: the trio's battle at the fortress (Claude)

- **Asked:** "When you're ready, go ahead and do the trio encounter" (after the fiend-motion milestone was promoted and pushed as e6903e7b).
- **Source:** `main` at e6903e7b; this commit.
- **Changed:**
  - `TempleSanctumBattle.h/.cpp`: a second site, `ESite::Fortress` (`SpawnFortress`, `GetFortress`; `Get` still returns only the sanctum's). Fortress: no beholder, its own cast, streamed in once he is past X 32000 in the wastes; arrows and bolts strike the fiend (`TheFiend`); `FireBolt` takes a spread (the wizard's fans); `BigFoeCentre`, `IsBigFoeFighting`, `HoleAt` (the sanctum's floor or the breach).
  - `FortressBattle.cpp` (new): the cast (heroes, seal over the breach, the fiend, six lemures), lit on channel 0; activity while he is near the forecourt (held when `APhlegFlameskull::bCalm`); the heroes' dodges; the fiend's blast on heroes and lemures; its fireballs (flipbook flames from `AChuckFlames`, a light, `GroundBlow`, the burst on heroes and on Chuck); the entrance's three shots and its subtitle-only lines.
  - `FortressFiend.cpp` (new): `ATempleSkeleton::TickFortress` (stance, roar, wing flares, walking in on the fighter, smash at him, hurl at wizard/ranger by turns, ordinary attacks if Chuck is under it), `TickHurl`, `FiendStruck` (flinches, never falls), `FortRoar`.
  - `TempleSkeleton.h/.cpp`: `EAttack::Hurl` (counters sized 6); the fortress fiend's tick; `Steer` and the arrows' lighting through the battle; `TakeHeroBlow` on a fiend only flinches; lemures melt in battle. `PitFiendMotion.cpp`: the hurl shares the smash's body layers; its impacts reach the heroes (`FiendBlast`); the smash's ground trace ignores creatures and heroes.
  - `TempleHero.h/.cpp`: aim at `BigFoeCentre` instead of only the beholder; the wizard's bolt fan; the ranger shoots the fiend every 1-2 s and leaves the fighter's lemures to him at the fortress; the fighter's cut at the fiend's legs (`FiendContact`); `EvadeFiendSmash`, `EvadeFire`, `TakeFiendBlast`; hole checks through the battle.
  - `TempleSeal.h/.cpp`: `PlaceAtBreach`. `TempleHeroTalk.cpp`: subtitles/speech from the fortress's lines.
  - `PhlegethosSetting.h/.cpp`: the breach (hole in the wastes' heightfield, basalt lip and faces, Astral depth), `BreachRadius`/`BreachInside`, its Astral death; spawn `Forecourt` (14 spawns); boulders kept off the forecourt.
  - `PhlegethosCreatures.cpp`: spawns the fortress battle; `PhlegethosPitFiend()` and the lemure count leave the battle's own out. `DockMenu.cpp`: checkpoint "Phlegethos Forecourt". `DockGameMode.cpp`: letterbox, subtitles and music duck for the fortress. `PhlegethosTest.cpp`: trial `trio`, the `lemures` count. `Tools/Verify-Package.ps1`: Phlegethos `passed=21`. `Tools/Verify-Menu.ps1`: the new checkpoint.
- **Tested (editor):** `trio` pass (`Local/phleg-trio-5.log`: entrance 13.4 s, 3 lines; arrows=13 bolts=12 cuts=5 smashes=3 hurls=4 bursts=4, smash dodges 3, lemures down 4, fiend 23 m clear of the breach); all 21 trials (`Local/phleg-all-trio.log`: 20 passed, `lemures` failed on its count of 14 lemures - the battle's six - fixed and rerun alone, pass, `Local/phleg-lemures-4.log`). Sheets `Local/trio-sheet-2.png`, `Local/trio-sheet-3.png`.
- **Package:** 182 passes, `Local/verify-package-20261117-004146.log`. The first verification inside `Build-Game.ps1` failed once on "Xbox A mapping jumps" (docks smoke test, untouched by this work, passed in every earlier package); the rerun on the same candidate passed everything.
- **Not tested:** anything by hand; the sanctum's battle after the hero changes except in the package verification's sanctum trials; how the crossfire feels to play through.
- **Remaining flaws:** no voice takes (subtitles only, and a made-up jaw rhythm); no dodge clip for the heroes; the fireball is a flying flipbook flame; sounds borrowed from the sanctum; dark forecourt; the fight never ends (escalation and river next).
- **Contract compatibility:** no rig, scale or control changes to Chuck. `ATempleSanctumBattle::Get` unchanged (sanctum only). `EAttack` gains `Hurl`. Phlegethos gains a 14th spawn at the end of `EPhlegSpawn` (earlier saves' indices unchanged).
- **Launcher:** promoted as PhlegethosTrioBattle (receipt `e6903e7b` plus this work, committed right after); previous package kept as `Builds/Windows-Previous-20261117-PhlegethosTrioBattle`, PhlegethosNPCs backup pruned; no candidate folder left.

Next part of the work can be done by Claude only (the Astral escalation, the Feywild river and the river cutscene; voice takes for the three fortress lines when the user supplies them); preference: Claude, here.

## November 16 — Phlegethos: the Pit Fiend's motion, its mace, lemures that throw (Claude)

- **Asked:** the user could not find the Pit Fiend; wants "more realistic leg movement and placement and overall more immersive body movements such as wings beating etc ... formidable in battle, not just a lumbering skeleton with dressing" (it will fight the trio later); lemures should pick up and throw Chuck as the skeletons do.
- **Source:** `main` at dadce124; this commit.
- **Changed:**
  - `Tools/build_phleg_creatures.py`: the fiend's own bones added to the rig before export (`fiend_bones`, `add_bones`): `wing_arm/fore/f1-3/t1-3_{l,r}`, `tail_01`-`tail_08`, `tail_tip`. Wing tubes on their bones; membranes weighted across the bone edges round each panel (`membrane_weights`); the tail remodelled to trail behind on the ground, on its bones, barbed. `import_phleg_creatures.py` saves the shared skeleton asset (the import merges the new bones into `SK_TempleSkeleton_Skeleton`; the skeleton's clips leave them at rest).
  - `SkeletonAnim.h/.cpp`: `FFiendPose` and `FiendLayers`: foot IK to traced ground with the sole on the slope, hips lowered/shifted, spine lean/roll/twist/bend/breath, left arm swing, right-arm IK with the haft turned (the mace), wing raise/sweep/fold/finger fold, tail yaw/pitch. Axes: Unreal's are left-handed (`Up x Forward` is the right side).
  - `PitFiendMotion.cpp` (new): `TickFiend` (ground traces, footfall detection, stance, roar, lean/roll, wing states and beats, tail sway/trail/lift/flick/ground fix), `FiendPlant`, `FiendImpact` (ash/embers via `APhlegethosSetting::GroundBlow`, `BeholderShake`, the blast), `TickSmash` (the mace's swing on a planned arc; hit judged at the planned landing point), `ShowRoar`/`ShowSmash` for review.
  - `TempleSkeleton.h/.cpp`: `EAttack::Smash` (the fiend's from 2.2-3 m, 60% at 1.4-2.2 m); 2.6 s roar; turning in place walks the stride; fiend footsteps from footfalls; stamp/swat impacts; warm `FiendFill` light; lemures grab and throw (`IsFiend()` only is excluded now); in the Phlegethos trials a throw goes only toward ground.
  - `PhlegethosCreatures.cpp`: the fiend's round along the path within 3 m of it. `PhlegethosSetting.h/.cpp`: `GroundBlow`; drops can land on a floor; puffs have their own drag. `ChuckCharacter.h/.cpp`: `Blasted` (knocked off his feet: flung tumbling, crash-lands, gets up; no harm).
  - `Content/Data/Audit/Allow.txt`: smoke wisps (`SM_CigaretteSmoke*`) allowed to float.
  - `PhlegethosTest.cpp`: `lemures` passes only if it threw him; `fiend` checks wing beats and footfalls, 40 s; new `fiend_show` (by name only; its own camera on the forecourt).
- **Tested (editor):** `fiend` pass (`Local/phleg-fiend-3.log`: killed by the mace at 16.7 s; beats=6 footfalls=11 smashes=1 blasts=1, planted ankles within 13 cm, tail 2 cm into the ground at worst); `lemures` pass (`Local/phleg-lemures-3.log`: held, hung, thrown 640 cm/s, 17 hits, melted); `fiend_show` pass (`Local/phleg-fiend-show.log`, stills `Saved/Screenshots/WindowsEditor/Phlegethos/Show_*`, sheet `Local/fiend-smash-sequence.png`). The full 20 ran in the package verification (below).
- **Package:** Package verified (`Local/verify-package-20261116-195926.log`, 182 passes) after one rerun: the first run failed only on the Waterdeep world audit (a dock NPC's pipe-smoke wisp counted as floating, 118 vs 117), now allow-listed (`SM_CigaretteSmoke*` in `Content/Data/Audit/Allow.txt`).
- **Not tested:** anything by hand; how the mace and the blast feel in play; the fiend's round on the path for difficulty.
- **Remaining flaws:**
  - The legs are still the skeleton's motion-capture walk under the IK and heavier stance, not a gait of its own; the swat and stamp are the skeleton's clips at 2.4x.
  - Folded wings stretch their membranes rather than crease; thigh plates and mace may cut into the body in some poses.
  - `fiend_show`'s forecourt pillars hide the walk frames; the stills are dark.
  - The tail's ground check can be fooled by a block beside it (85 cm on the forecourt in `fiend_show`).
- **Contract compatibility:** the shared temple skeleton asset gains the fiend's 22 bones (temple skeletons and their clips unaffected; their trials ran in the package verification). Chuck gains `Blasted` (no rig, scale or control changes). `EAttack` gains `Smash` (counters sized 5).
- **Launcher:** promoted as PhlegethosFiendMotion (receipt dadce124 plus this commit's then-uncommitted work); kept `Windows-Previous-20261116-PhlegethosFiendMotion` and `-PhlegethosNPCs`, pruned `-PhlegethosCreatures`. No candidate folder left.

Next part of the work can be done by Claude only (the trio vs Pit Fiend battle when the user wants it, then the Astral escalation and Feywild river); preference: Claude, here.

## November 16 — Phlegethos: lemures, spined devils and the Pit Fiend (Claude)

- **Asked:** proceed with the region's remaining creatures (no horned devils; the trio vs Pit Fiend battle not yet). Then a goal image for the Pit Fiend (added as `References/Phlegethos/pit-fiend-goal.jpg`): the fiend was remodelled to it.
- **Source:** `main` at 6562b930; this commit.
- **Changed:**
  - `Tools/build_phleg_creatures.py` (new): SDF sculpts (Blender 4.5.14, `sdf_kit.py`): `SK_Lemure` and `SK_PitFiend` skinned to the rebuilt temple skeleton rig (unchanged), the spined devil as five static parts, its spine, its basalt column; review renders (`--review`, `Local/creatures/`); `SourceAssets/Enemies/Phlegethos/*.fbx` + `creatures.json`.
  - `Tools/import_phleg_creatures.py` (new): imports into `/Game/Art/Phlegethos/Creatures`; `M_PhlegCreature` (vertex colour albedo with gamma 2.2, alpha glow, tiling detail normal) and its instances.
  - `ChultTempleInterior.h`: `FTempleSkeletonPost` gains `Kind` (1 lemure, 2 Pit Fiend) and `Route`.
  - Pit Fiend second pass (`fiend_body`/`fiend_look`/`fiend_mace` in the build script): long out-and-up horns, charcoal armour (collar, spiked pauldrons and bracers, skull belt, thigh plates, knee guards, greaves, armoured feet), smoother hide, wings 1.5x the first pass, a tail curled on the ground, a spiked mace in the right fist (skinned to `hand_r`); `creatures.json` "mace_head" = the head as affine joint weights. `TempleSkeleton.cpp` turns it into hand space, puts moving flipbook fire on the crown and a 700 cd flickering light 1.4 m above it (`TickMace`, `GetMaceHead`). `import_phleg_creatures.py -ChuckPhlegOnly=<mesh,...>` re-imports only those meshes; `MI_PitFiend` detail .3, roughness .45.
  - `TempleSkeleton.h/.cpp`: kinds load their own mesh and scale (lemure .93, fiend 2.4), sounds, speeds, notice ranges; the stage frame; the fiend's patrol route, roar, 4 s lose-sight, one-blow kill, unscratchable; lemures 17 health, melt into a puddle; calm gate for trials; scaled reach/radius getters.
  - `PhlegethosCreatures.h/.cpp`: `APhlegSpinedDevil` (column, tail flick wind-up/whip/release, 3-spine pool with moving flipbook fire, stone hits, wing flap, head look, contact 1 point, scratch fatal); spawns 8 lemures, 6 devils, the fiend; `PhlegethosScratchDevils`, `PhlegethosLemureCount`, `PhlegethosPitFiend`.
  - `ChuckCharacter.cpp`: the skeleton slash uses `RadiusNow`/`HalfHeightNow`; slashes reach the devils.
  - `PhlegethosTest.cpp`: trials `lemures`, `devils`, `fiend` (20). `Tools/Verify-Package.ps1` expects 20. `docs/regions/PHLEGETHOS.md`: Creatures built.
  - New assets: `Content/Art/Phlegethos/Creatures/*` (LFS).
- **Tested (editor):**
  - Phlegethos 20/20 (`Local/phleg-npcs-trials.log`): lemures hits=17 melt=3.5; devils thrown=9 hits=3 scratched=1; fiend noticed at 1764 cm, killed by a kick.
  - Fall cutscene `failures=0 handoff=1` (`Local/phleg-npcs-hellfall.log`); `lemures=8 devils=6 fiend=1`.
  - Stills in `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/Phlegethos/` (Lemure_*, Devil_*, Fiend_*); the remodelled fiend: 20/20 again (`Local/phleg-fiend2-trials.log`), then its own trial after the mace fixes (`Local/phleg-fiend2-one.log`, `Fiend_Close.png`); review renders `Local/creatures/SK_PitFiend_*.png`.
- **Package:** `Tools/Build-Game.ps1 -Package -SkipBaseAssets` into `Builds/PhlegethosCandidate`, then `Tools/Verify-Package.ps1`: `CHUCK_PACKAGE_VERIFIED passes=182` (`Local/verify-package-20261116-162735.log`, run log `Local/phleg-fiend2-verify.log`). Earlier verification runs of the first-pass creatures failed on known flakes unrelated to this work (gnome wrist gap 8.1 > 8, the ShipArrival playtest inside the hull, Chult stuck) and the last was stopped for the remodel.
- **Not tested:** anything by hand; how hard the devils' lanes and the fiend's round are in play.
- **Remaining flaws:**
  - Lemures use the skeleton's upright mocap walk, not a shamble.
  - The Pit Fiend's wings are rigid; its walk is the skeleton's at 2.4x; its thigh plates and mace may cut into the body in some poses of the skeleton's clips; its iron mace head has no glow of its own (the fire does it).
  - Earlier: Chuck's fur bright near the ascent's hot cracks; flat falls; plain hills; the ship's hidden-hull fault.
- **Contract compatibility:** no rig, scale or character changes; the temple skeleton rig is reused unchanged. `FTempleSkeletonPost` gains `Kind`/`Route` (default 0/empty; temple skeletons unchanged). Three trials added.
- **Launcher:** promoted with `Tools/Promote-Build.ps1 -Label PhlegethosNPCs` (receipt 6562b93, built from the working tree with this work uncommitted; committed right after). Kept `Windows-Previous-20261116-PhlegethosCreatures` and `-PhlegethosNPCs`; pruned `Windows-Previous-20261115-PhlegethosHills`. No candidate folder left. Not played by hand.

Next part of the work can be done by Claude only (the trio vs Pit Fiend battle when the user wants it, then the Astral escalation and Feywild river); preference: Claude, here.

## November 16 — Phlegethos: second geysers out, quieter splash, flameskulls and fire snakes (Claude)

- **Asked:** remove the second flame geysers; reduce the lava falls' splash and tone down its sound; do the region's creatures, without the 2D game's separate horned devils.
- **Source:** `main` at cf8f6720; this commit.
- **Changed:**
  - `PhlegethosSetting.h/.cpp`: the ramp's lava bombs (balls, marks, slams, burns, counters) removed; the volcano erupts every 6–9 s while Chuck is on the ramp. `TickSplash` toned down (fewer, lower, smaller drops; rarer gobbets; smaller domes; thinner smoke; 240 cd light; the hiss within 20 m only, quieter, rarer). `Enter()` spawns the creatures.
  - `PhlegethosCreatures.h/.cpp` (new): `APhlegFlameskull` (temple skull 1.2x, ember eyes, moving flipbook fire, flickering light; a fast run across a fixed haunt with a 3.1x weave and a bob; a touch costs 1 Sanity and pushes him back along −X; cannot be scratched; asleep beyond 90 m). `SpawnPhlegethosCreatures`: 4 road skulls, 3 ramp skulls, 5 harmless over the lava; 11 fire snakes.
  - `ChuckFlames.h`: `AddMoving`/`SetBase` for fire that follows an actor.
  - `ChultTempleInterior.h`: `FTempleSnakePost::bFire`.
  - `TempleSnake.h/.cpp`: `FrameOf`/`WorldOf` (temple or stage frame), `FloorAt` ground trace; a fire snake follows the ground, turns back from steps over 18 cm, lights on channel 0, wears `M_PhlegFireSnake` (Heat cools on death), has a glow light, skips the temple sound gate.
  - `Tools/create_phlegethos_assets.py`: `M_PhlegFireSnake` (snake scale textures as charred crust with heat in the gaps; Heat parameter); 9 materials. New asset `Content/Art/Phlegethos/M_PhlegFireSnake.uasset`; the other re-saved assets restored.
  - `PhlegethosTest.cpp`: trials `skulls`, `fire_snakes` added, `bombs` → `ramp`; 17 trials; the other trials run with the creatures calm. `Tools/Verify-Package.ps1` expects 17.
  - `PhlegethosFall.cpp`: a comment. `docs/regions/PHLEGETHOS.md`: the change section and a Creatures plan.
- **Tested (editor):**
  - Phlegethos 17/17 (`Local/phleg-creatures-trials.log`): skulls burns=2 with no deaths; fire_snakes strikes=1, scratches=1 (dead); splashes=25361.
  - Fall cutscene `failures=0 handoff=1` (`Local/phleg-creatures-hellfall.log`); creature log `skulls=12 harmful=7 fire_snakes=11`.
  - Stills in `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/Phlegethos/`.
- **Package:** `CHUCK_PACKAGE_VERIFIED passes=182` first run (`Local/phleg-creatures-package.log`, `Local/verify-package-20261116-081215.log`).
- **Not tested:** anything by hand; how hard the skull lanes are to time in play.
- **Remaining flaws:**
  - Lemures, spined devils and the Pit Fiend are not built.
  - Chuck's fur reads very bright near the hot cracks on the ascent (from the earlier light raise).
  - The falls are still flat sheets; the hills still lack rock detail; the ship's hidden-hull fault is open.
- **Contract compatibility:** no rig, scale or character changes. `FTempleSnakePost` gains `bFire` (default off; temple snakes unchanged). Trial `bombs` renamed `ramp`; two trials added.
- **Launcher:** promoted as PhlegethosCreatures. The receipt names cf8f6720 because the package was built from the working tree before this commit. PhlegethosHills kept as the previous package; PhlegethosLava pruned; no candidate left.

Next part of the work can be done by Claude only (lemures, spined devils, the Pit Fiend colossus); preference: Claude, here.

## November 16 — Phlegethos landing hills, hot cracks, fall splash, light and spawns (Claude)

- **Asked:**
  - Make the hot cracks look like the goal image (`References/Phlegethos/phlegethos-goal-hot-cracks.png`, added).
  - Remove the lava spigot hazards and their visuals, keeping the areas for flame skulls later.
  - Add a natural hilly land mass with a path before the first part, where Chuck lands, with no hazards.
  - Give the lava falls a natural splash.
  - Light the region slightly more.
  - Add spawn points.
- **Source:** `main` at 832f7087; this commit.
- **Changed:**
  - `PhlegethosSetting.h/.cpp`:
    - **Geysers removed:** the geysers, their vents, lights, sounds and hits are gone. The road's ground is unchanged.
    - **The hills (`HillsGround`, `HillsPathY`, from X −11000 to −2500):**
      - a level landing clearing;
      - a flagstone path over two rises (about 3.3 m) and a dip to the plain's neck;
      - rolling hills, crests and tors off the path, low near the clearing;
      - ridges framing the valley;
      - lava hollows well off the path;
      - edges down into the lake;
      - column clusters, boulders, fissures, stakes, vents and jars.
      - The badlands clear round them, and the plain's path flagstones now start at the join.
    - **Landing:** `PhlegethosLandingSpot` and the fall cutscene's handoff are at the hills.
    - **Splash (`TickSplash`):** each lava fall's foot has:
      - molten spray stretched along its flight and falling back in arcs, with bigger gobbets;
      - flattened boiling domes;
      - smoke;
      - a flickering 320 cd light;
      - a hiss.
    - **Light:** sun 1.2 (was lower), sky light 1.15, exposure bias −0.05, Chuck's under/ahead lights raised.
    - **Spawns:** `EPhlegSpawn` now has 13 entries:
      - Landing, Plain, Road, RoadLong, Lake, LakeSlabs, Gorge, GorgeShelf, GorgeChains, Ascent, AscentSteps, AscentChimney, Fortress;
      - each has a stretch start, so a death returns Chuck to the nearest one behind him.
  - `DockMenu.cpp`: 13 Phlegethos points in menu columns 7 and 8.
  - `ChuckCharacter.cpp`: the fall cutscene ends at the Landing spawn.
  - `PhlegethosTest.cpp`:
    - new `hills` trial (runs the path; checks it stays on it and on the ground);
    - geyser stills replaced by fall-splash stills;
    - the log prints `splashes=`.
  - `Tools/Verify-Package.ps1`: expects 15 Phlegethos passes.
  - `Tools/create_phlegethos_assets.py` and `M_PhlegBasalt` rebuilt: the hot-crack block is rewritten to the goal image.
    - plates 2–3 m, warped twice, ragged;
    - cracks 4–12 cm with a yellow core and licking fire;
    - lips bevelled and lit;
    - dark hairlines, a few faintly hot.
    - The other assets the script re-saves were restored unchanged.
  - `docs/regions/PHLEGETHOS.md`: a new section; the known gaps are updated.
- **Tested (editor):**
  - Phlegethos 15/15 (`Local/phleg-hills-trials2.log`). In the hills trial Chuck strayed at most 85 cm off the path; `splashes=69033`.
  - Fall cutscene `failures=0 handoff=1` at X −11000 (`Local/phleg-hills-hellfall.log`).
  - Stills in `Local/phleg-hills3/`, with contact sheets `sheet_spawns.png` and `sheet_fall.png`. Every spawn stands on ground.
- **Package:** `CHUCK_PACKAGE_VERIFIED passes=182` on the first run (`Local/phleg-hills-package.log`, `Local/verify-package-20261115-200122.log`).
- **Not tested:** anything by hand.
- **Remaining flaws:**
  - The hills read as a smooth, dark heightfield; they need rock detail and scatter.
  - The falls are still flat sheets above the splash.
  - The cracks are a little uniformly bright.
  - The cutscene's FromGround shot is dark (it was already dark before this work).
  - The ship's hidden-hull fault is open.
- **Contract compatibility:** no rig, scale or character changes. The `EPhlegSpawn` enum is reordered and extended, so old Phlegethos checkpoint saves map to different points. There is one new trial; one material changed.
- **Launcher:** promoted as PhlegethosHills. The receipt names 832f7087 because the package was built from the working tree before this commit. The previous package is kept; the PhlegethosWastes backup was pruned; the candidate folder was removed.

Next part of the work can be done by Claude only (flame skulls and hill creatures, lava-fall thickness, the ship's hidden-hull fault); preference: Claude, here.

## November 16 — Phlegethos lava from CC0 scans: crust plates, molten pools, streaky falls (Claude)

- **Asked:** "Download and implement free resource(s) to make all the lava look better both lava sea and flowing lava things". The user approved both ambientCG sets and gave two goal images (`References/Phlegethos/phlegethos-goal-lava-columns.png`, `phlegethos-goal-lava-fall.png`).
- **Source:** `main` at 9fe65ba9; this commit.
- **Downloaded (CC0, ambientCG):** Lava001_2K-JPG (34 MB zip) and Lava005_2K-JPG (35 MB zip); colour, emission and normal (DirectX) kept in `SourceAssets/Setting/Phlegethos/` (about 43 MB), SHA-256 in `manifest.json`. Zips cached in `Local/AmbientCG`. No software installed.
- **Changed:**
  - `Tools/Fetch-Phlegethos.ps1`: adds the Lava001/Lava005 records to an existing manifest and extracts them from the zips.
  - `Tools/create_phlegethos_assets.py`: an `emit` texture role (sRGB colour); `M_PhlegLava` rewritten. Lava004 is no longer used by it.
    - The lake: rounded dark crust plates (smooth-minimum Voronoi edge) on a net of thin bright veins, widening into open molten pools with swirling lines from the Lava005 emission; fades to the Lava001 scan from 60 m and to coarse layers further out, so there is no tiling grid at the horizon.
    - Flows, rivers and falls (vertex colour G = speed): bright melt with darker red streaks from both scans, moving with the speed.
    - The old diagonal pulse stripes are gone; molten base colour is near black so only emission shows.
  - New assets: `T_Lava001_2K_JPG_{Color,Emission,NormalDX}`, `T_Lava005_2K_JPG_{Color,Emission,NormalDX}` (Lava005 Color is imported but unused); `M_PhlegLava` rebuilt. The other Phlegethos materials and textures the script re-saves were restored unchanged.
  - `docs/regions/PHLEGETHOS.md`: free resources and a lava section.
- **Tested (editor):** Phlegethos 14/14 (`Local/phleg-lava-trials.log`); fall cutscene `failures=0 handoff=1` (`Local/phleg-lava-hellfall.log`); 54 stills in `Local/phleg-lava/`, reviewed against both goal images.
- **Package:** `CHUCK_PACKAGE_VERIFIED passes=182` on the first run (`Local/phleg-lava-package.log`, `Local/verify-package-20261115-164118.log`).
- **Not tested:** anything by hand.
- **Remaining flaws:**
  - The falls are flat sheets: no spray, splash or steam at their foot.
  - The walkable basalt keeps its straight hex glowing cracks; the columns are not yet the goal's rocky columns with glowing feet.
  - The ship's hidden-hull fault the playtester found is open.
- **Contract compatibility:** no rig, scale or character changes; six new textures, one material changed.
- **Launcher:** promoted as PhlegethosLava. The receipt names 9fe65ba9 because the package was built from the working tree before this commit. The previous package is kept at `Builds/Windows-Previous-20261115-PhlegethosLava`, the PhlegethosLook backup was pruned, and the candidate folder was removed.

Next part of the work can be done by Claude only (lava fall spray/steam and the goal's columns, the ship's hidden-hull fault, Phlegethos creatures); preference: Claude, here.

## November 16 — Phlegethos: geysers fixed, badlands, the wastes before the fortress (Claude)

- **Asked:**
  - The first challenge looked like "a weird flat at the top orange funnel that springs up", and "invisible things hit me".
  - "Add more inaccessible volcanic terrain ... less giant sections of lava", more cinematic for the cutscene's reveal, with setting-appropriate Phlegethos features.
  - Mid-task: "don't add the swirling fire things".
  - The fortress's staging area was "too close": a longer stretch of open, natural volcanic land with a path winding to the fortress, "less contrived block feel".
- **Source:** `main` at 5fad64c4; this commit.
- **Changed:**
  - `PhlegethosSetting.h/.cpp`:
    - The geysers are a visible fountain arcing onto a warning mark, and the burn is only there.
    - The badlands heightfield (no collision): river channels, pools, 46 lava flows, and the city of fire on a mesa 2 km out.
    - Fire rain.
    - The near mesa boxes removed.
    - The wastes: `GateX` moved from 28700 to 44000, plus `ForecourtX`, `ApproachPathY` and `ApproachGround`; a solid heightfield, the flagstone path, basalt column clusters, tors, fissures, stakes and braziers, steam. The idols are moved to the forecourt.
    - `InPhlegethos` extended to the new gate.
  - `PhlegethosTest.cpp`:
    - New `approach` trial (14 trials).
    - Geyser stills.
    - The gate and approach trials break on the frame they place him: a stale timer from the long walk had ended `gate` at once.
  - `PhlegethosFall.cpp`: the crane rises to 56 m and is reframed.
  - `Tools/create_phlegethos_assets.py`: new `M_PhlegMolten`.
  - `Tools/Verify-Package.ps1`: Phlegethos now expects `passed=14`.
  - The fire whirls were built and then removed at the user's request (no trace left).
- **Tested (editor):**
  - All 14 Phlegethos trials pass (`approach` worst off path 47 cm, 71 s); evidence `Local/phleg-trials-wastes.log`.
  - Fall cutscene: `failures=0 handoff=1` (`Local/phleg-hellfall-wastes.log`).
  - Stills are in `Local/phleg-wastes/`.
- **Package:**
  - The candidate built (`Builds/PhlegethosCandidate`), but three verification runs each failed on a different check outside Phlegethos, all timing or random-seed dependent:
    1. Waterdeep world audit, floating 118/117: an NPC's pipe smoke wisp.
    2. Chult sound audit, leaks 24/23: three more raptor steps and temple trap sounds than usual.
    3. ShipArrival playtester, `inside=1`: Chuck inside `ShipHidden` collision mid-jump, random seed -642331038.
  - The Phlegethos package steps come after these and did not run.
  - Logs: `Local/phleg-wastes-package.log`, `Local/phleg-wastes-verify2.log`, `Local/phleg-wastes-verify3.log`.
- **Not tested:** anything by hand; the Phlegethos and hell-fall checks in the package.
- **Remaining flaws:**
  - The wastes are dark between the braziers, and the walk is 70 s at a run.
  - The flagstones are plain slabs.
  - Boxes remain: the fortress walls, the mesa and the lake shores.
  - The far hills are smooth.
  - The ship's hidden-hull fault the playtester found is open.
- **Contract compatibility:** no rig, scale or character changes. One new asset, `M_PhlegMolten`.
- **Package rerun:** the fourth verification on the unchanged candidate passed every step, Phlegethos and hell fall included (`CHUCK_PACKAGE_VERIFIED passes=182`; `Local/phleg-wastes-verify4.log`, `Local/verify-package-20261115-120433.log`). The earlier failures were flaky checks in other regions.
- **Launcher:** promoted as PhlegethosWastes (receipt 5862442a). The previous package is kept at `Builds/Windows-Previous-20261115-PhlegethosWastes`, the 11-14 Phlegethos backup was pruned, and the candidate folder was removed.

Next part of the work can be done by Claude only (the ship's hidden-hull fault, Phlegethos creatures); preference: Claude, here.

## November 15 — Phlegethos second look pass and hell lightning (Claude)

- **Asked:** "start making plethegos look closer to goal"; "add some hell lightning too, overall more immersive and cool".
- **Source:** `main` at 4c87d9bc; this commit.
- **Changed:**
  - `PhlegethosSetting.h/.cpp`:
    - Crags: faceted rock skins over the slabs, the cliffs, the cleft, the far ridge, the chimney and the mesa. The collision stays plain hidden boxes, so the traversal is unchanged.
    - Lightning: `Strike()` and `TickLightning()`. Forked midpoint-displacement bolts as camera-facing ribbons, sky and plume strikes, return-stroke pulses, a sky flash toward the bolt, an unshadowed flash light, and thunder delayed by distance.
    - Fog, exposure and bloom retuned. The sun is redder.
    - Lava lights now fade over rock, so no glow pools sit on the path.
    - The volcano and the ranges were rescaled and moved out, and the lava-sea rings extended.
    - The plume rises and grows.
  - `PhlegethosTest.cpp`: the stills strike lightning in the rat shots.
  - `Tools/create_phlegethos_assets.py`:
    - Lava rewritten with crust plates, seams and flow streaks.
    - Basalt: a sin-free hash, glowing cracks only on steep faces, and an under-glow.
    - Path under-glow.
    - Sky rewritten dark, with a flash term.
    - New `M_PhlegBolt`, an unlit additive material.
  - `docs/regions/PHLEGETHOS.md`: the second look pass and the known gaps.
- **Tested:**
  - Editor: Phlegethos 13/13 with the crags (`Local/phleg-trials-look.log`).
  - Fall cutscene: ends at 62.56 s, handoff=1 (`Local/phleg-hellfall-look2.log`).
  - Stills are in `Local/phleg-look2/`.
  - Candidate: `CHUCK_PACKAGE_VERIFIED passes=182` and `CHUCK_CANDIDATE_VERIFIED` (`Local/verify-package-20261115-005220.log`, `Local/phleg-look2-package.log`).
- **Not tested:**
  - Anything by hand.
  - The thunder audio was not auditioned.
  - The editor trials weren't re-run after the last light-fade, plume and material changes; the package run covers them.
- **Remaining flaws:**
  - The fortress walls and plinths are still plain boxes, and there's no stepped stone path.
  - The idols are blocky.
  - The plume reads as one dark mass.
  - The lava falls are flat, and the far ranges are smooth.
  - The bolt material logs one harmless warning (`apply_fogging` does not exist).
  - The receipt names 4c87d9b, but the package was built with this commit's uncommitted changes.
- **Contract compatibility:** no rig, scale or character changes. One new asset (`M_PhlegBolt`) and four rebuilt Phlegethos materials.
- **Launcher:** promoted as PhlegethosLook. The previous package is kept as `Windows-Previous-20261115-PhlegethosLook` and `Windows-Previous-20261114-Phlegethos`; PlaytestFixes was pruned.

Next part of the work can be done by Claude only (Phlegethos creatures and the trio vs the Pit Fiend); the remaining art polish (fortress walls, stepped path, idols, plume, falls) by either — preference: Claude.

## November 14 — Phlegethos and the fall into it (Claude)

- **Asked:** build the fall to Phlegethos (the fall-to-Chult score, "a bit more of the song creating a slightly longer cut scene") and Phlegethos itself, which should "look really cool" with challenges that "draw on Chuck's parkour in ways that are fun to play"; not the trio vs the Pit Fiend yet; download free resources. Tracks supplied: `Phlegethos.wav` and `Phlegethos_2.wav` (for the intense part). Later, three goal-art images for the look.
- **Source:** `main` at 816d13d1 (with a4a8d2f5); commits 0f8f19fd, 9a8700b5 and this one.
- **Changed:**
  - New `PhlegethosSetting.h/.cpp`: the stage, its hazards, light, embers and smoke.
  - New `PhlegethosFall.h/.cpp`: the cutscene.
  - New `PhlegethosTest.cpp`: 13 trials, plus `-ChuckHellFallTest`.
  - New `Tools/Fetch-Phlegethos.ps1` and `Tools/create_phlegethos_assets.py`: CC0 textures, the two scores and six materials under `/Game/Art/Phlegethos`. The goal art is in `References/Phlegethos/`.
  - `ChuckCharacter`: `BeginHellFall` and `EndHellFall`. Lava is an Astral death, with respawn at the stretch start.
  - `ShipEndingScene.cpp`: its end leads into the fall instead of the end card.
  - `DockMenu.cpp`: six checkpoints.
  - `DockGameMode.cpp`: the two scores, crossfading. The duck test now expects 15 tracks.
  - `ShipEnding.cpp/.h`: up to 1.5 s in the air keeps his deck situation. A sea knock had restarted the 30 s count to the captain's clear order (a package run failed `orders`).
  - `Verify-Package.ps1` and `Verify-Menu.ps1`: the new steps and points.
- **Tested:**
  - Editor: Phlegethos 13/13, fall cutscene (ends at 62.56 s, handoff=1), ship ending 10/10 after the guidance fix.
  - Candidate `Builds/PhlegethosCandidate`: `CHUCK_PACKAGE_VERIFIED passes=182` and `CHUCK_CANDIDATE_VERIFIED`. Phlegethos 13/13, hell fall, ship ending, every audit within its limit.
  - Three earlier candidates failed before the fixes: `lake_hops` (the tall column), the ship's `orders`, and the duck test's track count.
  - Evidence: `Local/verify-package-20261114-215201.log`, `Local/verify-phlegethos-20261114-230114.log`, `Local/verify-hell-fall-20261114-225948.log`, `Local/verify-ship-ending-20261114-225645.log`, `Local/phleg-package4.log`, `Screenshots/.../HellFall/`.
- **Not tested:**
  - Anything by hand.
  - Verify-Menu for the six new points (Verify-Package's menu test counts them).
  - The sea-knock fix in the exact failing case (the knock didn't recur in the editor run).
- **Remaining flaws:**
  - No creatures; no audit baselines for Phlegethos.
  - Cliffs and fortress walls are mostly flat-faced blocks; no stepped stone path from the second goal image; the idols are blocky.
  - The geysers hit hard.
  - The receipt names 9a8700b, but the package was built with the uncommitted guidance and duck-test fixes now in this commit.
- **Contract compatibility:** no rig, scale or character asset changes. New assets only, under `/Game/Art/Phlegethos` and `/Game/Art/Audio/SW_Phlegethos*`.
- **Launcher:** promoted as Phlegethos (the previous package kept as `Windows-Previous-20261114-Phlegethos`, older pruned to two).

Next part of the work can be done by Claude only (Phlegethos creatures and the trio vs the Pit Fiend are Claude's); the setting's art polish could be done by either — preference: Claude, while the stage code is fresh.

## November 14 — the playtester's ship faults fixed (Claude)

- **Asked:** "Yes, fix them": the two ship faults the robot playtester found (the crawlway mouth, the hull under the gunport).
- **Source:** `main` at 816d13d1.
- **Changed:**
  - `ChuckCharacter.cpp/.h`:
    - **No hanging inside things.** New `FindHangClear`/`HangFits`: an edge is caught only if there is a spot to hang from it that is clear (3 cm tolerance; his feet may rest up to 16 cm in a floor right under him, as at the low quarterdeck rail). Checked by every grab path: the wall-jump catch, the drop hang, the hang hop, the edge grab and the wall-run catch. `EnterHang` uses the spot that check found when the strict one finds none (`CHUCK_HANG_TIGHT ... out= lower=`), never the plain hold further in.
    - **Safety net:** half a second inside solid on his feet (walking or crawling gaits) and he is moved to the nearest clear spot (`CHUCK_UNSTUCK`). The playtester reports inside after 3 frames, so it still sees what this hides from a player.
    - A refused edge logs `CHUCK_HANG_REFUSED` with what was in the way.
  - `ShipSetting.cpp`: the crawlway's crawl spaces stop at its 44 cm roof (they reached 80 cm up, so standing inside the boards over it he went down on all fours and could never get out).
  - `DockNPCPirate.inl`: the galley chef's steps stop at Chuck (they swept against walls only, so his capsule walked into Chuck's; found by playtest seed 3).
  - `Tools/Verify-Package.ps1`: the ship's playtest limit is now 0 (was 2).
  - The defect catalog: the findings marked fixed, with causes.
- **Causes found:** the crawlway mouth came from a hang off a timber's side on top of the stack (no room under it) and a climb up the stack's face, then the too-tall crawl space. The hull fault was a sprint leap caught on the gunport's sill, hung at the plain hold inside the hull.
- **Tested:**
  - Editor playtests on the ship, seeds 0–7 and the default: no inside, stuck or void faults after the last change (seeds 7 and 0 rerun on the final code; 1–6 on the code before the hang-spot change).
  - Editor ship trials rail, rail_climb, ratlines, mast_climb, rigging: 5/5. The first candidate failed `rail_climb` (the quarterdeck rail's hang has his feet in the deck); fixed with the floor allowance.
  - Candidate `Builds/AuditCandidate`: `CHUCK_PACKAGE_VERIFIED passes=182` and `CHUCK_CANDIDATE_VERIFIED`; playtest 0/0, 0/0, 0/0; every world, motion and sound line within its limit.
  - Evidence: `Local/verify-package-20261114-133510.log`, `Local/audit4-candidate-build.log`, `Local/railclimb-editor.log`.
- **Not tested:** the hang change by hand; other regions' climbing only through the package's trials (all passed).
- **Remaining flaws:** the playtester still covers one start point per region; the sewer zombie's steps (`StepZombie`) also sweep against walls only, like the chef's did (not seen to fail; not changed).
- **Contract compatibility:** no rig, scale, material or asset-path changes. Traversal behaviour change: edges with no clear hanging spot are no longer caught.
- **Launcher:** promoted as PlaytestFixes (see the receipt).

Next part of the work could be done by either — preference: Claude (step 4 of the catalog, the import checks, or the playtester on more checkpoints).

## November 14 — the robot playtester (Claude)

- **Asked:** "When you're ready go ahead and do your next step too": step 3 of the defect catalog's build order, the robot playtester (category 2: Chuck inside geometry, stuck, or out of the world).
- **Source:** `main` at f4b7005a.
- **Changed:**
  - **New `PlaytestAudit.cpp`:** `-ChuckPlaytest[=<checkpoint>]` drives Chuck for 90 s at a fixed 30 fps with seeded random input (headings scored on clearance, visited cells and a leash home; jumps, sprints, dodges, edge leaps). It reports:
    - **inside:** his capsule (shrunk 4 cm) overlaps blocking geometry, outside the placed gaits (hang, climb, vault, swing, ladder, held, thrown, Astral);
    - **stuck:** no more than 60 cm of movement in 8 s while trying to move;
    - **void:** falling 4 s with no floor below; falls into water are listed separately.
    - After a fault he is put back at a safe spot from about 8 s earlier. A planted cube 300 m up must be found as "inside", or the run fails.
    - Repeatable: `-ChuckPlaytestSeed=<n>`, `-ChuckPlaytestAt=X,Y,Z`. Writes `Saved/Audit/<Region>-playtest.csv`, a state line per fault and a screenshot in `Screenshots/Playtest/`.
  - `DockGameMode.cpp`: the audit switch takes the new kind.
  - `ChuckCharacter.cpp`: **bug fix** — `GetGaitName()` had no name for `Thrown` (22 names for 23 gaits, read past the table). Added, with a `static_assert` so it can't drift again.
  - `Tools/Verify-Package.ps1`: runs the playtester on Waterdeep, Chult and the ship; needs the probe and at least 20 cells; faults may not exceed 0/0/2.
  - The defect catalog: a playtester section with the findings and limits; step 3 marked done.
- **Tested:**
  - Editor runs on all three regions, several seeds; the probe is found every time.
  - Candidate `Builds/AuditCandidate`: `CHUCK_PACKAGE_VERIFIED passes=182` and `CHUCK_CANDIDATE_VERIFIED`. Playtest Waterdeep 0/0 (84 cells), Chult 0/0 (99), ship 0/2 (29). Every world, motion and sound line within its limit (the sound leaks varied: Waterdeep and ship 5 against a baseline of 4, within the +3 margin).
  - Evidence: `Local/verify-package-20261114-004605.log`, `Local/audit3-candidate-build.log`, `Local/playtest-ship-hull-inside.png`, `Local/playtest-ship-crawlway-stuck.png`.
- **Remaining flaws (findings, not fixed):**
  - **Ship crawlway mouth** (local 107,−325, 21 cm above the lower deck): crouched, he is inside the stone/hidden blocker and the crawl restarts every frame. Repro: `-ChuckPlaytest=ShipArrival -ChuckPlaytestAt=400107,1699675,386`.
  - **Ship hull under the gunport** (seed 7, t≈20.9 s): after landing he is partly inside the hull planking.
  - Waterdeep's dock edge gives 2–3 falls into the harbour per run (by design; not counted).
  - Limits: one start point per region, 90 s; the main route and other checkpoints aren't covered. The placed gaits aren't checked for being inside.
- **Contract compatibility:** no rig, scale, material or asset-path changes.
- **Launcher:** promoted as Playtest (see the receipt).

Next part of the work could be done by either — preference: Claude (the two ship faults are traversal/collision); step 4 of the catalog is open to either.

## November 13 — the ship's deck funnel removed; the motion and sound audits (Claude)

- **Asked:** "Remove the chimney/pipe on the ships deck then do step 2. Leave the crocks and plates for now". Step 2 of the defect catalog's build order is the motion audit (category 5) and the sound audit (category 6).
- **Source:** `main` at 540d53c7.
- **Changed:**
  - `ShipSetting.cpp`: the galley stove's funnel ends in a collar under the deck. The funnel above the deck, its bands, rain cap and stays are gone.
  - **New `MotionAudit.cpp`:** `-ChuckMotionAudit[=<checkpoint>]` records every shown skinned mesh within 60 m of Chuck at a fixed 30 fps for 15 s while he idles, walks, stops and slashes. It reads the bones in component space and finds one-frame pops and jitter.
    - A self-test on a made-up track must pass, and Chuck's own walk must register.
    - Writes `Saved/Audit/<Region>-motion.csv`.
  - **New `SoundAudit.cpp`:** `-ChuckSoundAudit[=<checkpoint>]` finds up to 80 places behind solid, near each sound source, and puts the listener there. It reports sounds heard through solid when no detour around the obstacle is clear.
    - A planted probe sound must be heard at least half the time.
    - Writes `Saved/Audit/<Region>-sound.csv`.
  - **New `Tools/Check-VoiceLoudness.py`** (standard library): the speech level of every voice file. It flags lines off their speaker, speakers off the rest, and clipping.
  - `WorldAudit.cpp` exports the allow-list check (`motion` and `sound` entries in `Content/Data/Audit/Allow.txt`).
  - `DockGameMode.cpp`: the audit switch takes the two new kinds.
  - `Tools/Verify-Package.ps1` runs both audits on Waterdeep, Chult and the ship:
    - motion limits are mesh counts (4/1/7, +1);
    - sound limits are leak counts (4/20/4, +3), with the probe required;
    - the voice report is information only.
  - `DockPantry.cpp`: the demo skip test checks 1.5 s after Esc instead of 1 s.
    - It failed 1 run in 3, on the launcher package too.
    - Cause: the skip's 0.5 s fade ended on the same frame as the check when frames take 400 ms.
  - The defect catalog has the new section, findings and limits.
- **Tested:**
  - Editor runs of all three regions for both audits.
  - Candidate `Builds/AuditCandidate`:
    - `CHUCK_PACKAGE_VERIFIED passes=182` and `CHUCK_CANDIDATE_VERIFIED`; every world, motion and sound verify line is within its limit.
    - Evidence: `Local/verify-package-20261113-222806.log` and `Local/audit2-candidate-build2.log`.
    - The ship's world audit stays at 0/0 after the funnel change.
  - An earlier verify of the same code failed once on the alchemist's wrist gap (8.1 cm against a limit of 8; 4.3–4.6 cm in every other run). It is a single-moment sample of his idle; not changed.
- **Not tested:** the deck by eye after the funnel removal (only the audits and the ship tests ran).
- **Remaining flaws (findings, not fixed):**
  - **Pops:**
    - the tavern keeper's hand and the ship dancer's foot (loop seams);
    - the blacksmith's brow and forearm; a sailor's forearm;
    - the fencers' thumbs; the concertina player's and the cheering pirate's calves; the chef's fingers;
    - Chuck's right calf, 5.2 cm in one frame of the slash (packaged only).
  - **Sound:** the temple interior heard 7–11 m above ground in Chult; ship rats and the fencing heard through the deck; the anvil, a rat and a sewer zombie through Waterdeep's walls.
  - **Voice:** the blacksmith is +5.5 dB over the other voices; the wizard's `fireball_01` is −10 dB under his other lines.
  - **Gaps:** cutscenes aren't recorded, and Chuck's walk-start pop isn't caught. Voice level in the running game, and against the music, isn't measured.
- **Contract compatibility:** no rig, scale, material or asset-path changes.
- **Launcher:** promoted as MotionSound (see the receipt).

Next part of the work could be done by either — preference: Claude (step 3, the robot playtester, is under way); the pops and leaks above are fixes for whoever owns each region.

## November 13 — the world audit: collision and support checks on every map (Claude)

- **Asked:** "Yes, start step 1": step 1 of the defect catalog's build order. The collision audit and the support (floating) audit run on every map and are added to Verify-Package.
- **Source:** `main` at 8e186db9.
- **Changed:**
  - **New `WorldAudit.cpp`** (replaces `WaterdeepCollisionAudit.cpp`, now deleted). `-ChuckWorldAudit[=<checkpoint>]` builds that region's checkpoint and runs two checks:
    - **Collision gaps:** visible static meshes from 25 cm to 30 m that sit on the floor but don't block Chuck, with no blocker inside them. The test uses overlaps plus four rays in the walking band, so hidden triangle-mesh blockers count.
    - **Floating:** touching pieces are grouped. A group is flagged when nothing holds it: no big anchor mesh (not one enclosing it), no procedural-mesh chunk and no collision.
    - **Planted probes:** each run plants a walk-through cube and a floating cube, so the audit can't pass while blind.
    - **Allow list:** `Content/Data/Audit/Allow.txt` (UFS).
    - **Output:** `Saved/Audit/<Region>-findings.csv`, plus a `-ChuckShots` list of views.
  - `-ChuckCollisionAudit` still works for Waterdeep, so its old log line remains.
  - `DockGameMode.cpp`: the audit switch.
  - `Tools/Verify-Package.ps1`:
    - Runs the audit on Waterdeep, Chult and ShipArrival first.
    - Requires both probes and enforces a baseline ratchet: counts may not rise. Waterdeep gaps 18, floating 117; Chult 0/0; ship 0/0.
    - `-WorldAuditOnly` runs only the audit.
  - **Fixes the audit found:**
    - `DockPlaza.cpp`: the tower cap was walk-through and is now solid.
    - `ChultTemple.cpp`: the fallen palms get a hidden blocking capsule. The back-band trees, the rear terrace tree and the pyramid and corner trees now block.
  - The defect catalog has a world-audit section and a findings table.
- **Tested:**
  - Editor runs of all three regions: probes 2/2 each; final counts as above. Chult started at 46,468 gaps before the allow list and fixes.
  - Candidate package `Builds/AuditCandidate`: the packaged audit gives the same counts. `CHUCK_PACKAGE_VERIFIED passes=182`, `CHUCK_CANDIDATE_VERIFIED`; evidence `Local/verify-package-20261113-172628.log` and `Local/audit-candidate-build.log`.
- **Not tested:** whether the new Chult tree and palm collision feels right in play. The verify run's temple tests pass, but nobody has walked the jungle by hand.
- **Remaining flaws:**
  - Waterdeep's 18 gaps are all the pantry shelf's plaster crocks (`DockPantry.cpp:304`). Whether they should block is the user's call.
  - Waterdeep's 117 floating findings are a triage list, not all real. Most are the repair masonry plates at `DockSetting.cpp:196`, 4–8 cm off the ground and 12 cm off the wall; the rest are roof stones, rails, forge iron and bottles. Some are false positives where the supporting piece has no collision, hence the ratchet rather than 0.
  - Palm capsules aren't lowered by Chult's tree grounding, so on slopes they may sit a little high.
  - Spline meshes and landscape are skipped. Floating doesn't check NPC-held or animated props.
- **Contract compatibility:** no rig, scale, material or asset-path changes. New data file `Content/Data/Audit/Allow.txt`.
- **Launcher:** promoted as WorldAudit (see the receipt).

Next part of the work could be done by either — preference: Claude (step 2 of the catalog's build order); the crocks and the masonry plates need the user's call first.

## November 13 — the defect catalog (Claude, documentation only)

- **Asked:** the user asked whether agents could automate most of the remaining game, including checks for the kinds of problems they have found and corrected by hand. First step agreed: a catalog of those corrections.
- **Source:** `main` at 69838d6f (after the `claude/improvements` merge; that session asked me to wait for it, and I did).
- **Changed:** new [../AUTOMATION-DEFECT-CATALOG.md](../AUTOMATION-DEFECT-CATALOG.md): 12 categories mined from the git history and the handoffs' requests, each with past cases, the checks that exist now, a proposed general audit and whether it is numeric, visual or needs the user; the November 12 list scored by hand (of 20 defects, about 12 numeric, 3 visual, 5 for the user); a build order.
- **Tested:** nothing to run; documentation only. The scoring is my judgment, not a run of any audit.
- **Contract compatibility:** no code, rig or asset changes.
- **Launcher:** unchanged (PauseSave).

Next part of the work could be done by either — preference: Claude (step 1 of the catalog's build order: the collision and support audits for every map, added to Verify-Package).

## November 13 — saving and a pause menu; the review's improvements (Claude)

- **Asked:**
  - "Begin on all improvement suggestions" from my review of how the game is built.
  - A save option in a pause menu. Later pause-menu options will turn the tutorial text and the golden guide lights on or off.
- **Source:**
  - Written on branch `claude/improvements` (worktree `C:\GameDev\CHUCK-3D-improve`) while another session had unfinished work on main.
  - Merged into main at 201b8913 over 65955119, then finished on main.
- **Changed:**
  - **Save and pause menu:**
    - `ChuckSave.h/.cpp`: one slot at `%LOCALAPPDATA%\CHUCK3D\SaveGames\Chuck.sav`, outside the build so promotions keep it. `-ChuckSaveSlot=` selects another slot for tests.
    - `DockMenu.cpp`:
      - `ADockGameMode::CurrentSavePoint` matches Chuck's death-recovery point to the nearest named dev checkpoint in the regions built so far. The docks after the sewer map to Night.
      - `SaveGame` and `ContinueFromSave`.
      - `ADockHUD::ShowPause/ClosePause/CanPause`: Esc or gamepad Start, only when his own camera is on him (no cutscene, letterbox or demo end) and not in `-Chuck` verification runs.
      - Pause menu: Resume, Save Game, Quit Game, and a "Last save / Saved: …" line.
      - Title menu: Continue when a save exists. The menu test expects 3 buttons when one does.
    - `-ChuckPauseTest[=<checkpoint>]`.
    - `ChuckCharacter.h`: `SetCigarettes`.
    - `ChuckCharacter.cpp` and `DefaultInput.ini`: the old "Quit" action on Esc/Start is gone. It quit the game outright in play and fired alongside the pause menu. Esc still exits at the demo's end card and skips cutscenes.
  - **Shot lists:**
    - `ChuckShots.h/.cpp` and `Tools/Take-Shots.ps1`: `-ChuckShots=<file>` takes stills from a text list with no recompile. A list can name a checkpoint, the evening, fov, HUD on or off, cameras, Chuck placed for scale, and an `audit` step.
    - The tavern bin's hand-written `-ChuckBinCapture` block was replaced by `Tools/shots/tavern-bin.txt`.
  - **Placement table:**
    - `ChuckPlacements.h/.cpp` reads `Content/Data/Placements/<Region>.txt`, staged as UFS via `DefaultGame.ini`.
    - The tavern bin moved there from `DockSetting.cpp`.
  - **Light audit:**
    - `LightAudit.h/.cpp` lists visible shadowless lights by their estimated light on a groom (threshold 100), skipping everything while voxels are on.
    - `Tools/shots/audit-*.txt` run it at seven checkpoints. `pantry-fur.txt` has stills of its strongest candidate.
  - **Smoke test:**
    - The 7 old editor failures ("…Art imports render separately from collision") were real: in uncooked runs the art meshes kept their own collision.
    - `DockGameMode.cpp`'s `Prop` now sets `bUseDefaultCollision=false` and the NoCollision profile. A failure now logs `CHUCK_PROP_ART_DETAIL`.
  - **Housekeeping:**
    - The 14 `Common/Materials` re-saves from Nov 5 are committed. Same sizes; every build since was made and verified with them.
    - October entries of HANDOFF.md and this file moved to `docs/archive/HANDOFF-2026-10.md` and `docs/archive/agent-handoffs/CLAUDE-2026-10.md`.
    - PROJECT-BRIEF's region table and open questions brought up to date (saving decided, naming done).
    - AGENT-WORKFLOW gained "Working efficiently (2026-11-13)", and PLAYING documents saving.
  - **Local storage:**
    - `git lfs prune --verify-remote` took `.git/lfs` from 21 GB to 3.6 GB.
    - It first found one old object never uploaded to GitHub (a version of `SourceAssets/NPCs/Humans/Textures/short02_diffuse.png`). It was pushed before pruning.
- **Tested:**
  - Pause test at Waterdeep, ChultTempleHall and ShipHold: failures=0 each (`Local/pause-test-*.log`). Stills in `Local/pause/` (`Waterdeep_menu.png` is the menu after saving).
  - `Take-Shots` with `tavern-bin.txt`: 5 stills, no HUD (`Local/shots/tavern-bin/`).
  - All seven audits ran (`Local/shots/audit-*.log`):
    - The sanctum is skipped (voxels on).
    - The ship flags one cabin lamp on one crew member.
    - The docks at night flag the tavern lamps on the keeper (about 130–150) and street lamps on two NPCs.
    - The pantry flags its purple Astral lights on Chuck's fur (about 1,200). The stills (`Local/shots/pantry-fur/`) show his fur magenta-pink on all sides. The user chose to leave it as it is (11-13).
  - Editor smoke test (`Tools/Test-Game.ps1`): failures=0, previously 7.
  - Packaged `Builds/PauseSaveCandidate`:
    - The first Verify-Package failed only on the temple wind's full run (one Astral death at leg 7). The same package's wind test alone then passed (full=1, 0 deaths; `Local/verify-temple-wind-rerun.log`). A flaky trial: that first run overlapped the 19 GB LFS prune, and packaged traversal trials fail when the machine runs slow.
    - A full rerun of Verify-Package on a quiet machine then passed: `CHUCK_PACKAGE_VERIFIED passes=182` (`Local/verify-package-20261113-122829.log`).
- **Not checked:**
  - Nobody has played it by hand, including the pause menu with a real keyboard or gamepad. The test sends Esc through player input, not through Slate's focus routing.
  - Verify-Menu was not run.
  - Continue from a save made aboard during the storm restarts the storm checkpoint fresh.
- **Remaining flaws:**
  - The save keeps the checkpoint, morning or evening, cigarettes and sanity only. Collected pickups return.
  - The light audit's brightness estimate does not separate reported from unreported cases. It is a list to look at, not a pass/fail.
- **Contract compatibility:** no rig or animation changes.
- **Binary/LFS:** the 14 re-saved Common/Materials only.
- **Launcher:** promoted as PauseSave. The receipt names 201b8913 (the merge); the build also contains the uncommitted post-merge edits that are committed right after it, so this work matches the launcher. Previous package kept as `Windows-Previous-20261113-PauseSave`; the candidate folder is gone.

Next part of the work could be done by either — preference: Claude (the pause menu's tutorial-text and guide-light options).

## November 12 (night) — the squiggle; thrown by the leg (Claude)

- **Asked:** "the squiggle look is much worse/more frequent now, please fix"; the skeleton to pick Chuck up by the leg and huck him, with realistic flight, crash landing and a forceful, damaging wall hit.
- **Source:** `main` at 7c513d05.
- **The squiggle, measured:**
  - The tail-twist stripping from earlier today is off; it stays behind `-ChuckTailTwistFix` only, for comparison.
  - New detectors in `ChuckCharacter.cpp`, logged each frame:
    - `CHUCK_TAIL_ZIGZAG`: alternating bends in the tail's bones. None found in either configuration, so the squiggle is not the skeleton bending.
    - `CHUCK_TAIL_JUMP`: the tail jumping between frames, measured in the body's own frame.
    - Both run only with `-ChuckTailWatch` or `-ChuckTailCapture`. Their per-frame bone reads were in play at first; in packaged runs that coincided with lower frame rates (about 39-43 fps against the usual 49) and with timing failures in the trap run and Astral wind traversal trials, so they were taken out of play.
  - The jump log found tail-only snaps, up to 18 cm at the tip in one frame with the body still (in Idle), and larger ones in cross-fades. Under TAA a jump like that smears into a squiggle for a moment.
  - Fix (`ChuckAnimInstance.cpp`): each tail bone follows its pose with a 45 ms lag and turns at most 360°/s. Tail-only snaps are gone from the smoke test.
  - What remains are whole-body pops at the start of the walk-start and slash cross-fades (the head moves 12-13 cm in one frame); the tail tip shows them most.
  - A fade begun during another fade now starts from a snapshot of the pose shown (`bSnapshotB`). That did not remove those pops, so their cause is still open.
  - `-ChuckTailCapture [-ChuckTailActions]`: close-up stills of the tail through walks, stops, turns, runs, a jump, a roll, a slash and a side jump (`ChuckTailCapture.cpp`).
- **Thrown by the leg:** see [../CHULT-TEMPLE-SKELETONS.md](../CHULT-TEMPLE-SKELETONS.md#by-the-leg-and-hucked-2026-11-12-later).
  - New Chuck clips `HeldLeg`, `Tumble`, `CrashLand` and `WallSlam` (`Tools/build_chuck_v1.py`; imported with `Import-ChuckV1.ps1 -Clips ...`). The other rewritten FBX files were restored from HEAD.
  - New skeleton clip `Throw` (CMU 124_01; `build_temple_skeleton.py`, `Tools/import_skeleton_clip.py`). `SkeletonClipData.h` gained only the Throw constants; the other rewritten FBX files were restored.
  - New `EGait::Thrown` and a body turn (`BodyQuat`) on the mesh.
- **Tests:**
  - Editor `-ChuckSkeletonTest`: 10/10. In `grab_lands` he is held, thrown at 9.5 m/s, crash-lands and gets up (`Local/throw3-free.log`).
  - With `-ChuckThrowAtWall`, two wall slams (2 cm and 82 cm up, sanity 5 to 4). The first throw went over an edge into a pit, a fall death, which failed that test-only trial (`Local/throw3-wall.log`).
  - Captures reviewed: `Saved/Screenshots/WindowsEditor/Skeleton/T3_*`, `Local/SkeletonWallShots`.
  - Smoke test with the tail lag (`Local/jump-smoke-limit.log`).
  - The first package's skeleton check failed: the random throw went over the pier hall's rift (a fall death). That is allowed in play, but under `-ChuckSkeletonTest` the throw now keeps its random heading only if it is open floor. It traces up to 11 m out, stopping at a wall, and needs floor within 50 cm of the skeleton's feet every metre; otherwise it tries the next of 16 headings. Two editor runs after the fix: 10/10 each (`Local/throw5-1.log`, `Local/throw5-2.log`).
  - Packaged (`Builds/ThrowCandidate`, now the launcher's):
    - `Verify-Package` 182 passes (`Local/verify-package-20261113-093937.log`). Its skeleton check now expects 9 clips (it was 8 before `Throw`).
    - Several earlier package runs overnight failed timing-sensitive traversal trials (trap run `pit_chain`, `leap_short`, `pit_twist`; Astral wind `full`). The machine was running at about 37-43 fps against its usual 49. The ListFix launcher package failed the same trap-run check at 41 fps (`Local/run-baseline-listfix.log`), so the code was not the cause.
    - After the machine woke from sleep the checks passed again (trap run 46 fps, `Local/run-after-wake.log`).
    - If those trials fail again, check the frame rate first.
- **Not checked:** not played by hand. The wall slam close to the floor reads modestly. The walk-start and slash cross-fade pops are not fixed.
- **Contract compatibility:** rig unchanged. New clips are appended at the end of `EClip`, and `EGait::Thrown` at the end of `EGait`.
- **Binary/LFS:** four Chuck clip FBX and uassets, one skeleton clip FBX and uasset, `Chuck_V1.blend`, and the BVH `124_01.bvh` (491 KB, CMU, recorded in the manifest and in `Fetch-CMUMocap.ps1`).
- **Launcher:** promoted as Throw (`Promote-Build.ps1`). Its receipt shows source 7c513d0 because the package was built from this work before its commit. The previous package is kept as `Windows-Previous-20261113-Throw` and ListFix as the second; ShipSpruce was pruned.

Next part of the work can be done here (play the throw and judge the squiggle by hand; the cross-fade pops if the squiggle persists).

## November 12 — the user's 23-item list (Claude)

- **Asked:** 23 adjustments across Waterdeep, Chult, the temple and the ship. Mid-way the user clarified the skeleton throw: a forceful throw in a random direction, wherever it lands, with only the throw's damage; a wall-slam animation may come later, after playing it.
- **Source:** `main` at d72817ae.
- **Changed, item by item:**
  1. Sewer zombies' rasps only on their own level (`DockNPC.cpp`: they were heard through the streets, 2D distance only).
  2. `-ChuckCollisionAudit` (new `WaterdeepCollisionAudit.cpp`): every visible stone, plaster, wood or brick box a rat could walk into with no collision. It found two: the side gate's stone jambs beside the sewer hatch. They and the lintel are now solid (`DockSetting.cpp`); a rerun found 0.
  3. The squiggle: twist is taken off the tail bones after the pose is sampled (`ChuckAnimInstance.cpp`). A rolled tail bone wrings the thin skinned tail into a corkscrew. Largest twist logged by the reruns: 15 degrees (a Roll). That is not proof the squiggle you saw is gone.
  4. Landing leaves (`FallCutscene.cpp`): each leaf keeps one instance. They were cleared and re-added every frame, and indices shifted, so leaves lying still took each other's last-frame place: the renderer's motion between frames made them twitch.
  5. New dinosaur roars, growls, calls, hisses and deaths (`Tools/build_dino_sfx.py`): real animals slowed down under the old voices. Six CC0 BigSoundBank recordings are used, in `SourceAssets/Audio/SFX/Dinos/Source/BigSoundBank`, credited, with hashes in its manifest. Unauditioned: I can't hear them.
  6. A fall into the gorge: 1.6 s past the old line (about 24 m), the view held at the lip, then the Astral (`ChuckCharacter.cpp`).
  7. Forward swing: stronger and wider after a back swing; on big leaps he lets go near the top of the forward swing. The stroke's best reach still decides the landing. Under boards (the bridge) the boost is 0.8x over 35 degrees and he lets go by 24 degrees, or his head rises through the planks (`docs/BRACHIATION.md`).
  8. The river's path attackers only where the road runs by the river (within 11 m of the bank): the five beyond about S 28000, where the road runs through the low trees, are gone. 67 zombies, was 72. The river test's counts are computed now, and its third dodge trial moved to S 26300.
  9. T-rex 15% larger; its stretch starts at S 39000 (was 35600), so the first two giants are outside it; its loop (20 legs) starts in the gap between the third and fourth giants.
  10. Dino faces: their head bone is ~2 cm long, so the spine-wrapped UVs gave the whole head one u and smeared the pattern down the face. UVs on the head are now along it and round a level axis (`build_raptor.py`, `build_trex.py`). Only the meshes were reimported, by the new `Tools/reimport_dino_meshes.py`, which keeps every slot's material (the old import scripts would reset the M_DinoSkin instances). The clip FBX/headers were unchanged.
  11. Temple interior sounds (skeletons, snakes) heard only from inside (`ChultTempleSoundAudible`); traps already were.
  12. The skeleton's stoop grabs him, lifts him for 0.55 s, holds him 0.25 s and throws him in a random direction at 640 cm/s out and 400 up (`ThrowFromHeld`). The grab's one sanity is the throw's damage; a last-sanity grab is a plain blow.
  13. Out of any sanctum pier (or the rubble map's copy) through its nearest face if he is ever left inside one (`ChultTempleSanctumPushOutOfPier`, logged `CHUCK_SANCTUM_PIER_PUSH_OUT`). Not reproduced here: the fling itself is swept, so whatever put him inside (a ledge or hang snap) is unknown.
  14. Rubble crevice glow about half again.
  15–19, 21–23. Ship: hammocks re-hung; captain's bed; galley stove (forge iron); the "simple lumber" pile (it was the spare sails); the seated pirate's voice played 1.45x; the storm heave integrated (it jumped by the whole time elapsed at every gust); the cap solid and no hole round the masthead; the binnacle hood solid (`docs/SHIP.md`).
  20. Ladders and nets: stick up climbs up, stick down climbs down, whichever way the camera looks; the way he got on holds while the stick is held (toward it up; walked off a top onto it, down). Tests that climbed down by pushing away from the net now press down.
- **Tests run:**
  - Editor, ship: 22/22 (`Local/t1112e-ship.log`; first run 20/22 before the ladder fixes). Skeleton 10/10, random throw seen (`t1112d-skeleton.log`). T-rex 4/4, route 20 legs (`t1112b-trex.log`). Bridge 11/11, head check passed at 9.2 cm (`t1112d-bridge.log`). River zombies 5/5 (`t1112b-river.log`). Fall cutscene passed, leaves all down by 2 s (`t1112-fall.log`). Giant canopy 72/72, canopy 48/48, raptor 8/8 (`t1112c-*.log`). Collision audit 0 gaps (`t1112b-audit.log`).
  - Sanctum: only the known intermittent `wizard_seal` failed (`t1112c-sanctum.log`).
  - Packaged `Builds/ListFixCandidate`: Verify-Package 182 passes (`Local/verify-package-20261112-180219.log`); ship 22/22 in the editor on the final source (`Local/t1112e-ship.log`). Verify-Menu was not run.
- **Not checked:** nothing played by hand; the sounds are not listened to; the pier push-out never fired in a test (no repro); the ladder change was not played with a controller.
- **Remaining flaws:** the stove's forge iron reads shinier and more streaked than cast iron; the dino neck shows a small mapping seam behind the skull; the stove's iron and the bed are procedural boxes and spars; item 2's picture is matched only through the audit.
- **Contract compatibility:** no rig, socket or clip changes (the dino meshes keep their skeletons; the clip data is unchanged).
- **Binary/LFS:** six source MP3s (about 1.5 MB), two FBX, two skeletal meshes, 16 sound waves, all LFS. The 14 `Common/Materials` re-saves are still not this work and stay uncommitted.
- **Launcher:** promoted as ListFix (receipt names d72817ae; this work is committed right after). `Windows-Previous-20261111-PreStormVoice` pruned.

Next part of the work could be done by either — preference: Claude (play the list by hand: the ladders, the bridge swing, the skeleton throw, the sounds).

## November 11 (evening) — the ship spruced up; up from any hang (Claude)

- **Asked:** the ship interior seen through the crevice to look like where he is heading (not the "3-boxed-grid"); a treasure chest; an ornate rug; nicer hammocks and sacks; pressing up while hanging pulls him up even onto tops he can't easily stand on (the ship's rails), and on the taller double rails from the middle bar up to the top one and onto it.
- **Source:** `main` at c7d0e684.
- **Changed:**
  - `ChuckCharacter.h/.cpp`: `TryHangUp` (room → pull-up; else `FindPerch` → stand centred on a narrow top; else `TryHangHop` → hang from the next edge up to 60 cm above, its top found a centimetre at a time). `FindLedge` accepts a 2.5 cm-deep top when the 8 cm look misses (`bThin`, off for the automatic mantle and scrambles). `StartClimb` takes an optional stand. Counters `GetPerchClimbs`, `GetHangHops`.
  - `ChultTempleRubbleBuild.inl` / `ChultTempleInterior.cpp`: the crevice's crack ragged; behind it the ship's lumber crawlway and compartment (new `FParts::Lumber`, `M_WoodLight`); two lights.
  - `ShipStorm.cpp` (domed chest lid, `ChestLid` 17), `ShipSetting.cpp` (chest body dress, carpet with UVs + fringe, hammocks, sacks; `M_ShipRug/M_ShipSack/M_ShipHammock`).
  - New `Tools/make_ship_rug.py` (stdlib only) → `SourceAssets/Setting/Ship/ShipRug.png`; `Tools/create_ship_furnishings.py` → `T_ShipRug`, `M_ShipRug`, `M_ShipSack`, `M_ShipHammock` (rough linen and caban textures already in the project).
  - `ShipTest.cpp`: trial `rail_climb` (22 trials), stills `ChestClose`, `CabinRug`, `Hammocks`, `HoldSacks`, `GalleySacks`, `ShipPlay_CreviceThrough(Back)`.
  - Outside my usual ownership: `Tools/Verify-Package.ps1` (Codex's tooling) now expects 22 ship trials, `rail_climb` passing and `wind_glows=56`.
- **Tested:** editor ship test 22/22 (`Local/ship-full.log`); packaged `Builds/ShipSpruceCandidate` Verify-Package 182 (`Local/verify-package-20261111-171956.log`), ship 22/22. The first package run failed only on the glow count and the trial count (expectations updated, same build re-verified). Verify-Menu was not run. Stills reviewed in the editor and the package.
- **Not checked:** not played by hand; other climbing regions are covered only by Verify-Package's existing trials (Waterdeep, Chult ruins, temple), which passed.
- **Remaining flaws:** perch and hop reuse the PullUp/Hang clips; the compartment through the crack is dim; the quarters' sea chests are plain; the blankets are flat navy.
- **Contract compatibility:** no rig, socket or clip changes.
- **Binary/LFS:** 4 new `.uasset` (LFS) and one PNG (LFS).
- **Launcher:** promoted as ShipSpruce (receipt names c7d0e684; this work sits on top of it, committed right after). `Windows-Previous-20261110-ShipFix` pruned. The 14 `Common/Materials` re-saves are still uncommitted (not this work).

Next part of the work could be done by either — preference: Claude (play the rails and the crevice by hand; dedicated reach-up/balance clips if the reused ones read wrong).

## November 11 — the ship's pre-storm lines voiced; the captain's pumps order (Claude)

- **Asked:**
  - Write ElevenLabs prompts for every ship NPC's pre-storm talk lines, then put the user's takes into the game.
  - Replace the captain's storm order BG_04 ("Starboard yer helm…"), which made no sense because he is at the helm himself.
  - The user cut and named every take by hand, so the audio files were used as delivered.
- **Source:** `main` at 01938373.
- **Changed:**
  - New takes under `SourceAssets/NPCs/Voice/<NPC>/source/`, built to `Voice/<NPC>/<id>.wav` by `Tools/build_npc_voice.py` and imported with `Tools/import_npc_voice.py`.
    - Seated, concertina, cheering and dancing pirates: first talk, second talk, then "Arrgh!" from the third talk on (`SEA_/CON_/CHE_/DAN_` ids; `*_ARR`).
    - Jeffries: JEF_P01–P02 the first time, JEF_P03–P04 every time after.
    - The captain: CAP_P00 first, CAP_P01 second, CAP_P02 "Arrgh!" after.
    - The chef: CHEF_01, shouted when he first sees the rat in his galley (the existing notice trigger).
    - BG_04 is now "MAN THE PUMPS! SOUND THE WELL AND SING OUT THE DEPTH! … SHE'S MAKIN' WATER, LADS! PUMP, DAMN YE!" (`source/BG_04_pumps.wav`).
  - `dialogue.json`, `manifest.json` and `NPCVoiceData.h` regenerated.
  - `DockNPC.h` / `DockNPCPirate.inl`:
    - `SetPirateVoice(Npc, First, Repeat, After)` swaps the voiced talk set as conversations finish.
    - `ShoutLine` plays a voiced line with its subtitle for the take's length.
  - `ShipCrew.cpp` gives each talker its voice sets. The captain can now be talked to before the storm; that is not one of the storm's gate conditions.
  - `ShipTest.cpp`: `crew_talk` now talks to six NPCs (the captain included) three times each and requires every line to be voiced.
  - `ShipEndingTest.cpp`: its storm line count skips `CAP_P`/`JEF_P`, so it stays at 61.
  - `docs/regions/SHIP.md` and `SHIP-STORM-ENDING.md` updated.
- **Tested:**
  - Editor `crew_talk`: 18/18 talks, every line voiced (`Local/ship-pre-storm-voice-1.log`).
  - Editor `chef_chase`: passed, with `CHUCK_PIRATE_SHOUT PirateChef CHEF_01 seconds=4.21` (`Local/ship-pre-storm-voice-chef.log`).
  - Packaged `Builds/PreStormVoiceCandidate`: Verify-Package 182 (`Local/verify-package-20261111-120400.log`).
    - The first run crashed in the temple heroes test while the user's game was open.
    - The second failed only the known intermittent `wizard_seal` (`Local/verify-temple-heroes-20261111-115630.log`).
  - Verify-Menu was not run.
- **Not checked:**
  - Nobody has listened to the takes in the game or played the talks by hand.
  - The storm with the new BG_04 has not been watched.
- **Remaining flaws:**
  - Subtitles reveal sentence by sentence on a timer, so they may run slightly ahead of or behind a take.
- **Contract compatibility:** no rig or animation changes.
- **Binary/LFS:** 43 new WAV/MP3 takes and generated WAVs (LFS), their new `VO_*` sound waves, and BG_04 replaced.
- **Launcher:**
  - Promoted as PreStormVoice.
  - The receipt names 01938373; the build is this work on top of it, committed right after.
  - The 14 `Common/Materials` re-saves are still uncommitted (not this work).

Next part of the work could be done by either — preference: Claude (listen to the pre-storm talks and the pumps order in game, then the fall into Phlegethos).

## November 10 — the tavern's rubbish bin out back (Claude)

- **Asked:** build a period rubbish bin behind the tavern in Waterdeep. The keeper's night line (`night_00`) has Bobert "rootin' about in me bin out back".
- **Source:** `main` at 0ced641c.
- **Changed:**
  - New `Tools/build_tavern_bin.py` (Blender 4.5.14) → `SourceAssets/NPCs/Props/SM_TavernBin.fbx` plus a `TavernBin` entry in the props manifest. Imported with `Tools/Import-NPCHumans.ps1 -Only prop:TavernBin`.
    - It is an old cask sawn down and stood on end: 62 cm tall and 44 cm across, with eighteen staves.
    - Two staves are left long as lugs, and one is cracked short.
    - It has four iron hoops and a boarded bottom.
    - Its plank lid has been knocked off and leans against it.
    - Wet scraps, cabbage leaves, a mutton bone and a fish's backbone are heaped inside. A few scraps and a gnawed bone are spilled at its foot.
    - About 9k triangles, with the slots Staves, Hoops, Refuse, Leaves and Bone (M_HumanFabric instances).
  - `DockSetting.cpp`:
    - The bin is placed in the alley behind the tavern at (60,1020), against the back wall between a window and a post.
    - It has a hidden 44×62 cm collision cylinder tagged `TavernBin` and logs `CHUCK_TAVERN_BIN placed=`.
    - Everything stays within y≤1043, so the alley's capsule route at y=1060 stays clear.
    - New `-ChuckBinCapture` (4 stills, `Screenshots/Bin`).
- **Tested:**
  - Blender review renders: `Local/tavern-bin/`.
  - Editor captures: `Local/tavern-bin/game/` and `Local/bin-capture*.log`.
  - Packaged `Builds/TavernBinCandidate`: Verify-Package 182 (`Local/verify-package-20261110-125607.log`), including `CHUCK_WORLD_CHECK_COMPLETE failures=0` (the alley capsule route) and `CHUCK_TAVERN_BIN placed=1`.
  - Verify-Menu was not run.
- **Not checked:**
  - Nobody has played it by hand.
  - It has not been seen at night.
- **Remaining flaws:**
  - The refuse heap reads very dark in the alley's shade.
  - The spilled scraps are simple lumps.
  - It is not interactive. Chuck can jump onto it (62 cm), the same as a crate.
- **Contract compatibility:** no rig or animation changes.
- **Binary/LFS:** SM_TavernBin plus 5 MIs (new) and the FBX source.
- **Launcher:**
  - Promoted as TavernBin.
  - The receipt names 0ced641c; the build is this work on top of it, committed right after.
  - The 14 `Common/Materials` re-saves are still uncommitted (not this work).

Next part of the work could be done by either — preference: Claude (the fall into Phlegethos after the burning rupture, and your answers to the plan's open questions).

## November 10 — the fall: land to the horizon round Chult; the pantry fall's white freeze halved (Claude)

- **Asked:** the fall to Chult showed a square of terrain that stops abruptly in the high views; make it look better. Also cut the load time at the pantry fall's start.
- **Source:** `main` at 4ff2f7df.
- **Changed:**
  - **Outer land** (`FallCutscene.h/.cpp`, `BuildOuterLand`):
    - The square was ChultVista's land ending over the flat far plane.
    - The cutscene now builds land round it in the canyon frame, out to 45 km:
      - It matches `ChultGorgeLandZ` at the seam.
      - The gorge carries on as a valley that shoals and widens.
      - Over 3 km it blends into calmer hills, which sink to the far plane 25–40 km out.
    - It uses M_ChultGorge. There are 32 chunks (118k triangles), built in about 40 ms. They are hidden with the sky at the crash, so playable Chult is unchanged.
  - **Async collision during the fall:**
    - `AChultLanding::bAsyncCook` is set round the fall's `BuildSet()`. Every Chult `CreateMeshSection` site sets `bUseAsyncCooking` from it: ChultGrid.h, ChultLandform.h, ChultCanopy, Ruins, Temple, TempleInterior (2), Expansion, Bridge, Canyon, Dressing, Wreck (2) and the landing's root collision.
    - `AChultLanding::AfterCollision(Owner, Delay, Fn)` waits until no procedural mesh has a pending cook (it reads `AsyncBodySetupQueue` by reflection, with a 20 s cap). The path (.6 s), tree grounding (1.1 s) and the wreck's second clearing (1.2 s) now use it.
    - The checkpoint entry still cooks synchronously.
  - **Parallel heights:**
    - In `ChultVista.cpp`, the band and far-sheet height and colour loops run in `ParallelFor`.
    - In `ChultUpriver.cpp`, the floor's corner heights are computed once in `ParallelFor` and the same quads are built in the same order.
  - `docs/CHULT-LANDING.md`: a contract bullet for `bAsyncCook`, `AfterCollision` and the thread-safety rule for height functions.
- **Contract:** no rig, character, camera or traversal changes. The landing transform, path, grounding and wreck results are unchanged; in the editor, grounding lowered=10992, no_ground=1538 and the wreck cleared 369, the same as before.
- **Tested:**
  - **Editor:**
    - The stage build went from a ~7.0 s single-frame freeze to 2.8 s. Collision was ready ~0.6 s later.
    - Stills: `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/FallCutscene/` (T15.0_CloudDeck, T16.9, T18.6, T20.3, T06.5, PlayableLanding).
    - Logs: `Local/fall-profile-*.log`, `Local/fall-outer-land.log`.
  - **Packaged, first candidate:**
    - Crashed 60 s after the stage build (`Local/verify-demo-end-20261110-090913.log`). The dump, symbolized with dbghelp, pointed at `AfterCollision`.
    - Cause: the poll lambda cleared its own timer and then used its destroyed captures.
    - Fix: copy them to the stack before `ClearTimer`.
  - **Packaged, second candidate:**
    - `CHUCK_PACKAGE_VERIFIED passes=182` (`Local/verify-package-20261110-091324.log`).
    - `CHUCK_FALL_STAGE build_ms` 3218–3254 (packaged was ~6.5 s before). `CHUCK_CHULT_COLLISION_READY` after ~470 ms.
    - `CHUCK_FALL_CUTSCENE_TEST failures=0 landing=1` and `CHUCK_DEMO_END_TEST failures=0`.
    - Promoted as OuterLand. The receipt names 4ff2f7d because the build was made before this commit. The prior package was kept and HeroHair pruned.
- **Not tested:** nobody has watched the full fall by hand; only stills were reviewed.
- **Remaining flaws:**
  - Laying the path still hitches ~0.4 s about 1 s after the white clears (pre-existing).
  - The rest of the 3.2 s is mostly the temple and interior (~0.5 s), vista dressing (~0.5 s) and the root collision mesh (~0.25 s).
  - Small steps may show where the outer land meets the vista edge.
  - The far haze reads tan at the left edge of the 3 km still.
  - The 14 `Content/Common/Materials/*.uasset` re-saves were uncommitted before this session. They are not this work and remain uncommitted.
- **Next owner:** either agent; Claude preferred for further fall/cutscene work.

## November 10 — the ship's sides named right; louder lines in the intense storm; guide lights after the orders; no Jeffries strain/choke; a longer storm-line pause; a swing across the main yard's middle (Claude)

- **Asked:**
  - The intense storm's dialogue is still a little too quiet; the start is good.
  - Every starboard and larboard reference is backwards; swap them.
  - The golden guide light comes on before the instructions; it should come on after them.
  - Take out Jeffries' coughing and struggling noises.
  - More time between the captain's "storm's upon us" and "the seas shall rue the day".
  - After one side's reef points are tied, he should be able to reach the other side with a back-and-forth swing and a leap across the middle of the yard (before, he could only drop there).
  - Confirmed: he falls into the hell (Phlegethos) rupture, as built.
- **Source:** `main` at 4ff2f7df.
- **Cause of the sides:**
  - Unreal's Y runs to starboard facing the bow (+X), but the ship's code and docs treated +Y as port.
  - So yardarm 0 ("larboard") was on the starboard side, and every side-specific line (NOW THE STARBOARD!, the crewman's FAST call, the deck call) named the wrong side.
- **Changed:**
  - **Sides:**
    - `ShipEnding.cpp`: side index 0 is now larboard, at -Y (work spots, grips, which-side-is-here, flapping canvas).
    - Comments in the ship sources were swapped (47 lines), the frame comment says Y to starboard, and the test spots were renamed (`ForeShroudsPort`/`Starboard` swapped, `BreakPort`).
    - `ShipEndingTest.cpp`: the trials are named for their real sides (`reef_starboard` first, `leap_starboard` for the +Y lane).
    - `Tools/Verify-Package.ps1`: expects the new trial order.
    - `docs/SHIP.md` and `docs/regions/SHIP-STORM-ENDING.md`: side words swapped (not in quoted dialogue).
  - **Volume:**
    - `ShipEnding.cpp`: voice ×(1.4 + .7·Intense), up to +6.4 dB (was +5.6).
    - `ShipStorm.cpp`: under a line the loops duck to .4 − .18·Intense (about 13 dB once intense), and the sea slams and thunder spawned meanwhile are halved.
  - **Guide lights:** `ShipEnding.h/.cpp`: `LitStep`. A step's lights show only once its queued orders have all been said (logs `CHUCK_SHIP_GUIDE_LIT`).
  - **Jeffries:** JEF_X01/X02 are no longer queued in play or in the cutscene. JEF_01 moved to 14 s in the cutscene. The voice files stay imported.
  - **Pause:** CAP_S01 now follows CAP_S00 by 4.5 s (was 1.5).
  - **Yard crossing:**
    - `ChuckClimbable.h`: `FChuckSwingGrip::LeapRange`.
    - `ChuckCharacter.cpp` `FindSwingTarget`: a pair of grips that both set it may be leapt up to that range.
    - `ShipDeck.inl`: the main course yard's innermost grips (±170) get 370, so he can leap the 340 cm middle on a pumped swing (the existing big-leap wait). Every other leap still reaches 240 cm.
  - `ShipEndingTest.cpp`: the larboard reef trial now starts on the starboard side of the top, catches the inner grip, pumps and crosses (`CHUCK_SHIP_YARD_CROSS`, stills `YardCrossMid`, `YardCrossed`), then goes out to larboard.
- **Tested:** 
  - Editor: `Local/ship-fix-3.log`, all 10 trials passed. The crossing waited 1.83 s for its swing (408 cm against 340), caught the -166 grip and logged crossed=1 at 2.6 s. Storm duck minimum 0.22. CAP_S00 to CAP_S01 about 4.6 s.
  - Package: `CHUCK_PACKAGE_VERIFIED passes=182` (`Local/verify-package-20261110-191053.log`). The packaged ship ending (`Local/verify-ship-ending-20261110-200442.log`) had failures=0 with the new trial names. The reef guide lit at 40.9 s, 0.1 s after CAP_02 finished.
- **Not checked:**
  - Nobody has listened to the new loudness or played it by hand.
  - The crossing has been done only with the test's in-time pump.
- **Remaining flaws:**
  - The leap across the middle flies with no collision. Its arc may pass close to the top's underside.
  - Earlier flaws (the comber's pale back, the clewline through Chuck, the skewed rupture under the tilt) are unchanged.
- **Contract compatibility:** no rig or clip changes. `FChuckSwingGrip` gains a defaulted field.
- **Binary/LFS:** none.
- **Launcher:** promoted as ShipFix (receipt c806645 plus this commit's source; the previous build is kept, OuterLand pruned).

Next part of the work could be done by either — preference: Claude (the fall into Phlegethos after the burning rupture, and your answers to the plan's open questions).

## November 9 (late night) — the storm ending: lines heard over the storm; a great sea knocks her over and Chuck loses his grip (Claude)

- **Asked:**
  - The storm dialogue is too quiet, above all once the storm intensifies.
  - The cutscene made Chuck look as if he jumped into the rupture. Instead, a big wave should knock the ship over, and a new animation should show him losing his grip, so that he falls into the rupture without it looking intentional.
  - More cinematic shots of the ship from further out, showing the storm's ferocity.
- **Source:** `main` at 0f5e52ef.
- **Changed:**
  - **Volume:**
    - `DockNPC.h/.cpp`: `SetVoiceVolume`.
    - `ShipEnding.cpp`:
      - Every ship line plays at ×1.4, plus .3×Intense (+3 to +5.6 dB).
      - Cutscene lines use an unattenuated, unspatialized `SceneVoice`.
    - `ShipStorm.cpp`:
      - The storm's loops (rain, gale, canvas, sea) duck to 40% under any line spoken in the ship, then come back about 1 s after it.
      - New `GetSpeechDuck`.
  - **Knock-down:**
    - `ShipSetting.h/.cpp`:
      - `SetKnock(RollDeg, Weight)` blends the sea's roll to a held heel.
      - `GetSeaUp` and `GetSeaTurn`.
      - The ship still never moves; the sea turns round her.
    - `ShipStorm.cpp`:
      - `DeckWaveNow` gains `From`, so a sea can break from either side (mirrored mesh, normals and shove).
      - New `BillowNow`.
  - **Chuck:**
    - `Tools/build_chuck_v1.py`: new clips `SwingSlip` (37 f: the yard jolts, the right paw is torn off and claws back, the left peels off at 70%) and `FallFlail` (24 f loop, clawing upward).
    - Built in Blender 4.5.14 (`Local/chuck-slip-blender.log`).
    - Only the two new FBX files, the manifest and the .blend were kept. The other rewritten FBX files were restored.
    - Imported with `Import-ChuckV1.ps1 -Clips SwingSlip,FallFlail`.
    - `ChuckCharacter.h/.cpp`: a cutscene hold mode (`BeginSceneHold`, `SetSceneClip`, `SetScenePose`). It is inert outside the hold.
  - **Cutscene** (`ShipEndingScene.h/.cpp`), 11 shots, about 42 s:
    - Crosstrees, then four wide shots in the sea's frame with a level horizon:
      - SeaLevel off her lee bow.
      - Aerial off her weather quarter, with lightning.
      - Astern, low, with lightning ahead.
      - Comber, from ahead.
    - The great sea (`M_ShipComber`, `Tools/create_ship_comber_material.py`) rises to 13 m on the weather side, curls, and breaks on her beam at 37.3 s.
    - Spray, billows and two deck seas follow. She is knocked to 26° toward his side, with an overshoot, and the camera shakes.
    - Grip shot: he swings out after the true vertical (~41°) and loses his grip (JEF_06 at 38.5 s).
    - Below shot: he falls straight down along the sea's up (~2 s) into the burning rupture.
    - His landing is predicted from the heel (`PredictFall`). The ruptures follow the tilted sea.
  - `ShipEndingTest.cpp`:
    - Requires a heel of at least 20° and 10+ shots.
    - Logs `storm_duck_min` and `heel_deg`.
    - New `-ChuckShipSceneSide=1` option climbs the larboard shrouds.
  - Docs: `docs/regions/SHIP-STORM-ENDING.md` (beat 7, the shot list, the voice bullet) and `docs/HANDOFF.md`.
- **Tested:**
  - Editor ending test:
    - Full run, starboard: failures=0, shots=11, `storm_duck_min=0.40`.
    - From the leap: missed the rupture's centre by 35–42 cm, heel 28.5°, swing ~41°, fall 1.97 s.
    - Port side: failures=0, missed by 35 cm.
    - Logs: `Local/ship-knock-*.log`. Stills: `Local/ship-knock/`.
  - Packaged `Builds/KnockCandidate`:
    - First Verify-Package run: the known temple `wizard_seal` flake (`max_coverage=0.25`, `Local/verify-temple-heroes-20261109-210013.log`).
    - Second run: every region test passed, ship ending included (`Local/verify-ship-ending-20261109-220154.log`: scene pass, heel 28.6°, missed by 40 cm). But the smoke test's roll drifted 2.04 cm sideways against a 2 cm limit (`Local/verify-package-20261109-210738.log`; earlier builds 1.80).
    - Smoke re-run on the same build: 1.80 cm, `CHUCK_TEST_COMPLETE failures=0`, 152 passes (`Local/knock-smoke-rerun.log`). It was promoted on that log.
    - The roll never touches the new code, so the 2.04 cm was a timing wobble near the limit.
- **Not checked:**
  - Nobody has listened to the new loudness, or played the ending by hand.
  - The frame cost of the shadowed rift lights is not measured.
  - Verify-Menu was not run.
- **Remaining flaws:**
  - The wave's back slope reads pale, a little like a snowy hillside.
  - In the Grip shot the clewline passes through Chuck.
  - Under the 26° tilt the Below shot's rupture looks skewed (its shader assumes a level disc).
  - The sea round it looks rocky from above (pre-existing).
  - Rain and spray tilt in frame during the knock.
  - He still falls into the burning (Phlegethos) rupture, as the plan says. The user's request said "astral sea rupture"; ask whether he should land in an Astral one instead.
- **Contract compatibility:**
  - Two new clips appended to `EClip` (after Crawl). The rig is unchanged.
  - Existing clip indices are unchanged.
- **Binary/LFS:**
  - 2 FBX files, `Chuck_V1.blend`, 2 clip uassets and `M_ShipComber.uasset`.
- **Launcher:**
  - Promoted as Knock.
  - The receipt names 0f5e52e; the build is this work on top of it, committed right after.
  - The 14 `Common/Materials` re-saves are still uncommitted (not this work).

Next part of the work could be done by either — preference: Claude (whether he lands in an Astral or the burning rupture, then the fall into Phlegethos, and your answers to the plan's open questions).

## November 9 (night) — the heroes' hair in the beholder encounter: fighter dark again, ranger auburn (Claude)

- **Asked:** in the beholder encounter the fighter's hair reads white/grey, and the ranger's reads very blonde/white over very dark roots. Fix it.
- **Source:** `main` at 111e0f12.
- **Cause:**
  - `r.HairStrands.Voxelization=0` (DefaultEngine.ini, kept for Waterdeep's frame rate) leaves strands with no self-shadowing.
  - The sanctum's unshadowed lights then light the hair straight through the head. These are the torches and braziers, the beholder's eye light, the FX lights and the cutscene lights.
  - So dark hair reads grey and red hair reads pale.
  - Close torch-lit portraits looked fine. The entrance and Fireball shots showed it.
  - A/B with voxelization 1: the fighter's beard is dark again and the ranger's hair a deep auburn.
- **Changed:**
  - `NPCGroom.h/.cpp`: `ChuckGroom::Want(Owner, HairAt)` / `Release()`.
    - Voxelization is switched on at runtime only while the camera is within the groom draw distance (+5 m) of a hero's hair. It is gathered per frame.
    - It goes off again elsewhere and on `EndPlay`, and logs `CHUCK_GROOM_VOXELS on=`.
    - The groom cull distance now has a helper, `-ChuckGroomCull=` to test. The value is unchanged at 25 m.
  - `TempleHero.h/.cpp`: each visible hero with grooms calls `Want` from `Tick` and `Release` on `EndPlay`.
  - `SourceAssets/Heroes/heroes.json`, `manifest.json`:
    - Ranger hair specular .5→.25 and root darkening .35→.15. Scalp shell tint (.27,.09,.03)→(.4,.14,.05), so the roots match the strands.
    - Wizard hair and beard whitened (they read cream once self-shadowed). Scalp and beard-shade tints follow.
  - `Tools/set_hero_groom_shading.py`: also sets the stubble shells' `Tint` from the manifest. Run with `CHUCK_GROOMS_ONLY=Wizard,Ranger`; it saved 7 MIs.
  - `docs/CHULT-TEMPLE-HEROES.md`: section "Hair under unshadowed light (November 9)".
- **Tested:**
  - Editor sanctum captures (`Local/beholder_iter.sh`, trials + entrance + Fireball). Before, voxels-forced and after sets are in `Local/hero-hair-before/`, `hero-hair-vox1/`, `hero-hair-after/` and `hero-hair-after2/` (compare*.png). All trials passed in the after runs (`Local/beholder-fight-hairafter*.log`).
  - The runtime switch was seen in the editor logs: on near the heroes, off after the teleport to the rubble map.
  - Packaged `Builds/HeroHairCandidate`: Verify-Package 182 (`Local/verify-package-20261109-142241.log`). That run never comes near the heroes, so it logs no voxel switch. Verify-Menu was not run.
- **Not checked:**
  - Nobody has played it by hand.
  - The sanctum's frame cost with voxels on has not been measured.
- **Remaining flaws:**
  - A few dark flecks along the ranger's front hairline, where the scalp shell shows between combed strands.
  - Other haired NPCs (Waterdeep, ship) still use the unshadowed path. They were not reported and were not changed.
- **Contract compatibility:** no rig or animation changes.
- **Binary/LFS:** 7 MI uassets (Ranger Hair/Scalp; Wizard Hair/Beard/BeardJaw/BeardShade/Scalp).
- **Launcher:**
  - Promoted as HeroHair.
  - The receipt names 111e0f12; the build is this work on top of it, committed right after.
  - The 14 `Common/Materials` re-saves are still uncommitted (not this work).

Next part of the work could be done by either — preference: Claude (the fall into Phlegethos after the burning rupture, and your answers to the plan's open questions).

## November 9 (evening) — the ship's gold glow softened; Jeffries' hair no longer lit purple (Claude)

- **Asked:**
  - Make the glowing gold on the ship, the golden cigarette in the main top and the ending's guide lights, a softer, less dramatic gold.
  - Stop Jeffries' hair lighting up unrealistically under the purple light.
- **Source:** `main` at 06639353.
- **Changed:**
  - `CigarettePickup.h/.cpp`: `SetSoftAura()`, used only by the ship's main-top leaf. Chult's golden leaf and the captain's carton are unchanged. In soft mode:
    - The halo is about a third as bright, and the haze and wisp are about a sixth.
    - The motes are dimmer and the breath is slower and shallower.
    - The light drops from 4.5–7 cd to 0.55–0.7 cd over 130 cm, and casts shadows.
  - `ShipEnding.cpp`: the guide lights' halo intensity drops from .45–.8 to .2–.26, and their size from .7 to .55. Their lamps drop from 2–3.5 cd to 0.45–0.55 cd and cast shadows. They breathe and bob slower.
  - `ShipStorm.cpp` (the companion-hatch rupture light) and `ShipEndingScene.cpp` (the cutscene's rift lights) now cast shadows. Unshadowed, they lit strand hair through the mainmast and through the head, so Jeffries' hair and the crew's beards blazed purple. The gold lights did the same to Chuck's fur.
  - `ShipEndingTest.cpp`: a still of the leaf in the main top (`LeafMainTop`).
- **Tested:**
  - Editor ending test, full and skip runs: failures=0 ×3 (`Local/ship-soft-gold-1..3.log`).
  - Stills: `Local/ship-soft-gold/`. Jeffries' hair is dark with no purple, and the leaf and guide lights are a soft warm pool.
  - Packaged `Builds/SoftGoldCandidate`: Verify-Package 182 (`Local/verify-package-20261109-103842.log`). Verify-Menu was not run.
- **Not checked:**
  - Nobody has played it by hand.
  - The frame cost of the shadowed rift lights (radius 1600–2400) has not been measured.
- **Remaining flaws:**
  - Standing right on the leaf, there is still a bright warm patch round Chuck's feet.
  - Chult's golden leaf keeps its strong aura, by design: the user's 2026-10-15 request to stand out to a first-time player.
- **Contract compatibility:** no rig or animation changes.
- **Binary/LFS:** none.
- **Launcher:**
  - Promoted as SoftGold.
  - The receipt names 06639353; the build is this work on top of it, committed right after.
  - The 14 `Common/Materials` re-saves are still uncommitted (not this work).

Next part of the work could be done by either — preference: Claude (the fall into Phlegethos after the burning rupture, and your answers to the plan's open questions).

## November 9 — the storm ending's sea ruptures reworked (Claude)

- **Asked:** "Make the hell ruptures look better" (the Astral ruptures and the burning Phlegethos one in the fore topsail cutscene).
- **Source:** `main` at 01fc07c5.
- **Changed** ([../regions/SHIP-STORM-ENDING.md](../regions/SHIP-STORM-ENDING.md)):
  - New `Tools/create_sea_rupture_materials.py`, which makes three materials: `M_SeaRuptureAstral`, `M_SeaRupturePhlegethos` and `M_SeaRuptureFoam` (under `/Game/Art/Materials/`).
    - Each rupture is still a flat ragged disc on the sea. The material traces the view down a well under it: the sea pouring over the lip and down the wall in streaks, lit from below.
    - The Astral floor is a nebula with stars, and its wall is lit violet with an oily film near the lip.
    - The Phlegethos floor is cracked basalt plates over molten rock with a lava lake in the middle, plus steam on the wall.
    - A foam ring on the sea is drawn in round each lip.
  - `ShipEndingScene.cpp/.h`:
    - New `BuildRuptures` (hole, foam ring, Astral oil veil, lights welling up).
    - `-ChuckShipSceneShots` takes six stills (`ShipRuptures/`).
    - The flat `M_AstralDepth`, `M_FireEmber` and `M_Dark` discs are gone.
    - A trial of fire and smoke round the burning lip was dropped; it hid the shot.
- **Tested:**
  - Editor `-game -ChuckChultTest -ChuckShipEndingTest -ChuckShipEndingFrom=leap -ChuckShipSceneShots`: failures=0 on every run, ending with `plunged=1 miss_cm=99 leaps=3` (`Local/ship-ruptures-6.log`).
  - Stills reviewed: `Local/ship-ruptures/Rupture_0..5`.
  - Packaged `Builds/RupturesCandidate`:
    - The first Verify-Package failed only on the temple `wizard_seal` trial (`max_coverage=0.26`), which this work does not touch and which had passed in the four previous runs.
    - The re-run on the same build passed 182 (`Local/verify-package-20261109-083909.log`).
    - Verify-Menu was not run (cutscene-only change).
- **Not checked:** a human has not watched it in motion.
- **Remaining flaws:**
  - The Astral nebula floor shows only when looking well down into a rupture.
  - The sea's own foam texture shows straight-edged tiles in the top-down shots (pre-existing).
  - The lip is a hard mesh edge in places.
  - There is no fire or smoke at the burning rupture.
  - `wizard_seal` looks flaky (one failure in six packaged runs).
- **Contract compatibility:** no rig, animation or public API changes.
- **Binary/LFS:** three new material `.uasset` files (LFS).
- **Launcher:**
  - Promoted as Ruptures.
  - The receipt names 01fc07c5; the build is this work on top of it, committed right after.
  - The 14 `Common/Materials` re-saves are still uncommitted (not this work).

Next part of the work could be done by either — preference: Claude (the fall into Phlegethos after the burning rupture, and your answers to the plan's open questions).

## November 8 (night) — the ship's storm ending built, with all 61 voiced lines (Claude)

- **Asked:** "create the storm sequence, including adding all the lines".
- **Source:** `main` at 06b2c422.
- **Changed** ([../regions/SHIP-STORM-ENDING.md](../regions/SHIP-STORM-ENDING.md#what-was-built-2026-11-08)):
  - The 61 lines added to `SourceAssets/NPCs/dialogue.json` as scene lines of `PirateCaptain`, `PirateCrewman1`, `PirateCrewman2` and `Jeffries`. `build_npc_voice.py` produced the levelled WAVs and `NPCVoiceData.h`; `import_npc_voice.py` (`CHUCK_VOICE_ONLY`) imported 61 SoundWaves under `/Game/Art/Audio/Voice/`.
  - New `ShipEnding.h/.cpp`, `ShipEndingScene.h/.cpp` and `ShipEndingTest.cpp`.
  - `DockNPC` gains `SpeakLine` and `StopSpeaking`, and a talk subtitle never shows a scripted line.
  - `ShipDeck.inl`/`ShipSetting.cpp`: the fore lower ratlines are their own mesh. The ending is spawned with the ship.
  - `ShipStorm.cpp`: the gate reset also resets the ending.
  - `DockGameMode.cpp`: subtitles and letterbox.
  - `DockMenu.cpp` and `Verify-Menu.ps1`: Ship Reef, Ship Foremast Leap and Ship Fore Topmast (dev).
  - `ChuckCharacter`: `EndDemo`, `CatchSwingGrip`.
  - `ShipCrew.cpp`: a guard for the fencers' unplanned first phrase. It crashed one run: `KeysAround` on an empty array.
  - `Verify-Package.ps1` runs the ending test.
- **Tested** (uncooked editor `-game`):
  - `-ChuckChultTest -ChuckShipEndingTest -ChuckShipEndingSkip`: failures=0, all 10 trials (`Local/ship-ending-full-skip.log`).
  - The same from the leap with the whole cutscene (`-ChuckShipEndingFrom=leap`): failures=0, 8 shots, 3 yard leaps, thrown, into the burning rupture 99 cm from its middle, end card (`Local/ship-ending-12.log`, `-13.log`).
  - Stills are in `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/ShipEnding/`.
  - Full ship test `-ChuckShipTest`: 21/21, ship check 0 (`Local/ship-full-ending.log`).
  - Packaged `Builds/StormEndingCandidate`: Verify-Package 182 passes, with the ship test at 21/21 and the ending at 10/10 (`Local/verify-package-20261108-214833.log`). The menu check first failed on the aloft checkpoints, because its spawn and recovery expectations were the deck. Both the spawns and the test were fixed, and the package rebuilt and verified again (see Launcher).
- **Not checked:** nobody has listened to the lines in the game or judged their mix against the storm; a human has not played it.
- **Remaining flaws:**
  - The main course stays furled with flogging canvas at the yardarms, not set close-reefed.
  - There is no tug or work clip, and no jammed quarter block.
  - The Astral sea ruptures are flat rainbow discs on the swell, and the Phlegethos one is a flat disc of dark plates over glowing channels.
  - On the starboard side the cutscene puts him on the yard directly: the braced yard's first grip is beyond his reach from that net.
  - The jar in the starboard run-up lane still stands there.
  - The fall into Phlegethos ends on the end card.
- **Contract compatibility:** new public members on `ADockNPC` and `AChuckCharacter` only; no rig or animation changes; the leap boost is applied by the ending, on the ship only.
- **Binary/LFS:** 61 new voice `.uasset` files (LFS); 61 new WAVs under `SourceAssets/NPCs/Voice/` (LFS).
- **Launcher:** promoted as StormEnding. It was rebuilt after the menu fix (the aloft checkpoints' spawns and expected recovery), and that build passed Verify-Package 182 (`Local/verify-package-20261108-234043.log`) and Verify-Menu 47/47 (`Local/verify-menu-storm-ending-3.txt`). The receipt names 6406aaf3; the build also contains the uncommitted menu fix, committed right after. The 14 `Common/Materials` re-saves are still uncommitted (not this work).

Next part of the work could be done by either — preference: Claude (the fall into Phlegethos after the burning rupture, and your answers to the plan's open questions: the set close-reefed course, the cutscene's music, and the leaf's owner).

## November 8 — the fore course clear of the ratlines; the masts solid to the truck; the landing's leaves (Claude)

- **Asked:** stop the fore course intersecting the fore ratlines unrealistically; give every part of each mast collision so that above the topsail he can climb the highest part of the mast directly on the golden leaf; after the fall to Chult, keep the leaves' rustle on landing but cut it off so it is brief and realistic.
- **Source:** `main` at 67dc301d (94772e06, the voice split, was committed meanwhile; no shared files).
- **Changed** ([../SHIP.md](../SHIP.md#the-fore-course-clear-of-the-ratlines-the-masts-solid-to-the-truck-the-landings-leaves-november-8)):
  - `ShipDeck.inl`: `CourseClew`/`CourseClewLead` (the fore course's clews at the ship's side, forward of the foremost shroud; sheet and tack to rail blocks); one hidden collision shell per mast round the topmast and topgallant, faired over the doubling to the truck; `CourseNetClearance`.
  - `ShipSetting.cpp`: ship check `CHUCK_SHIP_COURSE` (canvas vs fore shrouds/ratlines) and `CHUCK_SHIP_MASTS`.
  - `ChuckCharacter.cpp`: on the golden leaf the wall run's normal comes from the nearest of a fan of lines (0, ±8, ±16, ±30°), not the last line's hit normal, which spiralled off thin poles.
  - `ShipTest.cpp`: trial `mast_climb` (both masts, aft/port/fore, to the truck; 21 trials); still `ShipPlay_MastTruck`; `-ChuckMastTrace`.
  - `FallCutscene.h/.cpp`: landed leaves lie still and stay; the floor litter is heavy (falls 1.1–1.6 m/s, little drift); `CHUCK_FALL_CUTSCENE_LEAVES` at +1/+2/+4 s.
  - `Tools/Verify-Package.ps1`: expects 21 ship trials, the course and mast checks, mast_climb, and no airborne litter 2 s after the landing.
- **Tested:** editor full ship test 21/21, check 0 failures, course 0 crossings / 53 cm, masts 8/8, all six mast climbs to the truck, Jeffries 0.24 cm (`Local/ship-mast-full.log`); editor fall cutscene: airborne litter 17 → 0 → 0 at +1/+2/+4 s, test failures=0 (`Local/fall-leaves1.log`). Stills reviewed (`ForeShroudsPort`, `Sails`, `ShipPlay_MastTruck`, fall `T26.6`, `T29.0`). Packaged `Builds/MastsCandidate`: Verify-Package 182 passes (`Local/verify-package-20261108-160432.log`), ship 21/21 with all six mast climbs and the course check (`Local/verify-ship-20261108-164604.log`), fall litter airborne 19 → 0 → 0 (`Local/verify-fall*` of that run); Verify-Menu 44/44 (`Local/verify-menu-masts.txt`).
- **Remaining flaws:** the crosstrees, caps and yards are still not solid (he passes through the topsail and topgallant yards climbing the fore face); climbing a topmast's fore face he passes through the topsail sail's head; the climb uses the wall-run clip; the fan normal changes all golden-leaf climbs (Chult trunks covered only by Verify-Package's tests); not played by hand.
- **Contract compatibility:** no rig or animation changes; wall-run behaviour changed only under the golden leaf.
- **Binary/LFS:** none.
- **Launcher:** promoted as Masts (receipt names 94772e06, the voice-split commit that landed meanwhile; the build is this uncommitted work on top of it). The 14 `Common/Materials` re-saves are still left uncommitted (not this work).

Next part of the work could be done by either — preference: Claude (the reef points, the fore ratlines torn loose and the jump to the foremast).

## November 8 — all the ship's rope; Jeffries' lashings; the floating coil (Claude)

- **Asked:** "make the rest of the ship rigging rope look like our new good looking rope, also do this for the rope securing Jeffries (and it needs to be around him, not with parts of him passing through it impossibly). our current coiled rope decor is good as is but i did see one below deck that floats impossibly so make sure that doesnt happen".
- **Source:** `main` at 9d7763cd (the work started on fb913d59; the uncommitted files the previous entry left alone are this work).
- **Changed** ([../SHIP.md](../SHIP.md#all-the-ships-rope-jeffries-lashings-the-floating-coil-november-8)):
  - `ShipRope.h/.cpp`: the lay scales with rope radius; `Hull2D`.
  - `ShipSetting.h/.cpp`, `ShipDeck.inl`: every rigging rope is a `Cord` (ShipRope tube); standing rigging `MI_RopeTarred`, running rigging and lashings `M_Rope` (new `Hemp` surface); the arrival compartment's coil moved onto the floor; the ship check traces under every coil (`CHUCK_SHIP_COIL`).
  - `DockNPC.h/.cpp`, `DockNPCPirate.inl`, new `JeffriesRopeBody.inl`: Jeffries' lashings are taut hulls round his posed mesh and the mast (chest, hips, wrists, line round the mast); fists held clear of the mast; `-ChuckBakeJeffriesRope` regenerates the body points (editor only).
  - `ShipTest.cpp`: `CHUCK_SHIP_JEFFRIES_ROPE` (body inside a rope < 0.6 cm); new rope/Jeffries stills.
  - `Tools/create_rope_material.py`: `M_Rope` parameterised; builds `MI_RopeTarred`.
- **Tested:** editor full ship test 20/20 (`Local/ship-rigging-full.log`). Packaged `Builds/RiggingCandidate`: Verify-Package 182 passes (`Local/verify-package-20261108-120725.log`), ship 20/20, rope depth 0.24 cm, 7 coils resting (`Local/verify-ship-20261108-124853.log`), Verify-Menu 44/44 (`Local/verify-menu-rigging.txt`). Stills reviewed. Not played by hand.
- **Remaining flaws:** no knots on Jeffries' lashings; a short step where each turn climbs to the next behind the mast; the rope follows his outline in flat slices, not cloth folds; the tarred standing rigging reads dark against the sky.
- **Contract compatibility:** only new members on `ADockNPC`; no rig or animation changes.
- **Binary/LFS:** new `MI_RopeTarred.uasset`; `M_Rope.uasset` and the three `T_Rope001_*` re-saved (LFS).
- **Launcher:** promoted as Rigging (receipt names 9d7763cd, the build was of this work on top of it). The 14 `Common/Materials` re-saves from package builds are still left uncommitted (not this work).

Next part of the work could be done by either — preference: Claude (the reef points, the fore ratlines torn loose and the jump to the foremast).

## November 8 (later) — the ship storm's voice takes split by line (Claude)

- **Asked:** the user voiced the storm script in ElevenLabs, one file per section with every line in order (only JEF_01-06 of the cutscene, only JEF_X01/X02 of the sound-only takes, none of the storm-time Jeffries lines), plus two standalone captain lines for the storm's start; "go through the sound files and separate as needed for the game by 'looking' at the audio".
- **Source:** `main` at 9d7763cd.
- **Changed:** `SourceAssets/NPCs/Voice/ShipStorm/source/` (the 7 takes, LFS, renamed; the original names are in `lines.json`); `SourceAssets/NPCs/Voice/ShipStorm/lines.json` (61 lines: id, npc, text, source, start, end, in `build_npc_voice.py`'s form); new `Tools/split_ship_storm_voice.py` (clips to `Local/ShipStormVoice/<npc>/<id>.wav`); [../regions/SHIP-STORM-ENDING.md](../regions/SHIP-STORM-ENDING.md#the-voice-takes) (voiced lines approved, CAP_40 and the other Jeffries drafts dropped, the method and checks).
- **Tested:** the tool ran and wrote 61 clips. Each clip re-recognized: 35 named their own line, 16 no answer, none another line apart from CAP_20L, which the hiss check shows is "larboard" as scripted. Against neighbours only, no doubtful clip matched a neighbour. Not listened to by a person.
- **Method lesson:** Windows' System.Speech (en-US, already present) with a grammar of only the expected sentences works as an offline forced aligner. Pad each piece with 0.5 s of silence or short pieces come back empty. Too large a grammar (127 sentence pieces) makes it refuse almost everything. Blender's `aud` `write` failed at the end of some MP3s ("filling the audio frame failed with ffmpeg"): use `data()` + `wave` instead.
- **Remaining flaws:** speaker attribution in the scripted take follows the script (not confirmable by measurement); JEF_X01/X02 are split at a 0.19 s gap by sound character; `Captain_Storm_02` (an older REEF THOSE SAILS take, 11-05) in Downloads was not supplied and is not used.
- **Contract compatibility:** unchanged. **Binary/LFS:** 7 audio files (about 3.5 MB, LFS). **Launcher:** unchanged (Rope). The other uncommitted ship work in the tree is still left alone.

Next part of the work could be done by either — preference: Claude (the storm ending's build, using these lines).

## November 8 — the ship's new ending, planned (Claude)

- **Asked:** "dont add to the game yet but write documentation for this": no plank walk; the reef points on the main, then the fore yard's trouble with the lower fore ratlines torn loose, the golden-cigarette long jump from the main top to the foremast, the higher fore ratlines to the fore topsail, a cinematic cutscene (the ship in the storm, Astral ruptures on the current, Jeffries noticing) ending with Chuck knocked into a Phlegethos rupture and the fall cutscene; gold guide lights; period-jargon orders as a joke plus clear, situation-specific lines; flesh out the user's storm dialogue draft and keep it nautically accurate.
- **Source:** `main` at fb913d59.
- **Changed:** new [../regions/SHIP-STORM-ENDING.md](../regions/SHIP-STORM-ENDING.md) (the rig's proper names from `ShipDeck.inl`, the five steps, the jump's numbers, the dialogue: user lines verbatim, proposed lines marked, guidance table, background orders, nautical notes, build list, questions); a pointer in [../regions/SHIP.md](../regions/SHIP.md#the-ending); a line in HANDOFF.
- **Findings for the build:** she is a brig; the main course is furled today, so "reef points" needs it set close-reefed in the storm; the yardarms hang about 2 m outboard (work spots must sit over the deck); main-top rim to fore-top rim is 6.7 m with a 1 m drop, against about 2.9 m for his current sprint jump; the main top allows only a 2.6 m run-up, and the lookout's jar sits in the starboard lane.
- **Tested:** nothing (documentation only).
- **Contract compatibility:** unchanged.
- **Binary/LFS:** none.
- **Launcher:** unchanged (still Rope).
- **Left alone:** uncommitted work already in the tree that is not from this session and not in any handoff: `ShipRope.*`, `DockNPC*`, `ShipDeck.inl`, `ShipSetting.*`, `ShipTest.cpp`, `Tools/create_rope_material.py`, `M_Rope`/`T_Rope001_*`, new `MI_RopeTarred.uasset` and `JeffriesRopeBody.inl` (apparently Jeffries' ropes as `ShipRope`), plus the 14 `Common/Materials` re-saves.

Next part of the work could be done by either — preference: Claude (once the user approves the plan and lines: the reef, the carry-away, the leap, the cutscene).

## November 7 — the storm's lines as rope (Claude)

- **Asked:** find and use free resources to make the ship's ropes better: the lines the crew haul in the storm bent at sharp, unrealistic angles; realistic bends for how they hold them. (Kept in mind, not built: the plank walk is dropped; the fore ratlines will tear loose in the storm, so the rope code is general.)
- **Source:** `main` at e99eeef2.
- **Changed** ([../SHIP.md](../SHIP.md#the-storms-lines-as-rope-november-7)):
  - New `ShipRope.h/.cpp`: rope tube (parallel-transport frame, tangents, capped ends, UVs for the scan's lay), `Sheave`, `Bent` (fillets, fist hold limit), `Droop`, `Coil`, block `Shell`.
  - `ShipCrew.h/.cpp`: `UpdateStormLines` rebuilds both hauling lines and the halyard each frame through the men's fists, over lead blocks stropped to the deck, to a fife-rail pin, slack to the deck and coiled; hauled strands ratchet toward the tail.
  - `DockNPC.h`, `DockNPCPirate.inl`: `GetStormHold`/`GetStormBlend` (how firmly each hand holds).
  - `ShipTest.cpp`: `storm_work` checks fist gap < 2 cm and worst turn < 25°; stills `RopeHands`, `RopeBlock`, `RopeTail`, `RopePin`.
  - New resource: ambientCG Rope001 (CC0) in `SourceAssets/Surfaces/Rope` (manifest + hashes), `Tools/Fetch-RopeTextures.ps1`, `Tools/create_rope_material.py` → `M_Rope` and `T_Rope001_*`.
- **Tested:** editor `chest+storm_work` pass (`Local/ship-rope-storm-work.log`). Packaged `Builds/RopeCandidate`: Verify-Package 182 passes (`Local/verify-package-20261107-203223.log`), ship 20/20 (`Local/verify-ship-20261107-211348.log`: rope gap 1.0 cm, worst turn 18.1°). Verify-Menu 44/44 (`Local/verify-menu-rope.txt`).
- **Remaining flaws:** the rope runs diagonally from the deck to the front man's hand for about a second while the rear man arrives; no belaying turns on the pin; Jeffries' ropes and the standing rigging still use the canvas tubes; the fore line's front fencer grips at hip height; not played by hand.
- **Contract compatibility:** only new accessors on `ADockNPC`; no rig or animation changes.
- **Binary/LFS:** three CC0 jpgs (LFS), `M_Rope.uasset` and three texture uassets (LFS).
- **Launcher:** promoted as Rope. The 14 `Common/Materials` re-saves from package builds are still left uncommitted (not this work).

Next part of the work could be done by either — preference: Claude (the reef points, the fore ratlines torn loose and the jump to the foremast).

## November 5 (night) — the nets' feet on deck; every reachable yard; the door frames (Claude)

- **Asked:** start the ratlines from the deck so it looks realistic and reads clearly as where the climb starts (secured to the stanchions' bases); brachiation on the rest of the yards; fix the glitchy wood in the door frames below deck.
- **Source:** `main` at 16e9c9b0.
- **Changed** ([../SHIP.md](../SHIP.md#follow-up-the-nets-feet-on-deck-every-yard-the-door-frames-november-5-night)):
  - `ShipDeck.inl`: `RailPosts` (shared post layout); `NetFoot`/`NetJoin` and the deck net (sheer pole on chocks, legs out over the rail to the shrouds 2 m up, ratlines, lashings to the stanchions' feet with iron bands); the climb line follows the net's inside face then turns over the join; `YardGrips` now both course and both topsail yards (40); `SailGiveWay` (canvas pushed round Chuck).
  - `ShipSetting.cpp`: door posts/lintel no longer coplanar with the bulkhead or each other; sails 32 × 24; `UpdateSails` feeds Chuck's body to the sails; check expects 40 grips.
  - `ChuckCharacter.cpp`: at the top of a climb with no lip, still climbing, he takes a grip over him (facing out); the body leans along a net's foot too.
  - `ShipTest.cpp`: `ratlines` reworked (net foot, lowered onto shrouds, topmast shrouds → topsail yard → three leaps → let go); stills `NetFoot*`, `TopsailYard`.
- **Tested:** editor ship trials ratlines/rigging/yard_drop/deck_up/cabin/galley 6/6 (`Local/ship-ratlines-test5.log`). Packaged `Builds/RatlinesYardsCandidate`: Verify-Package 182 passes (`Local/verify-package-20261105-193540.log`; ship 20/20). Verify-Menu 44/44 (`Local/verify-menu-ratlines-yards.txt`).
- **Remaining flaws:** the canvas folds sharply round him; the topgallant yards are out of reach (no ratlines to them); door flicker checked only in stills, not by hand.
- **Contract compatibility:** unchanged.
- **Binary/LFS:** none.
- **Launcher:** promoted as RatlinesYards. The 14 `Common/Materials` re-saves from the package build are left uncommitted in the working tree (restoring them was blocked this session).

Next part of the work could be done by either — preference: Claude (the captain's sequence and the plank out of the storm).

## November 5 (evening) — the ship's ratlines climbable; climbing sideways (Claude)

- **Asked:** "on the ship, remove the ladder and instead make all the ratlines climbable by chuck. for both this and in general in the game, chuck needs to be able to also climb sideways (been meaning to have you add this for awhile since its also needed to make chult trees smoother)".
- **Source:** `main` at a5f228a9.
- **Changed** ([../SHIP.md](../SHIP.md#the-ratlines-and-climbing-sideways-november-5)):
  - `ChuckClimbable.h/.cpp`: climbables can be nets: `Line` (a slanted or bent climbing line), `SideTravel`/`TopSideTravel`, `Approach` + `TurnZ` (walk on from the far side and turn round), `bTopOut`; helpers `ChuckClimbPoint/SideTravel/Facing/Up`.
  - `ChuckCharacter.h/.cpp`: the Ladder gait climbs across nets, follows slanted lines with the body lying along them, keeps the stick's up/down through the turn at the rail, does not re-mount straight after stepping off with the stick still held; the top-lip mount and jump-off use the new line/facing. Wall runs move across the wall with the stick held 40°+ off square (55 cm/s; on the golden leaf, held only across, the rise slows to 35%).
  - `ShipDeck.inl`, `ShipSetting.h/.cpp`: the rope ladder and lubber's hole removed; `DeckShrouds` (8 nets); the lower shrouds end at the tops' rims, the topmast shrouds start by the mast; the lookout's jar moved to the main top's fore starboard corner; check counts 4 ladders + 8 nets.
  - `ShipTest.cpp`: `rigging` goes up the shrouds; new trial `ratlines` (20 trials); new stills. `Tools/Verify-Package.ps1` expects 20 trials and 4 ladders.
- **Tested:**
  - Editor: `ratlines`, `rigging`, `yard_drop`, `ladder_up`, `deck_up` pass (`Local/ship-ratlines-test*.log`; the first `deck_up` failure was `-NoSound`, it passes with sound). Golden leaf trees 0/1 pass (`Local/golden-tree*-ratlines-editor.log`).
  - First package: golden tree 1 failed 2/2 (`climb_timeout`, hanging at the top) while the launcher build passed: the 20° sideways threshold drifted the test's climber 15 cm round the trunk. Raised to 40°; then passes.
  - Packaged `Builds/RatlinesCandidate`: Verify-Package 182 passes (`Local/verify-package-20261105-160832.log`; ship 20/20).
  - Verify-Menu 44/44 (`Local/verify-menu-ratlines.txt`).
- **Remaining flaws:** climbing across plays the up/down climb clip (no sideways clip); the turn over the rail is a yaw blend through the rail's coils; the fore port shrouds sit close to the braced fore course; not played by hand or on a controller.
- **Lesson:** a climb line must never pass the capsule through solid geometry: flying-mode depenetration pinned him at the rail cap climbing down. And use absolute `-ArchiveDirectory` paths (a relative one archived under `Source/Chuck3D/Builds` again; moved).
- **Contract compatibility:** `FChuckClimbable` only gained fields with defaults; existing ladders and lianas unchanged.
- **Binary/LFS:** none.
- **Launcher:** promoted as Ratlines.

Next part of the work could be done by either — preference: Claude (the captain's sequence and the plank out of the storm).

## November 5 (afternoon) — the storm waits for the deck; louder storm music; the second part at half the track (Claude)

- **Asked:** "have the music louder during storm and especially during intense storm. Only do normal storm for about half the song then fade into storm-intense song and start storm intense sequence. make sure storm music and storm doesnt start until after player's on deck, i want to avoid player being able to get below deck once this begins".
- **Source:** `main` at f8896c33.
- **Changed** ([../SHIP.md](../SHIP.md#follow-up-november-5)):
  - `ShipStorm.h/.cpp`: the storm begins only once the gate is open **and** he is on the weather deck 1.7 m or more from the companion hatch (`ShipRatClearOnDeck`); the rupture forms in the same frame. The second part comes at half the storm track's length (101 s).
  - `DockGameMode.cpp`: the storm track at 75%, the intense track at 100% (other scores 45%), neither ducked under the gale; a 3.5 s crossfade between them.
  - `ShipTest.cpp`: `chest` checks the storm waits while he is below (`storm_waits`); the storm trials' first-part stills moved before 101 s.
- **Tested:**
  - Editor `-ChuckShipTrials=chest+storm_work`: 3/3 (`Local/ship-storm-work-7.log`: storm_waits=1, start on deck, intense at 101.2 s with the music switch).
  - Packaged `Builds/ShipStorm2Candidate`: Verify-Package 182 checks (`Local/verify-package-20261105-094542.log`; ship 19/19). Verify-Menu 44/44 (`Local/verify-menu-shipstorm2.txt`).
- **Remaining flaws:** the new levels have not been heard on speakers (100% could clip against the storm's loops); not played by hand. The candidate was first written under `Source/Chuck3D/Builds` (a relative path from the shell's folder) and moved to `Builds/` before the menu check and promotion.
- **Contract compatibility:** unchanged.
- **Binary/LFS:** none.
- **Launcher:** promoted as ShipStorm2 (receipt f8896c3: built from the working tree before this commit). ShipStorm kept, HeroLod pruned.

Next part of the work could be done by either — preference: Claude (the captain's sequence and the plank out of the storm).

## November 5 — the concertina player's hands; the storm developed (Claude)

- **Asked:** fix the concertina player's hands ("backwards on the concertina"); develop the storm: an Astral rupture at the ladder keeping Chuck above deck, the user's Ship_storm track at the storm's start and Ship_storm_intense after it as the storm intensifies, no concertina switching in the storm, the crew working the ship (captain at the helm, no concertina), and in the intense part more wave influence, more wind and seas crashing on deck; free resources as needed.
- **Source:** `main` at e0a10f63.
- **Changed** ([../SHIP.md](../SHIP.md#the-storm-developed-november-4)):
  - `DockNPCPirate.inl`: the concertina player's palms now face the instrument (they faced out). The crew's storm work (`SetStormWork`, `TickStormWork`, `StormDelta`, `PoseStorm`): hauling a line in pairs, sweating a halyard, the captain steering; grips kept within reach along their line and taken the way round that keeps the wrist natural.
  - `DockNPC.h`: `EStormWork` and the storm members; no talk or turning to the rat while they work.
  - `ShipCrew.h/.cpp`: `BeginStorm`/`EndStorm`, the stations and the rigged lines (hauling parts, blocks, coils, the halyard); no fencing in the storm.
  - `ShipStorm.h/.cpp`: the second part (from the storm track's end, 202 s; harder wind, heavier rain, more spray and lightning); the rupture over the companion hatch (`TickRupture`, `BuildRupture`, `ShipRuptureSwallows`, `ShipRuptureOpen`); seas breaking aboard (`DeckWaveNow`, the water sheet, the shove); spume billows; `SetClock` now starts the storm properly.
  - `ShipSetting.h/.cpp`, `ShipDeck.inl`: the wheel moved to the posts' aft side and split out so it turns (`HelmHub`, `HelmSpoke`); deeper rolling and a bigger sea in the second part; deaths respawn on deck while the rupture is open.
  - `ChuckClimbable.h/.cpp`: `bBlocked` and `SetChuckClimbableBlocked`; `ChuckCharacter.cpp` skips blocked climbables and dies in the rupture.
  - `DockGameMode.h/.cpp`: the two storm tracks, kept at the storm's clock, ducked less under the gale; no concertina switching while the storm runs.
  - `DockMenu.cpp`: **Ship Storm Intense (dev)** checkpoint.
  - `ShipTest.cpp`: trial `storm_work` (19 trials). `Verify-Package.ps1` expects 19, `Verify-Menu.ps1` includes the new checkpoint.
  - `Tools/import_ship_music.py` (the two tracks; storm once, intense looping), `Tools/create_ship_storm.py` (`M_ShipSpume` from the CC0 Unity Labs smoke flipbook already in the project, `M_ShipDeckWash` with the sea's own textures). No new download was needed.
  - Assets: `SourceAssets/Audio/ShipStorm.wav`, `ShipStormIntense.wav` (the user's files, byte-for-byte; `SourceAssets/Audio/README.md`); `SW_ShipStorm`, `SW_ShipStormIntense`, `M_ShipSpume`, `M_ShipDeckWash`; `M_ShipRain`/`Spray`/`Bolt` rebuilt by the same script.
- **Tested:**
  - Editor `-ChuckShipTest -ChuckShipTrials=storm_work`: passed after fixes (`Local/ship-storm-work-6.log`); stills `Saved/Screenshots/WindowsEditor/Ship/Storm/Work_*.png` and `Ship/CrewConcertina*.png` reviewed.
  - Packaged `Builds/ShipStormCandidate`: Verify-Package passed 182 checks (`Local/verify-package-20261104-225516.log`); its ship test 19/19 (`Local/verify-ship-20261104-233643.log`: stations 6/6, worst grip 2.3 cm, wheel swing 68°, rupture death back on deck, wave shove, 15.6° roll, both storm tracks). Verify-Menu passed 44/44 (`Local/verify-menu-shipstorm.txt`).
- **Remaining flaws:**
  - Not heard on speakers: the storm tracks against the gale, slams and thunder.
  - The sea washing aboard passes through deck fittings and the crew's feet; the crew don't react to it. The haulers' feet stay planted and the line doesn't visibly run.
  - The rupture's veils use the sewer's oil-film style; the nearest can read as a flat sheet.
  - Up to about 15° of horizon tilt at the peaks of the second part: may be too much for some players.
  - Not played by hand. No plank, captain's sequence or end to the storm yet.
- **Contract compatibility:** Chuck's rig, scale and controls unchanged; `FChuckClimbable` gained a field (default off).
- **Binary/LFS:** two WAVs (75 MB), two sound waves (52 MB), five materials (LFS).
- **Launcher:** promoted as ShipStorm. Its receipt names e0a10f6 because it was built from the working tree before this commit. The HeroLod package is kept and HeroLook pruned.

Next part of the work could be done by either — preference: Claude (the captain's sequence and the plank out of the storm).

## November 5 — the heroes' distant LODs draw the right materials (Claude)

- **Asked:** "when i load the beholder encounter the fighters beard is still grey and the wizards skirt part of robe is still white while casting".
- **Source:** `main` at 44c9469b.
- **Found:**
  - The Temple Sanctum checkpoint starts Chuck on the gallery, so the heroes draw their reduced LODs (1 to 3).
  - `r.SkeletalMeshLODBias 1`, `2` and `3` captures in the package showed every LOD of the fighter and the wizard drawing other slots' materials: the wizard's skirt white, his hat gold, his robe green; the fighter's armour as cloth, his beard shade as mail. The ranger's LOD 2 and 3 were partly wrong too.
  - The cause was stale `LODMaterialMap`s, probably left from the October 28 slot removal. The previous fix was checked only in close-ups at LOD 0.
- **Changed** ([../CHULT-TEMPLE-HEROES.md](../CHULT-TEMPLE-HEROES.md#follow-up-the-distant-lods-november-4-night)):
  - `ChuckReviewLibrary.h/.cpp`: `FixLODMaterials(Mesh, bFix)` reports each LOD's drawn slots and clears the generated LODs' maps.
  - `Tools/fix_hero_lod_materials.py` (new): runs it on the three heroes (applied; `SK_HeroFighter`/`Wizard`/`Ranger` saved).
  - `Tools/import_temple_heroes.py`: runs it after `regenerate_lod`.
- **Tested:**
  - The report after the fix: every LOD's slots follow LOD 0's (minus the dropped brows and lashes) (`Local/hero-lods-fix.txt`).
  - Forced-LOD captures, in the editor (bias 1, 2) and in the package (bias 2, portraits and the bolt cast): all right.
  - Packaged `Builds/HeroLodCandidate`: Verify-Package passed 182 checks (`Local/verify-package-20261104-192305.log`). The first run (`-175310`) failed only "roll carries Chuck its authored distance": side drift 2.05 cm, just past the limit, in code this change doesn't touch; the smoke test passed rerun alone (`Local/verify-smoke-rerun-herolod.log`). Verify-Menu passed 43/43 (`Local/verify-menu-herolod.txt`).
- **Remaining flaws:**
  - Waterdeep's human NPCs come from the same kind of pipeline and were not checked at reduced LODs.
  - Not played by hand.
- **Contract compatibility:** asset fix plus an editor-only library function. The game code is unchanged.
- **Binary/LFS:** three hero skeletal meshes (LFS).
- **Launcher:** promoted as HeroLod (receipt c8c69ac). The HeroLook package is kept and FireballDamage pruned.

Next part of the work could be done by either — preference: Claude (check the Waterdeep NPCs' LODs the same way, then the captain's sequence at the storm's trigger and the plank).

## November 4 (night) — the fighter's young beard; the wizard's robe not blown white (Claude)

- **Asked:** "In beholder encounter Currently the fighter's beard reads as white/gray and the wizards robe partially turns bright white, fix this, youthful beard look for fighter".
- **Source:** `main` at b0a17dd7.
- **Found:**
  - The beard was specified dark brown. Since `r.HairStrands.Voxelization=0` (October 30, for Waterdeep) the strands have no self-shadowing and took bright highlights, so it read salt-and-pepper (it was dark in the October 29 portrait).
  - The robe whiteout came from effect lights a metre or two from his dark velvet. The worst was the Fireball gather shot (the gem light at 10,500 by his hat brim); the seal's glow and break flash and the sea tear's under-floor light also contributed.
- **Changed** ([../CHULT-TEMPLE-HEROES.md](../CHULT-TEMPLE-HEROES.md#the-fighters-beard-and-the-wizards-robe-november-4)):
  - The fighter's Beard and BeardJaw grooms: colour (.016, .0095, .0055), root .6, roughness .85, specular .04 (`SourceAssets/Heroes/heroes.json` and the manifest; `MI_Fighter_Beard`, `MI_Fighter_BeardJaw`).
  - `Tools/set_hero_groom_shading.py` (new): applies a manifest's groom shading to the existing instances, without re-importing the strands.
  - Light intensities:
    - `TempleSanctumScene.cpp`: the gather gem 10,500 → 4,100; the bead 14,000 → 9,000.
    - `TempleHero.cpp`: the gem's cast spike 4,700 → 2,050.
    - `TempleSeal.cpp`: the glow up to 5,100 → 2,800; the flash 26,000 → 9,000.
    - `ChultTempleWindBuild.inl`: the sea tear's daylight 4,200 / 700 → 2,400 / 600.
- **Tested:**
  - `-ChuckSanctumCapture -ChuckSanctumOnly=portraits+wizard_seal+fireball` passed (`Local/hero-look-3.log`, `-4.log`); stills reviewed (fighter head and figure, the gather shot).
  - Packaged `Builds/HeroLookCandidate`: Verify-Package passed 182 checks (`Local/verify-package-20261104-145612.log`; the temple heroes' test passed with failures=0). Verify-Menu passed 43/43 (`Local/verify-menu-herolook.txt`).
- **Remaining flaws:**
  - The voxel volume stays off, so very fine tips of dark hair can still catch light. Turning it on only in the sanctum would cost about 2 ms; I didn't do it.
  - The sanctum's sea tear itself reads as pale sky (in `M_TempleRiftSea` or the lighting; not changed here).
  - Not played by hand.
- **Contract compatibility:** light intensities and material instance values only. Chuck is unchanged.
- **Binary/LFS:** two material instances (LFS).
- **Launcher:** promoted as HeroLook (receipt 3ee3f58). The FireballDamage package is kept and SanctumFallen pruned.

Next part of the work could be done by either — preference: Claude (the captain's sequence at the storm's trigger and the plank).

## November 4 (evening) — the Fireball's damage in the rubble map (Claude)

- **Asked:** "Add more damage from the fireball, burn singe on rubble and the snake statues, cracks running through the walls, pillar collapse below roof breaks, and some of the water world astral rip that the wizard was trying to seal in the beholder encounter"; then "knock some of the torches over" and "don't have all of the torches lit, some would have gone out in the explosion".
- **Source:** `main` at f6c345fb.
- **Changed** ([../CHULT-TEMPLE-RUBBLE.md](../CHULT-TEMPLE-RUBBLE.md#the-fireballs-damage)):
  - `Tools/create_temple_scorch.py` (new) creates `M_TempleScorch`, a deferred decal (soot).
    - It is one custom node with its noise written inline, and it has no specular.
    - Lessons: two custom nodes each declaring a helper struct drew nothing, the standard Noise node speckled, and dark soot alone did not read on the dark stone.
  - `ChultTempleInterior.cpp`:
    - `FScorch` decals (the material loaded by path; /Game/Art is always cooked).
    - The `SeaLow` mesh for the rubble map's sea tear, with `SeaZ` shifted and `Gain` 0.55.
    - The `BuildRubble` call takes the scorch list.
  - `ChultTempleWindBuild.inl`:
    - `FSanctumFall` gains `bSeaOpen`, `Fate` and outputs for the knocked torches and tipped bowls.
    - In `Sanctum()`, torches and bowls go through `WallTorch`/`Bowl` wrappers: burning, out (a black cup, no fire) or knocked down (the mount alone, or the plinth alone).
    - Without a fall, nothing changes.
  - `ChultTempleRubble.h/.cpp`:
    - Vault holes over both fallen piers; the nave and crevice holes moved.
    - `BlastS`/`BlastT`.
    - The sea tear counts as an Astral hole, with a slab half over it.
  - `ChultTempleRubbleBuild.inl`:
    - `Crack`, `FallenTorch` and `TippedBowl`.
    - Cracks down walls, up near the blast, from the stair breaks and across the vault.
    - The scorch list, kept off the lane because Chuck takes decals.
  - `ChultTempleRubbleTest.cpp`: 5 more stills.
  - `Tools/Verify-Package.ps1`: `wind_glows=55`.
- **Tested:**
  - Uncooked rubble test: failures=0 floating=0 heard=0 unsourced=0, with lane, crawl and rift_fall passing; knocked_torches=7, tipped_bowls=1, decals=50 with material=1 (`Local/rubble-fire-2.log`). Stills reviewed.
  - Packaged `Builds/FireCandidate`:
    - Verify-Package passed 182 checks (`Local/verify-package-20261104-115635.log`), with the packaged rubble test showing the same counts.
    - Verify-Menu passed 43/43 (`Local/verify-menu-fire.txt`).
- **Remaining flaws:**
  - The rip's sea reads paler and more sky-like than the sanctum's own.
  - Soot is decals only: no charred stone, smoke or glowing embers in it.
  - Cracks are flat lines.
  - Frame time is not measured (about 400k interior triangles, about 50 decals).
  - Not played by hand.
- **Contract compatibility:** `Sanctum()`'s fall parameter is still optional, and the sanctum is unchanged. Chuck is unchanged; the decals are kept away from him rather than turning his decals off.
- **Binary/LFS:** `M_TempleScorch.uasset` (about 10 KB, LFS).
- **Launcher:** promoted as FireballDamage (receipt a4fd719). The SanctumFallen package is kept and Storm pruned; the backup folder is named `Windows-Previous-20261104-FireballDamage` by the script, though it holds SanctumFallen.

Next part of the work could be done by either — preference: Claude (the captain's sequence at the storm's trigger and the plank), or a hand-play pass of the rubble map by the user.

## November 4 — the rubble map as the sanctum fallen in (Claude)

- **Asked:** "make the rubble map look more convincingly like the temple sanctum had partially collapsed in. there should be rubble from the balcony, blocked off stair cases, and only torches where there were torches in the temple sanctum map". Follow-up, mid-task: every piece of rubble must have fallen from somewhere discernible; no unexplained or unrealistically stacked piles and nothing floating; new Astral areas may close off access.
- **Source:** `main` at ebdccc18.
- **Changed** ([../CHULT-TEMPLE-RUBBLE.md](../CHULT-TEMPLE-RUBBLE.md)):
  - `ChultTempleWindBuild.inl`:
    - `Sanctum()` takes an optional `FSanctumFall`:
      - gallery runs kept;
      - piers broken to stumps;
      - no ceiling;
      - aisle ruptures and the sea tear buried by mask;
      - the arch walled;
      - wall and stair-mass openings;
      - stair breaks.
    - `StairMass` takes openings and breaks; `SanctumHoles` takes a rupture mask.
    - With no `FSanctumFall`, the sanctum builds as before; its architraves are now made in spans between piers, the same shape.
  - `ChultTempleRubble.h/.cpp`:
    - New layout: the hall frame is the sanctum's, `DropZ` −4000.
    - The crevice now opens under the −S stair (`MouthS`, `CrawlT` −5950), with a new lane.
    - The fall's own 4 ruptures and 4 vault holes.
    - `GalleryFall`, `StairBreakDebris`, `FallenPierPieces` and the hall's `Fittings`.
    - Heaps only under the vault holes, at most 1.4 m high.
    - `ChultTempleRubbleBaseAt`, `ChultTempleRubbleSourced` and `ChultTempleRubbleUnsourced`.
    - The old seeded chamber, round drums and lane braziers/torches are gone.
  - `ChultTempleRubbleBuild.inl`:
    - Builds the hall via `Sanctum(&Fall)`, shifted down.
    - The broken vault, the gallery's sagging broken ends, the arch choke, the stair-break faces.
    - Hidden walls past the breaks.
    - Gravel only where something came down.
  - `ChultTempleRubbleTest.cpp`:
    - 15 new stills.
    - A passage trace before the crawl.
    - Rift fall into the sanctum's +S aisle rupture.
    - Fails on `unsourced>0`.
  - `ChultTempleInterior.cpp`: the call site (idols, steps) and the spawn's facing.
  - `Tools/Verify-Package.ps1`: `wind_glows=53`.
- **Tested:**
  - Uncooked rubble test: failures=0 floating=0 heard=0, lane, crawl and rift_fall pass, 92 blocks, unsourced=0 (`Local/rubble-fall-10.log`). Interior check passes with spawns=21.
  - About 15 stills reviewed (`Saved/Screenshots/WindowsEditor/Chult/Rubble*.png`).
  - Packaged `Builds/RubbleCandidate`:
    - Verify-Package passed 182 checks (`Local/verify-package-20261104-093203.log`); the ship escape from the moved crevice passes.
    - The first verification (run by Build-Game) failed only the Astral wind `full` trial, with a spike-pit death in tunnel leg 4, which this change does not touch. A rerun of that test on the same package passed (`Local/verify-temple-wind-rerun-rubble.log`), so treat it as a flake.
    - Verify-Menu passed 43/43 (`Local/verify-menu-rubble.txt`).
- **Remaining flaws:**
  - Not played by hand.
  - The rubble is plain boxes.
  - The stair breaks are straight cuts with spalls.
  - One sagging gallery slab reads as a thin dark plane from below.
  - The copy doubles the sanctum's geometry (about 382k interior triangles) and adds about 40 unshadowed fire lights; frame time is not measured.
  - First tries that were rejected and replaced: stacked stair chokes, rubble standing in the Astral pit, and a crevice behind the stair's foot (the stairs drop 13 m, not 7).
- **Contract compatibility:** `Sanctum()`'s new parameter defaults to null. `TempleRubble` constants moved, but ShipTest and ShipEscape use only `CrawlBendS`, `CrawlEndT` and `AtChultTempleCreviceEnd`. Chuck is unchanged.
- **Binary/LFS:** none (code and docs only).
- **Launcher:** promoted as SanctumFallen (receipt e911248). The Storm package is kept and ShipSounds pruned. The backup folder is named `Windows-Previous-20261104-SanctumFallen` by the script, though it holds the Storm build.

Next part of the work could be done by either — preference: Claude (the captain's sequence at the storm's trigger, the plank and the fall), or a hand-play pass of the rubble map by the user.

## November 3 — the chest, the trigger and the storm (Claude)

- **Asked:** "when the ship trigger takes effect, i want a storm to roll in and for the ship going through a storm to be a thing for a good while before the captain kicks chuck off the plank. start working on this (the chest, the trigger, the storm). implement sounds for the storm too and really make it look and feel cool. don't do the plank part yet".
- **Source:** `main` at 37f3e30f.
- **Changed** ([../SHIP.md](../SHIP.md#the-chest-the-trigger-and-the-storm-november-3)):
  - `ShipStorm.h/.cpp` (new):
    - `ShipGate` holds the 2D game's seven conditions (the chef has seen him; one talk each with the seated pirate, concertina player, cheering pirate, dancer and Jeffries; the chest open). They are kept through deaths aboard and reset by a fresh ship start.
    - `AShipChest`: a hinged lid that opens once, by scratch or interact. It drops a golden carton.
    - `AShipStorm` runs the roll-in clocks (wind, dark, sea, rain, lightning) and handles:
      - rain streaks with splashes;
      - spray bursts with wave slams;
      - lightning bolts built from forks, with a flash light and delayed thunder;
      - six storm loops mixed for on deck and below decks.
  - `CigarettePickup.h/.cpp`: `SpawnCarton`. A carton gives 40 cigarettes and the 45 s stamina lock, and is taken once.
  - `ChuckCharacter.cpp`: `Interact()` opens the chest when Chuck faces it.
  - `ShipSetting.h/.cpp`: `ApplyStorm` (sun, sky, clouds, fog, post-process, sea), a second `FlashLight`, storm rocking (6.5° roll, 3° pitch, chop, heel). Swinging lamps and hammocks scale with the sea, and the ambience and creaks with the storm. The chest's lid was taken out of the hull mesh.
  - `ShipDeck.inl`: sail flutter and the flag in wind. `ShipSea.inl`: extra sea rings out to 150 m.
  - `DockGameMode.cpp`: the music ducks under the gale. `DockMenu.cpp`: new **Ship Storm (dev)** checkpoint, and a ship start resets the gate.
  - `ShipTest.cpp`: new `chest` and `storm` trials (18 trials), plus storm stills (`-ChuckStormStills=`).
  - Tools:
    - `Tools/create_ship_sea.py`: the sea's `Storm` parameter. It also rebuilds `M_ShipSail`.
    - `Tools/create_ship_storm.py` (new): `M_ShipRain`, `M_ShipSpray` and `M_ShipBolt`.
    - `Tools/make_storm_sounds.py`, `Tools/decode_storm_mp3.py` and `Tools/import_storm_sfx.py` (new): 23 sounds cut from 19 CC0 BigSoundBank recordings. `storm-recorded-manifest.json` records them.
    - `Verify-Package.ps1` expects 18 ship trials; `Verify-Menu.ps1` gains the new checkpoint.
  - Docs: `docs/SHIP.md`, `docs/regions/SHIP.md`, `docs/HANDOFF.md`, `SourceAssets/Audio/README.md`.
- **Tested:**
  - Uncooked full ship test passed 18/18 (`Local/storm-full.log`); the gate opened naturally, through the crew talks, the chef chase and the chest.
    - Chest: lid at 108°, 40 cigarettes, stamina locked.
    - Storm:
      - 2,539 drops shown, 6 sprays, 3 strikes with their thunder;
      - roll 10.3°;
      - worst frame 43 ms uncooked;
      - the mix checked on deck and below.
  - About 20 storm stills were reviewed (`Saved/Screenshots/WindowsEditor/Ship/Storm/`).
  - Packaged `Builds/StormCandidate`: Verify-Package passed 182 checks (`Local/verify-package-20261103-234647.log`), with the ship test at 18/18 (`Local/verify-ship-20261104-002802.log`). Verify-Menu passed 43/43, including Ship Storm (`Local/verify-menu-storm.txt`).
  - Editor smoke test after packaging:
    - The first run failed the two turn-in-place checks.
    - The rerun passed both (`Local/smoke-storm-rerun.txt`). This looks like a timing flake; `Interact` is the only character change.
    - The 7 "imports render separately from collision" checks still fail, as they do on unmodified main.
- **Remaining flaws:**
  - The storm has no end yet; only the menu stops it. The captain's sequence and the plank are not built.
  - Not heard on speakers: the balance is measured, not listened to. Not played by hand.
  - The crew do not react to the storm, and the sails are not reefed.
  - A faint speckle shows on edges against the clouds for a frame or two at a flash.
  - No wet look on deck or crew. Rain passes through the sails and tops; none comes through the gratings below.
- **Contract compatibility:** new actors and the `ShipGate` namespace; the `CigarettePickup` carton is additive. Chuck's only change is that `Interact` returns after starting an NPC talk, then tries the chest; nothing changes elsewhere.
- **Binary/LFS:** 23 WAVs (~36 MB), 23 sound uassets plus 3 new and 2 rebuilt materials (~18 MB), all LFS-tracked by the existing patterns.
- **Launcher:** promoted as Storm. The receipt says 37f3e30 because the source was uncommitted when it was built; the commit follows. The prior package was kept and Fencing pruned.

Next part of the work could be done by either — preference: Claude (the captain's sequence at the trigger, the plank and the fall out of the storm).

## November 3 (evening) — the ship heard: steel, feet, sea, rigging; rats only in the hold; quieter creaks (Claude)

- **Asked:** "Find and download sword fight sounds and also other immersive sounds, such as the boat going through the water. Maybe also some feet on deck sounds for the fighting and the dancing. Make it so you can only hear the rats in the hull and nowhere else on the ship. Decrease the volume of the creaking sound that we currently have."
- **Source:** `main` at 01b362f2.
- **Changed** ([../SHIP.md](../SHIP.md#the-ship-heard-november-3)):
  - Recordings (all CC0): BigSoundBank (Joseph Sardin): sword swish and scrape, waves and froth, marina lapping, wooden-pontoon steps and jumps, wooden-floor steps, masts whistling. Still North Media via OpenGameArt: sabre blade-on-blade and hilt-on-blade hits. Rejected: two ferry recordings (engine hum), one truncated download.
  - `Tools/make_ship_sounds.py` (new): checks the source hashes, cuts 30 sounds (mono 16-bit, −3 dBFS; the loops crossfaded) into `SourceAssets/Audio/SFX/SFX_{SwordClash,SwordBind,SwordScrape,SwordSwish,DeckStep,DeckStamp,ShipWater,ShipLap,ShipRigging}_*.wav`, writes `ship-recorded-manifest.json`. `Tools/import_ship_recorded_sfx.py` (new) imports them to `Content/Art/Audio/SFX`.
  - `ShipSetting.h/.cpp`: `ShipSound()` / `AShipSetting::PlayAt` (near or carrying attenuation, ×0.3 through a deck, small pitch spread, counted); `ShipHearsHold()`; three new ambience loops (water along the hull and rigging wind on deck, lapping below, the old wash kept) mixed by where he is and how high he has climbed; creaks 0.35–0.55 → 0.14–0.22.
  - `EnemyRat.h/.cpp`: aboard, the rats' sounds play only when he is in the hold (`ShipSoundsHeard` / `ShipSoundsHushed` counters).
  - `ShipCrew.h/.cpp`: each blow's swish, the clash at the blade crossing (bind hilt hits in a bind) and the bind's scrapes, on the beat; `GetFenceClashes()`.
  - `DockNPC.h`, `DockNPC.cpp`, `DockNPCPirate.inl`: the dancer's steps on each beat (a stamp every eighth); the fencers' steps (a stamp on a long lunge), only for steps over 16 cm.
  - `ShipTest.cpp`: new `sounds` trial (10 s on deck, 10 s in the hold); `Tools/Verify-Package.ps1` now expects 16 ship trials.
  - Docs: `docs/SHIP.md`, `docs/HANDOFF.md`, `SourceAssets/Audio/README.md`.
- **Tested:**
  - Uncooked full ship test 16/16, failures 0 (`Local/sounds-full.log`); fencers+sounds rerun 2/2 (`Local/sounds-steps.log`).
  - Packaged `Builds/SoundsCandidate`: Verify-Package 182 passes (`Local/verify-package-20261103-123030.log`); ship 16/16 (`Local/verify-ship-20261103-131134.log`): on deck 11 clashes, 94 feet, rats heard 0 (40 hushed), mix wash .20 / water .60 / lap 0 / rigging .35; in the hold rats heard 36, lap 1.0, water .14, rigging .04; 4 creaks. Verify-Menu 42/42.
- **Remaining flaws:** the mix and the volumes are measured, not listened to on speakers. The feet may still be busy (94 sounds in 10 s with the dancer and both fencers). Through-deck muffling is volume only (no low-pass). The "small waves" recording is a beach; whether it reads as a hull at sea is unchecked.
- **Contract compatibility:** new free functions `ShipSound`/`ShipHearsHold`; `EnemyRat` statics; `FBlow` gains Swish/Heard. Nothing else changes for Chuck or other regions; rats outside the ship sound as before.
- **Binary/LFS:** 30 WAVs (~11 MB) and 30 uassets (~6.3 MB), LFS-tracked by the existing patterns.
- **Launcher:** promoted as ShipSounds (receipt 01b362f; prior package kept, Swing4 pruned).

Next part of the work could be done by either — preference: Claude (milestone 4: the chest, what the captain does at the trigger, the plank and the fall).

## November 3 (morning) — the fencers' fight made harder (Claude)

- **Asked:** "Make the sword fighter's fight more intense and complex while being smooth realistic good looking movements".
- **Source:** `main` at 4685279e (begun on 5a3e115b; the brachiation commit landed in between and is untouched).
- **Changed** ([../SHIP.md](../SHIP.md#the-fight-made-harder-november-3)):
  - `DockNPC.h`: new `EFencePose` (20 sword positions), `FFenceKey`, `FenceCurve` (Hermite: flow keys sweep through, stop keys hold). `FFenceCue` now carries four keys, the partner's blade and head, and Lunge/Lean/Twist/Crouch/Stride.
  - `ShipCrew.h/.cpp`: `AShipCrew::PlanPhrase` plans each phrase (seeded by its number) as blows of seven kinds (head, flank, low, lunge, feint, beat, bind), ripostes 72%, a circling phrase every fifth; ground gained per blow kept on the fencing ground; FenceHalf 85 → 75. Each blow's blade gap is measured (`CHUCK_FENCE_BLOW`), with new getters for contacts, touching, worst gap and the tally.
  - `DockNPCPirate.inl`: the fencer pose rewritten: keyed fist/blade/edge/pole on the curve, each blow aimed onto the partner's blade near his hilt and clear of his head; body twist, lunge, lean, crouch and beat bounce; steps 0.15 s; the off hand thrown back in a lunge.
  - `ShipTest.cpp`: the `fencers` trial also needs at least 4 contacts with 90% touching (within 4 cm); new `-ChuckFenceFilm` (90 side stills, 0.1 s apart).
  - Docs: `docs/SHIP.md`, `docs/HANDOFF.md`.
- **Tested:**
  - Uncooked full ship test 15/15, failures 0 (`Local/fence-full.log`): 508 blows, 507 touching, worst 7.6 cm; near hits 4, far 0, fence/hull out 0.
  - Film review (`Local/fence-film3.log`, stills in `Saved/Screenshots/WindowsEditor/Ship/Film/`): 276/276 touching, worst 2.8 cm.
  - Packaged `Builds/FencingCandidate`: the first Verify-Package failed on the temple heroes `fireball` trial (`sanity_after=1`, `Local/verify-temple-heroes-20261103-092436.log`), unrelated code; the rerun on the same build passed: 182 passes (`Local/verify-package-20261103-093215.log`), fireball pass=1 (`Local/verify-temple-heroes-20261103-100253.log`), ship 15/15 with 506/506 touching, worst 2.3 cm (`Local/verify-ship-20261103-101318.log`). Verify-Menu 42/42.
- **Remaining flaws:** reviewed from stills, not watched in motion or played by hand. No steel sound (the user's audio to come). The fingers don't regrip between cuts and thrusts. In a bind the two bodies aren't checked against each other. The temple fireball trial failed once (Chuck's Sanity 1 instead of 2) and passed on the rerun: a flake worth watching.
- **Contract compatibility:** `FFenceCue` changed (old Thrust/Parry fields gone); only the ship fencers use it. Chuck, the rat hits and the other crew are unchanged.
- **Binary/LFS:** none.
- **Launcher:** promoted as Fencing (receipt 4685279; prior package kept, ShipFixes pruned).

Next part of the work could be done by either — preference: Claude (milestone 4: the chest, what the captain does at the trigger, the plank and the fall).

## November 3 — big leaps: one swing back then forward is enough; no sweet spot (Claude)

- **Asked:** "I want it to be easier to get a good back and forth swing, if the player goes back the full amount then forward the full amount and releases while going forward I want that to result in a successful jump to the next plank, and I want it to look and feel realistic. I don't want just holding forward to make it across, you should have to go back then forward at least for the bigger gaps. but I also don't want it to be as tricky of a sweet spot as it currently is ... make forward swing you more forward easier".
- **Source:** `main` at 5a3e115b.
- **Changed** ([../BRACHIATION.md](../BRACHIATION.md), 11-02 section):
  - `ChuckCharacter.h/.cpp`:
    - **Forward beat:** pushing forward on the forward stroke gets up to 2.25x the pump gain, scaled by how far back the last back stroke went (10° nothing, 20° and over full); reset at each forward turn, so holding forward builds no beat. New constants `SwingForwardBeat` 1.25, `SwingBeatBackFrom` 10, `SwingBeatBackFull` 20; members `SwingBackAmp`, `SwingTurnedAt`, `SwingTurnSign`.
    - **Lean start:** below 12° of swing the stick leans him (0.75 of the gain) instead of the old kick, which left him stuck near 6° when holding back from a still hang.
    - **No sweet spot:** a jump pressed anywhere on the forward stroke takes the best reach that stroke will give (`SwingStrokeBest`, sampled from the swing's energy up to the arc limit) and leaves at that point; pressed within `SwingTopGrace` 0.15 s after the forward top it still goes. Pressed on the back stroke it waits for the next forward one. New `SwingReachAt`, `SwingStrokeBest`; log `CHUCK_SWING_STROKE_BEST`.
  - `ChultBridge.cpp`: trial 8 is now one swing back then forward (stick back until it swings back, forward as it comes through, jump going forward); new trial 11 presses as it tops out. 11 trials.
  - `Tools/Verify-Package.ps1`: bridge `trials=11`.
  - Docs: `docs/BRACHIATION.md`, `docs/CHULT-BRIDGE.md`, `docs/HANDOFF.md`.
- **Tested:**
  - Uncooked bridge 11/11 (`Local/brach-b5.log`): from still, stick let go 49 cm, held forward 55.7, held forward and jumping on 153.6 (best 159.6) of 174.6, all short; one swing back then forward 203.6 (best 212.2) of 180, made; pressed as it tops out made (best 197.6).
  - Uncooked suite (`Local/brach-s4-*.log`): bridge 11/11, canopy 48/48, giant 72/72, raptor 8/8 (route 8/8), birds crossed, lamp 8/8.
  - Packaged `Builds/Swing4Candidate`: Verify-Package 182 passes (`Local/verify-package-20261102-233317.log`); bridge 11/11, failures 0, head 69 of 3624 frames more than 1 cm into a plank, 2.6 cm worst (`Local/verify-bridge-20261103-004844.log`).
- **Remaining flaws:** not played by hand or on a pad. A top-grace press looks like a thrown leap from nearly no swing speed. Holding back about 0.3 s too long, or switching forward too early, still falls short (by design). The 44° deck limit still clamps hard. Head worst 2.6 cm (was 1.9). The short-leap scaling still uses the current reach, not the stroke's best. Route tests use the pump assist.
- **Contract compatibility:** Chuck only: new constants, members and methods (above); no change to grips or other callers. Bridge test count 11.
- **Binary/LFS:** none.
- **Launcher:** promoted as Swing4 (receipt 5a3e115 plus this work; prior package kept, Concertina pruned).

Next part of the work could be done by either — preference: Claude (retune the swing after you've played it by hand).

## November 2 (late night) — ship fixes: the flag, the "mop", the lumber pile, the hammocks, the dancer, the captain's hand (Claude)

- **Asked:** "Make the pirate flag more prominent while still being in a spot consistent with real flag placement on a ship like this. Move the awkward floating mop on the deck so that it's not propping itself up impossibly. Instead of temple stones in the ship have this be a pile of lumber with just small section of stone where you climb out, with our fade out oil slick around the small stone opening, indicating collided worlds. Lower the hammocks to be more realistic placement. Move the dancing pirate forward some so that he's dancing on the deck and not on the grate. Fix the captains hand for how he picks up his sword".
- **Source:** `main` at 4b943915.
- **Changed** ([../SHIP.md](../SHIP.md), "Fixes on deck and below"):
  - `ShipDeck.inl`: `FlagHoist`/`FlagFly`/`FlagDrop`; the flag at the spanker gaff's peak, 2.6 x 1.65 m, streaming to leeward; a halyard with a hanging tail.
  - `ShipSetting.cpp`: flag grid 14 x 9; new surfaces `ShipLumber` (deck planks material, non-solid) and `ShipRiftVeil` (`M_AstralOilMist`, no shadow); the arrival corner rebuilt as a board stack with heavy timbers and off-cuts, a small stone mouth (jambs, lintel, three broken pieces, grit) and a 46 cm haze band round it. The crawlway's invisible collision boxes are unchanged. Hammock beds 50 cm lower; the two hammocks at x = -960 (whose lanyard came up through the deck: the "mop") moved forward of the break.
  - `ShipCrew.cpp`: dancer (215, 92), cheering pirate (100, 215); the captain's fist scale set from his hand against fencer A's (`CHUCK_CAPTAIN_FIST`).
  - `DockNPC.h/.cpp`: `GetHandWorld`, `GetHandLength`, `SetFistScale`/`GetFistScale` (fist reach and palm depth scaled; default 1, so other NPCs are unchanged); `SetCaptainTime(Seconds, HoldFor)` can hold his round for stills.
  - `DockNPCPirate.inl`: the cutlass before his right foot; reach 15.6–16.7 s; a new grip frame (thumb toward the blade, knuckles forward, elbow out); his right hand's fingers open and close through the reach and the let-go.
  - `ShipTest.cpp`: new stills (Flag*, ArrivalPile, ArrivalMouth, BreakStarboard, QuartersAft, Captain* and hand-following CaptainHand*); dancer and cheering views follow them.
  - Docs: `docs/SHIP.md`, `docs/regions/SHIP.md`.
- **Tested:**
  - Uncooked full run: ship check 0 failures, 13/15 (`Local/ship-fixes-full.log`); yard_drop and chef_chase failed while the frame rate fell to 2–8 fps mid-run, and passed on a rerun with rigging, seam and sea (`Local/ship-fixes-rerun.log`). Views-only runs reviewed by eye.
  - Packaged `Builds/ShipFixesCandidate`: Verify-Package 182 passes (`Local/verify-package-20261102-210941.log`), ship 15/15, failures 0 (`Local/verify-ship-20261102-215047.log`); Verify-Menu 42/42 (`Local/shipfixes-verify-menu.log`).
  - Stills: `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/Ship/` (Flag*, Arrival*, BreakStarboard, QuartersAft, Captain*, Dancer*).
- **Remaining flaws:** not played by hand; the haze keeps the cellar's colours, untuned for the lamplit sail locker; the flag is mirrored on its far side; the captain's fingers are a single open/close blend, not posed per finger round the grip.
- **Contract compatibility:** no changes to Chuck. New `ADockNPC` API (above); new surfaces `ShipLumber`, `ShipRiftVeil`; collision unchanged.
- **Binary/LFS:** none.
- **Launcher:** promoted as ShipFixes (receipt 4b94391 plus this work; prior package kept, Captain pruned).

Next part of the work could be done by either — preference: Claude (milestone 4: the chest, what the captain does at the trigger, the plank and the fall).

## November 2 (night) — the concertina version on deck, matched to the tune (Claude)

- **Asked:** "when you're belowdeck on the ship the main/current theme plays, then when you go above deck the concertina version starts playing BUT i want it to match the current part of the song as close as possible ... I'd also like this to feel like it's coming from the concertina player if it can get louder when you're closer to the concertina player (though not dramatically, i want to hear it from all points of the weather deck)" (file `Ship_concertina.wav`).
- **Source:** `main` at 9bf2b9bc.
- **Built** ([../SHIP.md](../SHIP.md), "Below decks and above"):
  - `SourceAssets/Audio/ShipConcertina.wav` (new, byte copy of the user's file, LFS) imported as `SW_ShipConcertina` (looping, 187.6 s) by `Tools/import_ship_music.py`, which now keeps tracks already imported unless `CHUCK_REIMPORT_SHIP_MUSIC=1`.
  - `Tools/align_ship_music.py` (new; runs on Blender's bundled Python for numpy): beat grids for both (ship 135.725 bpm, first beat 0.091 s; concertina 128.985 bpm, 0.417 s) and two-way time maps from a 4 s chroma similarity path with rare jumps (the concertina plays the repeated opening once) refined by onset cross-correlation. Writes `Source/Chuck3D/ShipMusicMap.inl`; `--demo` writes listening crossings to `Local/ShipMusicSwitch/`.
  - `ShipCrew.cpp/.h`: `ShipScore` now knows which track plays (`ETrack`), its position, the matching moment in the other, and a beat count that runs on through loops and changes of track. The old 136.12 bpm constant is gone.
  - `DockGameMode.cpp/.h`: the ship branch picks the track by `ShipSpaceAt` (deck = concertina), switches after 0.4 s with a 2 s fade out and 1.5 s fade in at the matched time, logs `CHUCK_SHIP_MUSIC_SWITCH`. The concertina is spatialised at the player (non-spatialised blend 200→6000 cm, no distance attenuation) and scaled x1.3→x0.75 by distance (150→1400 cm). `MusicTracks` has 11 entries; new getters `GetConcertinaNear`, `GetShipMusicSwitches`.
  - `ShipTest.cpp`: the deck_up and deck_down trials wait 1.5 s at the end and require the right track (`CHUCK_SHIP_MUSIC_FOLLOWS`).
- **Tested:**
  - Uncooked: deck_up, deck_down, fencers, quarterdeck 4/4 (`Local/ship-music-test2.log`); fencers' parry gap normal after the continuous beat fix.
  - Packaged `Builds/ConcertinaCandidate`: the first Verify-Package stopped on the temple wind trial pit2 (a death; `Local/verify-temple-wind-20261102-174053.log`), code unrelated, passed in all earlier runs. The re-run on the same build passed: 182 passes (`Local/verify-package-20261102-174800.log`), temple wind all passing (`Local/verify-temple-wind-20261102-181204.log`), ship 15/15 with both music trials passing and seven matched switches (`Local/verify-ship-20261102-182902.log`). Verify-Menu 42/42 (`Local/concertina-verify-menu.log`). Watch pit2 for another flake.
  - Listening crossings for the user: `Local/ShipMusicSwitch/*.wav`. Not heard in the game on speakers; not played by hand.
- **Remaining flaws:** the two beat grids agree to about 0.2 beat (median) after a switch, so the crew can be slightly off the concertina for a moment; the concertina player's hands are not matched to its notes; the match is to the nearest phrase where the concertina skips the repeat.
- **Contract compatibility:** no changes to Chuck. `ShipScore::Started` now takes an `ETrack`; new `/Game/Art/Audio/SW_ShipConcertina`.
- **Binary/LFS:** the WAV (36 MB) and the imported asset (22 MB), both LFS.
- **Launcher:** promoted as Concertina (receipt 9bf2b9b plus this work; prior package kept, older pruned).

Next part of the work could be done by either — preference: Claude (milestone 4: the chest, what the captain does at the trigger, the plank and the fall).

## November 2 (evening) — the pirate captain and the moustache flag (Claude)

- **Asked:** "Make the captain. Here are reference images. In the 2d game the captain only makes his appearance after the trigger but in this game I want him on deck from the start, we'll go over later what he does with the trigger. The captain is a short Halfling but he wears very tall platform boots. Also add this pirate flag to the ship" (two captain pictures and the flag picture).
- **Source:** `main` at 36fe2d72.
- **Built** ([../SHIP.md](../SHIP.md), "The captain and the flag"):
  - `humans.json` `PirateCaptain`: a halfling of about 99 cm on platform soles of about 13 cm (112 cm standing, 116 cm with the tricorn). He has a burgundy gold-braided frock coat with turned-back cuffs, a waistcoat and coat with brass buttons, a belt buckle over a sash, and knee boots with three buckled straps. His dark shoulder-length curls and waxed handlebar moustache with curled ends are strand grooms; he has no beard.
  - `build_npc_humans.py`: new kit options `coatskirt`, `bootstrap`, `platform` (boots), `hardware` (buckles and buttons, Brass slot), rolled-trim `above`/`below`, and `moustache_curl`. `npc_strands.py`: new `handlebar` groom style. All are optional, and older specs build unchanged.
  - `DockNPC.cpp/.h`, `DockNPCPirate.inl`: `EDockHuman::PirateCaptain`. He stands with his fists on his hips beside his cutlass, which is planted in the deck. Every 28 s he pulls it out, holds it up and out, then plants it again. Public `SetCaptainTime`, `GetCaptainPhase` and `GetCaptainTipDepth`. `ShipCrew.cpp` places him on the quarterdeck at (-1065, -45), just forward of the skylight. He has no lines.
  - The flag: `Tools/create_ship_flag.py` (new) imports the user's picture as `T_ShipFlag` and makes `M_ShipFlag` (two-sided foliage). `ShipSetting.cpp` puts it on the flag at the main truck, and the ship check counts it as missing if it is absent.
  - `ShipTest.cpp`: ten new stills (seven of the captain, three of the flag) and `CHUCK_SHIP_CAPTAIN`, which fails unless he is aboard with his mocap and his cutlass, the planted tip is 1–8 cm into the deck, and his fist is within 8 cm of the grip.
- **Tested:**
  - Uncooked: views-only run with 0 failures, and `CHUCK_SHIP_CAPTAIN pass=1 tip_depth_cm=4.0 grip_cm=2.9`. Trials quarterdeck, crew_talk and rail passed 3/3. The in-engine stills are in `Local/CaptainReview/captain_and_flag.png`.
  - Packaged `Builds/CaptainCandidate`: Verify-Package 182 passes (`Local/verify-package-20261102-145220.log`). The newest ship log has `CHUCK_SHIP_TEST failures=0 trials=15 passed=15` and `CHUCK_SHIP_MUSIC_CHECK failures=0`, and the captain check passed. Verify-Menu passed 42/42 (`Local/captain-verify-menu.log`).
  - Not played by hand; no Xbox pad.
- **Remaining flaws:**
  - Nothing happens with him at the trigger yet: no lines and no behaviour (to be decided with the user, milestone 4).
  - The handlebar moustache reads as a dense, solid mass rather than waxed hair.
  - His face is a generic halfling, not yet close to the reference.
  - The coat sleeves are bulky.
  - The Blender review renders don't show strands.
  - The flag is mirrored from the starboard side (the same picture on both faces, as on a real flag).
- **Contract compatibility:** no changes to Chuck's rig, slots or sockets. There is a new `EDockHuman` value before `Count` and a new public captain API. The ship now needs `/Game/Art/Ship/M_ShipFlag`.
- **Binary/LFS:** the captain's FBX, grooms and references are 13 MB; the imported captain 16 MB; the flag picture, texture and material about 1 MB. All binaries are LFS.
- **Launcher:** promoted as Captain (receipt 36fe2d7 plus this work; prior package kept, older pruned).

Next part of the work could be done by either — preference: Claude (milestone 4: the chest, what the captain does at the trigger, the plank and the fall; tell me what he should do then).

## November 2 (later) — the pirate ship's crew (Claude)

- **Asked:** "Do the next part" (milestone 3 of [../regions/SHIP.md](../regions/SHIP.md)); then "These are the mustachioed pirates ... make all the pirates have mustaches and not full beard". Asked which pirates should be women: "None for now". The mustache flag is for later.
- **Source:** `main` at 8874f5b8.
- **Built** ([../SHIP.md](../SHIP.md), milestone 3):
  - `ShipCrew.cpp/.h` (new): `AShipCrew` places the eight, gives them their 2D lines, hands them the beat (`ShipScore`, started by `DockGameMode`), runs the fencing phrases and the fencers' hits, and tracks `WorstHullOut`. Spawned by `ShipSetting.cpp`.
  - `DockNPCPirate.inl` (new, included by `DockNPC.cpp`): the performances, the chef's state machine (Work/Notice/Chase/Swing/Lost/Return), the props and Jeffries' ropes.
  - `DockNPC.cpp/.h`: eight `EDockHuman` pirate kinds (loaded only by the ship), `DrunkSway` and `Jog` clips (`Clips[9]`), `FFenceCue`, the pirate API, turning only while talking for performers, pirate shouts as ambient subtitles.
  - Kit: `humans.json` (eight specs, `skin_subsurface`, moustache-only grooms), `build_npc_humans.py` (`tricorn` hat, `sash`, `moustache_size/droop/out`), `build_npc_mocap.py` and `Fetch-CMUMocap.ps1` (137_15, 16_35; `-- Clip` builds only those), `import_npc_humans.py` (`clip:Name`), `build_pirate_props.py` (new: cutlass, cleaver, concertina ends and bellows).
  - `ShipTest.cpp`: crew and face stills, `CHUCK_SHIP_CREW`/`CHUCK_SHIP_PIRATE` logs, frame time in `CHUCK_SHIP_VIEW`, trials `crew_talk`, `chef_chase`, `fencers` (15 in all). `Verify-Package.ps1` expects 15.
- **Tested:**
  - Uncooked: 15/15 trials; all ten first/repeat line checks; mustache face stills (two grooms each, three on the dancer) in `Local/PirateReview/faces_moustache.png`.
  - Packaged `Builds/ShipCrewCandidate`: Verify-Package 182 passes (`Local/verify-package-20261102-111230.log`); ship `Local/verify-ship-20261102-115340.log`: `CHUCK_SHIP_TEST failures=0 trials=15 passed=15`, music check 0 failures; `CHUCK_SHIP_CHEF notices=1 swings=2 hits=2 state=Lost` (he gave up, the rat escaped); `CHUCK_SHIP_FENCERS near_hits=1 far_hits=0 fence_out=0.0 hull_out=0.0 parry_gap=1.5`; concertina grip 2.1 cm, chef grip 3.2 cm, no foot sink. Verify-Menu 42/42 (`Local/shipcrew-verify-menu.log`).
  - The first candidate failed Verify-Menu at Ship Galley: the chef saw the rat at the checkpoint and hit him before the check (`Local/verify-menu-20261102-105625-ShipGalley.log`). Fixed: a rat that has just appeared goes unseen until it moves 50 cm. Rebuilt and re-verified.
  - Deck views held the 60 fps cap (16.7 ms) at 1280×720; the uncapped cost was not measured.
  - Not played by hand; no Xbox pad.
- **Remaining flaws:**
  - No voices (subtitles only), waiting on the user's takes.
  - The chef's chop grip drifts up to about 6.6 cm and the cheering toast's about 5 cm at their extremes.
  - The Blender review renders don't show strand hair; only in-engine stills do.
  - The default skin subsurface (1.2, 0.45, 0.3) still reddens other NPCs in strong sun; only the crew have the milder value.
  - Blades have no collision with each other (the parry is posed, 2–3 cm apart).
- **Contract compatibility:** no rig, slot or socket changes on Chuck. New `EDockHuman` values before `Count`; `Clips[9]`; new public `ADockNPC` and `AShipCrew` API; the kit's new `tricorn`, `sash`, moustache and `skin_subsurface` options are optional (old specs build unchanged).
- **Binary/LFS:** eight pirate folders (meshes, textures, grooms), two clips, four props and their materials, two BVH takes; about 150 MB, all LFS.
- **Launcher:** promoted as ShipCrew (see the promotion receipt).

Next part of the work could be done by either — preference: Claude (milestone 4, the chest and the ending; the mustache flag when you want it; play the crew by hand first).

## November 2 — the pirate ship's weather deck, rig and sea, and its score (Claude)

- **Asked:** "Do the next part of the ship" (milestone 2 of [../regions/SHIP.md](../regions/SHIP.md)); then the user supplied `ship.wav`, "here is the soundtrack to add".
- **Source:** `main` at f0faf7b9.
- **Built** ([../SHIP.md](../SHIP.md), milestone 2 and "The ship's score"):
  - `ShipDeck.inl` (new): the open weather deck (post-and-rail bulwarks, quarterdeck rail and break ladders, companion hatch and ladder, helm and binnacle, skylight, capstan, bitts, windlass, catheads and anchors, fife and pin rails with coils, the bell, stores); the rig (masts, tops, yards, shrouds with ratlines, futtocks, backstays, stays, lifts, braces, halyards, the spanker); eight sails rebuilt each frame; the black flag; the rope ladder to the main top; 12 main-yard swing grips.
  - `ShipSea.inl` (new): the sea's ring mesh round the waterline out to 400 km, with a hidden curtain inside the planking.
  - `Tools/create_ship_sea.py` (new): `M_ShipSea` (WPO waves, tilt, flare-following edge ring, foam) and `M_ShipSail` (two-sided foliage canvas) in `/Game/Art/Ship`.
  - `ShipSetting.cpp/.h`: planking stops at the rail line; the deck floors gain the companion and skylight holes; the sky atmosphere, volumetric clouds and the camera-dependent sky light; `Rock()` tilts the sea, sun and sky planet; `EShipSpawn::Deck`; respawn from the last space after going over the side; the check extended (6 spawns, 20 rail places, the main top, 5 ladders, 12 grips, 8 sails, flag, sea material).
  - `ShipTest.cpp`: 18 more stills and six trials (deck_up, deck_down, quarterdeck, rigging, yard_drop, rail), 12 in all.
  - Ship score: `SourceAssets/Audio/Ship.wav` (the user's file, byte copy) → `/Game/Art/Audio/SW_Ship` (looping, 207.7 s) by `Tools/import_ship_music.py`. `DockGameMode.cpp` fades it in aboard (3 s, 0.45) and logs `CHUCK_SHIP_MUSIC_CHECK`; the music-duck smoke test now expects 10 tracks.
  - `DockMenu.cpp` and `Verify-Menu.ps1`: the Ship Deck checkpoint. `Verify-Package.ps1`: ship trials=12 and the music check.
- **Tested:**
  - Uncooked: `CHUCK_SHIP_CHECK failures=0` (168,826 triangles), 12/12 trials, escape pass.
  - Packaged `Builds/ShipDeckCandidate`: Verify-Package 182 passes (`Local/verify-package-20261101-224010.log`); ship `Local/verify-ship-20261101-232114.log`: 12/12 trials, `CHUCK_SHIP_MUSIC_CHECK failures=0 looping=1 playing_after_boundary=1`. Verify-Menu 42/42 (`Local/shipdeck-verify-menu.log`).
  - The first package with the music failed the smoke test's music-duck check (it expected 9 tracks); fixed and rebuilt.
  - Stills: `SourceAssets/Setting/Review/Ship_*.png` (deck, rig, sails, sea, waterline, yard catch).
  - Not played by hand; no Xbox pad; no frame-time measurement; the music not heard on speakers.
- **Remaining flaws:**
  - A faint darker band just below the horizon.
  - Furniture and fittings are simple shapes; the rigging has no collision.
  - His paws pass through the furled sail's roll on the main yard.
  - No camera roll (by design); only the sea, sun and sky move.
  - The sails are animated shapes, not cloth simulation.
- **Contract compatibility:** no rig, slot, clip or socket changes. New `EShipSpawn::Deck` (ShipSpawns 6); new public `AShipSetting` getters (`GetGrips`, `GetSails`, `GetSeaTilt`); new assets `M_ShipSea`, `M_ShipSail`, `SW_Ship`; `MusicTracks` now 10.
- **Binary/LFS:** `Ship.wav` (39.9 MB), `SW_Ship.uasset` (25.4 MB), the two materials and 23 review PNGs, all LFS.
- **Launcher:** promoted as ShipDeck (see the promotion receipt).

Next part of the work could be done by either — preference: Claude (milestone 3, the crew and Jeffries; play the deck by hand first).

## November 1 — the swing carries its momentum into the leap (Claude)

- **Asked:** the swing felt like it slowed or paused, then he jumped "unrealistically forward". He should "really feel the momentum": a good back-and-forth swing should carry him forward realistically on the forward swing and jump.
- **Source:** `main` at efc59b91.
- **Built** (docs/BRACHIATION.md, "Big gaps need a real swing", rewritten):
  - **Swing:** damping halved (`SwingDamping` .45, was .9), so a pumped swing keeps its energy instead of dying off between strokes. Pump gain 8 within 30° of the bottom, as before.
  - **Arc limits:** 1.15 rad in the open; under a deck, 44° in front (`SwingDeckLimit`) and 38° behind (`SwingDeckLimitBack`), so the bigger swing does not put his head through the planks.
  - **Every leap is a projectile now:** the flight starts at the swing's own velocity at release (taken numerically from the pendulum) and blends into the launch velocity over 0.14 s (`SwingLeapRamp`), with gravity throughout. That removes the pause-then-shoot: he leaves the grip at the speed he was swinging. `SwingLeapAt(T)` gives the path. The old lift curve (`SwingLeapLift`) is gone.
  - **Spin:** his body carries the swing's rotation into the flight (clamped at 30° or the release angle under a deck, 60° in the open), then settles to the catch pose.
  - **Catch:** he arrives still swinging (−8°, moving forward with a 20° carry), and the flight's velocity runs on into the new swing (τ 0.04 s), so momentum survives the catch too.
  - **Release:** a press at the top of the forward swing lets go at once; otherwise he still waits for the stroke's best point.
  - **Reach:** `SwingReachK` 2.8, throw 120 along, 100 up, for leaps over 165 cm. Short of the gap, the launch is scaled to 50–92% and he falls.
  - **Under a deck:** apex capped 12 cm, catch centre 6 cm low, launch angle 14–27° (20–48° in the open).
  - **Bridge test** (`ChultBridge.cpp`): the rhythm trial now needs an amplitude over 42.5°. New trial 10, `big_gap_held_forward_jumping_on_falls_short`: holding forward and jumping on as he catches the plank before a big gap falls short. New `CHUCK_BRIDGE_HEAD_ABOVE` log. Verify-Package expects `trials=10`.
- **Tested:**
  - Uncooked suite: bridge 10/10 (`Local/brach-s3c.log`), canopy 48/48, giant canopy 72/72, raptor 8/8 (route 8/8), birds crossed=1, lamp swing 8/8.
  - Reach at a 175–185 cm gap on the bridge: still, stick let go, 49.6 cm; held forward 50–58 cm; held forward and jumping on, 153.6 cm (short); rhythm 203–249 cm.
  - A rhythm leap: about 108 cm/s on the swing at release, rising to 420–440 cm/s over the ramp, 0.45 s of flight.
  - Head under the bridge: `CHUCK_BRIDGE_HEAD ok=1 frames=3693 through_1cm=72 max_through_cm=1.9 max_above_cm=9.2`.
  - Packaged `Builds/Swing3Candidate`: Verify-Package 182 passes (`Local/verify-package-20261101-170250.log`), bridge 10/10 (`Local/verify-bridge-20261101-181315.log`). Promoted as `Swing3`.
  - Not played by hand; no Xbox pad.
- **Remaining flaws:**
  - The launch is still about 4× his swing speed. A 31.5 cm pendulum cannot throw a 65 cm rat 180 cm on its own, so there is an assisted throw on top. The ramp hides most of it, but the pickup is still visible.
  - Easy-gap launches reach about 530 cm/s.
  - Frames with his head more than 1 cm through a plank rose to 72 (from 3), all at 1.9 cm or less.
  - The route tests (raptor, giant canopy) use the pump assist rather than a recorded human rhythm.
  - A late press at the very top of the swing falls short.
- **Contract compatibility:** no rig, slot, clip or socket changes. New constants `SwingDamping`, `SwingDeckLimit`, `SwingDeckLimitBack`, `SwingCatchCarry`, `SwingCatchDeg`, `SwingLeapRamp`; changed `SwingReachK`, `SwingThrowAlong`, `SwingThrowUp`. New members `SwingLeapC0/Vs/V0`, `SwingFromVel`, `SwingLeapSpin`; new `SwingLeapAt`, `SwingCentreFacing`; `SwingLeapLift` removed. Bridge test trials=10.
- **Binary/LFS:** none. The re-saved Common materials were restored, not committed.
- **Launcher:** promoted (`Windows-Previous-20261101-Swing3` and `-Swing2` kept, the Ship backup pruned). `Builds/Swing3Candidate` was moved into place by the promotion.

Next part of the work could be done by either — preference: Claude for any more swing feel (play it by hand first); the ship's milestone 2 is either, preference Claude.

## November 1 — the pirate ship: plan, escape and below decks (Claude)

- **Asked:** "start building the ship/next part of the game. base your approach on how we've handled the chult adaptation from 2d".
- **Plan first, as with Chult:**
  - Read the 2D Phase 6 escape, Phase 7, the six ship maps, the dialogue JSON and `pirate ship.png`, all read-only.
  - Wrote [../regions/SHIP.md](../regions/SHIP.md): one continuous ship, the crew's exact lines, the seven-condition ending gate and four milestones.
  - The user approved it with milestone 1 to start, and answered its questions:
    - the chest gives 40 cigarettes plus the golden leaf's stamina lock;
    - the sea rocks round a still hull;
    - the user will supply the ship track and pirate voices later.
- **Built (milestone 1; [../SHIP.md](../SHIP.md)):**
  - `ShipSetting.cpp`: the hull, decks, rooms, furniture, ladder, lanterns, sea, light and sound, plus spawns, respawns and the sea death. Its stage is 8 km from Chult.
  - `ShipEscape.cpp`: the escape cutscene from the rubble crevice's end.
  - `ShipTest.cpp`: `-ChuckChultTest -ChuckShipTest`.
  - New `AChuckCharacter::ResetCrawlingAt`.
  - Respawn, sea-death and music hooks in `ChuckCharacter.cpp` and `DockGameMode.cpp`; the letterbox while the escape plays.
  - A sixth Dev Checkpoint column (`DockMenu.cpp`); the crevice watcher spawned with Chult (`ChultLanding.cpp`).
  - `Tools/gen_ship_sfx.py` and `import_ship_sfx.py`.
  - `M_TempleRiftSea` gains a `Gain` parameter (default 1).
  - Verify-Package runs the ship test; Verify-Menu covers the new checkpoints and Temple Rubble.
- **Tested:**
  - Uncooked (`Local/ship-test-final.log`):
    - The escape plays from the real crawl to the crevice's end and hands control back facing the hole (pass, 24 s).
    - `CHUCK_SHIP_CHECK failures=0`: 5 spawns on their floors with room, 3 doorways open to a 180 cm human, the hull solid below the waterline, no missing materials.
    - 6/6 trials: galley, cabin, ladder down, ladder up, over the side into the sea and back, the crawlway in and out.
    - 60,698 triangles.
  - Packaged `Builds/ShipCandidate`:
    - The first package's verification stopped at the temple heroes' `wizard_seal` (web coverage 0.29). It is a known intermittent: it failed the same way earlier the same day before any material change, and on October 28.
    - The rerun found a real fault of mine: the rubble map's crawl trial reaches the crevice's end, and the new escape carried him off to the ship mid-test. The watcher now stands aside under `-ChuckTempleRubbleTest`.
    - Verify-Menu then failed Ship Hold: he drifted 50 cm off the spawn in 7 s, likely a roaming rat. No rat home is within 4.5 m of the ladder's foot now.
    - The final package passed Verify-Package with 182 passes (`Local/verify-package-20261101-035847.log`, ship `verify-ship-*`) and Verify-Menu 41/41 (`Local/ship-verify-menu2.log`). It was promoted as `Ship`.
  - Stills: `SourceAssets/Setting/Review/Ship_*.png`.
  - Not played by hand; no Xbox pad; no frame-time measurement.
- **Found on the way:** `AShipSetting::Enter` at first left `AInfo` actors alone, and Waterdeep's harbour fog (an `AInfo`) turned every opening white. Their fog, light and sky parts are now switched off too.
- **Remaining flaws:**
  - The weather deck is closed (gratings only). The sea is a view-ray backdrop that only works seen from inside.
  - No rocking yet beyond the swinging lamps and hammocks.
  - Furniture is simple boxes and spars; the folded sails read as stacked boards.
  - The masonry's faces are flat boxes.
  - The gunport lids are plain slabs.
  - The sea is pale through the ports.
  - The rats can't reach the raised crates; that is intended, but the route is untested by a person.
- **Contract compatibility:** no rig, slot, clip or socket changes. New public `ResetCrawlingAt`; new `Gain` parameter on `M_TempleRiftSea` (the sanctum is unchanged at 1).
- **Binary/LFS:** 5 WAV + 5 uasset (4.3 MB), 7 review PNGs (9.5 MB) and the rebuilt material, all under the existing LFS rules.
- **Launcher:** promoted (`Windows-Previous-20261101-Ship` kept, the Cream backup pruned).

Next part of the work could be done by either — preference: Claude (milestone 2, the weather deck, rigging traversal and sea; milestone 3's pirates and their animation are Claude's either way).

## November 1 (later) — big brachiation leaps need a back-then-forward swing (Claude)

- **Asked:** holding the stick forward got him across the bridge "no problem". A long leap should need a swing back then forward, with physics that make sense, "like a real person trying to make a big brachiation jump". The tree-to-tree gaps should need it too.
- **Source:** `main` at f6496980 (the brachiation code is from 5de52e20).
- **Built** (docs/BRACHIATION.md, "Big gaps need a real swing; under a deck"):
  - **Pump:** the stick only adds energy near the bottom of the arc (within 30°, strongest at the bottom) and in the direction he is already moving. Pushing against the motion takes energy away, so holding the stick one way nets about nothing. A dead hang gets a small start kick.
  - **Reach model:** for leaps over 165 cm, grip to grip, the leap's speed is a throw (140 along, 180 up) plus the swing's own speed at release (`SwingReachK` 2.4 × L × rate) along the arc's tangent. The reach is ballistic flight to the target grip's height. Short of the gap, he leaps 45–90% of the way and falls. `SwingNeedAt` is gone.
  - **Timing:** pressing jump on a big gap waits for the forward stroke and releases him just past the bottom, at the stroke's best reach. If he isn't swinging, he lets go after 0.45 s (or 1 s at most).
  - A leap's catch now carries 14° of swing (was 27°), so every big gap needs its own pumping.
  - **Under the bridge:** the duck level eases off past 30° of mesh pitch (factor −1.35).
- **Measured for a 180 cm bridge gap:**
  - from a still hang, about 84 cm (short);
  - with the stick held forward, about 83 cm (short);
  - with a back-forward rhythm from a catch, 230–255 cm;
  - in the test trial, 259.7 cm (amplitude 43.8°).
- **Tested:**
  - Uncooked (`Local/brach-s2-*.log`), all passing:
    - bridge 9/9, with the two new trials `big_gap_stick_held_forward_falls_short` and `big_gap_swing_back_then_forward`;
    - head through a plank on 3 of 4288 frames by more than 1 cm, worst 2.1 cm;
    - canopy 48/48 and giant canopy 72/72;
    - raptor 8/8 with its route 8/8;
    - birds crossed=1;
    - lamp 8/8.
  - Packaged `Builds/SwingCandidate`:
    - The first full Verify-Package failed only the temple heroes' `wizard_seal` (coverage 0.25 against 0.3). This is the known intermittent seen on 10-28, 10-31 and earlier today.
    - The rerun on the same package passed with 182 passes (`Local/verify-package-20261101-142151.log`). Bridge `trials=9`, head ok=1 (`Local/verify-bridge-20261101-153129.log`).
    - Promoted as `Swing2`. The receipt names f6496980 because the build was made from uncommitted changes, which are committed in this entry's commit.
  - Not played by hand; no Xbox pad test.
- **Contract compatibility:**
  - No rig, slot, clip or socket changes.
  - New public `GetSwingRate`, `GetSwingShorts` and `SwingReachNow`. `SetTestSwingPump` now pumps with the swing's motion, not toward the target.
  - Verify-Package expects bridge `trials=9`.
- **Remaining flaws:**
  - Route tests (canopy, giant, raptor, birds) cross big gaps with the test pump assist, not a scripted player stick.
  - Pressing jump while the stick points back aims the leap back. Players must be holding forward on the release stroke.
  - From a dead hang a big gap takes about two full swings.
  - The throw and whip are tuned constants, not a body simulation.
  - The crown fur still grazes the planks by up to about 1 cm on a few frames.
- **Launcher:** promoted (`Windows-Previous-20261101-Swing2` and `-Ship` kept, the 10-31 Swing backup pruned).

Next part of the work could be done by either — preference: Claude (ship milestone 2 still waits; a hand-play of the bridge on the pad would confirm the swing feels right).
