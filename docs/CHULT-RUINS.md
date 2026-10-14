# Chult grove ruins

User requested parkourable stone ruins by the tree parkour area on October 13,
2026. The boundary-cliff request was explicitly deferred. This is a setting
addition within the approved Chult landing/grove, not the later campaign temple.
Phase 4's stone/terrain exploration and Phase 5's weathered masonry/overgrowth
inform its art; original documents were read with absolute paths, read only.

All coordinates below are local centimetres from `AChultLanding::FloorOrigin()`.
Reserve X=4900–7600, Y=-2600–350 for masonry; keep random trees/palms out, clear
vegetation from the stone footprints and preserve the existing recovery lanes.

- Approach from the grove's southern side into an open courtyard. Three 30 cm
  mantle steps at Y=-2200 lead to a broad terrace (X=5300–5900), top Z=90.
- A second terrace at X=6000–6400, also Z=90, leaves a 100 cm running-jump gap.
  A third separated platform and rising broken steps offer optional exploration.
- Two stairwell walls at X=5380–5540 and X=5660–5820, Y=-1250–-750, leave a
  120 cm climbing gap. Cornice slabs at Z=650 overhang inward by 30 cm, leaving
  60 cm overhead clearance between the caps. Existing wall jumps/ledge pull-ups
  reach the standing tops. No golden cigarette is required.
- A 220 cm wide elevated causeway continues north from the western stairwell
  cap, at Z=650. Its wider end slab overlaps the southern outside of fork 0.
  A landing at (5530,820), size 360 x 360, top Z=670, rests on the trunk
  tops: a 20 cm mantle gives a firm approach across the rounded standing limb.
  Follow its western side at X=5460 to Y=900, then turn east into the fork;
  this stays clear of the south trunk's leaning continuation.
  The paired trees, inner climb faces, branches and grip proposals are unchanged.
  Support piers keep the grove's existing ground approach clear.
- A broken gateway, incomplete walls, varied ashlar corners, rounded/eroded
  edges and heavier moss provide a recognisable ruin rather than a bare
  obstacle course. Stone is human scale; Chuck's 65 cm scale makes it substantial.

`ChultRuins.cpp` owns seeded procedural masonry, visible mortar backings and
collision using the same rendered geometry. Structural stone/caps collide;
the finish uses the dedicated scanned gray stone/moss materials below. No software installs, Blender regeneration, character/controller edits or new active swing grips are needed.
The cutscene and playable setting share the addition through `BuildSet()`.

Codex builds and checks the setting, including floor/top traces, capsule
clearance and actual existing mantle, running jump, wall climb/pull-up and
causeway-walk trials. The automated driver uses existing public test input
hooks; it never changes movement tuning. Review cooked overview and player-height
captures, run package regressions, promote through the root launcher and record
actual evidence in HANDOFF. Full freeform camera comfort and physical Xbox
remain manual playtest work; Claude owns any subsequent traversal/camera fixes
and activation of the canopy's proposed brachiation grips.

Further setting refinement can be done here; Claude owns subsequent traversal/camera changes.

## October 13 — gray mossy ruin finish (user-approved)

The user approved Rock Wall 08 as the gray base, Mossy Stone Wall for heavier
moss/aged joints, and Rabdentse Ruins Wall for worn dark stone, with rounded
stone geometry. This updates the existing grove ruins only; cliffs stay deferred.

Material contract: the existing two-section procedural mesh keeps section 0 for
structural gray stone and section 1 for mossier stone/recessed backing. New
isolated materials are `/Game/Art/ChultRuins/M_RuinStone` and
`/Game/Art/ChultRuins/M_RuinMoss`; their nine verified 2K scan maps live in the
same folder. Existing shared stone, rock, jungle and character materials are
not rebuilt. Maps are Poly Haven CC0, with original URLs and provider MD5 plus
local SHA-256 in `SourceAssets/Setting/ChultRuins/manifest.json`.

Blocks get rounded corners and curved upper/lower edge bands, with seeded
variation in corner wear. Preserve nominal extents, standing heights, mantle
steps, 100 cm terrace gap, 120 cm stairwell and the fork stone landing. The
rendered surfaces continue to supply collision. Flat centres remain usable for
standing/climbing. No shader displacement or decorative hidden collision.
Gray stone and moss use world-aligned mapping; moss is heavier in joints,
damp lower areas and irregular surface patches, with brighter moss on ledges.
All five real-character ruin trials and the ground/top checks must pass after
the shape change. Review cooked views and promote a verified candidate through
the root launcher; record actual results, not planned checks.
