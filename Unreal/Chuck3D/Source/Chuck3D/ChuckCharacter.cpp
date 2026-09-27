#include "ChuckCharacter.h"
#include "ChuckAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "AnimationRuntime.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

AChuckCharacter::AChuckCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(15.0f, 32.5f);
    bUseControllerRotationYaw = false;
    auto* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = true;
    Movement->RotationRate = FRotator(0, 540, 0);
    Movement->MaxWalkSpeed = 95;
    Movement->MaxAcceleration = 550;
    // Constant braking over the authored WalkStop travel: 95^2 / (2 * 19 cm),
    // so a stop from full speed takes the clip's 0.4 s and is distance-matched.
    Movement->bUseSeparateBrakingFriction = true;
    Movement->BrakingFriction = 0;
    Movement->BrakingDecelerationWalking = 237.5f;
    Movement->JumpZVelocity = 170;
    Movement->GravityScale = 0.8f;
    Movement->AirControl = 0.35f;
    Movement->MaxStepHeight = 4;
    Movement->PerchRadiusThreshold = 1;
    Movement->SetWalkableFloorAngle(45);
    JumpMaxHoldTime = 0;

    // v1 character: SK_Chuck_Groomed (no geometric tufts) plus the strand groom.
    // -ChuckNoGroom swaps in SK_Chuck, whose geometric tufts stand in for fur.
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> GroomedAsset(TEXT("/Game/Characters/Chuck/V1/SK_Chuck_Groomed.SK_Chuck_Groomed"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> PlainAsset(TEXT("/Game/Characters/Chuck/V1/SK_Chuck.SK_Chuck"));
    PlainMesh = PlainAsset.Object;
    USkeletalMeshComponent* Body = GetMesh();
    Body->SetSkeletalMeshAsset(GroomedAsset.Object);
    Body->SetRelativeLocationAndRotation(FVector(0, 0, -32.5f), FRotator::ZeroRotator);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetAnimInstanceClass(UChuckAnimInstance::StaticClass());
    Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    static const TCHAR* ClipNames[] = {TEXT("Idle"), TEXT("WalkStart"), TEXT("WalkLoop"), TEXT("WalkStop"), TEXT("TurnLeft90"), TEXT("TurnRight90"), TEXT("JumpStart"), TEXT("JumpLoop"), TEXT("JumpLand")};
    for (const TCHAR* Name : ClipNames)
    {
        ConstructorHelpers::FObjectFinder<UAnimSequence> Clip(*FString::Printf(TEXT("/Game/Characters/Chuck/V1/Animations/AS_Chuck_%s.AS_Chuck_%s"), Name, Name));
        Clips.Add(Clip.Object);
    }
    static const TCHAR* Groups[] = {TEXT("Fur_Body"), TEXT("Fur_Back"), TEXT("Fur_Cream")};
    for (const TCHAR* Group : Groups)
    {
        ConstructorHelpers::FObjectFinder<UGroomAsset> GroomAsset(*FString::Printf(TEXT("/Game/Characters/Chuck/V1/GR_Chuck_%s.GR_Chuck_%s"), Group, Group));
        ConstructorHelpers::FObjectFinder<UGroomBindingAsset> Binding(*FString::Printf(TEXT("/Game/Characters/Chuck/V1/GB_Chuck_%s.GB_Chuck_%s"), Group, Group));
        ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(*FString::Printf(TEXT("/Game/Characters/Chuck/V1/M_%s.M_%s"), Group, Group));
        auto* Hair = CreateDefaultSubobject<UGroomComponent>(*FString::Printf(TEXT("Groom_%s"), Group));
        Hair->SetupAttachment(Body);
        Hair->SimulationSettings.bOverrideSettings = true;
        Hair->SimulationSettings.SolverSettings.bEnableSimulation = false;
        // Plain defaults: resources are created when the component registers.
        Hair->GroomAsset = GroomAsset.Object;
        Hair->BindingAsset = Binding.Object;
        Hair->SetMaterial(0, Material.Object);
        Hair->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Grooms.Add(Hair);
    }
    // Cigarette held in the left mouth corner (every goal image), a separate
    // prop on the jaw's socket bone so a future pickup can show or hide it.
    // The smoke wisp rises from the lit end and stays upright in world space.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CigaretteAsset(TEXT("/Game/Characters/Chuck/V1/Cigarette/SM_Cigarette.SM_Cigarette"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SmokeAsset(TEXT("/Game/Characters/Chuck/V1/Cigarette/SM_CigaretteSmoke.SM_CigaretteSmoke"));
    Cigarette = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cigarette"));
    Cigarette->SetupAttachment(Body, TEXT("socket_cigarette"));
    Cigarette->SetStaticMesh(CigaretteAsset.Object);
    Cigarette->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Smoke = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CigaretteSmoke"));
    Smoke->SetupAttachment(Cigarette);
    Smoke->SetStaticMesh(SmokeAsset.Object);
    Smoke->SetUsingAbsoluteRotation(true);
    Smoke->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Smoke->SetCastShadow(false);
    Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    Boom->SetupAttachment(GetRootComponent());
    Boom->SetUsingAbsoluteRotation(true);
    Boom->bDoCollisionTest = true;
    Boom->ProbeSize = 3;
    Boom->bEnableCameraLag = true;
    Boom->CameraLagSpeed = 14;
    Boom->CameraLagMaxDistance = 8;
    Boom->bUseCameraLagSubstepping = true;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
    Camera->FieldOfView = 65;
    UpdateCamera();
}

void AChuckCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (FParse::Param(FCommandLine::Get(), TEXT("ChuckNoGroom")))
    {
        for (UGroomComponent* Hair : Grooms)
        {
            Hair->DestroyComponent();
        }
        Grooms.Reset();
        GetMesh()->SetSkeletalMeshAsset(PlainMesh);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("ChuckNoCigarette")))
    {
        Smoke->DestroyComponent(); Cigarette->DestroyComponent();
        Smoke = Cigarette = nullptr;
    }
    else if (const USkeletalMesh* Asset = GetMesh()->GetSkeletalMeshAsset())
    {
        // The prop is modelled along +X from the filter end. Exported bones all
        // point along one local axis; read it from the imported rest pose
        // (thigh -> knee) rather than hard-coding the FBX axis conversion.
        const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
        const int32 Thigh = Ref.FindBoneIndex(TEXT("thigh_L")), Knee = Ref.FindBoneIndex(TEXT("calf_L"));
        if (Thigh != INDEX_NONE && Knee != INDEX_NONE)
        {
            const FTransform A = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Thigh);
            const FTransform B = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Knee);
            const FVector BoneAxis = A.InverseTransformVectorNoScale(B.GetLocation() - A.GetLocation()).GetSafeNormal();
            Cigarette->SetRelativeRotation(FQuat::FindBetweenNormals(FVector::XAxisVector, BoneAxis));
        }
        if (const UStaticMesh* Prop = Cigarette->GetStaticMesh())
        {
            Smoke->SetRelativeLocation(FVector(Prop->GetBoundingBox().Max.X, 0, 0)); // the lit end
        }
    }
    // Stance locks are world positions: pose after this frame's movement.
    GetMesh()->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
    for (int32 I = 0; I < Clips.Num(); ++I)
    {
        UE_LOG(LogTemp, Display, TEXT("CHUCK_CLIP %s length=%.4f"), Clips[I] ? *Clips[I]->GetName() : TEXT("missing"), Clips[I] ? Clips[I]->GetPlayLength() : -1.f);
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_V1_RUNTIME grooms=%d mesh=%s"), Grooms.Num(), *GetNameSafe(GetMesh()->GetSkeletalMeshAsset()));
    UpdateCamera();
    if (auto* PC = Cast<APlayerController>(Controller))
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
}
UChuckAnimInstance* AChuckCharacter::GetChuckAnim() const { return Cast<UChuckAnimInstance>(GetMesh()->GetAnimInstance()); }
int32 AChuckCharacter::GetGroomCount() const { return Grooms.Num(); }
const TCHAR* AChuckCharacter::GetGaitName() const
{
    static const TCHAR* Names[] = {TEXT("Idle"), TEXT("Start"), TEXT("Loop"), TEXT("Stop"), TEXT("Turn"), TEXT("Air"), TEXT("Land")};
    return Names[static_cast<int32>(Gait)];
}
void AChuckCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis("Forward", this, &AChuckCharacter::Forward);
    Input->BindAxis("Right", this, &AChuckCharacter::Right);
    Input->BindAxis("LookMouse", this, &AChuckCharacter::MouseLook);
    Input->BindAxis("TurnKeys", this, &AChuckCharacter::Turn);
    Input->BindAxis("PitchMouse", this, &AChuckCharacter::MousePitch);
    Input->BindAxis("PitchStick", this, &AChuckCharacter::StickPitch);
    Input->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
    Input->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
    Input->BindAction("Camera", IE_Pressed, this, &AChuckCharacter::ToggleCamera);
    Input->BindAction("Recenter", IE_Pressed, this, &AChuckCharacter::Recenter);
    Input->BindAction("Reset", IE_Pressed, this, &AChuckCharacter::ResetToDock);
    Input->BindAction("Quit", IE_Pressed, this, &AChuckCharacter::Quit);
}
void AChuckCharacter::Forward(float Value) { AddMovementInput(FRotator(0,ViewYaw,0).Vector(),Value); }
void AChuckCharacter::Right(float Value) { AddMovementInput(FRotationMatrix(FRotator(0,ViewYaw,0)).GetUnitAxis(EAxis::Y),Value); }
void AChuckCharacter::MouseLook(float Value) { ViewYaw = FRotator::NormalizeAxis(ViewYaw + Value * 0.8f); }
void AChuckCharacter::Turn(float Value) { ViewYaw = FRotator::NormalizeAxis(ViewYaw + Value * 100 * GetWorld()->GetDeltaSeconds()); }
void AChuckCharacter::MousePitch(float Value) { if(!bElevated) ViewPitch = FMath::Clamp(ViewPitch + Value * 0.8f,-25.f,40.f); }
void AChuckCharacter::StickPitch(float Value) { if(!bElevated) ViewPitch = FMath::Clamp(ViewPitch + Value * 65 * GetWorld()->GetDeltaSeconds(),-25.f,40.f); }
void AChuckCharacter::ToggleCamera() { bElevated = !bElevated; }
void AChuckCharacter::Recenter() { ViewYaw = GetActorRotation().Yaw; ViewPitch = 0; }
void AChuckCharacter::UpdateCamera(float DeltaSeconds)
{
    const float Target = bElevated ? 1.f : 0.f;
    CameraBlend = DeltaSeconds > 0 ? FMath::FInterpTo(CameraBlend, Target, DeltaSeconds, 7.f) : Target;
    Boom->TargetArmLength = FMath::Lerp(220.f, 400.f, CameraBlend);
    // The pivot ignores the arc of a jump (a 65 cm character's hop otherwise
    // bobs the whole view) but follows landings on a new level and falls.
    const float ActorZ = GetActorLocation().Z;
    if (!bFollowReady || DeltaSeconds <= 0) { FollowZ = ActorZ; bFollowReady = true; }
    else
    {
        const bool bAirborne = GetCharacterMovement()->IsFalling();
        FollowZ = FMath::FInterpTo(FollowZ, ActorZ, DeltaSeconds, bAirborne ? 1.5f : 8.f);
        FollowZ = FMath::Clamp(FollowZ, ActorZ - 40.f, ActorZ + 40.f);
    }
    // Rat-height: chest-high pivot and a 5 degree tilt put the lens about 75 cm
    // up, just over the ears, so Chuck sits low in frame instead of covering
    // the view ahead. Mouse/stick pitch still turns the lens, never the boom.
    Boom->SetRelativeLocation(FVector(0,0,FMath::Lerp(22.f,16.f,CameraBlend) + FollowZ - ActorZ));
    const FRotator TargetRotation(FMath::Lerp(-5.f, -48.f, CameraBlend), ViewYaw, 0);
    Boom->SetWorldRotation(DeltaSeconds > 0 ? FMath::RInterpTo(Boom->GetComponentRotation(),TargetRotation,DeltaSeconds,18.f) : TargetRotation);
    Camera->SetRelativeRotation(FRotator(ViewPitch*(1-CameraBlend),0,0));
    Camera->FieldOfView = FMath::Lerp(78.f,65.f,CameraBlend);
}
void AChuckCharacter::ResetToDock()
{
    GetCharacterMovement()->StopMovementImmediately();
    SetActorLocation(StartLocation(), false, nullptr, ETeleportType::TeleportPhysics);
    SetActorRotation(FRotator::ZeroRotator);
    ViewYaw = 0;
    ViewPitch = 0;
    Gait = EGait::Idle;
    Base = Fading = EClip::Idle;
    BaseTime = FadingTime = FadeWeight = StateTime = StartDistance = WalkPhase = StopTravel = 0;
    bStopPending = bStopMirror = false;
    GetCharacterMovement()->BrakingDecelerationWalking = 237.5f;
    if (GetCharacterMovement()->MovementMode == MOVE_None) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    PreviousMotionLocation = GetActorLocation();
    bFollowReady = false;
    UpdateCamera();
}

float AChuckCharacter::Period(EClip Clip) const
{
    // The importer keeps the full loop period (Idle 2.0 s, WalkLoop 0.3 s,
    // JumpLoop 0.4 s; logged as CHUCK_CLIP) and interpolates back to frame 0.
    const bool bLoop = Clip == EClip::Idle || Clip == EClip::WalkLoop || Clip == EClip::JumpLoop;
    const UAnimSequence* Sequence = Clips[static_cast<int32>(Clip)];
    return bLoop && Sequence ? Sequence->GetPlayLength() : 0.f;
}

void AChuckCharacter::SetClip(EClip Clip, float Time, float FadeSeconds)
{
    if (FadeSeconds > 0)
    {
        Fading = Base;
        FadingTime = BaseTime;
        FadeWeight = 1;
        FadeRate = 1 / FadeSeconds;
    }
    Base = Clip;
    BaseTime = Time;
    StateTime = 0;
}

float AChuckCharacter::FindGround(const FVector& Near, float Fallback) const
{
    // Visual contact only: it cannot climb a crate or bridge a gap.
    const float FloorZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckFootGround), false, this);
    const FVector Top(Near.X, Near.Y, FloorZ + 8.f);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 16), ECC_Visibility, Query)
        && Hit.ImpactNormal.Z >= .7f)
        return Hit.ImpactPoint.Z;
    return Fallback;
}

namespace
{
    // Capsule travel of the authored start/stop clips (Tools/build_chuck_v1.py,
    // manifest capsule_travel_cm_per_frame), u = t / 0.4 s, 19 cm each.
    float StartTravel(float U) { return 38.f * (U * U * U - U * U * U * U * .5f); }
    float StopTravelAt(float U) { return 38.f * (U - U * U * U + U * U * U * U * .5f); }
    float InvertTravel(float (*Travel)(float), float Distance)
    {
        float Low = 0, High = 1;
        for (int32 I = 0; I < 24; ++I)
        {
            const float U = (Low + High) * .5f;
            (Travel(U) < Distance ? Low : High) = U;
        }
        return (Low + High) * .2f;
    }
    constexpr float ClipTravel = 19.f;
    constexpr float WalkStride = 28.5f;
    constexpr float WalkPeriod = .3f;
    constexpr float StopDeceleration = 237.5f;
    constexpr float LandDeceleration = 1000.f;
    constexpr float TurnMinAngle = 60.f;
    constexpr float TurnEnd = .55f; // after the last plant at 0.52 s

    // Manifest stance intervals (s), foot_L then foot_R, end marked by a negative.
    struct FStance { float L[6]; float R[6]; float End; };
    constexpr FStance StartStance{{0.f, .28f, -1}, {0.f, .1f, .25f, .4f, -1}, .4f};
    constexpr FStance StopStance{{0.f, .2f, .36f, .5f, -1}, {0.f, .03f, .18f, .5f, -1}, .5f};
    constexpr FStance StopStanceMirrored{{0.f, .03f, .18f, .5f, -1}, {0.f, .2f, .36f, .5f, -1}, .5f};
    constexpr FStance TurnLeftStance{{0.f, .04f, .2f, .36f, .52f, .6667f}, {0.f, .2f, .36f, .6667f, -1}, .6667f};
    constexpr FStance TurnRightStance{{0.f, .2f, .36f, .6667f, -1}, {0.f, .04f, .2f, .36f, .52f, .6667f}, .6667f};
    bool InStance(const float* Intervals, float End, float Time)
    {
        // Trimmed at interior plant/lift events so locks never catch a moving paw.
        for (int32 I = 0; I + 1 < 6 && Intervals[I] >= 0 && Intervals[I + 1] >= 0; I += 2)
        {
            const float A = Intervals[I] > 0 ? Intervals[I] + .02f : -1.f;
            const float B = Intervals[I + 1] < End ? Intervals[I + 1] - .02f : 1e3f;
            if (Time >= A && Time <= B) return true;
        }
        return false;
    }
    // TurnLeft90/TurnRight90 capsule yaw per frame (magnitude, degrees).
    constexpr float TurnYaw[] = {0.f, .2045f, 2.3867f, 6.6667f, 12.6828f, 20.0733f, 28.4766f, 37.5309f, 46.8745f, 56.1458f,
        64.9831f, 73.0247f, 79.9089f, 85.2739f, 88.7582f, 90.f};
    float TurnProfile(float Time)
    {
        const float Frame = FMath::Clamp(Time * 30.f, 0.f, 15.f);
        const int32 I = FMath::Min(FMath::FloorToInt(Frame), 14);
        return FMath::Lerp(TurnYaw[I], TurnYaw[I + 1], Frame - I);
    }
}

void AChuckCharacter::UpdateMotion(float DeltaSeconds)
{
    auto* Anim = GetChuckAnim();
    if (!Anim || Clips.Contains(nullptr)) return;
    auto* Movement = GetCharacterMovement();
    const bool bAirborne = Movement->IsFalling();
    const float Speed = GetVelocity().Size2D();
    const bool bInput = Movement->GetCurrentAcceleration().SizeSquared2D() > 1.f;
    const FVector Location = GetActorLocation();
    float Travel = FVector::Dist2D(Location, PreviousMotionLocation);
    if (Travel > 50.f) Travel = 0; // teleport or reset
    PreviousMotionLocation = Location;
    StateTime += DeltaSeconds;
    FadeWeight = FMath::Max(0.f, FadeWeight - FadeRate * DeltaSeconds);
    const float Length = Clips[static_cast<int32>(Base)]->GetPlayLength();

    // Distance-matched gait: start, walk and stop clips advance by travelled
    // distance, so the clip's stance paw moves exactly with the ground.
    if (bAirborne)
    {
        if (Gait != EGait::Air)
        {
            // Takeoff is runtime-driven, so skip the clip's ground crouch and
            // start at its extension onto the toes.
            Gait = EGait::Air;
            SetClip(EClip::JumpStart, Clips[static_cast<int32>(EClip::JumpStart)]->GetPlayLength() * .5f, .06f);
        }
        BaseTime += DeltaSeconds;
        if (Base == EClip::JumpStart && BaseTime >= Length)
            SetClip(EClip::JumpLoop, 0, .1f);
    }
    else if (Gait == EGait::Air)
    {
        Gait = EGait::Land; SetClip(EClip::JumpLand, 0, .06f);
    }
    else if (Gait == EGait::Turn)
    {
        // Turn in place: the capsule yaw follows the clip's profile, scaled to
        // the requested angle; movement resumes after the last plant.
        BaseTime += DeltaSeconds;
        SetActorRotation(FRotator(0, TurnStartYaw + TurnProfile(BaseTime) * FMath::Abs(TurnDelta) / 90.f * FMath::Sign(TurnDelta), 0));
        if (BaseTime >= TurnEnd)
        {
            Movement->SetMovementMode(MOVE_Walking);
            if (GetLastMovementInputVector().SizeSquared2D() > 0) { Gait = EGait::Start; StartDistance = 0; SetClip(EClip::WalkStart, 0, .15f); }
            else { Gait = EGait::Idle; SetClip(EClip::Idle, 0, .15f); }
        }
    }
    else if (Gait == EGait::Land)
    {
        BaseTime += DeltaSeconds;
        if (bInput && Speed > 10.f && StateTime > .15f) { Gait = EGait::Loop; WalkPhase = 0; SetClip(EClip::WalkLoop, 0, .15f); }
        else if (BaseTime >= Length) { Gait = EGait::Idle; SetClip(EClip::Idle, 0, .2f); }
    }
    else if (Gait == EGait::Idle)
    {
        BaseTime += DeltaSeconds;
        const float Delta = bInput ? FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, Movement->GetCurrentAcceleration().Rotation().Yaw) : 0.f;
        if (bInput && Speed < 10.f && FMath::Abs(Delta) > TurnMinAngle)
        {
            // Unreal yaw is clockwise from above: a positive delta turns right.
            Gait = EGait::Turn; TurnStartYaw = GetActorRotation().Yaw; TurnDelta = Delta;
            SetClip(Delta > 0 ? EClip::TurnRight90 : EClip::TurnLeft90, 0, .12f);
            Movement->StopMovementImmediately();
            Movement->DisableMovement();
        }
        else if (Speed > 3.f) { Gait = EGait::Start; StartDistance = 0; SetClip(EClip::WalkStart, 0, .12f); }
    }
    else if (Gait == EGait::Stop)
    {
        if (bInput && Speed > 3.f) { Gait = EGait::Loop; WalkPhase = 0; SetClip(EClip::WalkLoop, 0, .15f); }
        else
        {
            StopTravel += Travel;
            BaseTime = FMath::Max(BaseTime, InvertTravel(StopTravelAt, FMath::Min(StopTravel, ClipTravel)));
            if (Speed < 1.f) BaseTime += DeltaSeconds; // blocked or already still: finish on time
            if (BaseTime >= Length) { Gait = EGait::Idle; SetClip(EClip::Idle, 0, .1f); }
        }
    }
    else if (Speed < 3.f && !bInput)
    {
        bStopPending = false;
        Gait = EGait::Idle; SetClip(EClip::Idle, 0, .25f);
    }
    else
    {
        // Released input: WalkStop begins from WalkLoop frame 0 (left paw
        // planted), or its mirror at half stride. Coast to the next of those,
        // at most half a stride, then brake through the stop.
        bStopPending = !bInput;
        bool bEnterStop = false;
        if (Gait == EGait::Start)
        {
            StartDistance += Travel;
            BaseTime = InvertTravel(StartTravel, StartDistance);
            if (StartDistance >= ClipTravel)
            {
                // WalkStart ends exactly on WalkLoop frame 0.
                Gait = EGait::Loop; WalkPhase = (StartDistance - ClipTravel) / WalkStride;
                SetClip(EClip::WalkLoop, 0, 0);
                bEnterStop = bStopPending;
                bStopMirror = false;
            }
        }
        else if (Gait == EGait::Loop)
        {
            const float Previous = WalkPhase;
            WalkPhase = FMath::Frac(WalkPhase + Travel / WalkStride);
            if (bStopPending && (WalkPhase < Previous || (Previous < .5f && WalkPhase >= .5f)))
            {
                bEnterStop = true;
                bStopMirror = WalkPhase >= .5f;
            }
        }
        if (bEnterStop)
        {
            // Enter where the stop's remaining authored travel equals this
            // speed's braking distance (the start of the clip at full speed).
            bStopPending = false;
            Gait = EGait::Stop;
            StopTravel = FMath::Max(0.f, ClipTravel - Speed * Speed / (2.f * StopDeceleration));
            SetClip(EClip::WalkStop, InvertTravel(StopTravelAt, StopTravel), .06f);
        }
    }
    if (Gait == EGait::Loop)
    {
        BaseTime = WalkPhase * WalkPeriod;
    }
    else if (Gait != EGait::Start)
    {
        bStopPending = false;
    }
    // A landing without input absorbs its momentum within a few cm, before the
    // landing paws lock (0.1 s), instead of walking on into a stop.
    Movement->BrakingDecelerationWalking = bStopPending ? 0.f : (Gait == EGait::Land ? LandDeceleration : StopDeceleration);

    FChuckAnimParams& P = Anim->Params;
    P.ClipA = Clips[static_cast<int32>(Base)];
    P.TimeA = BaseTime;
    P.PeriodA = Period(Base);
    P.bMirrorA = Base == EClip::WalkStop && bStopMirror;
    P.ClipB = FadeWeight > 0 ? Clips[static_cast<int32>(Fading)] : nullptr;
    P.TimeB = FadingTime;
    P.PeriodB = Period(Fading);
    P.bMirrorB = Fading == EClip::WalkStop && bStopMirror;
    P.WeightB = FadeWeight;
    P.bFootIK = !bAirborne;
    P.bAllowSettle = Gait == EGait::Idle;
    // Stance from the manifest intervals of whichever clip dominates. WalkLoop:
    // foot_L 0-0.18 s, foot_R 0.15-0.30 and 0-0.03 s of 0.3 s, trimmed likewise.
    const bool bLoopDominant = (Gait == EGait::Loop && FadeWeight < .5f) || (Gait == EGait::Stop && FadeWeight >= .5f);
    const bool bStanding = Gait == EGait::Idle || (Gait == EGait::Land && StateTime > .1f);
    const FStance* Clip = nullptr;
    if (Gait == EGait::Start) Clip = &StartStance;
    else if (Gait == EGait::Stop && !bLoopDominant) Clip = bStopMirror ? &StopStanceMirrored : &StopStance;
    else if (Gait == EGait::Turn) Clip = Base == EClip::TurnLeft90 ? &TurnLeftStance : &TurnRightStance;
    for (int32 I = 0; I < 2; ++I)
    {
        bool bStance = bStanding;
        if (Clip) bStance = InStance(I == 0 ? Clip->L : Clip->R, Clip->End, BaseTime);
        else if (bLoopDominant) bStance = I == 0 ? (WalkPhase > .02f && WalkPhase < .58f) : (WalkPhase > .52f || WalkPhase < .08f);
        P.bStance[I] = bStance;
    }

    // Place the mesh on the traced ground under the capsule, then offset each
    // paw by its own traced ground and drop the pelvis for a lower paw.
    const float CapsuleBottom = Location.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Ground = bAirborne ? CapsuleBottom : FindGround(Location, CapsuleBottom);
    MeshDrop = FMath::FInterpTo(MeshDrop, FMath::Clamp(CapsuleBottom - Ground, 0.f, 4.f), DeltaSeconds, 20.f);
    GetMesh()->SetRelativeLocation(FVector(0, 0, -32.5f - MeshDrop));
    const FChuckAnimResult Last = Anim->GetResult();
    const float MeshZ = CapsuleBottom - MeshDrop;
    float Lowest = 0;
    for (int32 I = 0; I < 2; ++I)
    {
        const FVector Near = Last.Evaluations ? Last.BallWorld[I] : Location;
        P.GroundOffset[I] = bAirborne ? 0.f : FMath::Clamp(FindGround(Near, MeshZ) - MeshZ, -6.f, 6.f);
        Lowest = FMath::Min(Lowest, P.GroundOffset[I]);
    }
    P.PelvisOffset = FMath::Max(Lowest, -4.f);
}

void AChuckCharacter::Quit() { UKismetSystemLibrary::QuitGame(this, Cast<APlayerController>(Controller), EQuitPreference::Quit, false); }
void AChuckCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateCamera(DeltaSeconds);
    // When collision pulls the lens inside Chuck, avoid an obstructing head/jacket.
    GetMesh()->SetVisibility(FVector::Dist(Camera->GetComponentLocation(),GetActorLocation()) > 70.f,true);
    UpdateMotion(DeltaSeconds);
    if (GetActorLocation().Z < -100) ResetToDock();
}
