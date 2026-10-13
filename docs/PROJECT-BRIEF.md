# Project brief — CHUCK 3D

Rewritten 2026-10-12, when the user declared the Waterdeep prototype done and started the full game. The prototype-era brief, with its dated authorizations, is kept as [archive/PROJECT-BRIEF-PROTOTYPE.md](archive/PROJECT-BRIEF-PROTOTYPE.md); it is history, not scope.

## What this is

CHUCK 3D is a Windows PC adaptation of the completed 2D game CHUCK, rebuilt as a third-person 3D exploration game in Unreal Engine 5.7.4, with Blender 4.5.14 for custom assets. It follows the 2D campaign region by region, from the Waterdeep docks to the return to Waterdeep.

The 2D game (`C:\Users\ashsm\OneDrive\Documents\CHUCK-game`, strictly read-only) is the direction for each region: its order, places, people, obstacles, dialogue and story beats. Its `docs/design/GAME-BIBLE.md` is the authority on character and tone. Its `docs/development/PHASE-*.md` documents describe each region as it was built in 2D. Its `chuck/chuck/data` holds the dialogue.

The adaptation is loose where 3D asks for it. Layouts are reinterpreted at human scale, and the 2D game's obstacles become traversal. It is faithful in character, tone, story order and dialogue.

## Chuck

- A gray rat about 65 cm tall, just above the kneecap of a 180 cm human, in an oversized purple open jacket. This 3D size supersedes the 2D game's one foot. The world stays built for humans, and his size shapes level design rather than being cosmetic.
- He is quiet, patient, fearless, compassionate, observant and persistent, with a thousand-yard stare and quiet exhaustion. He never speaks, has no inner monologue and never behaves like a cartoon. The world is funny; Chuck is not.
- He is a fey spirit who cannot die. At zero Sanity he vanishes into astral light and is summoned back. He never becomes more powerful. New traversal moves are how a small, persistent body gets around, not upgrades.
- The supplied character images (see [ART-DIRECTION.md](ART-DIRECTION.md)) are the visual target. Do not claim they have been reached.

## Rules carried over from the 2D game

- Exploration leads, with environmental storytelling over exposition. The world never explains itself directly, and the truth about the joined realities arrives slowly.
- Tone: cozy exploration, melancholy, mystery, deadpan comedy. Never cynical or overly dark; the world stays worth caring about.
- Campaign references are mostly environmental. Bobert stays asleep, unnamed and noninteractive. The opening area's only other reference is the HEROD COVER BAND / TONIGHT ONLY poster.
- NPC lines come from the 2D game or the user, never AI-written. Conversations contain NPC speech only, never replies from Chuck. Inspect text is external observation, not Chuck's thoughts.
- Combat is light. Exploration always matters more.

## Decisions already made in 3D (these override the 2D game)

- **Traversal** replaces top-down movement: jump, climb (with stamina), ledge pull-up, wall-run, four-legged sprint leap, brachiation and ladders. Each region should give these moves meaningful use.
- **Camera:** one continuous orbit that runs from elevated down to rat height. The prototype never formally closed the camera decision, so retuning per region is allowed.
- **Sanity and cigarettes:** damage lowers Sanity, shown as a bar of cigarettes. Cigarettes refill it, and beyond a full bar they count as currency.
- **Breakables:** Chuck shreds grass tufts and breaks clay jars, and later urns, to collect cigarettes. He never breaks crates or barrels, because he is not strong.
- **Temporary power-ups (user clarification, 2026-10-12):** starting in Chult and recurring throughout the game, occasional individual golden cigarettes named Premium Buhetian Halfling Leaf lock stamina at maximum and allow continuous wall climbing for their duration. This temporary effect does not change the no-permanent-power-progression rule. Duration and lifecycle details remain to be designed; reuse the existing stamina-lock capability.
- **Chult adaptation:** retain dinosaurs, including the original raptors, alongside jungle zombies; skeletons first appear in the temple. The jungle ship is an accessible abandoned wreck without crew. Keep a reachable waterfall cove without a treasure chest or chest system. Dense jungle brachiation is a major traversal focus. See [regions/CHULT.md](regions/CHULT.md).
- **Respawn:** the map spawn or a local region entrance is the respawn point. There are no Astral Anchors and no ashtray checkpoints.
- **Transitions:** use fades or physical falls between areas, never loading-question prompts.
- **Cutscenes (user, 2026-10-12):** each 2D cutscene becomes an authored sequence in the engine's own 3D. It keeps the 2D scene's beats and uses the music the user supplies, which may be cut short or faded at a musical phrase. There is no player control; Esc skips. The first is the fall to Chult (`FallCutscene.cpp`).
- **Hardware:** Windows PC with keyboard/mouse and Xbox controller. No browser, mobile or console builds.

## Regions

The order follows the 2D phases. "Done" means a playable, verified build exists.

| 2D phase | Region | 3D status |
| --- | --- | --- |
| 2 | Waterdeep docks, fountain plaza, sewer tutorial | Done (the prototype) |
| 3 | Tavern, pantry, fall to Chult | Done: the sky-hole fall plays the fall-to-Chult cutscene, which ends on the "End of demo" card until Chult's landing exists |
| 4 | Chult jungle | Next |
| 5 | Deeper into Chult | — |
| 6 | The jungle temple | — |
| 7 | The pirate ship | — |
| 8 | Phlegethos | — |
| 9 | The Feywild | — |
| 10 | Zephyros' tower | — |
| 11 | The rainy city | — |
| 12 | The cabin | — |
| 13 | The collided desert | — |
| 14 | Return to Waterdeep | Partly anticipated by the evening docks state |

## How a region gets built

1. **Plan first.** Before any region work, read its 2D phase document and data, then write `docs/regions/<REGION>.md`. It covers the 3D layout, the traversal and obstacles, the NPCs and their source dialogue, the entry and exit transitions, and what the region needs from each agent. The user approves the plan before building starts.
2. **Ownership.** Codex builds the setting. Claude builds the character, creatures, animation, traversal, camera and imports. See [AGENT-WORKFLOW.md](AGENT-WORKFLOW.md).
3. **Milestones.** A region, or a meaningful part of one, is a playable milestone: build, verify and promote it with `Tools/Promote-Build.ps1`, then record the actual tests in the handoff.
4. **Scope control.** Build only the approved region plan. Changes to Chuck's identity, controls, scale, tone or the rules above are the user's decision.

## Open questions for the user

- **Saving.** The prototype has no persistent save. The 2D game used twelve-character save codes. A multi-region game needs a decision.
- **Combat depth.** Small enemies (rats, a zombie) exist. Combat in later regions follows each region plan.
- **Naming.** The launcher, scripts and the Unreal `Content/Prototype` folder still use "Prototype". Renaming them is planned cleanup.

## Technical rules

Explain dependencies before installation and record exact versions. Keep generated Unreal output and packaged builds out of Git, and assess LFS before committing binary assets. Never put engine installations in this repository. Run only one Unreal or Blender heavy process at a time on this 16 GB machine.
