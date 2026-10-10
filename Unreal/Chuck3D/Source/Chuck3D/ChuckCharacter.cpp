#include "ChuckCharacter.h"
#include "DockSewer.h"
#include "DockPantry.h"
#include "SewerSlide.h"
#include "ChuckClimbable.h"
#include "ChuckAnimInstance.h"
#include "ChuckClipData.h"
#include "ChuckBreakable.h"
#include "EnemyRat.h"
#include "AstralSummon.h"
#include "DockNPC.h"
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
    static const TCHAR* ClipNames[] = {TEXT("Idle"), TEXT("WalkStart"), TEXT("WalkLoop"), TEXT("WalkStop"), TEXT("TurnLeft90"), TEXT("TurnRight90"), TEXT("JumpStart"), TEXT("JumpLoop"), TEXT("JumpLand"), TEXT("Roll"), TEXT("SideJumpLeft"), TEXT("SideJumpRight"), TEXT("RunLoop"), TEXT("RunJump"), TEXT("SlashRight"), TEXT("SlashLeft"), TEXT("WallRun"), TEXT("WallKick"), TEXT("Hang"), TEXT("PullUp"), TEXT("Mantle"), TEXT("ShimmyLeft"), TEXT("ShimmyRight"), TEXT("StrafeLeft"), TEXT("StrafeRight"), TEXT("StrafeRunLeft"), TEXT("StrafeRunRight"), TEXT("SlashLowRight"), TEXT("SlashLowLeft"), TEXT("Summon"), TEXT("SpeedVault"), TEXT("SprintLoop"), TEXT("SprintLeap"), TEXT("Swing"), TEXT("SwingLeapLeft"), TEXT("SwingLeapRight")};
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
    Load(StreamSplashSounds, TEXT("SFX_StreamSplash"), 6);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_STREAM_AUDIO loaded=%d"), StreamSplashSounds.Num());
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
    if(IsInDockSewerStream(Paw) && StreamSplashSounds.Num())
    {
        PlaySfx(StreamSplashSounds, ESfx::Step, Speed>ChuckClipData::WalkSpeed*1.25f ? .4f : .32f);
        ++StreamStepCount;
        UE_LOG(LogTemp, Verbose, TEXT("CHUCK_STREAM_STEP count=%d"), StreamStepCount);
        return;
    }
    // Stone or wood under the paw (dock materials are named M_<Surface>).
    bool bStone = false;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckStepSurface), false, this);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Paw + FVector(0, 0, 10), Paw - FVector(0, 0, 15), ECC_Visibility, Query))
        if (const UPrimitiveComponent* Floor = Hit.GetComponent())
            if (const UMaterialInterface* Surface = Floor->GetMaterial(0))
                bStone = Surface->GetName().Contains(TEXT("Stone")) || Surface->GetName().Contains(TEXT("Plaster")) || Surface->GetName().Contains(TEXT("SewerRock"));
    const bool bRun = Speed > (ChuckClipData::WalkSpeed + ChuckClipData::StrafeRunSpeed) * .5f;
    if (bRun) PlaySfx(bStone ? StepRunStone : StepRunWood, ESfx::Step, RunStepVolume);
    else PlaySfx(bStone ? StepWalkStone : StepWalkWood, ESfx::Step, WalkStepVolume * FMath::Clamp(Speed / ChuckClipData::WalkSpeed, .6f, 1.f));
}
UChuckAnimInstance* AChuckCharacter::GetChuckAnim() const { return Cast<UChuckAnimInstance>(GetMesh()->GetAnimInstance()); }
int32 AChuckCharacter::GetGroomCount() const { return Grooms.Num(); }
const TCHAR* AChuckCharacter::GetGaitName() const
{
    static const TCHAR* Names[] = {TEXT("Idle"), TEXT("Start"), TEXT("Loop"), TEXT("Stop"), TEXT("Turn"), TEXT("Air"), TEXT("Land"), TEXT("Roll"), TEXT("SideJump"), TEXT("Slash"), TEXT("WallRun"), TEXT("Hang"), TEXT("Climb"), TEXT("Strafe"), TEXT("Astral"), TEXT("WallSide"), TEXT("Ladder"), TEXT("Vault"), TEXT("Swing"), TEXT("SwingLeap")};
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
    Input->BindAction("Interact", IE_Pressed, this, &AChuckCharacter::Interact);
    Input->BindAction("Dodge", IE_Pressed, this, &AChuckCharacter::Dodge);
    Input->BindAction("Run", IE_Pressed, this, &AChuckCharacter::RunPressed);
    Input->BindAction("Sprint", IE_Pressed, this, &AChuckCharacter::SprintPressed);
    Input->BindAction("Slash", IE_Pressed, this, &AChuckCharacter::Slash);
    Input->BindAction("Slash", IE_Released, this, &AChuckCharacter::SlashReleased);
    Input->BindAction("Quit", IE_Pressed, this, &AChuckCharacter::Quit);
}
// A dodge owns the capsule; the stick is still read to choose the next move.
void AChuckCharacter::Forward(float Value) { InputForward = Value; if (!OwnsCapsule() && !IsTalking()) AddMovementInput(FRotator(0,ViewYaw,0).Vector(),Value); }
void AChuckCharacter::Right(float Value) { InputRight = Value; if (!OwnsCapsule() && !IsTalking()) AddMovementInput(FRotationMatrix(FRotator(0,ViewYaw,0)).GetUnitAxis(EAxis::Y),Value); }
// Q/E strafe: sideways along the camera without turning (the facing is held in UpdateMotion).
void AChuckCharacter::StrafeKeysAxis(float Value) { StrafeKeys = Value; if (!OwnsCapsule() && !IsTalking()) AddMovementInput(FRotationMatrix(FRotator(0,ViewYaw,0)).GetUnitAxis(EAxis::Y),Value); }
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
    // Swinging ring to ring: behind him, looking down the line of rings.
    if (DeltaSeconds > 0 && LookIdle > .3f && (Gait == EGait::Swing || Gait == EGait::SwingLeap))
        ViewYaw = FRotator::NormalizeAxis(ViewYaw + FMath::FindDeltaAngleDegrees(ViewYaw, GetActorRotation().Yaw) * FMath::Min(1.f, DeltaSeconds * 3.f));
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
    bSewerRespawn = bPantryRespawn = false;
    ResetAtLocation(StartLocation());
}
FVector AChuckCharacter::GetAreaStartLocation() const
{
    const bool Sewer=bSewerRespawn || (GetActorLocation().Z<-150 && IsWithinDockSewer(GetActorLocation()));
    if (bPantryRespawn) return DockPantryStartLocation();
    if (!Sewer) return StartLocation();
    // At or beyond the checkpoint before the wall-run rupture: back to it, not the entrance.
    const int32 Sample=DockSewerNearestSample(GetActorLocation());
    if(Sample!=INDEX_NONE && Sample>=DockSewerLeapCheckpointSample()) return DockSewerLeapCheckpointLocation();
    return Sample!=INDEX_NONE && Sample>=DockSewerCheckpointSample() ? DockSewerCheckpointLocation() : DockSewerStartLocation();
}
float AChuckCharacter::AreaStartYaw(const FVector& Location)
{
    if (Location.Z>=-150 || IsWithinDockPantry(Location)) return 0.f;
    if(FVector::Dist(Location,DockSewerLeapCheckpointLocation())<1.f) return DockSewerLeapCheckpointYaw();
    return FVector::Dist(Location,DockSewerCheckpointLocation())<1.f ? DockSewerCheckpointYaw() : 90.f;
}
void AChuckCharacter::RespawnAtAreaStart()
{
    ResetAtLocation(GetAreaStartLocation());
}
void AChuckCharacter::ResetAtLocation(const FVector& Location)
{
    GetCharacterMovement()->StopMovementImmediately();
    SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
    // A checkpoint teleport is not a fall from the previous area's elevation.
    AirApexZ = static_cast<float>(Location.Z);
    bPantryRespawn=IsWithinDockPantry(Location);
    bSewerRespawn=Location.Z<-150 && !bPantryRespawn;
    ViewYaw = bSewerRespawn ? AreaStartYaw(Location) : 0.f;
    LadderIndex = -1; bClimbReverse = false;
    bSprinting = false; SprintWeight = 0; Stamina = 1.f; RunupLength = 0; RunupDir = FVector::ZeroVector;   // a fresh start is a rested rat
    SetActorRotation(FRotator(0,ViewYaw,0));
    LookPitch = SmoothLook = FMath::Min(LookPitch, RatPitch);  // keep the chosen height
    Gait = EGait::Idle;
    Base = Fading = EClip::Idle;
    BaseTime = FadingTime = FadeWeight = StateTime = StartDistance = WalkPhase = StopTravel = 0;
    bStopPending = bStopMirror = bDodgeLaunched = bDodgeLanded = false;
    InputForward = InputRight = StrafeKeys = StrafeTrigger = 0;  // refreshed every frame while input is live
    bTestStrafe = bHangNeedsRelease = false; GetCharacterMovement()->bOrientRotationToMovement = true;
    bRunJump = bSprintLeap = bSprintLeapPending = bHardLanding = false;
    bSlashQueued = bSlashHeld = false; LayerTime = FadingLayerTime = -1; FadingLayerWeight = 0; SlashHitAt = -1; BiteImmuneUntil = -1;
    RestoreSlideCamera(); bAutoClimb = false;
    Sanity = MaxSanity; AstralPhase = EAstral::None; bPendingVanish = false; SetAstralHidden(false);
    TalkingTo.Reset(); TalkLine = 0;
    if (auto* PC = Cast<APlayerController>(Controller)) if (PC->PlayerCameraManager) PC->PlayerCameraManager->StopCameraFade();
    LastWallNormal = FVector::ZeroVector; bWallJumpFlight = bWallAuto = bChimney = false; WallCoyoteUntil = -1; AirJumpPressedAt = -1e3f; LedgeCooldownUntil = -1;
    StaminaLockUntil = -1.f;
    SwingGrip = SwingTarget = SwingLeft = -1; SwingRegrabAt = -1; bSwingJumpQueued = bHangClimbQueued = bBodyPitchLive = false; BodyPitch = 0;
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
    // The sprint pose blends over the run on a timer (he drops onto all fours
    // as the burst starts, not once he is up to speed); the stride follows it.
    float LoopStride(float Speed, float Sprint) { return FMath::Lerp(FMath::Lerp(WalkStride, RunStride, RunBlendAt(Speed)), SprintStride, Sprint); }
    constexpr float SprintBlendIn = 10.f, SprintBlendOut = 7.f;   // 1/s
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
    if (IsAstral() || IsTalking()) return;
    if (Gait == EGait::Hang) { DropFromHang(); return; }  // dodge while hanging: let go
    if (Gait == EGait::Swing) { LeaveSwing(SwingDir * 30.f, false); return; }
    if (Gait == EGait::SwingLeap) return;
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
void AChuckCharacter::StartSprintLeap()
{
    // Pushing off from the gallop, along the way he's going.
    auto* Movement = GetCharacterMovement();
    const FVector Along = GetVelocity().GetSafeNormal2D().IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : GetVelocity().GetSafeNormal2D();
    Movement->Velocity = Along * FMath::Max(static_cast<float>(GetVelocity().Size2D()), ChuckClipData::SprintLeapSpeed) + FVector(0, 0, ChuckClipData::SprintLeapVerticalSpeed);
    Gait = EGait::Air; bSprintLeap = true; bSprintLeapPending = bRunJump = false; SprintAirTime = 0;
    ++SprintLeaps;
    SetClip(EClip::SprintLeap, 0, .08f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SPRINT_LEAP takeoff=%d at=%s"), SprintLeaps, *GetActorLocation().ToString());
}
bool AChuckCharacter::TrySprint()
{
    auto* Movement = GetCharacterMovement();
    if (IsSprinting() || Stamina < 1.f || IsAstral() || IsTalking() || OwnsCapsule() || Movement->IsFalling() || StrafeHeld()) return false;
    if (Gait != EGait::Idle && Gait != EGait::Start && Gait != EGait::Loop && Gait != EGait::Stop && Gait != EGait::Land) return false;
    // Action events dispatch before this frame's axes: read the stick directly
    // so a direction pressed together with Ctrl counts. Standing still, the
    // press does nothing and costs nothing.
    if (InputComponent)
    {
        const float R = InputComponent->GetAxisValue(TEXT("Right")), F = InputComponent->GetAxisValue(TEXT("Forward"));
        if (R != 0 || F != 0) { InputRight = R; InputForward = F; }
    }
    if (FVector2D(InputRight, InputForward).SizeSquared() <= .04f) return false;
    bSprinting = true;
    RunupDir = FVector::ZeroVector; RunupLength = 0;
    ++Sprints;
    bRunHeld = true;   // he comes out of it still running
    bStopPending = false;
    if (Gait != EGait::Loop)
    {
        Gait = EGait::Loop; StateTime = 0;
        if (Base != EClip::WalkLoop) { WalkPhase = 0; SetClip(EClip::WalkLoop, 0, .12f); }
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SPRINT start=%d speed=%.1f"), Sprints, GetVelocity().Size2D());
    return true;
}
void AChuckCharacter::EndSprint(bool bShed)
{
    bSprinting = false;   // what stamina is left stays, and refills from there
    // Leaps, vaults and wall runs keep their run-speed tuning: the gaps and
    // walls were laid out for it.
    auto* Movement = GetCharacterMovement();
    const FVector Flat(Movement->Velocity.X, Movement->Velocity.Y, 0);
    if (bShed && Flat.Size() > ChuckClipData::RunSpeed)
    {
        const FVector Run = Flat.GetSafeNormal() * ChuckClipData::RunSpeed;
        Movement->Velocity = FVector(Run.X, Run.Y, Movement->Velocity.Z);
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SPRINT end=%d shed=%d gait=%s at=%s air_s=%.2f stamina=%.3f"), Sprints, bShed ? 1 : 0, GetGaitName(), *GetActorLocation().ToString(), SprintAirTime, Stamina);
}
void AChuckCharacter::LockStamina(float Seconds)
{
    StaminaLockUntil = FMath::Max(StaminaLockUntil, GetWorld()->GetTimeSeconds() + Seconds);
    Stamina = 1.f;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_STAMINA_LOCK seconds=%.1f"), Seconds);
}
bool AChuckCharacter::IsStaminaLocked() const { return GetWorld() && GetWorld()->GetTimeSeconds() < StaminaLockUntil; }
void AChuckCharacter::SpendStamina(float Cost)
{
    if (!IsStaminaLocked()) Stamina = FMath::Max(0.f, Stamina - Cost);
}
bool AChuckCharacter::IsClimbing() const
{
    return Gait == EGait::WallRun || Gait == EGait::WallSide || Gait == EGait::Swing || Gait == EGait::SwingLeap || (Gait == EGait::Air && bWallJumpFlight);
}
float AChuckCharacter::GetSprintCooldownLeft() const
{
    return IsSprinting() ? SprintCooldown : (1.f - Stamina) * SprintCooldown;
}
void AChuckCharacter::JumpPressed()
{
    auto* Movement = GetCharacterMovement();
    const float Now = GetWorld()->GetTimeSeconds();
    if (Gait == EGait::Climb || IsAstral() || IsTalking()) return;
    if (Gait == EGait::Hang)
    {
        // From a hang: pulling away and jumping kicks off backward; otherwise
        // climb up (the stick still held from before the catch doesn't count;
        // pressed during the catch itself, up once he has hold).
        if (!bHangNeedsRelease && FVector::DotProduct(StickWorld(), -HangNormal) < -.3f) { WallNormal = HangNormal; WallJump(); }
        else if (bHangRoom && HangClock < HangSnapTime) bHangClimbQueued = true;
        else if (bHangRoom) StartClimb(false, HangNormal, HangEdge);
        return;
    }
    if (Gait == EGait::Swing) { SwingJump(); return; }
    // Pressed in the flight between rings: on to the next as soon as he has this one.
    if (Gait == EGait::SwingLeap) { if (SwingLeapClock > SwingLeapTime - .35f) bSwingJumpQueued = true; return; }
    if (Gait == EGait::Ladder && GetChuckClimbables().IsValidIndex(LadderIndex))
    {
        WallNormal = GetChuckClimbables()[LadderIndex].Out; LadderIndex = -1;
        LadderCooldownUntil = Now + .6f;
        WallJump(); return;
    }
    if (Gait == EGait::WallRun || Gait == EGait::WallSide || (Movement->IsFalling() && Now < WallCoyoteUntil)) { WallJump(); return; }
    // Off a small step at a sprint (still within its grace): the leap all the same.
    if (Movement->IsFalling() && IsSprinting() && Gait == EGait::Air && !bSprintLeap && SprintAirTime < SprintDropGrace && RunupLength >= SprintLeapRunup) { StartSprintLeap(); PlaySfx(JumpSounds, ESfx::Jump, JumpVolume); return; }
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
    // Vaults and side wall runs out of a sprint are tuned for the run: try
    // them at run speed (they take over the capsule, which ends the sprint).
    const FVector Before = Movement->Velocity;
    if (IsSprinting())
    {
        const FVector Run = FVector(Before.X, Before.Y, 0).GetClampedToMaxSize(ChuckClipData::RunSpeed);
        Movement->Velocity = FVector(Run.X, Run.Y, Before.Z);
    }
    // At a run at something low and thin: over it in stride.
    if (bGrounded && Gait == EGait::Loop && RunWeight > .5f && TryVault()) return;
    // At a run with a wall right beside him: along the wall (not a side jump, not from a walk).
    if (bGrounded && Gait == EGait::Loop && RunWeight > .5f && TryWallSideRun()) return;
    Movement->Velocity = Before;
    // At a sprint with a run-up behind him: the long leap, carrying on into
    // the gallop. Without one, the ordinary running jump (the sprint ends,
    // keeping its stamina).
    bSprintLeapPending = IsSprinting() && bGrounded && Gait == EGait::Loop && RunupLength >= SprintLeapRunup;
    if (IsSprinting() && !bSprintLeapPending) EndSprint(true);
    Jump();
}
bool AChuckCharacter::TryEnterWallRun()
{
    // Jump into a wall while pushing toward it (or arrive from a wall jump)
    // and Chuck runs up it. Generous: a sphere probe reaching WallReach past
    // the capsule, anything within 60 degrees of head-on, any wall that isn't
    // the one he just left.
    auto* Movement = GetCharacterMovement();
    FVector Probe = (bWallJumpFlight || Gait == EGait::SideJump) ? Movement->Velocity.GetSafeNormal2D() : StickWorld().GetSafeNormal2D();
    // Stopped short by an eave or ledge in a wall jump's flight: still the way he faces.
    if (Probe.IsNearlyZero() && bWallJumpFlight) Probe = GetActorForwardVector().GetSafeNormal2D();
    if (Probe.IsNearlyZero()) return false;
    const float Reach = GetCapsuleComponent()->GetScaledCapsuleRadius() + WallReach;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckWall), false, this);
    bool bHit = false, bFeetOnly = false;
    for (const float Height : {5.f, -15.f, -21.f, -27.f})  // chest, then hips (a top below his chest), then lower (a top at his hips)
    {
        const FVector From = GetActorLocation() + FVector(0, 0, Height);
        bHit = GetWorld()->SweepSingleByChannel(Hit, From, From + Probe * (Reach - 4.f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(4.f), Query)
            && FMath::Abs(Hit.ImpactNormal.Z) <= .3f;
        if (bHit) { bFeetOnly = Height < -20.f; break; }
    }
    if (!bHit) return TryGrabEdge(Probe);
    const FVector Normal = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0).GetSafeNormal();
    if (FVector::DotProduct(Probe, -Normal) < .5f) return false;
    // A top edge within reach: grab it (any wall, even the one just left).
    FVector Edge; bool bRoom = false;
    if (Movement->Velocity.Z > -400.f && GetWorld()->GetTimeSeconds() >= LedgeCooldownUntil)
    {
        if (!bFeetOnly && FindLedge(Normal, Hit.ImpactPoint, -15.f, 45.f, Edge, bRoom))
        {
            EnterHang(Normal, Edge, bRoom);
            return true;
        }
        // Arriving with the top already at his hips (a leap a little high):
        // scramble straight on rather than bounce off the edge.
        if (FindLedge(Normal, Hit.ImpactPoint, -34.f, -15.f, Edge, bRoom) && bRoom)
        {
            StartClimb(true, Normal, Edge);
            ++LedgeScrambles;
            return true;
        }
    }
    if (TryGrabEdge(Probe)) return true;
    if (bFeetOnly || Movement->Velocity.Z < -250.f) return false;   // no wall run off a face below his hips
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
    bWallAuto = bWallJumpFlight || bFromSideJump; bWallJumpFlight = false; bRunJump = bSprintLeap = false;
    {
        FHitResult Behind;
        FCollisionQueryParams BehindQuery(SCENE_QUERY_STAT(ChuckChimney), false, this);
        const FVector Center = GetActorLocation();
        bChimney = GetWorld()->LineTraceSingleByChannel(Behind, Center, Center + Normal * 250.f, ECC_Visibility, BehindQuery)
            && FVector::DotProduct(FVector(Behind.ImpactNormal.X, Behind.ImpactNormal.Y, 0).GetSafeNormal(), -Normal) > .8f;
    }
    ++WallRuns;
    SpendStamina(WallRunStaminaCost);
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
    // A "top" with something solid right on it is a face inside a roof or wall (the trace began inside it), not an edge.
    if (GetWorld()->OverlapBlockingTestByChannel(Top.ImpactPoint + FVector(0, 0, 3.f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(1.5f), Query)) return false;
    // Nothing just above the top: the lip trace follows the top's own slope
    // inward, so a pitched roof's eave counts as an edge (a level trace would
    // run into the rising roof and reject it).
    const float Rise = -FVector::DotProduct(Top.ImpactNormal, -Normal) / Top.ImpactNormal.Z;   // cm up per cm inward
    FHitResult Above;
    const FVector Lip = FVector(FacePoint.X, FacePoint.Y, Top.ImpactPoint.Z + 6.f - 10.f * Rise) + Normal * 2.f;
    if (GetWorld()->LineTraceSingleByChannel(Above, Lip, Lip - Normal * 14.f + FVector(0, 0, 14.f * Rise), ECC_Visibility, Query)) return false;
    // An overhang (a roof's eave, a coping stone): the top carries on out past
    // the face below. His paws take its outer lip, where it really ends.
    FVector LipAt = FacePoint;
    float LipZ = static_cast<float>(Top.ImpactPoint.Z);
    for (float Out = 2.f; Out <= 24.f; Out += 2.f)
    {
        const FVector At = FacePoint + Normal * Out;
        const float Expect = static_cast<float>(Top.ImpactPoint.Z) - Rise * (Out + 8.f);
        FHitResult Eave;
        if (!GetWorld()->LineTraceSingleByChannel(Eave, FVector(At.X, At.Y, Expect + 6.f), FVector(At.X, At.Y, Expect - 6.f), ECC_Visibility, Query)
            || Eave.bStartPenetrating || Eave.ImpactNormal.Z < .7f) break;
        LipAt = At; LipZ = static_cast<float>(Eave.ImpactPoint.Z);
    }
    OutEdge = FVector(LipAt.X, LipAt.Y, LipZ);
    if (IsDockPantrySkyRim(OutEdge)) return false;   // the sky hole's rim crumbles under his paws
    FVector Stand;
    bRoom = FindStand(Normal, OutEdge, Radius + 4.f, Stand);
    return true;
}
bool AChuckCharacter::FindStand(const FVector& Normal, const FVector& Edge, float In, FVector& OutStand) const
{
    // Up on the top, a little in from the edge; on a slope he settles where
    // the capsule's round bottom rests, and a spot that is blocked (a chimney,
    // the roof rising into it) moves him further in.
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckStand), false, this);
    for (const float Extra : {0.f, 6.f, 14.f, 24.f})
    {
        const FVector At = Edge - Normal * (In + Extra);
        FHitResult Floor;
        const FVector From(At.X, At.Y, Edge.Z + 30.f + .9f * (In + Extra)), To(At.X, At.Y, Edge.Z - 12.f);
        if (!GetWorld()->LineTraceSingleByChannel(Floor, From, To, ECC_Visibility, Query) || Floor.bStartPenetrating || Floor.ImpactNormal.Z < .7f) continue;
        const FVector Stand(At.X, At.Y, Floor.ImpactPoint.Z + Half + 1.f + Radius * (1.f / Floor.ImpactNormal.Z - 1.f));
        if (!GetWorld()->OverlapBlockingTestByChannel(Stand + FVector(0, 0, 1.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, Half), Query))
        {
            OutStand = Stand;
            return true;
        }
    }
    return false;
}
FVector AChuckCharacter::HangHoldAt(const FVector& Edge) const
{
    return Edge + HangNormal * (GetCapsuleComponent()->GetScaledCapsuleRadius() + .5f + HangOut) - FVector(0, 0, HangDrop + HangLower);
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
    // Under an eave the usual hang would put his head in the roof: hang a
    // little further out and lower instead, clear of it.
    HangOut = HangLower = 0;
    {
        const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckHangClear), false, this);
        const auto Clear = [&](float Out, float Lower)
        {
            HangOut = Out; HangLower = Lower;
            return !GetWorld()->OverlapBlockingTestByChannel(HangHoldAt(Edge), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius - 1.f, Half - 1.f), Query);
        };
        bool bClear = false;
        for (const float Lower : {0.f, 8.f, 16.f, 24.f})
        {
            for (const float Out : {0.f, 5.f, 10.f, 15.f}) if (Clear(Out, Lower)) { bClear = true; break; }
            if (bClear) break;
        }
        if (!bClear) HangOut = HangLower = 0;
    }
    HangClock = 0; HangHold = 0; bHangRoom = bRoom; HangSnapTime = .12f; HangYawFrom = (-Normal).Rotation().Yaw; bCornerCarry = false;
    // Still pushing the way the last wall jump went (away from this wall): that
    // stick doesn't let go of the edge he just caught; it counts once released.
    bHangNeedsRelease = FVector::DotProduct(StickWorld(), -Normal) < -.3f;
    // Jump pressed on the way up to it: over the top as soon as he has hold.
    bHangClimbQueued = bRoom && GetWorld()->GetTimeSeconds() - AirJumpPressedAt < .3f;
    if (bHangClimbQueued) AirJumpPressedAt = -1e3f;
    LastWallNormal = Normal; bWallJumpFlight = false; WallCoyoteUntil = -1; bRunJump = bSprintLeap = false;
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
    bDodgeLaunched = bDodgeLanded = bStopPending = bRunJump = bSprintLeap = bSprintLeapPending = bHardLanding = false;
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
    bClimbReverse = false;
    ClimbDir = -Normal;
    // To where he will stand: the first clear spot in from the edge (on a
    // pitched roof, up the slope); a flat top gives the authored distances.
    FVector Stand;
    if (!FindStand(Normal, Edge, bMantle ? 17.f : 18.5f, Stand)) Stand = Edge - Normal * (bMantle ? 17.f : 18.5f) + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 1.f);
    ClimbRise = static_cast<float>(Stand.Z - ClimbStart.Z);
    ClimbAdvance = static_cast<float>(FVector::DotProduct(Stand - ClimbStart, -Normal));
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
bool AChuckCharacter::TryGrabEdge(const FVector& Probe)
{
    // "Look hard, be easy": any top edge his paws can reach ahead of him is
    // caught - a wall's top, the cut end of a pitched roof's eave, and an eave
    // jutting out over the wall he came up (it stops his head before the
    // usual reach finds its top: he stretches up for its lip instead).
    auto* Movement = GetCharacterMovement();
    if (Probe.IsNearlyZero() || Movement->Velocity.Z < -400.f || GetWorld()->GetTimeSeconds() < LedgeCooldownUntil) return false;
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Center = GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckGrabEdge), false, this);
    FHitResult Over;
    const FVector Head = Center + Probe * (Radius * .5f) + FVector(0, 0, Half - 2.f);
    const bool bUnder = GetWorld()->LineTraceSingleByChannel(Over, Head, Head + FVector(0, 0, 28.f), ECC_Visibility, Query) && Over.ImpactNormal.Z < -.3f;
    for (const float Height : {24.f, 12.f, 0.f, -12.f})
    {
        FHitResult Hit;
        const FVector From = Center + FVector(0, 0, Height);
        if (!GetWorld()->SweepSingleByChannel(Hit, From, From + Probe * (Radius + WallReach + 2.f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(4.f), Query)
            || Hit.bStartPenetrating || Hit.ImpactNormal.Z > .3f || Hit.ImpactNormal.Z < -.65f) continue;   // a face: upright, or an eave's cut end sloping under
        const FVector Normal = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0).GetSafeNormal();
        if (Normal.IsNearlyZero() || FVector::DotProduct(Probe, -Normal) < .5f) continue;
        FVector Edge; bool bRoom = false;
        if (FindLedge(Normal, Hit.ImpactPoint, -15.f, bUnder ? 75.f : 45.f, Edge, bRoom) && Edge.Z - Center.Z <= (bUnder ? 64.f : 45.f))
        {
            if (bUnder) ++EaveGrabs;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_EDGE_GRAB under=%d height=%.0f edge_above=%.1f room=%d at=%s"), bUnder ? 1 : 0, Height, Edge.Z - Center.Z, bRoom ? 1 : 0, *Edge.ToString());
            EnterHang(Normal, Edge, bRoom);
            return true;
        }
    }
    return false;
}
FVector AChuckCharacter::SwingCentre(const FVector& Grip, float BodyDeg) const
{
    // Hands at the grip, the body pitched BodyDeg about it (positive: feet
    // forward); upright, the clip's grip is ahead of and above his centre.
    const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
    const FVector Off = Forward * ChuckClipData::SwingGripX + FVector(0, 0, ChuckClipData::SwingGripZ - GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    const FQuat R(FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal(), FMath::DegreesToRadians(BodyDeg));
    return Grip - R.RotateVector(Off);
}
namespace
{
    // Upright, the clip's grip is ahead of his centre: this much body pitch puts
    // it straight above (the pendulum's rest). Swing length: grip to centre.
    float SwingRestTilt() { return FMath::RadiansToDegrees(FMath::Atan2(ChuckClipData::SwingGripX, ChuckClipData::SwingGripZ - 32.5f)); }
    float SwingLength() { return FMath::Sqrt(FMath::Square(ChuckClipData::SwingGripX) + FMath::Square(ChuckClipData::SwingGripZ - 32.5f)); }
    constexpr float SwingGravity = 980.f * .8f;   // his GravityScale
}
bool AChuckCharacter::TrySwingCatch()
{
    // His paws, reaching up, find a ring near them: he takes it. Generous - a
    // wall jump that passes near one catches it without aiming at it exactly.
    if (Gait == EGait::Swing || Gait == EGait::SwingLeap || Gait == EGait::Hang || Gait == EGait::Climb || IsAstral()) return false;
    if (GetCharacterMovement()->Velocity.Z < -500.f) return false;
    const TArray<FChuckSwingGrip>& Grips = GetChuckSwingGrips();
    const FVector Center = GetActorLocation();
    const FVector Paws = Center + FVector(0, 0, ChuckClipData::SwingGripZ - GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    const float Now = GetWorld()->GetTimeSeconds();
    int32 Best = -1; float BestScore = 1e9f;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckSwingCatch), false, this);
    for (int32 I = 0; I < Grips.Num(); ++I)
    {
        if (I == SwingLeft && Now < SwingRegrabAt) continue;
        const FVector Delta = Grips[I].Grip - Paws;
        const float Side = static_cast<float>(Delta.Size2D());
        if (Side > SwingCatchReach || Delta.Z > 30.f || Delta.Z < -45.f) continue;
        const float Score = Side + FMath::Abs(static_cast<float>(Delta.Z)) * .5f;
        FHitResult Block;
        if (Score < BestScore && !GetWorld()->LineTraceSingleByChannel(Block, Center, Grips[I].Grip, ECC_Visibility, Query)) { Best = I; BestScore = Score; }
    }
    if (Best < 0) return false;
    EnterSwing(Best, false);
    return true;
}
void AChuckCharacter::EnterSwing(int32 Index, bool bFromLeap)
{
    auto* Movement = GetCharacterMovement();
    const FChuckSwingGrip& Grip = GetChuckSwingGrips()[Index];
    const FVector Velocity = Movement->Velocity;
    // Swung along its line (the lanterns' alley), whichever way he was going or facing.
    // Caught crossing its line (a chimney's wall jump), the way the camera looks along it.
    FVector Heading = bFromLeap ? SwingLeapDir : (Velocity.SizeSquared2D() > 2500.f ? Velocity.GetSafeNormal2D() : GetActorForwardVector().GetSafeNormal2D());
    if (!bFromLeap && FMath::Abs(FVector::DotProduct(Heading, Grip.Along.GetSafeNormal2D())) < .35f) Heading = FRotator(0, ViewYaw, 0).Vector();
    SwingDir = Grip.Along.GetSafeNormal2D() * (FVector::DotProduct(Heading, Grip.Along) >= 0.f ? 1.f : -1.f);
    Movement->SetMovementMode(MOVE_Flying);
    Movement->StopMovementImmediately();
    const float L = SwingLength();
    if (bFromLeap)
    {
        // Caught with the body behind the ring, carrying on through: up to about
        // 35 degrees in front before it swings back.
        SwingAngle = FMath::DegreesToRadians(ChuckClipData::SwingCatch * ChuckClipData::SwingMaxAngle);
        SwingRate = FMath::Sqrt(FMath::Max(0.f, 2.f * SwingGravity / L * (FMath::Cos(SwingAngle) - FMath::Cos(FMath::DegreesToRadians(35.f)))));
        SwingBlendTime = .02f;
    }
    else
    {
        // From wherever his centre is: the arc's angle from there, a little of
        // his momentum along it; drawn onto the arc over the catch.
        const FVector Rel = GetActorLocation() - Grip.Grip;
        SwingAngle = FMath::Clamp(FMath::Atan2(static_cast<float>(FVector::DotProduct(Rel, SwingDir)), FMath::Max(5.f, static_cast<float>(-Rel.Z))), -.7f, .7f);
        SwingRate = FMath::Clamp(static_cast<float>(FVector::DotProduct(Velocity, SwingDir)) / L * .5f, -2.5f, 2.5f);
        SwingBlendTime = .18f;
    }
    SwingFrom = GetActorLocation();
    SwingYawFrom = GetActorRotation().Yaw;
    SwingGrip = Index; SwingTarget = -1; SwingClock = 0;
    Gait = EGait::Swing;
    LastWallNormal = FVector::ZeroVector; bWallJumpFlight = bRunJump = bSprintLeap = bSprintLeapPending = false; WallCoyoteUntil = -1;
    if (!bFromLeap)
    {
        bSwingJumpQueued = GetWorld()->GetTimeSeconds() - AirJumpPressedAt < .25f;
        if (bSwingJumpQueued) AirJumpPressedAt = -1e3f;
    }
    ++SwingCatches;
    SetClip(EClip::Swing, (FMath::Clamp(FMath::RadiansToDegrees(SwingAngle) / ChuckClipData::SwingMaxAngle, -1.f, 1.f) + 1.f) * .5f * Clips[static_cast<int32>(EClip::Swing)]->GetPlayLength(), bFromLeap ? .22f : .12f);   // (after a leap: time for the trailing paw to join the lead one on the bar)
    if (!bFromLeap) PlaySfx(LandSounds, ESfx::Land, LandVolume * .45f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SWING_CATCH grip=%d leap=%d catches=%d angle=%.1f at=%s"), Index, bFromLeap ? 1 : 0, SwingCatches, FMath::RadiansToDegrees(SwingAngle), *GetActorLocation().ToString());
}
int32 AChuckCharacter::FindSwingTarget(const FVector& Want) const
{
    // The nearest ring the way he wants to go, in reach and in plain sight.
    const TArray<FChuckSwingGrip>& Grips = GetChuckSwingGrips();
    if (!Grips.IsValidIndex(SwingGrip)) return -1;
    const FVector From = Grips[SwingGrip].Grip;
    const FVector Dir = Want.GetSafeNormal2D();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckSwingTarget), false, this);
    int32 Best = -1; float BestScore = 1e9f;
    for (int32 I = 0; I < Grips.Num(); ++I)
    {
        if (I == SwingGrip) continue;
        const FVector Delta = Grips[I].Grip - From;
        const float Reach = static_cast<float>(Delta.Size2D());
        if (Reach < 40.f || Reach > SwingLeapRange || Delta.Z > 70.f || Delta.Z < -130.f) continue;
        const float Along = static_cast<float>(FVector::DotProduct(Delta.GetSafeNormal2D(), Dir));
        if (Along < .55f) continue;
        const float Score = Reach * (1.6f - Along);
        if (Score >= BestScore) continue;
        FHitResult Block;
        const float Drop = ChuckClipData::SwingGripZ - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        if (GetWorld()->LineTraceSingleByChannel(Block, From, Grips[I].Grip, ECC_Visibility, Query)
            || GetWorld()->SweepSingleByChannel(Block, From - FVector(0, 0, Drop), Grips[I].Grip - FVector(0, 0, Drop), FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(GetCapsuleComponent()->GetScaledCapsuleRadius() - 1.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 1.f), Query)) continue;
        Best = I; BestScore = Score;
    }
    return Best;
}
void AChuckCharacter::SwingJump()
{
    // Toward the stick (straight on along the swing with it let go): the next
    // ring that way, flown to and caught for him; none, and he kicks off.
    const TArray<FChuckSwingGrip>& Grips = GetChuckSwingGrips();
    const FVector Stick = StickWorld();
    const bool bAimed = Stick.SizeSquared2D() > .12f;
    const FVector Want = bAimed ? Stick.GetSafeNormal2D() : SwingDir;
    int32 Target = FindSwingTarget(Want);
    // Let go of the stick at the end of a line of rings: back along it (only a
    // direction held toward no ring kicks him off).
    if (Target < 0 && !bAimed) Target = FindSwingTarget(-SwingDir);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SWING_JUMP grip=%d aimed=%d want=%s target=%d"), SwingGrip, bAimed ? 1 : 0, *Want.ToCompactString(), Target);
    if (Target < 0 || !Grips.IsValidIndex(SwingGrip))
    {
        LeaveSwing(Want * WallJumpOut * .9f + FVector(0, 0, WallJumpUp), true);
        return;
    }
    SwingLeapFrom = Grips[SwingGrip].Grip;
    SwingTarget = Target;
    const FVector Delta = Grips[Target].Grip - SwingLeapFrom;
    SwingLeapDir = Delta.GetSafeNormal2D();
    // A real flight's timing and arc (his gravity), a little quicker over short hops.
    SwingLeapTime = FMath::Clamp(static_cast<float>(Delta.Size()) / 320.f, .36f, .6f);
    SwingLeapLift = FMath::Min(30.f, SwingGravity * SwingLeapTime * SwingLeapTime / 8.f);
    SwingLeapFromAngle = SwingAngle;
    SwingLeapClock = 0;
    SwingYawFrom = GetActorRotation().Yaw;
    const FVector Along = Grips[Target].Along.GetSafeNormal2D();
    SwingLeapYaw = (Along * (FVector::DotProduct(SwingLeapDir, Along) >= 0.f ? 1.f : -1.f)).Rotation().Yaw;
    SwingLeft = SwingGrip; SwingGrip = -1;
    SwingRegrabAt = GetWorld()->GetTimeSeconds() + .4f;
    bSwingJumpQueued = false;
    Gait = EGait::SwingLeap;
    ++SwingLeapCount;
    SpendStamina(SwingLeapStaminaCost);
    // Reaching for it with the paw on its side (the rings alternate walls).
    const FVector Right = FRotationMatrix(FRotator(0, SwingLeapYaw, 0)).GetUnitAxis(EAxis::Y);
    SetClip(FVector::DotProduct(SwingLeapDir, Right) > 0.f ? EClip::SwingLeapRight : EClip::SwingLeapLeft, 0, .1f);
    PlaySfx(JumpSounds, ESfx::Jump, JumpVolume * .8f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SWING_LEAP from=%d to=%d leaps=%d dist=%.1f time=%.2f"), SwingLeft, Target, SwingLeapCount, Delta.Size(), SwingLeapTime);
}
void AChuckCharacter::LeaveSwing(const FVector& Velocity, bool bKick)
{
    // Let go (dodge) or kicked off with nowhere to swing to: in the air, the
    // ring out of reach for a moment; a kick runs a wall he reaches like a wall jump.
    auto* Movement = GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Falling);
    Movement->Velocity = Velocity;
    SwingLeft = SwingGrip; SwingGrip = -1;
    SwingRegrabAt = GetWorld()->GetTimeSeconds() + (bKick ? .5f : .7f);
    bSwingJumpQueued = false;
    LastWallNormal = FVector::ZeroVector; WallCoyoteUntil = -1;
    bWallJumpFlight = bKick;
    Gait = EGait::Air;
    if (bKick)
    {
        SetActorRotation(Velocity.GetSafeNormal2D().Rotation());
        ++SwingDismounts;
        PlaySfx(JumpSounds, ESfx::Jump, JumpVolume);
        SetClip(EClip::JumpLoop, 0, .12f);
    }
    else SetClip(EClip::JumpLoop, 0, .15f);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SWING_LEAVE grip=%d kick=%d at=%s"), SwingLeft, bKick ? 1 : 0, *GetActorLocation().ToString());
}
bool AChuckCharacter::ProbeSideWall(const FVector& From, const FVector& Side, float Reach, FVector& OutNormal, FVector& OutPoint) const
{
    // A wall out to Side: upright, or leaning over him (a tunnel's arch), or
    // sloping back a little; its horizontal normal faces him.
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckSideWall), false, this);
    if (!GetWorld()->SweepSingleByChannel(Hit, From, From + Side * Reach, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(4.f), Query)
        || Hit.bStartPenetrating || Hit.ImpactNormal.Z > .45f || Hit.ImpactNormal.Z < -.75f) return false;
    OutNormal = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0).GetSafeNormal();
    OutPoint = Hit.ImpactPoint;
    return FVector::DotProduct(OutNormal, -Side) > .7f;
}
bool AChuckCharacter::TryWallSideRun(bool bInAir)
{
    // Running with a wall beside him, or angled onto one by up to
    // WallSideAngle, that carries on ahead. Steeper than that (or a wall
    // squarely in front) is still the head-on climb.
    const FVector Location = GetActorLocation();
    const FVector Ahead = GetVelocity().GetSafeNormal2D();
    if (Ahead.IsNearlyZero() || GetVelocity().Size2D() < ChuckClipData::WalkSpeed * 1.5f) return false;
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float MaxInto = FMath::Sin(FMath::DegreesToRadians(WallSideAngle));
    const FVector Chest = Location + FVector(0, 0, 5);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckSideAhead), false, this);
    FHitResult Front;
    if (GetWorld()->SweepSingleByChannel(Front, Chest, Chest + Ahead * (Radius + 60.f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(6.f), Query)
        && FVector::DotProduct(FVector(Front.ImpactNormal.X, Front.ImpactNormal.Y, 0).GetSafeNormal(), -Ahead) > MaxInto) return false;
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Ahead);
    float Best = 1e6f; FVector Normal, Point;
    for (const float S : { 1.f, -1.f })
        for (const float Lead : { 0.f, .6f, 1.2f })   // straight out, then angled forward (a wall he is running onto)
        {
            const FVector Dir = (Right * S + Ahead * Lead).GetSafeNormal2D();
            FVector N, P, NAhead, PAhead;
            if (!ProbeSideWall(Chest, Dir, (Radius + WallSideReach) * FMath::Sqrt(1.f + Lead * Lead), N, P)) continue;
            if (FVector::DotProduct(N, Right * S) > -.3f) continue;               // on that side, facing him
            const float Into = static_cast<float>(-FVector::DotProduct(N, Ahead));
            if (Into > MaxInto || Into < -.35f) continue;                           // along or onto it, not at it, not away from it
            const float D = static_cast<float>(FVector::DotProduct(Location - P, N));
            if (D > Radius + WallSideReach) continue;                               // close enough, measured straight out from it
            const FVector Along = (Ahead - N * FVector::DotProduct(Ahead, N)).GetSafeNormal2D();
            if (!ProbeSideWall(Chest + Along * 120.f, -N, D + 40.f, NAhead, PAhead)) continue;   // and it carries on
            if (D < Best) { Best = D; Normal = N; Point = P; }
        }
    if (Best > 1e5f) return false;
    if (bInAir && !LastWallNormal.IsZero() && FVector::DotProduct(Normal, LastWallNormal) > .7f) return false;   // not the wall just left
    auto* Movement = GetCharacterMovement();
    WallNormal = Normal;
    WallSideAlong = (Ahead - Normal * FVector::DotProduct(Ahead, Normal)).GetSafeNormal2D();
    WallSideSpeed = FMath::Max(static_cast<float>(GetVelocity().Size2D()), ChuckClipData::RunSpeed) * 1.05f;
    WallSideClock = 0; WallSideTravel = 0; WallSideRise = 0;
    WallSideStartZ = static_cast<float>(Location.Z);
    // Close the remaining gap over the first frames (see the WallSide tick) rather than snapping in.
    WallSideGap = FMath::Max(0.f, Best - Radius - 1.f);
    Movement->SetMovementMode(MOVE_Flying);
    Movement->BrakingDecelerationFlying = 0;
    Movement->Velocity = WallSideAlong * WallSideSpeed + FVector(0, 0, WallSideUp) - WallNormal * (40.f + FMath::Min(WallSideGap * 12.f, 500.f));
    Gait = EGait::WallSide; bRunJump = bSprintLeap = false; bWallJumpFlight = false;
    LastWallNormal = FVector::ZeroVector;
    if (Base != EClip::WalkLoop) SetClip(EClip::WalkLoop, WalkPhase * Period(EClip::WalkLoop), .08f);
    RunWeight = 1.f;
    ++WallSideRuns;
    SpendStamina(WallSideStaminaCost);
    if (bInAir) ++WallSideAirCatches;
    else PlaySfx(JumpSounds, ESfx::Jump, JumpVolume);
    return true;
}
bool AChuckCharacter::TryMountLadder()
{
    // Taking hold needs no jump (user 2026-10-03): walk into a ladder's foot,
    // or, on the floor above, walk toward the drop at its top and he turns and
    // lowers himself onto it (the pull-up, backward).
    const FVector Stick = StickWorld();
    if (Stick.SizeSquared() < .16f || GetWorld()->GetTimeSeconds() < LadderCooldownUntil) return false;
    const FVector Location = GetActorLocation();
    const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Feet = static_cast<float>(Location.Z) - Half;
    const TArray<FChuckClimbable>& All = GetChuckClimbables();
    for (int32 I = 0; I < All.Num(); ++I)
    {
        const FChuckClimbable& C = All[I];
        const FVector Side = FVector::CrossProduct(FVector::UpVector, C.Out);
        const FVector Rel = Location - C.Foot;
        const float Front = static_cast<float>(FVector::DotProduct(Rel, C.Out));
        if (FMath::Abs(Feet - static_cast<float>(C.Foot.Z)) < 25.f && FMath::Abs(FVector::DotProduct(Rel, Side)) < C.HalfWidth + 10.f
            && Front > -10.f && Front < 45.f && FVector::DotProduct(Stick, -C.Out) > .6f)
        {
            EnterLadder(I); ++LadderMounts;
            return true;
        }
        const FVector Top = Location - C.Lip;
        const float Onto = static_cast<float>(FVector::DotProduct(Top, -C.Out));   // how far onto the floor above
        if (FMath::Abs(Feet - C.TopZ) < 20.f && FMath::Abs(FVector::DotProduct(Top, Side)) < C.HalfWidth + 10.f
            && Onto > 0.f && Onto < 55.f && FVector::DotProduct(Stick, C.Out) > .6f)
        {
            auto* Movement = GetCharacterMovement();
            Movement->SetMovementMode(MOVE_Flying);
            Movement->StopMovementImmediately();
            ClimbStart = FVector(C.Foot.X, C.Foot.Y, C.TopZ - ChuckClipData::HangDrop);
            ClimbDir = -C.Out;
            ClimbRise = static_cast<float>(C.Lip.Z - ClimbStart.Z) + Half + 1.f;
            ClimbAdvance = static_cast<float>(FVector::DotProduct(ClimbStart - C.Lip, C.Out)) + 18.5f;
            bClimbMantle = false; bClimbReverse = true; LadderIndex = I; ClimbFrom = Location;
            SetActorRotation((-C.Out).Rotation());
            Gait = EGait::Climb;
            const float Length = Clips[static_cast<int32>(EClip::PullUp)]->GetPlayLength();
            SetClip(EClip::PullUp, Length, .1f);
            BaseTime = Length;
            ++LadderMounts;
            return true;
        }
    }
    return false;
}
bool AChuckCharacter::TryVault()
{
    // Low enough to go over, thin enough to land beyond, floor on the far side
    // within reach and room to land and run on. Anything that carries on up
    // (a step onto a higher crate) or straight into something else is left to
    // the ordinary jump and its climbs.
    const FVector Location = GetActorLocation();
    const FVector Ahead = GetVelocity().GetSafeNormal2D();
    if (Ahead.IsNearlyZero() || GetVelocity().Size2D() < ChuckClipData::WalkSpeed * 1.6f) { VaultRefusal = 1; return false; }
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Feet = static_cast<float>(Location.Z) - Half;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckVault), false, this);
    UWorld* World = GetWorld();
    FHitResult Face;
    const FVector Shin(Location.X, Location.Y, Feet + 12.f);
    if (!World->SweepSingleByChannel(Face, Shin, Shin + Ahead * (Radius + VaultReach), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(5.f), Query)
        || Face.bStartPenetrating || FMath::Abs(Face.ImpactNormal.Z) > .3f) { VaultRefusal = 2; return false; }
    const FVector Normal = FVector(Face.ImpactNormal.X, Face.ImpactNormal.Y, 0).GetSafeNormal();
    if (FVector::DotProduct(Normal, -Ahead) < .6f) { VaultRefusal = 3; return false; }   // at it, not glancing along it
    const float FaceDist = static_cast<float>(FVector::DotProduct(Face.ImpactPoint - Location, Ahead));
    if (FaceDist < Radius + 8.f) { VaultRefusal = 4; return false; }   // already on top of it
    const auto Ground = [&](float Along, float From, float To, float& Z) -> bool
    {
        FHitResult Hit;
        const FVector P = Location + Ahead * Along;
        if (!World->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, Feet + From), FVector(P.X, P.Y, Feet + To), ECC_Visibility, Query)) return false;
        Z = static_cast<float>(Hit.ImpactPoint.Z) - Feet;
        return Hit.ImpactNormal.Z > .7f;
    };
    float Top = 0.f;
    if (!Ground(FaceDist + 6.f, VaultMaxHeight + 25.f, 2.f, Top) || Top < VaultMinHeight || Top > VaultMaxHeight) { VaultRefusal = 5; return false; }
    // Front to back: the top must end (the floor drops away again) within VaultMaxDepth.
    float Depth = -1.f;
    for (float D = 10.f; D <= VaultMaxDepth + 10.f; D += 8.f)
    {
        float Z = -1e4f;
        const bool bHit = Ground(FaceDist + D, Top + 30.f, -250.f, Z);
        if (!bHit || Z < Top - 12.f) { Depth = D - 4.f; break; }
        if (Z > Top + 10.f) { VaultRefusal = 6; return false; }   // it carries on up: a step, not a vault
    }
    if (Depth < 0.f) { VaultRefusal = 7; return false; }
    // Landing: floor beyond, not higher than a small step and no drop he'd vault off blind.
    const float Land = FaceDist + Depth + Radius + 25.f;
    float LandZ = 0.f;
    if (!Ground(Land, Top + 30.f, -150.f, LandZ) || LandZ > 15.f) { VaultRefusal = 8; return false; }
    // Room to come down and run on, and nothing to hit going over.
    FHitResult Block;
    const FVector LandAt = Location + Ahead * Land + FVector(0, 0, LandZ + 2.f);
    if (World->SweepSingleByChannel(Block, LandAt - Ahead * 18.f, LandAt + Ahead * 30.f, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(Radius, Half - 2.f), Query)) { VaultRefusal = 9; return false; }
    const FVector Over = FVector(0, 0, Top + 8.f);
    if (World->SweepSingleByChannel(Block, Location + Over, Location + Ahead * Land + Over, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(Radius - 2.f, Half - 2.f), Query)) { VaultRefusal = 10; return false; }
    VaultRefusal = 0;
    auto* Movement = GetCharacterMovement();
    VaultStart = Location; VaultDir = Ahead; VaultTotal = Land;
    VaultRise = Top + ChuckClipData::VaultClear;
    VaultLift = FMath::Max(0.f, LandZ);   // a little higher beyond: end on it; lower: drop off at the end
    VaultIn = FMath::Clamp((FaceDist - Radius) / Land, .1f, .45f);
    VaultOut = FMath::Clamp((FaceDist + Depth + Radius) / Land, VaultIn + .1f, .9f);
    VaultSpeed = FMath::Max(static_cast<float>(GetVelocity().Size2D()), ChuckClipData::RunSpeed);
    VaultTime = FMath::Clamp(Land / VaultSpeed, .32f, .6f);
    VaultClock = 0;
    Movement->SetMovementMode(MOVE_Flying);
    Movement->StopMovementImmediately();
    SetActorRotation(Ahead.Rotation());
    Gait = EGait::Vault; bRunJump = bSprintLeap = false;
    LayerTime = FadingLayerTime = -1; SlashHitAt = -1; bSlashQueued = false;
    SetClip(EClip::SpeedVault, 0, .06f);
    ++Vaults;
    PlaySfx(JumpSounds, ESfx::Jump, JumpVolume);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_VAULT top_cm=%.0f depth_cm=%.0f face_cm=%.0f land_cm=%.0f land_z=%.0f time=%.2f"), Top, Depth, FaceDist, Land, LandZ, VaultTime);
    return true;
}
void AChuckCharacter::EnterLadder(int32 Index)
{
    auto* Movement = GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Flying);
    Movement->StopMovementImmediately();
    LadderIndex = Index; LadderClock = 0; LadderFrom = GetActorLocation();
    Gait = EGait::Ladder;
    SetActorRotation((-GetChuckClimbables()[Index].Out).Rotation());
    SetClip(EClip::WallRun, FMath::Frac(LadderPhase) * Period(EClip::WallRun), .12f);
    LastWallNormal = FVector::ZeroVector; bWallJumpFlight = false; bRunJump = bSprintLeap = false; RunWeight = 0;
}
void AChuckCharacter::FallToDeath()
{
    // Off the edge of the world (user 2026-10-03): an Astral death like any
    // other, only there's nothing to see where he fell, so it goes dark at
    // once; then he's summoned back at this area's start.
    if (IsAstral()) return;
    auto* Movement = GetCharacterMovement();
    bPendingVanish = false;
    Movement->StopMovementImmediately();
    Movement->DisableMovement();
    LayerTime = FadingLayerTime = -1; SlashHitAt = -1; bSlashQueued = bSlashHeld = false; RunWeight = 0;
    RestoreSlideCamera(); LadderIndex = -1; bClimbReverse = false;
    Gait = EGait::Astral; AstralPhase = EAstral::Vanishing; AstralClock = .8f; bAstralFaded = true;
    SetAstralHidden(true);
    CameraFade(0.f, 1.f, .35f);
    ++FallDeaths;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FALL_DEATH at=%s return=%s"), *GetActorLocation().ToString(), *GetAreaStartLocation().ToString());
}
void AChuckCharacter::LeaveWallSide()
{
    // Off the end of the arc: carry on falling the way he was going, a touch out from the wall.
    auto* Movement = GetCharacterMovement();
    const FVector V = Movement->Velocity;
    Movement->SetMovementMode(MOVE_Falling);
    Movement->Velocity = FVector(V.X, V.Y, FMath::Min(static_cast<float>(V.Z), 0.f)) * FVector(.92f, .92f, 1.f) + WallNormal * 50.f;
    LastWallNormal = WallNormal;
    WallCoyoteUntil = GetWorld()->GetTimeSeconds() + WallCoyote;
    Gait = EGait::Air; bRunJump = bSprintLeap = false;
    SetClip(EClip::JumpLoop, 0, .15f);
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
    bWallJumpFlight = true; bRunJump = bSprintLeap = false;
    Gait = EGait::Air;
    ++WallJumps;
    SpendStamina(WallJumpStaminaCost);
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
    if (IsDodging() || IsAstral() || IsTalking() || Gait == EGait::Hang || Gait == EGait::Climb || Gait == EGait::WallRun || Gait == EGait::Ladder || Gait == EGait::Vault) return;
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
    // People: a scratch at the shins of anyone in reach. Nothing breaks; they react.
    for (const TWeakObjectPtr<ADockNPC>& Entry : ADockNPC::All())
    {
        ADockNPC* NPC = Entry.Get();
        if (!NPC || NPC->IsBobert()) continue;   // the sleeper in the barrel is never interacted with (GAME-BIBLE.md)
        const FVector To = NPC->GetActorLocation() - Location;
        if (static_cast<float>(To.Size2D()) - 24.f > SlashReach) continue;
        if (FVector::DotProduct(To.GetSafeNormal2D(), Ahead) < .3f) continue;
        NPC->TakeScratch(Location);
        ++SlashNPCHits;
    }
}
bool AChuckCharacter::TakeBite(const FVector& From, int32 Amount)
{
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < BiteImmuneUntil || IsAstral() || IsDodging() || Gait == EGait::Hang || Gait == EGait::Climb || Gait == EGait::WallRun) return false;
    BiteImmuneUntil = Now + BiteImmunity;
    ++BitesTaken;
    TalkingTo.Reset();
    Sanity = FMath::Max(0, Sanity - Amount);
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
ADockNPC* AChuckCharacter::GetTalkPrompt() const
{
    if (IsTalking() || IsAstral() || GetCharacterMovement()->IsFalling()) return nullptr;
    const FVector Location = GetActorLocation();
    const FVector Ahead = GetActorForwardVector().GetSafeNormal2D();
    ADockNPC* Best = nullptr; float BestDistance = ADockNPC::TalkRadius;
    for (const TWeakObjectPtr<ADockNPC>& Entry : ADockNPC::All())
    {
        ADockNPC* NPC = Entry.Get();
        if (!NPC || !NPC->CanTalk()) continue;
        const FVector To = NPC->GetActorLocation() - Location;
        if(FMath::Abs(To.Z)>100.f) continue; // no talking through floors or ceilings
        const float Distance = static_cast<float>(To.Size2D());
        // In reach and roughly in front (or right up against him).
        if (Distance < BestDistance && (FVector::DotProduct(To.GetSafeNormal2D(), Ahead) > .2f || Distance < 45.f)) { Best = NPC; BestDistance = Distance; }
    }
    return Best;
}
bool AChuckCharacter::GetDialogue(FString& Speaker, FString& Text) const
{
    const ADockNPC* NPC = TalkingTo.Get();
    if (!NPC || !NPC->Lines.IsValidIndex(TalkLine)) return false;
    Speaker = NPC->DisplayName; Text = NPC->GetTalkSubtitle(TalkLine);
    return true;
}
void AChuckCharacter::Interact()
{
    if (IsTalking())
    {
        // Next line, or the conversation's over.
        if (++TalkLine >= TalkingTo->Lines.Num()) { TalkingTo.Reset(); TalkLine = 0; }
        return;
    }
    if (ADockNPC* NPC = GetTalkPrompt())
    {
        TalkingTo = NPC; TalkLine = 0;
        GetCharacterMovement()->StopMovementImmediately();
        // Chuck turns to face whoever he's listening to.
        SetActorRotation(FRotator(0, (NPC->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0));
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
    static const TCHAR* Names[] = {TEXT("None"), TEXT("Vanishing"), TEXT("Away"), TEXT("Summoning"), TEXT("SlideDown"), TEXT("SlideAway")};
    return Names[static_cast<int32>(AstralPhase)];
}
void AChuckCharacter::SetAstralHidden(bool bHide)
{
    bAstralHidden = bHide;
    GetMesh()->SetVisibility(!bHide, true);
}
void AChuckCharacter::CameraFade(float From, float To, float Seconds, const FLinearColor& Colour)
{
    // By default the Astral Sea's colour: a deep, quiet indigo.
    if (auto* PC = Cast<APlayerController>(Controller))
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(From, To, Seconds, Colour, false, true);
}
void AChuckCharacter::BeginSlide(float Into)
{
    // Into the water slide at the end of the sewer: carried down it, out of
    // his control, until the view goes dark; then out at the pier.
    auto* Movement = GetCharacterMovement();
    SlideSpeed = FMath::Max(260.f, static_cast<float>(GetVelocity().Size2D()));
    SlideS = Into; SlideFrom = GetActorLocation();
    Movement->StopMovementImmediately();
    Movement->DisableMovement();
    Gait = EGait::Astral; AstralPhase = EAstral::SlideDown; AstralClock = 0; bAstralFaded = false;
    LayerTime = FadingLayerTime = -1; SlashHitAt = -1; bSlashQueued = bSlashHeld = false; RunWeight = 0;
    TalkingTo.Reset(); TalkLine = 0;
    SetClip(EClip::JumpLoop, 0, .15f);
    // The view stays at the mouth and watches him go.
    SavedCameraRelative = Camera->GetRelativeLocation(); SavedCameraRotation = Camera->GetRelativeRotation();
    SlideCameraAt = Camera->GetComponentLocation();
    bSlideCamera = true;
    ++Slides;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_SLIDE_BEGIN into=%.0f speed=%.0f"), Into, SlideSpeed);
}
void AChuckCharacter::HoldSlideCamera()
{
    Camera->SetWorldLocationAndRotation(SlideCameraAt, (GetActorLocation() - SlideCameraAt).Rotation());
}
void AChuckCharacter::RestoreSlideCamera()
{
    if (!bSlideCamera) return;
    bSlideCamera = false;
    Camera->SetRelativeLocationAndRotation(SavedCameraRelative, SavedCameraRotation);
}
bool AChuckCharacter::FindPierExit()
{
    // The pier end's outer face, probed from the water, and its top edge
    // measured from the hanging height there.
    FHitResult Face;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckPierExit), false, this);
    const FVector Probe = DockPierExitProbe();
    if (!GetWorld()->LineTraceSingleByChannel(Face, Probe, Probe - FVector(300, 0, 0), ECC_Visibility, Query) || FMath::Abs(Face.ImpactNormal.Z) > .3f) return false;
    const FVector Normal = FVector(Face.ImpactNormal.X, Face.ImpactNormal.Y, 0).GetSafeNormal();
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    SetActorLocation(Face.ImpactPoint + Normal * (Radius + .5f) + FVector(0, 0, -Probe.Z - ChuckClipData::HangDrop), false, nullptr, ETeleportType::TeleportPhysics);
    FVector Edge; bool bRoom = false;
    if (!FindLedge(Normal, Face.ImpactPoint, ChuckClipData::HangDrop - 30.f, ChuckClipData::HangDrop + 30.f, Edge, bRoom) || !bRoom) return false;
    ExitNormal = Normal; ExitEdge = Edge;
    SetActorLocation(Edge + Normal * (Radius + .5f) - FVector(0, 0, ChuckClipData::HangDrop), false, nullptr, ETeleportType::TeleportPhysics);
    return true;
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
            // Return to this area's entrance, whole again.
            AstralPhase = EAstral::Away; AstralClock = 0;
            const FVector Return=GetAreaStartLocation();
            ViewYaw = AreaStartYaw(Return);
            SetActorLocationAndRotation(Return, FRotator(0,ViewYaw,0), false, nullptr, ETeleportType::TeleportPhysics);
            LookPitch = SmoothLook = FMath::Min(LookPitch, -5.f); bFollowReady = false;
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
    case EAstral::SlideDown:
    {
        // Gathering speed down the slide; the view fades to black.
        SlideSpeed = FMath::Min(SlideSpeed + 700.f * DeltaSeconds, 950.f);
        SlideS = FMath::Min(SlideS + SlideSpeed * DeltaSeconds, DockSewerSlideLength());
        const FVector On = DockSewerSlidePoint(SlideS) + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 4.f);
        SetActorLocation(FMath::Lerp(SlideFrom, On, FMath::SmoothStep(0.f, .25f, AstralClock)), false, nullptr, ETeleportType::TeleportPhysics);
        SetActorRotation(FRotator(0, DockSewerSlideDirection(SlideS).Rotation().Yaw, 0));
        if (AstralClock >= .5f && !bAstralFaded) { CameraFade(0.f, 1.f, .45f, FLinearColor::Black); bAstralFaded = true; }
        if (AstralClock >= 1.05f)
        {
            // In the dark: come up at the end of the court pier.
            AstralPhase = EAstral::SlideAway; AstralClock = 0;
            RestoreSlideCamera();
            bExitFound = FindPierExit();
            if (!bExitFound) SetActorLocation(DockPierExitProbe() + FVector(-170, 0, 55), false, nullptr, ETeleportType::TeleportPhysics);
            ViewYaw = bExitFound ? (-ExitNormal).Rotation().Yaw : 180.f;
            SetActorRotation(FRotator(0, ViewYaw, 0));
            LookPitch = SmoothLook = FMath::Min(LookPitch, -5.f); bFollowReady = false;
            PreviousMotionLocation = GetActorLocation();
            bSewerRespawn = false;
            MarkDockSewerExited();
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SLIDE_EXIT found=%d edge=%s"), bExitFound, *ExitEdge.ToString());
        }
        break;
    }
    case EAstral::SlideAway:
        if (AstralClock >= .35f)
        {
            // Hanging off the pier's end, he pulls himself out as the view returns.
            AstralPhase = EAstral::None;
            CameraFade(1.f, 0.f, .8f, FLinearColor::Black);
            if (bExitFound) { EnterHang(ExitNormal, ExitEdge, true); bAutoClimb = true; }
            else { Gait = EGait::Idle; SetClip(EClip::Idle, 0, .25f); GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
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
    bBodyPitchLive = false;
    const bool bStrafe = StrafeHeld();
    if (!OwnsCapsule()) Movement->MaxWalkSpeed = IsSprinting() ? SprintSpeed : bStrafe ? (bRunHeld ? StrafeRunSpeed : StrafeSpeed) : (bRunHeld ? RunSpeed : WalkSpeed);
    Movement->MaxAcceleration = IsSprinting() ? SprintAcceleration : 550.f;
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
    const bool bGroundGait = !bAirborne && (Gait == EGait::Idle || Gait == EGait::Start || Gait == EGait::Loop || Gait == EGait::Stop || Gait == EGait::Strafe || Gait == EGait::Turn || Gait == EGait::Land);
    if (bGroundGait && TryMountLadder()) {}
    else if (bInput && !bAirborne && (Gait == EGait::Idle || Gait == EGait::Start || Gait == EGait::Loop || Gait == EGait::Stop || Gait == EGait::Strafe)) TryMantle();
    if (bPendingVanish && GetWorld()->GetTimeSeconds() >= PendingVanishAt && (Movement->IsMovingOnGround() || GetWorld()->GetTimeSeconds() >= PendingVanishAt + .8f)) BeginVanish();
    // The water slide at the sewer's end takes him once he's in its mouth.
    if (!IsAstral() && Gait != EGait::Hang && Gait != EGait::Climb && GetActorLocation().Z < -700.f)
    {
        const float Into = DockSewerSlideEntry(GetActorLocation());
        if (Into > 35.f) BeginSlide(Into);
    }
    if (Gait == EGait::Astral) UpdateAstral(DeltaSeconds);
    else if (Gait == EGait::Vault)
    {
        // Forward evenly, up onto a plateau clear of the top while he's over
        // it, down beyond; the clip runs with the progress. Then he runs on
        // (or drops on down, if the far side is lower).
        VaultClock += DeltaSeconds;
        const float U = FMath::Min(1.f, VaultClock / VaultTime);
        BaseTime = U * Clips[static_cast<int32>(EClip::SpeedVault)]->GetPlayLength();
        const float Plateau = FMath::SmoothStep(0.f, VaultIn, U) * (1.f - FMath::SmoothStep(VaultOut, 1.f, U));
        const float Up = FMath::Max(VaultRise * Plateau, VaultLift * FMath::SmoothStep(VaultIn, 1.f, U));
        SetActorLocation(VaultStart + VaultDir * (VaultTotal * U) + FVector(0, 0, Up), false, nullptr, ETeleportType::TeleportPhysics);
        Movement->Velocity = VaultDir * VaultSpeed;
        if (U >= 1.f)
        {
            Movement->SetMovementMode(MOVE_Falling);
            Movement->Velocity = VaultDir * VaultSpeed;
            Gait = EGait::Air; bRunJump = true;
            PreviousMotionLocation = GetActorLocation();
        }
    }
    else if (Gait == EGait::Ladder && GetChuckClimbables().IsValidIndex(LadderIndex))
    {
        // Up toward it, down away from it, the climb clip stepping with the
        // height covered; out over the top with the pull-up, off at the foot.
        const FChuckClimbable& C = GetChuckClimbables()[LadderIndex];
        const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const float Bottom = static_cast<float>(C.Foot.Z) + Half + 1.f, Top = C.TopZ - ChuckClipData::HangDrop;
        LadderClock += DeltaSeconds;
        const float Want = static_cast<float>(FVector::DotProduct(StickWorld(), -C.Out));
        const float Climb = FMath::Abs(Want) > .3f ? FMath::Clamp(Want * 1.3f, -1.f, 1.f) : 0.f;
        const float Z = FMath::Clamp(static_cast<float>(Location.Z) + Climb * LadderSpeed * DeltaSeconds, Bottom, Top);
        const float Settle = FMath::SmoothStep(0.f, .15f, LadderClock);
        const FVector At(FMath::Lerp(LadderFrom.X, C.Foot.X, Settle), FMath::Lerp(LadderFrom.Y, C.Foot.Y, Settle), Z);
        LadderPhase += (Z - static_cast<float>(Location.Z)) / LadderStride;
        BaseTime = FMath::Frac(LadderPhase) * Period(EClip::WallRun);
        SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
        Movement->Velocity = FVector::ZeroVector;
        if (Climb > 0.f && Z >= Top - .5f) { LadderCooldownUntil = GetWorld()->GetTimeSeconds() + .6f; StartClimb(false, C.Out, C.Lip); }
        else if (Climb < 0.f && Z <= Bottom + .5f)
        {
            Movement->SetMovementMode(MOVE_Walking);
            Gait = EGait::Idle; LadderIndex = -1;
            LadderCooldownUntil = GetWorld()->GetTimeSeconds() + .6f;
            SetClip(EClip::Idle, 0, .2f);
            PreviousMotionLocation = GetActorLocation();
        }
    }
    else if (Gait == EGait::Swing && GetChuckSwingGrips().IsValidIndex(SwingGrip))
    {
        // A pendulum from the ring (his gravity, the clip's arm length), drawn
        // onto from wherever he caught it; the stick along it pumps the swing
        // and it settles if left alone. The clip is posed by the angle; the
        // whole body pitches about the grip with it.
        SwingClock += DeltaSeconds;
        const FVector Grip = GetChuckSwingGrips()[SwingGrip].Grip;
        const float L = SwingLength();
        const float Push = static_cast<float>(FVector::DotProduct(StickWorld(), SwingDir));
        float Accel = -SwingGravity / L * FMath::Sin(SwingAngle) - .9f * SwingRate;
        if (FMath::Abs(Push) > .3f && Push * SwingRate > 0.f && FMath::Abs(SwingAngle) < FMath::DegreesToRadians(50.f)) Accel += Push * 9.f;
        SwingRate += Accel * DeltaSeconds;
        SwingAngle = FMath::Clamp(SwingAngle + SwingRate * DeltaSeconds, -1.f, 1.f);
        const float Turn = FMath::SmoothStep(0.f, .2f, SwingClock);
        SetActorRotation(FRotator(0, SwingYawFrom + FMath::FindDeltaAngleDegrees(SwingYawFrom, SwingDir.Rotation().Yaw) * Turn, 0));
        const float Body = SwingRestTilt() + FMath::RadiansToDegrees(SwingAngle);
        SetActorLocation(FMath::Lerp(SwingFrom, SwingCentre(Grip, Body), FMath::SmoothStep(0.f, SwingBlendTime, SwingClock)), false, nullptr, ETeleportType::TeleportPhysics);
        Movement->Velocity = FVector::ZeroVector;
        BodyPitch = Body; BodyPitchAxis = FVector::CrossProduct(GetActorForwardVector().GetSafeNormal2D(), FVector::UpVector).GetSafeNormal(); bBodyPitchLive = true;
        BaseTime = (FMath::Clamp(FMath::RadiansToDegrees(SwingAngle) / ChuckClipData::SwingMaxAngle, -1.f, 1.f) + 1.f) * .5f * Length;
        if (bSwingJumpQueued && SwingClock >= .1f) { bSwingJumpQueued = false; SwingJump(); }
    }
    else if (Gait == EGait::SwingLeap && GetChuckSwingGrips().IsValidIndex(SwingTarget))
    {
        // Ring to ring: his paws follow a real flight's arc from one to the
        // other; the body swings from its release through to hanging behind
        // the next ring (the clip's own release and catch angles), turning onto
        // the new ring's line, and the catch carries on into the swing.
        SwingLeapClock += DeltaSeconds;
        const float U = FMath::Min(1.f, SwingLeapClock / SwingLeapTime);
        const FVector To = GetChuckSwingGrips()[SwingTarget].Grip;
        const FVector Paws = FMath::Lerp(SwingLeapFrom, To, U) + FVector(0, 0, SwingLeapLift * 4.f * U * (1.f - U));
        const float Pose = ChuckClipData::SwingRelease + (ChuckClipData::SwingCatch - ChuckClipData::SwingRelease) * FMath::SmoothStep(.2f, 1.f, U);
        const float Swing = FMath::Lerp(FMath::RadiansToDegrees(SwingLeapFromAngle), Pose * ChuckClipData::SwingMaxAngle, FMath::SmoothStep(0.f, .25f, U));
        SetActorRotation(FRotator(0, SwingYawFrom + FMath::FindDeltaAngleDegrees(SwingYawFrom, SwingLeapYaw) * FMath::SmoothStep(0.f, .6f, U), 0));
        const float Body = SwingRestTilt() + Swing;
        SetActorLocation(SwingCentre(Paws, Body), false, nullptr, ETeleportType::TeleportPhysics);
        Movement->Velocity = FVector::ZeroVector;
        BodyPitch = Body; BodyPitchAxis = FVector::CrossProduct(GetActorForwardVector().GetSafeNormal2D(), FVector::UpVector).GetSafeNormal(); bBodyPitchLive = true;
        BaseTime = U * Length;
        if (U >= 1.f) EnterSwing(SwingTarget, true);
    }
    else if (Gait == EGait::Swing || Gait == EGait::SwingLeap) LeaveSwing(FVector::ZeroVector, false);   // (the grips went away: a world reset)
    else if (Gait == EGait::WallSide)
    {
        // Along the wall on a low arc, following its surface (re-found every
        // frame, so a curving tunnel wall carries him round); the stride runs
        // with the ground covered. Off when the arc's done, the wall ends,
        // something's in the way or the floor comes up under him.
        WallSideClock += DeltaSeconds;
        const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
        FVector N, P;
        bool bWall = false;
        for (const float Height : { 5.f, -15.f, 25.f })
            if (ProbeSideWall(Location + FVector(0, 0, Height), -WallNormal, Radius + 25.f + WallSideGap, N, P)) { bWall = true; break; }
        if (bWall)
        {
            WallNormal = N;
            WallSideGap = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(Location - P, N)) - Radius - 1.f);
        }
        WallSideAlong = (WallSideAlong - WallNormal * FVector::DotProduct(WallSideAlong, WallNormal)).GetSafeNormal2D();
        const bool bLocked = IsStaminaLocked();
        const float Up = WallSideUp - (bLocked ? LockedWallSideGravity : WallSideGravity) * WallSideClock;
        const float Moved = Travel;
        WallSideTravel += Moved;
        WallSideRise = FMath::Max(WallSideRise, static_cast<float>(Location.Z) - WallSideStartZ);
        WalkPhase = FMath::Frac(WalkPhase + Moved / RunStride);
        BaseTime = WalkPhase * WalkPeriod;
        RunWeight = 1.f;
        // Turned along the wall over a few frames (an angled approach turns him up to WallSideAngle).
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), WallSideAlong.Rotation(), DeltaSeconds, 16.f));
        FHitResult Floor;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckSideFloor), false, this);
        const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const bool bFloor = Up < 0.f && GetWorld()->LineTraceSingleByChannel(Floor, Location, Location - FVector(0, 0, Half + 4.f), ECC_Visibility, Query);
        const bool bStalled = WallSideClock > .15f && Moved < WallSideSpeed * DeltaSeconds * .3f;
        if (!bWall || WallSideClock >= (bLocked ? LockedWallSideTime : WallSideTime) || bFloor || bStalled) LeaveWallSide();
        else Movement->Velocity = WallSideAlong * WallSideSpeed + FVector(0, 0, Up) - WallNormal * (40.f + FMath::Min(WallSideGap * 12.f, 500.f));
    }
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
        if (TrySwingCatch()) {}   // a lantern's ring beside him on the way up
        else if (!bLetGo && FindLedge(WallNormal, Face, 5.f, 45.f, Edge, bRoom)) EnterHang(WallNormal, Edge, bRoom);
        else if (!bLetGo && TryGrabEdge(-WallNormal)) {}   // an eave over his head
        else if ((WallRunClock >= WallRunTime && !IsStaminaLocked()) || !bWall || bLetGo) LeaveWall();
        else
        {
            // Locked stamina: past the three steps he keeps climbing, steadily.
            float Up = 2.f * WallRunRise / WallRunTime * FMath::Max(0.f, 1.f - WallRunClock / WallRunTime);
            if (IsStaminaLocked()) Up = FMath::Max(Up, LockedWallRunSpeed);
            Movement->Velocity = FVector(0, 0, Up) - WallNormal * 30.f;
        }
    }
    else if (Gait == EGait::Hang)
    {
        // Snap in over 0.12 s, then hold. Toward the wall (held PullUpHold)
        // climbs up; away lets go.
        HangClock += DeltaSeconds;
        BaseTime += DeltaSeconds;
        const FVector Hold = HangHoldAt(HangEdge);
        const float Snap = FMath::SmoothStep(0.f, HangSnapTime, HangClock);
        SetActorLocation(FMath::Lerp(HangFrom, Hold, Snap), false, nullptr, ETeleportType::TeleportPhysics);
        const float WallYaw = (-HangNormal).Rotation().Yaw;
        SetActorRotation(FRotator(0, HangYawFrom + FMath::FindDeltaAngleDegrees(HangYawFrom, WallYaw) * Snap, 0));
        Movement->Velocity = FVector::ZeroVector;
        const FVector2D Raw(SideInput(), InputForward);
        if (bHangNeedsRelease && Raw.SizeSquared() < .04f) bHangNeedsRelease = false;
        if (bCornerCarry && (Raw.SizeSquared() < .04f || FVector2D::DotProduct(Raw.GetSafeNormal(), CornerCarryStick) < .7f)) bCornerCarry = false;
        const FVector Along = FRotationMatrix((-HangNormal).Rotation()).GetUnitAxis(EAxis::Y);  // his right, along the wall
        const float Toward = (bCornerCarry || bHangNeedsRelease || bAutoClimb) ? 0.f : FVector::DotProduct(StickWorld(), -HangNormal);
        const float Side = (bHangNeedsRelease || bAutoClimb) ? 0.f : bCornerCarry ? CornerCarrySide * FMath::Min(1.f, Raw.Size()) : FVector::DotProduct(StickWorld(), Along);
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
            const FVector From = HangHoldAt(HangEdge);
            const FVector To = HangHoldAt(Edge);
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
        // Up: the stick anywhere toward the wall (within about 70 degrees), held briefly.
        HangHold = Toward > .35f ? HangHold + DeltaSeconds : 0.f;
        if (bAutoClimb && HangClock >= .7f) { bAutoClimb = false; StartClimb(false, HangNormal, HangEdge); }   // out of the water at the pier
        else if (bHangClimbQueued && HangClock >= HangSnapTime) { bHangClimbQueued = false; StartClimb(false, HangNormal, HangEdge); }
        else if (HangHold >= PullUpHold && bHangRoom && HangClock >= HangSnapTime) StartClimb(false, HangNormal, HangEdge);
        else if (Toward < -.5f && HangClock > .15f) DropFromHang();
    }
    else if (Gait == EGait::Climb)
    {
        BaseTime = bClimbReverse ? FMath::Max(0.f, BaseTime - DeltaSeconds) : BaseTime + DeltaSeconds;
        const FVector2D Path = bClimbMantle ? PathAt(MantlePath, MantleFrames, BaseTime) : PathAt(PullUpPath, PullUpFrames, BaseTime);
        const float Forward = Path.X * ClimbAdvance / (bClimbMantle ? MantleAdvance : PullUpAdvance);
        const float Up = Path.Y * ClimbRise / (bClimbMantle ? MantleRefStep : PullUpRise);
        FVector Along = ClimbStart + ClimbDir * Forward + FVector(0, 0, Up);
        if (bClimbReverse) Along = FMath::Lerp(ClimbFrom, Along, FMath::SmoothStep(0.f, .2f, Length - BaseTime));   // from where he stood at the edge
        SetActorLocation(Along, false, nullptr, ETeleportType::TeleportPhysics);
        Movement->Velocity = FVector::ZeroVector;
        if (bClimbReverse && BaseTime <= 0.f)
        {
            bClimbReverse = false;
            if (GetChuckClimbables().IsValidIndex(LadderIndex)) EnterLadder(LadderIndex);
        }
        else if (!bClimbReverse && BaseTime >= Length)
        {
            // Never left inside anything (a roof, an attic): nudge out to the nearest clear spot.
            FVector Spot = GetActorLocation(); const FRotator Facing = GetActorRotation();
            if (GetWorld()->EncroachingBlockingGeometry(this, Spot, Facing) && GetWorld()->FindTeleportSpot(this, Spot, Facing))
            {
                UE_LOG(LogTemp, Display, TEXT("CHUCK_CLIMB_UNSTUCK from=%s to=%s"), *GetActorLocation().ToString(), *Spot.ToString());
                SetActorLocation(Spot, false, nullptr, ETeleportType::TeleportPhysics);
            }
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
        const bool bWalkedOff = GetVelocity().Z < 10.f && RunWeight < .5f && Speed > 10.f && Speed < WalkSpeed * 1.2f   // walking, not standing (dropped in)
            && (Gait == EGait::Idle || Gait == EGait::Start || Gait == EGait::Loop || Gait == EGait::Stop || Gait == EGait::Strafe);
        if (Gait != EGait::Air && bWalkedOff && TryDropHang()) {}
        else if (Gait != EGait::Air)
        {
            Gait = EGait::Air;
            // A jump at a sprint is the long leap; at a run (not a fall off an
            // edge) a leap with a little more lift.
            bSprintLeap = bSprintLeapPending && IsSprinting() && GetVelocity().Z > 50.f;
            bSprintLeapPending = false;
            bRunJump = !bSprintLeap && RunWeight > .5f && GetVelocity().Z > 50.f;
            RunTakeoffAt = bRunJump ? GetWorld()->GetTimeSeconds() : -1.f;
            if (GetVelocity().Z > 50.f) PlaySfx(JumpSounds, ESfx::Jump, JumpVolume * (bRunJump || bSprintLeap ? 1.f : .85f));
            if (bSprintLeap) StartSprintLeap();
            else if (bRunJump)
            {
                Movement->Velocity.Z = RunJumpVerticalSpeed;
                SetClip(EClip::RunJump, 0, .08f);
            }
            // Stepping down at a sprint: the gallop carries on over it.
            else if (IsSprinting()) {}
            // Takeoff is runtime-driven, so skip the clip's ground crouch and
            // start at its extension onto the toes.
            else SetClip(EClip::JumpStart, Clips[static_cast<int32>(EClip::JumpStart)]->GetPlayLength() * .5f, .06f);
        }
        // A running jump that reaches a wall along or angled onto it catches it and runs it.
        if (Gait == EGait::Air && bRunJump && GetWorld()->GetTimeSeconds() - RunTakeoffAt < WallSideAirTime
            && GetVelocity().Z > -150.f && TryWallSideRun(true)) {}
        else if (Gait == EGait::Hang || TrySwingCatch() || TryEnterWallRun()) {}  // (a drop-hang just caught the edge)
        else if (bSprintLeap)
        {
            const float Progress = FMath::Clamp((SprintLeapVerticalSpeed - static_cast<float>(GetVelocity().Z)) / (2.f * SprintLeapVerticalSpeed), 0.f, 1.f);
            BaseTime = FMath::Max(BaseTime, Progress * Clips[static_cast<int32>(EClip::SprintLeap)]->GetPlayLength());
        }
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
        // The leap lands on the forepaws, at that point of the gallop (or of
        // the run, if the sprint ran out in the air).
        else if (bSprintLeap && bStickHeld && Speed > WalkSpeed)
        {
            Gait = EGait::Loop; WalkPhase = SprintForeLand; SetClip(EClip::WalkLoop, WalkPhase * WalkPeriod, .06f); RunWeight = 1.f;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_SPRINT_LEAP land=%d at=%s sprinting=%d"), SprintLeaps, *Location.ToString(), IsSprinting() ? 1 : 0);
        }
        // Down a step at a sprint: on in stride.
        else if (IsSprinting() && Base == EClip::WalkLoop && bStickHeld) { Gait = EGait::Loop; RunWeight = 1.f; }
        else if (bRunJump && bStickHeld && Speed > WalkSpeed) { Gait = EGait::Loop; WalkPhase = 0; SetClip(EClip::WalkLoop, 0, .06f); RunWeight = RunBlendAt(Speed); }
        else { Gait = EGait::Land; SetClip(EClip::JumpLand, 0, .06f); bHardLanding = bRunJump || bSprintLeap; }
        bRunJump = bSprintLeap = false;
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
            const float Stride = LoopStride(Speed, SprintWeight);
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
            WalkPhase = FMath::Frac(WalkPhase + Travel / LoopStride(Speed, SprintWeight));
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
    // The sprint runs out, or ends with anything but the plain stride.
    if (IsStaminaLocked()) Stamina = 1.f;   // (the golden leaf: full whatever he does)
    if (IsSprinting())
    {
        const bool bLeaping = Gait == EGait::Air && bSprintLeap;
        if (!bLeaping) Stamina = FMath::Max(0.f, Stamina - DeltaSeconds / SprintDuration);   // the leap is free
        // The run-up: straight sprinting on the ground. Off the ground it starts
        // over on landing (what was run up stays for a leap off a small step).
        if (Movement->IsFalling()) RunupDir = FVector::ZeroVector;
        else
        {
            const FVector Dir = GetVelocity().GetSafeNormal2D();
            if (!Dir.IsNearlyZero())
            {
                if (RunupDir.IsNearlyZero() || FVector::DotProduct(Dir, RunupDir) < FMath::Cos(FMath::DegreesToRadians(25.f))) { RunupFrom = GetActorLocation(); RunupDir = Dir; }
                RunupLength = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(GetActorLocation() - RunupFrom, RunupDir)));
            }
        }
        SprintAirTime = Movement->IsFalling() && !bLeaping ? SprintAirTime + DeltaSeconds : 0.f;
        if (Stamina <= 0.f) EndSprint(false);
        else if (bLeaping) {}   // the leap carries the sprint
        else if (Movement->IsFalling()) { if (SprintAirTime > SprintDropGrace) EndSprint(true); }
        else if (Gait != EGait::Loop || bStrafe || !bInput || IsTalking()) EndSprint(false);   // let go of the stick: over
    }
    // Not sprinting, whatever he's doing but climbing or swinging: stamina refills.
    else if (Stamina < 1.f && !IsClimbing())
    {
        Stamina = FMath::Min(1.f, Stamina + DeltaSeconds / SprintCooldown);
        if (Stamina >= 1.f) StaminaFullAt = GetWorld()->GetTimeSeconds();
    }
    // A real fall once the sprint is over: out of the frozen gallop into the jump pose.
    if (!IsSprinting() && Gait == EGait::Air && !bSprintLeap && !bRunJump && Base == EClip::WalkLoop) SetClip(EClip::JumpLoop, 0, .15f);
    // Coming to a stop ends the run latch.
    if (Gait == EGait::Idle && GaitBefore != EGait::Idle) bRunHeld = false;
    // During a dodge nothing brakes the capsule but the dodge itself.
    // A landing without input absorbs its momentum within a few cm, before the
    // landing paws lock (0.1 s), instead of walking on into a stop.
    // Out of a sprint he sheds back down to the run within about 0.15 s.
    Movement->BrakingDecelerationWalking = OwnsCapsule() ? 0.f
        : (!IsSprinting() && Gait == EGait::Loop && Speed > RunSpeed * 1.02f) ? RunBrake
        : bStopPending ? (Speed > WalkSpeed * 1.05f ? RunBrake : 0.f)
        : (Gait == EGait::Land ? (bHardLanding ? RunLandDeceleration : LandDeceleration) : StopDeceleration);
    // The run layer follows the speed in the stride and is held while the
    // stride fades out under the next clip.
    if (Gait == EGait::Loop) RunWeight = FMath::FInterpTo(RunWeight, RunBlendAt(Speed), DeltaSeconds, 10.f);
    else if (Base != EClip::WalkLoop && !(FadeWeight > 0 && Fading == EClip::WalkLoop)) RunWeight = 0;
    // Held through the leap, so he lands into the gallop.
    SprintWeight = FMath::FInterpTo(SprintWeight, IsSprinting() && (Gait == EGait::Loop || Gait == EGait::Air) ? 1.f : 0.f, DeltaSeconds, IsSprinting() ? SprintBlendIn : SprintBlendOut);
    if (!IsSprinting() && SprintWeight < .01f) SprintWeight = 0;

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
    P.ClipSprint = SprintWeight > 0 ? Clips[static_cast<int32>(EClip::SprintLoop)] : nullptr;
    P.TimeSprint = WalkPhase * SprintPeriod;
    P.PeriodSprint = Period(EClip::SprintLoop);
    P.WeightSprint = SprintWeight;
    P.bRunOnA = Base == EClip::WalkLoop;
    P.bRunOnB = Fading == EClip::WalkLoop;
    // Slash layers on the move (a standing slash plays as the base clip).
    P.ClipUpper[0] = FadingLayerTime >= 0 ? Clips[static_cast<int32>(FadingLayerClip)] : nullptr;
    P.TimeUpper[0] = FadingLayerTime;
    P.WeightUpper[0] = FadingLayerWeight;
    P.ClipUpper[1] = LayerTime >= 0 ? Clips[static_cast<int32>(LayerClip)] : nullptr;
    P.TimeUpper[1] = LayerTime;
    P.WeightUpper[1] = LayerTime >= 0 ? LayerWeightAt(LayerTime, Clips[static_cast<int32>(LayerClip)]->GetPlayLength()) : 0.f;
    P.bFootIK = !bAirborne && AstralPhase != EAstral::SlideDown;   // nothing to plant on the way down the slide
    P.bAllowSettle = Gait == EGait::Idle;
    // Stance from the manifest intervals of whichever clip dominates. WalkLoop:
    // generated from the manifest (ChuckClipData.h), trimmed likewise.
    const bool bLoopDominant = ((Gait == EGait::Loop || (Gait == EGait::Strafe && Base == EClip::WalkLoop)) && FadeWeight < .5f) || (Gait == EGait::Stop && FadeWeight >= .5f);
    const bool bStanding = Gait == EGait::Idle || (Gait == EGait::Astral && AstralPhase != EAstral::SlideDown) || (Gait == EGait::Land && StateTime > .1f);   // the summon keeps his paws planted
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
        else if (bLoopDominant && SprintWeight >= .5f) bStance = I == 0 ? (WalkPhase > .02f && WalkPhase < SprintStanceFraction - .02f)
                                                                    : (WalkPhase > SprintHindLag + .02f && WalkPhase < SprintHindLag + SprintStanceFraction - .02f);
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
    if (Gait == EGait::WallRun || Gait == EGait::WallSide || Gait == EGait::Ladder || Gait == EGait::Vault || Gait == EGait::Hang || Gait == EGait::Climb || Gait == EGait::Swing || Gait == EGait::SwingLeap) P.bFootIK = false;
    // Tucked in the roll, the paws follow the clip untouched.
    if (Gait == EGait::Roll && !P.bStance[0] && !P.bStance[1]) P.bFootIK = false;

    // Place the mesh on the traced ground under the capsule, then offset each
    // paw by its own traced ground and drop the pelvis for a lower paw.
    const float CapsuleBottom = Location.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const bool bOffGround = bAirborne || Gait == EGait::WallRun || Gait == EGait::WallSide || Gait == EGait::Ladder || Gait == EGait::Vault || Gait == EGait::Hang || Gait == EGait::Climb || Gait == EGait::Swing || Gait == EGait::SwingLeap;
    const float Ground = bOffGround ? CapsuleBottom : FindGround(Location, CapsuleBottom);
    MeshDrop = FMath::FInterpTo(MeshDrop, FMath::Clamp(CapsuleBottom - Ground, 0.f, 4.f), DeltaSeconds, 20.f);
    GetMesh()->SetRelativeLocation(FVector(0, 0, -32.5f - MeshDrop));
    // On a side wall run his body leans out from the wall, paws toward it.
    WallSideTilt = FMath::FInterpTo(WallSideTilt, Gait == EGait::WallSide ? 1.f : 0.f, DeltaSeconds, Gait == EGait::WallSide ? 14.f : 8.f);
    if (WallSideTilt > .001f && !WallNormal.IsNearlyZero())
    {
        const float A = FMath::DegreesToRadians(WallSideLean * WallSideTilt);
        const FVector Lean = FVector::UpVector * FMath::Cos(A) + WallNormal * FMath::Sin(A);
        GetMesh()->SetWorldRotation(FQuat::FindBetweenNormals(FVector::UpVector, Lean) * GetActorQuat());
    }
    else GetMesh()->SetRelativeRotation(FRotator::ZeroRotator);
    // Swinging: the whole body pitched about his centre (about the ring, since
    // his centre hangs from it); after letting go it eases back upright.
    if (!bBodyPitchLive) BodyPitch = FMath::FInterpTo(BodyPitch, 0.f, DeltaSeconds, 9.f);
    if (FMath::Abs(BodyPitch) > .05f)
    {
        const FQuat R(BodyPitchAxis, FMath::DegreesToRadians(BodyPitch));
        GetMesh()->SetWorldLocationAndRotation(GetActorLocation() + R.RotateVector(FVector(0, 0, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - MeshDrop)), R * GetActorQuat());
    }
    else BodyPitch = 0;
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
    // The sewer's NPC-only floor over the Astral openings is not there for him.
    if (!bIgnoringAstralFloor)
        if (UPrimitiveComponent* AstralFloor = DockSewerAstralFloor())
        {
            GetCapsuleComponent()->IgnoreComponentWhenMoving(AstralFloor, true);
            bIgnoringAstralFloor = true;
        }
    // Remember entry before a fall leaves the valid tunnel footprint. Surface
    // travel/reset clears it, so later dock deaths still use the dock spawn.
    if(GetActorLocation().Z>=-100) bSewerRespawn=false;
    else if(IsWithinDockSewer(GetActorLocation())) bSewerRespawn=true;
    if(IsWithinDockPantry(GetActorLocation()) && GetActorLocation().Z<-150) bPantryRespawn=true;
    else if(GetActorLocation().Z>=-100 && !IsWithinDockPantry(GetActorLocation())) bPantryRespawn=false;
    UpdateCamera(DeltaSeconds);
    if (bSlideCamera) HoldSlideCamera();
    // When collision pulls the lens inside Chuck, avoid an obstructing head/jacket.
    GetMesh()->SetVisibility(!bAstralHidden && FVector::Dist(Camera->GetComponentLocation(),GetActorLocation()) > 70.f,true);
    UpdateMotion(DeltaSeconds);
    UpdateExhale(DeltaSeconds);
    if (GetActorLocation().Z < -100 && !IsWithinDockSewer(GetActorLocation()) && !IsWithinDockPantry(GetActorLocation())) FallToDeath();
}
