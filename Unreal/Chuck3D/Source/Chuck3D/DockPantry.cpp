#include "DockPantry.h"
#include "ChuckCharacter.h"
#include "SewerSlide.h"
#include "DockFire.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"
#include "UnrealClient.h"

bool IsWithinDockPantry(const FVector& P)
{
    // Bounded permission for this cellar/shaft, not a blanket below-world bypass.
    return (P.X>-278 && P.X<278 && P.Y>452 && P.Y<953 && P.Z>-340 && P.Z<-105)
        || (P.X>20 && P.X<130 && P.Y>875 && P.Y<955 && P.Z>=-105 && P.Z<70);
}

void BuildDockPantry(UWorld* World)
{
    auto* Owner=World->SpawnActor<AActor>();Owner->Tags.Add(TEXT("DockPantry"));
    auto* Root=NewObject<USceneComponent>(Owner);Owner->SetRootComponent(Root);Root->RegisterComponent();
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    TMap<FString,UInstancedStaticMeshComponent*> Batches;
    auto Shape=[&](FVector P,FVector Size,const TCHAR* Surface,bool Solid=false,int32 Form=0,FRotator Rotation=FRotator::ZeroRotator)
    {
        const FString Key=FString::Printf(TEXT("%s_%d_%d"),Surface,Solid,Form);
        auto*& Batch=Batches.FindOrAdd(Key);
        if(!Batch)
        {
            Batch=NewObject<UInstancedStaticMeshComponent>(Owner);Batch->SetupAttachment(Root);
            Batch->SetStaticMesh(Form==1?Cylinder:Form==2?Sphere:Cube);
            auto* Material=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),Surface,Surface));
            if(!Material) Material=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Prototype/Materials/M_%s.M_%s"),Surface,Surface));
            Batch->SetMaterial(0,Material);Batch->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));Batch->RegisterComponent();
        }
        Batch->AddInstance(FTransform(Rotation,P,Size/100.f));
    };
    Shape(FVector(0,702.5,-335),FVector(580,525,30),TEXT("Stone"),true);
    for(float X : {-290.f,290.f}) Shape(FVector(X,702.5,-205),FVector(24,525,230),TEXT("Stone"),true);
    for(float Y : {440.f,965.f}) Shape(FVector(0,Y,-205),FVector(580,24,230),TEXT("Stone"),true);
    // Ceiling/shaft lining carry the opening through all ground layers.
    Shape(FVector(-135,702.5,-100),FVector(310,525,20),TEXT("Stone"),true);
    Shape(FVector(210,702.5,-100),FVector(160,525,20),TEXT("Stone"),true);
    Shape(FVector(75,657.5,-100),FVector(110,435,20),TEXT("Stone"),true);
    Shape(FVector(75,960,-100),FVector(110,10,20),TEXT("Stone"),true);
    for(float X : {14.f,136.f}) Shape(FVector(X,915,-50),FVector(12,80,100),TEXT("Stone"),true);
    for(float Y : {869.f,961.f}) Shape(FVector(75,Y,-50),FVector(122,12,100),TEXT("Stone"),true);
    // Flush worn timber surround and upright lid; clear width stays 110 x 80cm.
    for(float X : {14.f,136.f}) Shape(FVector(X,915,1),FVector(12,104,2),TEXT("WoodLight"));
    for(float Y : {869.f,961.f}) Shape(FVector(75,Y,1),FVector(110,12,2),TEXT("WoodLight"));
    Shape(FVector(14,915,48),FVector(5,96,92),TEXT("Wood"));
    for(float Y : {884.f,946.f}) Shape(FVector(17,Y,48),FVector(2,6,80),TEXT("Dark"));
    // Visual ladder reserved for Claude: perpendicular to +X, no climb logic.
    for(float Y : {891.f,939.f}) Shape(FVector(124,Y,-150),FVector(6,6,330),TEXT("WoodLight"));
    for(float Z=-300;Z<=0;Z+=28) Shape(FVector(124,915,Z),FVector(7,54,5),TEXT("Wood"));
    for(float X : {-185.f,0.f,185.f}) Shape(FVector(X,700,-119),FVector(15,490,18),TEXT("Wood"),true);
    // Slightly varied masonry courses over the solid enclosing walls.
    for(int32 Row=0;Row<5;++Row) for(int32 I=0;I<9;++I)
    {
        const float Z=-297+Row*39.f,Y=472+I*53.f;
        for(float X : {-275.f,275.f}) Shape(FVector(X,Y+(Row%2)*8,Z),FVector(5,49,34+(I%3)),I%5?TEXT("Stone"):TEXT("Dark"));
    }
    for(int32 Row=0;Row<5;++Row) for(int32 I=0;I<10;++I)
        Shape(FVector(-252+I*55,455,-298+Row*39),FVector(51,5,35),TEXT("Stone"));
    // Flagstone floor joints, outside the shaft landing.
    for(int32 I=0;I<9;++I) Shape(FVector(-250+I*60,702,-319.8f),FVector(1,491,.2f),TEXT("Dark"));
    for(int32 I=0;I<8;++I) Shape(FVector(0,470+I*64,-319.7f),FVector(550,1,.2f),TEXT("Dark"));
    // Three stocked racks, tight to the walls, with open centre/landing aisles.
    for(const FVector P : {FVector(-235,590,-320),FVector(-235,795,-320),FVector(235,615,-320)})
    {
        for(float Y : {-72.f,72.f}) for(float X : {-22.f,22.f}) Shape(P+FVector(X,Y,87),FVector(7,7,174),TEXT("Wood"),true);
        for(float Z : {20.f,78.f,136.f})
        {
            Shape(P+FVector(0,0,Z),FVector(57,159,6),TEXT("WoodLight"),true);
            for(int32 I=0;I<4;++I)
            {
                const FVector Jar=P+FVector(0,-57+I*38,Z+17);
                if(I%2)
                {
                    Shape(Jar,FVector(27,29,25),TEXT("Plaster"),false,2);
                    Shape(Jar+FVector(0,0,14),FVector(12,12,6),TEXT("Wood"),false,1);
                }
                else
                {
                    Shape(Jar,FVector(21,21,27),TEXT("Amber"),false,1);
                    Shape(Jar+FVector(0,0,15),FVector(23,23,3),TEXT("Wood"),false,1);
                }
            }
        }
        Shape(P+FVector(27,0,98),FVector(3,150,7),TEXT("Dark"),false,0,FRotator(0,0,43));
    }
    auto* BarrelMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_DockBarrel.SM_DockBarrel"));
    for(const FVector P : {FVector(-193,912,-275),FVector(-105,917,-275),FVector(226,848,-275)})
    {
        Shape(P,FVector(62,62,90),TEXT("Wood"),true,1);
        if(BarrelMesh)
        {
            auto* Art=NewObject<UInstancedStaticMeshComponent>(Owner);Art->SetupAttachment(Root);Art->SetStaticMesh(BarrelMesh);
            Art->SetCollisionProfileName(TEXT("NoCollision"));Art->RegisterComponent();Art->AddInstance(FTransform(P));
        }
        for(float Z : {-30.f,30.f}) Shape(P+FVector(0,0,Z),FVector(65,65,6),TEXT("Dark"),false,1);
    }
    for(int32 I=0;I<4;++I)
    {
        const FVector P(-210+I*55,502,-299);
        Shape(P,FVector(47,50,42),TEXT("Plaster"),true,2);
        Shape(P+FVector(0,0,22),FVector(14,14,8),TEXT("Wood"),false,1);
    }
    // Small work bench/crate stack for provisions, no new items/interactions.
    Shape(FVector(214,746,-290),FVector(86,70,60),TEXT("Wood"),true);
    for(float Y : {716.f,776.f}) Shape(FVector(214,Y,-262),FVector(92,6,5),TEXT("WoodLight"));
    for(const FVector P : {FVector(-264,699,-162),FVector(264,900,-158)})
    {
        Shape(P+FVector(P.X<0?-8:8,0,0),FVector(3,24,35),TEXT("Dark"));
        for(float Y : {-10.f,10.f}) Shape(P+FVector(0,Y,0),FVector(3,3,32),TEXT("Dark"));
        AddDockFlame(Owner,P+FVector(0,0,-9),12,24);
        for(float Z : {-18.f,18.f}) Shape(P+FVector(0,0,Z),FVector(25,27,4),TEXT("Dark"));
        auto* Lamp=NewObject<UPointLightComponent>(Owner);Lamp->SetupAttachment(Root);Lamp->SetRelativeLocation(P+FVector(P.X<0?20:-20,0,0));
        Lamp->SetIntensity(650);Lamp->SetAttenuationRadius(430);Lamp->SetLightColor(FLinearColor(1,.69f,.4f));Lamp->SetSourceRadius(12);Lamp->SetCastShadows(false);Lamp->RegisterComponent();
    }
    const TArray<FVector> Walk={FVector(75,915,-320),FVector(75,795,-320),FVector(75,595,-320),FVector(-105,595,-320),FVector(-105,790,-320),FVector(75,795,-320)};
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest")) || FParse::Param(FCommandLine::Get(),TEXT("ChuckPantryTest")))
    {
        int32 Failures=0;FHitResult Hit;
        for(const FVector P : Walk) if(!World->LineTraceSingleByChannel(Hit,P+FVector(0,0,20),P-FVector(0,0,30),ECC_Visibility) || FMath::Abs(Hit.ImpactPoint.Z+320)>1) ++Failures;
        for(int32 I=1;I<Walk.Num();++I)
            if(World->SweepSingleByChannel(Hit,Walk[I-1]+FVector(0,0,35),Walk[I]+FVector(0,0,35),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f))) ++Failures;
        if(World->SweepSingleByChannel(Hit,FVector(75,915,35),FVector(75,915,-280),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f)))
        {++Failures;UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_SHAFT_BLOCK actor=%s component=%s point=%s"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString());}
        for(const FVector P : {FVector(-280,700,-230),FVector(280,700,-230),FVector(0,443,-230),FVector(0,962,-230)})
            if(!World->LineTraceSingleByChannel(Hit,FVector(0,700,-230),P,ECC_Visibility)) ++Failures;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_CHECK failures=%d floors=6 routes=5 shaft=1 walls=4 ladder_visual_only=1"),Failures);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckPantryCapture")))
    {
        auto* Camera=World->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(85);
        const FVector Views[]={FVector(205,940,115),FVector(75,915,-65),FVector(100,855,-240),FVector(-95,565,-215)};
        const FVector Targets[]={FVector(75,915,-15),FVector(75,840,-305),FVector(-100,610,-235),FVector(180,865,-235)};
        for(int32 I=0;I<4;++I)
        {
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,P=Views[I],T=Targets[I]](){Camera->SetActorLocationAndRotation(P,(T-P).Rotation());World->GetFirstPlayerController()->SetViewTarget(Camera);},4.f+I*4.f,false);
            World->GetTimerManager().SetTimer(Shot,[I](){const FString Folder=FPaths::ScreenShotDir()/TEXT("Pantry");IFileManager::Get().MakeDirectory(*Folder,true);FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);},6.f+I*4.f,false);
        }
        FTimerHandle Exit;World->GetTimerManager().SetTimer(Exit,[World](){World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));},21.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckPantryTest")))
    {
        struct FRun {int32 Target=1,Pass=0;float Time=0;bool Landed=false;FTimerHandle Timer;};auto Run=MakeShared<FRun>();
        FTimerHandle Start;World->GetTimerManager().SetTimer(Start,[World,Run,Walk](){
            MarkDockSewerExited();auto* Chuck=Cast<AChuckCharacter>(World->GetFirstPlayerController()->GetPawn());Chuck->ResetToDock();
            Chuck->SetActorLocation(FVector(75,915,40));Chuck->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            World->GetTimerManager().SetTimer(Run->Timer,[World,Run,Walk,Chuck](){
                Run->Time+=.02f;
                if(FMath::Fmod(Run->Time,5.f)<.02f) UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_POSITION %s grounded=%d"),*Chuck->GetActorLocation().ToString(),Chuck->GetCharacterMovement()->IsMovingOnGround());
                if(!Run->Landed && Chuck->GetCharacterMovement()->IsMovingOnGround() && Chuck->GetActorLocation().Z<-250)
                {Run->Landed=true;UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_LANDED z=%.2f"),Chuck->GetActorLocation().Z);}
                if(Run->Landed)
                {
                    const FVector Goal=Walk[Run->Target]+FVector(0,0,34.65f);
                    if(FVector::Dist2D(Chuck->GetActorLocation(),Goal)<18) ++Run->Target;
                    else Chuck->AddMovementInput((Goal-Chuck->GetActorLocation()).GetSafeNormal2D(),1);
                    if(Run->Target>=Walk.Num()){++Run->Pass;Run->Target=1;Chuck->ToggleCamera();}
                }
                if(Run->Pass>=2 || Run->Time>50)
                {
                    const bool Passed=Run->Pass>=2 && Chuck->GetAreaStartLocation().Z>-100 && IsWithinDockPantry(Chuck->GetActorLocation());
                    Chuck->ResetToDock();
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_PANTRY_TEST_COMPLETE failures=%d fell=%d circuits=%d surface_reset=%d elapsed=%.2f"),Passed?0:1,Run->Landed,Run->Pass,Chuck->GetActorLocation().Z>0,Run->Time);
                    World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
                }
            },.02f,true);
        },3.f,false);
    }
}
