#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeyMap.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UBoxComponent;
class UMaterialInterface;
struct FMapSurf;

// The Feywild in rough form (Claude, 2026-11-19; docs/regions/FEYWILD.md, build stage 3): every district of the plan's map
// walkable, with its routes, thresholds, district loading, checkpoints and music zones, to prove the size, flow and cost.
// The art pass is stage 4: the land, planting and blockout here are placeholders built from the look slice's kit.
//
// Local Fey frame (FeywildSetting.h). The look slice is the riverbank's east end; the map runs west (-X) from it. Each
// district stands in its own rectangle, and every road between two of them is a threshold: a winding passage about 40 m
// long through the gap between their rectangles. Its S-bend hides each side from the other at its middle, where the
// district behind is dropped once the one ahead is ready. At most one district is loaded, two while crossing.
namespace FeyMap
{
    enum EDistrict : int32 { Riverbank, Orchard, Crossroads, LilyMere, Deepwood, Highmeadow, Maze, Districts };
    enum EStyle : int32 { RockCut, RootTunnel, StoneStair };
    struct FDistrict
    {
        const TCHAR* Name; const TCHAR* Point; const TCHAR* Label;
        float X0, X1, Y0, Y1;
        int32 Music;                // 0 main, 1 dark, 2 open, 3 maze
        FVector2D Hub; float HubR;  // the paths meet on a ring round the hub
    };
    struct FThreshold { int32 A, B; FVector2D PA, PB; float ZA, ZB, Swing; int32 Style; };
    constexpr int32 Thresholds = 10;
    constexpr float Half = 230.f;   // a threshold's half width at its floor
    const FDistrict& District(int32 D);
    const FThreshold& Threshold(int32 T);
    /** The district whose land (X, Y) is on, or -1 (a threshold's gap, or outside). The look slice is the riverbank's. */
    int32 DistrictAt(float X, float Y);
    /** The threshold Local is in and how far along it (0 at A's mouth, 1 at B's), or -1. */
    int32 ThresholdAt(const FVector& Local, float& OutT);
    /** The ground of the map's own land at (X, Y) (not the look slice's), or of a threshold's floor. */
    bool GroundAt(float X, float Y, float& OutZ);
    /** A threshold's middle line at T (local, on its floor), and its way along. */
    FVector ThresholdAtT(int32 Th, float T);
    /** A district's checkpoint (local, on the ground) and the way it faces. The riverbank's is the river's shore. */
    FVector Checkpoint(int32 D);
    float CheckpointYaw(int32 D);
    /** The walk through district D from its mouth on threshold Ta to its mouth on Tb (local points on the ground; -1 is
     *  its checkpoint), along its paths. For the route test. */
    TArray<FVector> Walk(int32 D, int32 Ta, int32 Tb);
}
/** Inside the rough map's bounds (world). */
bool InFeyMap(const FVector& World);
/** Where a death in the Feywild returns him (world, his capsule's centre): the checkpoint of the district he is in, or of
 *  the side of the threshold he is on. */
FVector FeyRespawn(const FVector& World);
FVector FeyCheckpointWorld(int32 District);
bool IsFeyCheckpoint(const FVector& World, float& OutYaw);
/** The music zone at a place: 0 main, 1 dark, 2 open, 3 maze; -1 in a threshold (whatever plays goes on). */
int32 FeyMusicZone(const FVector& World);

/** -ChuckChultTest -ChuckFeyMapTest[=short] (FeyMapTest.cpp). */
void ReviewFeyMap(AActor* Owner, class AChuckCharacter* Chuck, int32 Failures);

/** One district's land, water, planting and blockout, built on a worker and then put in place a little each frame. */
UCLASS()
class CHUCK3D_API AFeyDistrict : public AActor
{
    GENERATED_BODY()
public:
    AFeyDistrict();
    virtual void Tick(float DeltaSeconds) override;
    void Begin(int32 InDistrict);
    /** Puts the rest in place now (a checkpoint start or a respawn, under the menu's cut). */
    void Finish();
    bool IsReady() const { return bReady; }
    int32 GetDistrict() const { return District; }
    float GetBuildSeconds() const { return BuildSeconds; }
    float GetPlanSeconds() const { return PlanSeconds; }
    float GetWorstStepMs() const { return WorstStepMs; }
    int32 Triangles = 0, Planted = 0, Missing = 0;
    struct FPlan;
private:
    bool Step(double BudgetMs);
    TSharedPtr<FPlan, ESPMode::ThreadSafe> Plan;
    int32 District = -1, Stage = 0, Item = 0, Offset = 0;
    bool bReady = false, bPlanned = false;
    double Started = 0;
    float BuildSeconds = 0, PlanSeconds = 0, WorstStepMs = 0;
    UPROPERTY() TArray<UHierarchicalInstancedStaticMeshComponent*> Kits;
    UPROPERTY() TArray<UMaterialInterface*> Mats;
};

/** The map: which districts stand, the thresholds, the painted distance round them, the far tower over the maze. */
UCLASS()
class CHUCK3D_API AFeyMap : public AActor
{
    GENERATED_BODY()
public:
    AFeyMap();
    virtual void Tick(float DeltaSeconds) override;
    static AFeyMap* Get(const UWorld* World);
    /** The map, spawned with its thresholds and distance if it is not there yet (in the Feywild, after the river). */
    static AFeyMap* Ensure(UWorld* World);
    /** Only district D, built at once (a checkpoint start or a respawn). */
    void LoadNow(int32 D);
    bool IsLoaded(int32 D) const;
    bool IsReady(int32 D) const;
    int32 GetLoadedCount() const;
    int32 GetCurrent() const { return Current; }
    int32 GetMissing() const;
    int32 GetMaxLoaded() const { return MaxLoaded; }
    int32 GetLateCount() const { return Late; }
    float GetWorstReadySeconds() const { return WorstReady; }
private:
    void Want(int32 D);
    void Drop(int32 D);
    void BuildThresholds();
    void BuildDistance();
    void BuildTower();
    void BuildStandIn();
    UProceduralMeshComponent* Emit(const TCHAR* Name, FMapSurf& S, UMaterialInterface* Material, bool bSolid, bool bShadow);
    UPROPERTY() TArray<AFeyDistrict*> Districts;
    UPROPERTY() TArray<UHierarchicalInstancedStaticMeshComponent*> BackTrees;   // the distance's trees on each district's ground
    UPROPERTY() TArray<UBoxComponent*> LateWalls;                                // each threshold's two ends, shut while late
    UPROPERTY() UProceduralMeshComponent* StandIn = nullptr;                     // the maze seen from the overlook
    UPROPERTY() class UMaterialInstanceDynamic* FarMid = nullptr;
    int32 Current = -1, MaxLoaded = 0, Late = 0, Missing = 0, Triangles = 0;
    float WorstReady = 0, Clock = 0, ReportAt = 0, Mist = 0;
    double WantAt[16] = {};
    TArray<bool> LateSeen;
    bool bSliceWanted = true;
};
