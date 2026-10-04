# Tavern basement pantry foundation

User authorized this connected basement on October 3; Claude will implement ladder climbing separately. The tavern still opens after the sewer-slide return. Reach the working aisle around the east end of the bar; the open hatch is behind its right half. Drop through for now, then use R / controller View to return to docks. No new interaction prompt, ladder action or item system.

A roughly 5.5 x 5m usable stone cellar beneath the existing tavern, floor z=-320cm, enclosing collision walls/ceiling, old masonry courses, flagstones, timber ceiling beams, stocked timber racks, jars/sacks, barrels and a crate. Two warm lamps reuse the animated fire system. Major furnishings block movement; small shelf stock remains decorative. Open centre and landing aisles stay rat-passable. Geometry/materials are provisional, below the reference art target.

`DockPantry.cpp/.h` owns the cellar, fixtures and review hooks. DockSetting splits the tavern structural floor and court/north-street foundations; DockTavern clips decorative planks; DockGameMode cuts the noncolliding sea sheet at the opening. This avoids invisible ground or water blocking/covering the shaft. A handful of submerged vista seabed triangles beneath the room are omitted so the nonplayable scenery cannot render through the cellar; visible coastal land is unchanged. A bounded `IsWithinDockPantry` exception in ChuckCharacter's existing below-world check lets Chuck remain here. It does not mark the cellar as sewer or change respawn/climbing/animation systems. Death/reset uses dock spawn; night music continues if the sewer was completed.

## Ladder contract for Claude

- Clear shaft bounds: world x20..130, y875..955cm (110 x 80cm), centre (75,915). Tavern floor z=0, ceiling underside=-110, cellar floor=-320.
- Ladder placeholder rails: x124, y891/939, z=-315..15. Rungs every28cm, in Y direction; ladder faces toward -X. Rails/rungs have no collision or climb logic yet. Upright hatch lid sits on the west edge; landing beneath the centre is clear.
- Recommended lower alignment near (90,915,-285.35), upper dismount near (175,915,34.65), facing +X while climbing. Confirm capsule and animation clearance when implementing; these are design targets, not tested ladder poses.
- Keep controller/rig ownership with Claude. There is no temporary bespoke climbing mechanic. R / View is the current way out. Keep the single root launcher; rebuild, verify and promote after ladder integration.

## Checks

`-ChuckPantryCapture` takes four fixed views (bar hatch, shaft, cellar and provisions). `-ChuckPantryTest` exercises a real drop from the hatch, two walking circuits with camera switching, checks that the cellar is not a sewer respawn, and resets to docks. Standard package verification requires six floor samples, five capsule routes, one shaft sweep and four boundary traces. These checks do not establish ladder play, subjective camera comfort, physical Xbox operation, music mixing or production art. Latest HANDOFF records actual results.

No new dependencies, asset downloads, binary imports or original-game changes. Existing Unreal 5.7.4 and materials/props are reused; generated output remains excluded from Git.
