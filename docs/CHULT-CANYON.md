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

## Bridge (Claude, October 14, 2026)

The bridge is no longer part of this file's contract: Claude rebuilt it as a
horizontal-plank rope bridge crossed by brachiation and owns it in
`ChultBridge.h/.cpp` (see [CHULT-BRIDGE.md](CHULT-BRIDGE.md)). Codex's 33 steep
plank proposals, `active_crossing=0` and `bridge_crossing_not_implemented=1`
markers are gone. Posts stay at S=-240/4800; the T=0 approach and exit, banks
and lips are unchanged. A fall into the gorge now returns him beside the bridge.

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

The broken-plank crossing is active and tested (CHULT-BRIDGE.md).

> **2026-10-15 (Claude):** the gorge walls, the plunge of this water into the gorge and the gorge mist are now [CHULT-GORGE.md](CHULT-GORGE.md). Floors, bridge and checks of the banks are unchanged.
