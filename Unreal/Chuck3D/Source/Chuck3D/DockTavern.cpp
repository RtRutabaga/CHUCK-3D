#include "DockTavern.h"
#include "ChuckCharacter.h"
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
#include "SewerSlide.h"
#include "UnrealClient.h"

void BuildDockTavern(UWorld* World)
{
    auto* Owner=World->SpawnActor<AActor>();Owner->Tags.Add(TEXT("DockTavernInterior"));
    auto* Root=NewObject<USceneComponent>(Owner);Owner->SetRootComponent(Root);Root->RegisterComponent();
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    TMap<FString,UInstancedStaticMeshComponent*> Batches;
    auto Shape=[&](FVector P,FVector Size,const TCHAR* Surface,bool Solid=false,bool Round=false,FRotator Rotation=FRotator::ZeroRotator)
    {
        const FString Key=FString(Surface)+(Solid?TEXT("_solid"):TEXT("_detail"))+(Round?TEXT("_round"):TEXT("_box"));
        auto*& Batch=Batches.FindOrAdd(Key);
        if(!Batch)
        {
            Batch=NewObject<UInstancedStaticMeshComponent>(Owner);Batch->SetupAttachment(Root);
            Batch->SetStaticMesh(Round?Cylinder:Cube);
            auto* Material=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Materials/M_%s.M_%s"),Surface,Surface));
            if(!Material) Material=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Prototype/Materials/M_%s.M_%s"),Surface,Surface));
            Batch->SetMaterial(0,Material);Batch->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));Batch->RegisterComponent();
        }
        Batch->AddInstance(FTransform(Rotation,P,Size/100.f));
    };
    auto Beam=[&](FVector A,FVector B,float W)
    {
        const FVector D=B-A;Shape((A+B)*.5f,FVector(D.Size(),W,W),TEXT("Wood"),false,false,D.Rotation());
    };
    // Flush planks over the structural floor; no decorative step at the door.
    for(int32 Row=0;Row<30;++Row)
    {
        const float Y=405+Row*18.5f;
        for(int32 Piece=0;Piece<4;++Piece)
        {
            const float X=-237+Piece*151.f;
            Shape(FVector(X,Y,.4f),FVector(150,18, .8f),(Row+Piece)%7==0?TEXT("WoodLight"):TEXT("Wood"));
            for(float End : {-69.f,69.f}) Shape(FVector(X+End,Y,.9f),FVector(1.1f,1.1f,.5f),TEXT("Dark"),false,true);
        }
    }
    // Timber wainscot and posts against old plaster, with vaulted roof lining.
    for(float X : {-314.f,294.f})
    {
        for(int32 I=0;I<29;++I)
        {
            const float Y=413+I*19.f;
            if(X>0 && Y>790) continue; // masonry hearth replaces timber here
            Shape(FVector(X,Y,52),FVector(4,18,104),TEXT("Wood"));
        }
        for(float Z : {12.f,105.f}) Shape(FVector(X,X>0?590.f:680.f,Z),FVector(7,X>0?386.f:566.f,7),TEXT("WoodLight"));
        for(float Y : {430.f,665.f,940.f})
            if(!(X>0 && Y>900)) Shape(FVector(X,Y,155),FVector(12,14,310),TEXT("Wood"));
    }
    for(float X : {-265.f,-10.f,245.f})
    {
        Shape(FVector(X,665,297),FVector(14,612,18),TEXT("Wood"));
        Beam(FVector(X,365,310),FVector(X,665,451),10);
        Beam(FVector(X,665,451),FVector(X,965,310),10);
    }
    for(float Side : {-1.f,1.f}) Shape(FVector(-10,665+Side*165,378),FVector(632,364,3),TEXT("Wood"),false,false,FRotator(0,0,Side*25));
    Shape(FVector(-10,665,443),FVector(636,18,22),TEXT("Wood"));
    // Inner faces of the front windows are closed shutters, with warm spill.
    for(float X : {-230.f,190.f})
    {
        Shape(FVector(X,398,160),FVector(80,6,95),TEXT("Wood"));
        Shape(FVector(X,402,160),FVector(60,2,74),TEXT("Dark"));
        for(float Z : {125.f,195.f}) Shape(FVector(X,404,Z),FVector(74,3,6),TEXT("WoodLight"));
    }
    auto Table=[&](FVector P,FVector2D Size)
    {
        Shape(P+FVector(0,0,75),FVector(Size.X,Size.Y,8),TEXT("Wood"),true);
        for(float X : {-Size.X*.37f,Size.X*.37f}) for(float Y : {-Size.Y*.33f,Size.Y*.33f})
            Shape(P+FVector(X,Y,35.5f),FVector(9,9,71),TEXT("Wood"),true);
        Shape(P+FVector(0,0,27),FVector(Size.X*.8f,8,8),TEXT("Wood"),true);
        for(float Side : {-1.f,1.f})
        {
            const FVector B=P+FVector(0,Side*(Size.Y*.5f+27),0);
            Shape(B+FVector(0,0,44),FVector(Size.X,26,7),TEXT("Wood"),true);
            for(float X : {-Size.X*.35f,Size.X*.35f}) Shape(B+FVector(X,0,20),FVector(10,21,40),TEXT("Wood"),true);
        }
        for(int32 I=0;I<3;++I)
        {
            const FVector Cup=P+FVector(-Size.X*.22f+I*Size.X*.22f,(I%2?1.f:-1.f)*Size.Y*.18f,85);
            Shape(Cup,FVector(8,8,12),I==1?TEXT("Metal"):TEXT("WoodLight"),false,true);
            Shape(Cup+FVector(0,0,6.1f),FVector(6,6,.5f),TEXT("Dark"),false,true);
        }
    };
    Table(FVector(-215,535,0),FVector2D(130,85));
    Table(FVector(-215,705,0),FVector2D(130,85));
    Table(FVector(220,560,0),FVector2D(110,100));
    // Bar leaves an aisle on the east and working room behind its counter.
    Shape(FVector(-65,840,47),FVector(410,60,94),TEXT("Wood"),true);
    Shape(FVector(-65,840,103),FVector(432,76,10),TEXT("WoodLight"),true);
    for(float X : {-250.f,-65.f,120.f}) Shape(FVector(X,807,47),FVector(10,6,88),TEXT("Dark"));
    for(float X : {-170.f,-65.f})
    {
        Shape(FVector(X,755,50),FVector(38,38,7),TEXT("Wood"),true,true);
        for(float Side : {-1.f,1.f}) Shape(FVector(X+Side*12,755,23),FVector(7,22,46),TEXT("Wood"),true);
    }
    for(float Z : {145.f,215.f})
    {
        Shape(FVector(-70,956,Z),FVector(420,30,7),TEXT("Wood"));
        for(float X : {-235.f,-75.f,90.f}) Beam(FVector(X,969,Z-26),FVector(X,942,Z-5),6);
        for(int32 I=0;I<10;++I)
        {
            const FVector Bottle(-250+I*38.f,954,Z+13);
            Shape(Bottle,FVector(10,10,19),I%3?TEXT("WoodLight"):TEXT("Amber"),false,true);
            Shape(Bottle+FVector(0,0,12),FVector(5,5,8),TEXT("Dark"),false,true);
        }
    }
    auto* BarrelMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_DockBarrel.SM_DockBarrel"));
    for(float X : {-255.f,-150.f})
    {
        if(BarrelMesh)
        {
            auto* Collision=NewObject<UInstancedStaticMeshComponent>(Owner);Collision->SetupAttachment(Root);Collision->SetStaticMesh(Cylinder);
            Collision->SetCollisionProfileName(TEXT("BlockAll"));Collision->SetHiddenInGame(true);Collision->RegisterComponent();
            Collision->AddInstance(FTransform(FRotator::ZeroRotator,FVector(X,919,45),FVector(.62f,.62f,.9f)));
            auto* Barrel=NewObject<UInstancedStaticMeshComponent>(Owner);Barrel->SetupAttachment(Root);Barrel->SetStaticMesh(BarrelMesh);
            Barrel->SetCollisionProfileName(TEXT("NoCollision"));Barrel->RegisterComponent();Barrel->AddInstance(FTransform(FVector(X,919,45)));
        }
        else
        {
            Shape(FVector(X,919,45),FVector(62,62,90),TEXT("Wood"),true,true);
            for(float Z : {14.f,72.f}) Shape(FVector(X,919,Z),FVector(65,65,7),TEXT("Dark"),false,true);
        }
    }
    // Small stone hearth; logs and flame are dressing, masonry has collision.
    Shape(FVector(285,870,10),FVector(60,164,20),TEXT("Stone"),true);
    Shape(FVector(293,870,80),FVector(6,116,140),TEXT("Dark"));
    for(float Y : {800.f,940.f}) Shape(FVector(282,Y,75),FVector(44,24,150),TEXT("Stone"),true);
    Shape(FVector(282,870,158),FVector(44,164,28),TEXT("Stone"),true);
    Shape(FVector(291,870,235),FVector(28,145,130),TEXT("Stone"));
    // Continue the flue through the roof; the visible stack and cap are solid.
    Shape(FVector(291,870,423),FVector(60,90,246),TEXT("Stone"),true);
    Shape(FVector(291,870,550),FVector(72,104,12),TEXT("Stone"),true);
    Shape(FVector(291,870,556.5f),FVector(38,63,1),TEXT("Dark"));
    Shape(FVector(279,870,27),FVector(15,95,14),TEXT("Wood"),false,false,FRotator(0,12,0));
    auto* Fire=NewObject<UInstancedStaticMeshComponent>(Owner);Fire->SetupAttachment(Root);
    Fire->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone.Cone")));
    Fire->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_TorchFlame.M_TorchFlame")));
    Fire->SetCollisionProfileName(TEXT("NoCollision"));Fire->RegisterComponent();
    for(int32 I=0;I<5;++I)
    {
        const float H=17+(I%3)*5.f;
        Fire->AddInstance(FTransform(FRotator(0,0,(I%2?1.f:-1.f)*9),FVector(280,835+I*17,29+H*.5f),FVector(.12f,.12f,H/100.f)));
    }
    auto Light=[&](FVector P,float Intensity,float Radius,FLinearColor Color)
    {
        auto* Lamp=NewObject<UPointLightComponent>(Owner);Lamp->SetupAttachment(Root);Lamp->SetRelativeLocation(P);
        Lamp->SetIntensity(Intensity);Lamp->SetAttenuationRadius(Radius);Lamp->SetLightColor(Color);Lamp->SetSourceRadius(16);
        Lamp->SetCastShadows(false);Lamp->RegisterComponent();
    };
    for(const FVector P : {FVector(-295,450,225),FVector(-295,900,225),FVector(274,450,225)})
    {
        Shape(P,FVector(19,19,32),TEXT("Amber"));
        for(float Z : {-18.f,18.f}) Shape(P+FVector(0,0,Z),FVector(25,25,5),TEXT("Dark"));
        for(float X : {-10.f,10.f}) Shape(P+FVector(X,-10,0),FVector(3,3,35),TEXT("Dark"));
        Light(P+FVector(18,18,0),650,370,FLinearColor(1,.66f,.34f));
    }
    Beam(FVector(-10,665,442),FVector(-10,665,270),2);
    Shape(FVector(-10,665,249),FVector(25,25,34),TEXT("Amber"));
    for(float Z : {229.f,269.f}) Shape(FVector(-10,665,Z),FVector(31,31,5),TEXT("Dark"));
    Light(FVector(-10,665,245),800,420,FLinearColor(1,.69f,.39f));
    Light(FVector(262,870,65),1000,390,FLinearColor(1,.43f,.16f));

    const TArray<FVector> Walk={FVector(40,280,0),FVector(40,450,0),FVector(40,730,0),FVector(225,740,0),FVector(225,930,0)};
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest")))
    {
        int32 Failures=0;
        for(const FVector P : Walk)
        {
            FHitResult Hit;
            if(!World->LineTraceSingleByChannel(Hit,P+FVector(0,0,50),P-FVector(0,0,50),ECC_Visibility) || FMath::Abs(Hit.ImpactPoint.Z)>1) ++Failures;
        }
        for(int32 I=1;I<Walk.Num();++I)
        {
            FHitResult Hit;
            const bool Blocked=World->SweepSingleByChannel(Hit,Walk[I-1]+FVector(0,0,35),Walk[I]+FVector(0,0,35),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f));
            if(I==1 ? (!Blocked || !Hit.GetActor()->ActorHasTag(TEXT("Door"))) : Blocked)
            {++Failures;UE_LOG(LogTemp,Display,TEXT("CHUCK_TAVERN_BLOCK leg=%d point=%s"),I,*Hit.ImpactPoint.ToString());}
        }
        FHitResult TableHit,BarHit,RoofHit;
        if(!World->LineTraceSingleByChannel(TableHit,FVector(-215,535,120),FVector(-215,535,0),ECC_Visibility) || FMath::Abs(TableHit.ImpactPoint.Z-79)>1) ++Failures;
        if(!World->LineTraceSingleByChannel(BarHit,FVector(-65,840,160),FVector(-65,840,0),ECC_Visibility) || FMath::Abs(BarHit.ImpactPoint.Z-108)>1) ++Failures;
        if(!World->LineTraceSingleByChannel(RoofHit,FVector(291,870,620),FVector(291,870,500),ECC_Visibility) || FMath::Abs(RoofHit.ImpactPoint.Z-556)>1) ++Failures;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_TAVERN_CHECK failures=%d floors=5 routes=3 furniture=2 roof=1 doorway_initially_closed=1"),Failures);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckTavernInteriorCapture")))
    {
        auto* Camera=World->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(85);
        const FVector Views[]={FVector(40,435,85),FVector(210,745,90),FVector(-280,905,170),FVector(40,775,120)};
        const FVector Targets[]={FVector(-55,830,120),FVector(280,870,80),FVector(30,520,120),FVector(40,300,100)};
        for(int32 I=0;I<4;++I)
        {
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,P=Views[I],T=Targets[I]](){Camera->SetActorLocationAndRotation(P,(T-P).Rotation());World->GetFirstPlayerController()->SetViewTarget(Camera);},4.f+I*4.f,false);
            World->GetTimerManager().SetTimer(Shot,[I](){const FString Folder=FPaths::ScreenShotDir()/TEXT("TavernInterior");IFileManager::Get().MakeDirectory(*Folder,true);FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);},6.f+I*4.f,false);
        }
        FTimerHandle Exit;World->GetTimerManager().SetTimer(Exit,[World](){World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));},20.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckTavernInteriorTest")))
    {
        struct FRun {int32 Target=1,Pass=0;float Time=0,PauseUntil=-1,ShotAt=-1;FTimerHandle Timer;};auto Run=MakeShared<FRun>();
        FTimerHandle Start;
        World->GetTimerManager().SetTimer(Start,[World,Run,Walk](){
            MarkDockSewerExited(); // This circuit tests the accessible post-sewer interior.
            auto* Chuck=Cast<AChuckCharacter>(World->GetFirstPlayerController()->GetPawn());
            Chuck->ResetToDock();Chuck->SetActorLocation(Walk[0]+FVector(0,0,34.65f));Chuck->SetActorRotation(FRotator(0,90,0));Chuck->Recenter();
            Chuck->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Chuck->SetRunHeld(false);
            if(Chuck->IsElevated()) Chuck->ToggleCamera();
            World->GetTimerManager().SetTimer(Run->Timer,[World,Run,Walk,Chuck](){
                Run->Time+=.02f;
                if(Run->ShotAt>=0 && Run->Time>=Run->ShotAt)
                {
                    Run->ShotAt=-1;const FString Folder=FPaths::ScreenShotDir()/TEXT("TavernPlay");IFileManager::Get().MakeDirectory(*Folder,true);
                    FScreenshotRequest::RequestScreenshot(Folder/(Run->Pass==0?TEXT("Low.png"):TEXT("High.png")),false,false);
                }
                if(Run->Time<Run->PauseUntil) return;
                const int32 Index=Run->Target<Walk.Num()?Run->Target:2*Walk.Num()-2-Run->Target;
                const FVector Goal=Walk[Index]+FVector(0,0,34.65f);
                if(FVector::Dist2D(Chuck->GetActorLocation(),Goal)<18)
                {
                    if(Run->Target==2)
                    {
                        Chuck->GetCharacterMovement()->StopMovementImmediately();Run->ShotAt=Run->Time+1.f;Run->PauseUntil=Run->Time+1.4f;
                    }
                    ++Run->Target;
                }
                else Chuck->AddMovementInput((Goal-Chuck->GetActorLocation()).GetSafeNormal2D(),1);
                if(Run->Target>=2*Walk.Num()-1)
                {
                    ++Run->Pass;UE_LOG(LogTemp,Display,TEXT("CHUCK_TAVERN_WALK camera_pass=%d entrance_bar_hearth_exit=1 elapsed=%.2f"),Run->Pass,Run->Time);
                    Run->Target=1;Chuck->ToggleCamera();
                }
                if(Run->Pass>=2 || Run->Time>100)
                {
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_TAVERN_TEST_COMPLETE failures=%d circuits=%d elapsed=%.2f"),Run->Pass>=2?0:1,Run->Pass,Run->Time);
                    World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
                }
            },.02f,true);
        },3.f,false);
    }
}
