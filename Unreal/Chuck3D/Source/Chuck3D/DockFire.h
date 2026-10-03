#pragma once
#include "CoreMinimal.h"
class AActor;
class UWorld;
void AddDockFlame(AActor* Owner, FVector Base, float Width, float Height);
void FinishDockFire(UWorld* World);
