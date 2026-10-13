# Chult — 3D adaptation plan

Written 2026-10-12 (Codex). **Planning only; no Chult geometry, imports, runtime changes or playable tests have been completed in this pass.** The user's changes below are confirmed direction. Layout, quantities and milestone boundaries are proposals for review before implementation.

## Direction and sources

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
| Landing clearing | Recover control after Claude's fall; see a route into humid jungle | Safe footing, a low first grip chain and a reachable rest branch. No enemy at the handoff point. |
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

## Delivery order and ownership

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

Current evidence is limited to source/document review. Existing docks brachiation is a reusable baseline, not proof that jungle branches, the wreck or transitions pass these checks.
