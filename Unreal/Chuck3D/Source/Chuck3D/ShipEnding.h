#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DockNPC.h"
#include "ShipEnding.generated.h"

// The ship's ending in the storm (Claude, 2026-11-08; docs/regions/SHIP-STORM-ENDING.md; user: "create the storm sequence,
// including adding all the lines"). Once the storm's second part comes on, the captain sends the cabin boy (Chuck) up the
// main shrouds to secure the reef points at both main yardarms, where the canvas has worked loose and flogs. With both fast,
// the crew call the fore topsail's clewline fouled in its quarter block, and a sea breaks aboard forward: the ratlines of the
// fore lower shrouds carry away. A golden cigarette (the lookout's stash) lies in the main top; on its stamina lock a running
// jump off the main top's fore rim is a long leap (LeapForward, LeapUp: on the ship only), drawn gently onto the fore top.
// From the fore top he climbs the topmast shrouds; at their head the fore topsail cutscene takes over (AShipForeTopsailScene).
// A gold guide light stands on the next target. Every line is the user's voiced take (NPCVoiceData: PirateCaptain,
// PirateCrewman1, PirateCrewman2, Jeffries): scripted beats once, guidance lines when he lingers in the wrong place (the
// period jargon first, a clear line after 30 s), the background orders in the gaps. One line at a time.
// The steps done survive deaths aboard; ShipGate::Reset (a fresh start from the menu) forgets them.

class AChuckCharacter;
class ACigarettePickup;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class USoundAttenuation;

UCLASS()
class CHUCK3D_API AShipEnding : public AActor
{
    GENERATED_BODY()
public:
    AShipEnding();
    virtual void Tick(float DeltaSeconds) override;
    static AShipEnding* Spawn(UWorld* World);
    static AShipEnding* Get(const UWorld* World);

    enum class EStep : uint8 { Waiting, Reef, Trouble, Leap, ForeClimb, Scene, Done };
    /** Where he is, as the crew see it. */
    enum class EWhere : uint8 { Deck, MainRigging, ForeRigging, MainTop, MainYard, MainAbove, ForeTop, ForeAbove, Air, Other };
    static const TCHAR* StepName(EStep Step);
    static const TCHAR* WhereName(EWhere Where);
    EStep GetStep() const { return Step; }
    EWhere GetWhere() const { return Where; }
    /** The main yardarms: 0 larboard (+Y), 1 starboard. */
    bool IsSideDone(int32 Side) const { return bSideDone[Side & 1]; }
    bool IsCarriedAway() const { return bCarried; }
    /** The line being spoken now: its subtitle (none for Jeffries' sounds). */
    bool GetSubtitle(FString& Speaker, FString& Text) const;
    bool IsSpeaking() const { return Speaking.Seconds > 0.f; }
    const FString& GetLastLine() const { return LastLine; }
    int32 GetLinesSpoken() const { return LinesSpoken; }
    int32 GetLineCount(const TCHAR* Id) const { const int32* N = Said.Find(Id); return N ? *N : 0; }
    int32 GetLeaps() const { return Leaps; }
    int32 GetForeLandings() const { return ForeLandings; }
    int32 GetGuides() const { return GuidesShown; }
    FVector GetLastLeapTakeoff() const { return LeapFrom; }
    FVector GetLastLeapLanding() const { return LeapTo; }
    /** A line from Who (the crew's body that speaks it), Npc/Id in NPCVoiceData. False if another is being spoken. */
    bool Say(EDockHuman Who, const TCHAR* Npc, const TCHAR* Id, bool bForce = false);
    /** Back to the start (ShipGate::Reset): the yardarms loose, the fore ratlines whole, no lines said. */
    void Reset();
    /** (Dev checkpoints and tests) as if the steps before Step were done. */
    void SkipTo(EStep Step);
    /** Where the golden cigarette lies in the main top (world), and the work spots on the main yard (world). */
    static FVector LeafSpot();
    static FVector WorkSpot(int32 Side);
    /** The fore top's landing (world, his capsule's centre) and the head of the fore topmast shrouds (world, Side +1/-1). */
    static FVector ForeTopSpot(float Y = 70.f);
    static FVector ForeShroudsHead(float Side);
    // The golden leaf's long leap off the main top (2026-11-08, the plan's starting values: about 7.4 m out over a 1 m drop).
    static constexpr float LeapForward = 600.f;
    static constexpr float LeapUp = 400.f;
    static constexpr float WorkHold = 1.2f;       // s hanging still at a work spot to tie its points
    static constexpr float WorkReach = 340.f;     // cm out from the yard's middle: the work spots begin here
    static constexpr float GuideFirst = 8.f;
    static constexpr float GuideRepeat = 11.f;
    static constexpr float GuideClear = 30.f;
private:
    void TickSpeech(float Dt);
    void TickWhere(const AChuckCharacter* Chuck);
    void TickSteps(float Dt, AChuckCharacter* Chuck);
    void TickGuidance(float Dt, const AChuckCharacter* Chuck);
    void TickLeap(float Dt, AChuckCharacter* Chuck);
    void TickGuideLights(float Dt);
    void TickFlaps(float Dt);
    void TickCarried(float Dt);
    void CarryAway();
    void SpawnLeaf();
    void Queue(EDockHuman Who, const TCHAR* Npc, const TCHAR* Id, float Gap);
    void Progress() { SituationTime = 0.f; GuideSaid = 0; NextGuideIn = -1.f; }
    void SetGuides(const TArray<FVector>& At);
    ADockNPC* Body(EDockHuman Who) const;
    EStep Step = EStep::Waiting;
    EWhere Where = EWhere::Other, LastGround = EWhere::Deck;
    bool bSideDone[2] = {false, false};
    float WorkTime[2] = {0.f, 0.f};
    bool bCarried = false, bStartLines = false, bTroubleQueued = false, bCaptainCalled = false, bJumpCalled = false, bMissed = false, bForeDropped = false,
        bLeafAgain = false, bLeafSaid = false, bLanded = false, bWasOnTop = false, bLeaping = false;
    float BothDoneAt = -1.f, Clock = 0.f, QuietFor = 0.f, StormSeen = -1.f;
    int32 Leaps = 0, ForeLandings = 0, GuidesShown = 0, LinesSpoken = 0, Background = 0, Respawns = 0;
    FVector LeapFrom = FVector::ZeroVector, LeapTo = FVector::ZeroVector;
    // Speech: the line being spoken, and the scripted ones waiting.
    struct FSpoken { EDockHuman Who = EDockHuman::Count; FString Npc, Id, Text; float Seconds = 0.f, Elapsed = 0.f; };
    FSpoken Speaking;
    struct FQueued { EDockHuman Who; FString Npc, Id; float Gap; };
    TArray<FQueued> Pending;
    float PendingWait = 0.f;
    FString LastLine;
    TMap<FString, int32> Said;
    UPROPERTY() USoundAttenuation* Shout = nullptr;
    UPROPERTY() USoundAttenuation* SceneVoice = nullptr;   // (the cutscene's: unattenuated, the camera often far off)
    // Guidance: the situation he is in, how long, and what has been said there.
    int32 Situation = -1, GuideSaid = 0;
    float SituationTime = 0.f, NextGuideIn = -1.f, ClearAfter = 0.f;
    // The golden cigarette in the main top.
    TWeakObjectPtr<ACigarettePickup> Leaf;
    float LeafGone = -1.f;
    // The gold guide lights (up to four).
    UPROPERTY() TArray<UStaticMeshComponent*> GuideHalo;
    UPROPERTY() TArray<UPointLightComponent*> GuideLamp;
    UPROPERTY() TArray<UMaterialInstanceDynamic*> GuideMat;
    TArray<FVector> GuideAt;
    float GuidePhase = 0.f;
    // The loose canvas at the main yardarms, and the fore ratlines' broken ends streaming in the wind.
    UPROPERTY() UProceduralMeshComponent* Flaps = nullptr;
    float FlapRoll[2] = {0.f, 0.f}, FlapClock = 0.f;
    UPROPERTY() UProceduralMeshComponent* Strands = nullptr;
    struct FStrand { FVector Anchor; FVector Lee; float Len, Phase; };
    TArray<FStrand> StrandList;
    float CarriedClock = 0.f;
    int32 WavesSeen = 0;
};

/** He is being carried along by the ship's ending cutscene (no control, letterboxed). */
bool ShipEndingSceneLetterboxed(const UWorld* World);
