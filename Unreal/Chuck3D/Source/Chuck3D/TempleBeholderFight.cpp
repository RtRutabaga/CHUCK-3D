// The beholder's fight (user 2026-10-25; docs/CHULT-TEMPLE-BEHOLDER.md#the-fight): its eye rays, the bite, the
// central eye's cone, and how it moves between them. Effects are drawn in TempleBeholderFx.cpp.
#include "TempleBeholder.h"
#include "ChuckCharacter.h"
#include "ChultTempleInterior.h"
#include "ChultTempleWind.h"
#include "TempleHero.h"
#include "TempleSanctumBattle.h"
#include "TempleSkeleton.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    using ESound = ATempleSanctumBattle::ESound;

    /** How much of the cone's horizontal throw Chuck can take without landing in, or over, the sanctum's broken floor
     *  (user 2026-10-25: flung onto a pier's island in the rupture, trapped there). His arc and his slide after it,
     *  sampled every 10 cm, must keep his capsule plus a margin off every rupture and every island (an island is
     *  floor, but there is no way back from one). The fraction of the throw (0..1), the reach it allows in Reach. */
    float SafeFling(const AChuckCharacter* Chuck, const FVector& Vel, float& Reach)
    {
        const UCharacterMovementComponent* M = Chuck->GetCharacterMovement();
        const float G = FMath::Max(100.f, -M->GetGravityZ());
        const float Air = 2.f * FMath::Max(0.f, static_cast<float>(Vel.Z)) / G;
        const FVector2D H(Vel.X, Vel.Y);
        const float Speed = static_cast<float>(H.Size());
        if (Speed < 1.f) { Reach = 0.f; return 1.f; }
        const FVector2D Dir = H / Speed;
        // After landing he skids on, braking at least at his walking stop rate.
        const float Brake = FMath::Max(300.f, M->BrakingDecelerationWalking + M->GroundFriction * 100.f);
        const float Grow = Chuck->GetCapsuleComponent()->GetScaledCapsuleRadius() + 35.f;
        const FVector Start = Chuck->GetActorLocation();
        // Only on the hall's floor: the gallery above overhangs the rupture, and a throw there stays on the gallery.
        const float Feet = static_cast<float>(ChultTempleInteriorFrameOf(Start).Z) - Chuck->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        if (Feet > TempleWind::HZ + 150.f) { Reach = 0.f; return 1.f; }
        auto OffFloor = [&](const FVector& At)
        {
            const FVector F = ChultTempleInteriorFrameOf(At);
            return ChultTempleSanctumHoleAt(FVector2D(F.X, F.Y), Grow) != 0 || ChultTempleSanctumOnIsland(FVector2D(F.X, F.Y), -Grow);
        };
        for (float Frac = 1.f; Frac > .01f; Frac -= .05f)
        {
            const float V = Speed * Frac;
            const float D = V * Air + V * V / (2.f * Brake);
            bool bSafe = true;
            for (float S = 0.f; S <= D + 1.f && bSafe; S += 10.f) bSafe = !OffFloor(Start + FVector(Dir.X, Dir.Y, 0.f) * S);
            if (bSafe) { Reach = D; return Frac; }
        }
        Reach = 0.f;
        return 0.f;
    }
    float Ease(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }
    float Approach(float From, float To, float Rate, float Dt) { return FMath::Lerp(From, To, 1.f - FMath::Exp(-Rate * Dt)); }
    FQuat Toward(const FVector& From, const FVector& To, float MaxDeg)
    {
        FQuat Q = FQuat::FindBetweenNormals(From, To);
        FVector Axis; float Angle; Q.ToAxisAndAngle(Axis, Angle);
        if (Angle > PI) Angle -= 2.f * PI;
        const float Max = FMath::DegreesToRadians(MaxDeg);
        return FQuat(Axis, FMath::Clamp(Angle, -Max, Max));
    }
    // Each ray: how long the stalk aims, how long the beam holds, how likely it is to strike a hero it is loosed at
    // (5e: the save; the heroes are the 2D game's survivors, so the disintegration ray is always evaded).
    struct FRayInfo { float Aim, Hold, Strike; };
    const FRayInfo RayInfo[] = {{.5f, .42f, 0.f}, {.5f, .6f, .65f}, {.45f, .7f, .7f}, {.5f, .5f, .55f}, {.45f, .55f, .6f}};
    const FRayInfo& Info(EBeholderRay K) { return RayInfo[FMath::Clamp(static_cast<int32>(K), 0, 4)]; }

    FVector AimPoint(const AActor* A)
    {
        if (const ATempleHero* H = Cast<ATempleHero>(A)) return H->GetActorLocation() + FVector(0, 0, 38);
        if (const ATempleSkeleton* S = Cast<ATempleSkeleton>(A)) return S->GetMesh()->GetBoneLocation(TEXT("spine_03"));
        if (A) return A->GetActorLocation();
        return FVector::ZeroVector;
    }
}

const TCHAR* ATempleBeholder::RayName(EBeholderRay K)
{
    static const TCHAR* Names[] = {TEXT("disintegrate"), TEXT("enervate"), TEXT("telekinetic"), TEXT("paralyze"), TEXT("fear")};
    return Names[FMath::Clamp(static_cast<int32>(K), 0, 4)];
}

int32 ATempleBeholder::GetRaysFired() const
{
    int32 N = 0;
    for (int32 V : RaysFired) N += V;
    return N;
}

float ATempleBeholder::GetHomeGap() const { return static_cast<float>(FVector::Dist(Place, HoverAt)); }

void ATempleBeholder::Sound(int32 Kind, const FVector& At, float Volume, float Pitch) const
{
    if (ATempleSanctumBattle* B = Battle.Get()) B->Sound(static_cast<ESound>(Kind), At, Volume, Pitch);
}

ATempleHero* ATempleBeholder::PickHero(float Fighter, float Wizard) const
{
    const ATempleSanctumBattle* B = Battle.Get();
    if (!B) return nullptr;
    const float R = Random.FRand();
    const int32 Pick = R < Fighter ? 0 : R < Fighter + Wizard ? 1 : 2;
    if (ATempleHero* H = B->GetHero(Pick)) return H;
    return B->GetHero(0);
}

FVector ATempleBeholder::StalkEye(int32 K) const
{
    if (!Stalks.IsValidIndex(K) || Stalks[K].Eye == INDEX_NONE) return GetEyeLocation();
    return Body->GetComponentTransform().TransformPosition(Pos[Stalks[K].Eye]);
}

FVector ATempleBeholder::StalkEyeForward(int32 K) const
{
    if (!Stalks.IsValidIndex(K) || Stalks[K].Eye == INDEX_NONE) return GetActorForwardVector();
    return Body->GetComponentTransform().TransformVectorNoScale(Delta[Stalks[K].Eye].RotateVector(Bones[Stalks[K].Eye].Dir));
}

int32 ATempleBeholder::FreeStalk(const FVector& Aim) const
{
    // The free stalk best placed for the mark: on its side of the head, its eye already turned that way.
    const FVector C = GetCentre();
    int32 Best = INDEX_NONE; float BestScore = -1e9f;
    for (int32 K = 0; K < Stalks.Num(); ++K)
    {
        if (Stalks[K].bBusy || Stalks[K].Eye == INDEX_NONE) continue;
        const FVector E = StalkEye(K);
        const float Side = static_cast<float>(FVector::DotProduct((E - C).GetSafeNormal(), (Aim - C).GetSafeNormal()));
        const float Score = Side + Random.FRandRange(0.f, .35f);
        if (Score > BestScore) { BestScore = Score; Best = K; }
    }
    return Best;
}

int32 ATempleBeholder::OrderRays(const TArray<FBeholderRayOrder>& Orders)
{
    int32 Taken = 0;
    for (const FBeholderRayOrder& O : Orders)
    {
        const int32 Before = Shots.Num();
        StartShot(O.Kind, O.Target.Get(), O.Aim, O.bStrike, O.Delay, O.Stalk, Taken);
        Taken += Shots.Num() > Before;
    }
    if (Taken > 1) ++Volleys;
    return Taken;
}

void ATempleBeholder::StartShot(EBeholderRay Kind, AActor* Target, const FVector& Aim, bool bStrike, float Delay, int32 Stalk, int32 Order)
{
    const FVector Mark = Target ? AimPoint(Target) : Aim;
    const int32 K = Stalks.IsValidIndex(Stalk) && !Stalks[Stalk].bBusy ? Stalk : FreeStalk(Mark);
    if (K == INDEX_NONE) return;
    FShot S;
    S.Kind = Kind; S.Stalk = K; S.Target = Target; S.Aim = Aim; S.bStrike = bStrike;
    S.Start = T + Delay; S.Fire = S.Start + Info(Kind).Aim; S.End = S.Fire + Info(Kind).Hold;
    S.Seed = Random.FRandRange(0.f, 100.f);
    Stalks[K].bBusy = true;
    Shots.Add(S);
}

bool ATempleBeholder::StartVolley(int32 Count)
{
    // 5e: up to three eye rays a round, each at a different target, each a different ray.
    const ATempleSanctumBattle* B = Battle.Get();
    if (!B) return false;
    TArray<ATempleHero*> Heroes;
    for (int32 R = 0; R < 3; ++R) if (ATempleHero* H = B->GetHero(R)) Heroes.Add(H);
    if (!Heroes.Num()) return false;
    for (int32 I = Heroes.Num() - 1; I > 0; --I) Heroes.Swap(I, Random.RandRange(0, I));
    TArray<int32> Kinds = {0, 1, 2, 3, 4};
    for (int32 I = Kinds.Num() - 1; I > 0; --I) Kinds.Swap(I, Random.RandRange(0, I));
    const int32 N = FMath::Min(Count, Heroes.Num());
    for (int32 I = 0; I < N; ++I)
    {
        const EBeholderRay Kind = static_cast<EBeholderRay>(Kinds[I]);
        StartShot(Kind, Heroes[I], FVector::ZeroVector, Random.FRand() < Info(Kind).Strike, .3f * I + Random.FRandRange(0.f, .08f), -1, I);
    }
    if (N > 1) ++Volleys;
    return N > 0;
}

bool ATempleBeholder::OrderBite(ATempleHero* Target)
{
    if (!Target || Move != EBeholderMove::Hover) return false;
    Move = EBeholderMove::Bite; MoveStart = T;
    BiteTarget = Target; BiteFrom = Place;
    const FVector Feet = Target->GetActorLocation() - FVector(0, 0, ATempleHero::HalfHeight);
    const FVector Back = (Place - Feet).GetSafeNormal2D();
    // Its maw (about 55 cm under its middle) on his shield, its face just clear of him.
    BiteStrike = Feet + Back * 168.f + FVector(0, 0, 172.f);
    bSnapped = bRoared = bCountered = false;
    ClosestBite = 1e9f;
    ++Bites; LastBite = FightClock;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_BITE_START target=%s reach_cm=%.0f"), Target->GetRoleName(), FVector::Dist(Place, BiteStrike));
    return true;
}

bool ATempleBeholder::OrderCone(const FVector& Toward)
{
    if (Move != EBeholderMove::Hover) return false;
    Move = EBeholderMove::Cone; MoveStart = T;
    ConeToward = Toward; bConeCharged = bConeBlasted = false; ConeFront = 0; ConeStruck.Reset();
    ConeApex = GetEyeLocation(); ConeAxis = (Toward - ConeApex).GetSafeNormal();
    ++Cones; LastCone = FightClock;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_CONE_START toward=%s"), *Toward.ToCompactString());
    return true;
}

void ATempleBeholder::TakeHit(const FVector& At, const FVector& Dir, float Strength)
{
    ++HitsTaken;
    const FVector L = Body->GetComponentTransform().InverseTransformVectorNoScale(Dir.GetSafeNormal());
    // Thrown the blow's way: a blow from below lifts its face, from the side rolls and turns it.
    ReelVel += FVector(-L.Z * 70.f - L.X * 30.f, L.Y * 90.f, L.Y * 50.f) * Strength;
    PlaceVel += Dir.GetSafeNormal() * 70.f * Strength;
    BlinkStart = T;
    for (FStalk& S : Stalks) S.Kick = FMath::Max(S.Kick, 5.f * Strength);
    Burst(At, -Dir, FLinearColor(.5f, .08f, .06f), 8, 260.f, 2.5f);
    if (T - LastHurtSound > 1.3f) { LastHurtSound = T; Sound(static_cast<int32>(ESound::BeholderHurt), GetCentre(), .85f, Random.FRandRange(.9f, 1.08f)); }
    UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_STRUCK strength=%.1f hits=%d"), Strength, HitsTaken);
}

void ATempleBeholder::StickArrowAt(const FVector& At, const FVector& Dir)
{
    const ATempleSanctumBattle* B = Battle.Get();
    if (!B || !B->GetArrowMesh()) return;
    if (StuckArrows.Num() >= 8) { if (StuckArrows[0]) StuckArrows[0]->DestroyComponent(); StuckArrows.RemoveAt(0); }
    auto* C = NewObject<UStaticMeshComponent>(this);
    C->SetStaticMesh(B->GetArrowMesh());
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetLightingChannels(false, true, false);
    C->SetupAttachment(Body, TEXT("body"));
    C->RegisterComponent();
    // A third of its length in the hide.
    C->SetWorldLocationAndRotation(At - Dir.GetSafeNormal() * 52.f, Dir.Rotation());
    StuckArrows.Add(C);
}

void ATempleBeholder::Drift()
{
    if (!ScriptHover.IsZero()) { HoverAt = ScriptHover; return; }
    if (!bFighting) { HoverAt = Home; return; }
    if (Move != EBeholderMove::Hover || FightClock < NextDrift) return;
    // Between attacks it shifts across the front of the dais, rising and sinking: never still for the heroes' aim.
    const FVector S = (ChultTempleInteriorWorldOf(FVector(1000, 0, 0)) - ChultTempleInteriorWorldOf(FVector::ZeroVector)).GetSafeNormal();
    const FVector Tv = (ChultTempleInteriorWorldOf(FVector(0, 1000, 0)) - ChultTempleInteriorWorldOf(FVector::ZeroVector)).GetSafeNormal();
    HoverAt = Home + S * Random.FRandRange(-430.f, 430.f) + Tv * Random.FRandRange(-120.f, 160.f) + FVector(0, 0, Random.FRandRange(-60.f, 110.f));
    NextDrift = FightClock + Random.FRandRange(3.f, 5.5f);
}

void ATempleBeholder::Direct()
{
    if (Move != EBeholderMove::Hover || Shots.Num() || FightClock < NextAttack) return;
    const ATempleSanctumBattle* B = Battle.Get();
    ATempleHero* Fighter = B ? B->GetHero(0) : nullptr;
    // The cone at a hero (mostly the fighter's line, sometimes the wizard's or the ranger's flank), on the floor beyond.
    auto Cone = [&]()
    {
        ATempleHero* H = PickHero(.5f, .25f);
        if (!H) return false;
        const FVector Feet = H->GetActorLocation() - FVector(0, 0, ATempleHero::HalfHeight);
        const FVector Out = (Feet - GetActorLocation()).GetSafeNormal2D();
        return OrderCone(Feet + Out * 500.f + FVector(0, 0, 40));
    };
    auto Single = [&]()
    {
        ATempleHero* H = PickHero(.35f, .35f);
        const EBeholderRay Kind = static_cast<EBeholderRay>(Random.RandRange(0, 4));
        StartShot(Kind, H, FVector::ZeroVector, Random.FRand() < Info(Kind).Strike, 0.f, -1, 0);
        return Shots.Num() > 0;
    };
    bool bDone = false; float Gap = 2.f;
    // Its opening is always the same (the entrance cutscene can count on it): a volley, a ray, the bite, a ray, two rays, the cone.
    switch (Script)
    {
    case 0: bDone = StartVolley(3); Gap = 2.6f; break;
    case 1: bDone = Single(); Gap = 1.7f; break;
    case 2: bDone = Fighter && FVector::Dist2D(Fighter->GetActorLocation(), Home) < 1150.f ? OrderBite(Fighter) : StartVolley(2); Gap = 2.4f; break;
    case 3: bDone = Single(); Gap = 1.5f; break;
    case 4: bDone = StartVolley(2); Gap = 2.3f; break;
    case 5: bDone = Cone(); Gap = 2.8f; break;
    default:
    {
        const float SinceCone = FightClock - LastCone, SinceBite = FightClock - LastBite;
        const float R = Random.FRand();
        // The bite only when the fighter is near the dais (it does not leave its place over the dais by much).
        const bool bFighterNear = Fighter && FVector::Dist2D(Fighter->GetActorLocation(), Home) < 1150.f && !Fighter->IsParalysed();
        if (SinceCone > 24.f || (SinceCone > 15.f && R < .3f)) { bDone = Cone(); Gap = Random.FRandRange(2.6f, 3.4f); }
        else if (bFighterNear && SinceBite > 12.f && R < .5f) { bDone = OrderBite(Fighter); Gap = Random.FRandRange(2.f, 2.8f); }
        else if (R < .78f) { bDone = StartVolley(Random.FRand() < .6f ? 3 : 2); Gap = Random.FRandRange(2.2f, 3.2f); }
        else { bDone = Single(); Gap = Random.FRandRange(1.3f, 2.f); }
    }
    }
    ++Script;
    NextAttack = FightClock + (bDone ? Gap : .5f);
}

void ATempleBeholder::TickFight(float Dt)
{
    if (bFighting) FightClock += Dt;
    if (bFighting) Direct();
    Drift();
    TickShots(Dt);
    if (Move == EBeholderMove::Bite) TickBite(Dt);
    else if (Move == EBeholderMove::Cone) TickCone(Dt);
    else
    {
        JawExtra = Approach(JawExtra, 0.f, 5.f, Dt); PitchExtra = Approach(PitchExtra, 0.f, 4.f, Dt);
        LidsWide = Approach(LidsWide, 0.f, 4.f, Dt); EyeFlare = Approach(EyeFlare, 0.f, 3.f, Dt);
        StalksBack = Approach(StalksBack, 0.f, 3.f, Dt); Writhe = Approach(Writhe, 1.f, 2.f, Dt);
        // Facing the fight: the fighter, whom the others stand behind, or the ray's mark while it aims.
        FVector Focus = GetActorLocation() + GetActorForwardVector() * 500.f;
        if (const ATempleSanctumBattle* B = Battle.Get()) if (const ATempleHero* F = B->GetHero(0)) Focus = F->GetActorLocation();
        if (Shots.Num() && Shots[0].Target.IsValid()) Focus = FMath::Lerp(Focus, Shots[0].Target->GetActorLocation(), .45f);
        WantYaw = static_cast<float>((Focus - GetActorLocation()).Rotation().Yaw);
    }
    // The stalks: curled to aim (TickShots), back out of the cone, or loose.
    const FTransform Comp = Body->GetComponentTransform();
    for (FStalk& S : Stalks)
    {
        if (!S.bBusy && S.Root != INDEX_NONE && S.Eye != INDEX_NONE)
        {
            S.CurlWant = StalksBack;
            if (StalksBack > .01f)
            {
                const FVector Rest = (S.RestEye - Bones[S.Root].RefPos).GetSafeNormal();
                const FVector Away = (Rest * .4f + FVector(-1.f, 0, .7f)).GetSafeNormal();
                S.Curl = Toward(Rest, Away, 80.f);
            }
            S.GlowW = Approach(S.GlowW, 0.f, 8.f, Dt);
        }
        S.CurlW = Approach(S.CurlW, S.CurlWant, S.bBusy ? 11.f : 5.f, Dt);
        S.Kick = Approach(S.Kick, 0.f, 9.f, Dt);
    }
    (void)Comp;
}

void ATempleBeholder::TickShots(float Dt)
{
    const FTransform Comp = Body->GetComponentTransform();
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    for (int32 I = Shots.Num() - 1; I >= 0; --I)
    {
        FShot& S = Shots[I];
        if (T < S.Start) continue;
        FStalk& St = Stalks[S.Stalk];
        // Where it is aimed: the mark (alive, it is followed), passing by it when it is to miss.
        FVector Mark = S.Target.IsValid() ? AimPoint(S.Target.Get()) : S.Aim;
        if (!S.bStrike && S.Target.IsValid())
        {
            const FVector From = StalkEye(S.Stalk);
            const FVector Across = FVector::CrossProduct((Mark - From).GetSafeNormal(), FVector::UpVector).GetSafeNormal();
            Mark += Across * (FMath::Fmod(S.Seed, 2.f) < 1.f ? 85.f : -85.f) - FVector(0, 0, 30);
        }
        if (!S.bFired)
        {
            // Aiming: the stalk curls toward the mark, its eye turns on it and gathers the ray's light.
            const FVector Rest = (St.RestEye - Bones[St.Root].RefPos).GetSafeNormal();
            const FVector Want = (Comp.InverseTransformPosition(Mark) - Bones[St.Root].RefPos).GetSafeNormal();
            St.Curl = Toward(Rest, Want, 70.f); St.CurlWant = 1.f;
            St.Look = Mark;
            const float U = (T - S.Start) / FMath::Max(.01f, S.Fire - S.Start);
            St.GlowW = Ease(U); St.Glow = FVector(RayColour(S.Kind).R, RayColour(S.Kind).G, RayColour(S.Kind).B);
            if (I == 0 || !Shots[0].Target.IsValid()) GazeAt = Mark;
            if (!S.bCharged)
            {
                S.bCharged = true;
                Sound(static_cast<int32>(ESound::BeholderRayCharge), StalkEye(S.Stalk), .55f, 1.f + .08f * static_cast<int32>(S.Kind));
            }
            // Light drawn into the eye.
            const FVector E = StalkEye(S.Stalk);
            for (int32 K = 0; K < 2; ++K)
            {
                const FVector Off = FMath::VRand() * Random.FRandRange(25.f, 55.f);
                Spark(E + Off, -Off * Random.FRandRange(4.f, 7.f), RayColour(S.Kind), Random.FRandRange(1.5f, 3.f), .16f, 1.f);
            }
            if (T >= S.Fire) { S.Aim = Mark; FireShot(S); }
            continue;
        }
        // The beam holds from the eye to what it struck; a struck hero or skeleton is followed.
        S.From = StalkEye(S.Stalk) + (S.To - StalkEye(S.Stalk)).GetSafeNormal() * 8.f;
        if (S.Struck.IsValid() && (Cast<ATempleHero>(S.Struck.Get()) || Cast<ATempleSkeleton>(S.Struck.Get())))
        {
            const FVector Now = AimPoint(S.Struck.Get());
            if (!Now.IsZero() && !S.Struck->IsHidden()) S.To = FMath::Lerp(S.To, Now, 1.f - FMath::Exp(-Dt * 12.f));
        }
        St.Look = S.To;
        // Chuck, anywhere along it (the beam sweeps with the stalk): a point of Sanity.
        if (Chuck && !S.bChuckHit && !Chuck->IsHidden())
        {
            const FVector C = Chuck->GetActorLocation();
            FVector P1, P2;
            FMath::SegmentDistToSegmentSafe(S.From, S.To, C - FVector(0, 0, 18), C + FVector(0, 0, 18), P1, P2);
            if (FVector::Dist(P1, P2) < 30.f)
            {
                S.bChuckHit = true;
                if (Chuck->TakeBite(S.From, 1))
                {
                    ++ChuckHits;
                    Burst(P2, -S.Dir, RayColour(S.Kind), 26, 420.f, 3.f);
                    Flare(P2, FVector::ZeroVector, RayColour(S.Kind), 60.f, .3f);
                    Afflict(Chuck, S.Kind, .8f);
                    BeholderShake(GetWorld(), P2, .7f, 600.f, 0);
                    UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_RAY_HIT_CHUCK kind=%s sanity=%d"), RayName(S.Kind), Chuck->GetSanity());
                }
            }
        }
        // Where it strikes: sparks and a little light, the whole time it holds.
        if (Random.FRand() < .7f) Spark(S.To, (FMath::VRand() + FVector(0, 0, .6f)) * Random.FRandRange(150.f, 420.f), RayColour(S.Kind), Random.FRandRange(1.5f, 3.5f), Random.FRandRange(.25f, .6f), 1.5f, 700.f, true);
        if (T >= S.End)
        {
            St.bBusy = false; St.CurlWant = 0.f;
            Shots.RemoveAt(I);
        }
    }
}

void ATempleBeholder::FireShot(FShot& S)
{
    S.bFired = true;
    ++RaysFired[static_cast<int32>(S.Kind)];
    const FVector Eye = StalkEye(S.Stalk);
    S.Dir = (S.Aim - Eye).GetSafeNormal();
    S.From = Eye + S.Dir * 8.f;
    const float Reach = 4200.f;
    FVector End = S.From + S.Dir * Reach;
    float Best = Reach;
    // The hall: its stone.
    {
        FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(BeholderRay), false, this);
        if (GetWorld()->LineTraceSingleByObjectType(Hit, S.From, End, FCollisionObjectQueryParams(ECC_WorldStatic), Q)) Best = static_cast<float>(Hit.Distance);
    }
    AActor* Struck = nullptr;
    FVector At = S.From + S.Dir * Best;
    ATempleSanctumBattle* B = Battle.Get();
    if (B)
    {
        // A skeleton in its way (or its mark).
        for (const TWeakObjectPtr<ATempleSkeleton>& Wk : B->Foes())
        {
            ATempleSkeleton* K = Wk.Get();
            if (!K || !K->IsUp()) continue;
            FVector P1, P2;
            FMath::SegmentDistToSegmentSafe(S.From, S.From + S.Dir * Best, K->GetMesh()->GetBoneLocation(TEXT("pelvis")), K->GetMesh()->GetBoneLocation(TEXT("head")), P1, P2);
            if (FVector::Dist(P1, P2) > 22.f) continue;
            const float D = static_cast<float>(FVector::Dist(S.From, P1));
            if (D < Best) { Best = D; Struck = K; At = P2; }
        }
        // A hero: the mark when it is to strike, or any other one in its way.
        for (int32 R = 0; R < 3; ++R)
        {
            ATempleHero* H = B->GetHero(R);
            // (The disintegration ray they always evade.)
            if (!H || (H == S.Target.Get() && !S.bStrike) || S.Kind == EBeholderRay::Disintegrate) continue;
            const FVector Q = FMath::ClosestPointOnSegment(H->GetActorLocation(), S.From, S.From + S.Dir * Best);
            if (H->GapTo(Q) > 4.f) continue;
            const float D = static_cast<float>(FVector::Dist(S.From, Q));
            if (D < Best) { Best = D; Struck = H; At = Q; }
        }
    }
    S.Struck = Struck;
    if (ATempleHero* Mark = Cast<ATempleHero>(S.Target.Get()))
        if (Mark != Struck) Mark->EvadeBeholderRay(S.From, FMath::ClosestPointOnSegment(Mark->GetActorLocation(), S.From, At));
    const FLinearColor Col = RayColour(S.Kind);
    FString What = TEXT("stone");
    if (ATempleHero* H = Cast<ATempleHero>(Struck))
    {
        FVector Impact = At;
        const bool bTook = H->TakeBeholderRay(S.Kind, S.From, Impact);
        At = Impact;
        if (bTook)
        {
            ++HeroHits;
            const float Long = S.Kind == EBeholderRay::Paralyze ? 2.2f : S.Kind == EBeholderRay::Enervate ? 1.8f : S.Kind == EBeholderRay::Fear ? 1.3f : .7f;
            Afflict(H, S.Kind, Long);
        }
        What = FString::Printf(TEXT("%s%s"), H->GetRoleName(), bTook ? TEXT("") : TEXT("_blocked"));
    }
    else if (ATempleSkeleton* K = Cast<ATempleSkeleton>(Struck))
    {
        if (S.Kind == EBeholderRay::Disintegrate)
        {
            // Dust: the bones go to powder from the beam's touch outward, a cloud of it and a scatter of fragments.
            ++Disintegrated;
            const FVector Mid = K->GetMesh()->GetBoneLocation(TEXT("spine_02"));
            K->Disintegrate(S.From);
            if (B)
            {
                B->Splinters(Mid, S.Dir, 60, 380.f);
                for (const TCHAR* Bone : {TEXT("head"), TEXT("spine_03"), TEXT("pelvis"), TEXT("thigh_l"), TEXT("thigh_r")})
                    B->Dust(K->GetMesh()->GetBoneLocation(Bone), S.Dir * .3f + FVector(0, 0, .5f), 4, 38.f);
            }
            // Green ash where each bone was, drifting up and out.
            for (const TCHAR* Bone : {TEXT("head"), TEXT("spine_03"), TEXT("spine_01"), TEXT("pelvis"), TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("calf_l"), TEXT("calf_r")})
            {
                const FVector At0 = K->GetMesh()->GetBoneLocation(Bone);
                for (int32 N = 0; N < 9; ++N)
                    Spark(At0 + FMath::VRand() * Random.FRandRange(0.f, 14.f), FMath::VRand() * Random.FRandRange(20.f, 90.f) + S.Dir * 60.f + FVector(0, 0, Random.FRandRange(20.f, 90.f)), Col * Random.FRandRange(.5f, 1.f), Random.FRandRange(2.5f, 5.f), Random.FRandRange(.6f, 1.8f), 1.2f);
            }
            Flare(Mid, FVector(0, 0, 60), Col, 120.f, .5f);
            Sound(static_cast<int32>(ESound::BeholderDisintegrate), Mid, 1.f);
            What = TEXT("skeleton_dust");
        }
        else
        {
            K->TakeHeroBlow(ATempleSkeleton::EBlow::Bolt, S.From, At);
            if (B) B->Splinters(At, S.Dir, 18, 320.f);
            What = TEXT("skeleton");
        }
    }
    else
    {
        // Stone: a scorch where it bites, chips and dust thrown off, embers left glowing on the flags.
        if (B) { B->Splinters(At, -S.Dir, 10, 300.f); B->Dust(At, -S.Dir + FVector(0, 0, .5f), 3, 30.f); }
        for (int32 N = 0; N < 14; ++N) Spark(At + FMath::VRand() * 12.f, FVector(FMath::VRand().X, FMath::VRand().Y, 0) * 40.f, Col * .6f, Random.FRandRange(2.f, 4.f), Random.FRandRange(1.5f, 3.f), 4.f, 0.f, true);
    }
    S.To = At;
    // The beam lances out with a crack of its own kind; the stalk is thrown back by it, the head jolted.
    Sound(static_cast<int32>(ESound::BeholderRayDisintegrate) + static_cast<int32>(S.Kind), S.From, 1.f, Random.FRandRange(.96f, 1.04f));
    Sound(static_cast<int32>(ESound::BeholderRayImpact), At, .9f, Random.FRandRange(.9f, 1.1f));
    Flare(S.From, S.Dir * 120.f, Col, 34.f, .22f);
    Flare(At, -S.Dir * 60.f + FVector(0, 0, 40), Col, 80.f, .45f);
    Burst(At, -S.Dir, Col, 30, 520.f, 3.f);
    Flash(At, Col, 26000.f, .35f);
    Stalks[S.Stalk].Kick = 10.f;
    ReelVel += FVector(Random.FRandRange(-12.f, 4.f), Random.FRandRange(-10.f, 10.f), Random.FRandRange(-8.f, 8.f));
    BeholderShake(GetWorld(), At, .35f, 700.f, 0);
    UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_RAY kind=%s stalk=%d strike=%d struck=%s length_cm=%.0f"), RayName(S.Kind), S.Stalk, S.bStrike ? 1 : 0, *What, FVector::Dist(S.From, At));
}

void ATempleBeholder::TickBite(float Dt)
{
    ATempleHero* H = BiteTarget.Get();
    const float U = T - MoveStart;
    if (H) WantYaw = static_cast<float>((H->GetActorLocation() - Place).Rotation().Yaw);
    // The wind-up: it draws back and up; if he has strayed far, it glides in toward him as it does (the lunge stays under 5.6 m).
    const FVector Away = (BiteFrom - BiteStrike).GetSafeNormal2D();
    const float Out = FMath::Min(static_cast<float>(FVector::Dist2D(BiteFrom, BiteStrike)) + 80.f, 520.f);
    const FVector Wind(BiteStrike.X + Away.X * Out, BiteStrike.Y + Away.Y * Out, FMath::Max(BiteFrom.Z, BiteStrike.Z) + 60.f);
    // Where its maw is (from where the lunge puts it this frame: the mesh's own transform is a frame behind).
    auto MawAt = [this, H]() { const FVector Fw = H ? (H->GetActorLocation() - Place).GetSafeNormal2D() : GetActorForwardVector(); return Place + Fw * 70.f - FVector(0, 0, 50); };
    constexpr float Hold = .55f;   // the jaws on the shield, worrying at it, before it lets go
    const float Lunge = BiteLungeAt, Snap = BiteSnapAt;
    if (H) GazeAt = H->HeadLocation();
    if (U < Lunge)
    {
        // It draws back and up, the maw gaping, head tipped back: a roar.
        const float A = Ease(U / Lunge);
        Place = FMath::Lerp(BiteFrom, Wind, A);
        JawExtra = 36.f * Ease(U / .42f); PitchExtra = -11.f * A; Writhe = 1.f + .8f * A; LidsWide = .5f * A;
        if (!bRoared && U > .06f) { bRoared = true; Sound(static_cast<int32>(ESound::BeholderRoar), GetCentre(), 1.f, Random.FRandRange(.94f, 1.04f)); }
    }
    else if (U < Snap)
    {
        // The lunge: gathering speed down onto him, face first.
        const float A = (U - Lunge) / (Snap - Lunge);
        const FVector Prev = Place;
        Place = FMath::Lerp(Wind, BiteStrike, FMath::Pow(A, 1.7f));
        PlaceVel = (Place - Prev) / FMath::Max(Dt, 1e-3f);
        PitchExtra = FMath::Lerp(-11.f, 3.f, Ease(A * 1.3f));   // level as it strikes: the maw, low on its face, leads
        JawExtra = 38.f;
    }
    else if (U < Snap + Hold)
    {
        // The snap, then a worrying tug at what it caught.
        const float A = U - Snap;
        JawExtra = FMath::Lerp(38.f, -3.f, Ease(A / .07f)) + (A > .07f ? 2.f * FMath::Sin(A * 60.f) : 0.f);
        Place = BiteStrike + FVector(0, 0, 6.f * FMath::Sin(A * 40.f)) + (BiteFrom - BiteStrike).GetSafeNormal2D() * 10.f * FMath::Sin(A * 25.f);
        PitchExtra = 3.f;
        if (!bSnapped)
        {
            bSnapped = true;
            const FVector Maw = MawAt();
            Sound(static_cast<int32>(ESound::BeholderSnap), Maw, 1.f, Random.FRandRange(.95f, 1.05f));
            bool bBlocked = false;
            if (H)
            {
                ClosestBite = FMath::Min(ClosestBite, static_cast<float>(FMath::Max(0.f, H->GapTo(Maw))));
                bBlocked = H->TakeBeholderBite(GetCentre(), Maw);
                Burst(Maw - GetActorForwardVector() * 20.f, -GetActorForwardVector(), bBlocked ? FLinearColor(1.f, .7f, .35f) : FLinearColor(.6f, .1f, .08f), bBlocked ? 34 : 14, 480.f, 2.5f);
                if (ATempleSanctumBattle* B = Battle.Get()) B->Dust(H->GetActorLocation() - FVector(0, 0, ATempleHero::HalfHeight - 10), GetActorForwardVector(), 4, 34.f);
            }
            BeholderShake(GetWorld(), Maw, .8f, 1400.f, 0);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_BITE_SNAP target=%s blocked=%d gap_cm=%.0f"), H ? H->GetRoleName() : TEXT("none"), bBlocked ? 1 : 0, ClosestBite);
            // The fighter answers a bite caught on his shield with a rising cut at its face.
            if (H && bBlocked && H->GetRole() == ATempleHero::ERole::Fighter) { bCountered = true; H->CounterBeholder(this); }
        }
    }
    else
    {
        // Back up to its place over the dais.
        const float A = Ease((U - Snap - Hold) / (BiteLength - Snap - Hold));
        Place = FMath::Lerp(BiteStrike, HoverAt, A);
        PlaceVel = FVector::ZeroVector;
        PitchExtra = FMath::Lerp(3.f, 0.f, A); JawExtra = FMath::Lerp(-3.f, 0.f, A); Writhe = FMath::Lerp(1.8f, 1.f, A); LidsWide = 0.f;
    }
    if (H && U < Snap + .1f)
    {
        const FVector Maw = MawAt();
        ClosestBite = FMath::Min(ClosestBite, static_cast<float>(FMath::Max(0.f, H->GapTo(Maw))));
    }
    if (U >= BiteLength) { Move = EBeholderMove::Hover; PlaceVel = FVector::ZeroVector; }
}

bool ATempleBeholder::InCone(const FVector& P) const
{
    const FVector V = P - ConeApex;
    const float D = static_cast<float>(V.Size());
    if (D < 1.f || D > ConeRange) return false;
    return FVector::DotProduct(V / D, ConeAxis) >= FMath::Cos(FMath::DegreesToRadians(ConeHalfDeg));
}

void ATempleBeholder::TickCone(float Dt)
{
    const float U = T - MoveStart;
    // The central eye's gaze lights its cone (a spotlight of exactly its angle and reach): faint as it opens, swelling
    // through the charge so the wedge of floor it will sweep stands out pale; a white flash with the blast, then gone.
    if (ConeLight)
    {
        float I = 0.f;
        if (U >= ConeChargeAt && U < ConeBlastAt) I = FMath::Lerp(1500.f, 26000.f, FMath::Square((U - ConeChargeAt) / (ConeBlastAt - ConeChargeAt))) * (.85f + .15f * FMath::Sin(T * 40.f));
        else if (U >= ConeBlastAt) I = 160000.f * FMath::Exp(-(U - ConeBlastAt) * 7.f);
        else I = 1500.f * Ease(U / ConeChargeAt);
        ConeLight->SetWorldLocationAndRotation(GetEyeLocation(), (bConeCharged ? ConeAxis : (ConeToward - GetEyeLocation()).GetSafeNormal()).Rotation());
        ConeLight->SetIntensity(I);
        if (U >= ConeLength - .05f) ConeLight->SetIntensity(0.f);
    }
    ATempleSanctumBattle* B = Battle.Get();
    WantYaw = static_cast<float>((ConeToward - GetActorLocation()).Rotation().Yaw);
    GazeAt = ConeToward;
    // How far it must tip its face down to look along the cone.
    const FVector ToT = ConeToward - GetEyeLocation();
    const float Down = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-ToT.Z), static_cast<float>(ToT.Size2D())));
    if (U < ConeChargeAt)
    {
        // It looms: up and back, the stalks curling away behind it (its rays fail in its own cone).
        const FVector Back = (GetActorLocation() - ConeToward).GetSafeNormal2D();
        HoverAt = Home + Back * 70.f + FVector(0, 0, 120);
        StalksBack = Approach(StalksBack, 1.f, 4.f, Dt);
        PitchExtra = Approach(PitchExtra, Down * .55f, 3.f, Dt);
        Writhe = Approach(Writhe, .45f, 3.f, Dt);
        JawExtra = Approach(JawExtra, 8.f, 3.f, Dt);
    }
    else if (U < ConeBlastAt)
    {
        // The great eye opens wide and gathers: light pours into it, the air is drawn toward it along the floor.
        if (!bConeCharged)
        {
            bConeCharged = true;
            ConeApex = GetEyeLocation(); ConeAxis = (ConeToward - ConeApex).GetSafeNormal();
            Sound(static_cast<int32>(ESound::BeholderConeCharge), ConeApex, 1.f);
            BeholderShake(GetWorld(), ConeApex + ConeAxis * 900.f, .6f, 3200.f, 2);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_CONE_CHARGE apex=%s axis=%s"), *ConeApex.ToCompactString(), *ConeAxis.ToCompactString());
        }
        const float A = (U - ConeChargeAt) / (ConeBlastAt - ConeChargeAt);
        LidsWide = Approach(LidsWide, 1.f, 8.f, Dt);
        EyeFlare = .15f + .55f * A * A;
        StalksBack = 1.f; Writhe = .3f;
        JawExtra = Approach(JawExtra, 14.f, 2.f, Dt);
        PitchExtra = Approach(PitchExtra, Down * .55f, 3.f, Dt);
        const float Floor = B ? B->FloorZ() : static_cast<float>(ConeApex.Z) - 350.f;
        for (int32 K = 0; K < 6; ++K)
        {
            // Motes over the floor in the cone, streaming back into the eye.
            const float D = Random.FRandRange(300.f, ConeRange * .95f);
            const float Ang = Random.FRandRange(-ConeHalfDeg, ConeHalfDeg) * .9f;
            const FVector Dir = ConeAxis.GetSafeNormal2D().RotateAngleAxis(Ang, FVector::UpVector);
            const FVector P(ConeApex.X + Dir.X * D, ConeApex.Y + Dir.Y * D, Floor + Random.FRandRange(5.f, 90.f));
            if (!InCone(P) && Random.FRand() < .7f) continue;
            Spark(P, (ConeApex - P).GetSafeNormal() * Random.FRandRange(500.f, 900.f) * (.4f + A), FLinearColor(.6f, .75f, 1.f), Random.FRandRange(1.5f, 3.f), Random.FRandRange(.4f, .8f), .2f);
        }
    }
    else
    {
        if (!bConeBlasted)
        {
            bConeBlasted = true;
            Sound(static_cast<int32>(ESound::BeholderConeBlast), ConeApex + ConeAxis * 300.f, 1.f);
            BeholderShake(GetWorld(), ConeApex + ConeAxis * 1000.f, 1.f, 4200.f, 1);
            ReelVel += FVector(-110.f, 0, 0);
            PlaceVel -= ConeAxis.GetSafeNormal2D() * 260.f;
            Flare(ConeApex + ConeAxis * 30.f, ConeAxis * 300.f, FLinearColor(.7f, .82f, 1.f), 160.f, .4f);
            Flash(ConeApex + ConeAxis * 200.f, FLinearColor(.7f, .82f, 1.f), 90000.f, .5f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_CONE_BLAST"));
        }
        const float A = (U - ConeBlastAt) / ConeTravel;
        const float Prev = ConeFront;
        ConeFront = FMath::Min(1.f, A) * ConeRange;
        EyeFlare = U < ConeBlastAt + .25f ? 1.f : Approach(EyeFlare, 0.f, 3.f, Dt);
        if (A < 1.f)
        {
            // Everything the front passes in the cone.
            TArray<AActor*> Who;
            if (APawn* C = UGameplayStatics::GetPlayerPawn(this, 0)) Who.Add(C);
            if (B)
            {
                for (int32 R = 0; R < 3; ++R) if (ATempleHero* H = B->GetHero(R)) Who.Add(H);
                for (const TWeakObjectPtr<ATempleSkeleton>& K : B->Foes()) if (K.IsValid() && K->IsUp()) Who.Add(K.Get());
            }
            for (AActor* W : Who)
            {
                if (ConeStruck.Contains(W)) continue;
                const FVector P = Cast<ATempleHero>(W) || Cast<ATempleSkeleton>(W) ? AimPoint(W) : W->GetActorLocation();
                if (FVector::Dist(P, ConeApex) <= ConeFront && InCone(P)) ConeStrike(W, P);
            }
            // The front tearing along the floor: a wall of dust thrown out ahead of it.
            const float Floor = B ? B->FloorZ() : static_cast<float>(ConeApex.Z) - 350.f;
            for (int32 K = 0; K < 5; ++K)
            {
                const float D = FMath::Lerp(Prev, ConeFront, Random.FRand());
                const FVector Dir = ConeAxis.GetSafeNormal2D().RotateAngleAxis(Random.FRandRange(-ConeHalfDeg, ConeHalfDeg), FVector::UpVector);
                const float Horizontal = D * FMath::Cos(FMath::DegreesToRadians(12.f));
                const FVector P(ConeApex.X + Dir.X * Horizontal, ConeApex.Y + Dir.Y * Horizontal, Floor + 15.f);
                if (!InCone(P + FVector(0, 0, 40)) && !InCone(P + FVector(0, 0, 150))) continue;
                if (B && K < 2) B->Dust(P, Dir + FVector(0, 0, .35f), 1, 70.f);
                Spark(P + FVector(0, 0, 30), Dir * Random.FRandRange(700.f, 1300.f) + FVector(0, 0, 200), FLinearColor(.65f, .8f, 1.f), Random.FRandRange(2.f, 4.f), .35f, 2.f);
            }
        }
        // Spent: it settles, the stalks uncurl.
        if (U > ConeBlastAt + ConeTravel + .2f)
        {
            StalksBack = Approach(StalksBack, 0.f, 2.5f, Dt); LidsWide = Approach(LidsWide, 0.f, 3.f, Dt);
            Writhe = Approach(Writhe, 1.f, 2.f, Dt); JawExtra = Approach(JawExtra, 0.f, 3.f, Dt); PitchExtra = Approach(PitchExtra, 0.f, 2.f, Dt);
            HoverAt = Home;
        }
        if (U >= ConeLength) { Move = EBeholderMove::Hover; NextDrift = FightClock; }
    }
}

void ATempleBeholder::ConeStrike(AActor* Who, const FVector& At)
{
    ConeStruck.Add(Who);
    ATempleSanctumBattle* B = Battle.Get();
    if (auto* Chuck = Cast<AChuckCharacter>(Who))
    {
        // A pier (or anything of stone) between him and the eye shelters him.
        FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(BeholderCone), false, this);
        if (GetWorld()->LineTraceSingleByObjectType(Hit, ConeApex, At, FCollisionObjectQueryParams(ECC_WorldStatic), Q) && Hit.Distance < FVector::Dist(ConeApex, At) - 40.f)
        {
            ++Sheltered;
            UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_CONE_SHELTERED by=%s"), *GetNameSafe(Hit.GetComponent()));
            return;
        }
        ++ConeHits;
        if (Chuck->IsHidden()) return;
        // The magic that holds a fey summon here torn at: two Sanity, and he is flung.
        if (Chuck->TakeBite(ConeApex, 2, false, true))
        {
            ++ConeChuckHits; ++ChuckHits;
            FVector Fling = ConeAxis.GetSafeNormal2D() * 470.f + FVector(0, 0, 330.f);
            // Never into the rupture, nor over it onto a pier's island: thrown short of its edge instead.
            float Reach = 0.f;
            const float Frac = SafeFling(Chuck, Fling, Reach);
            Fling.X *= Frac; Fling.Y *= Frac;
            Chuck->LaunchCharacter(Fling, true, true);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_CONE_FLING fraction=%.2f reach=%.0f"), Frac, Reach);
            Burst(At, ConeAxis, FLinearColor(.65f, .8f, 1.f), 30, 600.f, 3.f);
            Afflict(Chuck, EBeholderRay::Telekinetic, .9f);
            UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_CONE_HIT_CHUCK sanity=%d"), Chuck->GetSanity());
        }
        return;
    }
    ++ConeHits;
    if (auto* H = Cast<ATempleHero>(Who))
    {
        H->TakeBeholderCone(ConeApex, ConeAxis);
        Burst(At, ConeAxis, FLinearColor(.65f, .8f, 1.f), 16, 450.f, 2.5f);
        UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_CONE_HIT hero=%s"), H->GetRoleName());
    }
    else if (auto* K = Cast<ATempleSkeleton>(Who))
    {
        K->TakeHeroBlow(ATempleSkeleton::EBlow::Bolt, ConeApex, At);
        if (B) { B->Splinters(At, ConeAxis, 16, 520.f); B->Dust(At, ConeAxis, 2, 30.f); }
        UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_CONE_HIT skeleton"));
    }
}

// ---------------------------------------------------------------------------
// Camera shakes made in code: noise on the view's place and turn.
UBeholderShakePattern::UBeholderShakePattern(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}

void UBeholderShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
    OutInfo.Duration = FCameraShakeDuration(Duration);
    OutInfo.BlendIn = bSwell ? Duration * .8f : .02f;
    OutInfo.BlendOut = bSwell ? .05f : Duration * .7f;
}

void UBeholderShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
    Elapsed = 0.f; Seed = FMath::FRandRange(0.f, 100.f);
}

void UBeholderShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
    Elapsed += Params.DeltaTime;
    const float X = (Elapsed + Seed) * Freq;
    auto N = [X](float O) { return FMath::PerlinNoise1D(X + O) * 1.6f; };
    OutResult.Location = FVector(N(0.f), N(17.3f), N(41.7f)) * LocAmp;
    OutResult.Rotation = FRotator(N(71.1f) * RotAmp, N(93.9f) * RotAmp, N(123.4f) * RotAmp * .6f);
}

UBeholderShakeBlast::UBeholderShakeBlast(const FObjectInitializer& OI) : Super(OI)
{
    auto* P = OI.CreateDefaultSubobject<UBeholderShakePattern>(this, TEXT("Pattern"));
    P->Duration = 1.1f; P->LocAmp = 9.f; P->RotAmp = 2.2f; P->Freq = 16.f;
    SetRootShakePattern(P);
}
UBeholderShakeJolt::UBeholderShakeJolt(const FObjectInitializer& OI) : Super(OI)
{
    auto* P = OI.CreateDefaultSubobject<UBeholderShakePattern>(this, TEXT("Pattern"));
    P->Duration = .35f; P->LocAmp = 3.f; P->RotAmp = .9f; P->Freq = 24.f;
    SetRootShakePattern(P);
}
UBeholderShakeCharge::UBeholderShakeCharge(const FObjectInitializer& OI) : Super(OI)
{
    auto* P = OI.CreateDefaultSubobject<UBeholderShakePattern>(this, TEXT("Pattern"));
    P->Duration = 1.5f; P->LocAmp = 1.6f; P->RotAmp = .35f; P->Freq = 30.f; P->bSwell = true;
    SetRootShakePattern(P);
}

void BeholderShake(const UWorld* World, const FVector& At, float Strength, float Radius, int32 Kind)
{
    APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
    if (!PC || !PC->PlayerCameraManager) return;
    const float D = static_cast<float>(FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), At));
    const float Scale = Strength * FMath::Square(FMath::Clamp(1.f - D / Radius, 0.f, 1.f));
    if (Scale < .03f) return;
    const TSubclassOf<UCameraShakeBase> Class = Kind == 1 ? UBeholderShakeBlast::StaticClass() : Kind == 2 ? UBeholderShakeCharge::StaticClass() : UBeholderShakeJolt::StaticClass();
    PC->ClientStartCameraShake(Class, Scale);
}
