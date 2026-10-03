#include "DockReturn.h"
#include "SewerSlide.h"
#include "ChuckCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMeshActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

bool CheckDockReturn(UWorld* World,bool Evening)
{
    int32 Failed=0;FHitResult Hit;FCollisionQueryParams Q;
    if(auto* PC=World->GetFirstPlayerController()) if(PC->GetPawn()) Q.AddIgnoredActor(PC->GetPawn());
    for(float X : {-1660.f,-1580.f,-1500.f})
    {
        const bool Floor=World->LineTraceSingleByChannel(Hit,FVector(X,3900,50),FVector(X,3900,-100),ECC_Visibility,Q);
        if(Floor!=Evening || (Floor && (Hit.ImpactPoint.Z<5 || Hit.ImpactPoint.Z>18))) ++Failed;
    }
    const bool DoorBlocked=World->SweepSingleByChannel(Hit,FVector(40,280,35),FVector(40,460,35),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f),Q);
    if(DoorBlocked==Evening || (DoorBlocked && !Hit.GetActor()->ActorHasTag(TEXT("Door")))) ++Failed;
    int32 Suns=0,Skies=0;
    for(TActorIterator<ADirectionalLight> It(World);It;++It)
    {++Suns;if(FMath::Abs(It->GetLightComponent()->Intensity-(Evening?1.35f*.28f:1.35f))>.01f) ++Failed;}
    for(TActorIterator<ASkyLight> It(World);It;++It)
    {++Skies;if(FMath::Abs(It->GetLightComponent()->Intensity-(Evening?1.05f*.65f:1.05f))>.01f) ++Failed;}
    if(Suns!=1 || Skies!=1) ++Failed;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_RETURN_CHECK failures=%d evening=%d hatch_closed=%d tavern_open=%d sun=%.3f sky=%.3f"),Failed,Evening,Evening,!DoorBlocked,Evening?1.35f*.28f:1.35f,Evening?1.05f*.65f:1.05f);
    return Failed==0;
}

void BuildDockReturn(UWorld* World)
{
    auto* State=World->SpawnActor<AActor>();State->Tags.Add(TEXT("DockReturnState"));
    FTimerHandle Poll;
    World->GetTimerManager().SetTimer(Poll,[World,State](){
        if(!HasExitedDockSewer() || State->ActorHasTag(TEXT("DockEvening"))) return;
        // Claude marks this while the slide view is black, before the pier fade-in.
        State->Tags.Add(TEXT("DockEvening"));
        for(TActorIterator<AActor> It(World);It;++It)
        {
            if(It->ActorHasTag(TEXT("SewerHatchLid")))
            {
                It->SetActorRotation(FRotator::ZeroRotator);
                TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);
                for(auto* P : Parts)
                {
                    if(P->ComponentHasTag(TEXT("OpenHatchStay"))) {P->SetHiddenInGame(true);P->SetCollisionProfileName(TEXT("NoCollision"));}
                    if(P->ComponentHasTag(TEXT("ClosedHatchFloor"))) P->SetCollisionProfileName(TEXT("BlockAll"));
                }
            }
            if(It->ActorHasTag(TEXT("Door")))
            {It->SetActorLocation(FVector(-23,395,105));It->SetActorRotation(FRotator(0,90,0));}
            if(It->ActorHasTag(TEXT("DoorLatch")))
            {It->SetActorLocation(FVector(-30,442,100));It->SetActorRotation(FRotator(0,90,0));}
            if(It->ActorHasTag(TEXT("Horizon")))
                if(auto* Mesh=Cast<AStaticMeshActor>(*It))
                    if(auto* Material=Mesh->GetStaticMeshComponent()->CreateAndSetMaterialInstanceDynamic(0))
                        Material->SetVectorParameterValue(TEXT("SkyTint"),FLinearColor(.46f,.50f,.68f));
        }
        for(TActorIterator<ADirectionalLight> It(World);It;++It)
        {It->SetActorRotation(FRotator(-9,-40,0));It->GetLightComponent()->SetIntensity(1.35f*.28f);It->GetLightComponent()->SetLightColor(FLinearColor(.76f,.65f,.57f));}
        for(TActorIterator<ASkyLight> It(World);It;++It)
        {It->GetLightComponent()->SetIntensity(1.05f*.65f);It->GetLightComponent()->RecaptureSky();}
        for(TActorIterator<AExponentialHeightFog> It(World);It;++It)
            It->GetComponent()->SetFogInscatteringColor(FLinearColor(.12f,.14f,.21f));
        UE_LOG(LogTemp,Display,TEXT("CHUCK_DOCK_RETURN_APPLIED evening=1 hatch_closed=1 tavern_open=1"));
    },.05f,true);

    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckReturnTest")))
    {
        struct FRun {float Time=0;int32 Phase=0,Failures=0;FTimerHandle Timer;};auto Run=MakeShared<FRun>();FTimerHandle Start;
        World->GetTimerManager().SetTimer(Start,[World,Run](){
            auto* C=Cast<AChuckCharacter>(World->GetFirstPlayerController()->GetPawn());
            Run->Failures+=!CheckDockReturn(World,false);
            C->SetActorLocation(FVector(40,280,34.65f));C->SetRunHeld(false);
            World->GetTimerManager().SetTimer(Run->Timer,[World,Run,C](){
                Run->Time+=.02f;
                if(Run->Phase==0)
                {
                    C->AddMovementInput(FVector(0,1,0),1);
                    if(Run->Time>2)
                    {
                        if(C->GetActorLocation().Y>325) ++Run->Failures;
                        C->GetCharacterMovement()->StopMovementImmediately();C->ResetToDock();
                        C->SetActorLocation(DockSewerSlideApproach(),false,nullptr,ETeleportType::TeleportPhysics);
                        Run->Phase=1;
                    }
                }
                else if(Run->Phase==1)
                {
                    if(!C->IsAstral() && !HasExitedDockSewer()) C->AddMovementInput(DockSewerSlideInward(),1);
                    if(HasExitedDockSewer() && !C->IsAstral() && C->GetCharacterMovement()->IsMovingOnGround())
                    {
                        Run->Failures+=!CheckDockReturn(World,true);
                        if(C->GetActorLocation().X<1500 || FMath::Abs(C->GetActorLocation().Y-3080)>150) ++Run->Failures;
                        C->SetActorLocation(FVector(40,280,34.65f));C->GetCharacterMovement()->StopMovementImmediately();Run->Phase=2;
                    }
                }
                else if(Run->Phase==2)
                {
                    C->AddMovementInput(FVector(0,1,0),1);
                    if(C->GetActorLocation().Y>475)
                    {
                        C->ResetToDock();Run->Failures+=!CheckDockReturn(World,true);Run->Phase=3;
                    }
                }
                if(Run->Phase==3 || Run->Time>35)
                {
                    if(Run->Phase!=3) ++Run->Failures;
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_RETURN_TEST_COMPLETE failures=%d initial_door_blocked=1 slid=%d post_door_walked=%d persists_after_reset=%d elapsed=%.2f"),Run->Failures,C->GetSlides(),Run->Phase==3,HasExitedDockSewer(),Run->Time);
                    World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
                }
            },.02f,true);
        },3.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckReturnCapture")))
    {
        auto* Camera=World->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(78);
        for(int32 I=0;I<4;++I)
        {
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,I](){
                if(I==2) MarkDockSewerExited();
                const bool Door=I%2==0;const FVector P=Door?FVector(210,100,120):FVector(-1260,3700,190);
                const FVector Target=Door?FVector(40,440,135):FVector(-1580,3900,10);
                Camera->SetActorLocationAndRotation(P,(Target-P).Rotation());World->GetFirstPlayerController()->SetViewTarget(Camera);
            },4.f+I*4.f,false);
            World->GetTimerManager().SetTimer(Shot,[I](){
                const FString Folder=FPaths::ScreenShotDir()/TEXT("Return");IFileManager::Get().MakeDirectory(*Folder,true);
                FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);
            },6.f+I*4.f,false);
        }
        FTimerHandle Exit;World->GetTimerManager().SetTimer(Exit,[World](){World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));},21.f,false);
    }
}
