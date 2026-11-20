#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeywildSetting.generated.h"

class UInstancedStaticMeshComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class USceneCaptureComponentCube;
class UTextureRenderTargetCube;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
struct FFeySurf;

// The Feywild's look slice (Claude, 2026-11-18; docs/regions/FEYWILD.md, build stage 1): one small finished piece of
// riverbank and forest, for the user to judge the sky, light, water, plants and fungi against the goal images before the
// world is built out. A path runs east (+X) from the arrival past a still pond fed by a two-step waterfall. The sun never
// moves: it sits low in the east, ahead of him.
//
// The frame: X along the path, Y across (to the right looking along it), Z up from the pond's surface, in cm.
namespace Fey
{
    constexpr float WaterZ = 0;
    constexpr float PondX = 3400, PondY = 1300, PondRX = 1700, PondRY = 1150;
    constexpr float LedgeX = 5160, LedgeTop = 520, Ledge2X = 6400, Ledge2Top = 940, FallY = 1350, FallHalf = 230;
    constexpr float MinX = -2200, MaxX = 7000, HalfY = 4200;   // the slice's invisible bounds
    constexpr float GreatX = 62000, GreatY = -9000;            // the great tree, far across the valley
    float PathY(float X);
    float Ground(float X, float Y);
    /** 1 where the land east of the path falls away to the valley, 0 on the falls' side and the far headland. */
    float Vista(float Y);
    /** Where the overlook's edge begins. */
    float CliffX(float Y);
    // The river the cutscene rides (FeyRiverScene.h; stage 2, 2026-11-19): its own set far south of the slice, out of sight
    // of it, flowing east toward the low sun. S is cm along its middle from the root tunnel's dark end (0); the tunnel opens
    // at RiverMouth. RiverAt is the middle at the water's surface; RiverSide points to the left bank.
    constexpr float RiverStart = -800, RiverMouth = 2100, RiverEnd = 11800;
    FVector RiverAt(float S);
    FVector RiverSide(float S);
    float RiverHalf(float S);
    float RiverGround(float X, float Y);
    /** Where he washes up on the pond's south shore (local, on the ground), and the way he faces there. */
    FVector ShoreSpot();
    float ShoreYaw();
}

FVector FeywildOrigin();
inline FVector FeyWorld(const FVector& Local) { return FeywildOrigin() + Local; }
inline FVector FeyLocal(const FVector& World) { return World - FeywildOrigin(); }
bool InFeywild(const FVector& World);
/** A paw in the pond's water (his steps splash). */
bool FeyWading(const FVector& Paw);
/** The dev checkpoint "FeyLook" (world, his capsule's centre) and its facing. */
FVector FeywildSpawn();
float FeywildSpawnYaw();
/** The arrival checkpoint "FeyArrival", where the river leaves him (world, his capsule's centre), and its facing. */
FVector FeywildArrival();
float FeywildArrivalYaw();

UCLASS()
class CHUCK3D_API AFeywildSetting : public AActor
{
    GENERATED_BODY()
public:
    AFeywildSetting();
    virtual void Tick(float DeltaSeconds) override;
    /** Hide whatever stage stands and build the slice with its light (idempotent). */
    static AFeywildSetting* Enter(UWorld* World);
    static AFeywildSetting* Get(const UWorld* World);
    /** The river's set for the cutscene (idempotent); Show false hides it again once he is past it. */
    void BuildRiver();
    void ShowRiver(bool bShow);
    /** In the root tunnel: the sky's fill and the sky light down to K of themselves (the sun's shadow does the rest). */
    void SetShade(float K);
    /** Rings and drops on the pond where something hits it or moves in it (the cutscene's plunge and drift). */
    void Splash(const FVector& World, float Hard);
    int32 GetMissing() const { return Missing; }
    /** The look slice's own land and planting, built again, or dropped (stage 3: it is the riverbank's east end, FeyMap.h). */
    void LoadSlice();
    void UnloadSlice();
    bool IsSliceLoaded() const { return bSlice; }
    /** Mist thickening (0 to 1) while a district ahead is still loading. */
    void SetMistBoost(float K);
private:
    void Build();
    void BuildEnvironment();
    void BuildLand();
    void BuildPlanting();
    void BuildFarLand();
    void BuildProbe();
    void BuildSound();
    void TickWater(float Dt);
    UProceduralMeshComponent* Emit(const TCHAR* Name, FFeySurf& S, UMaterialInterface* Material, bool bSolid, bool bShadow);
    UHierarchicalInstancedStaticMeshComponent* Kit(const TCHAR* Path, float CullStart, float CullEnd, bool bShadow, bool bSolid = false, bool bInvisible = false);
    void TickAir(float Dt);
    int32 Missing = 0, Triangles = 0, Planted = 0;
    float Clock = 0;
    UPROPERTY() UInstancedStaticMeshComponent* Lights = nullptr;   // fireflies and gold dust
    UPROPERTY() UInstancedStaticMeshComponent* Mist = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* Rings = nullptr;     // ripples on the pond
    UPROPERTY() UProceduralMeshComponent* WaterMesh = nullptr;
    UPROPERTY() UProceduralMeshComponent* StreamMesh = nullptr;   // the streams on the two rock steps
    float AirShade = 1.f;                                         // the motes dim with SetShade (none in the root tunnel)
    UPROPERTY() UMaterialInstanceDynamic* FarMid = nullptr;
    UPROPERTY() USceneCaptureComponentCube* Probe = nullptr;
    UPROPERTY() UTextureRenderTargetCube* ProbeTarget = nullptr;
    UPROPERTY() UAudioComponent* FallsSound = nullptr;
    UPROPERTY() class USoundBase* SplashSound = nullptr;
    UPROPERTY() TArray<UHierarchicalInstancedStaticMeshComponent*> Kits;
    UPROPERTY() TArray<USceneComponent*> RiverParts;
    UPROPERTY() class UDirectionalLightComponent* FillLight = nullptr;
    UPROPERTY() class USkyLightComponent* Sky = nullptr;
    float FillBase = 1.7f, SkyBase = 1.7f;
    bool bRiver = false, bSlice = false, bMapStarted = false;
    UPROPERTY() TArray<UActorComponent*> SliceParts;
    UPROPERTY() class UExponentialHeightFogComponent* Fog = nullptr;
    int32 RiverVeil0 = -1;
    struct FSpark { FVector P, V; float Age = 0, Life = 1, Size = 1, Hue = 0, Phase = 0; bool bFly = false; };
    TArray<FSpark> Sparks;
    struct FVeil { FVector P; float Size = 500, Alpha = .15f, Seed = 0, Drift = 0; };
    TArray<FVeil> Veils;
    struct FRing { FVector P; float Age = 0, Life = 1.4f, Size = 100, Alpha = .6f; };
    TArray<FRing> Ripples;
    struct FDrop { FVector P, V; float Age = 0, Life = .5f, Size = 3; };
    TArray<FDrop> Drops;
    float NextRing = 0, NextFallRing = 0;
    bool bWasWading = false;
    double PerfMs = 0, PerfGpu = 0; int32 PerfFrames = 0; float PerfAt = 0;
};
