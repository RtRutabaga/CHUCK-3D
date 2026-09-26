#include "DockGameMode.h"
#include "ChuckCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
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
    Shape(TEXT("Sea"),FVector(900,0,-65),FVector(18000,18000,10),TEXT("Water"),nullptr,false);
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
    Shape(TEXT("HumanBody"),Human+FVector(0,0,115),FVector(49,30,70),TEXT("Navy"),Sphere);
    Shape(TEXT("HumanHead"),Human+FVector(0,0,165),FVector(25,25,30),TEXT("Skin"),Sphere);
    for(float X : {-31.f,31.f}) Shape(TEXT("HumanArm"),Human+FVector(X,0,112),FVector(13,17,62),TEXT("Navy"),Sphere);

    Shape(TEXT("WorkerCap"),Human+FVector(0,0,178),FVector(28,29,4),TEXT("Dark"),Sphere,false);
    Shape(TEXT("WorkerBelt"),Human+FVector(0,0,89),FVector(44,29,5),TEXT("Wood"),nullptr,false);
    for(float X : {-31.f,31.f}) Shape(TEXT("WorkerHand"),Human+FVector(X,0,79),FVector(10,12,15),TEXT("Skin"),Sphere,false);
    // Surface detail is nonblocking; the original simple collision remains predictable.
    FRandomStream DetailRandom(73);
    for(int32 Row=0;Row<20;++Row) for(int32 Col=0;Col<17;++Col)
    {
        const float X=-477+Col*40+(Row%2)*14;
        if(X>181) continue;
        auto* Paver=Shape(TEXT("Paving"),FVector(X,-380+Row*38,-1.2f),FVector(37,35,3),DetailRandom.FRand()>.45f ? TEXT("Stone") : TEXT("Plaster"),nullptr,false);
        Paver->SetActorRotation(FRotator(0,DetailRandom.FRandRange(-1.5f,1.5f),0));
    }
    for(int32 Row=0;Row<26;++Row)
    {
        if(Row==13) continue;
        for(float Y : {-73.f,73.f})
            Shape(TEXT("PlankNail"),FVector(212+Row*24,Y,.3f),FVector(1.6f,1.6f,.6f),TEXT("Dark"),Cylinder,false);
        // Long fine seams make grain visible at Chuck's scale.
        for(int32 Grain=0;Grain<3;++Grain)
            Shape(TEXT("WoodGrain"),FVector(205+Row*24+Grain*5,DetailRandom.FRandRange(-10,10),.05f),FVector(.3f,130, .1f),TEXT("Wood"),nullptr,false);
    }
    for(float X : {210.f,450.f,790.f}) for(float Y : {-100.f,100.f})
    {
        for(float Z : {12.f,19.f})
            Shape(TEXT("PostBand"),FVector(X,Y,Z),FVector(23,23,3),TEXT("Dark"),Cylinder,false);
        Shape(TEXT("PostCap"),FVector(X,Y,31),FVector(24,24,3),TEXT("WoodLight"),Cylinder,false);
    }
    for(float X : {-230.f,135.f})
    {
        Shape(TEXT("WindowMullion"),FVector(X,309,160),FVector(4,5,80),TEXT("Wood"),nullptr,false);
        Shape(TEXT("WindowCrossbar"),FVector(X,309,160),FVector(64,5,4),TEXT("Wood"),nullptr,false);
        Shape(TEXT("WindowSill"),FVector(X,305,112),FVector(90,24,7),TEXT("Stone"),nullptr,false);
    }
    for(float Z : {28.f,218.f,298.f})
        Shape(TEXT("HorizontalTimber"),FVector(-60,326,Z),FVector(550,14,10),TEXT("Wood"),nullptr,false);
    for(int32 Slat=0;Slat<8;++Slat)
        Shape(TEXT("DoorBoard"),FVector(-2+Slat*12,319,105),FVector(10.5f,3,205),Slat%3 ? TEXT("Wood") : TEXT("WoodLight"),nullptr,false);
    for(float Z : {42.f,177.f})
        Shape(TEXT("DoorIron"),FVector(40,315,Z),FVector(91,3,5),TEXT("Dark"),nullptr,false);
    // Overlapping slate strips and projecting rafters give the frontage a roof silhouette.
    for(int32 Row=0;Row<5;++Row) for(int32 Col=0;Col<20;++Col)
    {
        auto* Tile=Shape(TEXT("RoofSlate"),FVector(-349+Col*30,315+Row*22,320+Row*10),FVector(29,30,5),Row%2 ? TEXT("Roof") : TEXT("Dark"),nullptr,false);
        Tile->SetActorRotation(FRotator(0,0,24));
    }
    for(float X : {-320.f,-200.f,-80.f,40.f,160.f})
        Shape(TEXT("Rafter"),FVector(X,323,308),FVector(10,90,12),TEXT("Wood"),nullptr,false);
    for(int32 Band=0;Band<12;++Band)
    {
        const float Angle=Band*PI/6;
        Shape(TEXT("BarrelStave"),FVector(-330+30*FMath::Cos(Angle),-80+30*FMath::Sin(Angle),44),FVector(3,3,84),TEXT("WoodLight"),Cylinder,false);
    }
    for(float Z : {7.f,53.f})
        Shape(TEXT("CrateFrame"),FVector(-80,26,Z),FVector(64,5,6),TEXT("Wood"),nullptr,false);
    for(float X : {-107.f,-53.f})
        Shape(TEXT("CrateFrame"),FVector(X,26,30),FVector(6,5,60),TEXT("Wood"),nullptr,false);
    auto* Brace=Shape(TEXT("CrateBrace"),FVector(-80,23,30),FVector(73,4,5),TEXT("Wood"),nullptr,false);
    Brace->SetActorRotation(FRotator(40,0,0));
    // A quiet harbor silhouette beyond the playable dock; no additional map.
    for(int32 Building=0;Building<11;++Building)
    {
        const float Height=DetailRandom.FRandRange(280,580);
        Shape(TEXT("FarWarehouse"),FVector(3500,-2200+Building*420,Height*.5f-55),FVector(250,245,Height),Building%2 ? TEXT("Roof") : TEXT("Stone"),nullptr,false);
        Shape(TEXT("FarRoof"),FVector(3500,-2200+Building*420,Height-40),FVector(275,270,25),TEXT("Dark"),nullptr,false);
    }
    Shape(TEXT("MooredHull"),FVector(980,600,-30),FVector(550,160,90),TEXT("Wood"),Sphere,false);
    Shape(TEXT("ShipMast"),FVector(980,600,210),FVector(10,10,460),TEXT("Wood"),Cylinder,false);
    Shape(TEXT("ShipYard"),FVector(980,600,320),FVector(12,260,10),TEXT("Wood"),nullptr,false);
    Shape(TEXT("FurledSail"),FVector(980,600,312),FVector(18,245,19),TEXT("Plaster"),Sphere,false);
    // Slim ripple strips break up the flat water without costly transparent shaders.
    for(int32 Ripple=0;Ripple<70;++Ripple)
        Shape(TEXT("WaterRipple"),FVector(DetailRandom.FRandRange(300,1700),DetailRandom.FRandRange(-1000,1100),-59.8f),FVector(DetailRandom.FRandRange(20,100),1.2f,.1f),TEXT("Sky"),nullptr,false);
    auto* HarborFog=World->SpawnActor<AExponentialHeightFog>();
    HarborFog->GetComponent()->SetFogDensity(.018f);
    HarborFog->GetComponent()->SetStartDistance(1000);
    HarborFog->GetComponent()->SetFogInscatteringColor(FLinearColor(.38f,.48f,.53f));
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
    UE_LOG(LogTemp,Display,TEXT("CHUCK: docks ready; Chuck 65 cm, human 180 cm; two cameras available."));
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
        Check(FMath::IsNearlyEqual(Chuck->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()*2,65.f,.01f),TEXT("Chuck collision height is 65 cm"));
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
            Check(MaxJumpZ>48,TEXT("jump lifts Chuck above floor"));
            Check(Chuck->GetCharacterMovement()->IsMovingOnGround(),TEXT("jump lands back on quay"));
            Chuck->GetCharacterMovement()->StopMovementImmediately();
            Chuck->SetActorLocation(FVector(-395,0,36)); TestStage=3; StageTime=0;
        }
    }
    else if(TestStage==3)
    {
        Chuck->AddMovementInput(FVector(-1,0,0),1);
        if(StageTime>1)
        {
            Check(Chuck->GetActorLocation().X > -440,TEXT("warehouse wall blocks walking"));
            Check(Chuck->FindComponentByClass<UCameraComponent>()->GetComponentLocation().X > -441, TEXT("camera retracts before warehouse wall"));
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
        Check(FMath::IsNearlyEqual(Chuck->FindComponentByClass<USpringArmComponent>()->TargetArmLength,220.f,1.f),TEXT("rat-height camera blend settles"));
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
            Check(FMath::IsNearlyEqual(Chuck->FindComponentByClass<USpringArmComponent>()->TargetArmLength,400.f,1.f),TEXT("elevated camera blend settles"));
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
            Check(MaxJumpZ>48,TEXT("Xbox A mapping jumps"));
            Cast<APlayerController>(Chuck->GetController())->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_FaceButton_Bottom,IE_Released,0));
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(465,0,36));
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
            { Chuck->ResetToDock(); Chuck->ToggleCamera(); Chuck->SetActorLocation(FVector(465,0,36)); TestStage=8; StageTime=0; }
            else if(FParse::Param(FCommandLine::Get(),TEXT("ChuckCapture")))
            {
                Chuck->ResetToDock(); Chuck->ToggleCamera(); Chuck->SetActorLocation(FVector(20,130,36));
                Chuck->SetActorRotation(FRotator(0,60,0));
                Chuck->Recenter();
                auto* CapturePC=Cast<APlayerController>(Chuck->GetController());
                CapturePC->FlushPressedKeys();
                Chuck->DisableInput(CapturePC);
                TestStage=20; StageTime=0;
            }
            else TestStage=99;
        }
    }
    else if(TestStage==20 && StageTime>2)
    {
        Check(FMath::Abs(FMath::FindDeltaAngleDegrees(Chuck->FindComponentByClass<UCameraComponent>()->GetComponentRotation().Yaw,60.f))<1.f,TEXT("camera recenters behind Chuck"));
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
    DrawText(TEXT("65 cm rat  /  180 cm dock worker"),FLinearColor(.7f,.73f,.76f),30,76,GEngine->GetSmallFont());
    DrawRect(FLinearColor(0.035f,0.04f,0.045f,0.85f),18,Canvas->SizeY-65,Canvas->SizeX-36,47);
    DrawText(TEXT("WASD / Left stick: walk    Space / A: jump    C / Y: camera    Mouse, Q/E / Right stick: turn"),FLinearColor(.91f,.9f,.85f),30,Canvas->SizeY-58,GEngine->GetSmallFont());
    DrawText(TEXT("F / R-stick click: center    R / View: reset    Esc / Menu: exit    Compare the same route in both cameras."),FLinearColor(.75f,.77f,.8f),30,Canvas->SizeY-37,GEngine->GetSmallFont());
}
