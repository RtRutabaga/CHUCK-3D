#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "SkeletonAnim.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

/**
 * The temple skeletons' animation (Claude, 2026-10-21; docs/CHULT-TEMPLE-SKELETONS.md).
 * Motion-capture clips (Tools/build_temple_skeleton.py) cross-faded, the walk's
 * phase advanced by the ground covered (a planted foot stays planted), and three
 * small layers on top, in component space: the head and neck turned toward the
 * rat, the jaw opened (its teeth chatter in the tell) and a flinch back when a
 * scratch lands.
 */
struct FSkeletonTrack
{
    UAnimSequence* Clip = nullptr;
    float Time = 0.f;
    bool bLoop = false;
};

/**
 * The Pit Fiend's own layers over the clips (2026-11-16, the user: "more realistic leg movement and placement and overall
 * more immersive body movements such as wings beating ... formidable in battle, not just a lumbering skeleton"). Set each
 * tick by ATempleSkeleton::TickFiend (PitFiendMotion.cpp). Lengths are cm and angles degrees in the mesh's component space
 * (the rig's own units: 2.4 times smaller than the world's). Index 0 is the left side, 1 the right.
 */
struct FFiendPose
{
    bool bOn = false;
    float FootLift[2] = {0.f, 0.f};          // each foot raised (+) or lowered to the ground under it
    FVector FootNormal[2] = {FVector::UpVector, FVector::UpVector};   // that ground's slope (the sole laid on it)
    float PelvisDrop = 0.f;                  // the hips lowered, the knees bending under them, the feet kept
    FVector PelvisShift = FVector::ZeroVector;
    float Stance = 0.f;                      // each foot set out to its own side (a wide, heavy stance)
    float Lean = 0.f, Roll = 0.f;            // the spine pitched forward, rolled to its left
    float Breath = 0.f;                      // -1..1
    float ChestTwist = 0.f, ChestBend = 0.f; // the chest turned to its left, bent forward
    float LeftArm = 0.f;                     // the left arm swung back (+) or forward
    float WingRaise[2] = {0, 0}, WingSweep[2] = {0, 0}, WingFold[2] = {0, 0}, WingFinger[2] = {0, 0};
    float TailYaw[8] = {}, TailPitch[8] = {};// each tail bone turned to its left, raised
    // The right hand reaching for HandTarget (the mace's swings), its haft turned along HaftDir; HaftLocal is the
    // mace head's direction in the hand bone's own space.
    FVector HandTarget = FVector::ZeroVector, HaftDir = FVector::UpVector, HaftLocal = FVector::UpVector;
    float HandWeight = 0.f;
};

struct FSkeletonAnimParams
{
    FFiendPose Fiend;
    FSkeletonTrack From, To;
    float Fade = 1.f;                     // To over From
    float LookYaw = 0.f, LookPitch = 0.f; // deg: + left / + down
    float Jaw = 0.f;                      // deg open
    float Flinch = 0.f;                   // deg the chest is thrown back
    float Twist = 0.f;                    // deg the chest is turned by a blow (+ to its left)
    float HeadScale = 1.f;                // < 1: the skull (and jaw) gone (struck off: TempleSanctumBattle)
    FVector Forward = FVector::ForwardVector, Up = FVector::UpVector;   // the body's facing in component space
};

struct FSkeletonAnimProxy : public FAnimInstanceProxy
{
    FSkeletonAnimProxy() = default;
    explicit FSkeletonAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
    virtual bool Evaluate(FPoseContext& Output) override;
    FSkeletonAnimParams Params;
    int32 Evaluations = 0;
};

UCLASS(Transient, NotBlueprintable)
class CHUCK3D_API USkeletonAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    FSkeletonAnimParams Params;
    int32 GetEvaluations() { return GetProxyOnGameThread<FSkeletonAnimProxy>().Evaluations; }
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FSkeletonAnimProxy(this); }
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FSkeletonAnimProxy*>(InProxy); }
};

/** Game-thread driver for one skeleton. */
class FSkeletonAnimator
{
public:
    void Init(USkeletalMeshComponent* InMesh, const TMap<FName, UAnimSequence*>& InClips, float InWalkStride, const FVector& InForward);
    /** A clip at Rate (negative: backwards from its end), cross-faded over Blend seconds unless already playing. */
    void Play(FName Clip, bool bLoop, float Rate, bool bRestart = false, float Blend = .25f);
    /** The walk, its phase following the ground covered (Idle when nearly still). */
    void Locomote(float Speed);
    void Tick(float DeltaSeconds, float Travelled);
    void SetLook(float Yaw, float Pitch) { Look = FVector2D(Yaw, Pitch); }
    void SetJaw(float Degrees) { JawTarget = Degrees; }
    void Flinch(float Degrees) { FlinchKick = FMath::Max(FlinchKick, Degrees); }
    /** A blow's push on the chest: thrown back by Degrees and turned by TwistDegrees (+ to its left). */
    void Flinch(float Degrees, float TwistDegrees) { Flinch(Degrees); TwistKick += TwistDegrees; }
    void SetHeadScale(float S) { HeadScale = S; }
    /** Hit-stop: the clips' time runs at this rate (1 normal, near 0 frozen on a blow). */
    void SetTimeScale(float S) { TimeScale = S; }
    FName Playing() const { return Cur.Clip; }
    float Time() const { return Cur.Time; }
    float Length() const;
    bool Finished() const;
    float GetJaw() const { return Jaw; }
    float GetFlinch() const { return FlinchNow; }
    /** The walk's phase (0..1) and stride; the Pit Fiend's layers (sent with the clips each tick). */
    float GetPhase() const { return Phase; }
    bool IsWalking() const { return Cur.bLoco; }
    FFiendPose Fiend;
private:
    struct FTrack { FName Clip; bool bLoco = false, bLoop = false; float Time = 0.f, Rate = 1.f; };
    TWeakObjectPtr<USkeletalMeshComponent> Mesh;
    TMap<FName, UAnimSequence*> Clips;
    FTrack Cur, Prev;
    float Fade = 1.f, FadeTime = .25f, Phase = 0.f, WalkStride = 200.f, Speed = 0.f;
    FVector Forward = FVector::ForwardVector;
    FVector2D Look = FVector2D::ZeroVector, LookNow = FVector2D::ZeroVector;
    float JawTarget = 0.f, Jaw = 0.f, FlinchKick = 0.f, FlinchNow = 0.f, FlinchVel = 0.f;
    float TwistKick = 0.f, TwistNow = 0.f, TwistVel = 0.f, HeadScale = 1.f, TimeScale = 1.f;
    FSkeletonTrack Source(const FTrack& Track) const;
};
