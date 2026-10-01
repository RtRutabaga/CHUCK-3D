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
- Runtime: `ADockNPC` poses the shared skeleton procedurally. A standing pose
  is solved per body from the A-pose (upper arm hanging just clear of the hip,
  elbow soft, forearm twisted until the thumb points forward so the palm faces
  the thigh, wrist straight, fingers and thumb curled), then breathing, weight
  shift, glances and watching Chuck play on top. Mocap clips (CMU; `cmu_mb` uses its bone names) are the
  planned next pass.

Budget: dock worker 54.9k, guard 57.3k, market woman 47.8k triangles, 10
material slots each. Placement and lines: `ADockNPC::SpawnTownsfolk` (the
guard at the closed city gate, the market woman by the red market stalls,
lines from References/Original/PHASE-2.md).
