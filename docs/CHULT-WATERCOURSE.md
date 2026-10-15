# Chult source-to-canyon watercourse

Authorized October 14, 2026 with the user's vegetated jungle-cliff references. This refines the built Chult setting; it adds no region, dialogue, controls, checkpoint or active bridge traversal.

`ChultExpansion.cpp` owns the single sampled creek path and shallow bed. Water starts at local **(21500,6000,170)**, fed from a narrow rock cleft at Z=470 by a **3 m** cascade. The headwater rill starts within the rear rock, spills down into the pool, and joins that exact first creek sample. The creek meanders west toward the outside of the canopy line, then northeast and into the near canyon bank at **S=0,T=-1800,Z=50**, or **(25580,14060,50)**. It descends monotonically; the former upstream direction/isolated endpoint pools are superseded. Shallow fords remain walkable with roughly 14 cm water depth. Mesh UV direction makes the existing stream shader scroll downstream without modifying its material.

`ChultWatercourseGroundZ` supplies the same bed/bank profile to the extension and canyon terrain, including their boundary seam. Only a side drainage notch is lowered in the near bank. Bridge paw proposals, approach at T=0, banks, unsupported gap and far-bank contract remain. Random solid vegetation clears the water corridor. There is no backing floor across the gorge and no collision on water.

`ChultWatercourse.cpp/.h` builds the closed source-rock masses, palms/fern/shrub pockets, separate eroded gorge shelves, restrained impact mist/foam and narrow broken ribbons down roughly **47 m** of canyon face. The outfall starts at the exact last creek sample and follows the existing sloping rock toward the distant bottom. Its width remains consistent with this small creek. Dedicated `M_ChultWaterfall` uses animated irregular flow and soft edges, rather than a solid white curtain. `M_ChultCliff` uses natural boulder scan detail, without masonry. Both are narrow additions from existing textures; the shared ruin, water and foliage materials remain intact. No new audio is added in this visual pass.

Verification checks every sampled bed position and monotonic water height, exact source/outfall joins, old ground/canopy/ruin/bridge reservations, visible cliff blocking and real source/terrain-seam/mouth walks. `-ChuckChultTest -ChuckLandscapeReview -ChuckWatercourseWalksOnly` runs focused static checks, six scene captures and the three walks. Full `-ChuckChultTest` also captures those views before the existing ground/traversal/recovery tests. Final package/canopy/raptor/fall/impact regressions and launcher promotion are required; actual results are in HANDOFF.

Source and waterfalls are not a completed art target, fluid simulation or explorable gorge floor. Existing fall recovery remains. Claude still owns activating/testing the broken bridge's brachiation contract.

Next part of the work can be done here.
