# Human NPCs

Humans are data in `humans.json` (MakeHuman macro sliders, skin, eyes, brows,
lashes, optional hair and an outfit). A new NPC is a new entry: copy one,
change the sliders, skin and outfit colours.

Rebuild (Blender 4.5.14 with MPFB 2.0.17 and the MakeHuman CC0 asset pack, see
docs/SETUP.md), one heavy process at a time:

    blender --background --python Tools/build_npc_humans.py [-- Name ...] [--review <dir>]
    UnrealEditor-Cmd Unreal/Chuck3D/Chuck3D.uproject -ExecutePythonScript=Tools/import_npc_humans.py -unattended -nosplash -NoLiveCoding
    UnrealEditor Unreal/Chuck3D/Chuck3D.uproject -game -ChuckNPCCapture    # portraits to Saved/Screenshots/Windows/NPC_*.png

- `build_npc_humans.py`: MPFB body (CC0 MakeHuman mesh and targets) with the
  `cmu_mb` rig (31 bones, CMU motion-capture BVH names), fitted eyes, brows,
  lashes, hair. Clothing is cut from the body's own surface along smooth
  signed-distance hems (shirt with rolled sleeves, open-V jerkin, belt,
  trousers tucked into boots, knit cap), pushed out and thickened, so it
  carries the body's skin weights; boot feet are the smoothed hull of the foot
  (no toes). Skin is removed 2 cm inside each hem. Fabric UVs are
  box-projected at the fabric's real size. Output: `Humans/<Name>/SK_<Name>.fbx`
  (faces +X, feet at 0, exact `height_cm`), `Humans/manifest.json`,
  `Humans/Textures/*` (CC0 MakeHuman).
- `import_npc_humans.py`: rebuilds `/Game/Characters/Humans` from scratch: one
  shared skeleton `SKEL_Human`; masters `M_HumanSkin`, `M_HumanEye` (masked:
  the cornea), `M_HumanCard` (brows, lashes, hair), `M_HumanFabric` (Poly
  Haven weave greyed and brightness-normalised by `gain`, coloured by `tint`,
  which is the cloth's linear albedo); an instance per NPC slot.
- Runtime: `ADockNPC` poses the shared skeleton procedurally from the A-pose
  (arms lowered per body, palms turned in, breathing, weight shift, glances,
  watching Chuck). Mocap clips (CMU; `cmu_mb` uses its bone names) are the
  planned next pass.

Budget: the dock worker is 54.9k triangles in 10 material slots.
