# Northeast Chult canyon and broken bridge

User authorized October 14, 2026. Read-only Phase 4/5 and jungle map reviewed:
continue their vegetation, terrain, exploration and onward-route language. The
broken bridge/canyon are this user's 3D adaptation; no 2D dialogue or new NPCs,
ship/temple progression, controls or pickups are added.

Coordinates below are centimetres relative to `AChultLanding::FloorOrigin()`.
The canyon frame has origin **(24500,15500,0)**, forward **(.8,.6,0)** and across
**(-.6,.8,0)**. The new footprint is S=-6500–11500, T=-4500–4500: an 180 × 90 m
extension overlapping the old northeast edge. Emit new terrain only outside the
existing floor. Approach follows the old stream's far end to the near bank.
The eight canopy trees, 127 grips, ruins and raptor remain. The later user-authorized watercourse pass now continues the creek through a side drainage notch at T=-1800 to a narrow canyon waterfall; it preserves bridge coordinates and the T=0 approach. See [CHULT-WATERCOURSE.md](CHULT-WATERCOURSE.md).

Near lip S=0 and far lip S=4500: **45 m** of unsupported crossing, with banks
around Z=350 and a roughly **50 m** cleft below. The cleft spans the entire new
footprint across T; there is no solid connection around its ends or underneath
the bridge. Cliff collision follows its visible surface. Existing fall recovery
triggers below world Z=-100; the deep bottom is scenery beyond that threshold.
No new death controller or checkpoint is needed for the setting pass.

## Bridge hand-position contract for Claude

`ChultCanyon.cpp/.h` owns **33** surviving planks and `ChultBridgePlanks()`.
Their names are `ChultBridgePlank00` through `ChultBridgePlank32`. Each record
has a local `Grip`, width and tilt; matching scene markers have the
`ChultBridgePawProposal` tag. `ChultCanyonFrame(S,T,Z)` converts frame to local;
add FloorOrigin to obtain world space. Along is (.8,.6,0).

Plank hand points run S=-100 to 4540, 140 cm apart with four 180 cm breaks.
Z follows 460 minus a 90 cm sine sag; bank standing ground is Z=350. Planks hang
steeply (68–82°) from their upper edge. The hand proposal meets that actual edge,
with splintered ends and old rope lashings. Side ropes sag too, with frayed broken
ends. They have no grips. Planks/ropes are noncolliding handhold art, following
the existing canopy hand-bough convention; they supply no standing stepping
stones, so jumping along a deck is unavailable. Bank stubs/posts are solid.

Safe standing entry S=-280,T=0 and exit S=4820,T=0. No grips are registered by
this pass. The current player can reach/inspect the near bank, but **the bridge
cannot yet be crossed**. Far-bank dressing is setting-ready, not a completed
route. Claude owns activating the hand proposals, both directions, catches,
step spacing/selection, paw pose, collision/body clearance, camera, missing-board
leaps, dismount and death/recovery tests. Change the setting data through this
contract if the real traversal needs adjustments; do not conceal a walking deck
under the bridge. Only a verified actual crossing can establish 'brachiation is
the only way through'.

Activation also needs an inventory/test audit: the current package verifier
expects the canopy's 127 registered grips and inactive-canyon markers. Keep all
existing canopy identities/behavior, update total-versus-canopy inventory checks
deliberately, and replace the canyon's `active_crossing=0` /
`bridge_crossing_not_implemented=1` acceptance with actual crossing tests.
Do not treat these setting-only checks as proof that the player can swing across.

## Art and verification

Reuse existing gray mossy ruin rock, wet ground, bark/wood, scanned island trees,
palms, ferns, elephant ears and shrubs. Dense bank colonies, hanging vines and
low mist frame the canyon. No new assets/imports/dependencies or Blender rebuild.

Setting tests require ground approach/exit capsule clearance, bank/lip floors,
no floor below all bridge/canyon samples above the death threshold, visible
cliff collision and steep normals, 33 real-edge paw proposals with safe spacings
and body capsules, and actual approach/far-bank walks and fall recovery. Capture
wide canyon, bank/bridge, broken plank and deep gorge views. Run all existing
Chult/canopy/raptor/fall/package regressions. Handoff records actual results,
launcher state and unimplemented Claude traversal explicitly.

Next part of the work can only be done by Claude: activate/test the broken-plank
brachiation route with the user.
