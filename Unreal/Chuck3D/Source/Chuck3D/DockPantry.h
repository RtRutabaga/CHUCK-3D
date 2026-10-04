#pragma once
#include "CoreMinimal.h"
class UWorld;
// Shaft clear bounds x20..130, y875..955. Landing floor -320; tavern floor 0.
// Ladder plane x124, rails y891/939 (a ChuckClimbable). The cellar is 8 x 7 m
// (x-450..350, y400..1100), its floor broken by Astral ruptures and a wide
// hole onto open sky round the island with the cheese (user 2026-10-03).
void BuildDockPantry(UWorld* World);
bool IsWithinDockPantry(const FVector& P);
/** Where Chuck comes back after a death down here: by the foot of the ladder. */
FVector DockPantryStartLocation();
/** 0 floor, 1 Astral rupture, 2 sky, at a point of the cellar floor (tests). */
int32 DockPantryHoleAt(const FVector2D& At);
FVector2D DockPantrySkyCentre();
float DockPantrySkyRadius();
float DockPantryIslandRadius();
