#pragma once
#include "CoreMinimal.h"
class UWorld;
void BuildDockSewer(UWorld* World);
// Narrow exception to the surface fall reset, including the entry shaft.
bool IsWithinDockSewer(const FVector& Position);
FVector DockSewerStartLocation();
bool IsInDockSewerStream(const FVector& Position);
// The built route, read-only, for what lives in it (SewerLife.cpp): samples
// 65 cm apart along the stream, floor height, the sideways direction, half the
// floor width, Astral gaps and the wide chamber.
int32 DockSewerSamples();
FVector DockSewerPoint(int32 Index);
FVector DockSewerSide(int32 Index);
float DockSewerHalfWidth(int32 Index);
bool DockSewerIsGap(int32 Index);
bool DockSewerIsChamber(int32 Index);

// Full-width wall-run challenge immediately after the midpoint chamber.
int32 DockSewerWallRiftStart();
int32 DockSewerWallRiftEnd();
