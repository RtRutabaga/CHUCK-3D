#include "ChultRaptor.h"
#include "ChuckCharacter.h"
#include "ChultCanopy.h"
#include "ChultLanding.h"
#include "RaptorClipData.h"
#include "DinoAnim.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    TArray<TWeakObjectPtr<AChultRaptor>> RaptorRegistry;
    constexpr float HalfHeight = 70.f, Radius = 42.f;
}

const TArray<TWeakObjectPtr<AChultRaptor>>& AChultRaptor::All() { return RaptorRegistry; }

const TArray<FVector>& AChultRaptor::PatrolPoints()
{
    // Between the loop's trees and out along the staggered line, weaving round
    // its trunks: every leg keeps at least 5.2 m from every trunk (clear of the
    // buttresses). Ping-ponged.
    static const TArray<FVector> Points={{6100,700,0},{6950,1600,0},{8650,2620,0},{9550,2834,0},{9700,3725,0},{9802,4550,0},
        {10500,4975,0},{11216,5430,0},{11175,6175,0}};
    return Points;
}

AChultRaptor::AChultRaptor()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(Radius, HalfHeight);
    auto* Move = GetCharacterMovement();
    Move->bRunPhysicsWithNoController = true;   // driven by its own Tick, no AI controller
    Move->MaxWalkSpeed = PatrolSpeed;
    Move->MaxAcceleration = 1800;
    Move->BrakingDecelerationWalking = 1600;
    Move->MaxStepHeight = 40;
    Move->SetWalkableFloorAngle(44);
    bUseControllerRotationYaw = false;
    GetMesh()->SetRelativeLocation(FVector(0, 0, -HalfHeight));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetAnimInstanceClass(UDinoAnimInstance::StaticClass());
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> RaptorMesh(TEXT("/Game/Characters/Raptor/SK_Raptor.SK_Raptor"));
    GetMesh()->SetSkinnedAssetAndUpdate(RaptorMesh.Object);
    // The clips by hard reference: loaded by path alone, a packaged game would not contain them.
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/Raptor/Animations/AS_Raptor_Idle.AS_Raptor_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/Characters/Raptor/Animations/AS_Raptor_Walk.AS_Raptor_Walk"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> RunClip(TEXT("/Game/Characters/Raptor/Animations/AS_Raptor_Run.AS_Raptor_Run"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack(TEXT("/Game/Characters/Raptor/Animations/AS_Raptor_Attack.AS_Raptor_Attack"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Jump(TEXT("/Game/Characters/Raptor/Animations/AS_Raptor_Jump.AS_Raptor_Jump"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Death(TEXT("/Game/Characters/Raptor/Animations/AS_Raptor_Death.AS_Raptor_Death"));
    Clips.Add(TEXT("Idle"), Idle.Object); Clips.Add(TEXT("Walk"), Walk.Object); Clips.Add(TEXT("Run"), RunClip.Object);
    Clips.Add(TEXT("Attack"), Attack.Object); Clips.Add(TEXT("Jump"), Jump.Object); Clips.Add(TEXT("Death"), Death.Object);
}

void AChultRaptor::BeginPlay()
{
    Super::BeginPlay();
    RaptorRegistry.Add(this);
    // Its cycles, measured in the engine (24 fps): the walk is one stride
    // (frames 0-28: each foot planted 81-87 cm) and then a second of standing,
    // which looped made it glide; the run is frames 0-13. Strides: ground covered per cycle.
    FDinoGait Gait;
    Gait.IdlePeriod = RaptorClipData::IdleLength;
    Gait.WalkEnd = Gait.WalkPeriod = 28.f / 24.f; Gait.WalkStride = 168.f;
    Gait.RunEnd = Gait.RunPeriod = RaptorClipData::RunLength; Gait.RunStride = RaptorClipData::RunGroundSpeed * Gait.RunPeriod;
    Gait.RunFrom = 170.f; Gait.RunTo = 300.f;
    Gait.FadeTime = .2f; Gait.IKUp = 25.f; Gait.IKDown = 35.f; Gait.FootRestZ = 2.6f; Gait.TraceUp = 60.f;
    Animator.Init(GetMesh(), Clips, Gait);
    Play(TEXT("Idle"), true, 1.f, true);
    // Synthetic sounds (Tools/gen_raptor_sfx.py), heard well across the trees.
    auto Load = [](TArray<USoundBase*>& Set, const TCHAR* Stem, int32 Count)
    {
        for (int32 I = 0; I < Count; ++I)
            if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Stem, I, Stem, I))) Set.Add(S);
    };
    Load(StepSounds, TEXT("SFX_RaptorStep"), 3); Load(HissSounds, TEXT("SFX_RaptorHiss"), 1); Load(SnapSounds, TEXT("SFX_RaptorSnap"), 2);
    Load(CallSounds, TEXT("SFX_RaptorCall"), 1); Load(DeathSounds, TEXT("SFX_RaptorDeath"), 1);
    Attenuation = NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate = true;
    Attenuation->Attenuation.bSpatialize = true;
    Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Attenuation->Attenuation.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
    Attenuation->Attenuation.FalloffDistance = 2500.f;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_RAPTOR_SOUNDS steps=%d hiss=%d snap=%d call=%d death=%d"), StepSounds.Num(), HissSounds.Num(), SnapSounds.Num(), CallSounds.Num(), DeathSounds.Num());
    int32 Loaded = 0;
    for (const auto& Clip : Clips) Loaded += Clip.Value != nullptr;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_RAPTOR_SPAWN at=%s clips=%d mesh=%d"), *(GetActorLocation() - AChultLanding::FloorOrigin()).ToString(), Loaded, GetMesh()->GetSkinnedAsset() != nullptr);
}

void AChultRaptor::EndPlay(const EEndPlayReason::Type Reason)
{
    RaptorRegistry.RemoveAll([this](const TWeakObjectPtr<AChultRaptor>& R) { return !R.IsValid() || R.Get() == this; });
    Super::EndPlay(Reason);
}

AChultRaptor* AChultRaptor::SpawnCanopyRaptor(UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AChultRaptor> It(World); It; ++It) return *It;
    // The traversal tests assume nothing hunts him; the raptor has its own test.
    const TCHAR* Cmd = FCommandLine::Get();
    if ((FParse::Param(Cmd, TEXT("ChuckChultTest")) || FParse::Param(Cmd, TEXT("ChuckCanopyTest"))) && !FParse::Param(Cmd, TEXT("ChuckRaptorTest"))) return nullptr;
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    const FVector At = AChultLanding::FloorOrigin() + PatrolPoints()[1] + FVector(0, 0, HalfHeight + 40);
    return World->SpawnActor<AChultRaptor>(At, FRotator(0, 30, 0), Params);
}

const TCHAR* AChultRaptor::GetStateName() const
{
    switch (State)
    {
    case EState::Patrol: return TEXT("Patrol");
    case EState::Alert: return TEXT("Alert");
    case EState::Chase: return TEXT("Chase");
    case EState::Windup: return TEXT("Windup");
    case EState::Lunge: return TEXT("Lunge");
    case EState::Recover: return TEXT("Recover");
    case EState::Prowl: return TEXT("Prowl");
    case EState::Return: return TEXT("Return");
    case EState::Hurt: return TEXT("Hurt");
    default: return TEXT("Dead");
    }
}

void AChultRaptor::SetState(EState To)
{
    if (State == EState::Windup) LastWindup = StateTime;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_RAPTOR_STATE from=%s to=%s after=%.2f at=%s"), GetStateName(),
        To == EState::Patrol ? TEXT("Patrol") : To == EState::Alert ? TEXT("Alert") : To == EState::Chase ? TEXT("Chase") : To == EState::Windup ? TEXT("Windup")
        : To == EState::Lunge ? TEXT("Lunge") : To == EState::Recover ? TEXT("Recover") : To == EState::Prowl ? TEXT("Prowl") : To == EState::Return ? TEXT("Return")
        : To == EState::Hurt ? TEXT("Hurt") : TEXT("Dead"), StateTime, *(GetActorLocation() - AChultLanding::FloorOrigin()).ToString());
    State = To; StateTime = 0;
    if (To == EState::Prowl) { bSnapping = false; NextSnap = 1.f; }
    if (To == EState::Alert) Sound(CallSounds, 1.f);
    else if (To == EState::Windup) Sound(HissSounds, .85f);
    else if (To == EState::Dead) Sound(DeathSounds, 1.f);
}

void AChultRaptor::Sound(const TArray<USoundBase*>& Set, float Volume)
{
    if (!Set.Num()) return;
    UGameplayStatics::PlaySoundAtLocation(this, Set[FMath::RandRange(0, Set.Num() - 1)], GetActorLocation(), Volume, FMath::FRandRange(.94f, 1.06f), 0.f, Attenuation);
    ++SoundsPlayed;
}

void AChultRaptor::Play(FName Clip, bool bLoop, float Rate, bool bRestart)
{
    Animator.Play(Clip, bLoop, Rate, bRestart);
}

void AChultRaptor::Locomotion()
{
    // Walk blending into run by speed, the stride's phase kept by the distance covered (DinoAnim).
    Animator.Locomote(static_cast<float>(GetVelocity().Size2D()));
}

void AChultRaptor::Face(const FVector& Dir, float DegPerSecond, float DeltaSeconds)
{
    if (Dir.IsNearlyZero()) return;
    const float Yaw = FMath::FixedTurn(GetActorRotation().Yaw, Dir.Rotation().Yaw, DegPerSecond * DeltaSeconds);
    SetActorRotation(FRotator(0, Yaw, 0));
}

float AChultRaptor::FeetZ() const { return static_cast<float>(GetActorLocation().Z) - HalfHeight; }

bool AChultRaptor::CanReach(const AChuckCharacter* Chuck) const
{
    if (!Chuck || Chuck->IsAstral() || Chuck->IsSwinging() || Chuck->IsSwingLeaping()) return false;
    const float ChuckFeet = static_cast<float>(Chuck->GetActorLocation().Z) - Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    return ChuckFeet - FeetZ() < ReachHeight;
}

bool AChultRaptor::InTerritory(const FVector& World) const
{
    return InChultCanopyReserve(World - AChultLanding::FloorOrigin(), Territory);
}

bool AChultRaptor::Sees(const AChuckCharacter* Chuck) const
{
    const FVector Eye = GetActorLocation() + FVector(0, 0, 70) + GetActorForwardVector() * 120;
    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(RaptorSight), false, this);
    Q.AddIgnoredActor(Chuck);
    return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Chuck->GetActorLocation() + FVector(0, 0, 10), ECC_Visibility, Q);
}

FVector AChultRaptor::Steer(const FVector& Want, const AActor* Ignore) const
{
    // Round trunks, buttresses and rocks: the first clear heading near the one wanted.
    const FVector Dir = Want.GetSafeNormal2D();
    if (Dir.IsNearlyZero()) return Dir;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(RaptorSteer), false, this);
    if (Ignore) Q.AddIgnoredActor(Ignore);
    for (const TWeakObjectPtr<AChultRaptor>& R : RaptorRegistry) if (R.IsValid()) Q.AddIgnoredActor(R.Get());
    const FVector From = GetActorLocation() + FVector(0, 0, 15);
    for (const float Turn : {0.f, 35.f, -35.f, 70.f, -70.f, 110.f, -110.f})
    {
        const FVector Try = Dir.RotateAngleAxis(Turn, FVector::UpVector);
        FHitResult Hit;
        if (!GetWorld()->SweepSingleByChannel(Hit, From, From + Try * 170.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius - 4.f), Q)
            || Hit.ImpactNormal.Z > .7f) return Try;
    }
    return Dir;
}

void AChultRaptor::ResetTo(const FVector& At, float Yaw)
{
    SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
    SetActorRotation(FRotator(0, Yaw, 0));
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    HitsTaken = 0; NoticeCooldown = 0; OutOfReach = 0; SightLost = 0; bSnapping = false;
    float Best = 1e9f;
    for (int32 I = 0; I < PatrolPoints().Num(); ++I)
    {
        const float D = static_cast<float>(FVector::Dist2D(At - AChultLanding::FloorOrigin(), PatrolPoints()[I]));
        if (D < Best) { Best = D; Waypoint = I; }
    }
    SetState(EState::Patrol);
    Play(TEXT("Idle"), true, 1.f, true);
}

void AChultRaptor::TakeSlash(const FVector& Swing)
{
    if (State == EState::Dead) return;
    ++HitsTaken;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_RAPTOR_HIT hits=%d of=%d"), HitsTaken, Health);
    if (HitsTaken >= Health)
    {
        SetState(EState::Dead);
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Play(TEXT("Death"), false, 1.f, true);
        return;
    }
    Knock = Swing.GetSafeNormal2D() * 260.f;
    SetState(EState::Hurt);
}

void AChultRaptor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Animator.Tick(DeltaSeconds, GetVelocity(), GetCharacterMovement()->IsMovingOnGround() && State != EState::Dead);
    if (State == EState::Dead) return;
    StateTime += DeltaSeconds;
    NoticeCooldown = FMath::Max(0.f, NoticeCooldown - DeltaSeconds);
    auto* Move = GetCharacterMovement();
    TopSpeed = FMath::Max(TopSpeed, static_cast<float>(GetVelocity().Size2D()));
    // Footfalls: one per half stride of whichever gait it is in.
    {
        const float Speed = static_cast<float>(GetVelocity().Size2D());
        StepDistance += Speed * DeltaSeconds;
        const float HalfStride = Speed < 230.f ? RaptorClipData::WalkGroundSpeed * RaptorClipData::WalkLength * .5f : RaptorClipData::RunGroundSpeed * RaptorClipData::RunLength * .5f;
        if (Speed > 20.f && Move->IsMovingOnGround() && StepDistance > HalfStride) { StepDistance = 0; Sound(StepSounds, Speed < 230.f ? .5f : .8f); }
    }
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const FVector Here = GetActorLocation();
    const FVector ToChuck = Chuck ? Chuck->GetActorLocation() - Here : FVector::ZeroVector;
    const float Dist = static_cast<float>(ToChuck.Size2D());
    const bool bReach = CanReach(Chuck);
    const bool bTheirs = Chuck && !Chuck->IsAstral() && InTerritory(Chuck->GetActorLocation());

    // Noticing him, from its round (in view, or close by all round).
    auto Notices = [&]()
    {
        if (!bTheirs || NoticeCooldown > 0 || Dist > NoticeRange) return false;
        const bool bInView = FVector::DotProduct(ToChuck.GetSafeNormal2D(), GetActorForwardVector()) > .2f || Dist < CloseNotice;
        // Close by it senses him without seeing him (a limb over its head hides him from view).
        return (Dist < CloseNotice || (bInView && Sees(Chuck))) && (bReach || Dist < CloseNotice * 1.6f);
    };
    auto Walk = [&](const FVector& Target, float Speed, float TurnRate)
    {
        Move->MaxWalkSpeed = Speed;
        const FVector Want = Target - Here;
        const FVector Dir = Steer(Want, Chuck);
        AddMovementInput(Dir, 1.f);
        Face(GetVelocity().Size2D() > 20.f ? GetVelocity() : Dir, TurnRate, DeltaSeconds);
    };

    switch (State)
    {
    case EState::Patrol:
    {
        const FVector Target = AChultLanding::FloorOrigin() + PatrolPoints()[Waypoint];
        if (FVector::Dist2D(Here, Target) < 90.f)
        {
            // A pause at each point, looking about; then on (back the way it came at the ends).
            Move->MaxWalkSpeed = 0;
            if (StateTime > 1.6f)
            {
                if (Waypoint + WaypointStep < 0 || Waypoint + WaypointStep >= PatrolPoints().Num()) WaypointStep = -WaypointStep;
                Waypoint += WaypointStep; StateTime = 0;
            }
        }
        else
        {
            Walk(Target, PatrolSpeed, 160.f);
            StateTime = FMath::Min(StateTime, 0.f);
            // Wedged against something on its round: on to the next point.
            StuckTime = GetVelocity().Size2D() < 20.f ? StuckTime + DeltaSeconds : 0.f;
            if (StuckTime > 2.f) { StuckTime = 0; WaypointStep = -WaypointStep; Waypoint = FMath::Clamp(Waypoint + WaypointStep, 0, PatrolPoints().Num() - 1); }
        }
        Locomotion();
        if (Notices()) SetState(EState::Alert);
        break;
    }
    case EState::Alert:
        // A beat to turn and fix on him (and for him to see it has).
        Move->MaxWalkSpeed = 0;
        Face(ToChuck, 420.f, DeltaSeconds);
        Play(TEXT("Idle"), true, 1.6f);
        if (StateTime > .45f) SetState(bReach ? EState::Chase : EState::Prowl);
        break;
    case EState::Chase:
    {
        if (!bTheirs || Dist > LoseRange) { SetState(EState::Return); break; }
        SightLost = Sees(Chuck) ? 0.f : SightLost + DeltaSeconds;
        if (SightLost > LoseSightTime) { SightLost = 0; SetState(EState::Return); break; }
        if (!bReach) { OutOfReach = 0; SetState(EState::Prowl); break; }
        Walk(Chuck->GetActorLocation(), ChaseSpeed, 300.f);
        Locomotion();
        if (Dist < StrikeRange) { Play(TEXT("Attack"), false, .75f, true); SetState(EState::Windup); }
        break;
    }
    case EState::Windup:
        // The tell: it stops, crouches into the strike, fixed on him.
        Move->MaxWalkSpeed = 0;
        Face(ToChuck, 500.f, DeltaSeconds);
        if (StateTime >= WindupTime)
        {
            LungeDir = GetActorForwardVector().GetSafeNormal2D();
            bBit = false; ++Lunges;
            SetState(EState::Lunge);
        }
        break;
    case EState::Lunge:
    {
        Move->MaxWalkSpeed = LungeSpeed;
        Move->Velocity = FVector(LungeDir.X * LungeSpeed, LungeDir.Y * LungeSpeed, Move->Velocity.Z);
        // The jaws close at the clip's reach (its 0.375 s, played at 0.75).
        if (!bBit && WindupTime + StateTime >= RaptorClipData::AttackReachMoment / .75f)
        {
            bBit = true;
            Sound(SnapSounds, 1.f);
            const FVector Snout = Here + LungeDir * (RaptorClipData::SnoutXCm - 20.f);
            if (Chuck && bReach && FVector::Dist2D(Snout, Chuck->GetActorLocation()) < BiteReach && Chuck->TakeBite(Here, BiteDamage))
            {
                ++Bites;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_RAPTOR_BITE bites=%d sanity=%d"), Bites, Chuck->GetSanity());
            }
        }
        if (StateTime >= LungeTime) SetState(EState::Recover);
        break;
    }
    case EState::Recover:
        Move->MaxWalkSpeed = 0;
        Face(ToChuck, 200.f, DeltaSeconds);
        if (StateTime > .4f) Play(TEXT("Idle"), true, 1.2f);
        if (StateTime >= RecoverTime) SetState(bTheirs && bReach ? EState::Chase : bTheirs ? EState::Prowl : EState::Return);
        break;
    case EState::Prowl:
    {
        // He is up out of reach: circle under him, head up, snapping; then give up.
        if (!bTheirs || Dist > LoseRange) { SetState(EState::Return); break; }
        if (bReach) { SetState(EState::Chase); break; }
        OutOfReach += DeltaSeconds;
        if (OutOfReach > Patience) { NoticeCooldown = 5.f; SetState(EState::Return); break; }
        if (bSnapping)
        {
            Move->MaxWalkSpeed = 0;
            Face(ToChuck, 400.f, DeltaSeconds);
            if (StateTime >= NextSnap) { bSnapping = false; NextSnap = StateTime + SnapEvery; }
            break;
        }
        ProwlAngle += .55f * DeltaSeconds;
        const FVector Around = Chuck->GetActorLocation() + FVector(FMath::Cos(ProwlAngle), FMath::Sin(ProwlAngle), 0) * 220.f;
        Walk(Around, ProwlSpeed, 240.f);
        Locomotion();
        if (Dist < 450.f && StateTime >= NextSnap)
        {
            // A leap and a snap up at him (the jump clip, on the spot).
            bSnapping = true; ++Snaps;
            Play(TEXT("Jump"), false, 1.3f, true);
            NextSnap = StateTime + RaptorClipData::JumpLength / 1.3f;   // (while snapping: when it lands)
            UE_LOG(LogTemp, Display, TEXT("CHUCK_RAPTOR_SNAP snaps=%d"), Snaps);
            Sound(SnapSounds, .9f);
        }
        break;
    }
    case EState::Return:
    {
        // Back to the nearest point of its round.
        float Best = 1e9f;
        for (int32 I = 0; I < PatrolPoints().Num(); ++I)
        {
            const float D = static_cast<float>(FVector::Dist2D(Here - AChultLanding::FloorOrigin(), PatrolPoints()[I]));
            if (D < Best) { Best = D; Waypoint = I; }
        }
        Walk(AChultLanding::FloorOrigin() + PatrolPoints()[Waypoint], ReturnSpeed, 180.f);
        Locomotion();
        if (Best < 120.f) SetState(EState::Patrol);
        else if (StateTime > 1.5f && Notices() && bReach) SetState(EState::Alert);
        break;
    }
    case EState::Hurt:
        // A flinch back from the scratch; then straight back at him.
        Move->MaxWalkSpeed = 0;
        Move->Velocity = FVector(Knock.X, Knock.Y, Move->Velocity.Z) * FMath::Max(0.f, 1.f - StateTime / .35f);
        Play(TEXT("Idle"), true, 2.f);
        if (StateTime >= .45f) SetState(bTheirs && bReach ? EState::Chase : EState::Return);
        break;
    default: break;
    }
}
