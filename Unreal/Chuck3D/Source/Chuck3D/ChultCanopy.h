#pragma once
#include "CoreMinimal.h"

class AActor;
// Setting coordinates are centimetres relative to AChultLanding::FloorOrigin().
// These are layout proposals, not registered gameplay grips. Claude owns catches,
// leaps, dismounts, registry lifecycle and their traversal tests.
struct FChultCanopyChain
{
    FName Name;
    TArray<FVector> Paws;
    bool bVine = false;
    // First paw on the neighbouring tree: no wood spans this transition.
    int32 BranchBreak = INDEX_NONE;
};
const TArray<FChultCanopyChain>& ChultCanopyChains();
bool InChultCanopyReserve(const FVector& LocalPosition, float Margin = 0);
void BuildChultCanopy(AActor* Owner);
