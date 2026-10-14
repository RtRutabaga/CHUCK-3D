#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ChultRaptor.generated.h"

class UAnimSequence;
class USoundBase;
class USoundAttenuation;
class AChuckCharacter;

/**
 * Chult's fast raptor (user 2026-10-14: patrolling round the brachiation trees,
 * pushing the player to climb and get through by brachiation). One of the 2D
 * game's two large, scratchable raptors (src/entities/raptor.py): about 4 m
 * nose to tail, head 1.6 m up - Chuck reaches its knee. Model and clips:
 * Quaternius's CC0 Velociraptor, rebuilt by Tools/build_raptor.py.
 *
 * On the ground it is deadly: it notices him, runs faster than his run (not
 * his sprint), and after a clear tell (a crouch into the strike) lunges and
 * bites - a roll or side jump through the tell beats it. Up in the trees -
 * on a limb, swinging from branches, on the ruin top - it cannot reach him:
 * it prowls underneath, snapping up at him, then gives up and goes back to
 * its round. It keeps to its territory (the trees, plus 15 m). Ten scratches
 * kill it, as in the 2D game; it never has to be fought.
 */
UCLASS()
class CHUCK3D_API AChultRaptor : public ACharacter
{
    GENERATED_BODY()
public:
    AChultRaptor();
    virtual void Tick(float DeltaSeconds) override;
    enum class EState : uint8 { Patrol, Alert, Chase, Windup, Lunge, Recover, Prowl, Return, Hurt, Dead };
    EState GetState() const { return State; }
    const TCHAR* GetStateName() const;
    bool IsDead() const { return State == EState::Dead; }
    int32 GetHitsTaken() const { return HitsTaken; }
    int32 GetBites() const { return Bites; }
    int32 GetLunges() const { return Lunges; }
    int32 GetSnaps() const { return Snaps; }
    float GetLastWindupSeconds() const { return LastWindup; }
    float GetTopSpeed() const { return TopSpeed; }
    int32 GetSoundsPlayed() const { return SoundsPlayed; }
    /** Struck by Chuck's slash sweeping along Swing: it flinches, or dies on the tenth. */
    void TakeSlash(const FVector& Swing);
    /** Tests: stand here facing Yaw, on its round, unhurt. */
    void ResetTo(const FVector& At, float Yaw);
    /** Can it reach him where he is (on the ground or low on a liana, not up a tree)? */
    bool CanReach(const AChuckCharacter* Chuck) const;

    // The slash's reach footprint (cm).
    static constexpr float HitRadius = 75.f;     // its legs and body reach well past its narrow capsule
    static constexpr float HitHeight = 150.f;
    // Tuning (Chuck: walk 72, run 225, sprint 380 cm/s).
    static constexpr float PatrolSpeed = 115.f;
    static constexpr float ProwlSpeed = 160.f;
    static constexpr float ReturnSpeed = 150.f;
    static constexpr float ChaseSpeed = 330.f;
    static constexpr float NoticeRange = 1600.f;     // in view
    static constexpr float CloseNotice = 550.f;      // all round
    static constexpr float LoseRange = 3200.f;
    static constexpr float LoseSightTime = 3.f;
    static constexpr float StrikeRange = 230.f;      // centre to centre, where it starts the tell
    static constexpr float WindupTime = .32f;        // the tell
    static constexpr float LungeTime = .3f;
    static constexpr float LungeSpeed = 560.f;
    static constexpr float BiteReach = 150.f;        // snout to Chuck
    static constexpr float RecoverTime = .7f;
    static constexpr float ReachHeight = 110.f;      // his feet above its own that it can still bite
    static constexpr float Patience = 9.f;           // out of reach before it gives up
    static constexpr float SnapEvery = 2.4f;
    static constexpr float Territory = 1500.f;       // beyond the canopy reserve
    static constexpr int32 BiteDamage = 2;           // of his 5 sanity
    static constexpr int32 Health = 10;              // scratches (2D: RAPTOR_SCRATCHES)

    static const TArray<TWeakObjectPtr<AChultRaptor>>& All();
    /** The canopy's raptor, on its round (once; not in the traversal tests). */
    static AChultRaptor* SpawnCanopyRaptor(UWorld* World);
    /** Its round round the trees (local to the Chult floor origin). */
    static const TArray<FVector>& PatrolPoints();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TMap<FName, UAnimSequence*> Clips;   // hard references (set in the constructor), so the cooker packages them
    UPROPERTY() USoundAttenuation* Attenuation = nullptr;
    UPROPERTY() TArray<USoundBase*> StepSounds;
    UPROPERTY() TArray<USoundBase*> HissSounds;
    UPROPERTY() TArray<USoundBase*> SnapSounds;
    UPROPERTY() TArray<USoundBase*> CallSounds;
    UPROPERTY() TArray<USoundBase*> DeathSounds;
    float StepDistance = 0;
    int32 SoundsPlayed = 0;
    void Sound(const TArray<USoundBase*>& Set, float Volume);
    EState State = EState::Patrol;
    float StateTime = 0, SightLost = 0, OutOfReach = 0, NoticeCooldown = 0, NextSnap = 0, StuckTime = 0, ProwlAngle = 0;
    float LastWindup = 0, TopSpeed = 0;
    int32 Waypoint = 1, WaypointStep = 1, HitsTaken = 0, Bites = 0, Lunges = 0, Snaps = 0;
    bool bBit = false, bSnapping = false;
    FVector LungeDir = FVector::ForwardVector, Knock = FVector::ZeroVector;
    FName Playing;
    void SetState(EState To);
    void Play(FName Clip, bool bLoop, float Rate, bool bRestart = false);
    void Locomotion();
    void Face(const FVector& Dir, float DegPerSecond, float DeltaSeconds);
    FVector Steer(const FVector& Want, const AActor* Ignore) const;
    bool Sees(const AChuckCharacter* Chuck) const;
    bool InTerritory(const FVector& World) const;
    float FeetZ() const;
};

/** -ChuckRaptorTest: the raptor's round, chase, tell and bite, a dodge, escaping up a tree and
 *  across a chain, its territory, a death and the ruin-top return, and the ten scratches; then quit.
 *  -ChuckRaptorCapture also saves frames (Saved/Screenshots/.../Raptor). */
void StartChultRaptorReview(UWorld* World);
