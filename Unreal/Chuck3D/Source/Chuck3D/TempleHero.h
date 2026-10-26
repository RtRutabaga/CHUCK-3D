#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkeletonAnim.h"
#include "TempleBeholder.h"
#include "TempleHero.generated.h"

class UAnimSequence;
class UAudioComponent;
class UCapsuleComponent;
class UMaterialInterface;
class UPointLightComponent;
class UProceduralMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USoundAttenuation;
class USoundBase;
class ATempleSanctumBattle;
class ATempleSkeleton;

/**
 * The three adventurers of the temple sanctum (user 2026-10-22: "create the three heroes from the
 * 2d game and add them to the grand temple room at the end ... the feeling of a good hit";
 * docs/CHULT-TEMPLE-HEROES.md). The 2D game's male fighter, wizard and clearly female ranger
 * (CHUCK-game PHASE-6): it is their battle, not Chuck's. They pay him no attention.
 *
 * Their clips are authored motion (Tools/build_hero_anims.py), played through the skeletons' clip
 * player (FSkeletonAnimator: cross-fades, a look layer, a flinch with a twist, hit-stop).
 *
 *  - The fighter holds his ground before the dais in a shield guard. A skeleton that comes into his
 *    measure is met with one of his cuts (a descending diagonal, a level cut at the neck that takes
 *    the skull off, a heavy overhead cleave) or, if it is right on him, a shield bash. The cut lands
 *    only if the blade itself meets the bones at the measured moment of contact (the blade's
 *    segment against the skeleton's spine and skull, swept between frames), and then both freeze
 *    for a few hundredths of a second (hit-stop) while the bone cracks, the blade's trail hangs in
 *    the air and splinters and dust fly. Blows that reach him are taken on the shield (he rocks
 *    back over his back foot) unless he is mid-cut or turned away (then he is hit, and reels).
 *  - The ranger shoots: up, draw to the jaw, loose, the next arrow from her quiver; the arrows fly
 *    and stand in the bone they hit.
 *  - The wizard works his binding at the dais (where the beholder will hang), and now and then
 *    turns a bolt on a skeleton that comes for one of them.
 */
UCLASS()
class CHUCK3D_API ATempleHero : public AActor
{
    GENERATED_BODY()
public:
    enum class ERole : uint8 { Fighter, Wizard, Ranger };
    enum class EAct : uint8 { Idle, Attack, React, Step };
    ATempleHero();
    virtual void Tick(float DeltaSeconds) override;

    /** The battle spawns each hero with its assets (hard references in the battle, so they cook). */
    void Setup(ERole InRole, ATempleSanctumBattle* InBattle, USkeletalMesh* Mesh, const TMap<FName, UAnimSequence*>& InClips);
    ERole GetRole() const { return Role; }
    const TCHAR* GetRoleName() const;
    EAct GetAct() const { return Act; }
    FName GetClip() const { return Animator.Playing(); }
    USkeletalMeshComponent* GetMesh() const { return Mesh; }
    FVector HeadLocation() const;
    /** Distance from Point to his body (a capsule round his spine; < 0 inside). */
    float GapTo(const FVector& Point) const;
    /** A skeleton's blow reaches him at At: blocked on the shield (true) or taken (false). */
    bool TakeSkeletonBlow(ATempleSkeleton* From, const FVector& At, bool bHeavy);
    // The beholder's attacks on him (docs/CHULT-TEMPLE-BEHOLDER.md#the-fight).
    /** An eye ray reaches him at InOutAt from From: true if it struck him; false if the fighter took it on his shield (InOutAt: the shield). */
    bool TakeBeholderRay(EBeholderRay Kind, const FVector& From, FVector& InOutAt);
    /** A ray loosed at him that he evades: he twists or lunges out of its way. */
    void EvadeBeholderRay(const FVector& From, const FVector& Past);
    /** The bite: true if it was caught on the shield. */
    bool TakeBeholderBite(const FVector& From, const FVector& At);
    /** The central eye's cone: thrown back; the wizard's binding is snuffed a while. */
    void TakeBeholderCone(const FVector& Apex, const FVector& Axis);
    /** The fighter answers a bite caught on his shield: a rising cut at its face. */
    void CounterBeholder(ATempleBeholder* Beholder);
    bool IsSuppressed() const;
    /** The wizard: the sea tear has surged against his seal; he is wrenched toward it (SealStrain). */
    void SealSurge();
    /** The wizard is working the seal now (in his Seal or SealStrain clip, nothing else on hand). */
    bool IsSealing() const;
    int32 GetStrains() const { return Strains; }
    bool IsParalysed() const;
    /** Speaking: the jaw open this many degrees (the line's loudness, TempleHeroTalk.cpp). */
    void SetSpeech(float JawDegrees) { Animator.SetJaw(JawDegrees); }
    float GetJaw() const { return Animator.GetJaw(); }
    int32 GetBeholderCuts() const { return BeholderCuts; }
    /** Tests: the next attack is this clip (the fighter), and hold still otherwise. */
    void ForceClip(FName Clip) { Forced = Clip; }
    void SetHeld(bool bInHeld) { bHeld = bInHeld; }
    /** A cutscene's hold (TempleSanctumScene.cpp): out of his fight, playing Clip, turning to face Toward (zero: as he is). */
    void Script(FName Clip, bool bLoop, float Blend, const FVector& Toward);
    /** Back to his fight. */
    void EndScript();
    bool IsScripted() const { return bScripted; }
    void SetHome(const FVector& Feet, float Yaw) { Home = Feet; HomeYaw = Yaw; }
    /** Tests: straight back into the rest clip (guard, channel, ready). */
    void Rest() { BackToRest(); }
    float GetClipTime() const { return Animator.Time(); }
    /** The moment of contact of the blow he is in (clip seconds), or -1. */
    float GetContactTime() const;

    int32 GetSwings() const { return Swings; }
    int32 GetCleanHits() const { return CleanHits; }
    int32 GetBlocks() const { return Blocks; }
    int32 GetHitsTaken() const { return HitsTaken; }
    int32 GetShots() const { return Shots; }
    float GetLastHitStop() const { return LastHitStop; }
    float GetLastBladeSpeed() const { return LastBladeSpeed; }
    FName GetLastCut() const { return LastCut; }
    int32 GetAnimEvaluations() const;
    FVector BladeTip() const;
    FVector BladeBase() const;

    static constexpr float Radius = 34.f;
    static constexpr float HalfHeight = 92.f;
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY(VisibleAnywhere) UCapsuleComponent* Capsule = nullptr;
    UPROPERTY(VisibleAnywhere) USkeletalMeshComponent* Mesh = nullptr;
    UPROPERTY() UProceduralMeshComponent* Trail = nullptr;
    UPROPERTY() UPointLightComponent* GemLight = nullptr;
    UPROPERTY() UAudioComponent* Hum = nullptr;
    UPROPERTY() TMap<FName, UAnimSequence*> Clips;
    TWeakObjectPtr<ATempleSanctumBattle> Battle;
    FSkeletonAnimator Animator;
    ERole Role = ERole::Fighter;
    EAct Act = EAct::Idle;
    FName Forced = NAME_None, LastCut = NAME_None;
    FVector Home = FVector::ZeroVector, BodyForward = FVector::ForwardVector;
    float HomeYaw = 0, ActTime = 0, NextAt = 0, HitStopUntil = 0, LastHitStop = 0, LastBladeSpeed = 0, StepStart = 0;
    bool bScripted = false; FVector ScriptFace = FVector::ZeroVector;
    bool bHeld = false, bWhoosh = false, bContact = false, bFired = false, bDrawSound = false, bStepSound[3] = {false, false, false};
    int32 Swings = 0, CleanHits = 0, Blocks = 0, HitsTaken = 0, Shots = 0;
    TWeakObjectPtr<ATempleSkeleton> Target;
    TArray<TWeakObjectPtr<ATempleSkeleton>> StruckThisSwing;
    FVector PrevBase = FVector::ZeroVector, PrevTip = FVector::ZeroVector;
    float PrevClipT = 0.f, BonesClipT = 0.f;
    FName BonesClip = NAME_None;
    FVector ContactBase = FVector::ZeroVector, ContactTip = FVector::ZeroVector;
    struct FTrailSample { FVector Base, Tip; float Time; };
    TArray<FTrailSample> TrailSamples;
    FRandomStream Random;

    void Play(FName Clip, bool bLoop, float Blend = .2f);
    void Face(const FVector& Dir, float DegPerSecond, float DeltaSeconds);
    void TickFighter(float DeltaSeconds);
    void TickWizard(float DeltaSeconds);
    void TickRanger(float DeltaSeconds);
    void FighterContact(float DeltaSeconds);
    void UpdateTrail(bool bLive);
    void BackToRest();
    void Shove(const FVector& Velocity, float Seconds);
    void TickShove(float DeltaSeconds);
    void BeholderContact();
    FVector ShoveVel = FVector::ZeroVector;
    float ShoveStart = 0, ShoveUntil = 0, SuppressUntil = 0, ParalysedUntil = 0, NextBeholderShot = 0;
    TWeakObjectPtr<ATempleBeholder> CounterOn;
    bool bCounterHit = false;
    float CounterAt = 0;
    int32 BeholderCuts = 0, Strains = 0;
    ATempleSkeleton* NearestFoe(float Within, float Cone) const;
};
