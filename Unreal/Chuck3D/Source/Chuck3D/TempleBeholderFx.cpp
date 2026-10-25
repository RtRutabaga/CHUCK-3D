// The beholder's effects (docs/CHULT-TEMPLE-BEHOLDER.md#the-fight): eye-ray beams as camera-facing ribbons built
// each frame (M_BeholderBeam), the cone's shell of force (M_BeholderWave), sparks and motes (M_BeholderSpark) and
// bursts of energy (M_BeholderFlare: Unity Labs' CC0 fireball flipbook tinted to the ray) on instanced planes,
// and a few pooled lights. Materials: Tools/create_beholder_fx.py.
#include "TempleBeholder.h"
#include "ChuckCharacter.h"
#include "TempleHero.h"
#include "TempleSanctumBattle.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
    constexpr int32 SparkPool = 320, FlarePool = 40, LightPool = 5;
    const FTransform Hidden(FQuat::Identity, FVector(0, 0, -1e6), FVector(.001f));

    FTransform Billboard(const FVector& P, const FVector& Eye, float Spin, float Size)
    {
        FVector Zv = Eye - P; if (!Zv.Normalize()) Zv = FVector::UpVector;
        const FVector X0 = FVector::CrossProduct(FMath::Abs(Zv.Z) > .98f ? FVector::ForwardVector : FVector::UpVector, Zv).GetSafeNormal();
        const FVector Y0 = FVector::CrossProduct(Zv, X0);
        float S, C; FMath::SinCos(&S, &C, Spin);
        return FTransform(FMatrix(X0 * C + Y0 * S, Y0 * C - X0 * S, Zv, FVector::ZeroVector).ToQuat(), P, FVector(Size / 100.f));
    }

    struct FRibbon
    {
        TArray<FVector>& V; TArray<int32>& I; TArray<FLinearColor>& C; TArray<FVector2D>& UV; TArray<FVector>& N;
        const FTransform& X; const FVector& Eye;
        /** A camera-facing strip through Points, Width wide, coloured Col (alpha per point). */
        void Strip(const TArray<FVector>& Points, const TArray<float>& Alpha, float Width, const FLinearColor& Col, float Along0)
        {
            if (Points.Num() < 2) return;
            const int32 Base = V.Num();
            float Along = Along0;
            for (int32 K = 0; K < Points.Num(); ++K)
            {
                const FVector Tan = (Points[FMath::Min(K + 1, Points.Num() - 1)] - Points[FMath::Max(K - 1, 0)]).GetSafeNormal();
                const FVector Side = FVector::CrossProduct(Tan, (Eye - Points[K]).GetSafeNormal()).GetSafeNormal() * Width * .5f;
                if (K) Along += static_cast<float>(FVector::Dist(Points[K], Points[K - 1])) / 100.f;
                V.Add(X.InverseTransformPosition(Points[K] - Side)); V.Add(X.InverseTransformPosition(Points[K] + Side));
                const FLinearColor A(Col.R, Col.G, Col.B, Col.A * Alpha[K]);
                C.Add(A); C.Add(A);
                UV.Add(FVector2D(0, Along)); UV.Add(FVector2D(1, Along));
                N.Add(FVector::UpVector); N.Add(FVector::UpVector);
            }
            for (int32 K = 0; K + 1 < Points.Num(); ++K)
            {
                const int32 A = Base + K * 2;
                I.Append({A, A + 1, A + 3, A, A + 3, A + 2});
            }
        }
    };
}

FLinearColor ATempleBeholder::RayColour(EBeholderRay Kind)
{
    switch (Kind)
    {
    case EBeholderRay::Disintegrate: return FLinearColor(.25f, 1.f, .2f);    // the green of the 5e art
    case EBeholderRay::Enervate: return FLinearColor(.55f, .12f, 1.f);       // necrotic violet
    case EBeholderRay::Telekinetic: return FLinearColor(.3f, .65f, 1.f);     // pale force blue
    case EBeholderRay::Paralyze: return FLinearColor(1.f, .72f, .12f);       // gold
    case EBeholderRay::Fear: return FLinearColor(1.f, .1f, .12f);            // crimson
    default: return FLinearColor::White;
    }
}

void ATempleBeholder::SetFx(UMaterialInterface* Beam, UMaterialInterface* Wave, UMaterialInterface* SparkMat, UMaterialInterface* FlareMat, UStaticMesh* Plane)
{
    if (Beam) BeamMesh->SetMaterial(0, Beam);
    if (Wave) WaveMesh->SetMaterial(0, Wave);
    if (Plane)
    {
        SparkFx->SetStaticMesh(Plane); FlareFx->SetStaticMesh(Plane);
        if (SparkMat) SparkFx->SetMaterial(0, SparkMat);
        if (FlareMat) FlareFx->SetMaterial(0, FlareMat);
        TArray<FTransform> X; X.Init(Hidden, SparkPool); SparkFx->AddInstances(X, false, true);
        X.Init(Hidden, FlarePool); FlareFx->AddInstances(X, false, true);
    }
    for (int32 K = 0; K < LightPool; ++K)
    {
        auto* L = NewObject<UPointLightComponent>(this);
        L->SetupAttachment(RootComponent); L->SetIntensity(0.f); L->SetAttenuationRadius(900.f); L->SetCastShadows(false);
        // Both channels: some of the sanctum's floor slabs are on the default one, and a ray lights all the stone it passes.
        L->SetLightingChannels(true, true, false);
        L->RegisterComponent(); FxLights.Add(L);
    }
    bFxReady = Beam && Wave && SparkMat && FlareMat && Plane;
    UE_LOG(LogTemp, Display, TEXT("CHUCK_BEHOLDER_FX ready=%d beam=%d wave=%d spark=%d flare=%d"), bFxReady ? 1 : 0, Beam != nullptr, Wave != nullptr, SparkMat != nullptr, FlareMat != nullptr);
}

void ATempleBeholder::Spark(const FVector& At, const FVector& Vel, const FLinearColor& Col, float Size, float Life, float Drag, float Grav, bool bFloor)
{
    if (Motes.Num() >= SparkPool) Motes.RemoveAt(0, 1, EAllowShrinking::No);
    Motes.Add({At, Vel, Col, Size, 0.f, Life, Drag, Grav, bFloor});
}

void ATempleBeholder::Burst(const FVector& At, const FVector& Normal, const FLinearColor& Col, int32 Count, float Speed, float Size)
{
    const FVector Nn = Normal.GetSafeNormal();
    for (int32 K = 0; K < Count; ++K)
    {
        const FVector D = (Nn * Random.FRandRange(.2f, 1.f) + FMath::VRand() * .9f).GetSafeNormal();
        Spark(At, D * Speed * Random.FRandRange(.3f, 1.f), Col, Size * Random.FRandRange(.5f, 1.3f), Random.FRandRange(.25f, .7f), 2.5f, 600.f, true);
    }
}

void ATempleBeholder::Flare(const FVector& At, const FVector& Vel, const FLinearColor& Col, float Size, float Life)
{
    if (Flares.Num() >= FlarePool) Flares.RemoveAt(0, 1, EAllowShrinking::No);
    Flares.Add({At, Vel, Col, Size, Size * 1.2f, 0.f, Life, static_cast<float>(Random.FRandRange(0.f, 10.f)), 64.f / Life, static_cast<float>(Random.FRandRange(0.f, 6.28f))});
}

void ATempleBeholder::Flash(const FVector& At, const FLinearColor& Col, float Peak, float Life)
{
    if (Flashes.Num() >= 8) Flashes.RemoveAt(0);
    Flashes.Add({At, Col, Peak, 0.f, Life});
}

void ATempleBeholder::Afflict(AActor* Who, EBeholderRay Kind, float Seconds)
{
    for (FAffliction& A : Afflictions) if (A.Who == Who) { A.Kind = Kind; A.Until = T + Seconds; return; }
    Afflictions.Add({Who, Kind, T + Seconds, T});
}

void ATempleBeholder::TickFx(float Dt)
{
    if (!bFxReady) return;
    ATempleSanctumBattle* B = Battle.Get();
    const float Floor = B ? B->FloorZ() : -1e9f;
    // The afflicted wear the ray a while: gold crackling over the paralysed, violet seeping up from the enervated,
    // crimson whirling round the frightened, a blue shimmer round the thrown.
    for (int32 I = Afflictions.Num() - 1; I >= 0; --I)
    {
        FAffliction& A = Afflictions[I];
        AActor* W = A.Who.Get();
        if (!W || T > A.Until) { Afflictions.RemoveAt(I); continue; }
        if (T < A.Next) continue;
        A.Next = T + .03f;
        const bool bSmall = Cast<AChuckCharacter>(W) != nullptr;
        const float R = bSmall ? 22.f : 38.f, H = bSmall ? 30.f : 90.f;
        const FVector C = W->GetActorLocation();
        const float Ang = Random.FRandRange(0.f, 2.f * PI);
        const FVector P = C + FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, Random.FRandRange(-H, H));
        const FLinearColor Col = RayColour(A.Kind);
        switch (A.Kind)
        {
        case EBeholderRay::Paralyze: Spark(P, FMath::VRand() * 60.f, Col, Random.FRandRange(1.f, 2.5f), .15f, 1.f); break;
        case EBeholderRay::Enervate: Spark(P, FVector(0, 0, Random.FRandRange(30.f, 70.f)), Col * .8f, Random.FRandRange(2.f, 4.f), .9f, .5f); break;
        case EBeholderRay::Fear: Spark(P, FVector(-FMath::Sin(Ang), FMath::Cos(Ang), .3f) * 160.f, Col, Random.FRandRange(1.5f, 3.f), .4f, .5f); break;
        default: Spark(P, (P - C).GetSafeNormal() * 40.f, Col, Random.FRandRange(1.5f, 3.f), .3f, 1.f); break;
        }
    }
    // Motes: thrown, dragged, falling and bouncing once on the flags if they fall.
    for (FMote& M : Motes)
    {
        M.Age += Dt;
        M.Vel *= FMath::Exp(-M.Drag * Dt);
        M.Vel.Z -= M.Grav * Dt;
        M.Pos += M.Vel * Dt;
        if (M.bFloor && M.Pos.Z < Floor + 1.f) { M.Pos.Z = Floor + 1.f; M.Vel.Z *= -.3f; M.Vel.X *= .5f; M.Vel.Y *= .5f; }
    }
    Motes.RemoveAll([](const FMote& M) { return M.Age > M.Life; });
    for (FFlare& F : Flares) { F.Age += Dt; F.Pos += F.Vel * Dt; F.Vel *= FMath::Exp(-4.f * Dt); F.Size += F.Grow * Dt; F.Frame += F.Rate * Dt; }
    Flares.RemoveAll([](const FFlare& F) { return F.Age > F.Life; });
    for (FFxLight& L : Flashes) L.Age += Dt;
    Flashes.RemoveAll([](const FFxLight& L) { return L.Age > L.Life; });

    // Draw: motes and flares on their planes.
    {
        TArray<FTransform> X; X.Init(Hidden, SparkPool);
        for (int32 I = 0; I < SparkPool; ++I)
        {
            float D[4] = {0, 0, 0, 0};
            if (I < Motes.Num())
            {
                const FMote& M = Motes[I];
                const float U = M.Age / M.Life;
                const float A = FMath::Clamp(U / .08f, 0.f, 1.f) * FMath::Square(1.f - U);
                X[I] = Billboard(M.Pos, CamEye, 0.f, M.Size * (1.f - .4f * U));
                D[0] = M.Col.R; D[1] = M.Col.G; D[2] = M.Col.B; D[3] = A;
            }
            SparkFx->SetCustomData(I, MakeArrayView(D, 4), false);
        }
        SparkFx->BatchUpdateInstancesTransforms(0, X, true, true);
    }
    {
        TArray<FTransform> X; X.Init(Hidden, FlarePool);
        for (int32 I = 0; I < FlarePool; ++I)
        {
            float D[5] = {0, 0, 0, 0, 0};
            if (I < Flares.Num())
            {
                const FFlare& F = Flares[I];
                const float U = F.Age / F.Life;
                X[I] = Billboard(F.Pos, CamEye, F.Spin, F.Size);
                D[0] = F.Frame; D[1] = F.Col.R; D[2] = F.Col.G; D[3] = F.Col.B; D[4] = FMath::Clamp(U / .1f, 0.f, 1.f) * (1.f - U);
            }
            FlareFx->SetCustomData(I, MakeArrayView(D, 5), false);
        }
        FlareFx->BatchUpdateInstancesTransforms(0, X, true, true);
    }
    DrawBeams();
    DrawWave();
    // Lights: each beam lights the hall along its length; the flashes of its strikes and the cone.
    struct FWant { FVector Pos; FLinearColor Col; float I, R; };
    TArray<FWant> Want;
    for (const FShot& S : Shots)
        if (S.bFired && T < S.End) Want.Add({(S.From + S.To) * .5f, RayColour(S.Kind), 5500.f * FMath::Clamp((S.End - T) / .12f, 0.f, 1.f), static_cast<float>(FVector::Dist(S.From, S.To)) * .5f + 500.f});
    for (const FFxLight& L : Flashes) Want.Add({L.Pos, L.Col, L.Peak * FMath::Square(1.f - L.Age / L.Life), 1100.f});
    Want.Sort([](const FWant& A, const FWant& Bw) { return A.I > Bw.I; });
    for (int32 K = 0; K < FxLights.Num(); ++K)
    {
        if (K < Want.Num()) { FxLights[K]->SetWorldLocation(Want[K].Pos); FxLights[K]->SetLightColor(Want[K].Col); FxLights[K]->SetIntensity(Want[K].I); FxLights[K]->SetAttenuationRadius(FMath::Min(Want[K].R, 2200.f)); }
        else FxLights[K]->SetIntensity(0.f);
    }
}

void ATempleBeholder::DrawBeams()
{
    TArray<FVector> V, N; TArray<int32> I; TArray<FLinearColor> C; TArray<FVector2D> UV;
    const FTransform& X = BeamMesh->GetComponentTransform();
    FRibbon R{V, I, C, UV, N, X, CamEye};
    for (const FShot& S : Shots)
    {
        if (!S.bFired || T >= S.End) continue;
        const float Age = T - S.Fire, Left = S.End - T;
        // It lances out from the eye in a few hundredths of a second, swells, holds, and snaps off.
        const float Reach = FMath::Clamp(Age / .06f, 0.f, 1.f);
        const float Swell = FMath::Clamp(Age / .05f, 0.f, 1.f) * FMath::Clamp(Left / .1f, 0.f, 1.f) * (1.f + .35f * FMath::Exp(-Age * 12.f));
        const FVector A = S.From, Bv = FMath::Lerp(S.From, S.To, Reach);
        const float L = static_cast<float>(FVector::Dist(A, Bv));
        if (L < 5.f) continue;
        const FVector Dir = (Bv - A) / L;
        const FVector U1 = FVector::CrossProduct(Dir, FMath::Abs(Dir.Z) > .9f ? FVector::ForwardVector : FVector::UpVector).GetSafeNormal();
        const FVector U2 = FVector::CrossProduct(Dir, U1);
        const int32 Segs = FMath::Clamp(static_cast<int32>(L / 35.f), 8, 90);
        const FLinearColor Col = RayColour(S.Kind);
        TArray<FVector> Core, Sheath, F1, F2; TArray<float> Alpha;
        for (int32 K = 0; K <= Segs; ++K)
        {
            const float U = K / static_cast<float>(Segs), D = U * L;
            const FVector P = A + Dir * D;
            const float Ends = FMath::Clamp(D / 25.f, 0.f, 1.f);
            Alpha.Add(Ends);
            FVector Off = FVector::ZeroVector, O1 = FVector::ZeroVector, O2 = FVector::ZeroVector;
            const float Tt = T * 1.f;
            switch (S.Kind)
            {
            case EBeholderRay::Disintegrate:
                // Straight and terribly bright, with crackling filaments clinging round it.
                Off = (U1 * FMath::FRandRange(-1.f, 1.f) + U2 * FMath::FRandRange(-1.f, 1.f)) * 1.2f;
                O1 = (U1 * FMath::FRandRange(-1.f, 1.f) + U2 * FMath::FRandRange(-1.f, 1.f)) * 9.f;
                O2 = (U1 * FMath::FRandRange(-1.f, 1.f) + U2 * FMath::FRandRange(-1.f, 1.f)) * 9.f;
                break;
            case EBeholderRay::Enervate:
            {
                // Twin threads twisting round each other, writhing.
                const float Ph = D * .045f - Tt * 9.f;
                const float Rr = 7.f + 3.f * FMath::Sin(D * .02f + Tt * 5.f);
                O1 = (U1 * FMath::Cos(Ph) + U2 * FMath::Sin(Ph)) * Rr; O2 = -O1;
                Off = U1 * 2.5f * FMath::Sin(D * .013f - Tt * 7.f);
                break;
            }
            case EBeholderRay::Telekinetic:
                // A rippling pulse of force: waves run out along it.
                Off = U1 * 5.f * FMath::Sin(D * .03f - Tt * 22.f) + U2 * 3.f * FMath::Cos(D * .022f - Tt * 17.f);
                O1 = Off * 2.2f; O2 = -Off * 1.6f;
                break;
            case EBeholderRay::Paralyze:
                // Rigid, flickering with sparks of gold.
                Off = (U1 * FMath::FRandRange(-1.f, 1.f)) * .8f;
                O1 = (U1 * FMath::FRandRange(-1.f, 1.f) + U2 * FMath::FRandRange(-1.f, 1.f)) * 5.f; O2 = O1 * -1.3f;
                break;
            case EBeholderRay::Fear:
                // Shuddering.
                Off = U1 * 4.f * FMath::Sin(D * .07f + Tt * 31.f) * FMath::Sin(Tt * 13.f);
                O1 = U2 * 8.f * FMath::Sin(D * .05f - Tt * 19.f); O2 = -O1;
                break;
            default: break;
            }
            const float Taper = FMath::Clamp(D / 40.f, .25f, 1.f) * FMath::Clamp((L - D) / 30.f + .4f, .4f, 1.f);
            Core.Add(P + Off); Sheath.Add(P + Off * .5f); F1.Add(P + Off + O1 * Taper); F2.Add(P + Off + O2 * Taper);
        }
        const float W = S.Kind == EBeholderRay::Disintegrate ? 4.5f : S.Kind == EBeholderRay::Telekinetic ? 7.f : 6.f;
        const float Sh = S.Kind == EBeholderRay::Telekinetic ? 62.f : S.Kind == EBeholderRay::Disintegrate ? 38.f : 46.f;
        R.Strip(Sheath, Alpha, Sh * Swell, FLinearColor(Col.R, Col.G, Col.B, .5f), S.Seed);
        R.Strip(Core, Alpha, W * Swell, FLinearColor(Col.R, Col.G, Col.B, 1.f), S.Seed);
        R.Strip(F1, Alpha, 3.f * Swell, FLinearColor(Col.R, Col.G, Col.B, .7f), S.Seed + 3.f);
        R.Strip(F2, Alpha, 3.f * Swell, FLinearColor(Col.R, Col.G, Col.B, .7f), S.Seed + 7.f);
    }
    // The stalk eyes gathering their rays: a growing knot of the ray's light in each.
    for (const FStalk& St : Stalks)
    {
        if (St.GlowW < .02f || St.Eye == INDEX_NONE) continue;
        const FVector E = Body->GetComponentTransform().TransformPosition(Pos[St.Eye]);
        const FVector Side = FVector::CrossProduct((CamEye - E).GetSafeNormal(), FVector::UpVector).GetSafeNormal();
        TArray<FVector> P = {E - Side * 2.f, E + Side * 2.f};
        TArray<float> Al = {1.f, 1.f};
        const float Pulse = 1.f + .25f * FMath::Sin(T * 45.f);
        R.Strip(P, Al, (10.f + 26.f * St.GlowW) * Pulse, FLinearColor(St.Glow.X, St.Glow.Y, St.Glow.Z, .45f * St.GlowW), 0.f);
    }
    if (V.Num()) BeamMesh->CreateMeshSection_LinearColor(0, V, I, N, UV, C, TArray<FProcMeshTangent>(), false);
    else if (BeamMesh->GetNumSections()) BeamMesh->ClearAllMeshSections();
}

void ATempleBeholder::DrawWave()
{
    const float U = T - MoveStart;
    // Only the blast's front is drawn (the charge is shown by the eye's searchlight and the motes it draws in).
    const bool bOn = Move == EBeholderMove::Cone && U >= ConeBlastAt && U < ConeBlastAt + ConeTravel + .5f;
    if (!bOn) { if (WaveMesh->GetNumSections()) WaveMesh->ClearAllMeshSections(); return; }
    // The cone: a shell from the great eye, its open front capped by a curved sheet of force.
    //  Charging: the whole cone faintly outlined, flickering, so it can be seen and fled.
    //  The blast: a bright front racing out, the shell behind it fading.
    const bool bBlast = U >= ConeBlastAt;
    const float A = bBlast ? (U - ConeBlastAt) / ConeTravel : (U - ConeChargeAt) / (ConeBlastAt - ConeChargeAt);
    const float Front = bBlast ? FMath::Min(1.f, A) * ConeRange : ConeRange;
    const float Fade = bBlast ? FMath::Clamp(1.f - (U - ConeBlastAt - ConeTravel) / .7f, 0.f, 1.f) : 1.f;
    const FTransform& X = WaveMesh->GetComponentTransform();
    const FVector Ax = ConeAxis;
    const FVector U1 = FVector::CrossProduct(Ax, FVector::UpVector).GetSafeNormal(), U2 = FVector::CrossProduct(U1, Ax);
    const float Tan = FMath::Tan(FMath::DegreesToRadians(ConeHalfDeg));
    const FLinearColor Col(.62f, .78f, 1.f);
    constexpr int32 Around = 40, Along = 16, CapRings = 6;
    TArray<FVector> V, N; TArray<int32> I; TArray<FLinearColor> C; TArray<FVector2D> UV;
    for (int32 J = 0; J <= Along; ++J)
    {
        const float S = J / static_cast<float>(Along);
        const float D = S * Front;
        float Alpha;
        if (bBlast) Alpha = (.06f * S + 1.1f * FMath::Exp(-FMath::Square((D - Front) / 150.f))) * Fade;
        else Alpha = (.05f + .1f * A) * (.7f + .3f * FMath::Sin(T * 17.f + S * 6.f)) * FMath::Clamp(S * 4.f, 0.f, 1.f);
        for (int32 K = 0; K <= Around; ++K)
        {
            const float Ang = K / static_cast<float>(Around) * 2.f * PI;
            const FVector Radial = U1 * FMath::Cos(Ang) + U2 * FMath::Sin(Ang);
            const FVector P = ConeApex + Ax * D + Radial * D * Tan;
            V.Add(X.InverseTransformPosition(P));
            N.Add(X.InverseTransformVectorNoScale((Radial - Ax * Tan).GetSafeNormal()));
            C.Add(FLinearColor(Col.R, Col.G, Col.B, FMath::Clamp(Alpha, 0.f, 1.5f) / 1.5f));
            UV.Add(FVector2D(K / static_cast<float>(Around), S));
        }
    }
    for (int32 J = 0; J < Along; ++J)
        for (int32 K = 0; K < Around; ++K)
        {
            const int32 A0 = J * (Around + 1) + K, B0 = A0 + Around + 1;
            I.Append({A0, B0, B0 + 1, A0, B0 + 1, A0 + 1});
        }
    if (bBlast && Front > 50.f)
    {
        // The front: a spherical cap at the front's distance.
        const int32 Base = V.Num();
        const float Half = FMath::DegreesToRadians(ConeHalfDeg);
        for (int32 J = 0; J <= CapRings; ++J)
        {
            const float Th = Half * J / static_cast<float>(CapRings);
            for (int32 K = 0; K <= Around; ++K)
            {
                const float Ang = K / static_cast<float>(Around) * 2.f * PI;
                const FVector Dir = (Ax * FMath::Cos(Th) + (U1 * FMath::Cos(Ang) + U2 * FMath::Sin(Ang)) * FMath::Sin(Th)).GetSafeNormal();
                V.Add(X.InverseTransformPosition(ConeApex + Dir * Front));
                N.Add(X.InverseTransformVectorNoScale(Dir));
                C.Add(FLinearColor(Col.R, Col.G, Col.B, .5f * Fade * FMath::Pow(J / static_cast<float>(CapRings), 3.f)));
                UV.Add(FVector2D(K / static_cast<float>(Around), 1.f + J * .2f));
            }
        }
        for (int32 J = 0; J < CapRings; ++J)
            for (int32 K = 0; K < Around; ++K)
            {
                const int32 A0 = Base + J * (Around + 1) + K, B0 = A0 + Around + 1;
                I.Append({A0, B0, B0 + 1, A0, B0 + 1, A0 + 1});
            }
    }
    WaveMesh->CreateMeshSection_LinearColor(0, V, I, N, UV, C, TArray<FProcMeshTangent>(), false);
}
