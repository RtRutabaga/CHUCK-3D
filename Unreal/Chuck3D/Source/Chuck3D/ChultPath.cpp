#include "ChultPath.h"
#include "ChultCanyon.h"
#include "ChultExpansion.h"
#include "ChultRiver.h"
#include "ChultLandform.h"
#include "ChuckCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"

namespace
{
    // Two land routes, deliberately disconnected across the active bridge.
    TWeakObjectPtr<AActor> BuiltOwner;
    TArray<TArray<FVector>> Roads;
    int32 BuildFailures=0,Stones=0,Gaps=0;
    FVector Frame(FVector P){P-=ChultCanyonFrame(0,0);return FVector(P.X*.8+P.Y*.6,-P.X*.6+P.Y*.8,P.Z);}
    float RiverWidth(float S){return 460+90*FMath::Sin((S-4500)*.0007f);}
    float RiverCenter(float S){const float U=FMath::Clamp((S-4500)/23500,0.f,1.f);return 2200+1650*FMath::Sin(U*3*PI)*FMath::Sin(U*PI);}
    bool Dry(FVector P,float Margin=380)
    {
        const FVector F=Frame(P);
        if(F.X>0&&F.X<4500)return false;
        if(ChultWatercourseDistance(P)<Margin)return false;
        if(InChultRiverExtension(P)&&ChultRiverDistance(P)<RiverWidth(F.X)+Margin)return false;
        return true;
    }
    float Height(FVector P){return ChultCanyonGroundZ(P);}
    bool Support(AActor* Owner,FVector P,FHitResult& Hit)
    {
        const FVector G=AChultLanding::FloorOrigin();const float Z=Height(P);
        FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultPathSupport),true);
        // Short rays cannot mistake canopy branches for the road's terrain.
        return Owner->GetWorld()->LineTraceSingleByChannel(Hit,G+FVector(P.X,P.Y,Z+12),G+FVector(P.X,P.Y,Z-30),ECC_Visibility,Q)
            && Hit.ImpactNormal.Z>.82 && FMath::Abs(Hit.ImpactPoint.Z-G.Z-Z)<8;
    }
    bool Clear(AActor* Owner,FVector A,FVector B)
    {
        const FVector G=AChultLanding::FloorOrigin();FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultPathClear),false);
        // Ignore moving actors; the historic route is laid around solid setting.
        for(TActorIterator<APawn> It(Owner->GetWorld());It;++It)Q.AddIgnoredActor(*It);
        const auto Capsule=FCollisionShape::MakeCapsule(35,36);
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/65));FVector Previous;
        for(int32 I=0;I<=Steps;++I)
        {
            FVector P=FMath::Lerp(A,B,I/static_cast<float>(Steps));FHitResult Hit;
            if(!Dry(P)||!Support(Owner,P,Hit))return false;
            P.Z=Hit.ImpactPoint.Z-G.Z+38;
            if(I&&Owner->GetWorld()->SweepSingleByChannel(Hit,G+Previous,G+P,FQuat::Identity,ECC_Pawn,Capsule,Q))return false;
            if(Owner->GetWorld()->OverlapBlockingTestByChannel(G+P,FQuat::Identity,ECC_Pawn,Capsule,Q))return false;
            Previous=P;
        }
        return true;
    }
    // Local A* detours around existing roots/trunks without removing vegetation.
    // Only runs if the authored leg is obstructed; all shortcuts recheck water.
    TArray<FVector> Leg(AActor* Owner,FVector A,FVector B)
    {
        if(Clear(Owner,A,B))return {A,B};
        const FVector Min(FMath::Min(A.X,B.X)-1100,FMath::Min(A.Y,B.Y)-1100,0);
        const int32 NX=FMath::CeilToInt((FMath::Abs(B.X-A.X)+2200)/100)+1,NY=FMath::CeilToInt((FMath::Abs(B.Y-A.Y)+2200)/100)+1;
        auto Pos=[&](int32 I){return Min+FVector((I%NX)*100,(I/NX)*100,0);};
        auto Index=[&](FVector P){return FMath::RoundToInt((P.X-Min.X)/100)+NX*FMath::RoundToInt((P.Y-Min.Y)/100);};
        const int32 Start=Index(A),Goal=Index(B),Count=NX*NY;
        TArray<float> Cost;Cost.Init(1.e20f,Count);TArray<int32> Parent;Parent.Init(-1,Count);TArray<int8> State;State.Init(0,Count);
        TArray<int32> Open={Start};Cost[Start]=0;int32 End=-1,Iterations=0;
        while(Open.Num()&&Iterations++<15000)
        {
            int32 Best=0;float Score=1.e30f;
            for(int32 J=0;J<Open.Num();++J){float V=Cost[Open[J]]+FVector::Dist2D(Pos(Open[J]),B);if(V<Score){Score=V;Best=J;}}
            const int32 I=Open[Best];Open.RemoveAtSwap(Best);State[I]=2;
            if(I==Goal&&Clear(Owner,Pos(I),B)){End=I;break;}
            for(int32 DY=-1;DY<=1;++DY)for(int32 DX=-1;DX<=1;++DX)
            {
                if(!DX&&!DY)continue;const int32 X=I%NX+DX,Y=I/NX+DY;if(X<0||X>=NX||Y<0||Y>=NY)continue;
                const int32 J=X+Y*NX;if(State[J]==2)continue;const FVector P=Pos(J),From=I==Start?A:Pos(I);
                const float NewCost=Cost[I]+FVector::Dist2D(From,P);if(NewCost>=Cost[J]||!Clear(Owner,From,P))continue;
                Cost[J]=NewCost;Parent[J]=I;if(State[J]!=1){Open.Add(J);State[J]=1;}
            }
        }
        if(End<0){++BuildFailures;UE_LOG(LogTemp,Warning,TEXT("CHUCK_PATH_ROUTE_FAIL from=%s to=%s"),*A.ToString(),*B.ToString());return {A,B};}
        TArray<FVector> Reverse;Reverse.Add(B);for(int32 I=End;I!=Start&&I>=0;I=Parent[I])Reverse.Add(Pos(I));Reverse.Add(A);
        TArray<FVector> Result;int32 At=Reverse.Num()-1;Result.Add(A);
        while(At>0){int32 Next=At-1;for(int32 J=0;J<At-1;++J)if(Clear(Owner,Reverse[At],Reverse[J])){Next=J;break;}Result.Add(Reverse[Next]);At=Next;}
        return Result;
    }
}

void BuildChultPath(AActor* Owner)
{
    if(BuiltOwner==Owner)return;BuiltOwner=Owner;Roads.Reset();BuildFailures=Stones=Gaps=0;
    const double Started=FPlatformTime::Seconds();
    TArray<FVector> Near={{-6500,-6500,0},{-5800,-5800,0},{-4200,-3600,0},{-1900,-1800,0},{0,0,0},{700,0,0},{1600,500,0},{2450,300,0},
        {3300,-150,0},{4200,500,0},{4900,500,0},{5300,900,0},{6200,900,0},{6500,1600,0},{7400,2200,0},{7800,3200,0},
        {8750,4700,0},{10100,6100,0},{11000,7600,0},{11000,10000,0},{12000,11200,0},{13800,12300,0},{15800,13100,0},
        {17800,14250,0},{19500,15100,0},{21000,15300,0},{22000,14700,0},ChultCanyonFrame(-2300,0),ChultCanyonFrame(-1000,0),ChultCanyonFrame(-380,0)};
    TArray<FVector> Far={ChultCanyonFrame(5000,0),ChultCanyonFrame(6200,0),ChultCanyonFrame(7100,300)};
    for(float S=8000;S<=27600;S+=200)Far.Add(ChultCanyonFrame(S,RiverCenter(S)-RiverWidth(S)-650));
    for(auto Guide:{Near,Far})
    {
        // Guide points describe the old road's direction, not a mandate to pave
        // through a tree that has grown there. Keep bridge-axis anchors exact.
        for(FVector& P:Guide)
        {
            if(Clear(Owner,P,P))continue;const FVector F=Frame(P);
            if(FMath::Abs(F.Y)<.1&&F.X>-2500&&F.X<6500)continue;
            const FVector Original=P;bool Found=false;
            for(float Radius=75;Radius<=600&&!Found;Radius+=75)for(int32 K=0;K<24;++K)
            {
                const float Angle=K*2*PI/24;FVector Candidate=Original+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Radius;
                if(Clear(Owner,Candidate,Candidate)){P=Candidate;Found=true;break;}
            }
        }
        TArray<FVector> Road;Road.Add(Guide[0]);
        for(int32 I=1;I<Guide.Num();++I){auto Part=Leg(Owner,Guide[I-1],Guide[I]);for(int32 J=1;J<Part.Num();++J)Road.Add(Part[J]);}
        // Round authored bends only where the quadratic arc remains supported and clear.
        TArray<FVector> Rounded;Rounded.Add(Road[0]);
        for(int32 I=1;I<Road.Num()-1;++I)
        {
            FVector A=FMath::Lerp(Road[I],Road[I-1],.22f),B=FMath::Lerp(Road[I],Road[I+1],.22f);
            TArray<FVector> Arc;Arc.Add(A);for(int32 J=1;J<=6;++J){float U=J/6.f;Arc.Add(FMath::Lerp(FMath::Lerp(A,Road[I],U),FMath::Lerp(Road[I],B,U),U));}
            bool OK=Clear(Owner,Rounded.Last(),A);for(int32 J=1;J<Arc.Num()&&OK;++J)OK=Clear(Owner,Arc[J-1],Arc[J]);
            if(OK)Rounded.Append(Arc);else Rounded.Add(Road[I]);
        }
        Rounded.Add(Road.Last());Roads.Add(MoveTemp(Rounded));
    }
    FChultLandform Mesh;FRandomStream R(2026101516);const FVector G=AChultLanding::FloorOrigin();
    for(const auto& Road:Roads)
    {
        float Travel=0,Next=0;
        for(int32 I=1;I<Road.Num();++I)
        {
            const FVector A=Road[I-1],B=Road[I],D=(B-A).GetSafeNormal2D(),Side(-D.Y,D.X,0);const float Length=FVector::Dist2D(A,B);
            while(Next<=Travel+Length)
            {
                FVector Center=A+D*(Next-Travel);const float Distance=Next;Next+=88;
                // Long reclaimed sections alternate with broken, coherent road patches.
                const bool Gap=FMath::Sin(Distance*.0061f)+.48f*FMath::Sin(Distance*.0017f+2)<-.28f;
                const FVector F=Frame(Center);
                const bool BridgeApron=FMath::Abs(F.Y)<250&&((F.X>-2400&&F.X<=-380)||(F.X>=5000&&F.X<6500));
                // Surviving patches at the abutments make the intended crossing
                // legible even when the longer jungle stretches are reclaimed.
                if(Gap&&!(BridgeApron&&FMath::Fmod(FMath::Abs(F.X),550.f)<320)){++Gaps;continue;}
                for(int32 Lane=-1;Lane<=1;++Lane)
                {
                    if(R.FRand()<.22f+FMath::Abs(Lane)*.08f)continue;
                    const FVector P=Center+Side*(Lane*73+R.FRandRange(-8,8))+D*R.FRandRange(-12,12);
                    if(!Dry(P,250))continue;FHitResult Hit;if(!Support(Owner,P,Hit)||!Clear(Owner,P,P))continue;
                    const float W=R.FRandRange(28,36),L=R.FRandRange(34,46),Yaw=R.FRandRange(-.18f,.18f);
                    const FVector X=D*FMath::Cos(Yaw)+Side*FMath::Sin(Yaw),Y=FVector::CrossProduct(FVector::UpVector,X);
                    TArray<FVector> Edge;bool Valid=true;
                    for(int32 K=0;K<8;++K)
                    {
                        const float Angle=(K+.5f)*PI/4;const float CX=FMath::Cos(Angle),CY=FMath::Sin(Angle);
                        FVector V=P+X*(FMath::Sign(CX)*FMath::Sqrt(FMath::Abs(CX))*L*R.FRandRange(.86,1.05))+Y*(FMath::Sign(CY)*FMath::Sqrt(FMath::Abs(CY))*W*R.FRandRange(.86,1.05));
                        FHitResult Ground;if(!Dry(V,220)||!Support(Owner,V,Ground)){Valid=false;break;}V.Z=Ground.ImpactPoint.Z-G.Z+.28;Edge.Add(V);
                    }
                    if(!Valid)continue;FVector Top=P;Top.Z=Hit.ImpactPoint.Z-G.Z+R.FRandRange(1.2,2.8);
                    const uint8 Moss=static_cast<uint8>(R.FRandRange(12,115));
                    for(int32 K=0;K<8;++K)
                    {
                        const int32 Start=Mesh.V.Num();Mesh.Tri(Top,Edge[K],Edge[(K+1)%8]);
                        for(int32 J=Start;J<Mesh.V.Num();++J){Mesh.UV[J]=FVector2D(Mesh.V[J].X/200,Mesh.V[J].Y/200);Mesh.C[J]=FColor(J==Start?Moss:FMath::Min(255,Moss+50),255,255);Mesh.Tangents[J]=FProcMeshTangent(FVector(1,0,0),false);}
                    }
                    ++Stones;
                }
            }
            Travel+=Length;
        }
    }
    auto* Paving=Mesh.Finish(Owner,TEXT("ChultAncientRoad"),TEXT("/Game/Art/ChultPath/M_ChultAncientPath.M_ChultAncientPath"),false);
    Paving->ComponentTags.Add(TEXT("ChultAncientPath"));Paving->SetCastShadow(false);BuildFailures+=!Paving->GetMaterial(0);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_PATH stones=%d reclaimed_rows=%d triangles=%d routes=2 missing_material=%d route_failures=%d water_crossings=0 bridge_deck=0 build_ms=%.1f"),Stones,Gaps,Mesh.T.Num()/3,!Paving->GetMaterial(0),BuildFailures,(FPlatformTime::Seconds()-Started)*1000);
}

int32 CheckChultPath(AActor* Owner,AChuckCharacter* Chuck)
{
    int32 Failures=BuildFailures,Probes=0,Vertices=0;Failures+=BuiltOwner!=Owner||Roads.Num()!=2||Stones<400||Gaps<100;
    if(Roads.Num()!=2)return Failures+1;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ChultPathCheck),false,Chuck);
    for(TActorIterator<APawn> It(Owner->GetWorld());It;++It)Q.AddIgnoredActor(*It);
    const FVector G=AChultLanding::FloorOrigin();const float Half=Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius();
    for(const auto& Road:Roads)for(int32 I=1;I<Road.Num();++I)
    {
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Road[I-1],Road[I])/40));
        FVector Previous;
        for(int32 J=0;J<=Steps;++J)
        {
            FVector P=FMath::Lerp(Road[I-1],Road[I],J/static_cast<float>(Steps));FHitResult Hit;const bool Found=Support(Owner,P,Hit);bool Pass=Dry(P)&&Found;
            P.Z=Found?Hit.ImpactPoint.Z-G.Z+Half+5:Height(P)+Half+5;
            if(J&&Owner->GetWorld()->SweepSingleByChannel(Hit,G+Previous,G+P,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q))Pass=false;
            if(!Pass){++Failures;if(Failures<15)UE_LOG(LogTemp,Warning,TEXT("CHUCK_PATH_CHECK_FAIL at=%s hit=%s"),*P.ToString(),*GetNameSafe(Hit.GetComponent()));}Previous=P;++Probes;
        }
    }
    for(auto* Component:Owner->GetComponents())if(Component->ComponentHasTag(TEXT("ChultAncientPath")))
    {
        auto* Mesh=Cast<UProceduralMeshComponent>(Component);Failures+=!Mesh||Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision;
        if(!Mesh)continue;auto* Section=Mesh->GetProcMeshSection(0);if(!Section){++Failures;continue;}
        for(const auto& Vertex:Section->ProcVertexBuffer){FHitResult Hit;const FVector P=Vertex.Position;Failures+=!Dry(P,220)||!Support(Owner,P,Hit)||P.Z-(Hit.ImpactPoint.Z-G.Z)>3.1||P.Z<Hit.ImpactPoint.Z-G.Z;++Vertices;}
    }
    // The road meets both bridge banks on the crossing's axis; no road spans air.
    for(int32 Side=0;Side<2;++Side)
    {
        const auto& Road=Roads[Side];const FVector A=Frame(Side?Road[0]:Road.Last()),B=Frame(Side?Road[1]:Road[Road.Num()-2]);
        Failures+=FMath::Abs(A.Y)>.1||FMath::Abs(B.Y)>.1||FMath::Abs((B-A).GetSafeNormal2D().X)<.999;
    }
    UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_PATH_CHECK failures=%d route_probes=%d paving_vertices=%d dry_routes=2 bridge_alignment=1 paving_collision=0"),Failures,Probes,Vertices);return Failures;
}

namespace
{
    FVector RoadPoint(const TArray<FVector>& Road,float Distance)
    {
        for(int32 I=1;I<Road.Num();++I){const float L=FVector::Dist2D(Road[I-1],Road[I]);if(Distance<=L)return FMath::Lerp(Road[I-1],Road[I],Distance/FMath::Max(1.f,L));Distance-=L;}return Road.Last();
    }
    void WalkChultPath(AActor* Owner,AChuckCharacter* C,int32 PreviousFailures)
    {
        struct FWalk{int32 Trial=0,Failures=0;float Start=-1,Progress=0;FTimerHandle Timer;};auto Run=MakeShared<FWalk>();Run->Failures=PreviousFailures;
        float NearLength=0;for(int32 I=1;I<Roads[0].Num();++I)NearLength+=FVector::Dist2D(Roads[0][I-1],Roads[0][I]);
        const TArray<float> Starts={1000,8200,16500,NearLength-1800,850,13000};
        C->SetActorHiddenInGame(false);if(auto* PC=Cast<APlayerController>(C->GetController()))PC->SetViewTarget(C);
        Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,C,Run,Starts]()
        {
            const auto& Road=Roads[Run->Trial<4?0:1];const float Offset=Starts[Run->Trial];const FVector G=AChultLanding::FloorOrigin();auto* Movement=C->GetCharacterMovement();
            const float Now=Owner->GetWorld()->GetTimeSeconds();
            if(Run->Start<0){FVector P=RoadPoint(Road,Offset);P.Z=Height(P)+C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2;C->ResetAtLocation(G+P);Movement->SetMovementMode(MOVE_Walking);Run->Start=Now;Run->Progress=0;}
            const FVector End=G+RoadPoint(Road,Offset+500);const float Distance=FVector::Dist2D(C->GetActorLocation(),End),Elapsed=Now-Run->Start;
            if(Distance>30&&Elapsed<12)
            {
                FVector Target=G+RoadPoint(Road,Offset+FMath::Min(500.f,Run->Progress+65));
                if(FVector::Dist2D(C->GetActorLocation(),Target)<30)Run->Progress+=40;
                const FVector D=(Target-C->GetActorLocation()).GetSafeNormal2D();C->SetTestStickWorld(D);C->AddMovementInput(D,1,true);return;
            }
            C->SetTestStick(FVector2D::ZeroVector);Movement->StopMovementImmediately();const bool Pass=Distance<=30&&Movement->IsMovingOnGround();Run->Failures+=!Pass;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_PATH_WALK trial=%d pass=%d distance=%.1f seconds=%.2f"),Run->Trial,Pass,Distance,Elapsed);++Run->Trial;Run->Start=-1;
            if(Run->Trial==6)
            {
                const int32 Status=Run->Failures?1:0;FTimerHandle Done=Run->Timer;
                UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_PATH_TEST failures=%d trials=6"),Run->Failures);
                // Clearing the active timer destroys this lambda's captures.
                // Copy everything needed afterwards before removing it.
                Owner->GetWorldTimerManager().ClearTimer(Done);
                FPlatformMisc::RequestExitWithStatus(false,Status);
            }
        },.016f,true);
    }
}

void ReviewChultPath(AActor* Owner,AChuckCharacter* Chuck,int32 Failures)
{
    auto* Camera=Owner->GetWorld()->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(70);
    Chuck->SetActorHiddenInGame(true);if(auto* PC=Cast<APlayerController>(Chuck->GetController()))PC->SetViewTarget(Camera);
    const TArray<FVector> Eyes={{250,-40,180},{7200,2600,260},ChultCanyonFrame(-1800,0,570),ChultCanyonFrame(5750,-350,620),ChultCanyonFrame(17000,RiverCenter(17000)-RiverWidth(17000)-900,1100),{-5800,-5800,450}};
    const TArray<FVector> Aims={{1400,300,0},{8100,3700,0},ChultCanyonFrame(3000,0,350),ChultCanyonFrame(8500,400,350),ChultCanyonFrame(18500,RiverCenter(18500)-RiverWidth(18500)-650,700),{-4600,-4200,0}};
    const TArray<FString> Names={TEXT("PathLanding"),TEXT("PathRaptor"),TEXT("PathBridgeApproach"),TEXT("PathBridgeDeparture"),TEXT("PathRiverBank"),TEXT("PathCliffRemnants")};
    struct FRun{int32 Phase=0;FTimerHandle Timer;};auto Run=MakeShared<FRun>();
    Owner->GetWorldTimerManager().SetTimer(Run->Timer,[Owner,Chuck,Camera,Run,Eyes,Aims,Names,Failures]()
    {
        const int32 I=Run->Phase/2;if(I>=Eyes.Num())
        {
            FTimerHandle Done=Run->Timer;
            UE_LOG(LogTemp,Display,TEXT("CHUCK_CHULT_PATH_REVIEW_COMPLETE failures=%d views=6"),Failures);
            Camera->Destroy();WalkChultPath(Owner,Chuck,Failures);
            // Last access to any capture: removing our active delegate frees it.
            Owner->GetWorldTimerManager().ClearTimer(Done);return;
        }
        if(Run->Phase%2==0)Camera->SetActorLocationAndRotation(AChultLanding::FloorOrigin()+Eyes[I],(Aims[I]-Eyes[I]).Rotation());
        else FScreenshotRequest::RequestScreenshot(FPaths::ScreenShotDir()/TEXT("Chult")/(Names[I]+TEXT(".png")),true,false);
        ++Run->Phase;
    },2.f,true);
}
