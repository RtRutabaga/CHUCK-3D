#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "DinoAnim.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

/**
 * The Chult dinosaurs' animation (Claude, 2026-10-16; docs/CHULT-RAPTOR.md,
 * CHULT-TREX.md). Quaternius's raptor and T-rex share one rig: back legs
 * BackUpLeg_X -> BackLowLeg_X under Body, each foot a separate BackFoot_X
 * under root (the source's IK target). Replaces direct clip switching:
 * - loops trimmed to their real cycle (the raptor's Walk is one stride and then
 *   a second of standing; the runs end on a stray rest frame);
 * - the stride's phase advances with the distance travelled, so a planted foot
 *   moves back exactly as fast as the body moves on; walk and run share it;
 * - every clip change is a short cross-fade;
 * - the imported Idle clips hold the back feet in the air (48 cm on the
 *   raptor, 3 m on the T-rex: they floated whenever they stood still); their
 *   legs come from the rest stance (Attack's first frame) instead;
 * - the feet are put on the ground actually under them (the pelvis drops for
 *   the lower foot, two-bone IK for each leg, the foot tilted to the slope).
 */
struct FDinoSource
{
    UAnimSequence* Clip = nullptr;
    float Time = 0.f, End = 0.f, Period = 0.f;    // Period > 0: a loop of Period seconds whose keys end at End
    UAnimSequence* Clip2 = nullptr;               // blended over Clip by Weight2 at the same phase (walk -> run)
    float Time2 = 0.f, End2 = 0.f, Period2 = 0.f, Weight2 = 0.f;
    bool bRestLegs = false;                       // back legs from the rest stance (the imported Idle holds the feet up)
};

struct FDinoAnimParams
{
    FDinoSource From, To;
    UAnimSequence* Rest = nullptr;    // sampled at 0 for the rest stance's legs (Attack: feet together)
    float Fade = 1.f;                 // To over From
    float GroundOffset[2] = {0.f, 0.f};   // ground under each back foot relative to the mesh origin (cm)
    FVector GroundNormal[2] = {FVector::UpVector, FVector::UpVector};   // component space
    float PelvisOffset = 0.f;
    float MaxDrop = 0.f;                  // how far the body may sink further so both legs reach their feet
    float IKAlpha = 0.f;
};

struct FDinoAnimResult
{
    FVector FootWorld[2] = {FVector::ZeroVector, FVector::ZeroVector};
    float Shortfall[2] = {0.f, 0.f};
    float Drop = 0.f;
    int32 Evaluations = 0;
};

struct FDinoAnimProxy : public FAnimInstanceProxy
{
    FDinoAnimProxy() = default;
    explicit FDinoAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
    virtual bool Evaluate(FPoseContext& Output) override;
    FDinoAnimParams Params;
    FDinoAnimResult Result;
private:
    void Sample(const FDinoSource& Source, FPoseContext& Out);
};

UCLASS(Transient, NotBlueprintable)
class CHUCK3D_API UDinoAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    FDinoAnimParams Params;
    FDinoAnimResult GetResult() { return GetProxyOnGameThread<FDinoAnimProxy>().Result; }
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FDinoAnimProxy(this); }
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FDinoAnimProxy*>(InProxy); }
};

/** Each creature's measured cycles (Blender, from the exported clips; cm, s). */
struct FDinoGait
{
    float IdlePeriod = 2.5f;
    float WalkEnd = 1.f, WalkPeriod = 1.f, WalkStride = 100.f;   // stride: ground covered in one full cycle (both steps)
    float RunEnd = 0.f, RunPeriod = 0.f, RunStride = 0.f;       // no run layer when 0
    float RunFrom = 1e9f, RunTo = 1e9f;                         // speed band over which the run blends in
    float FadeTime = .2f;
    float IKUp = 20.f, IKDown = 30.f;                           // how far a foot may be raised / lowered
    float FootRestZ = 2.6f;                                     // the foot bone's height above the sole's ground when planted
    float TraceUp = 60.f;
};

/** Game-thread driver: the creature's Tick asks for clips or locomotion; this blends them and plants the feet. */
class FDinoAnimator
{
public:
    void Init(USkeletalMeshComponent* InMesh, const TMap<FName, UAnimSequence*>& InClips, const FDinoGait& InGait);
    /** A clip (Idle, Attack, Jump, Death, ...) at Rate; cross-faded unless it is already playing. */
    void Play(FName Clip, bool bLoop, float Rate, bool bRestart = false);
    /** Walk/run by ground speed (Idle when nearly still). */
    void Locomote(float Speed);
    void Tick(float DeltaSeconds, const FVector& Velocity, bool bOnGround);
    FName Playing() const { return Cur.Clip; }
    /** The planted (lower) foot's sole above the ground under it, cm (last frame, while walking on the ground). */
    float FootGap() const { return LastGap; }
    float MaxFootGap() const { return MaxGap; }
    int32 FootGapSamples() const { return GapSamples; }
    void ResetFootGap() { MaxGap = -1e6f; GapSamples = 0; }
private:
    struct FTrack
    {
        FName Clip;
        bool bLoco = false, bLoop = false;
        float Time = 0.f, Rate = 1.f;
    };
    TWeakObjectPtr<USkeletalMeshComponent> Mesh;
    TMap<FName, UAnimSequence*> Clips;
    FDinoGait Gait;
    FTrack Cur, Prev;
    float Fade = 1.f, Phase = 0.f, RunWeight = 0.f, Speed = 0.f;
    float Offset[2] = {0.f, 0.f}, Pelvis = 0.f, IKAlpha = 0.f;
    FVector Normal[2] = {FVector::UpVector, FVector::UpVector};
    float LastGap = 0.f, MaxGap = -1e6f;
    int32 GapSamples = 0;
    int32 Measured = 0;
    void Start(const FTrack& Next);
    FDinoSource Source(const FTrack& Track) const;
    void Feet(float DeltaSeconds, bool bOnGround);
};
