#pragma once
#include "CoreMinimal.h"
class UWorld;
void BuildDockSewer(UWorld* World);
// Narrow exception to the surface fall reset, including the entry shaft.
bool IsWithinDockSewer(const FVector& Position);
FVector DockSewerStartLocation();
bool IsInDockSewerStream(const FVector& Position);
