#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;
void BuildChultCliffs(AActor* Owner);
int32 CheckChultCliffs(AActor* Owner, AChuckCharacter* Chuck);
void StartChultCliffReview(AActor* Owner, AChuckCharacter* Chuck, int32 PreviousFailures);
