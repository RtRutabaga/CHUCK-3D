# Chult hostile life and the zombie grab (Claude, 2026-10-14)

User requests, 2026-10-14:
- "Add 15 zombies around the starting side of the jungle, but not around the brachiation trees, and add 4 more raptors around the brachiation trees."
- "Give the zombies a 25% chance on hitting to actually pick Chuck up and bite him instantly taking all his hp, do the new animation for this."

## Jungle population (`ChultLife.h/.cpp`)

- `SpawnChultLife` runs from `AChultLanding::Build`, after the canopy raptor (only in playable Chult, once).
- **Raptors:** four more `AChultRaptor`s on the canopy round ([CHULT-RAPTOR.md](CHULT-RAPTOR.md)). They start at points 0, 3, 5 and 8; the ones at 3 and 8 walk the round backward. There are five in all, tagged `ChultPack` apart from the first.
- **Zombies:** fifteen `ADockNPC::SpawnZombie`s, tagged `ChultZombie`, off either side of the landing's ground loop (`ChultLanding.cpp`'s `Route`).
  - **Placement:** 4.5–22 m out from the loop, at least 20 m from the landing and 8 m apart, chosen by a fixed random stream (seed 20261014).
  - **Exclusions:** the brachiation reserve plus 14 m (their leash is 11 m), the ruins plus 6 m, the expansion and the canyon.
  - **Ground check:** each spot must be open floor, found by a downward trace with a body-sized clearance test.
  - **Timing:** they are placed 0.5 s after the build. Collision built in the same frame cannot be traced yet (Chaos updates its query structure on the next physics tick), so an immediate placement found floor for only 6–9 of 15.
- **Automated runs:** all of this is skipped in every `-Chuck…Test/Capture/Review` run except `-ChuckJungleLifeTest` and `-ChuckZombieGrabTest`, so the older tests keep their worlds. The raptor test still has its one raptor.
- **Test:** `-ChuckJungleLifeTest`. It checks:
  - counts and placements;
  - zombies idle at home and raptors walking their round while Chuck is at the landing;
  - that the nearest zombie notices him and comes.

  Package verification requires `zombies=15` and 0 failures.

## The grab (`DockNPC.cpp` `TickGrab`/`PoseZombieGrab`, `ChuckCharacter.cpp` held gait, `ChuckAnimInstance.cpp` struggle layer)

- **The roll:** when a zombie's lunge connects (the same reach, height and facing tests as its bite), `ADockNPC::GetZombieGrabChance()` decides whether its hands close on him instead of the 2-sanity blow. The chance is 0.25 in play, 0 in automated runs and 1 in `-ChuckZombieGrabTest`.
- **What saves him:** `AChuckCharacter::BeginHeld` refuses exactly where `TakeBite` would: bite immunity, astral, dodging (a roll or side jump through the tell), hanging, climbing or wall-running.
- **Applies to:** all zombies, including the sewer's.
- **Zombie timeline (s from the seize):**
  - **0–0.4:** it squares up, closes to 62 cm and drops into a deep stoop (hips 34°, spine 28°, chest 10°, knees bent, hips 16 cm lower). Both hands reach to his chest and close.
  - **0.4–1.1:** it straightens, lifting him to 44 cm in front of it, 112 cm up (his chest).
  - **1.1–1.45:** it draws him in to 40/117 cm, the head going back and the jaw opening 26°.
  - **1.5:** the bite. The head snaps down onto his neck and shoulder and the jaw closes. `HeldBitten`: sanity 0, louder contact sound plus a rasp.
  - **1.5–2.3:** it worries him with hard head shakes that die away (5.5 Hz, 0.35 s decay).
  - **After he vanishes:** its arms fall over 0.5 s; then it recovers and shambles home.
- **Hands:** two-bone IK, wrists 13 cm either side of his chest, elbows out and down, fingers forward round his front, palms in, thumbs up. The fingers grip at 90%.
- **Chuck (`EGait::Held`):**
  - **State:** out of play while held (`IsAstral()` is true: no input, bites, raptors or zombie notice). Movement is disabled; the zombie sets his chest position and facing every tick (`SetHeldPose`).
  - **Animation:** the jump loop under a procedural struggle layer:
    - legs kicking at 2.4 Hz;
    - arms pushing forward at its hands;
    - back twisting at 1.7 Hz, with the head turned away and drawn back and the jaw a little open;
    - tail lashing.

    The bite jolts him (an exponential 0.12 s spine curl, head jerk and tail whip). Then he goes limp over 0.35 s: head down, arms and legs hanging, tail drooping.
  - **The vanish:** 0.65 s after the bite he vanishes into astral light out of its hands. The usual summon-down clip is skipped; he hangs limp in its grip until hidden. Then comes the normal fade and return: the area start, which is the landing in Chult and the canopy slab around the trees.
- **Test:** `-ChuckZombieGrabTest` (with `-ChuckZombieGrabCapture`, side-view frames `Chult/ZombieGrab_0..5.png`). It checks:
  - one grab;
  - held;
  - a lift of more than 70 cm;
  - sanity 0 while still in its hands;
  - vanished;
  - summoned back whole at the landing;
  - that it let go;
  - the hands' worst gap from his chest under 18 cm (measured from the previous frame's pose and target, because a long frame moves the target a long way).

  Package verification runs it.

## Far-bank river population (Codex, October 16)

The original layout below is historical; October 17 adds close attackers and
opposite-bank groups, replacing the harmless-walk acceptance check.

User requests many zombies along the later half of the far-bank river, with
manageable encounters on the path and more beside it to discourage shortcuts.
`ChultRiverLife.cpp` adds 32 existing zombies: eight separated road encounters
and 24 inland sentries, in eight stations from canyon-frame S=19000 to 32300
(small seeded variation). The river runs from S=4500 to 33800. The bridge
arrival and the giant-tree/T-rex approach remain outside this population.

Placement waits until the ancient road resolves around solid trees, then uses
that actual centre line. Road zombies stand 7–9.5 m beside it, facing along the
route so they do not all spot him head-on at once; inland groups
stay at least 13 m from any road, beyond their existing 11 m player/home pursuit
limit. Water, terrain height, body clearance and inter-zombie spacing are
checked before spawning. Normal zombie attacks, speed, grab odds and recovery
remain unchanged. New tags distinguish this population from the 15 landing
zombies so the original life checks keep their original meaning.

`-ChuckRiverZombieTest` enables all Chult life and verifies the 8/24 population,
dry supported positions and road separation, then walks three 15 m encounter
stretches with the real character and live enemies. Each must finish on foot,
with at least 3 Sanity left, without waking any inland sentry. Automated grab
odds use the existing zero-grab test behavior; the dedicated grab suite still
tests the lethal grab separately. The package verifier requires this check.
Human play feel, random grab outcomes and whole-population performance still
need player review; automated geometry checks alone cannot settle difficulty.

Cooked candidate evidence: `Local/verify-river-zombies-20261016-210518.log`:
32/32 dry, supported, separated placements; all three upstream walks finished
at full Sanity (24.1, 21.1 and 21.0 seconds), with zero inland sentries active.
An earlier closer layout failed the walking trials and was replaced. The three
normal follow-camera captures were reviewed, but start before reaching each
zombie; they do not establish how readable every enemy is in the undergrowth.

## River attack route and opposite bank (Codex, October 17)

User: "Add more zombies along the path by the far river, player should need to
dodge attacks while they go this way, add more on the other side of the river
also to discourage them going around."

The earlier 32 zombies remain. Sixteen new attackers alternate beside the
resolved road, about nine metres apart, 0.8–2.7 m from its centre line, facing
the incoming player. Twenty-four more stand on supported dry ground across the
river, in scattered groups of three along its later half. Total river population:
72. Bridge arrival and the giant-tree/T-rex stretch remain outside this layout.
No enemy statistics, attacks, grab odds, animation or player controls change.

`-ChuckRiverZombieTest` now requires all 72 placements, three live route trials
with actual lunges and timed player rolls (at least 3 Sanity left, no refills
during a trial), an unguarded attacker causing damage, and an opposite-bank
zombie noticing the approaching player. These are five trials. The package
verifier requires their new counts and completion. Actual results are recorded
in HANDOFF after running; the prior harmless-walk evidence does not apply.

Camera-only capsules on the 72 river zombies stop the follow camera passing
through pursuing torsos. They ignore pawn and visibility queries, so movement,
climbing and attacks keep the original collision behavior. Close limbs can
still occlude parts of the view; no general NPC camera or material change.

Cooked target checks pass in `Local/verify-river-zombies-20261017-161301.log`:
72 supported dry positions and camera guards; three 18 m routes each trigger
two lunges and two rolls, finish at 3 Sanity, and wake no inland sentries. The
unguarded control is hit; an opposite-bank zombie starts shambling toward him.
As before, automated tests disable random grabs; ordinary play retains the 25%
lethal grab on a landed attack. These walking/forward-roll trials do not prove
whole-route difficulty or human camera comfort.

## Not done / flaws

- Neither the grab nor the struggle is a captured or keyframed clip: both are procedural layers over existing clips. The head-to-neck bite interpenetrates a little by design; there is no contact solve.
- No camera treatment: the orbit simply follows his capsule up.
- One shared random roll per landed hit. There is no cooldown between grabs, and no way to break free.
- The jungle zombies are hard to see in the thick undergrowth.
- Five raptors share one round and ignore one another when steering.
- Not played by a person, and no Xbox test.

## Zombie sounds (user 2026-10-16)

"Find download and implement better noises for the zombies." The 12 synthetic zombie sounds (`gen_zombie_sfx.py`) are replaced, under the same names, by recordings. They are used by every zombie, in the jungle and the sewer. With the user's approval, Claude downloaded 8 CC0 files (about 10.7 MB, no accounts needed): BigSoundBank's studio zombie recordings "Zombie #1"-"#7" by Joseph Sardin (lossless FLAC), and artisticdude's Zombies Sound Pack (OpenGameArt). The hits reuse Kenney's Impact Sounds, and the lunge whoosh a rubberduck clip, both downloaded earlier for the dinosaurs. `Tools/build_zombie_sfx.py` cuts and mixes the clips:

| Slot | When | Made from |
|---|---|---|
| Rasp ×3 | idle groan every 4-7 s; the grab | breathy moans from Zombie #4 and #5 |
| Swing ×2 | its lunge | the loudest snarls of #2 and #3 over a whoosh |
| ClawHit ×3 | Chuck's scratch lands | a fleshy punch and a short grunt |
| ChuckHit ×3 | its blow or bite lands on Chuck | a heavy punch and a body thump |
| Death ×1 | the killing scratch | #6's burst-and-fade over a slow low groan |

The used clips (19, 5.2 MB, LFS; `*.flac` is now LFS-tracked) and their provenance are in `SourceAssets/Audio/SFX/Zombies/Source/`. They are credited in DEMO-CREDITS.txt. The cuts were chosen from spectrograms and have not been auditioned.

