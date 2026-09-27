#include "ChuckCharacter.h"
#include "ChuckAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
    Movement->BrakingDecelerationWalking = 700;
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
    static const TCHAR* ClipNames[] = {TEXT("Idle"), TEXT("WalkStart"), TEXT("WalkLoop"), TEXT("JumpStart"), TEXT("JumpLoop"), TEXT("JumpLand")};
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
    Boom->SetRelativeLocation(FVector(0,0,FMath::Lerp(30.f,16.f,CameraBlend)));
    // Horizontal rat-height boom keeps the lens above ground even when looking up.
    const FRotator TargetRotation(-48.f * CameraBlend, ViewYaw, 0);
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
    BaseTime = FadingTime = FadeWeight = StateTime = StartDistance = WalkPhase = 0;
    PreviousMotionLocation = GetActorLocation();
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
    // WalkStart capsule travel (Tools/build_chuck_v1.py): 95*0.4*(u^3 - u^4/2), u = t/0.4, 19 cm total.
    float WalkStartTime(float Distance)
    {
        float Low = 0, High = 1;
        for (int32 I = 0; I < 24; ++I)
        {
            const float U = (Low + High) * .5f;
            (95.f * .4f * (U * U * U - U * U * U * U * .5f) < Distance ? Low : High) = U;
        }
        return (Low + High) * .2f;
    }
    constexpr float WalkStartTravel = 19.f;
    constexpr float WalkStride = 28.5f;
    constexpr float WalkPeriod = .3f;
}

void AChuckCharacter::UpdateMotion(float DeltaSeconds)
{
    auto* Anim = GetChuckAnim();
    if (!Anim || Clips.Contains(nullptr)) return;
    const bool bAirborne = GetCharacterMovement()->IsFalling();
    const float Speed = GetVelocity().Size2D();
    const FVector Location = GetActorLocation();
    float Travel = FVector::Dist2D(Location, PreviousMotionLocation);
    if (Travel > 50.f) Travel = 0; // teleport or reset
    PreviousMotionLocation = Location;
    StateTime += DeltaSeconds;
    FadeWeight = FMath::Max(0.f, FadeWeight - FadeRate * DeltaSeconds);

    // Distance-matched gait: walk clips advance by travelled distance, so the
    // clip's stance paw moves exactly with the ground at any speed.
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
        if (Base == EClip::JumpStart && BaseTime >= Clips[static_cast<int32>(EClip::JumpStart)]->GetPlayLength())
            SetClip(EClip::JumpLoop, 0, .1f);
    }
    else if (Gait == EGait::Air)
    {
        Gait = EGait::Land; SetClip(EClip::JumpLand, 0, .06f);
    }
    else if (Gait == EGait::Land)
    {
        BaseTime += DeltaSeconds;
        if (Speed > 10.f && StateTime > .15f) { Gait = EGait::Loop; WalkPhase = 0; SetClip(EClip::WalkLoop, 0, .15f); }
        else if (BaseTime >= Clips[static_cast<int32>(EClip::JumpLand)]->GetPlayLength()) { Gait = EGait::Idle; SetClip(EClip::Idle, 0, .2f); }
    }
    else if (Gait == EGait::Idle)
    {
        BaseTime += DeltaSeconds;
        if (Speed > 3.f) { Gait = EGait::Start; StartDistance = 0; SetClip(EClip::WalkStart, 0, .12f); }
    }
    else if (Speed < 3.f)
    {
        Gait = EGait::Idle; SetClip(EClip::Idle, 0, .25f);
    }
    else if (Gait == EGait::Start)
    {
        StartDistance += Travel;
        BaseTime = WalkStartTime(StartDistance);
        if (StartDistance >= WalkStartTravel)
        {
            // WalkStart ends exactly on WalkLoop frame 0.
            Gait = EGait::Loop; WalkPhase = (StartDistance - WalkStartTravel) / WalkStride;
            SetClip(EClip::WalkLoop, 0, 0);
        }
    }
    if (Gait == EGait::Loop)
    {
        WalkPhase = FMath::Frac(WalkPhase + Travel / WalkStride);
        BaseTime = WalkPhase * WalkPeriod;
    }

    FChuckAnimParams& P = Anim->Params;
    P.ClipA = Clips[static_cast<int32>(Base)];
    P.TimeA = BaseTime;
    P.PeriodA = Period(Base);
    P.ClipB = FadeWeight > 0 ? Clips[static_cast<int32>(Fading)] : nullptr;
    P.TimeB = FadingTime;
    P.PeriodB = Period(Fading);
    P.WeightB = FadeWeight;
    P.bFootIK = !bAirborne;
    P.bAllowSettle = Gait == EGait::Idle;
    // Stance windows from the manifest: foot_L 0-0.18 s, foot_R 0.15-0.30 and
    // 0-0.03 s of the 0.3 s loop, trimmed at plant/lift so locks never pop.
    const bool bWalking = Gait == EGait::Loop && FadeWeight < .5f;
    const bool bStanding = Gait == EGait::Idle || (Gait == EGait::Land && StateTime > .1f);
    P.bStance[0] = bStanding || (bWalking && WalkPhase > .02f && WalkPhase < .58f);
    P.bStance[1] = bStanding || (bWalking && (WalkPhase > .52f || WalkPhase < .08f));

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
