# Chult — 3D adaptation plan

Written 2026-10-12 (Codex). **Landing work authorized by the user, 2026-10-12:** start Chult at the cutscene landing and match the supplied contact sheet; brachiation can have a better layout farther into the map. This approves the first setting pass, with a ground exploration clearing and jungle loop, deferring the first grip chains. Later milestone detail remains proposed. Actual implementation and test evidence are recorded in HANDOFF.

**Canopy raptor, user request 2026-10-14:** one fast raptor now patrols the brachiation trees, pushing the player to climb and cross by brachiation (Claude; [../CHULT-RAPTOR.md](../CHULT-RAPTOR.md)). This brings forward one of the original two raptors; the planned raptor territory beyond the wreck remains for later.

**Canopy setting expansion authorized by the user, 2026-10-13:** extend the map with closely spaced trees for existing cat-to-cat climbing, standing canopy forks and branch/vine routes for brachiation. Codex builds this setting; Claude implements and tests the traversal. Raptors are a tentative future reason to use the canopy, not part of this pass. Assess partial treetop vision fading if foliage obstructs the player; camera/material integration follows Claude's traversal work. A later tree will require Premium Buhetian Halfling Leaf to introduce the stamina lock, but neither that tree nor the pickup is authorized for this pass. See [../CHULT-CANOPY.md](../CHULT-CANOPY.md) for the authored setting contract.

**Far-bank jungle river authorized October 15, 2026:** the user's request extends beyond the bridge into dense jungle alongside a wider, faster meandering river ending in a broad waterfall into the bridge canyon. This is setting work within Chult; preserve the active crossing and earlier areas. Layout and checks: [../CHULT-RIVER.md](../CHULT-RIVER.md).

**Starting-side topography and canyon vista authorized October 15, 2026:** add realistic cliffs and distant landscape around the starting side, composing the canyon as the main reveal. Leave all far-side setting and future expansion space unchanged. Contract: [../CHULT-VISTA.md](../CHULT-VISTA.md).

**Upriver expansion requested October 15, 2026 (Claude):** extend the playable far bank beyond the old path end with the ancient road, push the vista back, and add taller brachiation trees for a later T-rex that can only be avoided in the canopy (reached later with the golden cigarette's stamina lock), then dense jungle before the later temple (not built). New areas get spawn points; every spawn point gets a Dev Checkpoint. Layout and checks: [../CHULT-UPRIVER.md](../CHULT-UPRIVER.md).

**Temple exterior requested October 15, 2026 (Claude):** "build the temple exterior, similar to 2d game", after the 2D Chult map 5. Exterior only; the interior waits. Layout and checks: [../CHULT-TEMPLE.md](../CHULT-TEMPLE.md).

## Direction and sources

**Ancient jungle path remnants authorized October 15, 2026:** the user requests a free resource for an overgrown ancient road from the cliffs behind the landing, through the landing and raptor territory, aligned to both ends of the bridge, then following the far river to its current end. Keep the road on dry banks; preserve the bridge crossing and future upstream expansion. Contract: [../CHULT-PATH.md](../CHULT-PATH.md).

**Northeast canyon and broken rope bridge authorized October 14, 2026:**
extend the current open jungle beyond the brachiation line and stream. Build a
deep canyon with jungle banks and a decrepit rope bridge whose surviving planks
are the proposed handholds across missing-board gaps. No usable walking deck,
rope walkway or ground bypass crosses the canyon. Codex owns the setting and
inactive hand-position contract; the user will work with Claude on registration,
brachiation, camera, entry/dismount and actual crossing tests. Existing Chult
setting/encounters and the current canopy remain intact. Layout and division of
work: [../CHULT-CANYON.md](../CHULT-CANYON.md).

**Additive vegetation/dressing and low fog authorized October 14, 2026:**
densify the currently built jungle toward Muhammed Haroon Neyyathoor's
[Fantasy Tropical Forest](https://kunjonart.artstation.com/projects/1Ne1JX),
using layered fern/broad-leaf colonies, mossy outcrops and ruin creepers.
Remove no existing scene elements. Preserve walking/cinematic lanes, canopy
grips/feet, ruin traversal and the stream. Add low mist and assess cooler light
in cooked views. No character, controls, encounters or onward region changes.

**Northeast jungle extension authorized October 14, 2026:** expand the floor beyond Claude's new canopy line, continue the starting clearing's tree/palm/fern/broad-leaf/moss/rock vegetation, add mild rolling elevation and a shallow flowing stream. Preserve all eight canopy trees, grips, lianas, ruin access and canopy respawn. Keep their tested ground feet level; place relief and the stream around that corridor, with walkable banks and ground recovery/exploration loops. This is setting work, not a new swimming/current mechanic, encounter, cliff boundary, pickup or region transition. See [../CHULT-EXPANSION.md](../CHULT-EXPANSION.md).

**Ruins beside the tree parkour area authorized by the user, October 13, 2026:**
add an optional ancient stone courtyard south of the canopy grove, with broken
terraces for short running jumps, low mantle steps, a paired-wall stairwell and
a standing causeway to the first tree fork. Reuse existing movement and stone/
moss materials. Keep the landing, existing ground loop, tree climbing faces,
named grip proposals and later temple sequence intact. No NPCs/dialogue,
encounters or region transitions are part of this addition. User explicitly
defers impassable Green Hell-inspired boundary cliffs: do not implement them.
Layout and verification contract: [../CHULT-RUINS.md](../CHULT-RUINS.md).

**Free-resource art pass authorized by the user, October 13, 2026:** implement
the researched Poly Haven bark, soil, four ferns and six mossy rocks, plus a
limited optimized island-tree background layer. Improve the existing landing
and grove setting, preserving the cinematic clearing, walking routes, paired
climb faces and named canopy proposals. No new encounters or grip activation.
Provenance/pipeline: `SourceAssets/Setting/ChultNature/README.md`.

Adapt the original Chult journey into a dense, layered jungle where brachiation is a major part of exploration. Ground routes, roots, branches and the shipwreck form one readable environment at Chuck's established 65 cm scale. Exploration leads; height and apparent danger can look demanding while the existing forgiving traversal makes movement approachable.

Read-only source root: `C:\Users\ashsm\OneDrive\Documents\CHUCK-game`.

- `docs/design/GAME-BIBLE.md`: character, tone, environmental storytelling and gradual discovery of incorrectly joined worlds.
- `docs/development/PHASE-3.md`: pantry fall and arrival in Chult; Claude owns its 3D cutscene.
- `docs/development/PHASE-4.md`: first jungle, small-body routes, abandoned expedition evidence and an onward route.
- `docs/development/PHASE-5.md`: jungle ship landmark, obstacle section, undead escape, respite and temple approach.
- `docs/development/PHASE-6.md`: temple progression, traps, shrine, final chamber and eventual passage to the separate ship at sea.
- `chuck/chuck/assets/maps/chult_{jungle,falls,cog,run,respite,temple}.txt`: spatial and pacing references. These are reference layouts, not grids to extrude into 3D.

The phase documents contain historical development instructions. Use their content as adaptation evidence, subject to the current 3D brief and the user's changes. Their old engine, resolution, checkpoint, enemy and crew requirements do not become implementation instructions here.

## Confirmed changes from 2D

| Original | Current 3D direction |
| --- | --- |
| Ground-based jungle navigation | Dense jungle brachiation parkour is a major traversal layer, reusing existing capability and animations. |
| Jungle zombies, skeletons and later raptors | Keep zombies and dinosaurs, including the original two raptors. No skeletons until the temple; mixed jungle undead encounters become zombies. |
| Temple skeletons and snakes | Skeletons first appear in the temple. The user's clarification concerns when skeletons appear, not removal of the original temple's other creatures/story encounters. |
| Intact jungle cog with a sailor | A substantial ship wrecked in the jungle, abandoned and accessible. Remove its sailor, all crew and the sailor's dialogue. |
| Ashtray checkpoints | Current 3D region/local entrance respawn rules; no ashtrays or Astral Anchors. |
| Waterfall treasure discovery | Keep a visible, reachable waterfall cove for exploration; no treasure chest or chest system. |
| Ordinary cigarette rewards | Starting in Chult, occasional individual golden cigarettes named Premium Buhetian Halfling Leaf grant a temporary stamina lock and continuous wall climbing. They recur throughout later regions. |
| One-foot Chuck and top-down presentation | Current 65 cm Chuck, human-scale world and continuous orbit camera. |

The LOST reference guides the discovery of an abandoned ship impossibly inland, overgrown and explorable. It does not introduce that show's characters, story, exact vessel or mythology. Retain a fantasy sailing vessel as the proposed base; build an original wreck, not a decorative fragment.

The jungle wreck is separate from the later pirate ship at sea. Do not remove or merge that later campaign region.

## Proposed journey

**Landing clearing → root-and-branch jungle → wreck basin → tangled jungle/zombie escape → quiet respite → temple forecourt → temple interior.**

Build connected spaces with recognizable landmarks and short optional loops. Dense vegetation should suggest a much larger jungle without committing to a huge open world. Preserve the original sequence of discovery, pressure, relief and monumental arrival rather than its map count or tile geometry.

Include a waterfall cove near the respite as a visible, reachable exploration space. Let water sound, glimpses of the falls and an open approach draw the player there; it is not a secret-map entrance or a treasure-chest destination. A pool, wet rocks, roots and a short optional branch route can make it worth visiting without a reward requirement. Do not place a chest or tie the cove to a chest system. The original Chult dialogue data also contains a tortle encounter; the enemy restrictions do not remove nonhostile NPCs. Preserve space for that encounter in later jungle planning, using its source lines if retained, without inventing new dialogue. Only the wreck sailor is explicitly removed by this request.

| Space | Main experience | Brachiation and grounded exploration |
| --- | --- | --- |
| Landing clearing | Recover control after Claude's fall; see a route into humid jungle | Safe footing matching the cutscene, ground exploration first. Grip chains start farther into the jungle. No enemy at the handoff point. |
| Root-and-branch jungle | Discover layered routes and signs of a failed expedition | Multiple short swing chains, root ramps, logs, a rat-sized passage and an optional branch loop overlooking the path. First avoidable zombies below. |
| Wreck basin | Gradually reveal a large broken vessel among trees | Approach from ground or branches; climb aboard, explore the deck and hull interior, then exit back into jungle. Rigging connects to nearby branches. |
| Tangled jungle | Retain distinct raptor territory, thorn navigation and zombie escape sections | Repeated canopy chains above dangerous ground, short standing sections and a narrow escape route. Give the two raptors their own space beyond the wreck. Thorn patches shape ground navigation without enclosing every path. |
| Respite | Let the player wander and breathe after the escape | Broad stable roots, gentler optional swings, cigarette grass and quiet views. No mandatory combat. |
| Temple forecourt | Reveal stone geometry overtaken by jungle | Final branch approach, ruined terraces and an obvious walkable entrance. Jungle zombies stay outside. |
| Temple interior | Move from organic routes to torch-lit masonry and skeletons | Ledges, columns, restrained hanging roots/chains where plausible, gaps and established traps. Brachiation supplements the dungeon rather than making every room a canopy course. |

### Brachiation as a substantial part of the jungle

- Put swing chains on the main journey repeatedly, not in one optional demonstration. Proposed target: roughly half of the authored traversal sections include brachiation; measure route use during playtesting rather than claiming a time percentage now.
- Alternate chains with stable branches, tree forks, root shelves, ground clearings and wreck surfaces. Long stretches of repeated button presses should not replace exploration.
- Give a branch route a reason to exist: crossing wet ground, avoiding zombies, reaching the wreck's upper deck, discovering an overlook or shortening a return journey.
- Offer local ground/recovery routes beneath early chains. Later, a short mandatory chain can cross a real obstacle once the player understands catching and dismounting. Avoid requiring an entire flawless canopy run to progress.
- Branch forks are exploration choices. Keep the next intended grip clear and avoid nearby unintended grips stealing selection. Show destinations through openings in foliage and recurring branch shapes; no floating arrows or quest markers are proposed.
- Keep Chuck's motion restrained. Reuse Swing and the one-arm SwingLeapLeft/Right animations; do not introduce grappling, swinging on a long free rope, animated moving grips or new acrobatics in the first milestone.

### Existing traversal contract and layout bounds

Reviewed `docs/BRACHIATION.md`, `ChuckClimbable.h` and the current swing selection/leap code in `ChuckCharacter.cpp`.

- Setting registers static world-space `FChuckSwingGrip` points through `AddChuckSwingGrip`, with a horizontal `Along` direction. Visible branch grips must meet the paws; a decorative branch somewhere near an invisible grip is insufficient.
- Existing selection accepts approximately 40–240 cm horizontal separation, at most 70 cm rise or 130 cm drop, within its directional cone, with an unobstructed visibility line and a clear capsule sweep. These are capability limits, not recommended routine spacing.
- Start with 125–170 cm spacing, nearly level chains and gradual turns. Validate actual catch, body clearance and camera behavior before extending height changes or spacing. Grip positions must clear solid branch geometry both at rest and during the leap arc.
- Existing first catches are automatic near the raised paws. Establish entry through a reachable low branch, jump or existing climb; do not copy the docks' chimney-climb entry into every tree.
- At a chain end, jumping without directional input may send Chuck back to the previous grip. Provide a visible dismount platform and a clear aimed exit; test this explicitly rather than assuming jump always moves forward.
- Stamina already takes small amounts on swing leaps and never prevents swinging when empty. Do not add exhaustion falls or permanent upgrades. Premium Buhetian Halfling Leaf uses the existing `LockStamina` hook for a temporary benefit; ordinary route completion must remain possible without the pickup.
- Registration lifetime/reset behavior needs a region-transition audit before implementation. Static global grip indices must not retain an unloaded region's points or refer to the wrong region after respawn/re-entry.
- Tree trunk collision must support the existing character rather than assume flat-wall climbing works on arbitrary bark. Ground ramps and authored climbable surfaces can provide reliable entry without changing the controller.

Use noncolliding peripheral leaves, simple trunk/branch collision, and deliberately clear traversal corridors. Sweep the real capsule and inspect the actual leap arcs; a straight target-selection sweep alone does not prove the complete animation clears foliage or timber.

### Dense jungle readability and camera

Dense means layers of trunks, understory, hanging foliage and distant canopy. It must not mean a camera constantly buried in leaves. Clear space around Chuck, his hands, the next grip and the landing surface. Compose canopy routes so both rat-height and elevated orbit views can read them.

Review leaf silhouettes, camera collision, automatic swing-camera alignment, manual orbit and fork selection in the same route. Thin foliage should not act like a solid camera wall. Any camera/controller change belongs to Claude; Codex first resolves setting obstructions. Wind animation must not move a gameplay grip away from the paws.

Start with a small reusable tree/root/branch kit and repeated instances. Profile foliage, shadow cost and overdraw on the actual machine before increasing density. No new rendering dependency, asset purchase or installation is part of this planning pass.

## Accessible jungle wreck

Proposed discovery: only part of the bow or broken mast appears through the trees at first. Approaching reveals a substantial timber hull, heeled into a jungle basin, split and threaded with roots. Its scale stays architectural beside Chuck. Vegetation and debris imply abandonment without explaining how it arrived.

Minimum playable coverage:

1. Walk around a meaningful part of the hull and see its placement inland.
2. Enter through a broken hull opening at ground/root height; the wreck never requires a crew interaction or key.
3. Explore a connected lower hold with damaged bulkheads, intact supporting timbers and human-scale cargo. Crates and barrels are terrain, not breakables.
4. Reach the fractured main deck using existing climbing/ledges or a root ramp.
5. Explore a small accessible cabin remnant or stern compartment and an optional rigging/branch route above the deck.
6. Leave through a second opening or deck-to-root connection, forming a loop back to the jungle route.

The main journey passes through the wreck discovery space; full interior exploration can remain optional. The wreck must nevertheless be accessible in ordinary play, with clear return routes. Avoid invisible walls around an otherwise promising opening.

No living crew, crew dialogue, replacement quest-giver or invented explanation. Proposed wreck interior is a quiet exploration pocket without combat. Use ruined sails, rotten planking, cargo and roots to tell the scene. Small Astral traces near the displaced hull may preserve the original implication, but should not dominate the wreck or block every entrance. Their exact presence is a layout proposal, not a new user requirement.

## Enemy and temple adaptation

Jungle undead encounters use zombies, with ground patrol/pursuit and escape routes; dinosaurs remain in their original encounter role. Canopy height is useful separation; zombies must not damage Chuck through branches or floors. Keep landing platforms outside unavoidable attack zones and provide opportunities to regain the canopy. Progress never requires clearing every zombie. Avoid unbounded hordes until the authored escape section has been profiled and tested.

Retain the original two large raptors in a distinct territory beyond the wreck. They provide faster ground pressure than zombies, with brachiation, elevated refuges and rat-sized routes offering avoidance and escape. Neither raptor must be defeated to progress. Claude owns their behavior, animation and imports. Preserve dinosaurs from the original game without introducing additional species under this plan. Replace mixed zombie/skeleton escape pressure with zombies exclusively; no skeletons appear in the jungle.

Skeletons first appear inside the temple. Retain the original broad-room/narrow-connector rhythm, shrine, traps, gaps, ruined stone and snake chamber as source direction for the later detailed temple plan. The corrected instruction does not impose a skeleton-only temple roster.

Preserve the original final-chamber adventurer/beholder tableau, scripted Fireball and onward escape as source story beats for the later detailed temple plan. The skeleton timing rule does not remove them. Their 3D staging and implementation remain outside the first jungle milestone; do not turn the tableau into a new player-fought boss encounter without an explicit design decision.

## Premium Buhetian Halfling Leaf

Confirmed user direction: occasional **individual golden cigarettes**, named **Premium Buhetian Halfling Leaf**, appear starting in Chult and continue throughout the game. For their duration they lock stamina at maximum and allow continuous wall climbing. This is a temporary power-up, not permanent character progression.

Reuse `AChuckCharacter::LockStamina(Seconds)` and `IsStaminaLocked()`. The existing hook already holds stamina full, allows continuous upward wall running and extends the side wall-run arc. Keep ordinary traversal and animation; the pickup does not unlock a new move. Controller behavior and any changes to the hook remain Claude's scope.

Proposed pickup design for later implementation:

- A visibly golden single cigarette in the world, distinct from ordinary cigarette grass/jar rewards. Proposed activation is immediate on collection, following the existing pickup interaction; no inventory or equip step is required.
- Occasional authored placements near inviting climbing surfaces or optional vertical shortcuts, including later regions. The first placement should sit on safe reachable ground with a clear climb nearby. Do not require a pickup already above the climb it is meant to assist.
- The golden pickup grants the timed effect separately from ordinary sanity/currency rewards. No additional sanity, currency or damage benefit is proposed.
- Show a restrained gold stamina-ring treatment and readable remaining duration; this UI is proposed, not implemented. Duration is not yet chosen. Select it through a representative climb trial, with enough time to understand the effect and reach stable footing.
- Inspect current repeat-pickup behavior before adopting refresh/extension semantics. Do not assume effects stack. Define expiry during a climb, respawn/reset and later save/load behavior before implementation; expiry must not cause an unavoidable fall or softlock.
- Leave the main route traversable without the effect. Golden cigarettes can make climbs continuous, improve flow and open shortcuts without turning pickup collection into a permanent progression gate.

The waterfall cove does not require a golden cigarette and is not its dedicated reward location. These are independently useful parts of the exploration plan.

Codex handles placement and pickup/tooling integration within agreed ownership; Claude handles character effect, presentation/animation and HUD changes. Add the first golden pickup to the first-jungle milestone once its duration and lifecycle are specified. No pickup asset, HUD or new timed-effect test has been implemented in this documentation pass.

## Cutscene-to-setting handoff for Claude

Claude owns the cellar fall trigger, fall/landing animation, cutscene camera and return of player control. Codex owns the destination setting and its safe landing volume. Proposed contract to agree before integration:

- A named destination such as `ChultLanding`, with floor height, capsule spawn transform and facing supplied by the setting. Exact coordinates follow the landing graybox; do not hard-code a cinematic endpoint before it exists.
- A clear landing footprint with room for the actual animation and camera, plus a stable forward view toward the first route. Validate its dimensions with Claude's landing motion.
- Destination geometry/collision and grip registration ready before control resumes. Spawn is neither inside foliage nor in an enemy/hazard zone.
- Existing sanity/cigarette state carries into the region. Cutscene input suppression and transient falling/swing state are cleared on handoff; pending input must not immediately launch a jump or repeat the transition.
- Audio moves from the fall into Chult ambience/music. Reuse original soundtrack direction and available supplied audio where appropriate; do not run the 2D music generators.
- Respawn uses the playable area's entrance/local spawn and does not replay the cinematic. Entering the temple uses its own safe local entry. Save/load design remains a full-game foundation decision; no historical ashtray/code-save implementation is silently restored.

This is a proposed integration contract, not a claim that a destination API or Chult level already exists. Claude can develop the cutscene independently, then connect it once the landing setting is ready.

**Status (Claude, 2026-10-12):** the cutscene is built (`Unreal/Chuck3D/Source/Chuck3D/FallCutscene.cpp`, about 47.5 s on the user's "Fall Cutscene theme"). It runs on its own stage with its own jungle set (`SourceAssets/Cutscenes/FallToChult`), 25 km from the docks, and ends on the existing "End of demo" card. Connecting it means replacing `AFallCutscene::Finish()`'s call to `EndFallCutscene()` with the handoff to `ChultLanding` under the contract above. The landing setting may also take over the floor shots, from his impact at 26.3 s onward. Chuck's Sanity is unchanged by the cutscene's death and return (it is presentation only).

**2026-10-13 (Claude):** the cutscene's floor shots now use the landing itself. `AChultLanding` was split into `BuildSet()` and `BuildEnvironment()`; `Build()` does both, and both are idempotent. The cutscene builds the set with its sky stage and calls `BuildEnvironment()` at the canopy crash, after hiding its own sky lighting. `EnterPreview` completes an existing landing rather than building a second one. Keep the cutscene camera lane (`OnRoute`) and the floor origin when changing the landing.

## Delivery order and ownership

The user's October 12 landing instruction starts with a **playable setting preview** before the full milestone below: match the cutscene clearing and add a winding ground loop, available through Dev Checkpoints. The first brachiation sample moves beyond that clearing. Claude's production cutscene handoff remains separate. See [../CHULT-LANDING.md](../CHULT-LANDING.md) for the concrete floor/entrance contract and current limits.

| Milestone | Codex setting/verification work | Claude work | Acceptance before promotion |
| --- | --- | --- | --- |
| 1: landing and jungle traversal sample | Safe landing, small dense jungle loop, low entry chain, raised branch chain, fork, dismounts and ground recovery | Cutscene connection; any required camera/traversal fixes | Complete fall-to-control handoff, forward/reverse chains, fork selection, safe release/recovery, both camera heights and keyboard/controller routes reviewed. |
| 2: first jungle area | Connected route, rat-sized passage, expedition remnants, optional canopy loop and first golden cigarette placement/integration | Zombie behavior/animation, timed-effect HUD and any trunk/climb support | Avoidable zombies, no through-floor damage, local respawn, readable brachiation and golden pickup activation/expiry reviewed. |
| 3: wreck discovery | Accessible hull/hold/deck/compartment, rigging grips and jungle return loop | Traversal integration and asset imports as needed | Every promised wreck space entered/exited; no crew; collisions, rigging routes and interior camera verified. |
| 4: deeper jungle and forecourt | Raptor territory, tangled route, zombie escape setting, respite and temple reveal | Raptors, encounter pacing and movement/camera refinements | Both raptors avoidable, pressure then relief, canopy recovery, no jungle skeletons and safe temple entry. |
| 5: temple | Separate detailed temple layout and setting after jungle review | Skeletons and original temple creatures/story/transition work as planned | Temple plan reviewed first; skeleton introduction, traps and onward story contract verified. |

Only one agent edits shared runtime files or runs Unreal/Blender heavy work at a time. This documentation pass can coexist with cutscene work because it changes no runtime, assets or tooling. Agree shared-file ownership at integration rather than starting simultaneous builds.

Before each playable milestone, inspect the current handoff and unfinished assets, build one candidate, verify and inspect it, promote through the root launcher tooling, and record actual results. Existing Waterdeep smoke checks remain regressions; add Chult checks without assuming docks-specific checks automatically apply to a new region.

## Verification to perform during implementation

- Entry, respawn and later re-entry restore the correct destination and grip registry; no stale world pointers/indices.
- Grip positions match visible surfaces; all intended chains work in both directions, empty-stamina state included.
- Actual leaps clear collision; catches and dismounts do not place the capsule inside a trunk, hull or branch.
- Forks select the intended target with keyboard and Xbox input. Physical controller play is recorded separately from simulated input.
- Missing a chain allows a readable recovery or local respawn; no repeated cinematic and no softlock beneath the canopy.
- Rat-height/elevated framing, manual orbit and swing-camera alignment preserve visibility through the dense jungle and inside the wreck.
- Jungle enemy spawn inventory includes zombies and the original two raptors, with no skeletons. Skeletons first appear inside the temple. Wreck contains no crew or sailor interaction.
- Wreck interior and upper surfaces are connected, supported and escapable; return routes remain open after exploration.
- Golden cigarette activates maximum-stamina lock and continuous climbing for the chosen duration; test expiry, repeat collection, local respawn/reset and later save/load. Ordinary routes remain usable without it.
- Waterfall cove is visibly reachable, has no treasure chest and does not depend on a chest or power-up reward system.
- Measure performance in a representative foliage/encounter view and retain timings; no performance or comfort claim before measurement and human play.

The checks above cover later milestones as well as the landing. Consult HANDOFF for checks actually run; they are not all acceptance claims for the first setting preview. Existing docks brachiation is a reusable baseline, not proof that jungle branches, the wreck or transitions pass these checks.

October 13 follow-up authorization: round/refine the existing brachiation grove,
replace continuous impossible branches with separate natural reaching boughs and
jump gaps, and add broader realistic roots. Preserve the ruin connection and
ground lanes. Boundary cliffs remain explicitly deferred. The revised setting
contract is in CHULT-CANOPY.md; active traversal integration remains Claude's work.

October 13 approval: implement the researched free gray stone wall scans and
heavier moss on the existing grove ruins, and round/erode the stone blocks.
Rock Wall 08, Mossy Stone Wall and Rabdentse Ruins Wall are the selected CC0
sources. Preserve parkour routes and the stone-to-tree connection. Cliffs remain
deferred; material/geometry contract is recorded in CHULT-RUINS.md.

## October 14 — partial boundary cliffs (authorized)

The user now requests impassable cliffs at parts of the treeline, inspired by Green Hell, and explicitly keeps the next expansion beyond the brachiation trees and stream open. This supersedes the earlier cliff deferrals above. Build a broken-height, weathered mossy rock escarpment behind the western/southern old forest, spanning bearings 100–325 degrees at roughly 102 m from the landing. Leave the northeast extension and its onward ground/stream corridors clear. Use existing rock materials and procedural visible-mesh collision; no downloads, binary imports or character changes. Continuous undercut faces exclude existing wall-climb/mantle surface acceptance; no reachable intermediate ledges or registered grips. Verify face normals, capsule blocking, preserved routes/canopy and cooked views before launcher promotion. This authorizes boundary setting only, not a new region or transition.

## October 14 follow-up — vegetated geology and continuous watercourse

User supplied two jungle-cliff reference images and requested varied vegetated cliffs, jungle reaching their feet without plain empty margins or invisible barriers, a modest source waterfall at the map boundary, and a continuous creek ending as a waterfall into the existing canyon. This authorizes replacement/refinement of the old boundary surface and stream setting, preserving all authored canopy/ruin/bridge coordinates and controls.

Replace the uniform overhanging ring with fractured buttresses, varied strata, outcrops/talus, vegetated ledges and crests, plus layered ground growth from the old forest to the visible rock. Reuse existing foliage/rock meshes and natural rock scans; use a dedicated cliff material without the masonry texture blend. Solid boundaries follow visible rock/trunks, never a hidden wall.

Place a small cleft-fed waterfall at the eastern edge near (21500,6000). Its pool feeds a shallow descending meander west toward the outer canopy, then northeast along the established creek corridor. Continue across the canyon's near bank to a side notch at T=-1800, away from the bridge's entry at T=0. Lower only that local drainage channel so water descends continuously into the gorge; do not move bridge planks/grips or add a crossing. A narrow broken water ribbon, lip foam and restrained mist represent the larger drop. The next expansion/bridge corridor remains open. No swimming, water force, NPC/dialogue, checkpoint, pickup or character changes.

Verify water/bed alignment and monotonic fall, exact source/outlet continuity, ford/ground/bridge/canopy clearances, physical cliff blocking and real walks. Review cooked close and wide vegetation/rock, source falls, stream and canyon outfall views before the final package suite and launcher promotion. Actual results belong in HANDOFF.
