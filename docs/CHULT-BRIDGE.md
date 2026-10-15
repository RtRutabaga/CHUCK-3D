# The canyon's broken rope bridge (Claude)

User request, October 14, 2026: rebuild Codex's decrepit bridge as a normal
horizontal-plank rope bridge so broken that Chuck crosses it only by
brachiating from intact plank to intact plank, and never pulls himself up to
stand on one. Broken nubs at the ropes, wholly missing stretches, some planks
split to a thin half; ropes properly lashed to the planks and the hand ropes.
The bridge and the far side play the user's "Chult 2" track.

Source: `ChultBridge.h/.cpp`, built from `BuildChultCanyon`. Codex's canyon,
banks, vegetation and the bridge coordinates (posts at S=-240/4800, T=0
approach) are unchanged; the canyon's random vegetation stream is drawn exactly
as before, so nothing else moved.

## Layout

- 166 plank slots at a normal 30 cm pitch between the posts. The deck sags a
  metre: top at Z=472 by the posts, 372 at mid-span (`ChultBridgeDeckZ`).
- **31 surviving planks** (`ChultBridgePlank00`–`30`, tag `ChultBridgeGrip`), 120–185 cm
  apart (4–7 slots). Plank 0 is over the near bank, 108 cm above it; the last is
  over the far bank. Four of them are split lengthwise: a thin 7–10 cm half spans,
  the lost half left as stubs at the ropes.
- Between them each gap has its own decay: bare (only rope and old lashing
  remnants), nubby, or mixed. 57 slots hold nubs broken off near one or both
  ropes, every strip at its own length, most ending in a splinter; 74 are
  missing (39 lashing remnants with frayed tails); 4 planks hang by one end's
  lashing. Nothing broken reaches nearer than 33 cm to the centre line, so his
  paws and body pass clear.
- Ropes: two continuous foot ropes under the plank ends and two hand ropes
  (the +T one snapped between S=2290 and 2470, both ends hanging and frayed),
  laid three-strand. 67 suspenders tie hand rope to foot rope (12 snapped
  stubs). Each plank end is bound to its foot rope with three skewed turns of
  cord; the ropes are wound round the solid posts.
- Planks and ropes have **no collision**: there is no deck to stand on, so no
  edge grab or pull-up can find one. The posts are solid.

## Traversal

The surviving planks are ordinary brachiation grips (`AddChuckSwingGrip`,
along the canyon frame). From standing under plank 0 a hop catches it; jump
(stick let go) goes on plank to plank; the stick steers back; at the last
plank a jump with the stick let go steps down onto the bank. Letting go over
the gorge is a fall death: he comes back on the near bank by the bridge
(S=-300, facing along it), or on the far bank (S=4820) if he went over by the
far lip. The canyon's own walk-off trial now expects that return.

## Music

Past S=-40 (on the first plank and beyond, the gorge and the far side) the
score cross-fades to `SW_Chult2` (2.5 s, 45%); back past S=-220 on the near
bank, `SW_Chult` returns. `ADockGameMode::GetMusicRegion()` reports 4 there.

## Verification

`-ChuckBridgeTest` (in `Verify-Package.ps1`): static checks (31 grips
registered, spacing, hanging bodies and leap paths clear, no deck under any
grip, end planks over the banks at hop height, nubs clear of the centre line),
then the real character: catch the first plank; cross all 30 leaps; step down
on the far bank; cross back steering with the stick and step down; let go over
the gorge and return to the near bank with Chult's score back; dropped onto a
plank, he does not stand on it. `-ChuckBridgeCapture` saves review stills to
`Saved/Screenshots/<platform>/Bridge`.
