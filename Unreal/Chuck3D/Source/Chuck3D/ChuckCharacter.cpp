#include "ChuckCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"
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

    RatVisual = CreateDefaultSubobject<USceneComponent>(TEXT("RatVisual"));
    RatVisual->SetupAttachment(GetRootComponent());
    RatVisual->SetRelativeLocation(FVector(0,0,-35.f));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> BodyAsset(TEXT("/Game/Characters/Chuck/SK_ChuckBody.SK_ChuckBody"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FootAsset(TEXT("/Game/Characters/Chuck/SM_ChuckFoot.SM_ChuckFoot"));
    Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("ChuckBody"));
    Body->SetupAttachment(RatVisual);
    Body->SetSkinnedAssetAndUpdate(BodyAsset.Object);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if(BodyAsset.Object)
    {
        const auto& Slots=BodyAsset.Object->GetMaterials();
        for(int32 I=0; I<Slots.Num(); ++I)
        {
            const FString Name=Slots[I].MaterialSlotName.ToString();
            if(auto* Surface=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),*Name,*Name)))
                Body->SetMaterial(I,Surface);
        }
    }
    auto MakeFoot = [&](const TCHAR* Name,float Side)
    {
        auto* Foot=CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Foot->SetupAttachment(RatVisual);
        Foot->SetStaticMesh(FootAsset.Object);
        Foot->SetRelativeLocation(FVector(4,Side*7,2.5f));
        Foot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if(auto* Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_Skin.M_Skin")))
            for(int32 I=0; I<Foot->GetNumMaterials(); ++I) Foot->SetMaterial(I,Surface);
        return Foot;
    };
    LeftFoot=MakeFoot(TEXT("FootLeft"),-1);
    RightFoot=MakeFoot(TEXT("FootRight"),1);
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
    UpdateCamera();
    if (auto* PC = Cast<APlayerController>(Controller))
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
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
    GaitPhase = MotionAmount = AirAmount = LandingCompression = 0;
    Body->SetRelativeTransform(FTransform::Identity);
    LeftFoot->SetRelativeLocationAndRotation(FVector(4,-7,2.5f),FRotator::ZeroRotator);
    RightFoot->SetRelativeLocationAndRotation(FVector(4,7,2.5f),FRotator::ZeroRotator);
    UpdateSkeleton();
    UpdateCamera();
}
void AChuckCharacter::Landed(const FHitResult& Hit)
{
    // Read impact speed before CharacterMovement clears vertical velocity.
    LandingCompression = FMath::Clamp(-GetVelocity().Z / 170.f, 0.f, 1.f);
    Super::Landed(Hit);
}

void AChuckCharacter::UpdateMotion(float DeltaSeconds)
{
    const bool bAirborne = GetCharacterMovement()->IsFalling();
    const float Speed = GetVelocity().Size2D();
    MotionAmount = FMath::FInterpTo(MotionAmount, bAirborne ? 0.f : FMath::Clamp(Speed/95.f,0.f,1.f),DeltaSeconds,12.f);
    AirAmount = FMath::FInterpTo(AirAmount,bAirborne ? 1.f : 0.f,DeltaSeconds,14.f);
    LandingCompression = FMath::FInterpTo(LandingCompression,0.f,DeltaSeconds,9.f);
    // One gait cycle per 46 cm travelled; pushing against a wall produces no steps.
    if (!bAirborne) GaitPhase = FMath::Fmod(GaitPhase + Speed * DeltaSeconds * UE_TWO_PI / 46.f, UE_TWO_PI);
    const float Wave = FMath::Sin(GaitPhase);
    const float Bob = (1.f-FMath::Cos(2.f*GaitPhase))*.3f*MotionAmount;
    const FRotator Pose(-2.5f*MotionAmount - 3.f*AirAmount,0,Wave*.65f*MotionAmount);
    // Rotate the static form around its hips, keeping all motion off the capsule/camera.
    const FVector Pivot(0,0,25);
    Body->SetRelativeLocationAndRotation(Pivot-Pose.RotateVector(Pivot)+FVector(0,0,Bob-1.2f*LandingCompression),Pose);
    auto PoseFoot = [&](UStaticMeshComponent* Foot, float Side, float Phase)
    {
        const float Swing = FMath::Sin(Phase);
        const float Lift = FMath::Max(0.f,FMath::Cos(Phase));
        Foot->SetRelativeLocationAndRotation(
            FVector(4+Swing*3.f*MotionAmount-1.5f*AirAmount,Side*7,2.5f+Lift*1.8f*MotionAmount+2.f*AirAmount),
            FRotator(-Lift*8.f*MotionAmount-12.f*AirAmount,0,0));
    };
    PoseFoot(LeftFoot,-1,GaitPhase);
    PoseFoot(RightFoot,1,GaitPhase+UE_PI);
    UpdateSkeleton();
}

void AChuckCharacter::UpdateSkeleton()
{
    const auto* RigAsset = Cast<USkeletalMesh>(Body->GetSkinnedAsset());
    if (!RigAsset) return;
    const FReferenceSkeleton& Ref = RigAsset->GetRefSkeleton();
    Body->BoneSpaceTransforms = Ref.GetRefBonePose();
    TArray<FTransform> Rest;
    Rest.SetNum(Ref.GetNum());
    for (int32 I=0; I<Ref.GetNum(); ++I)
    {
        const int32 Parent=Ref.GetParentIndex(I);
        Rest[I]=Parent==INDEX_NONE ? Ref.GetRefBonePose()[I] : Ref.GetRefBonePose()[I]*Rest[Parent];
    }
    auto CurrentComponent = [&](int32 Index)
    {
        FTransform Result=Body->BoneSpaceTransforms[Index];
        for (int32 Parent=Ref.GetParentIndex(Index); Parent!=INDEX_NONE; Parent=Ref.GetParentIndex(Parent))
            Result=Result*Body->BoneSpaceTransforms[Parent];
        return Result;
    };
    auto Rotate = [&](FName Name, const FVector& Axis, float Degrees)
    {
        const int32 I=Ref.FindBoneIndex(Name);
        if(I==INDEX_NONE) return;
        const FQuat LocalDelta(Rest[I].GetRotation().UnrotateVector(Axis),FMath::DegreesToRadians(Degrees));
        Body->BoneSpaceTransforms[I].SetRotation((Ref.GetRefBonePose()[I].GetRotation()*LocalDelta).GetNormalized());
    };
    const float Wave=FMath::Sin(GaitPhase)*MotionAmount;
    Rotate(TEXT("arm_L"),FVector::YAxisVector,7.f*Wave-3.f*AirAmount);
    Rotate(TEXT("arm_R"),FVector::YAxisVector,-7.f*Wave-3.f*AirAmount);
    Rotate(TEXT("forearm_L"),FVector::YAxisVector,-3.f*Wave-5.f*AirAmount);
    Rotate(TEXT("forearm_R"),FVector::YAxisVector,3.f*Wave-5.f*AirAmount);
    for(int32 I=0; I<4; ++I)
        Rotate(FName(*FString::Printf(TEXT("tail_%d"),I)),FVector::ZAxisVector,
            FMath::Sin(GaitPhase-I*.55f)*MotionAmount*2.5f);

    // Two-bone IK connects each leg to its animated ankle target. Solving in
    // body space compensates for lean and landing compression without moving feet.
    auto SolveLeg = [&](const TCHAR* Side,UStaticMeshComponent* Foot,float Sign)
    {
        const int32 Upper=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("thigh_%s"),Side)));
        const int32 Lower=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("shin_%s"),Side)));
        if(Upper==INDEX_NONE || Lower==INDEX_NONE) return;
        const FVector Hip=Rest[Upper].GetLocation(), Knee=Rest[Lower].GetLocation();
        const FVector RestAnkle(2,Sign*7,5);
        const FVector Target=Body->GetRelativeTransform().InverseTransformPosition(Foot->GetRelativeLocation()+FVector(-2,0,2.5f));
        const float A=FVector::Distance(Hip,Knee), B=FVector::Distance(Knee,RestAnkle);
        const FVector Direction=(Target-Hip).GetSafeNormal();
        const float D=FMath::Clamp(static_cast<float>(FVector::Distance(Target,Hip)),FMath::Abs(A-B)+.01f,A+B-.01f);
        const float Along=(A*A+D*D-B*B)/(2*D);
        const FVector Bend=(FVector(-1,0,0)-Direction*FVector::DotProduct(FVector(-1,0,0),Direction)).GetSafeNormal();
        const FVector NewKnee=Hip+Direction*Along+Bend*FMath::Sqrt(FMath::Max(0.f,A*A-Along*Along));
        FTransform UpperPose=Rest[Upper];
        UpperPose.SetRotation(FQuat::FindBetweenNormals((Knee-Hip).GetSafeNormal(),(NewKnee-Hip).GetSafeNormal())*Rest[Upper].GetRotation());
        Body->BoneSpaceTransforms[Upper]=UpperPose.GetRelativeTransform(CurrentComponent(Ref.GetParentIndex(Upper)));
        FTransform LowerPose=Rest[Lower];
        LowerPose.SetLocation(NewKnee);
        LowerPose.SetRotation(FQuat::FindBetweenNormals((RestAnkle-Knee).GetSafeNormal(),(Target-NewKnee).GetSafeNormal())*Rest[Lower].GetRotation());
        Body->BoneSpaceTransforms[Lower]=LowerPose.GetRelativeTransform(UpperPose);
    };
    SolveLeg(TEXT("L"),LeftFoot,-1);
    SolveLeg(TEXT("R"),RightFoot,1);
    Body->MarkRefreshTransformDirty();
}
void AChuckCharacter::Quit() { UKismetSystemLibrary::QuitGame(this, Cast<APlayerController>(Controller), EQuitPreference::Quit, false); }
void AChuckCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateCamera(DeltaSeconds);
    // When collision pulls the lens inside Chuck, avoid an obstructing head/jacket.
    RatVisual->SetVisibility(FVector::Dist(Camera->GetComponentLocation(),GetActorLocation()) > 70.f,true);
    UpdateMotion(DeltaSeconds);
    if (GetActorLocation().Z < -100) ResetToDock();
}
