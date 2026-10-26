# Chult — the temple heroes

User request, October 22, 2026 (Claude): "create the three heroes from the 2d game and add them to the grand temple room at the end. we'll add the beholder later. these three adventurers will be much more detailed and visually impressive than our NPCs so far, and they will have movements for attacking and taking hits that are much more involved than our existing NPCs, especially the fighter, one thing I want to emphasize is the feeling of a good hit, the fighter will have a sword and a shield and when he swings his sword to cut off the beholder's eye or to take down a skeleton I want the movement of the swing and hit to be realistic and satisfying to watch. download and install whatever you see fits best."

They stand in the [sanctum](CHULT-TEMPLE-WIND.md#the-sanctum), the hall at the end of the Astral wind, and fight skeletons that keep getting up out of the bones. Since October 23 the beholder hangs over the dais before them, alive but not yet fighting ([CHULT-TEMPLE-BEHOLDER.md](CHULT-TEMPLE-BEHOLDER.md)).

## The 2D direction

Read-only sources: `docs/development/PHASE-6.md`, `tools/generate_adventurer_sprites.py`, `data/dialogue/temple_sanctum.json`, `PROJECT_STATUS.md` (sessions 130-131 and the battle notes).

- **The cast:** a male fighter, a wizard and a clearly female ranger, battling a beholder and skeletons. It is their battle, not Chuck's: he only witnesses it. Do not explain who they are; they return much later.
- **The fighter:** dented plate with a helm, a sword with a gold crossguard and leather grip, a shield. He "occupies skeletons".
- **The wizard:** deep blue robes, grey beard, a pointed hat, a crooked staff with a sea-green gem. He is "working, rather than fighting" (the binding), and casts the Fireball later.
- **The ranger:** forest greens, a long auburn ponytail, a longbow. She fires arrows.

Not built here: the entrance lines, the beholder (since built: [CHULT-TEMPLE-BEHOLDER.md](CHULT-TEMPLE-BEHOLDER.md)), the battle's hazards for Chuck, the Fireball (since October 26: [CHULT-TEMPLE-BEHOLDER.md](CHULT-TEMPLE-BEHOLDER.md#the-cutscenes)) and the escape.

## What was downloaded, and what was used

The user's "download and install whatever you see fits best" covered these downloads. Nothing was installed. The tools are the ones already in the project (Blender 4.5.14 + MPFB 2.0.17, UE 5.7.4).

| Source | What | Used? |
|---|---|---|
| ambientCG (CC0) | Metal054A (hand-worked iron) and Metal038 (dark scratched steel), 2K | Yes: the plate, rims, rivets, blade, gilt (tinted) |
| Poly Haven (CC0) | velour_velvet, poly_wool_herringbone, fabric_leather_02, scuba_suede, hessian_230, 2K | Yes: the robe, hat, mantles, leather, gloves, gambeson |
| CMU motion capture (free for any use) | 17 candidate takes into `Local/HeroMocapCandidates` | Only 02_05 "punch/strike", as the skeletons' blow at a hero |
| Quaternius Universal Animation Library 1 and 2 (CC0), 15.9 and 18.7 MB | Sword combos, shield block and impact, hit reactions | No (below); kept in `Local/HeroQuaternius`, not committed |

Fetch scripts:
- `Tools/Fetch-HeroSurfaces.ps1` fetches the surfaces: 23 files, 69.6 MB, `SourceAssets/Surfaces/Heroes/manifest.json` with URLs, real-world sizes and hashes.
- `Tools/Fetch-CMUMocap.ps1` fetches 02_05, which is recorded in the CMU manifest.

**Why the fighter's motion is authored rather than downloaded.** No free, usable motion library has a realistic sword-and-shield fighter:
- Quaternius's sword clips are leaping, spinning action-game moves.
- CMU's "swordplay" (subject 2) is casual two-handed play-acting.
- Mixamo's sword-and-shield pack needs your Adobe sign-in, and its terms forbid redistributing the raw files, which this public repository would do.
- MoCap Online's free pack needs a store checkout with your email and has a custom licence.
- The paid packs (ActorCore and others) were not bought.

So the motion is made in Blender by `Tools/build_hero_anims.py` as physical motion, not posed keys (below).

## The heroes (`Tools/build_temple_heroes.py`, `SourceAssets/Heroes/heroes.json`)

Each body is an MPFB human made with the human kit (`Tools/build_npc_humans.py`, now importable as a module with an `extra` hook; its NPC output is unchanged). Everything else is built over that body.

### Plate
Plate is a shell of the body region, pushed out over the layers under it. It is then smoothed until it is a plate's simple convex form, with no pecs and no navel, while holding a set clearance off the body. Then it gets:
- thickness;
- a rolled rim swept round every edge;
- rivets set in from the rim.

Each piece rides one bone, or a fixed blend of two, as real plate does. Lames are separate pieces, each on its own blend, so they slide over one another.

### What each wears and carries
- **Fighter (187 cm, 162,520 triangles):**
  - an arming gambeson and mail hauberk with a mail skirt (kit pieces);
  - a keeled breastplate and backplate, four fauld lames, two-lame tassets;
  - four-lame pauldrons, rerebraces, winged couters, vambraces;
  - suede gloves with flared cuffs and back-of-hand plates;
  - cuisses, winged poleyns and greaves;
  - an open-faced bascinet with a rolled face opening and a mail aventail;
  - a sword belt with a scabbard and gilt chape;
  - a short, full beard.
- **Wizard (179 cm, 97,320 triangles):**
  - a deep-blue velvet robe with bell sleeves, falling to the floor;
  - a mantle with a broad gilt-thread band, a rope girdle with tasselled ends, a leather pouch;
  - a tall hat with a drooping brim, its crown leaning and bent over, with a band;
  - long white hair (MakeHuman's long01, greyed) and a long beard;
  - a 1.85 m crooked, knotted staff whose roots claw round a glowing sea-green gem.
- **Ranger (172 cm, 74,874 triangles):**
  - a linen tunic and leather jerkin;
  - a hooded wool mantle with the hood down;
  - a baldric, and a quiver with nine fletched arrows;
  - a belt and knife sheath, a bracer on her bow arm, fingerless gloves, tall boots;
  - an auburn ponytail;
  - a 1.78 m D-section longbow with a bound grip and a string, and an arrow at her fingers.
  - The bow was strung inside-out until October 23 (user: "the archer's bow is backwards"): its limbs curved away from the string, which ran in a V to her side. Now the limbs curve toward her, their tips 15 cm out, and the string runs straight from tip to tip at the 15 cm brace height. The nocking point and her clips are unchanged.

### The props and their bones
The props are skinned into each hero's mesh on extra bones (`docs/RIG-CONTRACT-V1.md` is not affected: heroes have their own skeletons):
- `weapon_r` and `weapon_tip`: the sword or staff, and the blade's point (the game reads the cut from it);
- `shield_l`;
- `weapon_l`: the bow;
- `bowstring`: its nocking point, drawn by the clips;
- `arrow_r`: the nocked arrow, hidden once loosed.

The sword:
- a 79 cm blade with a diamond section, a fuller and distal taper;
- a 20 cm gilt cruciform guard, a cord-bound leather grip and a wheel pommel.

The shield is a curved heater: painted leather over boards, with a steel rim, rim nails and enarmes.

**How it is carried (October 23).** The user: "the fighter's arm is in the shield in an unrealistic impractical angle". It had been mounted with the forearm running up its length, the elbow at its top edge and the hand at its point, so the elbow stood high behind it and the forearm hung vertical. It is now carried as a heater was:
- the forearm lies across its back a little above the middle, rising 12 degrees to the fist, its top on the thumb's side;
- the fist is on the grip by the middle, and the elbow strap is by the outer edge;
- the enarmes are bands from the boards round the arm (the forearm strap over the vambrace, the grip in the fist) with a leather pad under the forearm;
- the boards are 15 cm out from the forearm's axis and bow 6 cm to the rim (it was 11), so the rim is clear of the elbow's couter.

### Surfaces (`Tools/import_temple_heroes.py`, `/Game/Characters/Heroes`)
Every hero piece has a vertex colour baked by Cycles: R is ambient occlusion, G is edge wear (convexity), B is grime. The masters use it:

| Master | For | How it uses the vertex colour |
|---|---|---|
| `M_HeroMetal` | Plate, rims, rivets, blade, gilt | Brighter and more polished at the edges, duller in the hollows |
| `M_HeroCloth` | Leather, velvet, wool, rope, wood | Poly Haven weave; paler at the edges, grime in the hollows |
| `M_HeroSolid` | The gem, string and fletching | Occlusion only; the gem is emissive |
| `M_HeroSkin` | Skin | Subsurface (light through the ears and nose) |

The project's reflections fall back to the outdoor sky (`r.ReflectionMethod` 2), so in this hall the metal's sky term is occluded (`SkyOcclusion` 0.16) and the plate shows the torches, not blue sky. The steel's albedo is 0.34. The first in-game passes at 1.0 and 0.5 read chalk-white, then chrome.

Each hero has its own skeleton (`SKEL_Hero<Name>`) and four LODs:

| Hero | LOD vertices |
|---|---|
| Fighter | 119,575 / 37,128 / 21,440 / 12,690 |
| Wizard | 68,490 / 26,633 / 15,666 / 8,023 |
| Ranger | 47,330 / 15,137 / 8,870 / 5,211 |

`Tools/Import-TempleHeroes.ps1 [-Clean] [-Only Fighter,fx]` imports them. The FX materials are kept once made: a `-Clean` run rebuilds them, because the running game module holds them.

## The motion (`Tools/build_hero_anims.py`)

### How it is made
Twenty clips at 30 fps, keyed on every frame:
- **The weapon leads.** A cut is the sword's path: grip positions and blade directions keyed in time and joined by Catmull-Rom curves on time knots.
  - The blade is slow in the wind-up, fastest at contact and decelerating through the follow-through.
  - The cutting edge always faces the way the blade moves (taken from the tip's velocity), so every cut lands edge first.
- **The body drives it.** The hips turn first, the chest a few frames after, the arm last. The front foot steps and plants just before contact. The weight goes back in the wind-up and forward through the cut, and the knees sink as it lands.
- **Analytic two-bone IK** works from joint positions, because the FBX bones' tails are not reliable:
  - elbows and knees bend as hinges;
  - the forearm takes the hand's twist (a wrist cannot twist);
  - the clavicles lift and reach with the hand.
- **The hand follows the weapon.** It is placed from where the sword (or shield, or bow) must be.
- **The shield goes where his arm can hold it, clear of him** (October 23). The keys give its centre and facing; the solver then moves it, keeping the facing, until:
  - the elbow is exactly the upper arm's length from the shoulder (else the forearm, and the shield on it, would come out turned);
  - the elbow bends at least 60 degrees;
  - its boards are clear of his torso, hips, thighs, left shoulder, head and his own blade (capsules from that frame's joints).
  Each frame starts from most of the last frame's move, so it is smooth and eases back to the keys once clear. The keyed facings turn up to 30 degrees to his left (fully when facing his foe), so the forearm points forward across his body; in guard the elbow is forward of his ribs and bent about 60-65 degrees, the shield's top at his eyes.
- **The sword is kept out of him too** (October 23), except from 0.1 s before contact to 0.13 s after, where the cut keeps its measured path. The hand stays 30 cm from his spine and 27 cm from his head, and the blade turns about the grip, as little as will do, to clear his torso, hips, thighs and head.
- **Hits taken are springs.** A blow is an impulse through three damped springs: the hips go first, the chest whips after them, the head snaps last. Big ones add reactive steps and arms thrown out for balance.

### Measured results
Measured from the solved rig and written to `Unreal/Chuck3D/Source/Chuck3D/HeroClipData.h`. Contact points are relative to the fighter: ahead, to his right (− his left), and height.

| Clip | What | Contact | Contact point (cm) | Blade tip |
|---|---|---|---|---|
| CutDown | Forehand descending diagonal, step in | 0.433 s | 93 / 8 / 137 | 28 m/s |
| CutFlat | Level forehand at the throat (takes a skull off) | 0.433 s | 90 / −14 / 154 | 30 m/s |
| Overhead | Heavy blow from behind the head, long step, deep crouch | 0.5 s | 92 / 14 / 151 | 29 m/s |
| CutRise | Low right to high left, up on the toes (for the beholder's eye stalks; not drawn against skeletons) | 0.4 s | 77 / 4 / 199 (tip 218) | 30 m/s |
| Bash | Shield punched out with a step | 0.3 s | — | — |

Tip speeds of 26-32 m/s are in the range measured for real sword cuts.

The other clips:
- **Fighter:** Guard (a breathing 6.4 s loop), BlockHit, HitLight, HitHeavy (two steps back), Advance and Retreat (in place; the game moves him by their travel).
- **Wizard:** Idle (leaning on the staff), Channel (the binding: braced, the staff thrust at the dais, the left hand reaching out and trembling), Cast (the bolt leaves his left palm at 0.34 s), HitLight, HitHeavy.
- **Ranger:** Idle (arrow nocked, bow low), Shoot (below), HitLight, HitHeavy.

The ranger's Shoot runs 2.6 s:
1. Raise and draw to the corner of her jaw; the string follows her fingers.
2. Hold, then loose at 1.0 s. The string snaps forward with a damped twang, the hand flies back past her ear, and the arrow is gone.
3. Reach over her right shoulder to the quiver; an arrow appears in her fingers.
4. Bring it round, nock it, and return to ready.

Contact sheets and key-frame strips are in `Local/HeroReview` (anim1-4).

## The fight

The fight is run by `ATempleSanctumBattle` (TempleSanctumBattle.h/.cpp), `ATempleHero` (TempleHero.h/.cpp) and `ATempleSkeleton`'s sanctum mode. The heroes use the skeletons' clip player (`FSkeletonAnimator`), which gained a twisting flinch, a hideable head and a time scale for hit-stop. The existing skeletons are unchanged.

### The staging
Three heroes stand on the hall floor before the dais, facing the beholder's spot over its front: the fighter in front, the wizard behind him on his left and the ranger behind him on his right (the 2D arc).

Twelve skeletons lie in reserve among the bones round the dais end. While Chuck is in the sanctum (the hall, the gallery or the end of leg 4), three are on their feet at a time:
- each goes for a hero, mostly the fighter (two at him at most);
- each punches at its hero from about a metre (its fist reaches 63 cm ahead of its hips);
- each strikes again after 1.6-3 s.

The fallen lie where they fell for 14 s, then go back into reserve where Chuck is not looking.

Bones are evaluated every frame only while the fight is on.

### The fighter
He holds his ground and faces the nearest skeleton on its feet. When it is 64-118 cm off, he cuts:
- the descending diagonal, the level cut or the overhead (1 : 1 : 0.6);
- or a shield bash if it is about to strike and close.

**The hit test.** A cut lands only if the blade itself meets the bones. The blade's cutting length is swept from the frame before to this one in five segments, against the skeleton's spine (16 cm round), throat and skull, inside 0.1 s before to 0.12 s after the measured contact. The test uses the clip time the bones actually show: the mesh evaluates after the hero's tick. A frame hitch therefore cannot make a cut miss, which it did in the first trials.

**What a hit does:**
1. Hit-stop: both freeze for 75-110 ms (the overhead longest).
2. The bone crack plays on the frame of contact.
3. 22-34 bone splinters fly the blade's way, with dust.
4. The level cut, if it passed the throat or skull, takes the head off. The skull flies, tumbles, bounces and rolls; the headless body takes a breath before its knees go.
5. A faint smear of the blade's passage hangs for 0.11 s.

The bash knocks a skeleton back about 60 cm and does not kill.

**Blows on him:**
- In guard and facing the blow, he takes it on the shield (BlockHit), and the striker recoils.
- In his wind-up, the shield takes it with a jolt and the cut goes on.
- Past contact, or turned away, he is hit and reels: the twist turns him the way the blow came from.

### The ranger and the wizard
The ranger shoots skeletons going for her or the wizard, and the fighter's only while they are still more than 3.2 m from him. Her arrows fly ballistically at 50 m/s, led for the target's movement, stand in the bone they hit, and drop it on the third. She pauses 1.2-3 s between shots.

The wizard works the seal over the sea tear (since October 25; [below](#sealing-the-sea-tear)), once the binding at the dais. A sea-green light and a hum come from his gem. Every 7-12 s he turns a bolt on a skeleton going for him or the ranger. The bolt homes in at 15 m/s, trailing sea-green fire (the Unity Labs flipbooks, recoloured), and bursts in a flash that blasts the skeleton down.

### Arms and wrists
User request, October 25, 2026: "The archer's arm goes through herself often, fix that, and the fighter often holds his sword in a way where his hand/wrist is strangely bent, also fix this".

**Measured first** (`Tools/check_hero_limbs.py`, new). Each clip is solved as the generator solves it.
- **Arms:** every frame's skinned forearm, hand and finger triangles are tested against the torso, head, hips and what is worn on them.
  - Each vertex belongs to the bone weighting it most.
  - Triangles sharing a vertex in one garment, a sleeve meeting its body, are not counted.
- **Wrists:** the hand's turn relative to its own forearm, as flexion/extension and radial/ulnar deviation.
  - Limits: flexion 62°, extension 55°, radial 20°, ulnar 32°.

**What it found:**
- **The fighter's guard** (the start and end of every clip): the fist sat at his shoulder with the sword upright. The grip leaves the blade 60° off the forearm, so the only straight-wrist forearm needed an elbow the arm couldn't reach. The solver's fixed elbow direction left the wrist bent about 90°, and up to 120–180° in cuts and the bash.
- **The ranger's bow** was mounted with its string out of her palm. Aiming the string back at her turned her fingers across her: a wrist bent about 100° at every draw, and her bow arm folding into her chest after the loose.
- **Her ready stance** held the drawing hand on the string 12 cm before her belly: the forearm in her torso and her sleeve in her jerkin.
- **The reach for the next arrow** went through her neck and ponytail, because the quiver's mouth is right behind her head.

**The fixes** (`Tools/build_hero_anims.py`; all three heroes):
- **The elbow** (`choose_elbow`): with the shoulder and wrist fixed, the elbow can only lie on a circle. Each point on it is scored by:
  - the wrist's bend, the forearm against the hand's own axis;
  - the upper arm and forearm going into the torso, hips or head (each hero's girth: fighter 20 cm, wizard 17, ranger 19);
  - a little for leaving the keyed elbow direction, and for jumping from the last frame.

  The fighter's shield arm keeps its exact elbow (the shield's mount needs it).
- **The fighter's sword:** its roll about its own blade is chosen with the elbow for a natural wrist. The blade still points exactly as keyed.
  - Through each cut's contact window, it may turn at most 45° off the leading edge, either edge, since the sword is double-edged.
  - The cuts' contact points moved under 1 cm.
- **The fighter's keys:**
  - The guard's fist is 8 cm further forward, off his pauldron.
  - The bash carries the sword hand forward with his shoulder.
  - The flat cut's follow-through crosses his chest, not his face, and its recovery lifts the blade upright first.
  - The rising cut's wind-up goes round his right side (it swung through a reversal), and it finishes high in front of him rather than over his head.
  - The overhead cut finishes out in front, not against his belly.
- **The ranger's bow:**
  - Her fist holds it as an archer does: the grip across the knuckles, the hand continuing her forearm, from where the elbow sits.
  - The bow is turned on its grip separately (its bone, `weapon_l`, is keyed apart from the hand). Its tilt is squared to her forearm.
  - Her ready stance holds it out before her middle, so both arms clear her jerkin.
  - The drawing hand's way is set from her shoulder to the string.
- **Her full draw:** the anchor sits a few centimetres off her jaw.
- **Her quiver reach:** she takes an arrow by its fletching, above the quiver over her right shoulder, the hand continuing her arm, the path out past that shoulder.
- **The wizard:**
  - His left fist on the staff turns about the shaft to continue his forearm (it was bent about 115° in his idle).
  - The bolt cast and the hit guard keep the hand off his robe and hat.
  - The sealing palm is pushed less far back.

**Results** (`Local/limb-check-final.log`, then the later key changes re-checked):
- Wrists: the fighter's guard, blocks, hits, steps and bash are within ±10°. The ranger's bow hand is within about 10° through the whole shot; her drawing hand about 25–35° at full draw (relaxed fingers on the string).
- Arms: the ranger's ready stance and hits have no arm in her body.
- Shield and sword: `check_hero_shield.py` still finds no contact in any frame.
- In game: every cut trial lands on its first swing, the flat cut takes the skull, and all 18 sanctum trials pass.

**Still above the limits:**
- **The fighter's cuts:** a few frames each, in fast wind-ups and follow-throughs: ulnar deviation up to about 60° (the down and flat cuts' finishes), radial about 50–65° (the flat and overhead wind-ups), and 78–80° for two frames at the rising cut's contact.
- **His thumb** grazes his pauldron's rim for a few frames as cuts return to guard.
- **The ranger:**
  - Her sleeve brushes her mantle as she raises the bow (cloth on cloth).
  - Her glove touches the quiver's arrows (meant to).
  - Her drawing wrist bends to about 69° in the reach for the next arrow.
- **The wizard's** sleeve brushes his beard when he casts. His unused Idle clip has his left arm through his beard.

### Sealing the sea tear
User request, October 25, 2026: "Do the animation for the wizard attempting to seal the sea world rupture" (the tear to his left that the user named on October 24 as the one he is trying to seal; [CHULT-TEMPLE-WIND.md](CHULT-TEMPLE-WIND.md#the-sanctums-ruptures)).

His work is now the seal, not a binding at the dais. When nothing is coming at him or the ranger he turns to face the tear (its middle about 2.5 m ahead, its rim about 1.4 m) and works it.

**Two new clips** (`Tools/build_hero_anims.py`, `W_Seal` and `W_SealStrain`; reviewed in Blender, `Local/SealReview/r3`):
- `Seal`, a 6 s loop:
  - A wide braced stance, left foot forward, leaning over the tear.
  - The staff held up and out in his right hand, the gem high and leaning over the rim.
  - The left arm pushed out toward the tear, palm to it, fingers spread, weaving slow circles (0.5 Hz) and trembling with the strain.
  - Twice a loop he gathers the threads (the hand rises, turns down and closes) and drives them down: the palm opens and pushes, the body dips and the staff stabs toward the tear.
  - His head looks down at the edge nearest him.
- `SealStrain` (1.8 s), played when the tear surges:
  - He is wrenched toward it a stumbling half step, the hand dragged down and the staff dipping.
  - He heaves back, his fist clenched to his chest, leaning away on his braced back foot.
  - He works back into the seal.

**The seal** (`ATempleSeal`, TempleSeal.cpp; spawned by the battle over the tear):
- **The threads:** while he works it, five threads of sea-green light arc from his gem and three from his open hand down to the knitting edge. They sway, and are plucked taut in a surge. Light runs down them.
- **The web** (`M_TempleSealWeb`, `Tools/create_beholder_fx.py`): a disc that follows the tear's ragged rim. A woven lattice of light (rings, spokes, a crossed weave, a band of runes) knits inward from the rim at about 4.5% of the radius a second, its edge sparkling, to at most 82%. It never closes in the fight. In the Fireball cutscene (October 26) he turns from it to the beholder.
- **Fraying:** left alone (he casts, is struck or is snuffed), it frays back slowly.
- **Surges:** the tear fights it every 6–10 s once there is web to fight (it first gathers itself for 5 s).
  - The sea bursts up through it in spray, a light flash and a rushing roar (`SFX_SealSurge`).
  - It tears back 12–24%.
  - He strains (`SealStrain`).
- **The beholder's cone** (antimagic) shatters the seal: the web flies apart in shards of light with a glassy crash (`SFX_SealBreak`), and his binding is snuffed for 4 s.
- **His light:** a sea-green light over the tear grows with the web.

**Other changes:**
- He turns a bolt on the beholder less often (every 13–19 s): the seal has most of him.
- Bolts at skeletons coming for him or the ranger, and his reactions, are unchanged.
- The two sounds are in `Tools/gen_beholder_sfx.py` (`--only SFX_Seal`) and imported with `CHUCK_SFX_STEM=SFX_Seal Tools/import_beholder_sfx.py`.

**Test:** `wizard_seal` in `-ChuckSanctumTest`:
- He works the seal: it knits past 30%.
- A forced surge: he strains.
- The beholder's cone at him: it shatters, below 10% after.
- Stills from four sides (`Sanctum_wizard_seal_*`).

### Their lines
User request, October 25, 2026: the trio's ElevenLabs takes of their 2D lines (`data/dialogue/temple_sanctum.json`), "add their speaking to the game but hold off on making it the whole complete part yet (for instance dont do the fireball animation yet or the beginning to cast fireball or any cutscene scene etc). for gaps between lines that are clearly needed to be together, add a fair amount of time between the lines so that the player isnt rushed through this part, and wait awhile after player entrance to start."

**The lines** go through the NPC voice pipeline ([NPC-VOICE-PLAN.md](NPC-VOICE-PLAN.md)) as three new speakers, `HeroFighter`, `HeroWizard` and `HeroRanger`:
- Their takes are under `SourceAssets/NPCs/Voice/Hero*/source`; `dialogue.json` lists them as `scene` lines (spoken by the game, not by talking to them).
- `Tools/build_npc_voice.py` levels them like every NPC line (-18 dB speech, peaks -1 dB). The wizard's "......" is a quiet breath: a new `max_gain_db` keeps it at +6 dB instead of the +14 dB the levelling wanted.
- `CHUCK_VOICE_ONLY=HeroFighter,HeroWizard,HeroRanger Tools/import_npc_voice.py` imports them to `/Game/Art/Audio/Voice/Hero*/VO_Hero*_<id>`. They stream in with the battle's other assets.

**The script** (`TempleHeroTalk.cpp`, owned by `ATempleSanctumBattle`). The clock runs only while Chuck is in the sanctum (the hall, its gallery, the end of the last leg). Each wait is counted from the end of the line before.

| Wait | Who | Line | Length |
|---|---|---|---|
| 8 s after he comes in | Fighter | "Hold the line! It bleeds - I have seen it bleed!" | 4.2 s |
| 2.8 s | Wizard | "Keep its eye off me! The binding needs another minute!" | 4.7 s |
| 2.8 s | Ranger | "I'll cover you! We have to keep pushing forward!" | 3.5 s |
| 24 s | Ranger | "Wait, I know those sigils, you can't cast that here!" | 4.1 s |
| 2.2 s | Fighter | "... we're too close to an astral rip, we don't know what will happen" | 4.2 s |
| 2.2 s | Wizard | "I have to try, we're out of options!" | 3.2 s |
| 1.6 s | Wizard | "......" | 2.5 s |
| 1.2 s | Wizard | "FIREBALL!!" | 1.9 s |

- The calls start about 8 s after he enters, and the argument about 46 s after he enters. The 2D game starts the argument 24 s after Chuck is sealed into the hall. There is no seal here yet, so the 24 s count from the end of the calls.
- Each line plays from its hero's head. It can be heard across the hall and from the gallery: full volume within 15 m, fading out over 50 m.
- The subtitles are the NPCs' (one cue at a time, no speaker name). The boss music ducks under each line, as under the NPCs.
- The speaker's jaw opens with the line's loudness, up to 9°. The ranger's lips part; the fighter's and wizard's beards hide most of it.
- **Restarting:** if he is out of the sanctum for 4 s, or dies and respawns, the scene starts again from the top when he next comes in (as the 2D room reloads).
- **The Fireball (October 26):** the argument is now its cutscene ([CHULT-TEMPLE-BEHOLDER.md](CHULT-TEMPLE-BEHOLDER.md#the-cutscenes)). When the fourth line is due, `TickTalk` asks the scene to start, and holds the line until the beholder is between attacks. The lines then play with their own gaps, and the scene shoots whoever speaks. The first line now falls in the entrance cutscene's last shot. All the waits are in `Script` at the top of `TempleHeroTalk.cpp`.

**Test:** `hero_talk` in `-ChuckSanctumTest`. It restarts the scene with the waits at a quarter, then checks that:
- all eight lines play in order, each with its sound playing and a subtitle;
- each hero's jaw moves (over 3°);
- the music ducks under 0.5.

`Local/hero-talk-1.log` (uncooked): `spoken=8/8 ordered=1 sounds=8 subtitles=8 jaw=9.0/9.0/7.3 duck_min=0.35`; stills `Sanctum_hero_talk_*`.

### Sounds
`Tools/gen_hero_sfx.py` makes 37 stdlib-synthesised originals (`SFX_Hero*`):
- a sword whoosh (its Doppler sweep peaking at contact) and the blade going into bone;
- a blow on the shield, a fist on plate, armoured steps, the shield bash;
- the bow's creak and twang, an arrow's flight and its impact in bone;
- the bolt's cast and burst;
- a seamless 4 s channelling hum;
- skull bounces.

`Tools/import_hero_sfx.py` imports them.

### Chuck
None of it is aimed at him. The heroes ignore him, the sanctum skeletons ignore him, and no blow, arrow or bolt hurts him yet. That is the battle's work, with the beholder.

## Tests

**The shield and sword check** (October 23): `blender --background --python Tools/check_hero_shield.py [-- Clip,Clip]`. It solves every fighter clip as the builder does and tests every frame's skinned meshes triangle against triangle:
- the shield's face and rim against everything else he wears or holds, including the left forearm under it and the sword;
- the sword's blade, guard and pommel against his body, less contacts within 15 cm of the sword hand (the grip).
It also reports the shield's largest move between frames.

Results after the changes (all 11 clips, every frame):
- **Shield:** no contact in any frame of any clip (`CHUCK_SHIELD_CHECK failures=0`). Before them, every frame of every clip had its rim through the left elbow and upper arm; in the cuts it also went through the chest plate, shoulder plates, beard, helmet and thighs.
- **Sword:** no contact in eight clips. In 12 frames of three recoveries, the crossguard grazes his own right shoulder plate or sleeve: CutFlat 31-34, CutRise 4-6 and 34-37, Overhead 12. Before, it went through his chest, thigh, mail skirt and helmet in the four cuts' wind-ups and recoveries.
- **Smoothness:** the shield's largest step is 16 cm and 20 degrees per frame, during the cuts' swings. In guard it moves 1 mm.

Clip changes made for this:
- **CutFlat:** the follow-through rises over his left shoulder instead of wrapping round at shoulder height, and the shield drops low by his left hip, tipped out. Its contact moved 0.6 cm (1.536 m) and its speed is 30.22 m/s.
- **CutDown:** the shield rises high on his left while he cuts low. The recovery comes back low across his front to his right.
- **CutDown, Overhead, CutRise:** the shield returns to guard only once the sword is back on his right.
- **The other contact points, times and speeds are unchanged.**

`-ChuckSanctumTest` runs its trials (the list is `Trials` in `TempleSanctumBattle.cpp`; `hero_talk` since October 25), and `-ChuckSanctumCapture` adds stills to `Saved/Screenshots/.../Sanctum`. `-ChuckSanctumOnly=<trial>` runs a single trial.

1. Each blow on a fresh skeleton placed in his measure: `cut_down`, `cut_flat` (it must take the skull), `overhead`, `bash`. Since October 23 every skeleton left from the trial before goes back to reserve first. The forced cut is the fighter's next swing, and a leftover still chasing him had drawn it out of range, so the level cut's skull was taken only when a later swing happened to be a level cut.
2. `block`: a blow on the shield.
3. `hit_behind`: a blow on his back.
4. `arrows`: three arrows drop a skeleton.
5. `bolt`.
6. `portraits`: stills.
7. `battle`: 45 s of the battle running.

`Tools/Verify-Package.ps1` now runs it in the packaged build.

## Evidence (October 23)

- **Uncooked, `-ChuckSanctumCapture`:** all ten trials pass, in four consecutive full runs after the last behaviour change (`Local/sanctum-test-10.log` to `-14.log`).
  - Every forced cut lands; the level cut decapitates.
  - Blade speeds at contact are 28-32 m/s without screenshot hitches.
  - In the free battle, 8-12 skeletons fall in 45 s, the fighter landing 4-10 cuts, with 105-131 sounds.
- **Stills:** `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/Sanctum` (not committed).
  - Each cut from the side, from 0.24 s before contact to 1.2 s after: the decapitation's skull is in the air at +0.16 s.
  - Portraits, head and figure, of each hero.
- **Packaged:** see HANDOFF for the candidate's verification.

## What was tried and dropped

- **The motion sources above:** the Quaternius libraries, CMU swordplay, Mixamo and MoCap Online.
- **Steel at full albedo, then at half:** it read chalk-white, then chrome, under the torches with the sky in its reflections.
- **Skeletons stopping at 78 cm:** they crowded into his shield. They stand off at about a metre now (their measured reach).
- **The level cut at chest height (1.42 m):** it took collarbones, never a head. It is at the throat now (1.54 m).
- **The descending cut's contact at 1.56 m:** it passed over a crouched skeleton's head. It is at 1.37 m, centred.
- **A block that cancelled his own cut** when struck in the wind-up.
- **Arrows dropping a skeleton on the second:** the ranger took nearly all of them before they reached the fighter. Now three arrows, and she leaves his foes to him.
- **Clip FBX files carrying the whole armoured mesh:** about 170 MB. They now carry the body and the prop pieces, 26 MB.
- **Re-imported copies of the human kit's fabrics:** now reused from `/Game/Characters/Humans/Fabrics`.

## Not done, and known flaws
- **Their lines (October 25):**
  - Heard only in automated runs. No person has played through the timing.
  - Leaving the sanctum or respawning restarts the scene. This was reasoned through in the code, not tested.
  - The jaw is loudness-driven, not lip shapes.
  - Nobody turns to face whoever is speaking.
  - The calls can overlap the beholder's roars.

- **The seal (October 25):**
  - Reviewed in stills only; it has not been seen in motion by a person.
  - The threads' cores read almost white, the sea-green mostly in their glow.
  - The web's weave reads as a grid more than a woven net up close.
  - The surge and break sounds are synthetic and unheard.
  - No foot sliding was measured in `SealStrain`'s half step.

- The escape onto the ship. (The beholder's fight, its hazards for Chuck and the Fireball are built: [CHULT-TEMPLE-BEHOLDER.md](CHULT-TEMPLE-BEHOLDER.md).)
- **Faces** are MakeHuman's baseline (the same as the NPCs'): soft and young for a veteran fighter, smooth for an old wizard. They need sculpted detail, skin normal maps and better brows. This is the largest remaining gap to "much more detailed".
- **Hair and beards** are cards (the kit's), not strand grooms.
- **Clipping:**
  - Plate can intersect at extreme poses (the pauldrons in the overhead's wind-up), and the ranger's mantle clips her arm at full draw.
  - A striking skeleton's arm passes into the fighter's helmet or shield at close range.
  - The crossguard grazes his own right shoulder plate or sleeve in 12 frames of three recoveries (above). Clearing the blade off his arm drove it into his forearm, so it was left.
- **Movement:**
  - The fighter only steps forward and back along his facing. He turns on the spot by rotating, with his feet sliding a little.
  - Heroes do not walk.
- **Flying things:**
  - Arrows and bolts ignore walls and pillars.
  - Splinters and skulls bounce only on the floor plane, so a skull can roll through a plinth.
- **The rising cut** is authored for the beholder and is not yet used.
- **Sound:** since October 23 the sanctum plays the user's "boss_battle" score, from the moment Chuck enters, not tied to the fight's events.
