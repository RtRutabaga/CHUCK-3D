# Returning from the sewer

The user assigned the remaining step from Claude's sewer plan to Codex: after the water slide, Waterdeep is in early evening, the sewer hatch is closed and the tavern opens. This is a session state in the same world, with no loading screen or quest prompt. Existing rats, moss, zombie and slide/pier-climb behavior remain.

Start a fresh launch in morning light. The rusty hatch beside the side gate is open; the tavern's timber door blocks entry. Drop into the hatch, follow the winding sewer and enter the low water-slide mouth at its end. Claude's slide sequence marks the exit while the screen is black. The setting changes before the pier view fades in: dimmer warm-grey sunlight, darker sky/fog, lamps and torches remaining visible, hatch lowered on its hinge and tavern door swung inward.

The closed hatch has matching bars and a hidden collision plate under them so Chuck can walk over it without falling between narrow gaps. Fold-out hatch stays are hidden and lose collision when closed. The same furnished tavern room becomes accessible; there is no new dialogue, bartender, upstairs, pantry or campaign transition.

R returns Chuck to dock spawn but keeps evening, the closed hatch and open tavern. Falling or losing sanity does not undo the session state. Quit and relaunch to begin in morning again. No disk save system is introduced. Sewer fill/ruptures remain at the last approved grey-blue settings; restoring outdoor light applies the evening values after the exit.

Implementation: `DockReturn.cpp/.h` observes `HasExitedDockSewer()`. `DockSetting` owns the movable hatch; `DockGameMode` owns the movable tavern door. The sky material has a default-white `SkyTint` parameter, darkened only on return. `Tools/create_plaza_materials.py` accepts Unreal command-line `-ChuckSkyOnly` to regenerate only this owned material, preserving fountain/flame graphs. No new dependency.

Verification flags: `-ChuckReturnTest` checks morning state and actual door blocking, walks into the slide, checks evening/hatch/door/light state after the real pier climb, walks through the open tavern doorway and checks persistence after reset. `-ChuckReturnCapture` records paired tavern/hatch views before/after setting the exit flag for controlled visual comparison. Standard smoke checks now include both surface states and the tavern's initial closed doorway. `-ChuckTavernInteriorTest` explicitly enables the post-sewer state for its two interior circuits. See latest HANDOFF for what actually passed.
