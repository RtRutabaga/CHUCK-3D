#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShipSetting.generated.h"

// The pirate ship (Claude, 2026-11-01; docs/regions/SHIP.md, docs/SHIP.md), after the 2D game's Phase 7: one
// continuous two-masted ship at sea, built in code at human scale (the jungle wreck's way, ChultWreck.cpp). Milestone 1
// is below decks: the arrival compartment (a sail locker amidships, where the escape from the temple's rubble crevice
// comes out of a mass of temple masonry grown through the timbers), the hold under it (cargo, the pantry's shelves and
// jars, rats), the galley forward, the crew quarters aft and the captain's cabin in the stern. The weather deck above is
// closed (milestone 2), lit through its gratings.
//
// The ship's frame: X to the bow, Y to port, Z up from the bottom of the keel, in cm; it sails on its own stage, its
// axes the world's, 8 km from Chult's landing.
namespace Ship
{
    constexpr float SternX = -1600, BowX = 1750;           // the transom and the stem at the deck
    constexpr float HalfBeam = 450;                         // at the weather deck
    constexpr float WaterZ = 300;                           // the waterline
    constexpr float HoldZ = 70, LowerZ = 330, DeckZ = 580;  // floor tops: the hold, the lower deck, the weather deck
    constexpr float LowerBeam = 300, DeckBeam = 555;        // the undersides of the beams over the hold and the lower deck
    constexpr float RailZ = 680;                            // the bulwark's top
    constexpr float QuarterZ = 700;                         // the quarterdeck over the cabin (its top)
    constexpr float QuarterFore = -900;
    // Bulkheads across the lower deck (their mid-planes), and over the hold.
    constexpr float CabinFore = -1080, ArrivalAft = -170, ArrivalFore = 330, GalleyFore = 1150;
    constexpr float HoldAft = -1300, HoldFore = 1150;
    constexpr float Bulkhead = 12;                           // half thickness
    // Doorways (centre Y, half width; 200 high).
    constexpr float DoorHalf = 52, DoorH = 200;
    constexpr float CabinDoorY = 0, QuartersDoorY = 0, GalleyDoorY = 160;
    // The main hatch in the arrival compartment's floor, down to the hold (X0..X1, Y half); its ladder on the fore edge.
    constexpr float HatchX0 = 20, HatchX1 = 140, HatchHalf = 60;
    constexpr float MainMastX = -490, ForeMastX = 440, MastR = 30;
    // The temple masonry grown through the arrival compartment's starboard-aft corner, and the crawlway through it that
    // the escape comes out of (the crevice's other end): it runs from its blind end (X0) toward +X to its mouth (X1).
    constexpr float SeamX0 = -150, SeamX1 = 130, SeamY = -300, SeamHalf = 40, SeamH = 44;
    // The broken planking in the port side that shows the open sea (the 2D escape's "a hole reveals the open sea").
    constexpr float HoleX0 = 50, HoleX1 = 200, HoleZ0 = LowerZ + 45, HoleZ1 = LowerZ + 120;
}

enum class EShipSpawn : uint8 { Arrival, Hold, Quarters, Galley, Cabin };

/** The stage's origin (the ship frame's origin in the world). */
FVector ShipOrigin();
inline FVector ShipWorld(const FVector& Local) { return ShipOrigin() + Local; }
inline FVector ShipLocal(const FVector& World) { return World - ShipOrigin(); }
/** Within the ship's stage (world): the hull and the sea round it. */
bool InShip(const FVector& World);
/** The ship's inside half-breadth at (X, Z) in the ship frame (0 off the ends). */
float ShipInnerHalf(float X, float Z);
float ShipOuterHalf(float X, float Z);
/** Spawn points (world, his capsule's centre) and the way each faces. */
FVector ShipSpawn(EShipSpawn Spawn);
float ShipSpawnYaw(EShipSpawn Spawn);
/** Where a death aboard returns him (the start of the space he was in), and its facing. */
bool ShipRespawn(const FVector& World, FVector& Out);
bool IsShipSpawn(const FVector& World, float& Yaw);
/** Over the side into the sea (world, his feet): an Astral death, as off the edge of the world. */
bool ShipSeaSwallows(const FVector& World);
/** Which space a point (world) is in, for respawns and tests: -1 none. */
int32 ShipSpaceAt(const FVector& World);
const TCHAR* ShipSpaceName(int32 Space);
/** The jars (ship frame, on their shelf or floor) and the rats' homes (ship frame, floor). */
const TArray<FVector>& ShipJars();
const TArray<FVector>& ShipRats();

class AChuckCharacter;
// -ChuckChultTest -ChuckShipTest (ShipTest.cpp): the escape from the rubble crevice played for real, then stills and the
// real character through every space aboard.
void ReviewShip(AActor* Owner, AChuckCharacter* Chuck, int32 Failures);

UCLASS()
class CHUCK3D_API AShipSetting : public AActor
{
    GENERATED_BODY()
public:
    AShipSetting();
    virtual void Tick(float DeltaSeconds) override;
    /** Hide whatever stage is standing (Waterdeep, Chult), and build the ship and its sea and light. Idempotent. */
    static AShipSetting* Enter(UWorld* World);
    static AShipSetting* Get(const UWorld* World);
    bool IsBuilt() const { return bBuilt; }
    int32 GetMissingMaterials() const { return Missing; }
    int32 GetTriangles() const { return Triangles; }
    int32 GetRatsPlaced() const { return RatsPlaced; }
    int32 GetJarsPlaced() const { return JarsPlaced; }
    int32 GetLadders() const { return Ladders; }
    /** Checks of the built ship (floors, doorways, ladder, spawns): failures. */
    int32 Check() const;
private:
    void Build();
    void BuildEnvironment();
    bool bBuilt = false;
    int32 Missing = 0, Triangles = 0, RatsPlaced = 0, JarsPlaced = 0, Ladders = 0;
    UPROPERTY() TArray<class UPointLightComponent*> Lamps;
    UPROPERTY() TArray<class USceneComponent*> Swingers;   // hanging lamps and hammocks, swung with the sea
    UPROPERTY() class UAudioComponent* Wash = nullptr;
    TArray<float> SwingPhase, LampBase;
    float Clock = 0, NextCreak = 4;
};
