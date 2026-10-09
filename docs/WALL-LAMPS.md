# High wall lanterns beside the large pier

Twelve iron bracket lanterns sit on the opposed side walls of the four tall pitched-roof rear-row Dock Street houses between the sewer entrance and the large northern pier. This is the rear row centred at Y2830, not the timber workshops by the starting wharf or the small dock by the tavern. The earlier workshop placements were removed before launcher promotion after the user clarified the location.

Each of the three 120 cm alleys has two lanterns per wall, 220 cm apart, staggered 24 cm along the alley and 20 cm vertically across it. Arm tops are 530/590 cm on the western wall and 550/610 cm on the eastern wall, beneath the existing eaves and clear of the shuttered windows. These require climbing from the paving, beyond ordinary ground-jump reach. Dark iron mounting plates, rivets, diagonal braces and curled forge work support open lantern cages with rain caps, vent necks and warm animated flames. Existing materials and fire/flicker code are reused. No downloads, installations or binary assets.

The existing buildings, roof routes and controller are retained. Lanterns have no collision and cannot currently be grabbed or swung from. Brachiation is Claude's later work; no final brachiation reach or motion test is claimed.

Future grip contract: scene components on the `DockSetting` actor tagged `DockLampGrip`. House numbers 1–4 run west to east (centres X -1450, -770, -90, 590). Names are `WallLampGrip_Rear{house}_East_{index}` or `_West_{index}`. Only opposed walls receive grips: East on houses 1–3, West on houses 2–4. Index is 0 or 1. Centimetres, Z up:

| Lane | West-side grip X | East-side grip X | Grip Y / Z, index 0 | Grip Y / Z, index 1 |
| --- | ---: | ---: | --- | --- |
| Houses 1–2 | -1124 | -1096 | west 2720/530; east 2744/550 | west 2940/590; east 2964/610 |
| Houses 2–3 | -444 | -416 | west 2720/530; east 2744/550 | west 2940/590; east 2964/610 |
| Houses 3–4 | 236 | 264 | west 2720/530; east 2744/550 | west 2940/590; east 2964/610 |

Grip points are on the horizontal arm, separate from the hot hanging cage. Outward normals are +X on East facades and -X on West facades. Claude should evaluate hand clearance, offsets, transitions and spacing with the actual feature. No physics or standing ledge is added.

`-ChuckWallLampCapture` captures a rat-height alley approach, elevated alley view and lantern close-up under `Saved/Screenshots/Windows/WallLamps`. Normal cameras are unchanged. `CHUCK_WALLLAMPS_CHECK` checks twelve real wall mounts and unobstructed high arm tips; the default package verifier requires it alongside existing route/controller gates. See HANDOFF for actual build, visual and regression evidence.
