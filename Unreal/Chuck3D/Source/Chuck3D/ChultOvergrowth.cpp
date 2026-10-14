#include "ChultOvergrowth.h"
#include "ChultLanding.h"
#include "ChultExpansion.h"
#include "ChultCanopy.h"
#include "ChultRuins.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "HAL/IConsoleManager.h"

namespace
{
    bool Clear(FVector P,float Margin)
    {
        if(P.Size2D()>9700 && !InChultExpansion(P)) return false;
        if(AChultLanding::OnGroundRoute(P,Margin) || InChultRuinsReserve(P,Margin)) return false;
        for(const auto& Tree:ChultCanopyTrees())
            if(FVector::Dist2D(P,Tree.LianaFoot)<350 || FVector::Dist2D(P,Tree.Trunk)<220) return false;
        return !InChultExpansion(P) || ChultExpansionDressingAllowed(P,Margin);
    }
}

void BuildChultOvergrowth(AActor* Owner)
{
    const FVector G=AChultLanding::FloorOrigin();
    FRandomStream R(2026101405);
    TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;
    int32 Missing=0,Ferns=0,Leaves=0,Rocks=0,Clusters=0;
    auto Place=[&](FString Name,FVector At,FVector Scale,FRotator Rotation,bool Solid=false)
    {
        auto*& H=Sets.FindOrAdd(Name);
        if(!H)
        {
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,*FString::Printf(TEXT("ChultOvergrowth_%s"),*Name));
            const TCHAR* Dir=Name.StartsWith(TEXT("SM_Chult"))?TEXT("/Game/Art/ChultNature"):TEXT("/Game/Art/Cutscenes/FallToChult");
            H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),Dir,*Name,*Name)));
            Missing+=!H->GetStaticMesh();H->SetupAttachment(Owner->GetRootComponent());H->SetWorldLocation(G);
            H->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
            if(Solid) H->SetCollisionProfileName(TEXT("BlockAll"));
            H->SetCullDistances(Solid?8500:5500,Solid?12000:8000);H->RegisterComponent();
        }
        At.Z+=ChultExpansionGroundZ(At);
        H->AddInstance(FTransform(Rotation,G+At,Scale),true);
    };
    // Irregular colonies rather than a uniform lawn: ground cover, broad leaves,
    // then taller shrubs behind them. The original clearing/camera lane stays open.
    for(int32 I=0;I<1300;++I)
    {
        FVector Center;
        if(I<760) Center=FVector(R.FRandRange(-9400,9600),R.FRandRange(-9400,9600),0);
        else Center=FVector(R.FRandRange(7500,21400),R.FRandRange(2400,14900),0);
        if(!Clear(Center,330)) continue;
        ++Clusters;
        const float Size=R.FRandRange(130,310);
        for(int32 J=0;J<14;++J)
        {
            const float A=R.FRandRange(0,2*PI),D=Size*FMath::Sqrt(R.FRand());
            FVector At=Center+FVector(FMath::Cos(A)*D,FMath::Sin(A)*D,-2);
            if(!Clear(At,240)) continue;
            const float Scale=R.FRandRange(1.9f,3.5f),Yaw=R.FRandRange(0,360);
            if(J%4<2)
            {Place(FString::Printf(TEXT("SM_ChultFern%d"),(I+J)%4),At,FVector(Scale),FRotator(0,Yaw,0));++Ferns;}
            else if(!InChultCanopyReserve(At,150))
            {Place(J%4==2?TEXT("SM_FallElephantEar"):TEXT("SM_FallShrub"),At,FVector(Scale*.48f,Scale*.48f,Scale*R.FRandRange(.4f,.7f)),FRotator(0,Yaw,0));++Leaves;}
        }
        // Embedded scan outcrops give fern colonies a different silhouette.
        // A large reserve includes the entire rock extent, not just its pivot.
        if(I%9==0 && Clear(Center,750) && !InChultCanopyReserve(Center,1000))
        {
            for(int32 J=0;J<3;++J)
            {
                FVector At=Center+FVector(J*80-80,J*45,-35);
                if(!Clear(At,650)) continue;
                Place(FString::Printf(TEXT("SM_ChultRock%d"),(I+J)%6),At,
                    FVector(R.FRandRange(.65f,1.15f),R.FRandRange(.6f,1.f),R.FRandRange(.7f,1.25f)),FRotator(0,R.FRandRange(0,360),0),true);++Rocks;
            }
        }
    }
    // Fern pockets on the ground inside the grove, beneath the swing body arc.
    // Their small size and generous foot reserve preserve catches/dismounts.
    for(int32 I=0;I<650;++I)
    {
        FVector At(R.FRandRange(5100,11900),R.FRandRange(400,6800),-2);
        if(!InChultCanopyReserve(At) || !Clear(At,260)) continue;
        Place(FString::Printf(TEXT("SM_ChultFern%d"),I%4),At,FVector(R.FRandRange(1.4f,2.f)),FRotator(0,R.FRandRange(0,360),0));++Ferns;
    }
    // A natural mid-story silhouette breaks up the inherited straight trunks.
    // Original trees are preserved. New scanned trunks stay outside all routes.
    int32 NaturalTrees=0;
    for(int32 I=0;I<140 && NaturalTrees<24;++I)
    {
        FVector At=I<85?FVector(R.FRandRange(-8500,8800),R.FRandRange(-8500,8800),-8):FVector(R.FRandRange(12800,20800),R.FRandRange(3900,14200),-8);
        if(At.Size2D()<2300 || !Clear(At,1050) || InChultCanopyReserve(At,1600)) continue;
        const float Scale=R.FRandRange(1.7f,2.4f);const FRotator Rotation(0,R.FRandRange(0,360),0);
        Place(TEXT("SM_ChultIslandTree"),At,FVector(Scale),Rotation);
        Place(TEXT("SM_ChultIslandTrunkCollision"),At,FVector(Scale),Rotation,true);
        Sets[TEXT("SM_ChultIslandTrunkCollision")]->SetHiddenInGame(true);
        Sets[TEXT("SM_ChultIslandTrunkCollision")]->SetCastShadow(false);
        ++NaturalTrees;
    }
    // Vegetation on the OUTER ruin edges, clear of the paired stairwell walls,
    // cornices, gap landings and the causeway. The original masonry stays intact.
    for(int32 I=0;I<180;++I)
    {
        FVector At(R.FRandRange(6300,7520),R.FRandRange(-1830,-470),-2);
        if(FMath::Abs(At.Y+1250)<260 || FVector::Dist2D(At,FVector(7100,-1600,0))<260
            || FVector::Dist2D(At,FVector(6720,-950,0))<200 || FVector::Dist2D(At,FVector(7280,-950,0))<200) continue;
        Place(FString::Printf(TEXT("SM_ChultFern%d"),I%4),At,FVector(R.FRandRange(1.6f,2.7f)),FRotator(0,R.FRandRange(0,360),0));++Ferns;
    }
    for(float X:{6670.f,7330.f}) for(int32 J=0;J<8;++J)
    {
        FVector At(X,-1000+J*15.f,355);
        Place(TEXT("SM_FallElephantEar"),At,FVector(.35f),FRotator(0,R.FRandRange(0,360),0));++Leaves;
    }
    for(int32 J=0;J<16;++J)
    {
        FVector At(6870+J*39.f,-1335,95);
        Place(FString::Printf(TEXT("SM_ChultFern%d"),J%4),At,FVector(.9f),FRotator(0,R.FRandRange(0,360),0));++Ferns;
    }
    // Root-like creepers hang down the outer gateway piers, with small leaf
    // rosettes. Nothing spans a gameplay gap or becomes a new climbing surface.
    TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;TArray<FColor> C;TArray<FProcMeshTangent> Tangents;
    for(float X:{6617.f,7383.f}) for(int32 Strand=0;Strand<7;++Strand)
    {
        const float Length=R.FRandRange(190,330),Y=-1020+Strand*22.f;
        const int32 Base=V.Num();
        for(int32 J=0;J<=20;++J)
        {
            const float U=J/20.f;
            const FVector P(X+FMath::Sin(U*7+Strand)*9,Y+FMath::Sin(U*4)*7,355-Length*U);
            for(int32 K=0;K<=5;++K)
            {
                const float A=K*2*PI/5;const FVector Normal(FMath::Cos(A),FMath::Sin(A),0);
                V.Add(P+Normal*(2.5f-U));N.Add(Normal);UV.Add(FVector2D(K/5.f,U*3));C.Add(FColor::White);Tangents.Add(FProcMeshTangent(FVector::UpVector,false));
                if(J<20 && K<5) {const int32 B=Base+J*6+K;T.Append({B,B+6,B+1,B+1,B+6,B+7});}
            }
            if(J%5==0) {Place(TEXT("SM_FallElephantEar"),P,FVector(.17f),FRotator(0,Strand*53.f,0));++Leaves;}
        }
    }
    auto* Vine=NewObject<UProceduralMeshComponent>(Owner,TEXT("ChultOvergrowthCreepers"));
    Vine->SetupAttachment(Owner->GetRootComponent());Vine->SetWorldLocation(G);Vine->RegisterComponent();
    Vine->CreateMeshSection(0,V,T,N,UV,C,Tangents,false);Vine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Cutscenes/FallToChult/M_FallBark.M_FallBark"));Vine->SetMaterial(0,Mat);Missing+=!Mat;
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_OVERGROWTH clusters=%d ferns=%d broad_leaves=%d rocks=%d creepers=14 triangles=%d missing_assets=%d foliage_collision=0 rock_collision=1 existing_removed=0 natural_trees=%d"),Clusters,Ferns,Leaves,Rocks,T.Num()/3,Missing,NaturalTrees);
}

void BuildChultLowMist(AActor* Owner)
{
    const FVector G=AChultLanding::FloorOrigin();
    // UE defaults to a 20 m near exclusion for local fog. Chuck's camera and
    // these ground pools need close-range integration (the rest of the game
    // has no local fog volumes). This is a runtime value, not a saved setting.
    if(auto* Start=IConsoleManager::Get().FindConsoleVariable(TEXT("r.LocalFogVolume.GlobalStartDistance")))
        Start->Set(30.f,ECVF_SetByCode);
    // Soft bounded pools in the undergrowth and beside the stream. Uniform
    // volume scaling is required by UE; the height profile supplies the low bank.
    const FVector Pools[]={{600,1000,0},{2100,600,0},{3700,2100,0},{-1700,2200,0},
        {2800,-2800,0},{6200,-1750,0},{7800,3800,0},{9800,7700,0},
        {12300,8700,0},{15300,10200,0},{17900,11500,0},{20300,13900,0}};
    for(int32 I=0;I<UE_ARRAY_COUNT(Pools);++I)
    {
        FVector At=Pools[I];At.Z=ChultExpansionGroundZ(At)-700;
        auto* Fog=NewObject<ULocalFogVolumeComponent>(Owner,*FString::Printf(TEXT("ChultLowMist%d"),I));
        Fog->SetupAttachment(Owner->GetRootComponent());Fog->SetWorldLocation(G+At);Fog->SetWorldScale3D(FVector(2.6f));
        // UE multiplies radial and height COVERAGE: zero radial coverage
        // suppresses the height bank too. Radial coverage supplies soft edges.
        Fog->RadialFogExtinction=.8f;Fog->HeightFogExtinction=.22f;
        Fog->HeightFogFalloff=2400;Fog->HeightFogOffset=.61f;
        Fog->FogAlbedo=FLinearColor(.64f,.74f,.8f);Fog->FogPhaseG=.1f;Fog->RegisterComponent();
        Fog->SetFogEmissive(FLinearColor(.03f,.045f,.055f));
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_LOW_MIST pools=12 height_layer=1 cool_sun=1 collision=0"));
}
