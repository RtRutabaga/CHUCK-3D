#include "ChultRiver.h"
#include "ChultCanyon.h"
#include "ChultLandform.h"
#include "ChultGorge.h"
#include "ChuckCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "HAL/PlatformMisc.h"

namespace
{
    FVector Frame(FVector P){P-=ChultCanyonFrame(0,0);return FVector(P.X*.8+P.Y*.6,-P.X*.6+P.Y*.8,P.Z);}
    float Ease(float U){U=FMath::Clamp(U,0.f,1.f);return U*U*(3-2*U);}
    // Upstream of S=28000 (Claude, 2026-10-15) it swings toward +T to its head:
    // a pool under the source falls (ChultUpriver.cpp) at S=ChultRiverHead.
    float Center(float S){float U=FMath::Clamp((S-4500)/23500,0.f,1.f);return 2200+1650*FMath::Sin(U*3*PI)*FMath::Sin(U*PI)+3000*Ease((S-28000)/(ChultRiverHead-28000));}
    float Width(float S){return 460+90*FMath::Sin((S-4500)*.0007f)+260*Ease((S-ChultRiverHead+1400)/1400);}
    float WaterZ(float S){return 280+(S-4500)*.022f;}
    FVector Surface(float S,float Offset=0){return ChultCanyonFrame(S,Center(S)+Offset,WaterZ(S));}
    FVector Along(){return FVector(.8,.6,0);}
    FVector AcrossDir(){return FVector(-.6,.8,0);}
    // The broad fall itself is ChultGorge.cpp's fall 1 (Claude, 2026-10-15):
    // the river now leaves the lip and plunges free into the gorge mist.
    FVector Fall(float U,float Offset=0){return ChultGorgeFall(1,U*ChultGorgeFallDrop(1),Offset/920.f);}
    void FlowQuad(FChultLandform& M,FVector A,FVector B,FVector C,FVector D,float V0,float V1,float L,float H)
    {
        int32 Start=M.V.Num();M.Quad(A,B,C,D);
        // Quad emits A,B,C,A,C,D. Material-specific V follows the flow.
        const FVector2D UVs[]={{L,V0},{H,V0},{H,V1},{L,V0},{H,V1},{L,V1}};
        for(int32 I=0;I<6;++I){M.UV[Start+I]=UVs[I];float U=UVs[I].X;M.C[Start+I]=FColor(255,255,255,U<=0||U>=1?0:225);}
    }
}
bool InChultRiverExtension(const FVector& P){const FVector F=Frame(P);return F.X>=4500 && F.X<=ChultFarFloorEnd && FMath::Abs(F.Y)<=6500;}
float ChultRiverDistance(const FVector& P)
{
    const FVector F=Frame(P);if(F.X<4500)return 1.e9f;
    // Past its head the distance is to the head itself: a rounded pool.
    if(F.X>ChultRiverHead)return FVector2D(F.X-ChultRiverHead,F.Y-Center(ChultRiverHead)).Size();
    return FMath::Abs(static_cast<float>(F.Y)-Center(F.X));
}
float ChultRiverCenter(float S){return Center(S);}
float ChultRiverWidth(float S){return Width(FMath::Min(S,ChultRiverHead));}
float ChultRiverWaterZ(float S){return WaterZ(FMath::Min(S,ChultRiverHead));}
float ChultRiverGroundZ(const FVector& P,float Base)
{
    const FVector F=Frame(P);if(F.X<4500||F.X>ChultRiverHead+3000)return Base;
    const float S=FMath::Min(static_cast<float>(F.X),ChultRiverHead),D=ChultRiverDistance(P),W=Width(S);
    // Keep both banks above the water even where the broad terrain relief dips.
    Base=FMath::Lerp(Base,FMath::Max(Base,WaterZ(S)+70),1-Ease((D-W-900)/900));
    return FMath::Lerp(WaterZ(S)-20,Base,Ease((D-W+75)/420));
}
void BuildChultRiver(AActor* Owner)
{
    FChultLandform Ground,Water,Rock;const FVector G=AChultLanding::FloorOrigin();
    // Shared edge at S=11500: both banks use ChultCanyonGroundZ exactly.
    for(float S=4500;S<28000;S+=100)for(float T=-6500;T<6500;T+=100)
    {
        if(S<11500&&T>=-4500&&T<4500)continue;
        FVector A=ChultCanyonFrame(S,T),B=ChultCanyonFrame(S+100,T),C=ChultCanyonFrame(S+100,T+100),D=ChultCanyonFrame(S,T+100);
        for(FVector* P:{&A,&B,&C,&D})P->Z=ChultCanyonGroundZ(*P);Ground.Quad(A,B,C,D);
    }
    for(int32 I=0;I<Ground.V.Num();++I)Ground.UV[I]=FVector2D(Ground.V[I].X/260,Ground.V[I].Y/260);
    // Dense cross-sections follow both bends and the downhill water profile.
    for(float S=ChultRiverHead;S>4500;S-=50)for(int32 K=0;K<8;++K)
    {
        const float L=K/8.f,H=(K+1)/8.f;
        FlowQuad(Water,Surface(S,(L*2-1)*Width(S)),Surface(S,(H*2-1)*Width(S)),Surface(S-50,(H*2-1)*Width(S-50)),Surface(S-50,(L*2-1)*Width(S-50)),(S-28000)/480,(S-28050)/480,L,H);
    }
    // The head: a rounded pool end under the source falls.
    for(int32 K=0;K<16;++K)
    {
        const float A=-PI*.5f+K*PI/16,B=-PI*.5f+(K+1)*PI/16,W=Width(ChultRiverHead);
        const FVector C=Surface(ChultRiverHead);
        auto Rim=[&](float Angle){return C+(Along()*FMath::Cos(Angle)+AcrossDir()*FMath::Sin(Angle))*W;};
        const int32 Start=Water.V.Num();Water.Tri(C,Rim(A),Rim(B));
        for(int32 J=Start;J<Water.V.Num();++J){Water.UV[J]=FVector2D(.5f+FVector::DotProduct(Water.V[J]-C,AcrossDir())/(2*W),(ChultRiverHead-28000+FVector::DotProduct(Water.V[J]-C,Along()))/-480);Water.C[J]=FColor(255,255,255,J==Start?225:0);}
    }
    // Shoulders frame the broad fall without plugging its channel or overlook.
    for(int32 I=0;I<16;++I)
    {
        // Only the shoulders on the bank itself remain; the old ones down the
        // face sat on the previous wall profile.
        const float U=I/16.f,Z=FMath::Lerp(280.f,-4650.f,U);if(Z<150)continue;
        for(float Side:{-1.f,1.f})Rock.Rock(ChultCanyonFrame(4500-35*FMath::Min(U*10,1.f),2200+Side*(650+120*FMath::Sin(I*2.f)),Z)+FVector(.8,.6,0)*350-FVector(0,0,130),FVector(230,240,360),I*37,I+Side);
    }
    auto* Floor=Ground.Finish(Owner,TEXT("ChultRiverValley"),TEXT("/Game/Art/Cutscenes/FallToChult/M_FallGround.M_FallGround"));
    auto* River=Water.Finish(Owner,TEXT("ChultBroadRiver"),TEXT("/Game/Art/ChultTerrain/M_ChultStream.M_ChultStream"),false);
    auto* Shoulders=Rock.Finish(Owner,TEXT("ChultBroadFallsRock"),TEXT("/Game/Art/ChultTerrain/M_ChultCliff.M_ChultCliff"));
    River->SetCastShadow(false);
    int32 Missing=!Floor->GetMaterial(0)+!River->GetMaterial(0)+!Shoulders->GetMaterial(0),Trees=0,Plants=0;
    FRandomStream R(2026101501);TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;
    auto Place=[&](FString Name,FVector P,float Scale,float Yaw,bool Solid=false,bool Hidden=false)
    {
        auto*& H=Sets.FindOrAdd(Name);if(!H)
        {
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,*FString::Printf(TEXT("ChultRiver_%s"),*Name));
            const TCHAR* Dir=Name.StartsWith(TEXT("SM_Chult"))?TEXT("/Game/Art/ChultNature"):TEXT("/Game/Art/Cutscenes/FallToChult");
            H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),Dir,*Name,*Name)));Missing+=!H->GetStaticMesh();
            H->SetupAttachment(Owner->GetRootComponent());H->SetWorldLocation(G);H->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);if(Solid)H->SetCollisionProfileName(TEXT("BlockAll"));H->SetHiddenInGame(Hidden);H->SetCastShadow(!Hidden);H->SetCullDistances(Solid?10000:5500,Solid?18000:10000);H->RegisterComponent();
        }
        H->AddInstance(FTransform(FRotator(0,Yaw,0),G+P,FVector(Scale)),true);
    };
    for(int32 I=0;I<14000;++I)
    {
        float S=R.FRandRange(4700,27900),T=R.FRandRange(-6400,6400);if(S<11500&&FMath::Abs(T)<4500)continue;FVector P=ChultCanyonFrame(S,T);const float D=ChultRiverDistance(P),W=Width(S);
        // Two continuous clear bank walks; understory fills the jungle behind.
        if(D<W+160 || FMath::Abs(D-W-600)<210 || (S<14500&&FMath::Abs(T)<450))continue;
        P.Z=ChultCanyonGroundZ(P)-3;float Yaw=R.FRandRange(0,360);
        if(I%22==0 && D>W+1100)
        {
            float Scale=R.FRandRange(1.7,2.8);Place(TEXT("SM_ChultIslandTree"),P,Scale,Yaw);Place(TEXT("SM_ChultIslandTrunkCollision"),P,Scale,Yaw,true,true);++Trees;
            if(I%44==0)Place(TEXT("SM_FallPalm"),P+FVector(140,30,0),1.6f,Yaw);
        }
        else {int32 Type=I%6;Place(Type<4?FString::Printf(TEXT("SM_ChultFern%d"),Type):Type==4?TEXT("SM_FallElephantEar"):TEXT("SM_FallShrub"),P,R.FRandRange(1.5,2.6)*(Type<4?1:.65f),Yaw);++Plants;}
    }
    {
        // Spray at the lip; the plunge's own mist is in the gorge.
        const FVector P=Fall(.03f);
        auto* Fog=NewObject<ULocalFogVolumeComponent>(Owner);Fog->SetupAttachment(Owner->GetRootComponent());Fog->SetWorldLocation(G+P);Fog->SetWorldScale3D(FVector(1.1));Fog->RadialFogExtinction=.38;Fog->HeightFogExtinction=.16;Fog->HeightFogFalloff=1800;Fog->HeightFogOffset=.6;Fog->FogAlbedo=FLinearColor(.66,.78,.73);Fog->RegisterComponent();
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_RIVER extension_m=165 width_cm=920 fall_cm=4930 trees=%d plants=%d missing_assets=%d water_collision=0 triangles=%d"),Trees,Plants,Missing,(Ground.T.Num()+Water.T.Num()+Rock.T.Num())/3);
}
int32 CheckChultRiver(AActor* Owner,AChuckCharacter* Chuck)
{
    int32 Failures=0,Floors=0,Sweeps=0;auto* W=Owner->GetWorld();const FVector G=AChultLanding::FloorOrigin();FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultRiver),false,Chuck);
    const float Half=Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius();const auto Capsule=FCollisionShape::MakeCapsule(Radius,Half);
    for(float S=4700;S<=27700;S+=100)
    {
        for(float Side:{-1.f,0.f,1.f})
        {
            FVector P=Surface(S,Side*(Width(S)+600));FHitResult Hit;const float Z=ChultCanyonGroundZ(P);
            const bool Found=W->LineTraceSingleByChannel(Hit,G+P+FVector(0,0,900),G+P-FVector(0,0,500),ECC_Visibility,Q);++Floors;
            if(!Found||FMath::Abs(Hit.ImpactPoint.Z-G.Z-Z)>10 || (Side!=0&&Z<P.Z+55)){++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_RIVER_FLOOR_FAIL s=%.0f side=%.0f expected=%.1f actual=%.1f"),S,Side,Z,Hit.ImpactPoint.Z-G.Z);}
            if(Side!=0)
            {
                FVector End=Surface(S+100,Side*(Width(S+100)+600));P.Z=Z+Half+8;End.Z=ChultCanyonGroundZ(End)+Half+8;
                Failures+=W->SweepSingleByChannel(Hit,G+P,G+End,FQuat::Identity,ECC_Pawn,Capsule,Q);++Sweeps;
            }
        }
    }
    // The broad fall stays clear of its wall and in front of it, all the way down.
    int32 FallProbes=0;
    for(float U:{.05f,.2f,.4f,.65f,.85f})for(float Offset:{-400.f,-200.f,0.f,200.f,400.f})
    {
        const FVector P=G+Fall(U,Offset),Along(.8,.6,0);FHitResult Hit;
        const bool Found=W->LineTraceSingleByChannel(Hit,P-Along*20,P+Along*4000,ECC_Visibility,Q)&&Hit.GetComponent()&&Hit.GetComponent()->ComponentHasTag(TEXT("ChultGorgeWall"));
        const float Behind=FVector::DotProduct(Hit.ImpactPoint-P,Along);
        if(!Found||Behind<5){++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_RIVER_FALL_OCCLUDED u=%.2f offset=%.0f found=%d behind=%.1f"),U,Offset,Found,Behind);}++FallProbes;
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_RIVER_CHECK failures=%d floors=%d bank_sweeps=%d fall_visibility=%d downhill=1"),Failures,Floors,Sweeps,FallProbes);return Failures;
}
void StartChultRiverWalks(AActor* Owner,AChuckCharacter* C)
{
    struct FRun{int32 Trial=0,Failures=0;float Start=-1;FTimerHandle Timer;};auto Run=MakeShared<FRun>();
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,C,Run]()
    {
        if(Run->Trial>=3)return;const float Starts[]={11200,18400,5200};const float S=Starts[Run->Trial];const FVector G=AChultLanding::FloorOrigin();
        auto Bank=[](float Along){return Surface(Along,-Width(Along)-600);};const FVector A=Bank(S),B=Bank(S+500);
        auto* M=C->GetCharacterMovement();const float Now=Owner->GetWorld()->GetTimeSeconds();
        if(Run->Start<0){FVector P=A;P.Z=ChultCanyonGroundZ(P)+34.65f;C->ResetAtLocation(G+P);M->SetMovementMode(MOVE_Walking);Run->Start=Now;}
        const float Time=Now-Run->Start,Distance=FVector::Dist2D(C->GetActorLocation(),G+B);
        if(Distance>40&&Time<12){const FVector D=(G+B-C->GetActorLocation()).GetSafeNormal2D();C->SetTestStickWorld(D);C->AddMovementInput(D,1,true);return;}
        C->SetTestStick(FVector2D::ZeroVector);M->StopMovementImmediately();const bool Pass=Distance<40&&M->IsMovingOnGround();Run->Failures+=!Pass;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_RIVER_WALK trial=%d pass=%d distance=%.1f seconds=%.2f"),Run->Trial,Pass,Distance,Time);
        ++Run->Trial;Run->Start=-1;if(Run->Trial==3){UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_RIVER_TEST failures=%d trials=3"),Run->Failures);FPlatformMisc::RequestExitWithStatus(false,Run->Failures?1:0);}
    },.016f,true);
}

