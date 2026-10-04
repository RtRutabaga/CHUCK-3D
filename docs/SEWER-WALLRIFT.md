# Post-chamber wall-run rupture

The October 4 user request replaces the earlier wallrun-plan zombie obstacle with a short narrow passage and an Astral floor rupture **after** the wide midpoint chamber. Historical wall-run planning remains a reference, not the current enemy layout.

The existing route is retained. The former pinch at samples 96–106 returns to ordinary cave width; a nominal 1.4 m wide, 6.5 m long core at samples 219–229 has smooth narrowing shoulders and the existing steep rounded rock arch. A roughly 1.95 m long jagged floor break at 222–225 spans the passage, interrupts the stream, and uses existing recessed nebula/oil veil/purple light materials. Both cave walls provide the wall-run surface. Solid takeoff/landing banks remain before and after, with no walking ledge beside the break. The zombie is no longer spawned. Rats elsewhere remain.

The prior large rupture at 215 moves to 199, and small bank ruptures 228/235 move to 237/242, clearing the approach and landing. Nothing is removed from the total existing rupture count; one new large opening brings the total to 32 (12 large, 20 small). No new material, texture, engine dependency, controller tuning or character animation.

## Controls and testing

Approach along either wall. Tap run (Shift / Xbox LB), keep moving parallel to the wall, and press jump (Space / Xbox A) shortly before the broken floor. Keep moving forward to reach the landing. A jump directed into a wall still uses the existing upward climb. A walking jump stays an ordinary jump. Falling returns Chuck to the sewer entrance.

Use the root Launch-Prototype.cmd for the verified published build. Enter through the side-gate grate, follow the winding sewer past its wide chamber and look for the narrowed purple-lit break.

Automation: -ChuckWallRiftTest checks zombie removal, post-chamber location, ordinary running jump failure and local fall recovery. -ChuckWallSideTest checks actual crossings using each wall and unchanged walking-jump behavior. The full verifier retains these tests plus collision/route, sewer, smith, pantry and controller regressions. Geometry scans now distinguish the intentional full-width missing floor from ordinary walkable banks and separately check six hole probes. -ChuckSewerTest follows the full route including a physical wall run rather than teleporting over this obstacle.

Actual build/test results, launch receipt and remaining limitations are recorded in the latest HANDOFF. No physical controller or user comfort test is implied by automation.
