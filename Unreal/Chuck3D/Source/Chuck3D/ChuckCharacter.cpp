#include "ChuckCharacter.h"
#include "ChuckAnimInstance.h"
#include "ChuckClipData.h"
#include "ChuckBreakable.h"
#include "EnemyRat.h"
#include "AstralSummon.h"
#include "Camera/PlayerCameraManager.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"
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
    static const TCHAR* ClipNames[] = {TEXT("Idle"), TEXT("WalkStart"), TEXT("WalkLoop"), TEXT("WalkStop"), TEXT("TurnLeft90"), TEXT("TurnRight90"), TEXT("JumpStart"), TEXT("JumpLoop"), TEXT("JumpLand"), TEXT("Roll"), TEXT("SideJumpLeft"), TEXT("SideJumpRight"), TEXT("RunLoop"), TEXT("RunJump"), TEXT("SlashRight"), TEXT("SlashLeft"), TEXT("WallRun"), TEXT("WallKick"), TEXT("Hang"), TEXT("PullUp"), TEXT("Mantle"), TEXT("ShimmyLeft"), TEXT("ShimmyRight"), TEXT("StrafeLeft"), TEXT("StrafeRight"), TEXT("StrafeRunLeft"), TEXT("StrafeRunRight"), TEXT("SlashLowRight"), TEXT("SlashLowLeft"), TEXT("Summon")};
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
    // Exhaled smoke: soft puffs on instanced spheres, each fading on its own
    // (per-instance custom data 0 = opacity; M_SmokePuff). World-space, so a
    // breath stays where it was breathed.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PuffMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> PuffMaterial(TEXT("/Game/Characters/Chuck/V1/Cigarette/M_SmokePuff.M_SmokePuff"));
    ExhaleSmoke = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ExhaleSmoke"));
    ExhaleSmoke->SetupAttachment(GetCapsuleComponent());
    ExhaleSmoke->SetUsingAbsoluteLocation(true); ExhaleSmoke->SetUsingAbsoluteRotation(true); ExhaleSmoke->SetUsingAbsoluteScale(true);
    ExhaleSmoke->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ExhaleSmoke->SetCastShadow(false);
    ExhaleSmoke->NumCustomDataFloats = 1;
    ExhaleSmoke->SetStaticMesh(PuffMesh.Object);
    ExhaleSmoke->SetMaterial(0, PuffMaterial.Object);
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
    SlashRandom.Initialize(FMath::Rand());
    // Movement SFX: variants per kind (names from SourceAssets/Audio/SFX/manifest.json).
    auto Load = [](TArray<USoundBase*>& Set, const TCHAR* Stem, int32 Count)
    {
        for (int32 I = 0; I < Count; ++I)
            if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Art/Audio/SFX/%s_%02d.%s_%02d"), Stem, I, Stem, I))) Set.Add(Sound);
    };
    Load(StepWalkWood, TEXT("SFX_Step_Walk_Wood"), 6); Load(StepWalkStone, TEXT("SFX_Step_Walk_Stone"), 6);
    Load(StepRunWood, TEXT("SFX_Step_Run_Wood"), 6); Load(StepRunStone, TEXT("SFX_Step_Run_Stone"), 6);
    Load(JumpSounds, TEXT("SFX_Jump"), 3); Load(LandSounds, TEXT("SFX_Land"), 3);
    Load(SlashSounds, TEXT("SFX_Slash"), 4); Load(RollSounds, TEXT("SFX_Roll"), 2);
    Load(ExhaleSounds, TEXT("SFX_Exhale"), 2);
    NextExhaleAt = GetWorld()->GetTimeSeconds() + FMath::FRandRange(3.f, 6.f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SFX_LOADED %d"), GetSfxLoaded());
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
int32 AChuckCharacter::GetSfxLoaded() const
{
    return StepWalkWood.Num() + StepWalkStone.Num() + StepRunWood.Num() + StepRunStone.Num() + JumpSounds.Num() + LandSounds.Num() + SlashSounds.Num() + RollSounds.Num();
}
namespace
{
    // Mix under the soundtrack (music plays at 0.45); files peak at -3 dBFS.
    constexpr float WalkStepVolume = .3f, RunStepVolume = .4f, JumpVolume = .4f, LandVolume = .5f, SlashVolume = .45f, RollVolume = .45f;
}
void AChuckCharacter::PlaySfx(const TArray<USoundBase*>& Set, ESfx Kind, float Volume, float StartTime)
{
    if (Set.Num() == 0) return;
    // A random variant and a slight pitch spread, so repeats don't read as a loop.
    UGameplayStatics::PlaySound2D(this, Set[FMath::RandRange(0, Set.Num() - 1)], Volume, FMath::FRandRange(.95f, 1.05f), StartTime);
    ++SfxCounts[static_cast<int32>(Kind)];
}
void AChuckCharacter::PlayStep(const FVector& Paw, float Speed)
{
    // Stone or wood under the paw (dock materials are named M_<Surface>).
    bool bStone = false;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckStepSurface), false, this);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Paw + FVector(0, 0, 10), Paw - FVector(0, 0, 15), ECC_Visibility, Query))
        if (const UPrimitiveComponent* Floor = Hit.GetComponent())
            if (const UMaterialInterface* Surface = Floor->GetMaterial(0))
                bStone = Surface->GetName().Contains(TEXT("Stone")) || Surface->GetName().Contains(TEXT("Plaster"));
    const bool bRun = Speed > (ChuckClipData::WalkSpeed + ChuckClipData::StrafeRunSpeed) * .5f;
    if (bRun) PlaySfx(bStone ? StepRunStone : StepRunWood, ESfx::Step, RunStepVolume);
    else PlaySfx(bStone ? StepWalkStone : StepWalkWood, ESfx::Step, WalkStepVolume * FMath::Clamp(Speed / ChuckClipData::WalkSpeed, .6f, 1.f));
}
UChuckAnimInstance* AChuckCharacter::GetChuckAnim() const { return Cast<UChuckAnimInstance>(GetMesh()->GetAnimInstance()); }
int32 AChuckCharacter::GetGroomCount() const { return Grooms.Num(); }
const TCHAR* AChuckCharacter::GetGaitName() const
{
    static const TCHAR* Names[] = {TEXT("Idle"), TEXT("Start"), TEXT("Loop"), TEXT("Stop"), TEXT("Turn"), TEXT("Air"), TEXT("Land"), TEXT("Roll"), TEXT("SideJump"), TEXT("Slash"), TEXT("WallRun"), TEXT("Hang"), TEXT("Climb"), TEXT("Strafe"), TEXT("Astral")};
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
    Input->BindAxis("StrafeKeys", this, &AChuckCharacter::StrafeKeysAxis);
    Input->BindAxis("StrafeTrigger", this, &AChuckCharacter::StrafeTriggerAxis);
    Input->BindAction("Jump", IE_Pressed, this, &AChuckCharacter::JumpPressed);
    Input->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
    Input->BindAction("Recenter", IE_Pressed, this, &AChuckCharacter::Recenter);
    Input->BindAction("Dodge", IE_Pressed, this, &AChuckCharacter::Dodge);
    Input->BindAction("Run", IE_Pressed, this, &AChuckCharacter::RunPressed);
    Input->BindAction("Slash", IE_Pressed, this, &AChuckCharacter::Slash);
    Input->BindAction("Slash", IE_Released, this, &AChuckCharacter::SlashReleased);
    Input->BindAction("Reset", IE_Pressed, this, &AChuckCharacter::ResetToDock);
    Input->BindAction("Quit", IE_Pressed, this, &AChuckCharacter::Quit);
}
// A dodge owns the capsule; the stick is still read to choose the next move.
void AChuckCharacter::Forward(float Value) { InputForward = Value; if (!OwnsCapsule()) AddMovementInput(FRotator(0,ViewYaw,0).Vector(),Value); }
void AChuckCharacter::Right(float Value) { InputRight = Value; if (!OwnsCapsule()) AddMovementInput(FRotationMatrix(FRotator(0,ViewYaw,0)).GetUnitAxis(EAxis::Y),Value); }
// Q/E strafe: sideways along the camera without turning (the facing is held in UpdateMotion).
void AChuckCharacter::StrafeKeysAxis(float Value) { StrafeKeys = Value; if (!OwnsCapsule()) AddMovementInput(FRotationMatrix(FRotator(0,ViewYaw,0)).GetUnitAxis(EAxis::Y),Value); }
void AChuckCharacter::Dodge()
{
    // Action events dispatch before this frame's axis events: read the stick
    // directly so a direction pressed together with C counts.
    if (InputComponent) { InputRight = InputComponent->GetAxisValue(TEXT("Right")); InputForward = InputComponent->GetAxisValue(TEXT("Forward")); StrafeKeys = InputComponent->GetAxisValue(TEXT("StrafeKeys")); }
    DodgeToward(FVector2D(SideInput(), InputForward));
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
    // On a ledge the camera swings round behind him to face the wall, so the
    // stick reads naturally: up climbs, left and right shimmy. (Not on wall
    // runs: chimney bounces flip his facing every kick.)
    // Between two facing walls (a chimney) the camera turns side-on, looking
    // along the gap, so the bounces read left-right in front of it instead of
    // toward and past the lens.
    if (DeltaSeconds > 0 && LookIdle > .3f && bChimney && (Gait == EGait::WallRun || (Gait == EGait::Air && bWallJumpFlight)))
    {
        const float WallYaw = WallNormal.Rotation().Yaw;
        const float Left = WallYaw + 90.f, Right = WallYaw - 90.f;
        const float Target = FMath::Abs(FMath::FindDeltaAngleDegrees(ViewYaw, Left)) < FMath::Abs(FMath::FindDeltaAngleDegrees(ViewYaw, Right)) ? Left : Right;
        ViewYaw = FRotator::NormalizeAxis(ViewYaw + FMath::FindDeltaAngleDegrees(ViewYaw, Target) * FMath::Min(1.f, DeltaSeconds * 5.f));
    }
    if (DeltaSeconds > 0 && LookIdle > .3f && (Gait == EGait::Hang || Gait == EGait::Climb))
    {
        const float WallYaw = (-HangNormal).Rotation().Yaw;
        ViewYaw = FRotator::NormalizeAxis(ViewYaw + FMath::FindDeltaAngleDegrees(ViewYaw, WallYaw) * FMath::Min(1.f, DeltaSeconds * 4.f));
    }
    // The pivot ignores the arc of a jump (a 65 cm character's hop otherwise
    // bobs the whole view) but follows landings on a new level and falls.
    const float ActorZ = GetActorLocation().Z;
    if (!bFollowReady || DeltaSeconds <= 0) { FollowZ = ActorZ; bFollowReady = true; }
    else
    {
        const bool bAirborne = GetCharacterMovement()->IsFalling();
        // Hold height through ordinary jumps, but follow a wall climb up.
        FollowZ = FMath::FInterpTo(FollowZ, ActorZ, DeltaSeconds, bAirborne && !bWallJumpFlight ? 1.5f : 8.f);
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
    InputForward = InputRight = StrafeKeys = StrafeTrigger = 0;  // refreshed every frame while input is live
    bTestStrafe = bHangNeedsRelease = false; GetCharacterMovement()->bOrientRotationToMovement = true;
    bRunJump = bHardLanding = false;
    bSlashQueued = bSlashHeld = false; LayerTime = FadingLayerTime = -1; FadingLayerWeight = 0; SlashHitAt = -1; BiteImmuneUntil = -1;
    Sanity = MaxSanity; AstralPhase = EAstral::None; bPendingVanish = false; SetAstralHidden(false);
    if (auto* PC = Cast<APlayerController>(Controller)) if (PC->PlayerCameraManager) PC->PlayerCameraManager->StopCameraFade();
    LastWallNormal = FVector::ZeroVector; bWallJumpFlight = bWallAuto = bChimney = false; WallCoyoteUntil = -1; AirJumpPressedAt = -1e3f; LedgeCooldownUntil = -1;
    if (GetCharacterMovement()->MovementMode == MOVE_Flying) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
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
    const bool bLoop = Clip == EClip::Idle || Clip == EClip::WalkLoop || Clip == EClip::RunLoop || Clip == EClip::JumpLoop || Clip == EClip::WallRun || Clip == EClip::Hang || Clip == EClip::ShimmyLeft || Clip == EClip::ShimmyRight
        || Clip == EClip::StrafeLeft || Clip == EClip::StrafeRight || Clip == EClip::StrafeRunLeft || Clip == EClip::StrafeRunRight;
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
    // authored WalkStop takes over. (1000 since the 225 cm/s run; 700 at 190.)
    constexpr float RunBrake = 1000.f;
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
    float SlashTravelAt(float Time)
    {
        const float Frame = FMath::Clamp(Time * 30.f, 0.f, static_cast<float>(SlashFrames));
        const int32 I = FMath::Min(FMath::FloorToInt(Frame), SlashFrames - 1);
        return FMath::Lerp(SlashTravel[I], SlashTravel[I + 1], Frame - I);
    }
    // Upper-body slash layer envelope: in over 0.05 s, out over the last 0.15 s.
    float LayerWeightAt(float Time, float Length) { return FMath::SmoothStep(0.f, .05f, Time) * (1.f - FMath::SmoothStep(Length - .15f, Length, Time)); }
    FVector2D PathAt(const float (*Path)[2], int32 Frames, float Time)
    {
        const float Frame = FMath::Clamp(Time * 30.f, 0.f, static_cast<float>(Frames));
        const int32 I = FMath::Min(FMath::FloorToInt(Frame), Frames - 1);
        return FVector2D(FMath::Lerp(Path[I][0], Path[I + 1][0], Frame - I), FMath::Lerp(Path[I][1], Path[I + 1][1], Frame - I));
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
    if (IsAstral()) return;
    if (Gait == EGait::Hang) { DropFromHang(); return; }  // dodge while hanging: let go
    if (IsDodging() || Movement->IsFalling() || Gait == EGait::WallRun || Gait == EGait::Climb) return;
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
        bSideLong = bRunHeld;  // user 2026-09-29: long out of a run, short out of a walk
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
        PlaySfx(RollSounds, ESfx::Roll, RollVolume, Entry);
        RollDone = RollTravelAt(Entry);
    }
    bDodgeLaunched = bDodgeLanded = bStopPending = false;
}
FVector AChuckCharacter::StickWorld() const
{
    const FRotator View(0, ViewYaw, 0);
    return (View.Vector() * InputForward + FRotationMatrix(View).GetUnitAxis(EAxis::Y) * SideInput()).GetClampedToMaxSize(1.f);
}
void AChuckCharacter::JumpPressed()
{
    auto* Movement = GetCharacterMovement();
    const float Now = GetWorld()->GetTimeSeconds();
    if (Gait == EGait::Climb || IsAstral()) return;
    if (Gait == EGait::Hang)
    {
        // From a hang: pulling away and jumping kicks off backward; otherwise climb up.
        if (FVector::DotProduct(StickWorld(), -HangNormal) < -.3f) { WallNormal = HangNormal; WallJump(); }
        else if (bHangRoom) StartClimb(false, HangNormal, HangEdge);
        return;
    }
    if (Gait == EGait::WallRun || (Movement->IsFalling() && Now < WallCoyoteUntil)) { WallJump(); return; }
    if (Movement->IsFalling()) { AirJumpPressedAt = Now; return; }  // buffered for a wall reached just after
    // Jump with a strafe key down is a side jump that way, whatever else is
    // held (user 2026-09-30: running forward, press strafe + jump together to
    // hop sideways). Sideways only, never diagonal. The keys are read from the
    // controller's live key state, so a strafe key pressed on the same frame
    // as jump counts (action events run before that frame's axes; the axis
    // bindings themselves go stale once input is switched off).
    const bool bGrounded = Gait == EGait::Idle || Gait == EGait::Start || Gait == EGait::Loop || Gait == EGait::Stop || Gait == EGait::Land || Gait == EGait::Strafe || Gait == EGait::Turn;
    float Side = FMath::Sign(StrafeKeys);
    bool bTrigger = StrafeTrigger > .3f || bTestStrafe;
    if (const APlayerController* PC = Cast<APlayerController>(Controller))
    {
        const float Keys = (PC->IsInputKeyDown(EKeys::E) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::Q) ? 1.f : 0.f);  // DefaultInput.ini StrafeKeys
        if (Keys != 0) Side = Keys;
        bTrigger = bTrigger || PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis) > .3f;
    }
    // Held LT: the stick's sideways push picks the side.
    if (Side == 0 && bTrigger && FMath::Abs(InputRight) > .3f) Side = FMath::Sign(InputRight);
    if (Side != 0 && bGrounded)
    {
        ++StrafeJumps;
        DodgeToward(FVector2D(Side, 0));
        return;
    }
    Jump();
}
bool AChuckCharacter::TryEnterWallRun()
{
    // Jump into a wall while pushing toward it (or arrive from a wall jump)
    // and Chuck runs up it. Generous: a sphere probe reaching WallReach past
    // the capsule, anything within 60 degrees of head-on, any wall that isn't
    // the one he just left.
    auto* Movement = GetCharacterMovement();
    const FVector Probe = (bWallJumpFlight || Gait == EGait::SideJump) ? Movement->Velocity.GetSafeNormal2D() : StickWorld().GetSafeNormal2D();
    if (Probe.IsNearlyZero()) return false;
    const float Reach = GetCapsuleComponent()->GetScaledCapsuleRadius() + WallReach;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckWall), false, this);
    bool bHit = false;
    for (const float Height : {5.f, -15.f})  // chest, then hips (a top below his chest)
    {
        const FVector From = GetActorLocation() + FVector(0, 0, Height);
        bHit = GetWorld()->SweepSingleByChannel(Hit, From, From + Probe * (Reach - 4.f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(4.f), Query)
            && FMath::Abs(Hit.ImpactNormal.Z) <= .3f;
        if (bHit) break;
    }
    if (!bHit) return false;
    const FVector Normal = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0).GetSafeNormal();
    if (FVector::DotProduct(Probe, -Normal) < .5f) return false;
    // A top edge within reach: grab it (any wall, even the one just left).
    FVector Edge; bool bRoom = false;
    if (Movement->Velocity.Z > -300.f && GetWorld()->GetTimeSeconds() >= LedgeCooldownUntil
        && FindLedge(Normal, Hit.ImpactPoint, -15.f, 45.f, Edge, bRoom))
    {
        EnterHang(Normal, Edge, bRoom);
        return true;
    }
    if (Movement->Velocity.Z < -250.f) return false;
    if (!LastWallNormal.IsZero() && FVector::DotProduct(Normal, LastWallNormal) > .7f) return false;
    EnterWallRun(Hit, Normal);
    return true;
}
void AChuckCharacter::EnterWallRun(const FHitResult& Hit, const FVector& Normal)
{
    auto* Movement = GetCharacterMovement();
    const bool bFromSideJump = Gait == EGait::SideJump;
    WallNormal = Normal;
    // Snug against the wall (capsule surface 0.5 cm off it), facing it.
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    FVector Location = GetActorLocation();
    Location += Normal * (Radius + .5f - FVector::DotProduct(Location - Hit.ImpactPoint, Normal));
    SetActorLocation(Location, true);
    SetActorRotation((-Normal).Rotation());
    Movement->SetMovementMode(MOVE_Flying);
    Movement->BrakingDecelerationFlying = 0;
    Gait = EGait::WallRun;
    WallRunClock = 0; WallPhase = 0; WallPrevZ = static_cast<float>(GetActorLocation().Z);
    bWallAuto = bWallJumpFlight || bFromSideJump; bWallJumpFlight = false; bRunJump = false;
    {
        FHitResult Behind;
        FCollisionQueryParams BehindQuery(SCENE_QUERY_STAT(ChuckChimney), false, this);
        const FVector Center = GetActorLocation();
        bChimney = GetWorld()->LineTraceSingleByChannel(Behind, Center, Center + Normal * 250.f, ECC_Visibility, BehindQuery)
            && FVector::DotProduct(FVector(Behind.ImpactNormal.X, Behind.ImpactNormal.Y, 0).GetSafeNormal(), -Normal) > .8f;
    }
    ++WallRuns;
    SetClip(EClip::WallRun, 0, .08f);
    if (GetWorld()->GetTimeSeconds() - AirJumpPressedAt < WallBuffer) { AirJumpPressedAt = -1e3f; WallJump(); }
}
bool AChuckCharacter::FindLedge(const FVector& Normal, const FVector& FacePoint, float MinAbove, float MaxAbove, FVector& OutEdge, bool& bRoom) const
{
    // A walkable top between MinAbove and MaxAbove (relative to his centre),
    // just beyond the face in front of him, where the face really ends.
    const FVector Center = GetActorLocation();
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Over = FacePoint - Normal * 8.f;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckLedge), false, this);
    FHitResult Top;
    const FVector Start(Over.X, Over.Y, Center.Z + MaxAbove + 5.f), End(Over.X, Over.Y, Center.Z + MinAbove);
    if (!GetWorld()->LineTraceSingleByChannel(Top, Start, End, ECC_Visibility, Query) || Top.bStartPenetrating || Top.ImpactNormal.Z < .7f) return false;
    FHitResult Above;
    const FVector Lip = FVector(FacePoint.X, FacePoint.Y, Top.ImpactPoint.Z + 6.f) + Normal * 2.f;
    if (GetWorld()->LineTraceSingleByChannel(Above, Lip, Lip - Normal * 14.f, ECC_Visibility, Query)) return false;
    OutEdge = FVector(FacePoint.X, FacePoint.Y, Top.ImpactPoint.Z);
    const FVector Stand = OutEdge - Normal * (Radius + 4.f) + FVector(0, 0, Half + 2.f);
    bRoom = !GetWorld()->OverlapBlockingTestByChannel(Stand, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, Half), Query);
    return true;
}
bool AChuckCharacter::TryDropHang()
{
    // Walked gently off an edge (not jumped, not running): turn round and grab
    // it, GTA-style, if the drop below is more than a step.
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Location = GetActorLocation();
    FVector Out = GetVelocity().GetSafeNormal2D();
    if (Out.IsNearlyZero()) Out = GetActorForwardVector().GetSafeNormal2D();
    const float TopZ = static_cast<float>(Location.Z) - Half;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckDropHang), false, this);
    // The face he just stepped over, a little under the top.
    FHitResult Face;
    const FVector From = Location + Out * (Radius + 6.f) + FVector(0, 0, -Half - 8.f);
    if (!GetWorld()->LineTraceSingleByChannel(Face, From, From - Out * (2.f * Radius + 30.f), ECC_Visibility, Query)
        || Face.bStartPenetrating || FMath::Abs(Face.ImpactNormal.Z) > .3f) return false;
    const FVector Normal = FVector(Face.ImpactNormal.X, Face.ImpactNormal.Y, 0).GetSafeNormal();
    if (FVector::DotProduct(Normal, Out) < .6f) return false;
    // Only worth hanging over a real drop.
    FHitResult Below;
    const FVector Probe = FVector(Face.ImpactPoint.X, Face.ImpactPoint.Y, TopZ - 2.f) + Normal * (Radius + 5.f);
    if (GetWorld()->LineTraceSingleByChannel(Below, Probe, Probe - FVector(0, 0, DropHangMinDrop), ECC_Visibility, Query)) return false;
    FVector Edge; bool bRoom = false;
    if (!FindLedge(Normal, Face.ImpactPoint, -Half - 15.f, -Half + 12.f, Edge, bRoom)) return false;
    const float Yaw = GetActorRotation().Yaw;
    EnterHang(Normal, Edge, bRoom);
    // Swing round to face the wall on the way down rather than snapping.
    SetActorRotation(FRotator(0, Yaw, 0));
    HangYawFrom = Yaw; HangSnapTime = .35f;
    bHangNeedsRelease = true;
    ++DropHangs;
    return true;
}
void AChuckCharacter::EnterHang(const FVector& Normal, const FVector& Edge, bool bRoom)
{
    auto* Movement = GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Flying);
    Movement->StopMovementImmediately();
    SetActorRotation((-Normal).Rotation());
    Gait = EGait::Hang;
    HangNormal = Normal; HangEdge = Edge; HangFrom = GetActorLocation();
    HangClock = 0; HangHold = 0; bHangRoom = bRoom; HangSnapTime = .12f; HangYawFrom = (-Normal).Rotation().Yaw; bCornerCarry = false; bHangNeedsRelease = false;
    LastWallNormal = Normal; bWallJumpFlight = false; WallCoyoteUntil = -1; bRunJump = false;
    ++Hangs;
    SetClip(EClip::Hang, 0, .1f);
}
bool AChuckCharacter::TryHangCorner(const FVector& Along, float Side)
{
    // The edge ran out while shimmying toward Along. Inside corner: a wall
    // ahead facing back at him with an edge at the same height - turn onto
    // it. Outside corner: the edge wraps round the end of this face onto the
    // side face (facing Along) - go round it. Otherwise he just stops.
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const FVector Center = GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckCorner), false, this);
    FVector Edge; bool bRoom = false;
    FHitResult Ahead;
    const FVector Chest = Center + FVector(0, 0, 10);
    if (GetWorld()->LineTraceSingleByChannel(Ahead, Chest, Chest + Along * (Radius + 12.f), ECC_Visibility, Query)
        && FMath::Abs(Ahead.ImpactNormal.Z) < .3f)
    {
        const FVector Normal = FVector(Ahead.ImpactNormal.X, Ahead.ImpactNormal.Y, 0).GetSafeNormal();
        if (FVector::DotProduct(Normal, -Along) > .7f)
        {
            const FVector Face = FVector(Ahead.ImpactPoint.X, Ahead.ImpactPoint.Y, HangEdge.Z) + HangNormal * 8.f;
            if (FindLedge(Normal, Face, HangDrop - 8.f, HangDrop + 8.f, Edge, bRoom))
            {
                ++InnerCorners;
                TurnHangCorner(Normal, Edge, bRoom, Side);
                return true;
            }
        }
        return false;
    }
    // Outside: find where this face's edge ends (2 cm steps), then the side face.
    float Last = -1.f;
    for (float Step = 0.f; Step <= 16.f; Step += 2.f)
    {
        FVector Probe; bool bProbeRoom = false;
        if (FindLedge(HangNormal, HangEdge + Along * Step, HangDrop - 8.f, HangDrop + 8.f, Probe, bProbeRoom)) Last = Step;
        else break;
    }
    if (Last < 0.f) return false;
    const FVector Corner = HangEdge + Along * (Last + 1.f);
    const FVector Face = Corner - HangNormal * (Radius + 8.f);
    if (!FindLedge(Along, Face, HangDrop - 8.f, HangDrop + 8.f, Edge, bRoom)) return false;
    ++OuterCorners;
    TurnHangCorner(Along, Edge, bRoom, Side);
    return true;
}
void AChuckCharacter::TurnHangCorner(const FVector& Normal, const FVector& Edge, bool bRoom, float Side)
{
    bCornerCarry = true; CornerCarrySide = FMath::Sign(Side);
    CornerCarryStick = FVector2D(InputRight, InputForward).GetSafeNormal();
    // Swing round onto the new face over 0.3 s (position and facing blend).
    HangYawFrom = GetActorRotation().Yaw;
    HangFrom = GetActorLocation();
    HangNormal = Normal; HangEdge = Edge; bHangRoom = bRoom;
    HangClock = 0; HangHold = 0; HangSnapTime = .3f;
    LastWallNormal = Normal;
    SetClip(EClip::Hang, BaseTime, .15f);
}
void AChuckCharacter::LandingRoll()
{
    // A fall from more than a short height: tuck into a roll on touchdown,
    // toward the stick, else the way he was going, else straight ahead.
    auto* Movement = GetCharacterMovement();
    FVector Direction = StickWorld().GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction = Movement->Velocity.GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction = GetActorForwardVector().GetSafeNormal2D();
    DodgeDirection = Direction;
    SetActorRotation(Direction.Rotation());
    Gait = EGait::Roll;
    const float Entry = .12f;  // past the standing dive: straight into the tuck
    SetClip(EClip::Roll, Entry, .06f);
    RollDone = RollTravelAt(Entry);
    bDodgeLaunched = bDodgeLanded = bStopPending = bRunJump = bHardLanding = false;
    PlaySfx(LandSounds, ESfx::Land, LandVolume * .8f);
    PlaySfx(RollSounds, ESfx::Roll, RollVolume, Entry);
    ++LandingRolls;
}
void AChuckCharacter::DropFromHang()
{
    auto* Movement = GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Falling);
    Movement->Velocity = HangNormal * 30.f;
    WallNormal = HangNormal;
    LedgeCooldownUntil = GetWorld()->GetTimeSeconds() + .4f;  // don't re-grab it on the way down
    WallCoyoteUntil = GetWorld()->GetTimeSeconds() + WallCoyote;
    Gait = EGait::Air;
    SetClip(EClip::JumpLoop, 0, .15f);
}
void AChuckCharacter::StartClimb(bool bMantle, const FVector& Normal, const FVector& Edge)
{
    // The capsule follows the clip's authored path, scaled to the real step:
    // up to standing on the top, and far enough in to stand clear of the edge.
    auto* Movement = GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Flying);
    Movement->StopMovementImmediately();
    ClimbStart = GetActorLocation();
    ClimbDir = -Normal;
    ClimbRise = static_cast<float>(Edge.Z - ClimbStart.Z) + GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 1.f;
    ClimbAdvance = static_cast<float>(FVector::DotProduct(ClimbStart - Edge, Normal)) + (bMantle ? 17.f : 18.5f);
    bClimbMantle = bMantle;
    SetActorRotation((-Normal).Rotation());
    Gait = EGait::Climb;
    if (bMantle) ++Mantles; else ++PullUps;
    SetClip(bMantle ? EClip::Mantle : EClip::PullUp, 0, bMantle ? .08f : .05f);
}
bool AChuckCharacter::TryMantle()
{
    // Walking into a knee-high ledge: hop onto it (the only fully automatic
    // move, since it only does what he was already trying to do).
    auto* Movement = GetCharacterMovement();
    const FVector Direction = Movement->GetCurrentAcceleration().GetSafeNormal2D();
    if (Direction.IsNearlyZero()) return false;
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector From = GetActorLocation() + FVector(0, 0, -Half + MantleMin + 3.f);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckMantle), false, this);
    if (!GetWorld()->SweepSingleByChannel(Hit, From, From + Direction * (Radius + 3.f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(3.f), Query)) return false;
    if (FMath::Abs(Hit.ImpactNormal.Z) > .3f) return false;
    const FVector Normal = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0).GetSafeNormal();
    if (FVector::DotProduct(Direction, -Normal) < .6f) return false;
    FVector Edge; bool bRoom = false;
    if (!FindLedge(Normal, Hit.ImpactPoint, -Half + MantleMin, -Half + MantleMax, Edge, bRoom) || !bRoom) return false;
    StartClimb(true, Normal, Edge);
    return true;
}
void AChuckCharacter::LeaveWall()
{
    // Peel off: drift out from the wall and fall; a jump still kicks off it for a moment.
    auto* Movement = GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Falling);
    Movement->Velocity = WallNormal * 40.f;
    LastWallNormal = WallNormal;
    WallCoyoteUntil = GetWorld()->GetTimeSeconds() + WallCoyote;
    Gait = EGait::Air;
    SetClip(EClip::JumpLoop, 0, .15f);
}
void AChuckCharacter::WallJump()
{
    // Kick away from the wall; the stick along the wall angles it. He turns to
    // face where he's going, so the next wall is in front of him.
    auto* Movement = GetCharacterMovement();
    const FVector Stick = StickWorld();
    const FVector Along = Stick - WallNormal * FVector::DotProduct(Stick, WallNormal);
    const FVector Direction = (WallNormal + Along * .6f).GetSafeNormal2D();
    Movement->SetMovementMode(MOVE_Falling);
    Movement->Velocity = Direction * WallJumpOut + FVector(0, 0, WallJumpUp);
    SetActorRotation(Direction.Rotation());
    LastWallNormal = WallNormal;
    WallCoyoteUntil = -1;
    bWallJumpFlight = true; bRunJump = false;
    Gait = EGait::Air;
    ++WallJumps;
    SetClip(EClip::WallKick, 0, .05f);
    PlaySfx(JumpSounds, ESfx::Jump, JumpVolume);
}
AChuckCharacter::EClip AChuckCharacter::PickPaw(bool bFirst, EClip Previous)
{
    // User 2026-09-28: random paw order, steady timing. Capped at two of one
    // paw in a row so a streak never reads as a stuck arm.
    ++SlashStrikes;
    PlaySfx(SlashSounds, ESfx::Slash, SlashVolume);
    SlashHitAt = GetWorld()->GetTimeSeconds() + SlashStrike;   // what it reaches breaks on the cut
    const bool bPreviousRight = Previous == EClip::SlashRight || Previous == EClip::SlashLowRight;
    bool bRight;
    if (bFirst) { SamePawRun = 0; bRight = SlashRandom.FRand() < .5f; }
    else
    {
        const bool bSame = SamePawRun == 0 && SlashRandom.FRand() < .5f;
        SamePawRun = bSame ? 1 : 0;
        bRight = bSame ? bPreviousRight : !bPreviousRight;
    }
    // Low targets (grass, jars, small rats) get the low rake: same paw, same beat.
    if (LowTargetInReach()) return bRight ? EClip::SlashLowRight : EClip::SlashLowLeft;
    return bRight ? EClip::SlashRight : EClip::SlashLeft;
}
void AChuckCharacter::Slash()
{
    auto* Movement = GetCharacterMovement();
    bSlashHeld = true;
    const bool bStandingSlash = Gait == EGait::Slash;
    const bool bLayered = LayerTime >= 0;
    if (bStandingSlash || bLayered)
    {
        // Buffer the next paw; it chains once this strike has gone through.
        if ((bStandingSlash ? BaseTime : LayerTime) > .05f) bSlashQueued = true;
        return;
    }
    if (IsDodging() || IsAstral() || Gait == EGait::Hang || Gait == EGait::Climb || Gait == EGait::WallRun) return;
    const EClip Clip = PickPaw(true, EClip::SlashRight);
    bSlashQueued = false;
    if (Gait == EGait::Turn) Movement->SetMovementMode(MOVE_Walking);
    const bool bStanding = !Movement->IsFalling() && GetVelocity().Size2D() < 20.f
        && (Gait == EGait::Idle || Gait == EGait::Stop || Gait == EGait::Land || Gait == EGait::Turn || Gait == EGait::Start);
    if (bStanding)
    {
        // A stepping slash: the capsule follows the clip's 10 cm step-in.
        Movement->StopMovementImmediately();
        Gait = EGait::Slash;
        SetClip(Clip, 0, .06f);
        SlashDone = 0;
    }
    else
    {
        LayerClip = Clip;
        LayerTime = 0;
    }
}
void AChuckCharacter::SlashHit()
{
    // Everything breakable within reach in front (or right against him),
    // from the ground to his chest: grass, and later jars and low enemies.
    // The swing sweeps across his body away from the striking paw.
    const FVector Location = GetActorLocation();
    const float Feet = static_cast<float>(Location.Z) - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Ahead = GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Ahead);
    const bool bRightPaw = FCString::Strstr(GetSlashName(), TEXT("Right")) != nullptr;
    const FVector Swing = (Right * (bRightPaw ? -1.f : 1.f) + Ahead * .6f).GetSafeNormal();
    for (const TWeakObjectPtr<AChuckBreakable>& Entry : AChuckBreakable::All())
    {
        AChuckBreakable* Target = Entry.Get();
        if (!Target || Target->IsBroken()) continue;
        const FVector To = Target->GetActorLocation() - Location;
        const float Reach = static_cast<float>(To.Size2D()) - Target->GetHitRadius();
        const float TargetZ = static_cast<float>(Target->GetActorLocation().Z) - (Target->IsCentred() ? Target->GetHitHeight() * .5f : 0.f);
        if (Reach > SlashReach || TargetZ > Feet + 45.f || TargetZ + Target->GetHitHeight() < Feet - 10.f) continue;
        if (FVector::DotProduct(To.GetSafeNormal2D(), Ahead) < .3f && Reach > 5.f) continue;   // behind him, unless touching
        Target->Break(Swing);
        ++SlashBreaks;
    }
    // Rats: the same reach, ground to chest.
    for (const TWeakObjectPtr<AEnemyRat>& Entry : AEnemyRat::All())
    {
        AEnemyRat* Rat = Entry.Get();
        if (!Rat || Rat->IsDead()) continue;
        const FVector To = Rat->GetActorLocation() - Location;
        const float Reach = static_cast<float>(To.Size2D()) - AEnemyRat::HitRadius;
        if (Reach > SlashReach || FMath::Abs(To.Z) > 45.f) continue;
        if (FVector::DotProduct(To.GetSafeNormal2D(), Ahead) < .3f && Reach > 5.f) continue;
        Rat->TakeSlash(Swing);
        ++SlashRatHits;
    }
}
bool AChuckCharacter::TakeBite(const FVector& From)
{
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < BiteImmuneUntil || IsAstral() || IsDodging() || Gait == EGait::Hang || Gait == EGait::Climb || Gait == EGait::WallRun) return false;
    BiteImmuneUntil = Now + BiteImmunity;
    ++BitesTaken;
    Sanity = FMath::Max(0, Sanity - 1);
    if (Sanity == 0) { bPendingVanish = true; PendingVanishAt = Now + .35f; }
    FVector Away = (GetActorLocation() - From).GetSafeNormal2D();
    if (Away.IsNearlyZero()) Away = -GetActorForwardVector().GetSafeNormal2D();
    // A stumble back out of reach: a short hop away, the slash layer dropped.
    LayerTime = -1; bSlashQueued = false; SlashHitAt = -1;
    if (Gait == EGait::Slash || Gait == EGait::Turn) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    LaunchCharacter(Away * BiteKnockback + FVector(0, 0, 110.f), true, true);
    return true;
}
void AChuckCharacter::AddCigarettes(int32 Count)
{
    for (int32 I = 0; I < Count; ++I)
    {
        ++PickupsCollected;
        if (Sanity < MaxSanity && !IsAstral()) ++Sanity;
        else ++CigaretteCount;
    }
}
void AChuckCharacter::Exhale()
{
    const float Now = GetWorld()->GetTimeSeconds();
    ++Exhales;
    ExhaleUntil = Now + ExhaleLength; ExhaleCarry = 0;
    NextExhaleAt = Now + ExhaleEvery + FMath::FRandRange(0.f, 5.f);
    // A breath out: barely there in the mix.
    if (ExhaleSounds.Num()) UGameplayStatics::PlaySound2D(this, ExhaleSounds[FMath::RandRange(0, ExhaleSounds.Num() - 1)], .14f, FMath::FRandRange(.95f, 1.05f));
}
void AChuckCharacter::UpdateExhale(float DeltaSeconds)
{
    const float Now = GetWorld()->GetTimeSeconds();
    // Only when calm: on his feet, not mid-move, not away in the light.
    const bool bCalm = !GetCharacterMovement()->IsFalling() && !bAstralHidden
        && (Gait == EGait::Idle || Gait == EGait::Start || Gait == EGait::Loop || Gait == EGait::Stop || Gait == EGait::Land || Gait == EGait::Strafe || Gait == EGait::Turn);
    if (Now >= NextExhaleAt && ExhaleUntil < Now) { if (bCalm) Exhale(); else NextExhaleAt = Now + 1.f; }
    // The stream: about 16 puffs a second from the corner of his mouth where
    // the cigarette sits, forward and a little down, easing off at the end.
    if (Now < ExhaleUntil && GetMesh()->DoesSocketExist(TEXT("socket_cigarette")))
    {
        const float Left = (ExhaleUntil - Now) / ExhaleLength;
        ExhaleCarry += DeltaSeconds * 16.f;
        const FVector Mouth = GetMesh()->GetSocketLocation(TEXT("socket_cigarette"));
        const FVector Out = (GetActorForwardVector() + FVector(0, 0, -.25f)).GetSafeNormal();
        while (ExhaleCarry >= 1.f)
        {
            ExhaleCarry -= 1.f;
            FSmokePuff Puff;
            Puff.Position = Mouth + Out * 2.f;
            Puff.Velocity = Out * FMath::FRandRange(30.f, 50.f) * (.4f + .6f * Left) + FMath::VRand() * 6.f + GetVelocity() * .5f;
            Puff.Age = 0; Puff.Life = FMath::FRandRange(1.7f, 2.4f); Puff.Size = FMath::FRandRange(.8f, 1.2f);
            SmokePuffs.Add(Puff);
            ++SmokePuffsSpawned;
        }
    }
    // Each puff slows, swells, rises and thins away.
    ExhaleSmoke->ClearInstances();
    for (int32 I = SmokePuffs.Num() - 1; I >= 0; --I)
    {
        FSmokePuff& Puff = SmokePuffs[I];
        Puff.Age += DeltaSeconds;
        if (Puff.Age >= Puff.Life) { SmokePuffs.RemoveAtSwap(I); continue; }
        Puff.Velocity *= FMath::Max(0.f, 1.f - 1.6f * DeltaSeconds);
        Puff.Velocity.Z += (Puff.Age > .25f ? 14.f : 0.f) * DeltaSeconds;
        Puff.Position += Puff.Velocity * DeltaSeconds + FVector(2.f, 1.f, 0) * DeltaSeconds;   // a faint dock breeze
    }
    for (const FSmokePuff& Puff : SmokePuffs)
    {
        const float U = Puff.Age / Puff.Life;
        const float Diameter = FMath::Lerp(3.f, 18.f, 1.f - FMath::Square(1.f - U)) * Puff.Size;   // cm
        const int32 Index = ExhaleSmoke->AddInstance(FTransform(FRotator(0, Puff.Age * 40.f, 0), Puff.Position, FVector(Diameter / 100.f)), true);
        ExhaleSmoke->SetCustomDataValue(Index, 0, .3f * FMath::SmoothStep(0.f, .1f, Puff.Age) * FMath::Pow(1.f - U, 1.5f), false);
    }
    ExhaleSmoke->MarkRenderStateDirty();
}
const TCHAR* AChuckCharacter::GetAstralName() const
{
    static const TCHAR* Names[] = {TEXT("None"), TEXT("Vanishing"), TEXT("Away"), TEXT("Summoning")};
    return Names[static_cast<int32>(AstralPhase)];
}
void AChuckCharacter::SetAstralHidden(bool bHide)
{
    bAstralHidden = bHide;
    GetMesh()->SetVisibility(!bHide, true);
}
void AChuckCharacter::CameraFade(float From, float To, float Seconds)
{
    // The Astral Sea's colour: a deep, quiet indigo.
    if (auto* PC = Cast<APlayerController>(Controller))
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(From, To, Seconds, FLinearColor(.015f, .015f, .05f), false, true);
}
void AChuckCharacter::BeginVanish()
{
    auto* Movement = GetCharacterMovement();
    bPendingVanish = false;
    Gait = EGait::Astral; AstralPhase = EAstral::Vanishing; AstralClock = 0; bAstralFaded = false;
    Movement->StopMovementImmediately();
    Movement->DisableMovement();
    LayerTime = FadingLayerTime = -1; SlashHitAt = -1; bSlashQueued = bSlashHeld = false; RunWeight = 0;
    // He sinks down into the light: the summon played backward from standing.
    SetClip(EClip::Summon, Clips[static_cast<int32>(EClip::Summon)]->GetPlayLength(), .2f);
    const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    AAstralSummon::Start(GetWorld(), Feet, false);
}
void AChuckCharacter::UpdateAstral(float DeltaSeconds)
{
    AstralClock += DeltaSeconds;
    const float Length = Clips[static_cast<int32>(EClip::Summon)]->GetPlayLength();
    switch (AstralPhase)
    {
    case EAstral::Vanishing:
        BaseTime = FMath::Max(0.f, Length - AstralClock * 2.2f);
        if (AstralClock >= AAstralSummon::VanishPeak && !bAstralHidden) SetAstralHidden(true);
        if (AstralClock >= .8f && !bAstralFaded) { CameraFade(0.f, 1.f, .6f); bAstralFaded = true; }
        if (AstralClock >= 1.45f)
        {
            // Away: back to the spawn point (the Astral Anchor, later), whole again.
            AstralPhase = EAstral::Away; AstralClock = 0;
            SetActorLocationAndRotation(StartLocation(), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
            ViewYaw = 0; LookPitch = SmoothLook = FMath::Min(LookPitch, -5.f); bFollowReady = false;
            PreviousMotionLocation = GetActorLocation();
            Sanity = MaxSanity; BiteImmuneUntil = -1;
        }
        break;
    case EAstral::Away:
        if (AstralClock >= .35f)
        {
            AstralPhase = EAstral::Summoning; AstralClock = 0;
            SetClip(EClip::Summon, 0, 0);
            const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            AAstralSummon::Start(GetWorld(), Feet, true);
            CameraFade(1.f, 0.f, .9f);
        }
        break;
    case EAstral::Summoning:
        // Hidden until the column peaks, then he's there, curled, and rises.
        if (AstralClock >= AAstralSummon::SummonPeak && bAstralHidden) SetAstralHidden(false);
        BaseTime = FMath::Max(0.f, AstralClock - AAstralSummon::SummonPeak);
        if (BaseTime >= Length)
        {
            AstralPhase = EAstral::None;
            Gait = EGait::Idle; SetClip(EClip::Idle, 0, .25f);
            GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            ++Respawns;
        }
        break;
    default:
        break;
    }
}
bool AChuckCharacter::LowTargetInReach() const
{
    const FVector Location = GetActorLocation();
    const float Feet = static_cast<float>(Location.Z) - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Ahead = GetActorForwardVector().GetSafeNormal2D();
    for (const TWeakObjectPtr<AChuckBreakable>& Entry : AChuckBreakable::All())
    {
        const AChuckBreakable* Target = Entry.Get();
        if (!Target || Target->IsBroken()) continue;
        const FVector To = Target->GetActorLocation() - Location;
        const float Reach = static_cast<float>(To.Size2D()) - Target->GetHitRadius();
        const float Bottom = static_cast<float>(Target->GetActorLocation().Z) - (Target->IsCentred() ? Target->GetHitHeight() * .5f : 0.f);
        if (Reach <= SlashReach + 10.f && (FVector::DotProduct(To.GetSafeNormal2D(), Ahead) >= .3f || Reach <= 5.f)
            && Bottom + Target->GetHitHeight() < Feet + 35.f && Bottom > Feet - 20.f)
            return true;
    }
    for (const TWeakObjectPtr<AEnemyRat>& Entry : AEnemyRat::All())
    {
        const AEnemyRat* Rat = Entry.Get();
        if (!Rat || Rat->IsDead()) continue;
        const FVector To = Rat->GetActorLocation() - Location;
        const float Reach = static_cast<float>(To.Size2D()) - AEnemyRat::HitRadius;
        if (Reach <= SlashReach + 10.f && (FVector::DotProduct(To.GetSafeNormal2D(), Ahead) >= .3f || Reach <= 5.f) && FMath::Abs(To.Z) < 45.f) return true;
    }
    return false;
}
const TCHAR* AChuckCharacter::GetSlashName() const
{
    const EClip Clip = Gait == EGait::Slash ? Base : (LayerTime >= 0 ? LayerClip : EClip::Num);
    return Clip == EClip::SlashRight ? TEXT("SlashRight") : Clip == EClip::SlashLeft ? TEXT("SlashLeft")
        : Clip == EClip::SlashLowRight ? TEXT("SlashLowRight") : Clip == EClip::SlashLowLeft ? TEXT("SlashLowLeft") : TEXT("");
}
void AChuckCharacter::UpdateSlashLayer(float DeltaSeconds)
{
    if (FadingLayerTime >= 0)
    {
        FadingLayerTime += DeltaSeconds;
        FadingLayerWeight = FMath::Max(0.f, FadingLayerWeight - DeltaSeconds / .08f);
        if (FadingLayerWeight <= 0) FadingLayerTime = -1;
    }
    if (LayerTime < 0) return;
    LayerTime += DeltaSeconds;
    const float LayerLength = Clips[static_cast<int32>(LayerClip)]->GetPlayLength();
    if ((bSlashQueued || bSlashHeld) && LayerTime >= SlashChainAt)
    {
        // Chain: the finished paw fades out under the next one.
        FadingLayerClip = LayerClip; FadingLayerTime = LayerTime; FadingLayerWeight = LayerWeightAt(LayerTime, LayerLength);
        LayerClip = PickPaw(false, LayerClip);
        LayerTime = 0; bSlashQueued = false;
    }
    else if (LayerTime >= LayerLength) { LayerTime = -1; bSlashQueued = false; }
}
void AChuckCharacter::FinishDodge()
{
    // Back to the aplomb stance the clips end on; saunter on if the stick is held.
    if (StrafeHeld() && FVector2D(SideInput(), InputForward).SizeSquared() > .04f) { Gait = EGait::Strafe; StrafePhase = 0; SetClip(EClip::StrafeLeft, 0, .15f); }
    else if (FVector2D(InputRight, InputForward).SizeSquared() > .04f) { Gait = EGait::Start; StartDistance = 0; SetClip(EClip::WalkStart, 0, .15f); }
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
    const bool bStrafe = StrafeHeld();
    if (!OwnsCapsule()) Movement->MaxWalkSpeed = bStrafe ? (bRunHeld ? StrafeRunSpeed : StrafeSpeed) : (bRunHeld ? RunSpeed : WalkSpeed);
    // Strafing: the facing is held down the camera (Counter-Strike style)
    // instead of turning toward the movement.
    const bool bHoldFacing = bStrafe && !OwnsCapsule() && Gait != EGait::Turn;
    Movement->bOrientRotationToMovement = !bHoldFacing;
    if (bHoldFacing) SetActorRotation(FRotator(0, FMath::FixedTurn(static_cast<float>(GetActorRotation().Yaw), ViewYaw, 720.f * DeltaSeconds), 0));
    if (bStrafe && bInput && !bAirborne && (Gait == EGait::Idle || Gait == EGait::Start || Gait == EGait::Loop || Gait == EGait::Stop || Gait == EGait::Land))
    {
        Gait = EGait::Strafe; StrafePhase = 0; WalkPhase = 0;
    }
    UpdateSlashLayer(DeltaSeconds);
    if (SlashHitAt >= 0 && GetWorld()->GetTimeSeconds() >= SlashHitAt) { SlashHitAt = -1; SlashHit(); }
    const float Length = Clips[static_cast<int32>(Base)]->GetPlayLength();

    // Distance-matched gait: start, walk and stop clips advance by travelled
    // distance, so the clip's stance paw moves exactly with the ground.
    // Walking into a knee-high ledge: mantle onto it.
    if (bInput && !bAirborne && (Gait == EGait::Idle || Gait == EGait::Start || Gait == EGait::Loop || Gait == EGait::Stop || Gait == EGait::Strafe)) TryMantle();
    if (bPendingVanish && GetWorld()->GetTimeSeconds() >= PendingVanishAt && (Movement->IsMovingOnGround() || GetWorld()->GetTimeSeconds() >= PendingVanishAt + .8f)) BeginVanish();
    if (Gait == EGait::Astral) UpdateAstral(DeltaSeconds);
    else if (Gait == EGait::WallRun)
    {
        // Three steps up: rise speed falls linearly to zero over WallRunTime
        // (WallRunRise in all); the step cycle follows the height gained.
        WallRunClock += DeltaSeconds;
        const float Rise = static_cast<float>(Location.Z) - WallPrevZ;
        WallPrevZ = static_cast<float>(Location.Z);
        WallPhase = FMath::Frac(WallPhase + FMath::Max(0.f, Rise) / WallRunStride);
        BaseTime = WallPhase * Period(EClip::WallRun);
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckWallHold), false, this);
        const FVector Chest = Location + FVector(0, 0, 15);
        const bool bWall = GetWorld()->LineTraceSingleByChannel(Hit, Chest, Chest - WallNormal * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 8.f), ECC_Visibility, Query);
        const bool bLetGo = !bWallAuto && FVector::DotProduct(StickWorld(), -WallNormal) < -.3f;
        FVector Edge; bool bRoom = false;
        const FVector Face = Location - WallNormal * (GetCapsuleComponent()->GetScaledCapsuleRadius() + .5f);
        if (!bLetGo && FindLedge(WallNormal, Face, 5.f, 45.f, Edge, bRoom)) EnterHang(WallNormal, Edge, bRoom);
        else if (WallRunClock >= WallRunTime || !bWall || bLetGo) LeaveWall();
        else
        {
            const float Up = 2.f * WallRunRise / WallRunTime * (1.f - WallRunClock / WallRunTime);
            Movement->Velocity = FVector(0, 0, Up) - WallNormal * 30.f;
        }
    }
    else if (Gait == EGait::Hang)
    {
        // Snap in over 0.12 s, then hold. Toward the wall (held PullUpHold)
        // climbs up; away lets go.
        HangClock += DeltaSeconds;
        BaseTime += DeltaSeconds;
        const FVector Hold = HangEdge + HangNormal * (GetCapsuleComponent()->GetScaledCapsuleRadius() + .5f) - FVector(0, 0, HangDrop);
        const float Snap = FMath::SmoothStep(0.f, HangSnapTime, HangClock);
        SetActorLocation(FMath::Lerp(HangFrom, Hold, Snap), false, nullptr, ETeleportType::TeleportPhysics);
        const float WallYaw = (-HangNormal).Rotation().Yaw;
        SetActorRotation(FRotator(0, HangYawFrom + FMath::FindDeltaAngleDegrees(HangYawFrom, WallYaw) * Snap, 0));
        Movement->Velocity = FVector::ZeroVector;
        const FVector2D Raw(SideInput(), InputForward);
        if (bHangNeedsRelease && Raw.SizeSquared() < .04f) bHangNeedsRelease = false;
        if (bCornerCarry && (Raw.SizeSquared() < .04f || FVector2D::DotProduct(Raw.GetSafeNormal(), CornerCarryStick) < .7f)) bCornerCarry = false;
        const FVector Along = FRotationMatrix((-HangNormal).Rotation()).GetUnitAxis(EAxis::Y);  // his right, along the wall
        const float Toward = (bCornerCarry || bHangNeedsRelease) ? 0.f : FVector::DotProduct(StickWorld(), -HangNormal);
        const float Side = bHangNeedsRelease ? 0.f : bCornerCarry ? CornerCarrySide * FMath::Min(1.f, Raw.Size()) : FVector::DotProduct(StickWorld(), Along);
        float Moved = 0;
        if (HangClock >= HangSnapTime && FMath::Abs(Side) > .4f && FMath::Abs(Side) > Toward)
        {
            // Shimmy: the edge must carry on under both hands (the lead hand
            // 10 cm ahead) at about the same height, and nothing may block him.
            const float Direction = FMath::Sign(Side);
            const FVector Next = HangEdge + Along * Direction * ShimmySpeed * FMath::Min(1.f, FMath::Abs(Side)) * DeltaSeconds;
            FVector Edge, AheadEdge; bool bRoom = false, bAheadRoom = false;
            const bool bLedge = FindLedge(HangNormal, Next, HangDrop - 8.f, HangDrop + 8.f, Edge, bRoom)
                && FindLedge(HangNormal, Next + Along * Direction * 10.f, HangDrop - 8.f, HangDrop + 8.f, AheadEdge, bAheadRoom);
            const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
            const FVector From = HangEdge + HangNormal * (Radius + .5f) - FVector(0, 0, HangDrop);
            const FVector To = Edge + HangNormal * (Radius + .5f) - FVector(0, 0, HangDrop);
            FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckShimmy), false, this);
            const bool bBlocked = GetWorld()->SweepTestByChannel(From, To, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius - 1.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 1.f), Query);
            if ((!bLedge || bBlocked) && TryHangCorner(Along * Direction, Direction)) {}
            else if (bLedge && !bBlocked)
            {
                Moved = static_cast<float>(FVector::Dist(HangEdge, Edge));
                HangEdge = Edge; bHangRoom = bRoom; HangFrom = To;
                SetActorLocation(To, false, nullptr, ETeleportType::TeleportPhysics);
                const EClip Shimmy = Direction > 0 ? EClip::ShimmyRight : EClip::ShimmyLeft;
                ShimmyPhase = FMath::Frac(ShimmyPhase + Moved / ShimmyStride);
                if (Base != Shimmy) SetClip(Shimmy, ShimmyPhase * Period(Shimmy), .1f);
                BaseTime = ShimmyPhase * Period(Shimmy);
            }
        }
        if (Moved <= 0 && (Base == EClip::ShimmyLeft || Base == EClip::ShimmyRight)) SetClip(EClip::Hang, 0, .15f);
        HangHold = Toward > .5f ? HangHold + DeltaSeconds : 0.f;
        if (HangHold >= PullUpHold && bHangRoom && HangClock >= HangSnapTime) StartClimb(false, HangNormal, HangEdge);
        else if (Toward < -.5f && HangClock > .15f) DropFromHang();
    }
    else if (Gait == EGait::Climb)
    {
        BaseTime += DeltaSeconds;
        const FVector2D Path = bClimbMantle ? PathAt(MantlePath, MantleFrames, BaseTime) : PathAt(PullUpPath, PullUpFrames, BaseTime);
        const float Forward = Path.X * ClimbAdvance / (bClimbMantle ? MantleAdvance : PullUpAdvance);
        const float Up = Path.Y * ClimbRise / (bClimbMantle ? MantleRefStep : PullUpRise);
        SetActorLocation(ClimbStart + ClimbDir * Forward + FVector(0, 0, Up), false, nullptr, ETeleportType::TeleportPhysics);
        Movement->Velocity = FVector::ZeroVector;
        if (BaseTime >= Length)
        {
            Movement->SetMovementMode(MOVE_Walking);
            LastWallNormal = FVector::ZeroVector; bWallJumpFlight = false;
            PreviousMotionLocation = GetActorLocation();
            FinishDodge();
        }
    }
    else if (Gait == EGait::SideJump)
    {
        // Launch at the clip's takeoff, hold just before its landing while in
        // the air, stop the capsule at touchdown (the clip carries the hips on).
        BaseTime += DeltaSeconds;
        if (!bDodgeLaunched)
        {
            if (BaseTime >= SideTakeoff)
            {
                LaunchCharacter(DodgeDirection * (bSideLong ? SideLongLateralSpeed : SideShortLateralSpeed)
                    + FVector(0, 0, bSideLong ? SideLongVerticalSpeed : SideShortVerticalSpeed), true, true);
                PlaySfx(JumpSounds, ESfx::Jump, JumpVolume * (bSideLong ? 1.f : .8f));
                bDodgeLaunched = true;
            }
        }
        else if (!bDodgeLanded)
        {
            if (bAirborne && TryEnterWallRun()) {}  // a side jump into a wall runs up it
            else if (!bAirborne && BaseTime > SideTakeoff + .05f)
            {
                bDodgeLanded = true;
                Movement->StopMovementImmediately();
                PlaySfx(LandSounds, ESfx::Land, LandVolume * .75f);
                BaseTime = FMath::Max(BaseTime, SideLand);
            }
            else BaseTime = FMath::Min(BaseTime, SideLand - 1.f / 30.f);
        }
        // Stick held: walk or run on as soon as the landing has settled a little.
        const bool bStick = FVector2D(SideInput(), InputForward).SizeSquared() > .04f;
        if (bDodgeLanded && (BaseTime >= Length || (bStick && BaseTime >= SideLand + .15f))) FinishDodge();
    }
    else if (bAirborne)
    {
        // Walked gently off an edge (not a jump, not at a run): grab it.
        const bool bWalkedOff = GetVelocity().Z < 10.f && RunWeight < .5f && Speed < WalkSpeed * 1.2f
            && (Gait == EGait::Idle || Gait == EGait::Start || Gait == EGait::Loop || Gait == EGait::Stop || Gait == EGait::Strafe);
        if (Gait != EGait::Air && bWalkedOff && TryDropHang()) {}
        else if (Gait != EGait::Air)
        {
            Gait = EGait::Air;
            // A jump at a run (not a fall off an edge) becomes a leap with a
            // little more lift.
            bRunJump = RunWeight > .5f && GetVelocity().Z > 50.f;
            if (GetVelocity().Z > 50.f) PlaySfx(JumpSounds, ESfx::Jump, JumpVolume * (bRunJump ? 1.f : .85f));
            if (bRunJump)
            {
                Movement->Velocity.Z = RunJumpVerticalSpeed;
                SetClip(EClip::RunJump, 0, .08f);
            }
            // Takeoff is runtime-driven, so skip the clip's ground crouch and
            // start at its extension onto the toes.
            else SetClip(EClip::JumpStart, Clips[static_cast<int32>(EClip::JumpStart)]->GetPlayLength() * .5f, .06f);
        }
        if (Gait == EGait::Hang || TryEnterWallRun()) {}  // (a drop-hang just caught the edge)
        else if (bRunJump)
        {
            // Posed over the flight: progress from the vertical speed (0 at
            // takeoff, 1 back at takeoff height), never running backwards.
            const float Progress = FMath::Clamp((RunJumpVerticalSpeed - static_cast<float>(GetVelocity().Z)) / (2.f * RunJumpVerticalSpeed), 0.f, 1.f);
            BaseTime = FMath::Max(BaseTime, Progress * Clips[static_cast<int32>(EClip::RunJump)]->GetPlayLength());
        }
        else
        {
            BaseTime += DeltaSeconds;
            if ((Base == EClip::JumpStart || Base == EClip::WallKick) && BaseTime >= Length)
                SetClip(EClip::JumpLoop, 0, .1f);
        }
    }
    else if (Gait == EGait::Air)
    {
        // The leap ends on the run's foot_L touchdown: with the stick held,
        // land straight into the stride.
        // Back on the ground: every wall is fresh again.
        LastWallNormal = FVector::ZeroVector; bWallJumpFlight = false; WallCoyoteUntil = -1; bChimney = false;
        const bool bStickHeld = bInput || FVector2D(InputRight, InputForward).SizeSquared() > .04f;
        const float Fall = AirApexZ - static_cast<float>(Location.Z);
        if (Fall <= RollFallHeight) PlaySfx(LandSounds, ESfx::Land, LandVolume * FMath::Clamp(.6f + Fall / 120.f, .6f, 1.f));
        if (Fall > RollFallHeight) LandingRoll();
        else if (bRunJump && bStickHeld && Speed > WalkSpeed) { Gait = EGait::Loop; WalkPhase = 0; SetClip(EClip::WalkLoop, 0, .06f); RunWeight = RunBlendAt(Speed); }
        else { Gait = EGait::Land; SetClip(EClip::JumpLand, 0, .06f); bHardLanding = bRunJump; }
        bRunJump = false;
    }
    else if (Gait == EGait::Strafe)
    {
        // Sidestep / bounding shuffle while mostly sideways, phase from the
        // sideways travel; forward or back, the stride (played backward for a
        // back-pedal). Letting go of strafe walks on or settles.
        const FVector Facing = GetActorForwardVector().GetSafeNormal2D();
        const FVector Sideways = FVector::CrossProduct(FVector::UpVector, Facing);
        const FVector Velocity = GetVelocity();
        const float Lateral = static_cast<float>(FVector::DotProduct(Velocity, Sideways));
        const float Ahead = static_cast<float>(FVector::DotProduct(Velocity, Facing));
        if (!bStrafe)
        {
            if (Speed > 3.f) { Gait = EGait::Loop; WalkPhase = 0; SetClip(EClip::WalkLoop, 0, .15f); RunWeight = RunBlendAt(Speed); }
            else { Gait = EGait::Idle; SetClip(EClip::Idle, 0, .2f); }
        }
        else if (!bInput && Speed < 3.f) { Gait = EGait::Idle; SetClip(EClip::Idle, 0, .2f); }
        else if (FMath::Abs(Lateral) >= FMath::Abs(Ahead) * .8f)
        {
            const bool bRunClip = Speed > (StrafeSpeed + StrafeRunSpeed) * .5f;
            const EClip Want = Lateral > 0 ? (bRunClip ? EClip::StrafeRunRight : EClip::StrafeRight) : (bRunClip ? EClip::StrafeRunLeft : EClip::StrafeLeft);
            StrafePhase = FMath::Frac(StrafePhase + Travel * FMath::Abs(Lateral) / FMath::Max(Speed, 1.f) / (bRunClip ? StrafeRunStride : StrafeStride));
            if (Base != Want) SetClip(Want, StrafePhase * Period(Want), .12f);
            BaseTime = StrafePhase * Period(Want);
        }
        else
        {
            const float Stride = FMath::Lerp(WalkStride, RunStride, RunBlendAt(Speed));
            WalkPhase = FMath::Frac(WalkPhase + (Ahead >= 0 ? 1.f : -1.f) * Travel / Stride);
            if (Base != EClip::WalkLoop) SetClip(EClip::WalkLoop, WalkPhase * WalkPeriod, .12f);
            BaseTime = WalkPhase * WalkPeriod;
            RunWeight = FMath::FInterpTo(RunWeight, RunBlendAt(Speed), DeltaSeconds, 10.f);
        }
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
    else if (Gait == EGait::Slash)
    {
        // Standing slash: the capsule follows the clip's step-in (closed loop,
        // as for the roll); a buffered press chains the other paw.
        BaseTime += DeltaSeconds;
        SlashDone += Travel;
        if ((bSlashQueued || bSlashHeld) && BaseTime >= SlashChainAt)
        {
            bSlashQueued = false;
            SetClip(PickPaw(false, Base), 0, .06f);
            SlashDone = 0;
        }
        else if (BaseTime >= Length) FinishDodge();
        if (Gait == EGait::Slash)
        {
            const float Step = FMath::Max(0.f, SlashTravelAt(BaseTime + DeltaSeconds) - SlashDone) / FMath::Max(DeltaSeconds, 1e-4f);
            const FVector Ahead = GetActorForwardVector().GetSafeNormal2D();
            Movement->Velocity = FVector(Ahead.X * Step, Ahead.Y * Step, Movement->Velocity.Z);
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
    // The fall's apex: the ground (or wall, or ledge) height until airborne.
    AirApexZ = Movement->IsFalling() ? FMath::Max(AirApexZ, static_cast<float>(GetActorLocation().Z)) : static_cast<float>(GetActorLocation().Z);
    // Coming to a stop ends the run latch.
    if (Gait == EGait::Idle && GaitBefore != EGait::Idle) bRunHeld = false;
    // During a dodge nothing brakes the capsule but the dodge itself.
    // A landing without input absorbs its momentum within a few cm, before the
    // landing paws lock (0.1 s), instead of walking on into a stop.
    Movement->BrakingDecelerationWalking = OwnsCapsule() ? 0.f
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
    // Slash layers on the move (a standing slash plays as the base clip).
    P.ClipUpper[0] = FadingLayerTime >= 0 ? Clips[static_cast<int32>(FadingLayerClip)] : nullptr;
    P.TimeUpper[0] = FadingLayerTime;
    P.WeightUpper[0] = FadingLayerWeight;
    P.ClipUpper[1] = LayerTime >= 0 ? Clips[static_cast<int32>(LayerClip)] : nullptr;
    P.TimeUpper[1] = LayerTime;
    P.WeightUpper[1] = LayerTime >= 0 ? LayerWeightAt(LayerTime, Clips[static_cast<int32>(LayerClip)]->GetPlayLength()) : 0.f;
    P.bFootIK = !bAirborne;
    P.bAllowSettle = Gait == EGait::Idle;
    // Stance from the manifest intervals of whichever clip dominates. WalkLoop:
    // generated from the manifest (ChuckClipData.h), trimmed likewise.
    const bool bLoopDominant = ((Gait == EGait::Loop || (Gait == EGait::Strafe && Base == EClip::WalkLoop)) && FadeWeight < .5f) || (Gait == EGait::Stop && FadeWeight >= .5f);
    const bool bStanding = Gait == EGait::Idle || Gait == EGait::Astral || (Gait == EGait::Land && StateTime > .1f);   // the summon keeps his paws planted
    const FStance* Clip = nullptr;
    if (Gait == EGait::Start) Clip = &StartStance;
    else if (Gait == EGait::Stop && !bLoopDominant) Clip = bStopMirror ? &StopStanceMirrored : &StopStance;
    else if (Gait == EGait::Turn) Clip = Base == EClip::TurnLeft90 ? &TurnLeftStance : &TurnRightStance;
    else if (Gait == EGait::Roll) Clip = &RollStance;
    else if (Gait == EGait::SideJump) Clip = &SideJumpStance;
    else if (Gait == EGait::Slash) Clip = (Base == EClip::SlashRight || Base == EClip::SlashLowRight) ? &SlashRightStance : &SlashLeftStance;   // same footwork
    else if (Gait == EGait::Strafe && Base == EClip::StrafeLeft) Clip = &StrafeLeftStance;
    else if (Gait == EGait::Strafe && Base == EClip::StrafeRight) Clip = &StrafeRightStance;
    else if (Gait == EGait::Strafe && Base == EClip::StrafeRunLeft) Clip = &StrafeRunLeftStance;
    else if (Gait == EGait::Strafe && Base == EClip::StrafeRunRight) Clip = &StrafeRunRightStance;
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
    // Footsteps: a paw planting (stance begins) on the ground gaits.
    {
        const bool bStepGait = !bAirborne && (Gait == EGait::Start || Gait == EGait::Loop || Gait == EGait::Stop || Gait == EGait::Strafe || Gait == EGait::Turn);
        const FChuckAnimResult Pose = Anim->GetResult();
        for (int32 I = 0; I < 2; ++I)
        {
            if (bStepGait && P.bStance[I] && !bPrevStance[I]) PlayStep(Pose.Evaluations ? Pose.BallWorld[I] : Location, Gait == EGait::Turn ? ChuckClipData::WalkSpeed * .6f : Speed);
            bPrevStance[I] = P.bStance[I];
        }
    }
    // On a wall the paws follow the clip (planted on the wall plane).
    if (Gait == EGait::WallRun || Gait == EGait::Hang || Gait == EGait::Climb) P.bFootIK = false;
    // Tucked in the roll, the paws follow the clip untouched.
    if (Gait == EGait::Roll && !P.bStance[0] && !P.bStance[1]) P.bFootIK = false;

    // Place the mesh on the traced ground under the capsule, then offset each
    // paw by its own traced ground and drop the pelvis for a lower paw.
    const float CapsuleBottom = Location.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const bool bOffGround = bAirborne || Gait == EGait::WallRun || Gait == EGait::Hang || Gait == EGait::Climb;
    const float Ground = bOffGround ? CapsuleBottom : FindGround(Location, CapsuleBottom);
    MeshDrop = FMath::FInterpTo(MeshDrop, FMath::Clamp(CapsuleBottom - Ground, 0.f, 4.f), DeltaSeconds, 20.f);
    GetMesh()->SetRelativeLocation(FVector(0, 0, -32.5f - MeshDrop));
    const FChuckAnimResult Last = Anim->GetResult();
    const float MeshZ = CapsuleBottom - MeshDrop;
    float Lowest = 0;
    for (int32 I = 0; I < 2; ++I)
    {
        const FVector Near = Last.Evaluations ? Last.BallWorld[I] : Location;
        P.GroundOffset[I] = bOffGround ? 0.f : FMath::Clamp(FindGround(Near, MeshZ) - MeshZ, -6.f, 6.f);
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
    GetMesh()->SetVisibility(!bAstralHidden && FVector::Dist(Camera->GetComponentLocation(),GetActorLocation()) > 70.f,true);
    UpdateMotion(DeltaSeconds);
    UpdateExhale(DeltaSeconds);
    if (GetActorLocation().Z < -100) ResetToDock();
}
