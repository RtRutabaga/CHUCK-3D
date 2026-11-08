# The ship's ending: the reef, the leap to the foremast and the fall (plan)

Written 2026-11-08 (Claude) at the user's request: "dont add to the game yet but write documentation for this". **Not built. Draft for the user's approval.** It replaces step 2 of [SHIP.md](SHIP.md#the-ending) (the plank) and changes step 1 (the captain no longer comes on deck at the trigger; he has been on the quarterdeck from the start). Step 3, the fall into the Nine Hells, stays, now landing in **Phlegethos**, the 2D game's name for that layer.

The user's draft lines are kept word for word below, marked **(user)**. Lines marked **(proposed)** are Claude's suggestions. They go into the game only once the user approves or rewrites them, since the brief says NPC lines come from the 2D game or the user. Lines marked **(2D)** come from the 2D game.

**Voiced, 2026-11-08:** the user voiced every line in this document's dialogue section (scripted beats, guidance lines, background orders) in ElevenLabs, which approves them. They also voiced Jeffries' cutscene lines JEF_01 to JEF_06 and two sound-only takes, JEF_X01 (straining) and JEF_X02 (choking on seawater), and two captain lines for when the storm first starts (below). CAP_40 (the accusation) is dropped, and so are the other Jeffries lines Claude drafted. See [The voice takes](#the-voice-takes).

## The ship's rig, by its proper names

What the ship actually is, from `ShipDeck.inl`, so the dialogue matches what the player sees. She is a **brig**: two masts, both square-rigged, with a gaff **spanker** on the mainmast over the quarterdeck.

| In the game | Nautical name | Where (ship space, cm) |
| --- | --- | --- |
| The aft mast, Jeffries tied to it | **mainmast** | X −490 |
| The forward mast | **foremast** | X 440 |
| The platform at the mainmast's head | **main top** | Z 1600 (about 10.2 m above the deck), X −560 to −300, 2.6 m wide |
| The platform at the foremast's head | **fore top** | Z 1500 (about 9.2 m above the deck), X 370 to 570, 2.4 m wide |
| The lowest yard on each mast, just under its top | **main yard** / **fore yard** (they carry the **main course** and **fore course**) | 55 cm under each top. Main yard 13 m long, fore yard 12 m, braced round 12° |
| The second yard | **main topsail yard** / **fore topsail yard** | 7.6 m above each top |
| The third yard (out of reach) | **topgallant yards** | 12.3 m above each top |
| The rope nets from the deck to a top | **lower shrouds** with their **ratlines** (the rope rungs). Ships' crews call this the **rigging**: "the main rigging", "the fore rigging" | four shrouds a side on each mast |
| The nets from a top up to the next level | **topmast shrouds** and their ratlines | from the top to the **crosstrees** |
| The crossed timbers at the head of the topmast, where the topmast shrouds end | **fore topmast crosstrees** (no platform here, only the crosstrees and trestletrees) | about 8.7 m above the fore top, about 1 m above the fore topsail yard |
| The very end of a yard | **yardarm** | |
| Ropes under the yard that sailors stand on | **footropes** | already built (sagging cords) |

So the user's "foremast platform" is the **fore top**, the "mainmast platform" is the **main top**, the "fore course spar" is the **fore yard**, and the place where the climb ends is the **fore topmast crosstrees**, from which he steps down onto the **fore topsail yard**.

Larboard is +Y in the code (`YardEnd` Side +1 is port), which is the left-hand side when facing the bow.

## The sequence

The storm already rolls in when the seven-condition gate opens, and the Astral rupture already seals the companion hatch once he is on deck ([../SHIP.md](../SHIP.md#the-storm-developed-november-4)), so from here the whole sequence takes place on the weather deck and in the rig. Nothing below decks needs handling.

The orders begin once the storm is fully up: proposed, when the intense part of the storm track takes over (`IntenseAt`). From then on, each step below has a **gold guide light** on its target: the golden leaf's aura (`M_GoldenLeafAura`) without the cigarette, at the crevice glow's dimmer strength. Only the next target or targets glow. The characters never mention the light; it is only for the player.

### 1. The reef points on the main yard

- **Set-up:** in the storm, the main course is shown **let fall and close-reefed**, not furled as it is now. That is, the sail hangs a short way below the main yard, with its reef band tied up to the yard by a row of short lines, the **reef points**. At both yardarms the points have worked loose and the canvas bellies and flogs out of the reef. This change is needed for the crew's line to be accurate: a furled sail is held by gaskets, not reef points. (The alternative is to keep the course furled and say "gaskets" instead of "reef points", but that loses the user's line.)
- **Guide light:** at the foot of both main shroud nets on deck, then the main top, then the two work spots on the main yard.
- **The work spots:** one each side on the main yard. The real yardarm (6.5 m out) hangs about 2 m outboard of the hull (the half-beam is 4.5 m), so a drop from there lands in the sea. To match the user's expectation that he drops back onto the deck, the work spot should be the outermost grip that still hangs over the deck (about 4.2 m out). The flogging canvas runs out from there to the yardarm.
- **The task:** he brachiates out along the yard's grips from the main top. At the work spot he interacts while hanging (as with the chest), and plays a short two-handed tug clip of about 2 s; the points draw tight and that side's canvas stops flogging. Each side is done once. The done sides survive deaths and respawns.
- **Done when** both sides are secured. He may let go and drop to the deck (falls here are not fatal: he does a landing roll), or stay up.

### 2. The trouble on the foremast

- About 3 s after the second side is done (and once he is not mid-fall), the two crewmen call out the fore topsail's fouled clewlines and that the fore rigging has carried away.
- **What the player sees:** as the second crewman shouts, a sea breaks aboard at the fore channels (`DeckWaveNow`). The ratlines on the fore lower shrouds, both sides, part with a crack: their rungs snap and stream out in the wind as loose rope (`ShipRope`). The shrouds themselves stay standing (if they went, the foremast would go too). The fore deck nets are then blocked (`FChuckClimbable::bBlocked`, as the companion ladder is). The **fore topmast shrouds, above the fore top, stay intact.**
- **If he is on the fore lower shrouds at that moment,** the carry-away waits until he is off them.
- **If he is already on the fore top or above it** (he may have climbed the fore during step 1), there is no jump: skip to step 4 with a line of its own.
- **The fore topsail:** its clews hang half hauled up and flog, and the clewline on one side is jammed in its quarter block on the fore topsail yard. That is what he will clear in the cutscene.

### 3. The leap from the main top

- **Guide light:** the main shrouds' feet (if he is on deck), then the main top, the golden cigarette there (in its own aura), and the fore top.
- **The golden cigarette** appears in the main top, by the lookout's jar, once the fore rigging carries away. Taking it starts the golden leaf's usual 45 s stamina lock. If the lock runs out before he has made the jump, another one appears by the jar.
- **The jump:** from the main top's fore rim to the fore top: **6.7 m** rim to rim, landing **1 m lower**, on a landing area about 2 m deep and 2.4 m wide.
- **Why the golden leaf must lengthen the jump:** his jump today (170 cm/s up, gravity × 0.8, sprint 380 cm/s) carries about **2.9 m** over a 1 m drop. Proposed: while the stamina lock is on, a running or sprinting jump launches at about 600 cm/s forward and 400 cm/s up. That gives a peak about 1 m up after 0.5 s and lands 1 m lower about 7.4 m out, reaching the middle of the fore top. These are starting values, to be measured in the engine rather than trusted from the numbers.
- **Run-up:** the main top is only 2.6 m long. The run-up is a lane beside the mast, at about Y ±70 cm. The starboard lane passes the lookout's jar at (−340, −92), so either the jar moves inboard or the leaf sits clear of the lane. Running-jump boost rather than full-sprint boost, since he cannot reach sprint speed in 2.6 m without a measured check.
- **Landing assist (proposed):** if he leaves the top's fore rim aimed within about 20° of the fore top, the arc is drawn gently onto it, the way grips already catch him. A jump aimed well away still misses.
- **On the way:** the main topmast stay runs from the main topmast head down to the fore masthead, above the jump's path; by the numbers it clears him by about 1 m near the landing, which needs checking in the engine. The fore topsail's curved foot and the fore lower shrouds' heads at the fore top's rim must not catch him either.
- **A miss** drops him about 9 m to the deck (the jump runs along the centreline, so he lands aboard, not in the sea) with a landing roll. He climbs the main again.
- **Does the boost apply everywhere?** The user's words ("make it so golden cigarette makes longer sprinting jump") read as a general rule for the golden leaf. If so, the Chult golden-leaf climbs and canopy routes must be re-tested, because a 7 m jump could skip parts of them. The alternative is a boost only on the ship. A question for the user, below.

### 4. Up the fore topmast shrouds

- **Guide light:** the foot of the fore topmast shrouds (either side), then the crosstrees.
- He climbs the topmast shrouds from the fore top to under the crosstrees. These shrouds already work (the `ratlines` trial climbs the main's).
- **If he drops from the fore top to the deck,** he has to go back up the main and jump again; the captain says so.
- **Reaching the top of the fore topmast shrouds starts the cutscene.**

### 5. The cutscene (cinematic; Esc skips to the fall)

In-engine, after the FallCutscene pattern, cut to the user's music at a phrase (the pattern is `FallCutscene.cpp`). Proposed shots, in order:

1. Close: Chuck at the crosstrees, rain driving across. He steps down onto the fore topsail yard and edges out along it, hanging from it, toward the jammed clewline block.
2. Wide, from off the ship: the whole brig in the storm under close-reefed canvas, heeling, burying her bow, a sea coming green over the forecastle.
3. The sea round her: Astral ruptures opening in the water, purple light under the surface, sliding past on the current. Some are already astern, more are ahead.
4. The deck: Jeffries straining at his ropes on the mainmast, looking out at them. His 2D line: **"It's back! The purple is back!" (2D)**. The crew go on hauling.
5. Close: Chuck working the clewline free of the block. It runs; the fore topsail's clew flies loose; the sail flogs.
6. Ahead, a different rupture drifting toward the bow: a Phlegethos one, red light, cracked basalt plates and lava channels seen from above through it (the Hell fragments described in the original plank plan).
7. A gust; the flogging sail and the yard's jerk throw Chuck off the yard. He falls past the fore course, clear of the side.
8. From above: he drops into the Phlegethos rupture as it passes under the bow. Cut to the fall into Phlegethos (plan step 3, unchanged).

No reactions from Chuck: he works, he is thrown, he falls.

## The dialogue

All lines are subtitles only until the pirates' voice takes exist. The bracketed directions are kept for those takes. One shouted line at a time, with at least about 6 s between lines; a line never talks over another.

Two kinds of line:

- **The scripted beats** play once, in order.
- **Guidance lines** repeat while the player is in a given situation. Each situation has a jargon line first (the joke: correct period orders that most players won't follow) and a **clear line** that takes over after about 30 s without progress, so nobody is stuck.

### Scripted beats

1. [Captain, deep gravelly pirate voice, thick West Country accent, roaring over the howling wind] **REEF THOSE SAILS, YE USELESS BILGE RATS! MOVE!** (user)
2. [Crewman 1, young sailor, rough British accent, shouting urgently, alarmed] **CAP'N! THE REEF POINTS ON THE MAIN COURSE ARE COMIN' LOOSE!** (user)
3. [Crewman 2, older sailor, deep raspy voice, shouting in panic] **BOTH YARDARMS, CAP'N! THE CANVAS IS BREAKIN' FREE!** (user)
4. [Captain, booming, authoritative, shouting orders] **YOU THERE, CABIN BOY! YE LOOK SPRY ENOUGH! UP THE MAIN SHROUDS WITH YE! SECURE THOSE REEF POINTS, LARBOARD AND STARBOARD! MOVE!** (user)

   *(step 1: see the guidance lines below)*

5. When one side is done: [Crewman 1] **LARBOARD YARDARM'S FAST, CAP'N!** or **STARBOARD YARDARM'S FAST, CAP'N!** (proposed; "fast" = made secure)
6. When both are done: [Crewman 2] **MAIN YARD'S SECURE, CAP'N!** (proposed)
7. [Crewman 1, breathless, shouting desperately over the storm] **CAP'N! THE FORE TOPSAIL CLEWLINES ARE FOULED! WE CAN'T GET THE SAIL IN!** (user, with "TOPSAIL" added so it names the topsail and not the course)
8. [Crewman 1] **SHE'S JAMMED IN THE QUARTER BLOCK, UP ON THE TOPSAIL YARD!** (proposed; tells the player where the trouble is)
9. [Crewman 2, hoarse, alarmed, bellowing] **THE FORE RATLINES HAVE CARRIED AWAY! THERE'S NO WAY ALOFT!** (user)
10. The captain, depending on where Chuck is:
    - On the main top, the main yard or higher on the main: [Captain, gravelly, shouting furiously, then suddenly struck by an outrageous idea] **CABIN BOY! YE'RE ALREADY UP THERE!** (user)
    - Anywhere else except the fore: [Captain, gravelly, shouting furiously, then suddenly struck by an outrageous idea] **CABIN BOY! GET BACK UP THE MAINMAST, I HAVE AN IDEA** (user). In the accent, "I'VE AN IDEA!" (proposed, optional).
    - Already on the fore top or above: [Captain] **CABIN BOY! YE'RE ON THE FORE ALREADY! UP TO THE CROSSTREES WITH YE!** (proposed)
11. Once he is on the main top (straight after line 10 if he is already there): [Captain, booming, commanding, utterly confident] **JUMP FOR THE FOREMAST, YE LITTLE SCALLYWAG! JUMP!** (user; repeats, see below)
12. When he lands on the fore top: [Crewman 2] **HE'S MADE IT, CAP'N! HE'S IN THE FORE TOP!** (proposed)
13. [Captain] **THE TOPMAST RATLINES ARE SOUND, BOY! UP TO THE CROSSTREES! CLEAR THAT CLEWLINE!** (proposed; the user asked for lines here)

### Guidance lines, by situation

| Situation | Jargon line (first) | Clear line (after about 30 s) |
| --- | --- | --- |
| Step 1, on deck, not climbing | **SECURE THOSE REEF POINTS, LARBOARD AND STARBOARD! MOVE!** (user; repeats about every 10 s) | **THE MAINMAST, BOY! THE ONE JEFFRIES IS TIED TO! UP THE ROPE NETS!** (proposed) |
| Step 1, climbing the fore rigging instead | **NOT THE FORE, YE LUBBER! THE MAIN RIGGING!** (proposed) | **THE OTHER MAST! JEFFRIES'S MAST!** (proposed) |
| Step 1, standing on the main top | **LAY OUT ON THE YARD! LAY OUT!** (proposed) | **THE BIG SPAR UNDER YE, BOY! OUT ALONG IT, HAND OVER HAND!** (proposed) |
| Step 1, up the main topmast shrouds or on the main topsail yard | **BELAY THAT! THE MAIN YARD, NOT THE TOPSAIL YARD!** (proposed) | **TOO HIGH, BOY! THE LOWEST SPAR! UNDER THE TOP!** (proposed) |
| Step 1, on the yard, not at the work spot | **OUT TO THE YARDARM! WHERE SHE'S FLOGGIN'!** (proposed) | **FURTHER OUT, BOY! WHERE THE CANVAS IS FLAPPIN'!** (proposed) |
| Step 1, one side done, the other still loose | **NOW THE STARBOARD!** / **NOW THE LARBOARD!** (proposed; whichever is left) | **THE OTHER END O' THE YARD, BOY!** (proposed) |
| Step 1, one side done, he has dropped to the deck | **BACK UP THE MAIN SHROUDS! THE STARBOARD YARDARM'S STILL FLOGGIN'!** (proposed; or LARBOARD) | **UP THE MAINMAST AGAIN! THE OTHER SIDE!** (proposed) |
| Step 3, on deck | **CABIN BOY! GET BACK UP THE MAINMAST, I HAVE AN IDEA** (user; repeats as **UP THE MAIN SHROUDS, BOY! TO THE MAIN TOP!**, proposed) | **UP TO THE TOP, BOY! WHERE YE WERE BEFORE!** (proposed) |
| Step 3, on the main top, not yet smoked the leaf | **THERE'S LEAF IN THE LOOKOUT'S JAR! SMOKE IT, BOY!** (proposed) | the same, once more |
| Step 3, on the main top with the leaf lit | **JUMP FOR THE FOREMAST, YE LITTLE SCALLYWAG! JUMP!** (user; repeats about every 6 s until he lands on the fore top) | **RUN AT IT, BOY! RUN AND LEAP!** (proposed) |
| Step 3, missed the jump, back on deck | **AGAIN! UP YE GO! AND THIS TIME RUN AT IT!** (proposed) | then the step 3 on-deck lines |
| Step 3, the leaf ran out, another by the jar | **ANOTHER! THERE'S ANOTHER IN THE JAR!** (proposed) | |
| Step 4, on the fore top, not climbing | **UP THE FORE TOPMAST SHROUDS! TO THE CROSSTREES!** (proposed) | **CLIMB, BOY! THE ROPE NETS OVER YE, ALL THE WAY UP!** (proposed) |
| Step 4, dropped from the fore to the deck | **NOT DOWN, YE FOOL!** (proposed) | **BACK UP THE MAIN, BOY, AND JUMP AGAIN!** (proposed) |

"Too long in the wrong place" is measured per situation: the jargon line comes after about 8 s there, then repeats every 10 to 12 s; the clear line takes over after 30 s with no progress. Progress (a new net mounted, a side secured, the leaf lit, the jump made) resets the timer.

### Background orders (the joke)

Shouted by the captain and the crew in the gaps, in no situation in particular. All of them are correct period orders, and none of them is anything the player needs to do. (All proposed.)

- [Captain] **HANDSOMELY ON THAT HALYARD! HANDSOMELY! ... NOW SMARTLY, DAMN YE!** ("handsomely" means slowly; "smartly" means fast.)
- [Captain] **WEATHER EARING FIRST, THEN THE LEE! MIND YER EARINGS!** (the reef earings at the yardarm; it sounds like earrings.)
- [Captain] **TIE YER POINTS ROUND THE YARD! REEF KNOTS, MIND, NOT GRANNIES!**
- [Captain] **STARBOARD YER HELM! ... I SAID STARBOARD! THE OTHER STARBOARD!** (in the period, helm orders named the tiller's side, so "starboard the helm" turned her to larboard. Sailors argued about it for two centuries.)
- [Captain] **SHE'S GRIPIN'! UP HELM! LUFF AND TOUCH HER, THEN KEEP HER FULL!** ("griping": the ship trying to come up into the wind.)
- [Crewman 2] **WHO'S GOT THE WEATHER MAIN BRACE?! HAUL TAUT AND BELAY!**
- [Captain] **CLEW UP! BUNTLINES AND CLEWLINES, HAUL!**
- [Crewman 1] **LET GO THE FORE TOPSAIL HALYARD! ... NO, THE OTHER ONE!**
- [Captain] **OVER THE FUTTOCKS, LIKE A SAILOR! THE LUBBER'S HOLE'S FOR LUBBERS!**

### Unused 2D lines

The plank's lines from the 2D ending have no place now: "Captain on deck!", the accusation, "Put him on the plank.", "But we've all grown fond of the smoking rat, Cap.", "No.". One could come back: when Chuck lights the golden leaf in the main top, the captain recognises it: **"That's the rat who stole my Premium Buhetian Halfling Leaf." (2D)**, then "...", then the jump order. A question for the user.

## The voice takes

The user's ElevenLabs takes (2026-11-08; the two storm-start lines 2026-11-05) are in `SourceAssets/NPCs/Voice/ShipStorm/source/`, as supplied: one file per script section with every line in script order, and the two storm-start lines on their own. `SourceAssets/NPCs/Voice/ShipStorm/lines.json` lists the 61 lines: id, speaker (`PirateCaptain`, `PirateCrewman1`, `PirateCrewman2`, `Jeffries`), text, source take, and start and end in seconds. That is the `source`/`start`/`end` form `Tools/build_npc_voice.py` already reads, so building the sequence means copying these entries into `SourceAssets/NPCs/dialogue.json` and running it; that tool levels each line and makes the Unreal assets. `Tools/split_ship_storm_voice.py` writes each line as its own WAV to `Local/ShipStormVoice/<speaker>/<id>.wav` for listening.

Storm-start lines (new, the user's):
- **CAP_S00** [Captain] ALL HANDS ON DECK! STORM'S UPON US!
- **CAP_S01** [Captain] THE SEAS SHALL RUE THE DAY THEY DARED CHALLENGE THE MUSTACHIOED MARAUDERS! BWAHAHAHAHA!

How the cut points were found (once; the tool only applies them):
1. The silences in each take (10 ms loudness, 12 dB over the noise floor) split it at every pause of 0.12 s or more: 37 pieces in the scripted take, 54 in the guidance take, 21 in the background take, 12 in the cutscene take.
2. Windows' built-in offline recognizer (System.Speech, en-US; nothing installed), given only that take's sentences as its vocabulary, named about half the pieces. Every one it named fell in script order.
3. A best-path alignment in script order matched every piece to a sentence, using the named pieces and the syllable counts. Each line was found exactly once (the user made one take of each, so there are no a/b repeats). A full-file recognition pass gave the same line timings.
4. Two boundaries were settled by hand: "MOVE!" at 3.1 s is the end of CAP_01, not the crewman's "CAP'N!" (that is the piece at 4.0 s); CAP_31 begins at 78.0 s. Each cut has 0.1 s of lead and 0.2 s of tail, never past the midpoint of the gap to the next line.
5. Checked: each finished clip back through the recognizer. With all the script's lines as its choices, 35 named their own line or half of it, 16 gave no answer (mostly the hoarse Jeffries takes and lines with pauses inside), and none named another line, apart from CAP_20L. Against only their neighbours, no doubtful clip matched a neighbour. CAP_20L ("NOW THE LARBOARD!") was heard as "starboard". The hiss measurement settles it: the two "starboard" clips have the "s" burst where the word starts and both "larboard" clips have none.
6. The sound-only take has no words. Its first 4.5 s are three pitched, sustained sounds (the strain, JEF_X01), and from 4.7 s there are eight short noisy bursts (the choking, JEF_X02). The 0.19 s gap between them is the only judgement call.

Not done: nobody has listened to every clip. The speakers in the scripted take follow the script; when everyone is shouting, their voices' pitch is too close to confirm by measurement.

## Nautical notes

- **Larboard**, not port, fits the golden age of piracy. "Port" replaced it officially only in the 1840s.
- **Reef points are on a sail that is set and reefed**, not one that is furled: so the main course must be set close-reefed in the storm (step 1).
- **Clewlines** belong to topsails and topgallants; on a course the same ropes are **clew garnets**. So "the fore clewlines" already meant the topsail. "TOPSAIL" was added for the player.
- **"Carried away"** means broken or torn away by the weather: correct. Only the **ratlines** (rungs) of the fore lower rigging carry away, not the shrouds, so the mast still stands.
- **"Main shrouds"** for the nets up the mainmast is right; crews also said "the main rigging".
- **"Yardarm"** is the outer end of a yard; the reef points work loose there first, where the earings are.
- **The fore top** is the platform at the foremast's head; the **crosstrees** above it are timbers, not a platform.
- **Phlegethos** is the fourth layer of the Nine Hells; the 2D game uses it for the volcanic region.
- **Calling the rat "cabin boy"** fits: the ship's boys were the smallest and quickest aloft.

## What would change in the build (when approved)

- `ShipDeck.inl`, `ShipSetting.cpp`: the main course set close-reefed in the storm, with its reef points and the two loose, flogging yardarm sections; the fore lower ratlines' carry-away (broken rungs as `ShipRope` strands), and those nets blocked; the fore topsail's half-clewed state and the jammed quarter block; the golden cigarette spawn by the main top's jar (and the jar moved clear of the run-up lane if needed).
- `ShipStorm.h/.cpp`: the sequence's state (steps 1 to 4, sides done, carried away, leaf spawned) kept across deaths and respawns; Chuck's situation (deck, fore rigging, main top, main yard, main topmast, fore top, fore topmast) and the timers for the guidance lines; the gold guide lights.
- `ShipCrew.cpp`: the shouted lines as subtitles from the captain and two named crewmen (proposed: Crewman 1 one of the haulers, Crewman 2 the helmsman), the background orders.
- `ChuckCharacter.cpp/.h`: the golden-leaf long jump and the landing assist; the hanging work clip at the yard's work spots.
- A new cutscene (`ShipForeTopsailScene`, after `FallCutscene.cpp` and `TempleSanctumScene`), handing over to the fall into Phlegethos.
- `ShipTest.cpp`: trials `reef` (both sides, the drop to the deck, the sides kept across a respawn), `fore_carry` (the nets blocked, the topmast shrouds still climbable), `leap` (the boosted jump from both lanes lands on the fore top; without the leaf it falls short), `fore_climb` (to the crosstrees starts the cutscene), `guidance` (Chuck put in each situation gets the right line, the clear one after 30 s), `ending` (the cutscene plays and Esc skips into the fall). Golden-leaf trees 0 and 1 and the canopy trials again if the boost is general.
- Dev Checkpoints: Ship Reef (dev), Ship Foremast Leap (dev), Ship Fore Topmast (dev).

## Questions for the user

1. **Approval** of the sequence, and of each (proposed) line, or your rewrites.
2. **The main course:** set it close-reefed in the storm so "reef points" is accurate (recommended), or keep it furled and say "gaskets"?
3. **The golden leaf's long jump:** everywhere the golden leaf is smoked (Chult re-tested), or only on the ship?
4. **The leaf in the main top:** the lookout's stash by his jar (as written), or the captain's own leaf, with his 2D accusation line when Chuck lights it?
5. **The cutscene's music:** the storm tracks, the fall theme, or a track you supply?
6. **When the orders start:** with the intense part of the storm track (proposed), or at once when the storm begins?
