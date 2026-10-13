#include "ChultCanopy.h"
#include "ChultLanding.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
    const FVector Hubs[]={{5600,900,650},{7200,900,650},{8200,1800,760},{6800,2400,650}};
    struct FSurface
    {
        TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV; TArray<FColor> C;
        void Vertex(FVector P,FVector Normal,FVector2D Tex)
        { V.Add(P); N.Add(Normal); UV.Add(Tex); C.Add(FColor(190,180,0,0)); }
        void Tri(int32 A,int32 B,int32 D) { T.Append({A,B,D}); }
    };
    // A continuous swept surface, rather than capped cylinders at every bend.
    void Bough(FSurface& S,const TArray<FVector>& Path,const TArray<float>& Radii,int32 Sides=20,float Flatten=1)
    {
        const int32 Base=S.V.Num(); float Length=0;
        for(int32 J=0;J<Path.Num();++J)
        {
            const FVector D=(Path[FMath::Min(J+1,Path.Num()-1)]-Path[FMath::Max(J-1,0)]).GetSafeNormal();
            const FVector Axis=FMath::Abs(D.Z)>.95f?FVector::ForwardVector:FVector::UpVector;
            const FVector Side=FVector::CrossProduct(D,Axis).GetSafeNormal(),Up=FVector::CrossProduct(Side,D);
            if(J) Length+=FVector::Dist(Path[J-1],Path[J]);
            for(int32 K=0;K<=Sides;++K)
            {
                const float A=2*PI*K/Sides;
                const FVector Normal=(Side*FMath::Cos(A)+Up*FMath::Sin(A)/Flatten).GetSafeNormal();
                S.Vertex(Path[J]+(Side*FMath::Cos(A)+Up*FMath::Sin(A)*Flatten)*Radii[J],Normal,FVector2D(K/static_cast<float>(Sides),Length/200));
                if(J && K<Sides)
                { const int32 I=Base+J*(Sides+1)+K; S.Tri(I-Sides-1,I-Sides,I);S.Tri(I-Sides,I+1,I); }
            }
        }
        const int32 Cap=S.V.Num();
        S.Vertex(Path[0],(Path[0]-Path[1]).GetSafeNormal(),FVector2D::ZeroVector);
        S.Vertex(Path.Last(),(Path.Last()-Path[Path.Num()-2]).GetSafeNormal(),FVector2D::ZeroVector);
        for(int32 K=0;K<Sides;++K)
        { S.Tri(Cap,Base+K+1,Base+K);S.Tri(Cap+1,Base+(Path.Num()-1)*(Sides+1)+K,Base+(Path.Num()-1)*(Sides+1)+K+1); }
    }
    void Curve(FSurface& S,FVector A,FVector Bend,FVector B,float RA,float RB,int32 Steps=12,float Flatten=1)
    {
        TArray<FVector> Path;TArray<float> Radii;
        for(int32 J=0;J<=Steps;++J)
        { const float U=J/static_cast<float>(Steps);Path.Add(A*FMath::Square(1-U)+Bend*(2*U*(1-U))+B*U*U);Radii.Add(FMath::Lerp(RA,RB,U)); }
        Bough(S,Path,Radii,20,Flatten);
    }
    // Round exterior with an intentionally retained inner climbing strip.
    // Its face stays at +/-100 cm even as the outer base flares and tapers.
    void Trunk(FSurface& S,FVector At,float Height,float Sign,int32 Seed)
    {
        constexpr int32 Sides=32,Rings=24;
        const int32 Base=S.V.Num();
        for(int32 J=0;J<=Rings;++J)
        {
            const float U=FMath::Min(1.f,J/12.f),Upper=FMath::Max(0.f,(J-12)/12.f);
            const float Flare=FMath::Square(FMath::Max(0.f,1-U*5));
            const FVector Center=At+FVector(45*Upper*Upper,Sign*140*Upper,Height*U+540*Upper);
            for(int32 K=0;K<=Sides;++K)
            {
                const float A=2*PI*K/Sides;
                const float Radius=(122+(1-FMath::Cos(A*3+Seed))*3+Flare*35-U*10)*(1-.72f*Upper);
                float X=FMath::Cos(A)*Radius,Y=FMath::Sin(A)*Radius;
                const bool Inner=Upper==0 && Y*Sign<-100;
                if(Inner) Y=-Sign*100;
                else if(Y*Sign>0) Y+=Sign*(Flare*55+U*10)*(1-Upper);
                const FVector Normal=Inner?FVector(0,-Sign,0):FVector(FMath::Cos(A),FMath::Sin(A),.05).GetSafeNormal();
                S.Vertex(Center+FVector(X,Y,0),Normal,FVector2D(K/static_cast<float>(Sides)*4,(U*Height+540*Upper)/220));
                if(J && K<Sides)
                { const int32 I=Base+J*(Sides+1)+K;S.Tri(I-Sides-1,I,I-Sides);S.Tri(I-Sides,I,I+1); }
            }
        }
        const int32 Cap=S.V.Num();S.Vertex(At+FVector(45,Sign*140,Height+540),FVector::UpVector,FVector2D(.5,.5));
        for(int32 K=0;K<Sides;++K)S.Tri(Cap,Base+Rings*(Sides+1)+(K+1)%Sides,Base+Rings*(Sides+1)+K);
    }
    FVector PawAt(int32 E,float U)
    {
        const FVector A=Hubs[E],B=Hubs[(E+1)%4],D=(B-A).GetSafeNormal2D();
        const FVector Across(-D.Y,D.X,0);
        return FMath::Lerp(A+D*300,B-D*300,U)+FVector(0,0,115+45*FMath::Sin(PI*U))+Across*(35*FMath::Sin(PI*U)*(E%2?1:-1));
    }
    float PawRadius(float U) {return 12+16*FMath::Abs(2*U-1);}
    const FVector GroundLane[]={{4200,500,0},{4900,500,0},{5300,900,0},{5600,900,0},{6200,900,0},{6500,1600,0},{7400,2200,0},{7800,3200,0},{6800,3300,0},{6000,2900,0},{5100,1800,0},{4600,1350,0}};
    bool RootClear(FVector At)
    {
        At.Z=0;
        for(int32 I=1;I<UE_ARRAY_COUNT(GroundLane);++I)
            if(FVector::Dist2D(At,FMath::ClosestPointOnSegment(At,GroundLane[I-1],GroundLane[I]))<115) return false;
        return true;
    }
}

bool InChultCanopyReserve(const FVector& P,float Margin)
{ return P.X>5000-Margin && P.X<8800+Margin && P.Y>250-Margin && P.Y<3000+Margin; }

const TArray<FChultCanopyChain>& ChultCanopyChains()
{
    static const TArray<FChultCanopyChain> Chains=[]()
    {
        TArray<FChultCanopyChain> Result;
        for(int32 E=0;E<4;++E)
        {
            FChultCanopyChain Chain;Chain.Name=FName(*FString::Printf(TEXT("ChultCanopyChain%d"),E));
            const float Length=FVector::Dist2D(PawAt(E,0),PawAt(E,1));
            const float HalfGap=90/Length;
            for(int32 Side=0;Side<2;++Side)
            {
                if(Side) Chain.BranchBreak=Chain.Paws.Num();
                const float Start=Side?.5f+HalfGap:0,End=Side?1:.5f-HalfGap;
                const int32 Steps=FMath::CeilToInt(Length*(End-Start)/140);
                for(int32 I=0;I<=Steps;++I)Chain.Paws.Add(PawAt(E,FMath::Lerp(Start,End,I/static_cast<float>(Steps))));
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
    auto* Leaf=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Cutscenes/FallToChult/SM_FallLeafCard.SM_FallLeafCard"));
    auto Finish=[&](const TCHAR* Name,FSurface& S,bool Solid)
    {
        auto* P=NewObject<UProceduralMeshComponent>(Owner,FName(Name));
        P->SetupAttachment(Owner->GetRootComponent());P->SetWorldLocation(G);P->RegisterComponent();P->bUseComplexAsSimpleCollision=true;
        TArray<FVector> Normals;TArray<FProcMeshTangent> Tangents;
        UKismetProceduralMeshLibrary::CalculateTangentsForMesh(S.V,S.T,S.UV,Normals,Tangents);
        P->CreateMeshSection(0,S.V,S.T,S.N,S.UV,S.C,Tangents,Solid);
        P->SetMaterial(0,Bark);P->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(Solid)P->SetCollisionProfileName(TEXT("BlockAll"));P->ComponentTags.Add(TEXT("ChultCanopy"));
    };
    FSurface Solid,Branches;int32 MarkerCount=0,RootCount=0,LeafCount=0;
    for(int32 I=0;I<4;++I)
    {
        const FVector Hub=Hubs[I];
        for(float Sign:{-1.f,1.f})
        {
            const FVector Foot(Hub.X,Hub.Y+Sign*160,0);
            Trunk(Solid,Foot,Hub.Z,Sign,I);
            const FVector Top=Foot+FVector(45,Sign*140,Hub.Z+540);
            // Low, flared buttress ribs broaden into meandering surface roots.
            // All grow outward; candidate ribs near the recovery lane are omitted.
            for(int32 R=0;R<7;++R)
            {
                const float Angle=(20+R*23+I*3)*PI/180;
                const FVector Out(FMath::Cos(Angle),Sign*FMath::Sin(Angle),0);
                const FVector Across(-Out.Y,Out.X,0);
                const float Reach=350+((R*71+I*43)%180);
                const FVector End=Foot+Out*Reach+Across*(R%2?40:-35);
                bool Clear=true;
                for(int32 K=2;K<=10;++K)Clear&=RootClear(FMath::Lerp(Foot,End,K/10.f));
                if(!Clear)continue;
                // Flattened vertical ribs sink into the ground, with no floating tips.
                TArray<FVector> P;TArray<float> Rad;
                for(int32 K=0;K<=12;++K)
                {
                    const float U=K/12.f;
                    P.Add(Foot+Out*(60+(Reach-60)*U)+Across*(FMath::Sin(PI*U)*26+(R%2?40:-35)*U)+FVector(0,0,105*FMath::Pow(1-U,3)-4));
                    Rad.Add(43*FMath::Pow(1-U,1.4f)+2);
                }
                Bough(Solid,P,Rad,20,1.9f);++RootCount;
                const FVector Split=P[7],Toe=End+Across*(R%2?95:-95)+Out*65;
                if(RootClear(Toe))Curve(Solid,Split,End+Across*45,Toe+FVector(0,0,-5),11,2,8,.65f);
            }
            // Air between individual leaves replaces the old opaque mushroom core.
            auto* Crown=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,FName(*FString::Printf(TEXT("ChultCanopyCrown%d_%d"),I,Sign>0?1:0)));
            Crown->SetupAttachment(Owner->GetRootComponent());Crown->SetWorldLocation(G);Crown->SetStaticMesh(Leaf);
            Crown->SetCollisionEnabled(ECollisionEnabled::NoCollision);Crown->ComponentTags.Add(TEXT("ChultCanopyFoliage"));Crown->RegisterComponent();
            FRandomStream Rng(817+I*73+static_cast<int32>(Sign)*19);
            for(int32 L=0;L<5;++L)
            {
                const float Yaw=(L*71+I*23)*PI/180;
                const FVector Out(FMath::Cos(Yaw),Sign*FMath::Abs(FMath::Sin(Yaw)),0);
                const FVector Lobe=Top+Out*(170+L*19)+FVector(0,0,70+(L%3)*45);
                Curve(Branches,Top-FVector(0,0,100),Top+Out*120+FVector(0,0,120),Lobe,26,5);
                for(int32 K=0;K<220;++K)
                {
                    const FVector Offset(Rng.FRandRange(-150,150),Rng.FRandRange(-150,150),Rng.FRandRange(-75,105));
                    if(Offset.Size2D()>150)continue;
                    const float Scale=Rng.FRandRange(1.5f,2.6f);
                    Crown->AddInstance(FTransform(FRotator(Rng.FRandRange(-65,65),Rng.FRandRange(0,360),Rng.FRandRange(-30,30)),Lobe+Offset,FVector(Scale,Scale,Scale)),false);++LeafCount;
                }
            }
        }
        for(int32 Neighbour:{(I+1)%4,(I+3)%4})
        {
            const FVector Out=(Hubs[Neighbour]-Hub).GetSafeNormal2D();
            Curve(Solid,Hub+FVector(0,0,-55),Hub+Out*180+FVector(0,0,-55),Hub+Out*330+FVector(0,0,-55),55,48);
        }
        Curve(Solid,Hub+FVector(0,-160,-55),Hub+FVector(0,0,-55),Hub+FVector(0,160,-55),55,55);
        auto* Fork=NewObject<USceneComponent>(Owner,FName(*FString::Printf(TEXT("ChultCanopyFork%d"),I)));
        Fork->SetupAttachment(Owner->GetRootComponent());Fork->SetWorldLocation(G+Hub);Fork->ComponentTags.Add(TEXT("ChultCanopyStanding"));Fork->RegisterComponent();
    }
    const auto& Chains=ChultCanopyChains();int32 Failures=0;float MaxStep=0,MaxRise=0;
    for(int32 E=0;E<Chains.Num();++E)
    {
        const auto& Chain=Chains[E];const auto& Paws=Chain.Paws;
        for(int32 I=0;I<Paws.Num();++I)
        {
            const FVector Along=(Paws[FMath::Min(I+1,Paws.Num()-1)]-Paws[FMath::Max(I-1,0)]).GetSafeNormal2D();
            auto* Marker=NewObject<USceneComponent>(Owner,FName(*FString::Printf(TEXT("%s_Paw%02d"),*Chain.Name.ToString(),I)));
            Marker->SetupAttachment(Owner->GetRootComponent());Marker->SetWorldLocationAndRotation(G+Paws[I],Along.Rotation());
            Marker->ComponentTags.Add(TEXT("ChultCanopyGripProposal"));if(I==Chain.BranchBreak)Marker->ComponentTags.Add(TEXT("ChultCanopyBranchJump"));Marker->RegisterComponent();++MarkerCount;
            if(I)
            {
                const float Step=FVector::Dist2D(Paws[I-1],Paws[I]),Rise=FMath::Abs(Paws[I].Z-Paws[I-1].Z);
                MaxStep=FMath::Max(MaxStep,Step);MaxRise=FMath::Max(MaxRise,Rise);
                if(Step<40 || Step>240 || Rise>70)++Failures;
            }
        }
        const float Length=FVector::Dist2D(PawAt(E,0),PawAt(E,1)),HalfGap=90/Length;
        for(int32 Side=0;Side<2;++Side)
        {
            const FVector Hub=Hubs[(E+Side)%4],D=(Hubs[(E+1)%4]-Hubs[E]).GetSafeNormal2D()*(Side?-1:1);
            const float Sign=D.Y<0?-1.f:1.f;
            const FVector Root=Hub+FVector(0,Sign*160,105);
            const float Start=Side?1:0,End=.5f+(Side?1:-1)*HalfGap;
            TArray<FVector> Path;TArray<float> Radii;
            // Natural crotch bends from a supporting trunk into the reaching bough.
            const FVector First=PawAt(E,Start)+FVector(0,0,PawRadius(Start));
            const float NextU=Start+(Side?-.01f:.01f);
            const FVector Next=PawAt(E,NextU)+FVector(0,0,PawRadius(NextU));
            const FVector Bend=First-(Next-First).GetSafeNormal()*170;
            for(int32 K=0;K<8;++K)
            { const float U=K/8.f;Path.Add(Root*FMath::Square(1-U)+Bend*(2*U*(1-U))+First*U*U);Radii.Add(FMath::Lerp(67.f,PawRadius(Start),U)); }
            for(int32 K=0;K<=24;++K)
            { const float U=FMath::Lerp(Start,End,K/24.f);Path.Add(PawAt(E,U)+FVector(0,0,PawRadius(U)));Radii.Add(PawRadius(U)); }
            // A rounded taper ends 20 cm beyond the last paw, leaving 140 cm of air.
            Path.Add(Path.Last()+D*20);Radii.Add(2);
            Bough(Branches,Path,Radii);
        }
    }
    Finish(TEXT("ChultCanopySolidBark"),Solid,true);Finish(TEXT("ChultCanopyHandBranches"),Branches,false);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANOPY hubs=4 climb_gap=120 chains=%d proposed_paws=%d registered_grips=0 triangles=%d"),Chains.Num(),MarkerCount,(Solid.T.Num()+Branches.T.Num())/3);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANOPY_LAYOUT failures=%d branch_jumps=4 hand_gap=180 wood_gap=140 max_step=%.1f max_rise=%.1f roots=%d leaves=%d missing_assets=%d"),Failures,MaxStep,MaxRise,RootCount,LeafCount,(!Bark)+(!Leaf));
}
