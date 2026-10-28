# Chult prop audit (Claude, 2026-10-27)

The user saw trees floating a little off the ground, hollow logs with no collision that could be walked into and seen through, and fallen temple pillars intersecting urns. `ChultPropAudit.cpp` finds these by measurement and the fixes below remove them.

## What was wrong

Measured with `-ChuckChultTest -ChuckPropAudit -ChuckNoGrounding` (the fixes off) on `57f4458`:

- **Floating trees: 2,078 of 15,499.** Every planter takes the ground height at the tree's origin. The scanned island tree's trunk stands about 90 cm off its origin and ends in a thick root slab, so on a slope or a bank the slab's downhill rim hung up to 1.6 m over the ground. Palms and the landing's tall trees had the same problem on a smaller scale. 70 trees stood over a cliff or the gorge's mist, with nothing under part of their foot for more than 4 m.
- **Logs.** `SM_FallLog` is an open tube (the generator left the ends uncapped). The clearing's log had no collision, so Chuck could walk into it and see through it. Both logs stood 6–7 cm clear of the ground.
- **Jars in stone: 4.** In each of the three Astral bend rooms the pot stood inside the fallen column drum (the pot 40 cm from the wall, the drum 30 cm). In the snake chamber a jar sat in an idol's plinth.

## Fixes

- **Trees** (`GroundChultTrees`, run 1.1 s after the set is built, spread over frames at 4 ms each, about 2 s of work in all): each tree's foot is traced at its middle and round its outline. For the scanned tree this is the outline of its vertices below 50 cm, read from the imported asset; for the others it is a ring at their flare. The tree is lowered until its lowest foot point is in the ground. The scanned tree goes 14 cm × scale deeper, so its slab's rim does not read as a pedestal. Its hidden trunk collision moves with it. A tree whose foot would have to drop more than 1.6 m, or that has two foot points over a 4 m drop (including after it was lowered), is removed with its collision. Trees with no ground under them yet are tried again twice, 2.5 s apart. `-ChuckNoGrounding` turns the pass off.
- **Logs:** `Tools/build_fall_jungle.py` closes both ends with a ragged broken rim sinking to a dark heart (`log_end`). Rebuild with `-- --only SM_FallLog`, then import with `Tools/import_fall_log.py`, which keeps the existing materials. `AChultLanding` lays both logs 12 cm × scale into the ground and gives each a box along it (`FallenLog`).
- **Jars:** each bend room's pot now stands past the drum's broken end, or in the corner beyond it if a tear is in the way (`ChultTempleWindBuild.inl`). The snake chamber's jar moved from S 66550 to 66440.

## The audit

`-ChuckChultTest -ChuckPropAudit` waits for the grounding pass and then measures:

- every visible tree's foot;
- the logs: a sphere test at five points along the axis must hit collision, and the underside must touch the ground;
- every jar and urn: masonry inside a capsule raised 6 cm off its foot, another jar crowding it, and its foot on what is under it.

It logs `CHUCK_PROP_AUDIT_COUNTS` and the 40 worst issues, takes stills of the worst and of the trees moved most (`Saved/Screenshots/.../Chult/PropAudit_*`), and then logs `CHUCK_PROP_AUDIT failures=N`. `Verify-Package.ps1` requires failures=0.
