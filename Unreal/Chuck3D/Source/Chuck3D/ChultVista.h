#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;
void BuildChultVista(AActor* Owner);
int32 CheckChultVista(AActor* Owner,AChuckCharacter* Chuck);
void ReviewChultVista(AActor* Owner,AChuckCharacter* Chuck,int32 Failures,bool ContinueTests=false);
