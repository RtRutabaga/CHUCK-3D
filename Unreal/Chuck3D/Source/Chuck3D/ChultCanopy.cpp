#include "ChultCanopy.h"
#include "ChultLanding.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
    // Four mature tree forks, with a second route returning to the first.
    const FVector Hubs[]={{5600,900,650},{7200,900,650},{8200,1800,760},{6800,2400,650}};
    struct FSurface
    {
        TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV; TArray<FColor> C;
        void Vertex(FVector P,FVector Normal,FVector2D Tex)
        { V.Add(P); N.Add(Normal); UV.Add(Tex); C.Add(FColor(190,180,0,0)); }
        void Tri(int32 A,int32 B,int32 D) { T.Append({A,B,D}); }
    };
    void Limb(FSurface& S,FVector A,FVector B,float RA,float RB,int32 Sides=10)
    {
        const FVector D=(B-A).GetSafeNormal();
        const FVector Axis=FMath::Abs(D.Z)>.95f?FVector::ForwardVector:FVector::UpVector;
        const FVector Side=FVector::CrossProduct(D,Axis).GetSafeNormal(),Up=FVector::CrossProduct(Side,D);
        const int32 Base=S.V.Num();
        for(int32 J=0;J<2;++J) for(int32 K=0;K<=Sides;++K)
        {
            const float Angle=2*PI*K/Sides;
            const FVector Normal=Side*FMath::Cos(Angle)+Up*FMath::Sin(Angle);
            S.Vertex((J?B:A)+Normal*(J?RB:RA),Normal,FVector2D(K/static_cast<float>(Sides),J*FVector::Dist(A,B)/200));
            if(J==0 && K<Sides) {int32 I=Base+K; S.Tri(I,I+1,I+Sides+1);S.Tri(I+1,I+Sides+2,I+Sides+1);}
        }
        // Closed ends keep visible and colliding surfaces identical.
        const int32 Cap=S.V.Num(); S.Vertex(A,-D,FVector2D::ZeroVector);S.Vertex(B,D,FVector2D::ZeroVector);
        for(int32 K=0;K<Sides;++K)
        { S.Tri(Cap,Base+K+1,Base+K);S.Tri(Cap+1,Base+Sides+1+K,Base+Sides+2+K); }
    }
    // Bevelled trunks keep a broad flat inner bark face for cat-to-cat climbing.
    void Trunk(FSurface& S,FVector At,float Height)
    {
        const FVector2D Ring[]={{-70,-100},{70,-100},{100,-70},{100,70},{70,100},{-70,100},{-100,70},{-100,-70}};
        for(int32 K=0;K<8;++K)
        {
            const auto A=Ring[K],B=Ring[(K+1)%8];
            const FVector Normal=FVector(B.Y-A.Y,A.X-B.X,0).GetSafeNormal();
            const int32 I=S.V.Num();
            const FVector Corners[]={FVector(A.X,A.Y,0),FVector(B.X,B.Y,0),FVector(B.X,B.Y,Height),FVector(A.X,A.Y,Height)};
            for(int32 J=0;J<4;++J)
                S.Vertex(At+Corners[J],Normal,FVector2D((K+((J==1 || J==2)?1.f:0.f))*.7f,Corners[J].Z/220));
            // UE's procedural-mesh face convention is clockwise from outside.
            S.Tri(I,I+2,I+1); S.Tri(I,I+3,I+2);
        }
        const int32 Center=S.V.Num();S.Vertex(At+FVector(0,0,Height),FVector::UpVector,FVector2D(.5,.5));
        for(int32 K=0;K<8;++K) S.Vertex(At+FVector(Ring[K].X,Ring[K].Y,Height),FVector::UpVector,Ring[K]/200);
        for(int32 K=0;K<8;++K) S.Tri(Center,Center+1+(K+1)%8,Center+1+K);
    }
}

bool InChultCanopyReserve(const FVector& P,float Margin)
{ return P.X>5000-Margin && P.X<8800+Margin && P.Y>250-Margin && P.Y<3000+Margin; }

const TArray<FChultCanopyChain>& ChultCanopyChains()
{
    static const TArray<FChultCanopyChain> Chains=[]()
    {
        TArray<FChultCanopyChain> Result;
        const int32 Edges[][2]={{0,1},{1,2},{2,3},{3,0}};
        for(int32 E=0;E<4;++E)
        {
            const FVector A=Hubs[Edges[E][0]],B=Hubs[Edges[E][1]],D=(B-A).GetSafeNormal2D();
            const FVector Start=A+D*300+FVector(0,0,115),End=B-D*300+FVector(0,0,115);
            FChultCanopyChain Chain; Chain.Name=FName(*FString::Printf(TEXT("ChultCanopyChain%d"),E));Chain.bVine=E==2;
            const int32 Steps=FMath::CeilToInt(FVector::Dist2D(Start,End)/150.f);
            for(int32 I=0;I<=Steps;++I)
            {
                const float U=I/static_cast<float>(Steps);
                Chain.Paws.Add(FMath::Lerp(Start,End,U)-FVector(0,0,Chain.bVine?35*FMath::Sin(PI*U):0));
            }
            Result.Add(MoveTemp(Chain));
        }
        return Result;
    }();
    return Chains;
}

void BuildChultCanopy(AActor* Owner)
{
    const FVector G=AChultLanding::FloorOrigin();
    auto* Bark=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Cutscenes/FallToChult/M_FallBark.M_FallBark"));
    auto Finish=[&](const TCHAR* Name,FSurface& S,UMaterialInterface* Material,bool Solid)
    {
        auto* P=NewObject<UProceduralMeshComponent>(Owner,FName(Name));
        P->SetupAttachment(Owner->GetRootComponent());P->SetWorldLocation(G);P->RegisterComponent();
        P->bUseComplexAsSimpleCollision=true;
        TArray<FVector> Normals;TArray<FProcMeshTangent> Tangents;
        UKismetProceduralMeshLibrary::CalculateTangentsForMesh(S.V,S.T,S.UV,Normals,Tangents);
        P->CreateMeshSection(0,S.V,S.T,S.N,S.UV,S.C,Tangents,Solid);
        P->SetMaterial(0,Material);P->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(Solid) P->SetCollisionProfileName(TEXT("BlockAll"));
        P->ComponentTags.Add(TEXT("ChultCanopy"));
    };
    FSurface Solid,Branches,Vines;
    int32 MarkerCount=0;
    for(int32 I=0;I<4;++I)
    {
        const FVector Hub=Hubs[I];
        // 320 cm centres, 100 cm half-width: 120 cm clear between flat faces.
        // The tops are 6.5–7.6 m high; continuation trunks grow OUTSIDE the gap.
        for(float Sign:{-1.f,1.f})
        {
            const FVector Foot(Hub.X,Hub.Y+Sign*160,0);
            Trunk(Solid,Foot,Hub.Z);
            Limb(Solid,Foot+FVector(0,0,Hub.Z-80),Foot+FVector(45,Sign*90,Hub.Z+420),76,42);
            for(float Yaw:{35.f,145.f,255.f})
            {
                const FVector Out=FRotator(0,Yaw+I*35,0).Vector();
                if(Out.Y*Sign<.3f) continue; // Keep the climbing gap and ground recovery open.
                Limb(Solid,Foot+FVector(0,0,35),Foot+Out*240+FVector(0,0,10),40,8);
            }
        }
        // A broad limb beside the climb: top at Hub.Z, with a level standing crown.
        for(int32 Neighbour:{(I+1)%4,(I+3)%4})
        {
            const FVector Out=(Hubs[Neighbour]-Hub).GetSafeNormal2D();
            Limb(Solid,Hub+FVector(0,0,-55),Hub+Out*330+FVector(0,0,-55),55,48,12);
        }
        Limb(Solid,Hub+FVector(0,-160,-55),Hub+FVector(0,160,-55),55,55,12);
        auto* Fork=NewObject<USceneComponent>(Owner,FName(*FString::Printf(TEXT("ChultCanopyFork%d"),I)));
        Fork->SetupAttachment(Owner->GetRootComponent());Fork->SetWorldLocation(G+Hub);Fork->ComponentTags.Add(TEXT("ChultCanopyStanding"));Fork->RegisterComponent();
        // Crowns sit above and to the sides of the route rather than through it.
        for(float Sign:{-1.f,1.f})
        {
            auto* Crown=NewObject<UStaticMeshComponent>(Owner,FName(*FString::Printf(TEXT("ChultCanopyCrown%d_%d"),I,Sign>0?1:0)));
            Crown->SetupAttachment(Owner->GetRootComponent());
            Crown->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Cutscenes/FallToChult/SM_FallCrown.SM_FallCrown")));
            Crown->SetWorldLocation(G+Hub+FVector(45,Sign*370,540));Crown->SetWorldScale3D(FVector(.7f));
            Crown->SetWorldRotation(FRotator(0,I*73+Sign*30,0));Crown->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Crown->ComponentTags.Add(TEXT("ChultCanopyFoliage"));Crown->RegisterComponent();
        }
    }
    const auto& Chains=ChultCanopyChains();
    for(int32 E=0;E<Chains.Num();++E)
    {
        const auto& Chain=Chains[E];const auto& Paws=Chain.Paws;
        for(int32 I=0;I<Paws.Num();++I)
        {
            auto* Marker=NewObject<USceneComponent>(Owner,FName(*FString::Printf(TEXT("%s_Paw%02d"),*Chain.Name.ToString(),I)));
            const FVector Along=(Paws.Last()-Paws[0]).GetSafeNormal2D();
            Marker->SetupAttachment(Owner->GetRootComponent());Marker->SetWorldLocationAndRotation(G+Paws[I],Along.Rotation());
            Marker->ComponentTags.Add(TEXT("ChultCanopyGripProposal"));Marker->RegisterComponent();++MarkerCount;
            if(I>0) Limb(Chain.bVine?Vines:Branches,Paws[I-1]+FVector(0,0,Chain.bVine?5:14),Paws[I]+FVector(0,0,Chain.bVine?5:14),Chain.bVine?5:14,Chain.bVine?5:14);
        }
        // Thick parent branches lift out of the leap corridor to frame the thin handhold.
        const FVector A=Hubs[E],B=Hubs[(E+1)%4];
        Limb(Branches,A+FVector(0,0,450),Paws[0]+FVector(0,0,14),48,14);
        Limb(Branches,B+FVector(0,0,450),Paws.Last()+FVector(0,0,14),48,14);
    }
    Finish(TEXT("ChultCanopySolidBark"),Solid,Bark,true);
    Finish(TEXT("ChultCanopyHandBranches"),Branches,Bark,false);
    Finish(TEXT("ChultCanopyVineBridge"),Vines,Bark,false);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANOPY hubs=4 climb_gap=120 chains=%d proposed_paws=%d registered_grips=0 triangles=%d"),Chains.Num(),MarkerCount,(Solid.T.Num()+Branches.T.Num()+Vines.T.Num())/3);
}
