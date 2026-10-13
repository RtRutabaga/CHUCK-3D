# Rough-hewn pyrography trade signs

The tavern and all remaining business/trade signs use the shared `AddDockTradeSign` in `DockSigns.cpp`. Names and mounting positions are retained; removed area labels stay removed.

Each decorative sign has an irregular chipped timber silhouette, shallow bevel, weather checking and small rusted fixings. The board uses the existing scanned `M_WoodLight` wood with grain along the long dimension; darker wood detail marks battered ends. Lettering uses a dark brown scorched rim and near-black charcoal core on separate planes close to the timber face. This is a layered surface treatment suggesting burned-in letters, not a simulated carving or combustion system. Old board/text components are replaced, not overlaid.

Signs have no collision and do not change climb surfaces or movement. Existing shop brackets and architectural supports remain. The existing UE text font keeps trade names readable. No dependencies, imports or binary assets; existing timber provenance remains in WEATHERED-TIMBER.md. See HANDOFF for actual package/review evidence and launcher revision.

Developer review: `-ChuckSignCapture` discovers all actors tagged `DockTradeSign`, films 14 close-ups and exits automatically. Output: `Saved/Screenshots/Windows/TradeSigns/View00..13.png`. `Local/pyro-signs-contact.png` is an ignored contact sheet of the corrected close-ups. The review is opt-in and leaves normal player cameras unchanged. Front faces use Unreal clockwise winding; normals retain the outward direction, so rear cleats are properly occluded.
