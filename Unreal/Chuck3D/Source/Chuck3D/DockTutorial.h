#pragma once
#include "CoreMinimal.h"
class UWorld;
enum class EDockTutorial : uint8 { Run,Jump,WallCargo,WallLamps,Scratch,SmallEnemy,LargeEnemy,Dodge,WallRun,Leap,None };
struct FDockTutorial
{
    uint32 Seen=0;
    EDockTutorial Active=EDockTutorial::None;
    float Until=0;
    static EDockTutorial Candidate(UWorld* World,const FVector& P,bool Night,uint32 Suppressed=0);
    static FString Text(EDockTutorial Id);
    FString Update(UWorld* World,const FVector& P,bool Night,float Time);
};
void StartDockTutorialReview(UWorld* World);
