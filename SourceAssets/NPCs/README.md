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
