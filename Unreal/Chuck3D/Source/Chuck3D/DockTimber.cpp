#include "DockTimber.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "HighResScreenshot.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

void FinishDockTimber(UWorld* World)
{
    auto* Original=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_DockCrate.SM_DockCrate"));
    auto* Weathered=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Props/SM_WeatheredDockCrate.SM_WeatheredDockCrate"));
    int32 Crates=0;
    if(Original && Weathered) for(TActorIterator<AActor> It(World);It;++It)
    {
        TInlineComponentArray<UStaticMeshComponent*> Components(*It);
        for(auto* Component:Components) if(Component->GetStaticMesh()==Original)
        {
            Component->SetStaticMesh(Weathered);
            // Any explicit old slot overrides must follow the derived UV mesh.
            for(int32 Slot=0;Slot<Weathered->GetStaticMaterials().Num();++Slot)
                Component->SetMaterial(Slot,Weathered->GetMaterial(Slot));
            ++Crates;
        }
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_WEATHERED_TIMBER crate_art=%d derived_uv=%d"),Crates,Weathered!=nullptr);
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckCrateMotionCapture")))
    {
        auto* Camera=World->SpawnActor<ACameraActor>();
        Camera->GetCameraComponent()->SetFieldOfView(50);
        const FVector Target(-80,60,30);
        for(int32 I=0;I<80;++I)
        {
            const float Angle=FMath::DegreesToRadians(-70.f+((I-20)%30)*2.f);
            const float Radius=I<50?220.f:420.f,Height=I<50?75.f:160.f;
            const FVector P=I<20?Target+FVector(150,-160,75):Target+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,Height);
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,P,Target]()
            {Camera->SetActorLocationAndRotation(P,(Target-P).Rotation());if(auto* PC=World->GetFirstPlayerController()) PC->SetViewTarget(Camera);},5.f+I*.12f,false);
            World->GetTimerManager().SetTimer(Shot,[I]()
            {const FString Folder=FPaths::ScreenShotDir()/TEXT("CrateMotion");IFileManager::Get().MakeDirectory(*Folder,true);FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("Frame%03d.png"),I),false,false);},5.06f+I*.12f,false);
        }
        FTimerHandle Exit;
        World->GetTimerManager().SetTimer(Exit,[](){FPlatformMisc::RequestExit(false);},16.f,false);
        return;
    }
    if(!FParse::Param(FCommandLine::Get(),TEXT("ChuckTimberCapture"))) return;
    auto* Camera=World->SpawnActor<ACameraActor>();
    Camera->GetCameraComponent()->SetFieldOfView(60);
    const FVector Positions[]={FVector(-450,-620,145),FVector(600,-1180,240),FVector(-1000,-1900,165),FVector(1610,3270,140)};
    const FVector Targets[]={FVector(-210,-680,75),FVector(-220,-1190,190),FVector(-1510,-1800,180),FVector(1370,3610,70)};
    for(int32 I=0;I<UE_ARRAY_COUNT(Positions);++I)
    {
        FTimerHandle View,Shot;
        World->GetTimerManager().SetTimer(View,[World,Camera,P=Positions[I],T=Targets[I]]()
        { Camera->SetActorLocationAndRotation(P,(T-P).Rotation()); if(auto* PC=World->GetFirstPlayerController()) PC->SetViewTarget(Camera); },4.f+I*4.f,false);
        World->GetTimerManager().SetTimer(Shot,[I]()
        { const FString Folder=FPaths::ScreenShotDir()/TEXT("Timber"); IFileManager::Get().MakeDirectory(*Folder,true); FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false); },6.f+I*4.f,false);
    }
    FTimerHandle Exit;
    World->GetTimerManager().SetTimer(Exit,[](){FPlatformMisc::RequestExit(false);},23.f,false);
}
