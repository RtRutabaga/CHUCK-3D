#include "DockGameMode.h"
#include "ChuckCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

ADockGameMode::ADockGameMode()
{
    DefaultPawnClass = AChuckCharacter::StaticClass();
    HUDClass = ADockHUD::StaticClass();
    PrimaryActorTick.bCanEverTick = true;
}

void ADockGameMode::StartPlay()
{
    UWorld* World = GetWorld();
    auto* Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Sphere = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Cylinder = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto Material = [](const TCHAR* Name) {
        return LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Prototype/Materials/M_%s.M_%s"),Name,Name));
    };
    auto Shape = [&](const TCHAR* Name,FVector Position,FVector Size,const TCHAR* Color,UStaticMesh* Mesh=nullptr,bool Collision=true) {
        auto* Actor = World->SpawnActor<AStaticMeshActor>(Position,FRotator::ZeroRotator);
        Actor->Tags.Add(FName(Name));
        auto* Comp = Actor->GetStaticMeshComponent();
        Comp->SetMobility(EComponentMobility::Movable);
        Comp->SetStaticMesh(Mesh ? Mesh : Cube);
        Comp->SetWorldScale3D(Size/100.f);
        Comp->SetMaterial(0,Material(Color));
        Comp->SetCollisionProfileName(Collision ? TEXT("BlockAll") : TEXT("NoCollision"));
        return Actor;
    };
    // Units are centimetres. Ground top = 0; geometry is intentionally simple.
    Shape(TEXT("Quay"),FVector(-150,0,-20),FVector(700,800,40),TEXT("Stone"));
    Shape(TEXT("Sea"),FVector(900,0,-65),FVector(2200,2400,10),TEXT("Water"),nullptr,false);
    // Short pier with a 24 cm missing board. Chuck's jump travels about 41 cm.
    for(int32 Row=0;Row<26;++Row)
    {
        if(Row==13) continue;
        Shape(TEXT("DockPlank"),FVector(212+Row*24,0,-6),FVector(23,180,12),Row%2 ? TEXT("Wood") : TEXT("WoodLight"));
    }
    for(float X : {210.f,450.f,790.f}) for(float Y : {-100.f,100.f})
        Shape(TEXT("MooringPost"),FVector(X,Y,-25),FVector(22,22,110),TEXT("Wood"),Cylinder);
    // Human-sized tavern frontage, quiet and closed. No interior or dialogue.
    Shape(TEXT("Tavern"),FVector(-60,365,155),FVector(560,60,310),TEXT("Plaster"));
    Shape(TEXT("TavernRoof"),FVector(-60,357,314),FVector(600,100,16),TEXT("Roof"));
    for(float X : {-330.f,-140.f,40.f,210.f})
        Shape(TEXT("Timber"),FVector(X,328,153),FVector(14,14,306),TEXT("Wood"));
    Shape(TEXT("Door"),FVector(40,326,105),FVector(95,10,210),TEXT("Wood"));
    Shape(TEXT("DoorLatch"),FVector(70,318,100),FVector(8,6,3),TEXT("Dark"));
    for(float X : {-230.f,135.f}) {
        Shape(TEXT("WindowFrame"),FVector(X,322,160),FVector(80,12,95),TEXT("Wood"));
        Shape(TEXT("Window"),FVector(X,314,160),FVector(64,4,79),TEXT("Amber"));
    }
    Shape(TEXT("TavernSign"),FVector(40,313,250),FVector(170,12,32),TEXT("Wood"));
    auto* Sign = World->SpawnActor<AActor>();
    auto* Text = NewObject<UTextRenderComponent>(Sign);
    Sign->SetRootComponent(Text); Text->RegisterComponent();
    Text->SetWorldLocation(FVector(40,305,248));
    Text->SetWorldRotation(FRotator(0,-90,0));
    Text->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    Text->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    Text->SetWorldSize(18); Text->SetText(FText::FromString(TEXT("TAVERN")));
    Text->SetTextRenderColor(FColor(225,205,169));
    // Warehouse closes the back of the study; front/side edges remain readable.
    Shape(TEXT("Warehouse"),FVector(-470,50,160),FVector(60,600,320),TEXT("Plaster"));
    Shape(TEXT("Barrel"),FVector(-330,-80,45),FVector(62,62,90),TEXT("Wood"),Cylinder);
    for(float Z : {14.f,72.f}) Shape(TEXT("BarrelBand"),FVector(-330,-80,Z),FVector(65,65,7),TEXT("Dark"),Cylinder);
    Shape(TEXT("Crate"),FVector(-80,60,30),FVector(60,65,60),TEXT("WoodLight"));
    Shape(TEXT("LowStep"),FVector(-40,-155,5),FVector(60,65,10),TEXT("Wood"));
    Shape(TEXT("BenchTop"),FVector(-210,225,45),FVector(160,42,8),TEXT("WoodLight"));
    for(float X : {-275.f,-145.f}) Shape(TEXT("BenchLeg"),FVector(X,225,21),FVector(12,32,42),TEXT("Wood"));
    // 180 cm dock worker, including boots and head. A scale prop, not an NPC system.
    const FVector Human(90,200,0);
    for(float X : {-12.f,12.f}) {
        Shape(TEXT("HumanBoot"),Human+FVector(X,-4,7),FVector(19,34,14),TEXT("Dark"));
        Shape(TEXT("HumanLeg"),Human+FVector(X,0,48),FVector(17,20,70),TEXT("Navy"));
    }
    Shape(TEXT("HumanBody"),Human+FVector(0,0,115),FVector(49,27,70),TEXT("Navy"));
    Shape(TEXT("HumanHead"),Human+FVector(0,0,165),FVector(25,25,30),TEXT("Skin"),Sphere);
    for(float X : {-31.f,31.f}) Shape(TEXT("HumanArm"),Human+FVector(X,0,112),FVector(13,17,62),TEXT("Navy"));

    auto* Sun = World->SpawnActor<ADirectionalLight>(FVector(0,0,500),FRotator(-38,-40,0));
    Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sun->GetLightComponent()->SetIntensity(3.0f);
    Sun->GetLightComponent()->SetLightColor(FLinearColor(1,.86f,.68f));
    auto* Sky = World->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(0.8f);
    // The enclosing sky is 90 m away, below UE's default 1500 m sky threshold.
    // Capture it as ambient light so the shaded sides remain readable at rat height.
    Sky->GetLightComponent()->SkyDistanceThreshold = 1000;
    Sky->GetLightComponent()->bLowerHemisphereIsBlack = false;
    // A pale enclosing sphere gives skylight capture a quiet flat horizon.
    auto* Horizon = Shape(TEXT("Horizon"),FVector(0,0,0),FVector(18000,18000,18000),TEXT("Sky"),Sphere,false);
    Horizon->GetStaticMeshComponent()->SetCastShadow(false);
    Sky->GetLightComponent()->RecaptureSky();
    auto* Start = World->SpawnActor<APlayerStart>(AChuckCharacter::StartLocation(),FRotator::ZeroRotator);
    (void)Start;
    Super::StartPlay();
    if(auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this,0))) Chuck->ResetToDock();
    UE_LOG(LogTemp,Display,TEXT("CHUCK: docks ready; Chuck 30.48 cm, human 180 cm; two cameras available."));
    bSmokeTest = FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest"));
}

void ADockGameMode::Check(bool Passed,const TCHAR* Description)
{
    if(!Passed) ++TestFailures;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_TEST %s: %s"),Passed ? TEXT("PASS") : TEXT("FAIL"),Description);
}

void ADockGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!bSmokeTest) return;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Chuck) { Check(false,TEXT("player spawned")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    StageTime += DeltaSeconds;
    if(TestStage==0 && StageTime>1)
    {
        Check(Chuck->GetCharacterMovement()->IsMovingOnGround(),TEXT("spawn settles on quay"));
        Check(FMath::IsNearlyEqual(Chuck->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()*2,30.48f,.01f),TEXT("Chuck collision height is 30.48 cm"));
        Check(Chuck->IsElevated(),TEXT("starts in elevated camera"));
        Chuck->ToggleCamera(); Check(!Chuck->IsElevated(),TEXT("switches to rat-height camera"));
        Chuck->ToggleCamera(); Check(Chuck->IsElevated(),TEXT("switches back to elevated camera"));
        TestStage=1; StageTime=0;
    }
    else if(TestStage==1)
    {
        Chuck->AddMovementInput(FVector(1,0,0),1);
        if(StageTime>1)
        {
            Check(Chuck->GetActorLocation().X > -175,TEXT("walking advances across quay"));
            Chuck->Jump(); MaxJumpZ=Chuck->GetActorLocation().Z; TestStage=2; StageTime=0;
        }
    }
    else if(TestStage==2)
    {
        MaxJumpZ=FMath::Max(MaxJumpZ,static_cast<float>(Chuck->GetActorLocation().Z));
        if(StageTime>1)
        {
            Check(MaxJumpZ>30,TEXT("jump lifts Chuck above floor"));
            Check(Chuck->GetCharacterMovement()->IsMovingOnGround(),TEXT("jump lands back on quay"));
            Chuck->GetCharacterMovement()->StopMovementImmediately();
            Chuck->SetActorLocation(FVector(-395,0,18)); TestStage=3; StageTime=0;
        }
    }
    else if(TestStage==3)
    {
        Chuck->AddMovementInput(FVector(-1,0,0),1);
        if(StageTime>1)
        {
            Check(Chuck->GetActorLocation().X > -440,TEXT("warehouse wall blocks walking"));
            Chuck->SetActorLocation(FVector(1000,0,-130)); TestStage=4; StageTime=0;
        }
    }
    else if(TestStage==4 && StageTime>.3f)
    {
        Check(FVector::Dist2D(Chuck->GetActorLocation(),AChuckCharacter::StartLocation())<5,TEXT("fall resets to dock"));
        FHitResult Hit;
        const bool HitGap=GetWorld()->LineTraceSingleByChannel(Hit,FVector(524,0,50),FVector(524,0,-30),ECC_Visibility);
        Check(!HitGap,TEXT("missing pier board is a real gap"));
        const bool HitBoard=GetWorld()->LineTraceSingleByChannel(Hit,FVector(500,0,50),FVector(500,0,-30),ECC_Visibility);
        Check(HitBoard,TEXT("adjacent pier board has collision"));
        Chuck->ResetToDock();
        auto* PC=Cast<APlayerController>(Chuck->GetController());
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W,IE_Pressed,1));
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::C,IE_Pressed,1));
        TestStage=5; StageTime=0;
    }
    else if(TestStage==5 && StageTime>1)
    {
        auto* PC=Cast<APlayerController>(Chuck->GetController());
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W,IE_Released,0));
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::C,IE_Released,0));
        Check(Chuck->GetActorLocation().X > -175,TEXT("keyboard W mapping walks"));
        Check(!Chuck->IsElevated(),TEXT("keyboard C mapping switches camera"));
        Chuck->ResetToDock();
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Top,IE_Pressed,1));
        TestStage=6; StageTime=0;
    }
    else if(TestStage==6)
    {
        auto* PC=Cast<APlayerController>(Chuck->GetController());
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY,IE_Axis,1));
        if(StageTime>1)
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY,IE_Axis,0));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Top,IE_Released,0));
            Check(Chuck->GetActorLocation().X > -175,TEXT("Xbox left-stick mapping walks"));
            Check(Chuck->IsElevated(),TEXT("Xbox Y mapping switches camera"));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Bottom,IE_Pressed,1));
            MaxJumpZ=Chuck->GetActorLocation().Z;
            TestStage=7; StageTime=0;
        }
    }
    else if(TestStage==7)
    {
        MaxJumpZ=FMath::Max(MaxJumpZ,static_cast<float>(Chuck->GetActorLocation().Z));
        if(StageTime>1)
        {
            Check(MaxJumpZ>30,TEXT("Xbox A mapping jumps"));
            Cast<APlayerController>(Chuck->GetController())->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Bottom,IE_Released,0));
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(465,0,18));
            TestStage=8; StageTime=0;
        }
    }
    else if(TestStage==8)
    {
        Chuck->AddMovementInput(FVector(1,0,0),1);
        if(Chuck->GetActorLocation().X>=500 && Chuck->GetCharacterMovement()->IsMovingOnGround())
        { Chuck->Jump(); TestStage=9; StageTime=0; }
        else if(StageTime>2) { Check(false,TEXT("pier jump approach")); TestStage=9; StageTime=0; }
    }
    else if(TestStage==9)
    {
        Chuck->AddMovementInput(FVector(1,0,0),1);
        if(StageTime>1)
        {
            Check(Chuck->GetActorLocation().X>550 && Chuck->GetCharacterMovement()->IsMovingOnGround(),GapRuns==0 ? TEXT("pier gap crossed in elevated camera") : TEXT("pier gap crossed in rat-height camera"));
            ++GapRuns;
            if(GapRuns==1)
            { Chuck->ResetToDock(); Chuck->ToggleCamera(); Chuck->SetActorLocation(FVector(465,0,18)); TestStage=8; StageTime=0; }
            else if(FParse::Param(FCommandLine::Get(),TEXT("ChuckCapture")))
            {
                Chuck->ResetToDock(); Chuck->ToggleCamera(); Chuck->SetActorLocation(FVector(-30,100,18));
                Cast<APlayerController>(Chuck->GetController())->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseX,IE_Axis,28.125f));
                TestStage=20; StageTime=0;
            }
            else TestStage=99;
        }
    }
    else if(TestStage==20 && StageTime>2)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Windows/Scale_Elevated.png"),true,false);
        TestStage=21; StageTime=0;
    }
    else if(TestStage==21 && StageTime>1)
    { Chuck->ToggleCamera(); TestStage=22; StageTime=0; }
    else if(TestStage==22 && StageTime>2)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Windows/Scale_RatHeight.png"),true,false);
        TestStage=23; StageTime=0;
    }
    else if(TestStage==23 && StageTime>1) TestStage=99;
    else if(TestStage==99)
    {
        UE_LOG(LogTemp,Display,TEXT("CHUCK_TEST_COMPLETE failures=%d"),TestFailures);
        bSmokeTest=false;
        FPlatformMisc::RequestExitWithStatus(false,TestFailures ? 1 : 0);
    }
}

void ADockHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* Chuck = Cast<AChuckCharacter>(GetOwningPawn());
    if(!Chuck || !Canvas) return;
    DrawRect(FLinearColor(0.035f,0.04f,0.045f,0.85f),18,18,440,79);
    DrawText(TEXT("CHUCK  /  WATERDEEP DOCKS"),FLinearColor(.94f,.88f,.75f),30,27,GEngine->GetSmallFont(),1.25f);
    DrawText(Chuck->IsElevated() ? TEXT("ELEVATED CAMERA") : TEXT("RAT-HEIGHT FOLLOW CAMERA"),FLinearColor(.77f,.67f,.94f),30,54,GEngine->GetSmallFont(),1.1f);
    DrawText(TEXT("30.48 cm rat  /  180 cm dock worker"),FLinearColor(.7f,.73f,.76f),30,76,GEngine->GetSmallFont());
    DrawRect(FLinearColor(0.035f,0.04f,0.045f,0.85f),18,Canvas->SizeY-65,Canvas->SizeX-36,47);
    DrawText(TEXT("WASD / Left stick: walk    Space / A: jump    C / Y: camera    Mouse, Q/E / Right stick: turn"),FLinearColor(.91f,.9f,.85f),30,Canvas->SizeY-58,GEngine->GetSmallFont());
    DrawText(TEXT("R / View: reset    Esc / Menu: exit    Compare the same route in both cameras."),FLinearColor(.75f,.77f,.8f),30,Canvas->SizeY-37,GEngine->GetSmallFont());
}
