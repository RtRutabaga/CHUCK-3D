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

bool SetChuckClimbableBlocked(const FVector& Lip, bool bBlocked)
{
    for (FChuckClimbable& C : Climbables)
        if (FVector::Dist(C.Lip, Lip) < 5.f) { C.bBlocked = bBlocked; return true; }
    return false;
}

namespace
{
    TArray<FChuckSwingGrip> SwingGrips;
}

void AddChuckSwingGrip(const FChuckSwingGrip& Grip)
{
    for (const FChuckSwingGrip& G : SwingGrips)
        if (FVector::Dist(G.Grip, Grip.Grip) < 1.f) return;
    SwingGrips.Add(Grip);
}

const TArray<FChuckSwingGrip>& GetChuckSwingGrips() { return SwingGrips; }
