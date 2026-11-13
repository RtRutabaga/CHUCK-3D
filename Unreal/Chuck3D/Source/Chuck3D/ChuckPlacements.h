#pragma once

#include "CoreMinimal.h"

class UWorld;

// Hand-placed dressing props from a table (2026-11-13), so moving or adding one is an edit to a text file and a
// rerun, not a C++ edit and a recompile. Content/Data/Placements/<Region>.txt (staged into packages as UFS),
// one prop per line, # comments:
//
//   prop <mesh path> x y z yaw [scale=S] [cyl=DIAMxHEIGHT] [box=XxYxZ] [tag=Name]
//
// Positions in world centimetres, yaw in degrees. cyl/box add a hidden BlockAll proxy standing on z; without
// either the prop has no collision. Each prop is logged as CHUCK_PLACEMENT; a missing mesh is a warning.
// The procedural region builders stay the place for anything structural (floors, walls, routes).
namespace ChuckPlacements
{
    /** Spawns the region's table under one actor tagged Placements_<Region>. Returns how many were placed. */
    int32 Spawn(UWorld* World, const TCHAR* Region);
}
