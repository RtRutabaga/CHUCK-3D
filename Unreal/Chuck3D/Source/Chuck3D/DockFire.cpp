#include "DockFire.h"
#include "ChuckFlames.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "SewerSlide.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

void AddDockFlame(AActor* Owner,FVector Base,float Width,float Height)
{
    // Since 2026-10-27 every fire is drawn by AChuckFlames (flipbook flame sheets, sparks and smoke; ChuckFlames.h),
    // in place of three crossed cards of M_TorchFlame.
    if(Owner) if(AChuckFlames* Flames=AChuckFlames::Get(Owner->GetWorld())) Flames->Add(Owner,Base,Width,Height);
}

void FinishDockFire(UWorld* World)
{
    struct FLight {TWeakObjectPtr<UPointLightComponent> Component;float Intensity,Phase;};TArray<FLight> Lights;
    int32 Flames=0,Failures=0,Legacy=0;
    if(AChuckFlames* Fire=AChuckFlames::Get(World)) {Flames=Fire->GetFlameCount();Failures+=Fire->GetMissingMaterials();}
    else ++Failures;
    for(TActorIterator<AActor> It(World);It;++It)
    {
        TArray<UInstancedStaticMeshComponent*> Batches;It->GetComponents(Batches);
        for(auto* Batch : Batches) if(Batch->GetMaterial(0) && Batch->GetMaterial(0)->GetName()==TEXT("M_TorchFlame")) ++Legacy;
        if(It->ActorHasTag(TEXT("DockSetting")) || It->ActorHasTag(TEXT("DockTavernInterior")) || It->ActorHasTag(TEXT("WaterdeepPlaza")))
        {
            TArray<UPointLightComponent*> Points;It->GetComponents(Points);
            for(auto* P : Points)
                Lights.Add({P,P->Intensity,static_cast<float>(P->GetComponentLocation().X*.071+P->GetComponentLocation().Y*.043)});
        }
    }
    FTimerHandle Flicker;World->GetTimerManager().SetTimer(Flicker,[World,Lights](){
        const float Time=World->GetTimeSeconds();
        for(const auto& L : Lights) if(L.Component.IsValid())
            L.Component->SetIntensity(L.Intensity*(.96f+.045f*FMath::Sin(Time*6.3f+L.Phase)+.025f*FMath::Sin(Time*10.7f+L.Phase*1.7f)));
    },.075f,true);
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest")))
        UE_LOG(LogTemp,Display,TEXT("CHUCK_FIRE_CHECK failures=%d flames=%d flicker_lights=%d legacy_primitives=%d"),Failures+(Flames<30)+(Legacy>0),Flames,Lights.Num(),Legacy);
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckFireCapture")))
    {
        auto* Camera=World->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(62);
        const FVector Views[]={FVector(-1120,-3000,290),FVector(650,-2120,325),FVector(165,765,78),FVector(-630,-3540,100),FVector(-1120,-3000,290),FVector(165,765,78),FVector(-1070,145,270),FVector(1100,3500,310)};
        const FVector Targets[]={FVector(-1325,-3150,280),FVector(530,-2230,310),FVector(280,870,58),FVector(-780,-3685,73),FVector(-1325,-3150,280),FVector(280,870,58),FVector(-1175,40,251),FVector(940,3650,298)};
        for(int32 I=0;I<8;++I)
        {
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,P=Views[I],T=Targets[I],I](){
                if(I==4) MarkDockSewerExited();
                Camera->SetActorLocationAndRotation(P,(T-P).Rotation());World->GetFirstPlayerController()->SetViewTarget(Camera);
            },4.f+I*3.f,false);
            World->GetTimerManager().SetTimer(Shot,[I](){
                const FString Folder=FPaths::ScreenShotDir()/TEXT("Fire");IFileManager::Get().MakeDirectory(*Folder,true);
                FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);
            },6.f+I*3.f,false);
        }
        FTimerHandle Exit;World->GetTimerManager().SetTimer(Exit,[World](){World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));},30.f,false);
    }
}
