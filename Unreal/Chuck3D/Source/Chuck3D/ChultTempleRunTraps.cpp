// The trap run's moving traps (Claude, 2026-10-22; ChultTempleRun.h, docs/CHULT-TEMPLE-RUN.md):
// AChultTempleTraps' great axes, fire jets and saws. Every trap shows and sounds its state
// before it can hurt (the research behind the run: telegraph everything), and a hit knocks
// him back along the run, never sideways into a pit.
#include "ChultTempleTraps.h"
#include "ChuckCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"

namespace
{
    float CycleU(double Time, float Phase, float Period)
    {
        return FMath::Fmod(static_cast<float>(Time) - Phase + Period * 1000.f, Period);
    }
    // Distance from P to his capsule's axis (the segment between its spheres' centres).
    float ToCapsule(const FVector& P, const FVector& Location, float Half, float Radius)
    {
        const FVector A = Location - FVector(0, 0, Half - Radius), B = Location + FVector(0, 0, Half - Radius);
        return FMath::PointDistToSegment(P, A, B);
    }
}

// ---------------------------------------------------------------- axes
void AChultTempleTraps::AddAxe(UProceduralMeshComponent* Mesh, const FVector& Pivot, const FVector& Route, float Arm, float Amplitude, float Period, float Phase)
{
    LoadSounds();
    FAxe A; A.Mesh = Mesh; A.Pivot = Pivot; A.Route = Route.GetSafeNormal2D();
    A.Across = FVector::CrossProduct(FVector::UpVector, A.Route);
    A.Arm = Arm; A.Amplitude = Amplitude; A.Period = Period; A.Phase = Phase;
    A.Base = FRotationMatrix::MakeFromXY(A.Route, A.Across).Rotator();
    Axes.Add(A);
}

float AChultTempleTraps::AxeTheta(int32 I, double Time) const
{
    const FAxe& A = Axes[I];
    return A.Amplitude * FMath::Sin(2.f * PI * CycleU(Time, A.Phase, A.Period) / A.Period);
}

void AChultTempleTraps::AxePoints(int32 I, double Time, TArray<FVector>& Out) const
{
    const FAxe& A = Axes[I];
    const float Theta = FMath::DegreesToRadians(AxeTheta(I, Time));
    auto Dir = [&](float Off) { return A.Across * FMath::Sin(Theta + Off) - FVector::UpVector * FMath::Cos(Theta + Off); };
    // The arm's lower half.
    for (float R = A.Arm * .5f; R < A.Arm - AxeBladeIn; R += 25.f) Out.Add(A.Pivot + Dir(0) * R);
    // The crescent: from its back (inside the arm's end) out to the edge, widest in the middle.
    const float Span = AxeBladeHalf / A.Arm;
    for (int32 K = -4; K <= 4; ++K)
    {
        const float U = K / 4.f, Off = U * Span;
        float Back, Edge; AxeBladeRadii(U, A.Arm, Back, Edge);
        for (float F : {0.f, .35f, .7f, 1.f}) Out.Add(A.Pivot + Dir(Off) * FMath::Lerp(Back, Edge, F));
    }
}

bool AChultTempleTraps::AxeMeets(int32 I, double Time, const FVector& Location, float Half, float Radius) const
{
    if (FVector::Dist(Location, Axes[I].Pivot) > Axes[I].Arm + AxeBladeOut + Half + 60.f) return false;
    TArray<FVector> Points; AxePoints(I, Time, Points);
    for (const FVector& P : Points) if (ToCapsule(P, Location, Half, Radius) < Radius + 3.f) return true;
    return false;
}

void AChultTempleTraps::HitBack(AChuckCharacter* Chuck, const FVector& Route, int32& Counter, const TCHAR* What, int32 Index)
{
    // Knocked back along the run, toward where he came from (never off a causeway or into a pit).
    const FVector At = Chuck->GetActorLocation();
    if (!Chuck->TakeBite(At + Route.GetSafeNormal2D() * 60.f, 1, false, true)) return;
    ++Counter;
    Play(TrapHit, At, .8f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_HIT trap=%s index=%d sanity=%d"), What, Index, Chuck->GetSanity());
}

void AChultTempleTraps::TickAxes(AChuckCharacter* Chuck, double Now)
{
    const FVector At = Chuck ? Chuck->GetActorLocation() : FVector::ZeroVector;
    const float Half = Chuck ? Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
    const float Radius = Chuck ? Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
    for (int32 I = 0; I < Axes.Num(); ++I)
    {
        FAxe& A = Axes[I];
        const float Theta = AxeTheta(I, Now);
        if (A.Mesh.IsValid()) A.Mesh->SetWorldRotation(FQuat(A.Route, FMath::DegreesToRadians(Theta)) * A.Base.Quaternion());
        const bool Near = Chuck && FVector::Dist(At, A.Pivot) < 1800.f;
        // Through the bottom of its swing: the whoosh. At either end: the pivot's creak.
        if (Near && FMath::Sign(Theta) != FMath::Sign(A.LastTheta) && A.LastTheta != 0) Play(AxeSwoosh, A.Pivot - FVector(0, 0, A.Arm), .75f);
        const float U = CycleU(Now, A.Phase, A.Period), Was = CycleU(Now - .02, A.Phase, A.Period);
        for (float Mark : {A.Period * .25f, A.Period * .75f})
            if (Near && Was < Mark && U >= Mark) Play(AxeCreak, A.Pivot, .35f);
        A.LastTheta = Theta;
        if (Chuck && !Chuck->IsAstral() && AxeMeets(I, Now, At, Half, Radius)) HitBack(Chuck, A.Route, AxeHits, TEXT("axe"), I);
    }
}

// ---------------------------------------------------------------- fire jets
void AChultTempleTraps::AddJet(UProceduralMeshComponent* Flame, UProceduralMeshComponent* Ember, UPointLightComponent* Light, const FVector& Mouth, const FVector& Dir, const FVector& Route, float Reach, float Period, float Phase, float Warn, float Burn)
{
    LoadSounds();
    FJet J; J.Flame = Flame; J.Ember = Ember; J.Light = Light; J.Mouth = Mouth; J.Dir = Dir.GetSafeNormal(); J.Route = Route.GetSafeNormal2D();
    J.Reach = Reach; J.Period = Period; J.Phase = Phase; J.Warn = Warn; J.Burn = Burn;
    if (Flame) Flame->SetVisibility(false);
    if (Ember) Ember->SetVisibility(false);
    if (Light) Light->SetIntensity(0.f);
    Jets.Add(J);
}

int32 AChultTempleTraps::JetStage(int32 I, double Time, float* OutExtent) const
{
    const FJet& J = Jets[I];
    const float U = CycleU(Time, J.Phase, J.Period);
    if (OutExtent) *OutExtent = 0;
    if (U < J.Warn) return 1;
    if (U < J.Warn + J.Burn)
    {
        // Out to full length in 0.12 s, drawn back over the last 0.1 s.
        if (OutExtent) *OutExtent = FMath::Clamp(FMath::Min((U - J.Warn) / .12f, (J.Warn + J.Burn - U) / .1f), .05f, 1.f);
        return 2;
    }
    return 0;
}

bool AChultTempleTraps::JetMeets(int32 I, double Time, const FVector& Location, float Half, float Radius) const
{
    float Extent = 0;
    if (JetStage(I, Time, &Extent) != 2) return false;
    const FJet& J = Jets[I];
    const FVector Rel = Location - J.Mouth;
    const float Out = static_cast<float>(FVector::DotProduct(Rel, J.Dir)), Side = static_cast<float>(FMath::Abs(FVector::DotProduct(Rel, J.Route)));
    if (Out < -Radius || Out > J.Reach * Extent + Radius * .5f || Side > JetHalfWidth + Radius * .6f) return false;
    // The jet is about 70 cm tall round its line: his body anywhere in it.
    return Location.Z - Half < J.Mouth.Z + 35.f && Location.Z + Half > J.Mouth.Z - 35.f;
}

void AChultTempleTraps::TickJets(AChuckCharacter* Chuck, double Now)
{
    const FVector At = Chuck ? Chuck->GetActorLocation() : FVector::ZeroVector;
    const float Half = Chuck ? Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
    const float Radius = Chuck ? Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
    const float T = static_cast<float>(Now);
    for (int32 I = 0; I < Jets.Num(); ++I)
    {
        FJet& J = Jets[I];
        float Extent = 0;
        const int32 Stage = JetStage(I, Now, &Extent);
        const float U = CycleU(Now, J.Phase, J.Period);
        const float Flick = .88f + .08f * FMath::Sin(T * 23.f + I * 1.7f) + .06f * FMath::Sin(T * 37.f + I);
        if (UProceduralMeshComponent* F = J.Flame.Get())
        {
            F->SetVisibility(Stage == 2);
            if (Stage == 2) F->SetRelativeScale3D(FVector(Extent, Flick, Flick));
        }
        if (UProceduralMeshComponent* E = J.Ember.Get())
        {
            // The tell: a flame licking at the mouth, growing toward the burst.
            E->SetVisibility(Stage != 0);
            const float G = Stage == 1 ? .35f + .65f * U / J.Warn : 1.f;
            E->SetRelativeScale3D(FVector(G * Flick, G, G));
        }
        if (UPointLightComponent* L = J.Light.Get())
            L->SetIntensity(Stage == 2 ? 9000.f * Extent * Flick : Stage == 1 ? 1600.f * U / J.Warn : 0.f);
        const bool Near = Chuck && FVector::Dist(At, J.Mouth) < 2200.f;
        if (Stage != J.Stage)
        {
            if (Stage == 1 && Near) Play(FireHiss, J.Mouth, .55f);
            if (Stage == 2) { ++JetsFired; if (Near) Play(FireRoar, J.Mouth + J.Dir * J.Reach * .4f, .9f); }
            J.Stage = Stage;
        }
        if (Chuck && !Chuck->IsAstral() && JetMeets(I, Now, At, Half, Radius)) HitBack(Chuck, J.Route, JetHits, TEXT("fire"), I);
    }
}

// ---------------------------------------------------------------- saws
void AChultTempleTraps::AddSaw(UProceduralMeshComponent* Disc, UProceduralMeshComponent* Arm, const FVector& Axle, const FVector& Route, float Radius, bool bDrops, float Lift, float Period, float Phase)
{
    LoadSounds();
    FSaw S; S.Disc = Disc; S.Arm = Arm; S.Axle = Axle; S.Route = Route.GetSafeNormal2D();
    S.Across = FVector::CrossProduct(FVector::UpVector, S.Route);
    S.Radius = Radius; S.bDrops = bDrops; S.Lift = bDrops ? Lift : 0.f; S.Period = Period; S.Phase = Phase;
    if (Disc) S.DiscBase = Disc->GetComponentLocation();
    if (Arm) S.ArmBase = Arm->GetComponentLocation();
    Saws.Add(S);
}

float AChultTempleTraps::SawHeight(int32 I, double Time) const
{
    const FSaw& S = Saws[I];
    if (!S.bDrops) return 0.f;
    const float U = CycleU(Time, S.Phase, S.Period);
    if (U < SawUp) return S.Lift;
    if (U < SawUp + SawTell) return S.Lift - 12.f * (U - SawUp) / SawTell;   // its teeth lower: the tell
    const float Down = SawUp + SawTell;
    if (U < Down + .08f) return FMath::Lerp(S.Lift - 12.f, 0.f, (U - Down) / .08f);
    if (U < S.Period - SawRise) return 0.f;
    return S.Lift * FMath::SmoothStep(0.f, 1.f, (U - (S.Period - SawRise)) / SawRise);
}

bool AChultTempleTraps::SawMeets(int32 I, double Time, const FVector& Location, float Half, float Radius) const
{
    const FSaw& S = Saws[I];
    const FVector Centre = S.Axle + FVector(0, 0, SawHeight(I, Time));
    const FVector Rel = Location - Centre;
    if (FMath::Abs(FVector::DotProduct(Rel, S.Route)) > Radius + 2.f) return false;
    const float Side = FMath::Max(0.f, static_cast<float>(FMath::Abs(FVector::DotProduct(Rel, S.Across))) - Radius * .7f);
    if (Side >= S.Radius) return false;
    const float Bottom = static_cast<float>(Centre.Z) - FMath::Sqrt(S.Radius * S.Radius - Side * Side);
    return Location.Z + Half > Bottom + 3.f && Location.Z - Half < Centre.Z + S.Radius;
}

void AChultTempleTraps::TickSaws(AChuckCharacter* Chuck, double Now, float Dt)
{
    const FVector At = Chuck ? Chuck->GetActorLocation() : FVector::ZeroVector;
    const float Half = Chuck ? Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
    const float Radius = Chuck ? Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
    int32 Nearest = -1; float NearestDist = 1e9f;
    for (int32 I = 0; I < Saws.Num(); ++I)
    {
        FSaw& S = Saws[I];
        const float H = SawHeight(I, Now);
        S.Spin = FMath::Fmod(S.Spin + Dt * 520.f, 360.f);
        if (UProceduralMeshComponent* D = S.Disc.Get())
            D->SetWorldLocationAndRotation(S.DiscBase + FVector(0, 0, H), FQuat(S.Route, FMath::DegreesToRadians(-S.Spin)) * FRotationMatrix::MakeFromXY(S.Route, S.Across).ToQuat());
        if (UProceduralMeshComponent* A = S.Arm.Get()) A->SetWorldLocation(S.ArmBase + FVector(0, 0, H));
        const float Dist = Chuck ? static_cast<float>(FVector::Dist(At, S.Axle)) : 1e9f;
        if (Dist < NearestDist) { NearestDist = Dist; Nearest = I; }
        if (S.bDrops)
        {
            const float U = CycleU(Now, S.Phase, S.Period);
            const int32 Stage = U < SawUp ? 0 : U < SawUp + SawTell ? 1 : U < S.Period - SawRise ? 2 : 3;
            if (Stage != S.Stage)
            {
                if (Stage == 2) { ++SawDrops; if (Dist < 2000.f) Play(SawDrop, S.Axle, .8f); }
                S.Stage = Stage;
            }
        }
        if (!Chuck || Chuck->IsAstral()) continue;
        // Rolling, he passes under it: counted once a pass. Otherwise its teeth find him.
        const bool bLowEnough = SawMeets(I, Now, At, Half, Radius);
        if (!bLowEnough) { SawRolled.Remove(I); continue; }
        if (Chuck->IsRolling())
        {
            if (!SawRolled.Contains(I)) { SawRolled.Add(I); ++SawRollUnders; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_RUN_ROLL_UNDER saw=%d"), I); }
            continue;
        }
        HitBack(Chuck, S.Route, SawHits, TEXT("saw"), I);
    }
    // The nearest saw's whine, now and then.
    if (Nearest >= 0 && NearestDist < 1500.f && Now >= NextSawSpin) { Play(SawSpin, Saws[Nearest].Axle, .45f); NextSawSpin = Now + 1.1; }
}
