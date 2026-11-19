// The Pit Fiend in the trio's battle before Phlegethos's fortress (Claude, 2026-11-16; the user: "go ahead and do the trio
// encounter"; docs/regions/PHLEGETHOS.md#the-trios-battle). After the 2D game's InfernalBattleChoreographer: the fiend
// "answers across their lanes" - here its mace brought down on the fighter, who holds the ground between it and the breach,
// and fire hurled at the ranger and the wizard in turn; between, it roars and beats its wings. The heroes' arrows stand in
// it and the wizard's bolts burst on it (a flinch, no more). Chuck is not its quarry, but if he comes under it, it swats,
// stamps or smashes at him with the ordinary attacks (TempleSkeleton.cpp), and its blows and fire knock him down near them.
#include "TempleSkeleton.h"
#include "TempleSanctumBattle.h"
#include "TempleHero.h"
#include "ChuckCharacter.h"
#include "SkeletonAnim.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhlegethosSetting.h"
#include "TempleBeholder.h"

namespace
{
    float FortSmooth(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }
    FVector FortFeet(const ATempleHero* H) { return H->GetActorLocation() - FVector(0, 0, ATempleHero::HalfHeight); }
    // (as PitFiendMotion.cpp's: a point along an arc about Pivot from A to B; a direction turned from A toward B)
    FVector FortArc(const FVector& Pivot, const FVector& A, const FVector& B, float T)
    {
        const FVector VA = A - Pivot, VB = B - Pivot;
        const FQuat Q = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenVectors(VA, VB), T);
        return Pivot + Q.RotateVector(VA.GetSafeNormal()) * FMath::Lerp(static_cast<float>(VA.Size()), static_cast<float>(VB.Size()), T);
    }
    FVector FortTurned(const FVector& A, const FVector& B, float T)
    {
        return FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(A.GetSafeNormal(), B.GetSafeNormal()), T).RotateVector(A.GetSafeNormal());
    }
}

void ATempleSkeleton::FortRoar()
{
    bRoaring = true; StateTime = 0.f; ++FortRoars; HitsSinceRoar = 0;
    Fm.bRoarBeat = Fm.bRoarShake = false;
    Sound(RoarSounds, 1.f);
    NextRoarAt = GetWorld()->GetTimeSeconds() + Random.FRandRange(14.f, 20.f);   // (2026-11-17: more often; was 24 to 36 s)
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_FIEND_ROAR roars=%d"), FortRoars);
}

void ATempleSkeleton::FiendStruck(EBlow Kind, const FVector& From, const FVector& At)
{
    // An arrow is a sting, a bolt a burst against its hide, a cut at its leg a bite: it flinches, turned a little by the
    // blow's side, and does not stop.
    ++FortHits; ++HitsSinceRoar;
    const FVector Local = GetActorTransform().InverseTransformVectorNoScale((At - From).GetSafeNormal2D());
    const float Twist = FMath::Clamp(static_cast<float>(-Local.Y) * 6.f, -6.f, 6.f);
    Animator.Flinch(Kind == EBlow::Bolt ? 7.f : Kind == EBlow::Arrow ? 2.5f : 5.f, Twist);
    if (Kind == EBlow::Bolt) Fm.DipVel += 14.f;
    Sound(HitSounds, Kind == EBlow::Arrow ? .35f : .7f);
    // Too many stings: it roars at them.
    if (HitsSinceRoar >= 9 && State != EState::Attack && !bRoaring) FortRoar();
    // (no more than a quiver of arrows left standing in it)
    if (StuckArrows.Num() > 14)
    {
        if (StuckArrows[0]) StuckArrows[0]->DestroyComponent();
        StuckArrows.RemoveAt(0);
    }
}

bool ATempleSkeleton::TickFortress(float Dt, float LocoSpeed)
{
    ATempleSanctumBattle* B = Battle.Get();
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (State == EState::Attack && bOnChuck) return false;
    StateTime += Dt;
    auto* Move = GetCharacterMovement();
    const FVector Here = GetActorLocation();
    const float Sc = KindScale();
    const float W = static_cast<float>(GetWorld()->GetTimeSeconds());
    const float Speed = static_cast<float>(GetVelocity().Size2D());
    ATempleHero* Fighter = B ? B->GetHero(0) : nullptr;
    // Before he comes (or with him gone off), it stands at its post facing the trio.
    if (!B || !B->IsActive())
    {
        Move->MaxWalkSpeed = 0;
        if (State != EState::Guard && State != EState::Attack) { Animator.Play(TEXT("Idle"), true, 1.f, false, .3f); SetState(EState::Guard); }
        if (State == EState::Attack) { TickFortressAttack(Chuck, Dt); return true; }
        Face(FRotator(0, Post.Yaw, 0).Vector(), 40.f, Dt);
        Animator.Locomote(LocoSpeed);
        return true;
    }
    if (State == EState::Guard || State == EState::Dormant || State == EState::Return)
    {
        // The battle on: it roars at them (SetState's roar from Guard) and fights.
        if (State != EState::Guard) SetState(EState::Guard);
        SetState(EState::Chase); ++FortRoars;
        NextRoarAt = W + Random.FRandRange(14.f, 20.f); NextFlareAt = W + Random.FRandRange(3.f, 6.f);
        AttackReadyAt = W + 2.f;
        return true;
    }
    const FVector ChuckAt = Chuck ? Chuck->GetActorLocation() : Here + FVector(1e6, 0, 0);
    const FVector ToChuck = ChuckAt - Here;
    const float ChuckDist = static_cast<float>(ToChuck.Size2D());
    const bool bChuckNear = Chuck && !Chuck->IsAstral() && ChuckDist < 300.f * Sc && CanReach(Chuck);
    // Who it means to strike next: the fighter in its reach, else the one it will throw at.
    ATempleHero* Mark = Fighter;
    if (ATempleHero* H = HurlAt.Get(); H && State == EState::Attack && Attack == EAttack::Hurl) Mark = H;
    const FVector MarkAt = Mark ? Mark->GetActorLocation() : Here + GetActorForwardVector() * 500.f;
    // It watches him if he is under it, else its mark.
    {
        const FVector Look = bChuckNear ? ChuckAt : Mark ? Mark->HeadLocation() : MarkAt;
        const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Look - GetMesh()->GetBoneLocation(TEXT("head")));
        if (!bRoaring)
            Animator.SetLook(FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Local.Y), static_cast<float>(Local.X))), -55.f, 55.f) * -1.f,
                             FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-Local.Z), static_cast<float>(Local.Size2D()))) - 10.f, -15.f, 35.f));
    }
    switch (State)
    {
    case EState::Attack:
        Move->MaxWalkSpeed = 0;
        if (!TickFortressAttack(Chuck, Dt)) { Animator.Play(TEXT("Idle"), true, 1.f, false, .3f); SetState(EState::Recover); }
        return true;
    case EState::Recover:
        Move->MaxWalkSpeed = 0;
        Face((bChuckNear ? ChuckAt : MarkAt) - Here, 60.f, Dt);
        Animator.Locomote(LocoSpeed);
        if (StateTime > .8f) SetState(EState::Chase);   // (from Recover: no roar)
        return true;
    default: break;
    }
    // ---- Chase: its fighting stance.
    if (bRoaring)
    {
        Move->MaxWalkSpeed = 0;
        Face(MarkAt - Here, 40.f, Dt);
        Animator.SetJaw(StateTime > .35f && StateTime < 2.1f ? 38.f : 6.f); Animator.SetLook(0.f, StateTime > .3f && StateTime < 2.2f ? -26.f : 0.f);
        Animator.Locomote(LocoSpeed);
        if (StateTime > 2.6f) bRoaring = false;
        return true;
    }
    // (2026-11-18) Speaking (its Infernal lines): it stands, head up at the trio, and strikes at nothing until it is done.
    if (FortSpeech >= 0.f)
    {
        Move->MaxWalkSpeed = 0;
        Face(MarkAt - Here, 40.f, Dt);
        Animator.SetLook(0.f, -12.f);
        Animator.Locomote(LocoSpeed);
        AttackReadyAt = FMath::Max(AttackReadyAt, W + .6f);
        NextRoarAt = FMath::Max(NextRoarAt, W + 4.f);
        return true;
    }
    Animator.SetJaw(5.f);
    if (W >= NextRoarAt) { FortRoar(); return true; }
    // A threatening beat of its wings now and then as it stands.
    if (W >= NextFlareAt && Fm.Beats == 0) { Fm.Beats = 1; Fm.BeatPhase = 0.f; Fm.bDownDone = false; NextFlareAt = W + Random.FRandRange(4.f, 7.f); }   // (2026-11-17: was 7 to 12 s)
    const bool bReady = W >= AttackReadyAt;
    // Under it: he is vermin at its feet, and it strikes at him as it would in the wastes.
    if (bChuckNear && bReady && FortForced < 0)
    {
        const float Facing = FVector::DotProduct(ToChuck.GetSafeNormal2D(), GetActorForwardVector());
        const EAttack Kind = ChuckDist < 58.f * Sc ? EAttack::Swat : ChuckDist < 90.f * Sc ? (Random.FRand() < .5f ? EAttack::Kick : EAttack::Smash) : ChuckDist < 125.f * Sc ? EAttack::Smash : EAttack::None;
        if (Kind != EAttack::None && Facing > .55f) { bOnChuck = true; ShowAim = FVector::ZeroVector; StartAttack(Kind); return true; }
        if (Kind != EAttack::None) { Move->MaxWalkSpeed = 0; Face(ToChuck, 110.f, Dt); Animator.Locomote(LocoSpeed); return true; }
    }
    bOnChuck = false;
    // Walking to close on the fighter.
    if (!WalkGoal.IsZero())
    {
        const FVector Want = WalkGoal - Here;
        if (Want.Size2D() < 30.f || W > WalkUntil) { WalkGoal = FVector::ZeroVector; Move->MaxWalkSpeed = 0; }
        else
        {
            Move->MaxWalkSpeed = FiendPatrol * 1.3f;
            const FVector Dir = Steer(Want, Chuck);
            AddMovementInput(Dir, FMath::Clamp(static_cast<float>(Want.Size2D()) / 60.f, .3f, 1.f));
            Face(Fighter ? Fighter->GetActorLocation() - Here : Dir, 70.f, Dt);
            Animator.Locomote(LocoSpeed);
            return true;
        }
    }
    Move->MaxWalkSpeed = 0;
    const FVector ToF = Fighter ? Fighter->GetActorLocation() - Here : FVector::ZeroVector;
    const float DF = static_cast<float>(ToF.Size2D());
    const float FacingF = Fighter ? static_cast<float>(FVector::DotProduct(ToF.GetSafeNormal2D(), GetActorForwardVector())) : 0.f;
    Face(bChuckNear ? ToChuck : ToF, 60.f, Dt);
    Animator.Locomote(LocoSpeed);
    if (!bReady || !Fighter) return true;
    // Its blows go by turns: after the fire mostly its own weight at the fighter (the mace brought down, the mace swept
    // round, the leap onto him, the wings' buffet); after those mostly fire (2D: across their lanes). (2026-11-17: the
    // leap, the sweep and the buffet added, the user: "more animations especially from the fiend".)
    // FortBlow: 0 the mace down, 1 fire, 2 the sweep, 3 the buffet, 4 the leap.
    if (FortForced < 0 && !bChoseBlow)
    {
        bChoseBlow = true;
        const float R = Random.FRand();
        FortBlow = LastAttack == EAttack::Hurl ? (R < .3f ? 0 : R < .52f ? 2 : R < .74f ? 4 : R < .9f ? 3 : 1)
                                               : (R < .5f ? 1 : R < .62f ? 0 : R < .74f ? 2 : R < .87f ? 4 : 3);
    }
    int32 Blow = FortForced == 0 ? 0 : FortForced > 0 ? 1 : FortBlow;
    // The leap: from afar, up and over onto the fighter (never into an opening in the ground, never out of its ground).
    if (Blow == 4)
    {
        if (DF > 480.f && DF < 1500.f)
        {
            FVector Goal = Fighter->GetActorLocation() + (Here - Fighter->GetActorLocation()).GetSafeNormal2D() * 240.f;
            Goal.Z = Here.Z;
            if (Post.Leash.IsInside(FVector2D(FrameOf(Goal).X, FrameOf(Goal).Y)) && !B->HoleAt(Goal, RadiusNow() + 80.f))
            {
                Face(ToF, 90.f, Dt);
                if (FacingF > .8f)
                {
                    LeapGoal = Goal; bChoseBlow = false; FortBlow = -1;
                    StartAttack(EAttack::Leap);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_FIEND_LEAP_AT fighter dist=%.0f"), DF);
                }
                return true;
            }
        }
        Blow = DF <= 480.f ? 0 : 1;
    }
    // The buffet: at the trio before it, the fighter within 9 m.
    if (Blow == 3)
    {
        if (DF < 900.f)
        {
            Face(ToF, 90.f, Dt);
            if (FacingF > .8f) { bChoseBlow = false; FortBlow = -1; StartAttack(EAttack::Buffet); UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_FIEND_BUFFET dist=%.0f"), DF); }
            return true;
        }
        Blow = 1;
    }
    // The mace on the fighter, from 1.5 to 2.6 m (its reach), facing him: brought down, or swept round at his waist.
    const bool bSmashRange = DF > (Blow == 2 ? 130.f : 150.f) && DF < (Blow == 2 ? 290.f : 260.f);
    const bool bSmashing = Blow == 0 || Blow == 2;
    if (bSmashRange && bSmashing && FacingF > .75f)
    {
        FortForced = -1; bChoseBlow = false; FortBlow = -1;
        if (Blow == 2)
        {
            StartAttack(EAttack::Sweep);
            B->FiendSmashComing(Fighter, Fighter->GetActorLocation() - FVector(0, 0, ATempleHero::HalfHeight), .78f, .6f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_FIEND_SWEEP_AT fighter dist=%.0f"), DF);
            return true;
        }
        ShowAim = FortFeet(Fighter);
        StartAttack(EAttack::Smash);
        B->FiendSmashComing(Fighter, ShowAim);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_FIEND_SMASH_AT fighter dist=%.0f"), DF);
        return true;
    }
    // Closing on him if he is near: a few heavy steps to bring him into its reach (never far from its post, never into
    // the breach). (Forced to smash him: always.)
    if (bSmashing && DF < 1100.f && DF >= 260.f)
    {
        FVector Goal = Fighter->GetActorLocation() + (Here - Fighter->GetActorLocation()).GetSafeNormal2D() * 215.f;
        const FVector FromPost = Goal - Post.Feet;
        if (FromPost.Size2D() > 520.f) Goal = Post.Feet + FromPost.GetSafeNormal2D() * 520.f;
        Goal.Z = Here.Z;
        if (!B->HoleAt(Goal, RadiusNow() + 60.f)) { WalkGoal = Goal; WalkUntil = W + 4.5f; AttackReadyAt = W + .4f; return true; }
    }
    if (FortForced == 0) return true;   // (turning onto him first)
    // Back toward its post if the fighter has drawn it off.
    if (FortForced < 0 && FVector::Dist2D(Here, Post.Feet) > 420.f && (DF > 300.f || DF < 150.f))
    {
        WalkGoal = FVector(Post.Feet.X, Post.Feet.Y, Here.Z); WalkUntil = W + 5.f; return true;
    }
    // Fire at the wizard or the ranger, in turn.
    ATempleHero* Throw = FortForced == 1 ? B->GetHero(1) : FortForced == 2 ? B->GetHero(2) : B->GetHero(NextHurl % 2 ? 2 : 1);
    if (!Throw) return true;
    const FVector ToT = Throw->GetActorLocation() - Here;
    Face(ToT, 70.f, Dt);
    if (FVector::DotProduct(ToT.GetSafeNormal2D(), GetActorForwardVector()) < .8f) return true;
    FortForced = -1; ++NextHurl;
    HurlAt = Throw; bOnChuck = false; bChoseBlow = false;
    StartAttack(EAttack::Hurl);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_FIEND_HURL_AT %s dist=%.0f"), Throw->GetRoleName(), ToT.Size2D());
    return true;
}

void ATempleSkeleton::TickHurl(AChuckCharacter* Chuck, float DeltaSeconds)
{
    // The left hand drawn back past its hip, fire gathering in it, the chest turning away; then flung over and forward, the
    // body following it down, the fire loosed at its mark; back to its stance.
    FFiendMotion& M = Fm;
    const float T = StateTime, Dt = FMath::Min(DeltaSeconds, .05f);
    constexpr float Gather = .85f, Loose = .98f, Through = 1.12f, Done = 1.9f;
    ATempleSanctumBattle* B = Battle.Get();
    ATempleHero* H = HurlAt.Get();
    if (StateTime <= DeltaSeconds + 1e-3f) { bLoosed = false; bGathering = false; Sound(RattleSounds, .9f); }
    if (H && T < Loose) Face(H->GetActorLocation() - GetActorLocation(), 110.f, Dt);
    if (!bGathering && T > .12f && B) { bGathering = true; B->FireballGather(this); }
    float LeftArm = 0, Twist = 0, Bend = 0, Crouch = 0, WR = 20, WS = -6, WF = 22, WFi = 18, WingW = 0;
    if (T < Gather)
    {
        const float A = FortSmooth(T / Gather);
        LeftArm = 62.f * A; Twist = 24.f * A; Bend = -8.f * A; Crouch = 4.f * A; WingW = .7f * A;
        WR = 24.f; WS = -2.f; WF = 18.f; WFi = 14.f;
    }
    else if (T < Through)
    {
        const float A = FMath::Pow((T - Gather) / (Through - Gather), 1.5f);
        LeftArm = FMath::Lerp(62.f, -84.f, A); Twist = FMath::Lerp(24.f, -22.f, A); Bend = FMath::Lerp(-8.f, 26.f, A); Crouch = FMath::Lerp(4.f, 11.f, A);
        WR = -12.f; WS = 16.f; WF = 8.f; WFi = 4.f; WingW = .8f;
    }
    else
    {
        const float A = FortSmooth((T - Through) / (Done - Through));
        LeftArm = -84.f * (1.f - A); Twist = -22.f * (1.f - A); Bend = 26.f * (1.f - A); Crouch = 11.f * (1.f - A);
        WR = -12.f; WS = 16.f; WF = 8.f; WFi = 4.f; WingW = .8f * (1.f - A);
    }
    M.HandW = FMath::FInterpTo(M.HandW, 0.f, Dt, 6.f);
    M.Twist = Twist; M.Bend = Bend; M.LeftArm = LeftArm; M.Crouch = Crouch;
    M.WR = WR; M.WS = WS; M.WF = WF; M.WFi = WFi; M.WingW = WingW;
    Animator.SetJaw(T > Gather - .2f && T < Through + .3f ? 30.f : 10.f);
    if (!bLoosed && T >= Loose)
    {
        bLoosed = true; ++Hurls; bStruck = true;
        Sound(SwingSounds, 1.f);
        if (B) B->FireballLoose(this, H);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_FIEND_HURL hurls=%d at=%s"), Hurls, H ? H->GetRoleName() : TEXT("-"));
    }
    if (T >= Done)
    {
        ++Strikes;
        AttackReadyAt = GetWorld()->GetTimeSeconds() + Random.FRandRange(1.4f, 2.6f);   // (2026-11-17: was 2.6 to 4.5 s)
        Animator.Play(TEXT("Idle"), true, 1.f, false, .3f);
        SetState(EState::Recover);
    }
}

bool ATempleSkeleton::TickFortressAttack(AChuckCharacter* Chuck, float Dt)
{
    switch (Attack)
    {
    case EAttack::Smash: TickSmash(Chuck, Dt); return true;
    case EAttack::Hurl: TickHurl(Chuck, Dt); return true;
    case EAttack::Leap: TickLeap(Chuck, Dt); return true;
    case EAttack::Sweep: TickSweep(Chuck, Dt); return true;
    case EAttack::Buffet: TickBuffet(Chuck, Dt); return true;
    default: return false;
    }
}

void ATempleSkeleton::TickLeap(AChuckCharacter* Chuck, float DeltaSeconds)
{
    // A crouch, the wings raised; up off its feet on a downstroke, wings spread, the mace raised over its head; down onto
    // the ground before the fighter, the mace brought down with it into the ground; the blast; up out of the crouch.
    FFiendMotion& M = Fm;
    const float T = StateTime, Dt = FMath::Min(DeltaSeconds, .05f);
    USkeletalMeshComponent* SkMesh = GetMesh();
    const FTransform CT = SkMesh->GetComponentTransform();
    const FVector Fw = BodyForward, Up = FVector::UpVector, Left = FVector::CrossProduct(BodyForward, Up);
    auto Body = [&](float X, float Y, float Z) { return Fw * X + Left * Y + Up * Z; };
    ATempleSanctumBattle* B = Battle.Get();
    ATempleHero* Fighter = B ? B->GetHero(0) : nullptr;
    constexpr float Gather = .6f;
    if (!M.bSmashInit)
    {
        M.bSmashInit = true; M.bImpact = false; bLaunched = false; LeapLandT = -1.f;
        const FVector Hand = CT.InverseTransformPosition(SkMesh->GetBoneLocation(TEXT("hand_r")));
        M.HeadFrom = CT.InverseTransformPosition(GetMaceHead());
        M.DirFrom = (M.HeadFrom - Hand).GetSafeNormal();
        M.Target = Body(95.f, -10.f, 4.f);
        Sound(RoarSounds, .8f);
    }
    const FVector Pivot = Body(2.f, -20.f, 150.f);
    const FVector Top = Body(-30.f, -18.f, 262.f), TopDir = Body(-.35f, -.05f, .93f).GetSafeNormal(), HitDir = Body(.40f, -.02f, -.92f).GetSafeNormal();
    FVector Head = Top, Dir = TopDir;
    float W = 1.f, Twist = 0, Bend = 0, LeftArm = 0, Crouch = 0, WR = 0, WS = 0, WF = 0, WFi = 0, WingW = 1.f;
    const bool bGround = GetCharacterMovement()->IsMovingOnGround();
    if (T < Gather)
    {
        // Down into a crouch, the wings coming up, the mace drawn up.
        const float A = FortSmooth(T / Gather);
        Head = FortArc(Pivot, M.HeadFrom, Top, A); Dir = FortTurned(M.DirFrom, TopDir, A); W = FortSmooth(T / .25f);
        Crouch = 24.f * A; Bend = 20.f * A; Twist = -10.f * A; LeftArm = -30.f * A;
        WR = 44.f; WS = -18.f; WF = -2.f; WFi = -6.f; WingW = A;
        if (Fighter) Face(Fighter->GetActorLocation() - GetActorLocation(), 120.f, Dt);
    }
    else if (!bLaunched)
    {
        // Up: a ballistic leap onto its ground, a downstroke of the wings throwing the ash down under it.
        bLaunched = true; ++Leaps;
        const FVector From = GetActorLocation();
        const FVector To = LeapGoal + FVector(0, 0, HalfHeightNow() + 2.f);
        const float G = 980.f * GetCharacterMovement()->GravityScale;
        const float D = static_cast<float>(FVector::Dist2D(From, To));
        const float Tf = FMath::Clamp(D / 850.f, .75f, 1.15f);
        FVector V = (To - From) / Tf; V.Z = static_cast<float>((To.Z - From.Z) + .5f * G * Tf * Tf) / Tf;
        LaunchCharacter(V, true, true);
        M.Beats = 1; M.BeatPhase = .6f; M.bDownDone = false;
        Sound(SwingSounds, 1.f);
        FiendImpact(FVector(From.X, From.Y, From.Z - HalfHeightNow()), .55f, false, 0.f, .3f);
        if (B && Fighter) B->FiendSmashComing(Fighter, LeapGoal, FMath::Max(.1f, Tf - .3f), .7f);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_FIEND_LEAP leaps=%d dist=%.0f flight=%.2f"), Leaps, D, Tf);
    }
    else if (LeapLandT < 0.f)
    {
        // In the air: wings spread, legs drawn up a little, the mace held high; it lands (or, at the latest, 2.2 s on).
        const float A = FortSmooth((T - Gather) / .3f);
        Head = Top; Dir = TopDir;
        Crouch = FMath::Lerp(24.f, 6.f, A); Bend = FMath::Lerp(20.f, -10.f, A); Twist = -14.f; LeftArm = -45.f;
        WR = 34.f; WS = 8.f; WF = -8.f; WFi = -10.f;
        M.LeanAdd = -6.f;
        if ((bGround && T > Gather + .25f) || T > Gather + 2.2f) LeapLandT = T;
    }
    if (LeapLandT >= 0.f)
    {
        const float L = T - LeapLandT;
        if (L < .14f)
        {
            // The mace down with it.
            const float A = FMath::Pow(L / .14f, 1.5f);
            Head = FortArc(Pivot, Top, M.Target, A); Dir = FortTurned(TopDir, HitDir, A);
            Crouch = FMath::Lerp(6.f, 30.f, A); Bend = FMath::Lerp(-10.f, 48.f, A); Twist = FMath::Lerp(-14.f, 16.f, A); LeftArm = FMath::Lerp(-45.f, 40.f, A);
            WR = -28.f; WS = 24.f; WF = 10.f; WFi = 4.f;
        }
        else
        {
            const float A = FortSmooth((L - .5f) / 1.f);
            Head = FortArc(Pivot, M.Target, Body(40.f, -40.f, 80.f), A); Dir = FortTurned(HitDir, Body(.3f, -.1f, 1.f), A); W = 1.f - A;
            Crouch = 30.f * (1.f - A); Bend = 48.f * (1.f - A); Twist = 16.f * (1.f - A); LeftArm = 40.f * (1.f - A);
            WR = -24.f; WS = 18.f; WF = 14.f; WFi = 8.f; WingW = 1.f - A;
        }
        if (!M.bImpact && L >= .12f)
        {
            M.bImpact = true; M.Flash = 1.f; ++M.Smashes; bStruck = true;
            const FVector Ground = CT.TransformPosition(M.Target);
            Sound(HitSounds, 1.f); Sound(StepSounds, 1.f);
            // (him: under the head, dead; near, thrown)
            bool bLanded = false;
            if (Chuck && !Chuck->IsAstral() && FVector::Dist2D(Chuck->GetActorLocation(), Ground) < 70.f && FMath::Abs(static_cast<float>(Chuck->GetActorLocation().Z - Ground.Z)) < 120.f)
                bLanded = Chuck->TakeBite(GetActorLocation(), FiendBlow, false);
            FiendImpact(FVector(Ground.X, Ground.Y, GetActorLocation().Z - HalfHeightNow()), 1.f, true, bLanded ? 0.f : 480.f, 1.f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_FIEND_LEAP_LAND landed_on_chuck=%d"), bLanded ? 1 : 0);
        }
        if (L > 1.6f)
        {
            ++Strikes;
            AttackReadyAt = GetWorld()->GetTimeSeconds() + Random.FRandRange(1.2f, 2.2f);
            Animator.Play(TEXT("Idle"), true, 1.f, false, .3f);
            SetState(EState::Recover);
        }
    }
    M.Hand = Head - Dir * MaceLen; M.Haft = Dir; M.HandW = W;
    M.Twist = Twist; M.Bend = Bend; M.LeftArm = LeftArm; M.Crouch = Crouch;
    M.WR = WR; M.WS = WS; M.WF = WF; M.WFi = WFi; M.WingW = WingW;
    Animator.SetJaw(T > .3f && (LeapLandT < 0.f || T - LeapLandT < .8f) ? 34.f : 8.f);
}

void ATempleSkeleton::TickSweep(AChuckCharacter* Chuck, float DeltaSeconds)
{
    // The mace drawn back past its right hip, the chest turned away; swung round across its front at waist height, the
    // body unwinding behind it, and on past its left side; let go back to its stance.
    FFiendMotion& M = Fm;
    const float T = StateTime, Dt = FMath::Min(DeltaSeconds, .05f);
    USkeletalMeshComponent* SkMesh = GetMesh();
    const FTransform CT = SkMesh->GetComponentTransform();
    const FVector Fw = BodyForward, Up = FVector::UpVector, Left = FVector::CrossProduct(BodyForward, Up);
    auto Body = [&](float X, float Y, float Z) { return Fw * X + Left * Y + Up * Z; };
    ATempleSanctumBattle* B = Battle.Get();
    ATempleHero* Fighter = B ? B->GetHero(0) : nullptr;
    constexpr float Wind = .75f, Hold = .9f, Hit = 1.12f, Through = 1.38f, Done = 2.25f;
    if (!M.bSmashInit)
    {
        M.bSmashInit = true; M.bImpact = false;
        const FVector Hand = CT.InverseTransformPosition(SkMesh->GetBoneLocation(TEXT("hand_r")));
        M.HeadFrom = CT.InverseTransformPosition(GetMaceHead());
        M.DirFrom = (M.HeadFrom - Hand).GetSafeNormal();
        Sound(RattleSounds, .9f);
    }
    if (Fighter && T < Hold) Face(Fighter->GetActorLocation() - GetActorLocation(), 120.f, Dt);
    const FVector Pivot = Body(0.f, -10.f, 140.f);
    const FVector Back = Body(-75.f, -110.f, 150.f), Front = Body(118.f, -5.f, 92.f), Across = Body(25.f, 118.f, 118.f);
    const FVector DBack = Body(-.55f, -.7f, .2f).GetSafeNormal(), DFront = Body(1.f, .05f, -.12f).GetSafeNormal(), DAcross = Body(.15f, 1.f, .15f).GetSafeNormal();
    FVector Head, Dir;
    float W = 1.f, Twist = 0, Bend = 0, LeftArm = 0, Crouch = 0, WR = 0, WS = 0, WF = 0, WFi = 0, WingW = 1.f;
    if (T < Wind)
    {
        const float A = FortSmooth(T / Wind);
        Head = FortArc(Pivot, M.HeadFrom, Back, A); Dir = FortTurned(M.DirFrom, DBack, A); W = FortSmooth(T / .25f);
        Twist = -36.f * A; Bend = 4.f * A; LeftArm = 26.f * A; Crouch = 6.f * A;
        WR = 22.f; WS = -12.f; WF = 6.f; WFi = 2.f; WingW = FortSmooth(T / .4f);
    }
    else if (T < Hold) { Head = Back; Dir = DBack; Twist = -38.f; Bend = 4.f; LeftArm = 26.f; Crouch = 7.f; WR = 24.f; WS = -14.f; WF = 6.f; WFi = 2.f; }
    else if (T < Hit)
    {
        const float A = FMath::Pow((T - Hold) / (Hit - Hold), 1.6f);
        Head = FortArc(Pivot, Back, Front, A); Dir = FortTurned(DBack, DFront, A);
        Twist = FMath::Lerp(-38.f, 8.f, A); Bend = FMath::Lerp(4.f, 14.f, A); LeftArm = FMath::Lerp(26.f, -20.f, A); Crouch = FMath::Lerp(7.f, 11.f, A);
        WR = 6.f; WS = 18.f; WF = 10.f; WFi = 6.f;
    }
    else if (T < Through)
    {
        const float A = FortSmooth((T - Hit) / (Through - Hit));
        Head = FortArc(Pivot, Front, Across, A); Dir = FortTurned(DFront, DAcross, A);
        Twist = FMath::Lerp(8.f, 40.f, A); Bend = 14.f; LeftArm = FMath::Lerp(-20.f, -40.f, A); Crouch = 11.f;
        WR = 6.f; WS = 18.f; WF = 10.f; WFi = 6.f;
    }
    else
    {
        const float A = FortSmooth((T - Through) / (Done - Through));
        Head = FortArc(Pivot, Across, Body(40.f, -40.f, 80.f), A); Dir = FortTurned(DAcross, Body(.3f, -.1f, 1.f), A); W = 1.f - A;
        Twist = 40.f * (1.f - A); Bend = 14.f * (1.f - A); LeftArm = -40.f * (1.f - A); Crouch = 11.f * (1.f - A);
        WR = 6.f; WS = 18.f; WF = 10.f; WFi = 6.f; WingW = 1.f - A;
    }
    M.Hand = Head - Dir * MaceLen; M.Haft = Dir; M.HandW = W;
    M.Twist = Twist; M.Bend = Bend; M.LeftArm = LeftArm; M.Crouch = Crouch;
    M.WR = WR; M.WS = WS; M.WF = WF; M.WFi = WFi; M.WingW = WingW;
    Animator.SetJaw(T > .5f && T < Through ? 28.f : 8.f);
    if (!bSwung && T >= Hold) { bSwung = true; Sound(SwingSounds, 1.f); }
    if (!M.bImpact && T >= Hit)
    {
        // Through what stands before it at its waist: the fighter (out of its way, braced, or thrown), the lemures, him.
        M.bImpact = true; ++MaceSweeps; M.Flash = .6f; bStruck = true;
        const FVector At = CT.TransformPosition(Front);
        Sound(HitSounds, .7f);
        if (B) B->FiendBlast(FVector(At.X, At.Y, GetActorLocation().Z - HalfHeightNow()), .85f);
        BeholderShake(GetWorld(), At, .25f, 2200.f, 0);
        if (Chuck && !Chuck->IsAstral())
        {
            const float D = static_cast<float>(FVector::Dist2D(Chuck->GetActorLocation(), At));
            if (D < 260.f && FMath::Abs(static_cast<float>(Chuck->GetActorLocation().Z - (GetActorLocation().Z - HalfHeightNow()))) < 160.f)
                Chuck->Blasted(GetActorLocation(), FMath::Lerp(700.f, 400.f, D / 260.f), 300.f);
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_FIEND_SWEEP sweeps=%d"), MaceSweeps);
    }
    if (T >= Done)
    {
        ++Strikes;
        AttackReadyAt = GetWorld()->GetTimeSeconds() + Random.FRandRange(1.f, 2.f);
        Animator.Play(TEXT("Idle"), true, 1.f, false, .3f);
        SetState(EState::Recover);
    }
}

void ATempleSkeleton::TickBuffet(AChuckCharacter* Chuck, float DeltaSeconds)
{
    // Reared back, the wings opened wide; two great beats driving the air at the trio (each gust staggering them, throwing
    // the lemures down, the ash torn up off the ground before it); its jaws wide.
    FFiendMotion& M = Fm;
    const float T = StateTime, Dt = FMath::Min(DeltaSeconds, .05f);
    ATempleSanctumBattle* B = Battle.Get();
    ATempleHero* Fighter = B ? B->GetHero(0) : nullptr;
    if (!M.bSmashInit) { M.bSmashInit = true; BuffetBeats0 = M.WingBeats; Sound(RoarSounds, 1.f); ++Buffets; }
    if (Fighter && T < 1.2f) Face(Fighter->GetActorLocation() - GetActorLocation(), 80.f, Dt);
    const float Rear = FortSmooth(T / .6f) * (1.f - FortSmooth((T - 2.9f) / .5f));
    M.LeanAdd = -14.f * Rear; M.Spread = Rear;
    M.HandW = FMath::FInterpTo(M.HandW, 0.f, Dt, 6.f);
    M.Twist = 0.f; M.Bend = -6.f * Rear; M.LeftArm = 10.f * Rear; M.Crouch = 6.f * Rear; M.WingW = 0.f;
    Animator.SetJaw(T > .3f && T < 3.f ? 40.f : 8.f); Animator.SetLook(0.f, -10.f * Rear);
    if (T > .6f && M.Beats == 0 && M.WingBeats == BuffetBeats0) { M.Beats = 2; M.BeatPhase = 0.f; M.bDownDone = false; M.BeatRate = 1.1f; }
    // Each downstroke: the gust (two).
    const int32 Beaten = M.WingBeats - BuffetBeats0;
    if (B && ((Beaten >= 1 && !bStruck) || (Beaten >= 2 && !bSwung)))
    {
        const bool bFirst = !bStruck;
        if (bFirst) bStruck = true; else bSwung = true;
        const FVector Here = GetActorLocation(), Fw = GetActorForwardVector();
        for (int32 R = 0; R < 3; ++R)
            if (ATempleHero* H = B->GetHero(R))
            {
                const FVector To = H->GetActorLocation() - Here;
                const float D = static_cast<float>(To.Size2D());
                if (D < 1000.f && FVector::DotProduct(To.GetSafeNormal2D(), Fw) > .25f) H->TakeFiendBlast(Here, FMath::Clamp(.75f * (1.f - D / 1200.f), .3f, .7f));
            }
        if (APhlegethosSetting* Setting = APhlegethosSetting::Get(GetWorld()))
            for (float D : {220.f, 450.f, 700.f}) Setting->GroundBlow(Here + Fw * D - FVector(0, 0, HalfHeightNow()), .35f, false);
        B->FiendBlast(Here + Fw * 300.f - FVector(0, 0, HalfHeightNow()), .6f);
        BeholderShake(GetWorld(), Here, .3f, 2600.f, 0);
        if (Chuck && !Chuck->IsAstral())
        {
            const FVector To = Chuck->GetActorLocation() - Here;
            if (To.Size2D() < 900.f && FVector::DotProduct(To.GetSafeNormal2D(), Fw) > .3f) Chuck->Blasted(Here, 420.f, 160.f);
        }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FORTRESS_FIEND_BUFFET_GUST first=%d"), bFirst ? 1 : 0);
    }
    if (T >= 3.5f)
    {
        M.BeatRate = .85f;
        ++Strikes;
        AttackReadyAt = GetWorld()->GetTimeSeconds() + Random.FRandRange(1.f, 2.f);
        Animator.Play(TEXT("Idle"), true, 1.f, false, .3f);
        SetState(EState::Recover);
    }
}
