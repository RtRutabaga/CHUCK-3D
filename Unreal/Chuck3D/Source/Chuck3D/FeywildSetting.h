#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeywildSetting.generated.h"

class UInstancedStaticMeshComponent;
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
    float PathY(float X);
    float Ground(float X, float Y);
}

FVector FeywildOrigin();
inline FVector FeyWorld(const FVector& Local) { return FeywildOrigin() + Local; }
inline FVector FeyLocal(const FVector& World) { return World - FeywildOrigin(); }
bool InFeywild(const FVector& World);
/** The dev checkpoint "FeyLook" (world, his capsule's centre) and its facing. */
FVector FeywildSpawn();
float FeywildSpawnYaw();

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
private:
    void Build();
    void BuildEnvironment();
    void BuildLand();
    void BuildPlanting();
    UProceduralMeshComponent* Emit(const TCHAR* Name, FFeySurf& S, UMaterialInterface* Material, bool bSolid, bool bShadow);
    UHierarchicalInstancedStaticMeshComponent* Kit(const TCHAR* Path, float CullStart, float CullEnd, bool bShadow, bool bSolid = false, bool bInvisible = false);
    void TickAir(float Dt);
    int32 Missing = 0, Triangles = 0, Planted = 0;
    float Clock = 0;
    UPROPERTY() UInstancedStaticMeshComponent* Lights = nullptr;   // fireflies and gold dust
    UPROPERTY() UInstancedStaticMeshComponent* Mist = nullptr;
    UPROPERTY() TArray<UHierarchicalInstancedStaticMeshComponent*> Kits;
    struct FSpark { FVector P, V; float Age = 0, Life = 1, Size = 1, Hue = 0, Phase = 0; bool bFly = false; };
    TArray<FSpark> Sparks;
    struct FVeil { FVector P; float Size = 500, Alpha = .15f, Seed = 0, Drift = 0; };
    TArray<FVeil> Veils;
};
