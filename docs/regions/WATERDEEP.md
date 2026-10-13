# Waterdeep — 3D region summary

The first region, built as the prototype from 2026-09-24 to 2026-10-12. It adapts 2D Phase 2 (Waterdeep starting area and sewer tutorial) and most of Phase 3 (tavern and pantry). This page summarises what exists now. The dated, per-feature design and verification notes are in [../archive/](../archive/); the most useful are listed at the end.

## Layout

One map, `/Game/Maps/WaterdeepDocks`. Geometry is generated at play time by `ADockGameMode` and the `Dock*` setting classes; the editor map itself is empty.

- **Starting quay:** Bobert's barrel, where Chuck starts, plus the tavern frontage, the pier with a missing board, and the parkour practice yard (a cargo chimney, the harbour wall and a mooring plinth).
- **Cargo wharf, Chandlers' Row, Timber Yard:** the main parkour courses. They include a crate staircase, the warehouse and sail-loft alley, stepped row-house roofs, market stalls, the garden wall, the customs terrace, lumber stacks and the crane tower.
- **Dock Street loop, cargo court, service quay:** connected walkable streets and buildings, with weathered timber shops, trade signs and wall lamps.
- **Lamp alleys:** lantern rings for brachiation and chimney climbs between tall rear-row houses ([../BRACHIATION.md](../BRACHIATION.md)).
- **Fountain plaza:** reached through the market, behind the city walls. It has the fountain, shops, the smithy with a working blacksmith and forge, a closed city gate and guards.
- **Coastal vista:** a non-playable harbour city, roofs and countryside visible from high roofs.
- **Sewer:** entered by falling through the open grate beside the Dock Street side gate. It is a long winding natural-rock tunnel with a shallow stream, a wider midway chamber, purple Astral Sea floor ruptures (genuine falls that respawn Chuck at the tunnel entrance), a wall-run rupture, a sprint-leap gap, sewer rats and a water-slide exit to the pier.
- **Tavern:** a walk-in room with a bar, tables, hearth and keeper. It is closed in the morning and open in the evening.
- **Pantry:** a cellar entered by ladder through a hatch behind the bar. Its open sky hole currently ends on an "End of demo" screen, standing in for the Phase 3 fall to Chult.

## States

- **Morning:** the fresh start. The tavern is closed and the sewer grate is open.
- **Early evening:** reached after the sewer slide. The grate is closed, the tavern is open and the night music plays. This lasts for the session only; there is no save.

## People and creatures

- **Bobert:** asleep in his barrel. He is noninteractive and never named.
- **Human NPCs:** procedurally posed and 180 cm tall, built in Blender. They are the dock worker, a sailor, a market woman, the blacksmith, three town guards, the tavern keeper, a dwarf, an elf elder and a gnome alchemist. Some speak lines with recorded voices and subtitles ([../NPC-VOICE-PLAN.md](../NPC-VOICE-PLAN.md)). All lines come from the 2D game or the user.
- **Enemies:** dock rats on the surface and rats in the sewer. The zombie assets remain, but the sewer zombie was removed on October 4.

## Systems built here and reused by later regions

- **Movement and camera:** the v1 Chuck character, groom and authored clips. Full traversal: run, sprint and sprint leap, strafe and side jump, wall run, wall jump, ledge hang, shimmy and pull-up, drop to hang, mantle, landing roll, ladders and brachiation, plus climbing stamina. One continuous orbit camera.
- **Interaction and combat:** the slash and low rake, grass tufts and clay jars that drop cigarettes, Sanity as a cigarette bar, and the astral vanish and summon respawn.
- **Presentation:** talk (F / Y) with subtitles, first-time tutorial prompts, the title menu with dev checkpoints, music and ambience per area, and fire, lamp and water effects.

Controls and build steps: [../PLAYING.md](../PLAYING.md).

## Known gaps

- The supplied character and world art target has not been reached.
- Physical Xbox play has not been verified.
- The fall to Chult is a placeholder end screen.
- There is no persistent save.

## Detailed historical notes (archive)

DOCKS-SETTING, WATERDEEP-PLAZA, COASTAL-VISTA, SEWER-PROTOTYPE, SEWER-WALLRIFT, SEWER-SPRINT-RIFT, TAVERN-INTERIOR, TAVERN-PANTRY, DOCKS-RETURN, FORGE-DRESSING, FIRE-PASS, WALL-LAMPS, TRADE-SIGNS, WEATHERED-TIMBER, SURFACE-WATER, GRAPHICS-PASS, TUTORIALS, SUBTITLES, PROTOTYPE and PLAYTEST, all in [../archive/](../archive/). The full prototype handoff log is [../archive/HANDOFF-PROTOTYPE.md](../archive/HANDOFF-PROTOTYPE.md).
