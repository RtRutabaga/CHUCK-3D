#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "FallCutscene.h"
#include "FeyRiverScene.h"
#include "FeyMap.h"
#include "ProceduralMeshComponent.h"
#include "Algo/BinarySearch.h"
#include "Async/ParallelFor.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Info.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RHI.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/ConstructorHelpers.h"

// The Feywild's look slice (Claude, 2026-11-18; FeywildSetting.h, docs/regions/FEYWILD.md). The kit is Tools/build_feywild_kit.py
// and the materials Tools/create_feywild_assets.py. Everything here is a first pass to be judged against References/Feywild/.
using namespace Fey;

namespace
{
    float Noise(float X, float Y, float Scale, float Seed = 0) { return FMath::PerlinNoise2D(FVector2D(X / Scale + Seed * 3.71f + .31f, Y / Scale - Seed * 1.93f + .77f)); }
    float Smooth(float A, float B, float X) { const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f); return T * T * (3 - 2 * T); }
    constexpr float Stand = 34.65f;
    const FRotator SunRot(-10.f, 168.f, 0.f);   // the way its light travels: the sun itself is low over the valley, ahead and a little to the left

    // How far out of the pond's edge (1 on the shore), the edge wandering.
    float PondE(float X, float Y)
    {
        const float A = FMath::Atan2((Y - PondY) / PondRY, (X - PondX) / PondRX);
        return FVector2D((X - PondX) / PondRX, (Y - PondY) / PondRY).Size() / (1.f + .09f * FMath::Sin(3.f * A + 1.f) + .05f * FMath::Sin(7.f * A + 2.f));
    }
    // (2026-11-19: on west past the slice's old end, into the riverbank's Blooming Path)
    float PathK(float X, float Y) { return Smooth(210.f, 80.f, FMath::Abs(Y - PathY(X))) * Smooth(-7900.f, -7300.f, X) * Smooth(5480.f, 5200.f, X); }

    // The country beyond the overlook, out to some 6 km: a valley floor 26 m down, then ridge behind ridge, each a step higher,
    // and a flat-topped rise with the great tree on it.
    float FarLand(float X, float Y)
    {
        float H = -2600.f + 340.f * Noise(X, Y, 6400, 30) + 120.f * Noise(X, Y, 1500, 31);
        auto Ridge = [&](float Cx, float W, float Top, float Seed)
        {
            const float C = Cx + W * .8f * Noise(0, Y, W * 3.f, Seed);
            return Smooth(C - W, C + W * .3f, X) * Top * (.74f + .55f * Noise(X, Y, W * 1.3f, Seed + 1) + .22f * Noise(X, Y, W * .35f, Seed + 2));
        };
        H += Ridge(38000.f, 16000.f, 1800.f, 32) + Ridge(95000.f, 30000.f, 5200.f, 35) + Ridge(230000.f, 70000.f, 16000.f, 38) + Ridge(520000.f, 140000.f, 52000.f, 41);
        const float D = FVector2D(X - GreatX, Y - GreatY).Size() + 900.f * Noise(X, Y, 3000, 44);
        return FMath::Lerp(H, 1200.f + 90.f * Noise(X, Y, 1400, 45), Smooth(7000.f, 4300.f, D));
    }
}

// ------------------------------------------------------------------ the river (stage 2)
namespace
{
    // The river's middle: a Catmull-Rom curve through these, the water's surface falling gently to the east. The second
    // point is S = 0; the first only gives the tunnel's dark end its direction.
    const FVector RiverKeys[] = {{-21800, -30000, 330}, {-21000, -30000, 320}, {-19950, -29870, 300}, {-18900, -30060, 275}, {-17650, -30520, 250},
                                 {-16300, -30380, 225}, {-15000, -29760, 200}, {-13600, -29520, 175}, {-12250, -29960, 150}, {-11000, -30520, 125},
                                 {-9750, -30620, 100}, {-8500, -30260, 80}, {-7400, -29700, 60}};
    struct FCourse { TArray<FVector> P; TArray<float> S; };
    const FCourse& Course()
    {
        static FCourse C = []()
        {
            FCourse Out; constexpr int32 N = UE_ARRAY_COUNT(RiverKeys), Sub = 24;
            Out.P.Add(RiverKeys[0]);
            for (int32 I = 0; I + 1 < N - 1; ++I)
            {
                const FVector& P0 = RiverKeys[FMath::Max(I - 1, 0)], &P1 = RiverKeys[I], &P2 = RiverKeys[I + 1], &P3 = RiverKeys[FMath::Min(I + 2, N - 1)];
                for (int32 K = 1; K <= Sub; ++K)
                {
                    const float T = float(K) / Sub, T2 = T * T, T3 = T2 * T;
                    Out.P.Add(I == 0 ? FMath::Lerp(P1, P2, T) : .5f * ((2.f * P1) + (-P0 + P2) * T + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2 + (-P0 + 3.f * P1 - 3.f * P2 + P3) * T3));
                }
            }
            float Run = 0; Out.S.Add(0);
            for (int32 I = 1; I < Out.P.Num(); ++I) { Run += FVector::Dist2D(Out.P[I], Out.P[I - 1]); Out.S.Add(Run); }
            const float Zero = Out.S[Sub];   // (RiverKeys[1])
            for (float& S : Out.S) S -= Zero;
            return Out;
        }();
        return C;
    }
    // The nearest point of the middle to (X, Y): its S and the signed distance across (+ to the left bank).
    void RiverNearest(float X, float Y, float& OutS, float& OutD)
    {
        const FCourse& C = Course(); float Best = TNumericLimits<float>::Max(); OutS = 0; OutD = 0;
        const FVector2D Q(X, Y);
        for (int32 I = 0; I + 1 < C.P.Num(); ++I)
        {
            const FVector2D A(C.P[I]), B(C.P[I + 1]), AB = B - A; const float L2 = AB.SizeSquared();
            const float U = L2 > 0 ? FMath::Clamp(FVector2D::DotProduct(Q - A, AB) / L2, 0.f, 1.f) : 0.f;
            const FVector2D H = A + AB * U; const float D2 = FVector2D::DistSquared(Q, H);
            if (D2 < Best) { Best = D2; OutS = FMath::Lerp(C.S[I], C.S[I + 1], U); const FVector2D T = AB.GetSafeNormal(); OutD = T.X * (Q.Y - H.Y) - T.Y * (Q.X - H.X); }
        }
    }
    // Inside the river's set (for the air's motes and the region's bounds).
    bool InRiverBox(const FVector& L) { return L.X > -26000.f && L.X < -5000.f && L.Y > -34500.f && L.Y < -25500.f && L.Z > -1500.f && L.Z < 6000.f; }
}

FVector Fey::RiverAt(float S)
{
    const FCourse& C = Course();
    if (S <= C.S[0]) return C.P[0] + (C.P[1] - C.P[0]).GetSafeNormal() * (S - C.S[0]);
    const int32 I = FMath::Clamp(Algo::UpperBound(C.S, S) - 1, 0, C.S.Num() - 2);
    return FMath::Lerp(C.P[I], C.P[I + 1], FMath::Clamp((S - C.S[I]) / FMath::Max(C.S[I + 1] - C.S[I], 1.f), 0.f, 1.f));
}
FVector Fey::RiverSide(float S)
{
    const FVector T = (RiverAt(S + 40.f) - RiverAt(S - 40.f)).GetSafeNormal2D();
    return FVector(-T.Y, T.X, 0.f);
}
float Fey::RiverHalf(float S)
{
    return 250.f + 140.f * Smooth(2200.f, 3400.f, S) + 50.f * Smooth(3800.f, 5200.f, S) - 150.f * Smooth(6600.f, 8200.f, S) + 22.f * FMath::Sin(S / 700.f);
}
float Fey::RiverGround(float X, float Y)
{
    float S, D; RiverNearest(X, Y, S, D);
    const float A = FMath::Abs(D), W = RiverHalf(S), Zw = RiverAt(S).Z;
    const float Rise = 340.f * Smooth(10300.f, 11200.f, S);   // (the river goes on under the ground past its last bend)
    if (A < W) return Zw - 30.f - 70.f * (1.f - FMath::Square(A / W)) + Rise;
    const float Hb = FMath::Lerp(190.f, 105.f, Smooth(3000.f, 5200.f, S)) * (1.f + .25f * Noise(X, Y, 600, 60));
    return Rise + Zw - 10.f + Hb * Smooth(W, W + 260.f, A) + (650.f + 450.f * Noise(X, Y, 2400, 61)) * Smooth(W + 700.f, W + 2900.f, A) + 22.f * Noise(X, Y, 180, 62) * Smooth(W, W + 120.f, A);
}
FVector Fey::ShoreSpot()
{
    // From the falls' foot toward the south shore, to the first dry ground past the pond's edge.
    const FVector2D Dir = FVector2D(-.42f, -1.f).GetSafeNormal();
    FVector2D P(4600.f, 1250.f);
    for (int32 I = 0; I < 800 && PondE(P.X, P.Y) < 1.03f; ++I) P += Dir * 5.f;
    P += Dir * 8.f;   // (the grass starts a little further in)
    return FVector(P.X, P.Y, Ground(P.X, P.Y));
}
float Fey::ShoreYaw() { return FMath::RadiansToDegrees(FMath::Atan2(-1.f, -.42f)); }   // (inland, the way the current set him down)

float Fey::Vista(float Y) { return Smooth(500.f, -150.f, Y) * Smooth(-5200.f, -4250.f, Y); }
float Fey::CliffX(float Y) { return 5560.f + 90.f * Noise(0, Y, 700, 13); }

float Fey::PathY(float X) { return 230.f * FMath::Sin(X / 880.f) - 430.f * Smooth(1100.f, 3200.f, X); }

float Fey::Ground(float X, float Y)
{
    float H = 34.f + 26.f * Noise(X, Y, 420, 1) + 9.f * Noise(X, Y, 130, 2);
    const float Off = Smooth(280.f, 1700.f, FMath::Abs(Y - PathY(X)));
    H += Off * (70.f + 120.f * Noise(X, Y, 1400, 3));
    H += Smooth(2700.f, 4300.f, Y + 200.f * Noise(X, 0, 900, 4)) * (560.f + 280.f * Noise(X, Y, 1800, 5));      // the hill on the right
    H += Smooth(1500.f, 3800.f, -Y + 220.f * Noise(X, 0, 800, 6)) * (640.f + 300.f * Noise(X, Y, 1600, 7));     // and the higher one on the left
    H += Smooth(-700.f, -2300.f, X) * 620.f;                                                                    // behind the arrival
    // The pond and its shore.
    const float E = PondE(X, Y);
    H = FMath::Lerp(H, WaterZ + 9.f + 7.f * Noise(X, Y, 160, 8), Smooth(1.42f, 1.03f, E));
    H = FMath::Lerp(H, WaterZ - 14.f, Smooth(1.02f, .84f, E));   // (wading depth: he stays above it to the knee)
    // The path pressed into the ground.
    const float Pk = PathK(X, Y);
    if (Pk > 0) H = FMath::Lerp(H, 16.f + 12.f * Noise(X, 0, 700, 9) + 3.f * Noise(X, Y, 60, 10), Pk);
    // The two rock steps the falls come over, a channel cut through each.
    const float Edge = 70.f * Noise(0, Y, 500, 11);
    const float Cut = Smooth(FallHalf + 150.f, FallHalf + 20.f, FMath::Abs(Y - FallY));
    const float Step1 = Smooth(LedgeX - 90.f, LedgeX + 70.f, X + Edge) * (LedgeTop + 60.f * (1 - Cut) * Smooth(0.f, 500.f, FMath::Abs(Y - FallY)) - 34.f * Cut);
    const float Step2 = Smooth(Ledge2X - 90.f, Ledge2X + 70.f, X - Edge) * ((Ledge2Top - LedgeTop) + 80.f * (1 - Cut) - 34.f * Cut);
    const float Far = Smooth(7600.f, 9300.f, X) * 700.f + (X > LedgeX ? 40.f * Noise(X, Y, 380, 12) * (1 - Cut) : 0.f);
    const float V = Vista(Y);
    const float Raised = (Step1 + Step2 + Far) * (1.f - V);
    float Out = Raised > 1.f ? FMath::Max(H, Raised) : H;
    // East of the path the land ends at an edge and falls to the valley.
    if (V > 0.f) { const float Cl = CliffX(Y); Out = FMath::Lerp(Out, FarLand(X, Y), V * Smooth(Cl, Cl + 1000.f, X)); }
    return FMath::Lerp(Out, FarLand(X, Y), Smooth(7900.f, 8700.f, X));
}

FVector FeywildOrigin() { return AFallCutscene::LandingSpot() + FVector(0, -3200000, 0); }
bool InFeywild(const FVector& World)
{
    const FVector L = FeyLocal(World);
    return (L.X > MinX - 2000 && L.X < MaxX + 4000 && FMath::Abs(L.Y) < HalfY + 2000 && L.Z > -1500 && L.Z < 9000) || InRiverBox(L) || InFeyMap(World);
}
bool FeyWading(const FVector& Paw)
{
    if (!InFeywild(Paw)) return false;
    const FVector L = FeyLocal(Paw);
    return L.Z < WaterZ + 10.f && PondE(L.X, L.Y) < 1.02f;
}
FVector FeywildSpawn() { const float X = -300.f, Y = PathY(X); return FeyWorld(FVector(X, Y, Ground(X, Y) + Stand + 2.f)); }
float FeywildSpawnYaw() { return 0.f; }
FVector FeywildArrival() { return FeyWorld(ShoreSpot() + FVector(0, 0, Stand + 2.f)); }
float FeywildArrivalYaw() { return ShoreYaw(); }

// ------------------------------------------------------------------ mesh building
struct FFeySurf
{
    TArray<FVector> V, N; TArray<int32> T; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan; TArray<FColor> C;
    int32 Vert(const FVector& P, const FVector& Nn, const FVector2D& Uv, const FColor& Col)
    {
        V.Add(P); N.Add(Nn); UV.Add(Uv); C.Add(Col);
        Tan.Add(FProcMeshTangent(FVector::CrossProduct(FMath::Abs(Nn.Z) > .95f ? FVector::RightVector : FVector::UpVector, Nn).GetSafeNormal(), false));
        return V.Num() - 1;
    }
    void Quad(int32 A, int32 B, int32 Cc, int32 D) { T.Append({A, B, Cc, A, Cc, D}); }
};

AFeywildSetting::AFeywildSetting()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Quad(TEXT("/Engine/BasicShapes/Plane.Plane"));
    auto Air = [&](const TCHAR* Name, int32 Data)
    {
        auto* I = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
        I->SetupAttachment(RootComponent); I->SetStaticMesh(Quad.Object); I->NumCustomDataFloats = Data;
        I->SetCollisionEnabled(ECollisionEnabled::NoCollision); I->SetCastShadow(false); I->SetMobility(EComponentMobility::Movable); I->bNeverDistanceCull = true;
        return I;
    };
    Lights = Air(TEXT("FeyLights"), 2);
    Mist = Air(TEXT("FeyMist"), 2);
    Rings = Air(TEXT("FeyRings"), 2);
}

AFeywildSetting* AFeywildSetting::Get(const UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AFeywildSetting> It(const_cast<UWorld*>(World)); It; ++It) return *It;
    return nullptr;
}

AFeywildSetting* AFeywildSetting::Enter(UWorld* World)
{
    if (!World) return nullptr;
    if (AFeywildSetting* Existing = Get(World)) return Existing;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
    // Whatever stage stands goes dark, still and silent, as APhlegethosSetting::Enter does.
    int32 Hidden = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* A = *It;
        if (A == Chuck || A->GetAttachParentActor() == Chuck || A->IsA<AController>() || A->IsA<APlayerCameraManager>() || A->IsA<AHUD>() || A->IsA<AFeyRiverScene>()) continue;
        if (A->GetClass()->GetName().Contains(TEXT("CameraActor"))) continue;
        TInlineComponentArray<UActorComponent*> Parts(A);
        auto Dark = [](UActorComponent* Part) { if (Part->IsA<ULightComponentBase>() || Part->IsA<UExponentialHeightFogComponent>() || Part->IsA<USkyAtmosphereComponent>() || Part->IsA<UVolumetricCloudComponent>()) Cast<USceneComponent>(Part)->SetVisibility(false); };
        if (A->IsA<AInfo>()) { for (auto* Part : Parts) Dark(Part); continue; }
        if (auto* Volume = Cast<APostProcessVolume>(A)) Volume->bEnabled = false;
        A->SetActorHiddenInGame(true); A->SetActorEnableCollision(false); A->SetActorTickEnabled(false);
        for (auto* Part : Parts)
        {
            if (auto* Audio = Cast<UAudioComponent>(Part)) Audio->Stop();
            else if (auto* Post = Cast<UPostProcessComponent>(Part)) Post->bEnabled = false;
            else Dark(Part);
            if (Part != A->GetRootComponent()) Part->SetComponentTickEnabled(false);
        }
        ++Hidden;
    }
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Stage = World->SpawnActor<AFeywildSetting>(FeywildOrigin(), FRotator::ZeroRotator, P);
    Stage->Build();
    if (FParse::Param(FCommandLine::Get(), TEXT("ChuckFeyRiverSet"))) Stage->BuildRiver();   // (stills of the river's set from a shot list)
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEYWILD_ENTER hidden=%d triangles=%d planted=%d missing=%d"), Hidden, Stage->Triangles, Stage->Planted, Stage->Missing);
    return Stage;
}

UProceduralMeshComponent* AFeywildSetting::Emit(const TCHAR* Name, FFeySurf& S, UMaterialInterface* Material, bool bSolid, bool bShadow)
{
    auto* Mesh = NewObject<UProceduralMeshComponent>(this, FName(Name)); Mesh->SetupAttachment(RootComponent);
    Mesh->SetRelativeLocation(FVector::ZeroVector); Mesh->bUseComplexAsSimpleCollision = true; Mesh->bUseAsyncCooking = false; Mesh->RegisterComponent();
    Mesh->CreateMeshSection(0, S.V, S.T, S.N, S.UV, S.C, S.Tan, bSolid);
    Missing += !Material; Mesh->SetMaterial(0, Material);
    Mesh->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    if (bSolid) Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetCastShadow(bShadow);
    Triangles += S.T.Num() / 3;
    return Mesh;
}

UHierarchicalInstancedStaticMeshComponent* AFeywildSetting::Kit(const TCHAR* Path, float CullStart, float CullEnd, bool bShadow, bool bSolid, bool bInvisible)
{
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
    Missing += !Mesh;
    auto* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this); H->SetupAttachment(RootComponent);
    H->SetMobility(EComponentMobility::Movable); H->SetStaticMesh(Mesh); H->SetCastShadow(bShadow);
    H->InstanceStartCullDistance = int32(CullStart); H->InstanceEndCullDistance = int32(CullEnd);
    H->WorldPositionOffsetDisableDistance = 4500;
    H->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    if (bSolid) H->SetCollisionProfileName(TEXT("BlockAll"));
    H->SetHiddenInGame(bInvisible);
    H->RegisterComponent();
    Kits.Add(H);
    return H;
}

void AFeywildSetting::Build()
{
    const double Started = FPlatformTime::Seconds();
    if (auto* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Feywild/M_FeyMote.M_FeyMote"))) Lights->SetMaterial(0, M); else ++Missing;
    if (auto* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Feywild/M_FeyMist.M_FeyMist"))) Mist->SetMaterial(0, M); else ++Missing;
    if (auto* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Feywild/M_FeyRipple.M_FeyRipple"))) Rings->SetMaterial(0, M); else ++Missing;
    BuildEnvironment();
    LoadSlice();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEYWILD_BUILT seconds=%.2f"), FPlatformTime::Seconds() - Started);
}

// The look slice's own land, water, planting, mirror and falls (2026-11-19: since stage 3 the riverbank's east end, dropped
// with the rest of the riverbank when he is two thresholds away and built again when he comes back).
void AFeywildSetting::LoadSlice()
{
    if (bSlice) return;
    const double Started = FPlatformTime::Seconds();
    TSet<UActorComponent*> Before; for (UActorComponent* C : GetComponents()) Before.Add(C);
    const int32 Tris0 = Triangles, Planted0 = Planted;
    BuildLand();
    BuildFarLand();
    BuildPlanting();
    for (UHierarchicalInstancedStaticMeshComponent* H : Kits) H->BuildTreeIfOutdated(false, true);
    // Low veils of mist on the pond and at the falls' feet.
    Veils.Reset(); RiverVeil0 = -1;
    FRandomStream R(31);
    for (int32 I = 0; I < 34; ++I)
    {
        FVeil W; const float A = R.FRandRange(0.f, 2 * PI), Rr = FMath::Sqrt(R.FRand());
        W.P = I < 22 ? FVector(PondX + PondRX * .95f * Rr * FMath::Cos(A), PondY + PondRY * .95f * Rr * FMath::Sin(A), WaterZ + R.FRandRange(30.f, 110.f))
                     : FVector(LedgeX - R.FRandRange(60.f, 600.f), FallY + R.FRandRange(-650.f, 650.f), WaterZ + R.FRandRange(50.f, 240.f));
        W.Size = R.FRandRange(420.f, 900.f); W.Alpha = R.FRandRange(.035f, .09f); W.Seed = R.FRand(); W.Drift = R.FRandRange(-14.f, 14.f);
        Veils.Add(W);
    }
    // And long banks of it lying in the valley, one behind another.
    for (int32 I = 0; I < 18; ++I)
    {
        FVeil W; const float X = FMath::Lerp(6800.f, 70000.f, FMath::Square(R.FRand()));
        W.P = FVector(X, R.FRandRange(-.9f, .55f) * (2500.f + X * .8f), FMath::Lerp(-1900.f, -300.f, R.FRand()));
        W.Size = 2600.f + X * .22f; W.Alpha = R.FRandRange(.1f, .2f); W.Seed = R.FRand(); W.Drift = R.FRandRange(-20.f, 20.f);
        Veils.Add(W);
    }
    BuildProbe();
    BuildSound();
    SliceParts.Reset();
    for (UActorComponent* C : GetComponents()) if (!Before.Contains(C)) SliceParts.Add(C);
    bSlice = true;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEYWILD_SLICE_LOADED seconds=%.2f parts=%d triangles=%d planted=%d"), FPlatformTime::Seconds() - Started, SliceParts.Num(), Triangles - Tris0, Planted - Planted0);
}

void AFeywildSetting::UnloadSlice()
{
    if (!bSlice) return;
    for (UActorComponent* C : SliceParts) if (C) { if (auto* H = Cast<UHierarchicalInstancedStaticMeshComponent>(C)) Kits.Remove(H); C->DestroyComponent(); }
    SliceParts.Reset();
    WaterMesh = StreamMesh = nullptr; FallsSound = nullptr; Probe = nullptr; ProbeTarget = nullptr; FarMid = nullptr;
    Veils.Reset(); RiverVeil0 = -1; Ripples.Reset(); Drops.Reset(); bWasWading = false;
    bSlice = false;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEYWILD_SLICE_DROPPED"));
}

void AFeywildSetting::SetMistBoost(float K)
{
    const float Nk = FMath::Clamp(K, 0.f, 1.f);
    if (FMath::IsNearlyEqual(Nk, MistK, .002f)) return;
    MistK = Nk; ApplyFog();
}

void AFeywildSetting::ApplyFog()
{
    if (!Fog) return;
    const float G = GloomNow;
    Fog->SetFogDensity(.018f * (1.f + 5.f * MistK) * (1.f + 5.5f * G));   // (2026-11-21: the Deepwood foggier still)
    Fog->SetFogInscatteringColor(FMath::Lerp(FLinearColor(.17f, .3f, .46f), FLinearColor(.045f, .1f, .21f), G));
    Fog->SetDirectionalInscatteringColor(FMath::Lerp(FLinearColor(.8f, .42f, .17f), FLinearColor(.12f, .11f, .14f), G));
    Fog->SetVolumetricFogAlbedo(FMath::Lerp(FLinearColor(FColor(225, 235, 255)), FLinearColor(.45f, .62f, 1.f), G).ToFColor(true));
    Fog->SetVolumetricFogExtinctionScale(FMath::Lerp(.45f, 2.1f, G));
}

// The Deepwood (2026-11-20, the user: much darker, bluer, foggier, a very different part of the forest).
void AFeywildSetting::SetGloom(float G)
{
    G = FMath::Clamp(G, 0.f, 1.f);
    if (FMath::IsNearlyEqual(G, GloomNow, .002f)) return;
    GloomNow = G;
    if (SunLight) { SunLight->SetIntensity(5.5f * FMath::Lerp(1.f, .26f, G)); SunLight->SetLightColor(FMath::Lerp(FLinearColor(1.f, .66f, .36f), FLinearColor(.5f, .58f, .85f), G)); }
    if (FillLight) { FillLight->SetIntensity(FillBase * FMath::Lerp(1.f, 1.1f, G)); FillLight->SetLightColor(FMath::Lerp(FLinearColor(.46f, .64f, 1.f), FLinearColor(.22f, .4f, 1.f), G)); }   // (2026-11-21: the Deepwood's blue fill a little brighter)
    if (Sky) Sky->SetIntensity(SkyBase * FMath::Lerp(1.f, .5f, G));
    if (PostComp)
    {
        PostComp->Settings.AutoExposureBias = -.35f * G;
        PostComp->Settings.ColorGainShadows = FMath::Lerp(FVector4(.86f, .98f, 1.2f, 1.f), FVector4(.72f, .9f, 1.3f, 1.f), G);
        PostComp->Settings.bOverride_ColorSaturation = true; PostComp->Settings.ColorSaturation = FVector4(1, 1, 1, FMath::Lerp(1.16f, .95f, G));
    }
    ApplyFog();
}

void AFeywildSetting::SetExtraVeils(const TArray<FVector4>& V) { ExtraVeils = V; bVeilsDirty = true; }

void AFeywildSetting::BuildEnvironment()
{
    const FVector G0 = FeywildOrigin();
    auto Attach = [this, G0](USceneComponent* C, FVector At = FVector::ZeroVector, FRotator Rot = FRotator::ZeroRotator)
    { C->SetupAttachment(RootComponent); C->SetWorldLocationAndRotation(G0 + At, Rot); C->RegisterComponent(); };
    // The sky: a dome of M_FeySky, blue warming to peach and gold, never purple.
    {
        FFeySurf Dome; constexpr int32 Bands = 18, Segs = 48; constexpr float Rad = 4000000.f;
        TArray<TArray<int32>> Grid;
        for (int32 I = 0; I <= Bands; ++I)
        {
            TArray<int32>& Row = Grid.AddDefaulted_GetRef();
            for (int32 J = 0; J <= Segs; ++J)
            {
                const float Th = FMath::Lerp(-.12f, HALF_PI, float(I) / Bands), Ph = 2 * PI * J / Segs;
                const FVector D(FMath::Cos(Ph) * FMath::Cos(Th), FMath::Sin(Ph) * FMath::Cos(Th), FMath::Sin(Th));
                Row.Add(Dome.Vert(D * Rad, -D, FVector2D::ZeroVector, FColor::White));
            }
        }
        for (int32 I = 0; I < Bands; ++I) for (int32 J = 0; J < Segs; ++J) Dome.Quad(Grid[I][J], Grid[I][J + 1], Grid[I + 1][J + 1], Grid[I + 1][J]);
        UMaterialInterface* SkyMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Feywild/M_FeySky.M_FeySky"));
        auto* Mesh = Emit(TEXT("FeySky"), Dome, SkyMat, false, false);
        Mesh->bNeverDistanceCull = true; Mesh->SetBoundsScale(4.f);
        if (SkyMat)
        {
            auto* Mid = UMaterialInstanceDynamic::Create(SkyMat, this);
            const FVector ToSun = -SunRot.Vector();
            Mid->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(ToSun.X, ToSun.Y, ToSun.Z, 0)); Mesh->SetMaterial(0, Mid);
        }
    }
    auto* Sun = NewObject<UDirectionalLightComponent>(this);
    Sun->SetMobility(EComponentMobility::Movable); Sun->SetAtmosphereSunLight(false);
    Sun->SetIntensity(5.5f); Sun->SetLightColor(FLinearColor(1.f, .66f, .36f)); Sun->SetForwardShadingPriority(1);
    Sun->DynamicShadowDistanceMovableLight = 9000; Sun->DynamicShadowCascades = 4; Sun->SetVolumetricScatteringIntensity(2.2f);
    Sun->SetEnableLightShaftBloom(true); Sun->SetBloomScale(.25f); Sun->SetBloomThreshold(.6f); Sun->SetBloomTint(FColor(255, 196, 130));
    Attach(Sun, FVector::ZeroVector, SunRot);
    SunLight = Sun;
    auto* SkyLight = NewObject<USkyLightComponent>(this);
    SkyLight->SetMobility(EComponentMobility::Movable); SkyLight->bRealTimeCapture = false; SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene; SkyLight->SkyDistanceThreshold = 100000.f;
    SkyLight->SetIntensity(1.7f); SkyLight->SetLightColor(FLinearColor(.72f, .86f, 1.f));
    SkyLight->SetLowerHemisphereColor(FLinearColor(.05f, .09f, .1f)); SkyLight->bLowerHemisphereIsBlack = true;
    Attach(SkyLight);
    Sky = SkyLight; SkyBase = SkyLight->Intensity;
    FTimerHandle Capture; GetWorldTimerManager().SetTimer(Capture, [SkyLight]() { SkyLight->RecaptureSky(); UE_LOG(LogTemp, Display, TEXT("CHUCK_FEYWILD_SKY_CAPTURED")); }, .6f, false);
    auto* Fill = NewObject<UDirectionalLightComponent>(this);
    Fill->SetMobility(EComponentMobility::Movable); Fill->SetAtmosphereSunLight(false); Fill->SetForwardShadingPriority(0);
    Fill->SetIntensity(1.7f); Fill->SetLightColor(FLinearColor(.46f, .64f, 1.f)); Fill->SetCastShadows(false); Fill->SetVolumetricScatteringIntensity(0.f);
    Attach(Fill, FVector::ZeroVector, FRotator(-38.f, 20.f, 0.f));
    FillLight = Fill; FillBase = Fill->Intensity;
    Fog = NewObject<UExponentialHeightFogComponent>(this);
    Fog->SetFogDensity(.018f); Fog->SetFogHeightFalloff(.18f); Fog->SetFogMaxOpacity(.72f); Fog->FogCutoffDistance = 3000000.f; Fog->SetStartDistance(900.f);
    Fog->SetFogInscatteringColor(FLinearColor(.17f, .3f, .46f));
    Fog->SetDirectionalInscatteringExponent(7.f); Fog->SetDirectionalInscatteringStartDistance(1200.f); Fog->SetDirectionalInscatteringColor(FLinearColor(.8f, .42f, .17f));
    Fog->SetVolumetricFog(true); Fog->SetVolumetricFogScatteringDistribution(.72f); Fog->SetVolumetricFogExtinctionScale(.45f); Fog->SetVolumetricFogDistance(5500.f);
    Fog->SetVolumetricFogAlbedo(FColor(225, 235, 255));
    Attach(Fog, FVector(0, 0, -150));
    auto* Post = NewObject<UPostProcessComponent>(this);
    Post->bUnbound = true; Post->Priority = 50.f;
    FPostProcessSettings& P = Post->Settings;
    P.bOverride_AutoExposureMethod = true; P.AutoExposureMethod = AEM_Histogram;
    P.bOverride_AutoExposureMinBrightness = true; P.AutoExposureMinBrightness = .85f;
    P.bOverride_AutoExposureMaxBrightness = true; P.AutoExposureMaxBrightness = 1.5f;
    P.bOverride_AutoExposureBias = true; P.AutoExposureBias = 0.f;
    P.bOverride_BloomIntensity = true; P.BloomIntensity = 1.15f;
    P.bOverride_BloomThreshold = true; P.BloomThreshold = 1.1f;
    P.bOverride_VignetteIntensity = true; P.VignetteIntensity = .38f;
    P.bOverride_ColorSaturation = true; P.ColorSaturation = FVector4(1, 1, 1, 1.16f);
    P.bOverride_ColorContrast = true; P.ColorContrast = FVector4(1, 1, 1, 1.08f);
    P.bOverride_ColorGainShadows = true; P.ColorGainShadows = FVector4(.86f, .98f, 1.2f, 1.f);
    P.bOverride_ColorGainHighlights = true; P.ColorGainHighlights = FVector4(1.06f, 1.f, .92f, 1.f);
    P.bOverride_AmbientOcclusionIntensity = true; P.AmbientOcclusionIntensity = .7f;
    Attach(Post);
    PostComp = Post;
    // Invisible bounds of the slice.
    auto Wall = [&](const FVector& Ce, const FVector& Half)
    {
        auto* B = NewObject<UBoxComponent>(this); B->SetBoxExtent(Half); B->SetCollisionProfileName(TEXT("BlockAll")); B->SetHiddenInGame(true);
        Attach(B, Ce);
    };
    // (2026-11-19, stage 3: open to the west, where the path goes on into the riverbank's Blooming Path, FeyMap.h)
    constexpr float WestX = -4200;
    const float Mx = (WestX + MaxX) * .5f, Hx = (MaxX - WestX) * .5f;
    Wall(FVector(MaxX + 50, 0, 2000), FVector(50, HalfY + 100, 4000));
    Wall(FVector(Mx, HalfY + 50, 2000), FVector(Hx + 100, 50, 4000)); Wall(FVector(Mx, -HalfY - 50, 2000), FVector(Hx + 100, 50, 4000));
    // The overlook's edge, and along the top of the falls' side where it drops to the valley.
    constexpr float EdgeX = 5470, EdgeY = 540;
    Wall(FVector(EdgeX + 50, (EdgeY - HalfY) * .5f, 2000), FVector(50, (EdgeY + HalfY) * .5f + 50, 4000));
    Wall(FVector((EdgeX + MaxX) * .5f, EdgeY + 50, 2000), FVector((MaxX - EdgeX) * .5f + 50, 50, 4000));
}

void AFeywildSetting::BuildLand()
{
    auto Mat = [](const TCHAR* Path) { return LoadObject<UMaterialInterface>(nullptr, Path); };
    // The land: a grid over the slice and well beyond it, so the bounds never show an edge.
    {
        FFeySurf L; constexpr float X0 = -4200, X1 = 8600, Y0 = -5600, Y1 = 5600, Step = 44; const int32 Nx = FMath::CeilToInt((X1 - X0) / Step), Ny = FMath::CeilToInt((Y1 - Y0) / Step);
        L.V.Reserve((Nx + 1) * (Ny + 1));
        for (int32 I = 0; I <= Nx; ++I)
            for (int32 J = 0; J <= Ny; ++J)
            {
                const float X = X0 + I * Step, Y = Y0 + J * Step, H = Ground(X, Y), E = 30.f;
                const FVector Nn = FVector(Ground(X - E, Y) - Ground(X + E, Y), Ground(X, Y - E) - Ground(X, Y + E), 2 * E).GetSafeNormal();
                const float Pk = PathK(X, Y), Pe = PondE(X, Y);
                const float Damp = FMath::Max(Smooth(1.45f, 1.02f, Pe), Pk * .55f);
                const float Moss = FMath::Clamp(.8f - Pk * 1.3f + .5f * Noise(X, Y, 500, 21), 0.f, 1.f) * FMath::Lerp(.35f, 1.f, Nn.Z * Nn.Z);
                L.Vert(FVector(X, Y, H), Nn, FVector2D(X, Y) / 300.f, FLinearColor(Pk, Moss, Damp, 1).ToFColor(false));
            }
        for (int32 I = 0; I < Nx; ++I) for (int32 J = 0; J < Ny; ++J) { const int32 A = I * (Ny + 1) + J, B = A + Ny + 1; L.Quad(A, A + 1, B + 1, B); }
        Emit(TEXT("FeyLand"), L, Mat(TEXT("/Game/Art/Feywild/M_FeyGround.M_FeyGround")), true, true);
    }
    // The pond, and the stream on each rock step.
    {
        FFeySurf W; auto* Water = Mat(TEXT("/Game/Art/Feywild/M_FeyWater.M_FeyWater"));
        auto Sheet = [&](float X0, float X1, float Y0, float Y1, float Z, float Step, bool bPond)
        {
            const int32 Base = W.V.Num(), Nx = FMath::CeilToInt((X1 - X0) / Step), Ny = FMath::CeilToInt((Y1 - Y0) / Step);
            for (int32 I = 0; I <= Nx; ++I) for (int32 J = 0; J <= Ny; ++J)
            {
                const float X = FMath::Lerp(X0, X1, float(I) / Nx), Y = FMath::Lerp(Y0, Y1, float(J) / Ny);
                const float Shore = bPond ? Smooth(.68f, 1.f, PondE(X, Y)) : .8f;
                const float Churn = bPond ? Smooth(620.f, 120.f, FVector2D(X - LedgeX, (Y - FallY) * .55f).Size()) : .7f;
                W.Vert(FVector(X, Y, Z), FVector::UpVector, FVector2D(X, Y) / 200.f, FLinearColor(Shore, Churn, 0, 1).ToFColor(false));
            }
            for (int32 I = 0; I < Nx; ++I) for (int32 J = 0; J < Ny; ++J) { const int32 A = Base + I * (Ny + 1) + J, B = A + Ny + 1; W.Quad(A, A + 1, B + 1, B); }
        };
        Sheet(PondX - PondRX * 1.2f, LedgeX + 40, PondY - PondRY * 1.2f, PondY + PondRY * 1.2f, WaterZ, 60, true);
        WaterMesh = Emit(TEXT("FeyWater"), W, Water, false, false);
    }
    // The stream on each rock step, running west to its lip (M_FeyRiver, 2026-11-19: the river cutscene rides them). Wide
    // enough that their edges lie under the banks; on back east past where the cutscene films them from.
    {
        FFeySurf St; constexpr float Half = FallHalf + 150.f, Ds = 50.f; constexpr int32 Across = 10;
        auto Stream = [&](float X0, float X1, float Z)
        {
            const int32 Base = St.V.Num(), Rows = FMath::CeilToInt((X1 - X0) / Ds);
            for (int32 I = 0; I <= Rows; ++I)
                for (int32 J = 0; J <= Across; ++J)
                {
                    const float X = FMath::Lerp(X1, X0, float(I) / Rows), D = FMath::Lerp(-Half, Half, float(J) / Across);
                    const float Edge = FMath::Clamp(FMath::Abs(D) / FallHalf, 0.f, 1.f), Lip = Smooth(X0 + 500.f, X0, X);
                    St.Vert(FVector(X, FallY + D, Z), FVector::UpVector, FVector2D(-X, D) / 200.f, FLinearColor(Edge, FMath::Clamp(.3f + .4f * Lip + .15f * Edge, 0.f, 1.f), 1, 1).ToFColor(false));
                }
            for (int32 I = 0; I < Rows; ++I) for (int32 J = 0; J < Across; ++J) { const int32 A = Base + I * (Across + 1) + J, B = A + Across + 1; St.Quad(A, B, B + 1, A + 1); }
        };
        Stream(LedgeX + 40, Ledge2X + 40, LedgeTop - 14);
        Stream(Ledge2X + 40, Ledge2X + 1240, Ledge2Top - 14);
        StreamMesh = Emit(TEXT("FeyStreams"), St, Mat(TEXT("/Game/Art/Feywild/M_FeyRiver.M_FeyRiver")), false, false);
    }
    // The falls: curtains curving out over each lip and down.
    {
        FFeySurf F;
        auto Curtain = [&](float LipX, float Top, float Bottom, float Half, float Y, float Out, float Alpha)
        {
            constexpr int32 Across = 20, Down = 14; const int32 Base = F.V.Num();
            for (int32 I = 0; I <= Down; ++I)
            {
                const float V = float(I) / Down, Z = FMath::Lerp(Top, Bottom, V), X = LipX - Out * FMath::Sqrt(V);
                for (int32 J = 0; J <= Across; ++J)
                {
                    const float U = float(J) / Across, Sd = FMath::Abs(U - .5f) * 2;
                    const float Ragged = 1.f;
                    const float A = Alpha * Smooth(1.f, .72f, Sd) * Ragged;
                    F.Vert(FVector(X + 14.f * FMath::Sin(U * 19.f) * V, Y + (U - .5f) * 2 * Half * (1 + .06f * V), Z), FVector(-1, 0, 0), FVector2D(U * Half / 230.f, V * (Top - Bottom) / 520.f),
                           FLinearColor(1.f - .4f * V, 0, 0, A).ToFColor(false));
                }
            }
            for (int32 I = 0; I < Down; ++I) for (int32 J = 0; J < Across; ++J) { const int32 A = Base + I * (Across + 1) + J, B = A + Across + 1; F.Quad(A, B, B + 1, A + 1); }
        };
        Curtain(LedgeX + 46, LedgeTop - 10, WaterZ - 4, FallHalf, FallY, 290, 1.f);   // (clear of the rock face all the way down)
        Curtain(LedgeX + 30, LedgeTop - 10, WaterZ - 4, FallHalf * .86f, FallY + 22, 245, .75f);
        Curtain(Ledge2X + 46, Ledge2Top - 10, LedgeTop - 16, FallHalf * .9f, FallY, 260, 1.f);
        Emit(TEXT("FeyFalls"), F, Mat(TEXT("/Game/Art/Feywild/M_FeyFall.M_FeyFall")), false, false);
    }
}

// The valley and the ridges beyond the overlook: one fan of land opening eastward, and the far trees on it.
void AFeywildSetting::BuildFarLand()
{
    auto* FarMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Feywild/M_FeyFar.M_FeyFar")); Missing += !FarMat;
    if (FarMat)
    {
        FarMid = UMaterialInstanceDynamic::Create(FarMat, this);
        const FVector ToSun = -SunRot.Vector();
        FarMid->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(ToSun.X, ToSun.Y, ToSun.Z, 0)); FarMid->SetScalarParameterValue(TEXT("OriginX"), FeywildOrigin().X);
    }
    FFeySurf L; constexpr int32 Nu = 110, Nv = 200;
    auto At = [](float U, float W) { const float X = 8200.f + 700000.f * FMath::Pow(U, 2.6f); return FVector2D(X, W * (6000.f + 1.3f * (X - 8000.f))); };
    for (int32 I = 0; I <= Nu; ++I)
        for (int32 J = 0; J <= Nv; ++J)
        {
            const FVector2D P = At(float(I) / Nu, float(J) / Nv * 2.f - 1.f);
            const float E = FMath::Max(120.f, P.X * .012f), H = Ground(P.X, P.Y) - (I == 0 ? 30.f : 0.f);
            const FVector Nn = FVector(Ground(P.X - E, P.Y) - Ground(P.X + E, P.Y), Ground(P.X, P.Y - E) - Ground(P.X, P.Y + E), 2 * E).GetSafeNormal();
            L.Vert(FVector(P.X, P.Y, H), Nn, FVector2D::ZeroVector, FColor(150, 0, 0, 255));
        }
    for (int32 I = 0; I < Nu; ++I) for (int32 J = 0; J < Nv; ++J) { const int32 A = I * (Nv + 1) + J, B = A + Nv + 1; L.Quad(A, A + 1, B + 1, B); }
    auto* Land = Emit(TEXT("FeyFarLand"), L, FarMid, false, false); Land->bNeverDistanceCull = true;
    // Trees, thick in the valley and thinning to the second ridge, bigger far off so they still read as a forest.
    FRandomStream R(203);
    auto* Far = Kit(TEXT("/Game/Art/Feywild/SM_FeyFarTree.SM_FeyFarTree"), 0, 0, false); if (FarMid) Far->SetMaterial(0, FarMid);
    for (int32 I = 0; I < 4200; ++I)
    {
        const float X = 15000.f + 118000.f * FMath::Pow(R.FRand(), 1.7f), Y = -1400.f + R.FRandRange(-1.f, 1.f) * (2600.f + X * .95f);
        if (Noise(X, Y, 5200, 50) < -.28f) continue;
        const float S = FMath::Lerp(.75f, 3.6f, FMath::Clamp((X - 15000.f) / 100000.f, 0.f, 1.f)) * R.FRandRange(.75f, 1.35f);
        Far->AddInstance(FTransform(FRotator(0, R.FRandRange(0.f, 360.f), 0), FVector(X, Y, Ground(X, Y) - 30.f), FVector(S, S, S * R.FRandRange(.85f, 1.2f)))); ++Planted;
    }
    // The landmark: one vast old oak on the flat-topped rise, and amber giants standing up out of the valley's forest.
    auto* Great = Kit(TEXT("/Game/Art/Feywild/SM_FeyOak0.SM_FeyOak0"), 0, 0, false);
    Great->AddInstance(FTransform(FRotator(0, 40.f, 0), FVector(GreatX, GreatY, Ground(GreatX, GreatY) - 200.f), FVector(9.f))); ++Planted;
    // Real oaks fill the valley right under the overlook, where the far trees would show as what they are.
    auto* Under0 = Kit(TEXT("/Game/Art/Feywild/SM_FeyOak0.SM_FeyOak0"), 0, 0, false), *Under1 = Kit(TEXT("/Game/Art/Feywild/SM_FeyOak1.SM_FeyOak1"), 0, 0, false);
    for (int32 I = 0; I < 26; ++I)
    {
        const float X = R.FRandRange(6900.f, 15000.f), Y = R.FRandRange(-4600.f, -300.f - (X - 6900.f) * -.2f);
        if (Vista(Y) < .85f) continue;
        const float S = R.FRandRange(1.f, 1.5f);   // (their crowns stay below the overlook's edge)
        (I % 2 ? Under1 : Under0)->AddInstance(FTransform(FRotator(0, R.FRandRange(0.f, 360.f), 0), FVector(X, Y, Ground(X, Y) - 20.f), FVector(S))); ++Planted;
    }
    auto* Tall = Kit(TEXT("/Game/Art/Feywild/SM_FeyCapAmber.SM_FeyCapAmber"), 0, 0, false);
    const FVector Caps[] = {{16500, -3300, 5.f}, {23000, 1200, 7.f}, {29000, -8200, 8.f}, {36000, -1500, 10.f}, {47000, -13500, 12.f}, {54000, 6500, 13.f}};
    for (const FVector& C : Caps) { Tall->AddInstance(FTransform(FRotator(0, R.FRandRange(0.f, 360.f), 0), FVector(C.X, C.Y, Ground(C.X, C.Y) - 40.f), FVector(C.Z))); ++Planted; }
}

// A one-off capture of the finished glade from the pond's middle, for M_FeyWater to mirror: no scene is drawn twice per frame.
void AFeywildSetting::BuildProbe()
{
    ProbeTarget = NewObject<UTextureRenderTargetCube>(this);
    ProbeTarget->Init(1024, PF_FloatRGBA); ProbeTarget->ClearColor = FLinearColor::Black; ProbeTarget->UpdateResourceImmediate(true);
    Probe = NewObject<USceneCaptureComponentCube>(this); Probe->SetupAttachment(RootComponent);
    Probe->SetRelativeLocation(FVector(PondX, PondY, WaterZ + 60.f)); Probe->TextureTarget = ProbeTarget; Probe->CaptureSource = SCS_SceneColorHDR;
    Probe->bCaptureEveryFrame = false; Probe->bCaptureOnMovement = false;
    Probe->HideComponent(WaterMesh); Probe->HideComponent(Lights); Probe->HideComponent(Mist); Probe->HideComponent(Rings);
    if (AActor* Chuck = UGameplayStatics::GetPlayerPawn(this, 0)) Probe->HideActorComponents(Chuck);
    Probe->RegisterComponent();
    if (WaterMesh)
        if (UMaterialInterface* Water = WaterMesh->GetMaterial(0))
        {
            auto* Mid = UMaterialInstanceDynamic::Create(Water, this);
            Mid->SetTextureParameterValue(TEXT("Reflection"), ProbeTarget); WaterMesh->SetMaterial(0, Mid);
        }
    if (StreamMesh)
        if (UMaterialInterface* Running = StreamMesh->GetMaterial(0))
        {
            auto* Mid = UMaterialInstanceDynamic::Create(Running, this);
            Mid->SetTextureParameterValue(TEXT("Reflection"), ProbeTarget); StreamMesh->SetMaterial(0, Mid);
        }
    // Once the sky light has settled, and again when the textures have streamed in.
    for (const float When : {1.f, 6.f})
    {
        FTimerHandle Capture;
        GetWorldTimerManager().SetTimer(Capture, FTimerDelegate::CreateWeakLambda(this, [this]() { if (!Probe) return; Probe->CaptureScene(); UE_LOG(LogTemp, Display, TEXT("CHUCK_FEYWILD_PROBE_CAPTURED")); }), When, false);
    }
}

void AFeywildSetting::BuildSound()
{
    SplashSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_StreamSplash_02.SFX_StreamSplash_02"));
    USoundBase* Falls = LoadObject<USoundBase>(nullptr, TEXT("/Game/Art/Audio/SFX/SFX_FeyFalls_00.SFX_FeyFalls_00")); Missing += !Falls;
    if (!Falls) return;
    FallsSound = NewObject<UAudioComponent>(this); FallsSound->SetupAttachment(RootComponent);
    FallsSound->SetRelativeLocation(FVector(LedgeX - 160.f, FallY, 200.f)); FallsSound->SetSound(Falls); FallsSound->bAutoActivate = false;
    FallsSound->bOverrideAttenuation = true;
    FSoundAttenuationSettings& A = FallsSound->AttenuationOverrides;
    A.bAttenuate = true; A.bSpatialize = true; A.AttenuationShape = EAttenuationShape::Sphere; A.AttenuationShapeExtents = FVector(650.f, 0.f, 0.f); A.FalloffDistance = 4200.f; A.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
    FallsSound->SetVolumeMultiplier(.62f); FallsSound->RegisterComponent(); FallsSound->Play();
}

void AFeywildSetting::BuildPlanting()
{
    FRandomStream R(77);
    const FString Dir = TEXT("/Game/Art/Feywild/");
    auto Path = [&Dir](const TCHAR* Name) { return Dir + Name + TEXT(".") + Name; };
    auto Place = [&](UHierarchicalInstancedStaticMeshComponent* H, float X, float Y, float Scale, float Yaw, float Sink = 0.f, float Tilt = 0.f)
    {
        H->AddInstance(FTransform(FRotator(R.FRandRange(-Tilt, Tilt), Yaw, R.FRandRange(-Tilt, Tilt)), FVector(X, Y, Ground(X, Y) - Sink), FVector(Scale)));
        ++Planted;
    };
    // Where something may grow: on land, off the path by Clear cm, away from where he arrives.
    auto Free = [&](float X, float Y, float Clear)
    {
        if (PondE(X, Y) < 1.06f || FMath::Abs(Y - PathY(X)) < Clear || FVector2D(X + 300.f, Y).Size() < 200.f) return false;
        const bool bOpen = Vista(Y) > .5f;
        if (bOpen && X > CliffX(Y) - 110.f) return false;
        if (!bOpen && X > 5100.f && X < LedgeX + 160.f) return false;
        if (!bOpen && X > LedgeX - 130.f && X < LedgeX + 160.f) return false;
        if (FVector2D(X - LedgeX, Y - FallY).Size() < 700.f) return false;
        if (X > Ledge2X - 130.f && X < Ledge2X + 160.f) return false;
        if (X > LedgeX && FMath::Abs(Y - FallY) < FallHalf + 70.f) return false;
        return true;
    };
    auto Scatter = [&](UHierarchicalInstancedStaticMeshComponent* H, int32 Count, float X0, float X1, float Y0, float Y1, float Clear, float S0, float S1, float Sink, float Tilt, TFunctionRef<float(float, float)> Chance)
    {
        for (int32 I = 0; I < Count; ++I)
        {
            const float X = R.FRandRange(X0, X1), Y = R.FRandRange(Y0, Y1);
            if (!Free(X, Y, Clear) || R.FRand() > Chance(X, Y)) continue;
            Place(H, X, Y, R.FRandRange(S0, S1), R.FRandRange(0.f, 360.f), Sink, Tilt);
        }
    };
    auto Always = [](float, float) { return 1.f; };
    auto Clumps = [](float Scale, float Seed) { return [Scale, Seed](float X, float Y) { return Smooth(-.05f, .3f, Noise(X, Y, Scale, Seed)); }; };
    constexpr float NX0 = -2000, NX1 = 5450, NY0 = -3900, NY1 = 3900;

    // ------------------------------ the ancient oaks: placed by hand to frame the path, as the goal images do
    auto* Oak0 = Kit(*Path(TEXT("SM_FeyOak0")), 16000, 26000, true), *Oak1 = Kit(*Path(TEXT("SM_FeyOak1")), 16000, 26000, true);
    auto* Col0 = Kit(*Path(TEXT("SM_FeyOak0Collision")), 0, 0, false, true, true), *Col1 = Kit(*Path(TEXT("SM_FeyOak1Collision")), 0, 0, false, true, true);
    struct FTree { int32 Kind; float X, Y, Scale, Yaw; };
    const FTree Trees[] = {{0, 1050, -1560, 1.3f, 40}, {1, 2500, -2650, 1.15f, 200}, {0, 4350, -2300, 1.25f, 120}, {1, 1500, 3050, 1.3f, 300}, {0, 4600, 3350, 1.45f, 10}, {1, -900, 2050, 1.1f, 80},
                           {1, -1100, -2500, 1.2f, 150}, {0, 5050, -3150, 1.5f, 260}, {1, 7300, 3100, 1.7f, 30}, {0, 7900, -1000, 2.1f, 100}, {1, 8700, 1500, 2.3f, 210}, {0, 9700, -3200, 2.6f, 330}, {1, 10000, 3900, 2.8f, 60}};
    for (const FTree& T : Trees)
    {
        Place(T.Kind ? Oak1 : Oak0, T.X, T.Y, T.Scale, T.Yaw, 14.f);
        (T.Kind ? Col1 : Col0)->AddInstance(FTransform(FRotator(0, T.Yaw, 0), FVector(T.X, T.Y, Ground(T.X, T.Y) - 14.f), FVector(T.Scale)));
    }
    // ------------------------------ the amber-gilled giants, near and, house-high, on the far rises
    auto* Amber = Kit(*Path(TEXT("SM_FeyCapAmber")), 22000, 30000, true);
    const FVector Giants[] = {{2750, -1180, 1.f}, {3500, -1500, 1.45f}, {3050, -1950, .72f}, {380, 1500, 1.25f}, {-250, 1180, .7f}, {6900, 2500, 3.2f}, {7600, 3700, 4.2f}};
    for (const FVector& G : Giants) Place(Amber, G.X, G.Y, G.Z, R.FRandRange(0.f, 360.f), 6.f, 2.f);
    // ------------------------------ bell-flower lamps along the path, each with a small real light
    auto* Bells = Kit(*Path(TEXT("SM_FeyBellLamp")), 9000, 14000, true);
    const FVector2D Lamps[] = {{620, 210}, {1650, 215}, {2500, -215}, {3350, -160}, {4250, -170}, {5230, 190}};
    for (int32 I = 0; I < UE_ARRAY_COUNT(Lamps); ++I)
    {
        const float X = Lamps[I].X, Y = PathY(X) + Lamps[I].Y, Yaw = Lamps[I].Y > 0 ? -90.f : 90.f;
        Place(Bells, X, Y, 1.35f, Yaw, 2.f);
        if (I % 2 == 1) continue;   // (the rest glow by their material alone)
        auto* L = NewObject<UPointLightComponent>(this); L->SetupAttachment(RootComponent);
        L->SetRelativeLocation(FVector(X, Y + (Lamps[I].Y > 0 ? -1 : 1) * 95.f, Ground(X, Y) + 145.f));
        L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(26.f); L->SetLightColor(FLinearColor(1.f, .66f, .32f)); L->SetAttenuationRadius(620.f); L->SetSourceRadius(14.f);
        L->SetCastShadows(true); L->SetVolumetricScatteringIntensity(.6f); L->RegisterComponent();
    }
    // ------------------------------ glowing fungi, gathered round the roots and the path's edges
    auto Rooted = [&](float X, float Y)
    {
        float Best = 0; for (const FTree& T : Trees) Best = FMath::Max(Best, Smooth(1150.f * T.Scale, 240.f, FVector2D(X - T.X, Y - T.Y).Size()));
        return FMath::Max(Best, .3f * Smooth(620.f, 260.f, FMath::Abs(Y - PathY(X))));
    };
    Scatter(Kit(*Path(TEXT("SM_FeyCapBlue")), 7000, 11000, false), 900, NX0, NX1, NY0, NY1, 150, .7f, 2.1f, 1.f, 5.f, Rooted);
    Scatter(Kit(*Path(TEXT("SM_FeyCapViolet")), 7000, 11000, false), 520, NX0, NX1, NY0, NY1, 190, .8f, 2.f, 1.f, 5.f, Rooted);
    // ------------------------------ the undergrowth
    UHierarchicalInstancedStaticMeshComponent* Fern[4]; for (int32 I = 0; I < 4; ++I) { const FString N = FString::Printf(TEXT("/Game/Art/ChultNature/SM_ChultFern%d.SM_ChultFern%d"), I, I); Fern[I] = Kit(*N, 6000, 9000, false); }
    for (int32 I = 0; I < 4; ++I) Scatter(Fern[I], 420, NX0, NX1, NY0, NY1, 170, 1.f, 1.9f, 2.f, 8.f, Clumps(620, I + 3));
    Scatter(Kit(*Path(TEXT("SM_FeyBroadLeaf")), 6000, 9000, false), 420, NX0, NX1, NY0, NY1, 180, .8f, 1.7f, 2.f, 6.f, Clumps(720, 8));
    Scatter(Kit(*Path(TEXT("SM_FeyCrimsonLeaf")), 6000, 9000, false), 340, NX0, NX1, NY0, NY1, 180, .8f, 1.6f, 2.f, 6.f, Clumps(560, 9));
    Scatter(Kit(*Path(TEXT("SM_FeyFoxglove")), 7000, 11000, false), 300, NX0, NX1, NY0, NY1, 210, .9f, 1.5f, 2.f, 4.f, Clumps(460, 10));
    Scatter(Kit(*Path(TEXT("SM_FeyCelandine0")), 3200, 5000, false), 1100, NX0, NX1, NY0, NY1, 120, 1.6f, 2.8f, 1.f, 4.f, Clumps(380, 11));
    Scatter(Kit(*Path(TEXT("SM_FeyCelandine1")), 3200, 5000, false), 1100, NX0, NX1, NY0, NY1, 120, 1.6f, 2.8f, 1.f, 4.f, Clumps(380, 12));
    auto Walked = [](float X, float Y) { return FMath::Max(Smooth(1700.f, 800.f, FMath::Abs(Y - PathY(X))), Smooth(1.75f, 1.2f, PondE(X, Y))); };
    Scatter(Kit(*Path(TEXT("SM_FeyGrass0")), 3600, 5400, false), 46000, NX0, NX1, NY0, NY1, 125, 2.6f, 4.4f, 1.f, 7.f, Walked);
    Scatter(Kit(*Path(TEXT("SM_FeyGrass1")), 3600, 5400, false), 30000, NX0, NX1, NY0, NY1, 150, 2.f, 3.6f, 1.f, 7.f, Walked);
    // Fallen petals drifted on the path.
    auto* Fall = Kit(*Path(TEXT("SM_FeyPetals")), 3500, 5500, false);
    for (int32 I = 0; I < 150; ++I) { const float X = R.FRandRange(-600.f, 4700.f), Y = PathY(X) + R.FRandRange(-170.f, 170.f); Place(Fall, X, Y, R.FRandRange(.8f, 1.6f), R.FRandRange(0.f, 360.f), -1.f); }
    // ------------------------------ mossy boulders, and the rock of the two steps
    UHierarchicalInstancedStaticMeshComponent* Rock[6];
    for (int32 I = 0; I < 6; ++I) { const FString N = FString::Printf(TEXT("/Game/Art/ChultNature/SM_ChultRock%d.SM_ChultRock%d"), I, I); Rock[I] = Kit(*N, 14000, 22000, true, true); }
    for (int32 I = 0; I < 110; ++I)
    {
        const float X = R.FRandRange(NX0, NX1), Y = R.FRandRange(NY0, NY1);
        if (FMath::Abs(Y - PathY(X)) < 800.f || (X > 4200.f && Vista(Y) > .4f)) continue;   // (the walk and the view from the edge stay open)
        if (Free(X, Y, 260)) Place(Rock[R.RandRange(0, 5)], X, Y, R.FRandRange(.7f, 2.4f), R.FRandRange(0.f, 360.f), 18.f, 10.f);
    }
    // Low stones along the overlook's edge, with gaps: it reads as an edge and the view stays open.
    for (float Y = -4000.f; Y < 420.f; Y += R.FRandRange(240.f, 560.f))
        Place(Rock[R.RandRange(0, 5)], CliffX(Y) - R.FRandRange(50.f, 110.f), Y, R.FRandRange(.6f, 1.15f), R.FRandRange(0.f, 360.f), 16.f, 10.f);
    for (int32 Step = 0; Step < 2; ++Step)
        for (float Y = -4300.f; Y < 4300.f; Y += R.FRandRange(150.f, 260.f))
        {
            if (FMath::Abs(Y - FallY) < FallHalf + 190.f || Vista(Y) > .2f) continue;
            const float X = (Step ? Ledge2X : LedgeX) + R.FRandRange(30.f, 90.f), Top = Step ? Ledge2Top : LedgeTop, Base = Step ? LedgeTop : WaterZ;
            for (int32 Row = 0; Row < 4; ++Row)
            {
                const float Z = FMath::Lerp(Base - 40.f, Top - 120.f, Row / 3.f) + R.FRandRange(-20.f, 20.f);
                Rock[R.RandRange(0, 5)]->AddInstance(FTransform(FRotator(R.FRandRange(-16.f, 16.f), R.FRandRange(0.f, 360.f), R.FRandRange(-16.f, 16.f)), FVector(X, Y + R.FRandRange(-70.f, 70.f), Z), FVector(R.FRandRange(1.6f, 3.f))));
                ++Planted;
            }
        }
    // ------------------------------ on the pond: lily pads in drifts, pink lilies among them
    auto* Pads = Kit(*Path(TEXT("SM_FeyLilyPad")), 12000, 18000, false), *Lily = Kit(*Path(TEXT("SM_FeyLily")), 9000, 14000, false);
    for (int32 I = 0; I < 420; ++I)
    {
        const float A = R.FRandRange(0.f, 2 * PI), Rr = FMath::Sqrt(R.FRand()) * .94f;
        const float X = PondX + PondRX * Rr * FMath::Cos(A), Y = PondY + PondRY * Rr * FMath::Sin(A);
        if (PondE(X, Y) > .95f || FVector2D(X - LedgeX, (Y - FallY) * .6f).Size() < 720.f || Noise(X, Y, 520, 17) < -.05f) continue;
        const float S = R.FRandRange(.55f, 1.5f);
        Pads->AddInstance(FTransform(FRotator(0, R.FRandRange(0.f, 360.f), 0), FVector(X, Y, WaterZ + .6f), FVector(S, S, 1))); ++Planted;
        if (R.FRand() < .17f) { Lily->AddInstance(FTransform(FRotator(0, R.FRandRange(0.f, 360.f), 0), FVector(X + R.FRandRange(-16.f, 16.f), Y + R.FRandRange(-16.f, 16.f), WaterZ + 3.f), FVector(R.FRandRange(.9f, 1.5f)))); ++Planted; }
    }
}

// ------------------------------------------------------------------ the river's set (stage 2, 2026-11-19; FeyRiverScene.h)
// A root tunnel he surfaces in, dark; its mouth; then the river between banks that grow from bare rock and root to moss,
// fungi, flowers and old oaks, east toward the low sun. Only the cutscene sees it: built when it starts, hidden after.
void AFeywildSetting::BuildRiver()
{
    if (bRiver) { ShowRiver(true); return; }
    bRiver = true;
    const double Started = FPlatformTime::Seconds();
    const int32 KitsBefore = Kits.Num(), TrianglesBefore = Triangles, PlantedBefore = Planted;
    auto Mat = [](const TCHAR* Path) { return LoadObject<UMaterialInterface>(nullptr, Path); };
    auto Lush = [](float S) { return Smooth(RiverMouth + 300.f, RiverMouth + 2800.f, S); };
    // How much light reaches into the root tunnel: little, until near its mouth.
    auto TunnelLight = [](float S) { return FMath::Lerp(.05f, 1.f, Smooth(RiverMouth - 420.f, RiverMouth + 40.f, S)); };
    // (front faces wound to the normal wanted, whichever way the rows run)
    auto Face = [](FFeySurf& S, int32 A, int32 B, int32 C, int32 D)
    {
        const FVector Want = S.N[A] + S.N[C];
        if (FVector::DotProduct(FVector::CrossProduct(S.V[B] - S.V[A], S.V[C] - S.V[A]), Want) > 0.f) S.Quad(A, D, C, B); else S.Quad(A, B, C, D);
    };
    // The valley: one grid, its height from the river's middle (RiverGround).
    {
        constexpr float X0 = -25000, X1 = -5400, Y0 = -33900, Y1 = -26300, Step = 75;
        const int32 Nx = FMath::CeilToInt((X1 - X0) / Step), Ny = FMath::CeilToInt((Y1 - Y0) / Step), Count = (Nx + 1) * (Ny + 1);
        TArray<float> H, Ss, Ds; H.SetNumUninitialized(Count); Ss.SetNumUninitialized(Count); Ds.SetNumUninitialized(Count);
        ParallelFor(Nx + 1, [&](int32 I)
        {
            for (int32 J = 0; J <= Ny; ++J)
            {
                const int32 K = I * (Ny + 1) + J; const float X = X0 + I * Step, Y = Y0 + J * Step;
                H[K] = RiverGround(X, Y); RiverNearest(X, Y, Ss[K], Ds[K]);
            }
        });
        FFeySurf L; L.V.Reserve(Count);
        for (int32 I = 0; I <= Nx; ++I)
            for (int32 J = 0; J <= Ny; ++J)
            {
                const int32 K = I * (Ny + 1) + J; const float X = X0 + I * Step, Y = Y0 + J * Step;
                const float Hx0 = H[FMath::Max(I - 1, 0) * (Ny + 1) + J], Hx1 = H[FMath::Min(I + 1, Nx) * (Ny + 1) + J], Hy0 = H[I * (Ny + 1) + FMath::Max(J - 1, 0)], Hy1 = H[I * (Ny + 1) + FMath::Min(J + 1, Ny)];
                const FVector Nn = FVector(Hx0 - Hx1, Hy0 - Hy1, 4.f * Step).GetSafeNormal();
                const float W = RiverHalf(Ss[K]), A = FMath::Abs(Ds[K]);
                const float Damp = Smooth(W + 260.f, W - 20.f, A);
                const float Moss = FMath::Clamp(.25f + .65f * Lush(Ss[K]) + .45f * Noise(X, Y, 500, 63), 0.f, 1.f) * FMath::Lerp(.3f, 1.f, Nn.Z * Nn.Z);
                const float Lit = A < 480.f ? TunnelLight(Ss[K]) : 1.f;   // (inside the root tunnel)
                L.Vert(FVector(X, Y, H[K]), Nn, FVector2D(X, Y) / 300.f, FLinearColor(0, Moss, Damp, Lit).ToFColor(false));
            }
        for (int32 I = 0; I < Nx; ++I) for (int32 J = 0; J < Ny; ++J) { const int32 A = I * (Ny + 1) + J, B = A + Ny + 1; L.Quad(A, A + 1, B + 1, B); }
        RiverParts.Add(Emit(TEXT("FeyRiverLand"), L, Mat(TEXT("/Game/Art/Feywild/M_FeyGround.M_FeyGround")), false, true));
    }
    // The root tunnel: a long hollow of root and bark the water runs through, closed at its dark end, ragged at its mouth.
    {
        FFeySurf T; constexpr int32 Around = 30; constexpr float Ds = 60.f, Thick = 34.f;
        const int32 Rows = FMath::CeilToInt((RiverMouth - RiverStart) / Ds);
        TArray<int32> Inner, Outer;
        for (int32 I = 0; I <= Rows; ++I)
        {
            const float S = FMath::Lerp(RiverStart, RiverMouth, float(I) / Rows);
            const FVector C = RiverAt(S) + FVector(0, 0, 40.f), Side = RiverSide(S), Fwd = FVector::CrossProduct(FVector::UpVector, Side).GetSafeNormal() * -1.f;
            for (int32 J = 0; J <= Around; ++J)
            {
                const float Th = 2.f * PI * J / Around, Tw = J == Around ? 0.f : Th;   // (the seam shares its noise)
                float Rr = 430.f + 40.f * Noise(S, 0, 600, 70) + 42.f * Noise(Tw * 170.f, S, 380, 71) + 14.f * Noise(Tw * 420.f, S * 2.5f, 150, 72);
                const FVector Dir = Side * FMath::Cos(Th) + FVector::UpVector * FMath::Sin(Th);
                const float Ragged = I == Rows ? 150.f * (.5f + .5f * Noise(Tw * 300.f, 0, 260, 73)) : 0.f;
                const FVector P = C + Dir * Rr + Fwd * Ragged;
                const FVector2D Uv(Th * Rr / 160.f, S / 160.f);
                Inner.Add(T.Vert(P, -Dir, Uv, FLinearColor(.25f, TunnelLight(S), 1, 1).ToFColor(false)));
                Outer.Add(T.Vert(P + Dir * Thick, Dir, Uv, FLinearColor(.85f, 1, 1, 1).ToFColor(false)));
            }
        }
        for (int32 I = 0; I < Rows; ++I)
            for (int32 J = 0; J < Around; ++J)
            {
                const int32 A = I * (Around + 1) + J, B = A + 1, Cc = A + Around + 2, D = A + Around + 1;
                Face(T, Inner[A], Inner[B], Inner[Cc], Inner[D]); Face(T, Outer[A], Outer[B], Outer[Cc], Outer[D]);
            }
        // The lip at the mouth, inner to outer.
        for (int32 J = 0; J < Around; ++J)
        {
            const int32 A = Rows * (Around + 1) + J;
            const FVector Fwd = FVector::CrossProduct(FVector::UpVector, RiverSide(RiverMouth)).GetSafeNormal() * -1.f;
            // (copies: Vert grows the arrays these come from)
            const FVector P[4] = {T.V[Inner[A]], T.V[Inner[A + 1]], T.V[Outer[A + 1]], T.V[Outer[A]]};
            const FVector2D Uv[4] = {T.UV[Inner[A]], T.UV[Inner[A + 1]], T.UV[Outer[A + 1]], T.UV[Outer[A]]};
            int32 Q[4]; for (int32 K = 0; K < 4; ++K) Q[K] = T.Vert(P[K], Fwd, Uv[K], FLinearColor(.5f, 1, 1, 1).ToFColor(false));
            Face(T, Q[0], Q[1], Q[2], Q[3]);
        }
        // The dark end: shut.
        {
            const FVector Fwd = FVector::CrossProduct(FVector::UpVector, RiverSide(RiverStart)).GetSafeNormal() * -1.f;
            const FColor Dark = FLinearColor(.25f, .05f, 1, 1).ToFColor(false);
            const int32 Mid = T.Vert(RiverAt(RiverStart) + FVector(0, 0, 40.f), Fwd, FVector2D::ZeroVector, Dark);
            for (int32 J = 0; J < Around; ++J)
            {
                const FVector Pa = T.V[Inner[J]], Pb = T.V[Inner[J + 1]]; const FVector2D Ua = T.UV[Inner[J]], Ub = T.UV[Inner[J + 1]];
                const int32 A = T.Vert(Pa, Fwd, Ua, Dark), B = T.Vert(Pb, Fwd, Ub, Dark);
                if (FVector::DotProduct(FVector::CrossProduct(T.V[A] - T.V[Mid], T.V[B] - T.V[Mid]), Fwd) > 0.f) T.T.Append({Mid, B, A}); else T.T.Append({Mid, A, B});
            }
        }
        RiverParts.Add(Emit(TEXT("FeyRiverTunnel"), T, Mat(TEXT("/Game/Art/Feywild/M_FeyRoot.M_FeyRoot")), false, true));
    }
    // The water: a ribbon along the middle, its foam running downstream (M_FeyRiver), dark under the tunnel.
    {
        FFeySurf W; constexpr float Ds = 60.f; constexpr int32 Across = 8;
        const int32 Rows = FMath::CeilToInt((RiverEnd - RiverStart) / Ds);
        for (int32 I = 0; I <= Rows; ++I)
        {
            const float S = FMath::Lerp(RiverStart, RiverEnd, float(I) / Rows), Half = RiverHalf(S) + 110.f;
            const FVector C = RiverAt(S), Side = RiverSide(S);
            const float Light = TunnelLight(S);
            for (int32 J = 0; J <= Across; ++J)
            {
                const float D = FMath::Lerp(-Half, Half, float(J) / Across), Edge = FMath::Clamp(FMath::Abs(D) / (Half - 110.f), 0.f, 1.f);
                const float Churn = FMath::Clamp(.2f + .45f * Smooth(6600.f, 8200.f, S) + .3f * FMath::Max(0.f, Noise(S, D, 900, 64)) + .25f * Smooth(.7f, 1.f, Edge), 0.f, 1.f);
                W.Vert(C + Side * D, FVector::UpVector, FVector2D(S, D) / 200.f, FLinearColor(Edge, Churn, Light, 1).ToFColor(false));
            }
        }
        for (int32 I = 0; I < Rows; ++I) for (int32 J = 0; J < Across; ++J) { const int32 A = I * (Across + 1) + J, B = A + Across + 1; Face(W, A, A + 1, B + 1, B); }
        UMaterialInterface* RiverMat = Mat(TEXT("/Game/Art/Feywild/M_FeyRiver.M_FeyRiver"));
        auto* Mesh = Emit(TEXT("FeyRiverWater"), W, RiverMat, false, false);
        if (RiverMat && ProbeTarget) { auto* Mid = UMaterialInstanceDynamic::Create(RiverMat, this); Mid->SetTextureParameterValue(TEXT("Reflection"), ProbeTarget); Mesh->SetMaterial(0, Mid); }
        RiverParts.Add(Mesh);
    }
    // The banks: bare rock and root at the tunnel's mouth, growing to moss, fungi, flowers and old oaks downstream.
    {
        FRandomStream R(919);
        const FString Dir = TEXT("/Game/Art/Feywild/");
        auto Path = [&Dir](const TCHAR* Name) { return Dir + Name + TEXT(".") + Name; };
        auto Bank = [](float S, float D) { const FVector C = RiverAt(S), Side = RiverSide(S); const float X = C.X + Side.X * D, Y = C.Y + Side.Y * D; return FVector(X, Y, RiverGround(X, Y)); };
        auto Put = [&](UHierarchicalInstancedStaticMeshComponent* H, const FVector& P, float Scale, float Sink, float Tilt)
        {
            H->AddInstance(FTransform(FRotator(R.FRandRange(-Tilt, Tilt), R.FRandRange(0.f, 360.f), R.FRandRange(-Tilt, Tilt)), P - FVector(0, 0, Sink), FVector(Scale))); ++Planted;
        };
        auto Side = [&R]() { return R.FRand() < .5f ? -1.f : 1.f; };
        // (nothing grows against or inside the tunnel)
        auto Clear = [](float S, float D) { return S > RiverMouth + 80.f || FMath::Abs(D) > 520.f; };
        auto Scatter = [&](UHierarchicalInstancedStaticMeshComponent* H, int32 Count, float S0, float S1, float In, float Out, float Sc0, float Sc1, float Sink, float Tilt, TFunctionRef<float(float)> Chance)
        {
            for (int32 I = 0; I < Count; ++I)
            {
                const float S = R.FRandRange(S0, S1), D = Side() * (RiverHalf(S) + R.FRandRange(In, Out));
                if (!Clear(S, D) || R.FRand() > Chance(S)) continue;
                Put(H, Bank(S, D), R.FRandRange(Sc0, Sc1), Sink, Tilt);
            }
        };
        auto Any = [](float) { return 1.f; };
        auto Greening = [&Lush](float S) { return .12f + .88f * Lush(S); };
        auto* Oak0 = Kit(*Path(TEXT("SM_FeyOak0")), 0, 0, true), *Oak1 = Kit(*Path(TEXT("SM_FeyOak1")), 0, 0, true);
        Put(Oak0, Bank(RiverMouth - 300.f, RiverHalf(RiverMouth) + 1300.f), 1.6f, 20.f, 0.f);   // (back from the mouth: its low limbs stay off the river)
        Put(Oak1, Bank(RiverMouth + 260.f, -(RiverHalf(RiverMouth) + 1150.f)), 1.45f, 20.f, 0.f);
        for (const float S : {300.f, 950.f, 1550.f}) Put(Oak1, Bank(S, Side() * R.FRandRange(1100.f, 1500.f)), R.FRandRange(1.3f, 1.6f), 16.f, 0.f);
        for (int32 K = 0; K < 15; ++K)
        {
            const float S = RiverMouth + 900.f + K * 640.f + R.FRandRange(-180.f, 180.f), D = (K % 2 ? 1.f : -1.f) * (RiverHalf(S) + R.FRandRange(650.f, 1600.f));
            Put(K % 3 ? Oak0 : Oak1, Bank(S, D), R.FRandRange(1.f, 1.45f), 16.f, 0.f);
        }
        for (int32 K = 0; K < 16; ++K)
        {
            const float S = 600.f + K * 640.f + R.FRandRange(-200.f, 200.f), D = (K % 2 ? -1.f : 1.f) * (RiverHalf(S) + R.FRandRange(1700.f, 2700.f));
            Put(K % 2 ? Oak0 : Oak1, Bank(S, D), R.FRandRange(1.1f, 1.5f), 24.f, 0.f);
        }
        auto* Amber = Kit(*Path(TEXT("SM_FeyCapAmber")), 0, 0, true);
        for (int32 K = 0; K < 7; ++K) { const float S = 3200.f + K * 1100.f + R.FRandRange(-250.f, 250.f); Put(Amber, Bank(S, Side() * (RiverHalf(S) + R.FRandRange(520.f, 1700.f))), R.FRandRange(1.2f, 2.4f), 6.f, 2.f); }
        Scatter(Kit(*Path(TEXT("SM_FeyCapBlue")), 7000, 11000, false), 560, RiverMouth - 150.f, RiverEnd - 300.f, 10.f, 380.f, .7f, 2.f, 1.f, 5.f, Any);
        Scatter(Kit(*Path(TEXT("SM_FeyCapViolet")), 7000, 11000, false), 280, RiverMouth + 200.f, RiverEnd - 300.f, 40.f, 700.f, .8f, 1.9f, 1.f, 5.f, Greening);
        for (int32 I = 0; I < 4; ++I)
        {
            const FString N = FString::Printf(TEXT("/Game/Art/ChultNature/SM_ChultFern%d.SM_ChultFern%d"), I, I);
            Scatter(Kit(*N, 6000, 9000, false), 420, RiverMouth - 600.f, RiverEnd - 200.f, 120.f, 2700.f, 1.f, 2.2f, 2.f, 8.f, Greening);
        }
        Scatter(Kit(*Path(TEXT("SM_FeyBroadLeaf")), 6000, 9000, false), 240, RiverMouth, RiverEnd - 200.f, 90.f, 1200.f, .8f, 1.7f, 2.f, 6.f, Lush);
        Scatter(Kit(*Path(TEXT("SM_FeyCrimsonLeaf")), 6000, 9000, false), 180, RiverMouth + 400.f, RiverEnd - 200.f, 90.f, 1200.f, .8f, 1.6f, 2.f, 6.f, Lush);
        Scatter(Kit(*Path(TEXT("SM_FeyFoxglove")), 7000, 11000, false), 200, RiverMouth + 600.f, RiverEnd - 200.f, 150.f, 1400.f, .9f, 1.5f, 2.f, 4.f, Lush);
        Scatter(Kit(*Path(TEXT("SM_FeyCelandine0")), 3200, 5000, false), 700, RiverMouth, RiverEnd - 200.f, 80.f, 1300.f, 1.6f, 2.8f, 1.f, 4.f, Lush);
        Scatter(Kit(*Path(TEXT("SM_FeyCelandine1")), 3200, 5000, false), 700, RiverMouth, RiverEnd - 200.f, 80.f, 1300.f, 1.6f, 2.8f, 1.f, 4.f, Lush);
        Scatter(Kit(*Path(TEXT("SM_FeyGrass0")), 3600, 5400, false), 5500, RiverMouth - 400.f, RiverEnd, 70.f, 2000.f, 2.6f, 4.4f, 1.f, 7.f, Greening);
        Scatter(Kit(*Path(TEXT("SM_FeyGrass1")), 3600, 5400, false), 3800, RiverMouth - 400.f, RiverEnd, 90.f, 2000.f, 2.f, 3.6f, 1.f, 7.f, Greening);
        UHierarchicalInstancedStaticMeshComponent* Rock[6];
        for (int32 I = 0; I < 6; ++I) { const FString N = FString::Printf(TEXT("/Game/Art/ChultNature/SM_ChultRock%d.SM_ChultRock%d"), I, I); Rock[I] = Kit(*N, 14000, 22000, true); }
        for (int32 I = 0; I < 130; ++I)
        {
            const float S = R.FRandRange(RiverMouth - 300.f, RiverEnd - 400.f), D = Side() * (RiverHalf(S) + R.FRandRange(-110.f, 200.f));
            if (!Clear(S, D) || R.FRand() > .3f + .7f * (1.f - Lush(S))) continue;
            if (S > RiverMouth - 100.f && S < RiverMouth + 1300.f && FMath::Abs(D) < RiverHalf(S) + 120.f) continue;   // (the cutscene's view of the mouth)
            Put(Rock[R.RandRange(0, 5)], Bank(S, D), R.FRandRange(.6f, 1.9f), 16.f, 12.f);
        }
        // Pads in the wide, slow bend.
        auto* Pads = Kit(*Path(TEXT("SM_FeyLilyPad")), 12000, 18000, false), *Lily = Kit(*Path(TEXT("SM_FeyLily")), 9000, 14000, false);
        for (int32 I = 0; I < 90; ++I)
        {
            const float S = R.FRandRange(4200.f, 5800.f), D = Side() * (RiverHalf(S) - R.FRandRange(30.f, 170.f));
            const FVector C = RiverAt(S) + RiverSide(S) * D + FVector(0, 0, .6f);
            const float Sc = R.FRandRange(.5f, 1.3f);
            Pads->AddInstance(FTransform(FRotator(0, R.FRandRange(0.f, 360.f), 0), C, FVector(Sc, Sc, 1))); ++Planted;
            if (R.FRand() < .15f) { Lily->AddInstance(FTransform(FRotator(0, R.FRandRange(0.f, 360.f), 0), C + FVector(0, 0, 2.4f), FVector(R.FRandRange(.9f, 1.4f)))); ++Planted; }
        }
        // Low mist on the water.
        RiverVeil0 = Veils.Num();
        for (int32 I = 0; I < 26; ++I)
        {
            FVeil V; const float S = FMath::Lerp(RiverMouth - 200.f, RiverEnd - 600.f, (I + R.FRand()) / 26.f);
            V.P = RiverAt(S) + RiverSide(S) * R.FRandRange(-300.f, 300.f) + FVector(0, 0, R.FRandRange(40.f, 160.f));
            V.Size = R.FRandRange(320.f, 760.f); V.Alpha = R.FRandRange(.04f, .1f); V.Seed = R.FRand(); V.Drift = R.FRandRange(-10.f, 10.f);
            Veils.Add(V);
        }
    }
    for (int32 I = KitsBefore; I < Kits.Num(); ++I) { Kits[I]->BuildTreeIfOutdated(false, true); RiverParts.Add(Kits[I]); }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEYWILD_RIVER_BUILT seconds=%.2f triangles=%d planted=%d missing=%d"), FPlatformTime::Seconds() - Started, Triangles - TrianglesBefore, Planted - PlantedBefore, Missing);
}

void AFeywildSetting::ShowRiver(bool bShow)
{
    for (USceneComponent* Part : RiverParts) if (Part) Part->SetVisibility(bShow, true);
    if (!bShow && RiverVeil0 >= 0 && Veils.Num() > RiverVeil0) Veils.SetNum(RiverVeil0);
}

void AFeywildSetting::SetShade(float K)
{
    if (FillLight) FillLight->SetIntensity(FillBase * K);
    if (Sky) Sky->SetIntensity(SkyBase * K);
    AirShade = K;
}

void AFeywildSetting::Splash(const FVector& World, float Hard)
{
    const FVector L = FeyLocal(World), At(L.X, L.Y, WaterZ + 1.2f);
    auto Ring = [this](const FVector& P, float Size, float Alpha, float Life) { if (Ripples.Num() < 70) { FRing N; N.P = P; N.Size = Size; N.Alpha = Alpha; N.Life = Life; Ripples.Add(N); } };
    Ring(At, 160.f + 300.f * Hard, .95f, 2.2f); Ring(At, 90.f + 120.f * Hard, .75f, 1.3f);
    const int32 Count = int32(10 + 40 * Hard); const float Speed = 200.f + 340.f * Hard;
    for (int32 I = 0; I < Count && Drops.Num() < 90; ++I)
    {
        FDrop D; const float A = FMath::FRandRange(0.f, 2 * PI), Out = FMath::FRandRange(.15f, .7f) * Speed;
        D.P = FeyWorld(At); D.V = FVector(FMath::Cos(A) * Out, FMath::Sin(A) * Out, FMath::FRandRange(.55f, 1.f) * Speed); D.Life = FMath::FRandRange(.35f, .7f); D.Size = FMath::FRandRange(2.f, 4.2f);
        Drops.Add(D);
    }
}

void AFeywildSetting::Tick(float Dt)
{
    Super::Tick(Dt);
    Clock += Dt;
    // The rough map (stage 3, FeyMap.h): once he is out of the river, its districts load and drop as he goes.
    if (!bMapStarted)
        if (auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
            if (!Chuck->IsInFeyRiver() && InFeywild(Chuck->GetActorLocation())) { bMapStarted = true; AFeyMap::Ensure(GetWorld()); }
    TickWater(FMath::Min(Dt, .05f));
    TickAir(FMath::Min(Dt, .05f));
    static const bool bPerf = FParse::Param(FCommandLine::Get(), TEXT("ChuckFeyPerf"));
    if (bPerf)
    {
        PerfMs += Dt * 1000.; PerfGpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()); ++PerfFrames;
        if (Clock - PerfAt >= 2.f)
        {
            const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FEYWILD_PERF eye=%s frames=%d mean_ms=%.2f gpu_ms=%.2f"), Cam ? *FeyLocal(Cam->GetCameraLocation()).ToCompactString() : TEXT("-"), PerfFrames, PerfMs / PerfFrames, PerfGpu / PerfFrames);
            PerfMs = PerfGpu = 0; PerfFrames = 0; PerfAt = Clock;
        }
    }
}

// Ripples and drops: where he wades, and always a few at the falls' foot.
void AFeywildSetting::TickWater(float Dt)
{
    auto Ring = [this](const FVector& P, float Size, float Alpha, float Life) { if (Ripples.Num() < 70) { FRing N; N.P = P; N.Size = Size; N.Alpha = Alpha; N.Life = Life; Ripples.Add(N); } };
    auto Burst = [this](const FVector& P, int32 Count, float Speed)
    {
        for (int32 I = 0; I < Count && Drops.Num() < 90; ++I)
        {
            FDrop D; const float A = FMath::FRandRange(0.f, 2 * PI), Out = FMath::FRandRange(.15f, .7f) * Speed;
            D.P = P; D.V = FVector(FMath::Cos(A) * Out, FMath::Sin(A) * Out, FMath::FRandRange(.55f, 1.f) * Speed); D.Life = FMath::FRandRange(.32f, .6f); D.Size = FMath::FRandRange(1.8f, 3.6f);
            Drops.Add(D);
        }
    };
    if (auto* Chuck = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        const FVector L = FeyLocal(Chuck->GetActorLocation()); const float Feet = L.Z - Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const bool bWading = InFeywild(Chuck->GetActorLocation()) && Feet < WaterZ + 12.f && PondE(L.X, L.Y) < 1.f;
        const float Speed = Chuck->GetVelocity().Size2D(); const FVector At(L.X, L.Y, WaterZ + 1.2f);
        NextRing -= Dt;
        if (bWading && !bWasWading)
        {
            const float Hard = FMath::Clamp(-Chuck->GetVelocity().Z / 500.f, .3f, 1.f);
            Ring(At, 180.f + 160.f * Hard, .9f, 1.8f); Ring(At, 90.f, .7f, 1.1f); Burst(FeyWorld(At), int32(10 + 26 * Hard), 220.f + 240.f * Hard);
            if (SplashSound) UGameplayStatics::PlaySoundAtLocation(this, SplashSound, Chuck->GetActorLocation(), .35f + .4f * Hard, FMath::FRandRange(.72f, .88f));
        }
        else if (bWading && NextRing <= 0.f)
        {
            const bool bMoving = Speed > 30.f;
            NextRing = bMoving ? FMath::Lerp(.34f, .17f, FMath::Clamp(Speed / 500.f, 0.f, 1.f)) : 1.5f;
            Ring(At + (bMoving ? Chuck->GetVelocity().GetSafeNormal2D() * 16.f : FVector::ZeroVector), bMoving ? 130.f : 95.f, bMoving ? .7f : .4f, 1.5f);
            if (bMoving) Burst(FeyWorld(At), 5, 110.f + Speed * .25f);
        }
        bWasWading = bWading;
    }
    NextFallRing -= Dt;
    if (NextFallRing <= 0.f && bSlice)
    {
        NextFallRing = FMath::FRandRange(.14f, .3f);
        Ring(FVector(LedgeX - FMath::FRandRange(250.f, 520.f), FallY + FMath::FRandRange(-FallHalf, FallHalf), WaterZ + 1.2f), FMath::FRandRange(110.f, 240.f), .55f, 1.6f);
    }
    TArray<FTransform> Xf; Xf.Reserve(Ripples.Num());
    for (int32 I = Ripples.Num() - 1; I >= 0; --I) { Ripples[I].Age += Dt; if (Ripples[I].Age >= Ripples[I].Life) Ripples.RemoveAtSwap(I); }
    for (const FRing& R : Ripples) Xf.Add(FTransform(FQuat::Identity, FeyWorld(R.P), FVector(R.Size / 100.f)));
    if (Rings->GetInstanceCount() != Xf.Num()) { Rings->ClearInstances(); if (Xf.Num()) Rings->AddInstances(Xf, false, true); }
    else if (Xf.Num()) Rings->BatchUpdateInstancesTransforms(0, Xf, true, true);
    for (int32 I = 0; I < Ripples.Num(); ++I) { Rings->SetCustomDataValue(I, 0, Ripples[I].Age / Ripples[I].Life, false); Rings->SetCustomDataValue(I, 1, Ripples[I].Alpha, I == Ripples.Num() - 1); }
    for (int32 I = Drops.Num() - 1; I >= 0; --I)
    {
        FDrop& D = Drops[I]; D.Age += Dt; D.V.Z -= 980.f * Dt; D.P += D.V * Dt;
        if (D.Age >= D.Life || FeyLocal(D.P).Z < WaterZ) Drops.RemoveAtSwap(I);
    }
}

void AFeywildSetting::TickAir(float Dt)
{
    APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (!Cam || !InFeywild(Cam->GetCameraLocation())) { if (Lights->GetInstanceCount()) Lights->ClearInstances(); if (Mist->GetInstanceCount()) Mist->ClearInstances(); return; }
    const FVector Eye = Cam->GetCameraLocation(), LEye = FeyLocal(Eye);
    auto Facing = [&Eye](const FVector& World, float Size, float Spin = 0.f)
    {
        FVector Z = Eye - World; if (!Z.Normalize()) Z = FVector::UpVector;
        const FVector X0 = FVector::CrossProduct(FMath::Abs(Z.Z) > .98f ? FVector::ForwardVector : FVector::UpVector, Z).GetSafeNormal(), Y0 = FVector::CrossProduct(Z, X0);
        const FVector X = X0 * FMath::Cos(Spin) + Y0 * FMath::Sin(Spin), Y = FVector::CrossProduct(Z, X);
        return FTransform(FMatrix(X, Y, Z, FVector::ZeroVector).ToQuat(), World, FVector(Size / 100.f));
    };
    // Fireflies wandering near the ground, and gold dust drifting in the air, only round the eye.
    constexpr int32 Flies = 170, Dust = 130; int32 F = 0, D = 0;
    for (const FSpark& S : Sparks) (S.bFly ? F : D) += 1;
    auto New = [&](bool bFly, bool bAnywhere)
    {
        FSpark S; S.bFly = bFly; const float Rad = bFly ? 2100.f : 1300.f, A = FMath::FRandRange(0.f, 2 * PI), Rr = Rad * FMath::Sqrt(FMath::FRand());
        S.P = FVector(LEye.X + Rr * FMath::Cos(A), LEye.Y + Rr * FMath::Sin(A), 0);
        float Land = FMath::Max(Ground(S.P.X, S.P.Y), WaterZ);
        if (InRiverBox(LEye)) { float Rs, Rd; RiverNearest(S.P.X, S.P.Y, Rs, Rd); Land = FMath::Max(RiverGround(S.P.X, S.P.Y), static_cast<float>(RiverAt(Rs).Z)); }   // (on the river's set)
        else { float Z; if (FeyMap::GroundAt(S.P.X, S.P.Y, Z)) Land = FMath::Max(Z, 0.f); }                                                 // (on the rough map's land)
        S.P.Z = Land + (bFly ? FMath::FRandRange(20.f, 260.f) : FMath::FRandRange(30.f, 520.f));
        S.V = FVector(FMath::FRandRange(-12.f, 12.f), FMath::FRandRange(-12.f, 12.f), bFly ? 0.f : FMath::FRandRange(-5.f, 7.f));
        S.Life = bFly ? FMath::FRandRange(7.f, 16.f) : FMath::FRandRange(6.f, 13.f); S.Age = bAnywhere ? FMath::FRandRange(0.f, S.Life * .7f) : 0.f;
        S.Size = bFly ? FMath::FRandRange(7.f, 13.f) : FMath::FRandRange(1.6f, 3.4f); S.Hue = bFly && FMath::FRand() < .28f ? 1.f : 0.f; S.Phase = FMath::FRandRange(0.f, 6.28f);
        Sparks.Add(S);
    };
    const bool bFirst = Sparks.IsEmpty();
    while (F < Flies) { New(true, bFirst); ++F; }
    while (D < Dust) { New(false, bFirst); ++D; }
    TArray<FTransform> Xf; Xf.Reserve(Sparks.Num()); TArray<float> Data; Data.Reserve(Sparks.Num() * 2);
    for (int32 I = Sparks.Num() - 1; I >= 0; --I)
    {
        FSpark& S = Sparks[I]; S.Age += Dt;
        const FVector Wander = S.bFly ? FVector(FMath::Sin(Clock * .9f + S.Phase) * 32.f, FMath::Cos(Clock * .7f + S.Phase * 1.7f) * 32.f, FMath::Sin(Clock * 1.3f + S.Phase * 2.3f) * 14.f)
                                      : FVector(FMath::Sin(Clock * .3f + S.Phase) * 8.f, FMath::Cos(Clock * .27f + S.Phase) * 8.f, 0.f);
        S.P += (S.V + Wander) * Dt;
        if (S.Age > S.Life || FVector::DistSquared2D(S.P, LEye) > FMath::Square(S.bFly ? 2500.f : 1600.f)) { Sparks.RemoveAtSwap(I); continue; }
        const float Fade = FMath::Clamp(S.Age / 1.2f, 0.f, 1.f) * FMath::Clamp((S.Life - S.Age) / 1.5f, 0.f, 1.f);
        const float Blink = S.bFly ? FMath::Pow(.5f + .5f * FMath::Sin(Clock * 1.7f + S.Phase * 3.f), 3.f) : .6f + .4f * FMath::Sin(Clock * 2.3f + S.Phase);
        // (one passing right by the eye would be a blinding ball)
        Xf.Add(Facing(FeyWorld(S.P), S.Size)); Data.Add((S.bFly ? 9.f : 2.2f) * Fade * Blink * Smooth(70.f, 340.f, FVector::Dist(S.P, LEye)) * Smooth(.15f, .7f, AirShade)); Data.Add(S.Hue);
    }
    for (const FDrop& Dp : Drops) { Xf.Add(Facing(Dp.P, Dp.Size)); Data.Add(1.5f * (1.f - Dp.Age / Dp.Life)); Data.Add(2.f); }
    if (Lights->GetInstanceCount() != Xf.Num()) { Lights->ClearInstances(); Lights->AddInstances(Xf, false, true); }
    else Lights->BatchUpdateInstancesTransforms(0, Xf, true, true);
    for (int32 I = 0; I < Xf.Num(); ++I) { Lights->SetCustomDataValue(I, 0, Data[I * 2], false); Lights->SetCustomDataValue(I, 1, Data[I * 2 + 1], I == Xf.Num() - 1); }
    // The mist's veils, turned to the eye, drifting a little.
    TArray<FTransform> Mf; Mf.Reserve(Veils.Num() + ExtraVeils.Num());
    for (const FVeil& V : Veils) Mf.Add(Facing(FeyWorld(V.P + FVector(FMath::Sin(Clock * .05f + V.Seed * 6.f) * V.Drift * 8.f, FMath::Cos(Clock * .04f + V.Seed * 9.f) * V.Drift * 8.f, 0)), V.Size));
    for (int32 I = 0; I < ExtraVeils.Num(); ++I)   // (a district's own, the Deepwood's: only those near the eye)
    {
        const FVector4& E = ExtraVeils[I]; const float Fade = FVector::Dist2D(FVector(E.X, E.Y, 0), LEye) < 9000.f ? 1.f : 0.f;
        Mf.Add(Facing(FeyWorld(FVector(E.X + FMath::Sin(Clock * .05f + I) * 60.f, E.Y + FMath::Cos(Clock * .04f + I * 1.3f) * 60.f, E.Z)), float(E.W) * Fade + 1.f));
    }
    if (Mist->GetInstanceCount() != Mf.Num() || bVeilsDirty)
    {
        bVeilsDirty = false;
        Mist->ClearInstances(); Mist->AddInstances(Mf, false, true);
        for (int32 I = 0; I < Mf.Num(); ++I)
        {
            const bool bOwn = I < Veils.Num();
            Mist->SetCustomDataValue(I, 0, bOwn ? Veils[I].Alpha : .15f, false); Mist->SetCustomDataValue(I, 1, bOwn ? Veils[I].Seed : float(I % 17) / 17.f, I == Mf.Num() - 1);
        }
    }
    else Mist->BatchUpdateInstancesTransforms(0, Mf, true, true);
}
