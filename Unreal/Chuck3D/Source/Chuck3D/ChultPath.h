#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;
void BuildChultPath(AActor* Owner);
int32 CheckChultPath(AActor* Owner, AChuckCharacter* Chuck);
void ReviewChultPath(AActor* Owner, AChuckCharacter* Chuck, int32 Failures);
