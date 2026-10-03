#include "EnemyRat.h"
#include "ChuckCharacter.h"
#include "CigarettePickup.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    TArray<TWeakObjectPtr<AEnemyRat>> RatRegistry;
    constexpr float Size = 1.2f;          // a big dock rat: the model's 26 cm body at about 31 cm
    constexpr float HalfHeight = 11.f;
    // Bones posed each frame (SK_Rat, Tools/build_enemy_rat.py).
    enum EBone { Pelvis, Spine, Chest, Neck, Head, ArmL, ForearmL, HandL, ArmR, ForearmR, HandR,
                 ThighL, ShinL, FootL, ThighR, ShinR, FootR, Tail0, Tail1, Tail2, Tail3, Tail4, Tail5, BoneCount };
    const TCHAR* BoneNames[] = { TEXT("pelvis"), TEXT("spine"), TEXT("chest"), TEXT("neck"), TEXT("head"),
        TEXT("arm_L"), TEXT("forearm_L"), TEXT("hand_L"), TEXT("arm_R"), TEXT("forearm_R"), TEXT("hand_R"),
        TEXT("thigh_L"), TEXT("shin_L"), TEXT("foot_L"), TEXT("thigh_R"), TEXT("shin_R"), TEXT("foot_R"),
        TEXT("tail_0"), TEXT("tail_1"), TEXT("tail_2"), TEXT("tail_3"), TEXT("tail_4"), TEXT("tail_5") };
    // Component axes: X forward, Y right, Z up. A positive pitch (about +Y)
    // swings a hanging limb backward and tips a forward bone nose-down.
    FQuat Pitch(float Degrees) { return FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Degrees)); }
    FQuat Yaw(float Degrees) { return FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Degrees)); }
}

AEnemyRat::AEnemyRat()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(11.f, HalfHeight);
    auto* Move = GetCharacterMovement();
    Move->bRunPhysicsWithNoController = true;   // driven by its own Tick, no AI controller
    Move->bOrientRotationToMovement = true;
    Move->RotationRate = FRotator(0, 720, 0);
    Move->MaxWalkSpeed = RoamSpeed;
    Move->MaxAcceleration = 1400;
    Move->BrakingDecelerationWalking = 1400;
    Move->MaxStepHeight = 5;
    Move->SetWalkableFloorAngle(45);
    bUseControllerRotationYaw = false;
    GetMesh()->SetVisibility(false);   // the character's default mesh is unused
    Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
    Body->SetupAttachment(GetCapsuleComponent());
    Body->SetRelativeLocation(FVector(0, 0, -HalfHeight));
    Body->SetRelativeScale3D(FVector(Size));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Rat(TEXT("/Game/Characters/Rat/SK_Rat.SK_Rat"));
    Body->SetSkinnedAssetAndUpdate(Rat.Object);
}

void AEnemyRat::BeginPlay()
{
    Super::BeginPlay();
    RatRegistry.Add(this);
    Home = GetActorLocation();
    RoamTarget = Home;
    NextChitter = FMath::FRandRange(1.f, 4.f);
    auto Load = [](TArray<USoundBase*>& Set, const TCHAR* Stem, int32 Count)
    {
        for (int32 I = 0; I < Count; ++I)
            if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Stem, I, Stem, I))) Set.Add(Sound);
    };
    Load(ChitterSounds, TEXT("SFX_RatChitter"), 3); Load(HissSounds, TEXT("SFX_RatHiss"), 2);
    Load(BiteSounds, TEXT("SFX_RatBite"), 2); Load(HurtSounds, TEXT("SFX_RatHurt"), 2); Load(DeathSounds, TEXT("SFX_RatDeath"), 1);
    // Heard nearby, not across the docks.
    Attenuation = NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate = true;
    Attenuation->Attenuation.bSpatialize = true;
    Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Attenuation->Attenuation.AttenuationShapeExtents = FVector(200.f, 0.f, 0.f);
    Attenuation->Attenuation.FalloffDistance = 900.f;
    BoneIndex.Init(INDEX_NONE, BoneCount);
    if (const USkinnedAsset* Asset = Body->GetSkinnedAsset())
        for (int32 I = 0; I < BoneCount; ++I) BoneIndex[I] = Asset->GetRefSkeleton().FindBoneIndex(BoneNames[I]);
}

void AEnemyRat::EndPlay(const EEndPlayReason::Type Reason)
{
    RatRegistry.Remove(this);
    Super::EndPlay(Reason);
}

const TArray<TWeakObjectPtr<AEnemyRat>>& AEnemyRat::All()
{
    RatRegistry.RemoveAll([](const TWeakObjectPtr<AEnemyRat>& Entry) { return !Entry.IsValid(); });
    return RatRegistry;
}

const TCHAR* AEnemyRat::GetStateName() const
{
    static const TCHAR* Names[] = { TEXT("Roam"), TEXT("Chase"), TEXT("Windup"), TEXT("Lunge"), TEXT("Recover"), TEXT("Hurt"), TEXT("Dead") };
    return Names[static_cast<int32>(State)];
}

void AEnemyRat::SetState(EState NewState)
{
    State = NewState; StateTime = 0;
    GetCharacterMovement()->bOrientRotationToMovement = State == EState::Roam || State == EState::Chase;
    if (State == EState::Windup) Play(HissSounds, .5f);
    if (State == EState::Lunge) bBit = false;
}

void AEnemyRat::Play(const TArray<USoundBase*>& Set, float Volume)
{
    if (Set.Num()) UGameplayStatics::PlaySoundAtLocation(this, Set[FMath::RandRange(0, Set.Num() - 1)], GetActorLocation(), Volume, FMath::FRandRange(.94f, 1.06f), 0.f, Attenuation);
}

void AEnemyRat::FaceToward(const FVector& Target, float DeltaSeconds, float Rate)
{
    const FVector To = (Target - GetActorLocation()).GetSafeNormal2D();
    if (To.IsNearlyZero()) return;
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), To.Rotation(), DeltaSeconds, Rate));
}

void AEnemyRat::TakeSlash(const FVector& Swing)
{
    if (State == EState::Dead) return;
    ++HitsTaken;
    if (HitsTaken >= Health)
    {
        SetState(EState::Dead);
        Play(DeathSounds, .6f);
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const FVector At = GetActorLocation();
        ACigarettePickup::Burst(GetWorld(), At, Cigarettes, static_cast<float>(At.Z) - HalfHeight);
        return;
    }
    // Knocked back along the swing and away, a little off the ground.
    SetState(EState::Hurt);
    Play(HurtSounds, .55f);
    LaunchCharacter(Swing.GetSafeNormal2D() * 170.f + FVector(0, 0, 110.f), true, true);
}

void AEnemyRat::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    StateTime += DeltaSeconds;
    auto* Move = GetCharacterMovement();
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const FVector Location = GetActorLocation();
    float Distance = 1e6f, Rise = 1e6f;
    FVector ToChuck = FVector::ZeroVector;
    if (Chuck)
    {
        ToChuck = Chuck->GetActorLocation() - Location;
        Distance = static_cast<float>(ToChuck.Size2D());
        // Feet to feet: is he on the rat's level?
        Rise = static_cast<float>(FMath::Abs(ToChuck.Z - (Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - HalfHeight)));
    }
    const bool bReachable = Chuck && !Chuck->IsAstral() && Rise < 60.f;   // gone into the light: nothing to chase
    switch (State)
    {
    case EState::Roam:
    {
        Move->MaxWalkSpeed = RoamSpeed;
        if (bReachable && Distance < NoticeRange) { SetState(EState::Chase); break; }
        NextChitter -= DeltaSeconds;
        if (NextChitter <= 0) { Play(ChitterSounds, .35f); NextChitter = FMath::FRandRange(2.5f, 6.f); }
        if (RoamPause > 0) { RoamPause -= DeltaSeconds; break; }
        const FVector To = RoamTarget - Location;
        if (To.Size2D() < 10.f || StateTime > 4.f)
        {
            RoamPause = FMath::FRandRange(1.f, 2.5f);
            const FVector2D Off = FMath::RandPointInCircle(120.f);
            RoamTarget = Home + FVector(Off.X, Off.Y, 0);
            StateTime = 0;
        }
        else AddMovementInput(To.GetSafeNormal2D(), 1.f);
        break;
    }
    case EState::Chase:
        Move->MaxWalkSpeed = ChaseSpeed;
        if (!bReachable || Distance > LoseRange) { RoamTarget = Home; SetState(EState::Roam); break; }
        if (Distance <= StrikeRange) { SetState(EState::Windup); break; }
        AddMovementInput(ToChuck.GetSafeNormal2D(), 1.f);
        break;
    case EState::Windup:
        // The tell: stop, face him, crouch and hiss.
        if (Chuck) FaceToward(Chuck->GetActorLocation(), DeltaSeconds, 12.f);
        if (StateTime >= WindupTime)
        {
            LastWindupSeconds = StateTime;
            LungeDirection = GetActorForwardVector().GetSafeNormal2D();
            SetState(EState::Lunge);
        }
        break;
    case EState::Lunge:
        Move->Velocity = FVector(LungeDirection.X * LungeSpeed, LungeDirection.Y * LungeSpeed, Move->Velocity.Z);
        if (!bBit && StateTime >= .1f)
        {
            bBit = true;   // one snap per lunge, hit or miss
            Play(BiteSounds, .55f);
            if (Chuck && Distance <= BiteRange && FVector::DotProduct(LungeDirection, ToChuck.GetSafeNormal2D()) > .3f)
                Chuck->TakeBite(Location);
        }
        if (StateTime >= LungeTime) SetState(EState::Recover);
        break;
    case EState::Recover:
        // Back off a little, still facing him, then come again.
        if (Chuck)
        {
            FaceToward(Chuck->GetActorLocation(), DeltaSeconds, 8.f);
            Move->MaxWalkSpeed = 60.f;
            AddMovementInput(-ToChuck.GetSafeNormal2D(), 1.f);
        }
        if (StateTime >= RecoverTime) SetState(EState::Chase);
        break;
    case EState::Hurt:
        if (StateTime >= .5f) SetState(EState::Chase);
        break;
    case EState::Dead:
        if (StateTime >= 2.2f) { Destroy(); return; }
        break;
    }
    UpdatePose(DeltaSeconds);
}

void AEnemyRat::UpdatePose(float DeltaSeconds)
{
    if (!Body->GetSkinnedAsset() || BoneIndex.Contains(INDEX_NONE)) return;
    const float Now = GetWorld()->GetTimeSeconds();
    const float Speed = static_cast<float>(GetVelocity().Size2D());
    const bool bGround = GetCharacterMovement()->IsMovingOnGround();
    const float Amp = State == EState::Dead ? 0.f : FMath::Clamp(Speed / 120.f, 0.f, 1.f);
    // Trot phase from ground travel: a longer stride the faster it goes.
    if (bGround) GaitPhase = FMath::Frac(GaitPhase + Speed * DeltaSeconds / (10.f + .1f * Speed));
    auto Ease = [DeltaSeconds](float& Value, float Target, float Rate) { Value = FMath::FInterpTo(Value, Target, DeltaSeconds, Rate); };
    Ease(Crouch, State == EState::Windup ? 1.f : 0.f, State == EState::Windup ? 8.f : 12.f);
    Ease(Stretch, State == EState::Lunge ? 1.f : 0.f, 20.f);
    Ease(Flinch, State == EState::Hurt && StateTime < .3f ? 1.f : 0.f, 14.f);
    Ease(Sniff, State == EState::Roam && Speed < 5.f ? 1.f : 0.f, 4.f);
    Ease(DeathRoll, State == EState::Dead ? 1.f : 0.f, 9.f);
    const float W = UE_TWO_PI * GaitPhase;
    TArray<FQuat> Delta; Delta.Init(FQuat::Identity, BoneCount);
    // Body: a bob and a sway with the stride; a crouch before the lunge, a
    // stretch in it, a twist when hit.
    Delta[Spine] = Pitch(3.f * Amp * FMath::Sin(2 * W) + 8.f * Crouch - 7.f * Stretch) * Yaw(4.f * Amp * FMath::Sin(W) + 22.f * Flinch);
    Delta[Chest] = Yaw(-5.f * Amp * FMath::Sin(W) - 10.f * Flinch);
    const float Look = (1.f - Amp) * (1.f - Crouch) * 12.f * FMath::Sin(Now * .9f + GetUniqueID());
    Delta[Neck] = Yaw(Look);
    Delta[Head] = Pitch(-2.f * Amp * FMath::Sin(2 * W) + 5.f * Sniff * FMath::Sin(Now * UE_TWO_PI * 7.f) - 12.f * Crouch - 14.f * Stretch + 10.f * Flinch);
    // Legs: diagonal pairs (front-left with hind-right). The lower joints fold
    // on the forward swing so the paws clear the ground.
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const float Front = W + (Side == 0 ? 0.f : PI);
        const float Hind = W + (Side == 0 ? PI : 0.f);
        const int32 Arm = Side == 0 ? ArmL : ArmR, Forearm = Side == 0 ? ForearmL : ForearmR;
        const int32 Thigh = Side == 0 ? ThighL : ThighR, Shin = Side == 0 ? ShinL : ShinR, Foot = Side == 0 ? FootL : FootR;
        Delta[Arm] = Pitch(-28.f * Amp * FMath::Cos(Front) - 10.f * Crouch - 55.f * Stretch - 25.f * DeathRoll);
        Delta[Forearm] = Pitch(35.f * Amp * FMath::Max(0.f, FMath::Sin(Front)) + 25.f * Crouch - 10.f * Stretch + 40.f * DeathRoll);
        Delta[Thigh] = Pitch(-25.f * Amp * FMath::Cos(Hind) - 20.f * Crouch + 40.f * Stretch - 30.f * DeathRoll);
        Delta[Shin] = Pitch(-30.f * Amp * FMath::Max(0.f, FMath::Sin(Hind)) - 30.f * Crouch + 20.f * Stretch);
        Delta[Foot] = Pitch(20.f * Amp * FMath::Max(0.f, FMath::Sin(Hind)));
    }
    // Tail: a lazy wave travelling down it, lifted a touch when it runs,
    // lashing when it winds up.
    const float Lash = 1.f + 2.5f * Crouch;
    for (int32 I = 0; I < 6; ++I)
        Delta[Tail0 + I] = Yaw(7.f * FMath::Sin(Now * 3.f * Lash - I * .6f) * (.4f + .6f * Amp + .5f * Crouch)) * Pitch(I == 0 ? 8.f * Amp + 6.f * Crouch : 0.f);
    // Compose on the reference pose, parent first, in component space.
    const FReferenceSkeleton& Ref = Body->GetSkinnedAsset()->GetRefSkeleton();
    const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
    const int32 Count = Ref.GetNum();
    TArray<FTransform> Space; Space.SetNum(Count);
    TArray<int32> Which; Which.Init(INDEX_NONE, Count);
    for (int32 I = 0; I < BoneCount; ++I) Which[BoneIndex[I]] = I;
    for (int32 B = 0; B < Count; ++B)
    {
        const int32 Parent = Ref.GetParentIndex(B);
        Space[B] = Parent >= 0 ? RefPose[B] * Space[Parent] : RefPose[B];
        if (Which[B] != INDEX_NONE) Space[B].SetRotation(Delta[Which[B]] * Space[B].GetRotation());
        Body->SetBoneTransformByName(Ref.GetBoneName(B), Space[B], EBoneSpaces::ComponentSpace);
    }
    Body->MarkRefreshTransformDirty();
    // Whole-body offsets: crouch down, recoil, the death roll onto its side, then sink away.
    const float Sink = State == EState::Dead ? FMath::Clamp((StateTime - 1.5f) / .6f, 0.f, 1.f) : 0.f;
    Body->SetRelativeLocation(FVector(-3.f * Flinch, 0, -HalfHeight - 2.5f * Crouch - 5.f * Sink + 3.f * DeathRoll));
    Body->SetRelativeRotation(FRotator(0, 0, 95.f * DeathRoll));
    Body->SetRelativeScale3D(FVector(Size * (1.f - Sink)));
}

AEnemyRat* AEnemyRat::Place(UWorld* World, const FVector2D& At, float Yaw)
{
    FHitResult Hit;
    const FVector Top(At.X, At.Y, 400.f);
    if (!World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 520.f), ECC_Visibility) || Hit.ImpactNormal.Z < .9f) return nullptr;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    return World->SpawnActor<AEnemyRat>(Hit.ImpactPoint + FVector(0, 0, HalfHeight + 1.f), FRotator(0, Yaw, 0), Params);
}

AEnemyRat* AEnemyRat::PlaceAt(UWorld* World, const FVector& Ground, float Yaw)
{
    FHitResult Hit;
    const FVector Top = Ground + FVector(0, 0, 60.f);
    if (!World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 150.f), ECC_Visibility) || Hit.ImpactNormal.Z < .8f) return nullptr;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    return World->SpawnActor<AEnemyRat>(Hit.ImpactPoint + FVector(0, 0, HalfHeight + 1.f), FRotator(0, Yaw, 0), Params);
}

void AEnemyRat::SpawnDockRats(UWorld* World)
{
    // Away from the start, where there's room to deal with them: the cargo
    // wharf, the timber yard and the Chandlers' Row garden.
    const FVector2D Spots[] = { {0, -650}, {-300, -860}, {500, -600}, {760, -840}, {265, -1330} };
    int32 Placed = 0;
    for (const FVector2D& Spot : Spots)
        if (AEnemyRat* Rat = Place(World, Spot, FMath::FRandRange(0.f, 360.f))) { Rat->Cigarettes = 1 + (Placed % 2); ++Placed; }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_RATS_PLACED %d"), Placed);
}
