#include "ChultWatercourse.h"
#include "ChultLandform.h"
#include "ChultExpansion.h"
#include "ChultCanyon.h"
#include "ChultRuins.h"
#include "ChuckCharacter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"

namespace
{
    FVector Outfall(float U)
    {
        const float Z=FMath::Lerp(50.f,-4650.f,U),Top=ChultCanyonGroundZ(ChultCanyonFrame(0,-1800));
        const float F=FMath::Clamp((Top-Z)/(Top+4700),0.f,1.f);
        return ChultCanyonFrame(2000*F+90*FMath::Sin(-7.2f)*FMath::Sin(F*PI)+25*FMath::Min(U*10,1.f),-1800+FMath::Sin(U*13)*12,Z);
    }
}

void BuildChultWatercourse(AActor* Owner)
{
    const FVector G=AChultLanding::FloorOrigin();FChultLandform Rock,Water;
    Rock.Rock(FVector(22000,6600,1100),FVector(650,600,1400),18,2);
    Rock.Rock(FVector(22000,5400,700),FVector(550,500,1000),-24,5);
    Rock.Rock(FVector(22200,6000,450),FVector(300,200,500),0,9);
    Rock.Rock(FVector(21900,7050,350),FVector(650,450,450),25,7);
    Rock.Rock(FVector(21850,5000,150),FVector(650,600,350),-35,11);
    // Side shelves break up the gorge, well below the fall-recovery threshold.
    for(int32 I=0;I<36;++I)
    {
        const float T=-4300+(I%18)*480.f,Z=I<18?-1350.f:-3050.f;
        if(FMath::Abs(T+1800)<600)continue;
        const float F=(350-Z)/5050,S=2000*F+90*FMath::Sin(T*.004f)*FMath::Sin(F*PI);
        Rock.Rock(ChultCanyonFrame(S-100,T,Z),FVector(310+(I%3)*70,390,430+(I%5)*50),37,I*3.2f);
    }
    auto Ribbon=[&](TFunction<FVector(float)> At,int32 Steps,float Width,FVector Side)
    {
        for(int32 I=0;I<Steps;++I)
        {
            const float U=I/static_cast<float>(Steps),V=(I+1)/static_cast<float>(Steps);
            const FVector A=At(U),B=At(V),Normal=FVector::CrossProduct(Side,B-A).GetSafeNormal();
            for(int32 K=0;K<4;++K)
            {
                const float L=K/4.f,H=(K+1)/4.f;
                const FVector P[]={A+Side*(L-.5f)*Width,A+Side*(H-.5f)*Width,B+Side*(H-.5f)*Width,B+Side*(L-.5f)*Width};
                const int32 Start=Water.V.Num();
                for(int32 J=0;J<4;++J)
                {
                    const float Across=J==0||J==3?L:H,Along=J<2?U:V;
                    Water.V.Add(P[J]);Water.N.Add(Normal);Water.UV.Add(FVector2D(Across,Along*FVector::Dist(At(0),At(1))/160));
                    Water.C.Add(FColor(255,255,255,Across==0||Across==1?0:220));Water.Tangents.Add(FProcMeshTangent(Side,false));
                }
                Water.T.Append({Start,Start+2,Start+1,Start,Start+3,Start+2});
            }
        }
    };
    Ribbon([](float U){return FMath::Lerp(FVector(22050,6000,470),FVector(21700,6000,470),U);},5,65,FVector(0,1,0));
    Ribbon([](float U){return FMath::Lerp(FVector(21700,6000,470),FVector(21500,6000,170),U)+FVector(0,FMath::Sin(U*8)*4,0);},16,85,FVector(0,1,0));
    Ribbon([](float U){return Outfall(U);},100,125,FVector(-.6,.8,0));
    // Short broken secondary filaments, rather than an opaque white curtain.
    for(int32 I=0;I<6;++I)
        Ribbon([I](float U){return Outfall(.12f+I*.13f+U*.12f)+FVector(-.6,.8,0)*(I%2?65:-65);},12,26,FVector(-.6,.8,0));
    for(FVector Center:{FVector(21500,6000,172),Outfall(1)+FVector(0,0,8),ChultCanyonFrame(-70,-1800,52)})
    {
        for(int32 I=0;I<28;++I)
        {
            const float A=I*2*PI/28,B=(I+1)*2*PI/28;
            const int32 Start=Water.V.Num();
            for(FVector P:{Center,Center+FVector(FMath::Cos(A)*115,FMath::Sin(A)*85,0),Center+FVector(FMath::Cos(B)*115,FMath::Sin(B)*85,0)})
            {Water.V.Add(P);Water.N.Add(FVector::UpVector);Water.UV.Add(FVector2D((P.X-Center.X)/230+.5f,(P.Y-Center.Y)/170+.5f));Water.C.Add(FColor(255,255,255,P==Center?180:0));Water.Tangents.Add(FProcMeshTangent(FVector::ForwardVector,false));}
            Water.T.Append({Start,Start+2,Start+1});
        }
    }
    auto* Stone=Rock.Finish(Owner,TEXT("ChultWatercourseRock"),TEXT("/Game/Art/ChultTerrain/M_ChultCliff.M_ChultCliff"));
    auto* Falls=Water.Finish(Owner,TEXT("ChultWaterfalls"),TEXT("/Game/Art/ChultTerrain/M_ChultWaterfall.M_ChultWaterfall"),false);Falls->SetCastShadow(false);
    int32 Missing=!Stone->GetMaterial(0)+!Falls->GetMaterial(0),Plants=0;
    FRandomStream R(2026101411);TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;
    auto Plant=[&](FString Name,FVector At,float Scale)
    {
        auto*& H=Sets.FindOrAdd(Name);if(!H)
        {
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner);const TCHAR* Dir=Name.StartsWith(TEXT("SM_Chult"))?TEXT("/Game/Art/ChultNature"):TEXT("/Game/Art/Cutscenes/FallToChult");
            H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),Dir,*Name,*Name)));Missing+=!H->GetStaticMesh();H->SetupAttachment(Owner->GetRootComponent());H->SetWorldLocation(G);H->SetCollisionEnabled(ECollisionEnabled::NoCollision);H->SetCullDistances(6500,11000);H->RegisterComponent();
        }
        H->AddInstance(FTransform(FRotator(0,R.FRandRange(0,360),0),G+At,FVector(Scale)),true);++Plants;
    };
    for(int32 I=0;I<400;++I)
    {
        FVector At( R.FRandRange(20200,21900),R.FRandRange(4700,7300),0);
        if(ChultWatercourseDistance(At)<240)continue;At.Z=ChultExpansionGroundZ(At)-3;
        Plant(I%3?TEXT("SM_FallElephantEar"):FString::Printf(TEXT("SM_ChultFern%d"),I%4),At,I%3?1.5f:2.7f);
    }
    for(FVector At:{FVector(22000,6600,2480),FVector(22000,5400,1680),FVector(21850,5000,480)})
    {
        Plant(TEXT("SM_FallPalm"),At,1.2f);
        for(int32 J=0;J<20;++J)Plant(J%2?TEXT("SM_FallShrub"):TEXT("SM_ChultFern2"),At+FVector(R.FRandRange(-130,130),R.FRandRange(-130,130),-15),J%2?1.3f:2.2f);
    }
    for(int32 I=0;I<36;++I)
    {
        const float T=-4300+(I%18)*480.f,Z=I<18?-1350.f:-3050.f,F=(350-Z)/5050;
        if(FMath::Abs(T+1800)<600)continue;
        FVector At=ChultCanyonFrame(2000*F+90*FMath::Sin(T*.004f)*FMath::Sin(F*PI)-100,T,Z+430+(I%5)*50-15);
        for(int32 J=0;J<5;++J)Plant(J%2?TEXT("SM_FallShrub"):TEXT("SM_ChultFern1"),At+FVector(J*25-50,0,0),J%2?1.1f:2.f);
    }
    for(FVector At:{FVector(21500,6000,185),Outfall(1)+FVector(0,0,60)})
    {
        auto* Fog=NewObject<ULocalFogVolumeComponent>(Owner);Fog->SetupAttachment(Owner->GetRootComponent());Fog->SetWorldLocation(G+At);Fog->SetWorldScale3D(FVector(At.Z>0?.38f:.8f));Fog->RadialFogExtinction=.35f;Fog->HeightFogExtinction=.22f;Fog->HeightFogFalloff=1600;Fog->HeightFogOffset=.6f;Fog->FogAlbedo=FLinearColor(.7f,.79f,.74f);Fog->RegisterComponent();
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WATERCOURSE source_fall_cm=300 gorge_drop_cm=4700 plants=%d missing_assets=%d falls_collision=0 triangles=%d"),Plants,Missing,(Rock.T.Num()+Water.T.Num())/3);
}

int32 CheckChultWatercourse(AActor* Owner,AChuckCharacter* Chuck)
{
    int32 Failures=0,Samples=0;const FVector G=AChultLanding::FloorOrigin();FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultWatercourse),false,Chuck);
    const auto& Path=ChultWatercoursePath();
    for(int32 I=0;I<Path.Num();++I)
    {
        const FVector P=Path[I];const float Expected=InChultExpansion(P)?ChultExpansionGroundZ(P):ChultCanyonGroundZ(P);
        FHitResult Hit;const bool Found=Owner->GetWorld()->LineTraceSingleByChannel(Hit,G+P+FVector(0,0,80),G+P-FVector(0,0,100),ECC_Visibility,Q);
        const bool Pass=Found && FMath::Abs(Hit.ImpactPoint.Z-G.Z-Expected)<7 && P.Z-Hit.ImpactPoint.Z+G.Z>7 && P.Z-Hit.ImpactPoint.Z+G.Z<25;
        if(!Pass){++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_WATERCOURSE_BED_FAIL sample=%d at=%s expected=%.1f actual=%.1f component=%s"),I,*P.ToString(),Expected,Hit.ImpactPoint.Z-G.Z,*GetNameSafe(Hit.GetComponent()));}
        if(I)Failures+=Path[I].Z>Path[I-1].Z+.01f;++Samples;
    }
    Failures+=FVector::Dist(Path.Last(),ChultCanyonFrame(0,-1800,50))>.1f;
    Failures+=FVector::Dist(Path[0],FVector(21500,6000,170))>.1f;
    Failures+=FVector::Dist(Outfall(0),Path.Last())>.1f;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WATERCOURSE_CHECK failures=%d bed_samples=%d downhill=1 source_connected=1 canyon_connected=1"),Failures,Samples);return Failures;
}

void ReviewChultWatercourse(AActor* Owner,AChuckCharacter* Chuck,int32 Failures,bool ContinueTests)
{
    struct FRun{int32 Phase=0;FTimerHandle Timer;TWeakObjectPtr<ACameraActor> Camera;};auto Run=MakeShared<FRun>();
    auto* Camera=Owner->GetWorld()->SpawnActor<ACameraActor>();Run->Camera=Camera;Camera->GetCameraComponent()->SetFieldOfView(78);
    if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Camera);Chuck->SetActorHiddenInGame(true);
    TArray<FVector> Eyes={{-6100,-6100,750},{21050,6200,285},{19300,6300,550},ChultCanyonFrame(1200,-2100,200),ChultCanyonFrame(1500,-3000,-900),FVector(-4200,-5200,2100)};
    TArray<FVector> Aims={{-6900,-6900,1650},{21700,6000,350},{21500,6000,200},Outfall(.12f),ChultCanyonFrame(700,-1800,-1600),FVector(-7500,-7000,1800)};
    // Place the review camera in front of the actual fractured face, not inside
    // an overhanging shoulder guessed from the old constant-radius wall.
    if(auto* Cliff=FindObjectFast<UProceduralMeshComponent>(Owner,TEXT("ChultBoundaryCliffs")))
    {
        const FVector Out(-.70710678,-.70710678,0),G=AChultLanding::FloorOrigin();FHitResult Face;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(CliffCamera),false,Chuck);
        if(Cliff->LineTraceComponent(Face,G+Out*6000+FVector(0,0,750),G+Out*12000+FVector(0,0,750),Q))
        {Eyes[0]=Face.ImpactPoint-G-Out*650;Aims[0]=Face.ImpactPoint-G+FVector(0,0,550);}
    }
    const TArray<FString> Names={TEXT("JungleCliffs"),TEXT("SourceWaterfall"),TEXT("HeadwaterStream"),TEXT("CanyonWaterfallLip"),TEXT("CanyonWaterfall"),TEXT("JungleCliffsWide")};
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,Chuck,Run,Eyes,Aims,Names,Failures,ContinueTests]()
    {
        const int32 I=Run->Phase/2;if(I>=Eyes.Num())
        {
            if(Run->Phase==Eyes.Num()*2)
            {
                ++Run->Phase;UE_LOG(LogTemp,Display,TEXT("CHUCK_LANDSCAPE_REVIEW_COMPLETE failures=%d"),Failures);
                Chuck->SetActorHiddenInGame(false);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Chuck);if(Run->Camera.IsValid())Run->Camera->Destroy();
                if(ContinueTests)StartChultRuinsReview(Owner,Chuck);
                else if(!Failures && FParse::Param(FCommandLine::Get(),TEXT("ChuckWatercourseWalksOnly")))StartChultWatercourseWalks(Owner,Chuck);
                else FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
            }return;
        }
        if(Run->Phase%2==0)Run->Camera->SetActorLocationAndRotation(AChultLanding::FloorOrigin()+Eyes[I],(Aims[I]-Eyes[I]).Rotation());
        else FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult")/(Names[I]+TEXT(".png")),true,false);
        ++Run->Phase;
    },2.f,true);
}

void StartChultWatercourseWalks(AActor* Owner,AChuckCharacter* Chuck)
{
    struct FRun{int32 Trial=0,Failures=0;float Start=-1;FTimerHandle Timer;};auto Run=MakeShared<FRun>();
    const TArray<FVector> Starts={{21100,6160,0},{21750,12850,0},ChultCanyonFrame(-650,-1800)};
    const TArray<FVector> Ends={{21400,6040,0},{22100,12630,0},ChultCanyonFrame(-120,-1800)};
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,Chuck,Run,Starts,Ends]()
    {
        if(Run->Trial>=3)return;const float Now=Owner->GetWorld()->GetTimeSeconds();const FVector G=AChultLanding::FloorOrigin(),A=Starts[Run->Trial],B=Ends[Run->Trial];
        if(Run->Start<0)
        {
            const float Z=InChultExpansion(A)?ChultExpansionGroundZ(A):ChultCanyonGroundZ(A);
            Chuck->ResetAtLocation(G+A+FVector(0,0,Z+34.65f));Chuck->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Chuck->GetCharacterMovement()->StopMovementImmediately();Run->Start=Now;
        }
        const float Distance=FVector::Dist2D(Chuck->GetActorLocation(),G+B),Time=Now-Run->Start;
        if(Distance>45 && Time<14){const FVector D=(G+B-Chuck->GetActorLocation()).GetSafeNormal2D();Chuck->SetTestStickWorld(D);Chuck->AddMovementInput(D,1,true);return;}
        const bool Pass=Distance<45 && Chuck->GetCharacterMovement()->IsMovingOnGround();Run->Failures+=!Pass;Chuck->SetTestStick(FVector2D::ZeroVector);
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WATERCOURSE_WALK trial=%d pass=%d distance=%.1f time=%.2f"),Run->Trial,Pass,Distance,Time);
        ++Run->Trial;Run->Start=-1;
        if(Run->Trial==3){UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WATERCOURSE_TEST failures=%d trials=3"),Run->Failures);FPlatformMisc::RequestExitWithStatus(false,Run->Failures?1:0);}
    },.016f,true);
}
