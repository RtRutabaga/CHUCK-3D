# Chult — the beholder of the sanctum

User requests (Claude):
- October 23, 2026: "start creating the beholder, it should look really cool and detailed".
- October 24, 2026: "Make the beholder look more like this, here also is a clay render as a reference, use free resources where helpful". The user supplied a colour concept and a clay render.
- October 25, 2026: "create the attacks and movements the beholder will use ... dont make the cut scenes yet but make the beholder attack animations that will work for this. they should look good, be varies and consistent with dnd 5e beholder attacks, and one of them should be an area of effect attack that can hurt Chuck if he's in the way". The cutscenes (a brief one on entering, the wizard's Fireball at the end) are planned, with a playable fight between them. See [The fight](#the-fight).

The 2D game's Phase 6 final chamber has three adventurers fighting a beholder and skeletons; Chuck only witnesses it ([CHULT-TEMPLE-HEROES.md](CHULT-TEMPLE-HEROES.md)). The 2D sprite (`assets/sprites/npcs/beholder.png`) gave the first pass its direction: a mauve orb with one red-irised eye, a toothed grin and eyes on stalks.

Since October 24 the user's references lead instead. The second pass has:
- a lumpy, brain-like cranium tapering to a chin;
- one great green eye with a slit pupil, deep under a massive brow, with stacked bags beneath;
- a huge gaping maw filling the lower face, with thick lips, ridged red gums, rows of long ivory fangs and a broad tongue lolling out over the lower teeth;
- thick, wrinkled eyestalks hooking forward like question marks, each cupping a big slit-pupilled eye;
- wet maroon skin, tan-pink on its ridges and dark in its folds.

It hangs alive where the heroes look, and since October 25 it fights them ([The fight](#the-fight)).

- October 26, 2026: "finish the beholder encounter, ending on the 'rubble map' with crawlable crevice, we'll do the entry to ship later". The entrance and Fireball cutscenes are built ([The cutscenes](#the-cutscenes)); the Fireball throws Chuck into the rubble map ([CHULT-TEMPLE-RUBBLE.md](CHULT-TEMPLE-RUBBLE.md)).

## The model (`Tools/build_beholder.py` with `Tools/sdf_kit.py`, Blender 4.5.14)

`blender --background --factory-startup --python Tools/build_beholder.py [-- --review DIR] [--quick]` writes `SourceAssets/Enemies/Beholder/`:
- `SK_Beholder.fbx`: skinned, no clips;
- textures: `T_Beholder_D`, `_N` and `_ORM` (4096²), and `T_BeholderEye` (2048²);
- `beholder.json`.

A full build takes about 9 minutes; `--quick` takes about 5.

### Free resource
The skin's finest detail is ambientCG's **Leather037** (CC0): a pebbled leather grain with a fine wrinkle network.
- `Tools/Fetch-BeholderSkin.ps1` puts its displacement and normal maps (2K JPG, 2.7 and 6.1 MB) in `SourceAssets/Surfaces/Beholder`, with a manifest of source URL and hashes.
- The displacement map, box-projected at 70 cm a tile so its cells are about a centimetre, is the bump in the bake, and it darkens its own pits a little.
- It replaced a procedural cell pattern that read as cracked glaze.

Other free resources were considered:
- **Fan beholder models:** they need sign-ins, and their licences and quality vary.
- **3D-print sculpts:** they have no textures or rig, and are often non-commercial.

The creature itself is modelled here.

### Proportions (cm; facing +X, Z up)
- **Head:** about 2.0 m wide and 2.4 m tall.
  - The cranium (centre z 44) rises well above the eye and carries 11 sunk lobes and five big lumps.
  - A jaw mass and a chin taper it downward.
  - A forehead mass bulges over the brow.
- **Great eye:** radius 33, centre (66, 0, 28), about a third of the face across and 37% down. It bulges past the face under the brow.
  - Lids: an almond, upper margin 30 degrees and lower −32 at the middle.
  - Iris: 32 degrees, with a vertical slit pupil.
- **Maw:** open at rest, 140 cm across at its corners (z −18).
  - Its upper edge dips to −24 in the middle; its lower edge drops to −90.
  - A cavity behind it, roofed clear of the eye's socket.
- **Eyestalks:** ten.
  - Placement: from the cranium's upper sides, sides and top, at azimuths ±14 to ±118 degrees.
  - Length: 86–102 cm.
  - Thickness: about 20 cm across, with fleshy collars where they meet the head.
  - Ends: a 15.5 cm bulb holding a 13 cm eye.

### How it is made
`sdf_kit.py` is the reusable part:
- signed-distance-field operations (union, subtraction, intersection, and since this pass a displacement);
- block-culled evaluation;
- surface-nets meshing over sparse blocks;
- Newton projection onto the surface.

`build_beholder.py` defines the creature:
- **Base:** ellipsoids for the cranium, jaw, chin and forehead.
- **Face features:** laid in front view and snapped onto the plain head's surface (`on_face`):
  - the brow ridge, thickest over the eye and overhanging it;
  - three stacked bags under the eye;
  - folds from the eye's corners down to the maw's;
  - furrows over the brow;
  - thick rolled lips;
  - folds round the chin.
- **Additions:** cheek masses at the maw's corners, 140 small warts, and five spikes along each side of the jaw.
- **Stalks:** each leaves the head along the surface's normal, swells up and out in an arc, then hooks forward and down. Its direction blends along a cubic Bezier, so it has no kinks. Its eye faces the front.
- **Carved out:** the eye's socket, the maw's opening and its cavity. Gums are added inside the lips, and each stalk's bulb gets an eye hole.
- **Fine mesh only:** a displacement of wrinkle folds round the eye and maw, furrows across the middle of the forehead, vertical ridges on the gums, soft lumps and toad-like pebbles.
- **Separate fields:** the two lids (thick shells with rolled margins and fold creases) and the tongue (a broad, grooved chain of ellipsoids curling out to one side).

Each part is meshed twice:
- **0.75 cm:** for baking.
- **3 cm** (1.8 cm for the lids and tongue): the game mesh.

The fine mesh carries vertex colour, roughness and the bump mask:
- **Colour:** maroon, with stalks a little more magenta. It is tan-pink on the brow and cheeks, and paler on fold-scale ridges and darker in folds, from the field's Laplacian at 4 cm. (At the pebbles' scale that read as camouflage.)
- **Wet places:** the socket's rim and the stalks' eye holes are wet and pink.
- **The maw:** deep red, with red gums and darker lips.
- **Other marks:** veins near the eye and paler warts.
- **Occlusion:** from the distance field. It is lifted inside the maw, which is lit only from outside in play.

Cycles bakes colour, normal and ORM onto the game mesh's Smart UV Project atlas.

Built directly, not baked:
- **Teeth:** 50 curved conical fangs in four rows.
  - Outer rows, upper and lower: long fangs, with great canines at ±34 cm (38 cm long) and ±30 cm (30 cm long).
  - Inner rows: shorter.
  - A few are broken.
  - Vertex colour runs from stained roots to ivory tips.
- **Eyes:** spheres with polar UVs. The iris texture is made in numpy:
  - gold round the slit, then yellow-green, then green, fibrous, with a dark limbal ring;
  - a yellowed sclera with branching blood vessels.
- **Cornea:** a 1.1 cm dome on the great eye.

### Result
- **Triangles:** 144,724; Unreal's LODs: 100.5k / 42.9k / 25.9k / 14.5k vertices.
- **Materials:** Skin, Eye, Cornea, Teeth.
- **Bones:** 80 (81 in Unreal, which adds the armature as a root).
- **Review renders:** `Local/BeholderReview/f3`:
  - clay and textured views matching the reference's angle;
  - front, side, back, below;
  - eye, maw, posed.

### Bones
| Bone | Role |
|---|---|
| `root`, `body` | Hover, pitch and roll |
| `jaw` | Hinged at the maw's corners. Everything below the opening's middle follows it, fading out behind and to the sides. The maw is open at rest |
| `tongue_0..3` | Along the tongue, from the cavity's floor to its tip outside |
| `lid_upper`, `lid_lower`, `eye` | All pivot on the eye's centre |
| `stalk{k}_0..5`, `stalk{k}_eye` | Six bones along each stalk, and its eye at its centre. Each bulb belongs wholly to its stalk's last bone |

## Import (`Tools/Import-Beholder.ps1` → `Tools/import_beholder.py`)
The import creates `/Game/Characters/Beholder`: the textures, `SK_Beholder` with 4 LODs, and four materials, rebuilt on each run.
- **`M_BeholderSkin`:**
  - Inputs: the colour times a `Tint` parameter (0.92, 0.84, 0.92), creases darkened a little by the baked occlusion, the normal (green flipped: Blender bakes OpenGL normals), roughness from ORM.
  - Stronger occlusion darkening made every pebble's pit a dark speck.
  - A subsurface model, in the first pass, turned the skin flat pink.
- **`M_BeholderEye`:** the iris texture, glossy. The iris glows faintly by a `Glow` parameter.
- **`M_BeholderCornea`:** black, translucent and lit per pixel: only highlights.
- **`M_BeholderTeeth`:** vertex colour times old ivory.

`DefaultGame.ini` always cooks `/Game/Characters/Beholder`.

## In the sanctum (`ATempleBeholder`, TempleBeholder.h/.cpp)
`ATempleSanctumBattle` loads `SK_Beholder` with its other assets and spawns the beholder at `BeholderSpot()`: 2.6 m over the dais, 1.5 m in front of it, facing the fighter. A `UPoseableMeshComponent` is posed each frame by a forward pass over the rig; each bone's own turn is set in its rest frame and carried by its parent's.
- **Hover:** it bobs about 7 cm, drifts about 20 cm, and pitches and rolls a little toward what it watches.
- **Turn:** it turns to face the fighter, swaying.
- **Great eye:** it darts between the heroes, a standing skeleton, and Chuck when he is near and before it (1.1–3.4 s each). The lids follow its gaze.
- **Blinks:** every 2.4–6.5 s; the upper lid travels 62 degrees.
- **Stalks:** they writhe; each eye watches its own target.
- **Maw:** it breathes a little shut and open around its rest gape. Every 7–13 s it snarls, gaping 10–14 degrees wider.
- **Tongue:** it sways and curls.
- **Eye light:** the iris glows faintly, and a dim pale-green light before the eye brightens in a snarl.

Lighting channel 1, shadows, no collision.

## The fight

Code: `TempleBeholderFight.cpp` (moves, rays, the bite, the cone, camera shakes) and `TempleBeholderFx.cpp` (effects), both on `ATempleBeholder`. The reactions are in `ATempleHero` and `ATempleSkeleton::Disintegrate`. The battle (`ATempleSanctumBattle`) loads the effects and sounds with its other assets, and lets the beholder fight while Chuck is in the hall (`SetFighting`).

The 2D game's sanctum battle (`src/entities/battle_hazards.py`) has eye rays cycling the heroes' lanes and a charged, screen-shaking cone of force across the hall. Both are hazards to Chuck, but neither is ever aimed at him. This keeps that and follows the 5e beholder (Monster Manual): up to three eye rays a round at different targets, more as legendary actions; one bite; the central eye's antimagic cone.

### Eye rays
Each ray comes from a stalk:
- The stalk curls toward its mark and its writhing stills.
- Its eye turns on the mark and gathers the ray's light: motes are drawn in, with a rising whine.
- The beam lances out (a white-hot core in a coloured sheath, two filaments, a crack of its own kind) and holds a moment, following what it struck.
- The stalk is thrown back by it and the head jolted.
- The hall is lit in the ray's colour along its length. Where it strikes there are sparks, a burst of tinted fire, and embers.

| Ray (5e) | Colour, look | On a hero | On a skeleton |
|---|---|---|---|
| Disintegration | green, needle-straight, crackling | always evaded (the 2D heroes survive to the Fireball): he lunges out of its way | gone to dust: green ash from every bone, a cloud of bone dust and fragments |
| Enervation | violet, twin threads twisting | withered: a heavy hit, violet seeping up from him | struck down |
| Telekinetic | pale blue, rippling | seized and hurled back about 2 m | struck down |
| Paralysing | gold, rigid, flickering | held rigid two seconds, gold crackling over him | struck down |
| Fear | crimson, shuddering | driven back, turned half away | struck down |

The fighter, in guard and turned to it, takes 60% of rays on his shield. Whether a ray strikes is drawn per ray (the saving throw). A ray that misses passes its mark and strikes the stone behind, or whatever stands there.

**Chuck:** rays are never aimed at him. But a beam that passes through him costs a point of Sanity: anywhere along it, the whole time it holds, as the stalk sweeps. A dodge roll through it saves him.

Volleys are 5e's round: two or three rays, each a different kind at a different hero, a third of a second apart. Single rays (legendary actions) come between them.

### The bite
1. **Wind-up** (0–0.55 s): it draws back and up, maw gaping, head tipped back, and roars. If the fighter has strayed it glides in toward him as it does, so the lunge stays under about 5.5 m.
2. **Lunge** (to 0.9 s): gathering speed, level, maw first, onto his shield.
3. **Snap:** the jaws shut on the shield with a crunch and worry at it for half a second.
4. **Return** (to 2.45 s): it goes back up to its place.

The fighter, turned to it, takes it on his shield and is driven back half a step. He answers with a rising cut at its face (`CutRise`), which lands if it is still in his reach. Caught turned away, he is hit and thrown back.

### The central eye's cone (the area attack)
The 2D game's "cone of force", as 5e's antimagic cone:
1. **Loom** (0–0.9 s): it rises and draws back. Its stalks curl away behind it (its own rays fail in its cone), and its face tips down along the line.
2. **Charge** (0.9–2.4 s):
   - The great eye opens wide and blazes.
   - Its gaze becomes a searchlight exactly the cone's shape (a spot light of its angle and reach). It swells, so the wedge of floor it will sweep stands out pale.
   - Motes stream back along the floor into the eye.
   - A swelling drone, air drawn in, a rising whine; the view trembles.
3. **Blast** (2.4 s): a heavy boom and a white flash.
   - A rim of force races out to 26 m in 0.6 s, a wall of dust and streaks thrown along the floor ahead of it.
   - The view shakes hard (by distance). It recoils.
4. **Recover** (to 4.4 s): the stalks uncurl.

Its cone runs from the great eye along its axis, 26° to each side and 26 m long. From over the dais it reaches about to the edge of the broken floor before the gallery, depending on where the beholder has drifted. What the front passes:
- **Chuck:** two Sanity, and he is flung back about 3 m (3.5 m measured). Since October 25 the throw is shortened when it would carry him into the rupture or onto a pier's island in it, so he lands at least his width plus 35 cm short of its edge. The user had been flung onto an island and trapped there. A dodge roll does not save him; being out of the wedge, or behind a pier or any stone (on a line from the eye to him), does. To a fey summon held here by magic, the antimagic is a blow.
- **Heroes:** thrown back (the fighter braced behind his shield). A paralysis is undone. The wizard's binding is snuffed for four seconds: the gem goes dark, the hum stops, no bolts.
- **Skeletons:** flattened.
- **Bolts in flight:** gone in a gutter of sparks. Arrows drop.

### Movement
- **Between attacks** it shifts across the front of the dais (±4.3 m, up to a metre up or down) every 3–5.5 s on a damped spring.
- **Banking:** a sideways drift rolls it; a surge pitches it.
- **Facing:** it turns quickly to face its marks while fighting.
- **Reeling:** a blow makes it reel on a spring (pitch, roll and turn); the eye blinks, the stalks flinch, and it bellows in pain. Blows come from the ranger's arrows (which stand in its hide, up to eight), the wizard's bolts and the fighter's counter.

The heroes strike back:
- With nothing coming at her or the wizard, the ranger looses at its face every few seconds.
- The wizard turns a bolt on it every 8–13 s.

### Its pattern
The fight starts the same way each time, for the entrance cutscene: a three-ray volley, a ray, the bite, a ray, a two-ray volley, the cone. After that:
- **The cone:** once 15 s have passed (a 30% chance each turn), and always by 24 s.
- **The bite:** when the fighter is near the dais and 12 s have passed (half the time).
- **Otherwise:** volleys (78%) or single rays, 1.3–3.4 s apart.

### For the cutscenes
Every move can be ordered:
- `OrderRays` (kind, target or point, strike or miss, delay, stalk), `OrderBite`, `OrderCone`, `OrderHover`.
- `SetFighting(false)` stops its own fight.
- `GetMove`/`GetMoveTime` and the timing constants (`ConeChargeAt`, `ConeBlastAt`, `BiteSnapAt` …) let a cut land on a beat.
- Camera shakes (`BeholderShake`) work with any view.

### Effects and sounds
- **Materials** (`Tools/create_beholder_fx.py`, `/Game/Art/ChultTemple/BeholderFX`). Rebuild with `CHUCK_BEHOLDER_FX_CLEAN=1` and no game running.
  - `M_BeholderBeam`: camera-facing ribbons built each frame (core, sheath and filaments, energy flowing out in knots).
  - `M_BeholderWave`: the blast's rim, brightest edge-on.
  - `M_BeholderSpark`: instanced soft points (motes, sparks, embers, ash).
  - `M_BeholderFlare`: Unity Labs' CC0 fireball flipbook (`T_JetFireBall`), tinted to the ray.
- **Lights:**
  - Five pooled point lights, for beams and flashes, on both lighting channels (some floor slabs are on the default one).
  - The cone's spot light, shadowed.
- **Sounds:** 22 original synthetic sounds. `Tools/gen_beholder_sfx.py` makes them (stdlib, seeded; about 13 minutes); `Tools/import_beholder_sfx.py` imports them with `-AllowCommandletAudio`.
  - Rays: charge ×2, the five rays, impacts ×3, disintegration.
  - The cone: its charge and its blast.
  - Its voice: roar ×2, snap ×2, growl ×3 (its snarls while fighting), hurt ×2.

### Tests
Run with `-ChuckSanctumTest` or `-ChuckSanctumCapture`; `-ChuckSanctumOnly=a+b` runs several.
- `beholder_rays`: one of each ray, scripted. A skeleton set before the fighter is disintegrated; the enervation, telekinetic and paralysing rays strike the heroes; the fear ray is evaded.
- `beholder_chuck`: a ray through Chuck on the floor costs one Sanity.
- `beholder_bite`: the lunge reaches the shield (closest gap under 60 cm), is blocked, and returns to its place.
- `beholder_cone`: Chuck in its path beyond the fighter (two Sanity), well off its line (none), and behind a colonnade pier (sheltered, none).
- `cone_edge` (October 25): the beholder brought 16 m off Chuck, who stands 2 m before the rupture's edge beside the second colonnade pier (the throw cut to 30%), then before open rupture (35%), then well back from it (the full throw, 3.5 m). He loses two Sanity each time, stays alive, and ends on the hall's solid floor every time.
- `beholder_fight`: 52 s of its own fight with skeletons coming, with at least 10 rays, 2 volleys, a bite, a cone, and a hero's blow on it.

## The cutscenes

Code: `TempleSanctumScene.h/.cpp` (`ATempleSanctumScene`, spawned by the battle). It follows the 2D Phase 6 final chamber: the heroes' calls on entering, a fight Chuck only survives, then the scripted Fireball. The explosion leaves him at (at most) half Sanity and throws him into the rubble map. Both scenes:
- take his control and the view, are letterboxed (no HUD; the heroes' subtitles stay), and Esc skips;
- frame each shot from where the cast stands that frame, and keep the lens at least 3 m from the beholder's centre (it lunges).

### The entrance (11.2 s, once a session)
When he first comes out onto the gallery:
1. **Over his shoulder** (0–3.4 s): rising to the balustrade, down into the hall at the beholder.
2. **Low behind the heroes** (3.4–7.2 s): up at the beholder over the dais; its opening volley comes in this shot.
3. **The fighter** (7.2–11.2 s): side-on, as he calls "Hold the line!": the first line, 8 s after Chuck comes in, as before.

The view then blends back to him and the fight is his to survive. A death and respawn on the gallery does not replay it.

### The Fireball
When the heroes' argument is due (their fourth line, 24 s after their calls), and once the beholder is between attacks:
- **The setup:** the beholder stops fighting and hovers in its place, its gaze held on the wizard. No new skeletons rise; the fighter and ranger go on with those standing. The wizard turns from the sea tear to the beholder (`ATempleHero::Script`, his `Channel` clip). Fiery sigils (`Sigils`) turn round him: two rings on the flags with twelve runes between them, and a small ring round his staff's head. They brighten through the argument.
- **The argument, shot by who speaks:**
  1. The ranger, over the wizard's shoulder: "Wait, I know those sigils, you can't cast that here!"
  2. The fighter, in profile behind his shield: "...we're too close to an astral rip".
  3. The wizard, close, from the beholder's side: "I have to try, we're out of options!"
  4. "......": the view closes on his staff's head. A mote of fire swells there, sparks are drawn into it, his gem's light grows, and the cone's charging drone sounds low.
- **"FIREBALL!!":** over his shoulder at the beholder. He throws (his `Cast` clip). The bead leaves his left hand at the clip's release (0.34 s) and streaks at 23 m/s, trailing fire and sparks.
- **The blast**, when the bead reaches the beholder:
  - 70 fire flares (Unity Labs' CC0 fireball flipbook, the beholder's `M_BeholderFlare`) and 120 sparks burst from it. A shell of fire races out to about 11 m in the first second. The Astral erupts purple out of the pool before the gallery.
  - A heavy boom: the cone's blast pitched down, a bolt's impact and the beholder's bellow. A hard shake. The music stops (`ADockGameMode::HoldMusic`, 10 s).
  - The beholder reels from it (`TakeHit`, 4×).
  - Shown wide from the nave before the heroes for 1.15 s.
- **At him:** behind him and low, a wall of fire rolls across the hall to him, with its own roar. White at 1.75 s.
- **The throw** (2.1 s): Sanity becomes at most half (2 of 5); it never heals him. He is put at the rubble map's **Temple Rubble** spawn. The hall goes back to its fight and the lines restart, for whoever comes back to it (a checkpoint).
- **Coming to:** the white lifts over 2.6 s from 3.1 s. Control returns at 3.7 s and the bars go. The temple's score comes back in about 10 s after the blast.

Skipped with Esc, the view goes straight to white and on to the throw. If he were somehow killed before the blast, the scene would give up and restore everything. The beholder's own fate is left open: it is engulfed and reels; the heroes return much later in the 2D campaign.

### Tests (sanctum trials)
- `entrance`: its three shots, then control back. `CHUCK_SANCTUM_TRIAL trial=entrance pass=1 entrances=1 shots=3 control_back=1`.
- `fireball`: the lines at shortened gaps, the scene's seven shots, the throw and blast, at most 240 fire flares, Sanity 2, in the rubble, control back.
- `fireball_skip`: Esc 2 s in; Sanity 2, in the rubble, control back.
- The other trials run with the cutscenes off (`SetFireballScene`/`SetEntranceScene`).

Verify-Package requires all three and `CHUCK_SANCTUM_SCENE_READY fx=1 camera=1`. Stills: `Saved/Screenshots/.../Sanctum/Sanctum_entrance_*`, `Sanctum_fireball_*`.

## Test
The `beholder` trial of `-ChuckSanctumTest` / `-ChuckSanctumCapture` passes when the beholder has:
- posed over 200 frames;
- moved a stalk eye over 8 cm;
- blinked and snarled;
- made three saccades.

`-ChuckSanctumOnly=beholder` runs it alone, with six stills in `Saved/Screenshots/.../Sanctum/Sanctum_beholder_*.png`.

`Verify-Package` requires:
- `CHUCK_BEHOLDER_SPAWN mesh=1 bones=81 stalks=10 eye_mat=1`;
- `beholder=1` in the battle line;
- the trial's pass;
- since October 25: `sounds=61 ... beholder_fx=4`, `CHUCK_BEHOLDER_FX ready=1`, and passes of `beholder_rays`, `beholder_chuck`, `beholder_bite`, `beholder_cone` and `beholder_fight`.

## Evidence
- **October 23:** the first pass. Packaged and promoted after `Verify-Package` (`Local/verify-package-20261024-113915.log`).
- **October 25:** the fight. Packaged and promoted as `BeholderFight` after Verify-Package (`Local/verify-package-20261025-114925.log`).
- **October 24:** the second pass, reviewed against the references in quick and full Blender renders (`Local/BeholderReview/r1`–`r3`, `f1`–`f3`) and in uncooked trials (`Local/beholder-trial-5.log` onward). Packaged: see HANDOFF.

## Not done, and known flaws
- **The battle:** the entrance and Fireball cutscenes are built since October 26 ([The cutscenes](#the-cutscenes)). The beholder has no death: the Fireball engulfs it and the white-out leaves its fate open. The heroes have no health: they reel and recover.
- **The cutscenes are unplayed by a person:**
  - Reviewed in stills and logs only (`Local/scene-trial-2.log`).
  - The fireball is mostly white-hot at its heart, with orange only at its edges.
  - The sigils are sparks, not drawn runes.
  - No new sounds were made: the boom, roar and drone are the beholder's own, pitched down.
  - The 2D game seals Chuck into the hall (the Astral breaks through behind him); here the pool before the gallery already cuts him off, and the 24 s count runs from the heroes' calls.
- **The fight is unplayed:**
  - It has been reviewed only in stills and logs, not played by a person.
  - Chuck's route through it is untried: its difficulty, and how readable the cone's searchlight is at play speed.
  - The synthetic sounds have not been heard by a person.
- **Flaws in the fight:**
  - The blast's rim can show a hard edge, seen from outside the cone.
  - The petrification, sleep, charm, slowing and death rays and the lair actions are not used.
- **Stalks** can pass through each other.
- **The maw's interior** is a plain red cavity with a dark throat.
- **Close-up flaws:**
  - Teeth are smooth cones without wear.
  - The tongue's underside can show a dark band.
  - Smart UV seams may show up close.
- **Sculpting:** the wrinkles are procedural folds, not hand sculpting. The reference has more, deeper folds across the whole face.
- **Weight:** 145k triangles at LOD 0, heavier than the heroes.
- **Texture size:** the bakes are 8-bit PNGs since the second pass (58 MB for the four maps).
- **Not yet seen by a person** in motion. No Xbox test.
