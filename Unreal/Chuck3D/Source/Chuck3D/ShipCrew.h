#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DockNPC.h"
#include "ShipCrew.generated.h"

// The pirate ship's crew (Claude, 2026-11-02; docs/regions/SHIP.md, docs/SHIP.md milestone 3), after the 2D game's Phase 7.
// AShipCrew puts them aboard when the ship is built (AShipSetting::Build): the chef in the galley, the seated pirate at the
// crew quarters' mess table, and on the weather deck the concertina player, the cheering pirate with his tankard, the dancing
// pirate, two fencing pirates and Jeffries tied to the main mast. It gives each his lines (the 2D game's first and repeat
// lines, exactly), keeps them to the ship score's beat, runs the fencing (the two wander the open deck forward of the fore
// mast, advancing, lunging and parrying in eight-beat phrases) and costs Chuck Sanity where a blade or a fencer meets him.
// Their performances are ADockNPC's (DockNPCPirate.inl).

// The ship score's clock (the user's ship track, SW_Ship: 136.12 beats a minute, its first beat 0.107 s in; measured from the
// track, docs/SHIP.md). DockGameMode tells it when the track starts; the crew read the beat from it.
namespace ShipScore
{
    constexpr float BeatsPerMinute = 136.12f, FirstBeat = .107f, Duration = 207.72f;
    /** The track began playing now at OffsetSeconds into itself. */
    void Started(const UWorld* World, float OffsetSeconds);
    /** Beats since the track's first beat (looping with it); from the crew's own clock if the track isn't playing. */
    float Beat(const UWorld* World);
    bool IsPlaying();
}

UCLASS()
class CHUCK3D_API AShipCrew : public AActor
{
    GENERATED_BODY()
public:
    AShipCrew();
    virtual void Tick(float DeltaSeconds) override;
    /** Spawn the crew aboard (once). */
    static AShipCrew* Spawn(UWorld* World);
    static AShipCrew* Get(const UWorld* World);
    ADockNPC* Find(EDockHuman Kind) const;
    int32 GetCount() const;
    float GetBeat() const { return Beat; }
    /** Sanity the fencers' blades and bodies have cost Chuck (bites landed). */
    int32 GetFencerHits() const { return FencerHits; }
    /** The fencers' phrases so far, and the furthest either has gone outside his ground (cm; 0 inside). */
    int32 GetFencePhrases() const { return Phrases; }
    float GetWorstFenceOut() const { return WorstFenceOut; }
    /** The closest the fencers' blade tips came to each other's (cm) during this phrase's attacks: a parry meets the thrust. */
    float GetLastParryGap() const { return LastParryGap; }
    /** The middle between the fencers now (world, on the deck). */
    FVector GetFencePair() const;
    /** The furthest any of the crew's bodies has gone into the hull's planking (cm; 0 clear): nothing passes through it. */
    float GetWorstHullOut() const { return WorstHullOut; }
private:
    void PlaceCrew();
    void TickFencing(float DeltaSeconds);
    UPROPERTY() TArray<ADockNPC*> Crew;
    float Beat = 0.f;
    // The fencing: the pair's middle and the line between them (fencer A toward B), in the ship frame.
    FVector2D PairStart = FVector2D::ZeroVector, Pair = FVector2D::ZeroVector;
    float AxisYaw = 0.f, AxisStart = 0.f, AxisTurn = 0.f;
    int32 Phrase = -1, Leader = 0, Phrases = 0, FencerHits = 0;
    bool bPress = false;
    float WorstFenceOut = 0.f, LastParryGap = 1e3f, PhraseParryGap = 1e3f, WorstHullOut = 0.f;
};
