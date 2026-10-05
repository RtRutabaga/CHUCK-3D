# Waterdeep plaza adaptation

October 4 forge dressing replaces the small stone niche with an arched brick fire chamber and riveted hood/chimney, plus side-fed bellows, workbench/vise/tool rack and quenching tub. Existing smith/anvil positions and fuel stack stay intact. See FORGE-DRESSING.md and the latest HANDOFF for verification and limits.

October 4 fountain water pass: each basin now uses one lit translucent surface disk with animated normal detail, stone-bed refraction and four landing rings. Thin jets retain 24 traveling beads and gain 48 tiny impact splashes. The enclosing stonework/collision is unchanged. The harbor uses a separate wave material on single sheets with its two real shaft gaps preserved. See [SURFACE-WATER.md](SURFACE-WATER.md) and HANDOFF Update 69 for evidence. This supersedes the old FountainWater material on basin/jet geometry; that material remains for unrelated dressing.

October 2 update: the user's future sewer entrance is now the ground grate beside the small side gate in far Dock Street, opposite the bench (see DOCKS-SETTING.md). The plaza's existing barred arch remains drain dressing and stays inaccessible.

The user requested a loose, additive adaptation of the original top-down Waterdeep map. The original docks text map identifies district walls, torches, a sewer grate near the guard, tavern, market, piers and ruins. Its adjoining plaza has a central tiered fountain, smithy, alchemist, stands, lamps and closed gate. Reference copies and SHA-256 provenance are in References/PROVENANCE.md. No original generator was run.

## Layout and route

October 2 cleanup: the two narrow approaches now form one continuous 14 x 5.5 m apron between the existing streets and plaza. The obsolete inner parapets and enclosed water pocket are removed; the former boat is moored in open harbor water. The outer waterside kerb remains. Verification now samples 14 floors and 11 capsule segments, including crossings through the filled pocket.

October 2 follow-up: the crosswise 48 cm stone lip at the plaza approach is removed. A 4.8 m stone return with battlements and a torch joins the older western district boundary to the plaza wall, outside both approach routes. Beyond the closed city gate, a noncolliding town backdrop with supported paving, roof rows and a lane ending in buildings connects into the western city scenery. These buildings are inaccessible vista.

The existing 3D district stays intact. New land extends beyond the market/service quay, in the negative-Y direction, instead of displacing the old streets and parkour. Two approaches join it: a narrow quay path just beyond the customs terrace, and a wider continuation of the service quay beside the storehouse. Both are at the existing ground level. The extension adds 688 square metres of ground footprint, plus connecting paths; this is not net unobstructed walking area.

Follow Chandlers' Row to its far end, pass the customs terrace on its east side, and continue to the fountain. Alternatively, take the waterside route around the service-quay storehouse. The plaza has a ring route around a tiered fountain, market canopies and benches. A smithy and alchemist flank the far closed city gate; a small ruin fragment recalls the original docks. Shops remain exterior-only, with no crafting systems.

October 4 (user request): a blacksmith now works at an anvil in front of the smithy, the forge at his left hand. He is a heavy-set, gruff older man in a leather bib apron (`humans.json` Blacksmith). The anvil on its iron-hooped stump replaces the old box anvil. He hammers a glowing bar he holds with tongs, in sets of six blows with a pause to turn the work. Each blow rings and throws sparks; he gives two short lines if talked to. The anvil is solid and can be climbed. See `ADockNPC::SpawnBlacksmith`, `Tools/build_smith_props.py` and HANDOFF.

October 5 (user request): an old elf woman sits at the fountain end of the bench west of the fountain, her long grey braid down her back, watching the water. The bench is unchanged; see `ADockNPC::SpawnElfElder` and HANDOFF Update 86.

The barred sewer arch is in the eastern wall. It has solid collision backing and no interaction, destination or access yet. The city gate also stays closed. This is the same prototype map, not a campaign or sewer level.

## Light and implementation

The next detail pass adds a coping course to the fountain, thinner jets with 24 moving water beads, closed shop gables, window hoods, door boards/straps, simple anchor standards and gate hardware. A two-sided sign on the approach lamp points between the docks and fountain plaza. All new decoration is noncolliding; existing route and obstacle dimensions remain unchanged. The anchor is generic harbor dressing, not a new faction or quest symbol. This pass reuses existing materials and adds no binary assets.

An early-dawn palette replaces midday presentation: low warm directional fill, a cool ambient skylight, warm horizon, lit wall torches and street lamps. Local lights are shadowless with short radii to control cost; their color varies subtly. Older Dock Street lanterns are illuminated too. Fountain water has moving surface normals and four small modeled streams. Flame meshes and water remain prototype effects, not finished VFX.

`DockPlaza.cpp/.h` contains the new district and its checks, called after the existing setting construction. `create_plaza_materials.py` touches only three named new world materials, covered by LFS; no engine installation, purchased asset, character import or old material regeneration. Remote LFS allowance remains unknown; no paid storage was purchased.

The packaged verifier additionally requires 14 ground samples, 11 capsule-clearance segments and a sewer blocking trace. Existing world, music and Claude gameplay checks remain mandatory. These checks establish selected route continuity, not every possible climb over walls or subjective camera comfort. `-ChuckPlazaCapture` produces seven review views (overview, low fountain, sewer, gate, original spawn, wall connection and elevated town backdrop) in `Saved/Screenshots/Windows/Plaza`, then exits. Actual run evidence is recorded in HANDOFF.
