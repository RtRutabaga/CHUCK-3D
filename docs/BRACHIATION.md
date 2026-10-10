# Brachiation and eave grabs (lamp alleys)

User request, 2026-10-09: a brachiation feature on the high wall lanterns between the tall rear-row houses by the sewer — a demo and test area now, a major Chult mechanic later. "Look hard, be easy": a wall jump up to a lantern catches it seamlessly, and swinging lantern to lantern must not be hard. In the same alleys, a chimney climb (wall jumping between the two walls) must catch the edge of the pitched roof and make it easy to pull up and stand on it.

## Controls (PC)

- **Catch:** wall jump up near a lantern. Chuck catches its ring automatically when his raised paws pass within 45 cm sideways of it (from 45 cm below to 30 cm above). No button press is needed.
- **Swing to the next ring:** press Space. He flies to the nearest ring in the stick's direction (camera-relative W, A, S, D). With no direction held, he goes straight on along the swing. He catches it for you.
- **Pump:** hold the stick along the swing.
- **Kick off:** press Space with no ring that way, for example toward a wall. He launches like a wall jump, so a wall in reach is run up automatically, and the chimney climb continues.
- **Let go:** press C (dodge).
- **Roof edge:** in a chimney climb he catches the eave on his own, including when the eave stops his head below it. Press Space (or hold toward the wall) to pull up. A Space pressed on the way up, before he has hold, pulls him up as soon as he hangs. A stick still held away from the wall from the last kick no longer drops him off the edge.

## World (DockSetting.cpp)

Five lanterns per alley, fifteen in total. They alternate walls, 125–135 cm apart along each 120 cm alley (y 2570, 2705, 2830, 2955, 3090), clear of the shuttered windows. Arm tops are at 612–626 cm, below every eave. The plates are flush on the plaster. Each cage hangs a forged lamplighter's ring (7 cm radius, its plane across the alley, the bottom bar worn bright). The ring's bottom bar is the grip, at 543–557 cm, so it can only be reached by wall jumping. Grips are registered with `AddChuckSwingGrip` (ChuckClimbable.h); the named `DockLampGrip` components remain for tools. The lanterns still have no collision.

## Character (ChuckCharacter.cpp)

- New gaits **Swing** and **SwingLeap**, with clips **Swing** (posed by pendulum angle) and **SwingLeapLeft** / **SwingLeapRight** (posed by flight progress), authored in `Tools/build_chuck_v1.py`. Grip in mesh space: (5.5, 63.5) cm.
- One-arm reach (user follow-up, 2026-10-09): in the leap, the paw on the next ring's side lets go first and stretches forward and out to it, open. The other paw drops back and down for balance, with the shoulders and head turned into the reach. The lead paw takes the bar; the trailing one rises to it during a 0.22 s cross-fade into Swing. The runtime picks Left or Right by which side of his new facing the target ring is on. The earlier two-handed `SwingLeap` clip was removed.
- Swing is a pendulum (his own gravity, 31.5 cm from grip to centre) with damping. The whole mesh pitches about the grip, and the clip carries the pike or arch.
- SwingLeap follows a ballistic arc of the paws from ring to ring (0.36–0.6 s). The body goes from the release angle (+30°) to hanging behind the next ring (−25°), and the catch carries on into the swing. A jump pressed late in the flight chains to the next ring right after the catch.
- Targets: rings within 40–240 cm horizontally, from 130 cm below to 70 cm above, within about 57° of the wanted direction, with a clear line and a clear capsule path.
- Edge grab (`TryGrabEdge`): faces at head/chest/hip height, including an eave's sloped cut end. If an overhang is over his head, he reaches up to 64 cm above his centre for its lip.
- `FindLedge` now holds an overhang's outer lip instead of the wall face below it. It also rejects "tops" with solid geometry on them (found from inside a roof).

## Tests

`-ChuckLampSwingTest`, also run by `Tools/Verify-Package.ps1` as its own process; `-ChuckLampSwingCapture` saves frames to `Saved/Screenshots/Windows/LampSwing`. The seven trials:

0–2. Chimney climbs between the lanterns, one in each alley, onto the lower roof (no ring caught).
3. A chimney climb under the first lantern catches its ring.
4. Brachiation to the alley's fifth ring with the stick let go (four leaps).
5. Brachiation back with the stick held back (four leaps).
6. Kick off the ring toward the far wall, then wall jumps on up onto the roof.

The default smoke check `CHUCK_WALLLAMPS_CHECK` also sweeps Chuck's capsule along all twelve ring-to-ring flights. See HANDOFF for the actual results.

## Stamina (user 2026-10-09)

Climbing is mostly for flavour and takes small sips of the sprint stamina ring: 0.6% per wall run, 0.4% per wall jump, 3% per side wall run and 1.2% per ring-to-ring leap. The ring doesn't refill while he is wall running, side wall running, swinging or in a wall jump's flight; it refills as before everywhere else, hanging on a ledge included. Stamina never stops a climb: with an empty ring he wall jumps, climbs and swings exactly as with a full one. Measured: a 660 cm chimney climb costs about 18%, four leaps about 5%. Since a sprint still needs a full ring, a long climb delays the next sprint by a couple of seconds.

**Golden leaf hook (Chult, later):** `AChuckCharacter::LockStamina(Seconds)` / `IsStaminaLocked()`. While it is locked:

- stamina stays full and the sprint never runs down
- a wall run carries on past its three steps at 120 cm/s until it reaches a top (caught as usual), a ring or he jumps off
- the side wall run lasts 2.2 s instead of 1.1 s under half the gravity, a much longer arc

There is no pickup, HUD tint or timer display yet; `IsStaminaLocked()` is there for the HUD.

Tests: lamp-swing trial 0 now climbs with an empty ring, trial 1 requires a full-ring chimney to cost 5–30%, trial 4 requires the swing run to cost something but under 15%, and new trial 7 locks stamina and requires one wall run (no wall jumps) to reach the eave and pull up, with the ring full throughout. The locked side wall run is not covered by a test.
