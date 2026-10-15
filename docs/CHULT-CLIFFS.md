# Chult jungle cliffs

User authorized partial western/southern cliffs October 14, 2026, then supplied two jungle references and requested varied geology, vegetation right to the rock and connected source/canyon waterfalls. This supersedes the original smooth-wall setting pass. References: `References/ArtDirection/Chult/JungleCliffs-*-User-*.jpg` (user-supplied reference screenshots, not game assets).

`ChultCliffs.cpp/.h` owns the partial escarpment at bearings 100–325 degrees. It now uses varying heights/depths, stepped recesses and 72 overlapping eroded rock shoulders rather than a single leaning sheet. Low visible rock remains undercut to block current climbing; upper strata and shelves vary independently. All solid rock uses the same visible procedural triangles for collision. The adjoining jungle includes 3,852 added fern/broad-leaf/creeper instances, 85 scanned trees at feet/crests, and 81 embedded scanned rocks, with matched visible-trunk collision on reachable trees. Dense ground growth spans the old forest edge to rock. No invisible boundary wall or leaf-card collider is added. The northeast bridge/expansion corridor stays open.

`ChultLandform.h` supplies closed, irregular rock forms and clockwise UE front faces for this setting and the watercourse. Dedicated `/Game/Art/ChultTerrain/M_ChultCliff` samples an intact fissured patch of the existing natural `rock_moss_set_01` boulder atlas with mirrored world-space projection, normal detail and broad moss/colour variation. It contains no cut-stone/masonry texture blend. Existing shared materials, imported meshes and Blender sources remain untouched. Reproduce only the two new landscape graphs with `Tools/create_chult_landscape_materials.py`; no imports/dependencies/downloads.

This remains procedural scenery below the supplied references, with inherited tree/leaf limitations. Cliff checks now distinguish low climb-refusing faces from varied upper geology, probe at heights relative to each crest, sweep the actual capsule into the boundary, and exercise walking/jumping/locked-stamina contact. Actual results and image review belong in HANDOFF. Human play/camera comfort and Xbox are separate from automated tests.

Watercourse layout and fall/terrain continuity: [CHULT-WATERCOURSE.md](CHULT-WATERCOURSE.md).

Next part of the work can be done here.
