#include "DockSewer.h"
#include "ChuckCharacter.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/SplineComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
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
const int32 Rifts[]={26,54,84,114,146,180,215,249,282,316,346};
bool RiftSegment(int32 I) { for(int32 S : Rifts) if(I>=S && I<S+4) return true;return false; }
float RiftOffset(int32 I) { return 18.f*FMath::Sin(I*.7f); }
FVector SafePoint(int32 I,const TArray<FVector>& Right)
{
    int32 Distance=1000,Nearest=Rifts[0];
    for(int32 S : Rifts)
    {
        const int32 D=I<S?S-I:I>S+4?I-S-4:0;
        if(D<Distance){Distance=D;Nearest=S;}
    }
    const float Blend=FMath::Clamp(1.f-FMath::Max(0,Distance-4)/5.f,0.f,1.f);
    // Use the outside bank at a bend; the inside arch pinches at tight turns.
    const FVector Before=Route[Nearest+2]-Route[Nearest-4];
    const FVector After=Route[Nearest+8]-Route[Nearest+2];
    const float Side=FVector::CrossProduct(Before,After).Z>=0?1.f:-1.f;
    return Route[I]+Right[I]*(100*Blend*Side);
}
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
    // The art Dark surface is metallic and reflects the outdoor capture.
    // Use the existing matte prototype surface for the underground drainage.
    auto* Dark=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Prototype/Materials/M_Dark.M_Dark"));
    auto MakeMesh=[&](const TArray<FVector>& V,const TArray<int32>& T,const TArray<FVector>& N,const TArray<FVector2D>& UV,UMaterialInterface* Mat,bool Solid,bool ReverseFaces=true)
    {
        auto* Mesh=NewObject<UProceduralMeshComponent>(Owner); Mesh->SetupAttachment(Root);
        Mesh->bUseComplexAsSimpleCollision=true; Mesh->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));
        Mesh->SetLightingChannels(false,true,false);
        Mesh->RegisterComponent();
        // Render the enclosing shell from either side without changing the
        // shared stone material used by the surface town. Normals face inward.
        TArray<int32> Faces=T;
        if(ReverseFaces) for(int32 I=0;I<T.Num();I+=3) Faces.Append({T[I],T[I+2],T[I+1]});
        Mesh->CreateMeshSection_LinearColor(0,V,Faces,N,UV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),Solid);
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
        const float Z=(320+Bump)*FMath::Sin(A);
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
    for(int32 I=0;I<Count;++I) for(int32 Column=0;Column<4;++Column)
    {
        const float Offsets[]={-Width(I)-18,-55+RiftOffset(I),55+RiftOffset(I),Width(I)+18};
        V.Add(Route[I]+Right[I]*Offsets[Column]); N.Add(FVector::UpVector);
        UV.Add(FVector2D(Offsets[Column]/100,I*.65f));
    }
    for(int32 I=0;I<Count-1;++I) for(int32 C=0;C<3;++C)
    {
        if(C==1 && RiftSegment(I)) continue;
        const int32 A=I*4+C;T.Append({A,A+1,A+4,A+1,A+5,A+4});
    }
    MakeMesh(V,T,N,UV,Stone,true);
    // Recessed-looking shallow drainage, kept away from the landing and terminus.
    V.Reset();N.Reset();T.Reset();UV.Reset();
    for(int32 I=8;I<Count-4;++I) for(float Side : {-1.f,1.f})
    {
        V.Add(Route[I]+Right[I]*(Side*38)+FVector(0,0,.8f));N.Add(FVector::UpVector);UV.Add(FVector2D(Side*.5f,I*.12f));
    }
    for(int32 I=0;I<V.Num()/2-1;++I){if(RiftSegment(I+8)) continue;int32 A=I*2;T.Append({A,A+1,A+2,A+1,A+3,A+2});}
    // Dark effluent ribbon avoids reflecting the outdoor sky below ground.
    MakeMesh(V,T,N,UV,Dark,false);
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Rocks=NewObject<UInstancedStaticMeshComponent>(Owner);Rocks->SetupAttachment(Root);Rocks->SetStaticMesh(Sphere);
    Rocks->SetMaterial(0,Stone);Rocks->SetCollisionProfileName(TEXT("NoCollision"));Rocks->RegisterComponent();
    Rocks->SetLightingChannels(false,true,false);
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
        B->SetLightingChannels(false,true,false);
    };
    Box(Shaft+FVector(0,0,FloorZ-20),FVector(430,370,40),Stone,true);
    Box(Route[0]+FVector(0,0,150),FVector(Width(0)*2+35,40,340),Stone,true,
        (Route[1]-Route[0]).Rotation()+FRotator(0,90,0));
    // Extend the mouth's sides to the underground landing, with no flat blackout.
    for(float X : {-111.f,111.f}) Box(Shaft+FVector(X,0,-400),FVector(8,210,600),Stone,true);
    for(float Y : {-101.f,101.f}) Box(Shaft+FVector(0,Y,-400),FVector(214,8,600),Stone,true);
    auto* Astral=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_AstralRupture.M_AstralRupture"));
    auto* Oil=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_AstralOilMist.M_AstralOilMist"));
    for(int32 StartIndex : Rifts)
    {
        V.Reset();N.Reset();T.Reset();UV.Reset();
        for(int32 I=0;I<=4;++I) for(float Side : {-1.f,1.f})
        {
            const int32 Index=StartIndex+I;
            V.Add(Route[Index]+Right[Index]*(Side*55+RiftOffset(Index))+FVector(0,0,-65));
            N.Add(FVector::UpVector);UV.Add(FVector2D((Side+1)*.5f,I*.25f));
        }
        for(int32 I=0;I<4;++I){int32 A=I*2;T.Append({A,A+1,A+2,A+1,A+3,A+2});}
        MakeMesh(V,T,N,UV,Astral,false);
        V.Reset();N.Reset();T.Reset();UV.Reset();
        constexpr int32 Rows=16,Columns=10;
        for(int32 Layer=0;Layer<2;++Layer)
        {
            const int32 Base=V.Num();
            for(int32 I=0;I<=Rows;++I) for(int32 X=0;X<=Columns;++X)
            {
                const float Along=I*4.f/Rows,U=X/float(Columns);
                const int32 Index=StartIndex+FMath::Min(3,FMath::FloorToInt(Along));
                const float Fraction=Along-(Index-StartIndex);
                const FVector Center=FMath::Lerp(Route[Index],Route[Index+1],Fraction);
                const FVector Side=FMath::Lerp(Right[Index],Right[Index+1],Fraction).GetSafeNormal();
                const float Offset=FMath::Lerp(RiftOffset(Index),RiftOffset(Index+1),Fraction);
                V.Add(Center+Side*((U-.5f)*130+Offset)+FVector(0,0,35+Layer*45+8*FMath::Sin(Along+U*4+Layer)));
                N.Add(FVector::UpVector);UV.Add(FVector2D(U,I/float(Rows)));
            }
            for(int32 I=0;I<Rows;++I) for(int32 X=0;X<Columns;++X){int32 A=Base+I*(Columns+1)+X;T.Append({A,A+1,A+Columns+1,A+1,A+Columns+2,A+Columns+1});}
        }
        if(!FParse::Param(FCommandLine::Get(),TEXT("ChuckSewerNoMist"))) MakeMesh(V,T,N,UV,Oil,false,false);
        const FVector P=Route[StartIndex+2]+FVector(0,0,25);
        auto* Lamp=NewObject<UPointLightComponent>(Owner);Lamp->SetupAttachment(Root);Lamp->SetRelativeLocation(P);
        Lamp->SetIntensity(6500);Lamp->SetAttenuationRadius(1900);Lamp->SetLightColor(FLinearColor(.48f,.025f,1));
        Lamp->SetLightingChannels(false,true,false);
        Lamp->SetCastShadows(false);Lamp->RegisterComponent();
    }
    // Skylight ignores lighting channels. Disable outdoor sun/sky only while
    // the local view is underground, restoring their original surface values.
    TArray<TPair<TWeakObjectPtr<USkyLightComponent>,float>> Skies;
    for(TActorIterator<ASkyLight> It(World);It;++It) Skies.Add({It->GetLightComponent(),It->GetLightComponent()->Intensity});
    TArray<TPair<TWeakObjectPtr<ULightComponent>,float>> Suns;
    for(TActorIterator<ADirectionalLight> It(World);It;++It) Suns.Add({It->GetLightComponent(),It->GetLightComponent()->Intensity});
    auto Under=MakeShared<bool>(false);
    FTimerHandle LightTimer;
    World->GetTimerManager().SetTimer(LightTimer,[World,Under,Skies,Suns](){
        auto* PC=World->GetFirstPlayerController();if(!PC || !PC->PlayerCameraManager) return;
        const FVector P=PC->PlayerCameraManager->GetCameraLocation();
        const bool Below=P.Z<-150 && IsWithinDockSewer(P);
        if(Below==*Under) return;*Under=Below;
        for(const auto& Sky : Skies) if(Sky.Key.IsValid()) Sky.Key->SetIntensity(Below?0:Sky.Value);
        for(const auto& Sun : Suns) if(Sun.Key.IsValid()) Sun.Key->SetIntensity(Below?0:Sun.Value);
        if(APawn* Pawn=PC->GetPawn())
        {
            TArray<UPrimitiveComponent*> Parts;Pawn->GetComponents(Parts);
            for(auto* Part : Parts) Part->SetLightingChannels(!Below,Below,false);
        }
        UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_LIGHT_REGION purple_only=%d"),Below);
    },.1f,true);
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
            const FVector P=SafePoint(I,Right),Previous=SafePoint(I-1,Right);
            if(!World->LineTraceSingleByChannel(Hit,P+FVector(0,0,80),P-FVector(0,0,80),ECC_Visibility)
                || FMath::Abs(Hit.ImpactPoint.Z-FloorZ)>2) ++Failed;
            if(World->SweepSingleByChannel(Hit,Previous+FVector(0,0,36),P+FVector(0,0,36),
                FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f)))
            {
                ++Failed;UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_GEOMETRY_BLOCK sample=%d actor=%s component=%s point=%s"),I,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString());
            }
        }
        UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_GEOMETRY failures=%d samples=%d"),Failed,Count-4);
        int32 HazardFailures=0;
        for(int32 S : Rifts)
        {
            FHitResult Hit;const FVector P=Route[S+2]+Right[S+2]*RiftOffset(S+2);
            if(World->LineTraceSingleByChannel(Hit,P+FVector(0,0,50),P-FVector(0,0,160),ECC_Visibility)) ++HazardFailures;
        }
        UE_LOG(LogTemp,Display,TEXT("CHUCK_ASTRAL_HAZARDS failures=%d holes=11 purple_lights=11 torches=0"),HazardFailures);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSewerTest")))
    {
        struct FRun { int32 Target=5;float Time=0;float HazardAt=-1;float ResetAt=-1;bool HazardReset=false;bool Landed=false;bool Finished=false;bool Completed=false;FTimerHandle Timer; };
        auto Run=MakeShared<FRun>();
        FTimerHandle Start;
        World->GetTimerManager().SetTimer(Start,[World,Run,Right,Under](){
            auto* Chuck=Cast<AChuckCharacter>(World->GetFirstPlayerController()->GetPawn());
            Chuck->ResetToDock();Chuck->SetActorLocation(Shaft+FVector(0,0,70),false,nullptr,ETeleportType::TeleportPhysics);
            Chuck->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
            Chuck->SetRunHeld(true);
            World->GetTimerManager().SetTimer(Run->Timer,[World,Chuck,Run,Right,Under](){
                if(Run->Completed) return;
                Run->Time+=.02f;
                if(!Run->Landed && Chuck->GetCharacterMovement()->IsMovingOnGround() && Chuck->GetActorLocation().Z<-800)
                {Run->Landed=true;UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_FALL landed=1 elapsed=%.2f z=%.2f"),Run->Time,Chuck->GetActorLocation().Z);}
                if(Run->Landed && !Run->Finished)
                {
                    Chuck->SetRunHeld(true);
                    const FVector Goal=SafePoint(Run->Target,Right)+FVector(0,0,34.65f);
                    if(FVector::Dist2D(Chuck->GetActorLocation(),Goal)<18 && Run->Target<Route.Num()-5)
                    {
                        ++Run->Target;
                        if(Run->Target%50==0) UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_PROGRESS reached=%d elapsed=%.2f"),Run->Target,Run->Time);
                    }
                    Chuck->AddMovementInput((Goal-Chuck->GetActorLocation()).GetSafeNormal2D(),1);
                    if(FMath::FloorToInt(Run->Time*50)%500==0)
                    {
                        UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_POSITION target=%d p=%s goal=%s mode=%d"),Run->Target,*Chuck->GetActorLocation().ToString(),*Goal.ToString(),int32(Chuck->GetCharacterMovement()->MovementMode));
                        FHitResult Hit;FCollisionQueryParams Query;Query.AddIgnoredActor(Chuck);
                        if(World->SweepSingleByChannel(Hit,Chuck->GetActorLocation(),Goal,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(15,32.5f),Query))
                            UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_BLOCK actor=%s component=%s point=%s normal=%s"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString(),*Hit.ImpactNormal.ToString());
                    }
                    if(Run->Target>=Route.Num()-5) Run->Finished=true;
                }
                if(Run->Finished && Run->HazardAt<0)
                {
                    Run->HazardAt=Run->Time;Chuck->SetRunHeld(false);Chuck->GetCharacterMovement()->StopMovementImmediately();
                    Chuck->SetActorLocation(Route[Rifts[0]+2]+Right[Rifts[0]+2]*RiftOffset(Rifts[0]+2)+FVector(0,0,60),false,nullptr,ETeleportType::TeleportPhysics);
                    Chuck->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
                }
                if(Run->HazardAt>=0 && Run->ResetAt<0 && FVector::Dist(Chuck->GetActorLocation(),AChuckCharacter::StartLocation())<80)
                {Run->HazardReset=true;Run->ResetAt=Run->Time;}
                const bool Restored=Run->ResetAt>=0 && Run->Time>Run->ResetAt+2 && !*Under;
                if(Restored || Run->Time>210 || (Run->HazardAt>=0 && Run->Time>Run->HazardAt+5) || (Run->Time>4 && !Run->Landed) || (Run->Landed && !Run->Finished && Chuck->GetActorLocation().Z>-100))
                {
                    const int32 Failures=(!Run->Landed)+(!Run->Finished)+(!Run->HazardReset)+(!Restored);
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_TEST_COMPLETE failures=%d fall=%d walked=%d hazard_reset=%d reached=%d elapsed=%.2f surface_restored=%d"),Failures,Run->Landed,Run->Finished,Run->HazardReset,Run->Target,Run->Time,Restored);
                    Run->Completed=true;
                    // Do not destroy this captured delegate before using World.
                    World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
                }
            },.02f,true);
        },3.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSewerCapture")))
    {
        auto* Camera=World->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(78);
        for(int32 I=0;I<4;++I)
        {
            const int32 Index=I==0?23:I==1?111:I==2?211:343;
            const FVector P=Route[Index]+Right[Index]*80+FVector(0,0,100);
            const FVector Target=Route[FMath::Min(Index+8,Count-1)]+FVector(0,0,120);
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,P,Target](){Camera->SetActorLocationAndRotation(P,(Target-P).Rotation());World->GetFirstPlayerController()->SetViewTarget(Camera);},4.f+I*4.f,false);
            World->GetTimerManager().SetTimer(Shot,[I](){const FString Folder=FPaths::ScreenShotDir()/TEXT("Sewer");IFileManager::Get().MakeDirectory(*Folder,true);FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);},6.f+I*4.f,false);
        }
        FTimerHandle Exit;World->GetTimerManager().SetTimer(Exit,[World](){World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));},22.f,false);
    }
}
