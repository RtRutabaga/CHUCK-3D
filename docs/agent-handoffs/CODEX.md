# Agent handoff — Codex

Per-session log, newest first. Template: [TEMPLATE.md](TEMPLATE.md). Prototype-era entries are in [../archive/agent-handoffs/CODEX-PROTOTYPE.md](../archive/agent-handoffs/CODEX-PROTOTYPE.md).

## October 12 — Chult landing setting preview

The user authorized starting the map at the cutscene landing, matching the supplied contact sheet, with brachiation better supported farther in. `ChultLanding.h/.cpp` reuses the existing fall-set meshes/materials without imports, Blender runs or binary changes. The near tree/palm placements, floor origin/material and facing match the cutscene. A ground loop reaches about 46 m into the jungle and returns through a second lane. Simple capsule trunk collision, a box under the additional fallen timber, noncolliding foliage and a solid floor support this first pass. The ambient light/exposure were reduced after capture review.

`Dev Checkpoints → Chult Landing` enters the setting, hides/disables Waterdeep scenery and its environment, fades out the docks score and starts Chuck with the existing movement/camera. A Chult flag in the existing area-reset path makes recovery local; no new traversal or animation behavior. No grips registered. The normal pantry cutscene still ends on the demo card. Claude owns its production state/input/audio handoff; `docs/CHULT-LANDING.md` supplies the concrete contract and warns that the preview reset refills sanity/stamina and is not a state-preserving transition.

Engine/toolchain: Unreal **5.7.4 / CL 51494982**, MSVC **14.44.35229** (tools folder **14.44.35207**), Windows SDK **10.0.26100.0**, bundled .NET **8.0.412**. No new dependencies. Original CHUCK-game accessed read only by absolute path. No binary asset commits or LFS uploads needed.

Build issue resolved: UBT's cached makefile restores its saved environment (`BuildMode.cs`), including expired sandbox TEMP/TMP paths, causing MSVC D8050. Clearing PCHs alone did not solve it; rebuilding the makefile did. `Build-Game.ps1` now uses `-NoUBTMakefiles`, skips its already-built editor during packaging, and offers `-SkipBaseAssets` to preserve existing binary materials/map. The final standard command was `Build-Game.ps1 -SkipBaseAssets -Package -ArchiveDirectory Builds/ChultLandingCandidate` (`Local/chult-final-build.log`), and succeeded. An experimental `-NoPCH` build exposed existing missing includes and was abandoned without unrelated source edits. Generated cache/probe backups remain ignored under Local.

Test issue resolved: the first packaged Chult test passed its geometry/captures but crashed while deleting its executing timer lambda (`Local/verify-chult-20261012-211809.log`). The timer now lives through world shutdown; the corrected `verify-chult-20261012-213439.log` exits cleanly with assets present, **127** floor samples, **12** real-capsule segment sweeps and local recovery passing. Reviewed low/elevated landing and deeper-loop captures. The full package suite passed **182/0** (`verify-package-20261012-213439.log`), including cutscene skip/full play, pantry adjustments, leap rift and lamp swing. Real menu checks passed **8/8** (`verify-menu-20261012-214404-*.log`); Chult starts at exactly its entrance, on the floor, with gameplay input/possession and local recovery correct. Launcher promotion is pending.

Limits: flat floor, repeated procedural trees/foliage, simple collision rather than mesh-accurate roots/trunks. Buttress roots, lianas and original scenic log are visual. No outer boundary, onward transition, Chult score/ambience, brachiation, hazards, enemies or pickups yet. Not tested: physical Xbox, complete walking loop, representative FPS profile or production cinematic handoff. This is a first setting preview, not completion of Chult or its art target.

Next part of the work can be done here: promote the verified candidate; then Claude connects the cutscene and Codex extends the jungle.

## October 12 — Chult adaptation plan

Documentation only: `docs/regions/CHULT.md` records dense brachiation jungle, zombies plus original dinosaurs/raptors, no skeletons until the temple, and an accessible crewless jungle wreck. Subsequent user direction removes the waterfall chest, retains a visible cove, and adds occasional golden Premium Buhetian Halfling Leaf cigarettes beginning in Chult and recurring game-wide: temporary max-stamina lock and continuous wall climbing via the existing hook. PROJECT-BRIEF also records these rules. Proposes the landing-to-temple route, recurring swing chains, explorable wreck, traversal constraints, Claude cutscene handoff and staged milestones. Original 2D game read only; existing binaries/textures preserved.

Reviewed original bible, Phases 3–6, map/dialogue references and current brachiation/stamina-lock code/docs. No runtime or asset edits, builds, imports, packaging or gameplay tests. Current launcher/package unchanged. Layout, power-up duration/lifecycle/UI and milestone acceptance checks are proposals for review, not passed checks. Corrected the initial skeleton-only-temple interpretation; original temple snake and beholder/adventurer story beats remain source direction for later detailed planning.

Next part of the work could be done by either — preference: Claude continues the fall cutscene; Codex implements the first jungle setting milestone after plan review and landing-contract agreement.
