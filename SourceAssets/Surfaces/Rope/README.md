# Rope (CC0)

ambientCG **Rope 001** (https://ambientcg.com/view?id=Rope001), CC0 1.0, 1K JPG set: colour, DirectX normal and roughness.
Fetched by `Tools/Fetch-RopeTextures.ps1` (archive and file hashes in `manifest.json`); imported with
`Tools/create_rope_material.py` as `/Game/Art/Textures/Rope/T_Rope001_*` and the material `/Game/Art/Materials/M_Rope`.

The image is a seamless strip of diagonal strands: 8 strand crossings across its width (U) and 7 down its height (V),
about 30 degrees to U. On a rope tube the length runs along U (26 cm a repeat) and 3/7 of V goes round the circumference,
so three strands show round it, as on a hawser-laid (three-strand) rope with a lay of 9.75 cm (`Source/Chuck3D/ShipRope.h`).
