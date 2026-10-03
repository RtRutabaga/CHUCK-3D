#include "DockSewer.h"
#include "ChuckCharacter.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/SplineComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "TimerManager.h"

namespace {
constexpr float FloorZ=-900.f;
TArray<FVector> Route;
FVector Shaft(-1580,3900,0);
float Width(int32 I) { return I<6?230.f-12.f*I:158.f+12.f*FMath::Sin(I*.19f); }
}

bool IsWithinDockSewer(const FVector& P)
{
    if(P.Z<-1080 || P.Z>100) return false;
    if(FMath::Abs(P.X-Shaft.X)<118 && FMath::Abs(P.Y-Shaft.Y)<108) return true;
    if(P.Z>FloorZ+440) return false;
    for(int32 I=1;I<Route.Num();++I)
    {
        const FVector A(Route[I-1].X,Route[I-1].Y,P.Z),B(Route[I].X,Route[I].Y,P.Z);
        if(FMath::PointDistToSegment(P,A,B)<Width(I)+45) return true;
    }
    return false;
}

void BuildDockSewer(UWorld* World)
{
    auto* Owner=World->SpawnActor<AActor>(); Owner->Tags.Add(TEXT("DockSewer"));
    auto* Root=NewObject<USceneComponent>(Owner); Owner->SetRootComponent(Root); Root->RegisterComponent();
    auto* Spline=NewObject<USplineComponent>(Owner); Spline->SetupAttachment(Root); Spline->RegisterComponent();
    Spline->ClearSplinePoints(false);
    // Positive Y is north: snake north, cross east, then wind back south.
    const FVector Knots[]={FVector(-1580,3730,FloorZ),FVector(-1540,4600,FloorZ),
        FVector(-2040,5650,FloorZ),FVector(-1400,6800,FloorZ),FVector(-2150,8050,FloorZ),
        FVector(-1700,9450,FloorZ),FVector(-2350,10800,FloorZ),FVector(-1750,12000,FloorZ),
        FVector(-450,12300,FloorZ),FVector(800,11600,FloorZ),FVector(1650,12300,FloorZ),
        FVector(2550,11600,FloorZ),FVector(2200,10400,FloorZ),FVector(2850,9000,FloorZ),
        FVector(2150,7700,FloorZ),FVector(2800,6250,FloorZ),FVector(2100,4900,FloorZ),
        FVector(2550,3550,FloorZ),FVector(2100,2200,FloorZ)};
    for(int32 I=0;I<UE_ARRAY_COUNT(Knots);++I)
    {
        Spline->AddSplinePoint(Knots[I],ESplineCoordinateSpace::World,false);
        Spline->SetSplinePointType(I,ESplinePointType::CurveClamped,false);
    }
    Spline->UpdateSpline();
    const float Length=Spline->GetSplineLength();
    const int32 Count=FMath::CeilToInt(Length/65)+1;
    Route.Reset();
    TArray<FVector> Right;
    for(int32 I=0;I<Count;++I)
    {
        const float D=Length*I/(Count-1);
        Route.Add(Spline->GetLocationAtDistanceAlongSpline(D,ESplineCoordinateSpace::World));
        const FVector T=Spline->GetDirectionAtDistanceAlongSpline(D,ESplineCoordinateSpace::World);
        Right.Add(FVector(T.Y,-T.X,0).GetSafeNormal());
    }
    auto* Stone=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_Stone.M_Stone"));
    auto* Dark=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_Dark.M_Dark"));
    auto* Water=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_Water.M_Water"));
    auto MakeMesh=[&](const TArray<FVector>& V,const TArray<int32>& T,const TArray<FVector>& N,const TArray<FVector2D>& UV,UMaterialInterface* Mat,bool Solid)
    {
        auto* Mesh=NewObject<UProceduralMeshComponent>(Owner); Mesh->SetupAttachment(Root);
        Mesh->bUseComplexAsSimpleCollision=true; Mesh->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));
        Mesh->RegisterComponent();
        Mesh->CreateMeshSection_LinearColor(0,V,T,N,UV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),Solid);
        Mesh->SetMaterial(0,Mat);
    };
    // Inward-facing arch, continuous and collision true. Small bulges make
    // the stone contour irregular without obstructing the central walking line.
    TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV;
    constexpr int32 Arc=24;
    for(int32 I=0;I<Count;++I) for(int32 J=0;J<=Arc;++J)
    {
        const float A=PI*J/Arc;
        const float Bump=6*FMath::Sin(I*1.31f+J*2.11f)+4*FMath::Cos(I*.63f-J*1.73f);
        const float X=(Width(I)+Bump)*FMath::Cos(A);
        const float Z=12+(320+Bump)*FMath::Sin(A);
        V.Add(Route[I]+Right[I]*X+FVector(0,0,Z));
        N.Add((-Right[I]*FMath::Cos(A)-FVector::UpVector*FMath::Sin(A)).GetSafeNormal());
        UV.Add(FVector2D(I*.65f,J*.24f));
    }
    for(int32 I=0;I<Count-1;++I) for(int32 J=0;J<Arc;++J)
    {
        const int32 A=I*(Arc+1)+J,B=A+1,C=A+Arc+1,D=C+1;
        // Leave the entry shaft unroofed over its landing chamber.
        const FVector Mid=(V[A]+V[B]+V[C]+V[D])*.25f;
        if(FVector::Dist2D(Mid,Shaft)<150 && Mid.Z>FloorZ+220) continue;
        T.Append({A,B,C,B,D,C});
    }
    MakeMesh(V,T,N,UV,Stone,true);
    V.Reset();N.Reset();T.Reset();UV.Reset();
    for(int32 I=0;I<Count;++I) for(float Side : {-1.f,1.f})
    {
        V.Add(Route[I]+Right[I]*Side*(Width(I)+18)); N.Add(FVector::UpVector);
        UV.Add(FVector2D(Side*2,I*.65f));
    }
    for(int32 I=0;I<Count-1;++I){const int32 A=I*2;T.Append({A,A+1,A+2,A+1,A+3,A+2});}
    MakeMesh(V,T,N,UV,Stone,true);
    // Recessed-looking shallow drainage, kept away from the landing and terminus.
    V.Reset();N.Reset();T.Reset();UV.Reset();
    for(int32 I=8;I<Count-4;++I) for(float Side : {-1.f,1.f})
    {
        V.Add(Route[I]+Right[I]*(Side*38)+FVector(0,0,.8f));N.Add(FVector::UpVector);UV.Add(FVector2D(Side*.5f,I*.12f));
    }
    for(int32 I=0;I<V.Num()/2-1;++I){int32 A=I*2;T.Append({A,A+1,A+2,A+1,A+3,A+2});}
    MakeMesh(V,T,N,UV,Water,false);
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Rocks=NewObject<UInstancedStaticMeshComponent>(Owner);Rocks->SetupAttachment(Root);Rocks->SetStaticMesh(Sphere);
    Rocks->SetMaterial(0,Stone);Rocks->SetCollisionProfileName(TEXT("NoCollision"));Rocks->RegisterComponent();
    FRandomStream Rng(3917);
    for(int32 I=0;I<Count;I+=2) for(int32 J=0;J<11;++J)
    {
        const float A=PI*(J+.5f)/11;
        const FVector P=Route[I]+Right[I]*((Width(I)+11)*FMath::Cos(A))+FVector(0,0,12+329*FMath::Sin(A));
        if(FVector::Dist2D(P,Shaft)<155 && P.Z>FloorZ+210) continue;
        Rocks->AddInstance(FTransform(FRotator(Rng.FRandRange(-20,20),Rng.FRandRange(-180,180),0),P,
            FVector(Rng.FRandRange(.45f,.85f),Rng.FRandRange(.38f,.7f),Rng.FRandRange(.38f,.65f))));
    }
    auto Box=[&](FVector P,FVector Size,UMaterialInterface* Mat,bool Solid,FRotator Rot=FRotator::ZeroRotator)
    {
        auto* B=NewObject<UInstancedStaticMeshComponent>(Owner);B->SetupAttachment(Root);B->SetStaticMesh(Cube);B->SetMaterial(0,Mat);
        B->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));B->RegisterComponent();B->AddInstance(FTransform(Rot,P,Size/100));
    };
    Box(Shaft+FVector(0,0,FloorZ-20),FVector(430,370,40),Stone,true);
    // Extend the mouth's sides to the underground landing, with no flat blackout.
    for(float X : {-111.f,111.f}) Box(Shaft+FVector(X,0,-400),FVector(8,210,600),Stone,true);
    for(float Y : {-101.f,101.f}) Box(Shaft+FVector(0,Y,-400),FVector(214,8,600),Stone,true);
    for(int32 I=5;I<Count;I+=18)
    {
        const FVector P=Route[I]+Right[I]*((I/18)%2?130:-130)+FVector(0,0,175);
        Box(P,FVector(12,12,26),Dark,false);
        auto* Lamp=NewObject<UPointLightComponent>(Owner);Lamp->SetupAttachment(Root);Lamp->SetRelativeLocation(P);
        Lamp->SetIntensity(2100);Lamp->SetAttenuationRadius(1000);Lamp->SetLightColor(FLinearColor(.7f,.48f,.23f));
        Lamp->SetCastShadows(false);Lamp->RegisterComponent();
    }
    // Temporary collapsed end, rather than a door or invented next campaign map.
    const FVector End=Route.Last();
    Box(End+FVector(0,0,150),FVector(Width(Count-1)*2+50,65,340),Stone,true,
        (Route.Last()-Route[Count-2]).Rotation()+FRotator(0,90,0));
    UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_BUILT length_cm=%.0f samples=%d north_first=1 return_south=1"),Length,Count);

    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest")))
    {
        int32 Failed=0;
        for(int32 I=2;I<Count-2;++I)
        {
            FHitResult Hit;
            if(!World->LineTraceSingleByChannel(Hit,Route[I]+FVector(0,0,80),Route[I]-FVector(0,0,80),ECC_Visibility)
                || FMath::Abs(Hit.ImpactPoint.Z-FloorZ)>2) ++Failed;
            if(World->SweepSingleByChannel(Hit,Route[I-1]+FVector(0,0,36),Route[I]+FVector(0,0,36),
                FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f))) ++Failed;
        }
        UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_GEOMETRY failures=%d samples=%d"),Failed,Count-4);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSewerTest")))
    {
        struct FRun { int32 Target=5;float Time=0;bool Landed=false;bool Finished=false;FTimerHandle Timer; };
        auto Run=MakeShared<FRun>();
        FTimerHandle Start;
        World->GetTimerManager().SetTimer(Start,[World,Run](){
            auto* Chuck=Cast<AChuckCharacter>(World->GetFirstPlayerController()->GetPawn());
            Chuck->ResetToDock();Chuck->SetActorLocation(Shaft+FVector(0,0,70),false,nullptr,ETeleportType::TeleportPhysics);
            Chuck->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            Chuck->SetRunHeld(true);
            World->GetTimerManager().SetTimer(Run->Timer,[World,Chuck,Run](){
                Run->Time+=.02f;
                if(!Run->Landed && Chuck->GetCharacterMovement()->IsMovingOnGround() && Chuck->GetActorLocation().Z<-800)
                {Run->Landed=true;UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_FALL landed=1 elapsed=%.2f z=%.2f"),Run->Time,Chuck->GetActorLocation().Z);}
                if(Run->Landed)
                {
                    const FVector Goal=Route[Run->Target]+FVector(0,0,34.65f);
                    if(FVector::Dist2D(Chuck->GetActorLocation(),Goal)<55 && Run->Target<Route.Num()-5) ++Run->Target;
                    Chuck->AddMovementInput((Goal-Chuck->GetActorLocation()).GetSafeNormal2D(),1);
                    if(Run->Target>=Route.Num()-5) Run->Finished=true;
                }
                if(Run->Finished || Run->Time>210 || (Run->Time>4 && !Run->Landed) || (Run->Landed && Chuck->GetActorLocation().Z>-100))
                {
                    const int32 Failures=(!Run->Landed)+(!Run->Finished);
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_TEST_COMPLETE failures=%d fall=%d walked=%d reached=%d elapsed=%.2f"),Failures,Run->Landed,Run->Finished,Run->Target,Run->Time);
                    World->GetTimerManager().ClearTimer(Run->Timer);World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
                }
            },.02f,true);
        },3.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSewerCapture")))
    {
        auto* Camera=World->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(78);
        for(int32 I=0;I<4;++I)
        {
            const int32 Index=I==0?2:I==1?45:I==2?Count/2:Count-30;
            const FVector P=Route[Index]+Right[Index]*80+FVector(0,0,100);
            const FVector Target=Route[FMath::Min(Index+8,Count-1)]+FVector(0,0,120);
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,P,Target](){Camera->SetActorLocationAndRotation(P,(Target-P).Rotation());World->GetFirstPlayerController()->SetViewTarget(Camera);},4.f+I*4.f,false);
            World->GetTimerManager().SetTimer(Shot,[I](){const FString Folder=FPaths::ScreenShotDir()/TEXT("Sewer");IFileManager::Get().MakeDirectory(*Folder,true);FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);},6.f+I*4.f,false);
        }
        FTimerHandle Exit;World->GetTimerManager().SetTimer(Exit,[World](){World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));},22.f,false);
    }
}
