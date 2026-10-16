#include "ChultTRex.h"
#include "ChuckCharacter.h"
#include "ChultCanyon.h"
#include "ChultCanopy.h"
#include "ChultLanding.h"
#include "ChultUpriver.h"
#include "TRexClipData.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

// The giants' T-rex (Claude, 2026-10-15; docs/CHULT-TREX.md). Built like the
// canopy raptor (ChultRaptor.cpp): no AI controller, it drives its own
// character movement from Tick and steers round solids with sweeps.
namespace
{
    TArray<TWeakObjectPtr<AChultTRex>> TRexRegistry;
    // Its body is 12 m long; the capsule is its legs and belly (the head and
    // tail carry no collision), 5.8 m tall like the model.
    constexpr float HalfHeight = 290.f, Radius = 150.f;
    const FVector Along(.8,.6,0),Across(-.6,.8,0);
    float FrameS(const FVector& World){return FVector::DotProduct(World-AChultLanding::FloorOrigin()-ChultCanyonFrame(0,0),Along);}
    FVector Ground(float S,float T){const FVector P=ChultCanyonFrame(S,T);return FVector(P.X,P.Y,ChultCanyonGroundZ(P));}
    bool Suppressed()
    {
        const TCHAR* Cmd=FCommandLine::Get();
        if(FParse::Param(Cmd,TEXT("ChuckTRexTest")))return false;
        TArray<FString> Words;FString(Cmd).ParseIntoArrayWS(Words);
        for(const FString& Word:Words)
            if(Word.StartsWith(TEXT("-Chuck"))&&(Word.Contains(TEXT("Test"))||Word.Contains(TEXT("Capture"))||Word.Contains(TEXT("Review"))))return true;
        return false;
    }
}

const TArray<TWeakObjectPtr<AChultTRex>>& AChultTRex::All() { return TRexRegistry; }
bool AChultTRex::InTerritory(const FVector& World)
{
    const FVector L=World-AChultLanding::FloorOrigin()-ChultCanyonFrame(0,0);
    const float S=FVector::DotProduct(L,Along),T=FVector::DotProduct(L,Across);
    return S>=TerritoryFrom&&S<=TerritoryTo&&FMath::Abs(T)<=6500;
}

const TArray<FVector>& AChultTRex::PatrolPoints()
{
    // A loop round the giants: up the road's side of their line 12.5 m off it,
    // across between the last two trees (its stretch ends before the last
    // tree's liana), back down the far side, across before the first.
    static const TArray<FVector> Points=[]()
    {
        TArray<FVector> P;P.Add(Ground(35700,ChultGiantLineT(35700)));
        for(int32 I=0;I<11;++I){const float S=36600+1300*I+650;P.Add(Ground(S,ChultGiantLineT(S)-1250));}
        P.Add(Ground(50250,ChultGiantLineT(50250)));
        for(int32 I=10;I>=0;--I){const float S=36600+1300*I+650;P.Add(Ground(S,ChultGiantLineT(S)+1250));}
        return P;
    }();
    return Points;
}

AChultTRex::AChultTRex()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(Radius, HalfHeight);
    auto* Move = GetCharacterMovement();
    Move->bRunPhysicsWithNoController = true;
    Move->MaxWalkSpeed = PatrolSpeed;
    Move->MaxAcceleration = 900;
    Move->BrakingDecelerationWalking = 900;
    Move->MaxStepHeight = 45;
    Move->SetWalkableFloorAngle(44);
    bUseControllerRotationYaw = false;
    GetMesh()->SetRelativeLocation(FVector(0, 0, -HalfHeight));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> TRexMesh(TEXT("/Game/Characters/TRex/SK_TRex.SK_TRex"));
    GetMesh()->SetSkinnedAssetAndUpdate(TRexMesh.Object);
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/TRex/Animations/AS_TRex_Idle.AS_TRex_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/Characters/TRex/Animations/AS_TRex_Walk.AS_TRex_Walk"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> RunClip(TEXT("/Game/Characters/TRex/Animations/AS_TRex_Run.AS_TRex_Run"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack(TEXT("/Game/Characters/TRex/Animations/AS_TRex_Attack.AS_TRex_Attack"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Jump(TEXT("/Game/Characters/TRex/Animations/AS_TRex_Jump.AS_TRex_Jump"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Death(TEXT("/Game/Characters/TRex/Animations/AS_TRex_Death.AS_TRex_Death"));
    Clips.Add(TEXT("Idle"), Idle.Object); Clips.Add(TEXT("Walk"), Walk.Object); Clips.Add(TEXT("Run"), RunClip.Object);
    Clips.Add(TEXT("Attack"), Attack.Object); Clips.Add(TEXT("Jump"), Jump.Object); Clips.Add(TEXT("Death"), Death.Object);
}

void AChultTRex::BeginPlay()
{
    Super::BeginPlay();
    TRexRegistry.Add(this);
    Play(TEXT("Idle"), true, 1.f, true);
    // Synthetic sounds (Tools/gen_trex_sfx.py), heard far across the valley.
    auto Load = [](TArray<USoundBase*>& Set, const TCHAR* Stem, int32 Count)
    {
        for (int32 I = 0; I < Count; ++I)
            if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Stem, I, Stem, I))) Set.Add(S);
    };
    Load(StepSounds, TEXT("SFX_TRexStep"), 3); Load(RoarSounds, TEXT("SFX_TRexRoar"), 2); Load(SnapSounds, TEXT("SFX_TRexSnap"), 1); Load(GrowlSounds, TEXT("SFX_TRexGrowl"), 1);
    Attenuation = NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate = true;
    Attenuation->Attenuation.bSpatialize = true;
    Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Attenuation->Attenuation.AttenuationShapeExtents = FVector(1200.f, 0.f, 0.f);
    Attenuation->Attenuation.FalloffDistance = 6000.f;
    int32 Loaded = 0;
    for (const auto& Clip : Clips) Loaded += Clip.Value != nullptr;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TREX_SOUNDS steps=%d roar=%d snap=%d growl=%d"), StepSounds.Num(), RoarSounds.Num(), SnapSounds.Num(), GrowlSounds.Num());
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TREX_SPAWN at=%s clips=%d mesh=%d"), *(GetActorLocation() - AChultLanding::FloorOrigin()).ToString(), Loaded, GetMesh()->GetSkinnedAsset() != nullptr);
}

void AChultTRex::EndPlay(const EEndPlayReason::Type Reason)
{
    TRexRegistry.RemoveAll([this](const TWeakObjectPtr<AChultTRex>& R) { return !R.IsValid() || R.Get() == this; });
    Super::EndPlay(Reason);
}

AChultTRex* AChultTRex::SpawnUpriver(UWorld* World)
{
    if (!World || Suppressed()) return nullptr;
    for (TActorIterator<AChultTRex> It(World); It; ++It) return *It;
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    const FVector At = AChultLanding::FloorOrigin() + PatrolPoints()[6] + FVector(0, 0, HalfHeight + 40);
    auto* Rex = World->SpawnActor<AChultTRex>(At, Along.Rotation(), Params);
    if (Rex) Rex->Waypoint = 7;
    return Rex;
}

const TCHAR* AChultTRex::GetStateName() const
{
    switch (State)
    {
    case EState::Patrol: return TEXT("Patrol");
    case EState::Roar: return TEXT("Roar");
    case EState::Chase: return TEXT("Chase");
    case EState::Windup: return TEXT("Windup");
    case EState::Lunge: return TEXT("Lunge");
    case EState::Recover: return TEXT("Recover");
    case EState::Stalk: return TEXT("Stalk");
    default: return TEXT("Return");
    }
}

void AChultTRex::SetState(EState To)
{
    const TCHAR* From = GetStateName();
    State = To; StateTime = 0;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TREX_STATE from=%s to=%s at=%s"), From, GetStateName(), *(GetActorLocation() - AChultLanding::FloorOrigin()).ToString());
    if (To == EState::Roar) { Sound(RoarSounds, 1.f); ++Roars; }
    if (To == EState::Stalk) { bSnapping = false; NextSnap = 1.5f; NextGrowl = .5f; }
}

void AChultTRex::Sound(const TArray<USoundBase*>& Set, float Volume)
{
    if (!Set.Num()) return;
    UGameplayStatics::PlaySoundAtLocation(this, Set[FMath::RandRange(0, Set.Num() - 1)], GetActorLocation(), Volume, FMath::FRandRange(.94f, 1.04f), 0.f, Attenuation);
    ++SoundsPlayed;
}

void AChultTRex::Play(FName Clip, bool bLoop, float Rate, bool bRestart)
{
    UAnimSequence** Anim = Clips.Find(Clip);
    if (!Anim || !*Anim) return;
    if (Playing != Clip || bRestart) { GetMesh()->PlayAnimation(*Anim, bLoop); Playing = Clip; }
    GetMesh()->SetPlayRate(Rate);
}

void AChultTRex::Locomotion()
{
    // Its long stride: the walk carries it up to its chase, feet planted (4.3 m/s natural).
    const float Speed = static_cast<float>(GetVelocity().Size2D());
    if (Speed < 15.f) Play(TEXT("Idle"), true, 1.f);
    else if (Speed < 600.f) Play(TEXT("Walk"), true, FMath::Clamp(Speed / TRexClipData::WalkGroundSpeed, .3f, 1.3f));
    else Play(TEXT("Run"), true, FMath::Clamp(Speed / TRexClipData::RunGroundSpeed, .5f, 1.2f));
}

void AChultTRex::Face(const FVector& Dir, float DegPerSecond, float DeltaSeconds)
{
    if (Dir.IsNearlyZero()) return;
    SetActorRotation(FRotator(0, FMath::FixedTurn(GetActorRotation().Yaw, Dir.Rotation().Yaw, DegPerSecond * DeltaSeconds), 0));
}

float AChultTRex::FeetZ() const { return static_cast<float>(GetActorLocation().Z) - HalfHeight; }

bool AChultTRex::CanReach(const AChuckCharacter* Chuck) const
{
    if (!Chuck || Chuck->IsAstral() || Chuck->IsSwinging() || Chuck->IsSwingLeaping()) return false;
    const float ChuckFeet = static_cast<float>(Chuck->GetActorLocation().Z) - Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    return ChuckFeet - FeetZ() < ReachHeight;
}

bool AChultTRex::Sees(const AChuckCharacter* Chuck) const
{
    const FVector Eye = GetActorLocation() + FVector(0, 0, 200) + GetActorForwardVector() * 450;
    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(TRexSight), false, this);
    Q.AddIgnoredActor(Chuck);
    return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Chuck->GetActorLocation() + FVector(0, 0, 10), ECC_Visibility, Q);
}

FVector AChultTRex::Steer(const FVector& Want, const AActor* Ignore) const
{
    // Round trunks, buttresses and rocks: the first clear heading near the one wanted.
    const FVector Dir = Want.GetSafeNormal2D();
    if (Dir.IsNearlyZero()) return Dir;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(TRexSteer), false, this);
    if (Ignore) Q.AddIgnoredActor(Ignore);
    const FVector From = GetActorLocation() + FVector(0, 0, 160 - HalfHeight + Radius);
    for (const float Turn : {0.f, 25.f, -25.f, 50.f, -50.f, 80.f, -80.f, 115.f, -115.f})
    {
        const FVector Try = Dir.RotateAngleAxis(Turn, FVector::UpVector);
        FHitResult Hit;
        if (!GetWorld()->SweepSingleByChannel(Hit, From, From + Try * 450.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius - 10.f), Q)
            || Hit.ImpactNormal.Z > .7f) return Try;
    }
    return Dir;
}

int32 AChultTRex::NearestPoint() const
{
    int32 Best = 0; float D = 1e12f;
    for (int32 I = 0; I < PatrolPoints().Num(); ++I)
    {
        const float Di = static_cast<float>(FVector::Dist2D(GetActorLocation() - AChultLanding::FloorOrigin(), PatrolPoints()[I]));
        if (Di < D) { D = Di; Best = I; }
    }
    return Best;
}

void AChultTRex::ResetTo(const FVector& At, float Yaw)
{
    SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
    SetActorRotation(FRotator(0, Yaw, 0));
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    NoticeCooldown = 0; OutOfReach = 0; SightLost = 0; bSnapping = false;
    Waypoint = (NearestPoint() + 1) % PatrolPoints().Num();
    SetState(EState::Patrol);
    Play(TEXT("Idle"), true, 1.f, true);
}

void AChultTRex::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    StateTime += DeltaSeconds;
    NoticeCooldown = FMath::Max(0.f, NoticeCooldown - DeltaSeconds);
    auto* Move = GetCharacterMovement();
    const float Speed = static_cast<float>(GetVelocity().Size2D());
    TopSpeed = FMath::Max(TopSpeed, Speed);
    // Footfalls: one per half stride, heavy.
    StepDistance += Speed * DeltaSeconds;
    if (Speed > 20.f && Move->IsMovingOnGround() && StepDistance > TRexClipData::WalkGroundSpeed * TRexClipData::WalkLength * .5f)
    { StepDistance = 0; Sound(StepSounds, Speed > 300.f ? 1.f : .75f); }

    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const FVector Here = GetActorLocation();
    const FVector ToChuck = Chuck ? Chuck->GetActorLocation() - Here : FVector::ZeroVector;
    const float Dist = static_cast<float>(ToChuck.Size2D());
    const bool bReach = CanReach(Chuck);
    const bool bTheirs = Chuck && !Chuck->IsAstral() && InTerritory(Chuck->GetActorLocation());
    auto Notices = [&]()
    {
        if (!bTheirs || NoticeCooldown > 0 || Dist > NoticeRange) return false;
        const bool bInView = FVector::DotProduct(ToChuck.GetSafeNormal2D(), GetActorForwardVector()) > .1f;
        // Up a giant: it knows he is there when close, and stalks him beneath.
        return Dist < CloseNotice || (bInView && Sees(Chuck)) || (!bReach && Dist < CloseNotice * 1.6f);
    };
    auto Walk = [&](const FVector& Target, float WalkSpeed, float TurnRate)
    {
        Move->MaxWalkSpeed = WalkSpeed;
        FVector Goal = Target;
        // Never out of its stretch: hold at the edge.
        if (!InTerritory(Goal + FVector(0, 0, 0)))
        {
            const FVector L = Goal - AChultLanding::FloorOrigin() - ChultCanyonFrame(0, 0);
            const float S = FMath::Clamp(static_cast<float>(FVector::DotProduct(L, Along)), TerritoryFrom + 300.f, TerritoryTo - 300.f);
            Goal = AChultLanding::FloorOrigin() + ChultCanyonFrame(S, FVector::DotProduct(L, Across), L.Z);
        }
        const FVector Dir = Steer(Goal - Here, Chuck);
        if (FVector::Dist2D(Goal, Here) > 60.f) AddMovementInput(Dir, 1.f);
        Face(GetVelocity().Size2D() > 20.f ? GetVelocity() : Dir, TurnRate, DeltaSeconds);
    };

    switch (State)
    {
    case EState::Patrol:
    {
        const FVector Target = AChultLanding::FloorOrigin() + PatrolPoints()[Waypoint];
        if (FVector::Dist2D(Here, Target) < 200.f) { Waypoint = (Waypoint + 1) % PatrolPoints().Num(); StuckTime = 0; }
        Walk(Target, PatrolSpeed, 70.f);
        StuckTime = Speed < 20.f ? StuckTime + DeltaSeconds : 0.f;
        if (StuckTime > 3.f) { StuckTime = 0; Waypoint = (Waypoint + 1) % PatrolPoints().Num(); }
        Locomotion();
        if (Notices()) SetState(bReach ? EState::Roar : EState::Stalk);
        break;
    }
    case EState::Roar:
        // It turns on him and roars, rearing (the attack clip, slowed): his warning.
        Move->MaxWalkSpeed = 0;
        Face(ToChuck, 120.f, DeltaSeconds);
        if (StateTime < .05f) Play(TEXT("Attack"), false, .55f, true);
        if (StateTime > 1.3f) SetState(bReach ? EState::Chase : EState::Stalk);
        break;
    case EState::Chase:
    {
        if (!bTheirs || Dist > LoseRange) { NoticeCooldown = 3.f; SetState(EState::Return); break; }
        SightLost = Sees(Chuck) ? 0.f : SightLost + DeltaSeconds;
        if (SightLost > LoseSightTime) { SightLost = 0; SetState(EState::Return); break; }
        if (!bReach) { OutOfReach = 0; SetState(EState::Stalk); break; }
        Walk(Chuck->GetActorLocation(), ChaseSpeed, 110.f);
        Locomotion();
        if (Dist < StrikeRange) { Play(TEXT("Attack"), false, 1.f, true); SetState(EState::Windup); }
        break;
    }
    case EState::Windup:
        // The tell: it stops and rears back, fixed on him.
        Move->MaxWalkSpeed = 0;
        Face(ToChuck, 160.f, DeltaSeconds);
        if (StateTime >= WindupTime) { LungeDir = GetActorForwardVector().GetSafeNormal2D(); bBit = false; SetState(EState::Lunge); }
        break;
    case EState::Lunge:
    {
        Move->MaxWalkSpeed = LungeSpeed;
        Move->Velocity = FVector(LungeDir.X * LungeSpeed, LungeDir.Y * LungeSpeed, Move->Velocity.Z);
        if (!bBit && WindupTime + StateTime >= TRexClipData::AttackReachMoment)
        {
            bBit = true;
            Sound(SnapSounds, 1.f);
            const FVector Snout = Here + LungeDir * (TRexClipData::SnoutXCm - 60.f);
            // One bite empties his sanity: he dies.
            if (Chuck && CanReach(Chuck) && FVector::Dist2D(Snout, Chuck->GetActorLocation()) < BiteReach && Chuck->TakeBite(Here, 99))
            {
                ++Bites;
                UE_LOG(LogTemp, Display, TEXT("CHUCK_TREX_BITE bites=%d sanity=%d"), Bites, Chuck->GetSanity());
            }
        }
        if (StateTime >= LungeTime) SetState(EState::Recover);
        break;
    }
    case EState::Recover:
        Move->MaxWalkSpeed = 0;
        Face(ToChuck, 90.f, DeltaSeconds);
        if (StateTime > .6f) Play(TEXT("Idle"), true, 1.f);
        if (StateTime >= RecoverTime) SetState(bTheirs && bReach ? EState::Chase : bTheirs ? EState::Stalk : EState::Return);
        break;
    case EState::Stalk:
    {
        // He is up in the giants: it follows beneath him, growling, snapping up; then gives up.
        if (!bTheirs || Dist > LoseRange) { SetState(EState::Return); break; }
        if (bReach) { SetState(EState::Chase); break; }
        OutOfReach += DeltaSeconds;
        if (OutOfReach > Patience) { NoticeCooldown = 8.f; SetState(EState::Return); break; }
        if (StateTime >= NextGrowl) { Sound(GrowlSounds, .9f); NextGrowl = StateTime + FMath::FRandRange(3.5f, 6.f); }
        if (bSnapping)
        {
            Move->MaxWalkSpeed = 0;
            Face(ToChuck, 120.f, DeltaSeconds);
            if (StateTime >= NextSnap) { bSnapping = false; NextSnap = StateTime + SnapEvery; }
            break;
        }
        // Keep beside his ground point, outside the trunk he may be in, facing him.
        const FVector Below(Chuck->GetActorLocation().X, Chuck->GetActorLocation().Y, Here.Z);
        const FVector Off = (Here - Below).GetSafeNormal2D();
        Walk(Below + (Off.IsNearlyZero() ? GetActorForwardVector() : Off) * 650.f, StalkSpeed, 90.f);
        Locomotion();
        if (Dist < 1100.f && StateTime >= NextSnap)
        {
            bSnapping = true; ++Snaps;
            Play(TEXT("Attack"), false, .8f, true);
            NextSnap = StateTime + TRexClipData::AttackLength / .8f;
            Sound(SnapSounds, .9f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_TREX_SNAP snaps=%d"), Snaps);
        }
        break;
    }
    case EState::Return:
    default:
    {
        const int32 Near = NearestPoint();
        const FVector Target = AChultLanding::FloorOrigin() + PatrolPoints()[Near];
        Walk(Target, ReturnSpeed, 80.f);
        Locomotion();
        if (FVector::Dist2D(Here, Target) < 250.f) { Waypoint = (Near + 1) % PatrolPoints().Num(); SetState(EState::Patrol); }
        else if (StateTime > 2.f && Notices()) SetState(bReach ? EState::Roar : EState::Stalk);
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// -ChuckTRexTest

void StartChultTRexReview(UWorld* World)
{
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    AChultTRex* Rex = AChultTRex::SpawnUpriver(World);
    if (!Chuck || !Rex) { UE_LOG(LogTemp, Error, TEXT("CHUCK_TREX_TEST_COMPLETE failures=1 trials=0 (no T-rex)")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
    auto* PC = Cast<APlayerController>(Chuck->GetController());
    if (PC) { Chuck->DisableInput(PC); PC->SetViewTarget(Chuck); }
    const FVector G = AChultLanding::FloorOrigin();
    // The route: its capsule sweeps every leg of the loop clear of trunks and rocks.
    int32 RouteFailures = 0;
    {
        FCollisionQueryParams Q(SCENE_QUERY_STAT(TRexRoute), false, Rex); Q.AddIgnoredActor(Chuck);
        const auto& P = AChultTRex::PatrolPoints();
        for (int32 I = 0; I < P.Num(); ++I)
        {
            const FVector A = G + P[I] + FVector(0, 0, HalfHeight + 50), B = G + P[(I + 1) % P.Num()] + FVector(0, 0, HalfHeight + 50);
            FHitResult Hit;
            const bool Blocked = World->SweepSingleByChannel(Hit, A, B, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight - 40), Q);
            if (Blocked) { ++RouteFailures; UE_LOG(LogTemp, Warning, TEXT("CHUCK_TREX_ROUTE_BLOCK leg=%d by=%s at=%s"), I, *GetNameSafe(Hit.GetComponent()), *(Hit.ImpactPoint - G).ToString()); }
        }
        // The canopy's way down (the last giant's liana) lands outside its stretch.
        const bool bExitSafe = !AChultTRex::InTerritory(G + ChultGiantTrees().Last().LianaFoot);
        RouteFailures += !bExitSafe;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_TREX_ROUTE failures=%d segments=%d liana_exit_safe=%d"), RouteFailures, P.Num(), bExitSafe);
    }
    struct FRun { int32 Trial = 0, Failures = 0, Bites = 0, Respawns = 0, Shots = 0; float Start = -1, MinS = 1e9f, Moved = 0; bool bStalked = false, bDone = false; FVector Last; TWeakObjectPtr<ACameraActor> Camera; FTimerHandle Timer; };
    auto Run = MakeShared<FRun>(); Run->Failures = RouteFailures;
    const TArray<FString> Names = { TEXT("patrol"), TEXT("chase_and_kill"), TEXT("safe_in_the_giants"), TEXT("stays_in_its_stretch") };
    World->GetTimerManager().SetTimer(Run->Timer, [World, Chuck, Rex, Run, Names, G]()
    {
        if (Run->bDone || !IsValid(Rex)) return;
        const float Now = World->GetTimeSeconds();
        auto Road = [G](float S, float Up) { const FVector P = ChultCanyonFrame(S, ChultUpriverRoadT(S)); return G + P + FVector(0, 0, ChultCanyonGroundZ(P) + Up); };
        auto* Move = Chuck->GetCharacterMovement();
        if (Run->Start < 0)
        {
            Run->Start = Now; Run->Bites = Rex->GetBites(); Run->Respawns = Chuck->GetRespawns(); Run->MinS = 1e9f; Run->Moved = 0; Run->bStalked = false;
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->SetRunHeld(false);
            switch (Run->Trial)
            {
            case 0:   // its loop, him well away outside its stretch
                Chuck->ResetAtLocation(ChultUpriverSpawn(EChultUpriverSpawn::Giants));
                Rex->ResetTo(G + AChultTRex::PatrolPoints()[3] + FVector(0, 0, HalfHeight + 40), Along.Rotation().Yaw);
                break;
            case 1:   // on the ground in its stretch, in its view 22 m off
                Chuck->ResetAtLocation(Road(44000, 34.65f));
                Rex->ResetTo(Road(46200, HalfHeight + 40), (-Along).Rotation().Yaw);
                Chuck->SetActorRotation(Along.Rotation()); Chuck->Recenter(); Chuck->SetOrbitPitch(-2);
                break;
            case 2:   // up in a giant, it below
            {
                const auto& T = ChultGiantTrees()[4];
                Chuck->ResetAtLocation(G + T.Fork + FVector(0, 0, 40));
                Rex->ResetTo(Road(42400, HalfHeight + 40), (-Along).Rotation().Yaw);
                Chuck->SetActorRotation((Rex->GetActorLocation() - Chuck->GetActorLocation()).GetSafeNormal2D().Rotation()); Chuck->Recenter(); Chuck->SetOrbitPitch(-40);
                break;
            }
            default:  // just inside its stretch: he runs out of it as it comes
                Chuck->ResetAtLocation(Road(AChultTRex::TerritoryFrom + 250, 34.65f)); Chuck->SetActorRotation((-Along).Rotation());
                Rex->ResetTo(Road(AChultTRex::TerritoryFrom + 2900, HalfHeight + 40), (-Along).Rotation().Yaw);
                break;
            }
            Run->Last = Rex->GetActorLocation();
            return;
        }
        const float Time = Now - Run->Start;
        Run->Moved += static_cast<float>(FVector::Dist2D(Rex->GetActorLocation(), Run->Last)); Run->Last = Rex->GetActorLocation();
        Run->MinS = FMath::Min(Run->MinS, FrameS(Rex->GetActorLocation()));
        Run->bStalked |= Rex->GetState() == AChultTRex::EState::Stalk;
        // Stills: it on its loop from the side, coming at him, and below him in the giants.
        {
            auto* PC = Cast<APlayerController>(Chuck->GetController());
            const TCHAR* Shot = nullptr;
            if (Run->Trial == 0 && Run->Shots == 0 && Time > 4.f)
            {
                auto* Cam = World->SpawnActor<ACameraActor>(); Cam->GetCameraComponent()->SetFieldOfView(70); Run->Camera = Cam;
                const FVector At = Rex->GetActorLocation(), Side = Rex->GetActorRightVector();
                Cam->SetActorLocationAndRotation(At + Side * 1900 + Rex->GetActorForwardVector() * 300 + FVector(0, 0, 60), (At + FVector(0, 0, 80) - (At + Side * 1900 + Rex->GetActorForwardVector() * 300 + FVector(0, 0, 60))).Rotation());
                if (PC) PC->SetViewTarget(Cam);
                Run->Shots = 1;
            }
            else if (Run->Trial == 0 && Run->Shots == 1 && Time > 4.3f) { Shot = TEXT("TRex_Patrol"); Run->Shots = 2; }
            else if (Run->Trial == 0 && Run->Shots == 2 && Time > 4.6f) { if (PC) PC->SetViewTarget(Chuck); if (Run->Camera.IsValid()) Run->Camera->Destroy(); Run->Shots = 3; }
            else if (Run->Trial == 1 && Run->Shots == 3 && Rex->GetState() == AChultTRex::EState::Chase) { Shot = TEXT("TRex_Chase"); Run->Shots = 4; }
            else if (Run->Trial == 1 && Run->Shots == 4 && Rex->GetState() == AChultTRex::EState::Lunge) { Shot = TEXT("TRex_Bite"); Run->Shots = 5; }
            else if (Run->Trial == 2 && Run->Shots == 5 && Time > 7.f) { Shot = TEXT("TRex_FromTheGiant"); Run->Shots = 6; }
            if (Shot) FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir() / TEXT("Chult") / (FString(Shot) + TEXT(".png")), true, false);
        }
        bool Pass = false, Done = false;
        switch (Run->Trial)
        {
        case 0:
            Done = Time > 9.f;
            Pass = Run->Moved > 600.f && Rex->GetState() == AChultTRex::EState::Patrol && Rex->GetBites() == Run->Bites;
            break;
        case 1:
        {
            // Bitten once, dead, and back whole at the giants' spawn (outside its stretch).
            const FVector Spawn = ChultUpriverSpawn(EChultUpriverSpawn::Giants);
            Pass = Rex->GetBites() > Run->Bites && Chuck->GetRespawns() > Run->Respawns && !Chuck->IsAstral() && Move->IsMovingOnGround()
                && FVector::Dist2D(Chuck->GetActorLocation(), Spawn) < 40.f && !AChultTRex::InTerritory(Chuck->GetActorLocation());
            Done = (Pass && Time > 1.f) || Time > 35.f;
            break;
        }
        case 2:
            Done = Time > 14.f;
            Pass = Run->bStalked && Rex->GetBites() == Run->Bites && !Chuck->IsAstral() && Chuck->GetRespawns() == Run->Respawns && Rex->GetSnaps() > 0;
            break;
        default:
        {
            // He sprints out; it must not follow past its edge or catch him.
            if (Time < 4.f) { Chuck->SetRunHeld(true); const FVector D = -Along; Chuck->SetTestStickWorld(D); Chuck->AddMovementInput(D, 1.f, true); if (Time > .2f && Time < .4f) Chuck->TrySprint(); }
            else Chuck->SetTestStick(FVector2D::ZeroVector);
            Done = Time > 12.f;
            Pass = Rex->GetRoars() > 0 && Run->MinS >= AChultTRex::TerritoryFrom - 50.f && Rex->GetBites() == Run->Bites && !Chuck->IsAstral()
                && !AChultTRex::InTerritory(Chuck->GetActorLocation());
            break;
        }
        }
        if (!Done) return;
        Run->Failures += !Pass;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_TREX_TRIAL trial=%d name=%s pass=%d time=%.1f state=%s bites=%d snaps=%d roars=%d moved=%.0f min_s=%.0f stalked=%d top_speed=%.0f"),
            Run->Trial, *Names[Run->Trial], Pass, Time, Rex->GetStateName(), Rex->GetBites(), Rex->GetSnaps(), Rex->GetRoars(), Run->Moved, Run->MinS, Run->bStalked, Rex->GetTopSpeed());
        Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->SetRunHeld(false);
        ++Run->Trial; Run->Start = -1;
        if (Run->Trial >= Names.Num())
        {
            Run->bDone = true;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_TREX_TEST_COMPLETE failures=%d trials=%d sounds=%d top_speed=%.0f"), Run->Failures, Names.Num(), Rex->GetSoundsPlayed(), Rex->GetTopSpeed());
            FPlatformMisc::RequestExitWithStatus(false, Run->Failures ? 1 : 0);
        }
    }, .016f, true);
}
