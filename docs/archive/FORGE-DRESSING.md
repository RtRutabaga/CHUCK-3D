# Smithy forge dressing — October 4

The forge remains beside Claude's working blacksmith in the fountain plaza, centered at (-780, -3724) cm. Smith feet, anvil, hammer/tongs/bar motion, dialogue and strike/forge audio are unchanged. The old solid stone niche is replaced with a raised brick hearth, an actual arched opening, coal firepot, riveted iron extraction hood and a chimney rising above the smithy roof. A faint animated plume emerges from its open-looking top.

Side-fed bellows, an aged timber workbench with vise/stock and hanging tongs/pokers, a stave quenching tub with visible water and iron hoops, stored fuel and modest offcuts make the space read as a working dockside smithy. Existing fuel logs remain under/beside the bench. This is exterior dressing, not a new shop interior, crafting system or game area.

`DockForge.cpp/.h` owns this geometry and the fixed review cameras. `DockPlaza.cpp` calls it instead of the original niche. Structural masonry, arch/hood/chimney, bench and tub have collision; small tools, coal, smoke and flames are decoration. Four flames reuse `AddDockFlame` and `M_TorchFlame`; the new warm hearth light joins the existing plaza fire flicker. No existing fire, water or human material is regenerated.

`Tools/create_forge_materials.py` owns only four graphs: `M_ForgeBrick` (staggered rough brick, mortar and soot), `M_ForgeIron` (dark worn iron with restrained oxidation), `M_ForgeAsh` and `M_ForgeSmoke`. All are procedural, with no texture download or dependency installation. Existing UE5.7.4, DX11/SM5 and the native procedural mesh component are retained. Material packages use existing LFS exclusions/rules. Remote LFS allowance is unknown; no paid service is used.

`-ChuckForgeCapture` creates six stills under `Saved/Screenshots/Windows/Forge`, then exits: morning workshop/hood/rat-height hearth and evening workshop/hood/chimney. Do not combine it with other camera capture modes. The packaged verifier requires `CHUCK_FORGE_DRESSING`, four loaded graphs, four structural collision samples and three clear walking samples, alongside Claude's smith contact/sound check and existing full gameplay/route tests. Read the latest HANDOFF for actual build, rendered review and verification evidence.

Claude's 80 modified re-imported human packages and two untracked source textures were fingerprinted in `Local/forge-existing-assets.json` before work; they remain unfinished human work, outside this commit. Preserve them. The candidate cooks the current checkout as Claude's previous package did, so those retained binaries are part of its local build inputs.

Remaining visual limits: native/procedural surfaces, repeated material patterns, simplified bellows/tools and crossed smoke cards. No claim of reference-quality art, physical fire/quenching simulation or a sustained performance test.
