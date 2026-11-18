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
class UPointLightComponent;

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
    enum class EAttack : uint8 { Kick, Grab, Swat, Strike, Smash, Hurl, Leap, Sweep, Buffet, None };   // (Smash: the Pit Fiend's mace; Hurl: its fire at a hero, the trio's battle; 2026-11-16. Leap, Sweep, Buffet: its moves on the trio, 2026-11-17)
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
    int32 GetThrows() const { return Throws; }
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
    static constexpr float CarryLift = .45f;        // s rising out of the stoop with him, before the pitch
    static constexpr float ThrowRate = 1.15f;       // the pitch's playback, a little quicker than the man's
    static constexpr float ThrowSpeed = 640.f;      // cm/s, out (a forceful throw)
    static constexpr float ThrowUp = 400.f;         // and up
    static constexpr int32 SwatDamage = 1;

    // ---- Phlegethos (2026-11-16; docs/regions/PHLEGETHOS.md#creatures): the same rig, clips and lifecycle wearing other
    // bodies. A lemure (2D: the undead's behaviour, slowest and most durable): a melting mass of flesh a little smaller
    // than a man, slower than his walk, seventeen scratches to put down; it melts where it falls. The Pit Fiend (2D: the
    // colossus, wholly avoidable): 2.4 times a man (4.2 m), it walks a round through the wastes, notices him far off in
    // front of it, roars and strides after him (slower than his run, faster than his walk), and one blow kills him; it
    // cannot be scratched, and it gives him up at the edge of its ground.
    bool IsLemure() const { return Post.Kind == 1; }
    bool IsFiend() const { return Post.Kind == 2; }
    float KindScale() const { return IsFiend() ? FiendScale : IsLemure() ? LemureScale : 1.f; }
    float HalfHeightNow() const { return HalfHeight * KindScale(); }
    float RadiusNow() const { return Radius * KindScale(); }
    int32 GetMaxHealth() const { return IsLemure() ? LemureHealth : Health; }
    int32 GetNotices() const { return Notices; }
    float GetMelt() const { return MeltT; }
    /** The Pit Fiend's mace head, world (zero for the others). */
    FVector GetMaceHead() const;
    // The Pit Fiend's motion (2026-11-16 third pass; PitFiendMotion.cpp; the user: "more realistic leg movement and
    // placement and overall more immersive body movements such as wings beating etc ... formidable in battle"): its feet
    // set on the ground under them, a heavy bent-kneed stance that dips at each footfall, leaning into its going and its
    // turns, stepping round as it turns, wings folded on its round, half open as it comes, beating; a tail that sways,
    // trails its turns, lifts as it charges and lies along the ground; a roar reared back with its wings spread; and the
    // mace swung high over its shoulder and brought down two-handed-hard into the ground in front of it (a blow kills him,
    // the blast near it knocks him off his feet).
    int32 GetFootfalls() const { return Fm.Plants; }
    int32 GetWingBeats() const { return Fm.WingBeats; }
    int32 GetSmashes() const { return Fm.Smashes; }
    int32 GetSmashBlasts() const { return Fm.Blasts; }
    float GetWorstFootGap() const { return Fm.WorstFootGap; }
    float GetTailUnder() const { return Fm.WorstTailUnder; }
    float GetWingFold(int32 Side) const { return Animator.Fiend.WingFold[Side & 1]; }
    float GetWingRaise(int32 Side) const { return Animator.Fiend.WingRaise[Side & 1]; }
    bool IsRoaring() const { return bRoaring; }
    float GetStateTime() const { return StateTime; }
    /** Review (-ChuckPhlegethosTest=fiend_show): the roar now; the mace brought down on a world point. */
    void ShowRoar();
    void ShowSmash(const FVector& At);
    // ---- The trio's battle at the fortress (2026-11-16; FortressFiend.cpp; docs/regions/PHLEGETHOS.md#the-trios-battle):
    // a second Pit Fiend, before the breach, fights the three heroes. It brings its mace down on the fighter, hurls fire
    // at the ranger and the wizard, roars and beats its wings; arrows stand in it and the wizard's bolts burst on it, but
    // nothing brings it down. Chuck is no one's quarry here, but it swats, stamps and smashes at him if he comes under it,
    // and its blows and its fire knock him off his feet across the forecourt.
    bool IsFortressFiend() const { return IsFiend() && Battle.IsValid(); }
    int32 GetHurls() const { return Hurls; }
    int32 GetHeroBlowsTaken() const { return FortHits; }
    int32 GetFortRoars() const { return FortRoars; }
    // (2026-11-17, the user: "make the fight more intense, more animations especially from the fiend making this a cinematic
    // fight between the trio and this formidable foe".) Its leap: a crouch, its wings driving it up and over at the fighter,
    // the mace raised in the air and brought down as it lands. Its sweep: the mace swung back past its right hip and round
    // across its front at the fighter at waist height. Its buffet: reared back, wings spread wide and beaten twice at the trio,
    // the gust staggering them and throwing the lemures down.
    int32 GetLeaps() const { return Leaps; }
    int32 GetMaceSweeps() const { return MaceSweeps; }
    int32 GetBuffets() const { return Buffets; }
    /** Tests: the next blow at a hero (0 smash the fighter, 1 hurl at the wizard, 2 hurl at the ranger), whatever the distance. */
    void ForceFortress(int32 Which) { FortForced = Which; }
    static constexpr float FiendScale = 2.4f;
    static constexpr float LemureScale = .93f;
    static constexpr float LemureSpeed = 50.f;       // (2D: 15 to the skeleton's 25)
    static constexpr float FiendPatrol = 95.f;
    static constexpr float FiendChase = 170.f;
    static constexpr float FiendNotice = 2200.f;
    static constexpr float FiendClose = 800.f;
    static constexpr int32 LemureHealth = 17;        // (2D: 10 scratches to the skeleton's 6)
    static constexpr int32 FiendBlow = 5;            // all of him

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
    UPROPERTY() TArray<USoundBase*> RoarSounds;
    int32 RouteAt = 0, Notices = 0;
    float MeltT = 0.f, PitchScale = 1.f;
    bool bRoaring = false;
    // The Pit Fiend's mace (2026-11-16, the user's goal image): its head's point in the right hand's bone space, the fire
    // on it and its light.
    UPROPERTY() TObjectPtr<UPointLightComponent> MaceLight;
    UPROPERTY() TObjectPtr<UPointLightComponent> FillLight;
    int32 MaceFlame = INDEX_NONE, MaceFlame2 = INDEX_NONE, MaceBone = INDEX_NONE;
    FVector MacePrev = FVector::ZeroVector;
    FVector MaceLocal = FVector::ZeroVector;
    void TickMace();
    struct FFiendMotion
    {
        float Lift[2] = {0, 0}, Stand[2] = {1e9f, 1e9f}; FVector Normal[2] = {FVector::UpVector, FVector::UpVector}; bool bAir[2] = {false, false};
        float Dip = 0, DipVel = 0, Lean = 0, LeanAdd = 0, Roll = 0, YawRate = 0, LastYaw = 0, Crouch = 0, Clock = 0;
        float Raise[2] = {0, 0}, Sweep[2] = {0, 0}, Fold[2] = {0, 0}, Finger[2] = {0, 0}, Jig = 0, JigVel = 0, Spread = 0;
        int32 Beats = 0; float BeatPhase = 0, BeatRate = .85f, NextBeat = 3; bool bDownDone = false;
        float TailYaw[8] = {}, TailPitch[8] = {}, TailFix[8] = {}, NextFlick = 5, FlickT = -1, FlickSide = 1;
        // The swing (TickSmash): the head's path in component space, the arm's and body's targets, the wings' override.
        bool bSmashInit = false, bImpact = false; FVector HeadFrom = FVector::ZeroVector, DirFrom = FVector::UpVector, Target = FVector::ZeroVector;
        FVector Hand = FVector::ZeroVector, Haft = FVector::UpVector; float HandW = 0, Twist = 0, Bend = 0, LeftArm = 0;
        float WingW = 0, WR = 0, WS = 0, WF = 0, WFi = 0, Flash = 0;
        bool bRoarBeat = false, bRoarShake = false;
        int32 Plants = 0, WingBeats = 0, Smashes = 0, Blasts = 0;
        float WorstFootGap = 0, WorstTailUnder = 0;
    } Fm;
    float MaceLen = 0.f, LastYawTick = 0.f;
    FVector ShowAim = FVector::ZeroVector;
    void TickFiend(float DeltaSeconds, float Speed);
    void TickSmash(AChuckCharacter* Chuck, float DeltaSeconds);
    void FiendPlant(int32 Foot, const FVector& At);
    void FiendImpact(const FVector& At, float Strength, bool bEmbers, float BlastRadius, float Shake);
    FVector FrameOf(const FVector& World) const;
    FVector WorldOf(const FVector& Frame) const;
    float ChaseSpeedNow() const { return IsFiend() ? FiendChase : IsLemure() ? LemureSpeed : ChaseSpeed; }
    float ReturnSpeedNow() const { return IsFiend() ? FiendPatrol * 1.2f : IsLemure() ? LemureSpeed * .9f : ReturnSpeed; }
    float NoticeNow() const { return IsFiend() ? FiendNotice : NoticeRange; }
    float CloseNow() const { return IsFiend() ? FiendClose : CloseNotice; }
    FTempleSkeletonPost Post;
    FSkeletonAnimator Animator;
    FVector BodyForward = FVector::ForwardVector;   // the mesh's facing in component space
    EState State = EState::Dormant;
    EAttack Attack = EAttack::None, Forced = EAttack::None, LastAttack = EAttack::None;
    float AttackReadyAt = 0;   // world time it may strike again (a pause after each attack)
    float StateTime = 0, Lost = 0, OutOfReach = 0, StepDistance = 0, TopSpeed = 0, WorstLeash = 0, LastStrikeGap = 1e9f, StuckTime = 0;
    int32 HitsTaken = 0, Strikes = 0, Rises = 0, Settles = 0, SoundsPlayed = 0;
    int32 Attacks[9] = {}, Landed[9] = {};
    TWeakObjectPtr<ATempleSanctumBattle> Battle;
    TWeakObjectPtr<ATempleHero> Foe;
    UPROPERTY() TArray<UStaticMeshComponent*> StuckArrows;
    bool bHeadless = false, bDust = false;
    int32 ArrowsIn = 0;
    float HitStopUntil = 0.f, StaggerTime = 0.f, SlotAngle = 0.f;
    FVector StaggerVel = FVector::ZeroVector;
    void TickSanctum(float DeltaSeconds);
    bool TickFortress(float DeltaSeconds, float LocoSpeed);   // false: a blow at Chuck is under way (the ordinary attack runs it)
    void TickHurl(AChuckCharacter* Chuck, float DeltaSeconds);
    void TickLeap(AChuckCharacter* Chuck, float DeltaSeconds);
    void TickSweep(AChuckCharacter* Chuck, float DeltaSeconds);
    void TickBuffet(AChuckCharacter* Chuck, float DeltaSeconds);
    bool TickFortressAttack(AChuckCharacter* Chuck, float DeltaSeconds);   // a fortress blow under way: run it (true)
    int32 FortBlow = -1, Leaps = 0, MaceSweeps = 0, Buffets = 0, BuffetBeats0 = 0;
    FVector LeapGoal = FVector::ZeroVector;
    float LeapLandT = -1.f;
    bool bLaunched = false;
    void FortRoar();
    void FiendStruck(EBlow Kind, const FVector& From, const FVector& At);
    bool bOnChuck = false, bLoosed = false, bGathering = false, bChoseBlow = false, bWantSmash = false;
    TWeakObjectPtr<ATempleHero> HurlAt;
    int32 Hurls = 0, FortHits = 0, FortRoars = 0, HitsSinceRoar = 0, FortForced = -1, NextHurl = 0;
    float NextRoarAt = 0.f, NextFlareAt = 0.f, WalkUntil = 0.f;
    FVector WalkGoal = FVector::ZeroVector;
    void Fall(EBlow Kind, const FVector& From);
    bool bStruck = false, bSwung = false, bHeld = false;
    // The stoop's grab picks him up and throws him (user 2026-11-12: was a claw along the floor).
    bool bCarrying = false, bPitching = false; float CarryClock = 0.f; int32 Throws = 0;
    FVector ThrowDir = FVector::ForwardVector;
    void TickCarry(AChuckCharacter* Chuck, float DeltaSeconds);
    void ThrowChuck(AChuckCharacter* Chuck, float Speed);
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
