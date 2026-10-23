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

struct FSkeletonAnimParams
{
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
