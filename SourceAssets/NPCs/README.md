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
  the head, easing onto `spine_03` toward the tip. His axe:
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
