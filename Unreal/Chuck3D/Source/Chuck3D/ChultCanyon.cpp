#include "ChultCanyon.h"
#include "ChultWatercourse.h"
#include "ChultRiver.h"
#include "ChultLanding.h"
#include "ChultExpansion.h"
#include "ChultGorge.h"
#include "ChultUpriver.h"
#include "ChultTemple.h"
#include "ChuckCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"

namespace
{
    const FVector Origin(24500,15500,0),Along(.8,.6,0),Across(-.6,.8,0);
    FVector Frame(FVector P) {P-=Origin;return FVector(FVector::DotProduct(P,Along),FVector::DotProduct(P,Across),P.Z);}
    bool OldFloor(FVector P) {return InChultExpansion(P) || (FMath::Abs(P.X)<=13000 && FMath::Abs(P.Y)<=13000);}
    float Ease(float U) {U=FMath::Clamp(U,0.f,1.f);return U*U*(3-2*U);}
    struct FSurface
    {
        TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;TArray<FColor> C;TArray<FProcMeshTangent> Tangent;
        void Tri(FVector A,FVector B,FVector D)
        {
            const FVector Normal=FVector::CrossProduct(B-A,D-A).GetSafeNormal();
            FVector Side=FVector::CrossProduct(FVector::UpVector,Normal).GetSafeNormal();if(Side.IsNearlyZero()) Side=Along;
            // UE renders clockwise fronts; retain the outward shading normal.
            int32 Start=V.Num();for(FVector P:{A,B,D}) {V.Add(P);N.Add(Normal);UV.Add(FVector2D(FVector::DotProduct(P,Side)/180,P.Z/180));C.Add(FColor::White);Tangent.Add(FProcMeshTangent(Side,false));}T.Append({Start,Start+2,Start+1});
        }
        void Quad(FVector A,FVector B,FVector D,FVector E) {Tri(A,B,D);Tri(A,D,E);}
        void Box(FVector P,FVector X,FVector Y,FVector Z)
        {
            FVector A=P-X-Y-Z,B=P+X-Y-Z,D=P+X+Y-Z,E=P-X+Y-Z;
            Quad(A,E,D,B);Quad(A+Z*2,B+Z*2,D+Z*2,E+Z*2);
            Quad(A,B,B+Z*2,A+Z*2);Quad(B,D,D+Z*2,B+Z*2);Quad(D,E,E+Z*2,D+Z*2);Quad(E,A,A+Z*2,E+Z*2);
        }
        void Tube(FVector A,FVector B,float Radius)
        {
            FVector D=(B-A).GetSafeNormal(),X=FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal();if(X.IsNearlyZero())X=Along;
            const FVector Y=FVector::CrossProduct(D,X).GetSafeNormal();
            for(int32 I=0;I<6;++I) {const float U=I*2*PI/6,W=(I+1)*2*PI/6;FVector N0=(X*FMath::Cos(U)+Y*FMath::Sin(U))*Radius,N1=(X*FMath::Cos(W)+Y*FMath::Sin(W))*Radius;Quad(A+N0,A+N1,B+N1,B+N0);}
        }
    };
    UProceduralMeshComponent* Finish(AActor* Owner,const TCHAR* Name,FSurface& S,const TCHAR* Material,bool Solid)
    {
        auto* Mesh=NewObject<UProceduralMeshComponent>(Owner,FName(Name));Mesh->SetupAttachment(Owner->GetRootComponent());Mesh->SetWorldLocation(AChultLanding::FloorOrigin());Mesh->RegisterComponent();
        Mesh->bUseComplexAsSimpleCollision=true;Mesh->CreateMeshSection(0,S.V,S.T,S.N,S.UV,S.C,S.Tangent,Solid);
        Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,Material));Mesh->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);if(Solid)Mesh->SetCollisionProfileName(TEXT("BlockAll"));return Mesh;
    }
}
FVector ChultCanyonFrame(float S,float T,float Z) {return Origin+Along*S+Across*T+FVector(0,0,Z);}
bool InChultCanyon(const FVector& P) {FVector F=Frame(P);return (F.X>=-6500 && F.X<=11500 && FMath::Abs(F.Y)<=4500)||InChultRiverExtension(P);}
float ChultCanyonBaseZ(const FVector& P)
{
    const FVector F=Frame(P);
    const float Fade=Ease(FMath::Max(static_cast<float>(P.X)-22000.f,static_cast<float>(P.Y)-15500.f)/2300.f);
    const float Relief=Ease((FMath::Abs(static_cast<float>(F.Y))-600)/1400)*(55*FMath::Sin(F.X*.0011f)+40*FMath::Sin(F.Y*.0017f));
    return Fade*(350+Relief)+FMath::Max(0.f,static_cast<float>(F.X)-5000.f)*.022f;
}
float ChultCanyonGroundZ(const FVector& P)
{
    if(OldFloor(P))return ChultExpansionGroundZ(P);
    // Upriver (2026-10-15), the ground is level under each giant canopy tree.
    // The temple plaza (2026-10-15) is level.
    return ChultRiverGroundZ(P,ChultWatercourseGroundZ(P,ChultTempleGroundZ(P,ChultUpriverGroundZ(P,ChultCanyonBaseZ(P)))));
}
void BuildChultCanyon(AActor* Owner)
{
    FSurface Ground,Vines;
    FRandomStream R(2026101407);
    // Two independent banks. No cube/backing floor bridges the cleft.
    for(int32 Bank=0;Bank<2;++Bank)
    {
        const float Start=Bank==0?-6500:4500,End=Bank==0?0:11500;
        for(float S=Start;S<End;S+=100)for(float T=-4500;T<4500;T+=100)
        {
            FVector A=ChultCanyonFrame(S,T),B=ChultCanyonFrame(S+100,T),D=ChultCanyonFrame(S+100,T+100),E=ChultCanyonFrame(S,T+100);
            if(OldFloor(A)&&OldFloor(B)&&OldFloor(D)&&OldFloor(E))continue;
            for(FVector* P:{&A,&B,&D,&E})P->Z=ChultCanyonGroundZ(*P);
            Ground.Quad(A,B,D,E);
        }
        // The gorge walls below each bank are ChultGorge.cpp (Claude, 2026-10-15).
    }
    // Ground uses both horizontal coordinates like the existing extension;
    // vertical face mapping would stretch flat soil into stripes.
    for(int32 I=0;I<Ground.V.Num();++I)
    {
        Ground.UV[I]=FVector2D(Ground.V[I].X/260,Ground.V[I].Y/260);
        Ground.Tangent[I]=FProcMeshTangent(FVector(1,0,-Ground.N[I].X/Ground.N[I].Z).GetSafeNormal(),false);
    }
    // The bridge is ChultBridge.cpp (Claude, 2026-10-14). The anchor posts'
    // old rope ends drew 56 numbers here; keep drawing them so the vegetation
    // below lands exactly where it was checked.
    for(int32 I=0;I<56;++I)R.FRand();
    BuildChultBridge(Owner);
    int32 Missing=0;
    // Hanging vegetation follows the cliff faces below the canopy, away from
    // the bridge. It is scenery, without extra registered climbables or grips.
    for(float Edge:{0.f,4500.f})for(int32 I=0;I<30;++I)
    {
        float T=R.FRandRange(-4300,4300);if(FMath::Abs(T)<450)continue;
        // Same draws as before; the strands now follow the new gorge faces.
        const int32 Side=Edge==0?0:1;const float Sg=Side==0?1.f:-1.f,Top=ChultGorgeTop(Side,T);
        float Length=R.FRandRange(400,1900);FVector Previous=ChultCanyonFrame(ChultGorgeWallS(Side,T,Top-10)+Sg*14,T,Top-10);
        for(int32 J=1;J<=20;++J){float U=J/20.f,Z=Top-10-Length*U,Tj=T+FMath::Sin(U*7+I)*18;FVector Next=ChultCanyonFrame(ChultGorgeWallS(Side,Tj,Z)+Sg*14,Tj,Z);Vines.Tube(Previous,Next,5*(1-.7f*U));Previous=Next;}
    }
    Missing+=!Finish(Owner,TEXT("ChultCanyonBanks"),Ground,TEXT("/Game/Art/Cutscenes/FallToChult/M_FallGround.M_FallGround"),true)->GetMaterial(0);
    Missing+=!Finish(Owner,TEXT("ChultCanyonHangingVines"),Vines,TEXT("/Game/Art/Cutscenes/FallToChult/M_ChultHangingMoss.M_ChultHangingMoss"),false)->GetMaterial(0);
    TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;int32 Trees=0,Plants=0,Rocks=0;
    auto Place=[&](FString Name,FVector At,float Scale,bool Solid=false,bool Hidden=false)
    {
        auto*& H=Sets.FindOrAdd(Name);if(!H) {H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,*FString::Printf(TEXT("ChultCanyon_%s"),*Name));const TCHAR* Dir=Name.StartsWith(TEXT("SM_Chult"))?TEXT("/Game/Art/ChultNature"):TEXT("/Game/Art/Cutscenes/FallToChult");H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),Dir,*Name,*Name)));Missing+=!H->GetStaticMesh();H->SetupAttachment(Owner->GetRootComponent());H->SetWorldLocation(AChultLanding::FloorOrigin());H->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);if(Solid)H->SetCollisionProfileName(TEXT("BlockAll"));H->SetHiddenInGame(Hidden);H->SetCastShadow(!Hidden);H->SetCullDistances(Solid?9000:5000,Solid?14000:8500);H->RegisterComponent();}
        At.Z+=ChultCanyonGroundZ(At);H->AddInstance(FTransform(FRotator(0,R.FRandRange(0,360),0),AChultLanding::FloorOrigin()+At,FVector(Scale)),true);
    };
    // Keep a wide center approach/exit, lips and posts clear. Existing ground is untouched.
    for(int32 I=0;I<240;++I)
    {
        const float S=I%2==0?R.FRandRange(-5000,-700):R.FRandRange(5300,11000),T=R.FRandRange(-4300,4300);FVector At=ChultCanyonFrame(S,T,-6);
        if(OldFloor(At)||ChultRiverDistance(At)<1800||ChultWatercourseDistance(At)<850 || FMath::Abs(T)<1100 || ((S>-1800&&S<0 || S>4500&&S<6500)&&FMath::Abs(T)<1700))continue;
        const float Scale=R.FRandRange(1.7f,2.4f);
        // Both halves of a scanned tree use precisely the same transform.
        const int32 Seed=R.GetCurrentSeed();Place(TEXT("SM_ChultIslandTree"),At,Scale);R.Initialize(Seed);Place(TEXT("SM_ChultIslandTrunkCollision"),At,Scale,true,true);++Trees;
    }
    for(int32 I=0;I<4800;++I)
    {
        const float S=I%2==0?R.FRandRange(-5600,-140):R.FRandRange(4680,11200),T=R.FRandRange(-4400,4400);FVector At=ChultCanyonFrame(S,T,-2);
        if(OldFloor(At)||ChultRiverDistance(At)<1450||FMath::Abs(T)<260||ChultWatercourseDistance(At)<380)continue;
        const int32 Type=I%6;Place(Type<4?FString::Printf(TEXT("SM_ChultFern%d"),Type):Type==4?TEXT("SM_FallElephantEar"):TEXT("SM_FallShrub"),At,R.FRandRange(1.4f,2.4f)*(Type<4?1:.6f));++Plants;
        if(I%65==0&&FMath::Abs(T)>650){Place(FString::Printf(TEXT("SM_ChultRock%d"),I%6),At-FVector(0,0,22),R.FRandRange(.5f,1.f),true);++Rocks;}
    }
    // The six lip mist pools read as white spheres from the bridge; the
    // gorge's own mist sea replaced them (Claude, 2026-10-15).
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANYON span_cm=4500 depth_cm=mist trees=%d plants=%d rocks=%d missing_assets=%d deck_collision=0 rope_collision=0 triangles=%d"),Trees,Plants,Rocks,Missing,(Ground.T.Num()+Vines.T.Num())/3);
    BuildChultRiver(Owner);
}
int32 CheckChultCanyon(AActor* Owner,AChuckCharacter* C)
{
    int32 Failures=0,Floors=0,Voids=0,Faces=0;const FVector G=AChultLanding::FloorOrigin();auto* W=Owner->GetWorld();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultCanyon),false,C);const float Half=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=C->GetCapsuleComponent()->GetScaledCapsuleRadius();const auto Capsule=FCollisionShape::MakeCapsule(Radius,Half);
    for(float S:{-2800.f,-2400.f,-1600.f,-800.f,-280.f,-80.f,4580.f,4820.f,5400.f,6500.f,8000.f,10000.f})
    {
        FVector At=ChultCanyonFrame(S,0);const float Z=ChultCanyonGroundZ(At);FHitResult Hit;
        const bool HitFloor=W->LineTraceSingleByChannel(Hit,G+At+FVector(0,0,600),G+At-FVector(0,0,50),ECC_Visibility,Q);
        Failures+=!HitFloor||FMath::Abs(Hit.ImpactPoint.Z-G.Z-Z)>6||Hit.ImpactNormal.Z<.9f;++Floors;
    }
    for(float S=200;S<=4300;S+=250)for(float T=-4000;T<=4000;T+=1000)
    {
        FHitResult Hit;bool Found=W->LineTraceSingleByChannel(Hit,G+ChultCanyonFrame(S,T,650),G+ChultCanyonFrame(S,T,-80),ECC_Visibility,Q);
        if(Found)UE_LOG(LogTemp,Warning,TEXT("CHUCK_CANYON_VOID_FAIL s=%.0f t=%.0f component=%s"),S,T,*GetNameSafe(Hit.GetComponent()));
        Failures+=Found;++Voids;
    }
    // Both gorge faces stand beside the crossing (ChultGorge.cpp walls).
    for(float T:{-3000.f,0.f,3000.f})for(float S:{0.f,4500.f})
    {
        const float Z=-2000;FHitResult Hit;bool Found=W->LineTraceSingleByChannel(Hit,G+ChultCanyonFrame(2250,T,Z),G+ChultCanyonFrame(S==0?-1000:5500,T,Z),ECC_Visibility,Q);
        Failures+=!Found||!Hit.GetComponent()||!Hit.GetComponent()->ComponentHasTag(TEXT("ChultGorgeWall"));++Faces;
    }
    for(float Start:{-2800.f,4800.f})for(int32 I=0;I<60;++I)
    {
        float A=Start+I*30,B=A+30;FVector P=ChultCanyonFrame(A,0),End=ChultCanyonFrame(B,0);P.Z=ChultCanyonGroundZ(P)+Half+5;End.Z=ChultCanyonGroundZ(End)+Half+5;FHitResult Hit;Failures+=W->SweepSingleByChannel(Hit,G+P,G+End,FQuat::Identity,ECC_Pawn,Capsule,Q);
    }
    const int32 Bridge=CheckChultBridge(Owner,C);Failures+=Bridge;
    Failures+=CheckChultRiver(Owner,C);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANYON_CHECK failures=%d bank_floors=%d void_samples=%d cliff_faces=%d approach_sweeps=120 bridge_failures=%d active_crossing=1"),Failures,Floors,Voids,Faces,Bridge);return Failures;
}
void StartChultCanyonReview(AActor* Owner,AChuckCharacter* C)
{
    struct FRun{int32 Trial=0,Failures=0,Deaths=0;float Start=-1;FTimerHandle Timer;};auto Run=MakeShared<FRun>();const FVector G=AChultLanding::FloorOrigin();
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,C,Run,G]()
    {
        if(Run->Trial>=3)return;const float Now=Owner->GetWorld()->GetTimeSeconds();auto* Movement=C->GetCharacterMovement();
        if(Run->Start<0){Run->Start=Now;Run->Deaths=C->GetFallDeaths();FVector At=ChultCanyonFrame(Run->Trial==0?-1600:Run->Trial==1?4820:-150,0);At.Z=ChultCanyonGroundZ(At)+34.65f;C->ResetAtLocation(G+At);Movement->SetMovementMode(MOVE_Walking);}
        const float Time=Now-Run->Start;const FVector P=C->GetActorLocation()-G;
        bool Pass=false,Done=false;
        if(Run->Trial<2){C->SetTestStickWorld(Along);C->AddMovementInput(Along,1,true);Pass=Frame(P).X>(Run->Trial==0?-1250:5170)&&Movement->IsMovingOnGround();Done=Pass||Time>8;}
        else {if(C->GetFallDeaths()==Run->Deaths){C->SetTestStickWorld(Along);C->AddMovementInput(Along,1,true);}Pass=C->GetFallDeaths()>Run->Deaths&&IsChultBridgeRespawn(C->GetActorLocation())&&Movement->IsMovingOnGround();Done=Pass||Time>12;}
        if(!Done)return;Run->Failures+=!Pass;C->SetTestStick(FVector2D::ZeroVector);Movement->StopMovementImmediately();
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANYON_TRIAL trial=%d pass=%d time=%.2f at=%s fall_deaths=%d"),Run->Trial,Pass,Time,*P.ToString(),C->GetFallDeaths()-Run->Deaths);
        ++Run->Trial;Run->Start=-1;if(Run->Trial==3){UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CANYON_TEST_COMPLETE failures=%d trials=3 gorge_return=bridge"),Run->Failures);if(Run->Failures)FPlatformMisc::RequestExitWithStatus(false,1);else StartChultWatercourseWalks(Owner,C);}
    },.016f,true);
}
