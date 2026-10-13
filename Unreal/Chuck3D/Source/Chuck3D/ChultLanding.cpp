#include "ChultLanding.h"
#include "ChultRootProfiles.h"
#include "ChultDressing.h"
#include "ProceduralMeshComponent.h"
#include "FallCutscene.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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
    Sun->SetIntensity(4.5f); Sun->SetLightColor(FLinearColor(1,.91f,.73f));
    Sun->DynamicShadowDistanceMovableLight=9000; Sun->DynamicShadowCascades=3;
    Attach(Sun,FVector::ZeroVector,FRotator(-30,20,0));
    auto* Fill=NewObject<USkyLightComponent>(this);
    Fill->SetMobility(EComponentMobility::Movable); Fill->bRealTimeCapture=true;
    Fill->SetIntensity(.45f); Attach(Fill);
    auto* Fog=NewObject<UExponentialHeightFogComponent>(this);
    Fog->SetFogDensity(.035f); Fog->SetFogHeightFalloff(.18f); Fog->SetVolumetricFog(true);
    Fog->SetVolumetricFogDistance(9000); Fog->SetFogInscatteringColor(FLinearColor(.21f,.235f,.22f)); Attach(Fog);
    auto* Post=NewObject<UPostProcessComponent>(this);
    Post->bUnbound=true; Post->Priority=100;
    auto& P=Post->Settings;
    P.bOverride_AutoExposureMethod=true; P.AutoExposureMethod=AEM_Histogram;
    P.bOverride_AutoExposureMinBrightness=true; P.AutoExposureMinBrightness=-2;
    P.bOverride_AutoExposureMaxBrightness=true; P.AutoExposureMaxBrightness=10;
    P.bOverride_AutoExposureBias=true; P.AutoExposureBias=-1.3f;
    P.bOverride_ColorSaturation=true; P.ColorSaturation=FVector4(.93f,.93f,.9f,1);
    P.bOverride_ColorGain=true; P.ColorGain=FVector4(1,1,1,1);
    P.bOverride_BloomIntensity=true; P.BloomIntensity=.4f;
    P.bOverride_AmbientOcclusionIntensity=true; P.AmbientOcclusionIntensity=.6f;
    Attach(Post);
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Floor=NewObject<UStaticMeshComponent>(this);
    Floor->SetStaticMesh(Cube); Floor->SetMaterial(0,JungleMaterial(TEXT("M_FallGround")));
    Floor->SetCollisionProfileName(TEXT("BlockAll")); Floor->SetCastShadow(false);
    Floor->SetWorldScale3D(FVector(260,260,1)); Attach(Floor,FVector(0,0,-50));

    // One query mesh follows the existing art's buttress fins, without turning
    // broad foliage into collision or allocating thousands of root actors.
    TArray<FVector> RootVertices; TArray<int32> RootTriangles;
    auto Solid=[&](const TArray<FVector>& V)
    {
        const int32 Base=RootVertices.Num(); RootVertices.Append(V);
        const int32 Faces[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
        for(int32 I=0;I<UE_ARRAY_COUNT(Faces);I+=3)
            if(FVector::CrossProduct(V[Faces[I+1]]-V[Faces[I]],V[Faces[I+2]]-V[Faces[I]]).SizeSquared()>1.e-4f)
                for(int32 J=0;J<3;++J) RootTriangles.Add(Base+Faces[I+J]);
    };
    auto Roots=[&](FVector At,float Yaw,float Scale,bool TypeA)
    {
        const auto* Profiles=TypeA?ChultRoots::A:ChultRoots::B;
        const int32 Count=TypeA?UE_ARRAY_COUNT(ChultRoots::A):UE_ARRAY_COUNT(ChultRoots::B);
        const float BaseRadius=TypeA?78.f:62.f;
        const FTransform Transform(FRotator(0,Yaw,0),At,FVector(Scale));
        for(int32 K=0;K<Count;++K) for(int32 J=0;J<8;++J)
        {
            const auto& R=Profiles[K];
            const FVector Out=FRotator(0,R.Angle,0).Vector(),Side(-Out.Y,Out.X,0);
            TArray<FVector> V;
            for(int32 End=0;End<2;++End)
            {
                const float T=(J+End)/8.f, Radius=FMath::Lerp(BaseRadius*.7f,R.Reach,T);
                const float Z=R.Top*FMath::Pow(1.f-T,2.2f)-6;
                for(const FVector Local : {Out*Radius-Side*R.Width+FVector(0,0,-10),Out*Radius+Side*R.Width+FVector(0,0,-10),
                    Out*Radius+Side*R.Width+FVector(0,0,Z),Out*Radius-Side*R.Width+FVector(0,0,Z)})
                    V.Add(Transform.TransformPosition(Local));
            }
            Solid(V);
        }
        // The trunk flares to 2.1 times its nominal radius at ground level.
        // Match that base with a tapered twelve-sided collar below the capsule.
        for(int32 K=0;K<12;++K)
        {
            const FVector A=FRotator(0,K*30.f,0).Vector(),B=FRotator(0,(K+1)*30.f,0).Vector();
            TArray<FVector> V;
            for(const FVector Local : {FVector(0,0,-10),A*BaseRadius*2.1f+FVector(0,0,-10),B*BaseRadius*2.1f+FVector(0,0,-10),FVector(0,0,-10),
                FVector(0,0,360),A*BaseRadius+FVector(0,0,360),B*BaseRadius+FVector(0,0,360),FVector(0,0,360)})
                V.Add(Transform.TransformPosition(Local));
            Solid(V);
        }
    };

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
        if(FString(T.Name).Contains(TEXT("Tree")))
        { const bool A=FString(T.Name).EndsWith(TEXT("_A")); Trunk(At,(A?78:62)*T.Scale,2700*T.Scale); Roots(At,T.Yaw,T.Scale,A); }
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
        const bool A=R.FRand()<.5f; const float Yaw=R.FRandRange(0,360);
        Place(A?TEXT("SM_FallTree_A"):TEXT("SM_FallTree_B"),At,Yaw,Scale);
        Trunk(At,(A?78:62)*Scale,2700*Scale); Roots(At,Yaw,Scale,A); ++TreeCount;
    }
    Place(TEXT("SM_FallCrown"),FVector(60,-40,3300),40,.7f);
    Place(TEXT("SM_FallCrown"),FVector(-90,30,2250),160,.5f);
    const TCHAR* Under[]={TEXT("SM_FallFern"),TEXT("SM_FallElephantEar"),TEXT("SM_FallShrub"),TEXT("SM_FallPalm")};
    for(int32 I=0;I<1900;++I)
    {
        FVector At(R.FRandRange(-8000,8000),R.FRandRange(-8000,8000),0);
        if(At.Size2D()>8500 || OnRoute(At,230)) continue;
        const int32 Type=R.RandRange(0,3); const float Yaw=R.FRandRange(0,360),Scale=R.FRandRange(.65f,1.3f);
        Place(Under[Type],At,Yaw,Scale);
        if(Type==3) Trunk(At,14*Scale,700*Scale);
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
    auto* RootCollision=NewObject<UProceduralMeshComponent>(this,TEXT("ChultRootCollision"));
    RootCollision->bUseComplexAsSimpleCollision=true;
    RootCollision->CreateMeshSection(0,RootVertices,RootTriangles,TArray<FVector>(),TArray<FVector2D>(),TArray<FColor>(),TArray<FProcMeshTangent>(),true);
    RootCollision->SetCollisionProfileName(TEXT("BlockAll")); RootCollision->SetHiddenInGame(true); RootCollision->SetCastShadow(false); Attach(RootCollision);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_ROOTS triangles=%d trees=%d"),RootTriangles.Num()/3,TreeCount);
    BuildChultWetDressing(this,true);
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
    int32 RootFailures=0;
    const FTransform Tree(FRotator(0,30,0),FloorOrigin()+FVector(-760,200,0),FVector(1.4f));
    for(const auto& Root : ChultRoots::A)
    {
        const FVector Out=FRotator(0,Root.Angle,0).Vector(),Side(-Out.Y,Out.X,0);
        const FVector At=Out*FMath::Lerp(78.f*.7f,Root.Reach,.6f)+FVector(0,0,(Root.Top*FMath::Pow(.4f,2.2f)-6)*.5f);
        FHitResult Hit;
        const bool RayBlocked=GetWorld()->LineTraceSingleByChannel(Hit,Tree.TransformPosition(At+Side*60),Tree.TransformPosition(At-Side*60),ECC_Pawn,Q)
            && Hit.GetComponent() && Hit.GetComponent()->GetFName()==TEXT("ChultRootCollision");
        FVector Body=Tree.TransformPosition(At); Body.Z=FloorOrigin().Z+Half+3;
        const FVector Across=Tree.TransformVectorNoScale(Side)*90.f;
        const bool BodyBlocked=GetWorld()->SweepSingleByChannel(Hit,Body+Across,Body-Across,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q)
            && Hit.GetComponent() && Hit.GetComponent()->GetFName()==TEXT("ChultRootCollision");
        RootFailures+=!RayBlocked || !BodyBlocked;
    }
    Failures+=RootFailures;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_ROOT_CHECK failures=%d outer_roots=%d"),RootFailures,UE_ARRAY_COUNT(ChultRoots::A));
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
    struct FRun {int32 Phase=0;FTimerHandle Timer;TWeakObjectPtr<ACameraActor> DetailCamera;}; auto Run=MakeShared<FRun>();
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
            auto* Camera=GetWorld()->SpawnActor<ACameraActor>();
            Run->DetailCamera=Camera;
            const FVector From=FloorOrigin()+FVector(145,170,100),At=FloorOrigin()+FVector(320,440,110);
            Camera->SetActorLocationAndRotation(From,(At-From).Rotation());
            Camera->GetCameraComponent()->SetFieldOfView(58);
            if(auto* PC=Cast<APlayerController>(C->GetController())) PC->SetViewTarget(Camera);
            C->SetActorHiddenInGame(true);
        }
        else if(Run->Phase==7) FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult/Surface_Detail.png"),true,false);
        else if(Run->Phase==8)
        {
            C->SetActorHiddenInGame(false);
            if(auto* PC=Cast<APlayerController>(C->GetController())) PC->SetViewTarget(C);
            if(Run->DetailCamera.IsValid()) Run->DetailCamera->Destroy();
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEST_COMPLETE"));
            // Clearing this timer destroys the executing lambda (and its last
            // shared Run reference). Leave it alive until world shutdown.
            FPlatformMisc::RequestExit(false);
            return;
        }
        ++Run->Phase;
    },1.5f,true);
}
