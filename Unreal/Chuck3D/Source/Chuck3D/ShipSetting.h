#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShipSetting.generated.h"

// The pirate ship (Claude, 2026-11-01; docs/regions/SHIP.md, docs/SHIP.md), after the 2D game's Phase 7: one
// continuous two-masted ship at sea, built in code at human scale (the jungle wreck's way, ChultWreck.cpp). Milestone 1
// is below decks: the arrival compartment (a sail locker amidships, where the escape from the temple's rubble crevice
// comes out of a mass of temple masonry grown through the timbers), the hold under it (cargo, the pantry's shelves and
// jars, rats), the galley forward, the crew quarters aft and the captain's cabin in the stern. Milestone 2 (2026-11-02) is
// the weather deck in the open: its rails, hatches and ladders, the helm on the quarterdeck, the masts with their tops,
// sails and rigging, an optional climb up to the main top and out along the main yard, and the open sea, its waves
// meeting the hull, rocking round the still hull with the sky and the sun (ShipDeck.inl, ShipSea.inl).
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
    constexpr float RailZ = 680;                            // the waist's rail (its cap's top)
    constexpr float WaistTopZ = DeckZ + 28;                 // the waist's planking ends here; posts and rails above it
    constexpr float QuarterRailZ = 800, QuarterTopZ = 728;  // the same round the quarterdeck
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
    // The fighting tops (their floors' tops) and the main yard slung just under the main top, forward of the mast: its
    // grips run out from either side of the top to the yardarms (brachiation), first at MainGrip0, every MainGripStep.
    constexpr float MainTopZ = 1600, ForeTopZ = 1500;
    constexpr float MainYardZ = MainTopZ - 55, MainYardX = MainMastX + 45, MainYardHalf = 650;
    constexpr float MainGrip0 = 170, MainGripStep = 90;
    constexpr int32 MainGrips = 6;                          // a side
    // The main top: its platform (X0..X1, Y half) with the lubber's hole forward of the mast; the rope ladder up to it
    // from the deck hangs on the hole's fore edge (TopLadderX).
    constexpr float TopX0 = MainMastX - 70, TopX1 = MainMastX + 190, TopHalf = 130;
    constexpr float LubberX0 = MainMastX + 60, LubberX1 = MainMastX + 110, LubberHalf = 30, TopLadderX = LubberX1;
    // The companion hatch from the crew quarters up to the weather deck (its ladder on the fore edge), and the two
    // ladders up the quarterdeck's break.
    constexpr float CompX0 = -310, CompX1 = -190, CompY0 = 70, CompY1 = 190;
    constexpr float BreakLadderY = 250;
    // The skylight over the cabin, and the helm.
    constexpr float SkyX0 = -1220, SkyX1 = -1110, SkyHalf = 50;
    constexpr float HelmX = -1420;
    // The temple masonry grown through the arrival compartment's starboard-aft corner, and the crawlway through it that
    // the escape comes out of (the crevice's other end): it runs from its blind end (X0) toward +X to its mouth (X1).
    constexpr float SeamX0 = -150, SeamX1 = 130, SeamY = -300, SeamHalf = 40, SeamH = 44;
    // The broken planking in the port side that shows the open sea (the 2D escape's "a hole reveals the open sea").
    constexpr float HoleX0 = 50, HoleX1 = 200, HoleZ0 = LowerZ + 45, HoleZ1 = LowerZ + 120;
}

enum class EShipSpawn : uint8 { Arrival, Hold, Quarters, Galley, Cabin, Deck };
constexpr int32 ShipSpawns = 6;

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
/** A recorded ship sound (one of Stem_00..Stem_<Count-1>) at a place aboard (world): heard near it (or across the deck if
 *  it carries), and quieter through a deck (the weather deck's sounds below, the lower decks' above). No ship: silent. */
void ShipSound(const UObject* Context, const TCHAR* Stem, int32 Count, const FVector& At, float Volume, bool bCarries = false);
/** Whether he is down in the hold: the hold's rats are heard only there (2026-11-03, the user). */
bool ShipHearsHold(const UObject* Context);

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
    int32 GetGrips() const { return Grips; }
    int32 GetSails() const { return Sails.Num(); }
    /** The sea's tilt round the still hull now: the pitch and roll (degrees) its plane, the sky and the sun are turned by. */
    FVector2D GetSeaTilt() const { return Tilt; }
    /** The sea's surface height (world Z) at a world point, without its waves: the tilted plane through the waterline. */
    float SeaPlaneZ(const FVector& World) const;
    /** Checks of the built ship (floors, doorways, ladder, spawns): failures. */
    int32 Check() const;
    /** ShipSound's work (see it); and how many times each stem has played, for the tests. */
    void PlayAt(const TCHAR* Stem, int32 Count, const FVector& At, float Volume, bool bCarries);
    int32 GetSoundsPlayed(const TCHAR* Stem) const { const int32* N = SoundsPlayed.Find(Stem); return N ? *N : 0; }
    /** The ambience's levels now (before the opening fade): the wash, the water along the hull, the lapping, the rigging. */
    FVector4 GetAmbience() const { return FVector4(AmbNow[0], AmbNow[1], AmbNow[2], AmbNow[3]); }
    int32 GetCreaks() const { return Creaks; }
    /** The sun's light now (its intensity: dimmed by the storm, spiking in a lightning flash). */
    float GetSunIntensity() const;
private:
    void Build();
    void BuildEnvironment();
    void BuildSea();
    void BuildSails();
    void UpdateSails();
    void Rock();
    bool bBuilt = false;
    int32 Missing = 0, Triangles = 0, RatsPlaced = 0, JarsPlaced = 0, Ladders = 0, Grips = 0;
    UPROPERTY() TArray<class UPointLightComponent*> Lamps;
    UPROPERTY() TArray<class USceneComponent*> Swingers;   // hanging lamps and hammocks, swung with the sea
    UPROPERTY() class UAudioComponent* Wash = nullptr;
    // The recorded ambience (2026-11-03): the sea rushing along the hull and the wind in the rigging on deck, water
    // lapping at the planking below; and the one-shot sounds' attenuations and loaded sets.
    UPROPERTY() class UAudioComponent* Water = nullptr;
    UPROPERTY() class UAudioComponent* Lap = nullptr;
    UPROPERTY() class UAudioComponent* Rigging = nullptr;
    UPROPERTY() class USoundAttenuation* NearHearing = nullptr;
    UPROPERTY() class USoundAttenuation* FarHearing = nullptr;
    UPROPERTY() TArray<class USoundBase*> SoundPool;
    TMap<FString, TArray<int32>> SoundSets;
    TMap<FString, int32> SoundsPlayed;
    float AmbNow[4] = {};
    int32 Creaks = 0;
    UPROPERTY() TArray<class UProceduralMeshComponent*> Sails;
    UPROPERTY() class UMaterialInstanceDynamic* SeaMid = nullptr;
    UPROPERTY() class UDirectionalLightComponent* Sun = nullptr;
    UPROPERTY() class USkyAtmosphereComponent* Sky = nullptr;
    UPROPERTY() class UProceduralMeshComponent* Flag = nullptr;
    UPROPERTY() class USkyLightComponent* SkyFill = nullptr;   // faint below decks, fuller when the view is out on deck
    UPROPERTY() class UProceduralMeshComponent* SeaSurface = nullptr;
    // The storm (2026-11-03, ShipStorm.h): the parts of the sky it changes, and the swell's phases (accumulated, so its
    // periods can change as it builds).
    UPROPERTY() class UVolumetricCloudComponent* CloudLayer = nullptr;
    UPROPERTY() class UMaterialInstanceDynamic* CloudMid = nullptr;
    UPROPERTY() class UExponentialHeightFogComponent* HazeFog = nullptr;
    UPROPERTY() class UPostProcessComponent* Grade = nullptr;
    // Lightning's light on the ship (not the sky's: the clouds are lit by the sun alone, which only brightens slowly in a
    // flash; a sudden change there speckles every edge against the sky).
    UPROPERTY() class UDirectionalLightComponent* FlashLight = nullptr;
    float FlashSlow = 0;
    void ApplyStorm(float Dt);
    float RollPhase = 0, PitchPhase = 0, ChopPhase = 0, SailClock = 0, StormSea = 0, StormWind = 0, CloudSet = -1;
    TArray<float> SwingPhase, LampBase;
    FQuat SunBase = FQuat::Identity;
    FVector2D Tilt = FVector2D::ZeroVector;
    float Clock = 0, NextCreak = 4;
};
