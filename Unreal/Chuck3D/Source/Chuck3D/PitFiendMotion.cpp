// The Pit Fiend's motion (Claude, 2026-11-16, third pass; docs/regions/PHLEGETHOS.md#creatures). The user: "I want the
// fiend to have more realistic leg movement and placement and overall more immersive body movements such as wings beating
// etc, since the pit fiend will be fighting the trio eventually I want it to really feel cool and realistic and look
// formidable in battle, not just a lumbering skeleton with dressing".
//
// The skeleton's motion-capture clips still move its body; this sets the layers over them each tick (FFiendPose, applied
// in SkeletonAnim.cpp):
// * Its feet go onto the ground actually under them (the wastes are broken ground), the soles laid on its slope; the hips
//   come down into a heavy, bent-kneed stance (further where a foot's ground is lower), and dip as each foot comes down
//   (a tread, a puff of ash, the ground shaking a little near it). It steps round as it turns (TempleSkeleton.cpp).
// * It leans into its going and rolls into its turns, and breathes.
// * Its wings (their own bones, Tools/build_phleg_creatures.py) lie folded on its back on its round, half open as it comes
//   for him, spread in its roar and over a swing; they beat - up slow and part folded, down hard and spread - twice in
//   the roar and now and then as it charges, the downstroke throwing up the ash under it; they jolt at each footfall.
// * Its tail sways with its stride, trails its turns, lifts off the ground as it charges and lies along it at rest (kept
//   out of the ground), and now and then flicks.
// * The roar: reared back, head up, jaws wide, wings spread and beating.
// * The mace (EAttack::Smash): swung up high over its right shoulder, the chest turning away and the wings flaring, held,
//   then brought down in an arc gathering speed into the ground in front of it, the body bending into it; the ground
//   bursts (ash, embers, a flare of the mace's light, the view shaken); left in the ground a moment, then dragged up.
//   Under it, he is dead (a roll at the right moment saves him); near it, the blast knocks him off his feet.
#include "TempleSkeleton.h"
#include "TempleSanctumBattle.h"
#include "TempleHero.h"
#include "ChuckCharacter.h"
#include "PhlegethosSetting.h"
#include "TempleBeholder.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    float Smooth01(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }

    // A point along an arc about Pivot from A to B (component space): its direction turned, its reach between the two.
    FVector Arc(const FVector& Pivot, const FVector& A, const FVector& B, float T)
    {
        const FVector VA = A - Pivot, VB = B - Pivot;
        const FQuat Q = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenVectors(VA, VB), T);
        return Pivot + Q.RotateVector(VA.GetSafeNormal()) * FMath::Lerp(static_cast<float>(VA.Size()), static_cast<float>(VB.Size()), T);
    }

    FVector Turned(const FVector& A, const FVector& B, float T)
    {
        return FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(A.GetSafeNormal(), B.GetSafeNormal()), T).RotateVector(A.GetSafeNormal());
    }

    float RefAnkleZ(const USkeletalMeshComponent* Mesh)
    {
        const USkeletalMesh* M = Mesh ? Cast<USkeletalMesh>(Mesh->GetSkinnedAsset()) : nullptr;
        if (!M) return 0.f;
        const FReferenceSkeleton& Ref = M->GetRefSkeleton();
        int32 I = Ref.FindBoneIndex(TEXT("foot_l"));
        if (I == INDEX_NONE) return 0.f;
        FTransform T = Ref.GetRefBonePose()[I];
        for (int32 P = Ref.GetParentIndex(I); P != INDEX_NONE; P = Ref.GetParentIndex(P)) T = T * Ref.GetRefBonePose()[P];
        return static_cast<float>(T.GetLocation().Z);
    }
}

void ATempleSkeleton::TickFiend(float DeltaSeconds, float Speed)
{
    FFiendMotion& M = Fm;
    FFiendPose& P = Animator.Fiend;
    const float Dt = FMath::Clamp(DeltaSeconds, 1e-4f, .05f);
    M.Clock += Dt;
    P.bOn = true;
    P.HaftLocal = MaceLocal.IsNearlyZero() ? FVector::UpVector : MaceLocal.GetSafeNormal();
    USkeletalMeshComponent* SkMesh = GetMesh();
    UWorld* World = GetWorld();
    const FTransform CT = SkMesh->GetComponentTransform();
    const float Sc = KindScale(), Base = FeetZ();
    const float Moving = FMath::Clamp(Speed / FiendChase, 0.f, 1.f);
    const bool bAlert = State == EState::Chase || State == EState::Attack || State == EState::Recover;
    const bool bSmash = State == EState::Attack && (Attack == EAttack::Smash || Attack == EAttack::Hurl || Attack == EAttack::Leap || Attack == EAttack::Sweep || Attack == EAttack::Buffet);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(FiendGround), false, this);
    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0)) Q.AddIgnoredActor(Pawn);
    for (const TWeakObjectPtr<ATempleSkeleton>& S : All()) if (S.IsValid()) Q.AddIgnoredActor(S.Get());
    auto Ground = [&](const FVector& At, float& Z, FVector& N)
    {
        FHitResult G;
        if (!World->LineTraceSingleByChannel(G, FVector(At.X, At.Y, Base + 170.f), FVector(At.X, At.Y, Base - 260.f), ECC_Pawn, Q)) return false;
        Z = static_cast<float>(G.ImpactPoint.Z); N = G.ImpactNormal; return true;
    };

    // ---- The feet onto the ground under them; the footfalls.
    static const FName Ankles[2] = {TEXT("foot_l"), TEXT("foot_r")};
    static float AnkleRest = -1.f;
    if (AnkleRest < 0.f) AnkleRest = RefAnkleZ(SkMesh);
    float Lowest = 0.f;
    for (int32 I = 0; I < 2; ++I)
    {
        const FVector A = SkMesh->GetBoneLocation(Ankles[I]);
        float Z = Base; FVector N = FVector::UpVector;
        const bool bHit = Ground(A, Z, N);
        M.Lift[I] = FMath::FInterpTo(M.Lift[I], FMath::Clamp((Z - Base) / Sc, -55.f, 30.f), Dt, 16.f);
        // (the sole laid on the slope, up to 22 degrees of it)
        FVector NL = CT.InverseTransformVectorNoScale(N).GetSafeNormal();
        const float Slope = FMath::Acos(FMath::Clamp(static_cast<float>(NL.Z), -1.f, 1.f)), Most = FMath::DegreesToRadians(22.f);
        if (Slope > Most) NL = Turned(FVector::UpVector, NL, Most / Slope);
        M.Normal[I] = (M.Normal[I] + (NL - M.Normal[I]) * FMath::Min(1.f, Dt * 10.f)).GetSafeNormal();
        P.FootLift[I] = M.Lift[I]; P.FootNormal[I] = M.Normal[I];
        Lowest = FMath::Min(Lowest, M.Lift[I]);
        // A footfall: the ankle (its height over its own ground, the clip's step) coming down onto the ground.
        const float H = (static_cast<float>(A.Z) - Z) / Sc;
        M.Stand[I] = FMath::Min(M.Stand[I] + Dt * 1.5f, H);
        if (H > M.Stand[I] + 7.f) M.bAir[I] = true;
        else if (M.bAir[I] && H < M.Stand[I] + 2.5f) { M.bAir[I] = false; FiendPlant(I, FVector(A.X, A.Y, Z)); }
        // (the checks: a planted foot's ankle where it stands on flat ground, not floating or sunk)
        if (bHit && !M.bAir[I] && Speed > 40.f && AnkleRest > 0.f) M.WorstFootGap = FMath::Max(M.WorstFootGap, FMath::Abs(H - AnkleRest) * Sc);
    }

    // ---- The hips: a heavy, bent-kneed stance, lower still where a foot's ground is lower; the dip of each footfall.
    M.DipVel += (-M.Dip * 170.f - M.DipVel * 15.f) * Dt;
    M.Dip += M.DipVel * Dt;
    P.PelvisDrop = 3.f + 3.5f * Moving + FMath::Max(0.f, -Lowest) + M.Dip + M.Crouch;
    P.Stance = 2.5f;
    P.PelvisShift = FVector::ZeroVector;

    // ---- The roar: reared back, wings spread, two beats.
    const bool bRoar = bRoaring && State == EState::Chase;
    if (bRoar)
    {
        const float T = StateTime;
        const float Rear = Smooth01((T - .1f) / .45f) * (1.f - Smooth01((T - 1.9f) / .6f));
        M.LeanAdd = -22.f * Rear; M.Spread = Rear;
        if (!M.bRoarShake && T > .4f) { M.bRoarShake = true; BeholderShake(World, GetActorLocation(), .35f, 3200.f, 2); }
        if (!M.bRoarBeat && T > .7f) { M.bRoarBeat = true; M.Beats = 2; M.BeatPhase = 0.f; M.bDownDone = false; }
    }
    else
    {
        M.LeanAdd = FMath::FInterpTo(M.LeanAdd, 0.f, Dt, 3.f);
        M.Spread = FMath::FInterpTo(M.Spread, 0.f, Dt, 1.5f);
        M.bRoarBeat = M.bRoarShake = false;
    }

    // ---- Leaning into its going and its turns; breathing.
    const float Yaw = static_cast<float>(GetActorRotation().Yaw);
    M.YawRate = FMath::FInterpTo(M.YawRate, FMath::FindDeltaAngleDegrees(M.LastYaw, Yaw) / Dt, Dt, 5.f);
    M.LastYaw = Yaw;
    M.Lean = FMath::FInterpTo(M.Lean, 3.f + 9.f * Moving + M.LeanAdd, Dt, 3.5f);
    M.Roll = FMath::FInterpTo(M.Roll, FMath::Clamp(-M.YawRate * (.2f + Moving) * .05f, -7.f, 7.f), Dt, 3.f);
    P.Lean = M.Lean; P.Roll = M.Roll;
    P.Breath = FMath::Sin(M.Clock * 2.f * PI * (bAlert ? .32f : .2f));

    // ---- The swing's arm and body (TickSmash sets them; otherwise they ease back to the clips).
    if (!bSmash)
    {
        M.HandW = FMath::FInterpTo(M.HandW, 0.f, Dt, 4.f);
        M.Twist = FMath::FInterpTo(M.Twist, 0.f, Dt, 4.f); M.Bend = FMath::FInterpTo(M.Bend, 0.f, Dt, 4.f);
        M.LeftArm = FMath::FInterpTo(M.LeftArm, 0.f, Dt, 4.f); M.Crouch = FMath::FInterpTo(M.Crouch, 0.f, Dt, 4.f);
        M.WingW = FMath::FInterpTo(M.WingW, 0.f, Dt, 3.f);
    }
    P.HandWeight = M.HandW; P.HandTarget = M.Hand; P.HaftDir = M.Haft;
    P.ChestTwist = M.Twist; P.ChestBend = M.Bend; P.LeftArm = M.LeftArm;
    M.Flash = FMath::Max(0.f, M.Flash - Dt * 3.f);

    // ---- The wings: folded on its back, half open coming for him, spread in the roar; the swing's own; beats on top.
    float R = -14.f, Sw = -24.f, Fo = 72.f, Fi = 68.f;
    if (bAlert) { R = 2.f; Sw = -10.f; Fo = 34.f; Fi = 30.f; }
    R = FMath::Lerp(R, 26.f, M.Spread); Sw = FMath::Lerp(Sw, 4.f, M.Spread); Fo = FMath::Lerp(Fo, -6.f, M.Spread); Fi = FMath::Lerp(Fi, -8.f, M.Spread);
    R = FMath::Lerp(R, M.WR, M.WingW); Sw = FMath::Lerp(Sw, M.WS, M.WingW); Fo = FMath::Lerp(Fo, M.WF, M.WingW); Fi = FMath::Lerp(Fi, M.WFi, M.WingW);
    if (State == EState::Chase && !bRoaring && Moving > .45f && M.Beats == 0 && M.Clock > M.NextBeat)
    {
        M.Beats = 1; M.BeatPhase = 0.f; M.bDownDone = false; M.NextBeat = M.Clock + Random.FRandRange(2.6f, 4.2f);
    }
    float BR = 0.f, BS = 0.f, BF = 0.f, BFi = 0.f;
    if (M.Beats > 0)
    {
        M.BeatPhase += Dt * M.BeatRate;
        // (up slow, down fast)
        const float U = M.BeatPhase < .55f ? M.BeatPhase / .55f * .5f : .5f + (M.BeatPhase - .55f) / .45f * .5f;
        const float S = FMath::Sin(2.f * PI * U);
        BR = 36.f * S; BS = -12.f * S; BF = 26.f * FMath::Max(0.f, S); BFi = 22.f * FMath::Max(0.f, S) - 10.f * FMath::Max(0.f, -S);
        if (!M.bDownDone && U > .72f)
        {
            // The downstroke's end: the air driven down at the ground under it, the body lifted a little.
            M.bDownDone = true; ++M.WingBeats;
            M.DipVel -= 22.f;
            Sound(SwingSounds, 1.f);
            FiendImpact(FVector(GetActorLocation().X, GetActorLocation().Y, Base), .45f, false, 0.f, .12f);
        }
        if (M.BeatPhase >= 1.f) { M.BeatPhase = 0.f; M.bDownDone = false; --M.Beats; }
    }
    M.JigVel += (-M.Jig * 240.f - M.JigVel * 6.f) * Dt;
    M.Jig += M.JigVel * Dt;
    const float Ease = M.WingW > .05f ? 9.f : 3.f;
    for (int32 I = 0; I < 2; ++I)
    {
        const float Life = 1.5f * FMath::Sin(M.Clock * .8f + I * 1.7f);
        M.Raise[I] = FMath::FInterpTo(M.Raise[I], R + Life, Dt, Ease); M.Sweep[I] = FMath::FInterpTo(M.Sweep[I], Sw, Dt, Ease);
        M.Fold[I] = FMath::FInterpTo(M.Fold[I], Fo, Dt, Ease); M.Finger[I] = FMath::FInterpTo(M.Finger[I], Fi, Dt, Ease);
        P.WingRaise[I] = M.Raise[I] + BR + M.Jig + P.Breath * 1.5f;
        P.WingSweep[I] = M.Sweep[I] + BS;
        P.WingFold[I] = M.Fold[I] + BF;
        P.WingFinger[I] = M.Finger[I] + BFi;
    }

    // ---- The tail: swaying with its stride, trailing its turns, lifted as it charges, now and then flicked; kept out of
    // the ground.
    const bool bWalk = Animator.IsWalking();
    const float Ph = Animator.GetPhase();
    if (!bAlert && Speed < 20.f && M.FlickT < 0.f && M.Clock > M.NextFlick) { M.FlickT = 0.f; M.FlickSide = Random.FRand() < .5f ? 1.f : -1.f; }
    float Flick = 0.f;
    if (M.FlickT >= 0.f)
    {
        M.FlickT += Dt;
        Flick = FMath::Sin(PI * FMath::Min(1.f, M.FlickT / 1.8f));
        if (M.FlickT > 1.8f) { M.FlickT = -1.f; M.NextFlick = M.Clock + Random.FRandRange(4.f, 9.f); }
    }
    for (int32 K = 0; K < 8; ++K)
    {
        const float W = (K + 1) / 8.f;
        const float Sway = bWalk ? 4.5f * FMath::Sin(2.f * PI * Ph - K * .55f) * (.35f + .65f * W) : 2.5f * FMath::Sin(M.Clock * 1.1f - K * .5f) * (.3f + .7f * W);
        const float Trail = FMath::Clamp(-M.YawRate * .03f * (.4f + W), -8.f, 8.f);
        const float TYaw = Sway + Trail + (K >= 4 ? Flick * 9.f * M.FlickSide : 0.f) + (bSmash ? M.Twist * -.12f : 0.f);
        const float TLift = Moving * (K < 3 ? 6.f : K < 5 ? 0.f : -3.f) + (K >= 5 ? Flick * 10.f : 0.f) + M.TailFix[K];
        const float Lag = FMath::Max(1.5f, 6.f - K * .45f);
        M.TailYaw[K] = FMath::FInterpTo(M.TailYaw[K], TYaw, Dt, Lag);
        M.TailPitch[K] = FMath::FInterpTo(M.TailPitch[K], TLift, Dt, Lag * 1.6f);
        P.TailYaw[K] = M.TailYaw[K]; P.TailPitch[K] = M.TailPitch[K];
    }
    static const FName TailBones[9] = {TEXT("tail_01"), TEXT("tail_02"), TEXT("tail_03"), TEXT("tail_04"), TEXT("tail_05"), TEXT("tail_06"), TEXT("tail_07"), TEXT("tail_08"), TEXT("tail_tip")};
    const float Seg = 190.f / 8.f * Sc;
    float Under = -1e9f;
    for (int32 K = 2; K < 9; ++K)
    {
        const FVector J = SkMesh->GetBoneLocation(TailBones[K]);
        float Z = 0.f; FVector N;
        if (J.IsZero() || !Ground(J, Z, N)) continue;
        const float Rad = (7.5f - 6.4f * K / 8.f) * Sc * .8f;
        const float Depth = Z + Rad - static_cast<float>(J.Z);   // > 0: into the ground
        Under = FMath::Max(Under, Depth);
        float& Fix = M.TailFix[K - 1];
        if (Depth > 0.f) Fix = FMath::Min(Fix + FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Depth / Seg, 0.f, 1.f))) * Dt * 8.f, 40.f);
        else if (Depth < -6.f) Fix = FMath::Max(0.f, Fix - Dt * 6.f);
    }
    if (M.Clock > 2.f) M.WorstTailUnder = FMath::Max(M.WorstTailUnder, Under);
}

void ATempleSkeleton::ShowRoar()
{
    if (!IsFiend()) return;
    if (State != EState::Guard && State != EState::Return) SetState(EState::Guard);
    SetState(EState::Chase);
}

void ATempleSkeleton::ShowSmash(const FVector& At)
{
    if (!IsFiend()) return;
    ShowAim = At;
    StartAttack(EAttack::Smash);
}

void ATempleSkeleton::FiendPlant(int32 Foot, const FVector& At)
{
    // The weight coming down onto it: the hips dip, the wings jolt, the tread, a puff of ash, the ground shaken near it.
    ++Fm.Plants;
    const float Hard = FMath::Clamp(.35f + static_cast<float>(GetVelocity().Size2D()) / FiendChase * .65f, .35f, 1.f);
    Fm.DipVel += 26.f * Hard;
    Fm.JigVel -= 30.f * Hard;
    Sound(StepSounds, .7f + .3f * Hard);
    FiendImpact(At, .14f + .14f * Hard, false, 0.f, .08f + .12f * Hard);
}

void ATempleSkeleton::FiendImpact(const FVector& At, float Strength, bool bEmbers, float BlastRadius, float Shake)
{
    if (APhlegethosSetting* Setting = APhlegethosSetting::Get(GetWorld())) Setting->GroundBlow(At, Strength, bEmbers);
    if (Shake > 0.f) BeholderShake(GetWorld(), At, Shake, 1200.f + 2400.f * Shake, Shake > .7f ? 1 : 0);
    if (ATempleSanctumBattle* B = Battle.Get()) B->FiendBlast(At, Strength);   // (the trio's battle: the heroes near it)
    if (BlastRadius <= 0.f) return;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Chuck) return;
    const float D = static_cast<float>(FVector::Dist2D(Chuck->GetActorLocation(), At));
    if (D > BlastRadius || FMath::Abs(static_cast<float>(Chuck->GetActorLocation().Z - At.Z)) > 160.f) return;
    const float K = D / BlastRadius;
    if (Chuck->Blasted(At, FMath::Lerp(620.f, 320.f, K), FMath::Lerp(420.f, 260.f, K))) ++Fm.Blasts;
}

void ATempleSkeleton::TickSmash(AChuckCharacter* Chuck, float DeltaSeconds)
{
    FFiendMotion& M = Fm;
    const float T = StateTime, Dt = FMath::Min(DeltaSeconds, .05f);
    USkeletalMeshComponent* SkMesh = GetMesh();
    const FTransform CT = SkMesh->GetComponentTransform();
    const FVector Fw = BodyForward, Up = FVector::UpVector, Left = FVector::CrossProduct(BodyForward, Up);   // (left-handed axes: Fw x Up is its left)
    auto Body = [&](float X, float Y, float Z) { return Fw * X + Left * Y + Up * Z; };
    // (seconds: up over its shoulder, held at the top, down into the ground, left there, dragged up)
    constexpr float Raise = .95f, Hold = 1.1f, Hit = 1.32f, Stay = 1.8f, Done = 2.75f;
    if (!M.bSmashInit)
    {
        M.bSmashInit = true; M.bImpact = false;
        const FVector Hand = CT.InverseTransformPosition(SkMesh->GetBoneLocation(TEXT("hand_r")));
        M.HeadFrom = CT.InverseTransformPosition(GetMaceHead());
        M.DirFrom = (M.HeadFrom - Hand).GetSafeNormal();
        M.Target = Body(88.f, -10.f, 4.f);
        Sound(RattleSounds, .9f);   // a growl: the tell
    }
    // Winding up it turns onto him and chooses where the head will come down (his feet, within its reach, a little to its
    // right, the mace's side); from the top of the swing it is committed.
    if (T < Hold && (Chuck || !ShowAim.IsZero()))
    {
        const FVector Feet = !ShowAim.IsZero() ? ShowAim : Chuck->GetActorLocation() - FVector(0, 0, Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const FVector To = Feet - GetActorLocation();
        Face(To.GetSafeNormal2D().RotateAngleAxis(-6.f, FVector::UpVector), 150.f, Dt);
        const FVector L = CT.InverseTransformPosition(Feet);
        const float X = FMath::Clamp(static_cast<float>(FVector::DotProduct(L, Fw)), 72.f, 102.f), Y = FMath::Clamp(static_cast<float>(FVector::DotProduct(L, Left)), -26.f, 6.f);
        const FVector W = CT.TransformPosition(Body(X, Y, 0.f));
        FHitResult G;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(FiendSmashGround), false, this); if (Chuck) Q.AddIgnoredActor(Chuck);
        // (the ground, not a lemure or a hero standing on it)
        for (const TWeakObjectPtr<ATempleSkeleton>& S : All()) if (S.IsValid()) Q.AddIgnoredActor(S.Get());
        if (ATempleSanctumBattle* B = Battle.Get()) for (int32 R = 0; R < 3; ++R) if (ATempleHero* H = B->GetHero(R)) Q.AddIgnoredActor(H);
        float Z = 0.f;
        if (GetWorld()->LineTraceSingleByChannel(G, W + FVector(0, 0, 250.f), W - FVector(0, 0, 300.f), ECC_Pawn, Q))
            Z = static_cast<float>(FVector::DotProduct(CT.InverseTransformPosition(G.ImpactPoint), Up));
        M.Target = Body(X, Y, 0.f) + Up * (Z + 4.f);
    }
    const FVector Pivot = Body(2.f, -20.f, 150.f);
    const FVector Top = Body(-44.f, -22.f, 250.f), Gathered = Top + Body(-6.f, 0.f, -3.f), TopDir = Body(-.45f, -.06f, .89f).GetSafeNormal();
    const FVector HitDir = Body(.40f, -.02f, -.92f).GetSafeNormal();
    FVector Head, Dir;
    float W = 1.f, Twist = 0.f, Bend = 0.f, LeftArm = 0.f, Crouch = 0.f, WR = 0.f, WS = 0.f, WF = 0.f, WFi = 0.f, WingW = 1.f;
    if (T < Raise)
    {
        // Up over its right shoulder, the chest turning away, the left arm thrown forward, the wings flaring up.
        const float A = Smooth01(T / Raise);
        Head = Arc(Pivot, M.HeadFrom, Top, A); Dir = Turned(M.DirFrom, TopDir, A); W = Smooth01(T / .3f);
        Twist = -26.f * A; Bend = -12.f * A; LeftArm = -40.f * A; Crouch = 2.f * A;
        WR = 38.f; WS = -14.f; WF = -4.f; WFi = -6.f; WingW = Smooth01(T / .4f);
    }
    else if (T < Hold)
    {
        // The top: gathered, a little further back.
        const float A = (T - Raise) / (Hold - Raise);
        Head = Arc(Pivot, Top, Gathered, A); Dir = TopDir;
        Twist = -26.f - 4.f * A; Bend = -12.f - 2.f * A; LeftArm = -40.f; Crouch = 2.f + 2.f * A;
        WR = 40.f; WS = -16.f; WF = -4.f; WFi = -6.f;
    }
    else if (T < Hit)
    {
        // Down, gathering speed all the way into the ground, the body bending into it, the wings sweeping down.
        const float A = FMath::Pow((T - Hold) / (Hit - Hold), 2.f);
        Head = Arc(Pivot, Gathered, M.Target, A); Dir = Turned(TopDir, HitDir, A);
        Twist = FMath::Lerp(-30.f, 18.f, A); Bend = FMath::Lerp(-14.f, 50.f, A); LeftArm = FMath::Lerp(-40.f, 38.f, A); Crouch = FMath::Lerp(4.f, 15.f, A);
        WR = -30.f; WS = 24.f; WF = 10.f; WFi = 4.f;
    }
    else if (T < Stay)
    {
        // In the ground, the shock up its arm.
        const float A = Smooth01((T - Hit) / (Stay - Hit));
        Head = M.Target + Up * 2.f * FMath::Sin(T * 60.f) * (1.f - A); Dir = HitDir;
        Twist = 18.f; Bend = 50.f - 4.f * A; LeftArm = 38.f; Crouch = 15.f - 4.f * A;
        WR = -24.f; WS = 18.f; WF = 14.f; WFi = 8.f;
    }
    else
    {
        // Dragged up out of the ground and let go back to its stance.
        const float A = Smooth01((T - Stay) / (Done - Stay));
        Head = Arc(Pivot, M.Target, Body(40.f, -40.f, 80.f), A); Dir = Turned(HitDir, Body(.3f, -.1f, 1.f), A); W = 1.f - A;
        Twist = 18.f * (1.f - A); Bend = 46.f * (1.f - A); LeftArm = 38.f * (1.f - A); Crouch = 11.f * (1.f - A);
        WR = -24.f; WS = 18.f; WF = 14.f; WFi = 8.f; WingW = 1.f - A;
    }
    M.Hand = Head - Dir * MaceLen; M.Haft = Dir; M.HandW = W;
    M.Twist = Twist; M.Bend = Bend; M.LeftArm = LeftArm; M.Crouch = Crouch;
    M.WR = WR; M.WS = WS; M.WF = WF; M.WFi = WFi; M.WingW = WingW;
    Animator.SetJaw(T > .5f && T < Stay ? 30.f : 8.f);
    if (!bSwung && T >= Hold + .05f) { bSwung = true; Sound(SwingSounds, 1.f); }
    // The blow.
    if (!M.bImpact && T >= Hit)
    {
        M.bImpact = true; ++M.Smashes; M.Flash = 1.f; bStruck = true;
        // (Judged at the head's planned landing place: the bones read here lag the swing by a frame or two - at this speed
        // the measured head was still a metre up - and the mace is there a moment later.)
        const FVector Ground = CT.TransformPosition(M.Target);
        const FVector At = Ground + FVector(0, 0, 10.f * KindScale());
        bool bLanded = false;
        float Gap = 1e9f;
        if (Chuck)
        {
            const float Half = Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), R = Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius();
            const FVector C = Chuck->GetActorLocation();
            Gap = FMath::Sqrt(static_cast<float>(FMath::PointDistToSegmentSquared(At, C - FVector(0, 0, Half - R), C + FVector(0, 0, Half - R)))) - R;
            LastStrikeGap = Gap;
            // (the head is some 70 cm across: within it, he is under it)
            if (Gap < 38.f && CanReach(Chuck)) bLanded = Chuck->TakeBite(GetActorLocation(), FiendBlow, false);
        }
        if (bLanded) ++Landed[static_cast<int32>(EAttack::Smash)];
        Sound(HitSounds, 1.f); Sound(StepSounds, 1.f);
        FiendImpact(FVector(At.X, At.Y, Ground.Z), 1.f, true, bLanded ? 0.f : 420.f, 1.f);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_PHLEG_FIEND_SMASH landed=%d gap=%.0f blasts=%d head=%s ground=%.0f"), bLanded ? 1 : 0, Gap, Fm.Blasts, *At.ToCompactString(), Ground.Z);
    }
    if (T >= Done)
    {
        ShowAim = FVector::ZeroVector;
        ++Strikes;
        AttackReadyAt = GetWorld()->GetTimeSeconds() + Random.FRandRange(1.2f, 2.4f);
        Animator.Play(TEXT("Idle"), true, 1.f, false, .3f);
        SetState(EState::Recover);
    }
}
