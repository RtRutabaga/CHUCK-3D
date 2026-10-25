#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ChultTempleInterior.h"
#include "SkeletonAnim.h"
#include "TempleSkeleton.generated.h"

class UAnimSequence;
class USoundBase;
class USoundAttenuation;
class AChuckCharacter;
class ATempleHero;
class ATempleSanctumBattle;
class UStaticMeshComponent;
class UStaticMesh;

/**
 * The temple skeletons (user 2026-10-21: "create the skeleton enemies in temple,
 * use free resources where helpful, make them look good and have realistic attack
 * motions"; docs/CHULT-TEMPLE-SKELETONS.md). The 2D game's durable primary enemy
 * of the temple (Phase 6): human-sized, unarmed, faster than a zombie, fewer
 * scratches to put down (2D: 25 vs 18 speed, 6 vs 8 scratches, 15 vs 20 damage).
 *
 * A realistic human skeleton (Blender Studio's CC0 model) on its own rig, moved
 * by CMU motion capture: a crouched, ready idle; a stiff, purposeful walk; three
 * attacks a person would really use on something rat-sized at their feet - a
 * low front kick that stamps down after it, a stoop with a hand clawing along
 * the floor, and a swat down at its own feet. Each lands only if the striking
 * foot or hand actually meets the rat at the moment of the strike (bone
 * positions, not a range check): it turns to aim the strike while winding up,
 * then commits, so stepping away, rolling or getting behind it beats it.
 *
 * Some lie among the bones on the floor until he comes near, then get up; the
 * rest stand guard. Each is kept to its own space (a gap between spike bands,
 * one side of the rift): it stops at the edge rather than follow. It never
 * reaches him hanging from the rings. Ten scratches bring it down: its knees go
 * and it drops flat on its back.
 */
UCLASS()
class CHUCK3D_API ATempleSkeleton : public ACharacter
{
    GENERATED_BODY()
public:
    ATempleSkeleton();
    virtual void Tick(float DeltaSeconds) override;
    enum class EState : uint8 { Dormant, Rise, Guard, Chase, Attack, Recover, Return, Settle, Dead };
    enum class EAttack : uint8 { Kick, Grab, Swat, Strike, None };
    /** How a hero's blow lands (docs/CHULT-TEMPLE-HEROES.md). */
    enum class EBlow : uint8 { Cut, Cleave, Decapitate, Bash, Arrow, Bolt };
    EState GetState() const { return State; }
    const TCHAR* GetStateName() const;
    static const TCHAR* AttackName(EAttack Kind);
    bool IsDead() const { return State == EState::Dead; }
    const FTempleSkeletonPost& GetPost() const { return Post; }

    /** Struck by Chuck's slash sweeping along Swing: a flinch (it wakes if lying), or on the tenth, it falls (and stays down). */
    void TakeSlash(const FVector& Swing);
    /** Tests: back to its post as it began (lying or standing), unhurt. */
    void ResetToPost();
    /** Tests: the next attack is this one whatever the distance (None: chosen by distance). */
    void ForceAttack(EAttack Kind) { Forced = Kind; }
    /** Tests: held where it is (its pose kept, no notice, no attack), so a trial can watch one at a time. */
    void SetHeld(bool bInHeld) { bHeld = bInHeld; }
    /** Its own frame position inside its leash, and how far outside it the capsule's centre has ever been (cm). */
    bool InLeash(const FVector& World, float Margin = 0.f) const;
    float GetWorstLeash() const { return WorstLeash; }
    /** Can it strike him where he is (on the floor of its space, not hanging from a ring)? */
    bool CanReach(const AChuckCharacter* Chuck) const;

    int32 GetHitsTaken() const { return HitsTaken; }
    int32 GetAttacks(EAttack Kind) const { return Attacks[static_cast<int32>(Kind)]; }
    int32 GetLanded(EAttack Kind) const { return Landed[static_cast<int32>(Kind)]; }

    // ---- The sanctum (2026-10-22): skeletons that fight the three heroes, not Chuck. They lie in
    // reserve among the bones until the battle wakes them, go for the hero it gives them, strike at
    // him with a fist (Strike), and go down to the heroes' blows: a cut or a cleave drops them, a
    // level cut takes the skull off first, the shield knocks them back, the second arrow or one bolt
    // drops them. The battle puts the fallen back in reserve, out of sight, for the next wave.
    void SetSanctum(ATempleSanctumBattle* InBattle);
    bool IsSanctum() const { return Battle.IsValid(); }
    void SetFoe(ATempleHero* Hero);
    ATempleHero* GetFoe() const;
    /** A dormant sanctum skeleton gets up and goes for its foe. */
    void Wake();
    bool IsUp() const { return State != EState::Dormant && State != EState::Dead && State != EState::Settle; }
    /** On its feet (not still getting up off the floor): what the heroes fight. */
    bool IsStanding() const { return State == EState::Chase || State == EState::Attack || State == EState::Recover; }
    bool IsStriking() const { return State == EState::Attack; }
    float StrikeIn() const;   // seconds to its blow landing (attacking), else a large number
    /** A hero's blow: From is where it came from, At where it landed. True if it fell. */
    bool TakeHeroBlow(EBlow Kind, const FVector& From, const FVector& At);
    /** The hero's blow (or a block) has frozen it: hit-stop for Seconds. */
    void HitStop(float Seconds);
    float GetDeadTime() const { return State == EState::Dead ? StateTime : 0.f; }
    bool IsHeadless() const { return bHeadless; }
    int32 GetArrowsIn() const { return ArrowsIn; }
    /** Back to a reserve post, lying among the bones, whole (its skull back, the arrows out). */
    void ResetToReserve(const FTempleSkeletonPost& InPost);
    /** The beholder's disintegration ray (docs/CHULT-TEMPLE-BEHOLDER.md): struck down and gone to dust (hidden until it is
     *  back in reserve). */
    void Disintegrate(const FVector& From);
    bool IsDust() const { return bDust; }
    /** The striking hand's world position (the Strike). */
    FVector FistTip() const { return StrikingTip(EAttack::Strike); }
    /** Its skull's world transform (for the struck-off skull to fly from). */
    FTransform SkullTransform() const;
    /** An arrow that struck it, left standing in the nearest bone. */
    void StickArrow(UStaticMesh* Arrow, const FVector& At, const FVector& Dir);
    int32 GetStrikes() const { return Strikes; }
    int32 GetRises() const { return Rises; }
    int32 GetSettles() const { return Settles; }
    int32 GetSoundsPlayed() const { return SoundsPlayed; }
    float GetTopSpeed() const { return TopSpeed; }
    /** The striking end's nearest approach to his body at the last strike (cm beyond his capsule; < 0: inside). */
    float GetLastStrikeGap() const { return LastStrikeGap; }
    EAttack GetLastAttack() const { return LastAttack; }
    int32 GetAnimEvaluations() const;
    float GetJaw() const { return Animator.GetJaw(); }
    /** World position of the striking end of an attack now (toe or middle fingertip). */
    FVector StrikingTip(EAttack Kind) const;

    static constexpr float Radius = 34.f;   // wider than its bones at rest: a crouch or a kick must not swallow the rat
    static constexpr float HalfHeight = 88.f;
    // Tuning. Chuck: walk 72, run 225, sprint 380 cm/s; 65 cm tall, 5 sanity.
    static constexpr float ChaseSpeed = 82.f;        // a little over his walk: he outruns it, not outwalks it (2D: 25 to his 80 px/s)
    static constexpr float ReturnSpeed = 70.f;
    static constexpr float WakeRange = 380.f;        // a lying one gets up when he comes this close
    static constexpr float NoticeRange = 750.f;      // a standing one, in view
    static constexpr float CloseNotice = 300.f;      // all round
    static constexpr float LoseTime = 2.5f;          // out of its space or out of sight this long: back to its post
    static constexpr float Patience = 6.f;           // out of reach (on the rings) this long: back to its post
    static constexpr float RiseRate = 1.35f;         // the get-up, about 3.1 s
    static constexpr float SettleRate = -1.8f;       // lying back down at its post
    static constexpr float CollapseRate = 1.8f;      // the fall when it is destroyed (as the zombie's)
    static constexpr float HitSlack = 12.f;          // the striking end within this of his body
    static constexpr float PauseMin = .7f;           // between attacks it stands off and watches this long (s), up to PauseMax
    static constexpr float PauseMax = 1.5f;
    static constexpr int32 Health = 10;              // scratches (2D: SKELETON_SCRATCHES 6 vs the zombie's 8; the 3D zombie takes 14)
    static constexpr int32 KickDamage = 2;
    static constexpr int32 GrabDamage = 1;
    static constexpr int32 SwatDamage = 1;

    static const TArray<TWeakObjectPtr<ATempleSkeleton>>& All();
    static ATempleSkeleton* SpawnAt(UWorld* World, const FTempleSkeletonPost& Post);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TMap<FName, UAnimSequence*> Clips;   // hard references, so the cooker packages them
    UPROPERTY() USoundAttenuation* Attenuation = nullptr;
    UPROPERTY() TArray<USoundBase*> StepSounds;
    UPROPERTY() TArray<USoundBase*> RattleSounds;
    UPROPERTY() TArray<USoundBase*> SwingSounds;
    UPROPERTY() TArray<USoundBase*> HitSounds;
    UPROPERTY() TArray<USoundBase*> CollapseSounds;
    UPROPERTY() TArray<USoundBase*> RiseSounds;
    FTempleSkeletonPost Post;
    FSkeletonAnimator Animator;
    FVector BodyForward = FVector::ForwardVector;   // the mesh's facing in component space
    EState State = EState::Dormant;
    EAttack Attack = EAttack::None, Forced = EAttack::None, LastAttack = EAttack::None;
    float AttackReadyAt = 0;   // world time it may strike again (a pause after each attack)
    float StateTime = 0, Lost = 0, OutOfReach = 0, StepDistance = 0, TopSpeed = 0, WorstLeash = 0, LastStrikeGap = 1e9f, StuckTime = 0;
    int32 HitsTaken = 0, Strikes = 0, Rises = 0, Settles = 0, SoundsPlayed = 0;
    int32 Attacks[4] = {0, 0, 0, 0}, Landed[4] = {0, 0, 0, 0};
    TWeakObjectPtr<ATempleSanctumBattle> Battle;
    TWeakObjectPtr<ATempleHero> Foe;
    UPROPERTY() TArray<UStaticMeshComponent*> StuckArrows;
    bool bHeadless = false, bDust = false;
    int32 ArrowsIn = 0;
    float HitStopUntil = 0.f, StaggerTime = 0.f, SlotAngle = 0.f;
    FVector StaggerVel = FVector::ZeroVector;
    void TickSanctum(float DeltaSeconds);
    void Fall(EBlow Kind, const FVector& From);
    bool bStruck = false, bSwung = false, bHeld = false;
    FRandomStream Random;
    FVector LastLocation = FVector::ZeroVector;
    void SetState(EState To);
    void Sound(const TArray<USoundBase*>& Set, float Volume);
    void Face(const FVector& Dir, float DegPerSecond, float DeltaSeconds);
    FVector Steer(const FVector& Want, const AActor* Ignore) const;
    FVector ClampToLeash(const FVector& World) const;
    bool Sees(const AChuckCharacter* Chuck) const;
    void StartAttack(EAttack Kind);
    void Die(const FVector& Swing);
    float FeetZ() const;
};

/** Spawns every post's skeleton once (not in other automated runs). Called by BuildChultTempleInterior. */
void SpawnTempleSkeletons(UWorld* World);
/** -ChuckSkeletonTest: placement, waking, each attack landing by its bones, a dodge, the leash, the rings,
 *  ten scratches and the collapse; then quit. -ChuckSkeletonCapture also saves frames (Saved/Screenshots/.../Skeleton). */
void StartTempleSkeletonReview(UWorld* World);
