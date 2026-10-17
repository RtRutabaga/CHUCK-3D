#include "ChultTemple.h"
#include "ChultCanyon.h"
#include "ChultRiver.h"
#include "ChultLanding.h"
#include "ChuckCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
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
    constexpr float HouseFront=Sc-450,HouseBack=Sc+650,HouseHalf=650,DoorHalf=120,DoorTop=260,WallTop=420,Vestibule=300;
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
    struct FParts{FSurf Stone,Moss,Plaster,Steps,StepMoss,Ramp,Dark,Bone,Bark,Vines,Block;};
    // A box in the frame (S0<S1, T0<T1, Z above the plaza). Inward reverses every face.
    void Box(FSurf& M,float S0,float S1,float T0,float T1,float Z0,float Z1,bool Inward=false,bool Bottom=false)
    {
        auto Q=[&](FVector A,FVector B,FVector Cc,FVector D){if(Inward)M.Quad(A,D,Cc,B);else M.Quad(A,B,Cc,D);};
        Q(At(S0,T0,Z1),At(S1,T0,Z1),At(S1,T1,Z1),At(S0,T1,Z1));
        if(Bottom||Inward)Q(At(S0,T0,Z0),At(S0,T1,Z0),At(S1,T1,Z0),At(S1,T0,Z0));
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
    const TCHAR* BoneMat=TEXT("/Game/Art/Materials/M_Plaster.M_Plaster");
    const TCHAR* DarkMat=TEXT("/Game/Art/Materials/M_Dark.M_Dark");
    const TCHAR* BarkMat=TEXT("/Game/Art/Cutscenes/FallToChult/M_FallBark.M_FallBark");
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
            // The serpent: blocky head on the plaza, jaws open.
            const float Tc=Sg*(St.Half+St.Out)*.5f,Sa=BalusterFoot;
            Box(P.Stone,Sa-150,Sa+10,Tc-65,Tc+65,45,125);          // head
            Box(P.Stone,Sa-195,Sa-140,Tc-52,Tc+52,70,122);         // snout
            Box(P.Stone,Sa-185,Sa-20,Tc-56,Tc+56,0,35);            // lower jaw
            Box(P.Dark,Sa-178,Sa-30,Tc-46,Tc+46,35,70);            // the throat
            for(float F:{-1.f,1.f})
            {
                Box(P.Plaster,Sa-192,Sa-180,Tc+F*30-6,Tc+F*30+6,45,70);   // fangs
                Box(P.Plaster,Sa-182,Sa-172,Tc+F*42-5,Tc+F*42+5,35,52);
                Box(P.Stone,Sa-125,Sa-80,Tc+F*52-10,Tc+F*52+12,108,135);  // brow
                Box(P.Dark,Sa-118,Sa-92,Tc+F*62-4,Tc+F*62+6,96,116);      // eye
            }
            Box(P.Stone,Sa-192,Sa-160,Tc-26,Tc+26,120,146);         // nose curl
            Box(P.Moss,Sa-80,Sa+20,Tc-74,Tc+74,125,160);           // feathered crest
            for(int32 K=0;K<4;++K)Box(P.Moss,Sa-60+K*22,Sa-48+K*22,Tc-80,Tc+80,150,176+K*6);
        }
        return StepCount;
    }
    // A pyramid's summit house: a dark doorway into a shallow vestibule, a
    // corniced roof and a pierced roof comb.
    void SummitHouse(FParts& P,FRandomStream& R)
    {
        const float Z=Top;
        Box(P.Stone,Sc-90,HouseBack,-HouseHalf,HouseHalf,Z,Z+WallTop);
        for(float Sg:{-1.f,1.f})
        {
            const float A=Sg<0?-HouseHalf:Vestibule,B=Sg<0?-Vestibule:HouseHalf;
            Box(P.Stone,HouseFront,Sc-90,A,B,Z,Z+WallTop);
            const float A2=Sg<0?-Vestibule:DoorHalf,B2=Sg<0?-DoorHalf:Vestibule;
            Box(P.Stone,HouseFront,HouseFront+60,A2,B2,Z,Z+WallTop);
        }
        Box(P.Stone,HouseFront,HouseFront+60,-DoorHalf,DoorHalf,Z+DoorTop,Z+WallTop);
        Box(P.Stone,HouseFront+60,Sc-90,-Vestibule,Vestibule,Z+300,Z+WallTop);
        Box(P.Dark,HouseFront+60.5f,Sc-90.5f,-Vestibule+.5f,Vestibule-.5f,Z+.5f,Z+299.5f,true);
        Box(P.Dark,HouseFront-1,HouseFront+61,-DoorHalf+.5f,DoorHalf-.5f,Z+.5f,Z+DoorTop-.5f,true);
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
    void Prasat(FParts& P,float S,float T,float Z,float Half,float Body,float SpireHeight)
    {
        Box(P.Stone,S-Half-35,S+Half+35,T-Half-35,T+Half+35,Z,Z+30);
        Box(P.Moss,S-Half-20,S+Half+20,T-Half-20,T+Half+20,Z+30,Z+45);
        Box(P.Stone,S-Half,S+Half,T-Half,T+Half,Z+45,Z+Body);
        const float W=Half*.38f,D=(Body-45)*.62f,Zd=Z+45;
        Box(P.Dark,S-Half-3,S-Half,T-W,T+W,Zd,Zd+D);Box(P.Dark,S+Half,S+Half+3,T-W,T+W,Zd,Zd+D);
        Box(P.Dark,S-W,S+W,T-Half-3,T-Half,Zd,Zd+D);Box(P.Dark,S-W,S+W,T+Half,T+Half+3,Zd,Zd+D);
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
FVector ChultTempleSpawn(){return AChultLanding::FloorOrigin()+Flat(SpawnS,0,34.65f);}
float ChultTempleSpawnYaw(){return Along.Rotation().Yaw;}

void BuildChultTemple(AActor* Owner)
{
    const double Started=FPlatformTime::Seconds();const FVector G=AChultLanding::FloorOrigin();
    FParts P;FRandomStream R(2026101591);int32 Steps=0,Strand=0;
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
        Prasat(P,Sc+X*(LevelS[2]-240),Y*(LevelT[2]-240),CTop,190,330,1250);
        Prasat(P,Sc+X*(LevelS[1]-330),Y*(LevelT[1]-330),2*Tier,165,290,800);Towers+=2;
    }
    for(float Y:{-1.f,1.f}){Prasat(P,GateS,Y*GateT,0,260,400,380);++Towers;}
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
    int32 Stakes=0;
    for(float S:StakeS)for(float Sg:{-1.f,1.f})
    {
        const float T=Sg*StakeT,Lean=R.FRandRange(-4,4);const FVector Foot(S,T,-10),Head(S+Lean,T+R.FRandRange(-3,3),178);
        for(int32 K=0;K<8;++K)
        {
            const float A=K*PI/4,B=(K+1)*PI/4;
            const FVector Oa(FMath::Cos(A),FMath::Sin(A),0),Ob(FMath::Cos(B),FMath::Sin(B),0);
            auto Q=[&](const FVector& C,const FVector& O,float Rr){const FVector L=C+O*Rr;return At(L.X,L.Y,L.Z);};
            P.Bark.Quad(Q(Foot,Oa,6),Q(Foot,Ob,6),Q(Head,Ob,4),Q(Head,Oa,4));
        }
        const FVector Fwd(0,-Sg,0),Side(FVector::CrossProduct(FVector::UpVector,Fwd)),Skull=Head+FVector(0,0,10);
        Ellipsoid(P.Bone,Skull,Fwd,FVector(11,8.5f,9.5f));
        Ellipsoid(P.Bone,Skull+Fwd*5-FVector(0,0,8),Fwd,FVector(6,6.5f,3.5f),8);
        for(float E:{-1.f,1.f})Ellipsoid(P.Dark,Skull+Fwd*9.2f+Side*E*3.6f+FVector(0,0,1.2f),Fwd,FVector(1.8f,2.4f,2.4f),8);
        Ellipsoid(P.Dark,Skull+Fwd*10.2f-FVector(0,0,3.2f),Fwd,FVector(1.2f,1.2f,1.6f),6);
        ++Stakes;
    }
    // The closed back: a hidden wall across the whole precinct, ground to above
    // every roof comb, behind which the jungle has taken the temple. Dressed below.
    Box(P.Block,BackFrom,BackTo,-ChultTemplePrecinctHalf-100,ChultTemplePrecinctHalf+100,-300,5200);
    int32 Missing=0,Triangles=0;
    Emit(Owner,TEXT("ChultTempleMasonry"),{{&P.Stone,StoneMat},{&P.Moss,MossMat},{&P.Plaster,PlasterMat}},true,Missing,Triangles);
    Emit(Owner,TEXT("ChultTempleSteps"),{{&P.Steps,StoneMat},{&P.StepMoss,MossMat}},false,Missing,Triangles);
    auto* RampMesh=Emit(Owner,TEXT("ChultTempleStairRamp"),{{&P.Ramp,StoneMat}},true,Missing,Triangles);RampMesh->SetHiddenInGame(true);RampMesh->SetCastShadow(false);
    Emit(Owner,TEXT("ChultTempleDark"),{{&P.Dark,DarkMat}},false,Missing,Triangles);
    Emit(Owner,TEXT("ChultTempleSkulls"),{{&P.Bone,BoneMat}},false,Missing,Triangles);
    Emit(Owner,TEXT("ChultTempleStakes"),{{&P.Bark,BarkMat}},true,Missing,Triangles);
    Emit(Owner,TEXT("ChultTempleVines"),{{&P.Vines,VineMat}},false,Missing,Triangles)->SetCastShadow(false);
    auto* Wall=Emit(Owner,TEXT("ChultTempleJungleWall"),{{&P.Block,StoneMat}},true,Missing,Triangles);
    Wall->SetHiddenInGame(true);Wall->SetCastShadow(false);Wall->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);

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
    auto Plant=[&](const FVector& L,int32 Type,float Scale,float Yaw)
    {
        Place(Type<4?FString::Printf(TEXT("SM_ChultFern%d"),Type):Type<6?TEXT("SM_FallElephantEar"):TEXT("SM_FallShrub"),L,Scale*(Type<4?1:.65f),Yaw);++Plants;
    };
    auto Tree=[&](const FVector& L,float Scale,float Yaw,bool Solid)
    {
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
            const FVector At_=At(Sc+S,T,L.Z-2);
            if(Rear&&R.FRand()<.06f){Tree(At_,R.FRandRange(.7f,1.1f),R.FRandRange(0,360),false);continue;}
            Plant(At_,R.RandRange(0,6),R.FRandRange(1.2f,Back?2.8f:2.f),R.FRandRange(0,360));
        }
    }
    OffT=0;
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
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE tiers=%d pyramids=2 levels=%d towers=%d height_cm=%.0f steps=%d stakes=%d serpents=6 stelae=%d fragments=%d vines=%d plants=%d trees=%d tall_trees=%d missing_assets=%d triangles=%d build_ms=%.1f"),
        Tiers,Levels,Towers,CTop+TowerBody+42+TowerSpire*.9f+TowerSpire*.05f,Steps,Stakes,Stelae,Fragments,Strand,Plants,Trees,TallTrees,Missing,Triangles,(FPlatformTime::Seconds()-Started)*1000);
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
    auto* Camera=Owner->GetWorld()->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(72);
    Chuck->SetActorHiddenInGame(true);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Camera);
    const TArray<FVector> Eyes={Flat(SpawnS-300,0,180),Flat(70500,-500,140),Flat(CStairFoot-650,-900,90),Flat(CStairTop+200,-500,CTop+170),
        Flat(Sc-Base-6500,-1500,2600),Flat(69300,-150,95),Flat(Sc-LevelS[0]+Band+150,-900,Tier+120),Flat(Sc+LevelS[2]-200,600,CTop+180),Flat(64300,0,260)};
    const TArray<FVector> Aims={Flat(Sc,0,1600),Flat(Sc,0,1500),Flat(CStairFoot-100,-460,80),Flat(TowerFront,0,CTop+200),
        Flat(Sc,0,1500),Flat(68300,-StakeT,180),Flat(Sc-LevelS[0],-PyramidT,Tier+200),Flat(BackTo,3500,200),Flat(Sc,0,1800)};
    const TArray<FString> Names={TEXT("TempleAvenue"),TEXT("TemplePlaza"),TEXT("TempleSerpent"),TEXT("TempleDoorway"),TEXT("TempleOverview"),
        TEXT("TempleSkullStake"),TEXT("TempleLedgeJoin"),TEXT("TempleBackJungle"),TEXT("TempleApproach")};
    struct FRun{int32 Phase=0,Walk=0,Failures=0;double Next=0,Start=-1;bool bDone=false;TWeakObjectPtr<ACameraActor> Camera;FTimerHandle Timer;};
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
        if(Run->Start<0)
        {
            if(Now<Run->Next)return;
            // The real character: from the plaza up the central stair into the sanctuary's doorway.
            Chuck->SetActorHiddenInGame(false);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Chuck);
            if(Run->Camera.IsValid())Run->Camera->Destroy();
            Chuck->ResetAtLocation(G+Flat(72600,0,34.65f));Chuck->SetActorRotation(FRotator(0,ChultTempleSpawnYaw(),0));Chuck->SetRunHeld(true);
            Run->Start=Now;return;
        }
        const TArray<FVector> Way={Flat(CStairFoot-200,0,0),Flat(CStairTop+100,0,CTop),Flat(TowerFront+170,0,CTop)};
        const float Time=Now-Run->Start;const FVector P=Chuck->GetActorLocation()-G;
        if(Run->Walk<Way.Num())
        {
            const FVector To=Way[Run->Walk];
            if(FVector::Dist2D(P,To)<35){UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_WALK leg=%d z=%.1f seconds=%.1f ground=%d"),Run->Walk,P.Z-PlazaZ(),Time,M->IsMovingOnGround());++Run->Walk;if(Run->Walk<Way.Num())return;}
            else if(Time<150){const FVector D=(To-P).GetSafeNormal2D();Chuck->SetTestStickWorld(D);Chuck->AddMovementInput(D,1,true);return;}
        }
        Chuck->SetTestStick(FVector2D::ZeroVector);Chuck->SetRunHeld(false);
        const bool Pass=Run->Walk==Way.Num()&&M->IsMovingOnGround()&&FMath::Abs(P.Z-34.65f-PlazaZ()-CTop)<20&&Chuck->GetFallDeaths()==0;
        Run->bDone=true;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_TEMPLE_TEST failures=%d climb=%d legs=%d seconds=%.1f"),Run->Failures+!Pass,Pass,Run->Walk,Time);
        FPlatformMisc::RequestExitWithStatus(false,(Run->Failures||!Pass)?1:0);
    },.016f,true);
}
