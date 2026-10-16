#include "ChultVista.h"
#include "ChultGorge.h"
#include "ChultGrid.h"
#include "ChultLandform.h"
#include "ChultCanyon.h"
#include "ChultWatercourse.h"
#include "ChultRiver.h"
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

// The land on both rims of the gorge (Claude, 2026-10-15; replaces the
// earlier starting-side vista). The gorge itself is ChultGorge.cpp; this file
// lays the rims and plateaus from the same lip functions, so land and walls
// share every edge vertex, and dresses them.
namespace
{
    const FVector Along(.8,.6,0),Across(-.6,.8,0);
    FVector Frame(FVector P){P-=ChultCanyonFrame(0,0);return FVector(FVector::DotProduct(P,Along),FVector::DotProduct(P,Across),P.Z);}
    float Ease(float U){U=FMath::Clamp(U,0.f,1.f);return U*U*(3-2*U);}
    float Noise(float X,float Y,float Scale,float Seed){return FMath::PerlinNoise2D(FVector2D(X/Scale+5.13f+Seed*2.71f,Y/Scale+29.9f-Seed*1.37f));}
    TArray<float> Rows(float Fine,float Step,float Limit)
    {
        TArray<float> U={0,30,80,150,240,350,500,700,950,1250,1600,2000};
        for(float X=2000+Fine;X<Limit;X+=X<8000?Fine:Step)U.Add(X);U.Add(Limit);return U;
    }
    // Rock where the land is genuinely steep, broken up so it never follows a grid line.
    float Steepness(const FChultGrid& G,int32 I,int32 J)
    {
        const FVector N=G.Normal(I,J,FVector::UpVector);return FMath::Sqrt(FMath::Max(0.f,1-N.Z*N.Z))/FMath::Max(N.Z,.05f);
    }
}

void BuildChultVista(AActor* Owner)
{
    const double Started=FPlatformTime::Seconds();const FVector G=AChultLanding::FloorOrigin();
    BuildChultGorge(Owner);
    auto* Ground=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/ChultGorge/M_ChultGorge.M_ChultGorge"));
    int32 Missing=!Ground,Triangles=0,Chunks=0,SolidChunks=0,Reserved=0;
    const TArray<float> Cols=ChultGorgeColumns();
    // Each rim: a grid of columns along the gorge and rows inland from the lip,
    // finer where the player can see detail. Far inland a coarse sheet follows.
    for(int32 Side=0;Side<2;++Side)
    {
        // The far side's detailed land now reaches 120 m past the upriver floor's end (2026-10-15).
        const float Sg=Side==0?1.f:-1.f;const float Limit=Side==0?50000:ChultFarFloorEnd-4500+12000;
        for(int32 Band=0;Band<3;++Band)
        {
            // Band 0: |T|<=50000 at 2.5 m; bands 1, 2: either end, coarser.
            TArray<float> BandCols;
            for(float T:Cols)if(Band==0?FMath::Abs(T)<=50000:Band==1?T<=-50000:T>=50000)BandCols.Add(T);
            const TArray<float> U=Rows(Band==0?250:1000,Band==0?250:1000,Limit);
            FChultGrid Grid;Grid.Init(BandCols.Num(),U.Num());TArray<float> FloorDistance;FloorDistance.SetNum(Grid.P.Num());
            for(int32 J=0;J<U.Num();++J)for(int32 I=0;I<BandCols.Num();++I)
            {
                const float T=BandCols[I],S=ChultGorgeLip(Side,T)-Sg*U[J];const int32 K=Grid.Id(I,J);
                const float D=ChultGorgeFloorDistance(S,T);FloorDistance[K]=D;
                float Z=ChultGorgeLandZ(S,T,Side);
                // Inside an existing floor: buried just below it, never over it.
                if(D<=1){Z-=12;Grid.Hidden[K]=1;++Reserved;}
                Grid.P[K]=ChultCanyonFrame(S,T,Z);
            }
            for(int32 J=0;J<U.Num();++J)for(int32 I=0;I<BandCols.Num();++I)
            {
                const FVector P=Grid.P[Grid.Id(I,J)],F=Frame(P);
                const float Steep=Steepness(Grid,I,J),Break=.35f*Noise(F.X,F.Y,900,1)+.15f*Noise(F.X,F.Y,260,2);
                // Jungle clings to all but the steepest ground; far inland, bare
                // rock is mossy crag rather than the gorge's bedded sandstone.
                const float Inland=Ease((U[J]-6000)/20000);
                const float Rock=FMath::Clamp(Ease((Steep-FMath::Lerp(.95f,1.35f,Inland)+Break)/.75f),0.f,1.f);
                const float Crag=FMath::Clamp(.4f+.35f*Noise(F.X,F.Y,2400,3)+.5f*Inland,0.f,1.f);
                Grid.C[Grid.Id(I,J)]=FColor(FMath::RoundToInt(Rock*255),FMath::RoundToInt(Crag*255),0,255);
            }
            for(int32 I0=0;I0<BandCols.Num()-1;I0+=48)for(int32 J0=0;J0<U.Num()-1;J0+=40)
            {
                const int32 I1=FMath::Min(I0+48,BandCols.Num()-1),J1=FMath::Min(J0+40,U.Num()-1);
                bool Solid=false;for(int32 J=J0;J<=J1&&!Solid;J+=4)for(int32 I=I0;I<=I1&&!Solid;I+=4)Solid=FloorDistance[Grid.Id(I,J)]<5000;
                if(auto* Mesh=Grid.Emit(Owner,FString::Printf(TEXT("ChultRim%d_%d_%d_%d"),Side,Band,I0,J0),Ground,I0,I1,J0,J1,[](int32,int32){return FVector::UpVector;},Solid,Solid||U[J0]<6000,Triangles))
                {Mesh->ComponentTags.Add(TEXT("ChultStartVista"));++Chunks;SolidChunks+=Solid;}
            }
            // Skirts hide the seams where the coarser bands and sheets meet.
            if(Band>0)
            {
                FChultLandform Skirt;const int32 I=Band==1?BandCols.Num()-1:0;
                for(int32 J=0;J+1<U.Num();++J){const FVector A=Grid.P[Grid.Id(I,J)],B=Grid.P[Grid.Id(I,J+1)];Skirt.Quad(A,B,B-FVector(0,0,1500),A-FVector(0,0,1500));Skirt.Quad(A,A-FVector(0,0,1500),B-FVector(0,0,1500),B);}
                if(auto* Mesh=Skirt.Finish(Owner,*FString::Printf(TEXT("ChultRimSkirt%d_%d"),Side,Band),TEXT("/Game/Art/ChultGorge/M_ChultGorge.M_ChultGorge"),false)){Mesh->SetCastShadow(false);Mesh->ComponentTags.Add(TEXT("ChultStartVista"));Triangles+=Skirt.T.Num()/3;}
            }
        }
        // The far plateau and ranges: a coarse sheet beyond the fine rows.
        TArray<float> FarT,FarU;for(float T=-130000;T<=130000;T+=1800)FarT.Add(T);for(float X=Limit;X<=Limit+150000;X+=1800)FarU.Add(X);
        FChultGrid Far;Far.Init(FarT.Num(),FarU.Num());
        for(int32 J=0;J<FarU.Num();++J)for(int32 I=0;I<FarT.Num();++I)
        {
            const float T=FarT[I],S=ChultGorgeLip(Side,T)-Sg*FarU[J];const int32 K=Far.Id(I,J);
            if(ChultGorgeFloorDistance(S,T)<=1)Far.Hidden[K]=1;
            Far.P[K]=ChultCanyonFrame(S,T,ChultGorgeLandZ(S,T,Side)-(J==0?60:0));
        }
        for(int32 J=0;J<FarU.Num();++J)for(int32 I=0;I<FarT.Num();++I)
        {
            const FVector F=Frame(Far.P[Far.Id(I,J)]);const float Rock=FMath::Clamp(Ease((Steepness(Far,I,J)-1.4f+.3f*Noise(F.X,F.Y,5000,4))/.8f),0.f,1.f);
            Far.C[Far.Id(I,J)]=FColor(FMath::RoundToInt(Rock*255),235,0,255);
        }
        for(int32 I0=0;I0<FarT.Num()-1;I0+=26)
            if(auto* Mesh=Far.Emit(Owner,FString::Printf(TEXT("ChultRimFar%d_%d"),Side,I0),Ground,I0,FMath::Min(I0+26,FarT.Num()-1),0,FarU.Num()-1,[](int32,int32){return FVector::UpVector;},false,false,Triangles))
            {Mesh->ComponentTags.Add(TEXT("ChultStartVista"));++Chunks;}
    }

    // Dressing: existing scanned trees, palms, canopy crowns and understory.
    FRandomStream R(2026101531);TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;int32 Trees=0,Plants=0,Crowns=0;
    auto Place=[&](FString Name,FVector P,FQuat Rotation,float Scale,float Near,float Far)
    {
        auto*& H=Sets.FindOrAdd(Name);if(!H)
        {
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,*FString::Printf(TEXT("ChultVista_%s"),*Name));
            const TCHAR* Dir=Name.StartsWith(TEXT("SM_Chult"))?TEXT("/Game/Art/ChultNature"):TEXT("/Game/Art/Cutscenes/FallToChult");
            H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),Dir,*Name,*Name)));Missing+=H->GetStaticMesh()==nullptr;
            H->SetupAttachment(Owner->GetRootComponent());H->SetWorldLocation(G);const bool Solid=Name.Contains(TEXT("TrunkCollision"));
            H->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);if(Solid)H->SetCollisionProfileName(TEXT("BlockAll"));
            H->SetHiddenInGame(Solid);H->SetCastShadow(Name.Contains(TEXT("IslandTree"))&&!Solid);H->SetCullDistances(Near,Far);H->ComponentTags.Add(TEXT("ChultStartVista"));H->RegisterComponent();
        }
        H->AddInstance(FTransform(Rotation,G+P,FVector(Scale)),true);
    };
    auto Tree=[&](FVector P,float Scale,float Yaw,float Lean,int32 Side,bool Reach)
    {
        // Rim trees lean out over the drop towards the light.
        const FVector Out=Along*(Side==0?1.f:-1.f);const FQuat Q=FQuat(FVector::CrossProduct(FVector::UpVector,Out).GetSafeNormal(),FMath::DegreesToRadians(Lean))*FQuat(FVector::UpVector,FMath::DegreesToRadians(Yaw));
        Place(TEXT("SM_ChultIslandTree"),P-FVector(0,0,30),Q,Scale,60000,150000);
        if(Reach)Place(TEXT("SM_ChultIslandTrunkCollision"),P-FVector(0,0,30),Q,Scale,60000,150000);++Trees;
    };
    for(int32 Side=0;Side<2;++Side)
    {
        const float Sg=Side==0?1.f:-1.f;
        // Along both rims, densest near the crossing.
        for(float T=-42000;T<42000;)
        {
            const float A=FMath::Abs(T);T+=A<15000?R.FRandRange(420,780):R.FRandRange(900,1700);
            for(int32 K=0;K<2;++K)
            {
                const float U=K==0?R.FRandRange(60,600):R.FRandRange(700,3200),S=ChultGorgeLip(Side,T)-Sg*U;
                const float Distance=ChultGorgeFloorDistance(S,T);
                // Trees keep the floors' approaches clear; undergrowth does not need to.
                if(Distance>=850)Tree(ChultCanyonFrame(S,T,ChultGorgeLandZ(S,T,Side)),R.FRandRange(2.4f,4.3f),R.FRandRange(0,360),K==0?R.FRandRange(4,15)*Ease(1-U/600):R.FRandRange(-3,3),Side,Distance<4000);
                // Undergrowth spills over the very edge, densest by the bridge.
                if(A<20000)for(int32 J=0;J<(A<10000?10:5);++J)
                {
                    const float Uj=FMath::Square(R.FRand())*900,Tj=T+R.FRandRange(-350,350),Sj=ChultGorgeLip(Side,Tj)-Sg*Uj;if(ChultGorgeFloorDistance(Sj,Tj)<40)continue;
                    const int32 Type=R.RandRange(0,6);
                    Place(Type<3?FString::Printf(TEXT("SM_ChultFern%d"),Type):Type<5?TEXT("SM_FallElephantEar"):TEXT("SM_FallShrub"),ChultCanyonFrame(Sj,Tj,ChultGorgeLandZ(Sj,Tj,Side)-5),FQuat(FVector::UpVector,R.FRandRange(0,2*PI)),R.FRandRange(1.8f,3.4f)*(Type<3?1:.8f),11000,18000);++Plants;
                }
            }
        }
        // Forest over the plateau on gentle ground; canopy crowns farther out.
        for(int32 I=0;I<9000;++I)
        {
            const float T=R.FRandRange(-60000,60000),U=FMath::Square(R.FRand())*(Side==0?48000:22000)+300,S=ChultGorgeLip(Side,T)-Sg*U;
            const float Distance=ChultGorgeFloorDistance(S,T);if(Distance<850)continue;
            const float Z=ChultGorgeLandZ(S,T,Side),Slope=FMath::Max(FMath::Abs(ChultGorgeLandZ(S+200,T,Side)-ChultGorgeLandZ(S-200,T,Side)),FMath::Abs(ChultGorgeLandZ(S,T+200,Side)-Z)*2)/400;
            if(Slope>.75f)continue;const FVector P=ChultCanyonFrame(S,T,Z);
            if(U<9000&&FMath::Abs(T)<36000&&I%3==0){Tree(P,R.FRandRange(2.6f,4.6f),R.FRandRange(0,360),R.FRandRange(-4,4),Side,Distance<4000);if(I%9==0)Place(TEXT("SM_FallPalm"),P+FVector(R.FRandRange(-300,300),R.FRandRange(-300,300),-20),FQuat(FVector::UpVector,R.FRandRange(0,2*PI)),R.FRandRange(1.4f,2.2f),30000,60000);}
            else if(U>7000&&U<(Side==0?47000:21000)&&FMath::Abs(T)<48000){Place(TEXT("SM_FallCrown"),P+FVector(0,0,R.FRandRange(100,350)),FQuat(FVector::UpVector,R.FRandRange(0,2*PI)),R.FRandRange(1.8f,3.f),90000,160000);++Crowns;}
        }
        // Beside the upriver valley and beyond its end (2026-10-15): the same
        // forest and crowns over the far plateau, from its own random stream so
        // the scenery seen from the bridge keeps every earlier draw.
        if(Side==1)
        {
            FRandomStream Up(2026101571);
            for(int32 I=0;I<9000;++I)
            {
                const float T=Up.FRandRange(-60000,60000),U=Up.FRandRange(22000,ChultFarFloorEnd+30000),S=ChultGorgeLip(Side,T)-Sg*U;
                const float Distance=ChultGorgeFloorDistance(S,T);if(Distance<850)continue;
                const float Z=ChultGorgeLandZ(S,T,Side),Slope=FMath::Max(FMath::Abs(ChultGorgeLandZ(S+200,T,Side)-ChultGorgeLandZ(S-200,T,Side)),FMath::Abs(ChultGorgeLandZ(S,T+200,Side)-Z)*2)/400;
                if(Slope>.75f)continue;const FVector P=ChultCanyonFrame(S,T,Z);
                // Full trees on the valley's own slopes, crowns over the distance.
                if(Distance<9000&&I%2==0){Tree(P,Up.FRandRange(2.6f,4.6f),Up.FRandRange(0,360),Up.FRandRange(-4,4),Side,Distance<4000);if(I%8==0)Place(TEXT("SM_FallPalm"),P+FVector(Up.FRandRange(-300,300),Up.FRandRange(-300,300),-20),FQuat(FVector::UpVector,Up.FRandRange(0,2*PI)),Up.FRandRange(1.4f,2.2f),30000,60000);}
                else if(Distance>=6000){Place(TEXT("SM_FallCrown"),P+FVector(0,0,Up.FRandRange(100,350)),FQuat(FVector::UpVector,Up.FRandRange(0,2*PI)),Up.FRandRange(1.8f,3.f),90000,160000);++Crowns;}
            }
        }
        // The walls' soft benches carry ferns, shrubs and the odd clinging tree.
        for(float T=-24000;T<24000;T+=FMath::Abs(T)<9000?110:240)
        {
            const float Top=ChultGorgeTop(Side,T);
            for(float D=800;D<6800;D+=130)
            {
                const float Z=Top-D,Bench=ChultGorgeBench(Side,T,Z);if(Bench<.4f||R.FRand()>.6f)continue;
                const float Tj=T+R.FRandRange(-60,60),S=ChultGorgeWallS(Side,Tj,Z);
                const FVector P=ChultCanyonFrame(S+Sg*R.FRandRange(10,45),Tj,Z-10);
                if(R.FRand()<.05f){Tree(P,R.FRandRange(.9f,1.6f),R.FRandRange(0,360),R.FRandRange(6,20),Side,false);continue;}
                const int32 Type=R.RandRange(0,5);
                Place(Type<3?FString::Printf(TEXT("SM_ChultFern%d"),Type+1):Type==3?TEXT("SM_FallElephantEar"):TEXT("SM_FallShrub"),P,FQuat(FVector::UpVector,R.FRandRange(0,2*PI)),R.FRandRange(1.8f,3.4f)*(Type<3?1:.7f),14000,26000);++Plants;
            }
        }
    }
    // Lianas and moss hang from the rims and from the lips of the hard beds.
    FChultLandform Vines;int32 VineCount=0;
    auto Tube=[&](FVector A,FVector B,float Radius)
    {
        const FVector D=(B-A).GetSafeNormal();FVector X=FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal();if(X.IsNearlyZero())X=Along;
        const FVector Y=FVector::CrossProduct(D,X).GetSafeNormal();
        for(int32 I=0;I<5;++I){const float U=I*2*PI/5,W=(I+1)*2*PI/5;const FVector N0=(X*FMath::Cos(U)+Y*FMath::Sin(U))*Radius,N1=(X*FMath::Cos(W)+Y*FMath::Sin(W))*Radius;Vines.Quad(A+N0,A+N1,B+N1,B+N0);}
    };
    for(int32 Side=0;Side<2;++Side)for(int32 I=0;I<380;++I)
    {
        // Clustered curtains: most near the crossing, where they are seen.
        const float Sg=Side==0?1.f:-1.f,Cluster=FMath::RoundToFloat(R.FRandRange(-14.f,14.f))*900,T=Cluster*FMath::Lerp(.5f,1.f,FMath::Abs(Cluster)/12600)+R.FRandRange(-260,260);
        if(FMath::Abs(T)<300||FMath::Abs(T+1800)<500||FMath::Abs(T-2200)<900)continue;
        const float Top=ChultGorgeTop(Side,T),Start=I%3==0?R.FRandRange(900,3500):R.FRandRange(60,250),Length=R.FRandRange(400,3200);
        FVector Previous=FVector::ZeroVector;
        for(int32 J=0;J<=14;++J)
        {
            const float U=J/14.f,Z=Top-Start-Length*U,Tj=T+FMath::Sin(U*5+I)*25;
            const FVector P=ChultCanyonFrame(ChultGorgeWallS(Side,Tj,Z)+Sg*(16+30*U),Tj,Z);
            if(J)Tube(Previous,P,FMath::Lerp(5.f,1.5f,U));Previous=P;
        }
        ++VineCount;
    }
    if(auto* Mesh=Vines.Finish(Owner,TEXT("ChultGorgeVines"),TEXT("/Game/Art/Cutscenes/FallToChult/M_ChultHangingMoss.M_ChultHangingMoss"),false))
    {Missing+=!Mesh->GetMaterial(0);Mesh->ComponentTags.Add(TEXT("ChultStartVista"));Triangles+=Vines.T.Num()/3;}
    // Valley haze between the far layers on both sides.
    for(FVector At:{ChultCanyonFrame(-42000,-17000,1500),ChultCanyonFrame(-92000,28000,4500),ChultCanyonFrame(-125000,-47000,5200),ChultCanyonFrame(38000,-30000,1800),ChultCanyonFrame(70000,20000,4200)})
    {
        auto* Fog=NewObject<ULocalFogVolumeComponent>(Owner);Fog->SetupAttachment(Owner->GetRootComponent());Fog->SetWorldLocation(G+At);Fog->SetWorldScale3D(FVector(16));
        Fog->RadialFogExtinction=.075;Fog->HeightFogExtinction=.04;Fog->HeightFogFalloff=1400;Fog->HeightFogOffset=.55;Fog->FogAlbedo=FLinearColor(.54,.66,.68);
        Fog->ComponentTags.Add(TEXT("ChultStartVista"));Fog->RegisterComponent();
    }
    for(auto& Pair:Sets)Pair.Value->ComponentTags.AddUnique(TEXT("ChultStartVista"));
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_VISTA triangles=%d chunks=%d solid_chunks=%d reserved_vertices=%d trees=%d crowns=%d understory=%d vines=%d missing_assets=%d far_floor_changed=0 lighting=soft_fill build_ms=%.1f"),
        Triangles,Chunks,SolidChunks,Reserved,Trees,Crowns,Plants,VineCount,Missing,(FPlatformTime::Seconds()-Started)*1000);
}

int32 CheckChultVista(AActor* Owner,AChuckCharacter* Chuck)
{
    int32 Failures=0,Vertices=0,CollisionProbes=0,InGorge=0,OverFloor=0;const FVector G=AChultLanding::FloorOrigin();
    TArray<UActorComponent*> Components;Owner->GetComponents(Components);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultVista),false,Chuck);
    for(auto* Component:Components)
    {
        auto* Mesh=Cast<UProceduralMeshComponent>(Component);if(!Mesh||!Component->ComponentHasTag(TEXT("ChultStartVista")))continue;
        const FString Name=Mesh->GetName();if(!Name.StartsWith(TEXT("ChultRim"))||Name.StartsWith(TEXT("ChultRimSkirt")))continue;
        auto* Section=Mesh->GetProcMeshSection(0);Failures+=!Section;if(!Section)continue;
        for(const auto& V:Section->ProcVertexBuffer)
        {
            const FVector F=Frame(V.Position);++Vertices;
            // No land inside the gorge, and none above an existing floor.
            if(F.X>ChultGorgeLip(0,F.Y)+1&&F.X<ChultGorgeLip(1,F.Y)-1){++InGorge;++Failures;}
            if(ChultGorgeFloorDistance(F.X,F.Y)<=-5&&V.Position.Z>ChultGorgeLandZ(F.X,F.Y)-5){++OverFloor;++Failures;}
        }
        if(Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)
            for(int32 I=0;I+2<Section->ProcIndexBuffer.Num();I+=FMath::Max(3,static_cast<int32>(Section->ProcIndexBuffer.Num()/30/3)*3))
            {
                const FVector A=Section->ProcVertexBuffer[Section->ProcIndexBuffer[I]].Position,B=Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+1]].Position,C=Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+2]].Position;
                const FVector N=FVector::CrossProduct(B-A,C-A).GetSafeNormal(),P=(A+B+C)/3;FHitResult Hit;
                if(N.IsNearlyZero())continue;Failures+=!Mesh->LineTraceComponent(Hit,G+P+N*80,G+P-N*80,Q);++CollisionProbes;
            }
    }
    Failures+=Vertices<1000||CollisionProbes==0;
    // The crossing and both approaches stay open: no new land or tree in the way.
    int32 Views=0;for(float T:{-1800.f,0.f,1800.f})for(float Z:{500.f,900.f})
    {
        TArray<FHitResult> Hits;Owner->GetWorld()->LineTraceMultiByChannel(Hits,G+ChultCanyonFrame(-1700,T,Z),G+ChultCanyonFrame(5200,T,Z),ECC_Visibility,Q);
        for(const auto& Hit:Hits)Failures+=Hit.GetComponent()&&Hit.GetComponent()->ComponentHasTag(TEXT("ChultStartVista"));++Views;
    }
    const int32 Gorge=CheckChultGorge(Owner,Chuck);Failures+=Gorge;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_VISTA_CHECK failures=%d vertices=%d collision_probes=%d reveal_rays=%d land_in_gorge=%d land_over_floor=%d gorge_failures=%d"),
        Failures,Vertices,CollisionProbes,Views,InGorge,OverFloor,Gorge);
    return Failures;
}

void ReviewChultVista(AActor* Owner,AChuckCharacter* Chuck,int32 Failures,bool ContinueTests)
{
    struct FRun{int32 Phase=0;FTimerHandle Timer;TWeakObjectPtr<ACameraActor> Camera;double Last=0;float Worst=0,Sum=0;int32 Frames=0;};auto Run=MakeShared<FRun>();
    auto* Camera=Owner->GetWorld()->SpawnActor<ACameraActor>();Run->Camera=Camera;Camera->GetCameraComponent()->SetFieldOfView(78);
    if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Camera);Chuck->SetActorHiddenInGame(true);
    // From the bridge both ways, each fall, the approach, above, and the far ranges.
    const TArray<FVector> Eyes={ChultCanyonFrame(2250,-300,330),ChultCanyonFrame(2250,300,330),ChultCanyonFrame(1500,-700,300),ChultCanyonFrame(3000,700,300),
        ChultCanyonFrame(-1550,40,640),ChultCanyonFrame(-9000,-15000,9000),ChultCanyonFrame(-30000,-8000,16000)};
    const TArray<FVector> Aims={ChultCanyonFrame(2600,-12000,-2600),ChultCanyonFrame(1900,12000,-2600),ChultGorgeFall(0,1700,0),ChultGorgeFall(1,2200,0),
        ChultCanyonFrame(2600,-5200,-1400),ChultCanyonFrame(2250,6000,-2500),ChultCanyonFrame(-95000,-32000,5000)};
    const TArray<FString> Names={TEXT("GorgeFromBridgeSouth"),TEXT("GorgeFromBridgeNorth"),TEXT("GorgeCreekFall"),TEXT("GorgeBroadFall"),TEXT("GorgeApproach"),TEXT("GorgeOverview"),TEXT("GorgeRanges")};
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,Chuck,Run,Eyes,Aims,Names,Failures,ContinueTests]()
    {
        const int32 I=Run->Phase/2;if(I>=Eyes.Num())
        {
            if(Run->Phase==Eyes.Num()*2)
            {
                ++Run->Phase;UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_VISTA_REVIEW_COMPLETE failures=%d views=%d"),Failures,Eyes.Num());
                Chuck->SetActorHiddenInGame(false);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Chuck);if(Run->Camera.IsValid())Run->Camera->Destroy();
                if(ContinueTests)ReviewChultWatercourse(Owner,Chuck,Failures,true);else FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
            }
            return;
        }
        if(Run->Phase%2==0)Run->Camera->SetActorLocationAndRotation(AChultLanding::FloorOrigin()+Eyes[I],(Aims[I]-Eyes[I]).Rotation());
        else
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult")/(Names[I]+TEXT(".png")),true,false);
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_VISTA_VIEW name=%s frame_ms=%.1f"),*Names[I],FApp::GetDeltaTime()*1000);
        }
        ++Run->Phase;
    },2.5f,true);
}
