#include "ChuckCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
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
        if(FootAsset.Object)
        {
            const auto& Slots=FootAsset.Object->GetStaticMaterials();
            for(int32 I=0; I<Slots.Num(); ++I)
            {
                const FString SurfaceName=Slots[I].MaterialSlotName.ToString();
                if(auto* Surface=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),*SurfaceName,*SurfaceName)))
                    Foot->SetMaterial(I,Surface);
            }
        }
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
    bContactsReady=false; bFirstStep=true; SwingFoot=INDEX_NONE; NextFoot=0;
    PreviousMotionLocation=GetActorLocation();
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
    UpdateFootContacts(DeltaSeconds,bAirborne);
    const float Wave = FMath::Sin(GaitPhase);
    const FRotator Pose(-1.2f*MotionAmount - 1.5f*AirAmount,0,Wave*.15f*MotionAmount);
    // Restraint comes from the pose, not from reducing the actual planted stride.
    const FVector Pivot(0,0,25);
    Body->SetRelativeLocationAndRotation(Pivot-Pose.RotateVector(Pivot)+FVector(0,0,-.6f*LandingCompression),Pose);
    UpdateSkeleton();
}

bool AChuckCharacter::FindFootSupport(const FVector& Desired,FVector& Supported) const
{
    const float FloorZ=GetActorLocation().Z-GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ChuckFootSupport),false,this);
    const FVector Top(Desired.X,Desired.Y,FloorZ+12.f);
    const bool bHit=GetWorld()->LineTraceSingleByChannel(Hit,Top,Top-FVector(0,0,32),ECC_Visibility,Query);
    // Visual contact correction only: it cannot climb a crate or bridge a gap.
    if(bHit && Hit.ImpactNormal.Z>=.7f && Hit.ImpactPoint.Z<=FloorZ+7.f)
    {
        Supported=Hit.ImpactPoint+FVector(0,0,2.f); // mesh sole is 2 cm below its origin
        return true;
    }
    Supported=FVector(Desired.X,Desired.Y,FloorZ+2.f);
    return false;
}

void AChuckCharacter::UpdateFootContacts(float DeltaSeconds,bool bAirborne)
{
    UStaticMeshComponent* Components[2]={LeftFoot,RightFoot};
    const FVector Location=GetActorLocation();
    if(FVector::Dist(Location,PreviousMotionLocation)>50.f) bContactsReady=false;
    PreviousMotionLocation=Location;
    if(bAirborne)
    {
        bContactsReady=false; SwingFoot=INDEX_NONE; bFirstStep=true;
        for(int32 I=0; I<2; ++I)
            Components[I]->SetRelativeLocationAndRotation(FVector(3.f,(I==0 ? -7.f:7.f),4.5f+2.f*AirAmount),FRotator(4.f*AirAmount,0,0));
        return;
    }
    FVector Neutral[2];
    for(int32 I=0; I<2; ++I)
        Neutral[I]=GetActorTransform().TransformPosition(FVector(4.f,I==0 ? -7.f:7.f,0));
    const FQuat Heading=FRotator(0,GetActorRotation().Yaw,0).Quaternion();
    const FVector Velocity=GetVelocity()*FVector(1,1,0);
    const float Speed=Velocity.Size();
    if(!bContactsReady)
    {
        for(int32 I=0; I<2; ++I)
        {
            Feet[I].bSupported=FindFootSupport(Neutral[I],Feet[I].Position);
            Feet[I].Rotation=Heading;
        }
        bContactsReady=true; bFirstStep=true; SwingFoot=INDEX_NONE;
    }
    if(SwingFoot!=INDEX_NONE)
    {
        FFootContact& Foot=Feet[SwingFoot];
        Foot.Elapsed=FMath::Min(Foot.Elapsed+DeltaSeconds,Foot.Duration);
        const float T=Foot.Elapsed/Foot.Duration;
        const float Ease=T*T*(3.f-2.f*T);
        // Predict remaining travel, not a fixed distant landing point: braking
        // and turns can retarget a swing without dragging the planted foot.
        FVector Target;
        Foot.bSupported=FindFootSupport(Neutral[SwingFoot]+Velocity*(Foot.Duration-Foot.Elapsed+.05f),Target);
        Foot.Position=FMath::Lerp(Foot.Start,Target,Ease)+FVector(0,0,FMath::Sin(T*PI)*2.2f);
        Foot.Rotation=FQuat::Slerp(Foot.StartRotation,Heading,Ease)*FRotator(4.f*FMath::Sin(T*PI),0,0).Quaternion();
        GaitPhase=(SwingFoot==0 ? 0.f:PI)+T*PI;
        if(T>=1.f)
        {
            Foot.Position=Target; Foot.Rotation=Heading;
            NextFoot=1-SwingFoot; SwingFoot=INDEX_NONE;
        }
    }
    else
    {
        float Error[2];
        for(int32 I=0; I<2; ++I)
            Error[I]=FVector::Dist2D(Feet[I].Position,Neutral[I])+FMath::Abs(FMath::FindDeltaAngleDegrees(Feet[I].Rotation.Rotator().Yaw,GetActorRotation().Yaw))*.06f;
        const bool bWalking=Speed>4.f;
        const int32 Candidate=bWalking ? NextFoot : (Error[0]>=Error[1] ? 0:1);
        if(Error[Candidate]>(bWalking ? .35f:2.f))
        {
            SwingFoot=Candidate;
            FFootContact& Foot=Feet[Candidate];
            Foot.Start=Foot.Position; Foot.StartRotation=Foot.Rotation; Foot.Elapsed=0;
            Foot.Duration=bWalking ? FMath::Lerp(.3f,.2f,FMath::Clamp(Speed/95.f,0.f,1.f)):.16f;
            if(bFirstStep && bWalking) Foot.Duration=FMath::Min(Foot.Duration,.12f);
            bFirstStep=false;
        }
    }
    for(int32 I=0; I<2; ++I)
    {
        // Retain exact world position and heading throughout stance, including
        // character translation/yaw. The two-bone solve follows these targets.
        Components[I]->SetWorldLocationAndRotation(Feet[I].Position,Feet[I].Rotation);
    }
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
    Rotate(TEXT("arm_L"),FVector::YAxisVector,4.f*Wave-3.f*AirAmount);
    Rotate(TEXT("arm_R"),FVector::YAxisVector,-4.f*Wave-3.f*AirAmount);
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
        const FVector Target=Body->GetRelativeTransform().InverseTransformPosition(Foot->GetRelativeLocation()+Foot->GetRelativeRotation().RotateVector(FVector(-2,0,2.5f)));
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
