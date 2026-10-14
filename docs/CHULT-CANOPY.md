# Chult canopy grove — traversal and setting contract

**Status (2026-10-13, Claude):** active brachiation. The user asked Claude to "make this work" on Codex's
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

| Tree | Trunk axis (local) | Crotch / limb top Z | Limbs toward |
| --- | --- | --- | --- |
| 0 | (5560,1230) | 650 | tree 1, tree 3, the ruin slab (top 645, under the stone) |
| 1 | (7250,760) | 650 | tree 2, tree 0 |
| 2 | (8350,1800) | 760 | tree 3, tree 1 |
| 3 | (6750,2600) | 650 | tree 0, tree 2 |

Each tree (`ChultCanopyTrees()`):

- **Trunk:** one fluted, flared, slightly leaning trunk, about 3.5 m wide at the ground and 1.7 m
  at the crotch, with plank buttresses (concave-topped fins running out 2.7–3.9 m and sinking into
  the ground) and low snaking surface roots. No buttress or root crosses a ground lane or the
  liana's foot.
- **Crotch:** the trunk's top is a shallow standing bowl with a low saddle toward the front;
  `ChultCanopyFork<I>` (tag `ChultCanopyStanding`) marks a standing point in it.
- **Walking limbs:** one straight at each neighbour, 62 cm radius at the trunk, 34 cm at the first
  grip (300 cm out), level to there, then a slight droop. Past 430 cm each bends away from under the
  grips and tapers up into a leafy end. Their tops are walkable bark (checked every 30 cm).
- **Leaders and crown:** two leaders rise from the back of the crotch (the widest gap between
  limbs) to about 9 m above it, with five spreading boughs each and leaf clusters (the existing
  `SM_FallLeafCard`, about 7,100 instances in four `ChultCanopyCrown<I>` components, tag
  `ChultCanopyFoliage`, no collision). Leaves are kept out of the swing lanes, off the walking limbs
  and clear of the crotches.
- **Hand boughs:** from the nearer leader, each arcs over its walking limb well above head height
  and runs out along the chain, thinning from 12 to 6 cm radius toward the gap, with small twigs and
  leaves above the hands' line. Noncolliding, like the lamp rings.

Collision: trunk, saddle, walking limbs, leaders, buttresses and roots are one visible
procedural mesh (`ChultCanopySolidBark`, BlockAll, complex as simple). Hand boughs, crown boughs,
twigs (`ChultCanopyHandBranches`) and lianas (`ChultCanopyVines`) have none.

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

## Verification

- Every Chult run (`-ChuckChultTest`) logs `CHUCK_CHULT_CANOPY`, `CHUCK_CHULT_CANOPY_LAYOUT` and the
  static `CHUCK_CHULT_CANOPY_CHECK`: every grip registered and clear to hang from; every one of the
  34 leaps clear by the same line trace and capsule sweep `FindSwingTarget` uses; bark 98 cm under
  every end grip; walkable, unobstructed limb tops; standable forks; open liana feet, climb paths
  and lips.
- `-ChuckCanopyTest` (also run by `Verify-Package`) drives the real character through 18 trials:
  liana up; catch; all four chains in a loop with step-down dismounts and crotch crossings; out and
  back with the stick held back; let go over open ground; liana down; ruin slab to the first catch.
  `-ChuckCanopyCapture` adds review stills and side views in `Saved/Screenshots/.../Canopy/`.

## Not done

No crown fading (foliage is kept off the routes instead; camera comfort not assessed by a person).
No raptors, buff-only teaching tree or golden pickup. The trees are procedural: bark is the shared
`M_FallBark`, leaves the shared card; not at the art target. Physical Xbox play untested.
