#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChultBirds.generated.h"

class UStaticMeshComponent;
class AChuckCharacter;

/**
 * Chult's flying jungle birds (Claude, 2026-10-19; docs/CHULT-BIRDS.md). User:
 * "add some flying jungle birds to chult, in particular i want some birds to fly
 * past player while crossing bridge and i want birds flying above temple in a
 * cinematic wonderous way".
 *
 * Scarlet and blue-and-gold macaws (Tools/build_chult_birds.py), flown here: a
 * body with two-part wings rolled about the shoulder and wrist, so each bird
 * flaps, glides, banks into its turns and pitches with its climb.
 *  - The bridge: a flock sweeps up the gorge across the bridge just ahead of
 *    him as he swings out, and later a pair overtakes him from behind, low over
 *    his head, and peels away down the gorge. Once each per crossing.
 *  - The temple: pairs wheel in a slow gyre above the spire, two pairs trace a
 *    figure of eight over the pyramids, and as the temple comes into view up the
 *    road (with the score's swell) a flock bursts from the trees ahead and flies
 *    out to join the gyre.
 * They never collide and change nothing in play. Coordinates as Chult's (local
 * to FloorOrigin, canyon frame S along, T across).
 */
UCLASS()
class CHUCK3D_API AChultBirds : public AActor
{
    GENERATED_BODY()
public:
    AChultBirds();
    virtual void Tick(float DeltaSeconds) override;

    enum class ERole : uint8 { Gyre, Eight, Reveal, BridgeCross, BridgePass };
    struct FBird
    {
        ERole Role = ERole::Gyre;
        USceneComponent* Root = nullptr;
        UStaticMeshComponent* Body = nullptr;
        UStaticMeshComponent* In[2] = {nullptr, nullptr};   // left, right
        UStaticMeshComponent* Out[2] = {nullptr, nullptr};
        float Scale = 1, Speed = 1000, FlapHz = 4.5f, FlapPhase = 0, Flap = 0, Seed = 0;
        // An orbit (gyre/eight, and reveal birds once they arrive): radius, height, phase, direction.
        float Radius = 3000, Height = 5000, Phase = 0, Dir = 1;
        FVector Centre = FVector::ZeroVector;
        int32 Partner = -1; float Lag = 0; FVector Beside = FVector::ZeroVector;   // a pair: follows Partner's line
        // A one-off flight (bridge and reveal): polyline with cumulative lengths, from Start + Delay.
        TArray<FVector> Path; TArray<float> Len;
        double Start = -1; float Delay = 0; bool bJoinOrbit = false; double Arrive = 0;
        bool bOrbitBefore = false;   // on its orbit until the flight starts (a swoop), not perched (the reveal)
        bool bSwooper = false;       // a low-wheel leader: swoops across in front of him in the precinct
        bool bVisible = false;
        FVector LastPos = FVector::ZeroVector;
    };

    // Spawned once from AChultLanding::Build (not in automated runs except -ChuckBirdReview).
    static AChultBirds* Spawn(UWorld* World);
    static AChultBirds* Find(UWorld* World);

    const TArray<FBird>& GetBirds() const { return Birds; }
    int32 Count(ERole Kind) const;
    int32 GetMissingMeshes() const { return MissingMeshes; }
    int32 GetBridgeFlights(bool bPass) const { return bPass ? BridgePasses : BridgeCrossings; }
    int32 GetRevealFlights() const { return RevealFlights; }
    int32 GetSwoops() const { return Swoops; }
    // Whether a swoop is under way now (its leader off the wheel).
    bool IsSwooping(double Now) const;
    // A bird's world position at a time (as flown), and whether it is out.
    FVector Position(int32 Index, double Time) const;
    bool IsOut(int32 Index, double Time) const;
    // Tests: launch the bridge flights or the reveal for a given place and heading.
    void LaunchBridge(bool bPass, float S, float Z, float Dir, float Side, double Now);
    void LaunchReveal(double Now);
    // Clearance: world-static hits along every orbit (Seconds of flight) and sample flights.
    int32 CheckClearance(float Seconds, TArray<FString>& Hits);

private:
    TArray<FBird> Birds;
    int32 MissingMeshes = 0, BridgeCrossings = 0, BridgePasses = 0, RevealFlights = 0, Swoops = 0, SwoopTurn = 0;
    double NextSwoop = -1;
    bool TrySwoop(double Now, const FVector& ChuckLocal, const FVector& ViewForward);
    // The bridge's events this crossing: armed off the bridge, each fires once.
    bool bCrossArmed = true, bPassArmed = true, bRevealArmed = true;
    float LastS = 0; double LastSAt = -1; float AlongSpeed = 0;
    void AddBird(ERole Kind, bool bScarlet, float Scale);
    void Pose(FBird& B, int32 Index, double Now, float Dt);
    void Watch(double Now);
    FVector OrbitAt(const FBird& B, double Time) const;
    FVector FlightAt(const FBird& B, double Time, bool& bOut) const;
};

// -ChuckChultTest -ChuckBirdReview: static checks and clearance, stills of the
// temple's birds, then the real character crosses the bridge and runs up the
// road to the reveal; logs CHUCK_CHULT_BIRDS_* and quits.
void ReviewChultBirds(AActor* Owner, AChuckCharacter* Chuck, int32 Failures);
