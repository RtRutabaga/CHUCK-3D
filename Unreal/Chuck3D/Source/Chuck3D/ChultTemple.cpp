#include "ChultTemple.h"
#include "ChultCanyon.h"
#include "ChultRiver.h"
#include "ChultLanding.h"
#include "ChultUpriver.h"
#include "ChuckClipData.h"
#include "DockGameMode.h"
#include "ChuckCharacter.h"
#include "ClayJar.h"
#include "DockFire.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"

// The temple exterior (Claude, 2026-10-15; docs/CHULT-TEMPLE.md), made a
// mega temple on 2026-10-16: the pyramid twice over, either side of a
// Khmer-style central temple joined to both, after the user's reference.
// Everything is laid out in the canyon frame on a level plaza at PlazaZ():
// S runs from the jungle toward the temple, T across, Z up from the plaza.
namespace
{
    const FVector Along(.8,.6,0),Across(-.6,.8,0);
    float Ease(float U){U=FMath::Clamp(U,0.f,1.f);return U*U*(3-2*U);}
    FVector Frame(FVector P){P-=ChultCanyonFrame(0,0);return FVector(FVector::DotProduct(P,Along),FVector::DotProduct(P,Across),P.Z);}

    // The plaza: level from the deep jungle's edge to behind the temple, across the precinct.
    constexpr float PlazaFrom=64500,PlazaTo=91500,PlazaHalf=9000,PlazaBlend=1500;
    // Each pyramid: four terraces 5 m high, each 4 m in from the one below,
    // on a 44 m square; its stair (16 cm risers, 20 cm treads, 6 m wide)
    // climbs the -S face to the summit platform 20 m up at 39 degrees (walkable: 45).
    constexpr float Sc=78200,Base=2200,Inset=400,Tier=500,Talud=350,TaludIn=127,Band=112;
    constexpr int32 Tiers=4;
    constexpr float Rise=16,Tread=20,StairHalf=300,BalusterOut=420;
    constexpr float Top=Tiers*Tier;
    constexpr float StairTop=Sc-(Base-(Tiers-1)*Inset-Band),StairFoot=StairTop-Top/Rise*Tread;
    // The summit house: a dark doorway in its front wall, a roof comb behind.
    constexpr float HouseFront=Sc-450,HouseBack=Sc+650,HouseHalf=650,DoorHalf=120,DoorTop=260,WallTop=420;
    // The two pyramids stand either side of the central temple, their fronts in line with it.
    constexpr float PyramidT=5200;
    // The central temple: three terraces 5 m high in the same talud-tablero,
    // the lowest running into both pyramids' sides (its top meets their first
    // ledges), a broad stair up the middle to the sanctuary tower's doorway.
    constexpr int32 Levels=3;
    constexpr float LevelS[Levels]={2150,1600,1000},LevelT[Levels]={3260,2500,1800};
    constexpr float CTop=Levels*Tier,CStairHalf=400,CBalusterOut=520;
    constexpr float CStairTop=Sc-(LevelS[Levels-1]-Band),CStairFoot=CStairTop-CTop/Rise*Tread;
    // The sanctuary: a square cella with a walk-in dark vestibule under a curved spire.
    constexpr float TowerHalf=450,TowerFront=Sc-TowerHalf,TowerBody=620,TowerSpire=2400,CellHalf=260,CellDepth=300;
    // Gate pavilions either side of the central stair's foot.
    constexpr float GateS=74900,GateT=1300;
    // Behind this line the jungle has the temple: overgrown and closed.
    constexpr float BackFrom=79500,BackTo=82500;
    // The avenue: skull stakes either side, then the plaza.
    constexpr float SpawnS=65200,StakeT=520;
    const float StakeS[]={66500,68300,70100,71900};
    const float StelaS[]={67400,71000};

    struct FStair{float Foot,TopS,Height,Half,Out;};
    const FStair PyramidStair{StairFoot,StairTop,Top,StairHalf,BalusterOut};
    const FStair CentralStair{CStairFoot,CStairTop,CTop,CStairHalf,CBalusterOut};

    // Built pieces are offset across by OffT: each pyramid is built at T=0 and shifted.
    float OffT=0;
    float PlazaZ(){static const float Z=ChultCanyonBaseZ(ChultCanyonFrame(69000,0));return Z;}
    float BoxDistance(float S,float T)
    {
        const float DS=FMath::Max(FMath::Max(PlazaFrom-S,S-PlazaTo),0.f),DT=FMath::Max(FMath::Abs(T)-PlazaHalf,0.f);
        return FMath::Sqrt(DS*DS+DT*DT);
    }
    FVector At(float S,float T,float Z){return ChultCanyonFrame(S,T+OffT,PlazaZ()+Z);}
    FVector Flat(float S,float T,float Z){return ChultCanyonFrame(S,T,PlazaZ()+Z);}
    // A stair's line through the middle of each tread: the walking surface (a hidden ramp).
    float RampZ(const FStair& St,float S){return FMath::Clamp(Rise*.5f+(S-St.Foot)*Rise/Tread,0.f,St.Height);}
    float TierHalf(int32 I){return Base-I*Inset;}

    struct FSurf
    {
        TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;TArray<FColor> C;
        void Add(const FVector& P,const FVector& Normal)
        {
            V.Add(P);N.Add(Normal);C.Add(FColor::White);
            if(FMath::Abs(Normal.Z)>.7f)UV.Add(FVector2D(P.X,P.Y)/200);
            else{const FVector Axis=FVector::CrossProduct(FVector::UpVector,Normal).GetSafeNormal();UV.Add(FVector2D(FVector::DotProduct(P,Axis),P.Z)/200);}
        }
        // The outward normal is (B-A)x(C-A); UE draws the reversed winding.
        void Quad(const FVector& A,const FVector& B,const FVector& Cc,const FVector& D)
        {
            const FVector Normal=FVector::CrossProduct(B-A,Cc-A).GetSafeNormal();if(Normal.IsNearlyZero())return;
            const int32 I=V.Num();for(const FVector* P:{&A,&B,&Cc,&D})Add(*P,Normal);
            T.Append({I,I+2,I+1,I,I+3,I+2});
        }
    };
    // Modelled statues placed as static meshes (2026-10-18; Tools/build_temple_statues.py):
    // the skull stakes, the balustrades' serpent heads and the rooms' serpent idols.
    // Local is relative to FloorOrigin (At()), Rotation in the canyon frame.
    struct FStatue{const TCHAR* Mesh;FVector Local;FRotator Rotation;float Scale;bool bIndoors;};
    TArray<FStatue> Statues;
    const TCHAR* SerpentHeadMesh=TEXT("/Game/Art/ChultTemple/SM_TempleSerpentHead.SM_TempleSerpentHead");
    const TCHAR* SerpentIdolMesh=TEXT("/Game/Art/ChultTemple/SM_TempleSerpentIdol.SM_TempleSerpentIdol");
    const TCHAR* StakeMesh=TEXT("/Game/Art/ChultTemple/SM_SkullStake.SM_SkullStake");
    const TCHAR* SkullMesh=TEXT("/Game/Art/ChultTemple/SM_TempleSkull.SM_TempleSkull");
    // In* are the rooms' insides (2026-10-18): lit by their torches on lighting channel 1 only.
    struct FParts{FSurf Stone,Moss,Plaster,Steps,StepMoss,Ramp,Dark,Bone,Bark,Vines,Block,InStone,InMoss,InPlaster,InDark;};
    // A box in the frame (S0<S1, T0<T1, Z above the plaza). Inward reverses every face.
    // Always closed underneath (2026-10-17): Chuck looks up at every band,
    // cornice and overhang, which were hollow from below without it.
    void Box(FSurf& M,float S0,float S1,float T0,float T1,float Z0,float Z1,bool Inward=false)
    {
        auto Q=[&](FVector A,FVector B,FVector Cc,FVector D){if(Inward)M.Quad(A,D,Cc,B);else M.Quad(A,B,Cc,D);};
        Q(At(S0,T0,Z1),At(S1,T0,Z1),At(S1,T1,Z1),At(S0,T1,Z1));
        Q(At(S0,T0,Z0),At(S0,T1,Z0),At(S1,T1,Z0),At(S1,T0,Z0));
        Q(At(S0,T0,Z0),At(S1,T0,Z0),At(S1,T0,Z1),At(S0,T0,Z1));
        Q(At(S1,T0,Z0),At(S1,T1,Z0),At(S1,T1,Z1),At(S1,T0,Z1));
        Q(At(S1,T1,Z0),At(S0,T1,Z0),At(S0,T1,Z1),At(S1,T1,Z1));
        Q(At(S0,T1,Z0),At(S0,T0,Z0),At(S0,T0,Z1),At(S0,T1,Z1));
    }
    // A box turned by Rotation (in frame axes S,T,Z) about its centre.
    void TurnedBox(FSurf& M,FVector Center,FVector Half,FRotator Rotation)
    {
        auto C=[&](float X,float Y,float Z){const FVector L=Center+Rotation.RotateVector(FVector(X*Half.X,Y*Half.Y,Z*Half.Z));return At(L.X,L.Y,L.Z);};
        M.Quad(C(-1,-1,1),C(1,-1,1),C(1,1,1),C(-1,1,1));M.Quad(C(-1,-1,-1),C(-1,1,-1),C(1,1,-1),C(1,-1,-1));
        M.Quad(C(-1,-1,-1),C(1,-1,-1),C(1,-1,1),C(-1,-1,1));M.Quad(C(1,-1,-1),C(1,1,-1),C(1,1,1),C(1,-1,1));
        M.Quad(C(1,1,-1),C(-1,1,-1),C(-1,1,1),C(1,1,1));M.Quad(C(-1,1,-1),C(-1,-1,-1),C(-1,-1,1),C(-1,1,1));
    }
    // An ellipsoid with axes Fwd/Side/Up (frame directions) and radii R.
    void Ellipsoid(FSurf& M,FVector Center,FVector Fwd,FVector R,int32 Seg=10)
    {
        Fwd=Fwd.GetSafeNormal();const FVector Up(0,0,1),Side=FVector::CrossProduct(Up,Fwd);
        auto P=[&](int32 I,int32 J){const float A=I*2*PI/Seg,H=-PI*.5f+J*PI/(Seg/2);
            const FVector L=Center+Fwd*FMath::Cos(A)*FMath::Cos(H)*R.X+Side*FMath::Sin(A)*FMath::Cos(H)*R.Y+Up*FMath::Sin(H)*R.Z;return At(L.X,L.Y,L.Z);};
        for(int32 I=0;I<Seg;++I)for(int32 J=0;J<Seg/2;++J)M.Quad(P(I,J),P(I+1,J),P(I+1,J+1),P(I,J+1));
    }
    UProceduralMeshComponent* Emit(AActor* Owner,const TCHAR* Name,const TArray<TPair<FSurf*,const TCHAR*>>& Sections,bool bSolid,int32& Missing,int32& Triangles)
    {
        auto* Mesh=NewObject<UProceduralMeshComponent>(Owner,FName(Name));Mesh->SetupAttachment(Owner->GetRootComponent());
        Mesh->SetWorldLocation(AChultLanding::FloorOrigin());Mesh->bUseComplexAsSimpleCollision=true;Mesh->RegisterComponent();
        int32 Section=0;
        for(const auto& Pair:Sections)
        {
            const FSurf& S=*Pair.Key;TArray<FProcMeshTangent> Tangents;
            for(const FVector& N:S.N){const FVector Axis=FMath::Abs(N.Z)>.95f?FVector::ForwardVector:FVector::UpVector;Tangents.Emplace(FVector::CrossProduct(Axis,N).GetSafeNormal(),false);}
            Mesh->CreateMeshSection(Section,S.V,S.T,S.N,S.UV,S.C,Tangents,bSolid);
            auto* Material=LoadObject<UMaterialInterface>(nullptr,Pair.Value);Missing+=!Material;Mesh->SetMaterial(Section++,Material);Triangles+=S.T.Num()/3;
        }
        Mesh->SetCollisionEnabled(bSolid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(bSolid)Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->ComponentTags.Add(TEXT("ChultTemple"));
        return Mesh;
    }
    const TCHAR* StoneMat=TEXT("/Game/Art/ChultRuins/M_RuinStone.M_RuinStone");
    const TCHAR* MossMat=TEXT("/Game/Art/ChultRuins/M_RuinMoss.M_RuinMoss");
    const TCHAR* PlasterMat=TEXT("/Game/Art/Materials/M_WeatheredPlaster.M_WeatheredPlaster");
    const TCHAR* DarkMat=TEXT("/Game/Art/Materials/M_Dark.M_Dark");
    const TCHAR* VineMat=TEXT("/Game/Art/Cutscenes/FallToChult/M_ChultHangingMoss.M_ChultHangingMoss");

    // One talud-tablero terrace centred on Sc: a sloping talud, then a framed
    // tablero (a plastered panel between two stone bands, divided into bays by
    // stone ribs), its top the moss-grown ledge above. SkipT leaves the front's
    // middle (the stair) without ribs; without sides only the front and back are faced.
    void Terrace(FParts& P,float HS,float HT,float Zb,float SkipT,bool bSides)
    {
        const float hS=HS-TaludIn,hT=HT-TaludIn,Zt=Zb+Talud,BS=HS-Band,BT=HT-Band;
        auto Cn=[&](float SX,float SY,bool In,float Z){return At(Sc+SX*(In?hS:HS),SY*(In?hT:HT),Z);};
        P.Stone.Quad(Cn(-1,1,false,Zb),Cn(-1,-1,false,Zb),Cn(-1,-1,true,Zt),Cn(-1,1,true,Zt));
        P.Stone.Quad(Cn(1,-1,false,Zb),Cn(1,1,false,Zb),Cn(1,1,true,Zt),Cn(1,-1,true,Zt));
        if(bSides)
        {
            P.Stone.Quad(Cn(-1,-1,false,Zb),Cn(1,-1,false,Zb),Cn(1,-1,true,Zt),Cn(-1,-1,true,Zt));
            P.Stone.Quad(Cn(1,1,false,Zb),Cn(-1,1,false,Zb),Cn(-1,1,true,Zt),Cn(1,1,true,Zt));
        }
        Box(P.Stone,Sc-BS,Sc+BS,-BT,BT,Zt,Zt+25);
        Box(P.Plaster,Sc-hS,Sc+hS,-hT,hT,Zt+25,Zb+Tier-30);
        Box(P.Moss,Sc-BS,Sc+BS,-BT,BT,Zb+Tier-30,Zb+Tier);
        const float Z0=Zt+25,Z1=Zb+Tier-30;
        for(int32 Face=0;Face<(bSides?4:2);++Face)
        {
            const float H=Face<2?hT:hS;
            for(float U=-H+75;U<H-40;U+=150)
            {
                if(Face==0&&FMath::Abs(U)<SkipT)continue;
                if(Face==0)Box(P.Stone,Sc-hS-8,Sc-hS+2,U-14,U+14,Z0,Z1);else if(Face==1)Box(P.Stone,Sc+hS-2,Sc+hS+8,U-14,U+14,Z0,Z1);
                else if(Face==2)Box(P.Stone,Sc+U-14,Sc+U+14,-hT-8,-hT+2,Z0,Z1);else Box(P.Stone,Sc+U-14,Sc+U+14,hT-2,hT+8,Z0,Z1);
            }
        }
    }
    // A stair up the -S face: visible steps (no collision) over a hidden ramp
    // through their treads' middles, so he walks up it; a few treads are mossed
    // or chipped. Balustrades wall it in, 1 m above its noses, each ending at
    // the plaza in a serpent head looking down the avenue.
    int32 Stair(FParts& P,FRandomStream& R,const FStair& St)
    {
        const int32 StepCount=FMath::RoundToInt(St.Height/Rise);
        for(int32 K=0;K<StepCount;++K)
        {
            const float S0=St.Foot+K*Tread,S1=S0+Tread,Z0=K*Rise,Z1=(K+1)*Rise;
            FSurf& M=R.FRand()<.28f?P.StepMoss:P.Steps;
            const float Chip=R.FRand()<.15f?R.FRandRange(-120,120):0;
            M.Quad(At(S0,-St.Half,Z1),At(S1,-St.Half,Z1),At(S1,St.Half,Z1),At(S0,St.Half,Z1));
            M.Quad(At(S0,St.Half,Z0),At(S0,-St.Half,Z0),At(S0,-St.Half,Z1),At(S0,St.Half,Z1));
            if(Chip!=0){const float C=Chip>0?St.Half-Chip:-St.Half-Chip;Box(P.Steps,S0-3,S0+10,FMath::Min(C,C+40),FMath::Max(C,C+40),Z1-6,Z1+2);}
        }
        {
            // Ramp: from just before the foot (meeting the plaza flush) to just past the top edge.
            const float A0=St.Foot-Tread*.5f,A1=St.TopS-Tread*.5f,E=St.TopS+80,W=St.Half-5;
            auto Top_=[&](float S){return S<A1?FMath::Max(0.f,Rise*.5f+(S-St.Foot)*Rise/Tread):St.Height;};
            const float Ss[]={A0,A1,E};
            for(int32 I=0;I<2;++I)
            {
                const float S0=Ss[I],S1=Ss[I+1],Z0=Top_(S0),Z1=Top_(S1);
                P.Ramp.Quad(At(S0,-W,Z0),At(S1,-W,Z1),At(S1,W,Z1),At(S0,W,Z0));
                P.Ramp.Quad(At(S0,-W,Z0-40),At(S1,-W,Z1-40),At(S1,-W,Z1),At(S0,-W,Z0));
                P.Ramp.Quad(At(S1,W,Z1-40),At(S0,W,Z0-40),At(S0,W,Z0),At(S1,W,Z1));
                P.Ramp.Quad(At(S0,-W,Z0-40),At(S0,W,Z0-40),At(S1,W,Z1-40),At(S1,-W,Z1-40));
            }
        }
        const float BalusterFoot=St.Foot-60;
        for(float Sg:{-1.f,1.f})
        {
            const float T0=Sg<0?-St.Out:St.Half,T1=Sg<0?-St.Half:St.Out;
            const float ZA=Rise+100+(BalusterFoot-St.Foot)*Rise/Tread,ZC=St.Height+100;
            const FVector A(BalusterFoot,0,0),Bp(St.TopS,0,0),Cp(St.TopS,0,ZC),D(BalusterFoot,0,ZA);
            auto Q=[&](const FVector& V,float T){return At(V.X,T,V.Z);};
            P.Stone.Quad(Q(A,T0),Q(Bp,T0),Q(Cp,T0),Q(D,T0));
            P.Stone.Quad(Q(Bp,T1),Q(A,T1),Q(D,T1),Q(Cp,T1));
            P.Moss.Quad(Q(D,T0),Q(Cp,T0),Q(Cp,T1),Q(D,T1));
            P.Stone.Quad(Q(A,T1),Q(A,T0),Q(D,T0),Q(D,T1));
            // Its top end stands a metre proud of the platform: closed (2026-10-17;
            // open, the balustrade vanished when seen from the top of the stair).
            P.Stone.Quad(Q(Bp,T0),Q(Bp,T1),Q(Cp,T1),Q(Cp,T0));
            // The serpent's carved head on the plaza (2026-10-18; it was boxes): its back
            // against the balustrade's foot, jaws open down the avenue, tongue on the ground.
            const float Tc=Sg*(St.Half+St.Out)*.5f,Sa=BalusterFoot;
            Statues.Add({SerpentHeadMesh,At(Sa+10,Tc,0),FRotator(0,180,0),1.f,false});
        }
        return StepCount;
    }
    // A pyramid's summit house: its doorway opens into a ceremonial chamber
    // (2026-10-18; it was a shallow dark vestibule), under a corniced roof and
    // a pierced roof comb. The chamber's walls are 60 cm, its ceiling 3.4 m up.
    constexpr float HouseWall=60,HouseRoom=340;
    void SummitHouse(FParts& P,FRandomStream& R)
    {
        const float Z=Top,In0=HouseFront+HouseWall,In1=HouseBack-HouseWall,IH=HouseHalf-HouseWall;
        Box(P.Stone,In1,HouseBack,-HouseHalf,HouseHalf,Z,Z+WallTop);
        for(float Sg:{-1.f,1.f})
        {
            Box(P.Stone,In0,In1,Sg<0?-HouseHalf:IH,Sg<0?-IH:HouseHalf,Z,Z+WallTop);
            Box(P.Stone,HouseFront,In0,Sg<0?-HouseHalf:DoorHalf,Sg<0?-DoorHalf:HouseHalf,Z,Z+WallTop);
        }
        Box(P.Stone,HouseFront,In0,-DoorHalf,DoorHalf,Z+DoorTop,Z+WallTop);
        Box(P.Stone,In0,In1,-IH,IH,Z+HouseRoom,Z+WallTop);
        Box(P.Moss,HouseFront-60,HouseBack+60,-HouseHalf-60,HouseHalf+60,Z+WallTop,Z+WallTop+80);
        Box(P.Plaster,HouseFront-40,HouseBack+40,-HouseHalf-40,HouseHalf+40,Z+WallTop+80,Z+WallTop+140);
        Box(P.Moss,HouseFront-50,HouseBack+50,-HouseHalf-50,HouseHalf+50,Z+WallTop+140,Z+WallTop+160);
        const float C0=Sc+250,C1=Sc+340,Zc=Z+WallTop+160;
        for(float T:{-520.f,-260.f,0.f,260.f,520.f})Box(P.Stone,C0,C1,T-45,T+45,Zc,Zc+420);
        for(float Zb:{0.f,190.f,380.f})Box(P.Stone,C0,C1,-565,565,Zc+Zb,Zc+Zb+50);
        Box(P.Moss,C0-10,C1+10,-585,585,Zc+430,Zc+460);
        Box(P.Plaster,C0-6,C0,-500,500,Zc+60,Zc+180);
        for(int32 K=-3;K<=3;++K)Box(P.Plaster,HouseFront-6,HouseFront,K*34-12,K*34+12,Z+DoorTop+40,Z+DoorTop+40+R.FRandRange(40,90));
    }
    // A Khmer-style spire: a cornice, then shrinking storeys curving in toward
    // the top, each banded in pale plaster and crowned with antefixes at its
    // corners and face middles, and a lotus-bud finial.
    void Spire(FParts& P,float S,float T,float Z,float Half,float Height)
    {
        Box(P.Plaster,S-Half-25,S+Half+25,T-Half-25,T+Half+25,Z,Z+28);
        Box(P.Moss,S-Half-15,S+Half+15,T-Half-15,T+Half+15,Z+28,Z+42);
        float Zs=Z+42;constexpr int32 Storeys=9;const float H=Height*.9f/Storeys;
        for(int32 I=0;I<Storeys;++I)
        {
            // Conical, swelling slightly: the stepped Khmer cone.
            const float U=I/static_cast<float>(Storeys),R0=Half*.95f*(1-.78f*FMath::Pow(U,1.15f));
            Box(P.Stone,S-R0,S+R0,T-R0,T+R0,Zs,Zs+H*.7f);
            Box(P.Plaster,S-R0*1.05f,S+R0*1.05f,T-R0*1.05f,T+R0*1.05f,Zs+H*.7f,Zs+H*.84f);
            Box(I%2?P.Moss:P.Stone,S-R0*.97f,S+R0*.97f,T-R0*.97f,T+R0*.97f,Zs+H*.84f,Zs+H);
            const float A=FMath::Max(R0*.15f,10.f);
            for(int32 X=-1;X<=1;++X)for(int32 Y=-1;Y<=1;++Y)
            {
                if(X==0&&Y==0)continue;const float Ps=S+X*R0*.92f,Pt=T+Y*R0*.92f;
                Box(P.Stone,Ps-A,Ps+A,Pt-A,Pt+A,Zs+H,Zs+H+H*(X&&Y?.6f:.45f));
            }
            Zs+=H;
        }
        const float F=Half*.17f;
        Box(P.Stone,S-F,S+F,T-F,T+F,Zs,Zs+Height*.05f);
        Ellipsoid(P.Stone,FVector(S,T,Zs+Height*.05f+F*1.3f),FVector(1,0,0),FVector(F*1.1f,F*1.1f,F*2),8);
    }
    // A small prasat (corner tower or gate pavilion): a plinth, a solid cella
    // with dark false doors on each face, and a spire.
    // DoorFace (0 -S, 1 +S, 2 -T, 3 +T; 2026-10-18) makes that false door a real
    // one: the cella is hollow (45 cm walls, ceiling 55 cm below its top) and
    // three steps climb the plinth to the threshold.
    constexpr float CellaWall=45,CellaRoof=55;
    void Prasat(FParts& P,float S,float T,float Z,float Half,float Body,float SpireHeight,int32 DoorFace=-1)
    {
        Box(P.Stone,S-Half-35,S+Half+35,T-Half-35,T+Half+35,Z,Z+30);
        Box(P.Moss,S-Half-20,S+Half+20,T-Half-20,T+Half+20,Z+30,Z+45);
        const float W=Half*.38f,D=(Body-45)*.62f,Zd=Z+45;
        if(DoorFace<0)Box(P.Stone,S-Half,S+Half,T-Half,T+Half,Z+45,Z+Body);
        else
        {
            const float I=Half-CellaWall,Z1=Z+Body;
            // A wall along face F between U0 and U1 (across it) from Za to Zb.
            auto Wall=[&](int32 F,float U0,float U1,float Za,float Zb)
            {
                if(F==0)Box(P.Stone,S-Half,S-I,T+U0,T+U1,Za,Zb);else if(F==1)Box(P.Stone,S+I,S+Half,T+U0,T+U1,Za,Zb);
                else if(F==2)Box(P.Stone,S+U0,S+U1,T-Half,T-I,Za,Zb);else Box(P.Stone,S+U0,S+U1,T+I,T+Half,Za,Zb);
            };
            for(int32 F=0;F<4;++F)
            {
                const float U=F<2?Half:I;
                if(F!=DoorFace){Wall(F,-U,U,Zd,Z1);continue;}
                Wall(F,-U,-W,Zd,Z1);Wall(F,W,U,Zd,Z1);Wall(F,-W,W,Zd+D,Z1);
            }
            Box(P.Stone,S-I,S+I,T-I,T+I,Z1-CellaRoof,Z1);
            // Three 15 cm steps up the plinth to the threshold, over a hidden ramp.
            const FVector2D Out=DoorFace==0?FVector2D(-1,0):DoorFace==1?FVector2D(1,0):DoorFace==2?FVector2D(0,-1):FVector2D(0,1),Side(-Out.Y,Out.X);
            auto Pt=[&](float O,float Y){return FVector2D(S,T)+Out*O+Side*Y;};
            const float Face=Half+35,SW=W+12;
            for(int32 K=0;K<3;++K)
            {
                const FVector2D A=Pt(Face-2,-SW),B=Pt(Face+90-30*K,SW);
                Box(K?P.Steps:P.StepMoss,FMath::Min(A.X,B.X),FMath::Max(A.X,B.X),FMath::Min(A.Y,B.Y),FMath::Max(A.Y,B.Y),Z,Z+15*(K+1));
            }
            // Through the treads' middles (26.6 degrees), then level onto the plinth.
            auto Up=[&](float O0,float Z0,float O1,float Z1_)
            {
                const FVector2D A=Pt(O0,-SW),B=Pt(O0,SW),C=Pt(O1,SW),E=Pt(O1,-SW);
                const FVector V[]={At(A.X,A.Y,Z+Z0),At(B.X,B.Y,Z+Z0),At(C.X,C.Y,Z+Z1_),At(E.X,E.Y,Z+Z1_)};
                if(FVector::CrossProduct(V[1]-V[0],V[2]-V[0]).Z>0)P.Ramp.Quad(V[0],V[1],V[2],V[3]);else P.Ramp.Quad(V[0],V[3],V[2],V[1]);
            };
            Up(Face+105,0,Face+15,45);Up(Face+15,45,Face-5,45);
        }
        if(DoorFace!=0)Box(P.Dark,S-Half-3,S-Half,T-W,T+W,Zd,Zd+D);
        if(DoorFace!=1)Box(P.Dark,S+Half,S+Half+3,T-W,T+W,Zd,Zd+D);
        if(DoorFace!=2)Box(P.Dark,S-W,S+W,T-Half-3,T-Half,Zd,Zd+D);
        if(DoorFace!=3)Box(P.Dark,S-W,S+W,T+Half,T+Half+3,Zd,Zd+D);
        Box(P.Plaster,S-Half-8,S-Half,T-W-18,T+W+18,Zd+D,Zd+D+28);Box(P.Plaster,S+Half,S+Half+8,T-W-18,T+W+18,Zd+D,Zd+D+28);
        Box(P.Plaster,S-W-18,S+W+18,T-Half-8,T-Half,Zd+D,Zd+D+28);Box(P.Plaster,S-W-18,S+W+18,T+Half,T+Half+8,Zd+D,Zd+D+28);
        Spire(P,S,T,Z+Body,Half,SpireHeight);
    }
    // The sanctuary on the central temple's top: its real doorway (the way in
    // will be the temple interior) into a dark vestibule, then the great spire.
    void Sanctuary(FParts& P)
    {
        const float Z=CTop,F=TowerFront,B=Sc+TowerHalf,H=TowerHalf,In=F+60,Back=In+CellDepth;
        Box(P.Stone,Back,B,-H,H,Z,Z+TowerBody);
        for(float Sg:{-1.f,1.f})
        {
            Box(P.Stone,In,Back,Sg<0?-H:CellHalf,Sg<0?-CellHalf:H,Z,Z+TowerBody);
            Box(P.Stone,F,In,Sg<0?-H:DoorHalf,Sg<0?-DoorHalf:H,Z,Z+TowerBody);
            // Plaster jambs and false doors on the sides.
            Box(P.Plaster,F-8,F,Sg<0?-DoorHalf-35:DoorHalf,Sg<0?-DoorHalf:DoorHalf+35,Z,Z+DoorTop);
            const float Side=Sg*H;
            Box(P.Dark,Sc-90,Sc+90,Sg<0?Side-3:Side,Sg<0?Side:Side+3,Z+20,Z+300);
            Box(P.Plaster,Sc-120,Sc+120,Sg<0?Side-8:Side,Sg<0?Side:Side+8,Z+300,Z+330);
        }
        Box(P.Stone,F,In,-DoorHalf,DoorHalf,Z+DoorTop,Z+TowerBody);
        Box(P.Stone,In,Back,-CellHalf,CellHalf,Z+300,Z+TowerBody);
        Box(P.Dark,In+.5f,Back-.5f,-CellHalf+.5f,CellHalf-.5f,Z+.5f,Z+299.5f,true);
        Box(P.Dark,F-1,In+1,-DoorHalf+.5f,DoorHalf-.5f,Z+.5f,Z+DoorTop-.5f,true);
        Box(P.Plaster,F-8,F,-DoorHalf-35,DoorHalf+35,Z+DoorTop,Z+DoorTop+32);
        for(int32 K=-3;K<=3;++K)Box(P.Plaster,F-6,F,K*34-12,K*34+12,Z+DoorTop+50,Z+DoorTop+50+(K%2?45:75));
        Spire(P,Sc,0,Z+TowerBody,TowerHalf,TowerSpire);
    }
    // Hanging moss over a terrace's tablero edges.
    int32 Strands(FParts& P,FRandomStream& R,float HS,float HT,float ZTop,int32 Count,float SkipT,bool bSides)
    {
        int32 Made=0;
        for(int32 I=0;I<Count;++I)
        {
            const int32 Face=R.RandRange(0,bSides?3:1);const bool Front=Face<2;
            const float Dist=(Front?HS:HT)-Band+8,Span=(Front?HT:HS)-Band,U=R.FRandRange(-.9f,.9f)*Span;
            const FVector Out=Face==0?FVector(-1,0,0):Face==1?FVector(1,0,0):Face==2?FVector(0,-1,0):FVector(0,1,0);
            if(Face==0&&FMath::Abs(U)<SkipT+60)continue;
            const FVector Start=FVector(Sc,0,ZTop)+Out*Dist+FVector(Out.Y,-Out.X,0)*U;
            const float Length=R.FRandRange(120,Tier+150);FVector Prev=Start;
            for(int32 J=1;J<=10;++J)
            {
                const float Uj=J/10.f;FVector Next=Start+FVector(0,0,-Length*Uj)+Out*(Uj<.4f?14*Uj:TaludIn*(Uj-.4f)*.9f+6);
                const FVector D=(Next-Prev).GetSafeNormal(),X=FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal(),Y=FVector::CrossProduct(D,X);
                const float Ra=FMath::Lerp(4.f,1.5f,Uj);
                for(int32 K=0;K<5;++K)
                {
                    const float A=K*2*PI/5,B=(K+1)*2*PI/5;
                    auto Q=[&](const FVector& C,float Ang,float Rr){const FVector L=C+(X*FMath::Cos(Ang)+Y*FMath::Sin(Ang))*Rr;return At(L.X,L.Y,L.Z);};
                    P.Vines.Quad(Q(Prev,A,Ra),Q(Prev,B,Ra),Q(Next,B,Ra),Q(Next,A,Ra));
                }
                Prev=Next;
            }
            ++Made;
        }
        return Made;
    }
    // Where the masonry stands (with Margin round it), in the unshifted frame.
    bool OnStructure(float S,float T,float Margin)
    {
        const float A=FMath::Abs(T),Py=FMath::Abs(A-PyramidT);
        if(Py<Base+Margin&&FMath::Abs(S-Sc)<Base+Margin)return true;
        if(A<LevelT[0]+Margin&&FMath::Abs(S-Sc)<LevelS[0]+Margin)return true;
        if(A<CBalusterOut+Margin&&S>CStairFoot-280-Margin&&S<Sc)return true;
        if(Py<BalusterOut+Margin&&S>StairFoot-280-Margin&&S<Sc)return true;
        return FMath::Abs(S-GateS)<300+Margin&&FMath::Abs(A-GateT)<300+Margin;
    }
    // The ways in, kept open: the old road up the middle, the forecourt between
    // the gates, and from the road across the plaza to each pyramid's stair.
    bool OnWayIn(float S,float T,float Margin)
    {
        const float A=FMath::Abs(T);
        if(S<CStairFoot&&A<800+Margin)return true;
        if(S>72500&&A<GateT+450+Margin)return true;
        return FMath::PointDistToSegment(FVector(S,A,0),FVector(71500,1000,0),FVector(StairFoot-150,PyramidT,0))<500+Margin;
    }

    // The rooms (user 2026-10-18): the pyramids' summit houses, the gate
    // pavilions and the top terrace's corner towers open into small ceremonial
    // interiors (the sanctuary's doorway stays the future temple interior's).
    // A room in the unshifted frame: centre C, In from its door toward its
    // back wall, half depth HD along In and half width HW across, floor Z0 and
    // ceiling Z1 above the plaza; its door is centred in the near wall.
    enum class ERoom:uint8{Summit,Gate,Shrine};
    struct FRoom{ERoom Kind;FVector2D C,In;float HD,HW,Z0,Z1,DoorHalf,DoorTop;};
    TArray<FRoom> Rooms()
    {
        TArray<FRoom> Out;
        const float In0=HouseFront+HouseWall,In1=HouseBack-HouseWall;
        for(float Side:{-1.f,1.f})Out.Add({ERoom::Summit,FVector2D((In0+In1)*.5f,Side*PyramidT),FVector2D(1,0),(In1-In0)*.5f,HouseHalf-HouseWall,Top,Top+HouseRoom,DoorHalf,DoorTop});
        for(float Side:{-1.f,1.f})Out.Add({ERoom::Gate,FVector2D(GateS,Side*GateT),FVector2D(1,0),260-CellaWall,260-CellaWall,45,400-CellaRoof,260*.38f,(400-45)*.62f});
        for(float X:{-1.f,1.f})for(float Y:{-1.f,1.f})
            Out.Add({ERoom::Shrine,FVector2D(Sc+X*(LevelS[2]-240),Y*(LevelT[2]-240)),FVector2D(0,Y),190-CellaWall,190-CellaWall,CTop+45,CTop+330-CellaRoof,190*.38f,(330-45)*.62f});
        return Out;
    }
    // Which face of a frame-aligned box the door is in (0 -S, 1 +S, 2 -T, 3 +T).
    int32 DoorFaceOf(const FRoom& Rm){return Rm.In.X>.5f?0:Rm.In.X<-.5f?1:Rm.In.Y>.5f?2:3;}
    FVector2D SideOf(const FRoom& Rm){return FVector2D(-Rm.In.Y,Rm.In.X);}
    // A room-local point: X along In from the centre, Y across, Z above its floor.
    FVector RL(const FRoom& Rm,float X,float Y,float Z){const FVector2D P=Rm.C+Rm.In*X+SideOf(Rm)*Y;return FVector(P.X,P.Y,Rm.Z0+Z);}
    // A box in room-local coordinates (rooms are frame-aligned, so it stays a box).
    void RBox(FSurf& M,const FRoom& Rm,float X0,float X1,float Y0,float Y1,float Z0,float Z1)
    {
        const FVector A=RL(Rm,X0,Y0,Z0),B=RL(Rm,X1,Y1,Z1);
        Box(M,FMath::Min(A.X,B.X),FMath::Max(A.X,B.X),FMath::Min(A.Y,B.Y),FMath::Max(A.Y,B.Y),A.Z,B.Z);
    }
    // A quad facing Toward (all in frame coordinates).
    void QuadToward(FSurf& M,const FVector& A,const FVector& B,const FVector& C,const FVector& D,const FVector& Toward)
    {
        const FVector a=At(A.X,A.Y,A.Z),b=At(B.X,B.Y,B.Z),c=At(C.X,C.Y,C.Z),d=At(D.X,D.Y,D.Z),t=At(Toward.X,Toward.Y,Toward.Z);
        if(FVector::DotProduct(FVector::CrossProduct(b-a,c-a),t-a)>=0)M.Quad(a,b,c,d);else M.Quad(d,c,b,a);
    }
    // The room's inside faces, a centimetre in from the masonry, with the door
    // left open: a stone floor and ceiling, plastered walls over a stone dado.
    constexpr float Dado=55;
    void Lining(FParts& Parts,const FRoom& Rm)
    {
        FSurf& M=Parts.InStone;
        const FVector A=RL(Rm,-Rm.HD,-Rm.HW,0),B=RL(Rm,Rm.HD,Rm.HW,0);
        const float S0=FMath::Min(A.X,B.X)+1,S1=FMath::Max(A.X,B.X)-1,T0=FMath::Min(A.Y,B.Y)+1,T1=FMath::Max(A.Y,B.Y)-1,Z0=Rm.Z0+1,Z1=Rm.Z1-1;
        const FVector Mid((S0+S1)*.5f,(T0+T1)*.5f,(Z0+Z1)*.5f);
        for(float Z:{Z0,Z1})QuadToward(M,{S0,T0,Z},{S1,T0,Z},{S1,T1,Z},{S0,T1,Z},Mid);
        const FVector2D Ends[4][2]={{{S0,T0},{S0,T1}},{{S1,T0},{S1,T1}},{{S0,T0},{S1,T0}},{{S0,T1},{S1,T1}}};
        const int32 Door=DoorFaceOf(Rm);
        for(int32 F=0;F<4;++F)
        {
            const FVector2D E0=Ends[F][0],E1=Ends[F][1];
            auto Piece=[&](float U0,float U1,float Za,float Zb)
            {
                const FVector2D P=FMath::Lerp(E0,E1,U0),Q=FMath::Lerp(E0,E1,U1);const float Zd=Rm.Z0+Dado;
                if(Za<Zd)QuadToward(M,{P.X,P.Y,Za},{Q.X,Q.Y,Za},{Q.X,Q.Y,FMath::Min(Zb,Zd)},{P.X,P.Y,FMath::Min(Zb,Zd)},Mid);
                if(Zb>Zd)QuadToward(Parts.InPlaster,{P.X,P.Y,FMath::Max(Za,Zd)},{Q.X,Q.Y,FMath::Max(Za,Zd)},{Q.X,Q.Y,Zb},{P.X,P.Y,Zb},Mid);
            };
            if(F!=Door){Piece(0,1,Z0,Z1);continue;}
            const float W=(Rm.DoorHalf+1)/FVector2D::Distance(E0,E1);
            Piece(0,.5f-W,Z0,Z1);Piece(.5f+W,1,Z0,Z1);Piece(.5f-W,.5f+W,Rm.Z0+Rm.DoorTop-1,Z1);
        }
    }
    // A tube along Path (frame coordinates), radius from R0 to R1, capped.
    void PathTube(FSurf& M,const TArray<FVector>& Path,float R0,float R1,int32 Seg=8)
    {
        TArray<TArray<FVector>> Rings;
        for(int32 I=0;I<Path.Num();++I)
        {
            const FVector D=(Path[FMath::Min(I+1,Path.Num()-1)]-Path[FMath::Max(I-1,0)]).GetSafeNormal();
            const FVector X=FMath::Abs(D.Z)>.9f?FVector::CrossProduct(D,FVector::ForwardVector).GetSafeNormal():FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal(),Y=FVector::CrossProduct(D,X);
            const float Rr=FMath::Lerp(R0,R1,I/float(FMath::Max(Path.Num()-1,1)));
            TArray<FVector>& Ring=Rings.AddDefaulted_GetRef();
            for(int32 K=0;K<Seg;++K){const float A=K*2*PI/Seg;Ring.Add(Path[I]+(X*FMath::Cos(A)+Y*FMath::Sin(A))*Rr);}
        }
        auto P=[&](const FVector& V){return At(V.X,V.Y,V.Z);};
        for(int32 I=0;I+1<Rings.Num();++I)for(int32 K=0;K<Seg;++K)
        {
            const int32 L=(K+1)%Seg;const FVector Mid=(Path[I]+Path[I+1])*.5f;
            QuadToward(M,Rings[I][K],Rings[I+1][K],Rings[I+1][L],Rings[I][L],Mid+(Mid-(Rings[I][K]+Rings[I+1][L])*.5f)*-50);
        }
        for(int32 End:{0,Rings.Num()-1})
        {
            const FVector Out=Path[End]+(End?1:-1)*(Path.Last()-Path[0]).GetSafeNormal()*100;
            for(int32 K=1;K+1<Seg;++K)QuadToward(M,Rings[End][0],Rings[End][K],Rings[End][K+1],Rings[End][0],Out);
        }
    }
    void Drum(FSurf& M,const FVector& A,const FVector& B,float R,int32 Seg=12){PathTube(M,{A,B},R,R,Seg);}
    // A coiled serpent idol (the 2D temple's dressing): a tapering coil rising
    // three turns round its base, the head reared above it, jaws open.
    void SerpentIdol(FParts& P,const FVector& Foot,float R,float Yaw)
    {
        // The modelled idol is 25 cm in coil radius (2026-10-18; it was tubes and ellipsoids).
        Statues.Add({SerpentIdolMesh,At(Foot.X,Foot.Y,Foot.Z),FRotator(0,Yaw,0),R/25,true});
    }
    // A glyph frieze on a wall (at X, standing out against Dir): rows of carved
    // stone cartouches, each with a number in the bars and dots of the old
    // count, cut dark into its face.
    void Glyphs(FParts& P,FRandomStream& R,const FRoom& Rm,float X,float Dir,float Y0,float Y1,float Z0,float Z1,bool bBackWall)
    {
        // A box on the wall: across it Y0..Y1, up it Za..Zb, standing D0..D1 out from it.
        auto On=[&](FSurf& M,float A0,float A1,float Za,float Zb,float D0,float D1)
        {if(bBackWall)RBox(M,Rm,X-D1*Dir,X-D0*Dir,A0,A1,Za,Zb);else RBox(M,Rm,A0,A1,X-D1*Dir,X-D0*Dir,Za,Zb);};
        On(P.InStone,Y0-6,Y1+6,Z0-6,Z0-1,0,2.5f);On(P.InStone,Y0-6,Y1+6,Z1+1,Z1+6,0,2.5f);
        for(float Z=Z0+17;Z<Z1-12;Z+=36)for(float Y=Y0+15;Y<Y1-10;Y+=31)
        {
            const float H=R.FRandRange(13,16),W=R.FRandRange(11,13.5f);const int32 N=R.RandRange(1,19);
            On(P.InStone,Y-W,Y+W,Z-H,Z+H,0,2.5f);
            // Bars (fives) from the bottom up, then the dots (ones) above them.
            float Zm=Z-H+4;
            for(int32 B=0;B<N/5;++B){On(P.InDark,Y-W*.75f,Y+W*.75f,Zm,Zm+2.6f,2.5f,3.2f);Zm+=4.6f;}
            const int32 Dots=N%5;
            for(int32 K=0;K<Dots;++K){const float Yd=Y+(K-(Dots-1)*.5f)*5.2f;On(P.InDark,Yd-1.6f,Yd+1.6f,Zm+.4f,Zm+3.6f,2.5f,3.2f);}
        }
    }
    // A fire (frame coordinates of its flame's base) and where its light hangs.
    struct FFire{FVector Flame,Light;float Width,Height,Intensity,Radius;};
    struct FJarSpot{FVector At;float Yaw;bool bUrn;int32 Cigarettes;};
    // An iron wall torch: a bracket out from the wall at room-local (X,Y),
    // pointing along (DX,DY) into the room, its cup's rim at Z.
    void WallTorch(FParts& P,TArray<FFire>& Fires,const FRoom& Rm,float X,float Y,float DX,float DY,float Z)
    {
        auto L=[&](float O,float Zz){return RL(Rm,X+DX*O,Y+DY*O,Zz);};
        const FVector A=L(0,Z-26),B=L(22,Z-22);
        RBox(P.InDark,Rm,FMath::Min(X,X+DX*22)-(DX?0:2.5f),FMath::Max(X,X+DX*22)+(DX?0:2.5f),FMath::Min(Y,Y+DY*22)-(DY?0:2.5f),FMath::Max(Y,Y+DY*22)+(DY?0:2.5f),Z-26,Z-21);
        RBox(P.InDark,Rm,FMath::Min(X,X+DX*4)-(DX?0:6),FMath::Max(X,X+DX*4)+(DX?0:6),FMath::Min(Y,Y+DY*4)-(DY?0:6),FMath::Max(Y,Y+DY*4)+(DY?0:6),Z-46,Z-14);  // wall plate
        Drum(P.InDark,L(24,Z-15),L(24,Z-1),7.5f,10);
        Drum(P.InDark,L(24,Z-24),L(24,Z-15),3.5f,8);
        Fires.Add({L(24,Z-2),L(34,Z+18),18,30,4200,620});
    }
    // A standing brazier: a stone stem and a wide bowl, its fire at Z+84.
    void Brazier(FParts& P,TArray<FFire>& Fires,const FRoom& Rm,float X,float Y,float Z)
    {
        Drum(P.InStone,RL(Rm,X,Y,Z),RL(Rm,X,Y,Z+10),17,12);
        Drum(P.InStone,RL(Rm,X,Y,Z+10),RL(Rm,X,Y,Z+66),8,10);
        Drum(P.InStone,RL(Rm,X,Y,Z+66),RL(Rm,X,Y,Z+82),24,14);
        Drum(P.InDark,RL(Rm,X,Y,Z+82),RL(Rm,X,Y,Z+83),20,14);
        Fires.Add({RL(Rm,X,Y,Z+80),RL(Rm,X,Y,Z+125),30,42,5200,700});
    }
    // What each room holds: the 2D temple's dressing (serpent idols, glyphs,
    // urns, column drums) round an altar, its torches, and jars.
    void Furnish(FParts& P,FRandomStream& R,const FRoom& Rm,TArray<FFire>& Fires,TArray<FJarSpot>& Jars)
    {
        const float HD=Rm.HD,HW=Rm.HW,Yaw=FMath::RadiansToDegrees(FMath::Atan2(-Rm.In.Y,-Rm.In.X)),H=Rm.Z1-Rm.Z0;
        auto Jar=[&](float X,float Y,float Z,bool Urn){Jars.Add({RL(Rm,X,Y,Z),float(R.FRandRange(0,360)),Urn,Urn?R.RandRange(2,4):R.RandRange(1,2)});};
        if(Rm.Kind==ERoom::Summit)
        {
            // A dais across the back with the altar and the coiled serpent on it, braziers either side.
            RBox(P.InStone,Rm,250,HD,-300,300,0,18);RBox(P.InMoss,Rm,246,250,-300,300,0,14);
            RBox(P.InStone,Rm,330,440,-110,110,18,95);RBox(P.InStone,Rm,322,448,-120,120,95,105);
            Glyphs(P,R,Rm,330,1,-100,100,22,92,true);
            SerpentIdol(P,RL(Rm,385,0,105),25,Yaw);
            for(float Sg:{-1.f,1.f}){Brazier(P,Fires,Rm,300,Sg*205,18);Jar(430,Sg*235,18,true);}
            // Square piers carrying the corbelled ceiling; one has fallen, its drums on the floor.
            for(float X:{-150.f,130.f})for(float Y:{-330.f,330.f})
            {
                const bool Broken=X>0&&Y<0;
                RBox(P.InMoss,Rm,X-34,X+34,Y-34,Y+34,0,12);RBox(P.InStone,Rm,X-28,X+28,Y-28,Y+28,12,Broken?150:H);
                if(!Broken){RBox(P.InPlaster,Rm,X-36,X+36,Y-36,Y+36,H-46,H-30);RBox(P.InStone,Rm,X-40,X+40,Y-40,Y+40,H-30,H);}
            }
            Drum(P.InStone,RL(Rm,20,-420,26),RL(Rm,92,-455,26),26,12);
            Drum(P.InStone,RL(Rm,-15,-300,24),RL(Rm,-15,-300,72),24,12);
            for(float Sg:{-1.f,1.f})
            {
                RBox(P.InStone,Rm,-HD,HD,Sg<0?-HW:HW-28,Sg<0?-HW+28:HW,H-40,H);
                RBox(P.InStone,Rm,-HD,HD,Sg<0?-HW:HW-52,Sg<0?-HW+52:HW,H-18,H);
                // Glyph panels between the torches, dark niches in the back wall with jars before them.
                Glyphs(P,R,Rm,Sg*HW,Sg,-130,130,105,255,false);
                WallTorch(P,Fires,Rm,-260,Sg*HW,0,-Sg,180);WallTorch(P,Fires,Rm,260,Sg*HW,0,-Sg,180);
                RBox(P.InDark,Rm,HD-1.5f,HD,Sg*380-45,Sg*380+45,40,190);RBox(P.InPlaster,Rm,HD-4,HD,Sg*380-60,Sg*380+60,190,204);
                Jar(HD-30,Sg*380,0,false);
                Jar(-HD+40,Sg*(HW-40),0,false);Jar(-HD+38,Sg*(HW-82),0,false);Jar(-HD+80,Sg*(HW-45),0,false);
            }
            Glyphs(P,R,Rm,HD,1,-260,260,205,H-60,true);
        }
        else if(Rm.Kind==ERoom::Gate)
        {
            RBox(P.InStone,Rm,90,170,-45,45,0,70);RBox(P.InStone,Rm,84,176,-52,52,70,78);RBox(P.InMoss,Rm,86,174,-50,50,0,8);
            SerpentIdol(P,RL(Rm,130,0,78),15,Yaw);
            Glyphs(P,R,Rm,HD,1,-160,160,110,H-30,true);
            for(float Sg:{-1.f,1.f})WallTorch(P,Fires,Rm,30,Sg*HW,0,-Sg,170);
            Jar(-HD+35,-(HW-35),0,false);Jar(-HD+35,HW-35,0,false);Jar(HD-38,-(HW-38),0,true);Jar(HD-35,HW-35,0,false);
        }
        else
        {
            RBox(P.InStone,Rm,80,HD,-46,46,0,55);RBox(P.InStone,Rm,74,HD,-52,52,55,62);
            SerpentIdol(P,RL(Rm,112,0,62),12,Yaw);
            Glyphs(P,R,Rm,HD,1,-110,110,100,H-20,true);
            WallTorch(P,Fires,Rm,10,HW,0,-1,150);
            Jar(-HD+32,-(HW-32),0,false);Jar(-HD+32,HW-32,0,false);Jar(HD-36,-(HW-36),0,true);
        }
    }
    // Inside a room (or its doorway), world location.
    bool InRoom(const FVector& World,float Margin)
    {
        const FVector F=Frame(World-AChultLanding::FloorOrigin());const float Z=F.Z-PlazaZ();
        for(const FRoom& Rm:Rooms())
        {
            const FVector2D D=FVector2D(F.X,F.Y)-Rm.C;const float X=FVector2D::DotProduct(D,Rm.In),Y=FVector2D::DotProduct(D,SideOf(Rm));
            if(X>-Rm.HD-Margin&&X<Rm.HD&&FMath::Abs(Y)<Rm.HW&&Z>Rm.Z0-30&&Z<Rm.Z1)return true;
        }
        return false;
    }
}

float ChultTempleGroundZ(const FVector& P,float Base_)
{
    const FVector F=Frame(P);if(F.X<PlazaFrom-PlazaBlend-10)return Base_;
    return FMath::Lerp(PlazaZ(),Base_,Ease(BoxDistance(F.X,F.Y)/PlazaBlend));
}
bool InChultTempleReserve(const FVector& P,float Margin)
{
    // The precinct up to the closed jungle behind the temple: this file plants it.
    const FVector F=Frame(P);return F.X>PlazaFrom-Margin&&F.X<BackTo+Margin&&FMath::Abs(F.Y)<ChultTemplePrecinctHalf+Margin;
}
float ChultTempleRoadEndS(){return CStairFoot-150;}
// Running up the road, the central spire comes clear between the trees here
// (glimpses from S~63000; judged from the approach run's frames, -ChuckTempleApproachShots).
float ChultTempleRevealS(){return 63500;}
float ChultTempleMusicFromS()
{
    // The swell's lead time at run speed, measured back along the road's line.
    static const float From=[]()
    {
        float S=ChultTempleRevealS(),Left=ChultTempleMusicSwell*ChuckClipData::RunSpeed;
        while(Left>0){const float Back=S-10;Left-=FVector2D(10,ChultUpriverRoadT(S)-ChultUpriverRoadT(Back)).Size();S=Back;}
        return S;
    }();
    return From;
}
bool InChultTempleMusicZone(const FVector& World,bool bWasInside)
{
    const FVector F=Frame(World-AChultLanding::FloorOrigin());
    return F.X>ChultTempleMusicFromS()-(bWasInside?1500:0)&&FMath::Abs(F.Y)<ChultTemplePrecinctHalf+3000;
}
bool ChultTempleMusicRunIn(const FVector& World)
{
    const FVector F=Frame(World-AChultLanding::FloorOrigin());
    return F.X<ChultTempleMusicFromS()+1000&&FMath::Abs(F.Y-ChultUpriverRoadT(F.X))<1500;
}
FVector ChultTempleSpawn(){return AChultLanding::FloorOrigin()+Flat(SpawnS,0,34.65f);}
float ChultTempleSpawnYaw(){return Along.Rotation().Yaw;}

void BuildChultTemple(AActor* Owner)
{
    const double Started=FPlatformTime::Seconds();const FVector G=AChultLanding::FloorOrigin();
    FParts P;FRandomStream R(2026101591);int32 Steps=0,Strand=0;Statues.Reset();
    // The twin pyramids, each the October 15 pyramid: terraces, stair, summit house.
    for(float Side:{-1.f,1.f})
    {
        OffT=Side*PyramidT;
        for(int32 I=0;I<Tiers;++I){Terrace(P,TierHalf(I),TierHalf(I),I*Tier,BalusterOut+40,true);Strand+=Strands(P,R,TierHalf(I),TierHalf(I),I*Tier+Tier,22,BalusterOut,true);}
        Steps+=Stair(P,R,PyramidStair);
        SummitHouse(P,R);
    }
    OffT=0;
    // The central temple: three terraces (the lowest built into both pyramids),
    // its stair, the sanctuary and eight smaller towers, two gate pavilions.
    for(int32 I=0;I<Levels;++I){Terrace(P,LevelS[I],LevelT[I],I*Tier,CBalusterOut+40,I>0);Strand+=Strands(P,R,LevelS[I],LevelT[I],I*Tier+Tier,I?22:14,CBalusterOut,I>0);}
    Steps+=Stair(P,R,CentralStair);
    Sanctuary(P);int32 Towers=1;
    for(float X:{-1.f,1.f})for(float Y:{-1.f,1.f})
    {
        // The top terrace's four open toward the middle: small shrines (2026-10-18).
        Prasat(P,Sc+X*(LevelS[2]-240),Y*(LevelT[2]-240),CTop,190,330,1250,Y>0?2:3);
        Prasat(P,Sc+X*(LevelS[1]-330),Y*(LevelT[1]-330),2*Tier,165,290,800);Towers+=2;
    }
    for(float Y:{-1.f,1.f}){Prasat(P,GateS,Y*GateT,0,260,400,380,0);++Towers;}
    // Stelae beside the avenue, carved toward it, each with a low altar.
    int32 Stelae=0;
    for(float S:StelaS)for(float Sg:{-1.f,1.f})
    {
        const float T=Sg*1500;
        Box(P.Stone,S-28,S+28,T-65,T+65,0,290);Box(P.Moss,S-32,S+32,T-70,T+70,290,312);
        const float Face=T-Sg*65;
        for(int32 Rw=0;Rw<5;++Rw)for(int32 Cl=-1;Cl<=1;++Cl)
        {
            const float Zc=60+Rw*44,Sw=Cl*34,H=R.FRandRange(12,18),W=R.FRandRange(11,16);
            if(Sg<0)Box(P.Plaster,S+Sw-W,S+Sw+W,Face,Face+4,Zc-H,Zc+H);else Box(P.Plaster,S+Sw-W,S+Sw+W,Face-4,Face,Zc-H,Zc+H);
        }
        Box(P.Moss,S-40,S+40,T-Sg*170-40,T-Sg*170+40,0,45);++Stelae;
    }
    // Fallen carved blocks on the plaza, off the avenue and the structures.
    int32 Fragments=0;
    for(int32 I=0;I<90&&Fragments<26;++I)
    {
        const float S=R.FRandRange(66000,79000),T=R.FRandRange(1000,8500)*(R.FRand()<.5f?-1:1);
        if(OnStructure(S,T,250)||OnWayIn(S,T,150))continue;bool Clear=true;
        for(float St:StelaS)Clear&=FVector2D(S-St,FMath::Abs(T)-1500).Size()>400;if(!Clear)continue;
        const FVector Half(R.FRandRange(30,70),R.FRandRange(30,60),R.FRandRange(20,45));
        TurnedBox(R.FRand()<.4f?P.Moss:P.Stone,FVector(S,T,Half.Z*.7f),Half,FRotator(R.FRandRange(-12,12),R.FRandRange(0,360),R.FRandRange(-10,10)));
        if(R.FRand()<.5f)Box(P.Plaster,S-Half.X*.5f,S+Half.X*.5f,T-Half.Y-2,T-Half.Y+1,Half.Z*.4f,Half.Z*1.2f);
        ++Fragments;
    }
    // Skull stakes: eight, in pairs down the avenue, the skulls looking across it.
    // Since 2026-10-18 a hewn, lashed stake (Tools/build_temple_statues.py) with
    // the CC0 skull on its point; the two lean draws are kept, so nothing else moves.
    int32 Stakes=0;
    {
        FRandomStream Bones(2026101802);
        for(float S:StakeS)for(float Sg:{-1.f,1.f})
        {
            const float T=Sg*StakeT,Lean=R.FRandRange(-4,4),Sway=R.FRandRange(-3,3);
            const FRotator Tilt(Lean*.9f,Bones.FRandRange(0,360),Sway*.9f);
            Statues.Add({StakeMesh,At(S,T,0),Tilt,1.f,false});
            // The skull sits on the stake's point, its face across the avenue, a little askew.
            const FVector Point=At(S,T,0)+FRotator(0,Along.Rotation().Yaw,0).RotateVector(Tilt.RotateVector(FVector(1.8f,0,184)));
            Statues.Add({SkullMesh,Point,FRotator(Bones.FRandRange(-12,8),-90*Sg+Bones.FRandRange(-25,25),Bones.FRandRange(-10,10)),1.f,false});
            ++Stakes;
        }
    }
    // The closed back: a hidden wall across the whole precinct, ground to above
    // every roof comb, behind which the jungle has taken the temple. Dressed below.
    Box(P.Block,BackFrom,BackTo,-ChultTemplePrecinctHalf-100,ChultTemplePrecinctHalf+100,-300,5200);
    // The rooms' insides, from their own stream (the growth below keeps its draws).
    TArray<FFire> Fires;TArray<FJarSpot> JarSpots;const TArray<FRoom> RoomList=Rooms();
    {
        FRandomStream Inside(2026101801);
        for(const FRoom& Rm:RoomList){Lining(P,Rm);Furnish(P,Inside,Rm,Fires,JarSpots);}
    }
    int32 Missing=0,Triangles=0;
    Emit(Owner,TEXT("ChultTempleMasonry"),{{&P.Stone,StoneMat},{&P.Moss,MossMat},{&P.Plaster,PlasterMat}},true,Missing,Triangles);
    Emit(Owner,TEXT("ChultTempleSteps"),{{&P.Steps,StoneMat},{&P.StepMoss,MossMat}},false,Missing,Triangles);
    auto* RampMesh=Emit(Owner,TEXT("ChultTempleStairRamp"),{{&P.Ramp,StoneMat}},true,Missing,Triangles);RampMesh->SetHiddenInGame(true);RampMesh->SetCastShadow(false);
    Emit(Owner,TEXT("ChultTempleDark"),{{&P.Dark,DarkMat}},false,Missing,Triangles);
    // The statues: frame rotations turned into the world (the frame's S axis is Along).
    int32 StatueCount=0;
    for(const FStatue& St:Statues)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(Owner);Mesh->SetupAttachment(Owner->GetRootComponent());
        UStaticMesh* Asset=LoadObject<UStaticMesh>(nullptr,St.Mesh);Missing+=!Asset;Mesh->SetStaticMesh(Asset);
        Mesh->SetWorldLocationAndRotation(G+St.Local,FRotator(St.Rotation.Pitch,St.Rotation.Yaw+Along.Rotation().Yaw,St.Rotation.Roll));
        Mesh->SetWorldScale3D(FVector(St.Scale));
        const bool Solid=St.Mesh!=SkullMesh;
        Mesh->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(Solid)Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        if(St.bIndoors)Mesh->SetLightingChannels(false,true,false);
        Mesh->ComponentTags.Add(TEXT("ChultTemple"));Mesh->ComponentTags.Add(TEXT("ChultTempleStatue"));Mesh->RegisterComponent();++StatueCount;
    }
    Emit(Owner,TEXT("ChultTempleVines"),{{&P.Vines,VineMat}},false,Missing,Triangles)->SetCastShadow(false);
    auto* Wall=Emit(Owner,TEXT("ChultTempleJungleWall"),{{&P.Block,StoneMat}},true,Missing,Triangles);
    Wall->SetHiddenInGame(true);Wall->SetCastShadow(false);Wall->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
    // Inside, only the torches light the stone (lighting channel 1); the sky is dimmed while the camera is in.
    for(auto* Inner:{Emit(Owner,TEXT("ChultTempleRooms"),{{&P.InStone,StoneMat},{&P.InMoss,MossMat},{&P.InPlaster,PlasterMat}},true,Missing,Triangles),
        Emit(Owner,TEXT("ChultTempleRoomsDark"),{{&P.InDark,DarkMat}},false,Missing,Triangles)})
        Inner->SetLightingChannels(false,true,false);
    TArray<TPair<TWeakObjectPtr<UPointLightComponent>,float>> RoomLights;
    for(const FFire& F:Fires)
    {
        AddDockFlame(Owner,G+At(F.Flame.X,F.Flame.Y,F.Flame.Z),F.Width,F.Height);
        auto* L=NewObject<UPointLightComponent>(Owner);L->SetupAttachment(Owner->GetRootComponent());L->SetMobility(EComponentMobility::Movable);
        L->SetWorldLocation(G+At(F.Light.X,F.Light.Y,F.Light.Z));L->SetIntensity(F.Intensity);L->SetAttenuationRadius(F.Radius);
        L->SetLightColor(FLinearColor(1.f,.56f,.27f));L->SetCastShadows(false);L->SetLightingChannels(false,true,false);L->RegisterComponent();
        L->ComponentTags.Add(TEXT("ChultTempleTorch"));RoomLights.Add({L,F.Intensity});
    }
    int32 TempleJars=0,TempleUrns=0;
    for(const FJarSpot& J:JarSpots)
        if(AClayJar* Jar=AClayJar::PlaceKind(Owner->GetWorld(),J.bUrn?AClayUrn::StaticClass():ATempleJar::StaticClass(),G+At(J.At.X,J.At.Y,J.At.Z+1),J.Yaw,J.Cigarettes))
        {Jar->SetIndoors();Jar->Tags.Add(TEXT("ChultTempleJar"));(J.bUrn?TempleUrns:TempleJars)++;}
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_ROOMS rooms=%d fires=%d jars=%d urns=%d"),RoomList.Num(),Fires.Num(),TempleJars,TempleUrns);
    // While the camera is in a room: the sky dims, Chuck joins the rooms' light (the sewer's pattern), the fires flicker.
    {
        struct FInside{float Blend=0;bool bChuckIn=false;TArray<TPair<TWeakObjectPtr<USkyLightComponent>,float>> Skies;bool bFound=false;};
        auto State=MakeShared<FInside>();TWeakObjectPtr<AActor> Weak(Owner);
        FTimerHandle Timer;
        Owner->GetWorldTimerManager().SetTimer(Timer,[Weak,State,RoomLights]()
        {
            if(!Weak.IsValid())return;UWorld* World=Weak->GetWorld();
            if(!State->bFound){TArray<USkyLightComponent*> Found;Weak->GetComponents(Found);for(auto* K:Found)State->Skies.Add({K,K->Intensity});State->bFound=true;}
            auto* PC=World->GetFirstPlayerController();if(!PC||!PC->PlayerCameraManager)return;
            const bool In=InRoom(PC->PlayerCameraManager->GetCameraLocation(),40);
            const float Was=State->Blend;State->Blend=FMath::Clamp(State->Blend+(In?.125f:-.125f),0.f,1.f);
            if(State->Blend!=Was)for(const auto& K:State->Skies)if(K.Key.IsValid())K.Key->SetIntensity(K.Value*(1-.85f*State->Blend));
            if(APawn* Pawn=PC->GetPawn())if((State->Blend>.5f)!=State->bChuckIn)
            {
                State->bChuckIn=State->Blend>.5f;TArray<UPrimitiveComponent*> Parts;Pawn->GetComponents(Parts);
                for(auto* Part:Parts)Part->SetLightingChannels(!State->bChuckIn,State->bChuckIn,false);
            }
            const float Time=World->GetTimeSeconds();int32 I=0;
            for(const auto& L:RoomLights)
            {
                const float Ph=I++*1.37f;
                if(L.Key.IsValid())L.Key->SetIntensity(L.Value*(.93f+.06f*FMath::Sin(Time*6.3f+Ph)+.04f*FMath::Sin(Time*10.7f+Ph*1.7f)));
            }
        },.05f,true);
    }

    // Growth. The jungle is taking the precinct back: trees on the plaza and
    // its sides, thickening outward, the old road kept clear up the middle;
    // ferns and shrubs on every ledge, heavier toward the back; and behind the
    // temple a wall of growth.
    TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;int32 Plants=0,Trees=0;
    auto Place=[&](FString Name,FVector Local,float Scale,float Yaw,bool Solid=false,bool Hidden=false)
    {
        auto*& H=Sets.FindOrAdd(Name);if(!H)
        {
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,*FString::Printf(TEXT("ChultTemple_%s"),*Name));
            const TCHAR* Dir=Name.StartsWith(TEXT("SM_Chult"))?TEXT("/Game/Art/ChultNature"):TEXT("/Game/Art/Cutscenes/FallToChult");
            H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),Dir,*Name,*Name)));Missing+=!H->GetStaticMesh();
            H->SetupAttachment(Owner->GetRootComponent());H->SetWorldLocation(G);H->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
            if(Solid)H->SetCollisionProfileName(TEXT("BlockAll"));H->SetHiddenInGame(Hidden);H->SetCastShadow(!Hidden);
            // The tall trees stand over the whole precinct: seen from the avenue's start.
            const bool Tall=Name.StartsWith(TEXT("SM_FallTree"));
            H->SetCullDistances(Tall?30000:Solid?12000:9000,Tall?42000:Solid?20000:16000);H->RegisterComponent();
        }
        H->AddInstance(FTransform(FRotator(0,Yaw,0),G+Local,FVector(Scale)),true);
    };
    // Suppress keeps a spot bare without changing any draw (the shrines' doorways, 2026-10-18).
    bool Suppress=false;
    auto Plant=[&](const FVector& L,int32 Type,float Scale,float Yaw)
    {
        if(Suppress)return;
        Place(Type<4?FString::Printf(TEXT("SM_ChultFern%d"),Type):Type<6?TEXT("SM_FallElephantEar"):TEXT("SM_FallShrub"),L,Scale*(Type<4?1:.65f),Yaw);++Plants;
    };
    auto Tree=[&](const FVector& L,float Scale,float Yaw,bool Solid)
    {
        if(Suppress)return;
        Place(TEXT("SM_ChultIslandTree"),L,Scale,Yaw);if(Solid)Place(TEXT("SM_ChultIslandTrunkCollision"),L,Scale,Yaw,true,true);++Trees;
    };
    // Ledges: each terrace's top between its edge and the next terrace in.
    struct FLedge{float Off,HS,HT,InS,InT,Z,SkipT;bool Summit;};
    TArray<FLedge> Ledges;
    for(float Side:{-1.f,1.f})for(int32 I=0;I<Tiers;++I)
        Ledges.Add({Side*PyramidT,TierHalf(I)-Band,TierHalf(I)-Band,I+1<Tiers?TierHalf(I+1)+30:0,I+1<Tiers?TierHalf(I+1)+30:0,(I+1)*Tier,BalusterOut+80,I+1==Tiers});
    for(int32 I=0;I<Levels;++I)
        Ledges.Add({0,LevelS[I]-Band,FMath::Min(LevelT[I]-Band,PyramidT-Base),I+1<Levels?LevelS[I+1]+30:0,I+1<Levels?LevelT[I+1]+30:0,(I+1)*Tier,CBalusterOut+80,I+1==Levels});
    for(const FLedge& L:Ledges)
    {
        OffT=L.Off;const int32 Count=FMath::RoundToInt(L.HS*L.HT/2600);
        for(int32 I=0;I<Count;++I)
        {
            const float S=R.FRandRange(-L.HS+25,L.HS-25),T=R.FRandRange(-L.HT+25,L.HT-25);
            if(FMath::Abs(S)<L.InS&&FMath::Abs(T)<L.InT)continue;
            if(S<0&&FMath::Abs(T)<L.SkipT)continue;
            const bool Back=S>0,Rear=Sc+S>BackFrom-300;
            // The fronts stay walkable and lighter; the backs are overgrown.
            if(!Back&&R.FRand()<.45f)continue;
            if(L.Summit)
            {
                if(L.Off!=0&&Sc+S>HouseFront-150&&Sc+S<HouseBack+150&&FMath::Abs(T)<HouseHalf+150)continue;
                if(L.Off==0&&(FMath::Max(FMath::Abs(S),FMath::Abs(T))<TowerHalf+150||(S<0&&FMath::Abs(T)<DoorHalf+250)))continue;
                if(L.Off==0&&FMath::Abs(FMath::Abs(S)-(LevelS[2]-240))<260&&FMath::Abs(FMath::Abs(T)-(LevelT[2]-240))<260)continue;
                if(!Back&&R.FRand()<.6f)continue;
            }
            if(L.Off==0&&L.Z==2*Tier&&FMath::Abs(FMath::Abs(S)-(LevelS[1]-330))<230&&FMath::Abs(FMath::Abs(T)-(LevelT[1]-330))<230)continue;
            // Before each top-terrace shrine's door (its steps and the way to them),
            // and round every room, whose leaves would poke through its walls.
            {
                const float DS=FMath::Abs(FMath::Abs(S)-(LevelS[2]-240)),DT=FMath::Abs(FMath::Abs(T)-(LevelT[2]-240));
                Suppress=L.Off==0&&L.Summit&&((DS<200&&FMath::Abs(T)>LevelT[2]-240-700&&FMath::Abs(T)<LevelT[2]-240)||FMath::Max(DS,DT)<520);
                Suppress|=L.Off!=0&&L.Summit&&Sc+S>HouseFront-400&&Sc+S<HouseBack+400&&FMath::Abs(T)<HouseHalf+400;
            }
            const FVector At_=At(Sc+S,T,L.Z-2);
            if(Rear&&R.FRand()<.06f){Tree(At_,R.FRandRange(.7f,1.1f),R.FRandRange(0,360),false);continue;}
            Plant(At_,R.RandRange(0,6),R.FRandRange(1.2f,Back?2.8f:2.f),R.FRandRange(0,360));
        }
    }
    OffT=0;Suppress=false;
    // A few trees clinging to the pyramids' outer upper corners and the central temple's back corners.
    for(float Side:{-1.f,1.f})for(int32 Tr:{1,2})
    {
        const float H=TierHalf(Tr)-Band-120;
        Tree(Flat(Sc+H,Side*(PyramidT+H),Tr*Tier+Tier-4),R.FRandRange(.8f,1.1f),R.FRandRange(0,360),false);
        if(Tr==1)Tree(Flat(Sc-H,Side*(PyramidT+H),Tr*Tier+Tier-4),R.FRandRange(.7f,.9f),R.FRandRange(0,360),false);
    }
    for(float Side:{-1.f,1.f})Tree(Flat(Sc+LevelS[1]-250,Side*(LevelT[1]-250),2*Tier-4),R.FRandRange(.9f,1.2f),R.FRandRange(0,360),false);
    // The plaza and the precinct's sides: trees and growth thickening outward from the road.
    for(int32 I=0;I<16000;++I)
    {
        const float S=R.FRandRange(ChultTemplePrecinctFrom,BackFrom),T=R.FRandRange(-ChultTemplePrecinctHalf+100,ChultTemplePrecinctHalf-100),A=FMath::Abs(T);
        if(S<PlazaFrom&&A<6400)continue;                                 // the deep jungle's own (ChultUpriver.cpp)
        if(OnStructure(S,T,30))continue;
        if(S<CStairFoot&&A<800)continue;                                // the road
        bool Clear=true;for(float St:StelaS)Clear&=FVector2D(S-St,A-1500).Size()>260;
        const FVector Ground=Flat(S,T,ChultCanyonGroundZ(ChultCanyonFrame(S,T))-3-PlazaZ());
        Suppress=FMath::Abs(S-GateS)<580&&FMath::Abs(A-GateT)<580;   // the gate pavilions' rooms
        // The carved serpent heads' tongues reach 3.3 m out from the balustrades' feet.
        for(const TPair<const FStair*,float>& K:TArray<TPair<const FStair*,float>>{{&CentralStair,0.f},{&PyramidStair,PyramidT}})
            Suppress|=S>K.Key->Foot-420&&S<K.Key->Foot&&FMath::Abs(FMath::Abs(A-K.Value)-(K.Key->Half+K.Key->Out)*.5f)<120;
        // Wilder toward the sides and the back; the ways in stay open.
        const float Wild=FMath::Max(Ease((A-1600)/5500),Ease((S-76500)/2500)*.6f);
        if(I%4==0&&Clear&&!OnWayIn(S,T,250)&&A>1500&&!OnStructure(S,T,350)&&R.FRand()<.08f+.7f*Wild)
        {
            Tree(Ground,R.FRandRange(1.8f,3.f),R.FRandRange(0,360),true);
            if(I%12==0)Place(TEXT("SM_FallPalm"),Ground+(Across*120),R.FRandRange(1.3f,1.9f),R.FRandRange(0,360));
            continue;
        }
        if(!Clear||R.FRand()>.55f+.45f*Ease((A-800)/2500))continue;
        Plant(Ground,I%7,R.FRandRange(1.4f,2.8f),R.FRandRange(0,360));
    }
    Suppress=false;
    // Behind the temple the jungle closes: a dense band of trees and thick
    // growth along the hidden wall and up against the masonry's backs.
    for(int32 I=0;I<9000;++I)
    {
        const float S=R.FRandRange(BackFrom-500,BackTo+1500),T=R.FRandRange(-ChultTemplePrecinctHalf+100,ChultTemplePrecinctHalf-100);
        if(OnStructure(S,T,-40))continue;
        const FVector Ground=Flat(S,T,ChultCanyonGroundZ(ChultCanyonFrame(S,T))-3-PlazaZ());
        if(I%4==0&&!OnStructure(S,T,250)){Tree(Ground,R.FRandRange(2.f,3.2f),R.FRandRange(0,360),false);if(I%16==0)Place(TEXT("SM_FallPalm"),Ground+Across*150,R.FRandRange(1.4f,2.f),R.FRandRange(0,360));continue;}
        Plant(Ground,4+I%3,R.FRandRange(2.f,3.4f),R.FRandRange(0,360));
    }
    // The landing's tall trees (SM_FallTree_A/B, about 27 m at scale 1, 2026-10-17),
    // from their own stream so the growth above keeps its draws: a ring of
    // giants round the precinct, a few out on the plaza, more towering
    // behind the temple and some rooted on its back terraces. Where he can
    // reach them their trunks are solid, as at the landing.
    int32 TallTrees=0;
    {
        FRandomStream Tall(2026101701);
        auto TallTree=[&](float S,float T,float Z,float Scale,bool Solid)
        {
            const bool A=Tall.FRand()<.5f;const float Yaw=Tall.FRandRange(0,360);
            Place(A?TEXT("SM_FallTree_A"):TEXT("SM_FallTree_B"),Flat(S,T,Z-5),Scale,Yaw);++TallTrees;
            if(!Solid)return;
            const float Radius=(A?78:62)*Scale,Height=2700*Scale;
            auto* C=NewObject<UCapsuleComponent>(Owner);C->SetupAttachment(Owner->GetRootComponent());
            C->InitCapsuleSize(Radius,Height*.5f);C->SetCollisionProfileName(TEXT("BlockAll"));C->SetHiddenInGame(true);
            C->SetWorldLocation(G+Flat(S,T,Z+Height*.5f-Radius));C->RegisterComponent();C->ComponentTags.Add(TEXT("ChultTemple"));
        };
        auto GroundAt=[&](float S,float T){return ChultCanyonGroundZ(ChultCanyonFrame(S,T))-PlazaZ();};
        // The ring: both sides of the precinct, from the jungle's edge to the back.
        for(float S=PlazaFrom-1200;S<BackFrom+200;S+=Tall.FRandRange(900,1500))for(float Side:{-1.f,1.f})
        {
            const float T=Side*Tall.FRandRange(7900,10100);
            if(OnStructure(S,T,500))continue;
            TallTree(S,T,GroundAt(S,T),Tall.FRandRange(1.f,1.55f),true);
        }
        // Out on the plaza: scattered giants clear of the ways in, the avenue's stakes and stelae.
        for(int32 I=0,Made=0;I<200&&Made<14;++I)
        {
            const float S=Tall.FRandRange(65500,76500),A=Tall.FRandRange(2200,7600),T=A*(Tall.FRand()<.5f?-1:1);
            if(OnStructure(S,T,500)||OnWayIn(S,T,450))continue;
            bool Clear=true;for(float St:StelaS)Clear&=FVector2D(S-St,A-1500).Size()>700;if(!Clear)continue;
            TallTree(S,T,GroundAt(S,T),Tall.FRandRange(.95f,1.4f),true);++Made;
        }
        // Behind the temple: the tallest, rising over its back and between its towers.
        for(int32 I=0;I<34;++I)
        {
            const float S=Tall.FRandRange(BackFrom+300,BackTo+2500),T=Tall.FRandRange(-ChultTemplePrecinctHalf+300,ChultTemplePrecinctHalf-300);
            if(OnStructure(S,T,200))continue;
            TallTree(S,T,GroundAt(S,T),Tall.FRandRange(1.25f,1.7f),false);
        }
        // Rooted on the back terraces: two on each pyramid, two on the central temple.
        for(float Side:{-1.f,1.f})
        {
            for(int32 Tr:{0,1})
            {
                const float H=TierHalf(Tr)-Band-110,In=Tr+1<Tiers?TierHalf(Tr+1):0;
                const float S=Sc+(H+In)*.5f,T=Side*(PyramidT+Tall.FRandRange(-.6f,.6f)*H);
                TallTree(S,T,(Tr+1)*Tier,Tall.FRandRange(.7f,.9f),false);
            }
            TallTree(Sc+(LevelS[1]-Band+LevelS[2])*.5f,Side*Tall.FRandRange(600,1500),2*Tier,Tall.FRandRange(.65f,.8f),true);
        }
    }
    // Mist pooled round the temple's foot.
    for(const FVector& L:{Flat(Sc-Base-700,-PyramidT-1800,150),Flat(Sc-Base-700,PyramidT+1800,150),Flat(Sc+Base+800,0,150),Flat(Sc+Base+600,-8000,150),Flat(Sc+Base+600,8000,150)})
    {
        auto* Fog=NewObject<ULocalFogVolumeComponent>(Owner);Fog->SetupAttachment(Owner->GetRootComponent());Fog->SetWorldLocation(G+L);Fog->SetWorldScale3D(FVector(8));
        Fog->RadialFogExtinction=.05f;Fog->HeightFogExtinction=.05f;Fog->HeightFogFalloff=700;Fog->HeightFogOffset=.25f;Fog->FogAlbedo=FLinearColor(.6,.7,.66);Fog->RegisterComponent();
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE tiers=%d pyramids=2 levels=%d towers=%d height_cm=%.0f steps=%d stakes=%d serpents=6 stelae=%d fragments=%d vines=%d plants=%d trees=%d tall_trees=%d statues=%d missing_assets=%d triangles=%d build_ms=%.1f"),
        Tiers,Levels,Towers,CTop+TowerBody+42+TowerSpire*.9f+TowerSpire*.05f,Steps,Stakes,Stelae,Fragments,Strand,Plants,Trees,TallTrees,StatueCount,Missing,Triangles,(FPlatformTime::Seconds()-Started)*1000);
}

int32 CheckChultTemple(AActor* Owner,AChuckCharacter* Chuck)
{
    int32 Failures=0,StairProbes=0,Plaza=0,Sweeps=0;auto* World=Owner->GetWorld();const FVector G=AChultLanding::FloorOrigin();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultTemple),false,Chuck);
    const float Half=Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const auto Capsule=FCollisionShape::MakeCapsule(Radius,Half);
    auto Fail=[&](const TCHAR* What,const FVector& L){++Failures;if(Failures<20)UE_LOG(LogTemp,Warning,TEXT("CHUCK_TEMPLE_FAIL %s at=%s"),What,*L.ToString());};
    // The plaza is level where the ground function says.
    for(float S=PlazaFrom+200;S<Sc-Base-200;S+=500)for(float T=-8400;T<=8400;T+=1200)
    {
        const FVector P=ChultCanyonFrame(S,T);++Plaza;
        if(FMath::Abs(ChultCanyonGroundZ(P)-PlazaZ())>.5f)Fail(TEXT("plaza_not_level"),P);
    }
    // Each stair's walking surface follows its treads and is open to climb.
    auto CheckStair=[&](const FStair& St,float Off)
    {
        FVector Previous=FVector::ZeroVector;bool First=true;
        for(float S=St.Foot-100;S<=St.TopS+150;S+=40)
        {
            FHitResult Hit;const FVector P=Flat(S,Off,0);const float Want=S<St.Foot?0.f:RampZ(St,S);
            const bool Found=World->LineTraceSingleByChannel(Hit,G+P+FVector(0,0,Want+150),G+P+FVector(0,0,Want-60),ECC_Visibility,Q);
            if(!Found||FMath::Abs(Hit.ImpactPoint.Z-G.Z-PlazaZ()-Want)>10||Hit.ImpactNormal.Z<.75f)
                Fail(TEXT("stair_floor"),FVector(S,Off,Found?Hit.ImpactPoint.Z-G.Z-PlazaZ():-1.f));
            // On the 39 degree slope the capsule's round foot needs ~10 cm more than on the flat.
            const FVector Body=G+P+FVector(0,0,(Found?Hit.ImpactPoint.Z-G.Z-P.Z:Want)+Half+16);
            if(!First&&World->SweepSingleByChannel(Hit,Previous,Body,FQuat::Identity,ECC_Pawn,Capsule,Q))
            {Fail(TEXT("stair_blocked"),FVector(S,Off,Body.Z-G.Z-PlazaZ()));UE_LOG(LogTemp,Warning,TEXT("CHUCK_TEMPLE_BLOCKED_BY %s"),*GetNameSafe(Hit.GetComponent()));}
            Previous=Body;First=false;++StairProbes;
        }
    };
    CheckStair(CentralStair,0);for(float Side:{-1.f,1.f})CheckStair(PyramidStair,Side*PyramidT);
    auto Open=[&](const TCHAR* What,const FVector& A,const FVector& B)
    {
        FHitResult Hit;++Sweeps;
        if(World->SweepSingleByChannel(Hit,G+A,G+B,FQuat::Identity,ECC_Pawn,Capsule,Q)){Fail(What,Hit.ImpactPoint-G);UE_LOG(LogTemp,Warning,TEXT("CHUCK_TEMPLE_BLOCKED_BY %s"),*GetNameSafe(Hit.GetComponent()));}
    };
    auto Room=[&](const TCHAR* What,const FVector& A){++Sweeps;if(World->OverlapBlockingTestByChannel(G+A,FQuat::Identity,ECC_Pawn,Capsule,Q))Fail(What,A);};
    // Into each doorway: room to stand in the vestibule, and the way to it open.
    Room(TEXT("sanctuary_blocked"),Flat(TowerFront+60+CellDepth*.5f,0,CTop+Half+3));
    Open(TEXT("sanctuary_door_blocked"),Flat(CStairTop+60,0,CTop+Half+4),Flat(TowerFront+60+CellDepth*.5f,0,CTop+Half+4));
    for(float Side:{-1.f,1.f})
    {
        Room(TEXT("vestibule_blocked"),Flat(HouseFront+150,Side*PyramidT,Top+Half+3));
        Open(TEXT("door_blocked"),Flat(StairTop+60,Side*PyramidT,Top+Half+4),Flat(HouseFront+150,Side*PyramidT,Top+Half+4));
        // The joined temple: along the central temple's first ledge onto the pyramid's.
        Open(TEXT("ledge_join_blocked"),Flat(Sc-LevelS[0]+Band+150,Side*(CBalusterOut+250),Tier+Half+4),Flat(Sc-LevelS[0]+Band+150,Side*(PyramidT-BalusterOut-200),Tier+Half+4));
        // The plaza to each pyramid's stair foot.
        Open(TEXT("pyramid_approach_blocked"),Flat(72000,Side*1000,Half+3),Flat(StairFoot-320,Side*PyramidT,Half+3));
    }
    // The road up the avenue to the central stair is open.
    Open(TEXT("avenue_blocked"),Flat(SpawnS,0,Half+3),Flat(CStairFoot-120,0,Half+3));
    // The back is closed: on the ground at both sides and the middle, and along the ledges.
    for(float T:{-9500.f,-PyramidT-Base-400,0.f,PyramidT+Base+400,9500.f})
    {
        FHitResult Hit;++Sweeps;const float Z=FMath::Abs(T)<100?CTop:0;const float From=FMath::Abs(T)<100?Sc+LevelS[2]-150:Sc;
        if(!World->SweepSingleByChannel(Hit,G+Flat(From,T,Z+Half+20),G+Flat(BackTo+600,T,Z+Half+20),FQuat::Identity,ECC_Pawn,Capsule,Q))Fail(TEXT("back_open"),Flat(From,T,Z));
    }
    // The camera cannot slip inside the masonry (2026-10-17): from the platform
    // into each balustrade's top end, and from below up into the summit
    // houses' overhanging roofs. Inside, every face is culled and the stone vanishes.
    {
        const auto Probe=FCollisionShape::MakeSphere(12);
        auto Shut=[&](const TCHAR* What,const FVector& A,const FVector& B){FHitResult Hit;++Sweeps;if(!World->SweepSingleByChannel(Hit,G+A,G+B,FQuat::Identity,ECC_Camera,Probe,Q))Fail(What,A);};
        for(const TPair<const FStair*,float>& S:TArray<TPair<const FStair*,float>>{{&CentralStair,0.f},{&PyramidStair,-PyramidT},{&PyramidStair,PyramidT}})
            for(float Sg:{-1.f,1.f})
            {
                const FStair& St=*S.Key;const float T=S.Value+Sg*(St.Half+St.Out)*.5f;
                Shut(TEXT("balustrade_end_open"),Flat(St.TopS+150,T,St.Height+40),Flat(St.TopS-50,T,St.Height+40));
            }
        for(float Side:{-1.f,1.f})Shut(TEXT("roof_open_below"),Flat(HouseFront-30,Side*PyramidT+300,Top+WallTop-60),Flat(HouseFront-30,Side*PyramidT+300,Top+WallTop+40));
    }
    // The rooms (2026-10-18): room to stand inside, the doorway open from the
    // threshold, the prasats' steps walkable, the jars and urns all there and
    // whole; one of each new pot broken on the plaza to see it shatter.
    int32 RoomFailures=0,JarCount=0,UrnCount=0,ShardsOk=0;
    {
        const int32 Before=Failures;
        for(const FRoom& Rm:Rooms())
        {
            const FVector Mid=RL(Rm,-Rm.HD*.35f,0,Half+4);
            Room(TEXT("room_blocked"),Flat(Mid.X,Mid.Y,Mid.Z));
            const float Out=Rm.Kind==ERoom::Summit?HouseWall+90:CellaWall+30;
            const FVector A=RL(Rm,-Rm.HD-Out,0,Half+4);
            Open(TEXT("room_door_blocked"),Flat(A.X,A.Y,A.Z),Flat(Mid.X,Mid.Y,Mid.Z));
            if(Rm.Kind==ERoom::Summit)continue;
            // The steps' hidden ramp, through the treads' middles, from the ground to the plinth.
            const float Face=Rm.HD+CellaWall+35;
            for(float O:{95.f,75.f,45.f,15.f})
            {
                const FVector At_=RL(Rm,-Face-O,0,0),Want=At_+FVector(0,0,FMath::Clamp(45*(105-O)/90,0.f,45.f)-45);
                FHitResult Hit;const FVector W=G+Flat(Want.X,Want.Y,Want.Z);
                const bool Found=World->LineTraceSingleByChannel(Hit,W+FVector(0,0,60),W-FVector(0,0,30),ECC_Visibility,Q);
                if(!Found||FMath::Abs(Hit.ImpactPoint.Z-W.Z)>4||Hit.ImpactNormal.Z<.85f)Fail(TEXT("room_steps"),Want);
                ++Sweeps;
            }
        }
        for(const TWeakObjectPtr<AChuckBreakable>& B:AChuckBreakable::All())
            if(B.IsValid()&&B->ActorHasTag(TEXT("ChultTempleJar"))&&!B->IsBroken())(B->IsA<AClayUrn>()?UrnCount:JarCount)++;
        if(JarCount!=30||UrnCount!=10)Fail(TEXT("room_jars"),FVector(JarCount,UrnCount,0));
        for(const TPair<UClass*,int32>& K:TArray<TPair<UClass*,int32>>{{ATempleJar::StaticClass(),12},{AClayUrn::StaticClass(),19}})
        {
            AClayJar* Pot=AClayJar::PlaceKind(World,K.Key,G+Flat(69000,-7000-K.Value*20,0),0,2);
            if(!Pot){Fail(TEXT("room_pot_spawn"),FVector::ZeroVector);continue;}
            Pot->Break(FVector(1,0,0));
            if(Pot->GetShardsFlying()==K.Value&&Pot->PlayedBreakSound()&&Pot->Cigarettes==0)++ShardsOk;else Fail(TEXT("room_pot_break"),FVector(K.Value,Pot->GetShardsFlying(),Pot->PlayedBreakSound()));
        }
        RoomFailures=Failures-Before;
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_ROOMS_CHECK failures=%d rooms=%d jars=%d urns=%d breaks=%d"),RoomFailures,Rooms().Num(),JarCount,UrnCount,ShardsOk);
    // The spawn: on the floor with room to stand.
    {
        FHitResult Hit;const FVector P=ChultTempleSpawn();
        const bool Floor=World->LineTraceSingleByChannel(Hit,P,P-FVector(0,0,Half+40),ECC_Visibility,Q);
        if(!Floor||FMath::Abs(Hit.ImpactPoint.Z-(P.Z-34.65f))>6||World->OverlapBlockingTestByChannel(P+FVector(0,0,2),FQuat::Identity,ECC_Pawn,Capsule,Q))Fail(TEXT("spawn"),P-G);
        Failures+=!AChultLanding::Contains(P);
        // The precinct's widened floor counts as the far bank's.
        Failures+=!AChultLanding::Contains(G+Flat(70000,9800,0));
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_CHECK failures=%d plaza_probes=%d stair_probes=%d stairs=3 sweeps=%d spawn=1"),Failures,Plaza,StairProbes,Sweeps);
    return Failures;
}

void ReviewChultTemple(AActor* Owner,AChuckCharacter* Chuck,int32 Failures)
{
    // A point in room I: X as a fraction of its half depth (from the door, -1, to the back, +1), Y across, Z above its floor.
    auto RoomEye=[](int32 I,float X,float Y,float Z){const FRoom Rm=Rooms()[I];const FVector V=RL(Rm,X*Rm.HD,Y,Z);return Flat(V.X,V.Y,V.Z);};
    auto* Camera=Owner->GetWorld()->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(72);
    Chuck->SetActorHiddenInGame(true);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Camera);
    const TArray<FVector> Eyes={Flat(SpawnS-300,0,180),Flat(70500,-500,140),Flat(CStairFoot-650,-900,90),Flat(CStairTop+200,-500,CTop+170),
        Flat(Sc-Base-6500,-1500,2600),Flat(69300,-150,95),Flat(Sc-LevelS[0]+Band+150,-900,Tier+120),Flat(Sc+LevelS[2]-200,600,CTop+180),Flat(64300,0,260),
        // 2026-10-17: a central balustrade's top end from the platform, a pyramid stair's side from its top, up under a summit house's roof.
        Flat(CStairTop+260,(CStairHalf+CBalusterOut)*.5f+90,CTop+70),Flat(StairTop+180,PyramidT-BalusterOut-260,Top+120),Flat(HouseFront-260,-PyramidT-420,Top+45),
        // 2026-10-18: inside a summit chamber, a gate pavilion and a shrine; a gate's open door from the plaza.
        RoomEye(0,-.85f,-200,150),RoomEye(2,-.7f,110,120),RoomEye(5,-.75f,60,115),Flat(GateS-900,-GateT+420,150),RoomEye(0,-.45f,-330,75),
        // The modelled statues, close.
        Flat(StakeS[1]-170,-StakeT+150,165),Flat(CStairFoot-560,-(CStairHalf+CBalusterOut)*.5f-330,140),RoomEye(0,.4f,-90,135)};
    const TArray<FVector> Aims={Flat(Sc,0,1600),Flat(Sc,0,1500),Flat(CStairFoot-100,-460,80),Flat(TowerFront,0,CTop+200),
        Flat(Sc,0,1500),Flat(68300,-StakeT,180),Flat(Sc-LevelS[0],-PyramidT,Tier+200),Flat(BackTo,3500,200),Flat(Sc,0,1800),
        Flat(CStairTop-300,(CStairHalf+CBalusterOut)*.5f,CTop-120),Flat(StairTop-700,PyramidT-StairHalf-60,Top-420),Flat(HouseFront+40,-PyramidT-150,Top+WallTop+110),
        RoomEye(0,.79f,60,90),RoomEye(2,.6f,-20,80),RoomEye(5,.77f,-10,70),Flat(GateS-260,-GateT,110),RoomEye(0,-.88f,-500,12),
        Flat(StakeS[1],-StakeT,178),Flat(CStairFoot-200,-(CStairHalf+CBalusterOut)*.5f,75),RoomEye(0,.79f,0,140)};
    const TArray<FString> Names={TEXT("TempleAvenue"),TEXT("TemplePlaza"),TEXT("TempleSerpent"),TEXT("TempleDoorway"),TEXT("TempleOverview"),
        TEXT("TempleSkullStake"),TEXT("TempleLedgeJoin"),TEXT("TempleBackJungle"),TEXT("TempleApproach"),
        TEXT("TempleStairSideCentral"),TEXT("TempleStairSidePyramid"),TEXT("TempleRoofBelow"),
        TEXT("TempleRoomSummit"),TEXT("TempleRoomGate"),TEXT("TempleRoomShrine"),TEXT("TempleGateDoor"),TEXT("TempleRoomJars"),
        TEXT("TempleSkullClose"),TEXT("TempleSerpentClose"),TEXT("TempleIdolClose")};
    struct FRun{int32 Phase=0,Walk=0,Failures=0,Shot=0;double Next=0,ShotNext=0,Start=-1,RunStart=-1,MusicAt=-1,RevealAt=-1;bool bDone=false,bRan=false;TWeakObjectPtr<ACameraActor> Camera;FTimerHandle Timer;};
    auto Run=MakeShared<FRun>();Run->Failures=Failures;Run->Camera=Camera;
    // One timer throughout: views every 2.5 s, then the real character's climb.
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,Chuck,Run,Eyes,Aims,Names]()
    {
        if(Run->bDone)return;
        const FVector G=AChultLanding::FloorOrigin();const double Now=Owner->GetWorld()->GetTimeSeconds();
        const int32 I=Run->Phase/2;
        if(I<Eyes.Num())
        {
            if(Now<Run->Next)return;Run->Next=Now+2.5;
            if(Run->Phase%2==0){if(Run->Camera.IsValid())Run->Camera->SetActorLocationAndRotation(G+Eyes[I],(Aims[I]-Eyes[I]).Rotation());}
            else{FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult")/(Names[I]+TEXT(".png")),true,false);UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_VIEW name=%s"),*Names[I]);}
            ++Run->Phase;return;
        }
        auto* M=Chuck->GetCharacterMovement();
        if(!Run->bRan)
        {
            // The approach: the real character runs up the road from before the
            // score's start to past the reveal; the score's swell should come
            // as he reaches the reveal (ChultTempleMusicSwell after its start).
            if(Now<Run->Next)return;
            const float From=ChultTempleMusicFromS()-1200;
            if(Run->RunStart<0)
            {
                Chuck->SetActorHiddenInGame(false);
                const bool Shots=FParse::Param(FCommandLine::Get(),TEXT("ChuckTempleApproachShots"));
                if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Shots&&Run->Camera.IsValid()?static_cast<AActor*>(Run->Camera.Get()):Chuck);
                if(!Shots&&Run->Camera.IsValid())Run->Camera->Destroy();
                const FVector Foot=ChultCanyonFrame(From,ChultUpriverRoadT(From));
                Chuck->ResetAtLocation(G+Foot+FVector(0,0,ChultCanyonGroundZ(Foot)-Foot.Z+40));
                Chuck->SetActorRotation(FRotator(0,(ChultCanyonFrame(From+500,ChultUpriverRoadT(From+500))-Foot).Rotation().Yaw,0));
                Chuck->SetOrbitPitch(-8);Chuck->SetRunHeld(true);Run->RunStart=Now;Run->Next=Now;return;
            }
            const FVector F=Frame(Chuck->GetActorLocation()-G);const float RunTime=Now-Run->RunStart;
            auto* Mode=Owner->GetWorld()->GetAuthGameMode<ADockGameMode>();
            if(Run->MusicAt<0&&Mode&&Mode->GetMusicRegion()==5){Run->MusicAt=Now;UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_APPROACH music_start s=%.0f seconds=%.2f"),F.X,RunTime);}
            if(Run->RevealAt<0&&F.X>=ChultTempleRevealS()){Run->RevealAt=Now;UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_APPROACH reveal s=%.0f seconds=%.2f"),F.X,RunTime);}
            if(Run->Camera.IsValid())
            {
                // A player's eye on the run: 3 m behind him, 1.4 m above, looking up the road.
                const FVector Dir=Chuck->GetVelocity().GetSafeNormal2D(),Eye=Chuck->GetActorLocation()-(Dir.IsNearlyZero()?Along:Dir)*300+FVector(0,0,140);
                Run->Camera->SetActorLocationAndRotation(Eye,FRotator(-4,(Dir.IsNearlyZero()?Along:Dir).Rotation().Yaw,0));
            }
            if(Run->Camera.IsValid()&&Now>=Run->ShotNext)
            {
                Run->ShotNext=Now+.5;const FString Name=FString::Printf(TEXT("TempleRun_%02d"),Run->Shot++);
                FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult")/(Name+TEXT(".png")),true,false);
                UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_RUN_SHOT name=%s s=%.0f t=%.0f seconds=%.2f music=%d speed=%.0f max=%.0f"),*Name,F.X,F.Y,RunTime,Mode?Mode->GetMusicRegion():-1,Chuck->GetVelocity().Size2D(),M->MaxWalkSpeed);
            }
            if(F.X<ChultTempleRevealS()+300&&RunTime<60)
            {
                const FVector Ahead=G+ChultCanyonFrame(F.X+500,ChultUpriverRoadT(F.X+500));
                const FVector D=(Ahead-Chuck->GetActorLocation()).GetSafeNormal2D();Chuck->SetRunHeld(true);Chuck->SetTestStickWorld(D);Chuck->AddMovementInput(D,1,true);return;
            }
            const float Lead=Run->MusicAt>0&&Run->RevealAt>0?Run->RevealAt-Run->MusicAt:-1;
            const bool Timed=FMath::Abs(Lead-ChultTempleMusicSwell)<1.f;Run->Failures+=!Timed;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_APPROACH failures=%d lead=%.2f swell=%.2f music_from=%.0f reveal=%.0f"),!Timed,Lead,ChultTempleMusicSwell,ChultTempleMusicFromS(),ChultTempleRevealS());
            Chuck->SetTestStick(FVector2D::ZeroVector);Run->bRan=true;Run->Next=Now+.5;
            if(Run->Camera.IsValid())Run->Camera->Destroy();if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Chuck);return;
        }
        if(Run->Start<0)
        {
            if(Now<Run->Next)return;
            // The real character: from the plaza up the central stair into the sanctuary's doorway.
            Chuck->SetActorHiddenInGame(false);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Chuck);
            if(Run->Camera.IsValid())Run->Camera->Destroy();
            Chuck->ResetAtLocation(G+Flat(72600,0,34.65f));Chuck->SetActorRotation(FRotator(0,ChultTempleSpawnYaw(),0));Chuck->SetRunHeld(true);
            Run->Start=Now;return;
        }
        // 2026-10-18: then back out, across the top terrace and up the steps into a shrine.
        const FVector Shrine=RL(Rooms()[5],0,0,0),Steps=RL(Rooms()[5],-Rooms()[5].HD-CellaWall-35-160,0,0);
        const TArray<FVector> Way={Flat(CStairFoot-200,0,0),Flat(CStairTop+100,0,CTop),Flat(TowerFront+170,0,CTop),Flat(TowerFront-150,0,CTop),
            Flat(Steps.X,Steps.Y,CTop),Flat(Shrine.X,Shrine.Y,Shrine.Z)};
        const float Time=Now-Run->Start;const FVector P=Chuck->GetActorLocation()-G;
        if(Run->Walk<Way.Num())
        {
            const FVector To=Way[Run->Walk];
            if(FVector::Dist2D(P,To)<35){UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_WALK leg=%d z=%.1f seconds=%.1f ground=%d"),Run->Walk,P.Z-PlazaZ(),Time,M->IsMovingOnGround());++Run->Walk;if(Run->Walk<Way.Num())return;}
            else if(Time<150){const FVector D=(To-P).GetSafeNormal2D();Chuck->SetTestStickWorld(D);Chuck->AddMovementInput(D,1,true);return;}
        }
        Chuck->SetTestStick(FVector2D::ZeroVector);Chuck->SetRunHeld(false);
        const bool Pass=Run->Walk==Way.Num()&&M->IsMovingOnGround()&&FMath::Abs(P.Z-34.65f-PlazaZ()-Shrine.Z)<20&&InRoom(Chuck->GetActorLocation(),0)&&Chuck->GetFallDeaths()==0;
        Run->bDone=true;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_TEST failures=%d climb=%d legs=%d seconds=%.1f"),Run->Failures+!Pass,Pass,Run->Walk,Time);
        FPlatformMisc::RequestExitWithStatus(false,(Run->Failures||!Pass)?1:0);
    },.016f,true);
}
