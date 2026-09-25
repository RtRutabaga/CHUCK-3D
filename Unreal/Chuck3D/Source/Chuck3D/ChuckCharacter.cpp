#include "ChuckCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AChuckCharacter::AChuckCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(7.0f, 15.24f);
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
    RatVisual->SetRelativeLocation(FVector(0,0,-15.24f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gray(TEXT("/Game/Prototype/Materials/M_Gray.M_Gray"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Purple(TEXT("/Game/Prototype/Materials/M_Purple.M_Purple"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Pink(TEXT("/Game/Prototype/Materials/M_Pink.M_Pink"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Dark(TEXT("/Game/Prototype/Materials/M_Dark.M_Dark"));
    auto Part = [&](const TCHAR* Name, FVector Pos, FVector Size, UMaterialInterface* Material, bool Box=false)
    {
        auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Mesh->SetupAttachment(RatVisual);
        Mesh->SetStaticMesh(Box ? Cube.Object : Sphere.Object);
        Mesh->SetRelativeLocation(Pos);
        Mesh->SetRelativeScale3D(Size / 100.0f);
        Mesh->SetMaterial(0, Material);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return Mesh;
    };
    Part(TEXT("Body"), FVector(0,0,13), FVector(10,11,20), Gray.Object);
    Part(TEXT("Head"), FVector(2,0,24), FVector(12,10,9), Gray.Object);
    Part(TEXT("Muzzle"), FVector(8,0,23), FVector(9,6,5), Gray.Object);
    Part(TEXT("Nose"), FVector(12,0,23), FVector(2,3,2), Pink.Object);
    Part(TEXT("EarLeft"), FVector(0,-4,28), FVector(4,4,4.96f), Pink.Object);
    Part(TEXT("EarRight"), FVector(0,4,28), FVector(4,4,4.96f), Pink.Object);
    Part(TEXT("EyeLeft"), FVector(6,-4.2f,25), FVector(1.3f,1.3f,1), Dark.Object);
    Part(TEXT("EyeRight"), FVector(6,4.2f,25), FVector(1.3f,1.3f,1), Dark.Object);
    // Three pieces leave the gray chest exposed: oversized, visibly open jacket.
    Part(TEXT("JacketBack"), FVector(-4,0,12), FVector(8,15,21), Purple.Object);
    Part(TEXT("JacketLeft"), FVector(0,-6,12), FVector(10,6,21), Purple.Object);
    Part(TEXT("JacketRight"), FVector(0,6,12), FVector(10,6,21), Purple.Object);
    Part(TEXT("PawLeft"), FVector(3,-6,8), FVector(4,3,5), Gray.Object);
    Part(TEXT("PawRight"), FVector(3,6,8), FVector(4,3,5), Gray.Object);
    LeftFoot = Part(TEXT("FootLeft"), FVector(3,-3,2), FVector(8,3,4), Pink.Object);
    RightFoot = Part(TEXT("FootRight"), FVector(3,3,2), FVector(8,3,4), Pink.Object);
    Part(TEXT("Tail"), FVector(-15,0,3), FVector(24,2,2), Pink.Object);

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
    Boom->TargetArmLength = FMath::Lerp(145.f, 340.f, CameraBlend);
    Boom->SetRelativeLocation(FVector(0,0,FMath::Lerp(14.f,8.f,CameraBlend)));
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
    UpdateCamera();
}
void AChuckCharacter::Quit() { UKismetSystemLibrary::QuitGame(this, Cast<APlayerController>(Controller), EQuitPreference::Quit, false); }
void AChuckCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateCamera(DeltaSeconds);
    // When collision pulls the lens inside Chuck, avoid an obstructing head/jacket.
    RatVisual->SetVisibility(FVector::Dist(Camera->GetComponentLocation(),GetActorLocation()) > 38.f,true);
    const float WalkAmount = FMath::Clamp(GetVelocity().Size2D()/95.f,0.f,1.f);
    GaitPhase += DeltaSeconds * WalkAmount * 13.f;
    const float Stride = FMath::Sin(GaitPhase)*2.5f*WalkAmount;
    LeftFoot->SetRelativeLocation(FVector(3+Stride,-3,2+FMath::Max(0.f,Stride)*.35f));
    RightFoot->SetRelativeLocation(FVector(3-Stride,3,2+FMath::Max(0.f,-Stride)*.35f));
    if (GetActorLocation().Z < -100) ResetToDock();
}
