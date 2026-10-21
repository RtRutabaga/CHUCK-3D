#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChultTempleInterior.h"
#include "TempleSnake.generated.h"

class UPoseableMeshComponent;
class USoundBase;
class USoundAttenuation;
class AChuckCharacter;

/**
 * The temple snakes (user 2026-10-21: "Create and add the snakes enemies";
 * docs/CHULT-TEMPLE-SNAKES.md). The 2D game's Phase 6 snake chamber: twenty
 * small, quick snakes that one scratch kills ("briefly lets the player feel
 * powerful"). A 1.8 m jungle pit viper (Tools/build_temple_snake.py), posed
 * bone by bone along the path its snout has taken, as a real snake moves:
 * - lateral undulation: the snout weaves about its heading, every point of the
 *   body passes along the same track behind it, so nothing slides sideways and
 *   it uncoils from rest by crawling out of its own coil;
 * - the tell: it stops, draws its front third up into an S, head raised and
 *   turned on the rat, hissing, mouth a little open;
 * - the strike: the S straightens and the head shoots out about 40 cm, jaws
 *   gaping, then draws back into the S. It bites only if its snout reaches him.
 * - one scratch anywhere along it kills it: it thrashes, rolls half over and
 *   lies still, mouth open.
 * It keeps to the chamber, and goes back to its place when he leaves.
 */
UCLASS()
class CHUCK3D_API ATempleSnake : public AActor
{
    GENERATED_BODY()
public:
    ATempleSnake();
    virtual void Tick(float DeltaSeconds) override;
    enum class EState : uint8 { Rest, Wander, Alert, Chase, Rear, Strike, Recoil, Return, Dead };
    EState GetState() const { return State; }
    const TCHAR* GetStateName() const;
    bool IsDead() const { return State == EState::Dead; }
    const FTempleSnakePost& GetPost() const { return Post; }

    /** Chuck's scratch from Location facing Ahead with Reach (cm): true if any part of it is in reach (and, Kill, it dies). */
    bool Scratch(const FVector& Location, const FVector& Ahead, float Reach, const FVector& Swing, bool bKill);
    /** Tests: back to its post, coiled, alive. */
    void ResetToPost();
    void SetHeld(bool bInHeld) { bHeld = bInHeld; }

    /** The snout tip (world). */
    FVector SnoutTip() const { return Snout; }
    /** World points along the body, snout first (last pose). */
    const TArray<FVector>& BodyPoints() const { return Points; }
    float GetTopSpeed() const { return TopSpeed; }
    int32 GetStrikes() const { return Strikes; }
    int32 GetBites() const { return Bites; }
    float GetLastTell() const { return LastTell; }
    float GetLastStrikeGap() const { return LastStrikeGap; }
    float GetWorstLeash() const { return WorstLeash; }
    int32 GetSoundsPlayed() const { return SoundsPlayed; }
    /** How far the mid-body moves sideways to its own length, on average while crawling (0: it follows its track exactly). */
    float GetSideSlip() const { return SlipSamples ? SlipSum / SlipSamples : 0.f; }
    int32 GetSlipSamples() const { return SlipSamples; }
    float GetGape() const { return Gape; }
    float GetRear() const { return RearW; }

    static constexpr float Length = 180.f;        // snout to tail tip (cm)
    static constexpr float Front = 75.f;          // the length drawn up into the S
    static constexpr float WanderSpeed = 28.f;
    static constexpr float ChaseSpeed = 95.f;     // over his walk (72), well under his run (225); the 2D snake 32 to his 80
    static constexpr float NoticeRange = 450.f;   // it feels him coming, all round (2D: 6 tiles)
    static constexpr float StrikeStart = 80.f;    // the S's base (where the front leaves the floor) to him: it rears
    static constexpr float Ready = 24.f;          // the raised head ahead of the S's base
    static constexpr float TellTime = .38f;
    static constexpr float StrikeOut = .1f;
    static constexpr float StrikeHold = .08f;
    static constexpr float RecoilTime = .35f;
    static constexpr float Reach = 62.f;          // the furthest the head shoots out (each strike as far as he is, up to this)
    static constexpr float BiteSlack = 6.f;       // the snout within this of his body
    static constexpr int32 BiteDamage = 1;        // of his 5 (2D: 10 of 100)

    static const TArray<TWeakObjectPtr<ATempleSnake>>& All();
    static ATempleSnake* SpawnAt(UWorld* World, const FTempleSnakePost& Post);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() UPoseableMeshComponent* Body = nullptr;
    UPROPERTY() USoundAttenuation* Attenuation = nullptr;
    UPROPERTY() TArray<USoundBase*> HissSounds;
    UPROPERTY() TArray<USoundBase*> StrikeSounds;
    UPROPERTY() TArray<USoundBase*> SlitherSounds;
    UPROPERTY() TArray<USoundBase*> DeathSounds;
    FTempleSnakePost Post;
    EState State = EState::Rest;
    float StateTime = 0, Phase = 0, Heading = 0, GroundZ = 0, RearW = 0, Ext = 0, Gape = 0, Flick = 0, FlickClock = 0, NextFlick = 1.f;
    float StrikeReach = 40.f;   // this strike's reach
    float DeathT = -1, Cooldown = 0, Lost = 0, SlitherDistance = 0, TopSpeed = 0, LastTell = 0, LastStrikeGap = 1e9f, WorstLeash = 0, SlipSum = 0;
    int32 Strikes = 0, Bites = 0, SoundsPlayed = 0, SlipSamples = 0;
    bool bBit = false, bHeld = false, bPosed = false;
    FVector HeadPos = FVector::ZeroVector, Aim = FVector::ForwardVector, WanderGoal = FVector::ZeroVector, Swung = FVector::ZeroVector, LastMid = FVector::ZeroVector;
    FVector Snout = FVector::ZeroVector;
    TArray<FVector> Trail;      // ground points behind the snout, Spacing apart (Trail[0] within Spacing of HeadPos)
    TArray<FVector> Points;     // last posed body points (snout first, every 6 cm)
    FRandomStream Random;
    // The rig: each pose bone's reference transform in component space and its arc from the snout.
    struct FBoneRef { FName Name; int32 Index = INDEX_NONE; FTransform Ref; float Arc = 0; int32 Kind = 0; };   // Kind 0 spine, 1 head, 2 jaw, 3 tongue, 4 other
    TArray<FBoneRef> Bones;
    FTransform RefHead, RefJaw, RefTongue;
    void Coil();
    void Advance(const FVector& Delta);
    FVector TrailPoint(float Arc) const;
    FVector BodyPoint(float Arc) const;
    void PostureCurve(TArray<FVector>& Out) const;
    TArray<FVector> Curve;      // the S: points from the snout (0) to the base of the front (Front), 1 cm apart
    void Pose();
    void Crawl(const FVector& Goal, float Speed, float DeltaSeconds);
    bool Blocked(const FVector& From, const FVector& To) const;
    void Sound(const TArray<USoundBase*>& Set, float Volume);
    void SetState(EState To);
    bool InLeash(const FVector& World, float Margin = 0) const;
};

/** Spawns the chamber's twenty snakes once (not in other automated runs). Called by BuildChultTempleInterior. */
void SpawnTempleSnakes(UWorld* World);
/** -ChuckSnakeTest: the twenty in place, the chase and its crawl, a strike landing, a roll and a step back beating it,
 *  one scratch killing, the chamber keeping them. -ChuckSnakeCapture also saves frames (Saved/Screenshots/.../Snake). */
void StartTempleSnakeReview(UWorld* World);
