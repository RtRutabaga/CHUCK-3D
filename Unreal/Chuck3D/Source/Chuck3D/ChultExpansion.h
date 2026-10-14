#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;
bool InChultExpansion(const FVector& LocalPosition);
float ChultExpansionGroundZ(const FVector& LocalPosition);
void BuildChultExpansion(AActor* Owner);
int32 CheckChultExpansion(AActor* Owner, AChuckCharacter* Chuck);
void StartChultExpansionReview(AActor* Owner, AChuckCharacter* Chuck);
