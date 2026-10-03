# Tavern interior foundation

The user authorized tavern-interior work on October 3. This supersedes earlier exterior-only descriptions. The enlarged 6.6 x 6.6 m building now has a structural floor and enclosing walls around a 1.2 m nominal front doorway. The timber leaf stands open inward; walk through directly, with no loading screen or interaction prompt.

The first room uses worn timber planks and wainscot against aging plaster, exposed roof timbers, three human-sized tables with benches, two bar stools, a serving counter, stocked shelves, reused barrels and a small stone hearth with its flue extending through the roof. Warm lamps and the hearth provide interior light while the docks score continues. Cups, bottles, static flame shapes and rafters are decorative. Walls/gables, pitched roof, floor, furniture legs/tops, bar, barrels, hearth masonry and chimney cap have collision. Furnishings retain rat-scale passages; tables and counter use the existing traversal controller.

The clear route runs from the doorway up the centre aisle, around the east end of the bar toward the hearth, then returns through the door. No bartender, tavern dialogue, buying/selling, upstairs rooms or new character actions are included in this first pass. Graphics and flame shapes remain prototype geometry.

`DockTavern.cpp/.h` owns room dressing, lighting and review/check hooks. The hollow shell is in DockSetting; the open frontage and leaf are in DockGameMode. Existing assets/materials are reused; no installs, imported binaries or changes to the original CHUCK game.

`-ChuckTavernInteriorCapture` records four fixed interior views. `-ChuckTavernInteriorTest` walks a real character into the room, around the bar/hearth and back out, repeating with the other camera framing; it also records `TavernPlay/Low.png` and `High.png` with the live player camera. Standard package verification requires doorway/aisle capsule clearance, floor traces and table/bar/chimney-cap collision checks. These automated checks do not establish subjective camera comfort, physical Xbox operation, or finished art quality. See HANDOFF for actual results and launcher status.
