# Chult — the beholder of the sanctum

User request, October 23, 2026 (Claude): "start creating the beholder, it should look really cool and detailed".

The 2D game's Phase 6 final chamber has three adventurers fighting a beholder and skeletons; Chuck only witnesses it (see [CHULT-TEMPLE-HEROES.md](CHULT-TEMPLE-HEROES.md)). The 2D beholder (`assets/sprites/npcs/beholder.png`) is:
- a mauve-purple orb;
- one great eye with a red iris;
- a toothed grin;
- eyes on stalks;
- a hover above its shadow.

In 2D it fires eye rays down the adventurers' lanes and sometimes a cone of force across the hall.

This first pass builds the creature and hangs it, alive, where the heroes already look. It does not fight yet.

## The model (`Tools/build_beholder.py`, Blender 4.5.14, no downloads)

`blender --background --factory-startup --python Tools/build_beholder.py [-- --review DIR] [--quick]` writes `SourceAssets/Enemies/Beholder/`:
- `SK_Beholder.fbx`: skinned, no clips;
- textures: `T_Beholder_D`, `_N` and `_ORM` (4096²), and `T_BeholderEye` (2048²);
- `beholder.json`: sizes, bones, and where the eyes are.

A full build takes about 6 minutes. `--quick` (coarser, 1K textures) takes 3.

### Proportions
- **Orb:** 2.08 m across (radii 104 / 102 / 97 cm). The heroes are 1.72–1.79 m and Chuck is 65 cm.
- **Great eye:** 84 cm. It bulges 10 cm out of the orb, a little above its middle.
- **Grin:** 140 degrees wide, low on the front, corners turned up.
- **Eyestalks:** ten, 1.3–1.6 m long, rising from the crown. The 2D sprite shows five, a pixel-art simplification; ten is the creature's classic count. Changing it is one list in the builder.

Facing +X, centimetres, the orb's centre at the origin.

### How it is made
The creature is a signed distance field, meshed by surface nets. The vertices are then moved onto the true surface by Newton steps. The field is built from:
- **Body:** an ellipsoid with low lumps.
- **The eye's socket:** a fleshy mound round it, with a heavy brow ridge overhanging the eye and an under-eye bag.
- **The grin:** rolled lips, a slit through to a mouth cavity, gums, and cheeks at the corners.
- **Skin features:** 150 warts, and five old scars cut as grooves.
- **Armour:** several dozen broad, irregular chitin plates shingled over the crown and down the back. The face and flanks stay bare.
- **Spine:** a ridge of bony knobs down it.
- **Stalks:** ten ringed eyestalks fused into the crown with fleshy collars. Each rises, sways to one side and back, and turns its tip out and down to look. Each ends in a bulb with an eye hole.
- **Eyelids:** two separate fields, each a thick shell with a rolled margin and fold creases. The upper lid is low in the middle, a scowl; the lower rises to the corners.
- **Tongue:** a third field, grooved, on the mouth's floor.

Each field is meshed twice:
- **0.75 cm:** only for baking. Colour, roughness, wrinkle strength and distance-field ambient occlusion are computed on it per vertex.
- **3 cm** (1.8 cm for the lids and tongue): the game mesh, even quads that skin well.

Cycles bakes the fine mesh onto the game mesh's UVs (Smart UV Project):
- **Colour:** mauve, darker on the back and paler underneath, mottled and blotched. Darker glossy plates, paler warts and scars. The socket's rim and the stalks' eye holes wet and inflamed. Dark veins round the eye. Darker lips, a deep-red mouth and pink gums.
- **Normal:** includes procedural bump: cell-edge creases (cracked, leathery hide) and pores. The bump is strongest round the eye and mouth and weakest on the plates.
- **ORM map:** occlusion, roughness and the wrinkle mask.

Built directly, not baked:
- **Eyeballs:** spheres with polar UVs. The iris texture is made in numpy: a fibrous crimson iris with a faint gold collarette, a dark limbal ring, a round black pupil, and a yellowed sclera with branching blood vessels.
- **Cornea:** a 1 cm dome over the great eye's iris.
- **Teeth:** about thirty curved, uneven fangs in two interleaved rows, with longer fangs at about 30 degrees either side of the middle. They are short near the corners, and a few are broken. Their vertex colour runs from stained roots to ivory tips.

### Result
- **Triangles:** 137,352 (the heroes are 75–97k). Unreal's LODs: 92.6k / 37.2k / 21.4k / 11.4k vertices.
- **Materials:** Skin, Eye, Cornea, Teeth.
- **Bones:** 78 (79 in Unreal, which adds the armature as a root).
- **Review renders:** `Local/BeholderReview/full4` (front, three-quarter, side, back, top, below, eye, mouth, posed).

### Bones
| Bone | Role |
|---|---|
| `root`, `body` | Hover, pitch and roll |
| `jaw` | Hinged through the mouth corners. Everything below the slit follows it, fading out behind and under the chin |
| `tongue_0`, `tongue_1` | The tongue |
| `lid_upper`, `lid_lower`, `eye` | All pivot on the eye's centre |
| `stalk{k}_0..5` | Six bones along each stalk, k = 0..9 |
| `stalk{k}_eye` | Each stalk's eyeball, at its centre |

Each stalk's bulb belongs wholly to its last bone. It had been weighted only within the stalk's own radius, so half of every bulb stayed with the body and tore open when the stalk moved.

## Import (`Tools/Import-Beholder.ps1` → `Tools/import_beholder.py`)
The import creates `/Game/Characters/Beholder`: the textures, `SK_Beholder` with 4 LODs, and four materials, rebuilt on each run.
- **`M_BeholderSkin`:**
  - Inputs: the colour times a `Tint` parameter, the creases darkened by the baked occlusion, the normal, roughness from ORM.
  - The normal map's green channel is flipped, because Blender bakes OpenGL normals.
  - The `Tint` is cool (0.74, 0.70, 0.95) because the torches are warm. Untinted, and with a subsurface model, the skin rendered flat bright pink in the hall.
- **`M_BeholderEye`:** the iris texture, glossy. The iris ring (not the pupil or the white) glows by a `Glow` parameter.
- **`M_BeholderCornea`:** black, translucent and lit per pixel: only its highlights show. A white base had hazed the pupil grey.
- **`M_BeholderTeeth`:** the vertex colour times old ivory, satin.

`DefaultGame.ini` always cooks `/Game/Characters/Beholder`, because the sanctum loads it asynchronously.

## In the sanctum (`ATempleBeholder`, TempleBeholder.h/.cpp)
`ATempleSanctumBattle` loads `SK_Beholder` with its other assets and spawns the beholder at its `BeholderSpot()`: 2.6 m over the dais, 1.5 m in front of it, facing the fighter. A `UPoseableMeshComponent` is posed in component space each frame by a small forward pass over the rig. Each bone's own turn is set in its rest frame and carried by its parent's. Nothing is keyed.
- **Hover:** it bobs about 7 cm, drifts about 20 cm, and pitches and rolls a little. It pitches toward what it looks at.
- **Turn:** it turns to face the fighter at 10 degrees a second, swaying ±9 degrees.
- **Great eye:**
  - It picks someone to look at every 1.1–3.4 s: mostly the fighter, else the wizard, the ranger, a standing skeleton, or Chuck when he is within 22 m and before it.
  - It snaps there in a quick saccade, turning up to 30 degrees in its socket.
  - The lids follow its gaze down and up.
- **Blinks:** every 2.4–6.5 s, sometimes double. The upper lid travels 42 degrees to meet the lower.
- **Stalks:** they writhe, each segment about two axes at its own slow rates, more toward the tip. Each eye picks its own target every 1.5–4.5 s and turns up to 60 degrees to it.
- **Jaw:** it breathes 2–3.5 degrees. Every 7–13 s it snarls: the jaw drops 16–22 degrees, the lids squint, the tongue draws back, and the iris flares.
- **Eye's glow:** the iris smoulders; a dim red point light before the eye catches the lids and brightens in a snarl.

Its lighting is channel 1, like the interior. It casts shadows and has no collision. Nothing it does touches Chuck or the heroes yet.

## Test
`-ChuckSanctumTest` / `-ChuckSanctumCapture` gains a `beholder` trial, between `portraits` and `battle`. `-ChuckSanctumOnly=beholder` runs it alone.

It passes when the beholder has posed over 200 frames, a stalk eye has moved over 8 cm, it has blinked, snarled and made three saccades. The capture takes six stills into `Saved/Screenshots/.../Sanctum/Sanctum_beholder_*.png`: from the fighter's place, before it, its side, the great eye, a snarl, and the hall from behind the heroes.

`Verify-Package` requires:
- the spawn line (`mesh=1 bones=79 stalks=10 eye_mat=1`);
- `beholder=1` in the battle line;
- the trial's pass.

## Evidence (October 23–24)
- **Uncooked runs:** four runs of the trial; the last passed with `poses=696 stalk_travel_cm=60.5 blinks=3 snarls=2 saccades=4` (`Local/beholder-trial-4.log`).
- **Fixed between runs:**
  - the pink skin;
  - the torn bulbs;
  - the grey pupil;
  - the snarl re-triggered every frame by the test;
  - teeth through the corners' cheeks.
- **Stills:** `Unreal/Chuck3D/Saved/Screenshots/WindowsEditor/Sanctum` (not committed).
- **Packaged:** see HANDOFF.

## Not done, and known flaws
- **The battle:**
  - eye rays and the cone of force (the 2D hazards);
  - the heroes fighting it (the fighter's rising cut was authored for its stalks, but it hangs out of his reach);
  - being hit;
  - its death, if any;
  - the Fireball;
  - sounds: it is silent.
- **Stalks** pass through each other and through the crown's plates when they sway into them. There is no collision between them.
- **Lids:** the full blink has not been checked close up for gaps at the socket's corners.
- **Skin:** the wrinkles are procedural cell creases, read as cracked leather, not hand sculpting. Smart UV Project's seams may show at close range.
- **Weight:** 137k triangles at LOD 0 is heavier than the heroes.
- **Texture size:** the 4K normal and ORM PNGs are 16-bit (47 and 31 MB). 8-bit would do.
- **Not yet played or seen by a person** in motion. No Xbox test.
