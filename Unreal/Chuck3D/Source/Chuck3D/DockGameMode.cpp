#include "DockGameMode.h"
#include "ChuckCharacter.h"
#include "Camera/CameraComponent.h"
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
    Shape(TEXT("BenchTop"),FVector(-210,225,45),FVector(160,42,8),TEXT("WoodLight"));
    for(float X : {-275.f,-145.f}) Shape(TEXT("BenchLeg"),FVector(X,225,21),FVector(12,32,42),TEXT("Wood"));
    if(BenchMesh) Prop(TEXT("TavernBenchArt"),FVector(-210,225,0),BenchMesh);
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
    if(WorkerMesh) Prop(TEXT("DockWorkerArt"),Human,WorkerMesh)->SetActorRotation(FRotator(0,-90,0));
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
    // Animated opaque wave normals now replace the old geometric ripple strips.
    auto* HarborFog=World->SpawnActor<AExponentialHeightFog>();
    HarborFog->GetComponent()->SetFogDensity(.018f);
    HarborFog->GetComponent()->SetStartDistance(1000);
    HarborFog->GetComponent()->SetFogInscatteringColor(FLinearColor(.38f,.48f,.53f));
    auto* Sun = World->SpawnActor<ADirectionalLight>(FVector(0,0,500),FRotator(-38,-40,0));
    Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sun->GetLightComponent()->SetIntensity(4.0f);
    Sun->GetLightComponent()->SetLightColor(FLinearColor(1,.9f,.77f));
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
    Sky->GetLightComponent()->SetIntensity(0.55f);
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
    if(auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
    { Chuck->ResetToDock(); AddTickPrerequisiteActor(Chuck); }
    UE_LOG(LogTemp,Display,TEXT("CHUCK: docks ready; Chuck 65 cm, human 180 cm; two cameras available."));
    bSmokeTest = FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest"));
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
            const float Flight=2.f*ChuckClipData::SideVerticalSpeed/(980.f*Chuck->GetCharacterMovement()->GravityScale);
            const float Authored=ChuckClipData::SideLateralSpeed*Flight;
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
            Chuck->EnableInput(KeyPC);
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
            Check(bKeyMeasured && (bRight ? KeySide>55.f : KeySide<-55.f),bRight ? TEXT("keyboard D + C side-jumps right") : TEXT("keyboard A + C side-jumps left"));
            Chuck->ResetToDock(); StageTime=0; bKeyMeasured=false; bLocoFlag=false;
            if(bRight) { KeyPC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::A,IE_Pressed,1)); TestStage=63; }
            else
            {
                KeyPC->FlushPressedKeys(); Chuck->DisableInput(KeyPC);
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
        if(StageTime>2.2f) { Chuck->SetRunHeld(false); Chuck->SetTestStick(FVector2D::ZeroVector); Chuck->ResetToDock(); TestStage=52; StageTime=0; }
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
    DrawRect(FLinearColor(0.035f,0.04f,0.045f,0.85f),18,Canvas->SizeY-65,Canvas->SizeX-36,47);
    DrawText(TEXT("WASD / Left stick: walk    Shift / LB: run    Space / A: jump    C / B: roll (stick sideways: side jump)    Mouse / Right stick: orbit    Q/E: turn"),FLinearColor(.91f,.9f,.85f),30,Canvas->SizeY-58,GEngine->GetSmallFont());
    DrawText(TEXT("F / R-stick click: center    R / View: reset    Esc / Menu: exit    The camera drifts behind Chuck as he walks."),FLinearColor(.75f,.77f,.8f),30,Canvas->SizeY-37,GEngine->GetSmallFont());
}
