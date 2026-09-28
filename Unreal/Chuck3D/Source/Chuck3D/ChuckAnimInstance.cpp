#include "ChuckAnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "Misc/ScopeExit.h"

void FChuckAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
    FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
    Params = CastChecked<UChuckAnimInstance>(InAnimInstance)->Params;
    DeltaTime = DeltaSeconds;
}

namespace
{
    FCompactPoseBoneIndex Find(const FBoneContainer& Bones, const TCHAR* Name)
    {
        const int32 Mesh = Bones.GetPoseBoneIndexForBoneName(FName(Name));
        return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
    }

    void CopyPose(const FPoseContext& From, FPoseContext& To)
    {
        To.Pose.CopyBonesFrom(From.Pose);
        To.Curve.CopyFrom(From.Curve);
        To.CustomAttributes.CopyFrom(From.CustomAttributes);
    }

    // Stance lock, settle step and ground-offset tuning (cm, s).
    constexpr float LockFade = .08f;
    constexpr float WalkRelease = 6.f;
    constexpr float SettleThreshold = 3.f;
    constexpr float SettleHover = .5f;
    constexpr float SettleTime = .18f;
    constexpr float SettleLift = 1.5f;
}

void FChuckAnimProxy::Mirror(FCompactPose& Pose)
{
    const FBoneContainer& Bones = Pose.GetBoneContainer();
    const int32 Num = Bones.GetCompactPoseNumBones();
    if (MirrorBones.Num() != Num || MirrorSerial != Bones.GetSerialNumber())
    {
        // Pair bones by the rig's _L/_R suffix; centre bones mirror onto themselves.
        MirrorSerial = Bones.GetSerialNumber();
        MirrorBones.Init(FCompactPoseBoneIndex(INDEX_NONE), Num);
        MirrorRefRotations.SetNumUninitialized(Num);
        const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
        for (FCompactPoseBoneIndex Index(0); Index < Num; ++Index)
        {
            FString Name = Ref.GetBoneName(Bones.MakeMeshPoseIndex(Index).GetInt()).ToString();
            MirrorBones[Index.GetInt()] = Index;
            const bool bLeft = Name.EndsWith(TEXT("_L"), ESearchCase::CaseSensitive);
            if (bLeft || Name.EndsWith(TEXT("_R"), ESearchCase::CaseSensitive))
            {
                Name = Name.LeftChop(1) + (bLeft ? TEXT("R") : TEXT("L"));
                const FCompactPoseBoneIndex Other = Find(Bones, *Name);
                if (Other != INDEX_NONE) MirrorBones[Index.GetInt()] = Other;
            }
            const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
            const FQuat Local = Bones.GetRefPoseTransform(Index).GetRotation();
            MirrorRefRotations[Index] = Parent == INDEX_NONE ? Local : MirrorRefRotations[Parent] * Local;
        }
    }
    FAnimationRuntime::MirrorPose(Pose, EAxis::Y, MirrorBones, MirrorRefRotations);
}

void FChuckAnimProxy::Sample(UAnimSequence* Clip, float Time, float Period, bool bMirror, FPoseContext& Out)
{
    ON_SCOPE_EXIT { if (bMirror) Mirror(Out.Pose); };
    const float Length = Clip->GetPlayLength();
    if (Period > 0.f)
    {
        Time = FMath::Fmod(Time, Period);
        if (Time < 0.f)
        {
            Time += Period;
        }
    }
    if (Period > Length + KINDA_SMALL_NUMBER && Time > Length)
    {
        // Between the last key and the loop start: blend the two samples.
        FPoseContext Last(Out), First(Out);
        FAnimationPoseData LastData(Last), FirstData(First), OutData(Out);
        Clip->GetAnimationPose(LastData, FAnimExtractContext(static_cast<double>(Length), false));
        Clip->GetAnimationPose(FirstData, FAnimExtractContext(0.0, false));
        FAnimationRuntime::BlendTwoPosesTogether(LastData, FirstData, 1.f - (Time - Length) / (Period - Length), OutData);
        return;
    }
    FAnimationPoseData OutData(Out);
    Clip->GetAnimationPose(OutData, FAnimExtractContext(static_cast<double>(FMath::Clamp(Time, 0.f, Length)), false));
}

bool FChuckAnimProxy::Evaluate(FPoseContext& Output)
{
    if (!Params.ClipA)
    {
        Output.ResetToRefPose();
        return true;
    }
    // 1. Sample and cross-fade the requested clips (local space).
    {
        FPoseContext PoseA(Output);
        Sample(Params.ClipA, Params.TimeA, Params.PeriodA, Params.bMirrorA, PoseA);
        if (Params.ClipB && Params.WeightB > KINDA_SMALL_NUMBER)
        {
            FPoseContext PoseB(Output);
            Sample(Params.ClipB, Params.TimeB, Params.PeriodB, Params.bMirrorB, PoseB);
            FAnimationPoseData DataA(PoseA), DataB(PoseB), OutData(Output);
            FAnimationRuntime::BlendTwoPosesTogether(DataA, DataB, 1.f - FMath::Clamp(Params.WeightB, 0.f, 1.f), OutData);
        }
        else
        {
            CopyPose(PoseA, Output);
        }
        if (Params.ClipRun && Params.WeightRun > KINDA_SMALL_NUMBER)
        {
            FPoseContext Run(Output), Blended(Output);
            Sample(Params.ClipRun, Params.TimeRun, Params.PeriodRun, false, Run);
            FAnimationPoseData BaseData(Output), RunData(Run), OutData(Blended);
            FAnimationRuntime::BlendTwoPosesTogether(BaseData, RunData, 1.f - FMath::Clamp(Params.WeightRun, 0.f, 1.f), OutData);
            CopyPose(Blended, Output);
        }
    }

    const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
    const FTransform& ToWorld = GetComponentTransform();
    static const TCHAR* Thigh[] = {TEXT("thigh_L"), TEXT("thigh_R")};
    static const TCHAR* Calf[] = {TEXT("calf_L"), TEXT("calf_R")};
    static const TCHAR* Foot[] = {TEXT("foot_L"), TEXT("foot_R")};
    static const TCHAR* Toes[] = {TEXT("toes_L"), TEXT("toes_R")};
    FComponentSpacePoseContext CS(this);
    CS.Pose.InitPose(Output.Pose);
    ++Result.Evaluations;

    if (!Params.bFootIK)
    {
        for (int32 Side = 0; Side < 2; ++Side)
        {
            Locks[Side] = FFootLock();
            Result.LockAlpha[Side] = 0.f;
            Result.Shortfall[Side] = 0.f;
            Result.bSettling[Side] = false;
            const FCompactPoseBoneIndex I3 = Find(Bones, Toes[Side]);
            if (I3 != INDEX_NONE)
            {
                Result.BallWorld[Side] = ToWorld.TransformPosition(CS.Pose.GetComponentSpaceTransform(I3).GetLocation());
            }
        }
        bHasResult = true;
        return true;
    }

    // 2. Clip ball positions before any correction.
    FVector ClipBall[2];
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FCompactPoseBoneIndex I3 = Find(Bones, Toes[Side]);
        ClipBall[Side] = I3 == INDEX_NONE ? FVector::ZeroVector : CS.Pose.GetComponentSpaceTransform(I3).GetLocation();
    }
    const FCompactPoseBoneIndex Pelvis = Find(Bones, TEXT("pelvis"));
    if (Pelvis != INDEX_NONE && !FMath::IsNearlyZero(Params.PelvisOffset))
    {
        FTransform T = CS.Pose.GetComponentSpaceTransform(Pelvis);
        T.AddToTranslation(FVector(0, 0, Params.PelvisOffset));
        const FBoneTransform Moved[] = {FBoneTransform(Pelvis, T)};
        CS.Pose.LocalBlendCSBoneTransforms(Moved, 1.f);
    }

    // 3. Stance locks and settle steps, then two-bone IK per leg.
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FCompactPoseBoneIndex I0 = Find(Bones, Thigh[Side]), I1 = Find(Bones, Calf[Side]);
        const FCompactPoseBoneIndex I2 = Find(Bones, Foot[Side]), I3 = Find(Bones, Toes[Side]);
        if (I0 == INDEX_NONE || I1 == INDEX_NONE || I2 == INDEX_NONE || I3 == INDEX_NONE)
        {
            continue;
        }
        const FVector AnimTarget = ClipBall[Side] + FVector(0, 0, Params.GroundOffset[Side]);
        FFootLock& Lock = Locks[Side];
        if (!Params.bStance[Side])
        {
            Lock.bReleased = false;
            Lock.bLocked = false;
            Lock.Settle = -1.f;
        }
        else if (!Lock.bLocked && !Lock.bReleased)
        {
            // Stance begins where the paw was last drawn, so locking never pops.
            Lock.bLocked = true;
            Lock.World = bHasResult ? Result.BallWorld[Side] : ToWorld.TransformPosition(AnimTarget);
            Lock.Alpha = 1.f;
        }
        float Lift = 0.f;
        if (Lock.bLocked)
        {
            const FVector Held = ToWorld.InverseTransformPosition(Lock.World);
            const float Error = FVector::Dist2D(Held, AnimTarget);
            // A paw caught mid-swing when Chuck stops is held in the air; step it down too.
            const bool bHovering = Held.Z - AnimTarget.Z > SettleHover;
            const bool bOtherSettling = Locks[1 - Side].Settle >= 0.f;
            if (Lock.Settle < 0.f && Params.bAllowSettle && (Error > SettleThreshold || bHovering) && !bOtherSettling)
            {
                Lock.Settle = 0.f;
                Lock.SettleStart = Lock.World;
            }
            if (Lock.Settle >= 0.f)
            {
                Lock.Settle = FMath::Min(1.f, Lock.Settle + DeltaTime / SettleTime);
                const float Ease = Lock.Settle * Lock.Settle * (3.f - 2.f * Lock.Settle);
                Lock.World = FMath::Lerp(Lock.SettleStart, ToWorld.TransformPosition(AnimTarget), Ease);
                Lift = FMath::Sin(Lock.Settle * PI) * SettleLift;
                if (Lock.Settle >= 1.f)
                {
                    Lock.Settle = -1.f;
                }
            }
            else if (!Params.bAllowSettle && Error > WalkRelease)
            {
                // A sharp turn outran the planted paw: let it follow the clip
                // until its next stance rather than stretching the leg.
                Lock.bLocked = false;
                Lock.bReleased = true;
                ++Result.Releases;
            }
        }
        Lock.Alpha = Lock.bLocked ? 1.f : FMath::Max(0.f, Lock.Alpha - DeltaTime / LockFade);
        const FVector LockTarget = ToWorld.InverseTransformPosition(Lock.World);
        const FVector TargetBall = FMath::Lerp(AnimTarget, LockTarget, Lock.Alpha) + FVector(0, 0, Lift);

        FTransform Hip = CS.Pose.GetComponentSpaceTransform(I0);
        FTransform Knee = CS.Pose.GetComponentSpaceTransform(I1);
        FTransform Hock = CS.Pose.GetComponentSpaceTransform(I2);
        const FVector Ball = CS.Pose.GetComponentSpaceTransform(I3).GetLocation();
        // Move the hock so the ball lands on target, keeping the clip's own
        // hock-to-ball vector so paw pitch and toe roll survive.
        const FVector BallOffset = Ball - Hock.GetLocation();
        const FVector Effector = TargetBall - BallOffset;
        // Keep the clip's knee plane.
        const FVector Mid = (Hip.GetLocation() + Hock.GetLocation()) * .5;
        FVector Pole = Knee.GetLocation() - Mid;
        Pole = Pole.SizeSquared() < 1e-4 ? FVector(1, 0, 0) : Pole.GetSafeNormal();
        AnimationCore::SolveTwoBoneIK(Hip, Knee, Hock, Knee.GetLocation() + Pole * 10.0, Effector, false, 1.0, 1.0);
        const FBoneTransform Solved[] = {FBoneTransform(I0, Hip), FBoneTransform(I1, Knee), FBoneTransform(I2, Hock)};
        CS.Pose.LocalBlendCSBoneTransforms(Solved, 1.f);

        const FVector FinalBall = CS.Pose.GetComponentSpaceTransform(I3).GetLocation();
        Result.BallWorld[Side] = ToWorld.TransformPosition(FinalBall);
        Result.LockAlpha[Side] = Lock.Alpha;
        Result.Shortfall[Side] = static_cast<float>(FVector::Dist(FinalBall, TargetBall));
        Result.bSettling[Side] = Lock.Settle >= 0.f;
    }
    bHasResult = true;
    FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS.Pose), Output.Pose);
    return true;
}
