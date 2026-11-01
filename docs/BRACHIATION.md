# Brachiation and eave grabs (lamp alleys)

User request, 2026-10-09: a brachiation feature on the high wall lanterns between the tall rear-row houses by the sewer — a demo and test area now, a major Chult mechanic later. "Look hard, be easy": a wall jump up to a lantern catches it seamlessly, and swinging lantern to lantern must not be hard. In the same alleys, a chimney climb (wall jumping between the two walls) must catch the edge of the pitched roof and make it easy to pull up and stand on it.

## Controls (PC)

- **Catch:** wall jump up near a lantern. Chuck catches its ring automatically when his raised paws pass within 45 cm sideways of it (from 45 cm below to 30 cm above). No button press is needed.
- **Swing to the next ring:** press Space. He flies to the nearest ring in the stick's direction (camera-relative W, A, S, D). With no direction held, he goes straight on along the swing. He catches it for you.
- **Pump:** push the stick back as he swings back and forward as he swings forward. Held one way, it does nothing. Big gaps need it (see "Big gaps need a real swing" below).
- **Kick off:** press Space with no ring that way, for example toward a wall. He launches like a wall jump, so a wall in reach is run up automatically, and the chimney climb continues.
- **Let go:** press C (dodge).
- **Step down at a chain's end (Chult, 2026-10-13):** press Space with the stick released at the last grip of a line. If there is something to stand on within 170 cm below, just ahead along the swing (a tree's walking limb), he hops down onto it (`CHUCK_SWING_STEP_DOWN`). Otherwise (the lamp alleys) he goes back along the line as before.
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

The default smoke check `CHUCK_WALLLAMPS_CHECK` also sweeps Chuck's capsule along all twelve ring-to-ring flights. See archive/HANDOFF-PROTOTYPE.md for the actual results.

## Stamina (user 2026-10-09)

Climbing is mostly for flavour and takes small sips of the sprint stamina ring: 0.6% per wall run, 0.4% per wall jump, 3% per side wall run and 1.2% per ring-to-ring leap. The ring doesn't refill while he is wall running, side wall running, swinging or in a wall jump's flight; it refills as before everywhere else, hanging on a ledge included. Stamina never stops a climb: with an empty ring he wall jumps, climbs and swings exactly as with a full one. Measured: a 660 cm chimney climb costs about 18%, four leaps about 5%. Since a sprint still needs a full ring, a long climb delays the next sprint by a couple of seconds.

**Golden leaf hook (Chult, later):** `AChuckCharacter::LockStamina(Seconds)` / `IsStaminaLocked()`. While it is locked:

- stamina stays full and the sprint never runs down
- a wall run carries on past its three steps at 120 cm/s until it reaches a top (caught as usual), a ring or he jumps off
- the side wall run lasts 2.2 s instead of 1.1 s under half the gravity, a much longer arc

There is no pickup, HUD tint or timer display yet; `IsStaminaLocked()` is there for the HUD.

Tests: lamp-swing trial 0 now climbs with an empty ring, trial 1 requires a full-ring chimney to cost 5–30%, trial 4 requires the swing run to cost something but under 15%, and new trial 7 locks stamina and requires one wall run (no wall jumps) to reach the eave and pull up, with the ring full throughout. The locked side wall run is not covered by a test.

## Chult canopy (2026-10-13)

The same Swing/SwingLeap gaits on Chult's four canopy trees: 38 branch grips in four chains, with a 180 cm leap across each gap between two trees' boughs, lianas registered as climbables, and the step-down above. Layout, contract and tests: [CHULT-CANOPY.md](CHULT-CANOPY.md). `-ChuckCanopyTest` runs 18 real-character trials; the lamp-swing test still passes 8/8 with the step-down in place.

## Big gaps need a real swing; under a deck (2026-10-31, reworked 11-01)

User requests:
- 10-31: pumping on a hold had no practical effect on the leap. For the larger jumps he should need enough swing going to make it ("a slight challenge", a little risk on the bridge, not hard). His head and ears also passed through the bridge's planks.
- 11-01, after the first version: holding the stick forward still carried every gap. The player should have to swing back, then forward, then jump. The physics should "make sense... like a real person trying to make a big brachiation jump", and it applies to the gaps between the trees too.

**The pump** (`SwingPumpGain` = 7 rad/s²):
- The stick drives his legs the way it points, but only through the bottom of the swing (within 30°, strongest at the bottom), like a kip.
- Held one way, it gains on the stroke going that way what it takes from the stroke coming back, so holding forward builds nothing.
- Back while he swings back and forward while he swings forward, it builds the swing:
  - from a leap's catch: about 42° after one swing back;
  - from a near-still hang: about 35° after one swing back and 42° after two.
- From a dead hang a push starts the swing.
- A catch now carries the body only 14° in front (27° before, 35° originally), so a leap's momentum alone never makes the next big gap.

**The leap's reach** (`SwingReachNow`): leaps up to `SwingEasyLeap` (165 cm, grip to grip) need nothing, as before. A longer leap goes as far as his release carries him:
- His centre leaves at the swing's own velocity (L × rate along the arc: forward and up when he lets go on the way up in front). That is multiplied by `SwingReachK` = 2.4 for the legs' whip, and the arms' throw is added (140 cm/s along, 180 cm/s up).
- The launch is flown ballistically to the next grip's height under his gravity.
- Let go on the back stroke, the swing takes away from the throw.
- Measured reach for a 180 cm gap:

| How the player swings | Reach |
|---|---|
| Still, stick let go | about 84 cm |
| Stick held forward for 2.5 s | about 83 cm |
| One swing back from a catch, released at the best point | 230–255 cm |
| Two swings back from a near-still hang | 260 cm |

**The jump's timing:**
- A jump pressed for a big gap waits for the swing toward it. Pressed on the back stroke, it waits until the swing turns.
- He lets go at the best point of that forward stroke, in front of the grip where the reach stops growing (about 17–23° in front). The press doesn't have to be frame-exact; the swing is the challenge.
- If no swing toward the gap comes (under 4° after 0.45 s, or 1 s in all), he goes anyway.
- The grip it was aimed at is kept while it waits, so pumping back with the stick doesn't re-aim him backward.
- Short of the grip, his paws reach 45–90% of the way (by reach ÷ gap) and 22 cm low, and he drops (`CHUCK_SWING_SHORT`, `CHUCK_SWING_MISS`). That grip can't be caught for 1.5 s. Over the bridge's gorge that is a fall death and a return to the bank.
- Jumping while the stick points back aims the leap back, as always. At the bridge's last plank, with nothing behind, that kicks off.

**Where this bites:**
- The bridge's 6–7-slot gaps: 174–185 cm, about half its leaps.
- The gaps between the canopy's and the giant canopy's trees: 180–182 cm.

The lamp alleys (125–146 cm) and the temple's rings (140 cm) are unchanged.

**Under a board** (`FChuckSwingGrip::Depth`, set by the bridge for every plank):
- He holds a plank by its near edge going his way, not its centre: paws under it, fingers over the edge, inset depth/2 − 2.5 cm and 1.6 cm below its mid-thickness. His head hangs in the gap he came from.
- The plank-to-plank flight dips 6–14 cm instead of rising up to 30 cm over the deck.
- While he is under a board, the anim lays both ears flat back (−62°) and lifts the chin a little.
- It holds the head near level against the swing's body pitch (`FChuckAnimParams::DuckWeight`, `DuckLevel`: 0.7 × pitch, less 1.35 × the pitch past 30°, clamped to −20…30). Past 30° the head nods forward again, because at the top of a big swing the body's own tip lifts the crown toward the board.
- Rings and branches (no depth) are unchanged.

**Measured** on the bridge test's two crossings, with ear tips, crown and snout taken from the imported mesh (`-ChuckHeadProbe`; `CHUCK_BRIDGE_HEAD`):

| | Frames with a point inside a plank | Deepest | Head above the deck's underside mid-leap |
|---|---|---|---|
| Before (10-31) | 246 of 1903 | 15.1 cm | 31 cm |
| Now (`Local/brach-s2-bridge.log`) | 661 of 4288, only 3 deeper than 1 cm | 2.1 cm | 8.9 cm (in the gaps) |

- The 1 cm-or-less contact is the crown's fur brushing the board at the top of the big swings.
- The test fails at more than 5% of frames deeper than 1 cm, 5 cm through, or 10 cm above.
- Review still (10-31): `SourceAssets/Setting/Review/Bridge_BrachiationUnder.png`.

**Tests:** `-ChuckBridgeTest` has 9 trials, all with real stick input and no assist:
- The crossings pump like a player: the stick back as he swings back and toward the gap as he swings toward it, with the jump pressed on the forward stroke after a swing back.
- `big_gap_no_swing_falls_short`: hanging still, jump with the stick let go. He falls short and returns to the bank.
- `big_gap_stick_held_forward_falls_short`: stick held toward the gap for 2.5 s, then jump. He still falls short.
- `big_gap_swing_back_then_forward`: pumped in time, two swings back, jumped going forward. He makes it.

Route tests that cross big gaps (canopy, giant canopy, golden leaf, raptor, birds) call `SetTestSwingPump(true)`. It pumps in time for them, and their big leaps wait until the swing carries them; the bridge test covers the player's side.
