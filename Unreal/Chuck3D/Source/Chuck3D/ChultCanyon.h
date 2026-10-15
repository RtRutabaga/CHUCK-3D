#pragma once
#include "CoreMinimal.h"
#include "ChultBridge.h"
class AActor;
class AChuckCharacter;
// All setting data is local to Chult FloorOrigin. The bridge itself (planks,
// ropes, posts and its brachiation grips) is Claude's ChultBridge.h.
FVector ChultCanyonFrame(float S,float T,float Z=0);
bool InChultCanyon(const FVector& Local);
float ChultCanyonGroundZ(const FVector& Local);
void BuildChultCanyon(AActor* Owner);
int32 CheckChultCanyon(AActor* Owner,AChuckCharacter* Chuck);
void StartChultCanyonReview(AActor* Owner,AChuckCharacter* Chuck);
