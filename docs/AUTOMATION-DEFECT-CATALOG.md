# Defect catalog — what the user has caught by hand, and what could catch it automatically

Written 2026-11-13 (Claude). First step of the automation idea: turn the corrections the user has made so far into checks that run on every region, so the same kind of problem is not found by hand twice. Sources: the git history (to 69838d6f), the "Asked:" lines in `agent-handoffs/CLAUDE.md`, `CODEX.md` and the archived prototype handoffs, and the existing test flags in `Source/Chuck3D`.

Categories 1 and 4 are built as the world audit (below, 2026-11-13). The other proposed checks are not built; they are a design, not a claim that they work. See also [AGENT-WORKFLOW.md](AGENT-WORKFLOW.md#working-efficiently-2026-11-13) for the shot-list, placement-table and light-audit tooling referenced below.

How to read the "catch" column:
- **Numeric**: measurable from the game's own data (collision, transforms, bones, audio levels); can be a pass/fail line in Verify-Package.
- **Visual**: needs a picture; a vision reviewer can flag the obvious cases, the user still judges the rest.
- **Human**: feel, sound quality, taste, story. Stays with the user.

## The categories

### 1. Walk-through solids (collision holes)
- **Seen:** the side gate's stone jambs by the sewer hatch (11-12); the ship's mast cap and the hole round the masthead, the binnacle hood (11-12); chimney collision (10-02); stored barrels and planks walk-through (10-02); the cheese made solid (10-09); fallen logs open or walk-through (10-27); root collision (10-12).
- **Checks now:** the world audit (below): every region, every material. (Until 2026-11-13: `-ChuckCollisionAudit`, Waterdeep only, Stone/Plaster/Wood/Brick materials only.) `ChultPropAudit` still checks the fallen logs.
- **Proposed check:** one audit for every map: every visible static mesh above a size threshold either has collision or is on an explicit "decorative, pass-through" list (foliage, cloth, rope, FX). Probe each with a rat-sized capsule from several sides. Log `CHUCK_COLLISION_AUDIT map=… gaps=N`; Verify-Package requires 0.
- **Catch:** Numeric.

### 2. Chuck inside geometry / stuck / out of the world
- **Seen:** left inside a sanctum pier (11-12, never reproduced); props blocking the spawn or the dock entrance (10-02, 10-03); the dock vista shortcut (10-02).
- **Checks now:** per-region test trials follow fixed routes; the pier push-out logs `CHUCK_SANCTUM_PIER_PUSH_OUT` but only in the sanctum.
- **Proposed check:** a robot playtester per map: the region's main route plus some minutes of random/exploratory input (jumps, climbs, hangs, leaps at edges). Each frame: is Chuck's capsule overlapping blocking geometry, below the world's floor, outside the region bounds, or not moving for N seconds while input is held? Log positions and a still for each incident.
- **Catch:** Numeric.

### 3. Things passing through things
- **Seen:** the fore course through the fore ratlines (11-08); Jeffries' lashings through his body (11-08); rope bending at impossible angles through hands (11-07); hair through hats (10-29); the axe through the dwarf's shoulders (10-04); hands into thighs (10-01); the T-rex's route through the first giants (11-12, 10-15); the bridge planks through Chuck's head (11-12).
- **Checks now:** ad hoc and per case: the ship check's crossing count (`CHUCK_SHIP_RATLINES_ACROSS`), the rope-to-fist distance, the bridge head check, the T-rex route.
- **Proposed check:** a general "interpenetration" pass: for named pairs (rope vs body, cloth vs rigging, prop vs character, creature route vs large static), sample the meshes and report the deepest penetration. Creature routes get a swept clearance test against large statics. The region plan lists the pairs that matter.
- **Catch:** Numeric for most; hair through hats is Visual.

### 4. Floating, sunk or badly seated objects
- **Seen:** the floating coil below deck (11-08); the mop propping itself up (11-02); rubble that didn't fall from anywhere, floating (11-04); trees hanging over air on slopes (10-27); jars inside stone (10-27); dinosaurs' feet not on the ground (10-16); shrines over a terrace's edge (10-20); the bench over the sewer grate (10-01).
- **Checks now:** the world audit's floating check (below), every region; `ChultPropAudit` (trees on slopes, jars in stone) for Chult; `CHUCK_TEMPLE_RUBBLE_FLOATING` for the rubble map; the ship's coil check. Not yet: things sunk into stone, footprints half over air, creatures' feet.
- **Proposed check:** one support audit for every map (hand-placed props are listed in `Content/Data/Placements/<Region>.txt`, `ChuckPlacements.h`, so the audit can name the line to fix): for each prop not tagged as hanging/flying, trace down from several points of its footprint; report gaps over 2 cm, penetration over 5 cm into stone, and footprints more than a third over air. Characters and creatures: foot bones vs ground during idle and walk.
- **Catch:** Numeric.

### 5. Jitter, twitching and pops
- **Seen:** the tail squiggle (11-12); the landing leaves twitching (11-12); the storm crew's jitter from a phase bug (11-12); the NPC motion-capture shake (10-05); the elf twitch (10-06); still open: whole-body pops at the start of walk-start and slash cross-fades.
- **Checks now:** `-ChuckMotionAudit` on every region (pops and jitter of every skinned mesh near Chuck; in Verify-Package since 2026-11-13); `CHUCK_NPC_JITTER` (Waterdeep NPCs).
- **Proposed check:** a motion audit: record every skeletal mesh's bones and every "resting" instance for a few seconds at fixed frame rate; flag frame-to-frame jumps above a speed limit per bone class (tail tip, hands, root), and any movement of objects that should be still. Run it on idle, walk-start, slash and each cutscene.
- **Catch:** Numeric (this is the main way to catch motion problems; still images miss them).

### 6. Sound heard where it shouldn't be, or at the wrong level
- **Seen:** sewer zombies heard from the streets (11-12); temple interior sounds outside (11-12); rats heard everywhere on the ship instead of only in the hold (11-03); the creak too loud (11-03); the seated pirate too quiet (11-12); storm lines under the music (11-09, 11-10); the smith too quiet, Dougmund not heard from the stairs (10-06).
- **Checks now:** `-ChuckSoundAudit` on every region (sounds heard through solid; in Verify-Package since 2026-11-13); `Tools/Check-VoiceLoudness.py` (voice files' levels); music checks per region; `ChultTempleSoundAudible` for the temple.
- **Proposed check:** an audibility audit: for each sound source, its "zone" (room, level, deck) is declared; listener probes in every other zone must hear it below a threshold. A loudness check: every voice line normalized to a target loudness, and each line's level at its normal listening distance compared with the music under it.
- **Catch:** Numeric for leaks and levels. Whether a sound is good is Human (the agent can't hear).

### 7. Lighting that wrecks materials
- **Seen:** unshadowed lights bleaching the heroes' hair (11-09); purple rupture lights making hair blaze (11-09); the wizard's robe blown white by his own lights (11-04); the fighter's beard grey in cutscene light (10-29); the gnome's grey hair reading red (10-29); the crevice glow too bright (10-27, 11-12); sewer fill too grey or too dark (several, 10-03/04); outdoor sky leaking into the Astral chasms (10-02).
- **Checks now:** stills captured by the `...Capture` and `...Review` flags, looked at by the agent case by case. Since 2026-11-13: `CHUCK_LIGHT_AUDIT` (`LightAudit.h/.cpp`) lists every shadowless point/spot light whose reach covers a groom, run by shot lists `Tools/shots/audit-*.txt` at the night docks, pantry, sanctum and ship.
- **Proposed check:** add the light audit to Verify-Package for every map once merged. For every hero/NPC close-up still, measure the average colour of the hair, skin and clothing regions against the material's base colour; flag big shifts (white-out, saturation blow-up). Sky leaks: render enclosed spaces and look for sky pixels.
- **Catch:** Partly Numeric (light settings, colour shift), the rest Visual.

### 8. Wrong materials, stretched or smeared textures, flicker
- **Seen:** the distant LODs drawing the wrong material (white skirt, grey beard; 11-04); the dino faces smeared by UVs (11-12); the door frames flickering from shared faces (11-05); tunnel floor seams (10-02); oil-film seams (10-09); the neck's mapping seam behind the dino skull (still open).
- **Checks now:** none general; LOD repair is in the hero import.
- **Proposed check:** at import: every LOD's material slots must match LOD0; UV texel density per triangle within a range (flags smeared faces); coplanar overlapping faces in the same mesh or between touching meshes (flags flicker). In game: force each LOD on each character and compare stills.
- **Catch:** Numeric.

### 9. Look and realism of objects and placement
- **Seen:** hammocks hung too high, then re-hung (11-02, 11-12); the "3-boxed-grid" view through the crevice (11-11); the flag in the wrong place (11-02); the lumber pile that read as sails (11-12); the dancer's position (11-02); the concertina player's hands backwards (11-04); the terrain square ending abruptly in the fall's high views (11-10); NPC eyes like mascara (10-30); Chuck like a felt doll (10-31).
- **Checks now:** stills and the user.
- **Proposed check:** a vision reviewer: after each build, stills from fixed cameras (the `-ChuckShots=<file>` shot lists and `Tools/Take-Shots.ps1`, which need no recompile) plus the robot playtester's stills go to a reviewer agent with the reference images and this catalog; it lists what looks wrong. Expect it to find the obvious (backwards hands, a white robe, the edge of the world) and miss the subtle.
- **Catch:** Visual, then Human.

### 10. Triggers and timing
- **Seen:** the beholder entrance before Chuck settled (10-27); the storm starting before Chuck was on deck (11-05); guide lights before the orders (11-10); the captain's pause too short (11-10); the pantry fall's white freeze too long (11-10).
- **Checks now:** per-sequence tests (ship ending, sanctum).
- **Proposed check:** every cutscene and scripted sequence logs its start conditions; a check that Chuck is grounded and inside the expected area at the start, and that lines don't overlap. Long blank or frozen frames: measure.
- **Catch:** Numeric for conditions and freezes; pacing is Human.

### 11. Wrong in the world's own logic
- **Seen:** larboard and starboard mirrored (11-10); the captain ordering the helm while he holds it (11-11); torches only where the sanctum had them, some blown out (11-04); rubble only from visible sources (11-04).
- **Checks now:** none.
- **Proposed check:** a text reviewer agent that reads the region plan, the 2D source and the scripted lines/orders and flags contradictions. Partly a checklist in the region plan.
- **Catch:** Visual/Human; an agent can catch some.

### 12. Feel of the controls and moves
- **Seen:** the brachiation swing four times (10-29 to 11-12); ladders by stick direction (11-12); wall climbing cost (10-12); the gorge fall length (11-12); running, rolling, side jumps (09-27).
- **Checks now:** trials that the move works (it reaches, it lands).
- **Proposed check:** none can judge feel. Agents can keep the numbers stable once the user likes them (swing reach, timing windows, fall times) so they don't drift.
- **Catch:** Human.

## The world audit (built 2026-11-13; categories 1 and 4)

`-ChuckWorldAudit=<checkpoint>` (`Source/Chuck3D/WorldAudit.cpp`) starts at a dev checkpoint (Waterdeep when none; `Chult` builds the jungle and the temple; `ShipArrival` the ship), waits for the region to stand, then reads the scene and quits. About 0.2 s for Waterdeep and the ship, 22 s for Chult (340,000 pieces).

- **Collision gaps:** a visible mesh at least 25 cm each way, its foot within 40 cm of the floor under it, that does not block a pawn and has nothing solid inside it. "Solid inside" is looked for three ways, because collision comes in three kinds here: an overlap in its own box, an overlap in the band a rat walks into (its lowest 40 cm, across the middle of its footprint: a tree's trunk), and four rays 20 cm over the floor walking in from its sides (the ship's hidden boxes are closed triangle meshes, which an overlap wholly inside touches nowhere).
- **Floating props:** visible static meshes between 10 cm and 3 m that touch one another (oriented boxes within 3 cm) are grouped; a group floats if none of it touches a big mesh (3 m or more), a procedural surface (measured as boxes round every 24 triangles) or anything solid. A piece wholly inside a big box (the sky dome, a hull shell) is not held by it.
- **Probes:** each run plants a 30 cm cube in the air and a 60 cm walk-through cube on the floor beside Chuck, and must find both (`probes_found=2/2`), so a blind audit cannot pass.
- **Output:** `CHUCK_WORLD_AUDIT region=… gaps=… floating=…`, three example lines per kind, and in `Saved/Audit/`: `<region>-findings.csv` (every finding) and `<region>-findings.txt`, a shot list for `Tools/Take-Shots.ps1` with one camera per finding (the clearest of eight directions).
- **Intended cases** go in `Content/Data/Audit/Allow.txt` with a reason (Chult's walk-through undergrowth, canopy leaf cards, the distant vista, fire and smoke sprites, the grass tufts, the stake skulls).
- **Verify-Package** runs it on the three regions first (`-WorldAuditOnly` runs just that). A region fails if a probe is missed or a count rises above its known findings; lower the limit there when a finding is fixed.

### What it found, and what was done

| Region | Finding | Done |
| --- | --- | --- |
| Waterdeep | the two plaza towers' dark caps (35 cm, overhanging the stone) walk-through | made solid (`DockPlaza.cpp`) |
| Chult | the temple precinct's palms had no trunk collision at all (64) | a hidden trunk capsule each, as ChultExpansion's (`ChultTemple.cpp`) |
| Chult | the jungle band behind the temple starts 5 m in front of the hidden wall; its trees had no trunk collision | solid |
| Chult | the small trees on the pyramid terraces and the temple's back corners (15) had none | solid |
| Ship | none (the crates and barrels are held by the hidden boxes) | — |
| Waterdeep | the cellar shelves' plaster crocks (18) are walk-through | **open, the user's call:** solid crocks would change how Chuck climbs the pantry shelves |
| Waterdeep | 117 floating pieces, mostly the house fronts' repair masonry (thin stone plates 12 cm off the wall and 4–8 cm off the ground, `DockSetting.cpp` "lower masonry courses"); also stone blocks on a roof 5–10 cm over the stone under them, wooden rails and beams, forge iron, shop bottles | **open:** a triage list (`Saved/Audit/Waterdeep-findings.csv`); some are false alarms where the support has no collision and no box near enough, some are real but small |
| Chult | four vista ferns hang in the void 44 m down | allowed with the vista; not seen in play |

Limits: a support that is a spline mesh or a landscape holds props only through its collision; "reachable" is not known, so unreachable places need an allow line; oriented boxes are looser than the meshes, so a prop near (not on) a support passes.

## The motion and sound audits (built 2026-11-13; categories 5 and 6)

### Motion: `-ChuckMotionAudit=<checkpoint>` (`Source/Chuck3D/MotionAudit.cpp`)

- **Method:**
  - At a fixed 30 fps it records, for 15 s, the bones of every shown skinned mesh within 60 m of Chuck. That includes the NPCs' poseable bodies.
  - Bones are read in each mesh's own space, so walking, teleports and the ship's roll don't count.
  - Chuck idles, walks off, stops and slashes during the recording.
  - Frame-skipping optimisations are switched off for the run, because they would look like pops.
- **Pop:** a frame in which a bone's acceleration rises over four times anything within about a quarter-second and over 2.5 cm/frame², and its travel that frame is over 2.5 times its neighbours'. The second test separates a jump in place from a sharp stop, such as a hammer meeting the anvil.
- **Jitter:** five or more frames in a row in which a bone's acceleration turns round (over 120°) at over 0.3 cm/frame².
- **Grouping:** bones of one mesh that pop or shake together count as one finding.
- **Not blind:** a self-test track with one step and one shake must give exactly those (`selftest=1`), and Chuck's walk must register (`chuck_moved=1`).
- **Output:** `CHUCK_MOTION_AUDIT region=… pop_meshes=… jitter_meshes=…` and `Saved/Audit/<region>-motion.csv` (mesh, bone, frame, phase, size).

### Sound: `-ChuckSoundAudit=<checkpoint>` (`Source/Chuck3D/SoundAudit.cpp`)

- **Sources:** the region's audio components, its creatures and people, deduplicated to 4 m.
- **Listening places:** round each source, places a rat could stand 7 and 15 m off, on any floor within 9 m up or down, with solid between them and the source. At most 80 per region.
- **Measurement:** Chuck is set at each place for 1.2 s, with collision and movement off. The audio engine's own list of active sounds is read on the audio thread, with each sound's gain at the listener (its volume times distance and occlusion falloff).
  - The game's own gating is included, because a gated sound never plays.
  - The master volume is not included, because it is 0 while the test window is in the background.
- **Leak:** a spatialized sound over 0.05 (-26 dB) whose line to the listener goes through solid, with no clear way round (4 or 8 m over, or 4 m to either side). Each source counts once.
- **Not deaf:** a planted sound 3 m under the floor at each place must be heard at half the places or more.
- **Output:** `CHUCK_SOUND_AUDIT region=… leaks=… probe_heard=…/…` and `Saved/Audit/<region>-sound.csv`.

### Voice files: `Tools/Check-VoiceLoudness.py`

- Gated speech level of every line in `SourceAssets/NPCs/Voice`: BS.1770-style gating, without K-weighting.
- Flags a line more than 4 dB off its speaker's median, a speaker more than 4 dB off all speakers, and clipping.
- Standard-library Python only; it takes about 5 s.
- It checks the files only; the game's per-speaker volume comes on top.

### Verify-Package and baselines

- Verify-Package runs all three regions for motion and sound after the world audit, and prints the voice report (not a failure).
- **Limits** are the known findings plus a margin, because one-shot sounds and where a looping clip is when the recording starts vary between runs: motion +1 mesh, sound +3 sources. Lower the limits when findings are fixed.

### What they found (editor runs, 2026-11-13; nothing fixed yet)

| Region | Finding |
| --- | --- |
| Waterdeep | the tavern keeper's right hand jumps 17 cm in one frame every 6 s (the idle's loop seam); the blacksmith's brow bone jumps 12 cm, and his forearm 7.6 cm; a sailor's forearm 9 cm |
| Ship | the dancer's left foot jumps 28 cm every 3.5 s (loop seam); both fencers' thumbs snap 16–23 cm; the concertina player's and the cheering pirate's right calves pop 3–8 cm; the chef's fingers 10 cm |
| Chult | no pops or jitter near the landing |
| Every region (packaged only) | **Chuck:** his right calf jumps 5.2 cm in one frame of the slash (frame 187, 0.23 s into it). It is found in the packaged game but not in the editor run, and it is counted in every region's limit |
| Chult | **sound:** the temple interior's skeletons rising, spike traps and darts are heard 7–11 m above them (gain 0.2–0.67), the "temple interior heard outside" kind; raptor steps heard through a rise in the ground |
| Ship | **sound:** the hold's rats heard on the lower deck through the deck (0.15–0.26); the fencers' clashes and stamps heard below deck |
| Waterdeep | **sound:** a sewer zombie's rasp heard through the rock (0.18); the anvil through the forge wall; a rat's chitter |
| Voice files | the blacksmith's line is 5.5 dB louder than the other voices; the wizard's `fireball_01` is 10 dB quieter than his other lines |

**Limits:**
- Cutscenes are not recorded yet, and neither is wind motion in materials (the landing leaves); neither are still objects that move.
- Chuck's walk-start cross-fade pop, still open in the handoff, was not caught: the walk test may be too short, or the pop too small.
- The sound audit hears only what plays during its 1.2 s at each place.
- The listener is the camera, wherever it settles.
- Voice level in the running game, and against the music, is not measured.

## Scored against the November 12 list

The 23 items, with what the proposed checks would have caught (my judgment, not a run):

| Item | Category | Would catch |
| --- | --- | --- |
| 1 sewer zombies heard in the streets | 6 | Numeric |
| 2 side gate jambs | 1 | Numeric (already did) |
| 3 tail squiggle | 5 | Numeric |
| 4 landing leaves twitch | 5 | Numeric |
| 5 dino voices | 6 | Human |
| 6 gorge fall too short | 12 | Human |
| 7 forward swing | 12 | Human |
| 8 river zombies past the river | 11 | Human (an agent might, from the plan) |
| 9 T-rex size and the giants | 3 | Numeric for the overlap; size is Human |
| 10 dino faces smeared | 8 | Numeric |
| 11 temple sounds outside | 6 | Numeric |
| 12 skeleton grab and throw | — | new feature, not a defect |
| 13 Chuck inside a pier | 2 | Numeric |
| 14 crevice glow | 7 | Visual |
| 15 hammocks | 9 | Visual |
| 16 captain's bed | — | new content |
| 17 galley stove | — | new content |
| 18 lumber pile read as sails | 9 | Visual |
| 19 seated pirate too quiet | 6 | Numeric |
| 20 ladders by stick | 12 | Human |
| 21 storm crew jitter | 5 | Numeric |
| 22 mast cap and masthead hole | 1 | Numeric |
| 23 binnacle hood | 1 | Numeric |

Of the 20 that were defects, about 12 could have been caught by numeric checks, about 3 by a vision reviewer, and about 5 need the user (feel and sound).

## Suggested build order

1. ~~Generalize the collision audit (1) and the support audit (4) to every map; add both to Verify-Package.~~ Done 2026-11-13 (above).
2. ~~The motion audit (5) and the audibility/loudness audit (6).~~ Done 2026-11-13 (above); voice level in the running game is still to do.
3. The robot playtester (2), starting from the existing trial routes.
4. Import checks (8): LOD slots, UV density, coplanar faces.
5. The vision reviewer (7, 9) with the reference images, fed by the shot lists.
6. Then try the whole set on the next region and count what the user still finds.
