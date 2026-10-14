#pragma once
#include "CoreMinimal.h"
class AActor;
class AChuckCharacter;
struct FChultBridgePlank
{
    FName Name;
    FVector Grip;
    float Width=190, Tilt=75;
};
// All setting data is local to Chult FloorOrigin; proposals are NOT registered.
FVector ChultCanyonFrame(float S,float T,float Z=0);
const TArray<FChultBridgePlank>& ChultBridgePlanks();
bool InChultCanyon(const FVector& Local);
float ChultCanyonGroundZ(const FVector& Local);
void BuildChultCanyon(AActor* Owner);
int32 CheckChultCanyon(AActor* Owner,AChuckCharacter* Chuck);
void StartChultCanyonReview(AActor* Owner,AChuckCharacter* Chuck);
