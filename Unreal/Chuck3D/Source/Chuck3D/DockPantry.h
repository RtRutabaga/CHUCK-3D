#pragma once
#include "CoreMinimal.h"
class UWorld;
// Shaft clear bounds x20..130, y875..955. Landing floor -320; tavern floor 0.
// Ladder plane x124, rails y891/939. Claude owns climbing/return integration.
void BuildDockPantry(UWorld* World);
bool IsWithinDockPantry(const FVector& P);
