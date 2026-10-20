#include "ChultUpriver.h"
#include "ChultCanyon.h"
#include "ChultCanopy.h"
#include "ChultBridge.h"
#include "ChultRiver.h"
#include "ChultLandform.h"
#include "ChultLanding.h"
#include "ChultTemple.h"
#include "ChuckCharacter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"

// Upriver beyond the bridge (Claude, 2026-10-15; docs/CHULT-UPRIVER.md).
// The far bank's floor now runs on from S=28000 to ChultFarFloorEnd (64000):
//   28000-35500  the river's last bends and its head, a spring-fed fall out of a rock face;
//   35500-52000  the giant trees: a taller brachiation grove (ChultCanopy.cpp, grove 1)
//                over open ground, where a T-rex will later keep him in the canopy;
//   52000-64000  dense jungle, the road ending where the temple will stand.
// The old road (ChultPath.cpp) carries on along ChultUpriverRoadT.
namespace
{
    const FVector Along(.8,.6,0),Across(-.6,.8,0);
    float Ease(float U){U=FMath::Clamp(U,0.f,1.f);return U*U*(3-2*U);}
    FVector Frame(FVector P){P-=ChultCanyonFrame(0,0);return FVector(FVector::DotProduct(P,Along),FVector::DotProduct(P,Across),P.Z);}
    constexpr float GiantsFrom=35500,JungleFrom=52000;
    // Giant Trees: 10 m short of the T-rex's stretch (S>=35600, ChultTRex.h), so a return there is safe.
    constexpr float GiantsSpawnS=34600,JungleSpawnS=52900,TempleSpawnS=65200;
    // The upriver jungle's own scatter ends where the temple's plaza begins.
    constexpr float JungleTo=64000;
    // The source fall: off a bedrock shelf on the valley's +T side, into the river's head pool.
    constexpr float FallS=ChultRiverHead,LipT=6190,FootT=5900,FallWidth=520;
    float FallTop(){return ChultRiverWaterZ(ChultRiverHead)+1370;}
    FVector FallPoint(float U,float Offset=0)
    {
        // Free fall: out from the lip as the square root of the drop.
        const float W=ChultRiverWaterZ(ChultRiverHead);
        return ChultCanyonFrame(FallS+Offset,LipT-(LipT-FootT)*FMath::Sqrt(U),FMath::Lerp(FallTop(),W,U));
    }
    FVector RoadPoint(float S){const FVector P=ChultCanyonFrame(S,ChultUpriverRoadT(S));return FVector(P.X,P.Y,ChultCanyonGroundZ(P));}
    FVector SpawnLocal(EChultUpriverSpawn Spawn)
    {
        if(Spawn==EChultUpriverSpawn::GiantCanopy)return ChultGiantTrees()[0].Fork+FVector(0,0,40);
        return RoadPoint(Spawn==EChultUpriverSpawn::Giants?GiantsSpawnS:JungleSpawnS)+FVector(0,0,34.65f);
    }
    bool InRocks(FVector P,float Margin)
    {
        const FVector F=Frame(P);return F.X>FallS-1500-Margin&&F.X<FallS+1500+Margin&&F.Y>4300-Margin;
    }
}

// The giants' line: trunk I stands at S=36600+1300*I, either side of this.
float ChultGiantLineT(float S){return 900*FMath::Sin(.45f*(S-36600)/1300+.3f);}
namespace{float GiantLineT(float S){return ChultGiantLineT(S);}}
float ChultUpriverRoadT(float S)
{
    // From the riverbank the road ran beside (ChultPath.cpp, to S=27600), it
    // eases away from the river's swing toward its head and wanders up the
    // valley; among the giants it keeps to the -T side of their line, 12.5 m
    // off it (clear of every buttress and root), under their outer boughs.
    const float Bank=ChultRiverCenter(FMath::Min(S,28000.f))-ChultRiverWidth(FMath::Min(S,28000.f))-650;
    const float Wander=-650+850*FMath::Sin((S-27600)/5200.f-.9f)+180*FMath::Sin((S-27600)/1700.f);
    const float Beside=GiantLineT(S)-1250+150*FMath::Sin(S/1700.f);
    const float Grove=Ease((S-34000)/1500)*(1-Ease((S-51800)/1500));
    // Then onto the temple's avenue, straight up its middle (2026-10-15).
    return FMath::Lerp(FMath::Lerp(Bank,FMath::Lerp(Wander,Beside,Grove),Ease((S-27600)/2600)),0.f,Ease((S-62000)/2500));
}

const TArray<FVector2D>& ChultGiantTrunkSpots()
{
    static const TArray<FVector2D> Spots=[]()
    {
        // Twelve giants, 13.5-17 m apart, staggered either side of a gently winding line up the valley.
        TArray<FVector2D> Out;
        for(int32 I=0;I<12;++I)
        {
            const float S=36600+1300*I;const FVector P=ChultCanyonFrame(S,GiantLineT(S)+(I%2?380.f:-380.f));
            Out.Add(FVector2D(P.X,P.Y));
        }
        return Out;
    }();
    return Spots;
}
const TArray<FVector2D>& ChultScreenTrunkSpots()
{
    static const TArray<FVector2D> Spots=[]()
    {
        // Two staggered rows across the valley at the deep jungle's start, clear
        // of the road (11 m), the deep jungle's spawn point and the valley's sides.
        TArray<FVector2D> Out;
        const FVector Spawn=SpawnLocal(EChultUpriverSpawn::DeepJungle);
        for(int32 Row=0;Row<3;++Row)
        {
            const float S=Row==2?58700:Row?56300:53900;
            for(float T=-5700+(Row%2)*1250;T<=5800;T+=2500)
            {
                // One near the road moves to its edge (11.5 m off) rather than leave a gap.
                const float Road=ChultUpriverRoadT(S);
                const float At=FMath::Abs(T-Road)<1100?Road+(T>Road?1150.f:-1150.f):T;
                const FVector P=ChultCanyonFrame(S+(FMath::Frac(At*.00037f)-.5f)*500,At);
                if(FVector::Dist2D(P,Spawn)<1200)continue;
                Out.Add(FVector2D(P.X,P.Y));
            }
        }
        return Out;
    }();
    return Spots;
}
bool InChultGiantReserve(const FVector& P,float Margin)
{
    const auto& Spots=ChultGiantTrunkSpots();
    for(int32 I=1;I<Spots.Num();++I)
    {
        const FVector A(Spots[I-1],0),B(Spots[I],0);
        if(FVector::Dist2D(P,FMath::ClosestPointOnSegment(FVector(P.X,P.Y,0),A,B))<1000+Margin)return true;
    }
    return false;
}
float ChultUpriverGroundZ(const FVector& P,float Base)
{
    const FVector F=Frame(P);if(F.X<GiantsFrom-2000||F.X>JungleFrom+1000)return Base;
    // Level under each giant (520 cm round its axis), blended back over 2.8 m:
    // the trunk, buttresses and roots are built for flat ground at its foot.
    float W=0,Z=0;
    for(const FVector2D& Spot:ChultGiantTrunkSpots())
    {
        const float D=FVector2D::Distance(FVector2D(P.X,P.Y),Spot);if(D>800)continue;
        const float Wi=1-Ease((D-520)/280);W+=Wi;Z+=Wi*ChultCanyonBaseZ(FVector(Spot,0));
    }
    if(W<=0)return Base;
    return W>=1?Z/W:Base*(1-W)+Z;
}

FVector ChultUpriverSpawn(EChultUpriverSpawn Spawn)
{
    if(Spawn==EChultUpriverSpawn::FarBank)return ChultBridgeSpawn(true);
    if(Spawn==EChultUpriverSpawn::Temple)return ChultTempleSpawn();
    return AChultLanding::FloorOrigin()+SpawnLocal(Spawn);
}
float ChultUpriverSpawnYaw(EChultUpriverSpawn Spawn)
{
    if(Spawn==EChultUpriverSpawn::FarBank)return ChultBridgeRespawnYaw();
    if(Spawn==EChultUpriverSpawn::Temple)return ChultTempleSpawnYaw();
    if(Spawn==EChultUpriverSpawn::GiantCanopy){const auto& T=ChultGiantTrees()[0];return T.Limbs[0].Rotation().Yaw;}
    const float S=Spawn==EChultUpriverSpawn::Giants?GiantsSpawnS:JungleSpawnS;
    return (RoadPoint(S+300)-RoadPoint(S)).GetSafeNormal2D().Rotation().Yaw;
}
bool ChultUpriverRespawn(const FVector& DeathWorld,FVector& OutWorld)
{
    // On the far bank, back at the spawn point of the stretch he died in (before
    // these, any death there returned him to the landing across the gorge).
    const FVector Local=DeathWorld-AChultLanding::FloorOrigin();
    if(!InChultRiverExtension(Local))return false;
    const float S=Frame(Local).X;if(S<4560)return false;
    OutWorld=ChultUpriverSpawn(S<GiantsSpawnS-600?EChultUpriverSpawn::FarBank:S<JungleSpawnS-600?EChultUpriverSpawn::Giants:
        S<TempleSpawnS-800?EChultUpriverSpawn::DeepJungle:EChultUpriverSpawn::Temple);
    return true;
}
bool IsChultUpriverSpawn(const FVector& World,float& OutYaw)
{
    for(EChultUpriverSpawn Spawn:{EChultUpriverSpawn::Giants,EChultUpriverSpawn::DeepJungle,EChultUpriverSpawn::Temple})
        if(FVector::Dist(World,ChultUpriverSpawn(Spawn))<1){OutYaw=ChultUpriverSpawnYaw(Spawn);return true;}
    return false;
}

void BuildChultUpriver(AActor* Owner)
{
    const double Started=FPlatformTime::Seconds();const FVector G=AChultLanding::FloorOrigin();
    int32 Missing=0,Triangles=0,Chunks=0;
    // The floor, in 20 m chunks so the far ones cull. Its first row is the
    // river valley's last (S=28000): both use ChultCanyonGroundZ exactly.
    for(float S0=28000;S0<ChultFarFloorEnd;S0+=2000)
    {
        FChultLandform Ground;
        // From the temple precinct on, wider (2026-10-16).
        const float Wide=S0>=ChultTemplePrecinctFrom?ChultTemplePrecinctHalf:6500;
        for(float S=S0;S<S0+2000;S+=100)for(float T=-Wide;T<Wide;T+=100)
        {
            FVector A=ChultCanyonFrame(S,T),B=ChultCanyonFrame(S+100,T),C=ChultCanyonFrame(S+100,T+100),D=ChultCanyonFrame(S,T+100);
            for(FVector* P:{&A,&B,&C,&D})P->Z=ChultCanyonGroundZ(*P);Ground.Quad(A,B,C,D);
        }
        for(int32 I=0;I<Ground.V.Num();++I)Ground.UV[I]=FVector2D(Ground.V[I].X/260,Ground.V[I].Y/260);
        auto* Mesh=Ground.Finish(Owner,*FString::Printf(TEXT("ChultUpriverFloor%d"),Chunks),TEXT("/Game/Art/Cutscenes/FallToChult/M_FallGround.M_FallGround"));
        Mesh->ComponentTags.Add(TEXT("ChultUpriverGround"));Missing+=!Mesh->GetMaterial(0);Triangles+=Ground.T.Num()/3;++Chunks;
    }

    // The river's source: water bursting from a crevice in the valley side,
    // over a bedrock shelf and down 13.7 m into its head pool.
    FChultLandform Rock,Water;const float WZ=ChultRiverWaterZ(ChultRiverHead),Yaw0=FMath::RadiansToDegrees(FMath::Atan2(Along.Y,Along.X));
    auto At=[&](float S,float T,float Z){return ChultCanyonFrame(S,T,Z);};
    Rock.Rock(At(FallS,7500,WZ+700),FVector(1600,1000,1300),Yaw0,3);                      // the face behind
    for(float Side:{-1.f,1.f})Rock.Rock(At(FallS+Side*820,6650,WZ+700),FVector(420,620,900),Yaw0+Side*8,5+Side); // the notch's flanks
    Rock.Rock(At(FallS,6450,WZ+350),FVector(520,300,650),Yaw0,8);                          // under the lip
    for(float Side:{-1.f,1.f})Rock.Rock(At(FallS+Side*520,5950,WZ-40),FVector(220,260,160),Yaw0+Side*30,11+Side); // pool boulders
    Rock.Rock(At(FallS-1100,6900,WZ+300),FVector(700,600,700),Yaw0-20,14);                 // the spur downstream
    Rock.Rock(At(FallS+1250,7000,WZ+450),FVector(650,700,900),Yaw0+25,17);                 // and upstream
    {
        // The shelf: flat-topped bedrock the water runs across to the lip.
        const float Top=FallTop()-2,Bottom=WZ+950,T0=LipT,T1=6760,S0=FallS-330,S1=FallS+330;
        const FVector C[8]={At(S0,T0,Bottom),At(S1,T0,Bottom),At(S1,T1,Bottom),At(S0,T1,Bottom),At(S0,T0,Top),At(S1,T0,Top),At(S1,T1,Top),At(S0,T1,Top)};
        Rock.Quad(C[4],C[5],C[6],C[7]);Rock.Quad(C[0],C[3],C[2],C[1]);
        Rock.Quad(C[0],C[1],C[5],C[4]);Rock.Quad(C[1],C[2],C[6],C[5]);Rock.Quad(C[2],C[3],C[7],C[6]);Rock.Quad(C[3],C[0],C[4],C[7]);
    }
    // Mossy outcrops among the giants and in the deep jungle, off the road and the grove.
    FRandomStream R(2026101561);int32 Outcrops=0;
    for(int32 I=0;I<46;++I)
    {
        const float S=R.FRandRange(GiantsFrom,JungleTo-300),T=R.FRandRange(-6000,6000);const FVector P=ChultCanyonFrame(S,T);
        if(FMath::Abs(T-ChultUpriverRoadT(S))<700||InChultGiantReserve(P,700))continue;
        const float Size=R.FRandRange(120,320);
        Rock.Rock(P+FVector(0,0,ChultCanyonGroundZ(P)+Size*.25f),FVector(Size*R.FRandRange(1,1.8f),Size*R.FRandRange(.8f,1.4f),Size*R.FRandRange(.5f,.9f)),R.FRandRange(0,360),I*1.3f);++Outcrops;
    }
    auto* Stone=Rock.Finish(Owner,TEXT("ChultUpriverRock"),TEXT("/Game/Art/ChultTerrain/M_ChultCliff.M_ChultCliff"));
    Stone->ComponentTags.Add(TEXT("ChultUpriverRock"));Missing+=!Stone->GetMaterial(0);Triangles+=Rock.T.Num()/3;
    auto Ribbon=[&](TFunction<FVector(float)> Path,int32 Steps,float Width)
    {
        // As the watercourse's falls: four strips across, edges faded, V down the
        // flow. Side runs -S so the faces look away from the rock and up.
        const FVector Side=-Along;const float Length=FVector::Dist(Path(0),Path(1));
        for(int32 I=0;I<Steps;++I)
        {
            const float U=I/static_cast<float>(Steps),V=(I+1)/static_cast<float>(Steps);
            const FVector A=Path(U),B=Path(V),Normal=FVector::CrossProduct(Side,B-A).GetSafeNormal();
            for(int32 K=0;K<4;++K)
            {
                const float L=K/4.f,H=(K+1)/4.f;
                const FVector P[]={A+Side*(L-.5f)*Width,A+Side*(H-.5f)*Width,B+Side*(H-.5f)*Width,B+Side*(L-.5f)*Width};
                const int32 Start=Water.V.Num();
                for(int32 J=0;J<4;++J)
                {
                    const float X=J==0||J==3?L:H,Y=J<2?U:V;
                    Water.V.Add(P[J]);Water.N.Add(Normal);Water.UV.Add(FVector2D(X,Y*Length/160));
                    Water.C.Add(FColor(255,255,255,X==0||X==1?0:220));Water.Tangents.Add(FProcMeshTangent(Side,false));
                }
                Water.T.Append({Start,Start+2,Start+1,Start,Start+3,Start+2});
            }
        }
    };
    Ribbon([&](float U){return At(FallS,FMath::Lerp(6740.f,LipT,U),FallTop()+3);},6,FallWidth-40);
    Ribbon([&](float U){return FallPoint(U);},24,FallWidth);
    Ribbon([&](float U){return FallPoint(U,90)+ChultCanyonFrame(0,-25,0)-ChultCanyonFrame(0,0,0);},24,FallWidth*.55f);
    auto* Falls=Water.Finish(Owner,TEXT("ChultSourceFalls"),TEXT("/Game/Art/ChultTerrain/M_ChultWaterfall.M_ChultWaterfall"),false);
    Falls->SetCastShadow(false);Missing+=!Falls->GetMaterial(0);Triangles+=Water.T.Num()/3;

    // The giant trees (ChultCanopy.cpp, grove 1).
    BuildChultGiantCanopy(Owner);

    // Vegetation: the riverside jungle carries on to the head; open understory
    // under the giants with taller trees only out at the valley sides; then
    // dense jungle. The road, spawn points, grove and pool stay clear.
    TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;int32 Trees=0,Palms=0,Plants=0;
    auto Place=[&](FString Name,FVector P,float Scale,float Yaw,bool Solid=false,bool Hidden=false)
    {
        auto*& H=Sets.FindOrAdd(Name);if(!H)
        {
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,*FString::Printf(TEXT("ChultUpriver_%s"),*Name));
            const TCHAR* Dir=Name.StartsWith(TEXT("SM_Chult"))?TEXT("/Game/Art/ChultNature"):TEXT("/Game/Art/Cutscenes/FallToChult");
            H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),Dir,*Name,*Name)));Missing+=!H->GetStaticMesh();
            H->SetupAttachment(Owner->GetRootComponent());H->SetWorldLocation(G);H->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
            if(Solid)H->SetCollisionProfileName(TEXT("BlockAll"));H->SetHiddenInGame(Hidden);H->SetCastShadow(!Hidden);H->SetCullDistances(Solid?10000:5500,Solid?18000:10000);H->RegisterComponent();
        }
        H->AddInstance(FTransform(FRotator(0,Yaw,0),G+P,FVector(Scale)),true);
    };
    const FVector Spawns[]={SpawnLocal(EChultUpriverSpawn::Giants),SpawnLocal(EChultUpriverSpawn::DeepJungle)};
    const auto& Giants=ChultGiantTrees();
    for(int32 I=0;I<30000;++I)
    {
        const float S=R.FRandRange(28000,JungleTo-100),T=R.FRandRange(-6400,6400);FVector P=ChultCanyonFrame(S,T);
        const float River=ChultRiverDistance(P),W=ChultRiverWidth(S),Road=FMath::Abs(T-ChultUpriverRoadT(S));
        if(River<W+160||Road<150||InRocks(P,0))continue;
        bool Clear=true;for(const FVector& Spawn:Spawns)Clear&=FVector::Dist2D(P,Spawn)>450;
        for(const auto& Giant:Giants)Clear&=FVector::Dist2D(P,Giant.Trunk)>650&&(!Giant.bLiana||FVector::Dist2D(P,Giant.LianaFoot)>300);
        if(!Clear)continue;
        P.Z=ChultCanyonGroundZ(P)-3;const float Yaw=R.FRandRange(0,360);
        const bool Grove=S>=GiantsFrom&&S<JungleFrom,Deep=S>=JungleFrom;
        // Solid trees keep the road lane, the river's banks, the pool's rocks and the grove's air.
        const bool TreeHere=Road>320&&River>W+1100&&!InRocks(P,500)&&!InChultGiantReserve(P,900)&&(Deep?I%5==0:Grove?I%9==0:I%16==0);
        if(TreeHere)
        {
            const float Scale=Deep?R.FRandRange(2.f,3.1f):R.FRandRange(1.7f,2.8f);
            Place(TEXT("SM_ChultIslandTree"),P,Scale,Yaw);Place(TEXT("SM_ChultIslandTrunkCollision"),P,Scale,Yaw,true,true);++Trees;
            if(I%3==0){Place(TEXT("SM_FallPalm"),P+FVector(150,40,0),R.FRandRange(1.3f,1.9f),Yaw);++Palms;}
            continue;
        }
        // Understory: thinner on the giants' open floor, thick in the deep jungle.
        if(Grove&&R.FRand()<.55f)continue;
        if(!Deep&&!Grove&&R.FRand()<.35f)continue;
        const int32 Type=I%7;
        Place(Type<4?FString::Printf(TEXT("SM_ChultFern%d"),Type):Type<6?TEXT("SM_FallElephantEar"):TEXT("SM_FallShrub"),P,R.FRandRange(1.5,Deep?3.f:2.6f)*(Type<4?1:.65f),Yaw);++Plants;
    }
    // The deep jungle round the temple's plaza and behind the pyramid (2026-10-15),
    // from its own stream so the earlier jungle keeps every draw.
    {
        FRandomStream Up(2026101592);
        for(int32 I=0;I<24000;++I)
        {
            const float S=Up.FRandRange(JungleTo,ChultFarFloorEnd-100),T=Up.FRandRange(-6400,6400);FVector P=ChultCanyonFrame(S,T);
            if(InChultTempleReserve(P,0)||(FMath::Abs(T-ChultUpriverRoadT(S))<150&&S<JungleTo+800))continue;
            P.Z=ChultCanyonGroundZ(P)-3;const float Yaw=Up.FRandRange(0,360);
            if(I%5==0&&!InChultTempleReserve(P,700))
            {
                const float Scale=Up.FRandRange(2.f,3.1f);
                Place(TEXT("SM_ChultIslandTree"),P,Scale,Yaw);Place(TEXT("SM_ChultIslandTrunkCollision"),P,Scale,Yaw,true,true);++Trees;
                if(I%15==0){Place(TEXT("SM_FallPalm"),P+FVector(150,40,0),Up.FRandRange(1.3f,1.9f),Yaw);++Palms;}
                continue;
            }
            const int32 Type=I%7;
            Place(Type<4?FString::Printf(TEXT("SM_ChultFern%d"),Type):Type<6?TEXT("SM_FallElephantEar"):TEXT("SM_FallShrub"),P,Up.FRandRange(1.5f,3.f)*(Type<4?1:.65f),Yaw);++Plants;
        }
    }
    // The screen of giants (ChultCanopy.cpp, grove 2): the jungle's trees and
    // plants are taken out from round its trunks after planting, so every
    // earlier random draw is kept.
    BuildChultScreenCanopy(Owner);
    // Solid crown masses (SM_FallCrown, its own materials) inside each screen
    // tree's crown, through the band the temple showed through: leaf cards alone
    // let it show between them.
    int32 Masses=0;
    {
        auto* Mass=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,TEXT("ChultScreenCrownMass"));
        Mass->SetupAttachment(Owner->GetRootComponent());Mass->SetWorldLocation(G);
        Mass->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Cutscenes/FallToChult/SM_FallCrown.SM_FallCrown")));Missing+=!Mass->GetStaticMesh();
        Mass->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mass->RegisterComponent();
        const auto& Screen=ChultScreenTrees();
        for(int32 I=0;I<Screen.Num();++I)
        {
            const auto& Tree=Screen[I];FRandomStream Rng(5150+I*37);
            for(int32 K=0;K<5;++K)
            {
                const float Yaw=(K*2*PI/5)+I;const float Out=K==4?0:Rng.FRandRange(350,550);
                const FVector Pos=Tree.Trunk+FVector(FMath::Cos(Yaw)*Out,FMath::Sin(Yaw)*Out,Tree.Top+(K==4?900:Rng.FRandRange(-250,450)));
                Mass->AddInstance(FTransform(FRotator(0,Rng.FRandRange(0,360),0),G+Pos,FVector(Rng.FRandRange(.85f,1.1f))),true);++Masses;
            }
        }
    }
    int32 Cleared=0;
    for(auto& Set:Sets)
        for(int32 I=Set.Value->GetInstanceCount()-1;I>=0;--I)
        {
            FTransform X;Set.Value->GetInstanceTransform(I,X,true);
            for(const FVector2D& Spot:ChultScreenTrunkSpots())
                if(FVector2D::Distance(FVector2D(X.GetLocation()-G),Spot)<(Set.Key.StartsWith(TEXT("SM_ChultIsland"))||Set.Key==TEXT("SM_FallPalm")?900.f:420.f))
                {Set.Value->RemoveInstance(I);++Cleared;break;}
        }
    FString Where;for(const FVector2D& Spot:ChultScreenTrunkSpots()){const FVector D=FVector(Spot,0)-ChultCanyonFrame(0,0);Where+=FString::Printf(TEXT(" %.0f/%.0f"),FVector::DotProduct(D,(ChultCanyonFrame(1,0)-ChultCanyonFrame(0,0)).GetSafeNormal()),FVector::DotProduct(D,(ChultCanyonFrame(0,1)-ChultCanyonFrame(0,0)).GetSafeNormal()));}
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_SCREEN_CLEARING trees=%d crown_masses=%d removed_instances=%d road_t=%.0f/%.0f at(s/t)=%s"),ChultScreenTrunkSpots().Num(),Masses,Cleared,ChultUpriverRoadT(53900),ChultUpriverRoadT(56300),*Where);
    // The far canopy (user 2026-10-19: "the distant jungle depth to be visible while
    // standing on top of the pyramid stairs and looking back ... without making the
    // game run too heavy"). The jungle's scanned trees (24k triangles, no LODs) stop
    // drawing at 100 m; one light crown (SM_FallCrown, 1.1k triangles) stands over
    // each 14 m patch of far-bank trees in their place. Their materials dither them
    // in from 75 to 100 m from the camera (Tools/create_far_canopy_materials.py), so
    // near him there are none. -ChuckNoFarCanopy leaves them out (for comparison).
    int32 FarCrowns=0;
    if(!FParse::Param(FCommandLine::Get(),TEXT("ChuckNoFarCanopy")))
    {
        struct FCell{FVector Sum=FVector::ZeroVector;float Top=0;int32 N=0;};
        TMap<FIntPoint,FCell> Cells;
        const FVector AlongS=(ChultCanyonFrame(1,0)-ChultCanyonFrame(0,0)).GetSafeNormal();
        TArray<UHierarchicalInstancedStaticMeshComponent*> All;Owner->GetComponents(All);
        for(auto* H:All)
        {
            if(!H->GetStaticMesh()||H->GetStaticMesh()->GetName()!=TEXT("SM_ChultIslandTree")||H->bHiddenInGame||H->InstanceEndCullDistance>20000)continue;
            const float Height=H->GetStaticMesh()->GetBoundingBox().Max.Z;
            for(int32 I=0;I<H->GetInstanceCount();++I)
            {
                FTransform X;H->GetInstanceTransform(I,X,true);const FVector L=X.GetLocation()-G;
                if(FVector::DotProduct(L-ChultCanyonFrame(0,0),AlongS)<4300)continue;   // the far bank only
                FCell& C=Cells.FindOrAdd(FIntPoint(FMath::FloorToInt(L.X/1400),FMath::FloorToInt(L.Y/1400)));
                C.Sum+=L;C.Top+=L.Z+Height*X.GetScale3D().Z;++C.N;
            }
        }
        auto* Far=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,TEXT("ChultFarCanopy"));
        Far->SetupAttachment(Owner->GetRootComponent());Far->SetWorldLocation(G);
        Far->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Cutscenes/FallToChult/SM_FallCrown.SM_FallCrown")));Missing+=!Far->GetStaticMesh();
        Far->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/ChultNature/M_ChultFarCrownLeaves.M_ChultFarCrownLeaves")));
        Far->SetMaterial(1,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/ChultNature/M_ChultFarCanopy.M_ChultFarCanopy")));
        Missing+=!Far->GetMaterial(0)||!Far->GetMaterial(1)||!Far->GetMaterial(0)->GetName().Contains(TEXT("Far"));
        Far->SetCollisionEnabled(ECollisionEnabled::NoCollision);Far->SetCastShadow(false);Far->SetCullDistances(0,150000);Far->bReceivesDecals=false;
        Far->RegisterComponent();
        for(const auto& Cell:Cells)
        {
            const FCell& C=Cell.Value;const FVector Mid=C.Sum/C.N;const float Top=C.Top/C.N;
            // A tree top's size (6-10 m across, 3.6 m deep), its top just over the
            // trees' tops: from afar a crown on an unseen trunk, not a mound.
            const float Wide=.55f+.08f*FMath::Min(C.N,4)+.06f*FMath::Frac(Mid.X*.0013f+Mid.Y*.0007f),Deep=.5f;
            const float CrownTop=Far->GetStaticMesh()?Far->GetStaticMesh()->GetBoundingBox().Max.Z:700;
            Far->AddInstance(FTransform(FRotator(0,FMath::Frac(Mid.X*.00091f)*360,0),G+FVector(Mid.X,Mid.Y,Top+50-CrownTop*Deep),FVector(Wide,Wide,Deep)),true);++FarCrowns;
        }
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_FAR_CANOPY crowns=%d fade_from=7500 fade_to=10000"),FarCrowns);
    // The temple (ChultTemple.cpp).
    BuildChultTemple(Owner);
    // Spray at the pool, and the deep jungle's low mist.
    TArray<TPair<FVector,float>> Mists={{FallPoint(.95f),1.2f},{ChultCanyonFrame(55000,-3000,1500),9},{ChultCanyonFrame(60000,2500,1600),9},{ChultCanyonFrame(44000,4500,1200),7}};
    for(const auto& Mist:Mists)
    {
        auto* Fog=NewObject<ULocalFogVolumeComponent>(Owner);Fog->SetupAttachment(Owner->GetRootComponent());Fog->SetWorldLocation(G+Mist.Key);Fog->SetWorldScale3D(FVector(Mist.Value));
        const bool Spray=Mist.Value<2;
        Fog->RadialFogExtinction=Spray?.4f:.06f;Fog->HeightFogExtinction=Spray?.18f:.05f;Fog->HeightFogFalloff=Spray?1500:900;Fog->HeightFogOffset=Spray?.6f:.3f;
        Fog->FogAlbedo=FLinearColor(.62,.74,.7);Fog->RegisterComponent();
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_UPRIVER floor_m=%.0f floor_chunks=%d giants=%d trees=%d palms=%d plants=%d outcrops=%d source_fall_cm=%.0f missing_assets=%d water_collision=0 triangles=%d build_ms=%.1f"),
        (ChultFarFloorEnd-28000)/100,Chunks,Giants.Num(),Trees,Palms,Plants,Outcrops,FallTop()-WZ,Missing,Triangles,(FPlatformTime::Seconds()-Started)*1000);
}

int32 CheckChultUpriver(AActor* Owner,AChuckCharacter* Chuck)
{
    int32 Failures=0,Floors=0,Occluded=0,Levels=0,FallProbes=0;auto* World=Owner->GetWorld();const FVector G=AChultLanding::FloorOrigin();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultUpriver),false,Chuck);
    const float Half=Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius();
    auto Fail=[&](const TCHAR* What,const FVector& At){++Failures;if(Failures<20)UE_LOG(LogTemp,Warning,TEXT("CHUCK_UPRIVER_FAIL %s at=%s"),What,*At.ToString());};
    // The floor is where the ground function says, with no hole at the old seam.
    for(float S=27900;S<ChultFarFloorEnd;S+=S<28200?100:400)for(float T=-6300;T<=6300;T+=1050)
    {
        const FVector P=ChultCanyonFrame(S,T);if(ChultRiverDistance(P)<ChultRiverWidth(S)+200||InRocks(P,200))continue;
        const float Z=ChultCanyonGroundZ(P);FHitResult Hit;++Floors;
        if(!World->LineTraceSingleByChannel(Hit,G+P+FVector(0,0,Z+900),G+P+FVector(0,0,Z-400),ECC_Visibility,Q)){Fail(TEXT("floor_hole"),P);continue;}
        const bool Ground=Hit.GetComponent()&&(Hit.GetComponent()->ComponentHasTag(TEXT("ChultUpriverGround"))||Hit.GetComponent()->GetName().StartsWith(TEXT("ChultRiverValley")));
        if(!Ground){++Occluded;if(Occluded<4)UE_LOG(LogTemp,Display,TEXT("CHUCK_UPRIVER_FLOOR_OCCLUDED at=%s by=%s"),*P.ToString(),*GetNameSafe(Hit.GetComponent()));continue;}
        if(FMath::Abs(Hit.ImpactPoint.Z-G.Z-Z)>10)Fail(TEXT("floor_height"),P);
    }
    // Level ground round each giant: its buttresses and roots meet the soil.
    const auto& Giants=ChultGiantTrees();
    for(const auto& Tree:Giants)
    {
        Failures+=Tree.Top<1500;
        for(int32 K=0;K<12;++K)for(float D:{300.f,500.f})
        {
            const FVector P=Tree.Trunk+FVector(FMath::Cos(K*PI/6),FMath::Sin(K*PI/6),0)*D;++Levels;
            if(FMath::Abs(ChultCanyonGroundZ(P)-Tree.Trunk.Z)>1)Fail(TEXT("giant_ground_not_level"),P);
        }
    }
    // Ground spawn points: on the floor, room to stand, and their own death recovery.
    for(EChultUpriverSpawn Spawn:{EChultUpriverSpawn::Giants,EChultUpriverSpawn::DeepJungle})
    {
        const FVector P=ChultUpriverSpawn(Spawn);FHitResult Hit;FVector Back;float Yaw=0;
        const bool Floor=World->LineTraceSingleByChannel(Hit,P,P-FVector(0,0,Half+40),ECC_Visibility,Q);
        if(!Floor||FMath::Abs(Hit.ImpactPoint.Z-(P.Z-34.65f))>6||Hit.ImpactNormal.Z<.8f)Fail(TEXT("spawn_floor"),P-G);
        if(World->OverlapBlockingTestByChannel(P+FVector(0,0,2),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q))Fail(TEXT("spawn_blocked"),P-G);
        if(!AChultLanding::Contains(P)||!ChultUpriverRespawn(P,Back)||FVector::Dist(Back,P)>1||!IsChultUpriverSpawn(P,Yaw))Fail(TEXT("spawn_recovery"),P-G);
    }
    // Deaths: the far bank by the bridge, the giants and the deep jungle; the gorge stays the bridge's.
    {
        FVector Out;int32 Map=0;
        Map+=ChultUpriverRespawn(G+ChultCanyonFrame(12000,-3000,300),Out)&&FVector::Dist(Out,ChultBridgeSpawn(true))<1;
        Map+=ChultUpriverRespawn(G+ChultCanyonFrame(44000,0,1200),Out)&&FVector::Dist(Out,ChultUpriverSpawn(EChultUpriverSpawn::Giants))<1;
        Map+=ChultUpriverRespawn(G+ChultCanyonFrame(61000,2000,1400),Out)&&FVector::Dist(Out,ChultUpriverSpawn(EChultUpriverSpawn::DeepJungle))<1;
        Map+=!ChultUpriverRespawn(G+ChultCanyonFrame(2250,0,-900),Out);
        if(Map!=4)Fail(TEXT("respawn_map"),FVector(Map));
    }
    // The source fall stays in front of its rock all the way down, never inside it.
    for(float U:{.05f,.2f,.4f,.6f,.8f,.95f})for(float Offset:{-200.f,0.f,200.f})
    {
        const FVector P=G+FallPoint(U,Offset),Out=(ChultCanyonFrame(0,1,0)-ChultCanyonFrame(0,0,0));FHitResult Hit;++FallProbes;
        const bool Behind=World->LineTraceSingleByChannel(Hit,P,P+Out*3000,ECC_Visibility,Q)&&Hit.GetComponent()&&Hit.GetComponent()->ComponentHasTag(TEXT("ChultUpriverRock"));
        const float Gap=Behind?FVector::Dist(Hit.ImpactPoint,P):0;
        FHitResult Front;const bool Inside=World->LineTraceSingleByChannel(Front,P-Out*600,P,ECC_Visibility,Q)&&Front.GetComponent()&&Front.GetComponent()->ComponentHasTag(TEXT("ChultUpriverRock"));
        if(!Behind||Gap<5||Inside){++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_UPRIVER_FALL_FAIL u=%.2f offset=%.0f behind=%d gap=%.1f inside=%d"),U,Offset,Behind,Gap,Inside);}
    }
    const int32 Grove=CheckChultGiantCanopy(Owner,Chuck);Failures+=Grove;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_UPRIVER_CHECK failures=%d floors=%d occluded=%d level_probes=%d giants=%d spawns=2 fall_probes=%d respawn_map=4 giant_failures=%d"),
        Failures,Floors,Occluded,Levels,Giants.Num(),FallProbes,Grove);
    return Failures;
}

void ReviewChultUpriver(AActor* Owner,AChuckCharacter* Chuck,int32 Failures)
{
    auto* Camera=Owner->GetWorld()->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(72);
    Chuck->SetActorHiddenInGame(true);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Camera);
    auto Ground=[](float S,float T,float Up){const FVector P=ChultCanyonFrame(S,T);return P+FVector(0,0,ChultCanyonGroundZ(P)+Up);};
    const auto& Giants=ChultGiantTrees();
    const FVector Side=(ChultCanyonFrame(0,1,0)-ChultCanyonFrame(0,0,0));
    // Along the open river channel for the two river views: its banks are wooded.
    auto OverRiver=[](float S,float Up){return ChultCanyonFrame(S,ChultRiverCenter(S),ChultRiverWaterZ(S)+Up);};
    const TArray<FVector> Eyes={OverRiver(21500,700),OverRiver(31300,260),Ground(35300,ChultUpriverRoadT(35300),170),
        Giants[4].Fork+Side*1500+FVector(0,0,250),Ground(52700,ChultUpriverRoadT(52700),170),Ground(46000,-5600,3200),
        // 2026-10-19: from the end of the T-rex's stretch toward the temple (the screen of giants hides it).
        Giants.Last().Fork+FVector(0,0,170),Ground(50400,ChultUpriverRoadT(50400),170),Giants[9].Fork+FVector(0,0,170),
        // 2026-10-19: from the head of a pyramid's stair and the central stair, looking back down the valley.
        ChultTemplePyramidTop(false)-(ChultCanyonFrame(1300,0,0)-ChultCanyonFrame(0,0,0))+FVector(0,0,-250),
        ChultTempleSpireTop()-(ChultCanyonFrame(1200,0,0)-ChultCanyonFrame(0,0,0))+FVector(0,0,-(ChultTempleSpireTop().Z-ChultTemplePlazaZ())+1500+170),
        Ground(47500,0,9000)};
    const TArray<FVector> Aims={OverRiver(30500,500),FallPoint(.45f),Ground(41000,0,1500),
        Giants[6].Fork+FVector(0,0,150),Ground(57000,ChultUpriverRoadT(57000),220),ChultCanyonFrame(110000,0,4000),
        ChultTempleSpireTop()-FVector(0,0,900),ChultTempleSpireTop()-FVector(0,0,900),ChultTempleSpireTop()-FVector(0,0,900),
        ChultCanyonFrame(30000,-2500,ChultTemplePlazaZ()-1500),ChultCanyonFrame(30000,0,ChultTemplePlazaZ()-1500),Ground(55500,0,0)};
    const TArray<FString> Names={TEXT("UpriverFromFarBank"),TEXT("UpriverSourceFall"),TEXT("UpriverGiantsFromRoad"),TEXT("UpriverGiantCanopy"),TEXT("UpriverDeepJungle"),TEXT("UpriverValleyEnd"),
        TEXT("UpriverTRexEndCrown"),TEXT("UpriverTRexEndRoad"),TEXT("UpriverTRexEndGiant9"),TEXT("UpriverFromPyramidTop"),TEXT("UpriverFromCentralStair"),TEXT("UpriverScreenFromAbove")};
    // Frame time unthrottled (no vsync or cap), averaged over each view's hold.
    if(auto* PC=Cast<APlayerController>(Chuck->GetController())){PC->ConsoleCommand(TEXT("r.VSync 0"));PC->ConsoleCommand(TEXT("t.MaxFPS 0"));}
    struct FRun{int32 Phase=0;uint64 Frames=0;double At=0;FTimerHandle Timer;};auto Run=MakeShared<FRun>();
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,Chuck,Camera,Run,Eyes,Aims,Names,Failures]()
    {
        const int32 I=Run->Phase/2;
        if(I>=Eyes.Num())
        {
            const int32 Status=Failures?1:0;FTimerHandle Done=Run->Timer;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_UPRIVER_REVIEW_COMPLETE failures=%d views=%d"),Failures,Eyes.Num());
            Camera->Destroy();
            // Clearing the active timer frees this lambda's captures: copy first.
            Owner->GetWorldTimerManager().ClearTimer(Done);
            FPlatformMisc::RequestExitWithStatus(false,Status);return;
        }
        if(Run->Phase%2==0)
        {
            Camera->SetActorLocationAndRotation(AChultLanding::FloorOrigin()+Eyes[I],(Aims[I]-Eyes[I]).Rotation());
            Run->Frames=GFrameCounter;Run->At=FPlatformTime::Seconds();
        }
        else
        {
            const double Held=FPlatformTime::Seconds()-Run->At;const uint64 Frames=FMath::Max<uint64>(GFrameCounter-Run->Frames,1);
            FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult")/(Names[I]+TEXT(".png")),true,false);
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_UPRIVER_VIEW name=%s frame_ms=%.1f avg_ms=%.2f"),*Names[I],FApp::GetDeltaTime()*1000,Held*1000/Frames);
        }
        ++Run->Phase;
    },2.5f,true);
}
