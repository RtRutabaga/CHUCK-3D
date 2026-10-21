#include "SkeletonAnim.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
    constexpr float FrameTime = 1.f / 30.f;   // the clips' rate: a loop's last key runs on into its first over one frame

    FCompactPoseBoneIndex Find(const FBoneContainer& Bones, const TCHAR* Name)
    {
        const int32 Mesh = Bones.GetPoseBoneIndexForBoneName(FName(Name));
        return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
    }

    void Blend(FPoseContext& A, FPoseContext& B, float WeightOfB, FPoseContext& Out)
    {
        FAnimationPoseData DataA(A), DataB(B), OutData(Out);
        FAnimationRuntime::BlendTwoPosesTogether(DataA, DataB, 1.f - FMath::Clamp(WeightOfB, 0.f, 1.f), OutData);
    }

    void Sample(const FSkeletonTrack& Track, FPoseContext& Out)
    {
        UAnimSequence* Clip = Track.Clip;
        const float Length = Clip->GetPlayLength();
        float Time = Track.Time;
        if (Track.bLoop)
        {
            const float Period = Length + FrameTime;
            Time = FMath::Fmod(Time, Period);
            if (Time < 0.f) Time += Period;
            if (Time > Length)
            {
                FPoseContext Last(Out), First(Out);
                FAnimationPoseData LastData(Last), FirstData(First);
                Clip->GetAnimationPose(LastData, FAnimExtractContext(static_cast<double>(Length), false));
                Clip->GetAnimationPose(FirstData, FAnimExtractContext(0.0, false));
                Blend(Last, First, (Time - Length) / FrameTime, Out);
                return;
            }
        }
        FAnimationPoseData OutData(Out);
        Clip->GetAnimationPose(OutData, FAnimExtractContext(static_cast<double>(FMath::Clamp(Time, 0.f, Length)), false));
    }

    // Turn a bone (and so everything it carries) about Axis through its own joint, in component space.
    void Turn(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Bone, const FVector& Axis, float Degrees)
    {
        if (Bone == INDEX_NONE || FMath::IsNearlyZero(Degrees, .01f)) return;
        FTransform T = Pose.GetComponentSpaceTransform(Bone);
        T.SetRotation(FQuat(Axis.GetSafeNormal(), FMath::DegreesToRadians(Degrees)) * T.GetRotation());
        const FBoneTransform Moved[] = {FBoneTransform(Bone, T)};
        Pose.LocalBlendCSBoneTransforms(Moved, 1.f);
    }
}

void FSkeletonAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
    FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
    Params = CastChecked<USkeletonAnimInstance>(InAnimInstance)->Params;
}

bool FSkeletonAnimProxy::Evaluate(FPoseContext& Output)
{
    if (!Params.To.Clip) { Output.ResetToRefPose(); return true; }
    if (Params.From.Clip && Params.Fade < 1.f - KINDA_SMALL_NUMBER)
    {
        FPoseContext FromPose(Output), ToPose(Output);
        Sample(Params.From, FromPose);
        Sample(Params.To, ToPose);
        Blend(FromPose, ToPose, Params.Fade, Output);
    }
    else Sample(Params.To, Output);
    ++Evaluations;

    const bool bLook = !FMath::IsNearlyZero(Params.LookYaw, .05f) || !FMath::IsNearlyZero(Params.LookPitch, .05f);
    if (!bLook && Params.Jaw < .05f && Params.Flinch < .05f) return true;
    // The layers, about the body's own axes: Up, and Side (its left), so that a
    // turn of + about Side tips the facing down.
    const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
    FComponentSpacePoseContext CS(this);
    CS.Pose.InitPose(Output.Pose);
    const FVector Up = Params.Up.GetSafeNormal(), Side = FVector::CrossProduct(Params.Up, Params.Forward).GetSafeNormal();
    if (Params.Flinch > .05f)
    {
        // Thrown back at the chest, the head snapping on a little further.
        Turn(CS.Pose, Find(Bones, TEXT("spine_02")), Side, -Params.Flinch * .35f);
        Turn(CS.Pose, Find(Bones, TEXT("spine_03")), Side, -Params.Flinch * .45f);
        Turn(CS.Pose, Find(Bones, TEXT("head")), Side, -Params.Flinch * .3f);
    }
    if (bLook)
    {
        // The neck takes 40 % of the turn, the skull the rest.
        for (const TPair<const TCHAR*, float>& Part : {TPair<const TCHAR*, float>(TEXT("neck_01"), .4f), TPair<const TCHAR*, float>(TEXT("head"), .6f)})
        {
            const FCompactPoseBoneIndex B = Find(Bones, Part.Key);
            Turn(CS.Pose, B, Up, Params.LookYaw * Part.Value);
            Turn(CS.Pose, B, Side, Params.LookPitch * Part.Value);
        }
    }
    if (Params.Jaw > .05f) Turn(CS.Pose, Find(Bones, TEXT("jaw")), Side, Params.Jaw);
    FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS.Pose), Output.Pose);
    return true;
}

// ---------------------------------------------------------------------------

void FSkeletonAnimator::Init(USkeletalMeshComponent* InMesh, const TMap<FName, UAnimSequence*>& InClips, float InWalkStride, const FVector& InForward)
{
    Mesh = InMesh; Clips = InClips; WalkStride = FMath::Max(1.f, InWalkStride); Forward = InForward;
    Cur = FTrack(); Prev = FTrack(); Fade = 1.f;
}

float FSkeletonAnimator::Length() const
{
    const UAnimSequence* Clip = Clips.FindRef(Cur.bLoco ? FName(TEXT("Walk")) : Cur.Clip);
    return Clip ? Clip->GetPlayLength() : 0.f;
}

bool FSkeletonAnimator::Finished() const
{
    if (Cur.bLoop || Cur.bLoco) return false;
    return Cur.Rate >= 0.f ? Cur.Time >= Length() : Cur.Time <= 0.f;
}

void FSkeletonAnimator::Play(FName Clip, bool bLoop, float Rate, bool bRestart, float Blend)
{
    UAnimSequence* Seq = Clips.FindRef(Clip);
    if (!Seq) return;
    if (Cur.Clip == Clip && !Cur.bLoco && !bRestart) { Cur.Rate = Rate; Cur.bLoop = bLoop; return; }
    FTrack Next; Next.Clip = Clip; Next.bLoop = bLoop; Next.Rate = Rate;
    Next.Time = Rate < 0.f ? Seq->GetPlayLength() : 0.f;
    Prev = Cur; Cur = Next;
    FadeTime = Blend;
    Fade = Prev.Clip.IsNone() || Blend <= 0.f ? 1.f : 0.f;
}

void FSkeletonAnimator::Locomote(float InSpeed)
{
    // A little hysteresis, so easing to a stop does not flicker between the two.
    if (InSpeed < (Cur.bLoco ? 8.f : 20.f)) { Play(TEXT("Idle"), true, 1.f); return; }
    if (Cur.bLoco) return;
    FTrack Next; Next.Clip = TEXT("Walk"); Next.bLoco = true; Next.bLoop = true;
    Prev = Cur; Cur = Next; FadeTime = .3f; Fade = 0.f;
}

FSkeletonTrack FSkeletonAnimator::Source(const FTrack& Track) const
{
    FSkeletonTrack S;
    if (Track.bLoco)
    {
        S.Clip = Clips.FindRef(TEXT("Walk"));
        S.bLoop = true;
        if (S.Clip) S.Time = Phase * (S.Clip->GetPlayLength() + FrameTime);
        return S;
    }
    S.Clip = Clips.FindRef(Track.Clip);
    S.Time = Track.Time; S.bLoop = Track.bLoop;
    return S;
}

void FSkeletonAnimator::Tick(float DeltaSeconds, float Travelled)
{
    USkeletalMeshComponent* M = Mesh.Get();
    if (!M) return;
    // The stride's phase by the ground covered: a planted foot goes back exactly as fast as the body goes on.
    Phase = FMath::Fmod(Phase + Travelled / WalkStride, 1.f);
    Cur.Time += DeltaSeconds * Cur.Rate;
    Prev.Time += DeltaSeconds * Prev.Rate;
    Fade = FMath::Min(1.f, Fade + DeltaSeconds / FMath::Max(.01f, FadeTime));
    LookNow.X = FMath::FInterpTo(LookNow.X, Look.X, DeltaSeconds, 5.f);
    LookNow.Y = FMath::FInterpTo(LookNow.Y, Look.Y, DeltaSeconds, 5.f);
    Jaw = FMath::FInterpTo(Jaw, JawTarget, DeltaSeconds, 18.f);
    // The flinch: a kick back, then a damped spring home.
    if (FlinchKick > 0.f) { FlinchVel += FlinchKick * 28.f; FlinchKick = 0.f; }
    FlinchVel += (-FlinchNow * 220.f - FlinchVel * 18.f) * DeltaSeconds;
    FlinchNow = FMath::Max(0.f, FlinchNow + FlinchVel * DeltaSeconds);
    if (auto* Anim = Cast<USkeletonAnimInstance>(M->GetAnimInstance()))
    {
        FSkeletonAnimParams& P = Anim->Params;
        P.To = Source(Cur);
        P.From = Fade < 1.f ? Source(Prev) : FSkeletonTrack();
        P.Fade = Fade;
        P.LookYaw = LookNow.X; P.LookPitch = LookNow.Y; P.Jaw = Jaw; P.Flinch = FlinchNow;
        P.Forward = Forward; P.Up = FVector::UpVector;
    }
}
