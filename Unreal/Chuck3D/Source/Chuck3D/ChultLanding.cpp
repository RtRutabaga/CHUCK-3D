#include "ChultLanding.h"
#include "FallCutscene.h"
#include "Camera/PlayerCameraManager.h"
#include "ChuckCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LightComponentBase.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Info.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"

namespace
{
    // Clockwise ground loop from the cinematic clearing, around the old trees.
    // Dimensions are centimetres. No grips are registered in the landing.
    const FVector Route[] = {{0,0,0},{700,0,0},{1600,500,0},{2450,300,0},
        {3300,-150,0},{4200,500,0},{4600,1350,0},{3900,2200,0},
        {2800,2450,0},{1700,1900,0},{900,1500,0},{100,1100,0},{0,0,0}};
    float SegmentDistance(FVector P, FVector A, FVector B)
    {
        P.Z=A.Z=B.Z=0;
        const FVector AB=B-A;
        return FVector::Dist(P,A+AB*FMath::Clamp(FVector::DotProduct(P-A,AB)/AB.SizeSquared(),0.,1.));
    }
    bool OnRoute(FVector P, float Margin)
    {
        // Retain the cutscene camera's pullback lane, as well as walking space.
        if(P.Size2D()<300 || (P.X>-40 && P.X<1300 && FMath::Abs(P.Y+.27f*FMath::Max(0.,P.X-170))<290+P.X*.2f)) return true;
        for(int32 I=1;I<UE_ARRAY_COUNT(Route);++I)
            if(SegmentDistance(P,Route[I-1],Route[I])<Margin) return true;
        return false;
    }
    UStaticMesh* JungleMesh(const TCHAR* Name)
    { return LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/Art/Cutscenes/FallToChult/%s.%s"),Name,Name)); }
    UMaterialInterface* JungleMaterial(const TCHAR* Name)
    { return LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Cutscenes/FallToChult/%s.%s"),Name,Name)); }
}

AChultLanding::AChultLanding()
{
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Tags.Add(TEXT("ChultLanding"));
}
FVector AChultLanding::FloorOrigin() { return AFallCutscene::LandingSpot(); }
FVector AChultLanding::Entrance() { return FloorOrigin()+FVector(0,0,34.65f); }
bool AChultLanding::Contains(const FVector& P)
{ return FVector::Dist2D(P,FloorOrigin())<14000; }

AChultLanding* AChultLanding::EnterPreview(UWorld* World)
{
    if(!World) return nullptr;
    for(TActorIterator<AChultLanding> It(World);It;++It) return *It;
    auto* Chuck=Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World,0));
    // A checkpoint selection enters a fresh paused world. Keep its timers alive,
    // but disable Waterdeep rendering, collision and global environment effects.
    for(TActorIterator<AActor> It(World);It;++It)
    {
        AActor* A=*It;
        if(A==Chuck || A->GetAttachParentActor()==Chuck || A->IsA<AController>() || A->IsA<APlayerCameraManager>() || A->IsA<AHUD>() || A->IsA<AInfo>()) continue;
        if(auto* Volume=Cast<APostProcessVolume>(A)) Volume->bEnabled=false;
        A->SetActorHiddenInGame(true);
        A->SetActorEnableCollision(false);
        TInlineComponentArray<UActorComponent*> Parts(A);
        for(auto* Part:Parts)
        {
            if(auto* Audio=Cast<UAudioComponent>(Part)) Audio->Stop();
            else if(Part->IsA<ULightComponentBase>() || Part->IsA<UExponentialHeightFogComponent>() || Part->IsA<USkyAtmosphereComponent>() || Part->IsA<UVolumetricCloudComponent>())
                Cast<USceneComponent>(Part)->SetVisibility(false);
        }
    }
    auto* Setting=World->SpawnActor<AChultLanding>();
    Setting->Build();
    return Setting;
}

void AChultLanding::Build()
{
    const FVector G=FloorOrigin();
    auto Attach=[this,G](USceneComponent* C, FVector P=FVector::ZeroVector, FRotator R=FRotator::ZeroRotator)
    { C->SetupAttachment(RootComponent); C->SetWorldLocationAndRotation(G+P,R); C->RegisterComponent(); };
    auto* Sky=NewObject<USkyAtmosphereComponent>(this);
    Sky->SetRayleighScattering(FLinearColor(.13f,.43f,.66f)); Attach(Sky);
    auto* Sun=NewObject<UDirectionalLightComponent>(this);
    Sun->SetMobility(EComponentMobility::Movable); Sun->SetAtmosphereSunLight(true);
    Sun->SetIntensity(4.5f); Sun->SetLightColor(FLinearColor(1,.76f,.52f));
    Sun->DynamicShadowDistanceMovableLight=9000; Sun->DynamicShadowCascades=3;
    Attach(Sun,FVector::ZeroVector,FRotator(-30,20,0));
    auto* Fill=NewObject<USkyLightComponent>(this);
    Fill->SetMobility(EComponentMobility::Movable); Fill->bRealTimeCapture=true;
    Fill->SetIntensity(.7f); Attach(Fill);
    auto* Fog=NewObject<UExponentialHeightFogComponent>(this);
    Fog->SetFogDensity(.05f); Fog->SetFogHeightFalloff(20); Fog->SetVolumetricFog(true);
    Fog->SetVolumetricFogDistance(7000); Fog->SetFogInscatteringColor(FLinearColor(.12f,.2f,.16f)); Attach(Fog);
    auto* Post=NewObject<UPostProcessComponent>(this);
    Post->bUnbound=true; Post->Priority=100;
    auto& P=Post->Settings;
    P.bOverride_AutoExposureMethod=true; P.AutoExposureMethod=AEM_Histogram;
    P.bOverride_AutoExposureMinBrightness=true; P.AutoExposureMinBrightness=-2;
    P.bOverride_AutoExposureMaxBrightness=true; P.AutoExposureMaxBrightness=10;
    P.bOverride_AutoExposureBias=true; P.AutoExposureBias=-.7f;
    P.bOverride_BloomIntensity=true; P.BloomIntensity=.4f;
    P.bOverride_AmbientOcclusionIntensity=true; P.AmbientOcclusionIntensity=.6f;
    Attach(Post);
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Floor=NewObject<UStaticMeshComponent>(this);
    Floor->SetStaticMesh(Cube); Floor->SetMaterial(0,JungleMaterial(TEXT("M_FallGround")));
    Floor->SetCollisionProfileName(TEXT("BlockAll")); Floor->SetCastShadow(false);
    Floor->SetWorldScale3D(FVector(260,260,1)); Attach(Floor,FVector(0,0,-50));

    TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;
    auto Place=[&](const TCHAR* Name,FVector At,float Yaw,float Scale)
    {
        auto*& H=Sets.FindOrAdd(Name);
        if(!H)
        {
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
            H->SetStaticMesh(JungleMesh(Name)); H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Attach(H);
        }
        H->AddInstance(FTransform(FRotator(0,Yaw,0),G+At,FVector(Scale)),true);
    };
    // Meshes have no imported collision. Simple hidden blockers match solid
    // trunks, leaving the broad leaves and lianas transparent to the camera.
    auto Trunk=[&](FVector At,float Radius,float Height)
    {
        auto* C=NewObject<UCapsuleComponent>(this);
        C->InitCapsuleSize(Radius,Height*.5f); C->SetCollisionProfileName(TEXT("BlockAll"));
        C->SetHiddenInGame(true); Attach(C,At+FVector(0,0,Height*.5f-Radius));
    };
    struct FTree {const TCHAR* Name;float X,Y,Yaw,Scale;};
    // These near placements are copied exactly from the cutscene's clearing.
    const FTree Trees[]={
        {TEXT("SM_FallTree_A"),-760,200,30,1.4f},{TEXT("SM_FallTree_B"),320,-950,100,1.05f},
        {TEXT("SM_FallTree_A"),950,760,210,.85f},{TEXT("SM_FallTree_B"),-350,-1350,250,.95f},
        {TEXT("SM_FallTree_B"),-1500,950,15,1.1f},{TEXT("SM_FallTree_A"),1600,-320,140,.9f},
        {TEXT("SM_FallTree_A"),-1900,-800,300,1.15f},{TEXT("SM_FallTree_B"),2200,1500,75,1},
        {TEXT("SM_FallTree_A"),400,2300,190,1.05f},{TEXT("SM_FallTree_B"),-2600,200,120,.9f},
        {TEXT("SM_FallTree_A"),2700,-1900,45,1.1f},{TEXT("SM_FallTree_B"),-800,2900,280,1},
        {TEXT("SM_FallPalm"),-260,620,200,1},{TEXT("SM_FallPalm"),760,420,20,.9f},
        {TEXT("SM_FallPalm"),-1100,-420,110,1.1f},{TEXT("SM_FallPalm"),1300,300,290,.95f},
        {TEXT("SM_FallLog"),-380,-1050,10,1}};
    for(const auto& T:Trees)
    {
        const FVector At(T.X,T.Y,0); Place(T.Name,At,T.Yaw,T.Scale);
        if(FString(T.Name).Contains(TEXT("Tree"))) Trunk(At,70*T.Scale,2700*T.Scale);
        else if(FString(T.Name).Contains(TEXT("Palm"))) Trunk(At,14*T.Scale,700*T.Scale);
    }
    FRandomStream R(1994);
    // A nonuniform forest surrounds a deliberately open winding ground loop.
    int32 TreeCount=12;
    for(float Y=-8500;Y<=8500;Y+=720) for(float X=-8500;X<=8500;X+=720)
    {
        FVector At(X+R.FRandRange(-260,260),Y+R.FRandRange(-260,260),0);
        if(At.Size2D()<3000 || At.Size2D()>9000 || OnRoute(At,400)) continue;
        float Scale=R.FRandRange(.75f,1.3f);
        Place(R.FRand()<.5f?TEXT("SM_FallTree_A"):TEXT("SM_FallTree_B"),At,R.FRandRange(0,360),Scale);
        Trunk(At,70*Scale,2700*Scale); ++TreeCount;
    }
    Place(TEXT("SM_FallCrown"),FVector(60,-40,3300),40,.7f);
    Place(TEXT("SM_FallCrown"),FVector(-90,30,2250),160,.5f);
    const TCHAR* Under[]={TEXT("SM_FallFern"),TEXT("SM_FallElephantEar"),TEXT("SM_FallShrub"),TEXT("SM_FallPalm")};
    for(int32 I=0;I<1900;++I)
    {
        FVector At(R.FRandRange(-8000,8000),R.FRandRange(-8000,8000),0);
        if(At.Size2D()>8500 || OnRoute(At,230)) continue;
        Place(Under[R.RandRange(0,3)],At,R.FRandRange(0,360),R.FRandRange(.65f,1.3f));
    }
    Place(TEXT("SM_FallFern"),FVector(330,210,0),40,1.1f);
    // Layered foliage at the route edges: keep the playable centre unobscured.
    for(int32 I=1;I<UE_ARRAY_COUNT(Route);++I)
    {
        const FVector Along=(Route[I]-Route[I-1]).GetSafeNormal();
        const FVector Side(-Along.Y,Along.X,0);
        for(float D=0;D<FVector::Dist(Route[I],Route[I-1]);D+=190)
            for(float Sign:{-1.f,1.f})
                Place(TEXT("SM_FallFern"),Route[I-1]+Along*D+Side*Sign*R.FRandRange(280,370),R.FRandRange(0,360),R.FRandRange(.7f,1.1f));
    }
    // A fallen timber beside the loop is a small climbable exploration surface.
    Place(TEXT("SM_FallLog"),FVector(3350,950,0),15,1.2f);
    auto* Timber=NewObject<UBoxComponent>(this);
    Timber->SetBoxExtent(FVector(420,45,48)); Timber->SetCollisionProfileName(TEXT("BlockAll"));
    Attach(Timber,FVector(3350,950,58),FRotator(0,15,0));
    int32 Instances=0,Missing=0;
    for(auto& Set:Sets) {Instances+=Set.Value->GetInstanceCount(); Missing+=Set.Value->GetStaticMesh()==nullptr;}
    Missing+=Floor->GetMaterial(0)==nullptr;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_LANDING trees=%d instances=%d missing_assets=%d origin=%s grips=0"),TreeCount,Instances,Missing,*G.ToString());
}

void AChultLanding::VerifyAndCapture()
{
    auto* C=Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!C) return;
    int32 Failures=0,Samples=0;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultLanding),false,C);
    const float Half=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Radius=C->GetCapsuleComponent()->GetScaledCapsuleRadius();
    for(int32 I=1;I<UE_ARRAY_COUNT(Route);++I)
    {
        const FVector A=FloorOrigin()+Route[I-1],B=FloorOrigin()+Route[I];
        FHitResult Hit;
        Failures+=GetWorld()->SweepSingleByChannel(Hit,A+FVector(0,0,Half+3),B+FVector(0,0,Half+3),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q);
        for(float D=0;D<=FVector::Dist(A,B);D+=100)
        {
            const FVector At=FMath::Lerp(A,B,D/FVector::Dist(A,B));
            bool Ground=GetWorld()->LineTraceSingleByChannel(Hit,At+FVector(0,0,150),At-FVector(0,0,30),ECC_Visibility,Q);
            Failures+=!(Ground && FMath::Abs(Hit.ImpactPoint.Z-FloorOrigin().Z)<1 && Hit.ImpactNormal.Z>.95f); ++Samples;
        }
    }
    Failures+=!Contains(C->GetAreaStartLocation());
    C->SetOrbitPitch(-5);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CHECK failures=%d floor_samples=%d capsule_segments=%d local_respawn=%d"),Failures,Samples,UE_ARRAY_COUNT(Route)-1,Contains(C->GetAreaStartLocation()));
    struct FRun {int32 Phase=0;FTimerHandle Timer;}; auto Run=MakeShared<FRun>();
    GetWorldTimerManager().SetTimer(Run->Timer,[this,C,Run](){
        if(Run->Phase==0) FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult/Landing_Rat.png"),true,false);
        else if(Run->Phase==1) C->SetOrbitPitch(-48);
        else if(Run->Phase==2) FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult/Landing_Elevated.png"),true,false);
        else if(Run->Phase==3) C->ResetAtLocation(FloorOrigin()+Route[5]+FVector(0,0,34.65f));
        else if(Run->Phase==4) FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult/Jungle_Loop.png"),true,false);
        else if(Run->Phase==5)
        {
            C->RespawnAtAreaStart();
            const bool Pass=FVector::Dist(C->GetActorLocation(),Entrance())<1;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_RESPAWN failures=%d entrance=%d"),!Pass,Pass);
        }
        else if(Run->Phase==6)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEST_COMPLETE"));
            // Clearing this timer destroys the executing lambda (and its last
            // shared Run reference). Leave it alive until world shutdown.
            FPlatformMisc::RequestExit(false);
            return;
        }
        ++Run->Phase;
    },1.5f,true);
}
