// The temple fire jets' flame (Claude, 2026-10-22; docs/CHULT-TEMPLE-RUN.md): a small particle
// sim drawn on camera-facing instanced quads with Unity Labs' CC0 Houdini flipbooks
// (SourceAssets/Setting/TempleFire, Tools/import_temple_fire.py). What makes it read as a real
// flamethrower rather than a coloured shape:
//  - puffs leave the nozzle fast in a narrow cone and slow under heavy drag, so the jet is a
//    thin hot stream near the mouth that billows out toward its end;
//  - each puff swells as it slows, rolls (its flipbook and a spin), and cools from white-hot
//    toward a dull red, rising more as its forward speed dies (buoyancy);
//  - turbulence grows with age, so the end of the jet breaks up and curls;
//  - a puff that reaches a wall flattens against it and spreads along it;
//  - when the fuel cuts, the stream stops at the nozzle and the last puffs burn out quickly;
//  - burnt-out puffs leave dark smoke that rises and gathers under the ceiling;
//  - in the tell a pilot flame licks up at the mouth and it sputters small licks of fire.
// The hit test is unchanged (JetMeets): the puffs' speed is set so the flame's front reaches the
// jet's Reach at about the moment the hit test does (its 0.12 s), so what burns is what shows.
#include "ChultTempleTraps.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    constexpr float Drag = 9.f;            // 1/s: the stream's speed falls as exp(-Drag t)
    constexpr float FireRate = 95.f;       // puffs a second while burning
    constexpr float FireFrames = 64.f, SmokeFrames = 64.f, PilotFrames = 64.f;
    float Smooth01(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }

    // A quad facing the eye (its +Z), turned Spin about that axis, Size cm across.
    FTransform Billboard(const FVector& P, const FVector& Eye, float Spin, float Size)
    {
        FVector Z = Eye - P; if (!Z.Normalize()) Z = FVector::UpVector;
        FVector X0 = FVector::CrossProduct(FMath::Abs(Z.Z) > .98f ? FVector::ForwardVector : FVector::UpVector, Z).GetSafeNormal();
        const FVector Y0 = FVector::CrossProduct(Z, X0);
        float S, C; FMath::SinCos(&S, &C, Spin);
        const FVector X = X0 * C + Y0 * S, Y = Y0 * C - X0 * S;
        return FTransform(FMatrix(X, Y, Z, FVector::ZeroVector).ToQuat(), P, FVector(Size / 100.f));
    }
    // An upright quad turned toward the eye about the vertical, Width x Height cm, its centre at P.
    // The plane's UV V runs along its +Y, and a flipbook's top is V=0, so +Y points down.
    FTransform Upright(const FVector& P, const FVector& Eye, float Width, float Height)
    {
        FVector Z = Eye - P; Z.Z = 0; if (!Z.Normalize()) Z = FVector::ForwardVector;
        const FVector Y = -FVector::UpVector, X = FVector::CrossProduct(Y, Z);
        return FTransform(FMatrix(X, Y, Z, FVector::ZeroVector).ToQuat(), P, FVector(Width / 100.f, Height / 100.f, 1.f));
    }
}

void AChultTempleTraps::SimJetFire(int32 I, int32 Stage, float U, float Dt, const FVector& Eye, bool bNear)
{
    FJet& J = Jets[I];
    Dt = FMath::Clamp(Dt, 0.f, 1.f / 20.f);
    if (!bNear)
    {
        // Out of sight: drop the sim and hide what was drawn.
        if (J.bDrawn || J.Fire.Num() || J.Smoke.Num())
        {
            J.Fire.Reset(); J.Smoke.Reset(); DrawJetFire(J, Eye); J.bDrawn = false;
        }
        if (UPointLightComponent* L = J.Light.Get()) L->SetIntensity(0.f);
        return;
    }
    // The wall it fires at and the ceiling over it, traced once (the masonry is all built by then).
    if (J.WallAt < 0)
    {
        FCollisionQueryParams Q(TEXT("TempleJetFire"), false);
        if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0)) Q.AddIgnoredActor(Pawn);
        FHitResult Hit;
        const FVector From = J.Mouth + J.Dir * 10.f;
        J.WallAt = GetWorld()->LineTraceSingleByChannel(Hit, From, J.Mouth + J.Dir * (J.Reach + 400.f), ECC_Visibility, Q) ? 10.f + Hit.Distance : J.Reach + 400.f;
        const FVector Mid = J.Mouth + J.Dir * FMath::Min(J.Reach * .5f, J.WallAt - 20.f);
        J.CeilingAt = GetWorld()->LineTraceSingleByChannel(Hit, Mid, Mid + FVector(0, 0, 1200), ECC_Visibility, Q) ? Hit.Distance : 1200.f;
    }
    FRandomStream& R = J.Rand;
    const FVector Up = FVector::UpVector;
    const FVector Side = FVector::CrossProduct(Up, J.Dir).GetSafeNormal();
    const float BurnEnd = J.Warn + J.Burn - .1f;   // the hit test draws the jet back over its last 0.1 s
    const bool bBurning = Stage == 2 && U < BurnEnd;
    const float Tell = Stage == 1 ? FMath::Clamp(U / J.Warn, 0.f, 1.f) : Stage == 2 ? 1.f : 0.f;

    // ---- Spawning.
    auto SpawnFire = [&](bool bLick, float Lead)
    {
        if (J.Fire.Num() >= JetFirePool) return;
        FJetPuff F;
        F.Seed = R.FRand() * 100.f;
        F.Frame = R.FRand() * FireFrames; F.FrameRate = R.FRandRange(34.f, 48.f);
        F.Spin = R.FRand() * 2.f * PI; F.SpinRate = R.FRandRange(-2.5f, 2.5f);
        if (bLick)
        {
            // The tell's sputter: a small, slow lick of fire out of the mouth, curling up.
            F.Life = R.FRandRange(.22f, .34f);
            F.V = (J.Dir * R.FRandRange(90.f, 220.f) + Up * R.FRandRange(40.f, 110.f) + Side * R.FRandRange(-40.f, 40.f));
            F.Size0 = 8.f; F.Size1 = R.FRandRange(22.f, 34.f) * (.5f + .5f * Tell);
            F.Heat0 = R.FRandRange(.55f, .8f);
        }
        else
        {
            // The stream: fast in a narrow cone, so the front reaches about Reach by the end of its life.
            F.Life = R.FRandRange(.36f, .48f);
            const float Reach = FMath::Min(J.Reach, J.WallAt - 5.f) * R.FRandRange(.9f, 1.f);
            const float Speed = Reach * Drag / (1.f - FMath::Exp(-Drag * F.Life));
            const FVector D = (J.Dir + Side * R.FRandRange(-.06f, .06f) + Up * R.FRandRange(-.03f, .07f)).GetSafeNormal();
            F.V = D * Speed;
            F.Size0 = R.FRandRange(18.f, 26.f); F.Size1 = R.FRandRange(72.f, 96.f);
            F.Heat0 = 1.f;
        }
        F.P = J.Mouth + J.Dir * 6.f + Side * R.FRandRange(-3.f, 3.f) + Up * R.FRandRange(-3.f, 3.f);
        // Spread evenly through the frame: a puff born Lead seconds ago has already flown that far
        // (with the drag), so the stream is continuous rather than a bead a frame.
        F.Age = Lead;
        F.P += F.V * ((1.f - FMath::Exp(-Drag * Lead)) / Drag);
        F.V *= FMath::Exp(-Drag * Lead);
        J.Fire.Add(F);
    };
    if (bBurning)
    {
        J.FireCarry += FireRate * Dt;
        while (J.FireCarry >= 1.f) { J.FireCarry -= 1.f; SpawnFire(false, J.FireCarry / FireRate); }
    }
    else if (Stage == 1 && Tell > .25f)
    {
        J.FireCarry += (6.f + 26.f * Tell) * Dt;
        while (J.FireCarry >= 1.f) { J.FireCarry -= 1.f; SpawnFire(true, 0.f); }
    }
    else J.FireCarry = 0;
    // The fuel cut: the stream stops at the nozzle and what is out there burns out fast.
    if (!bBurning)
        for (FJetPuff& F : J.Fire)
            if (!F.bCut) { F.bCut = true; F.Life = FMath::Min(F.Life, F.Age + R.FRandRange(.12f, .22f)); }

    // ---- Moving the fire.
    const float T = static_cast<float>(GetWorld()->GetTimeSeconds());
    const FVector Wall = J.Mouth + J.Dir * J.WallAt;
    const float Ceiling = J.Mouth.Z + J.CeilingAt;
    for (int32 K = J.Fire.Num() - 1; K >= 0; --K)
    {
        FJetPuff& F = J.Fire[K];
        F.Age += Dt;
        if (F.Age >= F.Life)
        {
            // A burnt-out puff leaves smoke (not every one, or the hall would fill).
            if (!F.bCut || R.FRand() < .4f)
                if (J.Smoke.Num() < JetSmokePool && R.FRand() < .45f)
                {
                    FJetPuff S;
                    S.P = F.P; S.V = F.V * .25f + Up * R.FRandRange(30.f, 60.f);
                    S.Life = R.FRandRange(1.2f, 1.8f); S.Size0 = F.Size1 * .7f; S.Size1 = F.Size1 * R.FRandRange(1.6f, 2.2f);
                    S.Frame = R.FRand() * SmokeFrames; S.FrameRate = R.FRandRange(14.f, 22.f);
                    S.Spin = R.FRand() * 2.f * PI; S.SpinRate = R.FRandRange(-.6f, .6f);
                    S.Heat0 = .7f; S.Seed = F.Seed;
                    J.Smoke.Add(S);
                }
            J.Fire.RemoveAtSwap(K);
            continue;
        }
        const float A = F.Age / F.Life;
        // Drag, then buoyancy (hot gas climbs; it shows once the forward speed has gone),
        // then turbulence that grows as the stream breaks up.
        F.V *= FMath::Exp(-Drag * Dt);
        F.V.Z += (180.f + 420.f * A) * Dt;
        const float Turb = 250.f + 1300.f * A;
        F.V += FVector(FMath::Sin(F.P.Z * .045f + T * 9.f + F.Seed), FMath::Sin(F.P.X * .041f + T * 8.3f + F.Seed * 1.7f), FMath::Sin(F.P.Y * .038f + T * 7.1f + F.Seed * 2.3f)) * Turb * Dt;
        F.P += F.V * Dt;
        // The wall: flatten against it and spread along it.
        const float Size = FMath::Lerp(F.Size0, F.Size1, 1.f - FMath::Square(1.f - A));
        const float Into = static_cast<float>(FVector::DotProduct(F.P - Wall, J.Dir)) + Size * .3f;
        if (Into > 0)
        {
            F.P -= J.Dir * Into;
            const float Normal = static_cast<float>(FVector::DotProduct(F.V, J.Dir));
            if (Normal > 0)
            {
                F.V -= J.Dir * Normal;
                const float Ang = R.FRandRange(0.f, 2.f * PI);
                F.V += (Side * FMath::Cos(Ang) + Up * (.35f + .65f * FMath::Abs(FMath::Sin(Ang)))) * Normal * .55f;
            }
        }
        if (F.P.Z > Ceiling - Size * .35f) { F.P.Z = Ceiling - Size * .35f; F.V.Z = FMath::Min(F.V.Z, 0.f); }
        F.Frame += F.FrameRate * Dt; F.Spin += F.SpinRate * Dt;
    }
    // ---- Smoke: slow, rising, spreading under the ceiling.
    for (int32 K = J.Smoke.Num() - 1; K >= 0; --K)
    {
        FJetPuff& S = J.Smoke[K];
        S.Age += Dt;
        if (S.Age >= S.Life) { J.Smoke.RemoveAtSwap(K); continue; }
        const float A = S.Age / S.Life;
        S.V *= FMath::Exp(-1.6f * Dt);
        S.V.Z += 70.f * (1.f - A) * Dt;
        S.V += FVector(FMath::Sin(S.P.Z * .02f + T * 1.9f + S.Seed), FMath::Sin(S.P.X * .021f + T * 1.7f + S.Seed * 1.3f), 0) * 60.f * Dt;
        S.P += S.V * Dt;
        const float Size = FMath::Lerp(S.Size0, S.Size1, A);
        if (S.P.Z > Ceiling - Size * .3f)
        {
            S.P.Z = Ceiling - Size * .3f;
            S.V.Z = 0; S.V += Side * (FMath::Sin(S.Seed) > 0 ? 1.f : -1.f) * 40.f * Dt;
        }
        S.Frame += S.FrameRate * Dt; S.Spin += S.SpinRate * Dt;
    }
    J.PilotFrame += 30.f * Dt;

    // ---- The light: from the fire actually burning, where it burns.
    if (UPointLightComponent* L = J.Light.Get())
    {
        float Sum = 0; FVector Centre = FVector::ZeroVector;
        for (const FJetPuff& F : J.Fire)
        {
            const float A = F.Age / F.Life, W = F.Heat0 * (1.f - A);
            Sum += W; Centre += F.P * W;
        }
        if (Sum > .01f) J.LightAt = FMath::Lerp(J.LightAt, Centre / Sum + FVector(0, 0, 20), FMath::Min(1.f, Dt * 12.f));
        L->SetWorldLocation(J.LightAt);
        L->SetIntensity(9000.f * FMath::Min(Sum / 18.f, 1.15f) + 1600.f * Tell * (Stage == 1 ? 1.f : .4f) * (.85f + .15f * FMath::Sin(T * 31.f + I)));
    }
    DrawJetFire(J, Eye);
    J.bDrawn = true;
}

void AChultTempleTraps::DrawJetFire(FJet& J, const FVector& Eye)
{
    const float T = static_cast<float>(GetWorld()->GetTimeSeconds());
    TArray<FTransform> X;
    auto Push = [&](UInstancedStaticMeshComponent* C, int32 Pool, auto&& Fill)
    {
        if (!C) return;
        X.Reset(); X.Init(FTransform(FQuat::Identity, J.Mouth, FVector::ZeroVector), Pool);
        Fill(X);
        C->BatchUpdateInstancesTransforms(0, X, true, false, true);
        C->MarkRenderStateDirty();
    };
    Push(J.FireFx.Get(), JetFirePool, [&](TArray<FTransform>& Out)
    {
        UInstancedStaticMeshComponent* C = J.FireFx.Get();
        for (int32 K = 0; K < J.Fire.Num() && K < Out.Num(); ++K)
        {
            const FJetPuff& F = J.Fire[K];
            const float A = F.Age / F.Life;
            const float Size = FMath::Lerp(F.Size0, F.Size1, 1.f - FMath::Square(1.f - A));
            // Hot at the nozzle, cooling along the jet; a cut puff cools faster.
            const float Heat = F.Heat0 * FMath::Clamp(1.f - .9f * FMath::Pow(A, 1.05f) - (F.bCut ? .25f : 0.f), .05f, 1.f);
            const float Op = Smooth01(F.Age / .035f) * (1.f - Smooth01((A - .62f) / .38f));
            Out[K] = Billboard(F.P, Eye, F.Spin, Size);
            const float Data[3] = {F.Frame, Heat, Op};
            C->SetCustomData(K, MakeArrayView(Data, 3), false);
        }
    });
    Push(J.SmokeFx.Get(), JetSmokePool, [&](TArray<FTransform>& Out)
    {
        UInstancedStaticMeshComponent* C = J.SmokeFx.Get();
        for (int32 K = 0; K < J.Smoke.Num() && K < Out.Num(); ++K)
        {
            const FJetPuff& S = J.Smoke[K];
            const float A = S.Age / S.Life;
            Out[K] = Billboard(S.P, Eye, S.Spin, FMath::Lerp(S.Size0, S.Size1, A));
            const float Data[3] = {S.Frame, S.Heat0 * FMath::Max(0.f, 1.f - A * 3.f), .4f * Smooth01(A / .18f) * (1.f - Smooth01((A - .45f) / .55f))};
            C->SetCustomData(K, MakeArrayView(Data, 3), false);
        }
    });
    Push(J.PilotFx.Get(), JetPilots, [&](TArray<FTransform>& Out)
    {
        UInstancedStaticMeshComponent* C = J.PilotFx.Get();
        const float Period = FMath::Max(J.Period, .01f);
        const float U = FMath::Fmod(T - J.Phase + Period * 1000.f, Period);
        const float Tell = U < J.Warn ? U / J.Warn : U < J.Warn + J.Burn ? 1.f : 0.f;
        if (Tell <= 0) return;
        const FVector Side = FVector::CrossProduct(FVector::UpVector, J.Dir).GetSafeNormal();
        for (int32 K = 0; K < Out.Num(); ++K)
        {
            // Two flames side by side licking up from the lip of the mouth, growing toward the burst.
            const float G = .35f + .65f * Tell, Wd = (20.f + 6.f * K) * G, Ht = Wd * 2.f;
            const FVector Base = J.Mouth + J.Dir * (8.f + 4.f * K) + Side * (K ? 5.f : -5.f) + FVector(0, 0, -8);
            Out[K] = Upright(Base + FVector(0, 0, Ht * .42f), Eye, Wd, Ht);
            const float Data[3] = {J.PilotFrame + 23.f * K, .55f + .45f * Tell, Smooth01(Tell * 3.f)};
            C->SetCustomData(K, MakeArrayView(Data, 3), false);
        }
    });
}
