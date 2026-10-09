# Pier alley wall lanterns

Four high iron bracket lanterns dress the opposed warehouse and sail-loft walls beside the starting pier. Two per wall, 144 cm apart, staggered 24 cm along the alley. Existing buildings are short prototype workshops; the lanterns sit beneath their eaves rather than floating above the roofs. Dark iron mounting plates, rivets, diagonal braces and curled forge work support open lantern cages with rain caps, vent necks and warm animated flames. Existing materials and fire/flicker code are reused. No downloads, installations or binary assets.

The warehouse arm tops are 218 cm above paving; sail-loft arms are 244 cm. These are high wall-climb targets, beyond an ordinary ground jump. Current building shells, ledges, roof routes and controller remain unchanged. Lanterns have no collision and cannot currently be grabbed or swung from. Brachiation is Claude's later work; no brachiation reach or motion test is claimed.

Future grip contract: scene components on the `DockSetting` actor, tagged `DockLampGrip`, named `WallLampGrip_Warehouse_0`, `_1`, and `WallLampGrip_SailLoft_0`, `_1`. World coordinates (centimetres):

| Grip | X | Y | Z | Wall outward normal |
| --- | ---: | ---: | ---: | --- |
| Warehouse 0 | 6 | -852 | 218 | +X |
| Warehouse 1 | 6 | -708 | 218 | +X |
| Sail loft 0 | 34 | -828 | 244 | -X |
| Sail loft 1 | 34 | -684 | 244 | -X |

Grip points are on the horizontal arm, separate from the hot hanging cage. Do not assume these positions prove final reach or animation fit: Claude should evaluate hand clearance, offsets, transitions and spacing with the actual feature. The current 100 cm structural alley has shallow decorative cladding; lamps project 34 cm from their mounting planes. No physics/standing ledge is added.

`-ChuckWallLampCapture` captures a rat-height alley approach, elevated alley view and lantern cage close-up under `Saved/Screenshots/Windows/WallLamps`. Normal cameras are unchanged. `CHUCK_WALLLAMPS_CHECK` checks four real wall mounts and unobstructed arm tips; the default package verifier requires it alongside existing route/controller gates. See HANDOFF for actual build, visual and regression evidence.
