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
// Separate sprint-leap-only rupture farther along, before the final descent.
int32 DockSewerLeapRiftStart();
int32 DockSewerLeapRiftEnd();

// Checkpoint shortly before the wall-run rupture (user 2026-10-04): a fall or
// sanity loss at or beyond it returns Chuck here rather than to the entrance.
int32 DockSewerCheckpointSample();
FVector DockSewerCheckpointLocation();
float DockSewerCheckpointYaw();
// Nearest route sample to a point inside the sewer, or INDEX_NONE.
int32 DockSewerNearestSample(const FVector& Position);
// Invisible floor over every Astral opening that only non-Chuck pawns stand
// on (Pawn channel and WorldStatic object queries); Chuck's capsule ignores it.
class UPrimitiveComponent* DockSewerAstralFloor();
