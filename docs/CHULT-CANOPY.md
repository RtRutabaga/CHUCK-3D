# Chult canopy grove — traversal and setting contract

**Status (2026-10-13, Claude):** active brachiation. **Follow-up the same day (user):** branches in all
directions, all brachiatable the same way, with natural variety; and no sharp angles where roots go up
trunks, on every tree including the cutscene's. See *Spurs* and *Buttresses* below. The user asked Claude to "make this work" on Codex's
brachiation grove and allowed the trees to be changed to look more natural. The paired climbing
trunks and inactive paw proposals of Codex's two earlier passes are replaced by four single-trunk
canopy giants with real, registered grips. `ChultCanopy.cpp` owns the trees, grips, lianas and the
traversal tests; `ChuckCharacter.cpp` owns the moves (see [BRACHIATION.md](BRACHIATION.md)).

## Location and access

Coordinates are centimetres relative to `AChultLanding::FloorOrigin()` (400000,2500000,0). The
reserve (`InChultCanopyReserve`) is unchanged: X=5000–8800, Y=250–3000, kept free of random trunks
and colliding understory. The ground lanes (`GroundLane` here, `GroveRoute` in ChultLanding.cpp)
are unchanged and kept clear by every root and buttress (115 cm margin).

Ways up:

1. **The ruin's landing slab** (Codex's causeway, top Z=670 at (5530,820)). Its north edge drops
   20 cm onto tree 0's third limb, which runs under the slab and carries it, into tree 0's crotch.
2. **A liana on every tree** (`AddChuckClimbable`). Walk into its foot to climb; at the top he pulls
   up onto the limb. Walking off that side of the limb toward the drop lowers him onto it again.

## Trees

| Tree | Trunk axis (local) | Crotch / limb top Z | Girth | Limbs toward |
| --- | --- | --- | --- | --- |
| 0 | (5560,1230) | 650 | 1.00 | tree 1, tree 3, the ruin slab (top 645, under the stone), 2 spurs |
| 1 | (7250,760) | 650 | 1.08 | tree 2, tree 0, 3 spurs |
| 2 | (8350,1800) | 760 | 0.93 | tree 3, tree 1, 2 spurs |
| 3 | (6750,2600) | 650 | 1.04 | tree 0, tree 2, 2 spurs |

Each tree (`ChultCanopyTrees()`):

- **Trunk and buttresses:** one fluted, flared, slightly leaning trunk, about 3.5 m wide at the
  ground and 1.7 m at the crotch (girth varies per tree). **Buttresses are lobes of the trunk's own
  surface** (`ButtressedTrunk`): broad and filleted where they join it, a thin rounded plank further
  out, fading tangentially into the bark at their top (1.7–3.2 m up), reaching 2.5–4.2 m out; 5–8
  per tree. There is no separate piece, so no seam, crease or spike where root meets trunk. Low
  snaking surface roots run between them. No buttress or root crosses a ground lane or a liana's foot
  (checked along their real curves).
- **Crotch:** the trunk's top is a shallow standing bowl with a low saddle toward the front;
  `ChultCanopyFork<I>` (tag `ChultCanopyStanding`) marks a standing point in it.
- **Walking limbs:** one straight at each neighbour (each 4.05–4.6 m long), 62 cm radius at the trunk,
  34 cm at the first grip (300 cm out), level to there, then a slight droop. Past 430 cm each bends away from under the
  grips and tapers up into a leafy end. Their tops are walkable bark (checked every 30 cm).
- **Leaders and crown:** a main leader rises from the back of the crotch (the widest gap between the
  fixed limbs) to 8.4–10 m above it; a second branches off it above head height toward its own side.
  Boughs spread off both all round (golden-angle turns, each its own reach and rise; 44 in all), with
  leaf clusters (the existing `SM_FallLeafCard`, about 8,100 instances in four `ChultCanopyCrown<I>`
  components, tag `ChultCanopyFoliage`, no collision). Leaves are kept out of the swing lanes, off the walking limbs
  and clear of the crotches.
- **Hand boughs:** from the nearer leader, each arcs over its walking limb well above head height
  and runs out along the chain, thinning from 12 to 6 cm radius toward the gap, with small twigs and
  leaves above the hands' line. Noncolliding, like the lamp rings.

Collision: trunk, saddle, walking limbs, leaders, buttresses and roots are one visible
procedural mesh (`ChultCanopySolidBark`, BlockAll, complex as simple). Hand boughs, crown boughs,
twigs (`ChultCanopyHandBranches`) and lianas (`ChultCanopyVines`) have none.

## Dying near the trees (2026-10-14)

User request: if Chuck dies in the area around the brachiation trees, he comes back on the top of
the ruins where the trees are reached from. `AChuckCharacter::GetAreaStartLocation()` asks
`InChultCanopyDeathArea()`, which is the canopy reserve (the loop's rectangle and the line's 10 m
corridor) plus 5 m. Inside it, after a fall or astral death, he returns to `ChultCanopyRespawn()`:
Codex's landing slab top at (5600,885,670), a checked standing top by its north edge, facing tree 0
(`ChultCanopyRespawnYaw`). Anywhere else in Chult he still returns to the landing entrance. The
static check asserts the spot is stone with room to stand; three test trials die in a line tree,
on the ground under the line and on the landing loop.

## The line into the open ground (2026-10-14)

User request: more brachiation trees in a staggered line leading away into the blank part of the
map. Beyond 90 m from the landing the forest ring stops and the floor is bare out to its edge;
the line runs north-east from tree 2 into that strip. Four more canopy giants, built like the
others (lobed buttresses, crotch, walking limbs, leaders and crown, liana, spurs):

| Tree | Trunk axis (local) | Crotch Z | Girth | Linked to |
| --- | --- | --- | --- | --- |
| 4 | (9050,3250) | 700 | 0.97 | tree 2 (chain 4), tree 5 |
| 5 | (10350,4200) | 760 | 1.06 | tree 4, tree 6 |
| 6 | (10650,5750) | 690 | 0.95 | tree 5, tree 7 |
| 7 | (11700,6600) | 740 | 1.02 | tree 6 (end of the line; four spur tries) |

The trees alternate either side of the line (13.5–16 m apart). Chains 4–7 (2→4→5→6→7) are like the
loop's: end grips 98 cm over the limbs, a 180 cm branch gap. Trees take their limbs from
`Neighbours`; each chain records the limb it uses at either end (`LimbA`, `LimbB`). The loop's
trees keep their limb order and random draws; tree 2 gains a limb toward tree 4 and its spurs
moved to suit. `InChultCanopyReserve` now also covers a 10 m corridor along the line; spur grips
stay within the original 132 m reach from the landing. **October 14, Codex:** the map now extends
beyond the line with starting-area vegetation, mild relief and a shallow flowing stream; see
[CHULT-EXPANSION.md](CHULT-EXPANSION.md). The canopy corridor and liana feet remain level at Z=0,
and all eight trees/grips/respawn locations stay unchanged.

## Spurs (side branches, all brachiatable)

`ChultCanopySpurs()`: side limbs out of the crotches (14 with the line), in every direction left free, each its
own length (3.3–4.2 m), girth (0.78–0.95), pitch (−5 to +8 %), sideways bend and droop. Each has its
own hand bough with 3–5 grips (33 in all), the first 98 cm over the limb 2.7–3.1 m out, then
1.18–1.38 m apart. A spur is a dead end, used the same way as a chain: walk out, jump to the first
grip, swing out to the tip; at the tip a jump with the stick released comes back (nothing below to
step down to, which the static check asserts); at the first grip it steps down onto the limb.

Placement rules: at least 52 degrees from every other limb (first grips more than 2.4 m apart, so
no line steals another's grips), 54 from the main leader (walking room in the crotch), all grips
inside the reserve, more than 2.5 m from any chain or other spur grip, 3.3 m from other trunks and
clear of the ruin slab. Names: `ChultCanopySpur<Tree>_<Limb>`.

## Chains and grips

`ChultCanopyChains()` returns four chains, tree I to tree I+1 (0→1→2→3→0), 38 grips in all, each
registered with `AddChuckSwingGrip` (its `Along` is the chain's tangent) and marked by a
`<Chain>_PawNN` component (tag `ChultCanopyGrip`; the first grip after the gap also
`ChultCanopyBranchJump`).

- The first and last grips of a chain hang **98 cm above the walking limb, 300 cm out from the
  trunk**. Standing, his paws are 65 cm up and a standing hop adds about 18 cm, so a jump straight
  up from the limb catches it; hanging, his feet clear the bark by about 34 cm.
- Grips are 122–129 cm apart along each bough; the **branch gap is 180 cm** between the two boughs'
  end grips (no wood spans it). The chain arches up 28 cm and bows sideways 24 cm, with a small
  wander; the largest rise between grips is 28.5 cm (the leap allows 70 up, 130 down).
- Grips of different chains are at least 320 cm apart, so a chain never steals into another: the
  forks are standing breaks.

## Moves (summary; details in BRACHIATION.md)

- **Catch:** walk out along a limb and jump under the first grip.
- **Along a chain:** Space, toward the stick, or straight on with it released; the branch gap is an
  ordinary 180 cm leap.
- **Step down at a chain's end:** Space with the stick released, at the last grip, over a limb: a
  small hop down onto the limb (new, `CHUCK_SWING_STEP_DOWN`). Where there is nothing below within
  170 cm (the lamp alleys), the old behaviour stands: back along the chain.
- **Let go:** C, anywhere; over open ground it is a long fall with the landing roll, no death.

## Verification (counts as of the spur follow-up)

- Every Chult run (`-ChuckChultTest`) logs `CHUCK_CHULT_CANOPY`, `CHUCK_CHULT_CANOPY_LAYOUT` and the
  static `CHUCK_CHULT_CANOPY_CHECK`: all 71 grips (38 chain, 33 spur) registered and clear to hang
  from; every one of the 58 leaps clear by the same line trace and capsule sweep `FindSwingTarget` uses; bark 98 cm under
  every end grip; walkable, unobstructed limb tops; standable forks; open liana feet, climb paths
  and lips.
- `-ChuckCanopyTest` (also run by `Verify-Package`) drives the real character through 27 trials:
  liana up; catch; all four chains in a loop with step-down dismounts and crotch crossings; out and
  back with the stick held back; let go over open ground; liana down; ruin slab to the first catch;
  and each of the nine spurs from its crotch out to the tip, back and down onto its limb.
  `-ChuckCanopyCapture` adds review stills and side views in `Saved/Screenshots/.../Canopy/`.

## Not done

No crown fading (foliage is kept off the routes instead; camera comfort not assessed by a person).
No raptors, buff-only teaching tree or golden pickup. The trees are procedural: bark is the shared
`M_FallBark`, leaves the shared card; not at the art target. Physical Xbox play untested.
