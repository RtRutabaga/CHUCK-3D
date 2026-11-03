# Brachiation and eave grabs (lamp alleys)

User request, 2026-10-09: a brachiation feature on the high wall lanterns between the tall rear-row houses by the sewer — a demo and test area now, a major Chult mechanic later. "Look hard, be easy": a wall jump up to a lantern catches it seamlessly, and swinging lantern to lantern must not be hard. In the same alleys, a chimney climb (wall jumping between the two walls) must catch the edge of the pitched roof and make it easy to pull up and stand on it.

## Controls (PC)

- **Catch:** wall jump up near a lantern. Chuck catches its ring automatically when his raised paws pass within 45 cm sideways of it (from 45 cm below to 30 cm above). No button press is needed.
- **Swing to the next ring:** press Space. He flies to the nearest ring in the stick's direction (camera-relative W, A, S, D). With no direction held, he goes straight on along the swing. He catches it for you.
- **Pump:** push the stick back as he swings back and forward as he swings forward. Held one way, it does nothing. Big gaps need one full swing back, then forward (see "Big gaps need a real swing" below).
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

## Big gaps need a real swing; under a deck (2026-10-31, reworked 11-01 and 11-02)

User requests:
- 10-31: pumping on a hold had no practical effect on the leap. For the larger jumps he should need enough swing going to make it ("a slight challenge", a little risk on the bridge, not hard). His head and ears also passed through the bridge's planks.
- 11-01, after the first version: holding the stick forward still carried every gap. The player should have to swing back, then forward, then jump, with physics that "make sense... like a real person trying to make a big brachiation jump", between the trees too.
- 11-01, later: "really feel the momentum". He seemed to slow or pause on the swing and then jump unrealistically forward. He should propel forward with the momentum of the back-and-forth swing.
- 11-02: "impossible to make it across". Going back the full amount, then forward the full amount, and releasing while going forward should make the next plank; holding forward still shouldn't. Less of a sweet spot: "make forward swing you more forward easier".

**The swing** (a pendulum from the grip, L = 31.5 cm to his centre, his gravity 784 cm/s²):
- Damping is `SwingDamping` = 0.45/s (0.9 before 11-01 later, which made the swing drag and lose its speed).
- It goes as high as `SwingAngleLimit` (66°) from a ring or branch.
- Under a deck it stops at `SwingDeckLimit` (44°) in front and `SwingDeckLimitBack` (38°) behind. Higher, his snout meets the board he holds, or his head rises through the gap behind.

**The pump** (`SwingPumpGain` = 8 rad/s²):
- The stick drives his legs the way it points, only through the bottom of the swing (within 30°, strongest at the bottom), like a kip.
- Held one way, it gains on the stroke going that way what it takes from the stroke coming back, so holding forward builds nothing beyond one stroke.
- Back while he swings back and forward while he swings forward, it builds the swing.
- **A small swing, or none** (under 12°, 11-02): the push is a lean, 6 rad/s², the legs drawn that way. Held back from a still hang he swings back about 20° and hangs back there; turned forward then, he goes. Before, only a 6° kick: held back he barely moved, so a swing could hardly be started.
- **The forward beat** (11-02): facing his way, the forward push on the forward stroke drives harder after a back swing, by up to `SwingForwardBeat` (1.25) more. None after a back swing under 10°, full from 20° (`SwingBeatBackFrom`, `SwingBeatBackFull`). Held forward there is no back swing to drive from, so it gains nothing; one full swing back then forward is enough.

**The catch:** a leap's catch takes him at 8° behind the grip, moving forward, with a swing of `SwingCatchCarry` (20°). His flight's speed runs on into the arms for a moment, rather than stopping dead, so he visibly swings through under the new grip.
- That swing and one pushed forward stroke are never enough for a big gap. The next one needs a swing back and forward again.

**The leap's reach** (`SwingReachNow`; only for leaps over `SwingEasyLeap`, 165 cm grip to grip):
- What he has is the swing's own velocity as he lets go (L × rate along the arc), times `SwingReachK` = 2.8 for the legs' whip, plus the arms' throw (120 cm/s along, 100 up). It is flown to the next grip's height under his gravity.
- More of the reach now comes from the swing and less from the throw (2.4 and 140/180 before), so a weak swing falls well short.
- Let go on the back stroke, the swing takes away from the throw.

**The flight** (every leap, 11-01 later):
- His centre leaves exactly where the swing has it, at the swing's own velocity (`SwingLeapVs`).
- The throw builds on that over `SwingLeapRamp` (0.14 s), not in one frame. Then only his gravity acts, a real parabola, until he hangs behind the next grip (`SwingLeapAt`).
- The launch leaves at the angle the swing and throw give it (20–48° in the open, 14–27° under a deck), with the speed that angle needs to reach the grip.
- Under a deck the arc rises at most 12 cm. He comes in 6 cm low with his arms reaching, and is taken up onto the swing as they close.
- His body carries on turning the way the swing turned it, easing off (the legs thrown through), then hangs down for the catch. Under a deck that carry stops at 30°.
- Before, the paws slid in a straight line at a fixed 320 cm/s, whatever the swing was doing.
- `CHUCK_SWING_FLIGHT` logs the swing speed, launch speed and angle, flight time and apex.

**The jump's timing:**
- A jump pressed for a big gap waits for the swing toward it. Pressed on the back stroke, it waits until the swing turns.
- He lets go at the best point of that forward stroke, about 20–28° in front, where the reach stops growing.
- **No sweet spot** (11-02): a press anywhere on the stroke toward the gap goes with that stroke's best reach (`SwingStrokeBest`, from the swing's energy and the arc's limit), wherever he actually lets go. A press up to `SwingTopGrace` (0.15 s) after the forward swing tops out does too, and he lets go there. Pressed later, on the way back, the jump waits for the next forward stroke. `CHUCK_SWING_STROKE_BEST` logs when it helped.
- If no swing toward the gap comes (under 4° after 0.45 s, or 1 s in all), he goes anyway.
- The grip it was aimed at is kept while it waits, so pumping back with the stick doesn't re-aim him backward.
- Short of the grip, he flies the launch he has (45–92% of what it needed), so he falls short and low. When the flight ends he drops with its velocity (`CHUCK_SWING_SHORT`, `CHUCK_SWING_MISS`). That grip can't be caught for 1.5 s. Over the bridge's gorge that is a fall death and a return to the bank.
- Jumping while the stick points back aims the leap back, as always.

**Measured on the bridge** (`Local/brach-b5.log`, 11-02, 180 cm gaps unless noted):

| How the player swings | Reach (stroke's best) |
|---|---|
| Still, stick let go | 49 cm |
| Stick held forward for 2.5 s from still | 56 cm |
| Stick held forward, jumping on as he catches (175 cm gap) | 154 (160) cm: short |
| From a still hang: back until he turns, forward 0.12 s later | 204 (212) cm: made |
| The same, pressed as the forward swing tops out | 33 there, 198 best: made |
| One swing back and forward after a catch (the crossings) | 204–248 cm |

Before 11-02 (`Local/brach-s3c.log`) one swing back and forward from a still hang reached about 157 cm; it took two perfectly timed swings.

- Big-gap flights leave at 108–120 cm/s on the swing and reach about 420–440 cm/s at the end of the throw, in 0.45 s.
- From a catch, the pumping takes about 1.4 s, one swing back and forward. From a near-still hang it takes about 2–3 s.

**Where this bites:**
- The bridge's 6–7-slot gaps: 174–185 cm, about half its leaps.
- The gaps between the canopy's and the giant canopy's trees: 180–182 cm.

The lamp alleys (125–146 cm) and the temple's rings (140 cm) need no swing, but their leaps fly the same way.

**Under a board** (`FChuckSwingGrip::Depth`, set by the bridge for every plank):
- He holds a plank by its near edge going his way: paws under it, fingers over the edge, inset depth/2 − 2.5 cm and 1.6 cm below its mid-thickness.
- The anim lays both ears flat back (−62°) and holds the head near level against the body's pitch (`DuckWeight`, `DuckLevel`: 0.7 × pitch, less 1.35 × the pitch past 30°, clamped to −20…30).
- The swing and flight limits above keep his head under the boards.

**Measured** on the bridge test's two crossings (ear tips, crown and snout from the imported mesh; `CHUCK_BRIDGE_HEAD`):

| | Frames with a point inside a plank | Deepest | Head above the deck's underside |
|---|---|---|---|
| Before (10-31) | 246 of 1903 | 15.1 cm | 31 cm |
| 11-01 | 661 of 4288, 3 deeper than 1 cm | 2.1 cm | 8.9 cm |
| 11-01 later (`Local/brach-s3c.log`) | 835 of 3693, 72 deeper than 1 cm | 1.9 cm | 9.2 cm |
| 11-02 (`Local/brach-b5.log`) | 812 of 3619, 65 deeper than 1 cm | 2.6 cm | 9.2 cm |

- The contact is the crown's fur and the snout brushing the board at the top of the big swings.
- The test fails at more than 5% of frames deeper than 1 cm, 5 cm through, or 10 cm above.
- `CHUCK_BRIDGE_HEAD_ABOVE` logs frames more than 8 cm above.

**Tests:** `-ChuckBridgeTest` has 11 trials, all with real stick input and no assist:
- The crossings pump like a player: the stick back as he swings back and toward the gap as he swings toward it. Jump is pressed on the first forward stroke after one swing back (11-02; before, once the swing was above 42.5°).
- `big_gap_no_swing_falls_short`: hanging still, jump with the stick let go. Short, back to the bank.
- `big_gap_stick_held_forward_falls_short`: stick held toward the gap for 2.5 s, then jump. Short.
- `big_gap_held_forward_jumping_on_falls_short` (new): the stick held forward throughout, leaping onto the plank before a big gap and jumping on at once. Short.
- `big_gap_one_swing_back_then_forward` (11-02): from a still hang, the stick back until he has swung back and turned, forward 0.12 s later (a player's reaction), jump pressed going forward. Made.
- `big_gap_one_swing_pressed_as_it_tops_out` (11-02): the same, jump pressed only as the forward swing turns back. Made.

Route tests that cross big gaps (canopy, giant canopy, golden leaf, raptor, birds) call `SetTestSwingPump(true)`. It pumps in time for them, and their big leaps wait until the swing carries them.