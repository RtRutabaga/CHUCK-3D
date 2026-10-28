# Fire: torches, braziers, fireplaces, forge, lamps (Claude, 2026-10-27)

The user asked for every other fire in the game to look as good as the temple's flame traps. All of them are placed through `AddDockFlame` (`DockFire.h`): the docks' wall torches, plaza lamps and braziers, the tavern fireplace, the forge, the pantry candle, and the temple's room torches, braziers, great braziers and interior torches. Until now each was three crossed cards of `M_TorchFlame`.

Since this change `AddDockFlame` hands the fire to `AChuckFlames` (`ChuckFlames.h`), one per world. It draws the fire the way the jet traps do (`ChultTempleJetFire.cpp`), on instanced quads facing the camera, using Unity Labs' CC0 Houdini flipbooks:

- **Flame:** two or three sheets of `Flame02` (16×4, a simulated flame) side by side, each on its own frame, speed and height. Each sheet's height wavers on two slow beats of its own, so the flame never shows a loop. The fire jets' shader colours it by heat and fades it where it meets a wall or nears the camera (`M_ChuckFlame`).
- **Sparks:** a few a second, more from a larger fire. They rise on the heat with a wander and cool from yellow-white to dull red (`M_ChuckEmber`, procedural).
- **Smoke:** thin `WispySmoke01` wisps leave the tip, then rise, spread and thin (the jets' `M_TempleJetSmoke`, kept faint).
- **Cost:** only the nearest 40 fires within 45 m get every sheet plus sparks and smoke. Fires out to 160 m (80 in all) get one sheet. A fire whose owner is hidden (the docks while Chuck is in Chult, for example) is not drawn. The lights' flicker is unchanged: the docks' flicker and the temple's torch flicker.
- **Materials:** `Tools/create_flame_materials.py` builds both materials into `/Game/Art/Materials/Fire`. It reuses `import_temple_fire.py`'s flipbook shader and textures, so nothing was downloaded.
- **Checks:** the smoke test's `CHUCK_FIRE_CHECK` now counts the fires registered with `AChuckFlames` and their materials (70 fires, 0 missing). Stills: the docks' `-ChuckFireCapture` (`Saved/Screenshots/.../Fire/View*.png`) and the temple interior's stills.
- **Left alone:** the jets themselves, the beholder battle's Fireball and bolts, the smith's sparks (`M_FireEmber`), and Chuck's cigarette. `M_TorchFlame` is no longer used by anything.
