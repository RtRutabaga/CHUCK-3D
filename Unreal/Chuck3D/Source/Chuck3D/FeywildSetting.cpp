#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "FallCutscene.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Info.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// The Feywild's look slice (Claude, 2026-11-18; FeywildSetting.h, docs/regions/FEYWILD.md). The kit is Tools/build_feywild_kit.py
// and the materials Tools/create_feywild_assets.py. Everything here is a first pass to be judged against References/Feywild/.
using namespace Fey;

namespace
{
    float Noise(float X, float Y, float Scale, float Seed = 0) { return FMath::PerlinNoise2D(FVector2D(X / Scale + Seed * 3.71f + .31f, Y / Scale - Seed * 1.93f + .77f)); }
    float Smooth(float A, float B, float X) { const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f); return T * T * (3 - 2 * T); }
    constexpr float Stand = 34.65f;
    const FRotator SunRot(-10.f, 214.f, 0.f);   // the way its light travels: the sun itself is low ahead, a little to the right

    // How far out of the pond's edge (1 on the shore), the edge wandering.
    float PondE(float X, float Y)
    {
        const float A = FMath::Atan2((Y - PondY) / PondRY, (X - PondX) / PondRX);
        return FVector2D((X - PondX) / PondRX, (Y - PondY) / PondRY).Size() / (1.f + .09f * FMath::Sin(3.f * A + 1.f) + .05f * FMath::Sin(7.f * A + 2.f));
    }
    float PathK(float X, float Y) { return Smooth(210.f, 80.f, FMath::Abs(Y - PathY(X))) * Smooth(-1500.f, -900.f, X) * Smooth(5050.f, 4700.f, X); }
}

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
    H = FMath::Lerp(H, WaterZ - 34.f, Smooth(1.02f, .84f, E));
    // The path pressed into the ground.
    const float Pk = PathK(X, Y);
    if (Pk > 0) H = FMath::Lerp(H, 16.f + 12.f * Noise(X, 0, 700, 9) + 3.f * Noise(X, Y, 60, 10), Pk);
    // The two rock steps the falls come over, a channel cut through each.
    const float Edge = 70.f * Noise(0, Y, 500, 11);
    const float Cut = Smooth(FallHalf + 150.f, FallHalf + 20.f, FMath::Abs(Y - FallY));
    const float Step1 = Smooth(LedgeX - 90.f, LedgeX + 70.f, X + Edge) * (LedgeTop + 60.f * (1 - Cut) * Smooth(0.f, 500.f, FMath::Abs(Y - FallY)) - 34.f * Cut);
    const float Step2 = Smooth(Ledge2X - 90.f, Ledge2X + 70.f, X - Edge) * ((Ledge2Top - LedgeTop) + 80.f * (1 - Cut) - 34.f * Cut);
    const float Far = Smooth(7600.f, 9300.f, X) * 700.f + (X > LedgeX ? 40.f * Noise(X, Y, 380, 12) * (1 - Cut) : 0.f);
    const float Raised = Step1 + Step2 + Far;
    return Raised > 1.f ? FMath::Max(H, Raised) : H;
}

FVector FeywildOrigin() { return AFallCutscene::LandingSpot() + FVector(0, -3200000, 0); }
bool InFeywild(const FVector& World)
{
    const FVector L = FeyLocal(World);
    return L.X > MinX - 2000 && L.X < MaxX + 4000 && FMath::Abs(L.Y) < HalfY + 2000 && L.Z > -1500 && L.Z < 9000;
}
FVector FeywildSpawn() { const float X = -300.f, Y = PathY(X); return FeyWorld(FVector(X, Y, Ground(X, Y) + Stand + 2.f)); }
float FeywildSpawnYaw() { return 0.f; }

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
        if (A == Chuck || A->GetAttachParentActor() == Chuck || A->IsA<AController>() || A->IsA<APlayerCameraManager>() || A->IsA<AHUD>()) continue;
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
    BuildEnvironment();
    BuildLand();
    BuildPlanting();
    for (UHierarchicalInstancedStaticMeshComponent* H : Kits) H->BuildTreeIfOutdated(false, true);
    // Low veils of mist on the pond and at the falls' feet.
    FRandomStream R(31);
    for (int32 I = 0; I < 34; ++I)
    {
        FVeil W; const float A = R.FRandRange(0.f, 2 * PI), Rr = FMath::Sqrt(R.FRand());
        W.P = I < 22 ? FVector(PondX + PondRX * .95f * Rr * FMath::Cos(A), PondY + PondRY * .95f * Rr * FMath::Sin(A), WaterZ + R.FRandRange(30.f, 110.f))
                     : FVector(LedgeX - R.FRandRange(60.f, 600.f), FallY + R.FRandRange(-650.f, 650.f), WaterZ + R.FRandRange(50.f, 240.f));
        W.Size = R.FRandRange(420.f, 900.f); W.Alpha = R.FRandRange(.035f, .09f); W.Seed = R.FRand(); W.Drift = R.FRandRange(-14.f, 14.f);
        Veils.Add(W);
    }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEYWILD_BUILT seconds=%.2f"), FPlatformTime::Seconds() - Started);
}

void AFeywildSetting::BuildEnvironment()
{
    const FVector G0 = FeywildOrigin();
    auto Attach = [this, G0](USceneComponent* C, FVector At = FVector::ZeroVector, FRotator Rot = FRotator::ZeroRotator)
    { C->SetupAttachment(RootComponent); C->SetWorldLocationAndRotation(G0 + At, Rot); C->RegisterComponent(); };
    // The sky: a dome of M_FeySky, blue warming to peach and gold, never purple.
    {
        FFeySurf Dome; constexpr int32 Rings = 18, Segs = 48; constexpr float Rad = 4000000.f;
        TArray<TArray<int32>> Grid;
        for (int32 I = 0; I <= Rings; ++I)
        {
            TArray<int32>& Row = Grid.AddDefaulted_GetRef();
            for (int32 J = 0; J <= Segs; ++J)
            {
                const float Th = FMath::Lerp(-.12f, HALF_PI, float(I) / Rings), Ph = 2 * PI * J / Segs;
                const FVector D(FMath::Cos(Ph) * FMath::Cos(Th), FMath::Sin(Ph) * FMath::Cos(Th), FMath::Sin(Th));
                Row.Add(Dome.Vert(D * Rad, -D, FVector2D::ZeroVector, FColor::White));
            }
        }
        for (int32 I = 0; I < Rings; ++I) for (int32 J = 0; J < Segs; ++J) Dome.Quad(Grid[I][J], Grid[I][J + 1], Grid[I + 1][J + 1], Grid[I + 1][J]);
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
    Sun->SetIntensity(5.5f); Sun->SetLightColor(FLinearColor(1.f, .66f, .36f));
    Sun->DynamicShadowDistanceMovableLight = 9000; Sun->DynamicShadowCascades = 4; Sun->SetVolumetricScatteringIntensity(2.2f);
    Sun->SetEnableLightShaftBloom(true); Sun->SetBloomScale(.25f); Sun->SetBloomThreshold(.6f); Sun->SetBloomTint(FColor(255, 196, 130));
    Attach(Sun, FVector::ZeroVector, SunRot);
    auto* SkyLight = NewObject<USkyLightComponent>(this);
    SkyLight->SetMobility(EComponentMobility::Movable); SkyLight->bRealTimeCapture = false; SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene; SkyLight->SkyDistanceThreshold = 100000.f;
    SkyLight->SetIntensity(1.7f); SkyLight->SetLightColor(FLinearColor(.72f, .86f, 1.f));
    SkyLight->SetLowerHemisphereColor(FLinearColor(.05f, .09f, .1f)); SkyLight->bLowerHemisphereIsBlack = true;
    Attach(SkyLight);
    FTimerHandle Capture; GetWorldTimerManager().SetTimer(Capture, [SkyLight]() { SkyLight->RecaptureSky(); UE_LOG(LogTemp, Display, TEXT("CHUCK_FEYWILD_SKY_CAPTURED")); }, .6f, false);
    auto* Fill = NewObject<UDirectionalLightComponent>(this);
    Fill->SetMobility(EComponentMobility::Movable); Fill->SetAtmosphereSunLight(false); Fill->SetForwardShadingPriority(0);
    Fill->SetIntensity(1.7f); Fill->SetLightColor(FLinearColor(.46f, .64f, 1.f)); Fill->SetCastShadows(false); Fill->SetVolumetricScatteringIntensity(0.f);
    Attach(Fill, FVector::ZeroVector, FRotator(-38.f, 20.f, 0.f));
    auto* Fog = NewObject<UExponentialHeightFogComponent>(this);
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
    // Invisible bounds of the slice.
    auto Wall = [&](const FVector& Ce, const FVector& Half)
    {
        auto* B = NewObject<UBoxComponent>(this); B->SetBoxExtent(Half); B->SetCollisionProfileName(TEXT("BlockAll")); B->SetHiddenInGame(true);
        Attach(B, Ce);
    };
    const float Mx = (MinX + MaxX) * .5f, Hx = (MaxX - MinX) * .5f;
    Wall(FVector(MinX - 50, 0, 2000), FVector(50, HalfY + 100, 4000)); Wall(FVector(MaxX + 50, 0, 2000), FVector(50, HalfY + 100, 4000));
    Wall(FVector(Mx, HalfY + 50, 2000), FVector(Hx + 100, 50, 4000)); Wall(FVector(Mx, -HalfY - 50, 2000), FVector(Hx + 100, 50, 4000));
}

void AFeywildSetting::BuildLand()
{
    auto Mat = [](const TCHAR* Path) { return LoadObject<UMaterialInterface>(nullptr, Path); };
    // The land: a grid over the slice and well beyond it, so the bounds never show an edge.
    {
        FFeySurf L; constexpr float X0 = -4200, X1 = 11000, Y0 = -7000, Y1 = 7000, Step = 44; const int32 Nx = FMath::CeilToInt((X1 - X0) / Step), Ny = FMath::CeilToInt((Y1 - Y0) / Step);
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
        Sheet(LedgeX + 40, Ledge2X + 40, FallY - FallHalf - 40, FallY + FallHalf + 40, LedgeTop - 14, 60, false);
        Sheet(Ledge2X + 40, Ledge2X + 900, FallY - FallHalf - 40, FallY + FallHalf + 40, Ledge2Top - 14, 60, false);
        Emit(TEXT("FeyWater"), W, Water, false, false);
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
        if (X > LedgeX - 130.f && X < LedgeX + 160.f) return false;
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
    constexpr float NX0 = -2000, NX1 = 5100, NY0 = -3900, NY1 = 3900;

    // ------------------------------ the ancient oaks: placed by hand to frame the path, as the goal images do
    auto* Oak0 = Kit(*Path(TEXT("SM_FeyOak0")), 16000, 26000, true), *Oak1 = Kit(*Path(TEXT("SM_FeyOak1")), 16000, 26000, true);
    auto* Col0 = Kit(*Path(TEXT("SM_FeyOak0Collision")), 0, 0, false, true, true), *Col1 = Kit(*Path(TEXT("SM_FeyOak1Collision")), 0, 0, false, true, true);
    struct FTree { int32 Kind; float X, Y, Scale, Yaw; };
    const FTree Trees[] = {{0, 1050, -1560, 1.3f, 40}, {1, 2500, -2650, 1.15f, 200}, {0, 4350, -2300, 1.25f, 120}, {1, 1500, 3050, 1.3f, 300}, {0, 4600, 3350, 1.45f, 10}, {1, -900, 2050, 1.1f, 80},
                           {1, -1100, -2500, 1.2f, 150}, {0, 6000, -2300, 1.5f, 260}, {1, 7300, 3100, 1.7f, 30}, {0, 7900, -1000, 2.1f, 100}, {1, 8700, 1500, 2.3f, 210}, {0, 9700, -3200, 2.6f, 330}, {1, 10000, 3900, 2.8f, 60}};
    for (const FTree& T : Trees)
    {
        Place(T.Kind ? Oak1 : Oak0, T.X, T.Y, T.Scale, T.Yaw, 14.f);
        (T.Kind ? Col1 : Col0)->AddInstance(FTransform(FRotator(0, T.Yaw, 0), FVector(T.X, T.Y, Ground(T.X, T.Y) - 14.f), FVector(T.Scale)));
    }
    // ------------------------------ the amber-gilled giants, near and, house-high, on the far rises
    auto* Amber = Kit(*Path(TEXT("SM_FeyCapAmber")), 22000, 30000, true);
    const FVector Giants[] = {{2750, -1180, 1.f}, {3500, -1500, 1.45f}, {3050, -1950, .72f}, {380, 1500, 1.25f}, {-250, 1180, .7f}, {6900, 2500, 3.2f}, {8300, -2300, 5.f}, {9300, 500, 6.5f}, {7600, 3700, 4.2f}};
    for (const FVector& G : Giants) Place(Amber, G.X, G.Y, G.Z, R.FRandRange(0.f, 360.f), 6.f, 2.f);
    // ------------------------------ bell-flower lamps along the path, each with a small real light
    auto* Bells = Kit(*Path(TEXT("SM_FeyBellLamp")), 9000, 14000, true);
    const FVector2D Lamps[] = {{620, 210}, {1650, 215}, {2500, -215}, {3350, -160}, {4250, -170}};
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
        if (X < 2000.f && FMath::Abs(Y - PathY(X)) < 800.f) continue;
        if (Free(X, Y, 260)) Place(Rock[R.RandRange(0, 5)], X, Y, R.FRandRange(.7f, 2.4f), R.FRandRange(0.f, 360.f), 18.f, 10.f);
    }
    for (int32 Step = 0; Step < 2; ++Step)
        for (float Y = -4300.f; Y < 4300.f; Y += R.FRandRange(150.f, 260.f))
        {
            if (FMath::Abs(Y - FallY) < FallHalf + 190.f) continue;
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

void AFeywildSetting::Tick(float Dt)
{
    Super::Tick(Dt);
    Clock += Dt;
    TickAir(FMath::Min(Dt, .05f));
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
        S.P.Z = FMath::Max(Ground(S.P.X, S.P.Y), WaterZ) + (bFly ? FMath::FRandRange(20.f, 260.f) : FMath::FRandRange(30.f, 520.f));
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
        Xf.Add(Facing(FeyWorld(S.P), S.Size)); Data.Add((S.bFly ? 9.f : 2.2f) * Fade * Blink); Data.Add(S.Hue);
    }
    if (Lights->GetInstanceCount() != Xf.Num()) { Lights->ClearInstances(); Lights->AddInstances(Xf, false, true); }
    else Lights->BatchUpdateInstancesTransforms(0, Xf, true, true);
    for (int32 I = 0; I < Xf.Num(); ++I) { Lights->SetCustomDataValue(I, 0, Data[I * 2], false); Lights->SetCustomDataValue(I, 1, Data[I * 2 + 1], I == Xf.Num() - 1); }
    // The mist's veils, turned to the eye, drifting a little.
    TArray<FTransform> Mf; Mf.Reserve(Veils.Num());
    for (const FVeil& V : Veils) Mf.Add(Facing(FeyWorld(V.P + FVector(FMath::Sin(Clock * .05f + V.Seed * 6.f) * V.Drift * 8.f, FMath::Cos(Clock * .04f + V.Seed * 9.f) * V.Drift * 8.f, 0)), V.Size));
    if (Mist->GetInstanceCount() != Mf.Num())
    {
        Mist->ClearInstances(); Mist->AddInstances(Mf, false, true);
        for (int32 I = 0; I < Veils.Num(); ++I) { Mist->SetCustomDataValue(I, 0, Veils[I].Alpha, false); Mist->SetCustomDataValue(I, 1, Veils[I].Seed, I == Veils.Num() - 1); }
    }
    else Mist->BatchUpdateInstancesTransforms(0, Mf, true, true);
}
