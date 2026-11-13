#pragma once

#include "CoreMinimal.h"

class AHUD;

// General-purpose stills (2026-11-13): -ChuckShots=<file> takes the camera positions from a text file instead of
// a new hand-written -Chuck...Capture block and a recompile for every review. One instruction per line, # comments:
//
//   at ChultBridge                       start from this checkpoint first (any dev checkpoint name; default: the docks)
//   evening                              the docks' evening (after the sewer)
//   wait 6                               seconds before the next shot (default 2; the first waits 4 more for streaming)
//   fov 70                               field of view for the following shots (default 75)
//   hud on                               keep the HUD and tutorial text in the following shots (default off)
//   shot <name> x y z  tx ty tz          a camera at x y z looking at tx ty tz (centimetres, world space)
//   chuck <name> x y z  tx ty tz         the same, with Chuck placed on the floor under tx ty tz first
//   audit                                list unshadowed lights that reach strand hair (LightAudit.h)
//
// Stills go to Saved/Screenshots/<platform>/Shots/<file name>/<name>.png, logged as CHUCK_SHOT; the game quits
// after the last one (CHUCK_SHOTS_COMPLETE). Example: Tools/shots/tavern-bin.txt.
namespace ChuckShots
{
    /** True (and the run started) when -ChuckShots= is on the command line. */
    bool Start(AHUD* Hud);
}
