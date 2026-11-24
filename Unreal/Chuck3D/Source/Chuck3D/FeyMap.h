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
    /** A lily pad he hops across the Lily Mere's deep water (X, Y within one), and whether (X, Y) is over that water. */
    bool IsPad(float X, float Y);
    bool InDeepWater(float X, float Y);
    /** The maze: its cells and the length of the way through. */
    int32 MazeCells();
    float MazeWay();
    /** (2026-11-21) The see-through check: rays cast from threshold Th's floor between T0 and T1, every way, at a rat's and
     *  a camera's height; a leak is a ray that passes under the threshold's own rock (its intended surface) without striking
     *  anything that shows. For FeyMapTest's =cracks mode. Local positions and directions. */
    struct FLeak { FVector At, Dir; float Dist; };
    int32 SeeThrough(UWorld* World, const AActor* Ignore, int32 Th, float T0, float T1, TArray<FLeak>& Out);

    /** (2026-11-21, the user: more spawn points within the big districts.) Places within a district a death can return him
     *  to once he has reached them, and that the dev menu and the save can start from. The district's own checkpoint is
     *  its first. */
    struct FSpot { int32 D; const TCHAR* Point; const TCHAR* Label; };
    int32 SpotCount();
    const FSpot& Spot(int32 I);
    FVector SpotAt(int32 I);     // local, on the ground
    float SpotYaw(int32 I);
    int32 SpotOf(const FString& Point);
    /** (2026-11-21, for FeyLife) A district's road from its mouth on threshold T in to its ring, its spur, its land. */
    const TArray<FVector2D>& RoadOf(int32 D, int32 T);
    const TArray<FVector2D>& SpurOf(int32 D);
    float LandZ(int32 D, float X, float Y);
    /** (2026-11-22, stage 6) The maze for FeyLife: its shortcuts (hedge sections X, Y, half length, yaw; where its hit plant
     *  appears; how many cells it saves), dead ends, orchid lanes (X, Y, angle), the way through, its rings and the heart. */
    struct FMazeShortcut { TArray<FVector4> Hedges; FVector2D Plant = FVector2D::ZeroVector, Mid = FVector2D::ZeroVector; int32 Gain = 0; };
    const TArray<FMazeShortcut>& MazeShortcuts();
    const TArray<FVector2D>& MazeDeadEnds();
    const TArray<FVector>& MazeLanes();
    const TArray<FVector2D>& MazeRoute();
    void MazeRings(FVector2D& Hub, float& R0, float& Pitch, float& Rmax, float& Tall);
    float MazeHeartAngle();
    /** (2026-11-23, stage 6) The maze's parkour, each over a bed of thorns across the way through (the whole corridor's width):
     *  0 a trellis along one hedge to run (Side +1 the outer hedge, -1 the inner), 1 ivy hoops to swing along, 2 a raised
     *  walkway on the outer side (a ladder up, steps down), 3 a short bed to jump. Along the way, S cm from its middle, is the
     *  angle Ac + Dir * S / R (radians; Dir +1 where the way runs anticlockwise); Bed0 to Bed1 the thorns. */
    struct FMazeParkour { int32 Kind = 3; float R = 0, Ac = 0, Dir = 1, Side = 1, Bed0 = 0, Bed1 = 0; bool bOffWay = false, bReward = false; int32 NHoop = 5; };   // (NHoop: how many hoops; three on the way through itself)   // (off the way through; cigarettes beyond it, in the dead end)
    /** Root gaps: low holes through a hedge, at its foot, he crawls through and the redcaps cannot (centre; the hedge's run, degrees). */
    struct FMazeGap { FVector2D C = FVector2D::ZeroVector; float Yaw = 0; };
    const TArray<FMazeParkour>& MazeParkour();
    const TArray<FMazeGap>& MazeGaps();
    /** (2026-11-23, second round) The unusual things along the way through, and only there (the user: one way to solve the
     *  maze is to keep heading toward them): 0 a stone figure, 1 a statue that turns while unseen, 2 thorn mites, 3 cigarettes.
     *  Each stands against a hedge with no opening on its side; In points to the corridor's middle. */
    struct FMazeMark { FVector2D P = FVector2D::ZeroVector, In = FVector2D::ZeroVector; int32 Kind = 0; };
    const TArray<FMazeMark>& MazeMarks();
    /** (2026-11-23, the user: spots narrow enough to wall jump to the top, only to be knocked down by the bees) Squeezes on the
     *  way: both hedges bulge in, leaving a slot SqueezeGap wide and SqueezeLen long, the hedges' full height (P its middle,
     *  In across it). */
    const TArray<FMazeMark>& MazeSqueezes();
    /** (for stills) The dead ends' frowning statues (X, Y, facing X, Y) and the decor (X, Y, angle in, kind: 0 lantern, 4 low fog). */
    const TArray<FVector4>& MazeFrowns();
    const TArray<FVector4>& MazeDecor();
    constexpr float SqueezeGap = 90.f, SqueezeLen = 360.f;   // (its flat faces 2 cm inside the leaves: 80 cm between)
    /** A point S along the way of parkour P, Across cm out from the middle of the corridor (toward the outer hedge). */
    FVector2D MazeParkourAt(const FMazeParkour& P, float S, float Across = 0.f);
    /** Down among the thorns (his feet, local)? */
    bool InMazeThorns(const FVector& Feet);
    // Their measures (cm): the trellis's face out from the corridor's middle and its reach either way; the hoops' grips (Hoops
    // of them, HoopStep apart, centred) and their height; the walkway (from, to, height, its middle across) and its ladder's foot.
    constexpr float TrellisFace = 120.f, TrellisReach = 260.f, HoopStep = 135.f, GripUp = 98.f, WalkFrom = -380.f, WalkTo = 40.f, WalkUp = 160.f, WalkAcross = 80.f, LadderFoot = -408.f, GapHalf = 45.f, GapTall = 46.f;
    constexpr int32 Hoops = 5;
}
FVector FeySpotWorld(int32 Spot);
/** (2026-11-23) The hag's cottage in the Deepwood: local X, Y of its middle, Z its yaw (its door, local +X, toward the trail). */
FVector FeyHagCottage();
/** Its ground there (local Z), and a point of it (local, from its own frame: +X the front, Y along it) on that ground. */
FVector FeyHagPoint(const FVector& Own);
/** Marks the spots within 4 m of a place (world) reached; forgets them all (a new game, the river again). */
void FeyReachSpots(const FVector& World);
void FeyForgetSpots();
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
    /** Mist veils lying in the district (X, Y, Z, size), once it is ready. */
    TArray<FVector4> Veils;
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
    UPROPERTY() class USceneCaptureComponentCube* Mirror = nullptr;   // (2026-11-23) the Lily Mere's one capture for its water to mirror
    UPROPERTY() class UTextureRenderTargetCube* MirrorTarget = nullptr;
    UPROPERTY() class AFeyLife* Life = nullptr;   // (2026-11-21) its flowers, pollen, orchids and moths, once it stands
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
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
    /** Districts A and B both, built at once (the see-through check). */
    void LoadPair(int32 A, int32 B);
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
    void BuildBrink();
    void BuildWonders();   // (2026-11-23) the floating islands and the giant jellyfish, always there
    UProceduralMeshComponent* Emit(const TCHAR* Name, FMapSurf& S, UMaterialInterface* Material, bool bSolid, bool bShadow);
    UPROPERTY() TArray<AFeyDistrict*> Districts;
    UPROPERTY() TArray<UHierarchicalInstancedStaticMeshComponent*> BackTrees;   // the distance's trees on each district's ground
    UPROPERTY() TArray<UBoxComponent*> LateWalls;                                // each threshold's two ends, shut while late
    UPROPERTY() UProceduralMeshComponent* StandIn = nullptr;                     // the maze seen from the overlook
    UPROPERTY() UProceduralMeshComponent* StandInStone = nullptr;
    UPROPERTY() class UMaterialInstanceDynamic* FarMid = nullptr;
    int32 Current = -1, MaxLoaded = 0, Late = 0, Missing = 0, Triangles = 0;
    float WorstReady = 0, Clock = 0, ReportAt = 0, Mist = 0, Gloom = 0;
    FVector LastSafe = FVector::ZeroVector;   // the last ground he stood on outside the deep water (world)
    int32 Drowned = 0;
    int32 VeilsOf = -1;   // (whose mist is up: the Deepwood's or, 2026-11-23, the maze's low fog)
    UPROPERTY() TArray<class UStaticMeshComponent*> Isles;
    TArray<FVector> IsleAt;
    UPROPERTY() class UStaticMeshComponent* Jelly = nullptr;
    UPROPERTY() class UPointLightComponent* JellyLight = nullptr;
public:
    /** (Tests and stills) the jellyfish's place (local) now. */
    FVector GetJellyAt() const;
    static FVector JellyAt(float Time);
    static float JellyTime;   // (stills: >= 0 holds it at that time)
private:
public:
    int32 GetDrowned() const { return Drowned; }
private:
    double WantAt[16] = {};
    TArray<bool> LateSeen;
    bool bSliceWanted = true;
};
