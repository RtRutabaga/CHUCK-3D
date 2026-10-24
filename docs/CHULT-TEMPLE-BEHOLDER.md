# Chult — the beholder of the sanctum

User requests (Claude):
- October 23, 2026: "start creating the beholder, it should look really cool and detailed".
- October 24, 2026: "Make the beholder look more like this, here also is a clay render as a reference, use free resources where helpful". The user supplied a colour concept and a clay render.

The 2D game's Phase 6 final chamber has three adventurers fighting a beholder and skeletons; Chuck only witnesses it ([CHULT-TEMPLE-HEROES.md](CHULT-TEMPLE-HEROES.md)). The 2D sprite (`assets/sprites/npcs/beholder.png`) gave the first pass its direction: a mauve orb with one red-irised eye, a toothed grin and eyes on stalks.

Since October 24 the user's references lead instead. The second pass has:
- a lumpy, brain-like cranium tapering to a chin;
- one great green eye with a slit pupil, deep under a massive brow, with stacked bags beneath;
- a huge gaping maw filling the lower face, with thick lips, ridged red gums, rows of long ivory fangs and a broad tongue lolling out over the lower teeth;
- thick, wrinkled eyestalks hooking forward like question marks, each cupping a big slit-pupilled eye;
- wet maroon skin, tan-pink on its ridges and dark in its folds.

It hangs alive where the heroes look. It does not fight yet.

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

Lighting channel 1, shadows, no collision. Nothing it does touches Chuck or the heroes yet.

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
- the trial's pass.

## Evidence
- **October 23:** the first pass. Packaged and promoted after `Verify-Package` (`Local/verify-package-20261024-113915.log`).
- **October 24:** the second pass, reviewed against the references in quick and full Blender renders (`Local/BeholderReview/r1`–`r3`, `f1`–`f3`) and in uncooked trials (`Local/beholder-trial-5.log` onward). Packaged: see HANDOFF.

## Not done, and known flaws
- **The battle:** eye rays and the cone of force (the 2D hazards), the heroes fighting it, being hit, its death, the Fireball, and sounds. It is silent.
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
