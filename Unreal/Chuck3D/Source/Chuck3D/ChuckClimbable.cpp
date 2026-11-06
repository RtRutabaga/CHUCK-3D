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

namespace
{
    // The segment of its line at height Z, and how far up it (0..1).
    int32 ClimbSegment(const FChuckClimbable& C, float Z, float& Alpha)
    {
        const TArray<FVector>& L = C.Line;
        if (Z <= L[0].Z) { Alpha = 0; return 0; }
        for (int32 I = 0; I + 1 < L.Num(); ++I)
            if (Z <= L[I + 1].Z) { Alpha = static_cast<float>((Z - L[I].Z) / FMath::Max(1e-3, L[I + 1].Z - L[I].Z)); return I; }
        Alpha = 1; return L.Num() - 2;
    }
}

FVector ChuckClimbPoint(const FChuckClimbable& C, float Z)
{
    if (C.Line.Num() < 2) return FVector(C.Foot.X, C.Foot.Y, Z);
    float A; const int32 I = ClimbSegment(C, Z, A);
    const FVector P = FMath::Lerp(C.Line[I], C.Line[I + 1], A);
    return FVector(P.X, P.Y, Z);
}

float ChuckClimbSideTravel(const FChuckClimbable& C, float Z, float Bottom, float Top)
{
    return FMath::Lerp(C.SideTravel, C.TopSideTravel, FMath::Clamp((Z - Bottom) / FMath::Max(1.f, Top - Bottom), 0.f, 1.f));
}

FVector ChuckClimbFacing(const FChuckClimbable& C, float Z)
{
    const FVector Face = -C.Out;
    if (C.Approach.IsNearlyZero()) return Face;
    constexpr float Turn = 15.f;   // half the height he turns round over
    const float U = FMath::SmoothStep(C.TurnZ - Turn, C.TurnZ + Turn, Z);
    const float From = C.Approach.Rotation().Yaw;
    return FRotator(0, From + FMath::FindDeltaAngleDegrees(From, Face.Rotation().Yaw) * U, 0).Vector();
}

FVector ChuckClimbUp(const FChuckClimbable& C, float Z)
{
    if (C.Line.Num() < 2) return FVector::UpVector;
    float A; const int32 I = ClimbSegment(C, Z, A);
    return (C.Line[I + 1] - C.Line[I]).GetSafeNormal();
}

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
