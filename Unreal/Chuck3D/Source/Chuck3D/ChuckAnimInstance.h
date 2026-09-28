#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "ChuckAnimInstance.generated.h"

class UAnimSequence;

/**
 * Per-frame request written by AChuckCharacter on the game thread and copied
 * to the proxy in PreUpdate. Clip B is cross-faded over clip A by WeightB. A
 * positive period marks a looping clip; the proxy interpolates the gap between
 * its last key and the first so loops have no held frame.
 * Foot index 0 = _L, 1 = _R.
 */
struct FChuckAnimParams
{
    UAnimSequence* ClipA = nullptr;
    float TimeA = 0.f;
    float PeriodA = 0.f;
    /** Sample left/right mirrored (e.g. WalkStop from the right-planted half stride). */
    bool bMirrorA = false;
    UAnimSequence* ClipB = nullptr;
    float TimeB = 0.f;
    float PeriodB = 0.f;
    bool bMirrorB = false;
    float WeightB = 0.f;
    /** Speed layer over the A/B result: RunLoop blended in by WeightRun (saunter -> run). */
    UAnimSequence* ClipRun = nullptr;
    float TimeRun = 0.f;
    float PeriodRun = 0.f;
    float WeightRun = 0.f;

    /** Paw is in stance: its ball is locked in world space from the moment stance begins. */
    bool bStance[2] = {false, false};
    /** Standing: a locked paw that drifts from its animated place takes a short settle step. */
    bool bAllowSettle = false;
    /** Traced ground under each paw relative to the mesh origin (cm). */
    float GroundOffset[2] = {0.f, 0.f};
    /** Pelvis correction (cm, <= 0) so a paw on lower ground stays reachable. */
    float PelvisOffset = 0.f;
    bool bFootIK = false;
};

/** Measured output of the last evaluation, for tests and captures. */
struct FChuckAnimResult
{
    FVector BallWorld[2] = {FVector::ZeroVector, FVector::ZeroVector};
    float LockAlpha[2] = {0.f, 0.f};
    /** Distance the solved hock misses its IK effector by (0 when reachable). */
    float Shortfall[2] = {0.f, 0.f};
    bool bSettling[2] = {false, false};
    /** Stance locks dropped because a paw was pulled out of reach (cumulative). */
    int32 Releases = 0;
    int32 Evaluations = 0;
};

struct FChuckAnimProxy : public FAnimInstanceProxy
{
    FChuckAnimProxy() = default;
    explicit FChuckAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
    virtual bool Evaluate(FPoseContext& Output) override;

    FChuckAnimParams Params;
    FChuckAnimResult Result;
private:
    struct FFootLock
    {
        bool bLocked = false;
        bool bReleased = false;   // lock dropped mid-stance; wait for the next stance
        float Alpha = 0.f;
        FVector World = FVector::ZeroVector;
        float Settle = -1.f;      // settle step progress 0..1, negative when idle
        FVector SettleStart = FVector::ZeroVector;
    };
    FFootLock Locks[2];
    float DeltaTime = 0.f;
    bool bHasResult = false;
    // _L/_R mirror map for the current required bones.
    TArray<FCompactPoseBoneIndex> MirrorBones;
    TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> MirrorRefRotations;
    uint16 MirrorSerial = 0;
    void Sample(UAnimSequence* Clip, float Time, float Period, bool bMirror, FPoseContext& Out);
    void Mirror(FCompactPose& Pose);
};

/**
 * Native (graph-free) animation instance for the v1 rig. Pose generation is
 * reviewable C++ instead of a binary Animation Blueprint: clip sampling and
 * cross-fades, then per-leg two-bone IK for ground height, stance locking and
 * settle steps. See docs/RIG-CONTRACT-V1.md.
 */
UCLASS(Transient, NotBlueprintable)
class CHUCK3D_API UChuckAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    FChuckAnimParams Params;
    /** Blocks until any running evaluation finishes. */
    FChuckAnimResult GetResult() { return GetProxyOnGameThread<FChuckAnimProxy>().Result; }
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FChuckAnimProxy(this); }
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FChuckAnimProxy*>(InProxy); }
};
