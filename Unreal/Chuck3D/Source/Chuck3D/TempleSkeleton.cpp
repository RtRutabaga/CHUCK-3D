#include "TempleSkeleton.h"
#include "ChuckCharacter.h"
#include "SkeletonClipData.h"
#include "TempleHero.h"
#include "TempleSanctumBattle.h"
#include "ChultTempleInterior.h"
#include "ChultTempleWind.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    TArray<TWeakObjectPtr<ATempleSkeleton>> SkeletonRegistry;

    // Each attack: its clip's rate, the strike (clip seconds, SkeletonClipData), how far round to
    // the left its striking end is at the strike, the reach it is chosen for (centre to centre, cm),
    // and the window round the strike (clip seconds) in which a touch counts.
    struct FAttackSpec { FName Clip; float Rate, Strike, Aim, Before, After; const TCHAR* Tip; const TCHAR* TipParent; float TipAhead; int32 Damage; };
    const FAttackSpec& Spec(ATempleSkeleton::EAttack Attack)
    {
        static const FAttackSpec Specs[] = {
            {TEXT("Kick"), 1.f, SkeletonClipData::KickStrike, SkeletonClipData::KickAim, .14f, .16f, TEXT("ball_r"), TEXT("foot_r"), 7.f, ATempleSkeleton::KickDamage},
            {TEXT("Grab"), 1.25f, SkeletonClipData::GrabStrike, SkeletonClipData::GrabAim, .2f, .35f, TEXT("middle_03_r"), TEXT("middle_02_r"), 2.f, ATempleSkeleton::GrabDamage},
            {TEXT("Swat"), 1.3f, SkeletonClipData::SwatStrike, SkeletonClipData::SwatAim, .25f, .12f, TEXT("middle_03_l"), TEXT("middle_02_l"), 2.f, ATempleSkeleton::SwatDamage},
            // The sanctum's blow at a hero: a fist driven out at chest height (CMU 02_05).
            {TEXT("Strike"), 1.f, SkeletonClipData::StrikeStrike, SkeletonClipData::StrikeAim, .1f, .12f, TEXT("middle_03_r"), TEXT("middle_02_r"), 2.f, 1}};
        return Specs[FMath::Clamp(static_cast<int32>(Attack), 0, 3)];
    }

    // Spawned in play and in its own test only: the other automated runs keep the worlds their checks were written against.
    bool Suppressed()
    {
        const TCHAR* Cmd = FCommandLine::Get();
        if (FParse::Param(Cmd, TEXT("ChuckSkeletonTest")) || FParse::Param(Cmd, TEXT("ChuckSkeletonCapture"))) return false;
        TArray<FString> Words; FString(Cmd).ParseIntoArrayWS(Words);
        for (const FString& Word : Words)
            if (Word.StartsWith(TEXT("-Chuck")) && (Word.Contains(TEXT("Test")) || Word.Contains(TEXT("Capture")) || Word.Contains(TEXT("Review")))) return true;
        return false;
    }

    FTransform RefComponentSpace(const FReferenceSkeleton& Ref, int32 Index)
    {
        FTransform T = Ref.GetRefBonePose()[Index];
        for (int32 P = Ref.GetParentIndex(Index); P != INDEX_NONE; P = Ref.GetParentIndex(P)) T = T * Ref.GetRefBonePose()[P];
        return T;
    }
}

const TArray<TWeakObjectPtr<ATempleSkeleton>>& ATempleSkeleton::All() { return SkeletonRegistry; }

ATempleSkeleton::ATempleSkeleton()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(Radius, HalfHeight);
    auto* Move = GetCharacterMovement();
    Move->bRunPhysicsWithNoController = true;   // driven by its own Tick, no AI controller
    Move->MaxWalkSpeed = ChaseSpeed;
    Move->MaxAcceleration = 600;
    Move->BrakingDecelerationWalking = 900;
    Move->MaxStepHeight = 30;
    bUseControllerRotationYaw = false;
    GetMesh()->SetRelativeLocation(FVector(0, 0, -HalfHeight));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetAnimInstanceClass(USkeletonAnimInstance::StaticClass());
    GetMesh()->SetLightingChannels(false, true, false);   // the interior's torches light channel 1 only
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Characters/Skeleton/SK_TempleSkeleton.SK_TempleSkeleton"));
    GetMesh()->SetSkinnedAssetAndUpdate(Body.Object);
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/Skeleton/Animations/AS_Skeleton_Idle.AS_Skeleton_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/Characters/Skeleton/Animations/AS_Skeleton_Walk.AS_Skeleton_Walk"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Kick(TEXT("/Game/Characters/Skeleton/Animations/AS_Skeleton_Kick.AS_Skeleton_Kick"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Grab(TEXT("/Game/Characters/Skeleton/Animations/AS_Skeleton_Grab.AS_Skeleton_Grab"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Swat(TEXT("/Game/Characters/Skeleton/Animations/AS_Skeleton_Swat.AS_Skeleton_Swat"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Rise(TEXT("/Game/Characters/Skeleton/Animations/AS_Skeleton_Rise.AS_Skeleton_Rise"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Collapse(TEXT("/Game/Characters/Skeleton/Animations/AS_Skeleton_Collapse.AS_Skeleton_Collapse"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Strike(TEXT("/Game/Characters/Skeleton/Animations/AS_Skeleton_Strike.AS_Skeleton_Strike"));
    Clips.Add(TEXT("Strike"), Strike.Object);
    Clips.Add(TEXT("Idle"), Idle.Object); Clips.Add(TEXT("Walk"), Walk.Object); Clips.Add(TEXT("Kick"), Kick.Object);
    Clips.Add(TEXT("Grab"), Grab.Object); Clips.Add(TEXT("Swat"), Swat.Object); Clips.Add(TEXT("Rise"), Rise.Object); Clips.Add(TEXT("Collapse"), Collapse.Object);
}

ATempleSkeleton* ATempleSkeleton::SpawnAt(UWorld* World, const FTempleSkeletonPost& InPost)
{
    if (!World) return nullptr;
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    Params.bDeferConstruction = true;
    auto* S = World->SpawnActor<ATempleSkeleton>(InPost.Feet + FVector(0, 0, HalfHeight + 2), FRotator(0, InPost.Yaw, 0), Params);
    if (!S) return nullptr;
    S->Post = InPost;
    S->Random.Initialize(static_cast<int32>(InPost.Feet.X * 7 + InPost.Feet.Y * 13));
    UGameplayStatics::FinishSpawningActor(S, FTransform(FRotator(0, InPost.Yaw, 0), InPost.Feet + FVector(0, 0, HalfHeight + 2)));
    return S;
}

void SpawnTempleSkeletons(UWorld* World)
{
    if (!World || Suppressed()) return;
    for (TActorIterator<ATempleSkeleton> It(World); It; ++It) return;   // once
    TArray<FTempleSkeletonPost> Posts; ChultTempleSkeletonPosts(Posts);
    int32 Spawned = 0, Lying = 0;
    for (const FTempleSkeletonPost& P : Posts) if (ATempleSkeleton::SpawnAt(World, P)) { ++Spawned; Lying += P.bDormant; }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SKELETONS spawned=%d of=%d lying=%d"), Spawned, Posts.Num(), Lying);
}

void ATempleSkeleton::BeginPlay()
{
    Super::BeginPlay();
    SkeletonRegistry.Add(this);
    // The mesh's facing (toes ahead of ankles in its rest pose) turned onto the actor's.
    if (const USkeletalMesh* M = Cast<USkeletalMesh>(GetMesh()->GetSkinnedAsset()))
    {
        const FReferenceSkeleton& Ref = M->GetRefSkeleton();
        const int32 BL = Ref.FindBoneIndex(TEXT("ball_l")), BR = Ref.FindBoneIndex(TEXT("ball_r")), FL = Ref.FindBoneIndex(TEXT("foot_l")), FR = Ref.FindBoneIndex(TEXT("foot_r"));
        if (BL != INDEX_NONE && BR != INDEX_NONE && FL != INDEX_NONE && FR != INDEX_NONE)
        {
            FVector F = (RefComponentSpace(Ref, BL).GetLocation() + RefComponentSpace(Ref, BR).GetLocation() - RefComponentSpace(Ref, FL).GetLocation() - RefComponentSpace(Ref, FR).GetLocation());
            F.Z = 0; BodyForward = F.GetSafeNormal();
            GetMesh()->SetRelativeRotation(FRotator(0, -BodyForward.Rotation().Yaw, 0));
        }
    }
    Animator.Init(GetMesh(), Clips, SkeletonClipData::WalkSpeed * (SkeletonClipData::WalkLength), BodyForward);
    auto Load = [](TArray<USoundBase*>& Set, const TCHAR* Stem, int32 Count)
    {
        for (int32 I = 0; I < Count; ++I)
            if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Stem, I, Stem, I))) Set.Add(S);
    };
    Load(StepSounds, TEXT("SFX_SkeletonStep"), 4); Load(RattleSounds, TEXT("SFX_SkeletonRattle"), 2); Load(SwingSounds, TEXT("SFX_SkeletonSwing"), 3);
    Load(HitSounds, TEXT("SFX_SkeletonHit"), 3); Load(CollapseSounds, TEXT("SFX_SkeletonCollapse"), 2); Load(RiseSounds, TEXT("SFX_SkeletonRise"), 2);
    // Heard within the room, not through the masonry from the halls above (as the traps).
    Attenuation = NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate = true;
    Attenuation->Attenuation.bSpatialize = true;
    Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Attenuation->Attenuation.AttenuationShapeExtents = FVector(220.f, 0.f, 0.f);
    Attenuation->Attenuation.FalloffDistance = 1800.f;
    ResetToPost();
    int32 Loaded = 0;
    for (const auto& Clip : Clips) Loaded += Clip.Value != nullptr;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SKELETON_SPAWN space=%s dormant=%d clips=%d mesh=%d sounds=%d forward=%s"), Post.Space, Post.bDormant ? 1 : 0, Loaded,
        GetMesh()->GetSkinnedAsset() != nullptr,
        StepSounds.Num() + RattleSounds.Num() + SwingSounds.Num() + HitSounds.Num() + CollapseSounds.Num() + RiseSounds.Num(), *BodyForward.ToString());
}

void ATempleSkeleton::EndPlay(const EEndPlayReason::Type Reason)
{
    SkeletonRegistry.RemoveAll([this](const TWeakObjectPtr<ATempleSkeleton>& S) { return !S.IsValid() || S.Get() == this; });
    Super::EndPlay(Reason);
}

int32 ATempleSkeleton::GetAnimEvaluations() const
{
    auto* Anim = Cast<USkeletonAnimInstance>(GetMesh()->GetAnimInstance());
    return Anim ? Anim->GetEvaluations() : 0;
}

const TCHAR* ATempleSkeleton::AttackName(EAttack A)
{
    switch (A) { case EAttack::Kick: return TEXT("Kick"); case EAttack::Grab: return TEXT("Grab"); case EAttack::Swat: return TEXT("Swat"); case EAttack::Strike: return TEXT("Strike"); default: return TEXT("None"); }
}

const TCHAR* ATempleSkeleton::GetStateName() const
{
    static const TCHAR* Names[] = {TEXT("Dormant"), TEXT("Rise"), TEXT("Guard"), TEXT("Chase"), TEXT("Attack"), TEXT("Recover"), TEXT("Return"), TEXT("Settle"), TEXT("Dead")};
    return Names[static_cast<int32>(State)];
}

void ATempleSkeleton::SetState(EState To)
{
    const EState From = State;
    State = To; StateTime = 0;
    UE_LOG(LogTemp, Verbose, TEXT("CHUCK_TEMPLE_SKELETON_STATE from=%d to=%s"), static_cast<int32>(From), GetStateName());
    switch (To)
    {
    case EState::Dormant: Animator.Play(TEXT("Rise"), false, 0.f, true, .35f); Animator.SetJaw(0); Animator.SetLook(0, 0); break;
    case EState::Rise: ++Rises; Animator.Play(TEXT("Rise"), false, RiseRate, false, 0.f);   // from where it lies (or from part way down)
        Sound(RiseSounds, .9f); break;
    case EState::Settle: ++Settles; Animator.Play(TEXT("Rise"), false, SettleRate, true, .4f); Sound(RiseSounds, .7f); break;
    case EState::Chase: if (From == EState::Guard || From == EState::Rise) Sound(RattleSounds, .8f); break;
    default: break;
    }
}

void ATempleSkeleton::Sound(const TArray<USoundBase*>& Set, float Volume)
{
    if (!Set.Num()) return;
    UGameplayStatics::PlaySoundAtLocation(this, Set[Random.RandRange(0, Set.Num() - 1)], GetActorLocation(), Volume, Random.FRandRange(.92f, 1.08f), 0.f, Attenuation);
    ++SoundsPlayed;
}

float ATempleSkeleton::FeetZ() const { return static_cast<float>(GetActorLocation().Z) - HalfHeight; }

void ATempleSkeleton::Face(const FVector& Dir, float DegPerSecond, float DeltaSeconds)
{
    if (Dir.IsNearlyZero()) return;
    SetActorRotation(FRotator(0, FMath::FixedTurn(GetActorRotation().Yaw, Dir.Rotation().Yaw, DegPerSecond * DeltaSeconds), 0));
}

bool ATempleSkeleton::InLeash(const FVector& World, float Margin) const
{
    const FVector F = ChultTempleInteriorFrameOf(World);
    return F.X >= Post.Leash.Min.X - Margin && F.X <= Post.Leash.Max.X + Margin && F.Y >= Post.Leash.Min.Y - Margin && F.Y <= Post.Leash.Max.Y + Margin;
}

FVector ATempleSkeleton::ClampToLeash(const FVector& World) const
{
    const FVector F = ChultTempleInteriorFrameOf(World);
    const FVector C(FMath::Clamp(F.X, Post.Leash.Min.X, Post.Leash.Max.X), FMath::Clamp(F.Y, Post.Leash.Min.Y, Post.Leash.Max.Y), F.Z);
    return ChultTempleInteriorWorldOf(C);
}

bool ATempleSkeleton::CanReach(const AChuckCharacter* Chuck) const
{
    if (!Chuck || Chuck->IsAstral() || Chuck->IsSwinging() || Chuck->IsSwingLeaping()) return false;
    const float ChuckFeet = static_cast<float>(Chuck->GetActorLocation().Z) - Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    return FMath::Abs(ChuckFeet - FeetZ()) < 45.f && InLeash(Chuck->GetActorLocation(), 70.f);
}

bool ATempleSkeleton::Sees(const AChuckCharacter* Chuck) const
{
    const FVector Eye = GetMesh()->GetBoneLocation(TEXT("head")) + FVector(0, 0, 8);
    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SkeletonSight), false, this);
    Q.AddIgnoredActor(Chuck);
    for (const TWeakObjectPtr<ATempleSkeleton>& S : SkeletonRegistry) if (S.IsValid()) Q.AddIgnoredActor(S.Get());
    return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Chuck->GetActorLocation() + FVector(0, 0, 10), ECC_Visibility, Q);
}

FVector ATempleSkeleton::Steer(const FVector& Want, const AActor* Ignore) const
{
    // Round piers, jars and fallen drums: the first clear heading near the one wanted.
    const FVector Dir = Want.GetSafeNormal2D();
    if (Dir.IsNearlyZero()) return Dir;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SkeletonSteer), false, this);
    if (Ignore) Q.AddIgnoredActor(Ignore);
    for (const TWeakObjectPtr<ATempleSkeleton>& S : SkeletonRegistry) if (S.IsValid()) Q.AddIgnoredActor(S.Get());
    const FVector From = GetActorLocation() + FVector(0, 0, -20);
    const float Look = FMath::Min(120.f, static_cast<float>(Want.Size2D()));
    for (const float Turn : {0.f, 30.f, -30.f, 60.f, -60.f, 95.f, -95.f})
    {
        const FVector Try = Dir.RotateAngleAxis(Turn, FVector::UpVector);
        // The sanctum's broken floor: a heading that walks it into a hole is not clear.
        if (Battle.IsValid())
        {
            bool bHole = false;
            for (const float Ahead : {35.f, 80.f})
            {
                const FVector F = ChultTempleInteriorFrameOf(GetActorLocation() + Try * FMath::Min(Ahead, Look + 30.f));
                bHole |= ChultTempleSanctumHoleAt(FVector2D(F.X, F.Y), Radius + 8.f) != 0;
            }
            if (bHole) continue;
        }
        FHitResult Hit;
        if (!GetWorld()->SweepSingleByChannel(Hit, From, From + Try * Look, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius - 4.f), Q)
            || Hit.ImpactNormal.Z > .7f) return Try;
    }
    return Dir;
}

FVector ATempleSkeleton::StrikingTip(EAttack A) const
{
    const FAttackSpec& S = Spec(A);
    const FVector Tip = GetMesh()->GetBoneLocation(S.Tip), Parent = GetMesh()->GetBoneLocation(S.TipParent);
    return Tip + (Tip - Parent).GetSafeNormal() * S.TipAhead;
}

void ATempleSkeleton::ResetToPost()
{
    SetActorLocation(Post.Feet + FVector(0, 0, HalfHeight + 2), false, nullptr, ETeleportType::TeleportPhysics);
    SetActorRotation(FRotator(0, Post.Yaw, 0));
    auto* Move = GetCharacterMovement();
    Move->StopMovementImmediately();
    Move->SetMovementMode(MOVE_Walking);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    HitsTaken = 0; Lost = 0; OutOfReach = 0; AttackReadyAt = 0; Attack = EAttack::None; Forced = EAttack::None; LastStrikeGap = 1e9f;
    LastLocation = GetActorLocation();
    if (Post.bDormant) SetState(EState::Dormant);
    else { SetState(EState::Guard); Animator.Play(TEXT("Idle"), true, 1.f, true, 0.f); }
}

void ATempleSkeleton::StartAttack(EAttack Kind)
{
    Attack = Kind; LastAttack = Kind; bStruck = false; bSwung = false; LastStrikeGap = 1e9f;
    ++Attacks[static_cast<int32>(Kind)];
    const FAttackSpec& S = Spec(Kind);
    Animator.Play(S.Clip, false, S.Rate, true, .18f);
    GetCharacterMovement()->StopMovementImmediately();
    Sound(RattleSounds, .65f);   // the teeth chatter: the tell
    SetState(EState::Attack);
}

void ATempleSkeleton::TakeSlash(const FVector& Swing)
{
    if (State == EState::Dead) return;
    ++HitsTaken;
    Sound(HitSounds, .9f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SKELETON_HIT hits=%d of=%d state=%s"), HitsTaken, Health, GetStateName());
    if (HitsTaken >= Health) { Die(Swing); return; }
    Animator.Flinch(9.f);
    if (State == EState::Dormant || State == EState::Settle) SetState(EState::Rise);
    else if (State == EState::Guard || State == EState::Return) SetState(EState::Chase);
}

void ATempleSkeleton::Die(const FVector& Swing)
{
    SetState(EState::Dead);
    Sound(CollapseSounds, 1.f);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // It goes down where it stands: the knees go and it drops flat on its back (the motion-captured fall, fast),
    // then lies still, the skull lolled and the jaw hanging open.
    Animator.Play(TEXT("Collapse"), false, CollapseRate, true, .12f);
    Animator.SetLook(0, 0);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SKELETON_DEAD space=%s from=%s"), Post.Space, *Swing.GetSafeNormal2D().ToCompactString());
}

void ATempleSkeleton::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const FVector Here = GetActorLocation();
    const float Travelled = static_cast<float>(FVector::Dist2D(Here, LastLocation));
    LastLocation = Here;
    Animator.SetTimeScale(GetWorld()->GetTimeSeconds() < HitStopUntil ? .03f : 1.f);
    Animator.Tick(DeltaSeconds, State == EState::Chase || State == EState::Return ? Travelled : 0.f);
    if (Battle.IsValid()) { TickSanctum(DeltaSeconds); return; }
    if (State == EState::Dead)
    {
        // Down: the skull rolls aside and the jaw falls open as it settles.
        if (Animator.Finished()) { Animator.SetJaw(20.f); Animator.SetLook(28.f, -8.f); }
        return;
    }
    if (bHeld) { GetCharacterMovement()->MaxWalkSpeed = 0; return; }
    StateTime += DeltaSeconds;
    auto* Move = GetCharacterMovement();
    const float Speed = static_cast<float>(GetVelocity().Size2D());
    TopSpeed = FMath::Max(TopSpeed, Speed);
    if (!InLeash(Here)) WorstLeash = FMath::Max(WorstLeash, static_cast<float>(FVector::Dist2D(Here, ClampToLeash(Here))));
    // Footfalls: one each half cycle of the walk.
    StepDistance += Speed * DeltaSeconds;
    if (Speed > 15.f && StepDistance > SkeletonClipData::WalkSpeed * SkeletonClipData::WalkLength * .5f) { StepDistance = 0; Sound(StepSounds, .55f); }

    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const FVector ChuckAt = Chuck ? Chuck->GetActorLocation() : Here + FVector(1e6, 0, 0);
    const FVector ToChuck = ChuckAt - Here;
    const float Dist = static_cast<float>(ToChuck.Size2D());
    // Far off (another hall): nothing to do but keep its pose.
    if (Dist > 4000.f && (State == EState::Dormant || State == EState::Guard)) return;
    const bool bReach = CanReach(Chuck);
    const bool bInSpace = Chuck && !Chuck->IsAstral() && InLeash(ChuckAt, 120.f);

    // The head follows him while it is up and he is near; the jaw rests a little open.
    {
        const bool bUp = State == EState::Guard || State == EState::Chase || State == EState::Recover || (State == EState::Attack && Animator.Time() < Spec(Attack).Strike - .3f);
        float Yaw = 0, Pitch = 0;
        if (bUp && Dist < 1200.f)
        {
            const FVector Head = GetMesh()->GetBoneLocation(TEXT("head"));
            const FVector Local = GetActorTransform().InverseTransformVectorNoScale(ChuckAt - Head);
            Yaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Y), static_cast<float>(Local.X))), -65.f, 65.f) * -1.f;
            Pitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-Local.Z), static_cast<float>(Local.Size2D()))) - 15.f, -10.f, 40.f);
        }
        Animator.SetLook(Yaw, Pitch);
        if (State != EState::Attack) Animator.SetJaw(State == EState::Dormant ? 0.f : 4.f);
    }

    auto Walk = [&](const FVector& Target, float MaxSpeed, float TurnRate)
    {
        Move->MaxWalkSpeed = MaxSpeed;
        const FVector Want = Target - Here;
        if (Want.Size2D() < 12.f) { Animator.Locomote(Speed); return; }
        const FVector Dir = Steer(Want, Chuck);
        AddMovementInput(Dir, FMath::Clamp(static_cast<float>(Want.Size2D()) / 40.f, .25f, 1.f));
        Face(Speed > 15.f ? GetVelocity() : Dir, TurnRate, DeltaSeconds);
        Animator.Locomote(Speed);
    };
    auto Home = [&]()
    {
        if (FVector::Dist2D(Here, Post.Feet) < 25.f) return true;
        Walk(Post.Feet, ReturnSpeed, 140.f);
        return false;
    };

    switch (State)
    {
    case EState::Dormant:
        if (bInSpace && Dist < WakeRange && (bReach || Dist < WakeRange * .6f)) SetState(EState::Rise);
        break;
    case EState::Rise:
        Move->MaxWalkSpeed = 0;
        if (Animator.Finished())
        {
            Animator.Play(TEXT("Idle"), true, 1.f, false, .4f);
            SetState(bInSpace ? EState::Chase : EState::Return);
        }
        break;
    case EState::Guard:
    {
        Move->MaxWalkSpeed = 0;
        Animator.Play(TEXT("Idle"), true, 1.f);
        const bool bView = FVector::DotProduct(ToChuck.GetSafeNormal2D(), GetActorForwardVector()) > .25f;
        if (bInSpace && ((Dist < NoticeRange && bView) || Dist < CloseNotice) && Sees(Chuck)) SetState(EState::Chase);
        else if (FVector::Dist2D(Here, Post.Feet) > 40.f) SetState(EState::Return);
        else Face(FRotator(0, Post.Yaw, 0).Vector(), 60.f, DeltaSeconds);
        break;
    }
    case EState::Chase:
    {
        Lost = bInSpace && (Dist < 350.f || Sees(Chuck)) ? 0.f : Lost + DeltaSeconds;
        OutOfReach = bReach || !bInSpace ? 0.f : OutOfReach + DeltaSeconds;
        if (Lost > LoseTime || OutOfReach > Patience) { SetState(EState::Return); break; }
        // To him, but no further than the edge of its own space.
        const FVector Goal = ClampToLeash(ChuckAt);
        const float GoalGap = static_cast<float>(FVector::Dist2D(Here, Goal));
        // In reach and facing him, it strikes: the attack chosen by how far he is.
        const float Facing = FVector::DotProduct(ToChuck.GetSafeNormal2D(), GetActorForwardVector());
        EAttack Kind = Forced;
        // After a blow it stands off a moment before the next (an opening to scratch it).
        if (Kind == EAttack::None && GetWorld()->GetTimeSeconds() >= AttackReadyAt) Kind = Dist < 58.f ? EAttack::Swat : Dist < 98.f ? (Random.FRand() < .6f ? EAttack::Kick : EAttack::Grab) : EAttack::None;
        const float Ready = Kind == EAttack::Swat ? 62.f : Kind == EAttack::Kick ? 100.f : Kind == EAttack::Grab ? 100.f : 0.f;
        if (bReach && Kind != EAttack::None && Dist < Ready && Facing > .55f) { Forced = EAttack::None; StartAttack(Kind); break; }
        if (bReach && Dist < 70.f) { Move->MaxWalkSpeed = 0; Face(ToChuck, 220.f, DeltaSeconds); Animator.Locomote(Speed); break; }
        if (GoalGap < 15.f) { Move->MaxWalkSpeed = 0; Face(ToChuck, 160.f, DeltaSeconds); Animator.Locomote(Speed); break; }   // at its edge, watching
        Walk(Goal, ChaseSpeed, 170.f);
        break;
    }
    case EState::Attack:
    {
        Move->MaxWalkSpeed = 0;
        const FAttackSpec& S = Spec(Attack);
        const float T = Animator.Time();
        // Winding up it turns to bring the strike onto him; from just before the strike it is committed.
        if (T < S.Strike - .22f && Chuck)
            Face(ToChuck.GetSafeNormal2D().RotateAngleAxis(S.Aim, FVector::UpVector), 260.f, DeltaSeconds);   // (+ yaw turns right: a strike on its left needs it turned right of him)
        // The teeth chatter through the tell; the jaw drops wide at the strike.
        Animator.SetJaw(T < S.Strike - .1f ? (FMath::Fmod(StateTime * 14.f, 1.f) < .5f ? 11.f : 2.f) : T < S.Strike + .25f ? 24.f : 5.f);
        if (!bSwung && T >= S.Strike - .14f) { bSwung = true; Sound(SwingSounds, .8f); }
        if (T >= S.Strike - S.Before && T <= S.Strike + S.After)
        {
            const FVector Tip = StrikingTip(Attack);
            if (Chuck)
            {
                const float Half = Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), R = Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius();
                const FVector A = ChuckAt - FVector(0, 0, Half - R), B = ChuckAt + FVector(0, 0, Half - R);
                const float Gap = FMath::Sqrt(static_cast<float>(FMath::PointDistToSegmentSquared(Tip, A, B))) - R;
                LastStrikeGap = FMath::Min(LastStrikeGap, Gap);
                if (!bStruck && Gap < HitSlack && bReach)
                {
                    bStruck = true;
                    const bool bLanded = Chuck->TakeBite(Here, S.Damage, Attack == EAttack::Kick);
                    if (bLanded) ++Landed[static_cast<int32>(Attack)];
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SKELETON_STRIKE attack=%s landed=%d gap=%.1f clip_t=%.2f sanity=%d"), AttackName(Attack), bLanded ? 1 : 0, Gap, T, Chuck->GetSanity());
                }
            }
        }
        if (T > S.Strike + S.After && !bStruck) { bStruck = true; UE_LOG(LogTemp, Display, TEXT("CHUCK_TEMPLE_SKELETON_STRIKE attack=%s landed=0 gap=%.1f clip_t=%.2f miss=1"), AttackName(Attack), LastStrikeGap, T); }
        if (T >= Animator.Length() - .2f)
        {
            ++Strikes;
            AttackReadyAt = GetWorld()->GetTimeSeconds() + Random.FRandRange(PauseMin, PauseMax);
            Animator.Play(TEXT("Idle"), true, 1.f, false, .3f);
            SetState(EState::Recover);
        }
        break;
    }
    case EState::Recover:
        Move->MaxWalkSpeed = 0;
        Face(ToChuck, 120.f, DeltaSeconds);
        if (StateTime > .45f) SetState(EState::Chase);
        break;
    case EState::Return:
        if (bInSpace && bReach && Dist < NoticeRange * .6f && StateTime > 1.f && Sees(Chuck)) { SetState(EState::Chase); break; }
        if (Home())
        {
            Move->StopMovementImmediately();
            Face(FRotator(0, Post.Yaw, 0).Vector(), 120.f, DeltaSeconds);
            if (FMath::Abs(FRotator::NormalizeAxis(GetActorRotation().Yaw - Post.Yaw)) < 5.f)
            {
                if (Post.bDormant) SetState(EState::Settle);
                else { Animator.Play(TEXT("Idle"), true, 1.f); SetState(EState::Guard); }
            }
            else Animator.Locomote(0.f);
        }
        break;
    case EState::Settle:
        Move->MaxWalkSpeed = 0;
        if (bInSpace && Dist < WakeRange) { SetState(EState::Rise); break; }
        if (Animator.Finished()) SetState(EState::Dormant);
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// The sanctum (docs/CHULT-TEMPLE-HEROES.md): the heroes' foes, not Chuck's.

void ATempleSkeleton::SetFoe(ATempleHero* Hero)
{
    Foe = Hero;
    SlotAngle = Random.FRandRange(-60.f, 60.f);   // where round its hero it means to stand (+ to the hero's left)
}

ATempleHero* ATempleSkeleton::GetFoe() const { return Foe.Get(); }

// In the sanctum it never walks off its floor into the broken floor's holes (2026-10-24).
void ATempleSkeleton::SetSanctum(ATempleSanctumBattle* InBattle) { Battle = InBattle; GetCharacterMovement()->bCanWalkOffLedges = false; }

void ATempleSkeleton::Wake()
{
    if (State == EState::Dormant || State == EState::Settle) SetState(EState::Rise);
}

float ATempleSkeleton::StrikeIn() const
{
    if (State != EState::Attack) return 1e6f;
    return FMath::Max(0.f, (Spec(Attack).Strike - Animator.Time()) / FMath::Max(.1f, Spec(Attack).Rate));
}

void ATempleSkeleton::HitStop(float Seconds)
{
    HitStopUntil = FMath::Max(HitStopUntil, static_cast<float>(GetWorld()->GetTimeSeconds()) + Seconds);
}

FTransform ATempleSkeleton::SkullTransform() const
{
    const int32 B = GetMesh()->GetBoneIndex(TEXT("head"));
    return B == INDEX_NONE ? GetActorTransform() : GetMesh()->GetBoneTransform(B);
}

bool ATempleSkeleton::TakeHeroBlow(EBlow Kind, const FVector& From, const FVector& At)
{
    if (State == EState::Dead) return false;
    const FVector Push = (At - From).GetSafeNormal2D();
    const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Push);
    // Turned by a blow from its side (+ twist: to its left; a push from its left turns it right).
    const float Twist = FMath::Clamp(static_cast<float>(-Local.Y) * 18.f, -18.f, 18.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_BLOW kind=%d state=%s arrows=%d"), static_cast<int32>(Kind), GetStateName(), ArrowsIn);
    switch (Kind)
    {
    case EBlow::Bash:
        // Knocked back off its line: it staggers back and comes on again.
        Animator.Flinch(22.f, Twist);
        StaggerVel = Push * 260.f; StaggerTime = .45f;
        if (State == EState::Attack) { Animator.Play(TEXT("Idle"), true, 1.f, false, .15f); SetState(EState::Recover); }
        AttackReadyAt = GetWorld()->GetTimeSeconds() + 1.4f;
        return false;
    case EBlow::Arrow:
        if (++ArrowsIn < 3 && State != EState::Dormant)   // the third arrow drops it
        {
            Animator.Flinch(14.f, Twist * .6f);
            StaggerVel = Push * 90.f; StaggerTime = .25f;
            return false;
        }
        break;
    default: break;
    }
    Fall(Kind, From);
    return true;
}

void ATempleSkeleton::Fall(EBlow Kind, const FVector& From)
{
    Sound(CollapseSounds, 1.f);
    if (Kind == EBlow::Decapitate) { bHeadless = true; Animator.SetHeadScale(.001f); }
    const FVector Push = (GetActorLocation() - From).GetSafeNormal2D();
    // Struck down where it stands, thrown a little the blow's way: the knees go and it drops.
    Animator.Flinch(Kind == EBlow::Cleave ? 30.f : 18.f);
    StaggerVel = Push * (Kind == EBlow::Bolt ? 220.f : Kind == EBlow::Cleave ? 60.f : 110.f); StaggerTime = .3f;
    SetState(EState::Dead);
    GetCharacterMovement()->StopMovementImmediately();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Headless, it stands a breath before it goes (the body does not yet know).
    const float Rate = Kind == EBlow::Cleave ? CollapseRate * 1.25f : CollapseRate;
    Animator.Play(TEXT("Collapse"), false, Rate, true, Kind == EBlow::Decapitate ? .35f : .12f);
    Animator.SetLook(0, 0);
}

void ATempleSkeleton::ResetToReserve(const FTempleSkeletonPost& InPost)
{
    Post = InPost; Post.bDormant = true;
    for (UStaticMeshComponent* A : StuckArrows) if (A) A->DestroyComponent();
    StuckArrows.Reset();
    ArrowsIn = 0; bHeadless = false; Animator.SetHeadScale(1.f);
    if (bDust) { bDust = false; SetActorHiddenInGame(false); }
    StaggerVel = FVector::ZeroVector; StaggerTime = 0; HitStopUntil = 0;
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    ResetToPost();
}

void ATempleSkeleton::Disintegrate(const FVector& From)
{
    if (bDust) return;
    if (State != EState::Dead) Fall(EBlow::Bolt, From);
    bDust = true;
    // Gone a moment after the beam's touch (the beholder throws the ash and bone dust).
    FTimerHandle Gone;
    TWeakObjectPtr<ATempleSkeleton> Self(this);
    GetWorldTimerManager().SetTimer(Gone, [Self]() { if (Self.IsValid() && Self->bDust) Self->SetActorHiddenInGame(true); }, .12f, false);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_DISINTEGRATED"));
}

void ATempleSkeleton::StickArrow(UStaticMesh* Arrow, const FVector& At, const FVector& Dir)
{
    if (!Arrow) return;
    // In the bone nearest where it struck, along its flight, sunk a third of its length.
    static const TCHAR* Bones[] = {TEXT("spine_03"), TEXT("spine_02"), TEXT("pelvis"), TEXT("head"), TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("thigh_l"), TEXT("thigh_r")};
    FName Best = NAME_None; float BestD = 1e9f;
    for (const TCHAR* B : Bones)
    {
        const float D = static_cast<float>(FVector::Dist(GetMesh()->GetBoneLocation(B), At));
        if (D < BestD) { BestD = D; Best = B; }
    }
    auto* C = NewObject<UStaticMeshComponent>(this);
    C->SetStaticMesh(Arrow);
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetLightingChannels(false, true, false);
    C->RegisterComponent();
    C->SetWorldLocationAndRotation(At - Dir.GetSafeNormal() * 52.f, Dir.Rotation());
    C->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepWorldTransform, Best);
    StuckArrows.Add(C);
}

void ATempleSkeleton::TickSanctum(float DeltaSeconds)
{
    StateTime += DeltaSeconds;
    auto* Move = GetCharacterMovement();
    const FVector Here = GetActorLocation();
    const bool bFrozen = GetWorld()->GetTimeSeconds() < HitStopUntil;
    // A blow's push carries it (a stagger, or the fall's lurch), easing off.
    if (StaggerTime > 0.f && !bFrozen)
    {
        StaggerTime -= DeltaSeconds;
        FHitResult Hit;
        AddActorWorldOffset(StaggerVel * DeltaSeconds, true, &Hit);
        StaggerVel *= FMath::Exp(-6.f * DeltaSeconds);
    }
    if (State == EState::Dead)
    {
        if (Animator.Finished()) { Animator.SetJaw(bHeadless ? 0.f : 20.f); if (!bHeadless) Animator.SetLook(28.f, -8.f); }
        return;
    }
    const float Speed = static_cast<float>(GetVelocity().Size2D());
    StepDistance += Speed * DeltaSeconds;
    if (Speed > 15.f && StepDistance > SkeletonClipData::WalkSpeed * SkeletonClipData::WalkLength * .5f) { StepDistance = 0; Sound(StepSounds, .5f); }
    ATempleHero* Hero = Foe.Get();
    const FVector HeroAt = Hero ? Hero->GetActorLocation() : Here;
    const FVector ToHero = HeroAt - Here;
    const float Dist = static_cast<float>(ToHero.Size2D());
    // It watches its hero.
    if (Hero && (State == EState::Chase || State == EState::Recover || (State == EState::Attack && Animator.Time() < Spec(Attack).Strike - .3f)))
    {
        const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Hero->HeadLocation() - GetMesh()->GetBoneLocation(TEXT("head")));
        Animator.SetLook(FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Y), static_cast<float>(Local.X))), -60.f, 60.f) * -1.f,
                         FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-Local.Z), static_cast<float>(Local.Size2D()))), -25.f, 25.f));
        if (State != EState::Attack) Animator.SetJaw(4.f);
    }
    switch (State)
    {
    case EState::Dormant: Move->MaxWalkSpeed = 0; break;
    case EState::Rise:
        Move->MaxWalkSpeed = 0;
        if (Animator.Finished()) { Animator.Play(TEXT("Idle"), true, 1.f, false, .4f); SetState(EState::Chase); Sound(RattleSounds, .8f); }
        break;
    case EState::Chase:
    {
        if (!Hero) { Move->MaxWalkSpeed = 0; Animator.Locomote(Speed); break; }
        // To its place beside its hero: round toward the hero's front, by its slot.
        const FVector HeroFwd = Hero->GetActorForwardVector().GetSafeNormal2D();
        const FVector Slot = HeroAt + HeroFwd.RotateAngleAxis(-SlotAngle, FVector::UpVector) * 100.f;
        const float Facing = FVector::DotProduct(ToHero.GetSafeNormal2D(), GetActorForwardVector());
        // Its fist reaches 63 cm ahead of its hips (SkeletonClipData::StrikeReach): it strikes from about a metre.
        if (Dist < 110.f && Facing > .4f && GetWorld()->GetTimeSeconds() >= AttackReadyAt && !bFrozen) { StartAttack(EAttack::Strike); break; }
        const FVector Goal = Dist < 150.f ? Here + ToHero.GetSafeNormal2D() * (Dist - 98.f) : Slot;
        Move->MaxWalkSpeed = ChaseSpeed * 1.1f;
        const FVector Want = Goal - Here;
        if (Want.Size2D() < 10.f || bFrozen) { Move->MaxWalkSpeed = 0; Face(ToHero, 200.f, DeltaSeconds); Animator.Locomote(Speed); break; }
        const FVector Dir = Steer(Want, Hero);
        AddMovementInput(Dir, FMath::Clamp(static_cast<float>(Want.Size2D()) / 40.f, .25f, 1.f));
        Face(Dist < 160.f ? ToHero : (Speed > 15.f ? GetVelocity() : Dir), 170.f, DeltaSeconds);
        Animator.Locomote(Speed);
        break;
    }
    case EState::Attack:
    {
        Move->MaxWalkSpeed = 0;
        const FAttackSpec& S = Spec(Attack);
        const float T = Animator.Time();
        if (T < S.Strike - .2f && Hero && !bFrozen)
            Face(ToHero.GetSafeNormal2D().RotateAngleAxis(S.Aim, FVector::UpVector), 260.f, DeltaSeconds);
        Animator.SetJaw(T < S.Strike - .1f ? (FMath::Fmod(StateTime * 14.f, 1.f) < .5f ? 11.f : 2.f) : T < S.Strike + .25f ? 24.f : 5.f);
        if (!bSwung && T >= S.Strike - .14f) { bSwung = true; Sound(SwingSounds, .8f); }
        if (!bStruck && Hero && T >= S.Strike - S.Before && T <= S.Strike + S.After)
        {
            const FVector Tip = StrikingTip(Attack);
            const float Gap = Hero->GapTo(Tip);
            LastStrikeGap = FMath::Min(LastStrikeGap, Gap);
            if (Gap < 16.f)
            {
                bStruck = true; ++Landed[static_cast<int32>(Attack)];
                const bool bBlocked = Hero->TakeSkeletonBlow(this, Tip, Random.FRand() < .3f);
                HitStop(bBlocked ? .07f : .05f);
                if (bBlocked) { Animator.Flinch(12.f, Random.FRandRange(-8.f, 8.f)); AttackReadyAt = GetWorld()->GetTimeSeconds() + .6f; }
                UE_LOG(LogTemp, Display, TEXT("CHUCK_SANCTUM_STRIKE hero=%s blocked=%d gap=%.1f"), Hero->GetRoleName(), bBlocked ? 1 : 0, Gap);
            }
        }
        if (T > S.Strike + S.After) bStruck = true;
        if (T >= Animator.Length() - .2f)
        {
            ++Strikes;
            AttackReadyAt = FMath::Max(AttackReadyAt, static_cast<float>(GetWorld()->GetTimeSeconds()) + Random.FRandRange(1.6f, 3.f));
            Animator.Play(TEXT("Idle"), true, 1.f, false, .3f);
            SetState(EState::Recover);
        }
        break;
    }
    case EState::Recover:
        Move->MaxWalkSpeed = 0;
        if (Hero) Face(ToHero, 120.f, DeltaSeconds);
        if (StateTime > .5f) SetState(EState::Chase);
        break;
    default: Move->MaxWalkSpeed = 0; break;
    }
}
