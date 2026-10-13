#include "ChultDressing.h"
#include "ChultLanding.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
    struct FSurface
    {
        TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV; TArray<FColor> C;
        int32 Add(FVector P,FVector Normal,FVector2D Tex,FColor Colour=FColor::White)
        {int32 I=V.Add(P);N.Add(Normal);UV.Add(Tex);C.Add(Colour);return I;}
        void Tri(int32 A,int32 B,int32 D) {T.Append({A,B,D});}
    };
}

void BuildChultWetDressing(AActor* Owner, bool SolidRocks)
{
    FRandomStream R(73113);
    const FVector G=AChultLanding::FloorOrigin();
    auto Finish=[Owner,G](const TCHAR* Name,FSurface& S,const TCHAR* Material,bool Solid)
    {
        auto* P=NewObject<UProceduralMeshComponent>(Owner,FName(Name));
        P->SetupAttachment(Owner->GetRootComponent()); P->SetWorldLocation(G); P->RegisterComponent();
        P->bUseComplexAsSimpleCollision=true;
        P->CreateMeshSection(0,S.V,S.T,S.N,S.UV,S.C,TArray<FProcMeshTangent>(),Solid);
        P->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(Solid) P->SetCollisionProfileName(TEXT("BlockAll"));
        P->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Cutscenes/FallToChult/%s.%s"),Material,Material)));
        return P;
    };
    FSurface Wood,Moss,Water,Gravel;
    auto Tube=[&](FVector A,FVector B,float Radius)
    {
        const FVector D=(B-A).GetSafeNormal();
        const FVector Side=FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal(),Up=FVector::CrossProduct(Side,D).GetSafeNormal();
        int32 Base=Wood.V.Num();
        for(int32 J=0;J<=6;++J) for(int32 K=0;K<=8;++K)
        {
            float U=J/6.f,Angle=K*2*PI/8;
            FVector Normal=Side*FMath::Cos(Angle)+Up*FMath::Sin(Angle);
            FVector P=FMath::Lerp(A,B,U)+FVector(0,0,45*FMath::Sin(U*PI))+Normal*Radius*(1-.7f*U);
            Wood.Add(P,Normal,FVector2D(K/8.f,U*FVector::Dist(A,B)/260),FColor(190,180,0,0));
            if(J<6 && K<8) {int32 I=Base+J*9+K;Wood.Tri(I,I+9,I+1);Wood.Tri(I+1,I+9,I+10);}
        }
    };
    int32 Strands=0;
    auto Curtain=[&](FVector At,float Width,float Length)
    {
        float Yaw=R.FRandRange(0,360);
        FVector Side=FRotator(0,Yaw,0).Vector(),Face=FVector::CrossProduct(Side,FVector::UpVector);
        for(int32 S=0;S<15;++S)
        {
            FVector Start=At+Side*R.FRandRange(-Width,Width)+Face*R.FRandRange(-16,16);
            float L=Length*R.FRandRange(.4f,1.f),W=R.FRandRange(3,10),Phase=R.FRandRange(0,6);
            int32 Base=Moss.V.Num();
            for(int32 J=0;J<=18;++J)
            {
                float U=J/18.f;
                FVector P=Start+FVector(0,0,-L*U)+Side*(FMath::Sin(U*5+Phase)*18*U)+Face*FMath::Sin(U*3)*12;
                float Half=W*(1-.85f*U)*R.FRandRange(.7f,1.3f);
                FColor Col(R.RandRange(130,230),160,80,FMath::RoundToInt(U*255));
                Moss.Add(P-Side*Half,Face,FVector2D(0,U*5),Col);
                Moss.Add(P+Side*Half,Face,FVector2D(1,U*5),Col);
                if(J<18) {int32 I=Base+J*2;Moss.Tri(I,I+2,I+1);Moss.Tri(I+1,I+2,I+3);}
            }
            ++Strands;
        }
    };
    // Low, weathered lateral branches frame the clearing without moving its trees.
    const FVector Branch[][2]={
        {FVector(-760,200,720),FVector(-200,750,640)},
        {FVector(950,760,850),FVector(460,370,710)},
        {FVector(1600,-320,780),FVector(2150,-600,570)},
        {FVector(2200,1500,940),FVector(2790,1800,690)},
        {FVector(400,2300,860),FVector(-200,2050,570)},
        {FVector(2700,-1900,980),FVector(3200,-1320,770)},
        {FVector(-1500,950,980),FVector(-1150,1520,700)},
        {FVector(-800,2900,920),FVector(-250,3300,770)}};
    for(const auto& B:Branch)
    {
        Tube(B[0],B[1],24);
        for(float U:{.25f,.55f,.85f})
        {
            FVector At=FMath::Lerp(B[0],B[1],U)+FVector(0,0,45*FMath::Sin(U*PI));
            Curtain(At,20,FMath::Min(At.Z-170.f,R.FRandRange(300,630)));
        }
    }
    // Frayed moss adheres to the bark, with its ends drooping past the roots.
    for(const FVector At:{FVector(-760,200,0),FVector(950,760,0),FVector(1600,-320,0),FVector(2200,1500,0),FVector(400,2300,0)})
        for(int32 I=0;I<5;++I)
        {
            float A=R.FRandRange(0,2*PI);
            Curtain(At+FVector(FMath::Cos(A)*95,FMath::Sin(A)*95,R.FRandRange(320,550)),24,R.FRandRange(160,270));
        }
    struct FPuddle {float X,Y,RX,RY;};
    const FPuddle Puddles[]={{420,80,210,125},{1100,70,160,95},{1800,420,190,110},
        {3000,50,260,150},{4080,560,220,120},{4320,1380,180,120},{3300,2210,270,150},
        {1300,1730,200,100},{170,770,150,100}};
    for(const auto& P:Puddles)
    {
        int32 Center=Water.Add(FVector(P.X,P.Y,.9f),FVector::UpVector,FVector2D(.5,.5),FColor(255,255,255,240));
        for(int32 I=0;I<=48;++I)
        {
            float A=I*2*PI/48;
            float Jag=1+.09f*FMath::Sin(A*5+.7f)+.06f*FMath::Cos(A*9+P.X);
            for(int32 Ring=0;Ring<2;++Ring)
            {
                float S=Ring==0?.78f:1;
                Water.Add(FVector(P.X+FMath::Cos(A)*P.RX*Jag*S,P.Y+FMath::Sin(A)*P.RY*Jag*S,.9f),FVector::UpVector,
                    FVector2D(.5+FMath::Cos(A)*S*.5,.5+FMath::Sin(A)*S*.5),FColor(255,255,255,Ring==0?235:0));
            }
            if(I<48) {int32 K=Center+1+I*2;Water.Tri(Center,K,K+2);Water.Tri(K,K+1,K+2);Water.Tri(K+1,K+3,K+2);}
        }
    }
    // Tiny, embedded gravel breaks up the smooth puddle margins. At less than
    // three centimetres high it remains visual dressing, not a walking obstacle.
    for(const auto& P:Puddles) for(int32 Stone=0;Stone<12;++Stone)
    {
        float A=Stone*2*PI/12+R.FRandRange(-.2f,.2f),Size=R.FRandRange(7,16);
        FVector Center(P.X+FMath::Cos(A)*P.RX*R.FRandRange(.85f,1.12f),P.Y+FMath::Sin(A)*P.RY*R.FRandRange(.85f,1.12f),.5f);
        int32 Base=Gravel.V.Num();
        for(int32 Y=0;Y<=6;++Y) for(int32 X=0;X<=10;++X)
        {
            float Lat=Y*PI/6,Lon=X*2*PI/10;
            FVector N(FMath::Sin(Lat)*FMath::Cos(Lon),FMath::Sin(Lat)*FMath::Sin(Lon),FMath::Cos(Lat));
            Gravel.Add(Center+N*FVector(Size,Size*.8f,Size*.14f),N,FVector2D(X/10.f,Y/6.f));
            if(Y<6 && X<10) {int32 K=Base+Y*11+X;Gravel.Tri(K,K+1,K+11);Gravel.Tri(K+1,K+12,K+11);}
        }
    }
    // Moss-coated stone clusters sit beside the existing route, outside the
    // verified walking centre and landing/cinematic camera lane.
    const FVector RockAt[]={FVector(680,510,0),FVector(1400,-540,0),FVector(1970,1070,0),FVector(2940,-700,0),
        FVector(3250,780,0),FVector(4300,2500,0),FVector(2500,3150,0),FVector(250,1700,0),FVector(-640,-620,0)};
    UHierarchicalInstancedStaticMeshComponent* RockSets[6]={};
    int32 ScanRocks=0,MissingRocks=0;
    for(FVector At:RockAt) for(int32 Cluster=0;Cluster<3;++Cluster)
    {
        float Size=R.FRandRange(40,110);
        FVector Center=At+FVector(R.FRandRange(-100,100),R.FRandRange(-100,100),-8);
        FVector Scale(R.FRandRange(.9f,1.5f),R.FRandRange(.7f,1.2f),R.FRandRange(.6f,1.f));
        const int32 Variant=ScanRocks%6;
        auto*& H=RockSets[Variant];
        if(!H)
        {
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,*FString::Printf(TEXT("ChultScanRocks%d"),Variant));
            const FString Name=FString::Printf(TEXT("SM_ChultRock%d"),Variant);
            H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/Art/ChultNature/%s.%s"),*Name,*Name)));
            H->SetupAttachment(Owner->GetRootComponent());H->SetWorldLocation(G);H->RegisterComponent();
            H->SetCollisionEnabled(SolidRocks?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
            if(SolidRocks) H->SetCollisionProfileName(TEXT("BlockAll"));
            MissingRocks+=H->GetStaticMesh()==nullptr;
        }
        H->AddInstance(FTransform(FRotator(0,ScanRocks*137.5f,0),G+Center,Scale*(Size/150.f)),true);
        ++ScanRocks;
    }
    Finish(TEXT("ChultMossBranches"),Wood,TEXT("M_FallBark"),false);
    auto* M=Finish(TEXT("ChultHangingMoss"),Moss,TEXT("M_ChultHangingMoss"),false);
    auto* W=Finish(TEXT("ChultPuddles"),Water,TEXT("M_ChultPuddle"),false); W->SetCastShadow(false);
    Finish(TEXT("ChultPuddleGravel"),Gravel,TEXT("M_ChultRock"),false);
    // A broad-leaf foreground plant supplies the close reference's scale and
    // surface detail. Its stem stays beyond the walking centre, in both sets.
    auto* Leaf=NewObject<UStaticMeshComponent>(Owner,TEXT("ChultForegroundLeaf"));
    Leaf->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Cutscenes/FallToChult/SM_FallElephantEar.SM_FallElephantEar")));
    Leaf->SetCollisionEnabled(ECollisionEnabled::NoCollision); Leaf->SetupAttachment(Owner->GetRootComponent());
    Leaf->SetWorldLocationAndRotation(G+FVector(250,310,0),FRotator(0,215,0));
    Leaf->SetWorldScale3D(FVector(1.1f)); Leaf->RegisterComponent();
    const bool Ready=M->GetMaterial(0) && W->GetMaterial(0) && MissingRocks==0;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_WET_DRESSING materials=%d moss_strands=%d puddles=%d rocks=27 pebbles=108 triangles=%d foliage_collision=0 water_collision=0"),
        Ready && Leaf->GetStaticMesh()!=nullptr,Strands,UE_ARRAY_COUNT(Puddles),(Wood.T.Num()+Moss.T.Num()+Water.T.Num()+Gravel.T.Num())/3);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_SCAN_ROCKS instances=%d variants=6 missing_assets=%d visible_mesh_collision=%d"),ScanRocks,MissingRocks,SolidRocks);
}
