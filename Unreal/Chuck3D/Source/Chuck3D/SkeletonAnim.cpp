#include "SkeletonAnim.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "TwoBoneIK.h"
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

    FVector At(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Bone) { return Pose.GetComponentSpaceTransform(Bone).GetLocation(); }

    void SetCS(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Bone, const FTransform& T)
    {
        const FBoneTransform Moved[] = {FBoneTransform(Bone, T)};
        Pose.LocalBlendCSBoneTransforms(Moved, 1.f);
    }

    // Two bones reaching for Goal (component space), the joint between them bending toward Pole; the end's rotation set.
    void Reach(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex A, FCompactPoseBoneIndex B, FCompactPoseBoneIndex C, const FVector& Goal, const FVector& Pole, const FQuat& EndRotation)
    {
        if (A == INDEX_NONE || B == INDEX_NONE || C == INDEX_NONE) return;
        FTransform TA = Pose.GetComponentSpaceTransform(A), TB = Pose.GetComponentSpaceTransform(B), TC = Pose.GetComponentSpaceTransform(C);
        AnimationCore::SolveTwoBoneIK(TA, TB, TC, Pole, Goal, false, 1.0, 1.0);
        TC.SetRotation(EndRotation);
        SetCS(Pose, A, TA); SetCS(Pose, B, TB); SetCS(Pose, C, TC);
    }

    // The Pit Fiend's layers (FFiendPose; 2026-11-16).
    void FiendLayers(FCSPose<FCompactPose>& Pose, const FBoneContainer& Bones, const FSkeletonAnimParams& P)
    {
        const FFiendPose& F = P.Fiend;
        // (Unreal's axes are left-handed: Up x Forward points to the body's RIGHT. Rt is that; Lt its left.)
        const FVector Up = P.Up.GetSafeNormal(), Fw = P.Forward.GetSafeNormal(), Rt = FVector::CrossProduct(Up, Fw).GetSafeNormal(), Lt = -Rt;
        auto B = [&](const TCHAR* Name) { return Find(Bones, Name); };
        // The feet: where the clip puts them, onto the ground under them, set out a little and laid on its slope; kept
        // there below, whatever the hips and spine do.
        static const TCHAR* LegNames[2][3] = {{TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l")}, {TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")}};
        FCompactPoseBoneIndex Leg[2][3] = {{FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)}, {FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)}};
        FTransform Goal[2];
        bool bLegs = true;
        for (int32 I = 0; I < 2 && bLegs; ++I)
        {
            for (int32 K = 0; K < 3; ++K) { Leg[I][K] = B(LegNames[I][K]); bLegs &= Leg[I][K] != INDEX_NONE; }
            if (!bLegs) break;
            Goal[I] = Pose.GetComponentSpaceTransform(Leg[I][2]);
            Goal[I].AddToTranslation(Up * F.FootLift[I] + Lt * (I == 0 ? F.Stance : -F.Stance));
            Goal[I].SetRotation(FQuat::FindBetweenNormals(Up, F.FootNormal[I].GetSafeNormal()) * Goal[I].GetRotation());
        }
        // The hips lowered and shifted (the knees take it, below); the spine leaning into its going and rolling into its
        // turns, twisting and bending with a swing, breathing.
        const FCompactPoseBoneIndex Pelvis = B(TEXT("pelvis"));
        if (Pelvis != INDEX_NONE)
        {
            FTransform T = Pose.GetComponentSpaceTransform(Pelvis);
            T.AddToTranslation(F.PelvisShift - Up * F.PelvisDrop);
            SetCS(Pose, Pelvis, T);
        }
        const FCompactPoseBoneIndex S1 = B(TEXT("spine_01")), S2 = B(TEXT("spine_02")), S3 = B(TEXT("spine_03"));
        // (a turn of + about Rt tips the facing down; about Fw, + drops its left side; about Up, + turns it to its right)
        Turn(Pose, S1, Rt, F.Lean * .35f + F.ChestBend * .2f);
        Turn(Pose, S2, Rt, F.Lean * .35f + F.ChestBend * .4f);
        Turn(Pose, S3, Rt, F.Lean * .3f + F.ChestBend * .4f - F.Breath * 1.4f);
        Turn(Pose, S1, Fw, F.Roll * .4f); Turn(Pose, S2, Fw, F.Roll * .35f); Turn(Pose, S3, Fw, F.Roll * .25f);
        Turn(Pose, S2, Up, -F.ChestTwist * .45f); Turn(Pose, S3, Up, -F.ChestTwist * .55f);
        Turn(Pose, B(TEXT("upperarm_l")), Rt, F.LeftArm);
        // The legs reach for their feet, the knees bending forward.
        if (bLegs)
            for (int32 I = 0; I < 2; ++I)
                Reach(Pose, Leg[I][0], Leg[I][1], Leg[I][2], Goal[I].GetLocation(), At(Pose, Leg[I][1]) + Fw * 60.f, Goal[I].GetRotation());
        // The mace arm reaching for its swing, the elbow out to the right and back, the haft turned along it.
        if (F.HandWeight > .001f)
        {
            const FCompactPoseBoneIndex UA = B(TEXT("upperarm_r")), LA = B(TEXT("lowerarm_r")), H = B(TEXT("hand_r"));
            if (UA != INDEX_NONE && LA != INDEX_NONE && H != INDEX_NONE)
            {
                const FTransform TH = Pose.GetComponentSpaceTransform(H);
                const FVector Cur = TH.TransformVector(F.HaftLocal).GetSafeNormal();   // (with the bone's own scale: the rig's carries a sign)
                const FQuat Q = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Cur, F.HaftDir.GetSafeNormal()), F.HandWeight);
                Reach(Pose, UA, LA, H, FMath::Lerp(TH.GetLocation(), F.HandTarget, F.HandWeight), At(Pose, LA) + Rt * 40.f - Fw * 25.f, Q * TH.GetRotation());
            }
        }
        // The wings: raised and swept at the shoulder, the forearm folded back along the arm, the fingers folded in
        // along the forearm (the outer ones most).
        static const TCHAR* WingNames[2][8] = {
            {TEXT("wing_arm_l"), TEXT("wing_fore_l"), TEXT("wing_f1_l"), TEXT("wing_f2_l"), TEXT("wing_f3_l"), TEXT("wing_t1_l"), TEXT("wing_t2_l"), TEXT("wing_t3_l")},
            {TEXT("wing_arm_r"), TEXT("wing_fore_r"), TEXT("wing_f1_r"), TEXT("wing_f2_r"), TEXT("wing_f3_r"), TEXT("wing_t1_r"), TEXT("wing_t2_r"), TEXT("wing_t3_r")}};
        for (int32 I = 0; I < 2; ++I)
        {
            const float S = I == 0 ? 1.f : -1.f;
            FCompactPoseBoneIndex W[8] = {FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE),
                                          FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE)};
            bool bAll = true;
            for (int32 K = 0; K < 8; ++K) { W[K] = B(WingNames[I][K]); bAll &= W[K] != INDEX_NONE; }
            if (!bAll) continue;
            Turn(Pose, W[0], -Fw * S, F.WingRaise[I]);   // (the left wing, on Lt, rises turning about -Fw)
            Turn(Pose, W[0], Up * S, F.WingSweep[I]);
            {
                const FVector Sh = At(Pose, W[0]), El = At(Pose, W[1]), Wr = At(Pose, W[2]);
                const FVector Axis = FVector::CrossProduct((Wr - El).GetSafeNormal(), (Sh - El).GetSafeNormal()).GetSafeNormal();
                const float Gap = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct((Wr - El).GetSafeNormal(), (Sh - El).GetSafeNormal())), -1.f, 1.f)));
                if (!Axis.IsNearlyZero()) Turn(Pose, W[1], Axis, FMath::Clamp(F.WingFold[I], -40.f, Gap - 14.f));
            }
            const FVector El = At(Pose, W[1]), Wr = At(Pose, W[2]);
            const FVector Back = (El - Wr).GetSafeNormal();
            for (int32 K = 0; K < 3; ++K)
            {
                const FVector Dir = (At(Pose, W[5 + K]) - At(Pose, W[2 + K])).GetSafeNormal();
                const FVector Axis = FVector::CrossProduct(Dir, Back).GetSafeNormal();
                const float Gap = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(Dir, Back)), -1.f, 1.f)));
                if (!Axis.IsNearlyZero()) Turn(Pose, W[2 + K], Axis, FMath::Clamp(F.WingFinger[I] * (.8f + .12f * K), -30.f, Gap - 6.f - 4.f * K));
            }
        }
        // The tail, bone by bone from the hips: raised, then turned to its left.
        static const TCHAR* TailNames[9] = {TEXT("tail_01"), TEXT("tail_02"), TEXT("tail_03"), TEXT("tail_04"), TEXT("tail_05"), TEXT("tail_06"), TEXT("tail_07"), TEXT("tail_08"), TEXT("tail_tip")};
        TArray<FCompactPoseBoneIndex, TInlineAllocator<9>> Tail;
        for (int32 K = 0; K < 9; ++K) Tail.Add(B(TailNames[K]));
        for (int32 K = 0; K < 8; ++K)
        {
            if (Tail[K] == INDEX_NONE || Tail[K + 1] == INDEX_NONE) break;
            const FVector Seg = (At(Pose, Tail[K + 1]) - At(Pose, Tail[K])).GetSafeNormal();
            const FVector Lift = FVector::CrossProduct(Seg, Up).GetSafeNormal();
            if (!Lift.IsNearlyZero()) Turn(Pose, Tail[K], Lift, F.TailPitch[K]);
            Turn(Pose, Tail[K], Up, F.TailYaw[K]);
        }
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
    if (!bLook && Params.Jaw < .05f && Params.Flinch < .05f && FMath::Abs(Params.Twist) < .05f && Params.HeadScale > .999f && !Params.Fiend.bOn) return true;
    // The layers, about the body's own axes: Up, and Side (its left), so that a
    // turn of + about Side tips the facing down.
    const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
    FComponentSpacePoseContext CS(this);
    CS.Pose.InitPose(Output.Pose);
    const FVector Up = Params.Up.GetSafeNormal(), Side = FVector::CrossProduct(Params.Up, Params.Forward).GetSafeNormal();
    if (Params.Fiend.bOn) FiendLayers(CS.Pose, Bones, Params);
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
    if (FMath::Abs(Params.Twist) > .05f)
    {
        // A blow from the side turns the chest about the spine, the head snapping a little further.
        Turn(CS.Pose, Find(Bones, TEXT("spine_02")), Up, Params.Twist * .35f);
        Turn(CS.Pose, Find(Bones, TEXT("spine_03")), Up, Params.Twist * .45f);
        Turn(CS.Pose, Find(Bones, TEXT("head")), Up, Params.Twist * .3f);
    }
    if (Params.HeadScale < .999f)
    {
        const FCompactPoseBoneIndex H = Find(Bones, TEXT("head"));
        if (H != INDEX_NONE)
        {
            FTransform T = CS.Pose.GetComponentSpaceTransform(H);
            T.SetScale3D(FVector(FMath::Max(.001f, Params.HeadScale)));
            const FBoneTransform Moved[] = {FBoneTransform(H, T)};
            CS.Pose.LocalBlendCSBoneTransforms(Moved, 1.f);
        }
    }
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
    const float ClipDt = DeltaSeconds * TimeScale;   // hit-stop freezes the clips, not the springs' clock
    Cur.Time += ClipDt * Cur.Rate;
    Prev.Time += ClipDt * Prev.Rate;
    Fade = FMath::Min(1.f, Fade + DeltaSeconds / FMath::Max(.01f, FadeTime));
    LookNow.X = FMath::FInterpTo(LookNow.X, Look.X, DeltaSeconds, 5.f);
    LookNow.Y = FMath::FInterpTo(LookNow.Y, Look.Y, DeltaSeconds, 5.f);
    Jaw = FMath::FInterpTo(Jaw, JawTarget, DeltaSeconds, 18.f);
    // The flinch: a kick back, then a damped spring home.
    if (FlinchKick > 0.f) { FlinchVel += FlinchKick * 28.f; FlinchKick = 0.f; }
    FlinchVel += (-FlinchNow * 220.f - FlinchVel * 18.f) * DeltaSeconds;
    FlinchNow = FMath::Max(0.f, FlinchNow + FlinchVel * DeltaSeconds);
    if (!FMath::IsNearlyZero(TwistKick)) { TwistVel += TwistKick * 28.f; TwistKick = 0.f; }
    TwistVel += (-TwistNow * 220.f - TwistVel * 18.f) * DeltaSeconds;
    TwistNow += TwistVel * DeltaSeconds;
    if (auto* Anim = Cast<USkeletonAnimInstance>(M->GetAnimInstance()))
    {
        FSkeletonAnimParams& P = Anim->Params;
        P.To = Source(Cur);
        P.From = Fade < 1.f ? Source(Prev) : FSkeletonTrack();
        P.Fade = Fade;
        P.LookYaw = LookNow.X; P.LookPitch = LookNow.Y; P.Jaw = Jaw; P.Flinch = FlinchNow;
        P.Twist = TwistNow; P.HeadScale = HeadScale;
        P.Forward = Forward; P.Up = FVector::UpVector;
        P.Fiend = Fiend;
    }
}
