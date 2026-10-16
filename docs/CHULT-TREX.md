# Chult — the T-rex of the giant trees

**User request, 2026-10-15:** "Add the t-Rex to the tall brachiation trees before section, use free resource the help." The giant trees upriver ([CHULT-UPRIVER.md](CHULT-UPRIVER.md)) were laid out for "a t-rex that can only be avoided through brachiation in the high tree canopy". Claude owns the creature (asset, behaviour, animation, import), as with the raptor ([CHULT-RAPTOR.md](CHULT-RAPTOR.md)).

## Asset

- **Source:** "T-Rex" by Quaternius, CC0, from poly.pizza (https://poly.pizza/m/UYtneO5FpF). It is from the same "Animated LowPoly Dinosaurs" pack as the raptor, with the raptor's rig bone for bone and the same six clips. Downloaded with the user's approval (one 336,568-byte GLB). It is kept unmodified in `SourceAssets/Enemies/TRex/Source/`, and `manifest.json` records its URL, licence and SHA-256. It is credited in `docs/DEMO-CREDITS.txt`. Nothing was installed.
- **Build:** `Tools/build_trex.py` (Blender 4.5.14) is the raptor's build with the T-rex's names. It produces:
  - **Scale:** 12 m nose to tail, 5.8 m tall, hips at 3.7 m and head at 5.1 m (Chuck is 65 cm). The rescale is checked against the source's jump.
  - **Mesh:** welded, faces turned outward and subdivided once, giving 10,884 smooth-shaded triangles.
  - **Skin:** body-following UVs carry a generated dorsal-band and belly pattern plus tiled scales.
  - **Clips:** `SK_TRex.fbx`, the six clips and `TRexClipData.h`, with measured speeds (walk 430 cm/s, run 999 cm/s natural) and the bite's moment (0.5 s).
- **Import:** `Tools/import_trex.py` imports into `/Game/Characters/TRex`. `M_TRex` has one instance for each of the five source regions: hide, pale, belly, dark and mouth.
- **Sounds:** `Tools/gen_trex_sfx.py` synthesizes original sounds: ground-shaking footfalls, two roars, a jaw snap, a stalking growl and a death roar (not used yet). `Tools/import_trex_sfx.py` imports them into `/Game/Art/Audio/SFX`.

## Behaviour (`AChultTRex`, ChultTRex.h/.cpp)

It is built like the raptor: there is no AI controller, it drives its own movement and steers round trunks and rocks with sweeps. Its capsule (1.5 m radius, 5.8 m tall) is its legs and belly; the head and tail have no collision.

- **Its stretch:** S 35600–50400 of the upriver valley (the giants stand at S 36600–50900). It never leaves it. The far bank, the river's head, the last giant's liana down, the deep jungle and the temple are safe.
- **Patrol:** a 24-point loop at 1.8 m/s. It goes up the road's side of the giants' line 12.5 m off it, crosses between the last two trees, comes back down the far side and crosses before the first. Every point lies between two trees, at least 8.7 m from any trunk.
- **Notice:** within 32 m in view, or 13 m all round (the ground shakes). It turns on him and roars, rearing, for 1.3 s.
- **Chase:** 4.2 m/s, faster than his sprint (3.8 m/s), striding with its walk clip at its natural pace. On the ground he cannot outrun it. The way through is the canopy.
- **Bite:** at 6 m from him, a 0.45 s tell (it stops and rears back), then a lunge. One bite empties his sanity and kills him. A roll or side jump through the tell should beat a single bite (not yet tested).
- **Out of reach:** it cannot reach him when his feet are 3.3 m or more above its own, or when he is swinging. That covers every giant's limbs, crotches and grips, 15–17 m up. Below him it stalks, growling, and snaps up at him every 3.2 s. After 20 s it gives up and returns to its loop, not noticing him again for 8 s.
- **Scratches:** it cannot be scratched to death.
- **Death:** dying to it returns him to the **Chult Giant Trees** spawn. That spawn has moved back to S=34600, 10 m short of its stretch, so the return is safe; the Dev Checkpoint goes there too.
- **Spawning:** one T-rex appears when Chult becomes playable (`AChultLanding::Build`). It is left out of every automated `-Chuck…Test/Capture/Review` run except its own, so the other tests keep their worlds. The menu checkpoints do include it.

**The way past:** Codex's golden cigarette ([CHULT-GOLDEN-LEAF.md](CHULT-GOLDEN-LEAF.md), added the same evening) lies at the first giant's trunk, about 10 m inside the T-rex's stretch. He slips in, takes it and climbs the trunk on the stamina lock; above 3.3 m he is out of its reach. Then he crosses the canopy to the last giant and climbs down its liana, which lands just outside its stretch (checked by the test). **Chult Giant Canopy (dev)** still puts him straight up in the first giant.

## Test

`-ChuckTRexTest`, also run by `Verify-Package`:

- A static sweep of its whole loop (24 legs, 0 blocked), and a check that the last giant's liana lands outside its stretch.
- Four trials with the real character:
  - **Patrol:** it walks its loop.
  - **Chase and kill:** in its view on the road, it roars, chases, bites once, and he dies and returns whole at Giant Trees.
  - **Safe in the giants:** in a giant's crotch, it stalks and snaps below and never bites.
  - **Stays in its stretch:** he sprints out of the stretch as it roars, and it does not follow past its edge.
- At least the expected sounds load and play.

Stills are saved to `Screenshots/.../Chult/TRex_*.png`.

## Not done

- There is no hand-made animation, blending or foot IK; clips switch directly. Its turns are slow, but its 12 m body can swing its head or tail through a trunk while it turns.
- There is no camera shake on its footfalls and no hearing/occlusion model. The sounds are synthetic and have not been heard on speakers.
- There is no navmesh, only sweeps. A chase into the trunks can wedge it for a moment.
- It has not been played by a person, and there is no Xbox test.
