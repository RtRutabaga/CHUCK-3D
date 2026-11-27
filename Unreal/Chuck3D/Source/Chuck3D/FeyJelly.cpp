// The Deepwood's jellyfish (Claude, 2026-11-26). See FeyJelly.h.
#include "FeyJelly.h"
#include "FeyMap.h"
#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    TArray<TWeakObjectPtr<AFeyJelly>> GJellies;
    float Smooth(float A, float B, float X) { const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f); return T * T * (3 - 2 * T); }
    // Each one's tint (the plan: pale teal, a violet rim), a little different across them.
    const FLinearColor Tints[] = {FLinearColor(1.f, 1.f, 1.f), FLinearColor(.9f, 1.f, 1.1f), FLinearColor(1.08f, .95f, 1.f), FLinearColor(.95f, 1.05f, .95f)};
    constexpr int32 NS = 40;                 // the bell's segments round
    constexpr int32 NT = 15;                 // its rings, apex to the curled rim
    constexpr int32 StrandPts = 13;
    constexpr float Gravity = 980.f;
    constexpr float SwingDamp = 1.7f;
    constexpr float SwingLen = AFeyJelly::ArmDrop + 32.f;   // the bell's rim to his middle
    /** The pulse's contraction (0 relaxed, 1 drawn in): quick in, slow out. */
    float Contraction(float Ph) { return Ph < .28f ? FMath::Sin(Ph / .28f * HALF_PI) : FMath::Cos((Ph - .28f) / .72f * HALF_PI); }
    /** The bell's surface at ring parameter T (0 apex, 1 rim, beyond: the curled margin) and angle A, contracted by C. */
    FVector BellAt(float T, float A, float C)
    {
        const float R = AFeyJelly::BellR, H = AFeyJelly::BellH;
        float Rr, Z;
        if (T <= 1.f) { Rr = R * FMath::Pow(FMath::Sin(T * HALF_PI), .85f); Z = H * (1.f + .14f * C) * (1.f - FMath::Pow(T, 2.2f)); }
        else { const float U = (T - 1.f) / .14f; Rr = R * (1.f - .07f * U); Z = -9.f * U * U; }
        Rr *= 1.f - .2f * C * FMath::Pow(FMath::Min(T, 1.f), 1.5f);
        Rr += 2.6f * FMath::Cos(16.f * A) * Smooth(.86f, 1.1f, T);   // (the lappets)
        return FVector(Rr * FMath::Cos(A), Rr * FMath::Sin(A), Z);
    }
}

int32 AFeyJelly::Holds = 0, AFeyJelly::Releases = 0, AFeyJelly::Shakes = 0, AFeyJelly::Refusals = 0, AFeyJelly::Blocks = 0, AFeyJelly::Dips = 0;
float AFeyJelly::TopSpeed = 0.f;
FVector AFeyJelly::TestStick = FVector::ZeroVector;
bool AFeyJelly::bTestStick = false;

const TArray<TWeakObjectPtr<AFeyJelly>>& AFeyJelly::All() { GJellies.RemoveAll([](const TWeakObjectPtr<AFeyJelly>& J) { return !J.IsValid(); }); return GJellies; }
TArray<AFeyJelly::FBarrier>& AFeyJelly::Barriers() { static TArray<FBarrier> B; return B; }

AFeyJelly::AFeyJelly()
{
    PrimaryActorTick.bCanEverTick = true;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
    Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Bell"));
    Mesh->SetupAttachment(Root);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(false);
    Mesh->bUseAsyncCooking = false;
    Mesh->SetAbsolute(true, true, true);   // (drawn in world space: the strands are simulated there)
    Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Light->SetupAttachment(Root);
    Light->SetRelativeLocation(FVector(0, 0, 10.f));
    Light->SetIntensityUnits(ELightUnits::Candelas);
    Light->SetIntensity(14.f);
    Light->SetAttenuationRadius(650.f);
    Light->SetLightColor(FLinearColor(.4f, .9f, 1.f));
    Light->SetCastShadows(false);
    Light->SetSourceRadius(30.f);
    Hum = CreateDefaultSubobject<UAudioComponent>(TEXT("Hum"));
    Hum->SetupAttachment(Root);
    Hum->bAutoActivate = false;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> M(TEXT("/Game/Art/Feywild/Creatures/Jelly/M_FeyJelly.M_FeyJelly"));
    if (M.Succeeded()) { Mesh->SetMaterial(0, M.Object); Mesh->SetMaterial(1, M.Object); }
}

void AFeyJelly::BeginPlay()
{
    Super::BeginPlay();
    GJellies.Add(this);
    Rand.Initialize(static_cast<int32>(GetUniqueID()) * 7 + 3);
    Phase = Rand.FRand(); Wobble = Rand.FRandRange(0.f, 10.f);
    if (UMaterialInterface* M = Mesh->GetMaterial(0)) { Mat = UMaterialInstanceDynamic::Create(M, this); Mesh->SetMaterial(0, Mat); Mesh->SetMaterial(1, Mat); }
    Attenuation = NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate = true; Attenuation->Attenuation.bSpatialize = true;
    Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Attenuation->Attenuation.AttenuationShapeExtents = FVector(200.f, 0, 0);
    Attenuation->Attenuation.FalloffDistance = 1600.f;
    for (int32 I = 0; I < 3; ++I)
        if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/SFX_FeyJellyPulse_%02d.SFX_FeyJellyPulse_%02d"), I, I))) Pulses.Add(S);
    if (USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_FeyJellyHum.SFX_FeyJellyHum")))
    {
        Hum->SetSound(S); Hum->AttenuationSettings = Attenuation; Hum->SetVolumeMultiplier(.4f); Hum->Play(Rand.FRandRange(0.f, 4.f));
    }
}

void AFeyJelly::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Rider.IsValid()) Rider->EndJellyHang(false);
    Rider.Reset();
    GJellies.Remove(this);
    Super::EndPlay(Reason);
}

void AFeyJelly::Init(int32 InDistrict, const FVector& HomeWorld, int32 InIndex)
{
    District = InDistrict; Home = HomeWorld; Index = InIndex;
    if (Mat) Mat->SetVectorParameterValue(TEXT("Tint"), Tints[Index % UE_ARRAY_COUNT(Tints)]);
    ResetJelly();
}

void AFeyJelly::ResetJelly()
{
    if (Rider.IsValid()) { Rider->EndJellyHang(false); Rider.Reset(); }
    Vel = Accel = FVector::ZeroVector; Swing = SwingRate = FVector2D::ZeroVector; SwingQ = FQuat::Identity; Lift = 0; RefuseTime = 0; bRefusing = false;
    float G = Home.Z, Top = 1e9f; Heights(Home + FVector(0, 0, 300.f), G, Top);
    GroundZ = G;
    SetActorLocation(FVector(Home.X, Home.Y, G + IdleGripUp + ArmDrop), false, nullptr, ETeleportType::TeleportPhysics);
    Grip = GetActorLocation() - FVector(0, 0, ArmDrop);
    FacingYaw = Rand.FRandRange(-180.f, 180.f);
    Goal = GetActorLocation(); NextGoal = 0;
    SetState(EState::Drift);
    BuildStrands();
}

const TCHAR* AFeyJelly::GetStateName() const
{
    static const TCHAR* N[] = {TEXT("Drift"), TEXT("Rise"), TEXT("Held"), TEXT("Wait")};
    return N[static_cast<int32>(State)];
}

float AFeyJelly::GetSwingForwardDeg() const
{
    const FVector2D F(FMath::Cos(FMath::DegreesToRadians(FacingYaw)), FMath::Sin(FMath::DegreesToRadians(FacingYaw)));
    return FMath::RadiansToDegrees(FVector2D::DotProduct(Swing, F));
}

// ------------------------------------------------------------------ where it may go
bool AFeyJelly::Heights(const FVector& At, float& Ground, float& MaxTop) const
{
    // The ground: the highest of the land, roots and logs under him (a few points round where his feet hang), so it rises
    // over them rather than dragging him through.
    FCollisionQueryParams Q(SCENE_QUERY_STAT(FeyJellyHeight), false, this);
    if (Rider.IsValid()) Q.AddIgnoredActor(Rider.Get());
    const FVector D = Vel.GetSafeNormal2D().IsNearlyZero() ? FRotator(0, FacingYaw, 0).Vector() : Vel.GetSafeNormal2D();
    bool bAny = false; float Best = -1e9f;
    for (const FVector& O : {FVector::ZeroVector, D * 35.f, -D * 30.f, FVector(-D.Y, D.X, 0) * 22.f, FVector(D.Y, -D.X, 0) * 22.f})
    {
        FHitResult H;
        const FVector From(At.X + O.X, At.Y + O.Y, At.Z + BellH);
        if (GetWorld()->LineTraceSingleByChannel(H, From, From - FVector(0, 0, 3000.f), ECC_Visibility, Q)) { Best = FMath::Max(Best, static_cast<float>(H.ImpactPoint.Z)); bAny = true; }
    }
    if (!bAny) return false;
    Ground = Best;
    // Overhead: the lowest thing above where he hangs, up to where the bell would ride.
    MaxTop = 1e9f;
    FHitResult H;
    const FVector Up(At.X, At.Y, Ground + 45.f);
    if (GetWorld()->LineTraceSingleByChannel(H, Up, FVector(At.X, At.Y, Ground + RideGripUp + ArmDrop + BellH + 80.f), ECC_Visibility, Q)) MaxTop = static_cast<float>(H.ImpactPoint.Z) - 10.f;
    return true;
}

bool AFeyJelly::InShakeZone(const FVector& At) const
{
    // (The hare's rule, the user's: off only on its way out of its district.)
    const FVector L = FeyLocal(At);
    if (FeyMap::DistrictAt(L.X, L.Y) != District) return true;
    float T; return FeyMap::ThresholdAt(FVector(L.X, L.Y, L.Z - ArmDrop - RideGripUp), T) >= 0;
}

bool AFeyJelly::RefuseDir(const FVector& At, FVector& Dir) const
{
    const FVector2D L(FeyLocal(At));
    bool bRefused = false;
    auto Keep = [&](const FVector2D& Toward2, float D, float Range)
    {
        if (D > Range || D < 1.f) return;
        const FVector N(Toward2.X / D, Toward2.Y / D, 0.f);
        const float Toward = FVector::DotProduct(Dir, N);
        if (Toward > 0.f) { Dir -= N * Toward * (D < Range * .75f ? 1.25f : 1.f); bRefused |= Toward > .35f; }
    };
    // Threshold mouths on its side.
    for (int32 Th = 0; Th < FeyMap::Thresholds; ++Th)
    {
        const FeyMap::FThreshold& W = FeyMap::Threshold(Th);
        const FVector2D* Mouth = W.A == District ? &W.PA : W.B == District ? &W.PB : nullptr;
        if (Mouth) Keep(*Mouth - L, static_cast<float>((*Mouth - L).Size()), RefuseRange);
    }
    // Redcap Hollow's clearing: met on foot (the plan's recommendation).
    if (District == FeyMap::Deepwood)
    {
        const FVector2D C = FeyMap::RedcapAt(); const FVector2D To = C - L; const float D = static_cast<float>(To.Size());
        Keep(To, D - HollowKeep + RefuseRange, RefuseRange);
        if (D < HollowKeep && D > 1.f) { Dir -= FVector(To.X / D, To.Y / D, 0.f) * .6f; bRefused = true; }
    }
    // A crossing's barrier: never past it, either way.
    for (const FBarrier& B : Barriers())
    {
        const FVector2D AB = B.B - B.A; const float T = FMath::Clamp(FVector2D::DotProduct(L - B.A, AB) / FMath::Max(1.f, static_cast<float>(AB.SizeSquared())), 0.f, 1.f);
        const FVector2D To = B.A + AB * T - L;
        Keep(To, static_cast<float>(To.Size()), 600.f);
    }
    // ...and never off its district's land.
    const FVector Ahead = At + Dir.GetSafeNormal2D() * 300.f;
    const FVector La = FeyLocal(Ahead);
    if (FeyMap::DistrictAt(La.X, La.Y) != District) { Dir = -Dir * .2f; bRefused = true; }
    return bRefused;
}

bool AFeyJelly::PickGoal(FVector& Out)
{
    const FVector At = GetActorLocation();
    for (int32 Try = 0; Try < 8; ++Try)
    {
        const float A = Rand.FRandRange(0.f, 2.f * PI), D = Rand.FRandRange(250.f, 700.f);
        FVector P = At + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * D;
        if (FVector::Dist2D(P, Home) > Leash) P = Home + (P - Home).GetSafeNormal2D() * Leash * .7f;
        FVector Dir = (P - At).GetSafeNormal2D();
        if (RefuseDir(At, Dir)) continue;
        float G, Top;
        if (!Heights(FVector(P.X, P.Y, At.Z), G, Top) || Top - BellH < G + IdleGripUp + ArmDrop) continue;
        FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(FeyJellyGoal), false, this);
        if (GetWorld()->SweepSingleByChannel(H, At, FVector(P.X, P.Y, At.Z), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(BellR * .8f), Q)) continue;
        Out = FVector(P.X, P.Y, At.Z); return true;
    }
    return false;
}

// ------------------------------------------------------------------ holding on
AFeyJelly* AFeyJelly::HoldPrompt(const AChuckCharacter* Chuck)
{
    if (!Chuck) return nullptr;
    AFeyJelly* Best = nullptr; float BestD = Reach + 1.f;
    for (const TWeakObjectPtr<AFeyJelly>& J : All())
        if (J->CanHold(Chuck))
        {
            const float D = static_cast<float>(FVector::Dist2D(J->GetGrip(), Chuck->GetActorLocation()));
            if (D < BestD) { Best = J.Get(); BestD = D; }
        }
    return Best;
}

bool AFeyJelly::CanHold(const AChuckCharacter* Chuck) const
{
    if (!Chuck || Rider.IsValid() || State == EState::Rise || State == EState::Held) return false;
    if (State == EState::Wait && StateTime < 1.f) return false;
    if (!Chuck->CanBeginRide()) return false;
    const FVector Feet = Chuck->GetActorLocation() - FVector(0, 0, Chuck->GetSimpleCollisionHalfHeight());
    return FVector::Dist2D(Grip, Feet) < Reach && FMath::Abs(Grip.Z - (Feet.Z + PawsUp)) < 70.f;
}

bool AFeyJelly::Hold(AChuckCharacter* Chuck)
{
    if (!CanHold(Chuck)) return false;
    Rider = Chuck;
    FacingYaw = static_cast<float>(Chuck->GetActorRotation().Yaw);
    if (!Chuck->BeginJellyHang(this)) { Rider.Reset(); return false; }
    Vel = FVector::ZeroVector; Swing = SwingRate = FVector2D::ZeroVector;
    BuildStrands();   // (its arms laid to his paws)
    SetState(EState::Rise);
    ++Holds;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_HOLD jelly=%d at=%s"), Index, *FeyLocal(GetActorLocation()).ToCompactString());
    return true;
}

void AFeyJelly::Release(bool bShaken)
{
    if (AChuckCharacter* C = Rider.Get()) C->EndJellyHang(bShaken);
    Rider.Reset();
    SetState(EState::Wait);
    ++Releases; if (bShaken) ++Shakes;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_RELEASE jelly=%d shaken=%d at=%s"), Index, bShaken ? 1 : 0, *FeyLocal(GetActorLocation()).ToCompactString());
}

// ------------------------------------------------------------------ behaviour
void AFeyJelly::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Dt = FMath::Min(DeltaSeconds, .05f);
    Clock += Dt; StateTime += Dt;
    if (Rider.IsValid() && Rider->GetJellyMount() != this) { Rider.Reset(); if (State == EState::Rise || State == EState::Held) SetState(EState::Wait); }
    FVector Want = FVector::ZeroVector;
    if (State == EState::Rise || State == EState::Held) Ride(Dt, Want); else Think(Dt, Want);
    Lift = FMath::FInterpTo(Lift, Rider.IsValid() ? 1.f : 0.f, Dt, Rider.IsValid() ? 1.6f : .8f);
    if (State == EState::Rise && Lift > .95f) SetState(EState::Held);
    const bool bHeld = Rider.IsValid();
    Move(Dt, Want, bHeld ? Speed : DriftSpeed, bHeld ? 650.f : 120.f);
    if (bHeld && GetActorLocation() != FVector::ZeroVector) TopSpeed = FMath::Max(TopSpeed, static_cast<float>(Vel.Size2D()));
    UpdateSwing(Dt);
    // The pulse: quicker the faster it goes.
    const float Before = Phase;
    Phase = FMath::Frac(Phase + Dt * (.6f + .9f * GetSpeedFraction()));
    if (Phase < Before && Pulses.Num())
        if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
            if (FVector::Dist(P->GetActorLocation(), GetActorLocation()) < 2500.f)
                UGameplayStatics::PlaySoundAtLocation(this, Pulses[Rand.RandRange(0, Pulses.Num() - 1)], GetActorLocation(), .32f + .2f * GetSpeedFraction(), Rand.FRandRange(.92f, 1.08f), 0.f, Attenuation);
    const float C = Contraction(Phase);
    if (Mat) Mat->SetScalarParameterValue(TEXT("Glow"), .85f + .45f * C);
    Light->SetIntensity(11.f + 6.f * C);
    // Drawn only where it can be seen to move (60 m).
    const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
    const bool bNear = !Cam || FVector::Dist(Cam->GetCameraLocation(), GetActorLocation()) < 6000.f;
    if (bNear || !bMeshBuilt)
    {
        SimStrands(Dt);
        DrawBell(!bMeshBuilt); DrawStrands(!bMeshBuilt);
        bMeshBuilt = true;
    }
}

void AFeyJelly::Think(float Dt, FVector& Want)
{
    const FVector At = GetActorLocation();
    if (State == EState::Wait)
    {
        if (StateTime < 3.f) return;
        SetState(EState::Drift); NextGoal = 0;
    }
    if (Clock > NextGoal || FVector::Dist2D(At, Goal) < 60.f)
    {
        FVector G;
        if (FVector::Dist2D(At, Home) > Leash) Goal = FVector(Home.X, Home.Y, At.Z);
        else if (PickGoal(G)) Goal = G;
        NextGoal = Clock + Rand.FRandRange(6.f, 12.f);
    }
    const FVector To = (Goal - At) * FVector(1, 1, 0);
    const float D = static_cast<float>(To.Size());
    Want = D > 1.f ? To / D * FMath::Clamp(D / 120.f, 0.f, 1.f) : FVector::ZeroVector;
    if (!Want.IsNearlyZero()) RefuseDir(At, Want);
    if (Vel.Size2D() > 15.f) FacingYaw = FMath::FInterpTo(FacingYaw, FacingYaw + FMath::FindDeltaAngleDegrees(FacingYaw, static_cast<float>(Vel.Rotation().Yaw)), Dt, 1.f);
}

void AFeyJelly::Ride(float Dt, FVector& Want)
{
    AChuckCharacter* Chuck = Rider.Get();
    if (!Chuck) { SetState(EState::Wait); return; }
    const FVector At = GetActorLocation();
    if (State == EState::Held && InShakeZone(At)) { Release(true); return; }
    if (State == EState::Rise && StateTime < .45f) return;   // (he takes hold before it moves)
    FVector Stick = bTestStick ? TestStick : Chuck->GetRideStick();
    Stick.Z = 0.f;
    const float Push = FMath::Min(1.f, static_cast<float>(Stick.Size()));
    Want = Push > .1f ? Stick.GetSafeNormal() * Push : FVector::ZeroVector;
    if (!Want.IsNearlyZero())
    {
        const bool bNow = RefuseDir(At, Want);
        if (bNow && !bRefusing) { ++Refusals; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_REFUSE jelly=%d at=%s"), Index, *FeyLocal(At).ToCompactString()); }
        bRefusing = bNow;
    }
    else bRefusing = false;
    // He faces the way it goes.
    if (Vel.Size2D() > 40.f) FacingYaw = FMath::FInterpTo(FacingYaw, FacingYaw + FMath::FindDeltaAngleDegrees(FacingYaw, static_cast<float>(Vel.Rotation().Yaw)), Dt, 4.f);
}

void AFeyJelly::Move(float Dt, const FVector& Want, float MaxSpeed, float AccelRate)
{
    FVector P = GetActorLocation();
    const bool bHeld = Rider.IsValid();
    // Toward the wanted velocity, no faster than its acceleration.
    const FVector Target = FVector(Want.X, Want.Y, 0.f) * MaxSpeed;
    FVector Dv = Target - FVector(Vel.X, Vel.Y, 0.f);
    const float Rate = (Target.SizeSquared() < Vel.SizeSquared2D() ? 1.2f : 1.f) * AccelRate * Dt;
    if (Dv.Size() > Rate) Dv = Dv.GetSafeNormal() * Rate;
    FVector V2 = FVector(Vel.X, Vel.Y, 0.f) + Dv;
    // Too low a way ahead (a root arch, a tunnel's roof): it will not go under it.
    if (!V2.IsNearlyZero())
    {
        const FVector Dir = V2.GetSafeNormal2D();
        float Ga, Ta;
        const FVector Ahead = P + Dir * (Radius + 70.f);
        const float Need = (bHeld ? PawsUp + MinFeet : IdleGripUp * .6f) + ArmDrop + BellH;
        if (Heights(Ahead, Ga, Ta) && Ta < Ga + Need)
        {
            const float Into = static_cast<float>(FVector::DotProduct(V2, Dir));
            if (Into > 0.f) { V2 -= Dir * Into; if (bHeld && Into > 60.f) { ++Blocks; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_BLOCKED jelly=%d room=%.0f"), Index, Ta - Ga); } }
        }
    }
    // Across: its body from his feet (or its arms' ends) to the bell's top, as a capsule; trunks and stones turn it aside.
    const float Bottom = (bHeld ? Grip.Z - PawsUp + 28.f : P.Z - ArmDrop + 10.f), Top = P.Z + BellH;
    const float Half = FMath::Max(Radius, (Top - Bottom) * .5f);
    const FVector CentreOff(0, 0, (Bottom + Top) * .5f - P.Z);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(FeyJellyMove), false, this);
    if (bHeld) Q.AddIgnoredActor(Rider.Get());
    FVector Delta = V2 * Dt;
    for (int32 Iter = 0; Iter < 3 && !Delta.IsNearlyZero(); ++Iter)
    {
        FHitResult H;
        const FVector From = P + CentreOff;
        if (!GetWorld()->SweepSingleByChannel(H, From, From + Delta, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, Half), Q)) { P += Delta; break; }
        if (H.bStartPenetrating)
        {
            // Already into something: out along its normal, and on if moving away from it.
            P += FVector(H.Normal.X, H.Normal.Y, 0.f).GetSafeNormal() * (H.PenetrationDepth + .5f);
            if (FVector::DotProduct(Delta, H.Normal) > 0.f) P += Delta;
            break;
        }
        P += Delta * H.Time + FVector(H.Normal.X, H.Normal.Y, 0.f) * .3f;
        const FVector N = FVector(H.Normal.X, H.Normal.Y, 0.f).GetSafeNormal();
        const float Left = static_cast<float>(Delta.Size()) * (1.f - H.Time);
        Delta = Delta * (1.f - H.Time);
        Delta -= N * FMath::Min(0.f, static_cast<float>(FVector::DotProduct(Delta, N)));
        V2 -= N * FMath::Min(0.f, static_cast<float>(FVector::DotProduct(V2, N)));
        // Straight at it: round it, as he would lean it (the side its way already leans, else its right).
        if (Delta.Size() < .4f * Left && Left > .01f)
        {
            FVector Tan(-N.Y, N.X, 0.f);
            const FVector Way = FVector(Want.X, Want.Y, 0.f);
            if (FVector::DotProduct(Tan, Way) < -.02f || (FMath::Abs(FVector::DotProduct(Tan, Way)) <= .02f && Index % 2)) Tan = -Tan;
            Delta += Tan * Left * .8f;
            V2 += Tan * static_cast<float>(FVector::DotProduct(FVector(Want.X, Want.Y, 0.f), -N)) * MaxSpeed * .8f * FMath::Min(1.f, Dt * 8.f);
        }
    }
    // Up and down: following the ground at its height, eased, under whatever is overhead.
    float G = GroundZ, MaxTop = 1e9f;
    if (Heights(P, G, MaxTop)) GroundZ = G;
    const float GripUp = FMath::Lerp(IdleGripUp, RideGripUp, Lift);
    float Want_Z = GroundZ + GripUp + ArmDrop;
    const float Floor = GroundZ + (bHeld ? PawsUp + MinFeet : IdleGripUp * .6f) + ArmDrop;
    if (MaxTop - BellH < Want_Z)
    {
        if (bHeld && Want_Z - (MaxTop - BellH) > 25.f && P.Z > MaxTop - BellH + 20.f) { ++Dips; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_JELLY_DIP jelly=%d room=%.0f"), Index, MaxTop - GroundZ); }
        Want_Z = FMath::Max(Floor, MaxTop - BellH);
    }
    const float Bob = 2.5f * FMath::Sin(Clock * 1.3f + Wobble) * (1.f - Lift * .6f);
    const float Old = static_cast<float>(P.Z);
    P.Z = FMath::FInterpTo(P.Z, Want_Z + Bob, Dt, bHeld ? 2.2f : 1.2f);
    P.Z = FMath::Max(P.Z, Floor);   // (his feet never touch)
    Accel = FMath::Lerp(Accel, (V2 - FVector(Vel.X, Vel.Y, 0.f)) / FMath::Max(Dt, 1e-3f), FMath::Min(1.f, Dt * 12.f));
    Vel = FVector(V2.X, V2.Y, (P.Z - Old) / FMath::Max(Dt, 1e-3f));
    SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
    Grip = P + SwingQ.RotateVector(FVector(0, 0, -ArmDrop));
    FeetClear = static_cast<float>(Grip.Z - PawsUp - GroundZ);
}

void AFeyJelly::UpdateSwing(float Dt)
{
    // He and the arms hang as one pendulum from under the bell; its acceleration swings him (behind as it sets off, ahead
    // as it stops, out on a turn). Unridden there is nothing on the arms: the strands swing on their own.
    if (!Rider.IsValid()) { Swing = FMath::Vector2DInterpTo(Swing, FVector2D::ZeroVector, Dt, 2.f); SwingRate = FVector2D::ZeroVector; }
    else
    {
        const FVector2D A(Accel.X, Accel.Y);
        for (int32 Sub = 0; Sub < 2; ++Sub)
        {
            const float H = Dt * .5f;
            const FVector2D Acc = -(Gravity / SwingLen) * FVector2D(FMath::Sin(Swing.X), FMath::Sin(Swing.Y)) - A / SwingLen - SwingDamp * SwingRate;
            SwingRate += Acc * H; Swing += SwingRate * H;
        }
        const float M = static_cast<float>(Swing.Size());
        if (M > .85f) { Swing *= .85f / M; SwingRate *= .5f; }
    }
    const float Ang = static_cast<float>(Swing.Size());
    if (Ang < 1e-4f) SwingQ = FQuat::Identity;
    else
    {
        const FVector D(Swing.X / Ang, Swing.Y / Ang, 0.f);
        SwingQ = FQuat(FVector::CrossProduct(FVector(0, 0, -1), D).GetSafeNormal(), Ang);
    }
    Grip = GetActorLocation() + SwingQ.RotateVector(FVector(0, 0, -ArmDrop));
}

// ------------------------------------------------------------------ the strands
void AFeyJelly::BuildStrands()
{
    Strands.Reset();
    const FVector P = GetActorLocation();
    const FQuat Yaw = FRotator(0, FacingYaw, 0).Quaternion();
    // Four frilled oral arms from under the bell's middle (the front two are what he holds), twelve fine tendrils from
    // the rim.
    for (int32 I = 0; I < 4; ++I)
    {
        FStrand S; S.bArm = true; S.R0 = 1.5f; S.R1 = .5f; S.Seg = 125.f / (StrandPts - 1);
        S.RootLocal = FVector(I < 2 ? 3.f : -8.f, (I % 2 ? -1.f : 1.f) * 8.f, 3.f);
        S.Held = I < 2 && Rider.IsValid() ? 7 : -1;
        S.Color = FColor(175, 160, 255, 225);
        Strands.Add(S);
    }
    for (int32 I = 0; I < 12; ++I)
    {
        FStrand S; S.R0 = .55f; S.R1 = .22f; S.Seg = Rand.FRandRange(175.f, 240.f) / (StrandPts - 1);
        const float A = 2.f * PI * (I + .5f) / 12.f;
        S.RootLocal = FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * BellR * .94f + FVector(0, 0, -3.f);
        S.Color = FColor(130, 230, 255, 205);
        Strands.Add(S);
    }
    for (FStrand& S : Strands)
    {
        const FVector R = P + Yaw.RotateVector(S.RootLocal);
        S.X.SetNum(StrandPts); S.Prev.SetNum(StrandPts);
        for (int32 K = 0; K < StrandPts; ++K) S.X[K] = S.Prev[K] = R - FVector(0, 0, S.Seg * K);
    }
}

void AFeyJelly::SimStrands(float Dt)
{
    if (Strands.Num() == 0) BuildStrands();
    const FVector P = GetActorLocation();
    const float C = Contraction(Phase);
    const FQuat Yaw = FRotator(0, FacingYaw, 0).Quaternion();
    // The bell leans a little into its way at speed.
    const FVector V2 = FVector(Vel.X, Vel.Y, 0.f);
    const FQuat Lean = V2.IsNearlyZero() ? FQuat::Identity : FQuat(FVector::CrossProduct(FVector::UpVector, V2.GetSafeNormal()).GetSafeNormal(), FMath::DegreesToRadians(9.f * GetSpeedFraction()));
    const FVector Right = SwingQ.RotateVector(Yaw.RotateVector(FVector(0, 1, 0)));
    const float H2 = Dt * Dt;
    for (int32 Si = 0; Si < Strands.Num(); ++Si)
    {
        FStrand& S = Strands[Si];
        S.Held = S.bArm && Si < 2 && Rider.IsValid() ? 7 : -1;
        FVector Rl = S.RootLocal; if (!S.bArm) Rl *= FVector(1.f - .18f * C, 1.f - .18f * C, 1.f);
        S.X[0] = P + Lean.RotateVector(Yaw.RotateVector(Rl));
        for (int32 K = 1; K < StrandPts; ++K)
        {
            const FVector X = S.X[K];
            const FVector Drift(FMath::Sin(Clock * .7f + K * .5f + Si) * 6.f, FMath::Cos(Clock * .6f + K * .4f + Si * 1.7f) * 6.f, 0.f);   // (a slow sway, as if in water)
            S.X[K] += (X - S.Prev[K]) * .86f + (FVector(0, 0, -2400.f) + Drift) * H2;   // (heavy and damped, as in water: at speed they trail back and down, about 35 degrees, rather than stream)
            S.Prev[K] = X;
        }
        if (S.Held >= 0) S.X[S.Held] = Grip + Right * (Si == 0 ? 3.4f : -3.4f);
        for (int32 It = 0; It < 5; ++It)
            for (int32 K = 1; K < StrandPts; ++K)
            {
                FVector D = S.X[K] - S.X[K - 1]; const float L = static_cast<float>(D.Size()); if (L < 1e-3f) continue;
                const FVector Fix = D * ((L - S.Seg) / L);
                const bool bA = K - 1 == 0 || K - 1 == S.Held, bB = K == S.Held;
                if (bA && bB) continue;
                if (bA) S.X[K] -= Fix; else if (bB) S.X[K - 1] += Fix; else { S.X[K] -= Fix * .5f; S.X[K - 1] += Fix * .5f; }
            }
        for (int32 K = 1; K < StrandPts; ++K) S.X[K].Z = FMath::Max(S.X[K].Z, GroundZ + 1.5f);
    }
}

void AFeyJelly::DrawBell(bool bCreate)
{
    const float C = Contraction(Phase);
    const FVector P = GetActorLocation();
    const FVector V2 = FVector(Vel.X, Vel.Y, 0.f);
    const FQuat Lean = V2.IsNearlyZero() ? FQuat::Identity : FQuat(FVector::CrossProduct(FVector::UpVector, V2.GetSafeNormal()).GetSafeNormal(), FMath::DegreesToRadians(9.f * GetSpeedFraction()));
    const FQuat Yaw = FRotator(0, FacingYaw, 0).Quaternion() * Lean.Inverse();   // (the lean is applied in world space below)
    TArray<FVector> V, N; TArray<FColor> Col; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan;
    V.Reserve(NT * (NS + 1)); N.Reserve(NT * (NS + 1));
    auto T = [](int32 Ti) { return Ti < NT - 2 ? static_cast<float>(Ti) / (NT - 3) : 1.f + .07f * (Ti - (NT - 3)); };
    for (int32 Ti = 0; Ti < NT; ++Ti)
        for (int32 Ai = 0; Ai <= NS; ++Ai)
        {
            const float A = 2.f * PI * Ai / NS, Tt = T(Ti);
            const FVector L = BellAt(Tt, A, C);
            const FVector Dt = BellAt(Tt + .01f, A, C) - BellAt(FMath::Max(0.f, Tt - .01f), A, C), Da = BellAt(Tt, A + .01f, C) - BellAt(Tt, A - .01f, C);
            FVector Nl = Ti == 0 ? FVector::UpVector : FVector::CrossProduct(Da, Dt).GetSafeNormal();
            if (FVector::DotProduct(Nl, FVector(L.X, L.Y, L.Z + 20.f)) < 0.f) Nl = -Nl;
            V.Add(P + Lean.RotateVector(Yaw.RotateVector(L) * 1.f)); N.Add(Lean.RotateVector(Yaw.RotateVector(Nl)));
            if (bCreate)
            {
                // Pale teal, violet toward the rim; four faint horseshoe marks seen through the dome.
                const float Rim = Smooth(.8f, 1.12f, Tt), Mark = Smooth(.55f, .85f, FMath::Cos(4.f * A)) * Smooth(.28f, .38f, Tt) * (1.f - Smooth(.52f, .62f, Tt));
                const FLinearColor Lc = FMath::Lerp(FMath::Lerp(FLinearColor(.38f, .92f, 1.f), FLinearColor(.62f, .52f, 1.f), Rim), FLinearColor(.85f, .7f, 1.f), Mark);
                const float Alpha = FMath::Clamp(.04f + .5f * Rim * Rim + .45f * Mark, 0.f, 1.f);
                Col.Add(FColor(FMath::RoundToInt(Lc.R * 255), FMath::RoundToInt(Lc.G * 255), FMath::RoundToInt(Lc.B * 255), FMath::RoundToInt(Alpha * 255)));
                UV.Add(FVector2D(static_cast<float>(Ai) / NS, Tt));
            }
        }
    if (bCreate)
    {
        TArray<int32> Tri;
        for (int32 Ti = 0; Ti < NT - 1; ++Ti)
            for (int32 Ai = 0; Ai < NS; ++Ai)
            {
                const int32 A0 = Ti * (NS + 1) + Ai, A1 = A0 + 1, B0 = A0 + NS + 1, B1 = B0 + 1;
                Tri.Append({A0, B0, A1, A1, B0, B1});
            }
        Mesh->CreateMeshSection(0, V, Tri, N, UV, Col, Tan, false);
        BellColors = Col;
    }
    else Mesh->UpdateMeshSection(0, V, N, TArray<FVector2D>(), BellColors, Tan);
}

void AFeyJelly::DrawStrands(bool bCreate)
{
    TArray<FVector> V, N; TArray<FColor> Col; TArray<int32> Tri; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan;
    for (const FStrand& S : Strands)
    {
        const int32 Sides = S.bArm ? 6 : 4, Base = V.Num();
        for (int32 K = 0; K < StrandPts; ++K)
        {
            const FVector Tg = (S.X[FMath::Min(K + 1, StrandPts - 1)] - S.X[FMath::Max(K - 1, 0)]).GetSafeNormal();
            FVector Ax = FVector::CrossProduct(Tg, FVector::UpVector); if (!Ax.Normalize()) Ax = FVector::RightVector;
            const FVector Bx = FVector::CrossProduct(Tg, Ax);
            const float U = static_cast<float>(K) / (StrandPts - 1);
            float R = FMath::Lerp(S.R0, S.R1, U);
            if (S.bArm) R *= 1.f + .3f * FMath::Sin(U * 23.f + Clock * 2.f);   // (frilled)
            for (int32 J = 0; J < Sides; ++J)
            {
                const float A = 2.f * PI * J / Sides;
                const FVector Nn = Ax * FMath::Cos(A) + Bx * FMath::Sin(A);
                V.Add(S.X[K] + Nn * R); N.Add(Nn);
                if (bCreate) { FColor Cc = S.Color; Cc.A = static_cast<uint8>(Cc.A * (1.f - .55f * U)); Col.Add(Cc); UV.Add(FVector2D(U, static_cast<float>(J) / Sides)); }
            }
            if (bCreate && K < StrandPts - 1)
                for (int32 J = 0; J < Sides; ++J)
                {
                    const int32 A0 = Base + K * Sides + J, A1 = Base + K * Sides + (J + 1) % Sides, B0 = A0 + Sides, B1 = A1 + Sides;
                    Tri.Append({A0, B0, A1, A1, B0, B1});
                }
        }
    }
    if (bCreate) { Mesh->CreateMeshSection(1, V, Tri, N, UV, Col, Tan, false); StrandColors = Col; }
    else Mesh->UpdateMeshSection(1, V, N, TArray<FVector2D>(), StrandColors, Tan);
}
