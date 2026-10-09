# Post-chamber wall-run rupture

October 9: a separate sprint-only rupture now sits farther along the tunnel at samples 306–310. This earlier wall-run crossing is retained. See SEWER-SPRINT-RIFT.md; the current total is 33 openings.

The October 4 user request replaces the earlier wallrun-plan zombie obstacle with a short narrow passage and an Astral floor rupture **after** the wide midpoint chamber. Historical wall-run planning remains a reference, not the current enemy layout.

The existing route is retained. The former pinch at samples 96–106 returns to ordinary cave width; a nominal 1.4 m wide, 6.5 m long core at samples 219–229 has smooth narrowing shoulders and the existing steep rounded rock arch. A roughly 1.95 m long jagged floor break at 222–225 spans the passage, interrupts the stream, and uses existing recessed nebula/oil veil/purple light materials. Both cave walls provide the wall-run surface. Solid takeoff/landing banks remain before and after, with no walking ledge beside the break. The zombie is no longer spawned. Rats elsewhere remain.

The prior large rupture at 215 moves to 199, and small bank ruptures 228/235 move to 237/242, clearing the approach and landing. Nothing is removed from the total existing rupture count; one new large opening brings the total to 32 (12 large, 20 small). No new material, texture, engine dependency, controller tuning or character animation.

## Controls and testing

Approach along either wall. Tap run (Shift / Xbox LB), keep moving parallel to the wall, and press jump (Space / Xbox A) shortly before the broken floor. Keep moving forward to reach the landing. A jump directed into a wall still uses the existing upward climb. A walking jump stays an ordinary jump. Falling returns Chuck to the sewer entrance.

Use the root Launch-Prototype.cmd for the verified published build. Enter through the side-gate grate, follow the winding sewer past its wide chamber and look for the narrowed purple-lit break.

Automation: -ChuckWallRiftTest checks zombie removal, post-chamber location, ordinary running jump failure and local fall recovery. -ChuckWallSideTest checks actual crossings using each wall and unchanged walking-jump behavior. The full verifier retains these tests plus collision/route, sewer, smith, pantry and controller regressions. Geometry scans now distinguish the intentional full-width missing floor from ordinary walkable banks and separately check six hole probes. -ChuckSewerTest follows the full route including a physical wall run rather than teleporting over this obstacle.

Actual build/test results, launch receipt and remaining limitations are recorded in the latest HANDOFF. No physical controller or user comfort test is implied by automation.

## October 4 revision (Claude): easier wall run, checkpoint, zombies, NPC-only Astral floor

User request: the crossing was too hard; wall running in general should not demand a near-parallel approach; add a spawn point shortly before the break; restore zombies (one in the tunnel before the wide chamber, three in the chamber, one by the end chute); Astral openings affect only Chuck, never NPCs.

- **Angled side wall run (all walls, not only the sewer).** A running jump beside a wall, or angled onto it by up to 50° (`WallSideAngle`), is a side wall run; previously about 24° was the limit. Steeper than 50°, or a wall squarely ahead, is still the head-on climb. Side probes now also look diagonally forward, distance is measured straight out from the wall, and the remaining gap is closed over the first frames instead of snapped. Chuck turns along the wall smoothly.
- **Air catch.** For 0.45 s after a running takeoff, reaching such a wall in the air catches it and starts the side run (`GetWallSideAirCatches`). A jump started too early no longer simply fails.
- **Longer, flatter arc.** On-wall time 0.95→1.1 s, arc gravity 520→470 cm/s²: about 260 cm of travel and slightly more rise. The arc still ends when floor comes under him.
- **Checkpoint.** `DockSewerCheckpointSample()` = break start −9 samples (about 5.9 m of run-up on the right bank). A fall or sanity loss at or beyond it returns Chuck there, facing along the route; earlier deaths still return to the entrance. Surface reset is unchanged.
- **Zombies.** Five are placed: sample 100 (tunnel before the chamber), three spread over the chamber on both sides, one by the end chute. They are kinematic and stay leashed near home, so none can reach the break. They are still omitted in the scripted walk-through tests.
- **NPC-only Astral floor.** An invisible collision surface spans every opening, made from exactly the floor cells removed for the openings. It is WorldStatic, blocks only the Pawn channel and ignores Visibility/Camera. Rats (character movement) and the zombies (WorldStatic step queries) walk across. Chuck's capsule ignores it (`IgnoreComponentWhenMoving`), and his probes and the geometry checks trace Visibility, so he still falls. Before this change, the zombie never fell through; its step test refused any floor-less step, and the earlier zombie was intentionally removed in `56cbc96`.

Tests: the stage 113 check now requires the five-zombie layout and the NPC floor (pawn and WorldStatic traces hit it; Visibility is open; Chuck ignores it). The plain middle jump must fail and respawn at the checkpoint. The new wall-side sub-test 3 starts on the far side of the tunnel, veers 40° onto the right wall and must clear the break as a side run without a climb.
