#include "ChultExpansion.h"
#include "ChultCliffs.h"
#include "ChultLanding.h"
#include "ChultCanopy.h"
#include "ChultRootProfiles.h"
#include "ChuckCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"
#include "EngineGlobals.h"

namespace
{
    constexpr float MinX=7000,MaxX=22000,MinY=2000,MaxY=15500,Cell=50;
    const FVector WalkLoop[]={{7800,3200,0},{8300,4000,0},{8750,4700,0},{9650,5550,0},
        {10100,6100,0},{10900,6700,0},{11000,7600,0},{12600,7700,0},{13800,7400,0},{15000,8200,0},
        {15700,9750,0},{16800,11100,0},{18100,11800,0},{19200,12700,0},{19900,11700,0},
        {19100,10500,0},{17800,9400,0},{16200,8500,0},{15100,7400,0},{13300,6000,0},
        {12300,5050,0},{11600,4000,0},{10500,3150,0},{9600,2600,0},{8350,2300,0},{7800,3200,0}};
    const FVector StreamKnots[]={{8200,5500,24},{9300,7800,34},{11300,8900,48},
        {13400,9200,62},{15500,10400,86},{17800,11600,112},{20700,14300,140}};
    float Ease(float T) {T=FMath::Clamp(T,0.f,1.f);return T*T*(3-2*T);}
    float SegmentDistance(FVector P,FVector A,FVector B)
    {P.Z=A.Z=B.Z=0;return FVector::Dist2D(P,FMath::ClosestPointOnSegment(P,A,B));}
    const TArray<FVector>& StreamPath()
    {
        static const TArray<FVector> Path=[]()
        {
            TArray<FVector> P;
            for(int32 I=0;I<UE_ARRAY_COUNT(StreamKnots)-1;++I) for(int32 J=0;J<12;++J)
            {
                const FVector A=StreamKnots[FMath::Max(0,I-1)],B=StreamKnots[I],C=StreamKnots[I+1],D=StreamKnots[FMath::Min(I+2,static_cast<int32>(UE_ARRAY_COUNT(StreamKnots))-1)];
                const float U=J/12.f;
                FVector At=((B*2)+(-A+C)*U+(A*2-B*5+C*4-D)*U*U+(-A+B*3-C*3+D)*U*U*U)*.5f;
                At.Z=FMath::Lerp(B.Z,C.Z,U);P.Add(At);
            }
            P.Add(StreamKnots[UE_ARRAY_COUNT(StreamKnots)-1]);return P;
        }();
        return Path;
    }
    struct FStreamSample {float Distance=1.e9f,Water=0,Width=0,Along=0;};
    FStreamSample StreamAt(FVector P)
    {
        FStreamSample S;const auto& Path=StreamPath();P.Z=0;float Length=0;
        for(int32 I=1;I<Path.Num();++I)
        {
            FVector A=Path[I-1],B=Path[I];A.Z=B.Z=0;const FVector D=B-A;
            const float U=FMath::Clamp(static_cast<float>(FVector::DotProduct(P-A,D)/D.SizeSquared()),0.f,1.f);
            const float Distance=FVector::Dist2D(P,A+D*U),Segment=D.Size2D();
            if(Distance<S.Distance) {S.Distance=Distance;S.Water=FMath::Lerp(Path[I-1].Z,Path[I].Z,U);S.Along=Length+Segment*U;S.Width=62+15*FMath::Sin(S.Along*.0013f);}
            Length+=Segment;
        }
        return S;
    }
    float CanopyDistance(FVector P)
    {
        const float DX=FMath::Max(FMath::Max(5000.f-static_cast<float>(P.X),static_cast<float>(P.X)-8800.f),0.f);
        const float DY=FMath::Max(FMath::Max(250.f-static_cast<float>(P.Y),static_cast<float>(P.Y)-3000.f),0.f);
        float Distance=FMath::Sqrt(DX*DX+DY*DY)-350;
        const auto& Trees=ChultCanopyTrees();
        for(int32 I=4;I<Trees.Num();++I)
            Distance=FMath::Min(Distance,SegmentDistance(P,Trees[I==4?2:I-1].Trunk,Trees[I].Trunk)-1350);
        return Distance;
    }
    bool OnLane(FVector P,float Margin)
    {
        for(int32 I=1;I<UE_ARRAY_COUNT(WalkLoop);++I) if(SegmentDistance(P,WalkLoop[I-1],WalkLoop[I])<Margin) return true;
        return false;
    }
    struct FSurface
    {
        TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;TArray<FColor> C;TArray<FProcMeshTangent> Tangents;
        int32 Add(FVector P,FVector Normal,FVector2D Tex,FColor Colour=FColor::White)
        {int32 I=V.Add(P);N.Add(Normal);UV.Add(Tex);C.Add(Colour);Tangents.Add(FProcMeshTangent(FVector::ForwardVector,false));return I;}
        void Tri(int32 A,int32 B,int32 D) {T.Append({A,B,D});}
    };
    UMaterialInterface* Material(const TCHAR* Name)
    {return LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Cutscenes/FallToChult/%s.%s"),Name,Name));}
    UProceduralMeshComponent* Finish(AActor* Owner,const TCHAR* Name,FSurface& S,UMaterialInterface* Mat,bool Solid)
    {
        auto* P=NewObject<UProceduralMeshComponent>(Owner,FName(Name));P->SetupAttachment(Owner->GetRootComponent());P->SetWorldLocation(AChultLanding::FloorOrigin());P->RegisterComponent();
        P->bUseComplexAsSimpleCollision=true;P->CreateMeshSection(0,S.V,S.T,S.N,S.UV,S.C,S.Tangents,Solid);
        P->SetMaterial(0,Mat);P->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        if(Solid) P->SetCollisionProfileName(TEXT("BlockAll"));return P;
    }
}

bool InChultExpansion(const FVector& P) {return P.X>=MinX && P.X<=MaxX && P.Y>=MinY && P.Y<=MaxY;}
float ChultExpansionGroundZ(const FVector& P)
{
    if(!InChultExpansion(P)) return 0;
    if(InChultCanopyReserve(P,350)) return 0;
    const float Fade=Ease((P.X-MinX)/1000)*Ease((P.Y-MinY)/800)*Ease((MaxX-P.X)/800)*Ease((MaxY-P.Y)/800)
        *Ease((P.Size2D()-9000)/600)*Ease(CanopyDistance(P)/600);
    if(Fade<=0) return 0;
    const float Hill=70*FMath::Exp(-FVector::DistSquared2D(P,FVector(14800,6000,0))/(1800.f*1800.f));
    float Z=70+35*FMath::Sin(P.X*.00063f+P.Y*.00021f)+28*FMath::Sin(P.Y*.00082f-P.X*.00014f)+Hill;
    const auto Stream=StreamAt(P);const float Bank=Stream.Distance-Stream.Width;
    if(Bank<20) Z=Stream.Water-14;
    else if(Bank<200) Z=FMath::Lerp(Stream.Water-14,Stream.Water+26,Ease((Bank-20)/180));
    else if(Bank<500) Z=FMath::Lerp(Stream.Water+26,Z,Ease((Bank-200)/300));
    return FMath::Max(0.f,Z*Fade);
}

void BuildChultExpansion(AActor* Owner)
{
    const FVector G=AChultLanding::FloorOrigin();
    // Only the two strips beyond the original 130 m square: no overlapping floors.
    auto Floor=[&](FVector Center,FVector Scale)
    {
        auto* P=NewObject<UStaticMeshComponent>(Owner);P->SetupAttachment(Owner->GetRootComponent());
        P->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));P->SetMaterial(0,Material(TEXT("M_FallGround")));
        P->SetWorldLocation(G+Center);P->SetWorldScale3D(Scale);P->SetCollisionProfileName(TEXT("BlockAll"));P->SetCastShadow(false);P->RegisterComponent();
    };
    Floor(FVector(17500,8750,-50),FVector(90,135,1));Floor(FVector(10000,14250,-50),FVector(60,25,1));
    FSurface Ground,Water,Roots,Moss;
    constexpr int32 NX=300,NY=270;
    float MaxHeight=0;
    for(int32 Y=0;Y<=NY;++Y) for(int32 X=0;X<=NX;++X)
    {
        FVector P(MinX+Cell*X,MinY+Cell*Y,0);P.Z=ChultExpansionGroundZ(P);MaxHeight=FMath::Max(MaxHeight,static_cast<float>(P.Z));
        Ground.Add(P,FVector::UpVector,FVector2D(P.X/260,P.Y/260));
    }
    for(int32 Y=0;Y<=NY;++Y) for(int32 X=0;X<=NX;++X)
    {
        const int32 I=Y*(NX+1)+X;
        const float DX=(Ground.V[Y*(NX+1)+FMath::Min(NX,X+1)].Z-Ground.V[Y*(NX+1)+FMath::Max(0,X-1)].Z)/(Cell*(X>0 && X<NX?2:1));
        const float DY=(Ground.V[FMath::Min(NY,Y+1)*(NX+1)+X].Z-Ground.V[FMath::Max(0,Y-1)*(NX+1)+X].Z)/(Cell*(Y>0 && Y<NY?2:1));
        Ground.N[I]=FVector(-DX,-DY,1).GetSafeNormal();Ground.Tangents[I]=FProcMeshTangent(FVector(1,0,DX).GetSafeNormal(),false);
        if(X<NX && Y<NY && FMath::Max(FMath::Max(Ground.V[I].Z,Ground.V[I+1].Z),FMath::Max(Ground.V[I+NX+1].Z,Ground.V[I+NX+2].Z))>.02)
        {Ground.Tri(I,I+NX+1,I+1);Ground.Tri(I+1,I+NX+1,I+NX+2);}
    }
    Finish(Owner,TEXT("ChultExpansionTerrain"),Ground,Material(TEXT("M_FallGround")),true);
    const auto& Path=StreamPath();float Length=0;
    for(int32 I=0;I<Path.Num();++I)
    {
        if(I) Length+=FVector::Dist2D(Path[I],Path[I-1]);
        const FVector Along=(Path[FMath::Min(I+1,Path.Num()-1)]-Path[FMath::Max(0,I-1)]).GetSafeNormal2D(),Side(-Along.Y,Along.X,0);
        const float Width=62+15*FMath::Sin(Length*.0013f)+100;
        for(int32 K=0;K<5;++K)
        {
            const float U=K/4.f,Across=(U*2-1)*Width;
            const uint8 Alpha=K==0 || K==4?0:220;
            Water.Add(Path[I]+Side*Across+FVector(0,0,.5f),FVector::UpVector,FVector2D(U,Length/230),FColor(255,255,255,Alpha));
            Water.Tangents.Last()=FProcMeshTangent(Side,true);
            if(I>0 && K<4) {int32 A=(I-1)*5+K;Water.Tri(A,A+1,A+5);Water.Tri(A+1,A+6,A+5);}
        }
    }
    // Rounded source/outlet pools follow the bed's endpoint distance field.
    for(int32 End:{0,Path.Num()-1})
    {
        const FVector D=End==0?(Path[0]-Path[1]).GetSafeNormal2D():(Path.Last()-Path[Path.Num()-2]).GetSafeNormal2D(),Side(-D.Y,D.X,0);
        const float Width=62+15*FMath::Sin((End==0?0:Length)*.0013f)+100;
        const int32 Center=Water.Add(Path[End]+FVector(0,0,.5f),FVector::UpVector,FVector2D(.5f,End==0?0:Length/230),FColor(255,255,255,220));
        for(int32 J=0;J<=20;++J)
        {
            const float A=-PI*.5f+PI*J/20.f;
            Water.Add(Path[End]+(D*FMath::Cos(A)+Side*FMath::Sin(A))*Width+FVector(0,0,.5f),FVector::UpVector,FVector2D(.5f+.5f*FMath::Sin(A),(End==0?0:Length)/230+FMath::Cos(A)*Width/230),FColor(255,255,255,0));
            if(J) Water.Tri(Center,Center+1+J,Center+J);
        }
    }
    auto* StreamMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/ChultTerrain/M_ChultStream.M_ChultStream"));
    auto* Surface=Finish(Owner,TEXT("ChultExpansionStream"),Water,StreamMat,false);Surface->SetCastShadow(false);
    TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;
    int32 Missing=StreamMat==nullptr,Trees=0,Ferns=0,Palms=0,Rocks=0,Strands=0;
    auto Place=[&](const FString& Name,FVector At,float Yaw,float Scale,bool Solid=false,bool Hidden=false)
    {
        auto*& H=Sets.FindOrAdd(Name);
        if(!H)
        {
            const TCHAR* Dir=Name.StartsWith(TEXT("SM_Chult"))?TEXT("/Game/Art/ChultNature"):TEXT("/Game/Art/Cutscenes/FallToChult");
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner);H->SetupAttachment(Owner->GetRootComponent());H->SetWorldLocation(G);
            H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),Dir,*Name,*Name)));Missing+=H->GetStaticMesh()==nullptr;
            H->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);if(Solid) H->SetCollisionProfileName(TEXT("BlockAll"));
            H->SetHiddenInGame(Hidden);H->SetCastShadow(!Hidden);H->SetCullDistances(Name.StartsWith(TEXT("SM_ChultFern"))?4500:8000,Name.StartsWith(TEXT("SM_ChultFern"))?6500:11000);H->RegisterComponent();
        }
        At.Z+=ChultExpansionGroundZ(At);H->AddInstance(FTransform(FRotator(0,Yaw,0),G+At,FVector(Scale)),true);
    };
    auto Trunk=[&](FVector At,float Radius,float Height)
    {
        auto* C=NewObject<UCapsuleComponent>(Owner);C->InitCapsuleSize(Radius,Height*.5f);C->SetCollisionProfileName(TEXT("BlockAll"));C->SetHiddenInGame(true);
        C->SetupAttachment(Owner->GetRootComponent());C->SetWorldLocation(G+At+FVector(0,0,Height*.5f-Radius));C->RegisterComponent();
    };
    auto RootSolid=[&](const TArray<FVector>& V)
    {
        const int32 B=Roots.V.Num();for(const FVector P:V) Roots.Add(P,FVector::UpVector,FVector2D::ZeroVector);
        const int32 Faces[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
        for(int32 I=0;I<UE_ARRAY_COUNT(Faces);I+=3) if(FVector::CrossProduct(V[Faces[I+1]]-V[Faces[I]],V[Faces[I+2]]-V[Faces[I]]).SizeSquared()>1.e-4f) Roots.Tri(B+Faces[I],B+Faces[I+1],B+Faces[I+2]);
    };
    auto RootProfile=[&](FVector At,float Yaw,float Scale,bool A)
    {
        const auto* Profiles=A?ChultRoots::A:ChultRoots::B;const float Radius=A?78:62,Height=A?ChultRoots::HeightA:ChultRoots::HeightB;
        const FTransform T(FRotator(0,Yaw,0),At,FVector(Scale));
        const int32 Count=A?UE_ARRAY_COUNT(ChultRoots::A):UE_ARRAY_COUNT(ChultRoots::B);
        for(int32 I=0;I<Count;++I) for(int32 J=0;J<8;++J)
        {
            const auto& R=Profiles[I];const FVector Out=FRotator(0,R.Angle,0).Vector(),Side(-Out.Y,Out.X,0);TArray<FVector> V;
            for(int32 E=0;E<2;++E)
            {
                float Reach,Z,W;ChultRoots::FinEdge(R,Radius,Height,(J+E)/8.f,Reach,Z,W);
                for(FVector P:{Out*Reach-Side*W+FVector(0,0,-10),Out*Reach+Side*W+FVector(0,0,-10),Out*Reach+Side*W+FVector(0,0,Z),Out*Reach-Side*W+FVector(0,0,Z)}) V.Add(T.TransformPosition(P));
            }
            RootSolid(V);
        }
        for(int32 K=0;K<12;++K)
        {
            const FVector A0=FRotator(0,K*30.f,0).Vector(),B0=FRotator(0,(K+1)*30.f,0).Vector();TArray<FVector> V;
            for(FVector P:{FVector(0,0,-10),A0*Radius*2.1f+FVector(0,0,-10),B0*Radius*2.1f+FVector(0,0,-10),FVector(0,0,-10),
                FVector(0,0,360),A0*Radius+FVector(0,0,360),B0*Radius+FVector(0,0,360),FVector(0,0,360)}) V.Add(T.TransformPosition(P));
            RootSolid(V);
        }
    };
    FRandomStream R(20261014);
    for(float Y=2400;Y<15000;Y+=860) for(float X=7500;X<21500;X+=860)
    {
        FVector At(X+R.FRandRange(-250,250),Y+R.FRandRange(-250,250),0);
        const auto Stream=StreamAt(At);
        if(At.Size2D()<9400 || InChultCanopyReserve(At,900) || OnLane(At,650) || Stream.Distance<Stream.Width+600) continue;
        const float Yaw=R.FRandRange(0,360),Scale=R.FRandRange(.8f,1.15f);const bool A=R.FRand()<.5f;
        Place(A?TEXT("SM_FallTree_A"):TEXT("SM_FallTree_B"),At,Yaw,Scale);
        At.Z=ChultExpansionGroundZ(At)-3;Trunk(At,(A?78:62)*Scale,(A?ChultRoots::HeightA:ChultRoots::HeightB)*Scale);RootProfile(At,Yaw,Scale,A);++Trees;
        // Hanging moss, out of traversal lanes and above the ground plants.
        if(Trees%3==0) for(int32 S=0;S<12;++S)
        {
            const float Angle=R.FRandRange(0,2*PI),BarkRadius=(A?78.f:62.f)*Scale*.9f;
            const FVector Start=At+FVector(FMath::Cos(Angle)*BarkRadius,FMath::Sin(Angle)*BarkRadius,R.FRandRange(500,760));const float L=R.FRandRange(170,420),W=R.FRandRange(3,8);
            const FVector Side=FRotator(0,R.FRandRange(0,360),0).Vector();const int32 Base=Moss.V.Num();
            for(int32 J=0;J<=12;++J)
            {
                const float U=J/12.f;const FVector P=Start+FVector(0,0,-L*U)+Side*FMath::Sin(U*5+S)*15*U;
                for(float Sign:{-1.f,1.f}) Moss.Add(P+Side*Sign*W*(1-.9f*U),FVector::CrossProduct(Side,FVector::UpVector),FVector2D(Sign>0?1:0,U*4),FColor(190,180,80,FMath::RoundToInt(U*255)));
                if(J<12) {int32 A0=Base+J*2;Moss.Tri(A0,A0+2,A0+1);Moss.Tri(A0+1,A0+2,A0+3);}
            }
            ++Strands;
        }
    }
    for(int32 I=0;I<1800;++I)
    {
        FVector At(R.FRandRange(7500,21500),R.FRandRange(2400,15000),-2);
        if(At.Size2D()<9200 || OnLane(At,150) || StreamAt(At).Distance<400) continue;
        bool Foot=false;for(const auto& T:ChultCanopyTrees()) Foot|=FVector::Dist2D(At,T.LianaFoot)<350;
        if(Foot) continue;
        const float Yaw=R.FRandRange(0,360),Scale=R.FRandRange(1.3f,2.f);const int32 Type=R.RandRange(0,5);
        if(Type<3) {Place(FString::Printf(TEXT("SM_ChultFern%d"),I%4),At,Yaw,Scale);++Ferns;}
        else if(Type<5) Place(Type==3?TEXT("SM_FallElephantEar"):TEXT("SM_FallShrub"),At,Yaw,Scale*.65f);
        else if(!InChultCanopyReserve(At,650) && !OnLane(At,500) && StreamAt(At).Distance>500)
        {Place(TEXT("SM_FallPalm"),At,Yaw,Scale*.7f);At.Z=ChultExpansionGroundZ(At);Trunk(At,14*Scale*.7f,700*Scale*.7f);++Palms;}
    }
    int32 NaturalTrees=0;
    for(int32 I=0;I<100 && NaturalTrees<16;++I)
    {
        FVector At(R.FRandRange(9500,21000),R.FRandRange(3200,14800),-5);
        if(InChultCanopyReserve(At,1100) || OnLane(At,700) || StreamAt(At).Distance<750) continue;
        const float Yaw=R.FRandRange(0,360),Scale=R.FRandRange(1.6f,2.3f);
        Place(TEXT("SM_ChultIslandTree"),At,Yaw,Scale);Place(TEXT("SM_ChultIslandTrunkCollision"),At,Yaw,Scale,true,true);++NaturalTrees;
    }
    for(int32 I=0;I<Path.Num();I+=2) for(float Sign:{-1.f,1.f})
    {
        const FVector Along=(Path[FMath::Min(I+1,Path.Num()-1)]-Path[FMath::Max(0,I-1)]).GetSafeNormal2D(),Side(-Along.Y,Along.X,0);
        FVector At=Path[I]+Side*Sign*R.FRandRange(270,410);At.Z=-6;
        if(InChultCanopyReserve(At,400) || OnLane(At,240)) continue;
        Place(FString::Printf(TEXT("SM_ChultRock%d"),Rocks%6),At,R.FRandRange(0,360),R.FRandRange(.35f,.65f),true);++Rocks;
        Place(FString::Printf(TEXT("SM_ChultFern%d"),I%4),At+Side*Sign*130,R.FRandRange(0,360),1.8f);++Ferns;
    }
    auto* RootMesh=Finish(Owner,TEXT("ChultExpansionRoots"),Roots,nullptr,true);RootMesh->SetHiddenInGame(true);RootMesh->SetCastShadow(false);
    Finish(Owner,TEXT("ChultExpansionHangingMoss"),Moss,Material(TEXT("M_ChultHangingMoss")),false);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_EXPANSION trees=%d ferns=%d palms=%d rocks=%d moss_strands=%d missing_assets=%d terrain_triangles=%d max_height_cm=%.1f stream_samples=%d water_collision=0 natural_trees=%d"),Trees,Ferns,Palms,Rocks,Strands,Missing,Ground.T.Num()/3,MaxHeight,Path.Num(),NaturalTrees);
}

int32 CheckChultExpansion(AActor* Owner,AChuckCharacter* C)
{
    int32 Failures=0,Samples=0,Sweeps=0,Fords=0;UWorld* World=Owner->GetWorld();const FVector G=AChultLanding::FloorOrigin();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultExpansion),false,C);const float Half=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=C->GetCapsuleComponent()->GetScaledCapsuleRadius();
    auto Sample=[&](FVector At)
    {
        FHitResult Hit;const float Z=ChultExpansionGroundZ(At);
        const bool Floor=World->LineTraceSingleByChannel(Hit,G+At+FVector(0,0,Z+250),G+At+FVector(0,0,-30),ECC_Visibility,Q);
        if(!Floor || FMath::Abs(Hit.ImpactPoint.Z-G.Z-Z)>6 || Hit.ImpactNormal.Z<.82f) {++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_EXPANSION_FLOOR at=%s expected=%.1f actual=%.1f normal_z=%.3f hit=%s"),*At.ToString(),Z,Hit.ImpactPoint.Z-G.Z,Hit.ImpactNormal.Z,*GetNameSafe(Hit.GetComponent()));}
        ++Samples;return G+At+FVector(0,0,Hit.ImpactPoint.Z-G.Z+Half+3);
    };
    for(int32 I=1;I<UE_ARRAY_COUNT(WalkLoop);++I)
    {
        const FVector A=WalkLoop[I-1],B=WalkLoop[I];const int32 Steps=FMath::CeilToInt(FVector::Dist2D(A,B)/35.f);FVector Previous=Sample(A);
        for(int32 J=1;J<=Steps;++J)
        {
            const FVector At=FMath::Lerp(A,B,J/static_cast<float>(Steps)),Body=Sample(At);FHitResult Hit;
            const bool Blocked=World->SweepSingleByChannel(Hit,Previous,Body,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q);
            if(Blocked) {++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_EXPANSION_BLOCK at=%s component=%s"),*At.ToString(),*GetNameSafe(Hit.GetComponent()));}
            const auto Stream=StreamAt(At);if(Stream.Distance<Stream.Width+40) ++Fords;
            Previous=Body;++Sweeps;
        }
    }
    for(const auto& T:ChultCanopyTrees()) Failures+=FMath::Abs(ChultExpansionGroundZ(T.LianaFoot))>.01f;
    for(const FVector At:{FVector(18000,5000,0),FVector(20000,13500,0),FVector(12000,14500,0)}) {Sample(At);Failures+=!AChultLanding::Contains(G+At);}
    for(const FVector P:StreamPath()) {const float Depth=P.Z-ChultExpansionGroundZ(P);Failures+=Depth<8 || Depth>25;}
    Failures+=Fords==0;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_EXPANSION_CHECK failures=%d floor_samples=%d capsule_sweeps=%d ford_samples=%d canopy_feet=8 extent=1"),Failures,Samples,Sweeps,Fords);return Failures;
}

void StartChultExpansionReview(AActor* Owner,AChuckCharacter* C)
{
    // Three short real-character walks: rolling ground, a shallow ford, and the far floor.
    struct FRun {int32 Trial=0,Failures=0,Frames=0;float Start=-1,Worst=0;double FrameTime=0;uint64 LastFrame=MAX_uint64;FTimerHandle Timer;};auto Run=MakeShared<FRun>();
    const FVector G=AChultLanding::FloorOrigin();
    const TArray<FVector> Starts={{14300,6500,0},{16600,10700,0},{18300,11800,0}};
    const TArray<FVector> Ends={{15100,7000,0},{16800,11300,0},{19000,12200,0}};
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,C,Run,G,Starts,Ends]()
    {
        if(Run->Trial>=Starts.Num()) return;
        const float Now=Owner->GetWorld()->GetTimeSeconds();const FVector A=Starts[Run->Trial],B=Ends[Run->Trial];
        if(Run->Start<0)
        {
            FVector Spawn=G+A+FVector(0,0,ChultExpansionGroundZ(A)+C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+4);
            C->ResetAtLocation(Spawn);C->GetCharacterMovement()->StopMovementImmediately();Run->Start=Now;
            C->SetActorRotation((B-A).Rotation());C->Recenter();C->SetOrbitPitch(-12);
            Run->Frames=0;Run->FrameTime=0;Run->Worst=0;
        }
        if(Run->LastFrame!=GFrameCounter) {const float DT=Owner->GetWorld()->GetDeltaSeconds();++Run->Frames;Run->FrameTime+=DT;Run->Worst=FMath::Max(Run->Worst,DT);Run->LastFrame=GFrameCounter;}
        const float Distance=FVector::Dist2D(C->GetActorLocation(),G+B),Time=Now-Run->Start;
        if(Distance>45 && Time<20)
        {
            const FVector Direction=(G+B-C->GetActorLocation()).GetSafeNormal2D();
            // Match the existing ruin harness: special moves read the raw stick,
            // ordinary walking also receives the input normally supplied by axes.
            C->SetTestStickWorld(Direction);C->AddMovementInput(Direction,1.f,true);return;
        }
        C->SetTestStick(FVector2D::ZeroVector);
        const bool Pass=Distance<45 && C->GetCharacterMovement()->IsMovingOnGround();Run->Failures+=!Pass;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_EXPANSION_WALK trial=%d pass=%d distance=%.1f seconds=%.2f ground=%d fps=%.1f worst_frame_ms=%.1f"),Run->Trial,Pass,Distance,Time,C->GetCharacterMovement()->IsMovingOnGround(),Run->Frames/FMath::Max(.001,Run->FrameTime),Run->Worst*1000);
        ++Run->Trial;Run->Start=-1;
        if(Run->Trial==Starts.Num()) {UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_EXPANSION_TEST_COMPLETE failures=%d trials=3"),Run->Failures);StartChultCliffReview(Owner,C,Run->Failures);}
    },.016f,true);
}
