# Human NPCs

Humans are data in `humans.json` (MakeHuman macro sliders, skin, eyes, brows,
lashes, optional hair and an outfit). A new NPC is a new entry: copy one,
change the sliders, skin and outfit colours.

Rebuild (Blender 4.5.14 with MPFB 2.0.17 and the MakeHuman CC0 asset pack, see
docs/SETUP.md), one heavy process at a time:

    blender --background --python Tools/build_npc_humans.py [-- Name ...] [--review <dir>]
    Tools/Fetch-CMUMocap.ps1                                  # CMU takes (already in SourceAssets/Mocap/CMU)
    blender --background --python Tools/build_npc_mocap.py [-- --review <dir>]
    Tools/Import-NPCHumans.ps1 [-Clean]                       # -Clean after a skeleton or master-material change
    UnrealEditor Unreal/Chuck3D/Chuck3D.uproject -game -ChuckNPCCapture    # portraits to Saved/Screenshots/Windows/NPC_*.png

- `build_npc_humans.py`: MPFB body (CC0 MakeHuman mesh and targets) with the
  `game_engine` rig (53 bones, Unreal mannequin names, three bones in every
  finger; it replaced `cmu_mb`, whose single finger bone made paddle hands), fitted eyes, brows,
  lashes, hair. Clothing is cut from the body's own surface along smooth
  signed-distance hems, pushed out and thickened, so it carries the body's
  skin weights. Pieces: shirt (rolled or full sleeves, `collar` height),
  open-V jerkin, quilted gambeson, steel cuirass, belt (`z` for a hip belt),
  trousers tucked into boots (`height`: calf boot to shoe; feet are the
  smoothed hull of the foot, no toes), knit cap, steel helmet, kerchief,
  square-necked bodice, and a skirt and apron cut from MakeHuman's skirt
  helper (a weighted shell that bridges the legs; legs under it are removed). Skin is removed 2 cm inside each hem. Fabric UVs are
  box-projected at the fabric's real size. Output: `Humans/<Name>/SK_<Name>.fbx`
  (faces +X, feet at 0, exact `height_cm`), `Humans/manifest.json`,
  `Humans/Textures/*` (CC0 MakeHuman).
- `import_npc_humans.py`: rebuilds `/Game/Characters/Humans` from scratch: one
  shared skeleton `SKEL_Human`; masters `M_HumanSkin`, `M_HumanEye` (masked:
  the cornea), `M_HumanCard` (brows, lashes, hair), `M_HumanFabric` (Poly
  Haven weave greyed and brightness-normalised by `gain`, coloured by `tint`,
  which is the cloth's linear albedo); an instance per NPC slot.
- `build_npc_mocap.py`: three CMU motion-capture takes (free for any use;
  Motionbuilder-friendly BVH, mapped by name onto `game_engine`) retargeted
  onto the dock worker's rig: the limbs are aimed from the A-pose onto each
  take's T-pose frame, the spine, neck, head and collarbones keep the rig's own
  neutral carriage, then every bone follows the take's change of world
  rotation; facing and drift removed, 30 fps, the last second blended into the
  first so it loops. Clips: `StandHip` (111_28), `StandLook` (77_02), `Talk`
  (18_08). Every rig is named `HumanRig` so the shared skeleton has one root.
- Runtime: `ADockNPC` plays its idle (guard: StandLook; worker and market
  woman: StandHip, each from a random point) as changes of rotation from the
  skeleton's rest, so it fits every body; blends to Talk while Chuck talks to
  it; turns its body to a rat that stays off to its side (or that talks to
  it); head look and curled fingers are laid on top. Without clips it falls
  back to the procedural pose below. A standing pose
  is solved per body from the A-pose (upper arm hanging just clear of the hip,
  elbow soft, forearm twisted until the thumb points forward so the palm faces
  the thigh, wrist straight, fingers and thumb curled), then breathing, weight
  shift, glances and watching Chuck play on top. Hands (both modes): the
  clips' poor wrist data is mostly replaced by a straight wrist, the arms are
  eased 5 degrees out so hands clear wider hips, and each finger joint bends
  about its own axis (from the palms-down model pose) to a relaxed curl
  (little finger most, index least) or, via `SetGrip`, a fist round a shaft
  (for the guard's spear later).
- The sewer zombie (`Zombie` in `humans.json`, user 2026-10-03): a gaunt old
  man long dead (`skin_texture` / `eye_texture`: `Tools/build_zombie_textures.py`
  drains the `old_caucasian_male` skin grey-green, mottles it with rot and bruising,
  veins and clustered sores, sinks the eye sockets; the eyes cloud), barefoot, in a
  shirt and trousers cut `ragged` (`tatter_hem` / `tatter_holes`: jagged hems
  pulled back a few cm and small worn-through holes). Its clips are an old
  man's: `ZombieIdle` and `ZombieWalk` from 137_33 "Old Man Walk" (the walk in
  place: `build_npc_mocap.py` takes out the hips' steady travel, records the
  speed, 37.5 cm/s, and loops on the best-matching step), `ZombieFall` from
  113_08 "Lay down" to 4.5 s (flat on its back), played 1.8x as a collapse. Tried and dropped: 104_41
  "ZombieWalk" (arms held out), 91_24, 104_13, 90_16. `ADockNPC` in
  `EDockHuman::Zombie` mode moves it kinematically (walls block it, it never
  steps off floor or over a gap), shambling at the clip's speed so the feet
  don't slide; the rear-up tell, the lunge and the flinch are laid over the
  clips. Placed by `SewerLife.cpp`; review images in `Humans/Review/Zombie_*`.

- The dwarf (`Dwarf` in `humans.json`, user 2026-10-04: "a dwarf NPC by the
  smithy, wearing dwarven armor and holding a battle axe. Bearded."): 134 cm.
  `targets` (MakeHuman proportion targets loaded before the rig is fitted)
  shorten his legs and arms and broaden his torso, neck, head, hands and feet.
  The armour is layered with per-item `offset`s: a padded coat, a `mail` shirt and
  split `mailskirt` (generated 4-in-1 mail, `Tools/build_dwarf_textures.py`,
  `SourceAssets/Surfaces/Armor`, chosen by the item's `folder`), a cuirass,
  `pauldron`s and `vambrace`s (sharing the Cuirass slot via `slot`), brass `trim`
  bands along the edges of another piece (`of`, `width`), and a helmet with a
  `nasal`. MakeHuman has no beards, so `beard` builds one (`make_beard`): blobs
  over the jaw and a spade hang down the chest, a moustache and two braids,
  voxel-merged and grooved, plus loose strand cards (`beard_mass.png` /
  `beard_strands.png`, card material, tinted) and brass braid rings; skinned to
  the head, easing onto `spine_03` toward the tip. Since 2026-10-06 ("make the
  beards look better") every beard's strand cards use `beard_locks.png`
  (`Tools/build_beard_textures.py`: eight tapered, see-through locks, one per
  card via `lock_uv`, so a card no longer shows as a solid rectangle) in the
  `M_HumanHair` master (slot type `hair`: the card graph with a dithered mask).
  Non-bushy sideburns and jaw blobs sit close to the skin and thin toward the
  temple and ear. The sailor's brows are `eyebrow009_grey.png` and he has short
  grey hair under his cap (`fringe` with `locks`). His axe:
  `Tools/build_battle_axe.py` (`SM_BattleAxe`, double-bitted, 127 cm), held by
  `ADockNPC::GiveAxe` as the guards hold their spears.

- The old elf (`ElfElder` in `humans.json`, user 2026-10-05: "an elderly elf
  woman npc sitting on a bench by the fountain, long braided gray hair"): 171 cm,
  slight, MakeHuman age .95 on the `old_caucasian_female` skin. Ear `targets`
  (pointed, taller, a little back and out) plus high cheekbones, an oval face and
  lifted outer eye corners. MakeHuman's `braid01` hides the ears under a cap and
  sweeps a fringe over one eye, so `braided_hair` builds the hair instead
  (`make_hair`): a cap cut from the scalp, combed back, its hairline over the
  forehead and temples and a finger's width clear round each ear, lifted fuller
  over the crown and grooved along strands running to the nape; a 55 cm
  three-strand braid (each strand a figure of eight about the braid's line)
  from a gathered knot at the nape down her back, tied above a loose tuft; strand
  cards over the crown. All in the beard's textures, tinted silver. Her grey brows:
  `Tools/build_elf_textures.py` (`eyebrow010_grey.png`). A long-sleeved linen
  shirt, sage bodice, long dark green skirt, narrow belt and soft shoes. She only
  ever sits (`seated`), so her skirt is weighted by `seat_skirt` (hips at the
  waistband, onto the thighs below the hip joints, the calves below the knee,
  blended across the middle so it bridges her knees) rather than `soften_skirt`.
  `ADockNPC::SpawnElfElder` sits her at the fountain end of the plaza bench at
  x -440 (hips 9 cm over its top, legs and lap hands by IK). 89k triangles.

- The market woman's braid (`ponytail` in `humans.json`, user 2026-10-06: "a blond pony tail
  like the old lady's, but keep her cap over head"): `make_ponytail` builds the elf's
  three-strand braid alone (no scalp cap), 40 cm, its gathered top just under the back edge
  of her kerchief, tinted blond; the kerchief is unchanged.

- The alchemist (`GnomeAlchemist`, user 2026-10-05: "a gnome in black robes,
  hands together behind robe sleeves so that they aren't visible, DnD 5e gnome
  height and facial features"): 100 cm (5e gnomes are 3-4 ft). Targets give him
  a large round head on short legs and arms, a prodigious round nose, big eyes,
  pointed ears, round cheeks and an upturned mouth; wild white hair
  (`make_hair` with `braid` 0 and `wild_cards` standing out from the sides and
  back) and a white beard (`make_beard`, `card_scale` for short, fine strands).
  A charcoal-black `robe` (new piece: collar, sleeves to the wrist, to the upper
  thigh; `flare` pushes the forearm cloth out into bell sleeves and `cuff` runs
  them a little past the wrist) over a long skirt, a dark belt, soft shoes.
  `hide_hands` deletes his hands from the mesh: they are never seen.
  `ADockNPC::SpawnAlchemist` stands him before the alchemist's left window;
  `PoseSleeves` swings his upper arms forward and lays the forearms across so
  the cuffs meet, left over right. 48k triangles.

- Bobert (`Bobert` in `humans.json`, user 2026-10-05: "Bobert in his barrel
  asleep like in the 2d game, positioned by starting point"; GAME-BIBLE.md: he
  never wakes, is never named, never interacted with): a late-middle-aged
  halfling, 97 cm, from the user's three reference images (bald with a wispy
  fringe, big pointed ears with a ring in the right, drooping nose, bagged eyes,
  downturned mouth; ochre open coat with a fur collar, grey-violet scarf, brown
  shirt, red knitted wrist warmers banded off-white, baggy olive trousers,
  boots). New kit pieces: `coat` (open front widening to lapels, sleeves to
  `sleeve`, to `bottom` below the waist), `furcollar` (the coat's cut kept to a
  band `depth` deep), `scarf` (wound round the neck, an end down the chest),
  `cuff` (a band `from`..`to` down the forearm; a second with its own `slot` and
  `offset` makes the off-white stripe). `fringe` builds the leftover hair
  (`make_fringe`: cards scattered over the scalp's faces round the back and over
  the ears, lying along the scalp, a few wild wisps; beard strand texture) and
  `earring` a metal ring in one lobe (`make_earring`). His eyes are closed:
  `eye_texture` from `Tools/build_bobert_textures.py` (lid skin from his skin
  texture round the sockets). His barrel: `Tools/build_bobert_barrel.py`
  (`SM_BobertBarrel`, 86 cm cask on its side on chocks, open mouth, a sack on the
  floor and a rolled blanket against the back head; its manifest entry carries the
  numbers `ADockNPC` seats him with). Runtime: `ADockNPC::SpawnBobert` places the
  barrel beside Chuck's spawn and solves his sleeping pose from the A-pose
  (`SolveSleep`: back leaning into the blanket and slumping forward, head bowed
  and fallen sideways, knees up, shins down to the rising floor), hands folded
  over his knees by IK each frame, slow breathing and an occasional nod. The pose
  was first tried on his real MPFB body and the barrel in Blender (see the handoff).
  Rebuild order: `build_bobert_textures.py`, `build_npc_humans.py -- Bobert`,
  `build_bobert_barrel.py`, then `Tools/Import-NPCHumans.ps1 -Only Bobert,prop:BobertBarrel`.

- Eyes (user 2026-10-29: "currently everyone looks like they have thick mascara on"; the
  zombie stays as it was). For every human and hero except the zombie (any spec with a
  `skin_texture`), the build generates two textures:
  - **Lashes:** `fine_lash_texture` makes `Textures/<lashes>_fine.png` from the MakeHuman
    lash sheet, which is a cosmetic set (dense, long, jet black, a full lower row).
    - The strands are thinned to their cores (alpha through a high smoothstep).
    - The lower lids' rows, the sheet's lower right, are kept at under half strength, so
      only a few strands pass the card master's clip.
    - The texture is white, and the `Lash` slot is tinted: the first groom's `colour` × 0.45
      (lashes are darker than the hair on top), or a dark brown with no strand hair, or
      `lash_tint`.
  - **Eyeballs:** `soft_eye_texture` makes `Textures/<eyes>_eye_soft.png`. The whites are
    taken down about a tenth and warmed, darkening toward the eyeball's edge as if in the
    lids' shade. The iris and the cornea's clear corner are unchanged. Bobert's closed
    eyes (`eye_texture`) are unchanged.
  - Delete a generated PNG to regenerate it after changing its function; both are cached
    by name.
- Faces and beards (user 2026-10-28: "make all huminoid npc's look better, in particular
  faces and beard. for ones with beards i want to see mouths through beards"). Applies to
  every human here and to the temple heroes (they use this kit):
  - **Mouths through beards.** `Mouth` measures the lips (MPFB's `lips` group): corners,
    upper and lower borders. `make_beard` keeps an opening clear: no jaw blob over the
    lips; the hair just under the lower lip starts short and thickens down the chin; the
    hang's top is capped below the lower lip and set back, so from Chuck's low eye-line
    the lip isn't behind a shelf; the moustache is a tube along the upper lip's border,
    resting on it and drooping past the corners; jaw blobs fill the skin between nose and
    moustache. Whatever the remesh still leaves in front of the lips is pushed back behind
    the skin. A strand card that would cross the opening stops short of it. Short
    moustache cards are combed out and down to the lip's border. The build prints
    `CHUCK_BEARD mouth_cleared=... trimmed=... dropped=...`.
  - **Finer beards.** There are 1.6x the strand cards (`density`), 0.72x as wide (`thin`).
    `beard_locks.png` is regenerated at 96 x 768 per lock with 150-220 thinner strands,
    darker toward the root.
  - **Teeth and tongue** (MakeHuman CC0 `teeth_base`, `tongue01`) on every NPC with a
    `face` rig, decimated to 35%. The upper teeth are on `head`; the lower teeth and the
    tongue are on `jaw`, split about a bite plane fitted through the pieces (16 up, 17 down
    on every body). Slot type `teeth` (`M_HumanTeeth`: rough, little specular, shaded),
    tinted.
  - **Skin.** `M_HumanSkin` is now subsurface. Its subsurface colour is the skin's own
    colour times `SubsurfaceColor` (a fixed red lightened the darker skins); the zombie
    has `skin_subsurface`. It carries a tiling pore and fine-line normal,
    `skin_detail_normal.png` (`Tools/build_skin_detail.py`, `DetailTile` 20,
    `DetailStrength` 0.85). `M_HeroSkin` carries the same normal. The `Tint` parameter now
    exists, so `skin_tint` takes effect (the gnome is a little warmer than before).
  - **Brows** use the dithered hair material. Lashes stay clipped: dithered, they left an
    orange smear on the lids.
  - **Rebuild:** `build_skin_detail.py` and `build_beard_textures.py`, then
    `build_npc_humans.py`, then `Import-NPCHumans.ps1 -Only <every NPC> -RebuildMasters M_HumanSkin,M_HumanTeeth`.
    `-RebuildMasters` deletes those masters' files before the editor starts. Clearing a
    loaded master's graph trips an engine assertion. The heroes follow:
    `build_temple_heroes.py`, then `Import-TempleHeroes.ps1 -RebuildMasters M_HeroSkin`;
    restore the re-saved clips and PBR instances from HEAD. A texture whose source is
    newer than its asset is now re-imported.
  - **Review:** `-ChuckNPCCapture` has two more shots. `Head` is aimed at the head bone.
    `RatEye` is from 55 cm up and a metre off, mid-line. `python Tools/face_sheet.py <dir> RatEye <out.png>`
    makes a contact sheet.

- `build_spear.py`: the guard's 212 cm spear (ash shaft, leaf blade on a socket,
  leather grip wrap, iron butt), 492 tris, in the same fabric materials
  (`SourceAssets/NPCs/Props`, imported as `/Game/Characters/Humans/Props/SM_Spear`).
  At runtime it stands beside the guard's foot on either side (`ADockNPC::GiveSpear(Side)`),
  that arm solved onto it every frame (two-bone IK, elbow back and out, thumb up,
  fist closed with `SetGrip`), over whatever the motion capture is doing.

Two gate guards (`Guard`, `GuardWoman`: same kit, a 174 cm Caucasian woman) stand
either side of the plaza's closed gate, each spear on the outer side. A third (`SideGuard`, a
181 cm Caucasian man) stands by the Dock Street side gate in the west wall, south
of it, beside the open sewer hatch and clear of its approach.

Budget: dock worker 54.9k, guards 57.3k / 52.1k, market woman 47.8k triangles, 10
material slots each. Placement and lines: `ADockNPC::SpawnTownsfolk` (the
guard at the closed city gate, the market woman by the red market stalls,
lines from References/Original/PHASE-2.md).
- Strand hair (user 2026-10-28: "for all haired, make hair and beard-hair look better",
  free resources allowed). Every haired human and hero now has Unreal strand grooms
  instead of card hair:
  - **Styles** come from the MakeHuman community's hair editor pack (CC0, Tomas Klecer;
    `Tools/Get-HairPack.ps1` downloads and checks it into `Local/HairPack`, not committed).
    They are geometry-node hair curves on the MakeHuman base mesh. `Tools/npc_strands.py`
    moves each style's guides to the same UV on this body, scaled with the head. MPFB's own
    Surface Deform needs the base mesh's rest shape, which the kit stores as
    `rest_position` before baking. The pack's nodes then interpolate, clump and frizz them.
  - **Spec:** a `groom` list per NPC in `humans.json` / `heroes.json`. Each entry has:
    - `style`, `group` (`Hair`, `Beard`, `Braid`) and `points`.
    - `set` overrides a pack input, e.g. `"Duplicate Hair Curves/Amount"` or
      `"Set Hair Curve Profile/Length"`.
    - Material: `colour` (linear), `root` (root darkening), `grey` (share of grey
      strands), `grey_colour` and `roughness`.
    - Root filters: `fringe` (a bald man's band, thinning upward) and `hairline` (a
      receding one).
    - `under`: how flat hair lies under a cap, helmet, hat, kerchief or hood. Under headwear
      a point is also kept below half the room between the skin and the headwear above it,
      measured from the skin (2026-10-29: strands longer than the room under the sailor's
      cap and the wizard's hat came through them). A cap's rolled `trim` counts as headwear.
    - `specular` (default 0.5): dark hair is at 0.2–0.3, or it reads grey in rim light (the
      fighter's beard in the cutscenes).
  - **Beards** (`"fill": "beard"`) keep `make_beard`'s volume as their shape, its mouth
    already cleared. Strands start as the pack grows them, then lengthen (`length`), hang
    with a slow `wave`, rest on the chest or armour, and end where they leave the volume.
    The volume and its cards are then dropped; the dwarf's rings stay. Strands reaching
    the lips are cut (`Mouth.gap`).
    - `"mass": true` (the dwarf, user 2026-10-29: "a big bushy beard like before"): each
      root sends `copies` strands, bowed out (`bow`) to points spread through the whole
      volume, so the beard fills its depth instead of lying on the chest.
  - **Braids** (`"style": "braid"`): each of `make_braid`'s three plaits is filled with
    strands (`per_plait`), ending in a loose `tuft`, and the tube braid is dropped. They
    hang from the head bone: on the spine they stayed put as the head turned and came
    away from it. A `comb` entry with `"to": "braid"` combs the head's hair back into the
    braid's top (the elf's replaces her card cap; the market woman's lies under her kerchief).
  - **Ponytail:** `comb` draws a style's roots back over the scalp to where the previous
    style's strands gather tightest (the ranger's tie; that style is `tie_only`, its own
    thin tail left out, `snug` moving the tie onto the skull). With `tail`, each combed
    strand carries on through the tie and down as the ponytail, so the tail is the same
    hair. A strand under a tie that stands off the scalp rises straight into it.
  - **Hairline:** `hairline` applies in front of the ears; behind them hair goes down to the
    nape (it used to leave the back of the gnome's and the elf's heads bare).
  - **Scalp under strands:** every strand-haired head gets a scalp shell (`make_scalp`)
    tinted from its hair if the spec has none. Its vertex colour is red for stubble density
    and green for "solid": deep in the hair (several roots within 12 mm), the shell is
    painted opaque in the hair's colour, a shade darker (`M_HumanStubble`). Bare skin glowed
    red between strands, and from a distance, where strands thin out, the head read as skin.
  - **Bones:** each strand belongs to `jaw` (the body's jaw weight > 0.5, i.e. beard below
    the lips) or `head` (braids too); `spine_03` is still accepted. Groups become `<group>`,
    `<group>Jaw` and `<group>Back`. The build writes
    `SourceAssets/.../<Name>/GR_<Name>_<Group>.abc` (centimetres, the FBX's space) and
    lists them under `grooms` in the manifest, with the roots' mean and bounds.
  - **Import:** `Tools/Import-NPCGrooms.ps1 [-Set humans|heroes] [-Only ...]` runs after
    the meshes. It makes `GR_<Name>_<Group>` and `MI_<Name>_<Group>` of `M_NPCHair` (hair
    shading: per-strand shade, darker roots, grey strands). It checks the imported roots
    against the build's to within 1.5 cm. Unreal reorders curves, so the check is not per
    strand.
  - **Runtime:** `NPCGroom.cpp` attaches each groom to its bone rigidly, undoing the bone's
    rest transform. The NPCs are posed through a `PoseableMeshComponent`, which a groom
    binding can't follow. No simulation. The keeper's beard opens with his jaw. `/Game/Characters/Humans`
    is now always cooked, because the grooms are loaded by path.
  - **The tavern keeper has a face rig** (`face`): jaw, lids, brows, teeth, tongue. He
    blinks. His one voiced line (at night) drives the jaw like the others', but no
    capture has shown him speaking yet. The fighter's beard was seen opening with his jaw.
  - **Review:** `-ChuckNPCCapture -ChuckNPCShots=Head,HeadBack,HeadSide,Far` films only those
    shots (`HeadBack`, `HeadSide` and `Far` added for hair). The sanctum's `portraits`
    trial now also films the back of each hero's head.
  - **Known gaps:**
    - Braids and ponytails are rigid to the head; nothing swings, and a braid turns with the head.
    - Brows are still cards.
