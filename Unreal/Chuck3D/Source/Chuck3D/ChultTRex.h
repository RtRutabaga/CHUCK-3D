#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DinoAnim.h"
#include "ChultTRex.generated.h"

class UAnimSequence;
class USoundBase;
class USoundAttenuation;
class AChuckCharacter;

/**
 * The T-rex of the giant trees upriver (user 2026-10-15: "a t-rex that can only
 * be avoided through brachiation in the high tree canopy"; docs/CHULT-TREX.md).
 * Model and clips: Quaternius's CC0 T-Rex, rebuilt by Tools/build_trex.py - 12 m
 * nose to tail, head 5 m up; Chuck (65 cm) is shorter than its shin.
 *
 * It walks a loop round the giants. On the ground anywhere in its stretch of the
 * valley it is death: it notices him far off, roars, strides after him faster
 * than his sprint and, after a clear tell, bites once - he dies. A roll or side
 * jump through the tell can still beat one bite. Up in the giants (15-17 m) it
 * cannot reach him: it stalks beneath, growling and snapping up, then gives up.
 * It never leaves its stretch (S 35600-50400), so the far bank, the deep jungle
 * and the temple are safe - and so is the last giant's liana down - and it
 * cannot be scratched to death.
 */
UCLASS()
class CHUCK3D_API AChultTRex : public ACharacter
{
    GENERATED_BODY()
public:
    AChultTRex();
    virtual void Tick(float DeltaSeconds) override;
    enum class EState : uint8 { Patrol, Roar, Chase, Windup, Lunge, Recover, Stalk, Return };
    EState GetState() const { return State; }
    const TCHAR* GetStateName() const;
    int32 GetBites() const { return Bites; }
    int32 GetSnaps() const { return Snaps; }
    int32 GetRoars() const { return Roars; }
    int32 GetSoundsPlayed() const { return SoundsPlayed; }
    /** The planted foot's sole above the ground under it (cm), and the most since the last reset. */
    float GetFootGap() const { return Animator.FootGap(); }
    float GetMaxFootGap() const { return Animator.MaxFootGap(); }
    int32 GetFootGapSamples() const { return Animator.FootGapSamples(); }
    void ResetFootGap() { Animator.ResetFootGap(); }
    float GetTopSpeed() const { return TopSpeed; }
    /** Tests: stand here facing Yaw, on its loop. */
    void ResetTo(const FVector& At, float Yaw);
    bool CanReach(const AChuckCharacter* Chuck) const;
    static bool InTerritory(const FVector& World);

    // Tuning (Chuck: walk 72, run 225, sprint 380 cm/s).
    static constexpr float PatrolSpeed = 180.f;
    static constexpr float StalkSpeed = 220.f;
    static constexpr float ReturnSpeed = 240.f;
    static constexpr float ChaseSpeed = 420.f;       // faster than his sprint: on the ground he cannot outrun it
    static constexpr float NoticeRange = 3200.f;     // in view
    static constexpr float CloseNotice = 1300.f;     // all round (the ground shakes)
    static constexpr float LoseRange = 5500.f;
    static constexpr float LoseSightTime = 4.f;
    static constexpr float StrikeRange = 600.f;      // centre to Chuck, where it starts the tell
    static constexpr float WindupTime = .45f;        // the tell: it rears back
    static constexpr float LungeTime = .45f;
    static constexpr float LungeSpeed = 650.f;
    static constexpr float BiteReach = 260.f;        // round its snout
    static constexpr float RecoverTime = 1.1f;
    static constexpr float ReachHeight = 330.f;      // his feet above its own that it can still bite
    static constexpr float Patience = 20.f;          // out of reach before it gives up
    static constexpr float SnapEvery = 3.2f;
    static constexpr float TerritoryFrom = 35600.f;   // canyon frame S
    // Ends just short of the last giant's liana (it comes down at S~50700): the canopy's way out is safe.
    static constexpr float TerritoryTo = 50400.f;

    static const TArray<TWeakObjectPtr<AChultTRex>>& All();
    /** The giants' T-rex, on its loop (once; not in automated runs but its own test). */
    static AChultTRex* SpawnUpriver(UWorld* World);
    /** Its loop round the giants (local to the Chult floor origin). */
    static const TArray<FVector>& PatrolPoints();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TMap<FName, UAnimSequence*> Clips;   // hard references, so the cooker packages them
    UPROPERTY() USoundAttenuation* Attenuation = nullptr;
    UPROPERTY() TArray<USoundBase*> StepSounds;
    UPROPERTY() TArray<USoundBase*> RoarSounds;
    UPROPERTY() TArray<USoundBase*> SnapSounds;
    UPROPERTY() TArray<USoundBase*> GrowlSounds;
    float StepDistance = 0, StateTime = 0, SightLost = 0, OutOfReach = 0, NoticeCooldown = 0, NextSnap = 0, NextGrowl = 0, StuckTime = 0, TopSpeed = 0;
    int32 Waypoint = 0, Bites = 0, Snaps = 0, Roars = 0, SoundsPlayed = 0;
    bool bBit = false, bSnapping = false;
    FVector LungeDir = FVector::ForwardVector;
    FDinoAnimator Animator;
    EState State = EState::Patrol;
    void Sound(const TArray<USoundBase*>& Set, float Volume);
    void SetState(EState To);
    void Play(FName Clip, bool bLoop, float Rate, bool bRestart = false);
    void Locomotion();
    void Face(const FVector& Dir, float DegPerSecond, float DeltaSeconds);
    FVector Steer(const FVector& Want, const AActor* Ignore) const;
    bool Sees(const AChuckCharacter* Chuck) const;
    float FeetZ() const;
    int32 NearestPoint() const;
};

/** -ChuckTRexTest: its loop (route sweeps), a chase and the killing bite with the
 *  return to the giants' spawn, safe up in a giant while it stalks below, and it
 *  stopping at its stretch's edge; then quit. */
void StartChultTRexReview(UWorld* World);
