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
// mast in eight-beat phrases of cuts, thrusts, feints, beats and binds, each parry turning into the riposte; 2026-11-03)
// and costs Chuck Sanity where a blade or a fencer meets him.
// Their performances are ADockNPC's (DockNPCPirate.inl).

// The ship score's clock. Below decks the user's ship track plays (SW_Ship); on the weather deck, their concertina version of
// it (SW_ShipConcertina, 2026-11-02), taking over at the matching moment of the tune and handing back the same way. Each has
// its own beat grid (Tools/align_ship_music.py, ShipMusicMap.inl; docs/SHIP.md). DockGameMode tells this clock which track
// started where; the crew keep to the beat of whichever is playing.
namespace ShipScore
{
    enum class ETrack : uint8 { Ship, Concertina };
    /** The track began playing now at OffsetSeconds into itself. */
    void Started(const UWorld* World, ETrack Track, float OffsetSeconds);
    ETrack Current();
    float Duration(ETrack Track);
    /** Seconds into the playing track (looping with it). */
    float Position(const UWorld* World);
    /** The moment of the other track that sounds most like Seconds into this one. */
    float Matching(ETrack From, float Seconds);
    /** Beats since the score began, counted on through loops and changes of track (whole beats kept; the part of a beat
     *  follows the playing track's grid); from the crew's own clock if no track is playing. */
    float Beat(const UWorld* World);
    bool IsPlaying();
    /** How far (in beats, -0.5..0.5) the beat moved at the last change of track: how well the two lined up there. */
    float LastBeatStep();
    /** Where the concertina version comes from (the concertina player, on the weather deck). */
    FVector ConcertinaSource();
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
    /** How far apart the fencers' blades were (cm) at the last blow that should land on the other's blade (0: touching). */
    float GetLastParryGap() const { return LastParryGap; }
    /** The widest such gap since the fencing settled, and how many such blows there have been. */
    float GetWorstContactGap() const { return WorstContactGap; }
    int32 GetFenceContacts() const { return Contacts; }
    /** ... and how many of them touched (blades within 4 cm). */
    int32 GetFenceTouching() const { return Touching; }
    /** Blows of each kind so far (EAttack order: head, flank, low, lunge, feint, beat, bind), as text. */
    FString GetFenceTally() const;
    /** The middle between the fencers now (world, on the deck). */
    FVector GetFencePair() const;
    /** The furthest any of the crew's bodies has gone into the hull's planking (cm; 0 clear): nothing passes through it. */
    float GetWorstHullOut() const { return WorstHullOut; }
private:
    void PlaceCrew();
    void TickFencing(float DeltaSeconds);
    /** Set out this phrase's blows (a new choice each phrase, from the phrase's number). */
    void PlanPhrase();
    UPROPERTY() TArray<ADockNPC*> Crew;
    float Beat = 0.f;
    // The fencing: the pair's middle and the line between them (fencer A toward B), in the ship frame.
    FVector2D PairStart = FVector2D::ZeroVector, Pair = FVector2D::ZeroVector;
    float AxisYaw = 0.f, AxisStart = 0.f, AxisTurn = 0.f;
    int32 Phrase = -1, Leader = 0, Phrases = 0, FencerHits = 0;
    // This phrase: each fencer's sword keys, the pair's ground gained (Off: cm along fencer A's facing), and the blows that
    // should meet the other's blade (beat, and the closest the blades came about then).
    TArray<FFenceKey> Keys[2], PairKeys;
    struct FBlow { float Strike = 0.f; float Gap = 1e3f; bool bContact = true; bool bDone = false; uint8 Kind = 0; };
    TArray<FBlow> Blows;
    int32 Contacts = 0, Touching = 0;
    FVector2D FencerAt[2] = { FVector2D::ZeroVector, FVector2D::ZeroVector };
    float StrideNow[2] = { 0.f, 0.f };
    int32 Tally[7] = {};
    float WorstFenceOut = 0.f, LastParryGap = 1e3f, WorstContactGap = 0.f, WorstHullOut = 0.f;
};
