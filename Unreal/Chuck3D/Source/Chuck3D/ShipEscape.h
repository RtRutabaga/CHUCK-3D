#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShipEscape.generated.h"

class ACameraActor;
class AChuckCharacter;

/**
 * The escape from the temple onto the ship (Claude, 2026-11-01; docs/SHIP.md#the-escape), the 2D game's Phase 6 escape
 * cutscene: "Show Chuck crawling through a tight stone passage. A light appears ahead. Chuck emerges into a wooden room.
 * A hole reveals the open sea. He has unknowingly reached the deck of a ship."
 *
 * Watches for him at the end of the rubble map's crevice (AtChultTempleCreviceEnd). Then, with no control and letterboxed
 * (Esc skips): black; the ship's stage is built (AShipSetting::Enter) and he is crawling on along a stone passage toward
 * warm light - the crawlway through the temple masonry grown into the ship's arrival compartment; he comes out of it into
 * lamplight among sailcloth and timber and gets up; he goes to the broken planking in the hull and the open sea is there.
 * Control returns where he stands. It plays once; a respawn aboard never replays it.
 */
UCLASS()
class CHUCK3D_API AShipEscape : public AActor
{
    GENERATED_BODY()
public:
    AShipEscape();
    virtual void Tick(float DeltaSeconds) override;
    /** Spawned with Chult (AChultLanding::Build): watches the crevice's end. Idempotent. */
    static AShipEscape* Watch(UWorld* World);
    static AShipEscape* Get(const UWorld* World);
    /** Begin now (he is at the crevice's end; tests may call it directly). */
    void Start();
    void Skip();
    bool IsPlaying() const { return bPlaying; }
    bool IsLetterboxed() const { return bPlaying && !bReleased; }
    static bool IsLetterboxed(const UWorld* World);
    // Evidence.
    int32 GetPlays() const { return Plays; }
    int32 GetSkips() const { return Skips; }
    int32 GetFinishes() const { return Finishes; }
    float GetClock() const { return Clock; }
    const TCHAR* GetShotName() const { return Shot; }
    bool HasCrawledOut() const { return bOut; }

    // Beats (s from the start).
    static constexpr float BlackAt = .7f;       // black by then
    static constexpr float StageAt = .9f;       // the ship is built and he is in the passage
    static constexpr float FadeInTime = 1.4f;
    static constexpr float SeaHold = 3.2f;      // on the open sea before control returns
private:
    TWeakObjectPtr<AChuckCharacter> Chuck;
    UPROPERTY() ACameraActor* Camera = nullptr;
    bool bPlaying = false, bReleased = false, bStaged = false, bOut = false, bAtHole = false, bSkipping = false, bDone = false;
    int32 Plays = 0, Skips = 0, Finishes = 0;
    float Clock = 0, OutAt = -1, HoleAt = -1, Watch_ = 0;
    const TCHAR* Shot = TEXT("");
    FVector Eye = FVector::ZeroVector, Aim = FVector::ZeroVector;
    bool bCut = true;
    void Stage();
    void Release();
    void Finish();
    void Frame(const FVector& InEye, const FVector& InAim, const TCHAR* Name, float Fov = 55.f);
};
