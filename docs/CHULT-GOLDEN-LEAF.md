# Premium Buhetian Halfling Leaf — golden cigarettes

Authorized by the user October 15, 2026: individual golden cigarettes give a timed maximum-stamina lock and continuous wall climbing; place one at the first giant brachiation tree before the temple and make the stamina display glow golden.

- Duration: **45 seconds of gameplay time**. The tallest giant's 16.9 m rise takes roughly 14 seconds at the existing 120 cm/s locked climb speed, leaving approach and catch time. Pausing pauses the effect.
- Collect by walking over the individual cigarette. Gold material on the existing cigarette mesh, with restrained emission; no pack, inventory activation or new control. The pickup uses the existing collection sound.
- The effect immediately fills stamina and reuses `LockStamina`: sprint, wall climbing and brachiation cannot drain it. The whole existing stamina ring and paw turn gold, with a sustained soft pulsing halo. Expiry restores their normal colours and ordinary stamina rules.
- Repeat collection refreshes to at least 45 seconds from the new pickup; time does not add together. A placed pickup is consumed until Chuck dies/resets, when it returns and the lock clears. No persistent save system exists yet.
- The golden pickup does not add ordinary cigarette currency or refill Sanity. Ordinary pickups keep their existing behaviour.
- The first giant has a clear approach at the side of its standing fork, with only buttresses intersecting that approach omitted. The locked wall climb follows curved surface normals and reaches farther across tapering bark; normal unlocked climbing remains unchanged. The last giant retains its exit liana.

Implementation: `CigarettePickup`, `ChuckCharacter`, `DockGameMode` HUD, `ChultCanopy`; material generated only by `Tools/create_golden_leaf_material.py`. No Blender sources regenerated or dependencies installed. The new material uses existing `.uasset` LFS rules and the configured GitHub LFS endpoint; quota cannot be queried locally.

Verification entry point: `-ChuckGoldenLeafTest`, real collection, trunk ascent, pull-up onto the first fork, first brachiation catch, natural expiry, timer refresh, reset and re-collection. Actual results are recorded in HANDOFF; this document describes the contract, not passed checks.
