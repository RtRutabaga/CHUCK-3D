// The Feywild's great hare (Claude, 2026-11-25). See FeyHare.h.
#include "FeyHare.h"
#include "FeyMap.h"
#include "FeyLife.h"
#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    TArray<TWeakObjectPtr<AFeyHare>> GHares;
    float Smooth(float A, float B, float X) { const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f); return T * T * (3 - 2 * T); }
    FQuat PitchQ(float Deg) { return FRotator(Deg, 0, 0).Quaternion(); }   // + nose up (a hanging limb swings forward)
    FQuat YawQ(float Deg) { return FRotator(0, Deg, 0).Quaternion(); }
    FQuat RollQ(float Deg) { return FRotator(0, 0, Deg).Quaternion(); }
    const TCHAR* BoneNames[] = {TEXT("spine"), TEXT("chest"), TEXT("neck"), TEXT("head"), TEXT("hips"), TEXT("scut"), TEXT("saddle"), TEXT("ear1_l"), TEXT("ear2_l"), TEXT("ear1_r"), TEXT("ear2_r"),
                                TEXT("fupper_l"), TEXT("flower_l"), TEXT("fpaw_l"), TEXT("fupper_r"), TEXT("flower_r"), TEXT("fpaw_r"),
                                TEXT("thigh_l"), TEXT("shin_l"), TEXT("hfoot_l"), TEXT("htoe_l"), TEXT("thigh_r"), TEXT("shin_r"), TEXT("hfoot_r"), TEXT("htoe_r")};
    // The coats across a herd (the plan's recommendation: a mix), multiplied over the painted tawny.
    const FLinearColor Coats[] = {FLinearColor(.86f, .9f, .84f), FLinearColor(1.05f, .97f, .82f), FLinearColor(.96f, .94f, .86f), FLinearColor(1.f, .9f, .78f)};
    enum ESnd { SThump, SBound, SSqueal };
    // The gait (cm/s; per second): a hop-along at HopSpeed, the bound at BoundSpeed.
    constexpr float GaitLo = 60.f, GaitHi = 550.f;
}

int32 AFeyHare::Mounts = 0, AFeyHare::Dismounts = 0, AFeyHare::Throws = 0, AFeyHare::Refusals = 0, AFeyHare::Thumps = 0, AFeyHare::Shies = 0, AFeyHare::Leaps = 0, AFeyHare::Bolts = 0;
float AFeyHare::TopSpeed = 0.f;
FString AFeyHare::LastThrow;
FVector AFeyHare::TestStick = FVector::ZeroVector;
bool AFeyHare::bTestStick = false, AFeyHare::bTestRun = false;
bool AFeyHare::bPoseHold = false, AFeyHare::bPoseAir = false;
float AFeyHare::PoseSpeed = 0.f, AFeyHare::PosePhase = 0.f;
AFeyHare::EState AFeyHare::PoseState = AFeyHare::EState::Graze;

const TArray<TWeakObjectPtr<AFeyHare>>& AFeyHare::All() { GHares.RemoveAll([](const TWeakObjectPtr<AFeyHare>& H) { return !H.IsValid(); }); return GHares; }

AFeyHare::AFeyHare(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.DoNotCreateDefaultSubobject(ACharacter::MeshComponentName))
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(Radius, HalfHeight);
    auto* Move = GetCharacterMovement();
    Move->bRunPhysicsWithNoController = true;
    Move->bOrientRotationToMovement = true;
    Move->RotationRate = FRotator(0, 360, 0);
    Move->MaxWalkSpeed = HopSpeed;
    Move->MaxAcceleration = 950.f;
    Move->BrakingDecelerationWalking = 900.f;
    Move->GroundFriction = 6.f;
    Move->MaxStepHeight = 28.f;
    Move->SetWalkableFloorAngle(48.f);
    Move->JumpZVelocity = 370.f;
    Move->AirControl = .05f;
    bUseControllerRotationYaw = false;
    Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
    Body->SetupAttachment(GetCapsuleComponent());
    Body->SetRelativeLocation(FVector(4.f, 0, -HalfHeight));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetCastShadow(true);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> HareMesh(TEXT("/Game/Art/Feywild/Creatures/Hare/SK_FeyHare.SK_FeyHare"));
    if (HareMesh.Succeeded()) Body->SetSkinnedAssetAndUpdate(HareMesh.Object);
    for (int32 I = 0; I < 4; ++I) { FootGoal[I] = FVector::ZeroVector; HeelLift[I] = 34.f; }
    for (int32& B : Bone) B = INDEX_NONE;
}

void AFeyHare::BeginPlay()
{
    Super::BeginPlay();
    GHares.Add(this);
    Rand.Initialize(static_cast<int32>(GetUniqueID()));
    if (USkeletalMesh* HareMesh = Cast<USkeletalMesh>(Body->GetSkinnedAsset()))
    {
        const FReferenceSkeleton& Ref = HareMesh->GetRefSkeleton();
        const int32 N = Ref.GetNum();
        Local = Ref.GetRefBonePose(); Parent.SetNum(N); Names.SetNum(N); ChildDir.Init(FVector::XAxisVector, N); Space.SetNum(N);
        for (int32 B = 0; B < N; ++B) { Parent[B] = Ref.GetParentIndex(B); Names[B] = Ref.GetBoneName(B); }
        for (int32 B = 0; B < N; ++B) if (Parent[B] >= 0 && Local[B].GetTranslation().Size() > .5f) ChildDir[Parent[B]] = Local[B].GetTranslation();
        bRig = true;
        for (int32 I = 0; I < BCount; ++I) { Bone[I] = Ref.FindBoneIndex(BoneNames[I]); bRig &= Bone[I] != INDEX_NONE; }
        // (the chains' own children: the forearm toward the paw, the shin toward the hock, ...; the spine toward the chest)
        auto Child = [&](int32 B, int32 C) { if (B != INDEX_NONE && C != INDEX_NONE) ChildDir[B] = Local[C].GetTranslation(); };
        Child(Bone[BSpine], Bone[BChest]); Child(Bone[BChest], Bone[BNeck]); Child(Bone[BNeck], Bone[BHead]);
        // (the end bones, the paws and toes, have no child: their rest direction (Tools/build_fey_hare.py BONES, component
        // space) taken into each one's own frame)
        TArray<FTransform> Rest; Rest.SetNum(N);
        for (int32 B = 0; B < N; ++B) Rest[B] = Parent[B] >= 0 ? Local[B] * Rest[Parent[B]] : Local[B];
        auto End = [&](int32 B, const FVector& Dir) { if (B != INDEX_NONE) ChildDir[B] = Rest[B].InverseTransformVector(Dir); };   // (the transforms carry the import's scale)
        End(Bone[BFPawL], FVector(8.f, 0, -5.5f)); End(Bone[BFPawR], FVector(8.f, 0, -5.5f));
        End(Bone[BToeL], FVector(10.f, 0, -1.8f)); End(Bone[BToeR], FVector(10.f, 0, -1.8f));
        const int32 Side[4] = {Bone[BFPawL], Bone[BFPawR], Bone[BToeL], Bone[BToeR]};
        for (int32 I = 0; I < 4; ++I) if (Side[I] != INDEX_NONE) LegY[I] = static_cast<float>(Rest[Side[I]].GetTranslation().Y);
        // (2026-11-25) The pose is solved in component-space positions and turns about each joint: the import's rest
        // transforms carry a mirroring scale, and composing rotations through them sent the limbs astray.
        RestT = Rest; RestHead.SetNum(N); RestDir.SetNum(N); Head.SetNum(N); Turned.Init(FQuat::Identity, N); BoneLen.SetNum(N);
        TArray<int32> ChildIdx; ChildIdx.Init(INDEX_NONE, N);
        for (int32 B = 0; B < N; ++B) RestHead[B] = Rest[B].GetTranslation();
        // (2026-11-26) By component-space distance: the local translations carry the import's 1/100 scale, so a test on
        // them found no child for any limb bone, and every leg was aimed as if it pointed forward (the forelegs folded up
        // into the chest).
        for (int32 B = 0; B < N; ++B) if (Parent[B] >= 0 && FVector::Dist(RestHead[B], RestHead[Parent[B]]) > .5f) ChildIdx[Parent[B]] = B;
        auto Prefer = [&](int32 B, int32 C) { if (B != INDEX_NONE && C != INDEX_NONE) ChildIdx[B] = C; };
        Prefer(Bone[BSpine], Bone[BChest]); Prefer(Bone[BChest], Bone[BNeck]); Prefer(Bone[BNeck], Bone[BHead]);
        Prefer(Bone[BFUpL], Bone[BFLoL]); Prefer(Bone[BFLoL], Bone[BFPawL]); Prefer(Bone[BFUpR], Bone[BFLoR]); Prefer(Bone[BFLoR], Bone[BFPawR]);
        Prefer(Bone[BThighL], Bone[BShinL]); Prefer(Bone[BShinL], Bone[BFootL]); Prefer(Bone[BFootL], Bone[BToeL]);
        Prefer(Bone[BThighR], Bone[BShinR]); Prefer(Bone[BShinR], Bone[BFootR]); Prefer(Bone[BFootR], Bone[BToeR]);
        for (int32 B = 0; B < N; ++B) { RestDir[B] = ChildIdx[B] != INDEX_NONE ? RestHead[ChildIdx[B]] - RestHead[B] : FVector(10.f, 0, 0); }
        // (the end bones: toward the paw's and the toe's tips, Tools/build_fey_hare.py BONES)
        for (int32 B : {Bone[BFPawL], Bone[BFPawR]}) if (B != INDEX_NONE) RestDir[B] = FVector(8.f, 0, -5.5f);
        for (int32 B : {Bone[BToeL], Bone[BToeR]}) if (B != INDEX_NONE) RestDir[B] = FVector(10.f, 0, -1.8f);
        for (int32 B = 0; B < N; ++B) BoneLen[B] = static_cast<float>(RestDir[B].Size());
        if (!bRig) UE_LOG(LogTemp, Warning, TEXT("CHUCK_FEY_HARE_RIG_MISSING bones"));
    }
    else UE_LOG(LogTemp, Warning, TEXT("CHUCK_FEY_HARE_MISSING mesh"));
    if (UMaterialInterface* M = Body->GetMaterial(0)) { Coat = UMaterialInstanceDynamic::Create(M, this); Body->SetMaterial(0, Coat); }
    auto Load = [this](const TCHAR* Stem, int32 Count)
    {
        const int32 First = Sounds.Num();
        for (int32 I = 0; I < Count; ++I)
            if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Stem, I, Stem, I))) Sounds.Add(S);
        SoundSets.Add(FIntPoint(First, Sounds.Num() - First));
    };
    Load(TEXT("SFX_FeyHareThump"), 3); Load(TEXT("SFX_FeyHareBound"), 4); Load(TEXT("SFX_FeyHareSqueal"), 2);
    Attenuation = NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate = true; Attenuation->Attenuation.bSpatialize = true;
    Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Attenuation->Attenuation.AttenuationShapeExtents = FVector(250.f, 0, 0);
    Attenuation->Attenuation.FalloffDistance = 1800.f;
}

void AFeyHare::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Rider.IsValid()) Rider->EndRide(false, FVector::ZeroVector);   // (its district dropped under him: on his feet where he is)
    Rider.Reset();
    GHares.Remove(this);
    Super::EndPlay(Reason);
}

void AFeyHare::Init(int32 InDistrict, const FVector& HomeWorld, float Yaw, int32 InIndex)
{
    District = InDistrict; Home = HomeWorld; HomeYaw = Yaw; Index = InIndex;
    if (Coat) Coat->SetVectorParameterValue(TEXT("Tint"), Coats[Index % UE_ARRAY_COUNT(Coats)]);
    ResetHare();
}

void AFeyHare::ResetHare()
{
    if (Rider.IsValid()) { Rider->EndRide(false, FVector::ZeroVector); Rider.Reset(); }
    SetActorLocationAndRotation(Home + FVector(0, 0, HalfHeight + 2.f), FRotator(0, HomeYaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    BoundW = AirW = RearW = SitW = 0; bLeaping = false; RefuseTime = 0;
    SetState(EState::Graze, Rand.FRandRange(3.f, 8.f));
}

const TCHAR* AFeyHare::GetStateName() const
{
    static const TCHAR* N[] = {TEXT("Graze"), TEXT("Alert"), TEXT("Hop"), TEXT("Shy"), TEXT("Ridden"), TEXT("Refuse"), TEXT("Rear"), TEXT("Bolt")};
    return N[static_cast<int32>(State)];
}

void AFeyHare::SetState(EState S, float Len)
{
    State = S; StateTime = 0; StateLen = Len;
}

void AFeyHare::Sound(int32 Which, float Volume, float Pitch)
{
    if (!SoundSets.IsValidIndex(Which) || SoundSets[Which].Y <= 0) return;
    USoundBase* S = Sounds[SoundSets[Which].X + Rand.RandRange(0, SoundSets[Which].Y - 1)];
    UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation() - FVector(0, 0, HalfHeight), Volume, Pitch * Rand.FRandRange(.94f, 1.06f), 0.f, Attenuation);
}

// ------------------------------------------------------------------ where it may go
bool AFeyHare::OpenGround(const FVector& World) const
{
    const FVector L = FeyLocal(World);
    if (FeyMap::DistrictAt(L.X, L.Y) != District) return false;
    if (FeyMap::InDeepWater(L.X, L.Y) || FeyMap::InRiver(District, L.X, L.Y)) return false;
    float T; if (FeyMap::ThresholdAt(L, T) >= 0) return false;
    for (int32 Th = 0; Th < FeyMap::Thresholds; ++Th)
    {
        const FeyMap::FThreshold& W = FeyMap::Threshold(Th);
        if (W.A == District && FVector2D::Distance(W.PA, FVector2D(L)) < RefuseRange + 300.f) return false;
        if (W.B == District && FVector2D::Distance(W.PB, FVector2D(L)) < RefuseRange + 300.f) return false;
    }
    return !InThrowZone(World);
}

bool AFeyHare::InThrowZone(const FVector& World) const
{
    // (2026-11-26, the user: bucks only where the game needs them) Only on its way out of its district: off its land, or on
    // a threshold's way. Its refusals keep it from getting there; this is the last resort.
    const FVector L = FeyLocal(World);
    if (FeyMap::DistrictAt(L.X, L.Y) != District) return true;
    float T; return FeyMap::ThresholdAt(L, T) >= 0;
}

bool AFeyHare::RefuseDir(const FVector& World, FVector& Dir) const
{
    // Near a threshold's mouth on its own side (the way down to the maze too), it will not go on toward it: the way toward
    // the mouth is taken out.
    const FVector2D L(FeyLocal(World));
    bool bRefused = false;
    for (int32 Th = 0; Th < FeyMap::Thresholds; ++Th)
    {
        const FeyMap::FThreshold& W = FeyMap::Threshold(Th);
        const FVector2D* Mouth = W.A == District ? &W.PA : W.B == District ? &W.PB : nullptr;
        if (!Mouth) continue;
        const FVector2D To = *Mouth - L; const float D = To.Size();
        if (D > RefuseRange) continue;
        const FVector N(To.X / D, To.Y / D, 0.f);
        const float Toward = FVector::DotProduct(Dir, N);
        if (Toward > 0.f) { Dir -= N * Toward * (D < RefuseRange * .75f ? 1.25f : 1.f); bRefused |= Toward > .35f; }
    }
    // Highmeadow's western brink (the overlook's drop to the maze): it will not go over.
    if (District == FeyMap::Highmeadow)
    {
        const float Edge = FeyMap::District(District).X0 + BrinkRange;
        if (L.X < Edge && Dir.X < 0.f) { const float Toward = static_cast<float>(-Dir.X); Dir.X += Toward * (L.X < Edge - 400.f ? 1.25f : 1.f); bRefused |= Toward > .35f; }
    }
    // ...never leaves its district's land, and will not go into deep water (a stream it wades).
    const FVector Ahead = World + Dir.GetSafeNormal2D() * 300.f;
    const FVector La = FeyLocal(Ahead);
    if (FeyMap::DistrictAt(La.X, La.Y) != District || FeyMap::InDeepWater(La.X, La.Y)) { Dir = -Dir * .2f; bRefused = true; }
    return bRefused;
}

bool AFeyHare::PickGoal(const FVector& Near, float R, FVector& Out)
{
    for (int32 Try = 0; Try < 10; ++Try)
    {
        const float A = Rand.FRandRange(0.f, 2.f * PI), D = Rand.FRandRange(R * .35f, R);
        FVector P = Near + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * D;
        if (FVector::Dist2D(P, Home) > Leash) P = Home + (P - Home).GetSafeNormal2D() * Leash * .8f;
        if (!OpenGround(P)) continue;
        // Nothing solid between here and there at its height.
        FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(FeyHareGoal), false, this);
        const FVector From = GetActorLocation(), To(P.X, P.Y, From.Z + 10.f);
        if (GetWorld()->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Q)) continue;
        Out = To; return true;
    }
    return false;
}

void AFeyHare::Steer(const FVector& Dir, float Speed)
{
    auto* Move = GetCharacterMovement();
    FVector D = Dir.GetSafeNormal2D();
    // Its head and chest reach well ahead of its capsule: what is just in front of the nose turns it aside.
    if (!D.IsNearlyZero())
    {
        FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(FeyHareNose), false, this);
        if (Rider.IsValid()) Q.AddIgnoredActor(Rider.Get());
        const FVector From = GetActorLocation() + FVector(0, 0, 10.f);
        if (GetWorld()->SweepSingleByChannel(Hit, From, From + D * (Radius + 45.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(18.f), Q) && FMath::Abs(Hit.ImpactNormal.Z) < .6f)
        {
            const FVector N = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0).GetSafeNormal();
            D = (D - N * FMath::Min(0.f, static_cast<float>(FVector::DotProduct(D, N)))).GetSafeNormal2D();
            Speed *= FMath::Lerp(.25f, 1.f, Hit.Time);
        }
    }
    Move->MaxWalkSpeed = FMath::Max(Speed, 1.f);
    if (!D.IsNearlyZero() && Speed > 1.f) AddMovementInput(D, 1.f);
}

// ------------------------------------------------------------------ mounting
AFeyHare* AFeyHare::MountPrompt(const AChuckCharacter* Chuck)
{
    if (!Chuck) return nullptr;
    AFeyHare* Best = nullptr; float BestD = MountReach;
    for (const TWeakObjectPtr<AFeyHare>& H : All())
        if (H->CanMount(Chuck))
        {
            const float D = static_cast<float>(FVector::Dist2D(H->GetActorLocation(), Chuck->GetActorLocation()));
            if (D < BestD + 40.f) { Best = H.Get(); BestD = D; }
        }
    return Best;
}

bool AFeyHare::CanMount(const AChuckCharacter* Chuck) const
{
    if (!Chuck || Rider.IsValid() || !bRig) return false;
    if (State != EState::Graze && State != EState::Alert && State != EState::Hop) return false;
    if (!GetCharacterMovement()->IsMovingOnGround() || GetVelocity().Size2D() > 140.f) return false;   // (a slow hop stops for him)
    if (!Chuck->CanBeginRide()) return false;
    const FVector To = Chuck->GetActorLocation() - GetActorLocation();
    return FMath::Abs(To.Z) < 80.f && To.Size2D() < MountReach + 40.f;   // (his capsule to its centre: its body is long)
}

bool AFeyHare::Mount(AChuckCharacter* Chuck)
{
    if (!CanMount(Chuck)) return false;
    // He climbs on from the side he stands on: it turns a little so he is at its left flank.
    GetCharacterMovement()->StopMovementImmediately();
    Rider = Chuck;
    MoveIgnoreActorAdd(Chuck);
    if (!Chuck->BeginRide(this)) { Rider.Reset(); MoveIgnoreActorRemove(Chuck); return false; }
    SetState(EState::Ridden);
    ++Mounts;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_MOUNT hare=%d district=%s at=%s"), Index, FeyMap::District(District).Name, *FeyLocal(GetActorLocation()).ToCompactString());
    return true;
}

void AFeyHare::ReleaseRider(bool bThrow)
{
    if (AChuckCharacter* C = Rider.Get())
    {
        if (bThrow)
        {
            const FVector Fwd = GetActorForwardVector();
            C->EndRide(true, Fwd * FMath::Max(380.f, static_cast<float>(GetVelocity().Size2D()) * .8f) + FVector(0, 0, 360.f));
        }
        else C->EndRide(false, FVector::ZeroVector);
        MoveIgnoreActorRemove(C);
    }
    Rider.Reset();
}

void AFeyHare::Dismount()
{
    if (!Rider.IsValid()) return;
    ReleaseRider(false);
    GetCharacterMovement()->StopMovementImmediately();
    SetState(EState::Alert, Rand.FRandRange(2.f, 4.f));
    CalmUntil = Clock + 6.f;   // (it lets him climb straight back on)
    ++Dismounts;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_DISMOUNT hare=%d"), Index);
}

void AFeyHare::Leap()
{
    if (State != EState::Ridden || bLeaping || !GetCharacterMovement()->IsMovingOnGround() || (Rider.IsValid() && Rider->IsRideMounting())) return;
    const FVector Fwd = GetActorForwardVector();
    const float Ahead = FMath::Max(460.f, static_cast<float>(FVector::DotProduct(GetVelocity(), Fwd)));
    LaunchCharacter(Fwd * Ahead + FVector(0, 0, 370.f), true, true);
    bLeaping = true; ++Leaps;
    Sound(SBound, .9f, .85f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_LEAP hare=%d speed=%.0f"), Index, Ahead);
}

void AFeyHare::Throw(const TCHAR* Why)
{
    if (State != EState::Ridden && State != EState::Refuse) return;
    LastThrow = Why;
    SetState(EState::Rear, .42f);
    Sound(SSqueal, 1.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_REAR hare=%d why=%s at=%s"), Index, Why, *FeyLocal(GetActorLocation()).ToCompactString());
}

// ------------------------------------------------------------------ behaviour
void AFeyHare::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Dt = FMath::Min(DeltaSeconds, .05f);
    Clock += Dt; StateTime += Dt;
    if (Rider.IsValid() && Rider->GetRideMount() != this) { MoveIgnoreActorRemove(Rider.Get()); Rider.Reset(); if (State == EState::Ridden || State == EState::Refuse) SetState(EState::Alert, 2.f); }
    if (bPoseHold && Index == 0) { State = PoseState; GetCharacterMovement()->StopMovementImmediately(); }
    else if (State == EState::Ridden || State == EState::Refuse || State == EState::Rear) Ride(Dt);
    else Think(Dt);
    if (bLeaping && GetCharacterMovement()->IsMovingOnGround() && GetVelocity().Z <= 0.f) { bLeaping = false; Sound(SBound, 1.f, .8f); }
    Pose(Dt);
}

void AFeyHare::Think(float Dt)
{
    AChuckCharacter* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const FVector At = GetActorLocation();
    // Run or sprint straight at it from close by and it lollops off (a walking approach, 72 cm/s, does not trouble it).
    if (Chuck && !Chuck->IsRiding() && State != EState::Shy && State != EState::Bolt && Clock > CalmUntil)
    {
        const FVector To = At - Chuck->GetActorLocation(); const FVector V = Chuck->GetVelocity();
        if (To.Size2D() < ShyRange && V.Size2D() > 180.f && FVector::DotProduct(V.GetSafeNormal2D(), To.GetSafeNormal2D()) > .5f)
        {
            FVector Away = To.GetSafeNormal2D() * 700.f + FVector(Rand.FRandRange(-200.f, 200.f), Rand.FRandRange(-200.f, 200.f), 0.f);
            FVector G; if (!PickGoal(At + Away, 350.f, G)) PickGoal(At, 700.f, G);
            Goal = G; SetState(EState::Shy, 2.2f); ++Shies;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_SHY hare=%d"), Index);
        }
    }
    switch (State)
    {
    case EState::Graze:
    case EState::Alert:
        Steer(FVector::ZeroVector, 0.f);
        if (StateTime > StateLen)
        {
            if (State == EState::Graze && Rand.FRand() < .55f) SetState(EState::Alert, Rand.FRandRange(1.5f, 3.f));
            else
            {
                FVector G;
                const bool bHome = FVector::Dist2D(At, Home) > Leash * .6f;
                if (PickGoal(bHome ? FMath::Lerp(At, Home, .6f) : At, bHome ? 500.f : 450.f, G)) { Goal = G; SetState(EState::Hop, 6.f); }
                else SetState(EState::Graze, Rand.FRandRange(3.f, 6.f));
            }
        }
        break;
    case EState::Hop:
    case EState::Shy:
    case EState::Bolt:
    {
        const FVector To = Goal - At;
        const float Speed = State == EState::Hop ? 110.f : State == EState::Shy ? 300.f : 600.f;
        Steer(To, To.Size2D() < 80.f ? Speed * To.Size2D() / 80.f : Speed);
        if (To.Size2D() < 40.f || StateTime > (State == EState::Bolt ? 20.f : StateLen))
        {
            if (State == EState::Bolt && FVector::Dist2D(At, Home) > 400.f && StateTime < 20.f) { Goal = Home + FVector(0, 0, HalfHeight); break; }
            SetState(State == EState::Hop ? EState::Graze : EState::Alert, Rand.FRandRange(2.f, 7.f));
        }
        break;
    }
    default: break;
    }
}

void AFeyHare::Ride(float Dt)
{
    AChuckCharacter* Chuck = Rider.Get();
    if (State == EState::Rear)
    {
        Steer(FVector::ZeroVector, 0.f);
        GetCharacterMovement()->StopMovementImmediately();
        if (StateTime >= StateLen && Chuck)
        {
            ReleaseRider(true);
            ++Throws; ++Bolts;
            Goal = Home + FVector(0, 0, HalfHeight);
            SetState(EState::Bolt, 20.f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_THROW hare=%d why=%s"), Index, *LastThrow);
        }
        else if (!Chuck) SetState(EState::Bolt, 20.f);
        return;
    }
    if (!Chuck) { SetState(EState::Alert, 2.f); return; }
    if (Chuck->IsRideMounting()) { Steer(FVector::ZeroVector, 0.f); return; }
    const FVector At = GetActorLocation();
    // (2026-11-26, the user: bucks only where the game needs them) Bites, seeds and moths no longer throw him; he stays on.
    // Only its way out of the district does (InThrowZone), and deep water slides him off.
    if (InThrowZone(At)) { Throw(TEXT("leaving")); return; }
    { const FVector L = FeyLocal(At); if (FeyMap::InDeepWater(L.X, L.Y)) { Dismount(); return; } }
    if (State == EState::Refuse)
    {
        // A thump of a hind foot, and it turns away from where it would not go.
        Steer(FVector::ZeroVector, 0.f);
        if (ThumpAt < 0.f && StateTime > .12f) { ThumpAt = StateTime; Sound(SThump, 1.f); ++Thumps; }
        SetActorRotation(FRotator(0, FMath::FInterpTo(static_cast<float>(GetActorRotation().Yaw), TurnAway, Dt, 7.f), 0));
        if (StateTime > .7f) { SetState(EState::Ridden); RefuseTime = 0; }
        return;
    }
    FVector Stick = bTestStick ? TestStick.GetClampedToMaxSize(1.f) : Chuck->GetRideStick();
    Stick.Z = 0.f;
    const bool bRun = bTestStick ? bTestRun : Chuck->IsRunToggled();
    const float Push = static_cast<float>(Stick.Size());
    FVector Dir = Push > .1f ? Stick / Push : FVector::ZeroVector;
    float Speed = Push > .1f ? (bRun ? BoundSpeed : HopSpeed * FMath::Clamp(Push * 1.3f, .4f, 1.f)) : 0.f;
    Speed *= FeyGroundSpeedScale(At - FVector(0, 0, HalfHeight - 5.f));
    if (!Dir.IsNearlyZero() && RefuseDir(At, Dir))
    {
        RefuseTime += Dt;
        Speed *= .35f;
        if (RefuseTime > .6f)
        {
            ++Refusals; ThumpAt = -1.f;
            TurnAway = (-Stick).Rotation().Yaw;
            SetState(EState::Refuse, .7f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_REFUSE hare=%d at=%s"), Index, *FeyLocal(At).ToCompactString());
            return;
        }
    }
    else RefuseTime = FMath::Max(0.f, RefuseTime - Dt);
    auto* Move = GetCharacterMovement();
    Move->RotationRate = FRotator(0, FMath::Lerp(420.f, 230.f, BoundW), 0);
    Move->MaxAcceleration = bRun ? 1100.f : 900.f;
    Steer(Dir, Speed);
    if (Move->IsMovingOnGround()) TopSpeed = FMath::Max(TopSpeed, static_cast<float>(GetVelocity().Size2D()));
}

// ------------------------------------------------------------------ the pose
void AFeyHare::Aim(int32 B, const FVector& Dir)
{
    const FVector Cur = Turned[B].RotateVector(RestDir[B]).GetSafeNormal();
    const FVector Want = Dir.GetSafeNormal();
    if (Cur.IsNearlyZero() || Want.IsNearlyZero()) return;
    Turned[B] = (FQuat::FindBetweenNormals(Cur, Want) * Turned[B]).GetNormalized();
}

namespace
{
    /** The knee of a two-bone chain from Root to Goal (lengths A, B), bent toward Pole. */
    FVector Knee(const FVector& Root, const FVector& Goal, float A, float B, const FVector& Pole)
    {
        FVector U = Goal - Root; float D = static_cast<float>(U.Size());
        if (D < 1e-3f) return Root + Pole * A;
        U /= D; D = FMath::Clamp(D, FMath::Abs(A - B) + .01f, A + B - .01f);
        FVector P = Pole - U * FVector::DotProduct(Pole, U); if (!P.Normalize()) P = FVector::UpVector;
        const float Along = (A * A - B * B + D * D) / (2.f * D);
        return Root + U * Along + P * FMath::Sqrt(FMath::Max(0.f, A * A - Along * Along));
    }
}

void AFeyHare::Pose(float Dt)
{
    if (!bRig) return;
    auto* Move = GetCharacterMovement();
    const bool bHeld = bPoseHold && Index == 0;
    const bool bGround = bHeld ? !bPoseAir : Move->IsMovingOnGround();
    const float V = bHeld ? PoseSpeed : static_cast<float>(GetVelocity().Size2D());
    // How far into the bound (gait amplitude by speed), and the leap's extension in the air.
    const float A = FMath::Clamp((V - GaitLo) / (GaitHi - GaitLo), 0.f, 1.f);
    const float Gait = FMath::Clamp(V / 70.f, 0.f, 1.f);
    BoundW = FMath::FInterpTo(BoundW, A, Dt, 6.f);
    AirW = FMath::FInterpTo(AirW, !bGround ? 1.f : 0.f, Dt, !bGround ? 9.f : 14.f);
    const float Freq = FMath::Lerp(1.7f, 3.3f, A);
    if (bGround && V > 5.f) Phase = FMath::Frac(Phase + Freq * Dt);
    if (bHeld) Phase = PosePhase;
    // The bound's thuds: as the hind feet strike.
    if (bGround && V > 200.f && Phase >= .5f && BoundSoundPhase < .5f) Sound(SBound, FMath::Lerp(.25f, .55f, A), FMath::Lerp(1.1f, .9f, A));
    BoundSoundPhase = Phase;
    // Moods.
    const bool bStill = V < 20.f && bGround;
    const float Nib = FMath::Sin(Clock * 9.f + Index);
    auto Ease = [Dt](float& X, float To, float Rate) { X = FMath::FInterpTo(X, To, Dt, Rate); };
    Ease(GrazeW, State == EState::Graze && bStill ? 1.f : 0.f, 3.f);
    Ease(SitW, (State == EState::Alert && bStill) ? 1.f : 0.f, 4.f);
    Ease(RearW, State == EState::Rear ? 1.f : 0.f, State == EState::Rear ? 12.f : 5.f);
    Ease(Crouch, bStill && State != EState::Alert ? 1.f : 0.f, 3.f);
    Ease(EarBack, FMath::Max(A * 1.1f, State == EState::Rear || State == EState::Shy || State == EState::Bolt ? .8f : 0.f), 5.f);
    Ease(ScutUp, State == EState::Shy || State == EState::Bolt || State == EState::Rear ? 1.f : 0.f, 8.f);
    if (Clock > NextEarTurn) { EarTurnGoal = Rand.FRandRange(-25.f, 25.f) * (1.f - A); NextEarTurn = Clock + Rand.FRandRange(.6f, 2.2f); }
    Ease(EarTurn, EarTurnGoal, 6.f);
    // Turning: it leans in (and he with it).
    const float Yaw = static_cast<float>(GetActorRotation().Yaw);
    const float YawRate = Dt > 0.f ? FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / Dt : 0.f; LastYaw = Yaw;
    Ease(Lean, FMath::Clamp(YawRate * V / 9000.f, -14.f, 14.f), 5.f);
    // The ground's slope along its length.
    {
        FHitResult Hf, Hb; FCollisionQueryParams Q(SCENE_QUERY_STAT(FeyHareSlope), false, this); if (Rider.IsValid()) Q.AddIgnoredActor(Rider.Get());
        const FVector F = GetActorForwardVector().GetSafeNormal2D(), C = GetActorLocation();
        const bool bF = GetWorld()->LineTraceSingleByChannel(Hf, C + F * 35.f, C + F * 35.f - FVector(0, 0, 140.f), ECC_Visibility, Q);
        const bool bB = GetWorld()->LineTraceSingleByChannel(Hb, C - F * 35.f, C - F * 35.f - FVector(0, 0, 140.f), ECC_Visibility, Q);
        const float Want = bF && bB && bGround ? FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Hf.ImpactPoint.Z - Hb.ImpactPoint.Z), 70.f)) : 0.f;
        Ease(Slope, FMath::Clamp(Want, -25.f, 25.f), 6.f);
    }
    const float W = 2.f * PI * Phase;
    // The body: its bob and pitch in the bound, crouched when still, grazing nose down, sitting up, rearing.
    const float Bob = A * (4.5f * FMath::Cos(2.f * PI * (Phase - .85f)) + 1.8f * FMath::Cos(4.f * PI * (Phase - .33f))) + (1 - A) * Gait * 2.f * FMath::Abs(FMath::Sin(W));
    const float BodyPitch = -6.f * A * FMath::Cos(2.f * PI * (Phase - .1f)) - 1.f * GrazeW + 3.f * Crouch * (1.f - SitW) + 48.f * SitW * (1.f - RearW) + 66.f * RearW + Slope + 6.f * AirW * (1.f - A);
    const float Drop = 3.f * Crouch + 1.f * GrazeW;   // (2026-11-26: a hare grazes on straight forelegs; the crouch is in its hind legs)
    const float Flex = A * 13.f * FMath::Cos(2.f * PI * (Phase - .45f)) - 10.f * AirW + 6.f * Crouch;
    const FVector Pivot = FVector(-26.f, 0, 22.f) * FMath::Max(SitW, RearW) + FVector(-4.f, 0, 60.f) * (1.f - FMath::Max(SitW, RearW));
    const FQuat BodyQ = PitchQ(BodyPitch) * RollQ(Lean);
    const FVector BodyT = FVector(0, 0, Bob * Gait - Drop) + Pivot - BodyQ.RotateVector(Pivot);
    // Foot goals (component space; the ground is z 0, tilted with the slope).
    const float SlopeTan = FMath::Tan(FMath::DegreesToRadians(Slope));
    const float Freqv = FMath::Max(Freq, .1f);
    struct FLegGait { float Off, Duty, Lift, Xn, Y; };
    const float Df = FMath::Lerp(.45f, .2f, A), Dh = FMath::Lerp(.4f, .18f, A);
    const FLegGait Legs[4] = {{0.f, Df, 7.f + 7.f * A, 24.f, LegY[0]}, {FMath::Lerp(.12f, .07f, A), Df, 7.f + 7.f * A, 24.f, LegY[1]},
                              {FMath::Lerp(.5f, .48f, A), Dh, 10.f + 14.f * A, -12.f + 15.f * A, LegY[2]}, {FMath::Lerp(.54f, .53f, A), Dh, 10.f + 14.f * A, -12.f + 15.f * A, LegY[3]}};
    for (int32 I = 0; I < 4; ++I)
    {
        const FLegGait& L = Legs[I];
        const float Sweep = V * L.Duty / Freqv;
        const float U = FMath::Frac(Phase - L.Off);
        float X = L.Xn, Z = 0.f;
        if (U < L.Duty) X += Sweep * (.5f - U / L.Duty);
        else { const float S = (U - L.Duty) / (1.f - L.Duty); X += Sweep * (-.5f + Smooth(0.f, 1.f, S)); Z = L.Lift * FMath::Sin(PI * S); }
        X = FMath::Lerp(L.Xn, X, Gait); Z *= Gait;
        // In the air: forefeet reaching, hind feet trailing; then landing.
        X = FMath::Lerp(X, I < 2 ? L.Xn + 20.f : L.Xn - 26.f, AirW); Z = FMath::Lerp(Z, I < 2 ? 18.f : 12.f, AirW);
        FootGoal[I] = FVector(X, L.Y, Z + X * SlopeTan);
        HeelLift[I] = I < 2 ? 34.f : FMath::Lerp(FMath::Lerp(34.f, 14.f, Crouch * (1.f - SitW)), 50.f, A * (U < L.Duty ? Smooth(0.f, L.Duty, U) : 0.f));
    }
    // Compose, parent first.
    const int32 N = Local.Num();
    for (int32 B = 0; B < N; ++B)
    {
        const int32 Pa = Parent[B];
        if (Pa >= 0) { Turned[B] = Turned[Pa]; Head[B] = Head[Pa] + Turned[Pa].RotateVector(RestHead[B] - RestHead[Pa]); }
        else { Turned[B] = FQuat::Identity; Head[B] = RestHead[B]; }
        auto Turn = [&](const FQuat& Q) { Turned[B] = (Q * Turned[B]).GetNormalized(); };
        if (B == Bone[BSpine]) { Turned[B] = BodyQ * Turned[B]; Head[B] = BodyQ.RotateVector(Head[B]) + BodyT; }
        else if (B == Bone[BChest]) Turn(PitchQ(-Flex * .6f - 2.f * GrazeW));
        else if (B == Bone[BHips]) Turn(PitchQ(Flex * .7f + 18.f * SitW * (1.f - RearW)));
        else if (B == Bone[BNeck]) Turn(PitchQ(-58.f * GrazeW + 8.f * SitW - 10.f * A + 8.f * RearW) * YawQ(EarTurn * .3f * (1.f - GrazeW)));
        else if (B == Bone[BHead]) Turn(PitchQ(-22.f * GrazeW + 3.f * GrazeW * Nib - 40.f * SitW * (1.f - RearW) - 50.f * RearW + 8.f * A - BodyPitch * .5f * (1.f - GrazeW) + 1.f * FMath::Sin(Clock * 37.f) * (1 - A)));
        else if (B == Bone[BEar1L] || B == Bone[BEar1R]) { const float S = B == Bone[BEar1L] ? 1.f : -1.f; Turn(PitchQ(62.f * EarBack + 12.f * GrazeW) * RollQ(S * (8.f * GrazeW + 10.f * EarBack)) * YawQ(EarTurn * S)); }
        else if (B == Bone[BEar2L] || B == Bone[BEar2R]) Turn(PitchQ(10.f * EarBack + 6.f * A * FMath::Sin(W * 2.f)));
        else if (B == Bone[BScut]) Turn(PitchQ(-28.f * ScutUp));
        // Legs: the two-bone reach to each foot's goal, then the foot.
        else if (B == Bone[BFUpL] || B == Bone[BFUpR])
        {
            const int32 I = B == Bone[BFUpL] ? 0 : 1;
            const int32 Lo = Bone[I ? BFLoR : BFLoL];
            const float La = BoneLen[B], Lb = BoneLen[Lo];
            const FVector Root = Head[B];
            FVector Toe = FootGoal[I];
            // Sitting up or rearing, the forefeet come off the ground and hang (they paw at the air rearing).
            const float Up = FMath::Max(SitW, RearW);
            const FVector Hang = Root + FVector(4.f + 14.f * RearW + 6.f * RearW * FMath::Sin(Clock * 14.f + I * 2.f), 0.f, -30.f + 10.f * RearW + 4.f * RearW * FMath::Cos(Clock * 14.f + I * 2.f));   // (straight down from the shoulder sitting up; pawing forward rearing)
            const float PawA = FMath::DegreesToRadians(HeelLift[I]);
            FVector Wrist = Toe - FVector(FMath::Cos(PawA), 0.f, -FMath::Sin(PawA)) * 9.6f;
            Wrist = FMath::Lerp(Wrist, Hang, Up);
            const FVector K = Knee(Root, Wrist, La, Lb, FMath::Lerp(BodyQ.RotateVector(FVector(-1.f, 0.f, .15f)), FVector(-1.f, 0.f, .1f), Up));
            Aim(B, K - Root);
        }
        else if (B == Bone[BFLoL] || B == Bone[BFLoR])
        {
            const int32 I = B == Bone[BFLoL] ? 0 : 1;
            const float Up = FMath::Max(SitW, RearW);
            const float PawA = FMath::DegreesToRadians(HeelLift[I]);
            const FVector Root = Head[Bone[I ? BFUpR : BFUpL]];
            const FVector Hang = Root + FVector(4.f + 14.f * RearW + 6.f * RearW * FMath::Sin(Clock * 14.f + I * 2.f), 0.f, -30.f + 10.f * RearW + 4.f * RearW * FMath::Cos(Clock * 14.f + I * 2.f));   // (straight down from the shoulder sitting up; pawing forward rearing)
            const FVector Wrist = FMath::Lerp(FootGoal[I] - FVector(FMath::Cos(PawA), 0.f, -FMath::Sin(PawA)) * 9.6f, Hang, Up);
            Aim(B, Wrist - Head[B]);
        }
        else if (B == Bone[BFPawL] || B == Bone[BFPawR])
        {
            const int32 I = B == Bone[BFPawL] ? 0 : 1;
            const float Up = FMath::Max(SitW, RearW), Swing = FootGoal[I].Z > 1.f ? FMath::Clamp(FootGoal[I].Z / 10.f, 0.f, 1.f) : 0.f;
            const FVector Flat = FVector(FMath::Cos(FMath::DegreesToRadians(HeelLift[I])), 0.f, -FMath::Sin(FMath::DegreesToRadians(HeelLift[I])));
            const FVector Tucked = FVector(-.3f, 0.f, -1.f);   // folded back in the swing and hanging
            Aim(B, FMath::Lerp(Flat, Tucked, FMath::Max(Up, Swing * .7f)));
        }
        else if (B == Bone[BThighL] || B == Bone[BThighR])
        {
            const int32 I = B == Bone[BThighL] ? 2 : 3;
            const int32 Sh = Bone[I == 2 ? BShinL : BShinR], Ft = Bone[I == 2 ? BFootL : BFootR];
            const float La = BoneLen[B], Lb = BoneLen[Sh], Lf = BoneLen[Ft];
            const float H = FMath::DegreesToRadians(HeelLift[I]);
            const FVector Hock = FootGoal[I] + FVector(-FMath::Cos(H), 0.f, FMath::Sin(H)) * Lf;
            const FVector Root = Head[B];
            const FVector K = Knee(Root, Hock, La, Lb, BodyQ.RotateVector(FVector(1.f, 0.f, .1f)));
            Aim(B, K - Root);
        }
        else if (B == Bone[BShinL] || B == Bone[BShinR])
        {
            const int32 I = B == Bone[BShinL] ? 2 : 3;
            const float Lf = BoneLen[Bone[I == 2 ? BFootL : BFootR]];
            const float H = FMath::DegreesToRadians(HeelLift[I]);
            Aim(B, FootGoal[I] + FVector(-FMath::Cos(H), 0.f, FMath::Sin(H)) * Lf - Head[B]);
        }
        else if (B == Bone[BFootL] || B == Bone[BFootR])
        {
            const int32 I = B == Bone[BFootL] ? 2 : 3;
            Aim(B, FootGoal[I] - Head[B]);
        }
        else if (B == Bone[BToeL] || B == Bone[BToeR])
        {
            const int32 I = B == Bone[BToeL] ? 2 : 3;
            const float Swing = FMath::Clamp(FootGoal[I].Z / 10.f, 0.f, 1.f);
            Aim(B, FMath::Lerp(FVector(1.f, 0.f, -.18f), FVector(.2f, 0.f, -1.f), Swing * .6f));
        }
        Space[B] = FTransform(Turned[B] * RestT[B].GetRotation(), Head[B], RestT[B].GetScale3D());
        Body->SetBoneTransformByName(Names[B], Space[B], EBoneSpaces::ComponentSpace);
    }
    Body->MarkRefreshTransformDirty();
    if (bHeld && FMath::Frac(Clock) < Dt)
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_HARE_FORE state=%s goal=%s shoulder=%s elbow=%s wrist=%s pawtip=%s | hip=%s knee=%s hock=%s"), GetStateName(), *FootGoal[0].ToCompactString(),
               *Head[Bone[BFUpL]].ToCompactString(), *Head[Bone[BFLoL]].ToCompactString(), *Head[Bone[BFPawL]].ToCompactString(), *(Head[Bone[BFPawL]] + Turned[Bone[BFPawL]].RotateVector(RestDir[Bone[BFPawL]])).ToCompactString(),
               *Head[Bone[BThighL]].ToCompactString(), *Head[Bone[BShinL]].ToCompactString(), *Head[Bone[BFootL]].ToCompactString());
    // His seat on its back (world), turned with its body.
    const FTransform C = Body->GetComponentTransform();
    Seat = C.TransformPosition(Head[Bone[BSaddle]]);
    SeatQuat = C.GetRotation() * BodyQ * PitchQ(-Flex * .3f);
}
