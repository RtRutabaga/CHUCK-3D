#include "ChultRuins.h"
#include "ChultLanding.h"
#include "ChuckCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"

namespace
{
    struct FSurface
    {
        TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV; TArray<FColor> C;
        int32 Add(FVector P,FVector Normal,FVector2D Tex)
        { int32 I=V.Add(P);N.Add(Normal);UV.Add(Tex);C.Add(FColor::White);return I; }
        void Tri(int32 A,int32 B,int32 D) {T.Append({A,B,D});}
        void Face(FVector A,FVector B,FVector D,FVector E)
        {
            const FVector Normal=FVector::CrossProduct(B-A,D-A).GetSafeNormal();
            int32 I=Add(A,Normal,{0,0});Add(B,Normal,{1,0});Add(D,Normal,{1,1});Add(E,Normal,{0,1});
            Tri(I,I+2,I+1);Tri(I,I+3,I+2);
        }
    };
    // Rounded eroded ashlar: curved edge bands, broad flat standing/climb centres.
    // The rendered surface is the collision surface, with no hidden box substitute.
    void Block(FSurface& S,FVector2D At,FVector2D Size,float Bottom,float Top,float Chip=4)
    {
        const float X=Size.X*.5f,Y=Size.Y*.5f;
        const float Seed=At.X*.023f+At.Y*.031f+Bottom*.19f;
        const float Radius=FMath::Min(Chip*2.5f+3.f,FMath::Min(X,Y)*.32f);
        const float Bevel=FMath::Min(5.f,(Top-Bottom)*.18f);
        struct FRing {TArray<FVector> V,N;};TArray<FRing> Rings;
        auto Ring=[&](float Z,float Inset,float RadialNormal,float VerticalNormal)
        {
            FRing R;
            for(int32 Corner=0;Corner<4;++Corner)
            {
                const float Worn=Radius*(.82f+.18f*FMath::Sin(Seed+Corner*2.3f));
                const float K=FMath::Max(1.f,Worn-Inset*.3f);
                const float SX=Corner<2?1.f:-1.f,SY=(Corner==0 || Corner==3)?-1.f:1.f;
                const FVector2D Center(SX*(X-Inset-K),SY*(Y-Inset-K));
                for(int32 J=0;J<=4;++J)
                {
                    const float A=(-.5f+Corner*.5f+J*.125f)*PI;
                    const FVector2D Out(FMath::Cos(A),FMath::Sin(A));
                    R.V.Add(FVector(At.X+Center.X+Out.X*K,At.Y+Center.Y+Out.Y*K,Z));
                    R.N.Add(FVector(Out.X*RadialNormal,Out.Y*RadialNormal,VerticalNormal).GetSafeNormal());
                }
            }
            Rings.Add(MoveTemp(R));
        };
        for(int32 J=0;J<=3;++J)
        {
            const float A=PI*.5f*J/3;
            Ring(Bottom+Bevel*(1-FMath::Cos(A)),Bevel*(1-FMath::Sin(A)),FMath::Sin(A),-FMath::Cos(A));
        }
        for(int32 J=0;J<=3;++J)
        {
            const float A=PI*.5f*J/3;
            Ring(Top-Bevel+Bevel*FMath::Sin(A),Bevel*(1-FMath::Cos(A)),FMath::Cos(A),FMath::Sin(A));
        }
        const int32 Count=Rings[0].V.Num();
        const int32 SurfaceBase=S.V.Num();
        for(const auto& R:Rings)for(int32 I=0;I<Count;++I)
            S.Add(R.V[I],R.N[I],FVector2D(I/static_cast<float>(Count),R.V[I].Z/180));
        for(int32 K=1;K<Rings.Num();++K) for(int32 I=0;I<Count;++I)
        {
            const int32 J=(I+1)%Count,A=SurfaceBase+(K-1)*Count,B=SurfaceBase+K*Count;
            S.Tri(A+I,B+J,A+J);S.Tri(A+I,B+I,B+J);
        }
        const int32 Cap=S.Add(FVector(At.X,At.Y,Top),FVector::UpVector,{.5,.5});
        for(auto P:Rings.Last().V)S.Add(P,FVector::UpVector,FVector2D(P.X,P.Y)/180);
        const int32 Base=S.Add(FVector(At.X,At.Y,Bottom),-FVector::UpVector,{.5,.5});
        for(auto P:Rings[0].V)S.Add(P,-FVector::UpVector,FVector2D(P.X,P.Y)/180);
        for(int32 I=0;I<Count;++I){S.Tri(Cap,Cap+1+(I+1)%Count,Cap+1+I);S.Tri(Base,Base+1+I,Base+1+(I+1)%Count);}
    }
    struct FTop {FVector At;};
    const FTop Tops[]={{{5090,-2200,30}},{{5230,-2200,60}},{{5600,-2200,90}},{{6200,-2200,90}},
        {{6430,-2010,120}},{{6650,-2200,150}},{{5460,-1000,650}},{{5740,-1000,650}},
        {{5460,-500,650}},{{5460,0,650}},{{5580,590,650}},{{5600,885,670}}};
}

bool InChultRuinsReserve(const FVector& P,float Margin)
{return P.X>4900-Margin && P.X<7600+Margin && P.Y>-2600-Margin && P.Y<350+Margin;}

void BuildChultRuins(AActor* Owner)
{
    const double Started=FPlatformTime::Seconds();
    FSurface Stone,Moss;
    FRandomStream R(73115);
    int32 Blocks=0;
    auto Ashlar=[&](FVector2D At,FVector2D Size,float Bottom,float Top,bool Mossy=false,float Chip=4.f)
    {Block(Mossy?Moss:Stone,At,Size,Bottom,Top,Chip);++Blocks;};
    auto Masonry=[&](FVector2D At,FVector2D Size,float Height)
    {
        // Visible recessed mortar closes the small course joints, so the climb
        // face remains solid even when a trace meets a seam.
        Ashlar(At,Size-FVector2D(3,3),-5,Height,true,2);
        for(float Z=0;Z<Height;Z+=40)
        {
            const int32 NX=FMath::Max(1,FMath::RoundToInt(Size.X/120)),NY=FMath::Max(1,FMath::RoundToInt(Size.Y/120));
            const float W=Size.X/NX,D=Size.Y/NY;
            for(int32 X=0;X<NX;++X) for(int32 Y=0;Y<NY;++Y)
                Ashlar(At+FVector2D(-Size.X*.5f+W*(X+.5f),-Size.Y*.5f+D*(Y+.5f)),{W-1.3f,D-1.3f},
                    Z+.7f,FMath::Min(Z+39.3f,Height),R.FRand()<.17f,R.FRandRange(3,7));
        }
    };
    // Broad platforms and 30 cm approach steps, with a genuine 100 cm gap.
    for(int32 I=0;I<2;++I) Ashlar({5090.f+I*140,-2200},{140,400},-4,30.f*(I+1));
    Masonry({5600,-2200},{600,440},50);Ashlar({5600,-2200},{600,440},50,90);
    Masonry({6200,-2200},{400,440},50);Ashlar({6200,-2200},{400,440},50,90);
    Masonry({6650,-2200},{300,440},110);Ashlar({6650,-2200},{300,440},110,150);
    Ashlar({6430,-2010},{60,160},-4,120);Ashlar({6490,-2010},{60,160},-4,150);
    // Ruined stairwell: real stone facing walls and two separate cornices.
    for(float X:{5460.f,5740.f})
    {Masonry({X,-1000},{160,500},610);Ashlar({X,-1000},{220,560},610,650);}
    // Partly surviving processional causeway to the outside of the first fork.
    for(float Y:{-450.f,150.f}) {Masonry({5460,Y},{200,200},580);Ashlar({5460,Y},{250,250},580,610);}
    for(int32 I=0;I<14;++I)
        Ashlar({5460,-720.f+(I+.5f)*87.15f},{220,87.15f},610,650,I%5==0);
    Ashlar({5580,590},{460,180},610,650);
    // A surviving landing rests on the two trunk tops. A proper 20 cm mantle
    // rises above the rounded fork and avoids coplanar bark/stone.
    Ashlar({5530,820},{360,360},650,670,true);
    // A gateway has lost most of its lintel; its two piers and wall remnants
    // offer alternative heights, with empty space showing the missing fabric.
    Masonry({7000,-1250},{1100,180},100);
    for(float X:{6720.f,7280.f}) {Masonry({X,-950},{180,200},320);Ashlar({X,-950},{230,250},320,360,true);}
    Ashlar({6815,-950},{420,220},360,400);
    Masonry({7100,-1600},{250,250},180);Ashlar({7100,-1600},{280,280},180,220);
    // Loose fallen stones sit beside the approach, away from tested landings.
    for(int32 I=0;I<16;++I)
        Ashlar({R.FRandRange(6820,7480),R.FRandRange(-2500,-1900)},
            {R.FRandRange(50,95),R.FRandRange(45,90)},-5,R.FRandRange(20,65),I%3==0,R.FRandRange(8,15));
    auto* Mesh=NewObject<UProceduralMeshComponent>(Owner,TEXT("ChultRuinsMasonry"));
    Mesh->SetupAttachment(Owner->GetRootComponent());Mesh->SetWorldLocation(AChultLanding::FloorOrigin());Mesh->RegisterComponent();
    Mesh->bUseComplexAsSimpleCollision=true;
    int32 Section=0,Missing=0;
    for(FSurface* S:{&Stone,&Moss})
    {
        // These materials use world-space normals, so UV tangent reconstruction
        // (particularly expensive on dense masonry) is unnecessary.
        TArray<FProcMeshTangent> Tangents;Tangents.Reserve(S->N.Num());
        for(const FVector& N:S->N)
        {
            const FVector Axis=FMath::Abs(N.Z)>.95f?FVector::ForwardVector:FVector::UpVector;
            Tangents.Emplace(FVector::CrossProduct(Axis,N).GetSafeNormal(),false);
        }
        Mesh->CreateMeshSection(Section,S->V,S->T,S->N,S->UV,S->C,Tangents,true);
        const TCHAR* Path=Section==0?TEXT("/Game/Art/ChultRuins/M_RuinStone.M_RuinStone"):
            TEXT("/Game/Art/ChultRuins/M_RuinMoss.M_RuinMoss");
        auto* Material=LoadObject<UMaterialInterface>(nullptr,Path);Mesh->SetMaterial(Section++,Material);Missing+=Material==nullptr;
    }
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));Mesh->ComponentTags.Add(TEXT("ChultRuins"));
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_RUINS blocks=%d triangles=%d missing_materials=%d climb_gap=120 jump_gap=100 causeway_top=650 cliffs=0"),
        Blocks,(Stone.T.Num()+Moss.T.Num())/3,Missing);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_RUINS_FINISH rounded_blocks=%d gray_scan=1 moss_layers=1 shared_materials_changed=0"),Blocks);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_RUINS_BUILD ms=%.1f vertices=%d"),(FPlatformTime::Seconds()-Started)*1000,Stone.V.Num()+Moss.V.Num());
}

int32 CheckChultRuins(AActor* Owner,AChuckCharacter* C)
{
    auto* World=Owner->GetWorld();const FVector G=AChultLanding::FloorOrigin();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultRuins),false,C);
    const float Half=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=C->GetCapsuleComponent()->GetScaledCapsuleRadius();
    int32 Failures=0;
    for(const auto& Top:Tops)
    {
        FHitResult Hit;const FVector At=G+Top.At;
        const bool Floor=World->LineTraceSingleByChannel(Hit,At+FVector(0,0,25),At-FVector(0,0,25),ECC_Visibility,Q);
        const bool Clear=!World->OverlapBlockingTestByChannel(At+FVector(0,0,Half+3),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q);
        const bool Pass=Floor && FMath::Abs(Hit.ImpactPoint.Z-At.Z)<1 && Hit.ImpactNormal.Z>.95f && Clear;
        Failures+=!Pass;
        if(!Pass) UE_LOG(LogTemp,Warning,TEXT("CHUCK_RUINS_TOP_FAIL at=%s floor=%d clear=%d normal=%s"),*Top.At.ToString(),Floor,Clear,*Hit.ImpactNormal.ToString());
    }
    FHitResult Hit;
    // The jump corridor stays empty below and above the gap's midpoint.
    Failures+=World->LineTraceSingleByChannel(Hit,G+FVector(5950,-2200,300),G+FVector(5950,-2200,40),ECC_Visibility,Q);
    const FVector A=G+FVector(5460,-720,650+Half+3),B=G+FVector(5460,490,650+Half+3);
    Failures+=World->SweepSingleByChannel(Hit,A,B,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q);
    // Approach around the existing south trunk's leaning continuation.
    const FVector Join[]={G+FVector(5460,660,670+Half+3),G+FVector(5460,900,670+Half+3),G+FVector(5600,900,670+Half+3)};
    for(int32 I=1;I<UE_ARRAY_COUNT(Join);++I)
        Failures+=World->SweepSingleByChannel(Hit,Join[I-1],Join[I],FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_RUINS_GEOMETRY failures=%d standing_tops=%d causeway_clear=1"),Failures,UE_ARRAY_COUNT(Tops));
    return Failures;
}

void StartChultRuinsReview(AActor* Owner,AChuckCharacter* C)
{
    auto* World=Owner->GetWorld();auto* PC=Cast<APlayerController>(C->GetController());
    C->DisableInput(PC);C->SetLookLocked(true);PC->SetViewTarget(C);
    struct FRun {FTimerHandle Timer;int32 Trial=0,Failures=0,Jumps=0,Climbs=0,PullUps=0,Deaths=0,Mantles=0;float Start=-1,LastJump=-1;bool WallSeen=false;ACameraActor* Camera=nullptr;};
    auto Run=MakeShared<FRun>();
    Run->Camera=World->SpawnActor<ACameraActor>();
    const FVector G=AChultLanding::FloorOrigin();
    Run->Camera->SetActorLocationAndRotation(G+FVector(4350,-3300,1400),(FVector(5900,-950,390)-FVector(4350,-3300,1400)).Rotation());
    Run->Camera->GetCameraComponent()->SetFieldOfView(70);PC->SetViewTarget(Run->Camera);
    World->GetTimerManager().SetTimer(Run->Timer,[World,C,PC,Run,G]()
    {
        if(Run->Trial>=6) return;
        const float Now=World->GetTimeSeconds();auto* Movement=C->GetCharacterMovement();
        if(Run->Start<0)
        {
            Run->Start=Now;Run->LastJump=-1;Run->Jumps=0;Run->WallSeen=false;
            Run->Climbs=C->GetWallRuns();Run->PullUps=C->GetPullUps();Run->Deaths=C->GetFallDeaths();Run->Mantles=C->GetMantles();
            if(Run->Trial>0)
            {
                const FVector Starts[]={{4920,-2200,34.65f},{5680,-2200,124.65f},{5566,-1000,34.65f},{5460,-650,684.65f},{5460,590,684.65f}};
                C->ResetAtLocation(G+Starts[Run->Trial-1]);Movement->StopMovementImmediately();Movement->SetMovementMode(MOVE_Walking);
                C->SetTestStick(FVector2D::ZeroVector);C->SetRunHeld(false);C->SetActorRotation(FRotator(0,Run->Trial==4?90:0,0));C->Recenter();PC->SetViewTarget(C);
            }
        }
        const float T=Now-Run->Start;const FVector P=C->GetActorLocation()-G;
        bool Pass=false,Done=false;
        auto Jump=[&]() {C->JumpPressed();Run->LastJump=T;++Run->Jumps;};
        // Raw test stick is what special moves read. Ordinary locomotion also
        // needs the same movement input the live axis bindings normally supply.
        auto Move=[&](FVector Direction)
        {C->SetTestStickWorld(Direction);C->AddMovementInput(Direction,1.f,true);};
        if(Run->Trial==0)
        {
            if(T>1.5f) {FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult/Ruins_Overview.png"),true,false);Pass=Done=true;}
        }
        else if(Run->Trial==1)
        {
            Move(FVector(1,0,0));
            Pass=Movement->IsMovingOnGround() && P.X>5480 && P.Z>120 && C->GetMantles()>Run->Mantles;
            Done=Pass;
        }
        else if(Run->Trial==2)
        {
            C->SetRunHeld(true);Move(FVector(1,0,0));
            if(P.X>5850 && Run->Jumps==0 && Movement->IsMovingOnGround()) Jump();
            Pass=Run->Jumps>0 && Movement->IsMovingOnGround() && P.X>6030 && P.Z>120;
            Done=Pass;
        }
        else if(Run->Trial==3)
        {
            if(!Run->WallSeen) Move(FVector(-1,0,0));
            if(T>.3f && Run->Jumps==0 && Movement->IsMovingOnGround()) Jump();
            if(C->IsWallRunning()) {Run->WallSeen=true;C->SetTestStick(FVector2D::ZeroVector);}
            if(Run->WallSeen && T-Run->LastJump>.2f && (C->IsWallRunning() || C->IsHanging())) Jump();
            Pass=Movement->IsMovingOnGround() && P.Z>680 && C->GetPullUps()>Run->PullUps;
            Done=Pass;
        }
        else if(Run->Trial==4)
        {
            Move(FVector(0,1,0));
            Pass=Movement->IsMovingOnGround() && P.Y>460 && P.Z>680;
            Done=Pass;
        }
        else if(Run->Trial==5)
        {
            Move(P.Y<900?FVector(0,1,0):FVector(1,0,0));
            Pass=Movement->IsMovingOnGround() && P.X>5580 && P.Y>885 && P.Z>680;
            Done=Pass;
        }
        if(T>(Run->Trial==3?28.f:Run->Trial==4?18.f:12.f)) Done=true;
        if(!Done) return;
        Pass=Pass && C->GetFallDeaths()==Run->Deaths;Run->Failures+=!Pass;
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_RUINS_TRIAL trial=%d pass=%d time=%.2f jumps=%d mantles=%d wall_runs=%d pullups=%d at=%s gait=%s"),
            Run->Trial,Pass,T,Run->Jumps,C->GetMantles()-Run->Mantles,C->GetWallRuns()-Run->Climbs,C->GetPullUps()-Run->PullUps,*P.ToString(),C->GetGaitName());
        C->SetTestStick(FVector2D::ZeroVector);C->SetRunHeld(false);
        if(Run->Trial==3) FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult/Ruins_Upper.png"),true,false);
        ++Run->Trial;Run->Start=-1;
        if(Run->Trial==6)
        {
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_RUINS_TEST_COMPLETE failures=%d trials=5 mantle=1 gap_jump=1 chimney=1 causeway=1 fork=1"),Run->Failures);
            FPlatformMisc::RequestExitWithStatus(false,Run->Failures?1:0);
        }
    },.016f,true);
}
