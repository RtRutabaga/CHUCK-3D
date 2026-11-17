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

namespace
{
    float FortSmooth(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }
    FVector FortFeet(const ATempleHero* H) { return H->GetActorLocation() - FVector(0, 0, ATempleHero::HalfHeight); }
}

void ATempleSkeleton::FortRoar()
{
    bRoaring = true; StateTime = 0.f; ++FortRoars; HitsSinceRoar = 0;
    Fm.bRoarBeat = Fm.bRoarShake = false;
    Sound(RoarSounds, 1.f);
    NextRoarAt = GetWorld()->GetTimeSeconds() + Random.FRandRange(24.f, 36.f);
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
        if (State == EState::Attack) { if (Attack == EAttack::Smash) TickSmash(Chuck, Dt); else TickHurl(Chuck, Dt); return true; }
        Face(FRotator(0, Post.Yaw, 0).Vector(), 40.f, Dt);
        Animator.Locomote(LocoSpeed);
        return true;
    }
    if (State == EState::Guard || State == EState::Dormant || State == EState::Return)
    {
        // The battle on: it roars at them (SetState's roar from Guard) and fights.
        if (State != EState::Guard) SetState(EState::Guard);
        SetState(EState::Chase); ++FortRoars;
        NextRoarAt = W + Random.FRandRange(24.f, 36.f); NextFlareAt = W + Random.FRandRange(5.f, 9.f);
        AttackReadyAt = W + 3.f;
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
        if (Attack == EAttack::Smash) TickSmash(Chuck, Dt);
        else if (Attack == EAttack::Hurl) TickHurl(Chuck, Dt);
        else { Animator.Play(TEXT("Idle"), true, 1.f, false, .3f); SetState(EState::Recover); }
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
    Animator.SetJaw(5.f);
    if (W >= NextRoarAt) { FortRoar(); return true; }
    // A threatening beat of its wings now and then as it stands.
    if (W >= NextFlareAt && Fm.Beats == 0) { Fm.Beats = 1; Fm.BeatPhase = 0.f; Fm.bDownDone = false; NextFlareAt = W + Random.FRandRange(7.f, 12.f); }
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
    // The mace on the fighter, from 1.7 to 2.6 m (its reach), facing him.
    const bool bSmashRange = DF > 150.f && DF < 260.f;
    // Its blows go by turns: after the mace mostly fire, after the fire mostly the mace (2D: across their lanes).
    if (FortForced < 0 && !bChoseBlow) { bChoseBlow = true; bWantSmash = LastAttack == EAttack::Hurl ? Random.FRand() < .7f : Random.FRand() < .3f; }
    const bool bSmashing = FortForced == 0 || (FortForced < 0 && bWantSmash);
    if (bSmashRange && bSmashing && FacingF > .75f)
    {
        FortForced = -1;
        ShowAim = FortFeet(Fighter);
        StartAttack(EAttack::Smash); bChoseBlow = false;
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
        AttackReadyAt = GetWorld()->GetTimeSeconds() + Random.FRandRange(2.6f, 4.5f);
        Animator.Play(TEXT("Idle"), true, 1.f, false, .3f);
        SetState(EState::Recover);
    }
}
