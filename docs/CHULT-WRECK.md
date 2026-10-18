# Chult — the jungle wreck

User request, October 18, 2026 (Claude): "add the mysterious shipwreck in the jungle, place it beyond the bridge in a spot unavoidable by the player, it should be a wrecked Cog, not ancient it should look like a relatively recent wreck with minimal jungle growth on it, no crew. it should have astral ruptures around it with natural rupture look like the sewer ruptures not the blocky ruptures like the pantry ruptures".

This brings forward the region plan's wreck milestone ([regions/CHULT.md](regions/CHULT.md#accessible-jungle-wreck)). The 2D direction is the Phase 5 cog in the jungle with Astral Sea blocks round its base (`docs/development/PHASE-5.md` §2, `chult_cog.txt`, both read-only). As the region plan says, the sailor and all crew are gone, and nothing explains how the ship got there.

## Where it lies

It lies just past the bridge landing, on the inland side of the road's first bend toward the river. Coordinates are in the canyon frame (S along, T across), centimetres from the floor origin.

- The keel's midpoint is at S=8300, T=−950. The bow points back toward the bridge, swung 8° toward the road. The stem is at S≈7140, about 21 m beyond the bridge's far end, and the sternpost at S≈9450.
- Stepping off the bridge, the player faces the bow and its bowsprit. The road passes the bow and the low side 9–20 m off, and goes between the wreck and the river. The fallen mast's head and the spilled sail lie between the hull and the road. One rupture opens 4 m from the road just past the landing.
- The rest of the far bank (river zombies from S 18200, giants, temple) is unchanged. The wreck is a quiet pocket with no enemies, as the region plan proposes.

## The ship (`ChultWreck.cpp`)

An original cog, built in code in the ship's own frame (one rotated component, so the wood grain runs along the ship):

- **Hull:** 23 m on the keel and 7.6 m beam, 4 m deep amidships, with the sheer rising to both ends. Flush planks on the flat bottom and ten clinker strakes up each side, each lapping 4 cm over the one below. A straight raked stem and sternpost, the keel, and an inner face with plank thickness wherever the planking stops.
- **Cog details:** six through-beams protruding from both sides; a stern castle with a cabin under it (a door forward) and a railed roof platform; a forecastle platform in the bow on two posts; a bowsprit; and a stern-hung rudder swung hard over, with iron straps.
- **Inside:** a planked hold floor and frames every 75 cm up both sides. The planked deck follows the sheer, with a main hatch with coaming on three sides and the mast partners. Crates and barrels have slid down against the low side.
- **The wreck:** it is heeled 18° over toward the road, bow down 2° and sunk 55 cm. The mast broke 2.3 m above the deck, leaving a splintered stump with iron bands. The upper 13.7 m fell over the low side, tearing away 4.25 m of bulwark, with its butt resting on the deck and its head on the ground toward the road. The yard broke in two across the mast near the head. The sail is still bent to the yard and lies spread back over the mast and on the ground in long folds, with a ragged foot, a rip and a hole. Slack shrouds run from the mast to the high side's rail, with loose lines to the ground. Crates, a barrel and a rope coil have spilled out of the breach.
- **Recent, not ancient:** the timber is sound and the canvas whole enough to drape. The only growth is four creepers over the high gunwale and one up the high side's bilge. There are no crew and no remains.

Materials, all existing: `M_Wood` (hull, frames, spars), `M_WoodLight` (deck and floor planks), `M_Canvas` (sail and rope), `M_RustIron`, `M_ChultHangingMoss`, `M_FallGround`. Props: the docks' `SM_DockCrate`, `SM_DockBarrel`, `SM_RopeCoil` (plain mesh components). No new assets, downloads or installations.

## Getting in, round and out

Chuck jumps only 18 cm and steps 4 cm, so every way in is a slope or a drop:

1. **Breach:** the low side is torn open at the ground for 3.5 m (strakes 4–7). He walks in and drops onto a crate, then onto the hold floor.
2. **Hold:** in the hold, a ramp of three planks with gangway cleats runs from the floor up through the hatch, flush with the deck at the hatch's fore edge (20°; 27° at worst with the heel).
3. **Deck:** the deck slopes 18° across. The cabin door is aft; the stump and the mast's butt are amidships.
4. **Torn bulwark:** a panel of strakes on two frames lies from the gap in the low side's bulwark down to the ground. Its hidden slab is the walking surface, flush with the deck edge (116 cm up) and the ground 3 m out. It leads back to the road side. The mast lies beside it, clearing the deck edge by 38 cm.

The castle roof and the forecastle platform are seen, not reached.

## The Astral ruptures

There are nine, round the hull on all sides: by the road past the landing, off the bow, along the low side aft of the breach, off the stern, and inland of the high side. They are 1.3–3.8 m long. They use the sewer's natural rupture form and materials, not the pantry's boxes:

- A pointed, ragged, slightly bent tear in the ground. A soil lip laps over the cut edge and breaks down inside it.
- A well of `M_AstralDepth` 3 m deep, with its glowing surface 18 cm down.
- Two upright curved `M_AstralOilMist` veils along the long edges. They are lower than the sewer's (72 cm against 105 cm at full size), because in daylight the full height read as a glass box.
- A purple point light without shadows.

The ground really is open there. The far bank's 1 m ground cells that a rupture or the hull's buried part crosses are left out of `ChultCanyon.cpp` (`ChultWreckCutsCell`). The wreck lays them again exactly as the canyon does, in 10 cm squares where a cut crosses a cell, so there is no seam. Walking into a rupture is an Astral death, as in the sewer and the 2D game (`ChultWreckRuptureSwallows` in `AChuckCharacter::Tick`). He respawns at the bridge's far end, the far bank's first spawn. The sail tears back from any rupture it reaches, so canvas never hides one.

The jungle's plants, rocks and trees are cleared from the hull, the mast, the sail, the panel and each rupture: trees 4.2 m back so their crowns stay out of the hull. This happens once when the wreck is built and again after the road's bridge-exit planting (1.2 s). The jungle round it is otherwise untouched.

## Checks

- `CheckChultWreck` runs in every Chult test (`CHUCK_CHULT_WRECK_CHECK`). It checks that all nine ruptures are open to 4 m with ground at their rims, eight standing surfaces inside and on the wreck, a capsule sweep in through the breach, a capsule sweep up the torn bulwark, and that the road stays open from S 5000 to 10500.
- `-ChuckChultTest -ChuckWreckReview` takes eleven cooked views (`Wreck*.png` in the Chult screenshot folder). The real character then walks four legs:
  - **Road:** runs the road from the bridge past the wreck.
  - **Hold loop:** breach → hold → ramp → deck → torn bulwark → ground.
  - **Cabin:** walks into the cabin.
  - **Rupture:** walks into a rupture, which must be an Astral death and a respawn at the far bank.
- `Verify-Package.ps1` requires the build line, the check and all four legs.
