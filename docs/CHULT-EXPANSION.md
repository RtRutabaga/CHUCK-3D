# Northeast Chult jungle extension

User authorized October 14, 2026: make the map larger in the direction of Claude's canopy line, fill it with the starting Chult vegetation, mild elevation changes and a small flowing stream.

`ChultExpansion.cpp/.h` owns the extension setting. Coordinates are centimetres relative to `AChultLanding::FloorOrigin()`. The added footprint runs X=7000–22000, Y=2000–15500, overlapping the existing floor and continuing beyond tree 7 at (11700,6600). New solid floor lies beyond the old X/Y=13000 edges; no boundary cliffs are introduced.

Reach it from the ruins/tree grove, through tree 2 onto Claude's staggered line (trees 4–7), then descend a liana or continue northeast on the ground. The stream winds along the north side of that line and farther into the new forest. Ground lanes return to the grove; no new checkpoint/menu destination is needed.

The old forest inside its 90 m radius stays flat. The canopy's rectangle and line corridor, with an extra 350 cm margin, also stay flat, fading into relief over another 600 cm. Claude's trees, roots, liana feet, grips, spurs, tests and ruin respawn coordinates remain unchanged. Terrain elsewhere rises gently, about two metres at the largest low hill, with a shallow graded stream bed and walkable banks. Terrain collision follows the rendered mesh; flat protected patches use the existing floor.

The stream winds from the far northeast back southwest through the expanded forest. It is a shallow creek, roughly 3 m across, with approximately 14 cm between the water and bed. Its water elevation descends downstream, and a dedicated material scrolls ripples in that direction. Water is visual and noncolliding; existing walking remains available through the bed. No swimming, current force, damage, new controls, Niagara fluid simulation or water-system dependency is added. Open ground loops cross it at shallow reaches and return toward the canopy grove.

Reuse the existing Chult trees, palms, four scanned fern variants, elephant ears, shrubs, moss and six mossy rock variants. Random trunks/roots and colliding rocks clear the authored walking lanes, stream and canopy reserve; noncolliding foliage leaves liana feet and grip corridors readable. Existing Blender sources and imported meshes are not regenerated. The stream material is the only new Unreal asset and uses the already present water normal texture. Reproduce it with `Tools/create_chult_stream_material.py` through Unreal's Python commandlet (null RHI); the script edits only `/Game/Art/ChultTerrain/M_ChultStream`. The asset is 10,062 bytes, covered by existing uasset LFS rules and the established GitHub remote; account quota is not exposed. No new downloads or installations.

Verification covers floor extent, real-capsule route sweeps following the terrain, bank slopes, stream/bed alignment, protected canopy feet, material/vegetation inventory, cooked views and short actual character walks over relief and a ford. Run the existing canopy/ruin/fall/package regressions before promotion. Record actual evidence and remaining flaws in HANDOFF; these are requirements, not passed results.

The forest and terrain remain a setting pass below the supplied art target. Human camera/play-feel, Xbox, stream audio and larger-region encounter pacing remain separate follow-ups.

Next part of the work can be done here: refine the expanded setting after a play review. Claude retains traversal/camera ownership.
