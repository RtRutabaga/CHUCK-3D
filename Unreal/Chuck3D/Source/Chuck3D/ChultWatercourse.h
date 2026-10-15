#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;
void BuildChultWatercourse(AActor* Owner);
int32 CheckChultWatercourse(AActor* Owner,AChuckCharacter* Chuck);
void ReviewChultWatercourse(AActor* Owner,AChuckCharacter* Chuck,int32 Failures,bool ContinueTests=false);
void StartChultWatercourseWalks(AActor* Owner,AChuckCharacter* Chuck);
