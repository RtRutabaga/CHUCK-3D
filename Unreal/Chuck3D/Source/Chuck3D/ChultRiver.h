#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;
bool InChultRiverExtension(const FVector& P);
float ChultRiverDistance(const FVector& P);
float ChultRiverGroundZ(const FVector& P,float Base);
void BuildChultRiver(AActor* Owner);
int32 CheckChultRiver(AActor* Owner,AChuckCharacter* Chuck);
void StartChultRiverWalks(AActor* Owner,AChuckCharacter* Chuck);
