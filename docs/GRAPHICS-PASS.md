# Material and daylight study

This pass moves the existing docks toward the supplied character/harbor references. It is not a claim of Baldur's Gate 3 fidelity. The playable area, 65 cm scale, movement and two-camera comparison remain the same.

## Authored surfaces

Nineteen original materials live in `/Game/Art/Materials`. They distinguish gray-brown fur and a lighter chest; deep purple woven jacket cloth; skin, glossy eyes, claws, whiskers and brass; weathered timber, mineral stone, plaster, slate, iron, water and sky. Color variation and small surface-normal changes are procedural, without external texture packs or downloaded art. Fine cloth/fur detail is filtered at distance. Character patterns use interpolated undeformed positions so they follow the rig. World surfaces use centimetre-scaled world coordinates.

The fur material is a surface treatment, not a groom or hair-card system. Joint topology and the underlying character silhouette still need substantial work. Roughness varies by material; damp stone patches currently affect color, not a simulated water layer. The opaque water uses animated wave normals, not shoreline simulation, transparency or underwater rendering.

Quay paving now uses a consistent stone palette instead of alternating pale checkerboard colors. A directional sky gradient, restrained clouds, warmer direct light and lower ambient intensity replace the flat lighting balance. Distant existing warehouses gain pitched roofs, chimneys, windows and a harbor wall; they remain nonplayable scenery. No extra map or interior is included.

## Reproduce

The committed material assets are sufficient for ordinary builds. To regenerate the authored shaders, close Unreal and run from this repository:

```powershell
powershell -NoProfile -File .\Tools\Build-ArtMaterials.ps1
```

`Tools/create_art_materials.py` is the source. It reconnects new expressions without deleting objects held by loaded character materials. Repeated generation can leave disconnected graph nodes; these do not feed the rendered surface. Always inspect the cook log for `Failed to compile Material` or invalid shader maps: Unreal can finish packaging while substituting a default material. The wrapper checks generation errors, while the packaged rendered tests verify material assignments and gameplay.

The new art directory is explicitly included in cooking. Runtime character material slots select the authored art materials, with prototype assets retained as historical blockout resources. No new engine version, plugin, dependency, paid asset or installation is required. All generated `.uasset` source assets use the existing Git LFS policy; logs, caches and Windows packages remain ignored.

## Scanned stone and timber

Stone and the two timber materials now use six 2K CC0 Poly Haven maps, replacing their initial procedural color/normal treatment. Grey Stone Path by Amal Kumar tiles at 1.8 metres; Brown Planks 03 by Rob Tuytel uses a board-interior crop to avoid false joints across the modelled planks. Diffuse, packed AO/roughness/metallic and DirectX normals use world-space projection. The old decorative rectangular paving overlay is removed; quay collision is unchanged. Source URLs, license and SHA-256 hashes are recorded in SourceAssets/Surfaces/PolyHaven. Other surfaces remain original procedural materials.

Build-ArtMaterials.ps1 reapplies the committed scans after procedural generation, without downloading anything. The six source JPGs total about 16.3 MB and use LFS, as do imported texture assets. No new dependency or purchase is needed. This adds surface detail, not displacement, terrain geometry or a new map.

## Remaining visual work

Priorities after this study are better character anatomy/jacket folds and joint topology, a proper fur solution, authored UV texture work, believable prop meshes and improved water/shore transitions. Both camera views must remain useful at each step. Performance and visual fidelity need sustained profiling and playtesting before adopting heavier rendering features.

