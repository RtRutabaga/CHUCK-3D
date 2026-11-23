// The Feywild's creatures, stage 5 part two (Claude, 2026-11-22). See FeyCreatures.h.
#include "FeyCreatures.h"
#include "FeyMap.h"
#include "FeyLife.h"
#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
    TArray<TWeakObjectPtr<AFeyMite>> GMites;
    TArray<TWeakObjectPtr<AFeyDisplacer>> GBeasts;
    constexpr float Stand = 34.65f;   // his capsule's centre over the ground
    UStaticMesh* Mesh(const TCHAR* Name) { return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Art/Feywild/Creatures/%s.%s"), Name, Name)); }
    USoundBase* Sfx(const TCHAR* Stem, int32 I) { return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Stem, I, Stem, I)); }
    // (2026-11-22) Sets of takes (Tools/build_fey_sfx.py, CC0 recordings): each stem's _00 to _(N-1), one picked at random.
    struct FStem { const TCHAR* Stem; int32 N; };
    void LoadSets(TArray<USoundBase*>& Out, TArray<FIntPoint>& Sets, std::initializer_list<FStem> Stems)
    {
        for (const FStem& S : Stems) { const int32 First = Out.Num(); for (int32 I = 0; I < S.N; ++I) if (USoundBase* B = Sfx(S.Stem, I)) Out.Add(B); Sets.Add(FIntPoint(First, Out.Num() - First)); }
    }
    USoundBase* Pick(const TArray<USoundBase*>& All, const TArray<FIntPoint>& Sets, int32 Which)
    {
        if (!Sets.IsValidIndex(Which) || Sets[Which].Y <= 0) return nullptr;
        return All[Sets[Which].X + FMath::RandRange(0, Sets[Which].Y - 1)];
    }
    USoundAttenuation* Falloff(UObject* Outer, float Far)
    {
        auto* A = NewObject<USoundAttenuation>(Outer);
        A->Attenuation.bAttenuate = true; A->Attenuation.bSpatialize = true;
        A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
        A->Attenuation.AttenuationShapeExtents = FVector(Far * .1f); A->Attenuation.FalloffDistance = Far;
        return A;
    }
    UStaticMeshComponent* Part(AActor* Owner, USceneComponent* Parent, UStaticMesh* M, const FName& Name)
    {
        auto* C = NewObject<UStaticMeshComponent>(Owner, Name);
        C->SetupAttachment(Parent); C->SetStaticMesh(M); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCanEverAffectNavigation(false); C->bCastDynamicShadow = true; C->RegisterComponent();
        return C;
    }
    float GroundLocal(float X, float Y, float Fallback)
    {
        float Z = Fallback; return FeyMap::GroundAt(X, Y, Z) ? Z : Fallback;
    }
    AChuckCharacter* ChuckOf(const UObject* W) { return Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(W, 0)); }
    // Is the way from A to B (local, at height H over the ground) clear of anything solid but him and the creatures?
    bool Clear(const UWorld* World, const AActor* Self, const FVector& A, const FVector& B, float Radius)
    {
        FCollisionQueryParams Q(SCENE_QUERY_STAT(FeyCreatureSweep), false, Self);
        if (AChuckCharacter* C = ChuckOf(World)) Q.AddIgnoredActor(C);
        for (const TWeakObjectPtr<AFeyMite>& M : GMites) if (M.IsValid()) Q.AddIgnoredActor(M.Get());
        for (const TWeakObjectPtr<AFeyDisplacer>& D : GBeasts) if (D.IsValid()) Q.AddIgnoredActor(D.Get());
        FHitResult Hit;
        return !World->SweepSingleByChannel(Hit, FeyWorld(A), FeyWorld(B), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Q);
    }
}

// ================================================================== the thorn mite
int32 AFeyMite::Bites = 0;
int32 AFeyMite::Kills = 0;
const TArray<TWeakObjectPtr<AFeyMite>>& AFeyMite::All() { return GMites; }

AFeyMite::AFeyMite()
{
    PrimaryActorTick.bCanEverTick = true;
    Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot")); RootComponent = Pivot;
}

void AFeyMite::Init(const FVector& HomeLocal, float InYaw)
{
    Home = HomeLocal; HomeYaw = InYaw; Rand.Initialize(int32(HomeLocal.X * 3 + HomeLocal.Y * 7));
    GMites.Add(this);
    Body = Part(this, Pivot, Mesh(TEXT("SM_ThornMiteBody")), TEXT("Body"));
    UStaticMesh* LegMesh = Mesh(TEXT("SM_ThornMiteLeg"));
    const FVector Joints[] = {{5, 5, 4}, {0, 5.6f, 4}, {-5, 5, 4}};
    for (int32 S = 0; S < 2; ++S)
        for (const FVector& J : Joints)
        {
            UStaticMeshComponent* L = Part(this, Body, LegMesh, NAME_None);
            L->SetRelativeLocation(FVector(J.X, S ? -J.Y : J.Y, J.Z)); L->SetRelativeScale3D(FVector(1, S ? -1 : 1, 1));
            L->bCastDynamicShadow = false; Legs.Add(L);
        }
    // (2026-11-23, stage 8) Lightened: under the Feywild's canopy they read near black. One instance for the body, one shared by the legs.
    if (Body && Body->GetMaterial(0)) Body->CreateDynamicMaterialInstance(0, Body->GetMaterial(0))->SetVectorParameterValue(TEXT("Tint"), FLinearColor(2.2f, 2.1f, 1.8f));
    if (Legs.Num() && Legs[0]->GetMaterial(0))
    {
        UMaterialInstanceDynamic* LegMid = UMaterialInstanceDynamic::Create(Legs[0]->GetMaterial(0), this);
        LegMid->SetVectorParameterValue(TEXT("Tint"), FLinearColor(2.2f, 2.1f, 1.8f));
        for (UStaticMeshComponent* L : Legs) L->SetMaterial(0, LegMid);
    }
    Attenuation = Falloff(this, 1400.f);
    LoadSets(Sounds, SoundSets, {{TEXT("SFX_FeyMiteChitter"), 3}, {TEXT("SFX_FeyMiteClick"), 1}, {TEXT("SFX_RatBite"), 2}, {TEXT("SFX_FeyMiteDeath"), 1}});
    ResetMite();
}

void AFeyMite::ResetMite()
{
    Pos = FVector(Home.X, Home.Y, GroundLocal(Home.X, Home.Y, Home.Z)); Yaw = HomeYaw; Wander = Pos; DeadTime = 0;
    SetActorHiddenInGame(false); if (Body) Body->SetRelativeScale3D(FVector(1));
    SetState(EState::Roam);
    SetActorLocationAndRotation(FeyWorld(Pos), FRotator(0, Yaw, 0));
}

void AFeyMite::SetState(EState S)
{
    State = S; StateTime = 0; bBit = false;
    if (S == EState::Windup) Sound(1, .8f, 1.f);
    if (S == EState::Dead) Sound(3, .9f, 1.f);
}

void AFeyMite::Sound(int32 Which, float Volume, float Pitch)
{
    if (USoundBase* S = Pick(Sounds, SoundSets, Which)) UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation(), Volume, Pitch * FMath::FRandRange(.94f, 1.06f), 0.f, Attenuation);
}

bool AFeyMite::MoveToward(const FVector& Dir, float Speed, float Dt)
{
    const FVector D = Dir.GetSafeNormal2D(); if (D.IsNearlyZero()) return false;
    // The first clear heading near the one wanted (round stones, roots and trunks).
    for (const float Off : {0.f, 35.f, -35.f, 70.f, -70.f, 110.f, -110.f})
    {
        const FVector Try = D.RotateAngleAxis(Off, FVector::UpVector), Next = Pos + Try * Speed * Dt;
        const float Gz = GroundLocal(Next.X, Next.Y, Pos.Z);
        if (FMath::Abs(Gz - Pos.Z) > 40.f) continue;
        if (!Clear(GetWorld(), this, Pos + FVector(0, 0, 10), FVector(Next.X, Next.Y, Gz + 10), 8.f)) continue;
        Pos = FVector(Next.X, Next.Y, Gz);
        Yaw = FMath::FixedTurn(Yaw, Try.Rotation().Yaw, 540.f * Dt);
        return true;
    }
    return false;
}

void AFeyMite::Tick(float Dt)
{
    Super::Tick(Dt);
    StateTime += Dt;
    if (State == EState::Dead)
    {
        DeadTime += Dt;
        if (Body) Body->SetRelativeScale3D(FVector(FMath::Max(.05f, 1.f - DeadTime * .8f)));
        if (DeadTime > 1.3f) SetActorHiddenInGame(true);
        return;
    }
    AChuckCharacter* Chuck = ChuckOf(this);
    const FVector Him = Chuck ? FeyLocal(Chuck->GetActorLocation()) - FVector(0, 0, Stand) : Pos + FVector(1e6, 0, 0);
    const FVector To = Him - Pos; const float Dist = To.Size2D();
    const bool bCanSee = Chuck && !AFeyLife::bCalm && FMath::Abs(To.Z) < 120.f && !Chuck->IsAstral();
    if (Dist > 6000.f && State == EState::Roam) return;   // (far off: still)
    float Speed = 0;
    switch (State)
    {
    case EState::Roam:
        if (bCanSee && Dist < NoticeRange) { SetState(EState::Chase); ChitterAt = 0; break; }
        if (FVector::Dist2D(Pos, Wander) < 15.f || StateTime > 6.f) { const float A = Rand.FRandRange(0.f, 2 * PI), R = Rand.FRandRange(40.f, 220.f); Wander = Home + FVector(R * FMath::Cos(A), R * FMath::Sin(A), 0); StateTime = 0; }
        if (StateTime > 1.2f && MoveToward(Wander - Pos, RoamSpeed, Dt)) Speed = RoamSpeed;
        break;
    case EState::Chase:
        if (!bCanSee || Dist > LoseRange || FVector::Dist2D(Pos, Home) > 2200.f) { Wander = Home; SetState(EState::Roam); break; }
        if (Dist < StrikeRange) { SetState(EState::Windup); break; }
        if (MoveToward(To, ChaseSpeed, Dt)) Speed = ChaseSpeed;
        if ((ChitterAt -= Dt) <= 0) { Sound(0, .6f, 1.f); ChitterAt = Rand.FRandRange(.6f, 1.2f); }
        break;
    case EState::Windup:
        Yaw = FMath::FixedTurn(Yaw, To.Rotation().Yaw, 720.f * Dt);
        if (StateTime > WindupTime) { LungeDir = To.GetSafeNormal2D(); SetState(EState::Lunge); }
        break;
    case EState::Lunge:
        if (MoveToward(LungeDir, LungeSpeed, Dt)) Speed = LungeSpeed;
        if (!bBit && Chuck && Dist < BiteRange && FMath::Abs(To.Z) < 60.f)
        {
            bBit = true;
            if (Chuck->TakeBite(GetActorLocation(), 1)) { ++Bites; Sound(2, .8f, 1.45f); }
        }
        if (StateTime > LungeTime) SetState(EState::Recover);
        break;
    case EState::Recover:
        if (StateTime < .3f && MoveToward(-LungeDir, 90.f, Dt)) Speed = 90.f;
        if (StateTime > RecoverTime) SetState(bCanSee && Dist < LoseRange ? EState::Chase : EState::Roam);
        break;
    default: break;
    }
    Gait += Speed * Dt / 9.f;
    SetActorLocationAndRotation(FeyWorld(Pos), FRotator(0, Yaw, 0));
    Pose(Dt);
}

void AFeyMite::Pose(float Dt)
{
    if (!Body) return;
    const float Crouch = State == EState::Windup ? FMath::Min(StateTime / .15f, 1.f) : 0.f;
    const float Bob = FMath::Abs(FMath::Sin(Gait * PI)) * 1.2f * (Gait > 0.f);
    Body->SetRelativeLocation(FVector(0, 0, 1.f + Bob - 2.5f * Crouch));
    Body->SetRelativeRotation(FRotator(State == EState::Lunge ? -10.f : 8.f * Crouch, 0, FMath::Sin(Gait * PI) * 3.f));
    for (int32 I = 0; I < Legs.Num(); ++I)
    {
        const int32 Side = I / 3, K = I % 3; const float Ph = Gait * PI + ((K + Side) % 2) * PI;   // tripods
        const float Swing = FMath::Sin(Ph) * 22.f, Lift = FMath::Max(0.f, FMath::Cos(Ph)) * 14.f;
        const float Spread = (K - 1) * -28.f;   // front legs forward, back legs back
        Legs[I]->SetRelativeRotation(FRotator(0, (Side ? -1.f : 1.f) * (Spread + Swing), (Side ? 1.f : -1.f) * (Lift - 6.f * Crouch)));
    }
}

bool AFeyMite::TakeScratch(const FVector& Center, float Radius)
{
    if (State == EState::Dead) return false;
    const FVector W = GetActorLocation();
    if (FVector::Dist2D(W, Center) > Radius + 18.f || FMath::Abs(W.Z + 6.f - Center.Z) > 70.f) return false;
    ++Kills; SetState(EState::Dead); return true;
}

void AFeyMite::EndPlay(const EEndPlayReason::Type Reason)
{
    GMites.RemoveAll([this](const TWeakObjectPtr<AFeyMite>& P) { return !P.IsValid() || P.Get() == this; });
    Super::EndPlay(Reason);
}

// ================================================================== the displacer beast
int32 AFeyDisplacer::Lashes = 0;
int32 AFeyDisplacer::LashHits = 0;
int32 AFeyDisplacer::Notices = 0;
int32 AFeyDisplacer::Paces = 0;
const TArray<TWeakObjectPtr<AFeyDisplacer>>& AFeyDisplacer::All() { return GBeasts; }

namespace
{
    constexpr float Upper = 62.f, Lower = 64.f, Seg = 26.f;
    constexpr int32 TentacleSegs = 7;
    const FVector HeadAt(128, 0, 34), TailAt(-112, 0, 8), TentacleAt[2] = {{66, 20, 24}, {66, -20, 24}};
}

AFeyDisplacer::AFeyDisplacer()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

const TCHAR* AFeyDisplacer::GetStateName() const
{
    static const TCHAR* N[] = {TEXT("Prowl"), TEXT("Notice"), TEXT("Chase"), TEXT("Lash"), TEXT("Recover"), TEXT("Pace"), TEXT("Return"), TEXT("Flinch"), TEXT("Driven")};
    return N[int32(State)];
}

void AFeyDisplacer::Init(const TArray<FVector>& InRound, const TArray<FFeyRefuge>& InRefuges)
{
    Round = InRound; Refuges = InRefuges; Rand.Initialize(4711);
    GBeasts.Add(this);
    Image = NewObject<USceneComponent>(this, TEXT("Image")); Image->SetupAttachment(RootComponent); Image->RegisterComponent();
    Body = Part(this, Image, Mesh(TEXT("SM_DisplacerBody")), TEXT("Body"));
    Head = Part(this, Body, Mesh(TEXT("SM_DisplacerHead")), TEXT("Head")); Head->SetRelativeLocation(HeadAt);
    Tail = Part(this, Body, Mesh(TEXT("SM_DisplacerTail")), TEXT("Tail")); Tail->SetRelativeLocation(TailAt);
    UStaticMesh* U = Mesh(TEXT("SM_DisplacerUpper")); UStaticMesh* L = Mesh(TEXT("SM_DisplacerLower"));
    UStaticMesh* T = Mesh(TEXT("SM_DisplacerTentacle")); UStaticMesh* Pd = Mesh(TEXT("SM_DisplacerPad"));
    const FVector Hips[] = {{78, 30, -6}, {6, 31, -8}, {-74, 29, -2}, {78, -30, -6}, {6, -31, -8}, {-74, -29, -2}};   // FL ML HL FR MR HR
    for (int32 I = 0; I < 6; ++I)
    {
        FLeg G; G.HipLocal = Hips[I]; G.bFront = I % 3 != 2;
        G.Upper = Part(this, RootComponent, U, NAME_None); G.Lower = Part(this, RootComponent, L, NAME_None);
        G.Upper->SetUsingAbsoluteLocation(true); G.Upper->SetUsingAbsoluteRotation(true); G.Lower->SetUsingAbsoluteLocation(true); G.Lower->SetUsingAbsoluteRotation(true);
        Parts.Add(G.Upper); Parts.Add(G.Lower); LegsState.Add(G);
    }
    for (int32 S = 0; S < 2; ++S)
        for (int32 I = 0; I <= TentacleSegs; ++I)
        {
            UStaticMeshComponent* C = Part(this, RootComponent, I < TentacleSegs ? T : Pd, NAME_None);
            C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->bCastDynamicShadow = I % 2 == 0;
            Tentacles[S].Add(C); Parts.Add(C);
        }
    Attenuation = Falloff(this, 5000.f);
    LoadSets(Sounds, SoundSets, {{TEXT("SFX_FeyBeastStep"), 3}, {TEXT("SFX_FeyBeastGrowl"), 3}, {TEXT("SFX_FeyBeastRoar"), 2}, {TEXT("SFX_FeyBeastLash"), 2}, {TEXT("SFX_ZombieChuckHit"), 3}});
    // Its fur (M_FeyFur, CC0 ambientCG Fabric084): an instance per part, so its Shimmer can follow the slip of its image.
    TArray<UStaticMeshComponent*> All = Parts; All.Append({Body, Head, Tail});
    for (UStaticMeshComponent* C : All) if (C && C->GetMaterial(0)) FurMids.Add(C->CreateDynamicMaterialInstance(0, C->GetMaterial(0)));
    ResetBeast();
}

void AFeyDisplacer::ResetBeast()
{
    Pos = Round.Num() ? Round[0] : FVector::ZeroVector; Pos.Z = GroundZ(Pos.X, Pos.Y);
    Yaw = Round.Num() > 1 ? (Round[1] - Round[0]).Rotation().Yaw : 0.f; RoundAt = Round.Num() > 1 ? 1 : 0;
    Vel = FVector::ZeroVector; Hits = 0; Lost = 0; Lash = 0; Clock = 0; BodyZ = Pos.Z + HipHeight;
    SetActorLocationAndRotation(FeyWorld(Pos), FRotator(0, Yaw, 0));
    const FTransform Xf(FRotator(0, Yaw, 0), FeyWorld(GetImageLocal()));
    for (FLeg& G : LegsState) { const FVector Hl = FeyLocal(Xf.TransformPosition(G.HipLocal)); G.Foot = G.From = G.To = FeyWorld(FVector(Hl.X, Hl.Y, GroundZ(Hl.X, Hl.Y))); G.Lift = 1.f; }
    SetState(EState::Prowl);
    Pose(0.f);
}

float AFeyDisplacer::GroundZ(float X, float Y) const { return GroundLocal(X, Y, Pos.Z); }

bool AFeyDisplacer::InRefuge(const FVector& L, float Margin) const
{
    for (const FFeyRefuge& R : Refuges) if (FVector2D::Distance(FVector2D(L), R.C) < R.R + Margin) return true;
    return false;
}

FVector AFeyDisplacer::GetImageLocal() const
{
    // Never quite where it seems: the image sways 60 to 130 cm to one side, and now and then slips across to the other
    // (2026-11-22: over about half a second, shimmering, passing for a moment over where it truly is).
    const float Side = FMath::Clamp(FMath::Sin(Clock * .45f) * 5.f, -1.f, 1.f), Amount = 95.f + 35.f * FMath::Sin(Clock * 1.3f);
    const FVector Right = FRotator(0, Yaw, 0).RotateVector(FVector::RightVector);
    return Pos + Right * Side * Amount;
}

void AFeyDisplacer::SetState(EState S)
{
    const EState From = State; State = S; StateTime = 0;
    if (S == EState::Notice) { ++Notices; Sound(2, 1.f, 1.f); }
    if (S == EState::Lash) { ++Lashes; bLashLanded = false; }
    if (S == EState::Pace && From != EState::Pace) { ++Paces; Sound(1, .9f, 1.f); }
    if (S == EState::Flinch) Sound(1, .8f, 1.25f);
}

void AFeyDisplacer::Sound(int32 Which, float Volume, float Pitch)
{
    if (USoundBase* S = Pick(Sounds, SoundSets, Which)) UGameplayStatics::PlaySoundAtLocation(this, S, FeyWorld(GetImageLocal()) + FVector(0, 0, 100), Volume, Pitch * FMath::FRandRange(.95f, 1.05f), 0.f, Attenuation);
}

void AFeyDisplacer::Walk(const FVector& Goal, float Speed, float TurnRate, float Dt)
{
    FVector Want = (Goal - Pos).GetSafeNormal2D();
    if (Want.IsNearlyZero() || FVector::Dist2D(Goal, Pos) < 30.f) { Vel = FMath::VInterpTo(Vel, FVector::ZeroVector, Dt, 4.f); Pos += Vel * Dt; return; }
    FVector Best = FVector::ZeroVector;
    for (const float Off : {0.f, 30.f, -30.f, 60.f, -60.f, 95.f, -95.f, 130.f, -130.f})
    {
        const FVector Try = Want.RotateAngleAxis(Off, FVector::UpVector), Next = Pos + Try * 160.f;
        if (InRefuge(Next, 140.f)) continue;   // (it cannot get into a refuge)
        if (!Clear(GetWorld(), this, Pos + FVector(0, 0, 110), Next + FVector(0, 0, 110), 55.f)) continue;
        Best = Try; break;
    }
    if (Best.IsNearlyZero()) { Vel = FMath::VInterpTo(Vel, FVector::ZeroVector, Dt, 6.f); }
    else
    {
        Yaw = FMath::FixedTurn(Yaw, Best.Rotation().Yaw, TurnRate * Dt);
        const FVector Fwd = FRotator(0, Yaw, 0).Vector();
        Vel = FMath::VInterpTo(Vel, Fwd * Speed * FMath::Max(.25f, FVector::DotProduct(Fwd, Best)), Dt, 3.f);
    }
    const FVector Next = Pos + Vel * Dt;
    if (!InRefuge(Next, 120.f)) Pos = FVector(Next.X, Next.Y, GroundZ(Next.X, Next.Y));
}

void AFeyDisplacer::Tick(float Dt)
{
    Super::Tick(Dt);
    Clock += Dt; StateTime += Dt;
    AChuckCharacter* Chuck = ChuckOf(this);
    const FVector Him = Chuck ? FeyLocal(Chuck->GetActorLocation()) - FVector(0, 0, Stand) : Pos + FVector(1e6, 0, 0);
    const FVector To = Him - Pos; const float Dist = To.Size2D();
    const bool bSafe = InRefuge(Him);
    auto Sees = [&]()
    {
        if (!Chuck || AFeyLife::bCalm || Chuck->IsAstral() || FMath::Abs(To.Z) > 400.f) return false;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(DisplacerSight), false, this); Q.AddIgnoredActor(Chuck);
        FHitResult Hit; return !GetWorld()->LineTraceSingleByChannel(Hit, FeyWorld(Pos + FVector(0, 0, 160)), Chuck->GetActorLocation(), ECC_Visibility, Q);
    };
    switch (State)
    {
    case EState::Prowl:
    case EState::Return:
    {
        if (Dist < NoticeRange && !bSafe && Sees()) { SetState(EState::Notice); break; }
        if (State == EState::Return && Round.Num() && FVector::Dist2D(Pos, Round[RoundAt]) < 300.f) SetState(EState::Prowl);
        if (Round.Num() > 1 && FVector::Dist2D(Pos, Round[RoundAt]) < 200.f) RoundAt = (RoundAt + 1) % Round.Num();
        if (Round.Num()) Walk(Round[RoundAt], State == EState::Return ? ProwlSpeed * 1.3f : ProwlSpeed, 70.f, Dt);
        if ((GrowlAt -= Dt) <= 0) { Sound(1, .45f, 1.f); GrowlAt = Rand.FRandRange(7.f, 12.f); }
        break;
    }
    case EState::Notice:   // it stops, faces him and roars
        Vel = FMath::VInterpTo(Vel, FVector::ZeroVector, Dt, 5.f); Pos += Vel * Dt;
        Yaw = FMath::FixedTurn(Yaw, To.Rotation().Yaw, 160.f * Dt);
        if (StateTime > 1.3f) SetState(EState::Chase);
        break;
    case EState::Chase:
    {
        if (bSafe) { SetState(EState::Pace); break; }
        Lost = Sees() ? 0.f : Lost + Dt;
        if (Lost > 5.f || Dist > LoseRange || !Chuck || AFeyLife::bCalm) { SetState(EState::Return); break; }
        if (Dist < LashReach - 30.f) { SetState(EState::Lash); LashAt = Him; break; }
        Walk(Him, ChaseSpeed, 150.f, Dt);
        break;
    }
    case EState::Lash:   // the tentacles draw back (.55 s), then whip at where he was (.25 s)
    {
        Vel = FMath::VInterpTo(Vel, FVector::ZeroVector, Dt, 4.f); Pos += Vel * Dt;
        Yaw = FMath::FixedTurn(Yaw, (LashAt - Pos).Rotation().Yaw, 120.f * Dt);
        if (StateTime < .55f) { Lash = -StateTime / .55f; LashAt = FMath::VInterpTo(LashAt, Him, Dt, 3.f); }
        else Lash = FMath::Min((StateTime - .55f) / .25f, 1.f);
        if (StateTime > .7f && StateTime - Dt <= .7f) Sound(3, .9f, 1.f);   // (the crack as the pads reach him)
        if (!bLashLanded && Lash > .8f && Chuck && !bSafe && FVector::Dist2D(Him, LashAt) < 110.f && FVector::Dist2D(Him, Pos) < LashReach + 60.f && FMath::Abs(To.Z) < 140.f)
        {
            bLashLanded = true;
            if (Chuck->TakeBite(FeyWorld(Pos), 2, true)) { ++LashHits; Sound(4, .9f, 1.f); }
        }
        if (StateTime > 1.f) SetState(EState::Recover);
        break;
    }
    case EState::Recover:
        Lash = FMath::Max(0.f, 1.f - StateTime / .5f);
        if (StateTime > .7f) SetState(bSafe ? EState::Pace : EState::Chase);
        break;
    case EState::Pace:   // outside the refuge, back and forth, growling; then gives up
    {
        Lash = 0;
        if (!bSafe && StateTime > .5f) { SetState(Dist < NoticeRange ? EState::Chase : EState::Return); break; }
        const FVector2D Rc = [&]() { for (const FFeyRefuge& R : Refuges) if (FVector2D::Distance(FVector2D(Him), R.C) < R.R) return R.C; return FVector2D(Him); }();
        const float A = FMath::Atan2(Pos.Y - Rc.Y, Pos.X - Rc.X) + FMath::Sin(StateTime * .5f) * .9f;
        Walk(FVector(Rc.X + 520.f * FMath::Cos(A), Rc.Y + 520.f * FMath::Sin(A), Pos.Z), ProwlSpeed * 1.2f, 110.f, Dt);
        if ((GrowlAt -= Dt) <= 0) { Sound(1, .8f, 1.f); GrowlAt = Rand.FRandRange(3.f, 5.f); }
        if (StateTime > 8.f) SetState(EState::Return);
        break;
    }
    case EState::Flinch:
        Vel = FMath::VInterpTo(Vel, FVector::ZeroVector, Dt, 8.f); Pos += Vel * Dt;
        if (StateTime > .45f) SetState(Hits >= Health ? EState::Driven : EState::Chase);
        break;
    case EState::Driven:   // driven off: back to its round, and it leaves him be till he next returns
        if (Round.Num()) Walk(Round[0], ProwlSpeed, 70.f, Dt);
        break;
    default: break;
    }
    SetActorLocationAndRotation(FeyWorld(Pos), FRotator(0, Yaw, 0));
    Pose(Dt);
}

void AFeyDisplacer::Pose(float Dt)
{
    if (!Body) return;
    const FVector Img = GetImageLocal();
    const FRotator R(0, Yaw, 0); const FVector Fwd = R.Vector(), Right = R.RotateVector(FVector::RightVector);
    // The body rides its hips' height over the ground under it, pitched to the slope, breathing.
    const float Zf = GroundZ(Img.X + Fwd.X * 80.f, Img.Y + Fwd.Y * 80.f), Zh = GroundZ(Img.X - Fwd.X * 80.f, Img.Y - Fwd.Y * 80.f);
    const float Speed = Vel.Size2D();
    const float WantZ = (Zf + Zh) * .5f + HipHeight - (State == EState::Lash && Lash < 0.f ? 14.f * -Lash : 0.f) - (State == EState::Pace ? 10.f : 0.f);
    BodyZ = Dt > 0.f ? FMath::FInterpTo(BodyZ, WantZ, Dt, 6.f) : WantZ;
    BodyPitch = FMath::RadiansToDegrees(FMath::Atan2(Zf - Zh, 160.f));
    Image->SetRelativeLocation(R.UnrotateVector(FVector(Img.X - Pos.X, Img.Y - Pos.Y, BodyZ - Pos.Z + 1.5f * FMath::Sin(Clock * 2.f))));
    Image->SetRelativeRotation(FRotator(-BodyPitch, 0, 0));
    {   // the shimmer: only as the image crosses over
        const float Cross = 1.f - FMath::Abs(FMath::Clamp(FMath::Sin(Clock * .45f) * 5.f, -1.f, 1.f));
        const float Want = Cross * .85f;   // (none between slips: a faint constant flicker read as square patches)
        if (FMath::Abs(Want - Shimmer) > .01f) { Shimmer = Want; for (UMaterialInstanceDynamic* M : FurMids) if (M) M->SetScalarParameterValue(TEXT("Shimmer"), Shimmer); }
    }
    const FTransform BodyXf = Body->GetComponentTransform();
    // Head: on him while it hunts, else looking about.
    {
        float HY = 12.f * FMath::Sin(Clock * .4f), HP = 0.f;
        if (AChuckCharacter* Chuck = ChuckOf(this))
            if (State != EState::Prowl && State != EState::Return && State != EState::Driven)
            {
                const FVector Lh = BodyXf.InverseTransformPosition(Chuck->GetActorLocation()) - HeadAt;
                HY = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Lh.Y, Lh.X)), -45.f, 45.f);
                HP = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Lh.Z, Lh.Size2D())), -35.f, 15.f);
            }
        Head->SetRelativeRotation(FRotator(HP + (State == EState::Notice ? 18.f * FMath::Sin(FMath::Min(StateTime, 1.2f) * PI / 1.2f) : 0.f), HY, 0));
    }
    Tail->SetRelativeRotation(FRotator(6.f * FMath::Sin(Clock * 1.1f) + (State == EState::Pace ? 12.f : 0.f), 22.f * FMath::Sin(Clock * (State == EState::Pace ? 2.4f : .9f)), 0));
    // Legs: two tripods taking turns; a foot steps when it falls too far behind where it would stand.
    const float StepTime = FMath::Clamp(.42f - Speed / 1100.f, .2f, .42f), Reach = 26.f + Speed * .22f;
    StepGroupTime += Dt;
    bool bGroupMoving = false;
    for (int32 I = 0; I < 6; ++I) bGroupMoving |= ((I % 2 == 0) == (StepGroup == 0)) && LegsState[I].Lift < 1.f;
    if (!bGroupMoving)
    {
        bool bNeed = false;
        for (int32 I = 0; I < 6; ++I)
        {
            if ((I % 2 == 0) != (StepGroup == 0)) continue;   // (0, 2, 4 = FL HL MR; 1, 3, 5 = ML FR HR)
            FLeg& G = LegsState[I];
            const FVector H = BodyXf.TransformPosition(G.HipLocal);
            const FVector Want = H + Vel * StepTime * .9f;
            if (FVector::Dist2D(G.Foot, Want) > Reach) bNeed = true;
        }
        if (bNeed)
            for (int32 I = 0; I < 6; ++I)
            {
                if ((I % 2 == 0) != (StepGroup == 0)) continue;
                FLeg& G = LegsState[I];
                const FVector H = BodyXf.TransformPosition(G.HipLocal), Want = H + Vel * StepTime * 1.1f; const FVector Wl = FeyLocal(Want);
                G.From = G.Foot; G.To = FeyWorld(FVector(Wl.X, Wl.Y, GroundZ(Wl.X, Wl.Y))); G.Lift = 0.f;
            }
        else if (StepGroupTime > .05f) { StepGroup ^= 1; StepGroupTime = 0; }
    }
    for (int32 I = 0; I < 6; ++I)
    {
        FLeg& G = LegsState[I];
        if (G.Lift < 1.f)
        {
            G.Lift = FMath::Min(1.f, G.Lift + Dt / StepTime);
            G.Foot = FMath::Lerp(G.From, G.To, G.Lift) + FVector(0, 0, 26.f * FMath::Sin(G.Lift * PI));
            if (G.Lift >= 1.f && I % 3 == 0 && Speed > 60.f) Sound(0, .3f + Speed / 900.f, 1.f);
        }
        // Two-bone reach from the hip to the foot; the knee bends back on the front and middle legs, forward on the hind.
        const FVector H = BodyXf.TransformPosition(G.HipLocal); FVector F = G.Foot;
        FVector D = F - H; float Len = D.Size();
        if (Len > Upper + Lower - 1.f) { D *= (Upper + Lower - 1.f) / Len; Len = Upper + Lower - 1.f; F = H + D; }
        const FVector Dir = D / FMath::Max(Len, 1.f);
        const FVector Hint = (G.bFront ? -Fwd : Fwd);
        FVector Bend = (Hint - Dir * FVector::DotProduct(Hint, Dir)).GetSafeNormal(); if (Bend.IsNearlyZero()) Bend = Fwd;
        const float A = (Upper * Upper - Lower * Lower + Len * Len) / (2.f * Len), Hk = FMath::Sqrt(FMath::Max(0.f, Upper * Upper - A * A));
        const FVector Knee = H + Dir * A + Bend * Hk;
        G.Upper->SetWorldLocationAndRotation(H, FRotationMatrix::MakeFromZX(H - Knee, Fwd).Rotator());
        G.Lower->SetWorldLocationAndRotation(Knee, FRotationMatrix::MakeFromZX(Knee - F, Fwd).Rotator());
    }
    // Tentacles: arched up over the shoulders, rippling; drawn back, then whipped down at him in a lash.
    for (int32 S = 0; S < 2; ++S)
    {
        FVector P = BodyXf.TransformPosition(TentacleAt[S]);
        const float Sd = S ? -1.f : 1.f;
        const FVector Target = FeyWorld(LashAt) + FVector(0, 0, 30.f);
        for (int32 I = 0; I <= TentacleSegs; ++I)
        {
            const float U = float(I) / TentacleSegs;
            const float Pitch = 72.f - U * 150.f + 10.f * FMath::Sin(Clock * 1.6f + I * .7f + S) + (Lash < 0.f ? -Lash * 30.f : 0.f);
            const float YawOff = Sd * (18.f + U * 14.f + 9.f * FMath::Sin(Clock * 1.1f + I * .5f + S * 2.f));
            FVector Dir = FRotator(Pitch, Yaw + YawOff, 0).Vector();
            if (Lash > 0.f) Dir = FMath::Lerp(Dir, (Target - P).GetSafeNormal(), FMath::Clamp(Lash * (.4f + U), 0.f, 1.f)).GetSafeNormal();
            UStaticMeshComponent* C = Tentacles[S][I];
            C->SetWorldLocationAndRotation(P, FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).Rotator());
            P += Dir * (I < TentacleSegs ? Seg : 0.f);
        }
    }
}

bool AFeyDisplacer::TakeScratch(const FVector& Center, float Radius)
{
    if (State == EState::Driven) return false;
    const FVector W = GetActorLocation();
    if (FVector::Dist2D(W, Center) > Radius + 90.f || Center.Z > W.Z + 220.f) return false;
    Hits += 1.f; SetState(EState::Flinch);
    return true;
}

void AFeyDisplacer::EndPlay(const EEndPlayReason::Type Reason)
{
    GBeasts.RemoveAll([this](const TWeakObjectPtr<AFeyDisplacer>& P) { return !P.IsValid() || P.Get() == this; });
    Super::EndPlay(Reason);
}

bool FeyCreaturesScratch(const FVector& World, const FVector& Ahead, float Reach)
{
    const FVector C = World + Ahead.GetSafeNormal2D() * Reach * .6f - FVector(0, 0, Stand * .5f);
    bool bAny = false;
    for (const TWeakObjectPtr<AFeyMite>& M : GMites) if (M.IsValid()) bAny |= M->TakeScratch(C, Reach * .6f);
    for (const TWeakObjectPtr<AFeyDisplacer>& D : GBeasts) if (D.IsValid()) bAny |= D->TakeScratch(C, Reach * .6f);
    return bAny;
}
