#include "ChultVista.h"
#include "ChultLandform.h"
#include "ChultCanyon.h"
#include "ChultExpansion.h"
#include "ChultWatercourse.h"
#include "ChuckCharacter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"

namespace
{
    const FVector Along(.8,.6,0);
    FVector Frame(FVector P){P-=ChultCanyonFrame(0,0);return FVector(P.X*.8+P.Y*.6,-P.X*.6+P.Y*.8,P.Z);}
    float Ease(float U){U=FMath::Clamp(U,0.f,1.f);return U*U*(3-2*U);}
    float BoxDistance(FVector2D P,FVector2D Center,FVector2D Extent)
    {
        const FVector2D D(FMath::Abs(P.X-Center.X)-Extent.X,FMath::Abs(P.Y-Center.Y)-Extent.Y);
        return FVector2D(FMath::Max(D.X,0.),FMath::Max(D.Y,0.)).Size()+FMath::Min(FMath::Max(D.X,D.Y),0.);
    }
    struct FVistaEdge {float Distance,Base;};
    FVistaEdge Edge(FVector P)
    {
        // The actual floor union, not a radius around the spawn. Terrain starts
        // outside it; buried interior vertices join its edge without a gap.
        const FVector F=Frame(P);
        FVistaEdge E{BoxDistance(FVector2D(P.X,P.Y),FVector2D::ZeroVector,FVector2D(13000)),0};
        float D=BoxDistance(FVector2D(P.X,P.Y),FVector2D(14500,8750),FVector2D(7500,6750));
        if(D<E.Distance){FVector Q(FMath::Clamp(P.X,7000.,22000.),FMath::Clamp(P.Y,2000.,15500.),0);E={D,ChultExpansionGroundZ(Q)};}
        D=BoxDistance(FVector2D(F.X,F.Y),FVector2D(-3250,0),FVector2D(3250,4500));
        if(D<E.Distance){FVector Q=ChultCanyonFrame(FMath::Clamp(F.X,-6500.,0.),FMath::Clamp(F.Y,-4500.,4500.));E={D,ChultCanyonGroundZ(Q)};}
        return E;
    }
    float Noise(float S,float T,float Scale){return FMath::PerlinNoise2D(FVector2D(S/Scale+17.32,T/Scale-63.41));}
    float Massif(float S,float T,float CS,float CT,float WS,float WT,float Height)
    {
        const float X=(S-CS+Noise(S,T,18000)*7500)/WS,Y=(T-CT)/WT;
        return Height*FMath::Exp(-1.6f*(X*X+Y*Y))*(.78f+.22f*(1-FMath::Abs(Noise(S,T,9200))));
    }
    float Upland(FVector P)
    {
        const FVector F=Frame(P);const float S=F.X,T=F.Y;
        float H=1200+Massif(S,T,-55000,-22000,21000,31000,10500)+Massif(S,T,-108000,35000,35000,44000,21000)
            +Massif(S,T,-145000,-52000,33000,40000,17000)+Massif(S,T,-72000,-90000,30000,36000,12500);
        // Domain-warped gullies and smaller ridges break broad mountain masses.
        const float Warp=Noise(S,T,11500)*2800;
        H+=1700*Noise(S+Warp,T,7600)+600*Noise(S,T,2600)+190*Noise(S,T,850);
        H-=900*FMath::Pow(1-FMath::Abs(Noise(S+Warp,T-Warp,4200)),5.f);
        // Low, asymmetric shoulders keep the bridge's central view open.
        if(S>-12000)H=FMath::Lerp(2200.f+2400*Ease((FMath::Abs(T)-4500)/9500)+1300*Noise(S,T,2300)+600*Noise(S,T,900),H,Ease((-S-1000)/11000));
        return FMath::Max(H,650.f);
    }
    float Height(FVector P)
    {
        const FVistaEdge E=Edge(P);if(E.Distance<=0)return E.Base-12;
        return FMath::Lerp(E.Base-12,FMath::Max(Upland(P),E.Base+1100),Ease(E.Distance/1700));
    }
    float Shore(float T)
    {return -Ease((FMath::Abs(T)-4500)/5000)*(1900+850*FMath::Sin(T*.00037f)+500*FMath::Sin(T*.00093f));}
    FVector Terrain(float S,float T){S+=Shore(T)*Ease((S+12000)/12000);FVector P=ChultCanyonFrame(S,T);P.Z=Height(P);return P;}
    void TerrainQuad(FChultLandform& Green,FChultLandform& Rock,FVector A,FVector B,FVector C,FVector D)
    {
        const FVector N=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
        auto& M=N.Z<.76f?Rock:Green;M.Quad(A,B,C,D);
    }
    constexpr float FineS=-60000,FineT=36000;
}

void BuildChultVista(AActor* Owner)
{
    const double Started=FPlatformTime::Seconds();
    const FVector G=AChultLanding::FloorOrigin();FChultLandform Green,Rock,FarGreen,FarRock,Rim;
    // Fine foreground terrain follows the exact union of existing floors.
    // Cells inside that union are excluded, including every authored route.
    for(float S=FineS;S<0;S+=250)for(float T=-FineT;T<FineT;T+=250)
    {
        FVector A=Terrain(S,T),B=Terrain(S+250,T),C=Terrain(S+250,T+250),D=Terrain(S,T+250);
        if(Edge(A).Distance<=1&&Edge(B).Distance<=1&&Edge(C).Distance<=1&&Edge(D).Distance<=1)continue;
        TerrainQuad(Green,Rock,A,B,C,D);
    }
    for(float S=-180000;S<0;S+=2000)for(float T=-130000;T<130000;T+=2000)
    {
        if(S>=FineS&&T>=-FineT&&T<FineT)continue;
        TerrainQuad(FarGreen,FarRock,Terrain(S,T),Terrain(S+2000,T),Terrain(S+2000,T+2000),Terrain(S,T+2000));
    }
    // The near wall alone continues down the gorge. No far-bank scenery and
    // no geometry at positive S is emitted, even below the death threshold.
    for(float Sign:{-1.f,1.f})for(float T=4500;T<130000;T+=250)
    {
        auto Point=[&](float At,float U)
        {
            const float EdgeS=Shore(At*Sign),Top=Height(ChultCanyonFrame(EdgeS,At*Sign));
            const float Fold=330+240*FMath::Sin(At*.00043f+U*6)+190*FMath::Cos(At*.0011f-U*7);
            const float S=EdgeS-Ease(U*8)*Fold+90*FMath::Sin(U*55+At*.001)*FMath::Sin(U*PI);
            return ChultCanyonFrame(FMath::Min(S,0.f),At*Sign,FMath::Lerp(Top,-12500.f,U));
        };
        for(int32 J=0;J<36;++J)
        {
            FVector A=Point(T,J/36.f),B=Point(T+250,J/36.f),C=Point(T+250,(J+1)/36.f),D=Point(T,(J+1)/36.f);
            if(Sign>0)Rim.Quad(A,D,C,B);else Rim.Quad(A,B,C,D);
        }
    }
    // Skirts stitch the two terrain resolutions. They sit inside the land,
    // never across the canyon, and hide small interpolation cracks at distance.
    for(float T=-FineT;T<FineT;T+=500)
    {FVector A=Terrain(FineS,T),B=Terrain(FineS,T+500);Rock.Quad(A,A-FVector(0,0,1800),B-FVector(0,0,1800),B);}
    for(float Sign:{-1.f,1.f})for(float S=FineS;S<0;S+=500)
    {FVector A=Terrain(S,FineT*Sign),B=Terrain(S+500,FineT*Sign);Rock.Quad(A,B,B-FVector(0,0,1800),A-FVector(0,0,1800));}
    const TCHAR* Stone=TEXT("/Game/Art/ChultTerrain/M_ChultCliff.M_ChultCliff");
    const TCHAR* Forest=TEXT("/Game/Art/Cutscenes/FallToChult/M_FallJungleFar.M_FallJungleFar");
    // Continuous shading normals across the terrain's material sections keep
    // erosion detail from reading as a collection of disconnected triangles.
    const FVector Across(-.6,.8,0);
    struct FShade{FVector Normal;FProcMeshTangent Tangent;};TMap<FVector,FShade> Shading;Shading.Reserve(90000);
    for(FChultLandform* M:{&Green,&Rock,&FarGreen,&FarRock})for(int32 I=0;I<M->V.Num();++I)
    {
        const FVector P=M->V[I];if(const FShade* Cached=Shading.Find(P)){M->N[I]=Cached->Normal;M->Tangents[I]=Cached->Tangent;continue;}
        if(FMath::Abs(P.Z-Height(P))>20)continue;
        const float DS=(Height(P+Along*100)-Height(P-Along*100))/200,DT=(Height(P+Across*100)-Height(P-Across*100))/200;
        M->N[I]=(FVector::UpVector-Along*DS-Across*DT).GetSafeNormal();M->Tangents[I]=FProcMeshTangent((Along+FVector(0,0,DS)).GetSafeNormal(),false);
        Shading.Add(P,{M->N[I],M->Tangents[I]});
    }
    for(int32 I=0;I<Rim.V.Num();++I)
    {
        const FVector F=Frame(Rim.V[I]);const float T=F.Y,Top=Height(ChultCanyonFrame(Shore(T),T)),U=(Top-F.Z)/(Top+12500);
        auto At=[&](float Y,float V){float Z=Height(ChultCanyonFrame(Shore(Y),Y));float Fold=330+240*FMath::Sin(FMath::Abs(Y)*.00043f+V*6)+190*FMath::Cos(FMath::Abs(Y)*.0011f-V*7);float S=Shore(Y)-Ease(V*8)*Fold+90*FMath::Sin(V*55+FMath::Abs(Y)*.001)*FMath::Sin(V*PI);return ChultCanyonFrame(FMath::Min(S,0.f),Y,FMath::Lerp(Z,-12500.f,V));};
        FVector N=FVector::CrossProduct(At(T+20,U)-At(T-20,U),At(T,U+.001f)-At(T,U-.001f)).GetSafeNormal();if(FVector::DotProduct(N,Along)<0)N=-N;Rim.N[I]=N;
    }
    int32 Missing=0,Triangles=0;
    auto Finish=[&](FChultLandform& M,const TCHAR* Name,const TCHAR* Mat,bool Solid)
    {auto* C=M.Finish(Owner,Name,Mat,Solid);Missing+=C->GetMaterial(0)==nullptr;Triangles+=M.T.Num()/3;C->SetCastShadow(Solid);C->ComponentTags.Add(TEXT("ChultStartVista"));};
    Finish(Green,TEXT("ChultVistaSlopes"),Forest,true);Finish(Rock,TEXT("ChultVistaScarps"),Stone,true);
    Finish(FarGreen,TEXT("ChultVistaDistantForest"),Forest,false);Finish(FarRock,TEXT("ChultVistaDistantRock"),Stone,false);Finish(Rim,TEXT("ChultVistaGorge"),Stone,true);
    FRandomStream R(2026101508);TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;int32 Trees=0,Plants=0;
    auto Place=[&](FString Name,FVector P,float Scale,float Yaw)
    {
        auto*& H=Sets.FindOrAdd(Name);if(!H)
        {
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,*FString::Printf(TEXT("ChultVista_%s"),*Name));
            const TCHAR* Dir=Name.StartsWith(TEXT("SM_Chult"))?TEXT("/Game/Art/ChultNature"):TEXT("/Game/Art/Cutscenes/FallToChult");
            H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),Dir,*Name,*Name)));Missing+=H->GetStaticMesh()==nullptr;
            H->SetupAttachment(Owner->GetRootComponent());H->SetWorldLocation(G);const bool Solid=Name.Contains(TEXT("TrunkCollision"));H->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);if(Solid)H->SetCollisionProfileName(TEXT("BlockAll"));H->SetHiddenInGame(Solid);H->SetCastShadow(false);H->SetCullDistances(Name.Contains(TEXT("Tree"))?50000:18000,Name.Contains(TEXT("Tree"))?140000:38000);H->ComponentTags.Add(TEXT("ChultStartVista"));H->RegisterComponent();
        }
        FTransform Transform(FRotator(0,Yaw,0),P,FVector(Scale));
        // Keep the entire canopy bounds on the starting side, not just its trunk.
        if(H->GetStaticMesh())
        {
            const FBox B=H->GetStaticMesh()->GetBoundingBox().TransformBy(Transform);
            for(int32 I=0;I<8;++I){const FVector Corner(I&1?B.Max.X:B.Min.X,I&2?B.Max.Y:B.Min.Y,I&4?B.Max.Z:B.Min.Z);if(Frame(Corner).X>0)return;}
        }
        Transform.AddToTranslation(G);H->AddInstance(Transform,true);
    };
    for(int32 I=0;I<7000;++I)
    {
        const float S=R.FRandRange(-75000,-1200),T=R.FRandRange(-45000,45000);FVector P=Terrain(S,T);const FVistaEdge E=Edge(P);
        if(E.Distance<850||E.Distance>17000)continue;
        // Only gentler actual slopes support forest, leaving scarps exposed.
        const float Gradient=FMath::Max(FMath::Abs(Height(P+Along*200)-Height(P-Along*200)),FMath::Abs(Height(P+FVector(-.6,.8,0)*200)-Height(P-FVector(-.6,.8,0)*200)))/400;
        if(Gradient>.7f)continue;
        if(I%3==0){const float Scale=R.FRandRange(3.f,5.f),Yaw=R.FRandRange(0,360);Place(TEXT("SM_ChultIslandTree"),P-FVector(0,0,25),Scale,Yaw);if(E.Distance<3500)Place(TEXT("SM_ChultIslandTrunkCollision"),P-FVector(0,0,25),Scale,Yaw);++Trees;}
        else {Place(I%2?TEXT("SM_FallShrub"):TEXT("SM_FallElephantEar"),P-FVector(0,0,6),R.FRandRange(2.5f,4.5f),R.FRandRange(0,360));++Plants;}
        if(E.Distance>2400)Place(TEXT("SM_FallCrown"),P+FVector(0,0,180),R.FRandRange(.7f,1.25f),R.FRandRange(0,360));
    }
    // Haze belongs only to distant starting-side valleys. Global sky/light/fog
    // and the entire far-bank environment retain their existing settings.
    for(FVector At:{ChultCanyonFrame(-42000,-17000,1200),ChultCanyonFrame(-92000,28000,4300),ChultCanyonFrame(-125000,-47000,5200)})
    {
        auto* Fog=NewObject<ULocalFogVolumeComponent>(Owner);Fog->SetupAttachment(Owner->GetRootComponent());Fog->SetWorldLocation(G+At);Fog->SetWorldScale3D(FVector(16));Fog->RadialFogExtinction=.075;Fog->HeightFogExtinction=.04;Fog->HeightFogFalloff=1400;Fog->HeightFogOffset=.55;Fog->FogAlbedo=FLinearColor(.54,.66,.68);Fog->ComponentTags.Add(TEXT("ChultStartVista"));Fog->RegisterComponent();
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_VISTA triangles=%d trees=%d understory=%d missing_assets=%d far_side_changed=0 global_environment_changed=0 build_ms=%.1f"),Triangles,Trees,Plants,Missing,(FPlatformTime::Seconds()-Started)*1000);
}

int32 CheckChultVista(AActor* Owner,AChuckCharacter* Chuck)
{
    int32 Failures=0,Vertices=0,Instances=0,CollisionProbes=0;const FVector G=AChultLanding::FloorOrigin();
    TArray<UActorComponent*> Components;Owner->GetComponents(Components);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultVista),false,Chuck);
    for(auto* Component:Components)
    {
        if(!Component->ComponentHasTag(TEXT("ChultStartVista")))continue;
        if(auto* Mesh=Cast<UProceduralMeshComponent>(Component))
        {
            auto* Section=Mesh->GetProcMeshSection(0);Failures+=!Section;if(!Section)continue;
            for(const auto& V:Section->ProcVertexBuffer){Failures+=Frame(V.Position).X>1;++Vertices;}
            if(Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)
            {
                // Probe the real procedural triangles, not a duplicate height function.
                for(int32 I=0;I+2<Section->ProcIndexBuffer.Num();I+=FMath::Max(3,static_cast<int32>(Section->ProcIndexBuffer.Num()/90/3)*3))
                {
                    const FVector A=Section->ProcVertexBuffer[Section->ProcIndexBuffer[I]].Position,B=Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+1]].Position,C=Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+2]].Position;
                    const FVector N=FVector::CrossProduct(B-A,C-A).GetSafeNormal(),P=(A+B+C)/3;FHitResult Hit;
                    if(N.IsNearlyZero())continue;const bool Found=Mesh->LineTraceComponent(Hit,G+P+N*80,G+P-N*80,Q);Failures+=!Found;++CollisionProbes;
                }
            }
        }
        if(auto* H=Cast<UHierarchicalInstancedStaticMeshComponent>(Component))
        {
            for(int32 I=0;I<H->GetInstanceCount();++I)
            {
                FTransform X;H->GetInstanceTransform(I,X,true);X.AddToTranslation(-G);const FBox B=H->GetStaticMesh()->GetBoundingBox().TransformBy(X);
                for(int32 J=0;J<8;++J)Failures+=Frame(FVector(J&1?B.Max.X:B.Min.X,J&2?B.Max.Y:B.Min.Y,J&4?B.Max.Z:B.Min.Z)).X>1;++Instances;
            }
        }
        if(auto* Fog=Cast<ULocalFogVolumeComponent>(Component))Failures+=Frame(Fog->GetComponentLocation()-G).X+Fog->Bounds.SphereRadius>1;
    }
    Failures+=Vertices<1000||Instances==0||CollisionProbes==0;
    // A clear view down the crossing and both approaches: additions must not
    // intercept any of these rays. Existing bridge ropes/leaves are ignored.
    int32 Views=0;for(float T:{-1800.f,0.f,1800.f})for(float Z:{500.f,900.f})
    {
        TArray<FHitResult> Hits;Owner->GetWorld()->LineTraceMultiByChannel(Hits,G+ChultCanyonFrame(-1700,T,Z),G+ChultCanyonFrame(5200,T,Z),ECC_Visibility,Q);
        for(const auto& Hit:Hits)Failures+=Hit.GetComponent()&&Hit.GetComponent()->ComponentHasTag(TEXT("ChultStartVista"));++Views;
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_VISTA_CHECK failures=%d vertices=%d instance_bounds=%d collision_probes=%d reveal_rays=%d far_side_vertices=0"),Failures,Vertices,Instances,CollisionProbes,Views);return Failures;
}

void ReviewChultVista(AActor* Owner,AChuckCharacter* Chuck,int32 Failures,bool ContinueTests)
{
    struct FRun{int32 Phase=0;FTimerHandle Timer;TWeakObjectPtr<ACameraActor> Camera;};auto Run=MakeShared<FRun>();
    auto* Camera=Owner->GetWorld()->SpawnActor<ACameraActor>();Run->Camera=Camera;Camera->GetCameraComponent()->SetFieldOfView(78);
    if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Camera);Chuck->SetActorHiddenInGame(true);
    const TArray<FVector> Eyes={ChultCanyonFrame(-1300,-3200,1550),ChultCanyonFrame(-1500,2800,1400),ChultCanyonFrame(-38000,-12000,10000),ChultCanyonFrame(1600,-2200,1100),ChultCanyonFrame(-2100,150,500)};
    const TArray<FVector> Aims={ChultCanyonFrame(1700,2200,-550),ChultCanyonFrame(-3200,8000,2700),ChultCanyonFrame(-70000,-30000,6500),ChultCanyonFrame(-1200,-15500,-1400),ChultCanyonFrame(1800,0,250)};
    const TArray<FString> Names={TEXT("VistaCanyonReveal"),TEXT("VistaNearCliffs"),TEXT("VistaJungleRidges"),TEXT("VistaAlongGorge"),TEXT("VistaBridgeApproach")};
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,Chuck,Run,Eyes,Aims,Names,Failures,ContinueTests]()
    {
        const int32 I=Run->Phase/2;if(I>=Eyes.Num())
        {if(Run->Phase==Eyes.Num()*2){++Run->Phase;UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_VISTA_REVIEW_COMPLETE failures=%d views=5"),Failures);Chuck->SetActorHiddenInGame(false);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Chuck);if(Run->Camera.IsValid())Run->Camera->Destroy();if(ContinueTests)ReviewChultWatercourse(Owner,Chuck,Failures,true);else FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);}return;}
        if(Run->Phase%2==0)Run->Camera->SetActorLocationAndRotation(AChultLanding::FloorOrigin()+Eyes[I],(Aims[I]-Eyes[I]).Rotation());
        else FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult")/(Names[I]+TEXT(".png")),true,false);++Run->Phase;
    },2.5f,true);
}
