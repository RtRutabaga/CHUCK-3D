# Chult's flying jungle birds (Claude, 2026-10-19)

User request, October 19, 2026: "add some flying jungle birds to chult, in
particular i want some birds to fly past player while crossing bridge and i
want birds flying above temple in a cinematic wonderous way".

Source: `ChultBirds.h/.cpp` (`AChultBirds`, spawned from `AChultLanding::Build`
after the T-rex). Models: `Tools/build_chult_birds.py` (Blender 4.5.14) ->
`SourceAssets/Props/ChultBirds/*.fbx`, imported by `Tools/import_chult_birds.py`
into `/Game/Art/ChultBirds` (10 meshes and `M_ChultBird`). No downloads.

## The birds

Scarlet macaws and blue-and-gold macaws, modelled as lofts: body, head, hooked
beak, eyes, a long graduated tail, and each wing in two parts (shoulder to
wrist; the hand with six overlapping primaries). The plumage is vertex colour
(`M_ChultBird` takes it as base colour). About 86 cm beak to tail tip and 1.1 m
across at scale 1; the bridge birds fly at 1.3, the temple's at 1.35-1.5.

The code flies them. Each bird has a position along a line or orbit. It
yaws along its velocity, pitches with its climb and banks into its turns (roll
from lateral acceleration). Each wing part is rolled about its joint: flapping
at 3.8-5.2 Hz with the hand lagging the arm and sweeping back on the
upstroke, or gliding with the wings held slightly up. Flights mostly flap. The
wheels mostly glide, with flapping bouts, and anything climbing flaps. No part
collides, nothing changes play, and they are not in any other automated run.

## The bridge

Once each per crossing, re-armed when he leaves the bridge (or falls), while
he hangs or leaps from plank to plank:

- **The flock** (7 birds, from 14% of the way over): up out of the gorge from
  ahead of him on one side, across the bridge a few metres in front of where
  he will be (his along-bridge speed predicts it), and away past him up the
  other reach. His follow camera looks down on him, so most cross at or below
  the deck.
- **The pair** (2 birds, from 40-56% of the way over): from behind and above
  him, low over his head (about 2 m), away ahead, then banking off down the
  gorge.

Paths stay 6 m clear of both walls, which lean in below the lips.

## The temple

- **The high wheel:** eight pairs circle the spire at 27-50 m radius and
  35-56 m above the plaza, each pair wingtip to wingtip, the circle breathing
  in and out and drifting slowly.
- **The low wheel:** six pairs circle the other way over the avenue and the
  plaza's front, at 21-36 m radius and 22-32 m above the plaza.
- **The figure of eight:** two pairs fly round both pyramids, about 36-41 m up.
- **Swoops:** in the precinct, every 15-24 s a pair leaves the low wheel and
  swoops low through his view, then climbs back. The first swoop comes 6 s
  after he enters, and the kind alternates. One kind dives over his shoulder
  and away ahead of him, low up the avenue. The other comes in from one side,
  skims 2.5-5 m over the ground 11-19 m ahead and goes out the other side.
  Each swoop path is traced against the world when it is chosen. A blocked
  swoop tries another height and distance, and if all four fail it is
  skipped.
- **The reveal:** running up the road past the point where the spire comes
  into view, as "Chult 3" swells, seven birds burst from the undergrowth
  either side just ahead of him. They fly away up the road ahead of him
  through the strip of sky over it, climb toward the temple and join the
  wheel. Coming back from 80 m down the road re-arms it. A teleport (respawn
  or checkpoint) does not set it off.

The temple's birds are only posed while he is within 350 m of the spire.

## Verification

`-ChuckChultTest -ChuckBirdReview` (in `Verify-Package.ps1`). It runs four
checks and writes these stills and frames:

- **Static check** (`CHUCK_CHULT_BIRDS_CHECK`): 48 birds, all meshes loaded,
  and no world-static hit along any part of the flights:
  - 240 s of every wheel and figure-of-eight bird;
  - the bridge flock and pair from both banks, both sides, at each point
    where they can fire;
  - the reveal flight and its 240 s in the wheel.

  Hidden blockers, such as the jungle wall behind the temple, are ignored.
  The log also reports how often wheel birds fall inside a plant instance's
  bounding box. That count is for information only, because foliage does not
  collide.
- **Stills:** `BirdsTemplePlaza`, `BirdsAvenueUp`, `BirdsTempleStair`,
  `BirdsTempleAvenue`, `BirdsTempleWide`, plus three chase views of single
  birds (`BirdsClose*`).
- **Bridge** (`CHUCK_CHULT_BIRDS_BRIDGE`): the real character crosses plank to
  plank, with input locked and his own follow camera. The test requires:
  - both flights fire;
  - at least 3 flock birds on his screen within 40 m, for at least 1.5 s;
  - the pair on screen, passing within 0.6-4 m of him.

  Frames: `BirdsBridge_*`.
- **Reveal** (`CHUCK_CHULT_BIRDS_REVEAL`): he runs up the road past the
  reveal. The test requires the flock to launch, at least 3 of its birds on
  screen for at least 3 s, and all 7 to reach their circle. Frames:
  `BirdsReveal_*`.
- **Swoops** (`CHUCK_CHULT_BIRDS_SWOOPS`): he stands at the avenue's head for
  50 s. The test requires at least 2 swoops and wheel birds on screen within
  40 m for at least 4 s. Frames: `BirdsSwoop_*`.

## Limits

- "On screen" is a projection into his viewport. Foliage does not collide, so
  the test cannot tell when leaves hide a bird. On the reveal road the
  undergrowth hides much of the flock, and the frames show it smaller than
  the on-screen time suggests.
- The default elevated camera (48° down) sees less sky than the low camera the
  review uses (-8°). The bridge pair and the swoops come close enough for it.
  The wheels show when he looks up.
- From the plaza the wheels are small against the sky: they are far up, as
  real birds over a temple would be.
- No bird calls or wing sounds.
- The models are simple lofts with vertex colour, without feather texture.
  The primaries read as separate blades up close.
- Birds can pass through tree crowns: these have no collision for the
  clearance check to find.
