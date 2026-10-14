# Chult boundary cliffs

Authorized by the user October 14, 2026; supersedes the previous cliff deferral.

`ChultCliffs.cpp/.h` adds the western/southern treeline escarpment to the existing landing set (and therefore the fall cutscene's landing). Local coordinates use `AChultLanding::FloorOrigin()`. The partial arc runs from 100 to 325 degrees, about 102 m from the landing, behind the old 90 m forest. The rock rises roughly 23–37 m with an uneven crest, deep irregular vertical folds and shallow horizontal weathering. Its continuous undercut is intentional: the actual visible collision face is outside the existing head-on climb/mantle normal limits. No invisible blocker or character/traversal change is used. There are no intermediate standing ledges or grips. The ends remain partial boundaries; this is not a complete enclosure of all current floor edges.

Bearings 325 through 0 to 100 degrees remain open. In particular there are no cliffs around the northeast extension beyond the eight brachiation trees and shallow stream. Existing ground, stream, canopy and ruin placements stay intact. Later expansion still needs its own region-plan approval; no new destination or transition is added here.

The closed procedural rock surface reuses `M_RuinStone`, with the existing triplanar scanned gray rock/moss treatment, and complex-as-simple collision matching the rendered triangles. No imported/generated binary asset, dependency installation or original-game mutation is needed. The return faces/caps are outside the principal player approach; this is a setting pass below the supplied art target, not a claim of reference-quality cliffs.

Verification requires six-height undercut ray probes, actual capsule sweeps around the face, clear onward corridors and real-character walking/jumping attempts (including stamina lock), plus existing jungle/canopy/ruin/fall/package checks and cooked image review. Actual results belong in HANDOFF. Human play-feel, manual camera review and physical Xbox are separate from automated tests.
