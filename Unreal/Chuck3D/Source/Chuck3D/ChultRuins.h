#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;
bool InChultRuinsReserve(const FVector& LocalPosition, float Margin=0);
void BuildChultRuins(AActor* Owner);
int32 CheckChultRuins(AActor* Owner, AChuckCharacter* Chuck);
void StartChultRuinsReview(AActor* Owner, AChuckCharacter* Chuck);
