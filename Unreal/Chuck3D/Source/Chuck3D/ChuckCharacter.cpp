#include "ChuckCharacter.h"
#include "ChuckAnimInstance.h"
#include "ChuckClipData.h"
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
    Movement->MaxAcceleration = 550;
    // Saunter speed and constant braking over the authored WalkStop travel
    // (v^2 / 2d), both from the clip manifest (ChuckClipData.h), so a stop
    // from full speed takes the clip's own time and is distance-matched.
    Movement->MaxWalkSpeed = ChuckClipData::WalkSpeed;
    Movement->bUseSeparateBrakingFriction = true;
    Movement->BrakingFriction = 0;
    Movement->BrakingDecelerationWalking = ChuckClipData::WalkSpeed * ChuckClipData::WalkSpeed / (2.f * ChuckClipData::StopTravel);
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
    static const TCHAR* ClipNames[] = {TEXT("Idle"), TEXT("WalkStart"), TEXT("WalkLoop"), TEXT("WalkStop"), TEXT("TurnLeft90"), TEXT("TurnRight90"), TEXT("JumpStart"), TEXT("JumpLoop"), TEXT("JumpLand"), TEXT("Roll"), TEXT("SideJumpLeft"), TEXT("SideJumpRight"), TEXT("RunLoop"), TEXT("RunJump")};
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
    static const TCHAR* Names[] = {TEXT("Idle"), TEXT("Start"), TEXT("Loop"), TEXT("Stop"), TEXT("Turn"), TEXT("Air"), TEXT("Land"), TEXT("Roll"), TEXT("SideJump")};
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
    Input->BindAction("Recenter", IE_Pressed, this, &AChuckCharacter::Recenter);
    Input->BindAction("Dodge", IE_Pressed, this, &AChuckCharacter::Dodge);
    Input->BindAction("Run", IE_Pressed, this, &AChuckCharacter::RunPressed);
    Input->BindAction("Reset", IE_Pressed, this, &AChuckCharacter::ResetToDock);
    Input->BindAction("Quit", IE_Pressed, this, &AChuckCharacter::Quit);
}
// A dodge owns the capsule; the stick is still read to choose the next move.
void AChuckCharacter::Forward(float Value) { InputForward = Value; if (!IsDodging()) AddMovementInput(FRotator(0,ViewYaw,0).Vector(),Value); }
void AChuckCharacter::Right(float Value) { InputRight = Value; if (!IsDodging()) AddMovementInput(FRotationMatrix(FRotator(0,ViewYaw,0)).GetUnitAxis(EAxis::Y),Value); }
void AChuckCharacter::Dodge()
{
    // Action events dispatch before this frame's axis events: read the stick
    // directly so a direction pressed together with C counts.
    if (InputComponent) { InputRight = InputComponent->GetAxisValue(TEXT("Right")); InputForward = InputComponent->GetAxisValue(TEXT("Forward")); }
    DodgeToward(FVector2D(InputRight, InputForward));
}
namespace
{
    // One orbit spans both framings the prototype compared (GTA-style, no
    // switch button): look pitch -48 is the elevated view (400 cm boom, FOV 65)
    // and -5 the rat-height view (220 cm boom, lens just over the ears, FOV 78).
    // Between them boom length, pivot and FOV blend with the pitch; below -48
    // the camera climbs a little higher; above -5 the boom stays level and
    // only the lens looks up, so it never dips under the pier.
    constexpr float ElevatedPitch = -48.f;
    constexpr float RatPitch = -5.f;
    constexpr float HighestPitch = -60.f;
    constexpr float LookUpPitch = 30.f;
    constexpr float AutoFollowDelay = 1.2f;  // s without look input
    constexpr float AutoFollowRate = 1.5f;   // 1/s at full walking speed
}
void AChuckCharacter::MouseLook(float Value) { if (bLookLocked) return; ViewYaw = FRotator::NormalizeAxis(ViewYaw + Value * 0.8f); if (Value != 0) LookIdle = 0; }
void AChuckCharacter::Turn(float Value) { if (bLookLocked) return; ViewYaw = FRotator::NormalizeAxis(ViewYaw + Value * 100 * GetWorld()->GetDeltaSeconds()); if (Value != 0) LookIdle = 0; }
void AChuckCharacter::MousePitch(float Value) { if (bLookLocked) return; LookPitch = FMath::Clamp(LookPitch + Value * 0.8f, HighestPitch, LookUpPitch); if (Value != 0) LookIdle = 0; }
void AChuckCharacter::StickPitch(float Value) { if (bLookLocked) return; LookPitch = FMath::Clamp(LookPitch + Value * 80 * GetWorld()->GetDeltaSeconds(), HighestPitch, LookUpPitch); if (Value != 0) LookIdle = 0; }
bool AChuckCharacter::IsElevated() const { return LookPitch < (ElevatedPitch + RatPitch) * .5f; }
void AChuckCharacter::ToggleCamera() { LookPitch = IsElevated() ? RatPitch : ElevatedPitch; }
void AChuckCharacter::Recenter() { ViewYaw = GetActorRotation().Yaw; LookPitch = FMath::Min(LookPitch, RatPitch); }
void AChuckCharacter::UpdateCamera(float DeltaSeconds)
{
    SmoothLook = DeltaSeconds > 0 ? FMath::FInterpTo(SmoothLook, LookPitch, DeltaSeconds, 14.f) : LookPitch;
    const float BoomPitch = FMath::Clamp(SmoothLook, HighestPitch, RatPitch);
    const float CameraBlend = FMath::Clamp((BoomPitch - RatPitch) / (ElevatedPitch - RatPitch), 0.f, 1.f);
    Boom->TargetArmLength = FMath::Lerp(220.f, 400.f, CameraBlend);
    // GTA-style auto-follow: after a moment without look input the orbit eases
    // behind Chuck while he walks away from the camera. Strafing or walking
    // toward the lens leaves it alone.
    LookIdle += DeltaSeconds;
    const FVector Velocity = GetVelocity();
    const float Speed = Velocity.Size2D();
    if (DeltaSeconds > 0 && LookIdle > AutoFollowDelay && Speed > 10.f && GetCharacterMovement()->IsMovingOnGround())
    {
        const float Along = FVector::DotProduct(Velocity.GetSafeNormal2D(), FRotator(0, ViewYaw, 0).Vector());
        const float Weight = FMath::Clamp((Along - .5f) / .5f, 0.f, 1.f) * FMath::Min(Speed / ChuckClipData::WalkSpeed, 1.f);
        const float Behind = FMath::FindDeltaAngleDegrees(ViewYaw, GetActorRotation().Yaw);
        ViewYaw = FRotator::NormalizeAxis(ViewYaw + Behind * FMath::Min(1.f, DeltaSeconds * AutoFollowRate * Weight));
    }
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
    // Rat-height end: chest-high pivot and a 5 degree tilt put the lens about
    // 75 cm up, just over the ears, so Chuck sits low in frame instead of
    // covering the view ahead. Looking further up turns the lens, not the boom.
    Boom->SetRelativeLocation(FVector(0,0,FMath::Lerp(22.f,16.f,CameraBlend) + FollowZ - ActorZ));
    const FRotator TargetRotation(BoomPitch, ViewYaw, 0);
    Boom->SetWorldRotation(DeltaSeconds > 0 ? FMath::RInterpTo(Boom->GetComponentRotation(),TargetRotation,DeltaSeconds,18.f) : TargetRotation);
    Camera->SetRelativeRotation(FRotator(SmoothLook - BoomPitch,0,0));
    Camera->FieldOfView = FMath::Lerp(78.f,65.f,CameraBlend);
}
void AChuckCharacter::ResetToDock()
{
    GetCharacterMovement()->StopMovementImmediately();
    SetActorLocation(StartLocation(), false, nullptr, ETeleportType::TeleportPhysics);
    SetActorRotation(FRotator::ZeroRotator);
    ViewYaw = 0;
    LookPitch = SmoothLook = FMath::Min(LookPitch, RatPitch);  // keep the chosen height
    Gait = EGait::Idle;
    Base = Fading = EClip::Idle;
    BaseTime = FadingTime = FadeWeight = StateTime = StartDistance = WalkPhase = StopTravel = 0;
    bStopPending = bStopMirror = bDodgeLaunched = bDodgeLanded = false;
    InputForward = InputRight = 0;  // refreshed every frame while input is live
    bRunJump = bHardLanding = false;
    GetCharacterMovement()->BrakingDecelerationWalking = ChuckClipData::WalkSpeed * ChuckClipData::WalkSpeed / (2.f * ChuckClipData::StopTravel);
    if (GetCharacterMovement()->MovementMode == MOVE_None) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    PreviousMotionLocation = GetActorLocation();
    bFollowReady = false;
    UpdateCamera();
}

float AChuckCharacter::Period(EClip Clip) const
{
    // The importer keeps the full loop period (Idle 2.0 s, WalkLoop 0.3 s,
    // JumpLoop 0.4 s; logged as CHUCK_CLIP) and interpolates back to frame 0.
    const bool bLoop = Clip == EClip::Idle || Clip == EClip::WalkLoop || Clip == EClip::RunLoop || Clip == EClip::JumpLoop;
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
    using namespace ChuckClipData;  // generated from the clip manifest
    // Capsule travel of the authored start/stop clips (smoothstep speed ramps,
    // Tools/build_chuck_v1.py `travel`): u = t / duration.
    float StartTravelAt(float U) { return WalkSpeed * StartDuration * (U * U * U - U * U * U * U * .5f); }
    float StopTravelAt(float U) { return WalkSpeed * StopDuration * (U - U * U * U + U * U * U * U * .5f); }
    float InvertTravel(float (*Travel)(float), float Duration, float Distance)
    {
        float Low = 0, High = 1;
        for (int32 I = 0; I < 24; ++I)
        {
            const float U = (Low + High) * .5f;
            (Travel(U) < Distance ? Low : High) = U;
        }
        return (Low + High) * .5f * Duration;
    }
    constexpr float StopDeceleration = WalkSpeed * WalkSpeed / (2.f * StopTravel);
    constexpr float LandDeceleration = 1000.f;
    // Letting go mid-run brakes down to the saunter (about 22 cm), then the
    // authored WalkStop takes over.
    constexpr float RunBrake = 700.f;
    // A running landing without the stick: shed the run within a few cm,
    // before the landing paws lock (0.1 s).
    constexpr float RunLandDeceleration = 2500.f;
    // Saunter -> run blend on the capsule speed; the stride follows it.
    float RunBlendAt(float Speed) { return FMath::Clamp((Speed - WalkSpeed) / (RunSpeed - WalkSpeed), 0.f, 1.f); }
    constexpr float TurnMinAngle = 60.f;
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
    float RollTravelAt(float Time)
    {
        const float Frame = FMath::Clamp(Time * 30.f, 0.f, static_cast<float>(RollFrames));
        const int32 I = FMath::Min(FMath::FloorToInt(Frame), RollFrames - 1);
        return FMath::Lerp(RollTravel[I], RollTravel[I + 1], Frame - I);
    }
    float TurnProfile(float Time)
    {
        const float Frame = FMath::Clamp(Time * 30.f, 0.f, static_cast<float>(TurnYawFrames));
        const int32 I = FMath::Min(FMath::FloorToInt(Frame), TurnYawFrames - 1);
        return FMath::Lerp(TurnYaw[I], TurnYaw[I + 1], Frame - I);
    }
}

void AChuckCharacter::DodgeToward(FVector2D Stick)
{
    auto* Movement = GetCharacterMovement();
    if (IsDodging() || Movement->IsFalling()) return;
    // A dodge cuts a turn in place short (pressing a direction from standstill
    // starts one).
    if (Gait == EGait::Turn) Movement->SetMovementMode(MOVE_Walking);
    const FRotator View(0, ViewYaw, 0);
    const FVector Ahead = View.Vector(), Side = FRotationMatrix(View).GetUnitAxis(EAxis::Y);
    if (FMath::Abs(Stick.X) > .5f && FMath::Abs(Stick.X) > FMath::Abs(Stick.Y))
    {
        // Side jump: square up down the camera and spring toward the stick
        // (Unreal +Y is right; the source clip's +Y is Chuck's left).
        SetActorRotation(View);
        DodgeDirection = Side * FMath::Sign(Stick.X);
        Movement->StopMovementImmediately();
        Gait = EGait::SideJump;
        SetClip(Stick.X > 0 ? EClip::SideJumpRight : EClip::SideJumpLeft, 0, .08f);
    }
    else
    {
        // Roll toward the stick, or straight ahead. Enter the dive where its
        // speed matches the current pace, so a roll from the saunter flows.
        const FVector Wanted = Ahead * Stick.Y + Side * Stick.X;
        DodgeDirection = Wanted.SizeSquared2D() > .04f ? Wanted.GetSafeNormal2D() : GetActorForwardVector().GetSafeNormal2D();
        SetActorRotation(DodgeDirection.Rotation());
        const float Pace = GetVelocity().Size2D();
        float Entry = 0;
        while (Entry < .1f && (RollTravelAt(Entry + 1.f / 60.f) - RollTravelAt(Entry)) * 60.f < Pace) Entry += 1.f / 120.f;
        Gait = EGait::Roll;
        SetClip(EClip::Roll, Entry, .08f);
        RollDone = RollTravelAt(Entry);
    }
    bDodgeLaunched = bDodgeLanded = bStopPending = false;
}
void AChuckCharacter::FinishDodge()
{
    // Back to the aplomb stance the clips end on; saunter on if the stick is held.
    if (FVector2D(InputRight, InputForward).SizeSquared() > .04f) { Gait = EGait::Start; StartDistance = 0; SetClip(EClip::WalkStart, 0, .15f); }
    else { Gait = EGait::Idle; SetClip(EClip::Idle, 0, .2f); }
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
    const EGait GaitBefore = Gait;
    if (!IsDodging()) Movement->MaxWalkSpeed = bRunHeld ? RunSpeed : WalkSpeed;
    const float Length = Clips[static_cast<int32>(Base)]->GetPlayLength();

    // Distance-matched gait: start, walk and stop clips advance by travelled
    // distance, so the clip's stance paw moves exactly with the ground.
    if (Gait == EGait::SideJump)
    {
        // Launch at the clip's takeoff, hold just before its landing while in
        // the air, stop the capsule at touchdown (the clip carries the hips on).
        BaseTime += DeltaSeconds;
        if (!bDodgeLaunched)
        {
            if (BaseTime >= SideTakeoff)
            {
                LaunchCharacter(DodgeDirection * SideLateralSpeed + FVector(0, 0, SideVerticalSpeed), true, true);
                bDodgeLaunched = true;
            }
        }
        else if (!bDodgeLanded)
        {
            if (!bAirborne && BaseTime > SideTakeoff + .05f)
            {
                bDodgeLanded = true;
                Movement->StopMovementImmediately();
                BaseTime = FMath::Max(BaseTime, SideLand);
            }
            else BaseTime = FMath::Min(BaseTime, SideLand - 1.f / 30.f);
        }
        // Stick held: walk or run on as soon as the landing has settled a little.
        const bool bStick = FVector2D(InputRight, InputForward).SizeSquared() > .04f;
        if (bDodgeLanded && (BaseTime >= Length || (bStick && BaseTime >= SideLand + .15f))) FinishDodge();
    }
    else if (bAirborne)
    {
        if (Gait != EGait::Air)
        {
            Gait = EGait::Air;
            // A jump at a run (not a fall off an edge) becomes a leap with a
            // little more lift.
            bRunJump = RunWeight > .5f && GetVelocity().Z > 50.f;
            if (bRunJump)
            {
                Movement->Velocity.Z = RunJumpVerticalSpeed;
                SetClip(EClip::RunJump, 0, .08f);
            }
            // Takeoff is runtime-driven, so skip the clip's ground crouch and
            // start at its extension onto the toes.
            else SetClip(EClip::JumpStart, Clips[static_cast<int32>(EClip::JumpStart)]->GetPlayLength() * .5f, .06f);
        }
        if (bRunJump)
        {
            // Posed over the flight: progress from the vertical speed (0 at
            // takeoff, 1 back at takeoff height), never running backwards.
            const float Progress = FMath::Clamp((RunJumpVerticalSpeed - static_cast<float>(GetVelocity().Z)) / (2.f * RunJumpVerticalSpeed), 0.f, 1.f);
            BaseTime = FMath::Max(BaseTime, Progress * Clips[static_cast<int32>(EClip::RunJump)]->GetPlayLength());
        }
        else
        {
            BaseTime += DeltaSeconds;
            if (Base == EClip::JumpStart && BaseTime >= Length)
                SetClip(EClip::JumpLoop, 0, .1f);
        }
    }
    else if (Gait == EGait::Air)
    {
        // The leap ends on the run's foot_L touchdown: with the stick held,
        // land straight into the stride.
        const bool bStickHeld = bInput || FVector2D(InputRight, InputForward).SizeSquared() > .04f;
        if (bRunJump && bStickHeld && Speed > WalkSpeed) { Gait = EGait::Loop; WalkPhase = 0; SetClip(EClip::WalkLoop, 0, .06f); RunWeight = RunBlendAt(Speed); }
        else { Gait = EGait::Land; SetClip(EClip::JumpLand, 0, .06f); bHardLanding = bRunJump; }
        bRunJump = false;
    }
    else if (Gait == EGait::Roll)
    {
        // The capsule follows the clip's authored travel along the roll
        // direction. Movement ticks before the character, so this velocity
        // is next frame's step: aim it at the travel due then (assuming a
        // steady frame time), correcting whatever the last step missed.
        BaseTime += DeltaSeconds;
        RollDone += Travel;
        // Stick held: keep the pace through the roll and come out of it
        // straight into the stride as the paws come round, skipping the rise.
        const bool bCarry = FVector2D(InputRight, InputForward).SizeSquared() > .04f;
        const float Pace = bRunHeld ? RunSpeed : WalkSpeed;
        if (bCarry && BaseTime >= RollPlant)
        {
            Movement->Velocity = FVector(DodgeDirection.X * FMath::Max(Speed, Pace), DodgeDirection.Y * FMath::Max(Speed, Pace), Movement->Velocity.Z);
            Gait = EGait::Loop; WalkPhase = 0; SetClip(EClip::WalkLoop, 0, .12f);
            RunWeight = RunBlendAt(FMath::Max(Speed, Pace));
        }
        else if (BaseTime >= Length) FinishDodge();
        else
        {
            float RollSpeed = FMath::Max(0.f, RollTravelAt(BaseTime + DeltaSeconds) - RollDone) / FMath::Max(DeltaSeconds, 1e-4f);
            if (bCarry && BaseTime > .1f) RollSpeed = FMath::Max(RollSpeed, Pace);
            Movement->Velocity = FVector(DodgeDirection.X * RollSpeed, DodgeDirection.Y * RollSpeed, Movement->Velocity.Z);
        }
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
        // Landing at a run carries straight on into the stride.
        if (bInput && Speed > 10.f && (StateTime > .15f || Speed > WalkSpeed * 1.2f)) { Gait = EGait::Loop; WalkPhase = 0; SetClip(EClip::WalkLoop, 0, .12f); RunWeight = RunBlendAt(Speed); }
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
            BaseTime = FMath::Max(BaseTime, InvertTravel(StopTravelAt, StopDuration, FMath::Min(StopTravel, ChuckClipData::StopTravel)));
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
            BaseTime = InvertTravel(StartTravelAt, StartDuration, StartDistance);
            if (StartDistance >= ChuckClipData::StartTravel)
            {
                // WalkStart ends exactly on WalkLoop frame 0.
                Gait = EGait::Loop; WalkPhase = (StartDistance - ChuckClipData::StartTravel) / WalkStride;
                SetClip(EClip::WalkLoop, 0, 0);
                bEnterStop = bStopPending;
                bStopMirror = false;
            }
        }
        else if (Gait == EGait::Loop)
        {
            const float Previous = WalkPhase;
            WalkPhase = FMath::Frac(WalkPhase + Travel / FMath::Lerp(WalkStride, RunStride, RunBlendAt(Speed)));
            if (bStopPending && Speed <= WalkSpeed * 1.05f && (WalkPhase < Previous || (Previous < .5f && WalkPhase >= .5f)))
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
            StopTravel = FMath::Max(0.f, ChuckClipData::StopTravel - Speed * Speed / (2.f * StopDeceleration));
            SetClip(EClip::WalkStop, InvertTravel(StopTravelAt, StopDuration, StopTravel), .06f);
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
    // Coming to a stop ends the run latch.
    if (Gait == EGait::Idle && GaitBefore != EGait::Idle) bRunHeld = false;
    // During a dodge nothing brakes the capsule but the dodge itself.
    // A landing without input absorbs its momentum within a few cm, before the
    // landing paws lock (0.1 s), instead of walking on into a stop.
    Movement->BrakingDecelerationWalking = IsDodging() ? 0.f
        : bStopPending ? (Speed > WalkSpeed * 1.05f ? RunBrake : 0.f)
        : (Gait == EGait::Land ? (bHardLanding ? RunLandDeceleration : LandDeceleration) : StopDeceleration);
    // The run layer follows the speed in the stride and is held while the
    // stride fades out under the next clip.
    if (Gait == EGait::Loop) RunWeight = FMath::FInterpTo(RunWeight, RunBlendAt(Speed), DeltaSeconds, 10.f);
    else if (Base != EClip::WalkLoop && !(FadeWeight > 0 && Fading == EClip::WalkLoop)) RunWeight = 0;

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
    P.ClipRun = RunWeight > 0 ? Clips[static_cast<int32>(EClip::RunLoop)] : nullptr;
    P.TimeRun = WalkPhase * RunPeriod;
    P.PeriodRun = Period(EClip::RunLoop);
    P.WeightRun = RunWeight;
    P.bRunOnA = Base == EClip::WalkLoop;
    P.bRunOnB = Fading == EClip::WalkLoop;
    P.bFootIK = !bAirborne;
    P.bAllowSettle = Gait == EGait::Idle;
    // Stance from the manifest intervals of whichever clip dominates. WalkLoop:
    // generated from the manifest (ChuckClipData.h), trimmed likewise.
    const bool bLoopDominant = (Gait == EGait::Loop && FadeWeight < .5f) || (Gait == EGait::Stop && FadeWeight >= .5f);
    const bool bStanding = Gait == EGait::Idle || (Gait == EGait::Land && StateTime > .1f);
    const FStance* Clip = nullptr;
    if (Gait == EGait::Start) Clip = &StartStance;
    else if (Gait == EGait::Stop && !bLoopDominant) Clip = bStopMirror ? &StopStanceMirrored : &StopStance;
    else if (Gait == EGait::Turn) Clip = Base == EClip::TurnLeft90 ? &TurnLeftStance : &TurnRightStance;
    else if (Gait == EGait::Roll) Clip = &RollStance;
    else if (Gait == EGait::SideJump) Clip = &SideJumpStance;
    for (int32 I = 0; I < 2; ++I)
    {
        bool bStance = bStanding;
        if (Clip) bStance = InStance(I == 0 ? Clip->L : Clip->R, Clip->End, BaseTime);
        // WalkLoop: foot_L stands for [0, StanceFraction) of the cycle, foot_R half a cycle later, trimmed like the clips.
        else if (bLoopDominant && RunWeight >= .5f) bStance = I == 0 ? (WalkPhase > .02f && WalkPhase < RunStanceFraction - .02f)
                                                                 : (WalkPhase > .52f && WalkPhase < .5f + RunStanceFraction - .02f);
        else if (bLoopDominant) bStance = I == 0 ? (WalkPhase > .02f && WalkPhase < StanceFraction - .02f)
                                               : (WalkPhase > .52f || WalkPhase < StanceFraction - .52f);
        P.bStance[I] = bStance;
    }
    // Tucked in the roll, the paws follow the clip untouched.
    if (Gait == EGait::Roll && !P.bStance[0] && !P.bStance[1]) P.bFootIK = false;

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
