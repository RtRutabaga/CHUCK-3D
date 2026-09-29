# Connected docks setting — September 29, 2026

The user reopened setting work after Claude's traversal progress. This remains one docks map, not a campaign expansion. Claude's character, controller, camera and existing obstacle dimensions are preserved.

`DockSetting.cpp` adds a continuous west Dock Street, cargo loading court, tavern rear court and market service quay. Seven additional solid buildings have human-sized closed doors, shuttered windows, timber framing, pitched roofs and chimneys. The old tavern and warehouse frontage have building depth. Warehouse loading doors, a hoist, workshop signs, timber bands and crane bracing explain the existing parkour obstacles without adding collision to their dressing.

The new ground footprints total 513 square metres before subtracting buildings. This is not a measurement of unobstructed walking space. A street loop connects back to the old wharf, and the market extends toward a new waterside storehouse. Buildings are exterior shells, not interiors. Twenty-two nonplayable background buildings stand on continuous land behind the playable blocks; retaining faces extend into the water. Background detail remains a blockout, not a finished Waterdeep skyline.

## Explore

From the starting quay, walk toward the cargo steps and turn west through the opening beside the old warehouse. Continue along Dock Street past the loading gantry, around the cooperage at its south end, then return through Chandlers' Row. The service quay is beyond the market stalls beside the new storehouse. Walk north around the old warehouse for the tavern rear court.

Repeat in low and elevated camera framing using mouse/right-stick vertical orbit. Test the original cargo-stack chimney, crate stairs, warehouse/loft gap, row-house roof jumps and timber-yard climb to check readability with the surrounding buildings. Controls are unchanged; C / Xbox B is dodge, not a camera toggle.

## Implementation and checks

New boxes share instanced mesh batches by material and collision profile. Seven existing world materials have their instanced-mesh usage enabled by `Tools/configure_setting_materials.py`; their graphs are preserved. No new asset types, downloads or dependencies. The seven updated LFS assets total about 191 KB; remote account allowance remains unknown and no paid storage was purchased. `BuildDockSetting` is the setting entry point; `DockGameMode` calls it during scene construction. Keep character/traversal work with Claude.

`Verify-Package.ps1` requires both the existing gameplay suite and `CHUCK_WORLD_CHECK_COMPLETE failures=0`. The world checks sample nine ground points and six rat-capsule route segments. These check floor continuity and clearance, not subjective camera comfort or every new roof. `-ChuckSettingCapture` runs a separate four-view review and exits; it does not change normal play. Images go to `Saved/Screenshots/Windows/Setting` within the package.

Known scope limits: repeated blockout architecture, closed doors, no interiors or NPC activity, and a simple opaque sea. The new buildings need art refinement and player feedback on routes and camera obstruction. Passing collision checks does not establish finished graphics or natural city scale.
