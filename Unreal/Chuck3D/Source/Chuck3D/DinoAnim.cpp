#include "DinoAnim.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

namespace
{
    const TCHAR* UpLeg[] = {TEXT("BackUpLeg_L"), TEXT("BackUpLeg_R")};
    const TCHAR* LowLeg[] = {TEXT("BackLowLeg_L"), TEXT("BackLowLeg_R")};
    const TCHAR* FootBone[] = {TEXT("BackFoot_L"), TEXT("BackFoot_R")};
    const TCHAR* LegChain[] = {TEXT("BackLeg_L"), TEXT("BackUpLeg_L"), TEXT("BackLowLeg_L"), TEXT("BackFoot_L"),
                               TEXT("BackLeg_R"), TEXT("BackUpLeg_R"), TEXT("BackLowLeg_R"), TEXT("BackFoot_R")};
    constexpr float MaxTilt = 25.f;        // degrees a foot follows the slope under it

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

    void Blend(FPoseContext& A, FPoseContext& B, float WeightOfB, FPoseContext& Out)
    {
        FAnimationPoseData DataA(A), DataB(B), OutData(Out);
        FAnimationRuntime::BlendTwoPosesTogether(DataA, DataB, 1.f - FMath::Clamp(WeightOfB, 0.f, 1.f), OutData);
    }

    void SampleOne(UAnimSequence* Clip, float Time, float End, float Period, FPoseContext& Out)
    {
        const float Length = Clip->GetPlayLength();
        End = FMath::Min(End > 0.f ? End : Length, Length);
        if (Period > 0.f)
        {
            Time = FMath::Fmod(Time, Period);
            if (Time < 0.f) Time += Period;
            if (Period > End + KINDA_SMALL_NUMBER && Time > End)
            {
                // Between the cycle's last good key and its start: blend the two.
                FPoseContext Last(Out), First(Out);
                FAnimationPoseData LastData(Last), FirstData(First);
                Clip->GetAnimationPose(LastData, FAnimExtractContext(static_cast<double>(End), false));
                Clip->GetAnimationPose(FirstData, FAnimExtractContext(0.0, false));
                Blend(Last, First, (Time - End) / (Period - End), Out);
                return;
            }
        }
        FAnimationPoseData OutData(Out);
        Clip->GetAnimationPose(OutData, FAnimExtractContext(static_cast<double>(FMath::Clamp(Time, 0.f, Length)), false));
    }
}

void FDinoAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
    FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
    Params = CastChecked<UDinoAnimInstance>(InAnimInstance)->Params;
}

void FDinoAnimProxy::Sample(const FDinoSource& Source, FPoseContext& Out)
{
    SampleOne(Source.Clip, Source.Time, Source.End, Source.Period, Out);
    if (Source.bRestLegs && Params.Rest)
    {
        // Thigh to foot as they stand at rest (the feet hang from root, the legs from the body).
        FPoseContext Rest(Out);
        SampleOne(Params.Rest, 0.f, 0.f, 0.f, Rest);
        const FBoneContainer& Bones = Out.Pose.GetBoneContainer();
        for (const TCHAR* Name : LegChain)
        {
            const FCompactPoseBoneIndex I = Find(Bones, Name);
            if (I != INDEX_NONE) Out.Pose[I] = Rest.Pose[I];
        }
    }
    if (Source.Clip2 && Source.Weight2 > KINDA_SMALL_NUMBER)
    {
        FPoseContext Over(Out), Base(Out);
        SampleOne(Source.Clip2, Source.Time2, Source.End2, Source.Period2, Over);
        CopyPose(Out, Base);
        Blend(Base, Over, Source.Weight2, Out);
    }
}

bool FDinoAnimProxy::Evaluate(FPoseContext& Output)
{
    if (!Params.To.Clip)
    {
        Output.ResetToRefPose();
        return true;
    }
    // 1. The clips, cross-faded (local space).
    if (Params.From.Clip && Params.Fade < 1.f - KINDA_SMALL_NUMBER)
    {
        FPoseContext FromPose(Output), ToPose(Output);
        Sample(Params.From, FromPose);
        Sample(Params.To, ToPose);
        Blend(FromPose, ToPose, Params.Fade, Output);
    }
    else
    {
        Sample(Params.To, Output);
    }
    ++Result.Evaluations;

    // 2. The feet on the ground under them: the body drops for the lower foot,
    //    each leg reaches its foot's place by two-bone IK, the foot tilts to the slope.
    const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
    const FTransform& ToWorld = GetComponentTransform();
    FComponentSpacePoseContext CS(this);
    CS.Pose.InitPose(Output.Pose);
    const float Alpha = FMath::Clamp(Params.IKAlpha, 0.f, 1.f);
    const FCompactPoseBoneIndex Body = Find(Bones, TEXT("Body"));
    // A wide stride needs the hips lower: sink the body just enough that each
    // leg reaches its foot's target (its length as posed, less a little slack).
    float Drop = 0.f;
    if (Alpha > 0.f && Params.MaxDrop > 0.f)
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const FCompactPoseBoneIndex I0 = Find(Bones, UpLeg[Side]), I1 = Find(Bones, LowLeg[Side]), I2 = Find(Bones, FootBone[Side]);
            if (I0 == INDEX_NONE || I1 == INDEX_NONE || I2 == INDEX_NONE) continue;
            const FVector Hip = CS.Pose.GetComponentSpaceTransform(I0).GetLocation() + FVector(0, 0, Params.PelvisOffset * Alpha);
            const FVector Foot = CS.Pose.GetComponentSpaceTransform(I2).GetLocation();
            const FVector Knee = CS.Pose.GetComponentSpaceTransform(I1).GetLocation() + FVector(0, 0, Params.PelvisOffset * Alpha);
            const float Reach = static_cast<float>(FVector::Dist(Hip, Knee) + FVector::Dist(Knee, Foot + FVector(0, 0, Params.PelvisOffset * Alpha))) * .97f;
            const FVector Target = Foot + FVector(0, 0, Params.GroundOffset[Side] * Alpha);
            const float Flat = static_cast<float>(FVector::Dist2D(Hip, Target));
            const float Down = static_cast<float>(Hip.Z - Target.Z);
            if (Flat < Reach && Down > 0.f) Drop = FMath::Max(Drop, Down - FMath::Sqrt(Reach * Reach - Flat * Flat));
        }
    Drop = FMath::Min(Drop, Params.MaxDrop);
    Result.Drop = Drop;
    const float Pelvis = Params.PelvisOffset * Alpha - Drop;
    if (Alpha > 0.f && Body != INDEX_NONE && !FMath::IsNearlyZero(Pelvis))
    {
        FTransform T = CS.Pose.GetComponentSpaceTransform(Body);
        T.AddToTranslation(FVector(0, 0, Pelvis));
        const FBoneTransform Moved[] = {FBoneTransform(Body, T)};
        CS.Pose.LocalBlendCSBoneTransforms(Moved, 1.f);
    }
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FCompactPoseBoneIndex I0 = Find(Bones, UpLeg[Side]), I1 = Find(Bones, LowLeg[Side]), I2 = Find(Bones, FootBone[Side]);
        if (I0 == INDEX_NONE || I1 == INDEX_NONE || I2 == INDEX_NONE) continue;
        const FTransform Foot = CS.Pose.GetComponentSpaceTransform(I2);
        if (Alpha <= 0.f)
        {
            Result.FootWorld[Side] = ToWorld.TransformPosition(Foot.GetLocation());
            Result.Shortfall[Side] = 0.f;
            continue;
        }
        // The foot hangs from root, not the leg: the leg's end moved with the body.
        const FVector EndAt = Foot.GetLocation() + FVector(0, 0, Pelvis);
        const FVector Target = Foot.GetLocation() + FVector(0, 0, Params.GroundOffset[Side] * Alpha);
        FTransform Hip = CS.Pose.GetComponentSpaceTransform(I0);
        FTransform Knee = CS.Pose.GetComponentSpaceTransform(I1);
        FTransform End(Foot.GetRotation(), EndAt);
        // Keep the clip's knee plane.
        FVector Pole = Knee.GetLocation() - (Hip.GetLocation() + EndAt) * .5;
        Pole = Pole.SizeSquared() < 1e-4 ? FVector(1, 0, 0) : Pole.GetSafeNormal();
        AnimationCore::SolveTwoBoneIK(Hip, Knee, End, Knee.GetLocation() + Pole * 10.0, Target, false, 1.0, 1.0);
        const FBoneTransform Leg[] = {FBoneTransform(I0, Hip), FBoneTransform(I1, Knee)};
        CS.Pose.LocalBlendCSBoneTransforms(Leg, 1.f);
        // The foot at the leg's end, its sole turned to the ground's slope.
        FVector N = Params.GroundNormal[Side].GetSafeNormal();
        if (N.IsNearlyZero()) N = FVector::UpVector;
        FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, N);
        const float Angle = FMath::RadiansToDegrees(static_cast<float>(Tilt.GetAngle()));
        if (Angle > MaxTilt) Tilt = FQuat::Slerp(FQuat::Identity, Tilt, MaxTilt / Angle);
        Tilt = FQuat::Slerp(FQuat::Identity, Tilt, Alpha);
        const FBoneTransform Placed[] = {FBoneTransform(I2, FTransform(Tilt * Foot.GetRotation(), End.GetLocation()))};
        CS.Pose.LocalBlendCSBoneTransforms(Placed, 1.f);
        Result.FootWorld[Side] = ToWorld.TransformPosition(End.GetLocation());
        Result.Shortfall[Side] = static_cast<float>(FVector::Dist(End.GetLocation(), Target));
    }
    FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS.Pose), Output.Pose);
    return true;
}

// ---------------------------------------------------------------------------

void FDinoAnimator::Init(USkeletalMeshComponent* InMesh, const TMap<FName, UAnimSequence*>& InClips, const FDinoGait& InGait)
{
    Mesh = InMesh; Clips = InClips; Gait = InGait;
    Cur = FTrack(); Prev = FTrack(); Fade = 1.f;
}

void FDinoAnimator::Start(const FTrack& Next)
{
    if (Next.bLoco && !Cur.bLoco) Phase = 0.f;   // the cycle starts from the rest stance, as the idle does
    Prev = Cur;
    Cur = Next;
    Fade = Prev.Clip.IsNone() ? 1.f : 0.f;
}

void FDinoAnimator::Play(FName Clip, bool bLoop, float Rate, bool bRestart)
{
    if (!Clips.FindRef(Clip)) return;
    if (Cur.Clip == Clip && !Cur.bLoco && !bRestart) { Cur.Rate = Rate; Cur.bLoop = bLoop; return; }
    FTrack Next; Next.Clip = Clip; Next.bLoop = bLoop; Next.Rate = Rate;
    Start(Next);
}

void FDinoAnimator::Locomote(float InSpeed)
{
    // A little hysteresis so a creature easing to a stop does not flicker between the two.
    if (InSpeed < (Cur.bLoco ? 10.f : 25.f)) { Play(TEXT("Idle"), true, 1.f); return; }
    if (Cur.bLoco) return;
    FTrack Next; Next.Clip = TEXT("Walk"); Next.bLoco = true; Next.bLoop = true;
    Start(Next);
}

FDinoSource FDinoAnimator::Source(const FTrack& Track) const
{
    FDinoSource S;
    if (Track.bLoco)
    {
        S.Clip = Clips.FindRef(TEXT("Walk"));
        S.End = Gait.WalkEnd; S.Period = Gait.WalkPeriod; S.Time = Phase * Gait.WalkPeriod;
        if (Gait.RunPeriod > 0.f && RunWeight > 0.f)
        {
            S.Clip2 = Clips.FindRef(TEXT("Run"));
            S.End2 = Gait.RunEnd; S.Period2 = Gait.RunPeriod; S.Time2 = Phase * Gait.RunPeriod; S.Weight2 = RunWeight;
        }
        return S;
    }
    S.Clip = Clips.FindRef(Track.Clip);
    S.Time = Track.Time;
    S.bRestLegs = Track.Clip == TEXT("Idle");
    if (Track.bLoop && S.Clip)
    {
        S.Period = Track.Clip == TEXT("Idle") ? Gait.IdlePeriod : S.Clip->GetPlayLength();
        S.End = S.Period;
    }
    return S;
}

void FDinoAnimator::Tick(float DeltaSeconds, const FVector& Velocity, bool bOnGround)
{
    USkeletalMeshComponent* M = Mesh.Get();
    if (!M) return;
    Speed = static_cast<float>(Velocity.Size2D());
    // The stride's phase by distance: a planted foot slides back as fast as the body goes on.
    const float RunTarget = Gait.RunPeriod > 0.f ? FMath::Clamp((Speed - Gait.RunFrom) / FMath::Max(1.f, Gait.RunTo - Gait.RunFrom), 0.f, 1.f) : 0.f;
    RunWeight = FMath::FInterpTo(RunWeight, RunTarget, DeltaSeconds, 5.f);
    const float Stride = FMath::Lerp(Gait.WalkStride, Gait.RunPeriod > 0.f ? Gait.RunStride : Gait.WalkStride, RunWeight);
    Phase = FMath::Fmod(Phase + Speed * DeltaSeconds / FMath::Max(1.f, Stride), 1.f);
    Cur.Time += DeltaSeconds * Cur.Rate;
    Prev.Time += DeltaSeconds * Prev.Rate;
    Fade = FMath::Min(1.f, Fade + DeltaSeconds / FMath::Max(.01f, Gait.FadeTime));
    Feet(DeltaSeconds, bOnGround);

    if (auto* Anim = Cast<UDinoAnimInstance>(M->GetAnimInstance()))
    {
        FDinoAnimParams& P = Anim->Params;
        P.Rest = Clips.FindRef(TEXT("Attack"));
        P.To = Source(Cur);
        P.From = Fade < 1.f ? Source(Prev) : FDinoSource();
        P.Fade = Fade;
        const FTransform& C = M->GetComponentTransform();
        for (int32 Side = 0; Side < 2; ++Side)
        {
            P.GroundOffset[Side] = Offset[Side];
            P.GroundNormal[Side] = C.InverseTransformVectorNoScale(Normal[Side]);
        }
        P.PelvisOffset = Pelvis;
        P.MaxDrop = Gait.IKDown;
        P.IKAlpha = IKAlpha;
    }
}

void FDinoAnimator::Feet(float DeltaSeconds, bool bOnGround)
{
    USkeletalMeshComponent* M = Mesh.Get();
    UWorld* World = M ? M->GetWorld() : nullptr;
    if (!World) return;
    IKAlpha = FMath::FInterpConstantTo(IKAlpha, bOnGround ? 1.f : 0.f, DeltaSeconds, 4.f);
    const float MeshZ = static_cast<float>(M->GetComponentLocation().Z);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(DinoFeet), false, M->GetOwner());
    FCollisionObjectQueryParams Objects(ECC_WorldStatic);
    float Ground[2] = {MeshZ, MeshZ};
    bool bFound[2] = {false, false};
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FVector Foot = M->GetBoneLocation(FName(FootBone[Side]));
        FHitResult Hit;
        float Target = 0.f;
        FVector N = FVector::UpVector;
        if (World->LineTraceSingleByObjectType(Hit, FVector(Foot.X, Foot.Y, MeshZ + Gait.TraceUp), FVector(Foot.X, Foot.Y, MeshZ - Gait.IKDown - 40.f), Objects, Q)
            && Hit.ImpactNormal.Z > .6f)
        {
            Target = FMath::Clamp(static_cast<float>(Hit.ImpactPoint.Z) - MeshZ, -Gait.IKDown, Gait.IKUp);
            N = Hit.ImpactNormal;
            Ground[Side] = static_cast<float>(Hit.ImpactPoint.Z);
            bFound[Side] = true;
        }
        if (!bOnGround) { Target = 0.f; N = FVector::UpVector; }
        Offset[Side] = FMath::FInterpTo(Offset[Side], Target, DeltaSeconds, 12.f);
        Normal[Side] = FMath::VInterpNormalRotationTo(Normal[Side], N, DeltaSeconds, 180.f);
    }
    Pelvis = FMath::FInterpTo(Pelvis, FMath::Clamp(FMath::Min(Offset[0], Offset[1]), -Gait.IKDown, Gait.IKUp * .5f), DeltaSeconds, 10.f);

    // How high the planted foot's sole stands off the ground under it (last evaluated pose).
    if (bOnGround && IKAlpha >= 1.f && Fade >= 1.f && bFound[0] && bFound[1])
        if (auto* Anim = Cast<UDinoAnimInstance>(M->GetAnimInstance()))
        {
            const FDinoAnimResult R = Anim->GetResult();
            // Only a pose evaluated since the last look (off screen it is not evaluated).
            if (R.Evaluations > Measured)
            {
                Measured = R.Evaluations;
                LastGap = FMath::Min(static_cast<float>(R.FootWorld[0].Z) - Ground[0], static_cast<float>(R.FootWorld[1].Z) - Ground[1]) - Gait.FootRestZ;
                MaxGap = FMath::Max(MaxGap, LastGap);
                ++GapSamples;
            }
        }
}
