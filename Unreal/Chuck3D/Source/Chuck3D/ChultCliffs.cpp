#include "ChultCliffs.h"
#include "ChultLandform.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "ChultLanding.h"
#include "ChultCanyon.h"
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
    constexpr int32 Columns=180,Rows=36;
    constexpr float First=100,Last=325;
    float Angle(float U) { return FMath::DegreesToRadians(FMath::Lerp(First,Last,U)); }
    FVector Point(float U,float V,bool Back=false)
    {
        const float A=Angle(U);
        const float Height=2700+650*FMath::Sin(A*7+.4f)+350*FMath::Sin(A*19)+180*FMath::Cos(A*37);
        const float Z=-90+V*(Height+90);
        const float Base=9600+550*FMath::Sin(A*7)+300*FMath::Cos(A*17);
        // A continuous undercut rather than climbable steps; relief never reverses it.
        const float Radius=Base-.7f*FMath::Min(Z+90,300.f)-.12f*FMath::Max(Z-210,0.f)+FMath::Clamp((Z-300)/350.f,0.f,1.f)*(330*FMath::Sin(A*21+V*14)+190*FMath::Sin(A*43-V*6))+(Back?1900:0);
        return FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,Z);
    }
}

void BuildChultCliffs(AActor* Owner)
{
    FChultLandform Cliff;
    for(int32 I=0;I<Columns;++I)
    {
        const float A=I/static_cast<float>(Columns),B=(I+1)/static_cast<float>(Columns);
        for(int32 J=0;J<Rows;++J)
        {
            const float L=J/static_cast<float>(Rows),H=(J+1)/static_cast<float>(Rows);
            Cliff.Quad(Point(A,L),Point(A,H),Point(B,H),Point(B,L));
            Cliff.Quad(Point(A,L,true),Point(B,L,true),Point(B,H,true),Point(A,H,true));
        }
        Cliff.Quad(Point(A,1),Point(A,1,true),Point(B,1,true),Point(B,1));
        Cliff.Quad(Point(A,0),Point(B,0),Point(B,0,true),Point(A,0,true));
    }
    for(int32 J=0;J<Rows;++J)
    {
        const float L=J/static_cast<float>(Rows),H=(J+1)/static_cast<float>(Rows);
        Cliff.Quad(Point(0,L),Point(0,L,true),Point(0,H,true),Point(0,H));
        Cliff.Quad(Point(1,L),Point(1,H),Point(1,H,true),Point(1,L,true));
    }
    FRandomStream R(2026101409);TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;
    int32 Missing=0,Plants=0,Trees=0,Rocks=0;
    auto Place=[&](FString Name,FVector At,FVector Scale,float Yaw,bool Solid=false,bool Hidden=false)
    {
        const FString Key=Name+(Hidden?TEXT("_collision"):TEXT(""));auto*& H=Sets.FindOrAdd(Key);
        if(!H)
        {
            H=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,*FString::Printf(TEXT("CliffJungle_%s"),*Key));
            const TCHAR* Dir=Name.StartsWith(TEXT("SM_Chult"))?TEXT("/Game/Art/ChultNature"):TEXT("/Game/Art/Cutscenes/FallToChult");
            H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),Dir,*Name,*Name)));Missing+=!H->GetStaticMesh();
            H->SetupAttachment(Owner->GetRootComponent());H->SetWorldLocation(AChultLanding::FloorOrigin());
            H->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);if(Solid)H->SetCollisionProfileName(TEXT("BlockAll"));
            H->SetHiddenInGame(Hidden);H->SetCastShadow(!Hidden);H->SetCullDistances(Plants?7000:9500,Plants?11000:14500);H->RegisterComponent();
        }
        H->AddInstance(FTransform(FRotator(0,Yaw,0),AChultLanding::FloorOrigin()+At,Scale),true);
    };
    // Rock shoulders and shelves vary independently of the tall background core.
    // Vegetation is anchored to their actual summit/face, not a floating green wall.
    for(int32 I=0;I<72;++I)
    {
        const float U=(I+.4f)/72,A=Angle(U);const FVector Out(FMath::Cos(A),FMath::Sin(A),0);
        FVector Center=Point(U,R.FRandRange(.2f,.68f))-Out*180;
        const FVector Size(R.FRandRange(320,660),R.FRandRange(280,550),R.FRandRange(380,750));
        Center.Z=FMath::Max(static_cast<float>(Center.Z),static_cast<float>(Size.Z)+260);
        Cliff.Rock(Center,Size,FMath::RadiansToDegrees(A),I*1.7f);++Rocks;
        for(int32 J=0;J<7;++J)
        {
            FVector At=Center+FVector(R.FRandRange(-90,90),R.FRandRange(-90,90),Size.Z-15);
            Place(J%2?TEXT("SM_FallShrub"):FString::Printf(TEXT("SM_ChultFern%d"),J%4),At,FVector(J%2?1.2f:2.2f),R.FRandRange(0,360));++Plants;
        }
        // Hanging broad-leaf creepers follow a shoulder down to the next ledge.
        for(int32 J=0;J<9;++J)
        {
            FVector At=Center-Out*Size.X*.8f+FVector(0,0,Size.Z*.6f-J*65);
            Place(TEXT("SM_FallElephantEar"),At,FVector(.5f),R.FRandRange(0,360));++Plants;
        }
    }
    // Fill the former bare ring with an irregular forest edge all the way to rock.
    for(int32 I=0;I<180;++I)
    {
        const float U=(I+.35f)/180,A=Angle(U);const FVector Out(FMath::Cos(A),FMath::Sin(A),0),Side(-Out.Y,Out.X,0);
        const FVector Foot=Point(U,0)+FVector(0,0,90);
        const float Degrees=FMath::RadiansToDegrees(A);
        const bool Trial=FMath::Abs(Degrees-160)<3 || FMath::Abs(Degrees-225)<3 || FMath::Abs(Degrees-290)<3;
        for(int32 J=0;J<15;++J)
        {
            const float D=R.FRandRange(65,1900);FVector At=Foot-Out*D+Side*R.FRandRange(-95,95);At.Z=-3;
            const bool Fern=J%3==0;Place(Fern?FString::Printf(TEXT("SM_ChultFern%d"),J%4):J%2?TEXT("SM_FallElephantEar"):TEXT("SM_FallShrub"),At,FVector(Fern?R.FRandRange(2,3.3f):R.FRandRange(1.2f,2.1f)),R.FRandRange(0,360));++Plants;
        }
        if(I%3==0 && !Trial)
        {
            FVector At=Foot-Out*R.FRandRange(450,1300);At.Z=-8;const float Scale=R.FRandRange(1.7f,2.3f),Yaw=R.FRandRange(0,360);
            Place(TEXT("SM_ChultIslandTree"),At,FVector(Scale),Yaw);Place(TEXT("SM_ChultIslandTrunkCollision"),At,FVector(Scale),Yaw,true,true);++Trees;
        }
        if(I%6==0)
        {
            FVector At=Point(U,1)+Out*650-FVector(0,0,20);
            Place(TEXT("SM_ChultIslandTree"),At,FVector(R.FRandRange(1.4f,2.f)),R.FRandRange(0,360));++Trees;
        }
        if(I%2==0 && !Trial)
        {
            FVector At=Foot-Out*R.FRandRange(50,320);At.Z=-35;
            Place(FString::Printf(TEXT("SM_ChultRock%d"),I%6),At,FVector(R.FRandRange(1.4f,2.4f)),R.FRandRange(0,360),true);++Rocks;
        }
    }
    auto* Mesh=Cliff.Finish(Owner,TEXT("ChultBoundaryCliffs"),TEXT("/Game/Art/ChultTerrain/M_ChultCliff.M_ChultCliff"));
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_CLIFFS triangles=%d missing_material=%d arc=100-325 northeast_open=1 plants=%d trees=%d outcrops=%d missing_assets=%d visible_collision=1"),Cliff.T.Num()/3,Mesh->GetMaterial(0)==nullptr,Plants,Trees,Rocks,Missing);
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
            FHitResult Hit;const FVector Start=G+Out*6500+FVector(0,0,FMath::Min(Z,static_cast<float>(Point(U,1).Z)*.8f));
            const bool Found=Cliff->LineTraceComponent(Hit,Start,Start+Out*6000,Q);
            // A face beyond the existing climb/mantle acceptance of |normal.z| <= .3.
            const bool Pass=Found && Hit.GetComponent() && Hit.GetComponent()->GetFName()==TEXT("ChultBoundaryCliffs") && (Z>100 || Hit.ImpactNormal.Z<-.3f);
            if(!Pass) {++Failures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_CLIFF_FACE_FAIL column=%d z=%.0f normal=%.3f component=%s"),I,Z,Hit.ImpactNormal.Z,*GetNameSafe(Hit.GetComponent()));}
            ++Faces;
        }
        const FVector Out(FMath::Cos(Angle(U)),FMath::Sin(Angle(U)),0);
        FHitResult Hit;FVector Start=G+Point(U,0)-Out*180;Start.Z=G.Z+Half+4;
        const bool Block=World->SweepSingleByChannel(Hit,Start,Start+Out*1000,FQuat::Identity,ECC_Pawn,Capsule,Q);
        Failures+=!Block;++Blocks;
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
            if(Run->Failures)FPlatformMisc::RequestExitWithStatus(false,1);
            else StartChultCanyonReview(Owner,Chuck);
        }
    },.016f,true);
}
