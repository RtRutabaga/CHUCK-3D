# Tavern basement pantry foundation

October 9 suspended island adjustment: the long cylindrical stone support beneath the cheese crate is removed. The existing walkable cellar floor now hangs over a short broken masonry foundation and a smaller jagged earth layer, ending roughly 85 cm below the floor with open sky underneath. The underside is decorative and noncolliding; the floor, crate and cheese retain their collision. The isolated pantry check additionally verifies a supported island floor and clear space underneath.

October 9 adjustment: the four Astral holes now have shaped upright oil-slick rims instead of flat haze sheets. The outer sky opening and the central island rim use the same fading vertical material, following their jagged perimeters. These six rims have no collision; the existing floor holes, sky/cloud well and lights remain. The cheese's body and top piece now block movement. NPC talk selection rejects height differences over 100 cm, preventing keeper dialogue through the cellar ceiling while retaining normal same-floor reach. `-ChuckPantryAdjustmentsTest` checks the six noncolliding rims, a pawn-channel sweep against the cheese, blocked cellar prompt/interaction and preserved upstairs keeper dialogue. Default Verify-Package includes this isolated check. See the latest HANDOFF for actual package evidence.

User authorized this connected basement on October 3; Claude will implement ladder climbing separately. The tavern still opens after the sewer-slide return. Reach the working aisle around the east end of the bar; the open hatch is behind its right half. Drop through for now, then use R / controller View to return to docks. No new interaction prompt, ladder action or item system.

A roughly 5.5 x 5m usable stone cellar beneath the existing tavern, floor z=-320cm, enclosing collision walls/ceiling, old masonry courses, flagstones, timber ceiling beams, stocked timber racks, jars/sacks, barrels and a crate. Two warm lamps reuse the animated fire system. Major furnishings block movement; small shelf stock remains decorative. Open centre and landing aisles stay rat-passable. Geometry/materials are provisional, below the reference art target.

`DockPantry.cpp/.h` owns the cellar, fixtures and review hooks. DockSetting splits the tavern structural floor and court/north-street foundations; DockTavern clips decorative planks; DockGameMode cuts the noncolliding sea sheet at the opening. This avoids invisible ground or water blocking/covering the shaft. A handful of submerged vista seabed triangles beneath the room are omitted so the nonplayable scenery cannot render through the cellar; visible coastal land is unchanged. A bounded `IsWithinDockPantry` exception in ChuckCharacter's existing below-world check lets Chuck remain here. It does not mark the cellar as sewer or change respawn/climbing/animation systems. Death/reset uses dock spawn; night music continues if the sewer was completed.

## Ladder contract for Claude

- Clear shaft bounds: world x20..130, y875..955cm (110 x 80cm), centre (75,915). Tavern floor z=0, ceiling underside=-110, cellar floor=-320.
- Ladder placeholder rails: x124, y891/939, z=-315..15. Rungs every28cm, in Y direction; ladder faces toward -X. Rails/rungs have no collision or climb logic yet. Upright hatch lid sits on the west edge; landing beneath the centre is clear.
- Recommended lower alignment near (90,915,-285.35), upper dismount near (175,915,34.65), facing +X while climbing. Confirm capsule and animation clearance when implementing; these are design targets, not tested ladder poses.
- Keep controller/rig ownership with Claude. There is no temporary bespoke climbing mechanic. R is the current way out. Keep the single root launcher; rebuild, verify and promote after ladder integration.

## Checks

`-ChuckPantryCapture` takes four fixed views (bar hatch, shaft, cellar and provisions). `-ChuckPantryTest` exercises a real drop from the hatch, two walking circuits with camera switching, checks that the cellar is not a sewer respawn, and resets to docks. Standard package verification requires six floor samples, five capsule routes, one shaft sweep and four boundary traces. These checks do not establish ladder play, subjective camera comfort, physical Xbox operation, music mixing or production art. Latest HANDOFF records actual results.

No new dependencies, asset downloads, binary imports or original-game changes. Existing Unreal 5.7.4 and materials/props are reused; generated output remains excluded from Git.

## October 3 (Claude, user request): ladder, broken floor, cheese

- The cellar is now 8 x 7 m (x-450..350, y400..1100). Its floor is built from 10 cm strips with real holes: four Astral ruptures (the sewer's depth material in a well below each, its oil haze, purple light, broken flagstone chips), and a wide jagged hole onto open sky (`M_PantrySky`, drifting cloud puffs `M_PantryCloud`, daylight coming up) round a masonry island with a crate and the cheese (`M_Cheese`). The sky ring is 2.13 m wide: beyond Chuck's longest jump with enough margin that he can't even catch the island's edge. `Tools/create_pantry_materials.py` makes the three materials.
- As in the 2D pantry: three stocked racks whose lowest-shelf jars are breakable clay jars (`AClayJar`, 1-2 cigarettes each), and four floor jars; barrels, sacks and a work crate are solid dressing.
- The ladder is a `ChuckClimbable` (Foot (103.5,915,-320), lip (130,915,0), faces +X). Chuck takes hold by walking into its foot, or by walking toward the drop at its top from the tavern floor (he lowers himself on with the pull-up played backward); stick toward the ladder climbs, away descends; the pull-up takes him out over the top; jump kicks off. Ropes and the like can register the same way.
- Falls: any off-map fall in the game is now an Astral death (dark at once, summoned back at the area start). Down here the area start is by the ladder foot (55,835); `IsWithinDockPantry` reaches 380 cm under the floor so a fall through a hole is seen first.
- Checks: `CHUCK_PANTRY_CHECK` adds four open-hole traces; `-ChuckPantryLadderTest` (smoke stages 118-124) climbs up and out and back down, falls into a rupture, and takes a full running jump at the cheese.
