# Waterdeep movement and camera milestone

Latest sewer layout: small Astral ruptures occupy the side banks, leaving the shallow stream continuous; only large ruptures cut through it. The zombie passage is narrower with steeper arched sides for the existing lateral wall run. Running parallel beside a wall + jump triggers that arc; running into a wall retains the original climb. See SEWER-PROTOTYPE.md and HANDOFF for actual checks; controls/controller code unchanged.

Latest tavern addition: a provision cellar beneath the tavern, with an open hatch behind the bar and a visual ladder. Climbing is deferred to Claude. After the sewer return opens the tavern, drop through the hatch to inspect the pantry; R / controller View returns to docks. See TAVERN-PANTRY.md for the ladder contract and HANDOFF for verified launcher status.

Latest soundtrack/lighting pass: Waterdeep night.mp3 plays after the sewer-slide return; morning and sewer scores remain. Twenty small Astral fall ruptures add purple light alongside eleven large ones; grey fill is about 30% dimmer. Small holes respawn Chuck at the sewer entrance. Use root Launch-Prototype.cmd and the normal sewer exit to hear the night score. See HANDOFF Update62 for verified checks and limitations.

Latest Astral pass: irregular tapered fissures replace the rectangular holes, with matching floor collision and recessed stone edges. Upright oil veils follow the edges and fade upward. The openings remain genuine falls with local sewer respawn. See SEWER-PROTOTYPE.md / HANDOFF for actual checks.

Latest sewer work: stronger neutral-grey fill shows enemies; a1.8m pinch point holds the zombie, with the wider chamber retained. Collapsed rock surrounds the water chute's irregular opening. See SEWER-PROTOTYPE.md / HANDOFF for actual traversal and launcher checks.

Latest fire pass: torches, lamps, forge and tavern hearth use animated flame silhouettes with gentle local light flicker and exposed lamp frames. Hearth logs and embers add detail. See FIRE-PASS.md and HANDOFF for verified package evidence; controls remain the same.

Latest return state: the tavern now starts closed. The sewer-end slide returns Chuck to the pier in early evening, closes the sewer hatch and opens the tavern. R / View preserves this session state; a fresh launch starts morning. See DOCKS-RETURN.md and HANDOFF for actual checks.

Latest tavern work: the enlarged tavern is a walk-in room through the open front door. Worn timber floor/beams, a bar, tables/benches, shelves and a hearth establish the interior at human scale. See TAVERN-INTERIOR.md and HANDOFF for actual verification; earlier closed-frontage/no-interior descriptions are historical.

Latest cave refinement: falls and zero-sanity deaths inside the sewer respawn at its entrance. Blue night fill keeps the natural rock tunnel readable; purple astral patches show a nebula at floor level. The central channel is shallow flowing water and the midpoint opens into a wider chamber. R / controller View explicitly returns to docks. This supersedes the earlier atmosphere/respawn paragraphs below; see HANDOFF for actual verification and launcher status.

October 2 atmosphere: eleven Astral Sea holes break the sewer floor; use the side banks or jump, since their visual bed has no collision. Falling into one returns Chuck to dock spawn. Purple rupture lighting replaces sewer lamps. Supplied Sewer.wav crossfades in underground and loops; the docks soundtrack returns on reset. Existing controls apply.

October 2 sewer update: the side-gate hatch leads by physical fall into the connected winding tunnel prototype. Existing movement/camera controls apply underground; R / controller View returns to dock spawn. No entry interaction, loading question or sewer checkpoint. See SEWER-PROTOTYPE.md and latest HANDOFF for the verified package.

September 30 setting scope: additive Waterdeep fountain plaza, walls, closed sewer gate and dawn lamps/torches, loosely based on the original 2D text maps. Preserve existing geometry and Claude's current gameplay/NPC systems. See WATERDEEP-PLAZA.md and latest HANDOFF for routes and actual validation.

September 29 scope update: the user authorized a larger connected docks setting around Claude's existing parkour routes. See DOCKS-SETTING.md. Current runtime uses Claude's v1 character, authored clips and traversal, and continuous camera orbit; historical procedural-character and world-pause notes below no longer describe the latest build. HANDOFF records what has actually been packaged and checked.

Current scope follows the user's request to test fundamentals before adding interactions. One Windows PC scene: detailed primitive docks, tavern frontage, stationary human scale reference, barrel, crate, low step and bench. The presentation pass adds paving, plank nails/grain, timber and door details, roof tiles and distant nonplayable harbor silhouettes. Chuck uses an imported Blender form study with restrained procedural movement. No combat, dialogue, pickups, additional maps or finished art.

Chuck is a 65 cm gray rat with an oversized purple open jacket. His capsule is 65 cm tall and 30 cm wide; the human is 180 cm tall and tavern door 210 cm tall. World-locked stance, alternating short swings, slight body lean, airborne foot tuck and landing compression affect visual components only. A Blender skeletal rig now articulates the legs, sleeves and four-bone tail. Procedural two-bone leg IK follows the foot targets; ground traces place feet on the current static route. Authored clips, toe joints and general slope/stair/platform support remain future work. Use real scale rather than shrinking a human template visually.

Unreal 5.7.4 with a small C++ runtime module replaces the initial Blueprint-only suggestion. Dock geometry is generated at play time; Python scripts create the level/materials and import the Blender 4.5.14 character study. See SourceAssets/Chuck/README.md for the asset pipeline. No new dependencies are required for procedural animation.

## Camera comparison

- Elevated: perspective, 400 cm collision-tested boom, 48-degree downward angle, 65-degree FOV.
- Rat-height follow: 220 cm collision-tested boom from a chest-high pivot, tilted 5 degrees down, lens about 76 cm above the floor (just over Chuck's ears, so he sits low in frame rather than covering the view ahead), 78-degree FOV; limited look up/down rotates the camera without lowering the boom beneath the floor. Until 2026-09-27 the boom was horizontal with the lens at ear height (about 65 cm).
- Both views keep their height through a jump (pivot moves about 5.5 cm for an 18 cm hop) and follow landings on a new level or falls.
- Since 2026-09-27 the two views are the ends of one continuous GTA-style orbit, with no switch button. Mouse/right-stick look down climbs toward elevated; look up sweeps down to rat height, then tilts the lens up to +30 degrees. Boom length, pivot height and FOV blend with the orbit pitch. After 1.2 s without look input the orbit eases behind Chuck while he walks away from it. Before this, C / Xbox Y blended between the two fixed views. Both modes use the same movement, jumping and scene. Follow smoothing is capped at 8 cm; orbit rotation is damped. No motion blur. F / right-stick click recenters behind Chuck. Mouse sensitivity is reduced.

Follow the same route in [PLAYTEST.md](PLAYTEST.md) in both modes. Compare visibility near the barrel/crate, human scale, view of the tavern, landing readability at the pier gap, and comfort. Reset preserves the selected camera. Repeat with keyboard and Xbox controller. No final camera choice until the user plays and gives feedback.

## Acceptance

- Walk/turn/stop at rat scale using keyboard and Xbox mappings.
- Jump and land, collide with the world, and recover locally after falling.
- Read Chuck's gray body and purple jacket against human-scale placeholders.
- Both framings are reachable in one continuous orbit in the same Windows build (switchable until 2026-09-27).
- Controls and exact launch instructions are documented.
- Runtime checks and visual inspection are recorded honestly, with physical-controller verification distinguished from simulated input tests.
- Keep source and LFS assets in the separate public CHUCK-3D repository; public publication was authorized on 2026-09-24.

Camera decision: OPEN. This milestone exists for the user's comparison, not a commitment to either view.

When an obstacle forces the camera within 70 cm of Chuck, his proxy is hidden temporarily to keep the lens clear. It reappears when the camera has room. Tight-space framing still needs user feedback.

The material/daylight pass adds original procedural surfaces, atmospheric sky and richer nonplayable harbor roofs. See GRAPHICS-PASS.md. These improve the study without claiming finished artwork or changing collision and camera controls.

Character-first revision (2026-09-26): the user's feedback rejects the current primitive appearance, cartoony leg placement and jacket-edge gaps. Preserve this scene for comparison while character work proceeds; stop further world detail. Plan future run/roll/side-jump/climb/mouth-held smoking support in the rig, without treating those actions as implemented. Verification expected 43 checks with captures at that time; the v1 runtime now expects 49 (see docs/agent-handoffs/CLAUDE.md). See CHARACTER-PLAN.md, AGENT-WORKFLOW.md and RIG-CONTRACT-V1.md for the accepted next rig interface (not yet in the game).
