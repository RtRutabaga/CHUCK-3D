#include "DockSewer.h"
#include "ChuckCharacter.h"
#include "SewerSlide.h"
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
TArray<FVector> RouteRight;
FVector Shaft(-1580,3900,0);
float Chamber(int32 I)
{
    const float U=FMath::Clamp(1.f-FMath::Abs(I-Route.Num()*.5f)/24.f,0.f,1.f);
    return U*U*(3.f-2.f*U);
}
float Narrow(int32 I) { return 1.f-FMath::SmoothStep(5.f,10.f,FMath::Abs(float(I-101))); }
float Width(int32 I) { return FMath::Lerp((I<6?230.f-12.f*I:158.f+12.f*FMath::Sin(I*.19f))+315.f*Chamber(I),90.f,Narrow(I)); }
float Height(int32 I) { return 320.f+125.f*Chamber(I); }
float WallWidth(int32 I,float Side)
{
    float Limit=Width(I);
    // Offset curves fold into knife edges when the inner wall is wider than
    // the local bend radius. Keep that bank inside its radius of curvature.
    for(int32 J=FMath::Max(2,I-2);J<=FMath::Min(Route.Num()-3,I+2);++J)
    {
        const FVector A=Route[J]-Route[J-2],B=Route[J+2]-Route[J];
        const float Cross=FVector::CrossProduct(A.GetSafeNormal(),B.GetSafeNormal()).Z;
        const float Angle=FMath::Atan2(Cross,FVector::DotProduct(A.GetSafeNormal(),B.GetSafeNormal()));
        if(Angle*Side<-.01f)
        {
            const float Radius=(A.Size()+B.Size())*.5f/FMath::Abs(Angle);
            // Retain a usable inner bank even at the sharpest spline extrema.
            Limit=FMath::Min(Limit,FMath::Max(100.f,Radius*.68f));
        }
    }
    return Limit;
}
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

FVector DockSewerStartLocation() { return Shaft+FVector(0,0,FloorZ+34.65f); }
int32 DockSewerSamples() { return Route.Num(); }
FVector DockSewerPoint(int32 I) { return Route.IsValidIndex(I) ? Route[I] : FVector::ZeroVector; }
FVector DockSewerSide(int32 I) { return RouteRight.IsValidIndex(I) ? RouteRight[I] : FVector::RightVector; }
float DockSewerHalfWidth(int32 I) { return Route.IsValidIndex(I) ? FMath::Min(WallWidth(I,1.f),WallWidth(I,-1.f)) : 0.f; }
bool DockSewerIsGap(int32 I) { return RiftSegment(I); }
bool DockSewerIsChamber(int32 I) { return Route.IsValidIndex(I) && Chamber(I)>.05f; }
bool IsInDockSewerStream(const FVector& P)
{
    if(P.Z<FloorZ-16 || P.Z>FloorZ+12) return false;
    for(int32 I=0;I<Route.Num()-5;++I)
    {
        if(RiftSegment(I)) continue;
        auto Centre=[](int32 J) {
            const FVector Tangent=(Route[FMath::Min(J+1,Route.Num()-1)]-Route[FMath::Max(J-1,0)]).GetSafeNormal();
            return Route[J]+FVector(Tangent.Y,-Tangent.X,0)*RiftOffset(J);
        };
        const FVector A=Centre(I), B=Centre(I+1), D=B-A;
        const FVector Flat(P.X,P.Y,FloorZ);
        const float Along=FVector::DotProduct(Flat-A,D)/FMath::Max(D.SizeSquared(),1.f);
        if(Along>=0 && Along<=1 && FVector::DistSquared2D(Flat,A+D*Along)<=41.5f*41.5f) return true;
    }
    return false;
}

bool IsWithinDockSewer(const FVector& P)
{
    if(IsInDockSewerSlide(P)) return true;   // the slide down from its end
    if(P.Z<-1080 || P.Z>100) return false;
    if(FMath::Abs(P.X-Shaft.X)<118 && FMath::Abs(P.Y-Shaft.Y)<108) return true;
    if(P.Z>FloorZ+590) return false;
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
    // The clamped spline has no direction at its last point, which pinched the
    // final ring to a line; give it the previous one so the end is a full arch
    // (the slide's mouth sits in it).
    if(Count>1 && Right.Last().IsNearlyZero()) Right.Last()=Right[Count-2];
    auto* Stone=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_SewerRock.M_SewerRock"));
    auto* Stream=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_SewerStream.M_SewerStream"));
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
        TArray<FVector> Normals;Normals.Init(FVector::ZeroVector,V.Num());
        for(int32 I=0;I<T.Num();I+=3)
        {
            const int32 A=T[I],B=T[I+1],C=T[I+2];
            FVector Face=FVector::CrossProduct(V[B]-V[A],V[C]-V[A]);
            if(FVector::DotProduct(Face,N[A])<0) Face=-Face;
            Normals[A]+=Face;Normals[B]+=Face;Normals[C]+=Face;
        }
        for(int32 I=0;I<Normals.Num();++I) Normals[I]=Normals[I].IsNearlyZero()?N[I]:Normals[I].GetSafeNormal();
        Mesh->CreateMeshSection_LinearColor(0,V,Faces,Normals,UV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),Solid);
        Mesh->SetMaterial(0,Mat);
    };
    // A continuous eroded rock shell: broad asymmetry and stratified ledges,
    // with collision on the same surface. No attached spherical decorations.
    TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV;
    constexpr int32 Arc=32;
    for(int32 I=0;I<Count;++I) for(int32 J=0;J<=Arc;++J)
    {
        const float A=PI*J/Arc;
        const float Broad=FMath::PerlinNoise2D(FVector2D(I*.12f,J*.18f));
        const float Grain=FMath::PerlinNoise2D(FVector2D(I*.47f+17,J*.53f));
        const float Relief=22*Broad+9*Grain+9*FMath::Sin(A*5+I*.13f);
        const float Lateral=WallWidth(I,FMath::Cos(A)>0?1.f:-1.f);
        const float X=(Lateral+Relief*(Lateral/Width(I)))*FMath::Cos(A)+14*FMath::Sin(A)*FMath::Sin(I*.17f);
        const float Z=(Height(I)+Relief*1.5f+12*FMath::Sin(I*.09f))*FMath::Sin(A);
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
    RouteRight=Right;
    V.Reset();N.Reset();T.Reset();UV.Reset();
    for(int32 I=0;I<Count;++I) for(int32 Column=0;Column<8;++Column)
    {
        const float O=RiftOffset(I);
        const float Offsets[]={-Width(I)-35,-55+O,-45+O,-32+O,32+O,45+O,55+O,Width(I)+35};
        const float Depth=(I<Count-4 && (Column==3 || Column==4))?-8.f:0.f;
        V.Add(Route[I]+Right[I]*Offsets[Column]+FVector(0,0,Depth)); N.Add(FVector::UpVector);
        UV.Add(FVector2D(Offsets[Column]/100,I*.65f));
    }
    for(int32 I=0;I<Count-1;++I) for(int32 C=0;C<7;++C)
    {
        if(C>=1 && C<=5 && RiftSegment(I)) continue;
        const int32 A=I*8+C;T.Append({A,A+1,A+8,A+1,A+9,A+8});
    }
    MakeMesh(V,T,N,UV,Stone,true);
    // Five-centimetre-deep flowing water over a solid, shallow channel bed.
    V.Reset();N.Reset();T.Reset();UV.Reset();
    for(int32 I=0;I<Count-4;++I) for(float Side : {-1.f,1.f})
    {
        V.Add(Route[I]+Right[I]*(Side*41.5f+RiftOffset(I))+FVector(0,0,-2.5f));N.Add(FVector::UpVector);UV.Add(FVector2D((Side+1)*.5f,I*.65f));
    }
    for(int32 I=0;I<V.Num()/2-1;++I){if(RiftSegment(I)) continue;int32 A=I*2;T.Append({A,A+1,A+2,A+1,A+3,A+2});}
    MakeMesh(V,T,N,UV,Stream,false,false);
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Box=[&](FVector P,FVector Size,UMaterialInterface* Mat,bool Solid,FRotator Rot=FRotator::ZeroRotator)
    {
        auto* B=NewObject<UInstancedStaticMeshComponent>(Owner);B->SetupAttachment(Root);B->SetStaticMesh(Cube);B->SetMaterial(0,Mat);
        B->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));B->RegisterComponent();B->AddInstance(FTransform(Rot,P,Size/100));
        B->SetLightingChannels(false,true,false);
    };
    // The continuous route floor supports the landing without covering the water bed.
    Box(Route[0]+FVector(0,0,150),FVector(Width(0)*2+35,40,340),Stone,true,
        (Route[1]-Route[0]).Rotation()+FRotator(0,90,0));
    // Extend the mouth's sides to the underground landing, with no flat blackout.
    for(float X : {-111.f,111.f}) Box(Shaft+FVector(X,0,-400),FVector(8,210,600),Stone,true);
    for(float Y : {-101.f,101.f}) Box(Shaft+FVector(0,Y,-400),FVector(214,8,600),Stone,true);
    auto* Astral=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_AstralDepth.M_AstralDepth"));
    auto* Oil=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_AstralOilMist.M_AstralOilMist"));
    for(int32 StartIndex : Rifts)
    {
        V.Reset();N.Reset();T.Reset();UV.Reset();
        for(int32 I=0;I<=4;++I) for(float Side : {-1.f,1.f})
        {
            const int32 Index=StartIndex+I;
            V.Add(Route[Index]+Right[Index]*(Side*55+RiftOffset(Index))+FVector(0,0,-300));
            N.Add(FVector::UpVector);UV.Add(FVector2D((Side+1)*.5f,I*.25f));
        }
        for(int32 I=0;I<4;++I){int32 A=I*2;T.Append({A,A+1,A+2,A+1,A+3,A+2});}
        // Close the visual chasm, including its ends. A bed restricted to the
        // opening's footprint lets oblique camera rays see the outdoor sky
        // through the space between the floor and bed. All faces stay nonsolid.
        for(int32 I=0;I<10;++I)
        {
            V.Add(V[I]+FVector(0,0,301));N.Add(FVector::UpVector);
            UV.Add(FVector2D((I%2),I/2.f*.25f));
        }
        for(int32 I=0;I<4;++I) for(int32 Side=0;Side<2;++Side)
        {
            const int32 A=I*2+Side,B=A+2;
            T.Append({A,B,A+10,B,B+10,A+10});
        }
        T.Append({0,10,1,1,10,11,8,9,18,9,19,18});
        // A view into space at floor level, with camera-dependent cloud/star
        // layers supplying depth. This surface is visual only, never a bridge.
        for(int32 I=0;I<10;++I)
        {
            V.Add(V[I]+FVector(0,0,296));N.Add(FVector::UpVector);UV.Add(FVector2D(I%2,(I/2)*.25f));
        }
        for(int32 I=0;I<4;++I){const int32 A=20+I*2;T.Append({A,A+1,A+2,A+1,A+3,A+2});}
        MakeMesh(V,T,N,UV,Astral,false,false);
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
        Lamp->SetIntensity(1800);Lamp->SetAttenuationRadius(460);Lamp->SetLightColor(FLinearColor(.48f,.035f,1));
        Lamp->SetLightingChannels(false,true,false);
        Lamp->SetCastShadows(false);Lamp->RegisterComponent();
    }
    // Soft cinematic night fill, without visible lamps or hot spots. Purple
    // remains the only light visibly emanating from an object in the cave.
    int32 FillLights=0;
    for(int32 I=1;I<Count;I+=8)
    {
        auto* Fill=NewObject<UPointLightComponent>(Owner);Fill->SetupAttachment(Root);
        Fill->SetRelativeLocation(Route[I]+FVector(0,0,Height(I)*.45f));
        Fill->SetIntensity(4300);Fill->SetAttenuationRadius(1150+400*Chamber(I));
        Fill->SetLightColor(FLinearColor(.42f,.44f,.47f));Fill->SetSourceRadius(160);
        Fill->SetLightingChannels(false,true,false);Fill->SetCastShadows(false);Fill->RegisterComponent();++FillLights;
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
        for(const auto& Sky : Skies) if(Sky.Key.IsValid()) Sky.Key->SetIntensity(Below?0:Sky.Value*(HasExitedDockSewer()?.65f:1.f));
        for(const auto& Sun : Suns) if(Sun.Key.IsValid()) Sun.Key->SetIntensity(Below?0:Sun.Value*(HasExitedDockSewer()?.28f:1.f));
        if(APawn* Pawn=PC->GetPawn())
        {
            TArray<UPrimitiveComponent*> Parts;Pawn->GetComponents(Parts);
            for(auto* Part : Parts) Part->SetLightingChannels(!Below,Below,false);
        }
        UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_LIGHT_REGION underground_night=%d"),Below);
    },.1f,true);
    // The end: the stream runs into a narrow water slide (Claude, SewerSlide.cpp;
    // user 2026-10-03), replacing the temporary collapsed wall.
    BuildDockSewerSlide(Owner,Route,Right,Width(Count-1)+35,Height(Count-1)+35,RiftOffset(Count-5),Stone,Stream);
    UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_BUILT length_cm=%.0f samples=%d north_first=1 return_south=1"),Length,Count);

    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSmokeTest")))
    {
        int32 Failed=0;
        for(int32 I=2;I<Count-2;++I)
        {
            FHitResult Hit;
            const FVector P=SafePoint(I,Right),Previous=SafePoint(I-1,Right);
            if(!World->LineTraceSingleByChannel(Hit,P+FVector(0,0,80),P-FVector(0,0,80),ECC_Visibility)
                || Hit.ImpactPoint.Z<FloorZ-8.1f || Hit.ImpactPoint.Z>FloorZ+.1f) ++Failed;
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
        int32 CaveFailures=0,WallChecks=0;
        for(int32 I=12;I<Count-12;I+=12) for(float Side : {-1.f,1.f})
        {
            FHitResult Hit;const FVector P=Route[I]+FVector(0,0,75);
            if(!World->LineTraceSingleByChannel(Hit,P,P+Right[I]*(Side*(Width(I)+70)),ECC_Visibility))
            {++CaveFailures;UE_LOG(LogTemp,Display,TEXT("CHUCK_CAVE_WALL_MISS sample=%d side=%.0f"),I,Side);}
            ++WallChecks;
        }
        const int32 Middle=Count/2;
        for(float Offset : {-300.f,300.f})
        {
            FHitResult Hit;const FVector P=Route[Middle]+Right[Middle]*Offset;
            if(!World->LineTraceSingleByChannel(Hit,P+FVector(0,0,80),P-FVector(0,0,80),ECC_Visibility))
            {++CaveFailures;UE_LOG(LogTemp,Display,TEXT("CHUCK_CAVE_CHAMBER_MISS offset=%.0f"),Offset);}
        }
        FHitResult ChannelHit;
        const FVector Channel=Route[40]+Right[40]*RiftOffset(40);
        if(!World->LineTraceSingleByChannel(ChannelHit,Channel+FVector(0,0,50),Channel-FVector(0,0,50),ECC_Visibility)
            || FMath::Abs(ChannelHit.ImpactPoint.Z-(FloorZ-8))>1)
        {++CaveFailures;UE_LOG(LogTemp,Display,TEXT("CHUCK_CAVE_STREAM_MISS z=%.3f"),ChannelHit.ImpactPoint.Z);}
        UE_LOG(LogTemp,Display,TEXT("CHUCK_CAVE_CHECK failures=%d wall_traces=%d chamber_width_cm=%.0f stream_depth_cm=5.5 night_fill_lights=%d"),CaveFailures,WallChecks,Width(Middle)*2,FillLights);
        int32 NarrowFailures=0;
        for(int32 I=96;I<=106;++I) for(float Side : {-1.f,1.f})
        {
            FHitResult Hit;const FVector P=Route[I]+FVector(0,0,50);
            if(!World->LineTraceSingleByChannel(Hit,P,P+Right[I]*Side*140,ECC_Visibility)
                || FVector::Dist2D(P,Hit.ImpactPoint)<55 || FVector::Dist2D(P,Hit.ImpactPoint)>125) ++NarrowFailures;
        }
        UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_NARROW failures=%d samples=11 nominal_width_cm=180"),NarrowFailures);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckStreamTest")))
    {
        struct FRun { float Time=0; int32 Target=4, Phase=0, Wet=0, DrySteps=0, Before=0; FTimerHandle Timer; };
        auto Run=MakeShared<FRun>(); FTimerHandle Start;
        World->GetTimerManager().SetTimer(Start,[World,Run,Right](){
            auto* Chuck=Cast<AChuckCharacter>(World->GetFirstPlayerController()->GetPawn());
            Chuck->SetActorLocation(Route[3]+Right[3]*RiftOffset(3)+FVector(0,0,34.65f),false,nullptr,ETeleportType::TeleportPhysics);
            Chuck->GetCharacterMovement()->SetMovementMode(MOVE_Falling);Chuck->SetRunHeld(false);
            World->GetTimerManager().SetTimer(Run->Timer,[World,Run,Right,Chuck](){
                Run->Time+=.02f;
                if(Run->Time<1) return;
                const FVector Goal=Route[Run->Target]+Right[Run->Target]*(Run->Phase?110.f:RiftOffset(Run->Target));
                Chuck->AddMovementInput((Goal-Chuck->GetActorLocation()).GetSafeNormal2D(),1);
                if(FVector::Dist2D(Goal,Chuck->GetActorLocation())<20) ++Run->Target;
                if(Run->Target>=13 && Run->Phase==0)
                {
                    Run->Wet=Chuck->GetStreamStepCount();Run->Phase=1;Run->Target=4;
                    Chuck->GetCharacterMovement()->StopMovementImmediately();
                    Chuck->SetActorLocation(Route[3]+Right[3]*110+FVector(0,0,34.65f),false,nullptr,ETeleportType::TeleportPhysics);
                    Run->Before=Chuck->GetSfxCount(AChuckCharacter::ESfx::Step);
                }
                if(Run->Target>=13 || Run->Time>40)
                {
                    const bool Passed=Run->Phase==1 && Run->Target>=13 && Run->Wet>3 && Chuck->GetStreamStepCount()==Run->Wet
                        && Chuck->GetSfxCount(AChuckCharacter::ESfx::Step)>Run->Before+3 && Chuck->GetSplashLoaded()==6;
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_STREAM_TEST_COMPLETE failures=%d wet_steps=%d final_wet_steps=%d dry_steps=%d loaded=%d elapsed=%.2f"),!Passed,Run->Wet,Chuck->GetStreamStepCount(),Chuck->GetSfxCount(AChuckCharacter::ESfx::Step)-Run->Before,Chuck->GetSplashLoaded(),Run->Time);
                    World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
                }
            },.02f,true);
        },3.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ChuckSewerTest")))
    {
        struct FRun { int32 Target=5;int32 DeathsBefore=0;float Time=0;float HazardAt=-1;float ResetAt=-1;float ExitAt=-1;bool HazardReset=false;bool SanityReset=false;bool Landed=false;bool Finished=false;bool Completed=false;FTimerHandle Timer; };
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
                if(Run->HazardAt>=0 && Run->ResetAt<0 && FVector::Dist(Chuck->GetActorLocation(),DockSewerStartLocation())<65)
                {
                    Run->HazardReset=true;Run->ResetAt=Run->Time;
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_RESPAWN fall_local=1 location=%s"),*Chuck->GetActorLocation().ToString());
                    // Exercise the actual zero-sanity vanish/summon path too.
                    Run->DeathsBefore=Chuck->GetRespawns();
                    Chuck->SetActorLocation(Route[Route.Num()/2]+Right[Route.Num()/2]*180+FVector(0,0,34.65f),false,nullptr,ETeleportType::TeleportPhysics);
                    Chuck->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
                    Chuck->SetSanity(1);Chuck->TakeBite(Chuck->GetActorLocation()+FVector(100,0,0));
                }
                if(Run->ResetAt>=0 && !Run->SanityReset && Chuck->GetRespawns()>Run->DeathsBefore && !Chuck->IsAstral()
                    && Chuck->GetSanity()==AChuckCharacter::MaxSanity && FVector::Dist(Chuck->GetActorLocation(),DockSewerStartLocation())<65)
                {
                    Run->SanityReset=true;
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_RESPAWN sanity_local=1 location=%s"),*Chuck->GetActorLocation().ToString());
                    Chuck->ResetToDock();Run->ExitAt=Run->Time;
                }
                const bool Restored=Run->ExitAt>=0 && Run->Time>Run->ExitAt+2 && !*Under
                    && FVector::Dist(Chuck->GetAreaStartLocation(),AChuckCharacter::StartLocation())<1;
                if(Restored || Run->Time>240 || (Run->HazardAt>=0 && Run->Time>Run->HazardAt+22) || (Run->Time>4 && !Run->Landed) || (Run->Landed && !Run->Finished && Chuck->GetActorLocation().Z>-100))
                {
                    const int32 Failures=(!Run->Landed)+(!Run->Finished)+(!Run->HazardReset)+(!Run->SanityReset)+(!Restored);
                    UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_TEST_COMPLETE failures=%d fall=%d walked=%d hazard_reset=%d sanity_reset=%d reached=%d elapsed=%.2f surface_restored=%d"),Failures,Run->Landed,Run->Finished,Run->HazardReset,Run->SanityReset,Run->Target,Run->Time,Restored);
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
        for(int32 I=0;I<8;++I)
        {
            const int32 Indices[]={23,173,187,343,112,3,97,Count-5};const int32 Index=Indices[I];
            const FVector P=SafePoint(Index,Right)+FVector(0,0,I==4?150:110);
            const FVector Target=Route[I==5?0:FMath::Min(Index+(I==4?4:10),Count-1)]+FVector(0,0,I==4?-4:(I==5?0:120));
            FTimerHandle View,Shot;
            World->GetTimerManager().SetTimer(View,[World,Camera,P,Target](){Camera->SetActorLocationAndRotation(P,(Target-P).Rotation());World->GetFirstPlayerController()->SetViewTarget(Camera);},4.f+I*4.f,false);
            World->GetTimerManager().SetTimer(Shot,[World,I](){
                for(float Y : {550.f,600.f,650.f})
                {
                    FHitResult Hit;
                    if(World->GetFirstPlayerController()->GetHitResultAtScreenPosition(FVector2D(700,Y),ECC_Visibility,true,Hit))
                    {
                        auto* Component=Cast<UPrimitiveComponent>(Hit.GetComponent());
                        UE_LOG(LogTemp,Display,TEXT("CHUCK_SEWER_CAPTURE_HIT view=%d y=%.0f actor=%s component=%s material=%s point=%s"),I,Y,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Component),*GetNameSafe(Component?Component->GetMaterial(0):nullptr),*Hit.ImpactPoint.ToString());
                    }
                }
                const FString Folder=FPaths::ScreenShotDir()/TEXT("Sewer");IFileManager::Get().MakeDirectory(*Folder,true);FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("View%d.png"),I),false,false);
            },6.f+I*4.f,false);
        }
        FTimerHandle Exit;World->GetTimerManager().SetTimer(Exit,[World](){World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));},38.f,false);
    }
}
