# PHASE 2 — Waterdeep Starting Area and Sewer Tutorial

## Status

**Current implementation phase.**

Agents should work only on Phase 2 unless Sean explicitly changes the active phase.

## Phase Goal

Transform the completed Waterdeep exterior into the game’s proper starting area and tutorial region.

Teach the player how to interact with NPCs and objects, how dialogue and choices work, how to jump, how to use Chuck’s scratch attack, and that the geography of the world is beginning to break down in strange ways.

Keep the tutorial integrated into exploration. Avoid modern tutorial feel, large instructional panels, quest markers, or excessive explanation.

## Human NPC Scale Rule

Every new human NPC uses the existing dock worker as the human-scale reference.

Clothing, hair, armor, and silhouette may vary. Overall height and proportions remain consistent.

Do not introduce larger or more detailed human sprite proportions.

## Waterdeep Guard

Add an armored human guard near the upper-right portion of the Waterdeep map so he appears to guard the normal route away from the docks.

He blocks Chuck.

Dialogue:

> “Stick to the docks, rat.”

Keep the sprite simple and consistent with the existing procedural pixel-art style.

## Sewer Grate

Add a readable medieval sewer grate near/in front of the guard. Do not make it look like a modern industrial drain.

Interaction opens a normal dialogue box:

> “Jump into the sewer?”

Choices:

- YES
- NO

NO closes the dialogue. YES transitions Chuck to the sewer map.

## Market NPC

Add a human market woman near the existing red market awning at the same general scale and proportions as the dock worker.

Dialogue:

> “No handouts here. If you’re hungry, you should check the sewer for scraps.”

She subtly directs the player toward the sewer. Do not create a quest, marker, or waypoint.

## Interaction Tutorial

For Waterdeep and the sewer tutorial areas only, show brief white tutorial text near the top of the screen when Chuck is close enough to interact:

> “Press E to interact”

Use equivalent wording if the actual interaction key differs.

The text appears only in interaction range and disappears when Chuck moves away.

This is temporary tutorial UI and should not become persistent throughout the game.

## Intended Misdirection

The guard blocks the obvious route out. The grate appears to be a workaround. The market woman reinforces the sewer.

The player enters expecting to get around the guard.

This expectation is intentionally wrong.

The sewer loops Chuck back to the Waterdeep docks.

Never explain the joke or design intention to the player.

## Sewer Map

Create a separate medieval sewer map using stone, dirt, mud, and a muddy flowing sewer river or drainage channel.

The sewer is relatively narrow, long, and mostly linear. Do not create a large maze.

Chuck cannot climb back through the entrance. The only way out is forward.

## Sewer Atmosphere and Music

Create or use a sewer soundtrack that feels slightly eerie and slightly funky.

Keep it consistent with the SNES-inspired sound direction.

Do not use realistic ambient sewer audio as a major storytelling device.

## First Signs of the Collided Worlds

Add small areas of pixelated, glitch-like Astral Sea material.

Use a dark blue and purple nebula-like appearance.

These should look like blocks or chunks of another environment have incorrectly appeared inside the sewer.

They are not portals. Do not add swirling gateways, magical doorways, or teleportation effects.

The visual language is that part of the sewer map has been replaced by part of another world's map. The transition may be abrupt and visibly wrong.

One or two readable artifacts or fragments from other settings may be included only if clearly conveyed at the game's native scale.

Do not rely on tiny details, realistic graphics, close inspection, or complex animation.

## Jump Tutorial

An Astral Sea void section physically interrupts Chuck's path and must be jumped.

When Chuck approaches, show:

> “Press SPACE to jump”

Use the actual key if controls differ.

The required jump should be obvious and reasonable.

This is a real gameplay jump, unlike the later impossible pantry jump.

Remove the tutorial text after the obstacle is successfully crossed.

## Rat Combat Tutorial

After the jump obstacle, place a small group of ordinary rats in Chuck's path.

They are visibly slightly smaller than Chuck.

The player must fight them to continue.

Introduce the scratch attack and show the actual attack control, for example:

> “Press F to scratch”

Use a simple animation: brief forward movement, paw scratch, wind-swipe, or similarly minimal readable motion.

Do not create elaborate combat animation.

Keep combat simple. Do not add inventory, equipment, stat screens, or a complex combat tutorial.

One hit kills one rat. Later enemies may be stronger.

## Sewer Exit

At the end, add a drainage gate or sewer outflow.

Moving through it transitions Chuck back to the Waterdeep exterior.

Use a brief, simple animation showing Chuck climbing back onto the dock from the water.

Only communicate that Chuck came out in the water and climbed onto the dock.

Do not explain the sewer route.

## Return to Waterdeep

Return Chuck to the recognizable Waterdeep docks. The guard remains.

The tavern entrance is now visually changed. The doors are gone or replaced by a black/open doorway that clearly reads as accessible.

Do not build the tavern interior during Phase 2.

Phase 3 will implement the tavern interior and cheese/pantry/falling-through-the-world transition.

Phase 2 only prepares the exterior state so the tavern clearly reads as the next accessible location.

## Scope Restrictions

Do not:

- implement the tavern interior
- implement the pantry or cheese sequence
- implement the fall into Chult
- add inventory or equipment systems
- add quest markers or a quest log
- redesign the existing Waterdeep map
- substantially alter the completed Waterdeep exterior art

Build on the current procedural rendering system and established native visual style.

Preserve the existing map layout unless a small adjustment is technically necessary for the guard, grate, or tutorial interactions.

The purpose of Phase 2 is to make Waterdeep a functional starting area, teach basic controls, introduce collided-world geography, and return Chuck to the docks ready for Phase 3.
