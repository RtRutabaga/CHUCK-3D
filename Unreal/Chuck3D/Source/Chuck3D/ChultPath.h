#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;
void BuildChultPath(AActor* Owner);
// Resolved local centre line, including detours around solid trees.
const TArray<FVector>& ChultFarRoad();
float ChultPathDistance(const FVector& Local);
int32 CheckChultPath(AActor* Owner, AChuckCharacter* Chuck);
void ReviewChultPath(AActor* Owner, AChuckCharacter* Chuck, int32 Failures);
