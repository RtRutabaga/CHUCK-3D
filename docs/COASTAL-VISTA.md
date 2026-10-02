# Coastal vista

The October 1 request expands only inaccessible scenery. The playable docks, plaza, Dock Street and their collision boundaries remain the same. The view should read as one dockside district of a larger coastal city, not a literal reconstruction of Waterdeep.

`DockVista.cpp` builds continuous low-detail terrain around the west, north and east of the harbor. A northern strip joins the main town to the opposite waterfront; the sea remains open to the south. Roof districts extend inland, with four taller civic silhouettes. Terrain fades from coastal rock/soil into muted green hills and ridges. The sea and sky envelope are enlarged so their old edges do not cut off the backdrop. Existing dawn lighting is retained.

All new components have collision disabled. The terrain uses 23,254 vertices, vertex colors and a single rough material; city silhouettes share instanced batches with simple dark windows/timber bands and do not cast shadows. Sparse cone silhouettes suggest woodland beyond the city. The build enables Unreal 5.7's bundled ProceduralMeshComponent plugin; no engine installation or download is needed. `Tools/create_vista_material.py` creates only `M_VistaTerrain` and `M_VistaFoliage` (small LFS materials). Existing art assets are reused. Remote LFS allowance is unknown; no storage was purchased.

`-ChuckVistaCapture` records west, north, east, sea and northern-harbor views from above the highest Dock Street roof, then exits. Captures are under `Saved/Screenshots/Windows/Vista`. This is a review camera, not a change to the player's camera or a teleport control. Actual build and testing evidence belongs in HANDOFF.md.

Limits: terrain and distant buildings are deliberately simple, buildings repeat, no distant NPC simulation/interiors, and the country is scenery. Visual acceptance requires inspecting the packaged roof views; compilation alone does not establish a good skyline.
