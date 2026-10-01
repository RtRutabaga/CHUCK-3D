#include "DockGameMode.h"
#include "DockSetting.h"
#include "DockPlaza.h"
#include "GrassTuft.h"
#include "ClayJar.h"
#include "CigarettePickup.h"
#include "ClayJarData.h"
#include "EngineUtils.h"
#include "EnemyRat.h"
#include "AstralSummon.h"
#include "DockNPC.h"
#include "Components/SkinnedMeshComponent.h"

#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"
#include "ChuckCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ChuckAnimInstance.h"
#include "ChuckClipData.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
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
#include "HAL/FileManager.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "DynamicRHI.h"

ADockGameMode::ADockGameMode()
{
    DefaultPawnClass = AChuckCharacter::StaticClass();
    HUDClass = ADockHUD::StaticClass();
    PrimaryActorTick.bCanEverTick = true;
}

void ADockGameMode::StartPlay()
{
    UWorld* World = GetWorld();
    // Non-spatial background music, unaffected by Chuck's position or resets.
    auto* Music=LoadObject<USoundWave>(nullptr,TEXT("/Game/Art/Audio/SW_WaterdeepDocks.SW_WaterdeepDocks"));
    const bool TestMusic=FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest"));
    auto* MusicComponent=Music ? UGameplayStatics::CreateSound2D(World,Music,1.f,1.f,0.f,nullptr,false,true) : nullptr;
    if(MusicComponent)
    {
        // Smoke runs start just before the end to exercise looping without a long wait.
        const float Start=TestMusic ? FMath::Max(0.f,Music->Duration-2.f) : 0.f;
        MusicComponent->FadeIn(1.5f,.45f,Start);
    }
    if(TestMusic)
    {
        FTimerHandle MusicCheck;
        TWeakObjectPtr<UAudioComponent> WeakMusic=MusicComponent;
        const bool LoopConfigured=Music && Music->bLooping;
        World->GetTimerManager().SetTimer(MusicCheck,[WeakMusic,LoopConfigured]()
        {
            const bool Playing=WeakMusic.IsValid() && WeakMusic->IsPlaying();
            UE_LOG(LogTemp,Display,TEXT("CHUCK_MUSIC_CHECK failures=%d looping=%d playing_after_boundary=%d"),
                LoopConfigured && Playing ? 0 : 1,LoopConfigured,Playing);
        },6.f,false);
    }
    auto* Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Sphere = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Cylinder = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* BarrelMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_DockBarrel.SM_DockBarrel"));
    auto* CrateMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_DockCrate.SM_DockCrate"));
    auto* PlankMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_DockPlank.SM_DockPlank"));
    auto* BoatMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_HarborBoat.SM_HarborBoat"));
    auto* RopeMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_RopeCoil.SM_RopeCoil"));
    auto* WorkerMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_DockWorker.SM_DockWorker"));
    auto* BenchMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_TavernBench.SM_TavernBench"));
    auto* WindowMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_TavernWindow.SM_TavernWindow"));
    auto Prop = [&](const TCHAR* Name,FVector Position,UStaticMesh* Asset)
    {
        auto* Actor=World->SpawnActor<AStaticMeshActor>(Position,FRotator::ZeroRotator);
        Actor->Tags.Add(FName(Name));
        Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Actor->GetStaticMeshComponent()->SetStaticMesh(Asset);
        Actor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return Actor;
    };
    auto Material = [](const TCHAR* Name) {
        if(auto* Art=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),Name,Name))) return Art;
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
        if(WorkerMesh && (FString(Name).StartsWith(TEXT("Human")) || FString(Name).StartsWith(TEXT("Worker"))))
            Actor->SetActorHiddenInGame(true);
        if((BenchMesh && FString(Name).StartsWith(TEXT("Bench"))) || (WindowMesh && FString(Name).StartsWith(TEXT("Window"))))
            Actor->SetActorHiddenInGame(true);
        return Actor;
    };
    // Units are centimetres. Ground top = 0; geometry is intentionally simple.
    Shape(TEXT("Quay"),FVector(-150,0,-20),FVector(700,800,40),TEXT("Stone"));
    Shape(TEXT("Sea"),FVector(900,0,-65),FVector(18000,18000,10),TEXT("Water"),nullptr,false);
    // Short pier with a 24 cm missing board. Chuck's jump travels about 41 cm.
    for(int32 Row=0;Row<26;++Row)
    {
        if(Row==13) continue;
        auto* Collision=Shape(TEXT("DockPlank"),FVector(212+Row*24,0,-6),FVector(23,180,12),Row%2 ? TEXT("Wood") : TEXT("WoodLight"));
        if(PlankMesh) { Collision->SetActorHiddenInGame(true); Prop(TEXT("DockPlankArt"),Collision->GetActorLocation(),PlankMesh); }
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
        if(WindowMesh) Prop(TEXT("TavernWindowArt"),FVector(X,322,160),WindowMesh)->SetActorRotation(FRotator(0,180,0));
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
    auto* BarrelCollision=Shape(TEXT("Barrel"),FVector(-330,-80,45),FVector(62,62,90),TEXT("Wood"),Cylinder);
    if(BarrelMesh) { BarrelCollision->SetActorHiddenInGame(true); Prop(TEXT("DockBarrelArt"),BarrelCollision->GetActorLocation(),BarrelMesh); }
    else for(float Z : {14.f,72.f}) Shape(TEXT("BarrelBand"),FVector(-330,-80,Z),FVector(65,65,7),TEXT("Dark"),Cylinder);
    auto* CrateCollision=Shape(TEXT("Crate"),FVector(-80,60,30),FVector(60,65,60),TEXT("WoodLight"));
    if(CrateMesh) { CrateCollision->SetActorHiddenInGame(true); Prop(TEXT("DockCrateArt"),CrateCollision->GetActorLocation(),CrateMesh); }
    Shape(TEXT("LowStep"),FVector(-40,-155,5),FVector(60,65,10),TEXT("Wood"));
    // Parkour practice yard (user request 2026-09-28), on the quay's empty south
    // strip, dock-built so it can be dressed later and kept: a cargo chimney of
    // two 240 cm crate stacks 100 cm apart (wall runs, wall jumps back and
    // forth) and a 115 cm stone harbour wall with a walkable top (ledge grabs).
    for(const float X : {-390.f,-230.f})
    {
        auto* Stack=Shape(TEXT("CargoStack"),FVector(X,-337.5f,120),FVector(60,65,240),TEXT("WoodLight"));
        if(CrateMesh)
        {
            Stack->SetActorHiddenInGame(true);
            for(int32 Level=0;Level<4;++Level)
                Prop(TEXT("CargoCrateArt"),FVector(X,-337.5f,30.f+60.f*Level),CrateMesh)->SetActorRotation(FRotator(0,Level%2 ? 2.f : -1.5f,0));
        }
    }
    Shape(TEXT("HarbourWall"),FVector(-40,-360,57.5f),FVector(180,50,115),TEXT("Stone"));
    Shape(TEXT("HarbourCoping"),FVector(-40,-360,113),FVector(184,54,6),TEXT("Stone"),nullptr,false);
    // Its east end returns north (an L): an inside corner to shimmy round.
    Shape(TEXT("HarbourWallReturn"),FVector(75,-295,57.5f),FVector(50,80,115),TEXT("Stone"));
    Shape(TEXT("HarbourCoping"),FVector(75,-295,113),FVector(54,84,6),TEXT("Stone"),nullptr,false);
    if(RopeMesh) Prop(TEXT("HarbourRopeArt"),FVector(20,-368,116),RopeMesh);
    // A knee-high stone mooring plinth (30 cm): walk into it and Chuck mantles up.
    Shape(TEXT("MooringPlinth"),FVector(150,-330,15),FVector(60,60,30),TEXT("Stone"));
    Shape(TEXT("MooringRing"),FVector(150,-299,18),FVector(8,3,8),TEXT("Dark"),Cylinder,false);
    // Cargo wharf (user request 2026-09-29: "a bigger obstacle area"): a stone
    // quay extension south of the yard, laid out as one connected parkour
    // course in dock materials (collision-true primitives, to be dressed later):
    //  - a crate staircase up the west edge (30/60/120/180/230 cm: mantles and
    //    jump-and-grab steps) with a plank bridge to the warehouse roof;
    //  - a timber-and-plaster warehouse (230 cm, flat roof) on a stone plinth
    //    that juts 40 cm at 115 cm: a long ledge to grab, shimmy and stand on;
    //  - a 100 cm alley between it and a 260 cm sail loft: a second chimney;
    //  - a knee-high field of harbour walls, bollards, barrels and crates;
    //  - boats moored off the wharf edge.
    Shape(TEXT("Wharf"),FVector(-100,-700,-20),FVector(800,600,40),TEXT("Stone"));
    for(const float X : {-500.f,300.f}) Shape(TEXT("WharfEdgeBeam"),FVector(X,-700,1),FVector(12,600,4),TEXT("Wood"),nullptr,false);
    Shape(TEXT("WharfEdgeBeam"),FVector(-100,-1000,1),FVector(812,12,4),TEXT("Wood"),nullptr,false);
    auto CrateColumn = [&](const TCHAR* Name,float X,float Y,float Height)
    {
        auto* Column=Shape(Name,FVector(X,Y,Height*.5f),FVector(60,65,Height),TEXT("WoodLight"));
        if(!CrateMesh) return;
        Column->SetActorHiddenInGame(true);
        for(float Z=0; Z<Height-1.f; Z+=60.f)
        {
            const float Part=FMath::Min(60.f,Height-Z);
            auto* Art=Prop(TEXT("WharfCrateArt"),FVector(X,Y,Z+Part*.5f),CrateMesh);
            Art->SetActorScale3D(FVector(1,1,Part/60.f));
            Art->SetActorRotation(FRotator(0,FMath::RoundToInt(Z/60.f)%2 ? 1.5f : -1.f,0));
        }
    };
    // Crate staircase, north to south along the west edge.
    {
        const float Heights[]={30.f,60.f,120.f,180.f,230.f};
        for(int32 Step=0; Step<5; ++Step) CrateColumn(TEXT("CrateStair"),-440,-462.5f-65.f*Step,Heights[Step]);
        Shape(TEXT("PlankBridge"),FVector(-370,-722.5f,227),FVector(80,40,6),TEXT("Wood"));
        if(RopeMesh) Prop(TEXT("WharfRopeArt"),FVector(-440,-722,230),RopeMesh);
    }
    // Warehouse on its stone plinth.
    Shape(TEXT("WharfWarehouse"),FVector(-180,-790,115),FVector(300,220,230),TEXT("Plaster"));
    Shape(TEXT("WarehousePlinth"),FVector(-180,-660,57.5f),FVector(300,40,115),TEXT("Stone"));
    Shape(TEXT("WarehouseRoofTrim"),FVector(-180,-790,231),FVector(306,226,4),TEXT("Dark"),nullptr,false);
    for(const float X : {-328.f,-32.f}) for(const float Y : {-682.f,-898.f})
        Shape(TEXT("WarehouseTimber"),FVector(X,Y,115),FVector(8,8,230),TEXT("Wood"),nullptr,false);
    Shape(TEXT("WarehouseBeam"),FVector(-180,-679,170),FVector(300,3,8),TEXT("Wood"),nullptr,false);
    for(const float X : {-280.f,-180.f,-80.f})
    {
        Shape(TEXT("WarehouseWindowFrame"),FVector(X,-679.5f,195),FVector(42,2,40),TEXT("Wood"),nullptr,false);
        Shape(TEXT("WarehouseWindow"),FVector(X,-679,195),FVector(34,2,32),TEXT("Amber"),nullptr,false);
    }
    Shape(TEXT("WarehouseDoor"),FVector(-180,-900.5f,70),FVector(90,2,140),TEXT("Wood"),nullptr,false);
    if(RopeMesh) Prop(TEXT("WharfRopeArt"),FVector(-100,-660,115),RopeMesh);
    // Sail loft across the alley.
    Shape(TEXT("SailLoft"),FVector(170,-770,130),FVector(200,260,260),TEXT("Plaster"));
    Shape(TEXT("SailLoftRoofTrim"),FVector(170,-770,261),FVector(206,266,4),TEXT("Dark"),nullptr,false);
    for(const float X : {72.f,268.f}) for(const float Y : {-642.f,-898.f})
        Shape(TEXT("SailLoftTimber"),FVector(X,Y,130),FVector(8,8,260),TEXT("Wood"),nullptr,false);
    Shape(TEXT("SailLoftHoistBeam"),FVector(170,-630,250),FVector(10,30,10),TEXT("Wood"),nullptr,false);
    Shape(TEXT("SailLoftDoor"),FVector(170,-639.5f,150),FVector(70,2,110),TEXT("Wood"),nullptr,false);
    // Knee-high field: harbour walls, bollards, barrels, a crate.
    for(const FVector& Wall : {FVector(-380,-950,17.5f),FVector(-140,-965,17.5f),FVector(120,-950,17.5f)})
    {
        Shape(TEXT("HarbourLowWall"),Wall,FVector(120,30,35),TEXT("Stone"));
        Shape(TEXT("HarbourLowCoping"),Wall+FVector(0,0,18.5f),FVector(124,34,3),TEXT("Stone"),nullptr,false);
    }
    for(const float X : {-460.f,-260.f,-20.f,230.f})
        Shape(TEXT("WharfBollard"),FVector(X,-985,17.5f),FVector(22,22,35),TEXT("Dark"),Cylinder);
    for(const FVector& Spot : {FVector(-270,-935,45),FVector(-20,-935,45)})
    {
        auto* Barrel=Shape(TEXT("WharfBarrel"),Spot,FVector(62,62,90),TEXT("Wood"),Cylinder);
        if(BarrelMesh) { Barrel->SetActorHiddenInGame(true); Prop(TEXT("WharfBarrelArt"),Spot,BarrelMesh); }
    }
    CrateColumn(TEXT("WharfCrate"),240,-930,60);
    if(BoatMesh)
    {
        Prop(TEXT("WharfBoatArt"),FVector(-100,-1960,-60),BoatMesh)->SetActorRotation(FRotator(0,90,0));
        Prop(TEXT("WharfBoatArt"),FVector(1030,-800,-60),BoatMesh);
    }
    // Second expansion (user 2026-09-29: "expand the area more"): two more
    // districts on new quay slabs, same dock materials and collision-true
    // primitives.
    // Chandlers' Row, south of the wharf (X -500..300, Y -1800..-1000).
    Shape(TEXT("RowQuay"),FVector(-100,-1400,-20),FVector(800,800,40),TEXT("Stone"));
    Shape(TEXT("WharfEdgeBeam"),FVector(-100,-1800,1),FVector(812,12,4),TEXT("Wood"),nullptr,false);
    auto House = [&](const TCHAR* Name,FVector Center,FVector Size,const TCHAR* Wall)
    {
        Shape(Name,Center+FVector(0,0,Size.Z*.5f),Size,Wall);
        Shape(TEXT("HouseRoofTrim"),Center+FVector(0,0,Size.Z+1),Size+FVector(6,6,-Size.Z+4),TEXT("Dark"),nullptr,false);
        for(const float SX : {-1.f,1.f}) for(const float SY : {-1.f,1.f})
            Shape(TEXT("HouseTimber"),Center+FVector(SX*(Size.X*.5f-2),SY*(Size.Y*.5f-2),Size.Z*.5f),FVector(8,8,Size.Z),TEXT("Wood"),nullptr,false);
        for(float Z=60; Z<Size.Z-40; Z+=80)
        {
            Shape(TEXT("HouseWindow"),Center+FVector(Size.X*.5f+.5f,-Size.Y*.25f,Z),FVector(2,34,30),TEXT("Amber"),nullptr,false);
            Shape(TEXT("HouseWindow"),Center+FVector(Size.X*.5f+.5f,Size.Y*.25f,Z),FVector(2,34,30),TEXT("Amber"),nullptr,false);
        }
        Shape(TEXT("HouseDoor"),Center+FVector(Size.X*.5f+.5f,0,55),FVector(2,60,110),TEXT("Wood"),nullptr,false);
    };
    // A rooftop row stepping up south: leap the 70 cm gaps, catch the next roof.
    House(TEXT("RowHouse"),FVector(-370,-1150,0),FVector(220,180,180),TEXT("Plaster"));
    House(TEXT("RowHouse"),FVector(-370,-1400,0),FVector(220,180,230),TEXT("Stone"));
    House(TEXT("RowHouse"),FVector(-370,-1650,0),FVector(220,180,280),TEXT("Plaster"));
    Shape(TEXT("LeanToShed"),FVector(-420,-1040,50),FVector(120,40,100),TEXT("Wood"));
    Shape(TEXT("LeanToRoof"),FVector(-420,-1040,101),FVector(126,46,3),TEXT("Roof"),nullptr,false);
    // Market stalls: tables to hop onto, canopies overhead (no collision).
    for(const FVector& Stall : {FVector(-100,-1150,0),FVector(50,-1150,0),FVector(-100,-1330,0),FVector(50,-1330,0)})
    {
        Shape(TEXT("MarketStall"),Stall+FVector(0,0,40),FVector(120,60,80),TEXT("WoodLight"));
        for(const float SX : {-1.f,1.f}) Shape(TEXT("MarketPost"),Stall+FVector(SX*56,-26,95),FVector(5,5,190),TEXT("Wood"),nullptr,false);
        Shape(TEXT("MarketCanopy"),Stall+FVector(0,-10,190),FVector(130,80,3),TEXT("Roof"),nullptr,false);
    }
    // A long garden wall with a narrow walkable top, reached from a crate.
    Shape(TEXT("GardenWall"),FVector(200,-1400,60),FVector(30,600,120),TEXT("Stone"));
    Shape(TEXT("GardenCoping"),FVector(200,-1400,121),FVector(34,604,3),TEXT("Stone"),nullptr,false);
    CrateColumn(TEXT("GardenCrate"),200,-1060,60);
    // Customs terrace with a ramp up from the market (30 degrees).
    Shape(TEXT("CustomsTerrace"),FVector(-25,-1690,45),FVector(350,180,90),TEXT("Stone"));
    for(float X=-190; X<=140; X+=55) Shape(TEXT("TerracePost"),FVector(X,-1776,105),FVector(6,6,30),TEXT("Wood"),nullptr,false);
    {
        const float Rise=90.f, Run=Rise/FMath::Tan(FMath::DegreesToRadians(30.f)), Length=FMath::Sqrt(Rise*Rise+Run*Run);
        const FVector Up=FVector(0,-Run,Rise).GetSafeNormal();       // along the ramp, rising south
        const FVector Normal=FVector(0,Rise,Run).GetSafeNormal();    // its top surface normal
        const FVector TopMid(-25,-1600+Run*.5f,Rise*.5f);
        auto* Ramp=Shape(TEXT("TerraceRamp"),TopMid-Normal*5.f,FVector(120,Length,10),TEXT("Stone"));
        Ramp->SetActorRotation(FRotationMatrix::MakeFromYZ(-Up,Normal).Rotator());
    }
    // Timber Yard, east of the wharf (X 300..900, Y -1000..-400).
    Shape(TEXT("TimberQuay"),FVector(600,-700,-20),FVector(600,600,40),TEXT("Stone"));
    Shape(TEXT("WharfEdgeBeam"),FVector(900,-700,1),FVector(12,600,4),TEXT("Wood"),nullptr,false);
    auto Lumber = [&](float Y,float Height)
    {
        Shape(TEXT("LumberStack"),FVector(510,Y,Height*.5f),FVector(300,60,Height),TEXT("Wood"));
        for(float Z=12; Z<Height; Z+=24) Shape(TEXT("LumberBand"),FVector(510,Y,Z),FVector(302,62,3),TEXT("WoodLight"),nullptr,false);
    };
    Lumber(-490,160); Lumber(-650,200); Lumber(-830,90);   // the first two leave a 100 cm chimney
    Shape(TEXT("CraneTower"),FVector(800,-620,125),FVector(120,120,250),TEXT("Wood"));
    for(const float SX : {-1.f,1.f}) for(const float SY : {-1.f,1.f})
        Shape(TEXT("CraneLeg"),FVector(800+SX*58,-620+SY*58,125),FVector(10,10,250),TEXT("Dark"),nullptr,false);
    Shape(TEXT("CraneArm"),FVector(1000,-620,262),FVector(320,14,14),TEXT("Wood"),nullptr,false);
    Shape(TEXT("CraneRope"),FVector(1140,-620,195),FVector(2,2,120),TEXT("Dark"),nullptr,false);
    if(CrateMesh) Prop(TEXT("CraneCrateArt"),FVector(1140,-620,105),CrateMesh);
    for(const FVector& Spot : {FVector(720,-890,45),FVector(800,-900,45)})
    {
        auto* Barrel=Shape(TEXT("YardBarrel"),Spot,FVector(62,62,90),TEXT("Wood"),Cylinder);
        if(BarrelMesh) { Barrel->SetActorHiddenInGame(true); Prop(TEXT("YardBarrelArt"),Spot,BarrelMesh); }
    }
    Shape(TEXT("HandCart"),FVector(450,-945,40),FVector(120,70,40),TEXT("WoodLight"));
    for(const float SX : {-1.f,1.f}) Shape(TEXT("CartWheel"),FVector(450+SX*40,-908,20),FVector(40,6,40),TEXT("Dark"),Cylinder,false);
    Shape(TEXT("BenchTop"),FVector(-210,225,45),FVector(160,42,8),TEXT("WoodLight"));
    for(float X : {-275.f,-145.f}) Shape(TEXT("BenchLeg"),FVector(X,225,21),FVector(12,32,42),TEXT("Wood"));
    if(BenchMesh) Prop(TEXT("TavernBenchArt"),FVector(-210,225,0),BenchMesh);
    // The dock worker by the spawn (user 2026-09-30): a rigged, procedurally
    // posed NPC (ADockNPC) replacing the old static 180 cm scale figure, on the
    // same spot and facing the same way. Ambient: no dialogue yet (user's call).
    const FVector Human(90,200,0);
    ADockNPC::SpawnDockWorker(World,Human,-90.f);
    // The 2D game's guard (at the closed city gate) and market woman (by the red stalls).
    ADockNPC::SpawnTownsfolk(World);
    // Surface detail is nonblocking; the original simple collision remains predictable.
    FRandomStream DetailRandom(73);
    // Scanned paving supplies irregular joints without a second rectangular overlay.
    for(int32 Row=0;!PlankMesh && Row<26;++Row)
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
    for(int32 Band=0;!BarrelMesh && Band<12;++Band)
    {
        const float Angle=Band*PI/6;
        Shape(TEXT("BarrelStave"),FVector(-330+30*FMath::Cos(Angle),-80+30*FMath::Sin(Angle),44),FVector(3,3,84),TEXT("WoodLight"),Cylinder,false);
    }
    if(!CrateMesh)
    {
    for(float Z : {7.f,53.f})
        Shape(TEXT("CrateFrame"),FVector(-80,26,Z),FVector(64,5,6),TEXT("Wood"),nullptr,false);
    for(float X : {-107.f,-53.f})
        Shape(TEXT("CrateFrame"),FVector(X,26,30),FVector(6,5,60),TEXT("Wood"),nullptr,false);
    auto* Brace=Shape(TEXT("CrateBrace"),FVector(-80,23,30),FVector(73,4,5),TEXT("Wood"),nullptr,false);
    Brace->SetActorRotation(FRotator(40,0,0));
    }
    // A quiet harbor silhouette beyond the playable dock; no additional map.
    for(int32 Building=0;Building<11;++Building)
    {
        const float Height=DetailRandom.FRandRange(280,580);
        Shape(TEXT("FarWarehouse"),FVector(3500,-2200+Building*420,Height*.5f-55),FVector(250,245,Height),Building%2 ? TEXT("Roof") : TEXT("Stone"),nullptr,false);
        Shape(TEXT("FarRoof"),FVector(3500,-2200+Building*420,Height-40),FVector(275,270,25),TEXT("Dark"),nullptr,false);
        const float CenterY=-2200+Building*420;
        for(float Side : {-1.f,1.f})
        {
            auto* Slope=Shape(TEXT("FarPitchedRoof"),FVector(3500,CenterY+Side*65,Height-8),FVector(280,164,12),TEXT("Roof"),nullptr,false);
            Slope->SetActorRotation(FRotator(0,0,Side*32));
        }
        Shape(TEXT("FarChimney"),FVector(3540,CenterY+70,Height+38),FVector(32,34,115),TEXT("Stone"),nullptr,false);
        for(float Z=90; Z<Height-80; Z+=85) for(float Offset : {-65.f,0.f,65.f})
            Shape(TEXT("FarWindow"),FVector(3373,CenterY+Offset,Z),FVector(3,27,43),TEXT("Dark"),nullptr,false);
    }
    Shape(TEXT("FarHarborWall"),FVector(3440,0,-8),FVector(170,4850,94),TEXT("Stone"),nullptr,false);
    if(BoatMesh) Prop(TEXT("HarborBoatArt"),FVector(980,600,-60),BoatMesh);
    if(RopeMesh) Prop(TEXT("RopeCoilArt"),FVector(425,56,0),RopeMesh);
    BuildDockSetting(World);
    BuildDockPlaza(World);
    // Shreddable grass tufts (after all collision exists: planted by ground traces).
    AGrassTuft::SpawnDockGrass(World);
    AClayJar::SpawnDockJars(World);
    // Rats roam in play; the smoke test places its own so none wander into other checks.
    if(!FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest"))) AEnemyRat::SpawnDockRats(World);
    // Animated opaque wave normals now replace the old geometric ripple strips.
    auto* HarborFog=World->SpawnActor<AExponentialHeightFog>();
    HarborFog->GetComponent()->SetFogDensity(.018f);
    HarborFog->GetComponent()->SetStartDistance(1000);
    HarborFog->GetComponent()->SetFogInscatteringColor(FLinearColor(.28f,.31f,.42f));
    auto* Sun = World->SpawnActor<ADirectionalLight>(FVector(0,0,500),FRotator(-6,-40,0));
    Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sun->GetLightComponent()->SetIntensity(1.35f);
    Sun->GetLightComponent()->SetLightColor(FLinearColor(1,.59f,.36f));
    Sun->GetLightComponent()->ContactShadowLength=8.f;
    Sun->GetLightComponent()->ContactShadowLengthInWS=true;
    auto* Post=World->SpawnActor<APostProcessVolume>();
    Post->bUnbound=true;
    Post->Settings.bOverride_AmbientOcclusionIntensity=true;
    Post->Settings.AmbientOcclusionIntensity=.65f;
    Post->Settings.bOverride_AmbientOcclusionRadius=true;
    Post->Settings.AmbientOcclusionRadius=35.f;
    Post->Settings.bOverride_AmbientOcclusionRadiusInWS=true;
    Post->Settings.AmbientOcclusionRadiusInWS=true;
    Post->Settings.bOverride_ScreenSpaceReflectionIntensity=true;
    Post->Settings.ScreenSpaceReflectionIntensity=75.f;
    Post->Settings.bOverride_ScreenSpaceReflectionQuality=true;
    Post->Settings.ScreenSpaceReflectionQuality=50.f;
    auto* Sky = World->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(1.05f);
    // The enclosing sky is 90 m away, below UE's default 1500 m sky threshold.
    // Capture it as ambient light so the shaded sides remain readable at rat height.
    Sky->GetLightComponent()->SkyDistanceThreshold = 1000;
    Sky->GetLightComponent()->bLowerHemisphereIsBlack = false;
    // A pale enclosing sphere gives skylight capture a quiet flat horizon.
    auto* Horizon = Shape(TEXT("Horizon"),FVector(0,0,0),FVector(18000,18000,18000),TEXT("DawnSky"),Sphere,false);
    Horizon->GetStaticMeshComponent()->SetCastShadow(false);
    Sky->GetLightComponent()->RecaptureSky();
    auto* Start = World->SpawnActor<APlayerStart>(AChuckCharacter::StartLocation(),FRotator::ZeroRotator);
    (void)Start;
    Super::StartPlay();
    if(auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
    { Chuck->ResetToDock(); AddTickPrerequisiteActor(Chuck); }
    UE_LOG(LogTemp,Display,TEXT("CHUCK: docks ready; Chuck 65 cm, human 180 cm; two cameras available."));
    bSmokeTest = FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest"));
    bNPCCapture = FParse::Param(FCommandLine::Get(),TEXT("ChuckNPCCapture"));
}

void ADockGameMode::TickNPCCapture(float DeltaSeconds)
{
    struct FShot { const TCHAR* Name; float Distance, Yaw, Height, Aim, Fov; };
    static const FShot Shots[] = {
        {TEXT("Front"),320.f,0.f,100.f,92.f,62.f}, {TEXT("ThreeQuarter"),320.f,40.f,100.f,92.f,62.f},
        {TEXT("Back"),190.f,180.f,100.f,92.f,80.f}, {TEXT("Face"),80.f,25.f,166.f,162.f,40.f} };
    constexpr int32 ShotCount=UE_ARRAY_COUNT(Shots);
    constexpr float Settle=3.f, Each=1.2f;
    NPCCaptureTime+=DeltaSeconds;
    APlayerController* PC=GetWorld()->GetFirstPlayerController();
    if(APawn* Chuck=UGameplayStatics::GetPlayerPawn(this,0)) Chuck->SetActorHiddenInGame(true);
    const TArray<TWeakObjectPtr<ADockNPC>>& NPCs=ADockNPC::All();
    if(NPCCaptureTime<Settle || !PC) return;
    const int32 Step=FMath::FloorToInt((NPCCaptureTime-Settle)/Each);
    if(Step>=NPCs.Num()*ShotCount) { FPlatformMisc::RequestExit(false); return; }
    const ADockNPC* NPC=NPCs[Step/ShotCount].Get();
    const FShot& Shot=Shots[Step%ShotCount];
    if(!NPC) return;
    if(Step!=NPCShot)
    {
        if(!NPCCamera.IsValid()) NPCCamera=GetWorld()->SpawnActor<ACameraActor>();
        const FVector Feet=NPC->GetActorLocation()-FVector(0,0,NPC->GetSimpleCollisionHalfHeight());
        const FVector Dir=NPC->GetActorForwardVector().RotateAngleAxis(Shot.Yaw,FVector::UpVector);
        // The face shot is aimed from each body's own eye height (a 166 cm woman, a 183 cm guard).
        const float Lift=Shot.Distance<100.f ? NPC->GetEyeHeight()-167.f : 0.f;
        const FVector At=Feet+Dir*Shot.Distance+FVector(0,0,Shot.Height+Lift);
        NPCCamera->SetActorLocationAndRotation(At,(Feet+FVector(0,0,Shot.Aim+Lift)-At).Rotation());
        NPCCamera->GetCameraComponent()->SetFieldOfView(Shot.Fov);
        PC->SetViewTarget(NPCCamera.Get());
        NPCShot=Step; bNPCShotTaken=false;
    }
    else if(!bNPCShotTaken && NPCCaptureTime-Settle-Step*Each>.8f)
    {
        const FString Name=(NPC->DisplayName.IsEmpty() ? NPC->GetName() : NPC->DisplayName).Replace(TEXT(" "),TEXT(""));
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/NPC_%s_%s.png"),*Name,Shot.Name),false,false);
        bNPCShotTaken=true;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_NPC_SHOT %s look=(%.1f,%.1f) watching=%d turn=%.1f mocap=%d"),*Name,NPC->GetLookAngles().X,NPC->GetLookAngles().Y,NPC->IsWatchingChuck()?1:0,NPC->GetBodyTurn(),NPC->HasMocap()?1:0);
    }
}

void ADockGameMode::Check(bool Passed,const TCHAR* Description)
{
    if(!Passed) ++TestFailures;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_TEST %s: %s"),Passed ? TEXT("PASS") : TEXT("FAIL"),Description);
}

void ADockGameMode::ProbeLockedPaws(AChuckCharacter* Chuck,float DeltaSeconds)
{
    // World speed of paws the pose holds locked in stance (settle steps excluded).
    const FChuckAnimResult Pose=Chuck->GetChuckAnim() ? Chuck->GetChuckAnim()->GetResult() : FChuckAnimResult();
    if(Pose.Evaluations==LocoEvaluations) return;
    for(int32 I=0; I<2; ++I)
    {
        const bool bLocked=Pose.LockAlpha[I]>=1.f && !Pose.bSettling[I];
        if(LocoEvaluations>=0 && bLocoLocked[I] && bLocked)
        {
            LocoMaxSlip=FMath::Max(LocoMaxSlip,static_cast<float>(FVector::Dist(Pose.BallWorld[I],LocoFoot[I]))/FMath::Max(DeltaSeconds,.001f));
            ++LocoSamples;
        }
        LocoFoot[I]=Pose.BallWorld[I]; bLocoLocked[I]=bLocked;
    }
    LocoEvaluations=Pose.Evaluations;
}

void ADockGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(bNPCCapture) { TickNPCCapture(DeltaSeconds); return; }
    if(!bSmokeTest) return;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Chuck) { Check(false,TEXT("player spawned")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    StageTime += DeltaSeconds;
    if(TestStage==0 && StageTime>1)
    {
        Check(Chuck->GetCharacterMovement()->IsMovingOnGround(),TEXT("spawn settles on quay"));
        for(const TCHAR* Tag : {TEXT("DockBarrelArt"),TEXT("DockCrateArt"),TEXT("DockPlankArt"),TEXT("HarborBoatArt"),TEXT("RopeCoilArt"),TEXT("DockWorkerArt"),TEXT("TavernBenchArt"),TEXT("TavernWindowArt")})
        {
            TArray<AActor*> Props;
            UGameplayStatics::GetAllActorsWithTag(this,FName(Tag),Props);
            const int32 Expected=FString(Tag)==TEXT("DockPlankArt") ? 25 : (FString(Tag)==TEXT("TavernWindowArt") ? 2 : 1);
            bool bValid=Props.Num()==Expected;
            for(AActor* Actor:Props)
            {
                if(FString(Tag)==TEXT("DockWorkerArt"))
                {
                    // The worker is a rigged NPC now: a skinned body, its own blocker.
                    const auto* Skin=Actor->FindComponentByClass<USkinnedMeshComponent>();
                    bValid &= Skin && Skin->GetSkinnedAsset() && Skin->GetCollisionEnabled()==ECollisionEnabled::NoCollision;
                    continue;
                }
                const auto* Component=Actor->FindComponentByClass<UStaticMeshComponent>();
                bValid &= Component && Component->GetStaticMesh() && Component->GetCollisionEnabled()==ECollisionEnabled::NoCollision;
            }
            Check(bValid,*FString::Printf(TEXT("%s imports render separately from collision"),Tag));
        }
        FHitResult PropHit;
        bool bHit=GetWorld()->LineTraceSingleByChannel(PropHit,FVector(-400,-80,45),FVector(-270,-80,45),ECC_Visibility);
        Check(bHit && PropHit.GetActor() && PropHit.GetActor()->ActorHasTag(TEXT("Barrel")),TEXT("hidden barrel proxy still blocks collision"));
        bHit=GetWorld()->LineTraceSingleByChannel(PropHit,FVector(-150,60,30),FVector(-10,60,30),ECC_Visibility);
        Check(bHit && PropHit.GetActor() && PropHit.GetActor()->ActorHasTag(TEXT("Crate")),TEXT("hidden crate proxy still blocks collision"));
        Check(FMath::IsNearlyEqual(Chuck->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()*2,65.f,.01f),TEXT("Chuck collision height is 65 cm"));
        auto* Body=Chuck->GetMesh();
        auto* Rig=Body ? Body->GetSkeletalMeshAsset() : nullptr;
        const bool bNoGroom=FParse::Param(FCommandLine::Get(),TEXT("ChuckNoGroom"));
        Check(Rig && Rig->GetName()==(bNoGroom ? TEXT("SK_Chuck") : TEXT("SK_Chuck_Groomed")) && Chuck->GetChuckAnim(),TEXT("v1 skeletal body loads with its animation instance"));
        if(Rig)
        {
            const auto Bounds=Rig->GetImportedBounds();
            Check(FMath::IsNearlyEqual(static_cast<float>(Bounds.Origin.Z+Bounds.BoxExtent.Z),65.f,1.f),TEXT("imported model ear height is 65 cm"));
            Check(Body->GetBoneIndex(TEXT("toes_L"))!=INDEX_NONE && Body->GetBoneIndex(TEXT("upperarm_R"))!=INDEX_NONE && Body->GetBoneIndex(TEXT("tail_5"))!=INDEX_NONE && Body->GetBoneIndex(TEXT("socket_cigarette"))!=INDEX_NONE,TEXT("v1 leg arm tail and cigarette bones survive packaged import"));
            const int32 Hip=Rig->GetRefSkeleton().FindBoneIndex(TEXT("thigh_L"));
            Check(Hip!=INDEX_NONE && FVector::Dist(FAnimationRuntime::GetComponentSpaceTransformRefPose(Rig->GetRefSkeleton(),Hip).GetLocation(),FVector(-2,-6.6,22.5))<.1f,TEXT("rig hip uses centimetre scale and expected axes"));
            bool bCorrectMaterials=Body->GetNumMaterials()>0;
            for(int32 I=0; I<Body->GetNumMaterials(); ++I)
            {
                const auto* Mat=Body->GetMaterial(I);
                bCorrectMaterials &= Mat && Mat->GetPathName().StartsWith(TEXT("/Game/Characters/Chuck/V1/"));
            }
            Check(bCorrectMaterials && Chuck->GetGroomCount()==(bNoGroom ? 0 : 3),TEXT("v1 material and groom assignments persist"));
            // Rest direction filter -> lit end of socket_cigarette (v1.2 table,
            // Unreal component space): forward and to Chuck's left, slightly down.
            // The aplomb idle turns and lifts the head a little (about 12 deg), hence 0.95.
            const auto* Cig=Chuck->GetCigarette();
            const auto* Wisp=Chuck->GetCigaretteSmoke();
            const FTransform& MeshToWorld=Body->GetComponentTransform();
            const FVector Along=Cig ? MeshToWorld.InverseTransformVectorNoScale(Cig->GetForwardVector()) : FVector::ZeroVector;
            const float Aim=FVector::DotProduct(Along,FVector(3.256,-2.1,-.5).GetSafeNormal());
            const float AtMouth=Cig ? FVector::Dist(Cig->GetComponentLocation(),Body->GetSocketLocation(TEXT("socket_cigarette"))) : 99.f;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CIGARETTE_MEASURE aim_dot=%.4f at_mouth_cm=%.4f smoke_up=%.4f"),Aim,AtMouth,Wisp ? Wisp->GetUpVector().Z : -1.);
            Check(Cig && Wisp && Cig->GetStaticMesh() && Wisp->GetStaticMesh() && AtMouth<.05f && Aim>.95f && Wisp->GetUpVector().Z>.999f,TEXT("cigarette held in the left mouth corner with upright smoke"));
        }
        Check(Chuck->IsElevated(),TEXT("starts in elevated camera"));
        Chuck->ToggleCamera(); Check(!Chuck->IsElevated(),TEXT("switches to rat-height camera"));
        Chuck->ToggleCamera(); Check(Chuck->IsElevated(),TEXT("switches back to elevated camera"));
        TestStage=1; StageTime=0;
    }
    else if(TestStage==1)
    {
        Chuck->AddMovementInput(FVector(1,0,0),1);
        PerfFrameMs+=DeltaSeconds*1000.; PerfGpuMs+=FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()); ++PerfFrames;
        // Measured on the evaluated pose: a paw locked in stance on flat
        // ground during steady straight walking should not move in world space.
        const FChuckAnimResult Pose=Chuck->GetChuckAnim() ? Chuck->GetChuckAnim()->GetResult() : FChuckAnimResult();
        const bool bNewPose=Pose.Evaluations!=ProbeEvaluations;
        for(int32 I=0; I<2 && bNewPose; ++I)
        {
            const FVector Position=Pose.BallWorld[I];
            ProbeReachExcess=FMath::Max(ProbeReachExcess,Pose.Shortfall[I]);
            const bool bLocked=Pose.LockAlpha[I]>=1.f && !Pose.bSettling[I];
            if(bProbeReady && bProbeLocked[I] && bLocked && StageTime>.25f && Chuck->GetVelocity().Size2D()>.9f*ChuckClipData::WalkSpeed)
            {
                const float SlipSpeed=FVector::Dist(Position,ProbeFoot[I])/FMath::Max(DeltaSeconds,.001f);
                ProbeSlip+=SlipSpeed; ProbeMaxSpeed=FMath::Max(ProbeMaxSpeed,SlipSpeed); ++ProbeSamples;
            }
            ProbeFoot[I]=Position; bProbeLocked[I]=bLocked;
        }
        ProbeEvaluations=Pose.Evaluations;
        if(bNewPose) bProbeReady=true;
        if(StageTime>1)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_PERF_MEASURE grooms=%d frames=%d mean_frame_ms=%.3f mean_gpu_ms=%.3f"),Chuck->GetGroomCount(),PerfFrames,PerfFrames ? PerfFrameMs/PerfFrames : -1.,PerfFrames ? PerfGpuMs/PerfFrames : -1.);
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CONTACT_MEASURE samples=%d mean_cm_s=%.4f max_cm_s=%.4f"),ProbeSamples,ProbeSamples ? ProbeSlip/ProbeSamples : -1.,ProbeMaxSpeed);
            Check(Chuck->GetActorLocation().X > -200,TEXT("walking advances across quay"));
            auto* MovingBody=Chuck->GetMesh();
            Check(ProbeSamples>=10 && ProbeMaxSpeed<1.f,TEXT("steady walk stance feet stay planted within 1 cm/s"));
            UE_LOG(LogTemp,Display,TEXT("CHUCK_REACH_MEASURE max_shortfall_cm=%.4f"),ProbeReachExcess);
            Check(ProbeReachExcess<.5f,TEXT("walking paw targets remain within leg reach"));
            if(MovingBody)
            {
                const auto* Rig=MovingBody->GetSkeletalMeshAsset();
                const int32 Arm=MovingBody->GetBoneIndex(TEXT("upperarm_L"));
                const TArray<FTransform> Local=MovingBody->GetBoneSpaceTransforms();
                Check(Rig && Local.IsValidIndex(Arm) && Local[Arm].GetRotation().AngularDistance(Rig->GetRefSkeleton().GetRefBonePose()[Arm].GetRotation())>.01f,TEXT("walking articulates jacket sleeve"));
            }
            Chuck->Jump(); MaxJumpZ=Chuck->GetActorLocation().Z; TestStage=2; StageTime=0;
        }
    }
    else if(TestStage==2)
    {
        MaxJumpZ=FMath::Max(MaxJumpZ,static_cast<float>(Chuck->GetActorLocation().Z));
        {
            // The orbit pivot, not the lens: look pitch moves the lens freely.
            const float CameraZ=Chuck->FindComponentByClass<USpringArmComponent>()->GetComponentLocation().Z;
            if(StageTime<=DeltaSeconds*1.5f) { CameraMinZ=CameraMaxZ=CameraZ; }
            CameraMinZ=FMath::Min(CameraMinZ,CameraZ); CameraMaxZ=FMath::Max(CameraMaxZ,CameraZ);
        }
        const FChuckAnimResult Pose=Chuck->GetChuckAnim() ? Chuck->GetChuckAnim()->GetResult() : FChuckAnimResult();
        if(Chuck->GetCharacterMovement()->IsFalling())
            MaxAirFootLift=FMath::Max(MaxAirFootLift,static_cast<float>(Pose.BallWorld[0].Z-Chuck->GetMesh()->GetComponentLocation().Z));
        if(StageTime>1)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_AIR_MEASURE max_ball_lift_cm=%.4f"),MaxAirFootLift);
            Check(MaxAirFootLift>4.f,TEXT("airborne feet tuck above resting pose"));
            FHitResult FootGround;
            FCollisionQueryParams FootQuery(SCENE_QUERY_STAT(ChuckLandingCheck),false,Chuck);
            const FVector FootPosition=Pose.BallWorld[0];
            const bool bGround=GetWorld()->LineTraceSingleByChannel(FootGround,FootPosition+FVector(0,0,8),FootPosition-FVector(0,0,15),ECC_Visibility,FootQuery);
            const auto* Rig=Chuck->GetMesh()->GetSkeletalMeshAsset();
            const int32 Toes=Rig ? Rig->GetRefSkeleton().FindBoneIndex(TEXT("toes_L")) : INDEX_NONE;
            const float RestBall=Toes!=INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(Rig->GetRefSkeleton(),Toes).GetLocation().Z : -1.f;
            const float Clearance=bGround ? static_cast<float>(FootPosition.Z-FootGround.ImpactPoint.Z) : -99.f;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_LAND_MEASURE ball_above_ground_cm=%.4f rest_cm=%.4f"),Clearance,RestBall);
            Check(Pose.Evaluations>0 && bGround && FMath::IsNearlyEqual(Clearance,RestBall,.3f),TEXT("feet settle on traced ground after landing"));
            Check(MaxJumpZ>48,TEXT("jump lifts Chuck above floor"));
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CAMERA_MEASURE jump_camera_travel_cm=%.3f"),CameraMaxZ-CameraMinZ);
            Check(CameraMaxZ-CameraMinZ<8.f,TEXT("camera holds its height through a jump"));
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
        TestStage=5; StageTime=0;
    }
    else if(TestStage==5 && (StageTime<=1.2f || (StageTime<4 && StageTime-LookReached<.6f)))
    {
        // Right stick up orbits the camera down to rat height; no switch
        // button. Held until it gets there at any frame rate, then the blend
        // settles. (Simulated mouse axes are not sampled in the capped run.)
        auto* PC=Cast<APlayerController>(Chuck->GetController());
        if(StageTime>1 && StageTime-DeltaSeconds<=1) PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W,IE_Released,0));
        const bool bLow=Chuck->GetLookPitch()>=-5.f;
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightY,IE_Axis,bLow ? 0.f : 1.f));
        if(!bLow) LookReached=StageTime;
    }
    else if(TestStage==5)
    {
        auto* PC=Cast<APlayerController>(Chuck->GetController());
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W,IE_Released,0));
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightY,IE_Axis,0));
        Check(Chuck->GetActorLocation().X > -200,TEXT("keyboard W mapping walks"));
        Check(!Chuck->IsElevated(),TEXT("right stick up orbits down to rat height"));
        Check(FMath::IsNearlyEqual(Chuck->FindComponentByClass<USpringArmComponent>()->TargetArmLength,220.f,1.f),TEXT("rat-height camera blend settles"));
        {
            FHitResult Floor;
            const FVector Lens=Chuck->FindComponentByClass<UCameraComponent>()->GetComponentLocation();
            FCollisionQueryParams LensQuery(SCENE_QUERY_STAT(ChuckLensHeight),false,Chuck);
            const bool bFloor=GetWorld()->LineTraceSingleByChannel(Floor,Lens,Lens-FVector(0,0,200),ECC_Visibility,LensQuery);
            const float Height=bFloor ? static_cast<float>(Lens.Z-Floor.ImpactPoint.Z) : -1.f;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CAMERA_MEASURE rat_lens_height_cm=%.3f"),Height);
            Check(Height>68.f && Height<85.f,TEXT("rat-height lens sits just over Chuck's ears"));
        }
        Chuck->ResetToDock();
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightY,IE_Axis,-1));
        TestStage=6; StageTime=0;
    }
    else if(TestStage==6)
    {
        auto* PC=Cast<APlayerController>(Chuck->GetController());
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY,IE_Axis,1));
        if(StageTime>1)
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY,IE_Axis,0));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightY,IE_Axis,0));
            Check(Chuck->GetActorLocation().X > -200,TEXT("Xbox left-stick mapping walks"));
            Check(Chuck->IsElevated(),TEXT("Xbox right stick orbits up to elevated"));
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
            else
            {
                // The simulated gamepad stick from stage 6 stays deflected; the
                // stop/turn stages drive Chuck directly, as the captures do.
                auto* ProbePC=Cast<APlayerController>(Chuck->GetController());
                ProbePC->FlushPressedKeys(); Chuck->DisableInput(ProbePC);
                Chuck->ResetToDock(); TestStage=50; StageTime=0;
            }
        }
    }
    else if(TestStage==50)
    {
        // Walk, release input: the distance-matched WalkStop brakes over its
        // authored travel with planted paws held, then Chuck stands.
        if(StageTime<1.2f) Chuck->AddMovementInput(FVector(1,0,0),1);
        else
        {
            const bool bStopping=FCString::Strcmp(Chuck->GetGaitName(),TEXT("Stop"))==0;
            // The stop is triggered by the frame whose travel crossed the half
            // stride, so its braking distance is measured from before that frame.
            if(!bLocoFlag && bStopping) { bLocoFlag=true; LocoStart=LocoPrevious; LocoEvaluations=-1; LocoSamples=0; LocoMaxSlip=0; LocoReleases=Chuck->GetChuckAnim()->GetResult().Releases; }
            if(bStopping) ProbeLockedPaws(Chuck,DeltaSeconds);
            else if(!bLocoFlag) LocoPrevious=Chuck->GetActorLocation();
            if(Chuck->GetVelocity().Size2D()>1.f) LocoValue=StageTime-1.2f;
        }
        if(StageTime>2.4f)
        {
            const FChuckAnimResult Pose=Chuck->GetChuckAnim()->GetResult();
            const float Distance=FVector::Dist2D(Chuck->GetActorLocation(),LocoStart);
            UE_LOG(LogTemp,Display,TEXT("CHUCK_STOP_MEASURE distance_cm=%.3f time_s=%.3f samples=%d max_cm_s=%.4f releases=%d gait=%s"),Distance,LocoValue,LocoSamples,LocoMaxSlip,Pose.Releases-LocoReleases,Chuck->GetGaitName());
            Check(bLocoFlag && LocoSamples>=5 && LocoMaxSlip<1.f && Pose.Releases==LocoReleases && FMath::IsNearlyEqual(Distance,ChuckClipData::StopTravel,2.f),TEXT("walk stop brakes over its clip with planted paws still"));
            Check(FCString::Strcmp(Chuck->GetGaitName(),TEXT("Idle"))==0 && Pose.LockAlpha[0]>=1.f && Pose.LockAlpha[1]>=1.f,TEXT("walk stop settles into planted idle stance"));
            Chuck->ResetToDock(); bLocoFlag=false; TestStage=51; StageTime=0;
        }
    }
    else if(TestStage==51)
    {
        // Ask to walk 90 degrees left from standing: Chuck pivots in place on
        // planted paws first, then walks off facing the input.
        if(StageTime>.3f) Chuck->AddMovementInput(FVector(0,-1,0),1);
        if(StageTime<.3f) { LocoEvaluations=-1; LocoSamples=0; LocoMaxSlip=0; LocoReleases=Chuck->GetChuckAnim()->GetResult().Releases; bLocoFlag=false; LocoValue=999; }
        if(FCString::Strcmp(Chuck->GetGaitName(),TEXT("Turn"))==0) { bLocoFlag=true; ProbeLockedPaws(Chuck,DeltaSeconds); }
        if(bLocoFlag && LocoValue>900 && Chuck->GetVelocity().Size2D()>10.f) LocoValue=Chuck->GetActorRotation().Yaw;
        if(StageTime>2)
        {
            const int32 Releases=Chuck->GetChuckAnim()->GetResult().Releases-LocoReleases;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_TURN_MEASURE turned=%d samples=%d max_cm_s=%.4f releases=%d yaw_at_walk=%.3f"),bLocoFlag ? 1:0,LocoSamples,LocoMaxSlip,Releases,LocoValue);
            Check(bLocoFlag && LocoSamples>=5 && LocoMaxSlip<1.f && Releases==0,TEXT("turn in place pivots on planted paws"));
            Check(bLocoFlag && FMath::Abs(FMath::FindDeltaAngleDegrees(LocoValue,-90.f))<5.f,TEXT("turn in place faces input before walking"));
            Chuck->ResetToDock(); TestStage=53; StageTime=0;
        }
    }
    else if(TestStage==53)
    {
        // GTA-style auto-follow: with the orbit left 35 degrees off Chuck's
        // heading and no look input, walking away eases it back behind him.
        if(StageTime<=DeltaSeconds*1.5f) { Chuck->SetActorRotation(FRotator(0,35,0)); Chuck->Recenter(); Chuck->SetActorRotation(FRotator::ZeroRotator); }
        Chuck->AddMovementInput(FVector(1,0,0),1);
        if(StageTime>3)
        {
            const float Off=FMath::FindDeltaAngleDegrees(Chuck->FindComponentByClass<UCameraComponent>()->GetComponentRotation().Yaw,Chuck->GetActorRotation().Yaw);
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CAMERA_MEASURE follow_offset_deg=%.3f"),Off);
            Check(FMath::Abs(Off)<8.f,TEXT("orbit drifts behind Chuck while walking"));
            // Next: a roll straight ahead from standing.
            Chuck->ResetToDock(); Chuck->DodgeToward(FVector2D::ZeroVector);
            LocoEvaluations=-1; LocoSamples=0; LocoMaxSlip=0; bLocoFlag=false;
            TestStage=54; StageTime=0;
        }
    }
    else if(TestStage==54)
    {
        if(FCString::Strcmp(Chuck->GetGaitName(),TEXT("Roll"))==0) { bLocoFlag=true; ProbeLockedPaws(Chuck,DeltaSeconds); }
        if(StageTime>1.3f)
        {
            const FVector Moved=Chuck->GetActorLocation()-AChuckCharacter::StartLocation();
            const float Authored=ChuckClipData::RollTravel[ChuckClipData::RollFrames];
            UE_LOG(LogTemp,Display,TEXT("CHUCK_ROLL_MEASURE travel_cm=%.3f authored_cm=%.3f side_cm=%.3f samples=%d max_cm_s=%.4f gait=%s"),Moved.X,Authored,Moved.Y,LocoSamples,LocoMaxSlip,Chuck->GetGaitName());
            Check(bLocoFlag && FMath::Abs(Moved.X-Authored)<8.f && FMath::Abs(Moved.Y)<2.f,TEXT("roll carries Chuck its authored distance"));
            Check(LocoSamples>=3 && LocoMaxSlip<1.f,TEXT("roll paws hold in stance"));
            Check(FCString::Strcmp(Chuck->GetGaitName(),TEXT("Idle"))==0 && Chuck->GetCharacterMovement()->IsMovingOnGround(),TEXT("roll recovers to the aplomb stance"));
            // Next: a side jump to the right (camera-relative stick right).
            Chuck->ResetToDock(); Chuck->DodgeToward(FVector2D(1,0));
            LocoEvaluations=-1; LocoSamples=0; LocoMaxSlip=0; bLocoFlag=false; MaxJumpZ=Chuck->GetActorLocation().Z;
            TestStage=55; StageTime=0;
        }
    }
    else if(TestStage==55)
    {
        MaxJumpZ=FMath::Max(MaxJumpZ,static_cast<float>(Chuck->GetActorLocation().Z));
        if(FCString::Strcmp(Chuck->GetGaitName(),TEXT("SideJump"))==0) { bLocoFlag=true; ProbeLockedPaws(Chuck,DeltaSeconds); }
        if(StageTime>1.3f)
        {
            const FVector Moved=Chuck->GetActorLocation()-AChuckCharacter::StartLocation();
            const float Flight=2.f*ChuckClipData::SideShortVerticalSpeed/(980.f*Chuck->GetCharacterMovement()->GravityScale);
            const float Authored=ChuckClipData::SideShortLateralSpeed*Flight;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_SIDEJUMP_MEASURE side_cm=%.3f authored_cm=%.3f forward_cm=%.3f apex_cm=%.3f yaw=%.3f samples=%d max_cm_s=%.4f gait=%s"),Moved.Y,Authored,Moved.X,MaxJumpZ-AChuckCharacter::StartLocation().Z,Chuck->GetActorRotation().Yaw,LocoSamples,LocoMaxSlip,Chuck->GetGaitName());
            Check(bLocoFlag && FMath::Abs(Moved.Y-Authored)<12.f && FMath::Abs(Moved.X)<3.f,TEXT("side jump springs Chuck sideways its authored distance"));
            Check(FMath::Abs(Chuck->GetActorRotation().Yaw)<1.f && LocoSamples>=3 && LocoMaxSlip<1.f,TEXT("side jump keeps facing with paws held in stance"));
            Check(FCString::Strcmp(Chuck->GetGaitName(),TEXT("Idle"))==0 && Chuck->GetCharacterMovement()->IsMovingOnGround(),TEXT("side jump recovers to the aplomb stance"));
            // Next: run straight down the quay, then let go.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,0,36)); Chuck->SetRunHeld(true);
            LocoEvaluations=-1; LocoSamples=0; LocoMaxSlip=0; LocoValue=0;
            TestStage=58; StageTime=0;
        }
    }
    else if(TestStage==58)
    {
        // Hold run: the capsule reaches the run speed, the blend goes fully
        // to RunLoop and the planted paws hold at speed.
        Chuck->AddMovementInput(FVector(1,0,0),1);
        LocoValue=FMath::Max(LocoValue,static_cast<float>(Chuck->GetVelocity().Size2D()));
        if(StageTime>1.f) ProbeLockedPaws(Chuck,DeltaSeconds);
        if(StageTime>2.f)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_RUN_MEASURE speed_cm_s=%.3f authored_cm_s=%.3f blend=%.3f samples=%d max_cm_s=%.4f"),LocoValue,ChuckClipData::RunSpeed,Chuck->GetRunWeight(),LocoSamples,LocoMaxSlip);
            Check(FMath::Abs(LocoValue-ChuckClipData::RunSpeed)<3.f && Chuck->GetRunWeight()>.95f,TEXT("run reaches the authored run speed and blend"));
            Check(LocoSamples>=10 && LocoMaxSlip<1.f,TEXT("running paws hold in stance"));
            Chuck->SetRunHeld(false); LocoPrevious=Chuck->GetActorLocation();
            TestStage=59; StageTime=0;
        }
    }
    else if(TestStage==59 && StageTime>1.8f)
    {
        const float Coast=FVector::Dist2D(Chuck->GetActorLocation(),LocoPrevious);
        UE_LOG(LogTemp,Display,TEXT("CHUCK_RUN_STOP_MEASURE distance_cm=%.3f gait=%s"),Coast,Chuck->GetGaitName());
        Check(FCString::Strcmp(Chuck->GetGaitName(),TEXT("Idle"))==0 && Coast<60.f,TEXT("a stop from a run settles into the aplomb stance"));
        // Next: roll out of a run with the stick held.
        Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,0,36));
        Chuck->SetRunHeld(true); Chuck->SetTestStick(FVector2D(0,1));
        LocoEvaluations=-1; LocoSamples=0; LocoMaxSlip=0; LocoValue=-2;
        TestStage=61; StageTime=0;
    }
    else if(TestStage==61)
    {
        const bool bRolling=FCString::Strcmp(Chuck->GetGaitName(),TEXT("Roll"))==0;
        if(!bRolling) Chuck->AddMovementInput(FVector(1,0,0),1);
        if(StageTime>=1.2f && StageTime-DeltaSeconds<1.2f) { Chuck->DodgeToward(FVector2D(0,1)); LocoValue=-1; }
        if(LocoValue==-1 && !bRolling && FCString::Strcmp(Chuck->GetGaitName(),TEXT("Loop"))==0 && Chuck->GetVelocity().Size2D()>=.95f*ChuckClipData::RunSpeed)
            LocoValue=StageTime-1.2f;
        if(LocoValue>=0 && StageTime-1.2f>LocoValue+.05f) ProbeLockedPaws(Chuck,DeltaSeconds);
        if(StageTime>2.3f)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_ROLL_RUN_MEASURE back_to_run_s=%.3f samples=%d max_cm_s=%.4f"),LocoValue,LocoSamples,LocoMaxSlip);
            Check(LocoValue>=0 && LocoValue<.75f,TEXT("a roll out of a run springs straight back into the run"));
            Check(LocoSamples>=5 && LocoMaxSlip<1.f,TEXT("running paws hold after the roll"));
            Chuck->SetRunHeld(false); Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock();
            // Next: the real keyboard path, D held from standing then C.
            auto* KeyPC=Cast<APlayerController>(Chuck->GetController());
            Chuck->EnableInput(KeyPC); Chuck->SetLookLocked(true);
            KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::D,IE_Pressed,1));
            bKeyMeasured=false; bLocoFlag=false;
            TestStage=62; StageTime=0;
        }
    }
    else if(TestStage==62 || TestStage==63)
    {
        auto* KeyPC=Cast<APlayerController>(Chuck->GetController());
        const bool bRight=TestStage==62;
        if(StageTime>=.15f && StageTime-DeltaSeconds<.15f) { KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::C,IE_Pressed,1)); LocoPrevious=Chuck->GetActorLocation(); }
        if(StageTime>=.22f && StageTime-DeltaSeconds<.22f) KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::C,IE_Released,0));
        const bool bJumping=FCString::Strcmp(Chuck->GetGaitName(),TEXT("SideJump"))==0;
        if(bJumping) bLocoFlag=true;
        else if(bLocoFlag && !bKeyMeasured)
        {
            // Sideways travel over the jump, along Chuck's right (he squares up down the camera).
            KeySide=FVector::DotProduct(Chuck->GetActorLocation()-LocoPrevious,FRotationMatrix(Chuck->GetActorRotation()).GetUnitAxis(EAxis::Y));
            bKeyMeasured=true;
        }
        if(StageTime>1.5f)
        {
            KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(bRight ? EKeys::D : EKeys::A,IE_Released,0));
            UE_LOG(LogTemp,Display,TEXT("CHUCK_KEY_SIDEJUMP_MEASURE key=%s side_cm=%.3f jumped=%d"),bRight ? TEXT("D") : TEXT("A"),KeySide,bLocoFlag ? 1 : 0);
            Check(bKeyMeasured && (bRight ? KeySide>40.f : KeySide<-40.f),bRight ? TEXT("keyboard D + C side-jumps right") : TEXT("keyboard A + C side-jumps left"));
            Chuck->ResetToDock(); StageTime=0; bKeyMeasured=false; bLocoFlag=false;
            if(bRight) { KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::A,IE_Pressed,1)); TestStage=63; }
            else
            {
                KeyPC->FlushPressedKeys(); Chuck->DisableInput(KeyPC); Chuck->SetLookLocked(false);
                // Next: a running jump with the stick held.
                Chuck->SetActorLocation(FVector(-240,0,36)); Chuck->SetRunHeld(true); Chuck->SetTestStick(FVector2D(0,1));
                LocoEvaluations=-1; LocoSamples=0; LocoMaxSlip=0; LocoValue=-3; bLocoFlag=false; bKeyMeasured=false; KeySide=0;
                TestStage=64;
            }
        }
    }
    else if(TestStage==64)
    {
        // LocoValue: -3 running up, -1 jump pressed, -2 airborne, >= 0 landing time.
        const bool bAir=Chuck->GetCharacterMovement()->IsFalling();
        Chuck->AddMovementInput(FVector(1,0,0),1);  // stick held throughout, as a player would
        if(StageTime>=1.2f && StageTime-DeltaSeconds<1.2f) { Chuck->Jump(); LocoValue=-1; }
        if(LocoValue==-1 && bAir) { LocoPrevious=Chuck->GetActorLocation(); MaxJumpZ=LocoPrevious.Z; bLocoFlag=Chuck->IsRunJumping(); LocoValue=-2; }
        if(LocoValue==-2)
        {
            MaxJumpZ=FMath::Max(MaxJumpZ,static_cast<float>(Chuck->GetActorLocation().Z));
            if(!bAir)
            {
                KeySide=FVector::Dist2D(Chuck->GetActorLocation(),LocoPrevious);
                bKeyMeasured=FCString::Strcmp(Chuck->GetGaitName(),TEXT("Loop"))==0 && Chuck->GetVelocity().Size2D()>=.9f*ChuckClipData::RunSpeed;
                LocoValue=StageTime;
            }
        }
        if(LocoValue>=0 && StageTime>LocoValue+.1f && StageTime<LocoValue+.6f) ProbeLockedPaws(Chuck,DeltaSeconds);
        if(StageTime>2.4f)
        {
            const float Flight=2.f*ChuckClipData::RunJumpVerticalSpeed/(980.f*Chuck->GetCharacterMovement()->GravityScale);
            const float Ballistic=ChuckClipData::RunSpeed*Flight;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_RUN_JUMP_MEASURE leap=%d distance_cm=%.3f ballistic_cm=%.3f apex_cm=%.3f into_run=%d samples=%d max_cm_s=%.4f"),bLocoFlag ? 1 : 0,KeySide,Ballistic,MaxJumpZ-LocoPrevious.Z,bKeyMeasured ? 1 : 0,LocoSamples,LocoMaxSlip);
            Check(bLocoFlag && FMath::Abs(KeySide-Ballistic)<12.f,TEXT("running jump leaps its ballistic distance"));
            Check(bKeyMeasured,TEXT("running jump lands straight into the run"));
            Check(LocoSamples>=5 && LocoMaxSlip<1.f,TEXT("paws hold after a running landing"));
            Chuck->SetRunHeld(false); Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); StageTime=0;
            // Next: the same through the real keys, Shift + W held, then Space.
            Chuck->SetActorLocation(FVector(-380,0,36));
            auto* KeyPC=Cast<APlayerController>(Chuck->GetController());
            Chuck->EnableInput(KeyPC); Chuck->SetLookLocked(true);
            KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftShift,IE_Pressed,1));
            KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W,IE_Pressed,1));
            bLocoFlag=false; LocoValue=0;
            TestStage=65;
        }
    }
    else if(TestStage==65)
    {
        auto* KeyPC=Cast<APlayerController>(Chuck->GetController());
        // Shift is a tap: released at 0.2 s, the run carries on; Space at 1.2 s
        // leaps; a second tap at 1.8 s drops back to the saunter.
        auto Tap=[KeyPC](const FKey& Key,bool bDown){ KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,bDown ? IE_Pressed : IE_Released,bDown ? 1.f : 0.f)); };
        if(StageTime>=.2f && StageTime-DeltaSeconds<.2f) Tap(EKeys::LeftShift,false);
        if(StageTime>=1.2f && StageTime-DeltaSeconds<1.2f) { LocoValue=Chuck->GetVelocity().Size2D(); Tap(EKeys::SpaceBar,true); }
        if(StageTime>=1.3f && StageTime-DeltaSeconds<1.3f) Tap(EKeys::SpaceBar,false);
        if(StageTime>=1.8f && StageTime-DeltaSeconds<1.8f) Tap(EKeys::LeftShift,true);
        if(StageTime>=1.85f && StageTime-DeltaSeconds<1.85f) Tap(EKeys::LeftShift,false);
        if(Chuck->IsRunJumping()) bLocoFlag=true;
        if(StageTime>3.f)
        {
            const float Saunter=Chuck->GetVelocity().Size2D();
            UE_LOG(LogTemp,Display,TEXT("CHUCK_KEY_RUN_JUMP_MEASURE speed_at_press_cm_s=%.3f leap=%d speed_after_second_tap_cm_s=%.3f gait=%s"),LocoValue,bLocoFlag ? 1 : 0,Saunter,Chuck->GetGaitName());
            Check(bLocoFlag && LocoValue>.95f*ChuckClipData::RunSpeed,TEXT("tapped Shift keeps running; Space leaps without holding Shift"));
            Check(FMath::Abs(Saunter-ChuckClipData::WalkSpeed)<3.f,TEXT("a second Shift tap drops back to the saunter"));
            Tap(EKeys::W,false);
            KeyPC->FlushPressedKeys(); Chuck->DisableInput(KeyPC); Chuck->SetLookLocked(false);
            // Next: a standing slash, with a second press buffered to chain the other paw.
            Chuck->ResetToDock(); Chuck->Slash(); Chuck->SlashReleased(); SlashStrikeBase=Chuck->GetSlashStrikes();
            LocoEvaluations=-1; LocoSamples=0; LocoMaxSlip=0; bLocoFlag=false; bKeyMeasured=false;
            SlashMin=1e3f; SlashMax=-1e3f; SlashSpeed=0; SlashLeftMin=1e3f; SlashLeftMax=-1e3f; bSlashHave=false;
            TestStage=66; StageTime=0;
        }
    }
    else if(TestStage==66 || TestStage==67)
    {
        // Paw sweep across the body in actor space (+Y right), and its speed.
        const bool bRunning=TestStage==67;
        if(bRunning) Chuck->AddMovementInput(FVector(1,0,0),1);
        if(!bRunning && StageTime>=.1f && StageTime-DeltaSeconds<.1f) { Chuck->Slash(); Chuck->SlashReleased(); }  // a tap, buffered: chains
        if(bRunning && StageTime>=1.f && StageTime-DeltaSeconds<1.f) { Chuck->Slash(); Chuck->SlashReleased(); SlashStrikeBase=Chuck->GetSlashStrikes(); LocoValue=1e3f; }
        // The paw order is random: follow whichever paw the first strike uses,
        // with its outward side as +. The second strike's paw: SlashLeft* fields.
        const FString Name=Chuck->GetSlashName();
        const FTransform Actor=Chuck->GetActorTransform();
        const bool bRightPaw=Name==TEXT("SlashRight");
        const FVector Paw=Actor.InverseTransformPosition(Chuck->GetMesh()->GetSocketLocation(bRightPaw ? TEXT("hand_R") : TEXT("hand_L")));
        const float Outward=bRightPaw ? Paw.Y : -Paw.Y;
        const int32 Strike=Chuck->GetSlashStrikes()-SlashStrikeBase;
        if(!Name.IsEmpty() && Strike==0 && (!bRunning || StageTime>=1.f))
        {
            SlashMin=FMath::Min(SlashMin,Outward); SlashMax=FMath::Max(SlashMax,Outward);
            // Skip the first frames: the pose still reflects the pre-reset position.
            if(bSlashHave && StageTime>.05f) SlashSpeed=FMath::Max(SlashSpeed,static_cast<float>(FVector::Dist(Paw,SlashPrevious))/FMath::Max(DeltaSeconds,.001f));
            SlashPrevious=Paw; bSlashHave=true;
        }
        else bSlashHave=false;
        if(!Name.IsEmpty() && Strike==1) { bKeyMeasured=true; SlashLeftMin=FMath::Min(SlashLeftMin,Outward); SlashLeftMax=FMath::Max(SlashLeftMax,Outward); }
        if(!bRunning && FCString::Strcmp(Chuck->GetGaitName(),TEXT("Slash"))==0) ProbeLockedPaws(Chuck,DeltaSeconds);
        if(bRunning && !Name.IsEmpty()) { LocoValue=FMath::Min(LocoValue,static_cast<float>(Chuck->GetVelocity().Size2D())); ProbeLockedPaws(Chuck,DeltaSeconds); }
        if(StageTime>(bRunning ? 2.f : 1.5f))
        {
            const FVector Moved=Chuck->GetActorLocation()-AChuckCharacter::StartLocation();
            UE_LOG(LogTemp,Display,TEXT("CHUCK_SLASH_MEASURE mode=%s first_paw_outward=[%.2f,%.2f] peak_cm_s=%.1f second_paw_outward=[%.2f,%.2f] chained=%d travel_cm=%.3f min_speed_cm_s=%.3f samples=%d max_cm_s=%.4f gait=%s"),bRunning ? TEXT("running") : TEXT("standing"),SlashMin,SlashMax,SlashSpeed,SlashLeftMin,SlashLeftMax,bKeyMeasured ? 1 : 0,Moved.X,bRunning ? LocoValue : 0.f,LocoSamples,LocoMaxSlip,Chuck->GetGaitName());
            // The striking paw starts on its own side and rakes across the midline.
            const bool bRake=SlashMax>8.f && SlashMin<0.f && SlashSpeed>250.f;
            if(!bRunning)
            {
                Check(bRake,TEXT("standing slash rakes its paw fast across the body"));
                Check(bKeyMeasured && SlashLeftMin<0.f && SlashLeftMax>8.f,TEXT("a second press chains another raking strike"));
                // The chain cuts the first step-in short at the chain point.
                auto TravelAt=[](float Time){ const float Frame=FMath::Clamp(Time*30.f,0.f,static_cast<float>(ChuckClipData::SlashFrames)); const int32 Index=FMath::Min(FMath::FloorToInt(Frame),ChuckClipData::SlashFrames-1); return FMath::Lerp(ChuckClipData::SlashTravel[Index],ChuckClipData::SlashTravel[Index+1],Frame-Index); };
                const float Full=ChuckClipData::SlashTravel[ChuckClipData::SlashFrames];
                const float Shortest=TravelAt(ChuckClipData::SlashChainAt)+Full, Longest=TravelAt(ChuckClipData::SlashChainAt+.04f)+Full;
                UE_LOG(LogTemp,Display,TEXT("CHUCK_SLASH_STEP_MEASURE travel_cm=%.3f expected_cm=[%.3f,%.3f]"),Moved.X,Shortest,Longest);
                Check(Moved.X>Shortest-1.f && Moved.X<Longest+1.f && FCString::Strcmp(Chuck->GetGaitName(),TEXT("Idle"))==0,TEXT("two chained slashes step in and settle"));
                Check(LocoSamples>=5 && LocoMaxSlip<1.f,TEXT("slash paws hold in stance"));
                Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-380,0,36)); Chuck->SetRunHeld(true);
                LocoEvaluations=-1; LocoSamples=0; LocoMaxSlip=0; bKeyMeasured=false;
                SlashMin=1e3f; SlashMax=-1e3f; SlashSpeed=0; bSlashHave=false;
                TestStage=67; StageTime=0;
            }
            else
            {
                Check(bRake,TEXT("running slash rakes its paw across over the stride"));
                Check(LocoValue>.95f*ChuckClipData::RunSpeed && LocoSamples>=5 && LocoMaxSlip<1.f,TEXT("running slash keeps the stride at speed with paws holding"));
                Chuck->SetRunHeld(false); Chuck->ResetToDock();
                auto* KeyPC=Cast<APlayerController>(Chuck->GetController());
                Chuck->EnableInput(KeyPC); Chuck->SetLookLocked(true);
                KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton,IE_Pressed,1));
                bLocoFlag=false; TestStage=68; StageTime=0;
            }
        }
    }
    else if(TestStage==68)
    {
        auto* KeyPC=Cast<APlayerController>(Chuck->GetController());
        if(StageTime>=.05f && StageTime-DeltaSeconds<.05f) KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton,IE_Released,0));
        if(FCString::Strlen(Chuck->GetSlashName())>0) bLocoFlag=true;
        if(StageTime>.8f)
        {
            Check(bLocoFlag,TEXT("left mouse button slashes"));
            KeyPC->FlushPressedKeys(); Chuck->DisableInput(KeyPC); Chuck->SetLookLocked(false);
            // Next: hold slash for a flurry.
            Chuck->ResetToDock(); Chuck->SetSlashSeed(20260928); Chuck->Slash();
            FlurryStarts.Reset(); FlurryNames.Reset(); FlurryStarts.Add(0); FlurryNames.Add(Chuck->GetSlashName()); SlashStrikeBase=Chuck->GetSlashStrikes();
            LocoEvaluations=-1; LocoSamples=0; LocoMaxSlip=0;
            TestStage=70; StageTime=0;
        }
    }
    else if(TestStage==70)
    {
        // Held: a strike on each chain beat, the paw random (never three of one
        // in a row). Released at 1.6 s: the flurry ends after that paw.
        if(Chuck->GetSlashStrikes()!=SlashStrikeBase) { SlashStrikeBase=Chuck->GetSlashStrikes(); FlurryStarts.Add(StageTime); FlurryNames.Add(Chuck->GetSlashName()); }
        if(FCString::Strcmp(Chuck->GetGaitName(),TEXT("Slash"))==0) ProbeLockedPaws(Chuck,DeltaSeconds);
        if(StageTime>=1.6f && StageTime-DeltaSeconds<1.6f) Chuck->SlashReleased();
        if(StageTime>2.4f)
        {
            int32 Rights=0, Repeats=0, LongestRun=1, Run=1;
            for(int32 I=0; I<FlurryNames.Num(); ++I)
            {
                Rights+=FlurryNames[I].Contains(TEXT("Right"));
                if(I>0) { const bool bSame=FlurryNames[I]==FlurryNames[I-1]; Repeats+=bSame; Run=bSame ? Run+1 : 1; LongestRun=FMath::Max(LongestRun,Run); }
            }
            float Shortest=1e3f, Longest=0;
            for(int32 I=1; I<FlurryStarts.Num(); ++I) { const float Gap=FlurryStarts[I]-FlurryStarts[I-1]; Shortest=FMath::Min(Shortest,Gap); Longest=FMath::Max(Longest,Gap); }
            FString Order; for(const FString& Name : FlurryNames) Order+=Name.Contains(TEXT("Right")) ? TEXT("R") : TEXT("L");
            UE_LOG(LogTemp,Display,TEXT("CHUCK_FLURRY_MEASURE strikes=%d order=%s gaps_s=[%.3f,%.3f] chain_s=%.3f samples=%d max_cm_s=%.4f gait=%s"),FlurryNames.Num(),*Order,Shortest,Longest,ChuckClipData::SlashChainAt,LocoSamples,LocoMaxSlip,Chuck->GetGaitName());
            Check(FlurryNames.Num()>=5 && Rights>0 && Rights<FlurryNames.Num() && Repeats>0 && LongestRun<=2 && Shortest>ChuckClipData::SlashChainAt-.02f && Longest<ChuckClipData::SlashChainAt+.05f,TEXT("held slash flurries on a steady beat with a random paw order"));
            Check(LocoSamples>=5 && LocoMaxSlip<1.f && FCString::Strcmp(Chuck->GetGaitName(),TEXT("Idle"))==0,TEXT("flurry paws hold and it settles on release"));
            // Next: parkour. Jump into a tall wall (crate stack A), pushing toward it.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-390,-275,36));  // crate stack A's north face: too tall to top out
            Chuck->SetActorRotation(FRotator(0,-90,0)); Chuck->Recenter(); Chuck->SetTestStick(FVector2D(0,1));
            WallStart=Chuck->GetActorLocation(); WallRunsBase=Chuck->GetWallRuns(); WallJumpsBase=Chuck->GetWallJumps();
            WallEnterZ=WallPeakZ=0; WallEnterAt=WallLeaveAt=-1; bWallLanded=false;
            TestStage=71; StageTime=0;
        }
    }
    else if(TestStage==71 || TestStage==73)
    {
        // 71: run up the wall and drop off it; the same wall gives no second
        // run before landing. 73: a jump just after leaving the wall kicks off.
        const bool bRunning=Chuck->IsWallRunning();
        if(!bRunning && WallEnterAt<0) Chuck->AddMovementInput(FVector(0,-1,0),1);
        if(StageTime>=.3f && StageTime-DeltaSeconds<.3f) Chuck->JumpPressed();
        if(bRunning && WallEnterAt<0) { WallEnterAt=StageTime; WallEnterZ=Chuck->GetActorLocation().Z; }
        if(bRunning) WallPeakZ=FMath::Max(WallPeakZ,static_cast<float>(Chuck->GetActorLocation().Z));
        if(!bRunning && WallEnterAt>=0 && WallLeaveAt<0) WallLeaveAt=StageTime;
        if(TestStage==73 && WallLeaveAt>=0 && StageTime>=WallLeaveAt+.08f && StageTime-DeltaSeconds<WallLeaveAt+.08f) Chuck->JumpPressed();
        if(WallLeaveAt>=0 && Chuck->GetCharacterMovement()->IsMovingOnGround()) bWallLanded=true;
        if(StageTime>2.5f)
        {
            const int32 Runs=Chuck->GetWallRuns()-WallRunsBase, Jumps=Chuck->GetWallJumps()-WallJumpsBase;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_WALL_MEASURE stage=%d runs=%d jumps=%d rise_cm=%.3f time_on_wall_s=%.3f landed=%d gait=%s"),TestStage,Runs,Jumps,WallPeakZ-WallEnterZ,WallLeaveAt-WallEnterAt,bWallLanded ? 1 : 0,Chuck->GetGaitName());
            if(TestStage==71)
            {
                Check(Runs>=1 && WallPeakZ-WallEnterZ>=.8f*AChuckCharacter::WallRunRise,TEXT("jumping into a wall runs up it"));
                Check(Runs==1 && FMath::Abs(WallLeaveAt-WallEnterAt-AChuckCharacter::WallRunTime)<.08f && bWallLanded,TEXT("three steps up, then he drops off; the same wall gives one run"));
                Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-390,-275,36));
                Chuck->SetActorRotation(FRotator(0,-90,0)); Chuck->Recenter(); Chuck->SetTestStick(FVector2D(0,1));
                WallRunsBase=Chuck->GetWallRuns(); WallJumpsBase=Chuck->GetWallJumps();
                WallEnterZ=WallPeakZ=0; WallEnterAt=WallLeaveAt=-1; bWallLanded=false;
                TestStage=73; StageTime=0;
            }
            else
            {
                Check(Jumps==1,TEXT("a wall jump just after leaving the wall still counts"));
                // Next: the cargo chimney. Jump at stack A (face X -360); kick
                // off each wall once a run is under way.
                Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-335,-337,36));
                Chuck->SetActorRotation(FRotator(0,180,0)); Chuck->Recenter(); Chuck->SetTestStick(FVector2D(0,1));
                WallRunsBase=Chuck->GetWallRuns(); WallJumpsBase=Chuck->GetWallJumps(); WallStart=Chuck->GetActorLocation();
                WallPeakZ=WallStart.Z; WallEnterAt=-1; WallSides.Reset(); HangAt=-1; HangsBase=Chuck->GetHangs(); PullUpsBase=Chuck->GetPullUps();
                TestStage=72; StageTime=0;
            }
        }
    }
    else if(TestStage==72)
    {
        if(StageTime>=.1f && StageTime-DeltaSeconds<.1f) Chuck->JumpPressed();
        const bool bRunning=Chuck->IsWallRunning();
        if(bRunning && WallEnterAt<0) { WallEnterAt=StageTime; WallSides.Add(Chuck->GetActorForwardVector().X<0 ? -1 : 1); Chuck->SetTestStick(FVector2D::ZeroVector); }
        if(!bRunning) WallEnterAt=-1;
        // Kick off 0.25 s into each run, for five runs.
        if(bRunning && WallEnterAt>=0 && StageTime>=WallEnterAt+.25f && StageTime-DeltaSeconds<WallEnterAt+.25f && WallSides.Num()<=5) Chuck->JumpPressed();
        WallPeakZ=FMath::Max(WallPeakZ,static_cast<float>(Chuck->GetActorLocation().Z));
        // At the top of the stacks he catches an edge; a jump then pulls him up.
        if(Chuck->IsHanging() && HangAt<0) HangAt=StageTime;
        if(HangAt>=0 && StageTime>=HangAt+.3f && StageTime-DeltaSeconds<HangAt+.3f) Chuck->JumpPressed();
        if(StageTime>4.f)
        {
            // Each bounce gains ~70 cm: three runs reach the 240 cm stack tops (and catch one).
            bool bAlternate=WallSides.Num()>=3;
            for(int32 I=1; I<WallSides.Num(); ++I) bAlternate&=WallSides[I]!=WallSides[I-1];
            const int32 Jumps=Chuck->GetWallJumps()-WallJumpsBase;
            FString Sides; for(const int32 Side : WallSides) Sides+=Side<0 ? TEXT("A") : TEXT("B");
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHIMNEY_MEASURE runs=%d walls=%s jumps=%d climb_cm=%.3f alternate=%d end=%s z=%.1f"),WallSides.Num(),*Sides,Jumps,WallPeakZ-WallStart.Z,bAlternate ? 1 : 0,Chuck->GetGaitName(),Chuck->GetActorLocation().Z);
            Check(bAlternate && Jumps>=2 && WallPeakZ-WallStart.Z>=150.f,TEXT("wall jumps chain back and forth up the cargo chimney"));
            const float OnTop=Chuck->GetActorLocation().Z-(240.f+32.5f);
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHIMNEY_TOP_MEASURE hangs=%d pullups=%d above_stack_top_cm=%.3f ground=%d"),Chuck->GetHangs()-HangsBase,Chuck->GetPullUps()-PullUpsBase,OnTop,Chuck->GetCharacterMovement()->IsMovingOnGround() ? 1 : 0);
            Check(Chuck->GetHangs()>HangsBase && Chuck->GetPullUps()>PullUpsBase && FMath::Abs(OnTop)<3.f && Chuck->GetCharacterMovement()->IsMovingOnGround(),TEXT("the chimney bounce catches a stack top; jump pulls him up onto it"));
            // Next: the harbour wall - run up, catch the top, keep pushing: pull up.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-40,-305,36));
            Chuck->SetActorRotation(FRotator(0,-90,0)); Chuck->Recenter(); Chuck->SetTestStick(FVector2D(0,1));
            HangsBase=Chuck->GetHangs(); PullUpsBase=Chuck->GetPullUps(); HangAt=-1; bStillHanging=false; HangZ=HangZ2=0;
            TestStage=75; StageTime=0;
        }
    }
    else if(TestStage==75 || TestStage==76)
    {
        // 75: keep the stick toward the wall: run up, catch the top, pull up.
        // 76: let go of the stick once hanging: he hangs; then pull away: he drops.
        const FString G=Chuck->GetGaitName();
        if(StageTime<.3f) Chuck->AddMovementInput(FVector(0,-1,0),1);
        if(StageTime>=.3f && StageTime-DeltaSeconds<.3f) Chuck->JumpPressed();
        if(G==TEXT("Hang") && HangAt<0) { HangAt=StageTime; if(TestStage==76) Chuck->SetTestStick(FVector2D::ZeroVector); }
        if(HangAt>=0 && StageTime>=HangAt+.3f && StageTime-DeltaSeconds<HangAt+.3f) HangZ=Chuck->GetActorLocation().Z;  // after the snap-in
        if(TestStage==75 && G==TEXT("Climb")) Chuck->SetTestStick(FVector2D::ZeroVector);  // don't walk off the far side
        if(TestStage==76 && HangAt>=0 && StageTime>=HangAt+1.f && StageTime-DeltaSeconds<HangAt+1.f)
        { HangZ2=Chuck->GetActorLocation().Z; bStillHanging=G==TEXT("Hang"); Chuck->SetTestStick(FVector2D(0,-1)); }
        if(StageTime>3.f)
        {
            const FVector At=Chuck->GetActorLocation();
            const bool bGround=Chuck->GetCharacterMovement()->IsMovingOnGround();
            UE_LOG(LogTemp,Display,TEXT("CHUCK_LEDGE_MEASURE stage=%d hangs=%d pullups=%d hang_z=%.2f later_z=%.2f still=%d end_z=%.2f ground=%d gait=%s"),TestStage,Chuck->GetHangs()-HangsBase,Chuck->GetPullUps()-PullUpsBase,HangZ,HangZ2,bStillHanging ? 1 : 0,At.Z,bGround ? 1 : 0,*G);
            if(TestStage==75)
            {
                Check(Chuck->GetHangs()>HangsBase,TEXT("running up the harbour wall catches its top edge"));
                Check(Chuck->GetPullUps()>PullUpsBase && FMath::Abs(At.Z-(115.f+32.5f))<3.f && bGround,TEXT("keeping the stick toward the wall pulls him up onto it"));
                Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-40,-305,36));
                Chuck->SetActorRotation(FRotator(0,-90,0)); Chuck->Recenter(); Chuck->SetTestStick(FVector2D(0,1));
                HangsBase=Chuck->GetHangs(); PullUpsBase=Chuck->GetPullUps(); HangAt=-1; bStillHanging=false; HangZ=HangZ2=0;
                TestStage=76; StageTime=0;
            }
            else
            {
                Check(bStillHanging && FMath::Abs(HangZ2-HangZ)<.5f && Chuck->GetPullUps()==PullUpsBase && bGround && At.Z<40.f,TEXT("he hangs until told; pulling away lets go"));
                // Next: walk into the knee-high mooring plinth.
                Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(150,-250,36));
                Chuck->SetActorRotation(FRotator(0,-90,0)); Chuck->Recenter();
                MantlesBase=Chuck->GetMantles(); WallPeakZ=0;
                TestStage=78; StageTime=0;
            }
        }
    }
    else if(TestStage==78)
    {
        if(Chuck->GetMantles()==MantlesBase) Chuck->AddMovementInput(FVector(0,-1,0),1);  // let go once he's mantling
        WallPeakZ=FMath::Max(WallPeakZ,static_cast<float>(Chuck->GetActorLocation().Z));
        if(StageTime>2.2f)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_MANTLE_MEASURE mantles=%d peak_z=%.2f end_z=%.2f gait=%s"),Chuck->GetMantles()-MantlesBase,WallPeakZ,Chuck->GetActorLocation().Z,Chuck->GetGaitName());
            Check(Chuck->GetMantles()>MantlesBase && FMath::Abs(Chuck->GetActorLocation().Z-(30.f+32.5f))<3.f && Chuck->GetCharacterMovement()->IsMovingOnGround(),TEXT("walking into a knee-high ledge mantles onto it"));
            // Next: hang on the harbour wall with the camera 30 degrees off,
            // then shimmy right along it to its end.
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-40,-305,36));
            Chuck->SetActorRotation(FRotator(0,-60,0)); Chuck->Recenter(); Chuck->SetActorRotation(FRotator(0,-90,0));
            Chuck->SetTestStick(FVector2D(0,1)); HangAt=-1; ShimmyX0=ShimmyX1=ShimmyZ0=ShimmyZ1=0; CameraYawAtHang=0; InnerBase=Chuck->GetInnerCorners();
            TestStage=80; StageTime=0;
        }
    }
    else if(TestStage==80)
    {
        if(StageTime<.3f) Chuck->AddMovementInput(FVector(0,-1,0),1);
        if(StageTime>=.3f && StageTime-DeltaSeconds<.3f) Chuck->JumpPressed();
        if(Chuck->IsHanging() && HangAt<0) { HangAt=StageTime; Chuck->SetTestStick(FVector2D::ZeroVector); }
        const FVector At=Chuck->GetActorLocation();
        if(HangAt>=0 && StageTime>=HangAt+1.f && StageTime-DeltaSeconds<HangAt+1.f)
        {
            CameraYawAtHang=Chuck->FindComponentByClass<UCameraComponent>()->GetComponentRotation().Yaw;
            ShimmyX0=At.X; ShimmyZ0=At.Z; Chuck->SetTestStick(FVector2D(1,0));  // camera-relative right
        }
        if(HangAt>=0 && StageTime>=HangAt+2.2f && StageTime-DeltaSeconds<HangAt+2.2f) { ShimmyX1=At.X; ShimmyZ1=At.Z; }
        if(HangAt>=0 && StageTime>HangAt+6.f)
        {
            const bool bHanging=Chuck->IsHanging();
            UE_LOG(LogTemp,Display,TEXT("CHUCK_SHIMMY_MEASURE camera_yaw=%.2f moved_cm=%.2f dz=%.3f end=(%.1f,%.1f,%.2f) yaw=%.1f inner=%d outer=%d hanging=%d"),CameraYawAtHang,ShimmyX1-ShimmyX0,ShimmyZ1-ShimmyZ0,At.X,At.Y,At.Z,Chuck->GetActorRotation().Yaw,Chuck->GetInnerCorners()-InnerBase,Chuck->GetOuterCorners()-OuterBase,bHanging ? 1 : 0);
            Check(FMath::Abs(FMath::FindDeltaAngleDegrees(CameraYawAtHang,-90.f))<5.f,TEXT("hanging turns the camera to face the wall with him"));
            Check(FMath::Abs(ShimmyX1-ShimmyX0-1.2f*AChuckCharacter::ShimmySpeed)<12.f && FMath::Abs(ShimmyZ1-ShimmyZ0)<1.f,TEXT("stick sideways shimmies along the edge"));
            Check(bHanging && Chuck->GetInnerCorners()>InnerBase && FMath::Abs(At.Z-ShimmyZ0)<1.f,TEXT("shimmying into an inside corner turns onto the next wall, still hanging"));
            // Next: shimmy left to the harbour wall's west end and round the outside corner.
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-40,-305,36));
            Chuck->SetActorRotation(FRotator(0,-90,0)); Chuck->Recenter(); Chuck->SetTestStick(FVector2D(0,1));
            HangAt=-1; OuterBase=Chuck->GetOuterCorners(); ShimmyZ0=0; CornerYaw=999;
            TestStage=89; StageTime=0;
        }
    }
    else if(TestStage==89)
    {
        if(StageTime<.3f) Chuck->AddMovementInput(FVector(0,-1,0),1);
        if(StageTime>=.3f && StageTime-DeltaSeconds<.3f) Chuck->JumpPressed();
        if(Chuck->IsHanging() && HangAt<0) { HangAt=StageTime; Chuck->SetTestStick(FVector2D::ZeroVector); }
        if(HangAt>=0 && StageTime>=HangAt+.5f && StageTime-DeltaSeconds<HangAt+.5f) { ShimmyZ0=Chuck->GetActorLocation().Z; Chuck->SetTestStick(FVector2D(-1,0)); }
        // Just after the first outside corner (0.3 s turn): he faces the west end's side face (+X).
        if(CornerYaw>900 && Chuck->GetOuterCorners()>OuterBase) CornerAt=StageTime, CornerYaw=-999;
        if(CornerYaw<-900 && StageTime>=CornerAt+.35f) CornerYaw=Chuck->GetActorRotation().Yaw;
        if(HangAt>=0 && StageTime>HangAt+4.f)
        {
            const FVector At=Chuck->GetActorLocation();
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CORNER_MEASURE outer=%d yaw_after_turn=%.1f dz=%.3f hanging=%d end=(%.1f,%.1f)"),Chuck->GetOuterCorners()-OuterBase,CornerYaw,At.Z-ShimmyZ0,Chuck->IsHanging() ? 1 : 0,At.X,At.Y);
            Check(Chuck->GetOuterCorners()>OuterBase && FMath::Abs(FMath::FindDeltaAngleDegrees(CornerYaw,0.f))<10.f && Chuck->IsHanging() && FMath::Abs(At.Z-ShimmyZ0)<1.f,TEXT("shimmying off the end of a ledge goes round the outside corner"));
            // Next: into the cargo chimney side-on. Camera 20 degrees off the gap,
            // side jump (stick left) at stack A, then bounce with jump alone.
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-310,-337,36));
            Chuck->SetActorRotation(FRotator(0,-70,0)); Chuck->Recenter(); Chuck->SetActorRotation(FRotator(0,-90,0));
            Chuck->DodgeToward(FVector2D(-1,0));
            WallRunsBase=Chuck->GetWallRuns(); WallEnterAt=-1; WallSides.Reset(); ChimneyCamYaws.Reset(); HangAt=-1;
            bSideEntry=false; PrevGait.Reset(); PullUpsBase=Chuck->GetPullUps();
            TestStage=90; StageTime=0;
        }
    }
    else if(TestStage==90)
    {
        const FString G=Chuck->GetGaitName();
        const bool bRunning=Chuck->IsWallRunning();
        if(bRunning && WallEnterAt<0)
        {
            if(WallSides.Num()==0) bSideEntry=PrevGait==TEXT("SideJump");
            WallEnterAt=StageTime; WallSides.Add(Chuck->GetActorForwardVector().X<0 ? -1 : 1);
            ChimneyCamYaws.Add(Chuck->FindComponentByClass<UCameraComponent>()->GetComponentRotation().Yaw);
        }
        if(!bRunning) WallEnterAt=-1;
        if(bRunning && WallEnterAt>=0 && StageTime>=WallEnterAt+.25f && StageTime-DeltaSeconds<WallEnterAt+.25f && WallSides.Num()<=5) Chuck->JumpPressed();
        if(Chuck->IsHanging() && HangAt<0) HangAt=StageTime;
        if(HangAt>=0 && StageTime>=HangAt+.3f && StageTime-DeltaSeconds<HangAt+.3f) Chuck->JumpPressed();
        PrevGait=G;
        if(StageTime>5.f)
        {
            bool bSideOn=ChimneyCamYaws.Num()>=2;
            FString Yaws;
            for(int32 I=0; I<ChimneyCamYaws.Num(); ++I)
            {
                Yaws+=FString::Printf(TEXT("%.0f "),ChimneyCamYaws[I]);
                if(I>=1) bSideOn&=FMath::Abs(FMath::FindDeltaAngleDegrees(ChimneyCamYaws[I],-90.f))<10.f;
            }
            UE_LOG(LogTemp,Display,TEXT("CHUCK_SIDE_CHIMNEY_MEASURE side_entry=%d runs=%d camera_yaws=[%s] pullups=%d z=%.2f"),bSideEntry ? 1 : 0,WallSides.Num(),*Yaws,Chuck->GetPullUps()-PullUpsBase,Chuck->GetActorLocation().Z);
            Check(bSideEntry,TEXT("a side jump into a wall runs up it"));
            Check(bSideOn && WallSides.Num()>=3,TEXT("the chimney camera turns side-on and frames the bounce"));
            // Next: a fall from more than a short height lands in a roll.
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock();
            Chuck->SetActorLocation(FVector(-100,-560,36+220),false,nullptr,ETeleportType::TeleportPhysics);
            RollsBase=Chuck->GetLandingRolls(); bLocoFlag=false;
            TestStage=87; StageTime=0;
        }
    }
    else if(TestStage==87 || TestStage==88)
    {
        // 87: dropped from 220 cm: lands in a roll. 88: from 120 cm (under the 160 cm threshold): just lands.
        if(FCString::Strcmp(Chuck->GetGaitName(),TEXT("Roll"))==0) bLocoFlag=true;
        const int32 Rolls=Chuck->GetLandingRolls()-RollsBase;
        if(StageTime>1.8f) UE_LOG(LogTemp,Display,TEXT("CHUCK_FALL_MEASURE stage=%d rolls=%d rolled=%d gait=%s ground=%d"),TestStage,Rolls,bLocoFlag ? 1 : 0,Chuck->GetGaitName(),Chuck->GetCharacterMovement()->IsMovingOnGround() ? 1 : 0);
        if(StageTime>1.8f && TestStage==87)
        {
            Check(Rolls==1 && bLocoFlag && Chuck->GetCharacterMovement()->IsMovingOnGround(),TEXT("a fall of more than a short height lands in a roll"));
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-100,-560,36+120),false,nullptr,ETeleportType::TeleportPhysics);
            RollsBase=Chuck->GetLandingRolls(); bLocoFlag=false;
            TestStage=88; StageTime=0;
        }
        else if(StageTime>1.8f)
        {
            Check(Rolls==0 && !bLocoFlag && Chuck->GetCharacterMovement()->IsMovingOnGround(),TEXT("a drop under the roll height just lands, no roll"));
            // Next: strafe on the real keys. Hold Q (left), then Space. Clear quay:
            // left of the dock start is the practice yard's crate stack.
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,0,36));
            auto* KeyPC=Cast<APlayerController>(Chuck->GetController());
            Chuck->EnableInput(KeyPC); Chuck->SetLookLocked(true);
            KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Q,IE_Pressed,1));
            LocoEvaluations=-1; LocoSamples=0; LocoMaxSlip=0; LocoValue=0; bLocoFlag=false; bKeyMeasured=false; KeySide=0;
            StrafeJumpsBase=Chuck->GetStrafeJumps();
            TestStage=91; StageTime=0;
        }
    }
    else if(TestStage==91 || TestStage==92)
    {
        // 91: Q held (strafe walk left), Space: the short side jump.
        // 92: run latched, E held (strafe run right), Space: the long one.
        auto* KeyPC=Cast<APlayerController>(Chuck->GetController());
        const bool bRun=TestStage==92;
        const float Sign=bRun ? 1.f : -1.f;
        const bool bStrafing=Chuck->IsStrafing();
        if(StageTime>.6f && StageTime<1.2f && bStrafing)
        {
            ProbeLockedPaws(Chuck,DeltaSeconds);
            LocoValue=FMath::Max(LocoValue,static_cast<float>(FMath::Abs(Chuck->GetVelocity().Y)));
            bLocoFlag=bLocoFlag || FMath::Abs(Chuck->GetActorRotation().Yaw)>2.f;  // facing must hold
        }
        if(StageTime>=1.2f && StageTime-DeltaSeconds<1.2f)
        {
            KeySide=bStrafing ? 1.f : 0.f;  // still strafing when jump is pressed
            LocoPrevious=Chuck->GetActorLocation();
            KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar,IE_Pressed,1));
        }
        if(StageTime>=1.25f && StageTime-DeltaSeconds<1.25f) KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar,IE_Released,0));
        const bool bJumping=FCString::Strcmp(Chuck->GetGaitName(),TEXT("SideJump"))==0;
        if(StageTime>1.2f && bJumping && !bKeyMeasured && Chuck->GetCharacterMovement()->IsMovingOnGround() && Chuck->GetActorLocation().Z<LocoPrevious.Z+1.f && FVector::Dist2D(Chuck->GetActorLocation(),LocoPrevious)>20.f)
        {
            KeyJumpSide=static_cast<float>(Chuck->GetActorLocation().Y-LocoPrevious.Y)*Sign;
            bKeyMeasured=true;
        }
        if(StageTime>2.4f)
        {
            const float Speed=bRun ? ChuckClipData::StrafeRunSpeed : ChuckClipData::StrafeSpeed;
            const float Vz=bRun ? ChuckClipData::SideLongVerticalSpeed : ChuckClipData::SideShortVerticalSpeed;
            const float Authored=(bRun ? ChuckClipData::SideLongLateralSpeed : ChuckClipData::SideShortLateralSpeed)*2.f*Vz/(980.f*Chuck->GetCharacterMovement()->GravityScale);
            UE_LOG(LogTemp,Display,TEXT("CHUCK_STRAFE_MEASURE key=%s run=%d speed_cm_s=%.2f authored_cm_s=%.2f yaw_moved=%d samples=%d max_cm_s=%.4f strafing_at_jump=%d jump_cm=%.2f authored_jump_cm=%.2f long=%d back_to_strafe=%d"),
                bRun ? TEXT("E") : TEXT("Q"),bRun ? 1 : 0,LocoValue,Speed,bLocoFlag ? 1 : 0,LocoSamples,LocoMaxSlip,KeySide>0 ? 1 : 0,KeyJumpSide,Authored,Chuck->WasLongSideJump() ? 1 : 0,bStrafing ? 1 : 0);
            Check(KeySide>0 && !bLocoFlag && FMath::Abs(LocoValue-Speed)<4.f && LocoSamples>=5 && LocoMaxSlip<1.f,
                bRun ? TEXT("E with run latched: strafe run facing the camera, paws holding") : TEXT("Q held: strafe walk facing the camera, paws holding"));
            Check(bKeyMeasured && FMath::Abs(KeyJumpSide-Authored)<15.f && Chuck->WasLongSideJump()==bRun,
                bRun ? TEXT("jump while strafe-running is the long side jump") : TEXT("jump while strafe-walking is the short side jump"));
            if(!bRun)
            {
                Check(bStrafing && Chuck->GetStrafeJumps()==StrafeJumpsBase+1,TEXT("after the side jump he strafes on while Q is held"));
                KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Q,IE_Released,0));
                Chuck->ResetToDock(); Chuck->SetRunHeld(true);  // rightward from the dock start (it passed there)
                KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1));
                LocoEvaluations=-1; LocoSamples=0; LocoMaxSlip=0; LocoValue=0; bLocoFlag=false; bKeyMeasured=false; KeySide=0; KeyJumpSide=0;
                TestStage=92; StageTime=0;
            }
            else
            {
                KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0));
                // Next: running forward on W, press Q and Space on the same frame.
                Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,0,36)); Chuck->SetRunHeld(true);
                KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W,IE_Pressed,1));
                bKeyMeasured=false; bLocoFlag=false; KeyJumpSide=0; KeySide=0; LocoValue=0; StrafeJumpsBase=Chuck->GetStrafeJumps();
                TestStage=94; StageTime=0;
            }
        }
    }
    else if(TestStage==94)
    {
        auto* KeyPC=Cast<APlayerController>(Chuck->GetController());
        const bool bJumping=FCString::Strcmp(Chuck->GetGaitName(),TEXT("SideJump"))==0;
        if(StageTime>=1.f && StageTime-DeltaSeconds<1.f)
        {
            LocoValue=Chuck->GetVelocity().Size2D();   // speed when the keys go down
            LocoPrevious=Chuck->GetActorLocation();
            KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Q,IE_Pressed,1));
            KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar,IE_Pressed,1));
        }
        if(StageTime>=1.05f && StageTime-DeltaSeconds<1.05f) KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar,IE_Released,0));
        if(StageTime>1.f && StageTime<1.2f && bJumping) bLocoFlag=true;     // side jump straight away
        if(StageTime>1.f && bJumping && !bKeyMeasured && Chuck->GetCharacterMovement()->IsMovingOnGround() && FVector::Dist2D(Chuck->GetActorLocation(),LocoPrevious)>20.f)
        {
            KeyJumpSide=static_cast<float>(LocoPrevious.Y-Chuck->GetActorLocation().Y);   // leftward = -Y (camera yaw 0)
            KeySide=static_cast<float>(Chuck->GetActorLocation().X-LocoPrevious.X);       // forward drift
            bKeyMeasured=true;
        }
        if(StageTime>2.4f)
        {
            const float Authored=ChuckClipData::SideLongLateralSpeed*2.f*ChuckClipData::SideLongVerticalSpeed/(980.f*Chuck->GetCharacterMovement()->GravityScale);
            UE_LOG(LogTemp,Display,TEXT("CHUCK_FORWARD_STRAFE_JUMP_MEASURE speed_at_press_cm_s=%.1f side_jump_at_once=%d side_cm=%.2f authored_cm=%.2f forward_cm=%.2f long=%d strafe_jumps=%d"),
                LocoValue,bLocoFlag ? 1 : 0,KeyJumpSide,Authored,KeySide,Chuck->WasLongSideJump() ? 1 : 0,Chuck->GetStrafeJumps()-StrafeJumpsBase);
            Check(LocoValue>.9f*ChuckClipData::RunSpeed && bLocoFlag && bKeyMeasured && FMath::Abs(KeyJumpSide-Authored)<15.f && FMath::Abs(KeySide)<10.f && Chuck->WasLongSideJump(),
                TEXT("running forward, strafe + jump pressed together is a side jump (sideways, not diagonal)"));
            KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Q,IE_Released,0));
            KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W,IE_Released,0));
            KeyPC->FlushPressedKeys(); Chuck->DisableInput(KeyPC); Chuck->SetLookLocked(false);
            {
                // Next: walk gently off the harbour wall's north face: he grabs it.
                Chuck->SetRunHeld(false); Chuck->ResetToDock();
                Chuck->SetActorLocation(FVector(-40,-350,115+32.5f),false,nullptr,ETeleportType::TeleportPhysics);
                Chuck->SetActorRotation(FRotator(0,90,0)); Chuck->Recenter(); Chuck->SetTestStick(FVector2D(0,1));
                HangAt=-1; DropHangsBase=Chuck->GetDropHangs(); PullUpsBase=Chuck->GetPullUps(); CornerYaw=999;
                TestStage=93; StageTime=0;
            }
        }
    }
    else if(TestStage==93)
    {
        // Stick held forward (away from the wall) the whole way: it must not
        // let go or climb until released; then jump climbs back up.
        if(HangAt<0) Chuck->AddMovementInput(FVector(0,1,0),1);
        if(Chuck->IsHanging() && HangAt<0) HangAt=StageTime;
        if(HangAt>=0 && StageTime>=HangAt+.6f && StageTime-DeltaSeconds<HangAt+.6f) CornerYaw=Chuck->GetActorRotation().Yaw, ShimmyZ0=Chuck->GetActorLocation().Z;
        if(HangAt>=0 && StageTime>=HangAt+1.2f && StageTime-DeltaSeconds<HangAt+1.2f)
        {
            bLocoFlag=Chuck->IsHanging();   // still hanging with the stick held
            Chuck->SetTestStick(FVector2D::ZeroVector);
        }
        if(HangAt>=0 && StageTime>=HangAt+1.4f && StageTime-DeltaSeconds<HangAt+1.4f) Chuck->JumpPressed();
        if(StageTime>4.f)
        {
            const FVector At=Chuck->GetActorLocation();
            UE_LOG(LogTemp,Display,TEXT("CHUCK_DROP_HANG_MEASURE drop_hangs=%d hang_yaw=%.1f hang_z=%.2f held=%d pullups=%d end_z=%.2f"),Chuck->GetDropHangs()-DropHangsBase,CornerYaw,ShimmyZ0,bLocoFlag ? 1 : 0,Chuck->GetPullUps()-PullUpsBase,At.Z);
            Check(Chuck->GetDropHangs()==DropHangsBase+1 && FMath::Abs(FMath::FindDeltaAngleDegrees(CornerYaw,-90.f))<5.f && FMath::Abs(ShimmyZ0-(115.f-ChuckClipData::HangDrop))<2.f,TEXT("walking gently off an edge turns round and hangs from it"));
            Check(bLocoFlag && Chuck->GetPullUps()>PullUpsBase && FMath::Abs(At.Z-(115.f+32.5f))<3.f,TEXT("the stick that walked him off is ignored until released; then he climbs back"));
            // Next: grass. A tuft 30 cm ahead (slashed), one 60 cm behind (left
            // alone) and one 110 cm ahead (walked through), on clear quay.
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,-20,36));
            TestTufts.Reset();
            for(const float Dx : {30.f,-60.f,110.f}) TestTufts.Add(AGrassTuft::Plant(GetWorld(),FVector2D(-240+Dx,-20),0,0.f,1.f));
            if(TestTufts[0].IsValid()) TestTufts[0]->Cigarettes=1;
            BreaksBase=Chuck->GetSlashBreaks(); LocoValue=0; bLocoFlag=false; KeySide=0; PickupsBase=ACigarettePickup::CountInWorld(GetWorld()); CigsBase=Chuck->GetPickupsCollected();
            TestStage=95; StageTime=0;
        }
    }
    else if(TestStage==95)
    {
        const bool bPlanted=TestTufts.Num()==3 && TestTufts[0].IsValid() && TestTufts[1].IsValid() && TestTufts[2].IsValid();
        if(StageTime>=.2f && StageTime-DeltaSeconds<.2f) Chuck->Slash();
        if(StageTime>=.25f && StageTime-DeltaSeconds<.25f) Chuck->SlashReleased();
        if(StageTime>=.3f && StageTime-DeltaSeconds<.3f) bLocoFlag=FString(Chuck->GetSlashName()).Contains(TEXT("Low"));
        if(StageTime>=1.f && StageTime-DeltaSeconds<1.f) KeySide=ACigarettePickup::CountInWorld(GetWorld())-PickupsBase+Chuck->GetPickupsCollected()-CigsBase;   // out, or already pocketed
        if(bPlanted) LocoValue=FMath::Max(LocoValue,static_cast<float>(TestTufts[0]->GetClippingsFlying()));
        if(StageTime>=1.2f && StageTime<3.2f) Chuck->AddMovementInput(FVector(1,0,0),1);   // walk on through the far tuft
        if(StageTime>3.4f)
        {
            const bool bCut=bPlanted && TestTufts[0]->IsBroken();
            const bool bBehind=bPlanted && !TestTufts[1]->IsBroken();
            const bool bThrough=bPlanted && !TestTufts[2]->IsBroken() && Chuck->GetActorLocation().X>TestTufts[2]->GetActorLocation().X+20.f;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_GRASS_MEASURE planted=%d tufts_in_play=%d low_rake=%d cut=%d clippings=%.0f sound=%d cigarettes_out=%.0f behind_intact=%d walked_through=%d breaks=%d x=%.1f"),
                bPlanted ? 1 : 0,AChuckBreakable::All().Num(),bLocoFlag ? 1 : 0,bCut ? 1 : 0,LocoValue,bPlanted && TestTufts[0]->PlayedShredSound() ? 1 : 0,KeySide,bBehind ? 1 : 0,bThrough ? 1 : 0,Chuck->GetSlashBreaks()-BreaksBase,Chuck->GetActorLocation().X);
            Check(bLocoFlag && bCut && LocoValue>=10 && TestTufts[0]->PlayedShredSound() && Chuck->GetSlashBreaks()==BreaksBase+1 && KeySide==1,
                TEXT("a low rake shreds the grass tuft in front: stubble, a spray of clippings, a rustle, and its cigarette pops out"));
            Check(bBehind && bThrough && AChuckBreakable::All().Num()>40,TEXT("tufts outside the swing are untouched, Chuck walks through grass, and the docks are planted"));
            for(auto& Tuft : TestTufts) if(Tuft.IsValid()) Tuft->Destroy();
            TestTufts.Reset();
            for(TActorIterator<ACigarettePickup> It(GetWorld()); It; ++It) It->Destroy();
            // Next: a cigarette lying 60 cm ahead: walk over it to pocket it.
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,-20,36));
            ACigarettePickup::Spawn(GetWorld(),FVector(-180,-20,1),FVector::ZeroVector,0.f);
            PickupsBase=Chuck->GetCigarettes(); LocoValue=0;
            TestStage=97; StageTime=0;
        }
    }
    else if(TestStage==97)
    {
        if(StageTime>=.5f && StageTime<2.f) Chuck->AddMovementInput(FVector(1,0,0),1);
        if(StageTime>2.2f)
        {
            const int32 Left=ACigarettePickup::CountInWorld(GetWorld());
            UE_LOG(LogTemp,Display,TEXT("CHUCK_PICKUP_MEASURE collected=%d left_in_world=%d count=%d x=%.1f"),Chuck->GetCigarettes()-PickupsBase,Left,Chuck->GetCigarettes(),Chuck->GetActorLocation().X);
            Check(Chuck->GetCigarettes()==PickupsBase+1 && Left==0,TEXT("walking over a cigarette pockets it and the counter goes up"));
            // Next: a clay jar 40 cm ahead. Walk into it (blocked, no hop onto
            // it), then slash it: shards, a crack, two cigarettes.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,-20,36));
            TestJar=AClayJar::Place(GetWorld(),FVector2D(-240+40,-20),0.f,2);
            BreaksBase=Chuck->GetSlashBreaks(); MantlesBase=Chuck->GetMantles(); LocoValue=0; bLocoFlag=false; KeySide=0;
            PickupsBase=ACigarettePickup::CountInWorld(GetWorld()); CigsBase=Chuck->GetPickupsCollected();
            TestStage=98; StageTime=0;
        }
    }
    else if(TestStage==98)
    {
        const bool bJar=TestJar.IsValid();
        if(StageTime<1.f) Chuck->AddMovementInput(FVector(1,0,0),1);
        if(StageTime>=1.f && StageTime-DeltaSeconds<1.f) LocoValue=Chuck->GetActorLocation().X;   // stopped at the jar
        if(StageTime>=1.2f && StageTime-DeltaSeconds<1.2f) { Chuck->Slash(); Chuck->SlashReleased(); }
        if(StageTime>=1.3f && StageTime-DeltaSeconds<1.3f) bLocoFlag=FString(Chuck->GetSlashName()).Contains(TEXT("Low"));
        if(bJar && StageTime>1.2f) KeySide=FMath::Max(KeySide,static_cast<float>(TestJar->GetShardsFlying()));
        if(StageTime>=2.f && StageTime-DeltaSeconds<2.f) KeyJumpSide=ACigarettePickup::CountInWorld(GetWorld())-PickupsBase+Chuck->GetPickupsCollected()-CigsBase;
        if(StageTime>=2.2f && StageTime<3.4f) Chuck->AddMovementInput(FVector(1,0,0),1);   // on through where it stood
        if(StageTime>3.6f)
        {
            const float JarX=-200.f;
            const bool bBlocked=LocoValue<JarX-ClayJarData::Radius-10.f && Chuck->GetMantles()==MantlesBase;
            const bool bBroken=bJar && TestJar->IsBroken();
            UE_LOG(LogTemp,Display,TEXT("CHUCK_JAR_MEASURE blocked_at_x=%.1f mantles=%d low_rake=%d broken=%d shards=%.0f sound=%d cigarettes_out=%.0f passed=%d x=%.1f"),
                LocoValue,Chuck->GetMantles()-MantlesBase,bLocoFlag ? 1 : 0,bBroken ? 1 : 0,KeySide,bJar && TestJar->PlayedBreakSound() ? 1 : 0,KeyJumpSide,Chuck->GetActorLocation().X>JarX+20.f ? 1 : 0,Chuck->GetActorLocation().X);
            Check(bBlocked,TEXT("a clay jar is solid: Chuck stops at it and doesn't hop onto it"));
            Check(bLocoFlag && bBroken && KeySide==ClayJarData::ShardCount && TestJar->PlayedBreakSound() && KeyJumpSide==2 && Chuck->GetActorLocation().X>JarX+20.f,
                TEXT("a low rake breaks the jar into shards with a crack, its cigarettes pop out, and the way is clear"));
            if(bJar) TestJar->Destroy();
            for(TActorIterator<ACigarettePickup> It(GetWorld()); It; ++It) It->Destroy();
            // Next: a rat 150 cm ahead. Chuck stands still: it should notice
            // him, come in, give its tell (crouch and hiss), lunge and bite.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,-20,36));
            TestRat=AEnemyRat::Place(GetWorld(),FVector2D(-90,-20),180.f);
            BitesBase=Chuck->GetBitesTaken(); LocoPrevious=Chuck->GetActorLocation(); LocoValue=0; KeySide=-1; KeyJumpSide=0; bLocoFlag=false;
            TestStage=101; StageTime=0;
        }
    }
    else if(TestStage==101)
    {
        const bool bRat=TestRat.IsValid();
        if(bRat)
        {
            const FString RatState=bRat ? TestRat->GetStateName() : TEXT("");
            if(RatState==TEXT("Chase")) bLocoFlag=true;
            if(KeySide<0 && Chuck->GetBitesTaken()>BitesBase) { KeySide=StageTime; KeyJumpSide=TestRat->GetLastWindupSeconds(); LocoPrevious=Chuck->GetActorLocation(); }
            if(KeySide>=0 && StageTime<KeySide+.6f) LocoValue=FMath::Max(LocoValue,static_cast<float>(FVector::Dist2D(Chuck->GetActorLocation(),LocoPrevious)));
        }
        if(StageTime>(KeySide>=0 ? KeySide+.8f : 5.f))
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_RAT_ATTACK_MEASURE chased=%d bitten_at_s=%.2f windup_s=%.2f bites=%d knockback_cm=%.1f"),bLocoFlag ? 1 : 0,KeySide,KeyJumpSide,Chuck->GetBitesTaken()-BitesBase,LocoValue);
            Check(bLocoFlag && KeySide>0 && KeyJumpSide>=AEnemyRat::WindupTime-.02f && Chuck->GetBitesTaken()==BitesBase+1 && LocoValue>25.f,
                TEXT("a rat notices Chuck, comes in, gives its tell and lunges; the bite knocks him back"));
            if(bRat) TestRat->Destroy();
            // Next: a fresh rat 45 cm ahead: slash it whenever it's in reach.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,-20,36));
            TestRat=AEnemyRat::Place(GetWorld(),FVector2D(-195,-20),180.f);
            if(TestRat.IsValid()) TestRat->Cigarettes=1;
            PickupsBase=ACigarettePickup::CountInWorld(GetWorld()); CigsBase=Chuck->GetPickupsCollected();
            RatHitsBase=Chuck->GetSlashRatHits(); LocoValue=-1; bLocoFlag=false; KeySide=0; KeyJumpSide=0; bKeyMeasured=false;
            TestStage=102; StageTime=0;
        }
    }
    else if(TestStage==102)
    {
        const bool bRat=TestRat.IsValid();
        // Slash when it's within reach in front (the test aims for him: Chuck faces it).
        if(bRat && !TestRat->IsDead())
        {
            const FVector To=TestRat->GetActorLocation()-Chuck->GetActorLocation();
            Chuck->SetActorRotation(FRotator(0,To.Rotation().Yaw,0));
            if(To.Size2D()<AChuckCharacter::SlashReach+AEnemyRat::HitRadius+4.f && StageTime-LocoValue>.5f && LocoValue<StageTime)
            {
                Chuck->Slash(); Chuck->SlashReleased(); LocoValue=StageTime;
                bKeyMeasured=true;
            }
        }
        if(bKeyMeasured && StageTime-LocoValue>=.05f && StageTime-LocoValue<.05f+DeltaSeconds*1.5f) bLocoFlag=bLocoFlag || FString(Chuck->GetSlashName()).Contains(TEXT("Low"));
        if(bRat && TestRat->IsDead() && KeySide==0) { KeySide=StageTime; KeyJumpSide=TestRat->GetHitsTaken(); }
        if(StageTime>(KeySide>0 ? KeySide+2.6f : 8.f))
        {
            const int32 Out=ACigarettePickup::CountInWorld(GetWorld())-PickupsBase+Chuck->GetPickupsCollected()-CigsBase;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_RAT_KILL_MEASURE killed_at_s=%.2f hits=%.0f rat_hits=%d low_rake=%d cigarettes_out=%d rat_removed=%d"),KeySide,KeyJumpSide,Chuck->GetSlashRatHits()-RatHitsBase,bLocoFlag ? 1 : 0,Out,TestRat.IsValid() ? 0 : 1);
            Check(KeySide>0 && KeyJumpSide==AEnemyRat::Health && bLocoFlag && Out==1 && !TestRat.IsValid(),
                TEXT("two slashes (low rakes) kill a rat; it drops a cigarette and is cleared away"));
            if(TestRat.IsValid()) TestRat->Destroy();
            for(TActorIterator<ACigarettePickup> It(GetWorld()); It; ++It) It->Destroy();
            // Next: Sanity. A bite costs one; a cigarette refills it; the next counts.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,-20,36));
            CigsBase=Chuck->GetCigarettes(); LocoValue=0; KeySide=0; KeyJumpSide=0;
            TestStage=104; StageTime=0;
        }
    }
    else if(TestStage==104)
    {
        if(StageTime>=.1f && StageTime-DeltaSeconds<.1f) { Chuck->TakeBite(Chuck->GetActorLocation()+FVector(30,0,0)); LocoValue=Chuck->GetSanity(); }
        if(StageTime>=1.2f && StageTime-DeltaSeconds<1.2f) ACigarettePickup::Spawn(GetWorld(),Chuck->GetActorLocation()-FVector(0,0,34.f),FVector::ZeroVector,static_cast<float>(Chuck->GetActorLocation().Z)-35.f);
        if(StageTime>=1.8f && StageTime-DeltaSeconds<1.8f) { KeySide=Chuck->GetSanity(); KeyJumpSide=Chuck->GetCigarettes()-CigsBase; ACigarettePickup::Spawn(GetWorld(),Chuck->GetActorLocation()-FVector(0,0,34.f),FVector::ZeroVector,static_cast<float>(Chuck->GetActorLocation().Z)-35.f); }
        if(StageTime>2.5f)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_SANITY_MEASURE after_bite=%.0f after_first=%.0f counted_after_first=%.0f after_second=%d counted_after_second=%d max=%d"),LocoValue,KeySide,KeyJumpSide,Chuck->GetSanity(),Chuck->GetCigarettes()-CigsBase,AChuckCharacter::MaxSanity);
            Check(LocoValue==AChuckCharacter::MaxSanity-1 && KeySide==AChuckCharacter::MaxSanity && KeyJumpSide==0 && Chuck->GetSanity()==AChuckCharacter::MaxSanity && Chuck->GetCigarettes()==CigsBase+1,
                TEXT("a bite costs a cigarette of Sanity; a cigarette refills the bar, and once it's full they count up"));
            // Next: the last point of Sanity lost on the wharf: he vanishes and is summoned back at the start.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-100,-560,36)); Chuck->SetSanity(1);
            AstralBase=AAstralSummon::GetStarted(); RespawnsBase=Chuck->GetRespawns(); AstralSeen=0; LocoValue=-1; LocoPrevious=FVector::ZeroVector;
            TestStage=105; StageTime=0;
        }
    }
    else if(TestStage==105)
    {
        if(StageTime>=.1f && StageTime-DeltaSeconds<.1f) Chuck->TakeBite(Chuck->GetActorLocation()+FVector(0,30,0));
        const FString Phase=Chuck->GetAstralName();
        if(Phase==TEXT("Vanishing")) AstralSeen|=1;
        if(Phase==TEXT("Away")) AstralSeen|=2;
        if(Phase==TEXT("Summoning")) AstralSeen|=4;
        // Back: then he must walk off under his own power.
        if(LocoValue<0 && AstralSeen==7 && Phase==TEXT("None") && Chuck->GetRespawns()>RespawnsBase)
        {
            LocoValue=StageTime; LocoPrevious=Chuck->GetActorLocation();
            KeySide=static_cast<float>(FVector::Dist2D(Chuck->GetActorLocation(),AChuckCharacter::StartLocation()));
        }
        if(LocoValue>=0 && StageTime<LocoValue+.8f) Chuck->AddMovementInput(FVector(1,0,0),1);
        if(StageTime>(LocoValue>=0 ? LocoValue+1.f : 9.f))
        {
            const float Walked=LocoValue>=0 ? static_cast<float>(FVector::Dist2D(Chuck->GetActorLocation(),LocoPrevious)) : 0.f;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_ASTRAL_MEASURE phases=%d fx=%d respawned_at_s=%.2f from_start_cm=%.2f sanity=%d visible=%d walked_cm=%.1f"),
                AstralSeen,AAstralSummon::GetStarted()-AstralBase,LocoValue,KeySide,Chuck->GetSanity(),Chuck->GetMesh()->IsVisible() ? 1 : 0,Walked);
            Check(AstralSeen==7 && AAstralSummon::GetStarted()-AstralBase==2 && LocoValue>0 && KeySide<5.f && Chuck->GetSanity()==AChuckCharacter::MaxSanity && Walked>20.f,
                TEXT("at zero Sanity Chuck vanishes into astral light and is summoned back at the start, whole, and walks on"));
            // Next: the dock worker. Chuck 1.3 m in front of him: he looks down
            // at the rat; then far off: he looks away. He's solid, not climbable.
            Worker=nullptr;
            for(const TWeakObjectPtr<ADockNPC>& Entry : ADockNPC::All()) if(Entry.IsValid() && Entry->ActorHasTag(TEXT("DockWorkerArt"))) Worker=Entry;
            Chuck->ResetToDock();
            if(Worker.IsValid()) Chuck->SetActorLocation(Worker->GetActorLocation()+Worker->GetActorForwardVector()*130.f-FVector(0,0,Worker->GetActorLocation().Z-36.f));
            Chuck->SetActorRotation((-(Worker.IsValid() ? Worker->GetActorForwardVector() : FVector::ForwardVector)).Rotation());
            LocoValue=0; KeySide=0; KeyJumpSide=0; bLocoFlag=false; WallRunsBase=Chuck->GetWallRuns(); MantlesBase=Chuck->GetMantles();
            TestStage=108; StageTime=0;
        }
    }
    else if(TestStage==108)
    {
        const bool bWorker=Worker.IsValid();
        if(bWorker && StageTime>=1.5f && StageTime-DeltaSeconds<1.5f) { LocoValue=Worker->GetLookAngles().Y; bLocoFlag=Worker->IsWatchingChuck(); }
        // Then walk into him and jump at him: blocked, no wall run, no mantle.
        if(bWorker && StageTime>=1.6f && StageTime<3.2f) Chuck->AddMovementInput((Worker->GetActorLocation()-Chuck->GetActorLocation()).GetSafeNormal2D(),1);
        if(StageTime>=2.6f && StageTime-DeltaSeconds<2.6f) Chuck->JumpPressed();
        if(bWorker && StageTime>=3.4f && StageTime-DeltaSeconds<3.4f)
        {
            KeySide=FVector::Dist2D(Chuck->GetActorLocation(),Worker->GetActorLocation());
            Chuck->ResetToDock(); Chuck->SetActorLocation(Worker->GetActorLocation()+Worker->GetActorForwardVector()*800.f-FVector(0,0,Worker->GetActorLocation().Z-36.f));
        }
        if(bWorker && StageTime>=5.2f && StageTime-DeltaSeconds<5.2f) KeyJumpSide=Worker->IsWatchingChuck() ? 1.f : 0.f;
        // Then the rat waits off to his left (his right is the harbour): he
        // turns his body to it.
        if(bWorker && StageTime>=5.4f && StageTime-DeltaSeconds<5.4f)
        {
            Chuck->ResetToDock();
            Chuck->SetActorLocation(Worker->GetActorLocation()-Worker->GetActorRightVector()*170.f-FVector(0,0,Worker->GetActorLocation().Z-36.f));
        }
        if(StageTime>8.6f)
        {
            int32 Moving=0;
            for(const TWeakObjectPtr<ADockNPC>& Entry : ADockNPC::All()) if(Entry.IsValid() && Entry->HasMocap()) ++Moving;
            const float Turned=bWorker ? Worker->GetBodyTurn() : 0.f;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_HUMAN_LIFE_MEASURE mocap=%d worker_turn_deg=%.1f"),Moving,Turned);
            Check(Moving>=3 && FMath::Abs(Turned)>60.f,TEXT("the townsfolk move with motion capture, and the worker turns his body to a rat at his side"));
            UE_LOG(LogTemp,Display,TEXT("CHUCK_WORKER_MEASURE present=%d watching_near=%d look_down_deg=%.1f blocked_at_cm=%.1f wall_runs=%d mantles=%d watching_far=%.0f talkable=%d"),
                bWorker ? 1 : 0,bLocoFlag ? 1 : 0,LocoValue,KeySide,Chuck->GetWallRuns()-WallRunsBase,Chuck->GetMantles()-MantlesBase,KeyJumpSide,bWorker && Worker->CanTalk() ? 1 : 0);
            Check(bWorker && bLocoFlag && LocoValue>15.f && KeyJumpSide==0.f,TEXT("the dock worker watches the rat when he's near, looking down at him, and looks away when he's gone"));
            Check(bWorker && KeySide>24.f+14.f && KeySide<24.f+15.f+12.f && Chuck->GetWallRuns()==WallRunsBase && Chuck->GetMantles()==MantlesBase,
                TEXT("the worker is solid to Chuck but can't be run up or climbed"));
            // Every human stands with arms down: not the model's A-pose (hands
            // ~45 cm out), not held out in front like a sleepwalker (40+ cm
            // ahead). Motion-capture hands clasped or on a hip sit ~17 cm ahead.
            int32 Humans=0; bool bPosed=true;
            for(const TWeakObjectPtr<ADockNPC>& Entry : ADockNPC::All())
            {
                if(!Entry.IsValid() || Entry==TalkNPC) continue;
                const float Out=Entry->GetWiderHandReach(), Ahead=Entry->GetHandsForward();
                UE_LOG(LogTemp,Display,TEXT("CHUCK_HUMAN_POSE_MEASURE who=%s hand_out_cm=%.1f hand_ahead_cm=%.1f"),*Entry->DisplayName,Out,Ahead);
                bPosed &= Out>10.f && Out<34.f && Ahead<25.f; ++Humans;
            }
            Check(Humans==3 && bPosed,TEXT("the worker, guard and market woman stand with their arms down by their sides, not in the A-pose or held out in front"));
            // The 2D game's townsfolk, where it put them, with its lines.
            TArray<AActor*> GuardFound, WomanFound;
            UGameplayStatics::GetAllActorsWithTag(this,TEXT("DockGuard"),GuardFound);
            UGameplayStatics::GetAllActorsWithTag(this,TEXT("MarketWoman"),WomanFound);
            const auto* GuardNPC=GuardFound.Num()==1 ? Cast<ADockNPC>(GuardFound[0]) : nullptr;
            const auto* WomanNPC=WomanFound.Num()==1 ? Cast<ADockNPC>(WomanFound[0]) : nullptr;
            Check(GuardNPC && WomanNPC && GuardNPC->Lines.Num()==1 && GuardNPC->Lines[0]==TEXT("Stick to the docks, rat.")
                && WomanNPC->Lines.Num()==1 && WomanNPC->Lines[0].Contains(TEXT("check the sewer for scraps"))
                && GuardNPC->GetActorLocation().Y<-3900.f && FVector::Dist2D(WomanNPC->GetActorLocation(),FVector(-25,-1240,0))<250.f,
                TEXT("the guard stands at the city gate and the market woman by the red market stalls, each with the 2D game's line"));
            // Next: talk, on the real keys, with a stand-in NPC who has lines.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,-20,36));
            TalkNPC=GetWorld()->SpawnActor<ADockNPC>(FVector(-240+100,-20,90),FRotator(0,180,0));
            if(TalkNPC.IsValid()) { TalkNPC->DisplayName=TEXT("Stand-in"); TalkNPC->Lines={TEXT("First line."),TEXT("Second line.")}; }
            auto* KeyPC=Cast<APlayerController>(Chuck->GetController());
            Chuck->EnableInput(KeyPC); Chuck->SetLookLocked(true);
            TalkSeen=0; LocoPrevious=Chuck->GetActorLocation();
            TestStage=109; StageTime=0;
        }
    }
    else if(TestStage==109)
    {
        auto* KeyPC=Cast<APlayerController>(Chuck->GetController());
        auto Press=[&](const FKey& Key,float At){ if(StageTime>=At && StageTime-DeltaSeconds<At) KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,IE_Pressed,1)); if(StageTime>=At+.05f && StageTime-DeltaSeconds<At+.05f) KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,IE_Released,0)); };
        FString Speaker, Line;
        if(StageTime>=.4f && StageTime-DeltaSeconds<.4f && Chuck->GetTalkPrompt()==TalkNPC.Get()) TalkSeen|=1;   // prompt in reach
        Press(EKeys::F,.5f);
        if(StageTime>=.7f && StageTime-DeltaSeconds<.7f && Chuck->GetDialogue(Speaker,Line) && Line==TEXT("First line.")) TalkSeen|=2;
        // Trying to walk off mid-line does nothing.
        if(StageTime>=.8f && StageTime<1.2f) KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::S,StageTime-DeltaSeconds<.8f ? IE_Pressed : IE_Repeat,1));
        if(StageTime>=1.2f && StageTime-DeltaSeconds<1.2f) { KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::S,IE_Released,0)); if(FVector::Dist2D(Chuck->GetActorLocation(),LocoPrevious)<2.f) TalkSeen|=4; }
        Press(EKeys::F,1.4f);
        if(StageTime>=1.6f && StageTime-DeltaSeconds<1.6f && Chuck->GetDialogue(Speaker,Line) && Line==TEXT("Second line.") && Speaker==TEXT("Stand-in")) TalkSeen|=8;
        Press(EKeys::F,1.8f);
        if(StageTime>=2.f && StageTime-DeltaSeconds<2.f && !Chuck->IsTalking()) TalkSeen|=16;
        if(StageTime>2.2f)
        {
            // The silent worker: no prompt beside him.
            bool bSilent=true;
            for(const TWeakObjectPtr<ADockNPC>& Entry : ADockNPC::All()) if(Entry.IsValid() && Entry->ActorHasTag(TEXT("DockWorkerArt")) && Entry->CanTalk()) bSilent=false;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_TALK_MEASURE steps=%d worker_silent=%d"),TalkSeen,bSilent ? 1 : 0);
            Check(TalkSeen==31 && bSilent,TEXT("F / Y talks to an NPC with lines: a prompt in reach, lines advance, Chuck stays put, then it closes; the silent worker has none"));
            if(TalkNPC.IsValid()) TalkNPC->Destroy();
            KeyPC->FlushPressedKeys(); Chuck->DisableInput(KeyPC); Chuck->SetLookLocked(false);
            // Next: the cargo wharf.
            // Next: the cargo wharf. Run up the warehouse's stone plinth and climb onto it.
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-180,-610,36));
            Chuck->SetActorRotation(FRotator(0,-90,0)); Chuck->Recenter(); Chuck->SetTestStick(FVector2D(0,1));
            PullUpsBase=Chuck->GetPullUps(); HangAt=-1; WallEnterAt=-1;
            TestStage=82; StageTime=0;
        }
    }
    else if(TestStage==82)
    {
        if(StageTime<.3f) Chuck->AddMovementInput(FVector(0,-1,0),1);
        if(StageTime>=.3f && StageTime-DeltaSeconds<.3f) Chuck->JumpPressed();
        if(FCString::Strcmp(Chuck->GetGaitName(),TEXT("Climb"))==0) Chuck->SetTestStick(FVector2D::ZeroVector);
        if(StageTime>3.f)
        {
            const FVector At=Chuck->GetActorLocation();
            const bool bGround=Chuck->GetCharacterMovement()->IsMovingOnGround();
            UE_LOG(LogTemp,Display,TEXT("CHUCK_WHARF_MEASURE route=plinth pullups=%d z=%.2f ground=%d"),Chuck->GetPullUps()-PullUpsBase,At.Z,bGround ? 1 : 0);
            Check(Chuck->GetPullUps()>PullUpsBase && FMath::Abs(At.Z-(115.f+32.5f))<3.f && bGround,TEXT("the warehouse's stone plinth is a ledge he climbs onto"));
            // Next: bounce up the alley between the warehouse and the sail loft.
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-5,-760,36));
            Chuck->SetActorRotation(FRotator(0,180,0)); Chuck->Recenter(); Chuck->SetTestStick(FVector2D(0,1));
            PullUpsBase=Chuck->GetPullUps(); HangAt=-1; WallEnterAt=-1;
            TestStage=83; StageTime=0;
        }
    }
    else if(TestStage==83)
    {
        if(StageTime>=.1f && StageTime-DeltaSeconds<.1f) Chuck->JumpPressed();
        const bool bRunning=Chuck->IsWallRunning();
        if(bRunning && WallEnterAt<0) { WallEnterAt=StageTime; Chuck->SetTestStick(FVector2D::ZeroVector); }
        if(!bRunning) WallEnterAt=-1;
        if(bRunning && WallEnterAt>=0 && StageTime>=WallEnterAt+.25f && StageTime-DeltaSeconds<WallEnterAt+.25f) Chuck->JumpPressed();
        if(Chuck->IsHanging() && HangAt<0) HangAt=StageTime;
        if(HangAt>=0 && StageTime>=HangAt+.3f && StageTime-DeltaSeconds<HangAt+.3f) Chuck->JumpPressed();
        if(StageTime>5.f)
        {
            const FVector At=Chuck->GetActorLocation();
            const bool bGround=Chuck->GetCharacterMovement()->IsMovingOnGround();
            UE_LOG(LogTemp,Display,TEXT("CHUCK_WHARF_MEASURE route=alley pullups=%d z=%.2f ground=%d"),Chuck->GetPullUps()-PullUpsBase,At.Z,bGround ? 1 : 0);
            Check(Chuck->GetPullUps()>PullUpsBase && At.Z>255.f && bGround,TEXT("bouncing up the alley between the warehouse and the sail loft reaches the roofs"));
            // Next: Chandlers' Row. Run across the first roof and leap the gap to the higher one.
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-370,-1100,214),false,nullptr,ETeleportType::TeleportPhysics);
            Chuck->SetActorRotation(FRotator(0,-90,0)); Chuck->Recenter(); Chuck->SetTestStick(FVector2D(0,1)); Chuck->SetRunHeld(true);
            PullUpsBase=Chuck->GetPullUps(); bKeyMeasured=false;
            TestStage=85; StageTime=0;
        }
    }
    else if(TestStage==85)
    {
        const FVector At=Chuck->GetActorLocation();
        const bool bGround=Chuck->GetCharacterMovement()->IsMovingOnGround();
        if(bGround && !bKeyMeasured) Chuck->AddMovementInput(FVector(0,-1,0),1);
        if(bGround && !bKeyMeasured && At.Y<-1212.f) { Chuck->JumpPressed(); bKeyMeasured=true; }  // leap from the roof edge
        if(FCString::Strcmp(Chuck->GetGaitName(),TEXT("Climb"))==0) Chuck->SetTestStick(FVector2D::ZeroVector);
        if(StageTime>5.f)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_DISTRICT_MEASURE route=roof_leap leaped=%d pullups=%d z=%.2f y=%.2f ground=%d"),bKeyMeasured ? 1 : 0,Chuck->GetPullUps()-PullUpsBase,At.Z,At.Y,bGround ? 1 : 0);
            Check(bKeyMeasured && Chuck->GetPullUps()>PullUpsBase && FMath::Abs(At.Z-(230.f+34.65f))<3.f && bGround,TEXT("a leap across the roof gap catches the higher roof and climbs up"));
            // Next: walk up the ramp onto the customs terrace.
            Chuck->SetRunHeld(false); Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock();
            Chuck->SetActorLocation(FVector(-25,-1380,36),false,nullptr,ETeleportType::TeleportPhysics); Chuck->SetActorRotation(FRotator(0,-90,0)); Chuck->Recenter();
            TestStage=86; StageTime=0;
        }
    }
    else if(TestStage==86)
    {
        if(StageTime<4.5f) Chuck->AddMovementInput(FVector(0,-1,0),1);
        if(StageTime>5.2f)
        {
            const FVector At=Chuck->GetActorLocation();
            const bool bGround=Chuck->GetCharacterMovement()->IsMovingOnGround();
            UE_LOG(LogTemp,Display,TEXT("CHUCK_DISTRICT_MEASURE route=ramp z=%.2f y=%.2f ground=%d"),At.Z,At.Y,bGround ? 1 : 0);
            Check(FMath::Abs(At.Z-(90.f+34.65f))<3.f && bGround,TEXT("the ramp walks him up onto the customs terrace"));
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); StageTime=0;
            // Screenshots stall frames, so they come from an unmeasured replay:
            // the roll seen from the side, then the side jump from behind.
            if(FParse::Param(FCommandLine::Get(),TEXT("ChuckCapture")))
            {
                Chuck->SetActorRotation(FRotator(0,90,0)); Chuck->Recenter(); Chuck->SetActorRotation(FRotator::ZeroRotator);
                Chuck->DodgeToward(FVector2D::ZeroVector); TestStage=56;
            }
            else TestStage=52;
        }
    }
    else if(TestStage==56 || TestStage==57)
    {
        const bool bRoll=TestStage==56;
        for(const float Shot : {.1f,.18f,.3f,.5f})
            if(StageTime>=Shot && StageTime-DeltaSeconds<Shot)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/%s_%03d.png"),bRoll ? TEXT("Roll") : TEXT("SideJump"),FMath::RoundToInt(Shot*100)),true,false);
        if(StageTime>1.5f)
        {
            Chuck->ResetToDock(); StageTime=0;
            if(bRoll) { Chuck->DodgeToward(FVector2D(1,0)); TestStage=57; }
            else
            {
                // Then the run from the side.
                Chuck->SetActorLocation(FVector(-240,0,36)); Chuck->SetActorRotation(FRotator(0,90,0)); Chuck->Recenter();
                Chuck->SetActorRotation(FRotator::ZeroRotator); Chuck->SetRunHeld(true); TestStage=60;
            }
        }
    }
    else if(TestStage==60)
    {
        Chuck->AddMovementInput(FVector(1,0,0),1);
        // The run, then a running jump (the stick held so it lands into the stride).
        if(StageTime>=1.4f && StageTime-DeltaSeconds<1.4f) { Chuck->SetTestStick(FVector2D(0,1)); Chuck->Jump(); }
        for(const float Shot : {1.2f,1.27f,1.34f,1.5f,1.6f,1.7f,1.85f})
            if(StageTime>=Shot && StageTime-DeltaSeconds<Shot)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/%s_%03d.png"),Shot<1.4f ? TEXT("Run") : TEXT("RunJump"),FMath::RoundToInt(Shot*100)),true,false);
        if(StageTime>2.2f)
        {
            // Then a slash pair, seen from the front three-quarter.
            Chuck->SetRunHeld(false); Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock();
            Chuck->SetActorRotation(FRotator(0,150,0)); Chuck->Recenter(); Chuck->SetActorRotation(FRotator::ZeroRotator);
            Chuck->Slash(); Chuck->SlashReleased(); TestStage=69; StageTime=0;
        }
    }
    else if(TestStage==69)
    {
        if(StageTime>=.1f && StageTime-DeltaSeconds<.1f) { Chuck->Slash(); Chuck->SlashReleased(); }
        for(const float Shot : {.1f,.16f,.22f,.3f,.38f,.44f,.52f})
            if(StageTime>=Shot && StageTime-DeltaSeconds<Shot)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/Slash_%03d.png"),FMath::RoundToInt(Shot*100)),true,false);
        if(StageTime>1.5f)
        {
            // Then a grass shred among a few tufts, from the front three-quarter.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,-20,36));
            Chuck->SetActorRotation(FRotator(0,150,0)); Chuck->Recenter(); Chuck->SetActorRotation(FRotator::ZeroRotator);
            TestTufts.Reset();
            for(const FVector& Spot : {FVector(30,0,0),FVector(20,45,1),FVector(55,-40,2),FVector(-15,-50,1),FVector(80,25,0)})
                TestTufts.Add(AGrassTuft::Plant(GetWorld(),FVector2D(-240+Spot.X,-20+Spot.Y),static_cast<int32>(Spot.Z),Spot.X*7.f,1.f));
            TestJar=AClayJar::Place(GetWorld(),FVector2D(-240+34,-20-26),30.f,2);
            TestStage=96; StageTime=0;
        }
    }
    else if(TestStage==96)
    {
        if(StageTime>=.3f && StageTime-DeltaSeconds<.3f) { Chuck->Slash(); Chuck->SlashReleased(); }
        for(const float Shot : {.25f,.46f,.52f,.6f,.72f,.9f,1.3f})
            if(StageTime>=Shot && StageTime-DeltaSeconds<Shot)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/Grass_%03d.png"),FMath::RoundToInt(Shot*100)),true,false);
        if(StageTime>2.f)
        {
            for(auto& Tuft : TestTufts) if(Tuft.IsValid()) Tuft->Destroy();
            TestTufts.Reset();
            if(TestJar.IsValid()) TestJar->Destroy();
            for(TActorIterator<ACigarettePickup> It(GetWorld()); It; ++It) It->Destroy();
            // Then a rat encounter from the side at rat height, on open quay:
            // it comes in, gives its tell, lunges; Chuck rakes it twice.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-150,-20,36));
            Chuck->SetActorRotation(FRotator(0,90,0)); Chuck->Recenter(); Chuck->SetActorRotation(FRotator::ZeroRotator);
            TestRat=AEnemyRat::Place(GetWorld(),FVector2D(-30,-20),180.f);
            LocoValue=-1;
            TestStage=103; StageTime=0;
        }
    }
    else if(TestStage==103)
    {
        // Chuck holds still until the bite lands, then rakes whenever it's in reach.
        if(TestRat.IsValid() && !TestRat->IsDead() && StageTime>1.4f)
        {
            const FVector To=TestRat->GetActorLocation()-Chuck->GetActorLocation();
            Chuck->SetActorRotation(FRotator(0,To.Rotation().Yaw,0));
            if(To.Size2D()<AChuckCharacter::SlashReach+AEnemyRat::HitRadius+4.f && StageTime-LocoValue>.5f) { Chuck->Slash(); Chuck->SlashReleased(); LocoValue=StageTime; }
        }
        for(const float Shot : {.3f,.8f,1.05f,1.2f,1.35f,1.6f,2.f,2.4f,2.8f,3.4f})
            if(StageTime>=Shot && StageTime-DeltaSeconds<Shot)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/Rat_%03d.png"),FMath::RoundToInt(Shot*100)),true,false);
        if(StageTime>4.f)
        {
            if(TestRat.IsValid()) TestRat->Destroy();
            for(TActorIterator<ACigarettePickup> It(GetWorld()); It; ++It) It->Destroy();
            // Then the last point of Sanity: he vanishes, and is summoned back
            // at the start, seen from the front three-quarter.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-150,-20,36)); Chuck->SetSanity(1);
            Chuck->TakeBite(Chuck->GetActorLocation()+FVector(0,30,0));
            LocoValue=-1;
            TestStage=106; StageTime=0;
        }
    }
    else if(TestStage==106)
    {
        const FString Phase=Chuck->GetAstralName();
        if(Phase==TEXT("Vanishing") && StageTime<3.f)
            for(const float Shot : {.75f,1.05f,1.3f})
                if(StageTime>=Shot && StageTime-DeltaSeconds<Shot)
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/Vanish_%03d.png"),FMath::RoundToInt(Shot*100)),true,false);
        if(Phase==TEXT("Summoning") && LocoValue<0)
        {
            // Turn the view to his front for the capture (play keeps it behind him).
            LocoValue=StageTime;
            Chuck->SetActorRotation(FRotator(0,150,0)); Chuck->Recenter(); Chuck->SetActorRotation(FRotator::ZeroRotator);
        }
        if(LocoValue>=0)
            for(const float Shot : {.35f,.7f,.95f,1.15f,1.45f,1.9f,2.4f,2.9f})
                if(StageTime-LocoValue>=Shot && StageTime-LocoValue-DeltaSeconds<Shot)
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/Summon_%03d.png"),FMath::RoundToInt(Shot*100)),true,false);
        if(StageTime>12.f || (LocoValue>=0 && StageTime-LocoValue>3.2f))
        {
            // Then a breath of smoke, close in from the front three-quarter.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-150,-20,36));
            Chuck->SetActorRotation(FRotator(0,150,0)); Chuck->Recenter(); Chuck->SetActorRotation(FRotator::ZeroRotator);
            Chuck->SetOrbitPitch(-5.f);
            TestStage=107; StageTime=0;
        }
    }
    else if(TestStage==107)
    {
        if(StageTime>=.4f && StageTime-DeltaSeconds<.4f) Chuck->Exhale();
        for(const float Shot : {.7f,1.f,1.4f,1.9f,2.6f})
            if(StageTime>=Shot && StageTime-DeltaSeconds<Shot)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/Exhale_%03d.png"),FMath::RoundToInt(Shot*100)),true,false);
        if(StageTime>3.f)
        {
            // Then the dock worker watching the rat: Chuck walks up to him
            // (camera behind Chuck at rat height), stops, looks up.
            Worker=nullptr;
            for(const TWeakObjectPtr<ADockNPC>& Entry : ADockNPC::All()) if(Entry.IsValid() && Entry->ActorHasTag(TEXT("DockWorkerArt"))) Worker=Entry;
            Chuck->ResetToDock();
            if(Worker.IsValid())
            {
                const FVector Front=Worker->GetActorForwardVector();
                Chuck->SetActorLocation(Worker->GetActorLocation()+Front*320.f+FVector(-60.f,0,0)-FVector(0,0,Worker->GetActorLocation().Z-36.f));
                Chuck->SetActorRotation((Worker->GetActorLocation()-Chuck->GetActorLocation()).GetSafeNormal2D().Rotation());
                Chuck->Recenter(); Chuck->SetOrbitPitch(-5.f);
            }
            TestStage=110; StageTime=0;
        }
    }
    else if(TestStage==110)
    {
        if(Worker.IsValid() && StageTime<1.6f) Chuck->AddMovementInput((Worker->GetActorLocation()-Chuck->GetActorLocation()).GetSafeNormal2D(),1);
        for(const float Shot : {.3f,1.8f,2.6f})
            if(StageTime>=Shot && StageTime-DeltaSeconds<Shot)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/Worker_%03d.png"),FMath::RoundToInt(Shot*100)),true,false);
        if(StageTime>3.f)
        {
            // Then the cargo chimney, seen from the quay (north).
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-335,-337,36));
            Chuck->SetActorRotation(FRotator(0,-90,0)); Chuck->Recenter(); Chuck->SetActorRotation(FRotator(0,180,0));
            Chuck->SetTestStick(FVector2D(-1,0)); WallEnterAt=-1; WallSides.Reset();
            TestStage=74; StageTime=0;
        }
    }
    else if(TestStage==74)
    {
        if(StageTime>=.1f && StageTime-DeltaSeconds<.1f) Chuck->JumpPressed();
        const bool bRunning=Chuck->IsWallRunning();
        if(bRunning && WallEnterAt<0) { WallEnterAt=StageTime; WallSides.Add(1); Chuck->SetTestStick(FVector2D::ZeroVector); }
        if(!bRunning) WallEnterAt=-1;
        if(bRunning && WallEnterAt>=0 && StageTime>=WallEnterAt+.25f && StageTime-DeltaSeconds<WallEnterAt+.25f && WallSides.Num()<=4) Chuck->JumpPressed();
        for(const float Shot : {.25f,.4f,.55f,.7f,.85f,1.f,1.2f,1.4f})
            if(StageTime>=Shot && StageTime-DeltaSeconds<Shot)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/Chimney_%03d.png"),FMath::RoundToInt(Shot*100)),true,false);
        if(StageTime>3.f)
        {
            // Then the harbour wall climb, from the side: run up, hang, pull up.
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-40,-305,36));
            Chuck->SetActorRotation(FRotator(0,0,0)); Chuck->Recenter(); Chuck->SetActorRotation(FRotator(0,-90,0));
            Chuck->SetTestStick(FVector2D(-1,0)); HangAt=-1;
            TestStage=79; StageTime=0;
        }
    }
    else if(TestStage==79)
    {
        if(StageTime<.3f) Chuck->AddMovementInput(FVector(0,-1,0),1);
        if(StageTime>=.3f && StageTime-DeltaSeconds<.3f) Chuck->JumpPressed();
        // Hang a moment, shimmy right along the edge, then pull up.
        if(Chuck->IsHanging() && HangAt<0) { HangAt=StageTime; Chuck->SetTestStick(FVector2D::ZeroVector); }
        if(HangAt>=0 && StageTime>=HangAt+.5f && StageTime-DeltaSeconds<HangAt+.5f) Chuck->SetTestStick(FVector2D(1,0));
        if(HangAt>=0 && StageTime>=HangAt+1.5f && StageTime-DeltaSeconds<HangAt+1.5f) { Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->JumpPressed(); }
        for(const float Shot : {.35f,.45f,.55f,.8f,1.2f,1.4f,1.6f,2.1f,2.4f})
            if(StageTime>=Shot && StageTime-DeltaSeconds<Shot)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/Ledge_%03d.png"),FMath::RoundToInt(Shot*100)),true,false);
        if(StageTime>3.f)
        {
            // Then the cargo wharf: from its south edge looking north over the
            // whole course (only water behind the camera), then from the warehouse roof.
            Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-100,-975,36));
            Chuck->SetActorRotation(FRotator(0,90,0)); Chuck->Recenter(); Chuck->SetOrbitPitch(-38.f);
            TestStage=84; StageTime=0;
        }
    }
    else if(TestStage==84)
    {
        if(StageTime>=1.f && StageTime-DeltaSeconds<1.f)
        {
            Chuck->SetActorLocation(FVector(-120,-780,230+35),false,nullptr,ETeleportType::TeleportPhysics);
            Chuck->SetActorRotation(FRotator(0,150,0)); Chuck->Recenter(); Chuck->SetOrbitPitch(-22.f);
        }
        if(StageTime>=2.2f && StageTime-DeltaSeconds<2.2f)
        {
            // Chandlers' Row from the tallest roof, looking north over the row and market.
            Chuck->SetActorLocation(FVector(-370,-1650,280+35),false,nullptr,ETeleportType::TeleportPhysics);
            Chuck->SetActorRotation(FRotator(0,60,0)); Chuck->Recenter(); Chuck->SetOrbitPitch(-26.f);
        }
        if(StageTime>=3.4f && StageTime-DeltaSeconds<3.4f)
        {
            // The Timber Yard from the crane tower, looking west over the lumber stacks.
            Chuck->SetActorLocation(FVector(800,-620,250+35),false,nullptr,ETeleportType::TeleportPhysics);
            Chuck->SetActorRotation(FRotator(0,160,0)); Chuck->Recenter(); Chuck->SetOrbitPitch(-24.f);
        }
        for(const float Shot : {1.9f,3.1f,4.3f})  // (the south-edge view is now blocked by the market canopies)
            if(StageTime>=Shot && StageTime-DeltaSeconds<Shot)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/Wharf_%03d.png"),FMath::RoundToInt(Shot*100)),true,false);
        if(StageTime>4.7f) { Chuck->ResetToDock(); TestStage=52; StageTime=0; }
    }
    else if(TestStage==52)
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("ChuckCapture")))
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
    else if(TestStage==23 && StageTime>1)
    {
        Chuck->SetActorRotation(FRotator(0,240,0));
        TestStage=24; StageTime=0;
    }
    else if(TestStage==24 && StageTime>1)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Windows/Chuck_Front.png"),true,false);
        TestStage=25; StageTime=0;
    }
    else if(TestStage==25 && StageTime>1)
    {
        Chuck->ResetToDock();
        Chuck->SetActorRotation(FRotator(0,-55,0)); Chuck->Recenter();
        Chuck->SetActorRotation(FRotator::ZeroRotator);
        TestStage=26; StageTime=0;
    }
    else if(TestStage==26 || TestStage==28)
    {
        Chuck->AddMovementInput(FVector(1,0,0),1);
        if(StageTime>.65f)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/(TestStage==26 ? TEXT("Screenshots/Windows/Rig_Walk_RatHeight.png") : TEXT("Screenshots/Windows/Rig_Walk_Elevated.png")),true,false);
            ++TestStage; StageTime=0;
        }
    }
    else if(TestStage==27 && StageTime>1)
    {
        Chuck->ResetToDock(); Chuck->ToggleCamera();
        Chuck->SetActorRotation(FRotator(0,-55,0)); Chuck->Recenter();
        Chuck->SetActorRotation(FRotator::ZeroRotator);
        TestStage=28; StageTime=0;
    }
    else if(TestStage==29 && StageTime>1)
    {
        Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-220,-90,36));
        Chuck->SetActorRotation(FRotator(0,180,0)); Chuck->Recenter();
        if(Chuck->IsElevated()) Chuck->ToggleCamera();
        TestStage=30; StageTime=0;
    }
    else if(TestStage==30 && StageTime>2)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Windows/Props_Barrel.png"),true,false);
        TestStage=31; StageTime=0;
    }
    else if(TestStage==31 && StageTime>1)
    {
        Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(690,0,36));
        Chuck->SetActorRotation(FRotator(0,64,0)); Chuck->Recenter();
        if(Chuck->IsElevated()) Chuck->ToggleCamera();
        TestStage=32; StageTime=0;
    }
    else if(TestStage==32 && StageTime>2)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Windows/Harbor_RatHeight.png"),true,false);
        TestStage=33; StageTime=0;
    }
    else if(TestStage==33 && StageTime>1)
    { Chuck->ToggleCamera(); TestStage=34; StageTime=0; }
    else if(TestStage==34 && StageTime>2)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Windows/Harbor_Elevated.png"),true,false);
        TestStage=35; StageTime=0;
    }
    else if(TestStage==35 && StageTime>1)
    {
        Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-230,30,36));
        Chuck->SetActorRotation(FRotator(0,90,0)); Chuck->Recenter();
        if(Chuck->IsElevated()) Chuck->ToggleCamera();
        TestStage=36; StageTime=0;
    }
    else if(TestStage==36 && StageTime>2)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Windows/Tavern_RatHeight.png"),true,false);
        TestStage=37; StageTime=0;
    }
    else if(TestStage==37 && StageTime>1)
    {
        TestStage=FParse::Param(FCommandLine::Get(),TEXT("ChuckMotionCapture")) ? 40:99;
        StageTime=0;
    }
    else if(TestStage>=40 && TestStage<=47)
    {
        const int32 View=(TestStage-40)/2;
        if(TestStage%2==0)
        {
            Chuck->ResetToDock(); Chuck->SetActorLocation(FVector(-240,-180,36));
            const float Yaws[]={0.f,180.f,90.f,0.f};
            Chuck->SetActorRotation(FRotator(0,Yaws[View],0)); Chuck->Recenter();
            Chuck->SetActorRotation(FRotator::ZeroRotator);
            if(Chuck->IsElevated()!=(View==3)) Chuck->ToggleCamera();
            ++TestStage; StageTime=0; MotionFrame=0; bMotionJump=false;
        }
        else
        {
            if(StageTime>2 && StageTime<3.1f) Chuck->AddMovementInput(FVector::ForwardVector,1);
            if(StageTime>=3.1f && StageTime<3.5f) Chuck->AddMovementInput(FVector::RightVector,1);
            if(StageTime>3.9f && !bMotionJump) { Chuck->Jump(); bMotionJump=true; }
            if(StageTime>4.05f) Chuck->StopJumping();
            if(StageTime>=1.5f+MotionFrame*.1f && StageTime<5.3f)
            {
                const FString Directory=FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/Windows/Motion/View%d"),View);
                IFileManager::Get().MakeDirectory(*Directory,true);
                FScreenshotRequest::RequestScreenshot(Directory/FString::Printf(TEXT("frame%03d.png"),MotionFrame++),false,false);
            }
            if(StageTime>5.4f) { ++TestStage; StageTime=0; if(TestStage==48) TestStage=99; }
        }
    }
    else if(TestStage==99)
    {
        {
            using ESfx=AChuckCharacter::ESfx;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_SFX_MEASURE loaded=%d steps=%d jumps=%d lands=%d slashes=%d rolls=%d"),Chuck->GetSfxLoaded(),
                Chuck->GetSfxCount(ESfx::Step),Chuck->GetSfxCount(ESfx::Jump),Chuck->GetSfxCount(ESfx::Land),Chuck->GetSfxCount(ESfx::Slash),Chuck->GetSfxCount(ESfx::Roll));
            UE_LOG(LogTemp,Display,TEXT("CHUCK_EXHALE_MEASURE exhales=%d puffs=%d"),Chuck->GetExhales(),Chuck->GetSmokePuffsSpawned());
            Check(Chuck->GetExhales()>=5 && Chuck->GetSmokePuffsSpawned()>=Chuck->GetExhales()*8,TEXT("Chuck exhales smoke every so often on his own"));
            Check(Chuck->GetSfxLoaded()==36 && Chuck->GetSfxCount(ESfx::Step)>20 && Chuck->GetSfxCount(ESfx::Jump)>0 && Chuck->GetSfxCount(ESfx::Land)>0
                && Chuck->GetSfxCount(ESfx::Slash)>0 && Chuck->GetSfxCount(ESfx::Roll)>0,TEXT("movement sound effects load and play (steps, jump, land, slash, roll)"));
        }
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
    DrawText(Chuck->IsElevated() ? TEXT("ORBIT CAMERA: HIGH") : TEXT("ORBIT CAMERA: RAT HEIGHT"),FLinearColor(.77f,.67f,.94f),30,54,GEngine->GetSmallFont(),1.1f);
    DrawText(TEXT("65 cm rat  /  180 cm dock worker"),FLinearColor(.7f,.73f,.76f),30,76,GEngine->GetSmallFont());
    // Sanity as a row of cigarettes (the 2D game's bar): burnt-out slots for
    // what's been lost. Below it the cigarettes collected beyond a full bar.
    const float PanelX=Canvas->SizeX-290;
    DrawRect(FLinearColor(0.035f,0.04f,0.045f,0.85f),PanelX,18,272,86);
    DrawText(TEXT("SANITY"),FLinearColor(.77f,.67f,.94f),PanelX+12,24,GEngine->GetSmallFont(),.95f);
    for(int32 I=0;I<AChuckCharacter::MaxSanity;++I)
    {
        const float X=PanelX+12+I*51, Y=48;
        const bool bFull=I<Chuck->GetSanity();
        const FLinearColor Spent(.2f,.2f,.22f,.9f);
        DrawRect(bFull ? FLinearColor(.72f,.47f,.22f) : Spent,X,Y,12,10);          // filter
        DrawRect(bFull ? FLinearColor(.93f,.91f,.86f) : Spent,X+12,Y,31,10);       // paper
        DrawRect(bFull ? FLinearColor(.42f,.4f,.38f) : Spent,X+43,Y,3,10);         // ash at the tip
    }
    DrawText(FString::Printf(TEXT("CIGARETTES   %d"),Chuck->GetCigarettes()),FLinearColor(.94f,.88f,.75f),PanelX+12,72,GEngine->GetSmallFont(),1.1f);
    // Talk: a quiet prompt near the top in reach (as in the 2D game), and a
    // plain dialogue box above the controls while someone's speaking.
    FString Speaker, Line;
    if(Chuck->GetDialogue(Speaker,Line))
    {
        const float BoxW=FMath::Min(900.f,Canvas->SizeX-80.f), BoxX=(Canvas->SizeX-BoxW)*.5f, BoxY=Canvas->SizeY-190;
        DrawRect(FLinearColor(0.035f,0.04f,0.045f,0.92f),BoxX,BoxY,BoxW,108);
        DrawText(Speaker.ToUpper(),FLinearColor(.77f,.67f,.94f),BoxX+20,BoxY+12,GEngine->GetSmallFont(),1.05f);
        DrawText(Line,FLinearColor(.95f,.93f,.9f),BoxX+20,BoxY+40,GEngine->GetSmallFont(),1.3f);
        DrawText(TEXT("F / Y"),FLinearColor(.6f,.62f,.66f),BoxX+BoxW-70,BoxY+84,GEngine->GetSmallFont(),.9f);
    }
    else if(Chuck->GetTalkPrompt())
    {
        const FString Prompt=TEXT("F / Y   Talk");
        DrawText(Prompt,FLinearColor(.95f,.95f,.95f),Canvas->SizeX*.5f-60,120,GEngine->GetSmallFont(),1.2f);
    }
    DrawRect(FLinearColor(0.035f,0.04f,0.045f,0.85f),18,Canvas->SizeY-65,Canvas->SizeX-36,47);
    DrawText(TEXT("WASD / Left stick: walk    Shift / LB: run (tap)    Q/E or hold LT: strafe (jump: side jump)    Space / A: jump    LMB / X: slash    C / B: roll    Mouse / Right stick: orbit"),FLinearColor(.91f,.9f,.85f),30,Canvas->SizeY-58,GEngine->GetSmallFont());
    DrawText(TEXT("Jump or side jump into a wall: run up it; jump again: kick off    Middle mouse / R-stick click: center    F / Y: talk    R / View: reset    Esc / Menu: exit    The camera drifts behind Chuck as he walks."),FLinearColor(.75f,.77f,.8f),30,Canvas->SizeY-37,GEngine->GetSmallFont());
}
