#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhlegethosSetting.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UDirectionalLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;
class UAudioComponent;
class AChuckCharacter;

// Phlegethos, the first layer of the Nine Hells (Claude, 2026-11-14; docs/regions/PHLEGETHOS.md), after the 2D game's
// Phase 8: one continuous stage built in code on its own site, 8 km beyond the ship's. A molten lake at Z 0 everywhere,
// and the basalt he walks on about 3 m above it. Six stretches along the route (X, north toward the fortress): the
// arrival plain where the fall lands, the lava road's spine, the basalt column lake, the lava-fall gorge with its chains,
// the ascent (the volcano erupting: bombs, crumbling steps, a wall-jump chimney) and the fortress approach to its shut
// gate. Falling into the lava is an Astral death, as the Astral Sea is: back at the start of the stretch.
//
// The frame: X along the route, Y across (to the right looking north), Z up from the lava, in cm.
namespace Phleg
{
    constexpr float LavaZ = 0;        // the molten surface
    constexpr float G = 300;          // the basalt's walking height over it
    constexpr float Rim = -60;        // where the basalt's sides go down to (under the lava)
    // The stretches, by X.
    constexpr float RoadX = 2500, LakeX = 8000, GorgeX = 12800, AscentX = 20000, FortressX = 25410, GateX = 28700;
    // The lava road: its spine's segments (X0, X1, width at each end), the gaps between them.
    struct FSeg { float X0, X1, W0, W1, Y0, Y1; };
    constexpr FSeg Spine[] = {{2300, 3800, 520, 180, 0, 40}, {3880, 4600, 160, 150, 40, 10}, {4695, 5600, 145, 140, 10, -60},
                              {5600, 7000, 135, 130, -60, -60}, {7230, 8000, 200, 560, -60, 0}};
    // Geysers beside the spine on the long straight: (X, side). Each erupts for Burn s every Period s.
    struct FGeyser { float X; float Side; float Phase; };
    constexpr FGeyser Geysers[] = {{4950, 1, 0.f}, {5300, -1, 1.1f}, {5850, 1, .4f}, {6250, -1, 1.6f}, {6650, 1, 2.3f}};
    constexpr float GeyserPeriod = 3.4f, GeyserWarn = .9f, GeyserBurn = 1.3f, GeyserReach = 150.f, GeyserSpread = 95.f;
    // The column lake: each column's centre, radius, top (above G) and whether it sinks when stood on.
    struct FColumn { float X, Y, R, Top; bool bSinks; };
    constexpr FColumn Columns[] = {
        {8571, -60, 55, 0, false}, {8694, 30, 50, 20, false}, {8823, -40, 48, 40, false}, {8942, 60, 55, 30, true},
        {9069, -10, 52, 70, false}, {9197, 70, 46, 60, false}, {9337, 0, 58, 25, true}, {9466, -80, 68, 25, false},
        {9598, -20, 62, 125, false}, {9745, 40, 55, 125, false}, {9912, 0, 75, 110, false},
        // (the slabs' gap: 9987..10427 is lava between the walls; a chained side wall run crosses it)
        {10496, 0, 95, 110, false}, {10653, -80, 50, 80, true}, {10787, -10, 48, 60, false}, {10917, 70, 52, 40, true}, {11065, 10, 55, 20, false}, {11201, -70, 50, 0, true}, {11337, 0, 50, -10, false}, {11467, 80, 55, 10, true}, {11607, 20, 50, 30, false}, {11754, -50, 60, 10, false}, {11925, 10, 70, 0, false}};
    constexpr float SlabX0 = 9840, SlabX1 = 10520, SlabHalfGap = 90, SlabThick = 45, SlabTop = G + 380;
    constexpr float LakeShoreX = 12200;   // the far shore (the gorge's start platform)
    constexpr float SinkDelay = .55f, SinkSpeed = 80.f;
    // The gorge: the cliff on the -Y side and the shelf along its foot, the breaks in it, the chains.
    constexpr float CliffY = -150, ShelfY1 = -40, CliffTop = G + 900;
    constexpr float ShelfX0 = GorgeX, Break1X0 = 15400, Break1X1 = 15640, Break2X0 = 16500, Break2X1 = 16620, LowShelf = G - 60;
    // The chains' rings hang below the low shelf (caught dropping off its end, as the ship's yard is off the main top), and
    // the far side is lower still, for the step down off the last ring; it rises to the ascent's foot.
    // (The far side starts 20 cm past the last ring: his step down off a line's end looks 40 cm ahead.)
    constexpr int32 ChainGrips = 9;
    constexpr float ChainX0 = 17300, ChainStep = 145, ChainX1 = ChainX0 + 70 + (ChainGrips - 1) * ChainStep + 20, ChainGripZ = LowShelf - 50, ChainY = -95, ChainLand = ChainGripZ - 95;
    // The ascent: the bomb ramp, the crumbling steps, the chimney, the last leap.
    constexpr float RampX0 = 20650, RampX1 = 22440, RampTop = G + 300, RampHalf = 230;
    struct FStep { float X, Y, Half; };
    constexpr FStep Steps[] = {{22560, 0, 60}, {22740, 60, 55}, {22920, -10, 55}, {23100, -70, 58}, {23280, 0, 55}, {23460, 70, 55}, {23640, 0, 60}};
    constexpr float StepCrack = .45f, StepFall = .95f;
    constexpr float ChimneyX0 = 23900, ChimneyX1 = 24060, ChimneyHalf = 50, ChimneyBase = RampTop, MesaTop = RampTop + 400, MesaX1 = 25180;
    constexpr float FortressTop = MesaTop;   // the approach plateau, across the last 2.7 m gap
    // The volcano (the land beyond, PhlegethosSetting.cpp; the fall cutscene looks at it).
    // (2026-11-15: 12 km off and 1.5 km high, about 7 degrees above his horizon as in the goal art; at 3.5 km and 2.2 km it
    // and the ranges walled in the sky from his eye.)
    inline const FVector2D VolcanoAt(1060000, -550000);
    constexpr float VolcanoTop = 150000;
    constexpr float VolcanoBase = 420000;   // its foot's radius
    constexpr float CraterR = 30000;
}

enum class EPhlegSpawn : uint8 { Arrival, Road, Lake, Gorge, Ascent, Fortress };
constexpr int32 PhlegSpawns = 6;

FVector PhlegethosOrigin();
inline FVector PhlegWorld(const FVector& Local) { return PhlegethosOrigin() + Local; }
inline FVector PhlegLocal(const FVector& World) { return World - PhlegethosOrigin(); }
/** Within the stage (world). */
bool InPhlegethos(const FVector& World);
/** Where the fall lands (world, the floor). */
FVector PhlegethosLandingSpot();
/** Spawn points (world, his capsule's centre) and their facing. */
FVector PhlegethosSpawn(EPhlegSpawn Spawn);
float PhlegethosSpawnYaw(EPhlegSpawn Spawn);
/** Where a death here returns him (the start of the stretch), and whether a point is a spawn (its yaw). */
bool PhlegethosRespawn(const FVector& World, FVector& Out);
bool IsPhlegethosSpawn(const FVector& World, float& Yaw);
/** Into the lava (world, his feet): an Astral death. */
bool PhlegethosLavaSwallows(const FVector& Feet);
/** Which stretch (0..5) a point is in, -1 outside; and whether the intense score plays there. */
int32 PhlegethosStretchAt(const FVector& World);
const TCHAR* PhlegethosStretchName(int32 Stretch);
bool PhlegethosIntense(const FVector& World, bool bWasIntense);

// -ChuckPhlegethosTest[=<stretch>] (PhlegethosTest.cpp): stills and the real character through each stretch's challenges.
void ReviewPhlegethos(AActor* Owner, AChuckCharacter* Chuck, int32 Failures);

UCLASS()
class CHUCK3D_API APhlegethosSetting : public AActor
{
    GENERATED_BODY()
public:
    APhlegethosSetting();
    virtual void Tick(float DeltaSeconds) override;
    /** Hide whatever stage stands and build Phlegethos with its light (idempotent). bLive: its hazards and life run. */
    static APhlegethosSetting* Enter(UWorld* World, bool bLive = true);
    static APhlegethosSetting* Get(const UWorld* World);
    /** The fall cutscene builds the stage early (no hazards) and makes it live as it ends. */
    void SetLive(bool bOn);
    bool IsLive() const { return bLive; }
    /** Everything moved by the hazards back as it was (a death, a checkpoint). */
    void ResetHazards();
    /** The volcano's eruption (the ascent, and the cutscene's crane up). */
    void Erupt(float Strength);
    /** Hell lightning: a bolt now, across the smoke sky or (bPlume) in the volcano's plume, its flash and its thunder. */
    void Strike(bool bPlume, float YawSpread = 65.f);
    int32 GetStrikes() const { return Strikes; }
    int32 GetMissingMaterials() const { return Missing; }
    int32 GetTriangles() const { return Triangles; }
    int32 GetSunk() const { return SunkCount; }
    int32 GetCrumbled() const { return CrumbledCount; }
    int32 GetGeyserHits() const { return GeyserHits; }
    int32 GetBombHits() const { return BombHits; }
    int32 GetBombs() const { return BombsFired; }
    /** A sinking column's or crumbling step's mesh (for tests): Kind 0 columns, 1 steps. */
    int32 GetMoverCount(int32 Kind) const;
    float GetMoverDrop(int32 Kind, int32 Index) const;
private:
    void Build();
    void BuildEnvironment();
    UProceduralMeshComponent* Emit(const TCHAR* Name, struct FPhlegSurf& S, UMaterialInterface* Material, bool bSolid, bool bShadow);
    void TickHazards(float Dt, AChuckCharacter* C);
    void TickMotes(float Dt);
    void TickLight(float Dt, AChuckCharacter* C);
    bool bBuilt = false, bLive = false;
    int32 Missing = 0, Triangles = 0;
    UPROPERTY() UMaterialInterface* BasaltMat = nullptr;
    UPROPERTY() UMaterialInterface* PathMat = nullptr;
    UPROPERTY() UMaterialInterface* LavaMat = nullptr;
    UPROPERTY() UMaterialInterface* GlowMat = nullptr;
    UPROPERTY() UMaterialInterface* IronMat = nullptr;
    UPROPERTY() UMaterialInterface* DarkMat = nullptr;
    UPROPERTY() UPointLightComponent* UnderLight = nullptr;   // the lava's light thrown up round him
    UPROPERTY() UPointLightComponent* AheadLight = nullptr;   // and on what is ahead of him
    UPROPERTY() UDirectionalLightComponent* Sun = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* Motes = nullptr;
    UPROPERTY() UAudioComponent* Rumble = nullptr;
    UPROPERTY() TArray<USoundBase*> Sounds;   // see the .cpp's ESound
    // Movers: sinking columns (0) and crumbling steps (1).
    struct FMover { TWeakObjectPtr<UProceduralMeshComponent> Mesh; FVector Top; float Half = 0; float Touched = -1; float Drop = 0; float DownFor = 0; int32 Kind = 0; bool bCracked = false; };
    TArray<FMover> Movers;
    // Geysers: their spout meshes and light.
    struct FSpout { TWeakObjectPtr<UProceduralMeshComponent> Mesh; TWeakObjectPtr<UPointLightComponent> Light; FVector Base; float Side = 1; float Phase = 0; int32 Stage = 0; };
    TArray<FSpout> Spouts;
    // Lava bombs: a glowing ball falling on a marked spot.
    struct FBomb { FVector From, To; float Age = 0, Flight = 2.f; bool bLanded = false; float After = 0; };
    TArray<FBomb> Bombs;
    UPROPERTY() UInstancedStaticMeshComponent* BombBalls = nullptr;
    UPROPERTY() UInstancedStaticMeshComponent* BombMarks = nullptr;
    float NextBomb = 0, Clock = 0, EruptUntil = -1, EruptStrength = 0;
    struct FMote { FVector P, V; float Age = 0, Life = 1, Size = 1, Heat = 0; };
    // Smoke (the temple jets' CC0 flipbook smoke): steam off the lava at vents near the route, and the volcano's plume.
    UPROPERTY() UInstancedStaticMeshComponent* Smoke = nullptr;
    struct FPuff { FVector P, V; float Age = 0, Life = 1, Size0 = 1, Size1 = 1, Frame = 0, Rate = 12, Heat = 0, Spin = 0; };
    TArray<FPuff> Puffs;
    TArray<FVector> Vents;
    float VentCarry = 0, PlumeCarry = 0;
    void TickSmoke(float Dt, const FVector& Eye);
    TArray<FMote> MoteList;
    // Hell lightning (2026-11-15): a few bolt meshes rebuilt each strike facing the eye, the sky's clouds lit round them,
    // a flash light across the land, the thunder after it.
    struct FFlash { int32 Bolt = 0; float Age = 0, Life = .7f, Power = 1, Thunder = 0; FVector Dir = FVector::ForwardVector; int32 Sound = 0; float Volume = .5f; bool bHeard = false; float Pulse[4] = {0, .1f, .22f, .36f}; float Gain[4] = {1, .55f, .85f, .3f}; };
    TArray<FFlash> Flashes;
    UPROPERTY() TArray<UProceduralMeshComponent*> Bolts;
    UPROPERTY() TArray<UMaterialInstanceDynamic*> BoltMids;
    UPROPERTY() UMaterialInstanceDynamic* SkyMid = nullptr;
    UPROPERTY() UDirectionalLightComponent* FlashLight = nullptr;
    float NextStrike = 4.f, UnderOn = 1.f, AheadOn = 1.f;
    int32 Strikes = 0, NextBolt = 0;
    void TickLightning(float Dt, const FVector& Eye);
    int32 SunkCount = 0, CrumbledCount = 0, GeyserHits = 0, BombHits = 0, BombsFired = 0;
    bool bWasAstral = false;
};
