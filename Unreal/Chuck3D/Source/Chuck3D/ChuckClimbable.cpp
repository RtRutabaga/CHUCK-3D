#include "ChuckClimbable.h"

namespace
{
    TArray<FChuckClimbable> Climbables;
}

void AddChuckClimbable(const FChuckClimbable& Climbable)
{
    for (const FChuckClimbable& C : Climbables)
        if (FVector::Dist(C.Foot, Climbable.Foot) < 1.f && FMath::Abs(C.TopZ - Climbable.TopZ) < 1.f) return;
    Climbables.Add(Climbable);
}

const TArray<FChuckClimbable>& GetChuckClimbables() { return Climbables; }
