#include "ChultCliffs.h"
#include "ChultLanding.h"
#include "ChuckCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "HAL/PlatformMisc.h"

namespace
{
    // Partial escarpment behind the old forest. The northeast extension stays open.
    constexpr int32 Columns=150,Rows=12;
    constexpr float First=100,Last=325;
    float Angle(float U) { return FMath::DegreesToRadians(FMath::Lerp(First,Last,U)); }
    FVector Point(float U,float V,bool Back=false)
    {
        const float A=Angle(U);
        const float Height=3000+350*FMath::Sin(A*7+.4f)+220*FMath::Sin(A*19)+140*FMath::Cos(A*37);
        const float Z=-90+V*(Height+90);
        const float Base=10200+650*FMath::Sin(A*7)+250*FMath::Cos(A*17);
        // A continuous undercut rather than climbable steps; relief never reverses it.
        const float Radius=Base-.65f*(Z+90)+15*FMath::Sin(V*PI*6+A*4)+(Back?1900:0);
        return FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,Z);
    }
}

void BuildChultCliffs(AActor* Owner)
{
    TArray<FVector> V,N;TArray<int32> T;TArray<FVector2D> UV;
    TArray<FColor> C;TArray<FProcMeshTangent> Tangents;
    auto Tri=[&](FVector A,FVector B,FVector D)
    {
        const FVector Normal=FVector::CrossProduct(B-A,D-A).GetSafeNormal();
        const FVector Side=FVector::CrossProduct(FVector::UpVector,Normal).GetSafeNormal();
        const int32 Start=V.Num();
        for(FVector P:{A,B,D})
        {
            V.Add(P);N.Add(Normal);UV.Add(FVector2D(FVector::DotProduct(P,Side)/400,P.Z/400));
            C.Add(FColor::White);Tangents.Add(FProcMeshTangent(Side,false));
        }
        T.Append({Start,Start+1,Start+2});
    };
    auto Quad=[&](FVector A,FVector B,FVector D,FVector E) {Tri(A,B,D);Tri(A,D,E);};
    for(int32 I=0;I<Columns;++I)
    {
        const float A=I/static_cast<float>(Columns),B=(I+1)/static_cast<float>(Columns);
        for(int32 J=0;J<Rows;++J)
        {
            const float L=J/static_cast<float>(Rows),H=(J+1)/static_cast<float>(Rows);
            Quad(Point(A,L),Point(A,H),Point(B,H),Point(B,L));
            Quad(Point(A,L,true),Point(B,L,true),Point(B,H,true),Point(A,H,true));
        }
        Quad(Point(A,1),Point(A,1,true),Point(B,1,true),Point(B,1));
        Quad(Point(A,0),Point(B,0),Point(B,0,true),Point(A,0,true));
    }
    for(int32 J=0;J<Rows;++J)
    {
        const float L=J/static_cast<float>(Rows),H=(J+1)/static_cast<float>(Rows);
        Quad(Point(0,L),Point(0,L,true),Point(0,H,true),Point(0,H));
        Quad(Point(1,L),Point(1,H),Point(1,H,true),Point(1,L,true));
    }
    auto* Mesh=NewObject<UProceduralMeshComponent>(Owner,TEXT("ChultBoundaryCliffs"));
    Mesh->SetupAttachment(Owner->GetRootComponent());Mesh->SetWorldLocation(AChultLanding::FloorOrigin());Mesh->RegisterComponent();
    Mesh->bUseComplexAsSimpleCollision=true;
    Mesh->CreateMeshSection(0,V,T,N,UV,C,Tangents,true);
    auto* Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/ChultRuins/M_RuinStone.M_RuinStone"));
    Mesh->SetMaterial(0,Mat);Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CLIFFS triangles=%d missing_material=%d arc=100-325 northeast_open=1"),T.Num()/3,Mat==nullptr);
}

int32 CheckChultCliffs(AActor* Owner,AChuckCharacter* Chuck)
{
    int32 Failures=0,Faces=0,Blocks=0,Open=0;
    const FVector G=AChultLanding::FloorOrigin();UWorld* World=Owner->GetWorld();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultCliffs),false,Chuck);
    auto* Cliff=FindObjectFast<UProceduralMeshComponent>(Owner,TEXT("ChultBoundaryCliffs"));
    if(!Cliff) return 1;
    const float Half=Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const auto Capsule=FCollisionShape::MakeCapsule(Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half);
    for(int32 I=1;I<Columns;++I)
    {
        const float U=(I+.31f)/Columns;
        for(float Z:{20.f,100.f,350.f,700.f,1400.f,2100.f})
        {
            // Independent radial probes of the rendered/colliding face.
            const FVector Out(FMath::Cos(Angle(U)),FMath::Sin(Angle(U)),0);
            FHitResult Hit;const FVector Start=G+Out*7000+FVector(0,0,Z);
            const bool Found=Cliff->LineTraceComponent(Hit,Start,Start+Out*5000,Q);
            // A face beyond the existing climb/mantle acceptance of |normal.z| <= .3.
            const bool Pass=Found && Hit.GetComponent() && Hit.GetComponent()->GetFName()==TEXT("ChultBoundaryCliffs") && Hit.ImpactNormal.Z<-.3f;
            if(!Pass) {++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_CLIFF_FACE_FAIL column=%d z=%.0f normal=%.3f component=%s"),I,Z,Hit.ImpactNormal.Z,*GetNameSafe(Hit.GetComponent()));}
            ++Faces;
        }
        const FVector Out(FMath::Cos(Angle(U)),FMath::Sin(Angle(U)),0);
        FHitResult Hit;FVector Start=G+Point(U,0)-Out*180;Start.Z=G.Z+Half+4;
        const bool Block=World->SweepSingleByChannel(Hit,Start,Start+Out*1000,FQuat::Identity,ECC_Pawn,Capsule,Q);
        Failures+=!Block || !Hit.GetComponent() || Hit.GetComponent()->GetFName()!=TEXT("ChultBoundaryCliffs");++Blocks;
    }
    // Wide onward corridors beyond the final tree and across/along the stream.
    for(const FVector At:{FVector(13000,6500,0),FVector(15000,8000,0),FVector(18000,12000,0),FVector(20500,14300,0)})
    {
        FHitResult Hit;
        const bool Found=World->LineTraceSingleByChannel(Hit,G+At+FVector(0,0,4000),G+At+FVector(0,0,200),ECC_Visibility,Q);
        Failures+=Found && Hit.GetComponent() && Hit.GetComponent()->GetFName()==TEXT("ChultBoundaryCliffs");++Open;
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CLIFF_CHECK failures=%d undercut_probes=%d capsule_blocks=%d onward_corridors=%d"),Failures,Faces,Blocks,Open);
    return Failures;
}

void StartChultCliffReview(AActor* Owner,AChuckCharacter* Chuck,int32 PreviousFailures)
{
    struct FRun {int32 Trial=0,Failures=0,Walls=0,Mantles=0,Jumps=0;float Start=-1,MaxZ=0;FTimerHandle Timer;};
    auto Run=MakeShared<FRun>();Run->Failures=PreviousFailures;
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,Chuck,Run]()
    {
        if(Run->Trial>=3) return;
        const float U=(60.f+Run->Trial*65)/225.f,A=Angle(U),Now=Owner->GetWorld()->GetTimeSeconds();
        const FVector Out(FMath::Cos(A),FMath::Sin(A),0),Base=Point(U,0);
        if(Run->Start<0)
        {
            Chuck->ResetAtLocation(AChultLanding::FloorOrigin()+Base-Out*180+FVector(0,0,90+Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3));
            Chuck->GetCharacterMovement()->StopMovementImmediately();Chuck->SetActorRotation(Out.Rotation());
            if(Run->Trial==2) Chuck->LockStamina(6);
            Run->Start=Now;Run->Jumps=0;Run->MaxZ=0;Run->Walls=Chuck->GetWallRuns();Run->Mantles=Chuck->GetMantles();
        }
        const float Time=Now-Run->Start;
        const FVector Local=Chuck->GetActorLocation()-AChultLanding::FloorOrigin();Run->MaxZ=FMath::Max(Run->MaxZ,static_cast<float>(Local.Z));
        Chuck->SetTestStickWorld(Out);Chuck->AddMovementInput(Out,1,true);
        if(Run->Trial>0 && Time>.8f+Run->Jumps*.85f) {Chuck->JumpPressed();++Run->Jumps;}
        if(Time<4.5f) return;
        // Walking can slide sideways along a fold. Judge containment against
        // the actual face at the final bearing, not the starting fold's radius.
        FHitResult Face;FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultCliffWalk),false,Chuck);
        auto* Cliff=FindObjectFast<UProceduralMeshComponent>(Owner,TEXT("ChultBoundaryCliffs"));
        const FVector Radial=Local.GetSafeNormal2D(),G=AChultLanding::FloorOrigin();
        const bool HitFace=Cliff && Cliff->LineTraceComponent(Face,G+Radial*7000+FVector(0,0,Local.Z),G+Radial*13000+FVector(0,0,Local.Z),Q);
        const float Clearance=HitFace?FVector::Dist2D(Face.ImpactPoint,Chuck->GetActorLocation()):-1;
        const bool Inside=HitFace && Local.Size2D()<FVector::Dist2D(Face.ImpactPoint,G)-Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius()*.5f;
        const bool Pass=Inside && Clearance<100 && Run->MaxZ<250 && Chuck->GetWallRuns()==Run->Walls && Chuck->GetMantles()==Run->Mantles;
        Run->Failures+=!Pass;Chuck->SetTestStick(FVector2D::ZeroVector);
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CLIFF_WALK trial=%d pass=%d jumping=%d locked_stamina=%d max_z=%.1f wall_runs=%d mantles=%d inside=%d face_clearance=%.1f"),Run->Trial,Pass,Run->Trial>0,Run->Trial==2,Run->MaxZ,Chuck->GetWallRuns()-Run->Walls,Chuck->GetMantles()-Run->Mantles,Inside,Clearance);
        ++Run->Trial;Run->Start=-1;
        if(Run->Trial==3)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CLIFF_TEST_COMPLETE failures=%d trials=3"),Run->Failures);
            FPlatformMisc::RequestExitWithStatus(false,Run->Failures?1:0);
        }
    },.016f,true);
}
