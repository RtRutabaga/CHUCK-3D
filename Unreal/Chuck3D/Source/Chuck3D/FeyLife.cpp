// The Feywild's mechanics and small living hazards, stage 5 part one (Claude, 2026-11-21). See FeyLife.h.
#include "FeyLife.h"
#include "FeyMap.h"
#include "FeyMesh.h"
#include "FeywildSetting.h"
#include "ChuckCharacter.h"
#include "CigarettePickup.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

using namespace FeyMap;

namespace
{
    TArray<TWeakObjectPtr<AFeyLife>> GLife;
    constexpr float Stand = 34.65f;
    float Smooth(float A, float B, float X) { const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f); return T * T * (3 - 2 * T); }
    FColor Col(float R, float G, float B, float A = 0.f) { return FLinearColor(R, G, B, A).ToFColor(false); }
    enum EMat { MPetal, MBlade, MPlant, MRoot };
    const TCHAR* MatPaths[] = {TEXT("/Game/Art/Feywild/M_FeyPetal.M_FeyPetal"), TEXT("/Game/Art/Feywild/M_FeyBlade.M_FeyBlade"), TEXT("/Game/Art/Feywild/M_FeyPlant.M_FeyPlant"), TEXT("/Game/Art/Feywild/M_FeyRoot.M_FeyRoot")};
    constexpr float OrchidBeat = 2.4f, OrchidWind = .7f, SeedSpeed = 1000.f, SeedRadius = 3.f, MothRadius = 22.f;

    // ------------------------------------------------------------------ what stands where (local Fey frame)
    struct FPlace
    {
        TArray<FFeyFlower> Flowers; TArray<FFeyVines> Vines; TArray<FVector> Rocks; TArray<FVector> Cigs;   // rocks (X, Y, scale)
        TArray<FFeyPollen> Pollen; TArray<FFeyOrchid> Orchids; TArray<FFeyMoth> Moths; TArray<FVector> Clear;
    };
    FVector2D Perp(const FVector2D& D) { return FVector2D(-D.Y, D.X); }
    const FPlace& Placed(int32 D)
    {
        static const TArray<FPlace> S = []()
        {
            TArray<FPlace> Out; Out.SetNum(Districts);
            {   // The riverbank: the first flower, safe and in sight of the road. Its vines shut the only way into a ring of boulders.
                FPlace& P = Out[Riverbank]; const FVector2D N(-14500.f, -3700.f);
                for (int32 K = 0; K < 10; ++K) { const float A = FMath::DegreesToRadians(90.f + 48.f + K * 264.f / 9.f); P.Rocks.Add(FVector(N.X + 460.f * FMath::Cos(A), N.Y + 460.f * FMath::Sin(A), 2.2f + .25f * FMath::Sin(K * 2.1f))); }
                FFeyVines V; V.C = FVector(N.X, N.Y + 440.f, LandZ(Riverbank, N.X, N.Y + 440.f)); V.Yaw = 0.f; V.Width = 330.f; V.Height = 210.f; P.Vines.Add(V);
                FFeyFlower F; const FVector2D Fp = N + FVector2D(420.f, 830.f); F.Pos = FVector(Fp.X, Fp.Y, LandZ(Riverbank, Fp.X, Fp.Y)); F.Yaw = 110.f; F.Vines = 0; P.Flowers.Add(F);
                for (const FVector2D& C : {FVector2D(-12.f, 0.f), FVector2D(10.f, 9.f), FVector2D(2.f, -13.f)}) P.Cigs.Add(FVector(N.X + C.X, N.Y + C.Y, LandZ(Riverbank, N.X + C.X, N.Y + C.Y)));
                P.Clear.Add(FVector(N.X, N.Y, 620.f)); P.Clear.Add(FVector(Fp.X, Fp.Y, 160.f));
            }
            {   // The orchard: a strip over its road a jump clears, then a broad bed; two more among its rows.
                FPlace& P = Out[Orchard]; const TArray<FVector2D>& R = RoadOf(Orchard, 0);
                const FVector2D A = R[1], B = R[2], C = R[3];
                auto Bed = [&](const FVector2D& At, const FVector2D& Dir, float Along, float Across) { FFeyPollen Pb; Pb.C = At; Pb.R = FVector2D(Along, Across); Pb.Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)); P.Pollen.Add(Pb); P.Clear.Add(FVector(At.X, At.Y, FMath::Max(Along, Across) + 60.f)); };
                Bed(FMath::Lerp(A, B, .5f), (B - A).GetSafeNormal(), 60.f, 420.f);
                Bed(FMath::Lerp(B, C, .45f), (C - B).GetSafeNormal(), 430.f, 620.f);
                Bed(FVector2D(-34000.f, -5500.f), FVector2D(1.f, .35f).GetSafeNormal(), 520.f, 380.f);
                Bed(FVector2D(-37000.f, 6500.f), FVector2D(1.f, -.55f).GetSafeNormal(), 600.f, 450.f);
                // The Needle Garden: orchids south of its road, spitting north across it, a beat apart.
                const TArray<FVector2D>& Rn = RoadOf(Orchard, 1); const FVector2D Na = Rn[1], Nb = Rn[2], Dn = (Nb - Na).GetSafeNormal(), Pn = Perp(Dn);
                for (int32 K = 0; K < 3; ++K)
                {
                    const FVector2D At = FMath::Lerp(Na, Nb, .25f + .25f * K) - Pn * 650.f; FFeyOrchid O;
                    O.Pos = FVector(At.X, At.Y, LandZ(Orchard, At.X, At.Y)); O.Dir = FVector(Pn.X, Pn.Y, 0.f); O.Phase = K * OrchidBeat / 3.f; P.Orchids.Add(O); for (int32 Q = 0; Q < 6; ++Q) { const FVector2D Lp = At + Pn * (Q * 260.f); P.Clear.Add(FVector(Lp.X, Lp.Y, 220.f)); }
                }
            }
            {   // The Moonmoth Fen: moths across the way out to it.
                FPlace& P = Out[LilyMere]; const TArray<FVector2D>& Sp = SpurOf(LilyMere); const FVector2D S0 = Sp[0], S1 = Sp.Last(), Ds = (S1 - S0).GetSafeNormal();
                for (int32 K = 0; K < 3; ++K)
                {
                    const FVector2D At = FMath::Lerp(S0, S1, .35f + .25f * K); FFeyMoth M;
                    M.Home = FVector(At.X, At.Y, LandZ(LilyMere, At.X, At.Y) + 72.f); M.Bob = 24.f;   // (at his shoulder: low enough to meet) M.Axis = FVector(Perp(Ds).X, Perp(Ds).Y, 0.f); M.Range = 300.f;
                    M.Rate = 1.45f + .2f * K; M.Phase = 1.7f * K; P.Moths.Add(M);
                }
            }
            return Out;
        }();
        return S[FMath::Clamp(D, 0, Districts - 1)];
    }

    // ------------------------------------------------------------------ shapes
    // A reactive flower's head at Open (0 a bud, 1 a star with curled tips) and Glow (its pulse), on its stem's top Top.
    void FlowerHead(FMapSurf& Petals, FMapSurf& Stamens, float Open, float Glow)
    {
        const FVector Top(0, 0, 52.f);
        for (int32 I = 0; I < 5; ++I)
        {
            const float A = 2 * PI * I / 5 + .3f, Lean = FMath::Sin(I * 2.3f) * .06f;
            const FVector Rd(FMath::Cos(A), FMath::Sin(A), 0.f), Sd(-FMath::Sin(A), FMath::Cos(A), 0.f);
            constexpr int32 Nu = 10, Ns = 6; constexpr float L = 30.f;
            TArray<FVector> Spine; FVector P = Top + Rd * FMath::Lerp(4.f, 7.f, Open);
            for (int32 U = 0; U <= Nu; ++U)
            {
                const float Uf = float(U) / Nu;
                const float Closed = FMath::Lerp(-4.f, -38.f, Uf * Uf) + 8.f, Opened = FMath::Lerp(40.f, 92.f, FMath::Pow(Uf, 1.2f)) + 45.f * FMath::Pow(Uf, 4.f);
                const float Th = FMath::DegreesToRadians(FMath::Lerp(Closed, Opened, Open) + Lean * 57.f);
                Spine.Add(P); P += (Rd * FMath::Sin(Th) + FVector(0, 0, FMath::Cos(Th))) * (L / Nu);
            }
            const int32 Base = Petals.V.Num();
            for (int32 U = 0; U <= Nu; ++U)
            {
                const float Uf = float(U) / Nu, W = 13.f * FMath::Pow(FMath::Max(FMath::Sin(PI * FMath::Pow(Uf, .8f)), 0.f), .7f) + .3f;
                const FVector Along = (Spine[FMath::Min(U + 1, Nu)] - Spine[FMath::Max(U - 1, 0)]).GetSafeNormal(), Out = FVector::CrossProduct(Sd, Along).GetSafeNormal();
                for (int32 J = 0; J <= Ns; ++J)
                {
                    const float S = FMath::Lerp(-1.f, 1.f, float(J) / Ns), Cup = FMath::Lerp(4.f, 1.4f, Open) * S * S;
                    const FVector Q = Spine[U] + Sd * (S * W) + Out * Cup;
                    const float Tip = Smooth(.25f, .95f, Uf);
                    Petals.Vert(Q, -Out, FVector2D(Uf, (S + 1.f) * .5f), Col(FMath::Lerp(.9f, .84f, Tip), FMath::Lerp(.9f, .46f, Tip), FMath::Lerp(.48f, .86f, Tip), FMath::Lerp(.04f, .14f, Tip) + Glow * .35f));
                }
            }
            for (int32 U = 0; U < Nu; ++U) for (int32 J = 0; J < Ns; ++J) { const int32 Q = Base + U * (Ns + 1) + J; Petals.Quad(Q, Q + Ns + 1, Q + Ns + 2, Q + 1); }
        }
        // Stamens, curling up out of the heart (they show as it opens).
        for (int32 I = 0; I < 6; ++I)
        {
            const float A = 2 * PI * I / 6; const FVector Rd(FMath::Cos(A), FMath::Sin(A), 0.f);
            const float H = FMath::Lerp(6.f, 20.f, Open);
            const FVector Pts[] = {Top, Top + FVector(0, 0, H * .5f) + Rd * 2.f, Top + FVector(0, 0, H) + Rd * FMath::Lerp(2.f, 7.f, Open), Top + FVector(0, 0, H - 2.f) + Rd * FMath::Lerp(3.f, 10.f, Open)};
            Stamens.Pipe(Pts, .5f, .35f, 5, 3, Col(.95f, .9f, .45f, .1f + Glow * .4f), I * 1.3f);
        }
    }
    void FlowerBody(FMapSurf& Leaves, FMapSurf& Stem, float Seed)
    {
        const FVector Pts[] = {FVector(0, 0, -6.f), FVector(1.f, 0, 20.f), FVector(-.5f, .6f, 40.f), FVector(0, 0, 53.f)};
        Stem.Pipe(Pts, 3.f, 2.2f, 8, 4, Col(.55f, .7f, .32f), Seed);
        for (int32 I = 0; I < 6; ++I)   // broad glossy leaves, low and drooping
        {
            const float A = 2 * PI * I / 6 + .5f + .3f * FMath::Sin(I * 3.7f + Seed), Len = 58.f + 12.f * FMath::Sin(I * 1.9f + Seed), Wm = 19.f;
            const FVector Rd(FMath::Cos(A), FMath::Sin(A), 0.f), Sd(-FMath::Sin(A), FMath::Cos(A), 0.f);
            constexpr int32 Nu = 10, Ns = 4; const int32 Base = Leaves.V.Num();
            for (int32 U = 0; U <= Nu; ++U)
            {
                const float Uf = float(U) / Nu, W = Wm * FMath::Pow(FMath::Max(FMath::Sin(PI * FMath::Pow(Uf, .7f)), 0.f), .8f) + .2f;
                const FVector Sp = Rd * (Uf * Len) + FVector(0, 0, 4.f + 22.f * FMath::Sin(PI * Uf * .8f) - 20.f * Uf * Uf);
                for (int32 J = 0; J <= Ns; ++J)
                {
                    const float S = FMath::Lerp(-1.f, 1.f, float(J) / Ns);
                    Leaves.Vert(Sp + Sd * (S * W) + FVector(0, 0, -3.f * S * S), FVector::UpVector, FVector2D(Uf, (S + 1.f) * .5f), Col(.42f, .58f, .24f));
                }
            }
            for (int32 U = 0; U < Nu; ++U) for (int32 J = 0; J < Ns; ++J) { const int32 Q = Base + U * (Ns + 1) + J; Leaves.Quad(Q, Q + 1, Q + Ns + 2, Q + Ns + 1); }
        }
    }
    // A curtain of vines across a gap (local to its centre, X across, Z up), with leaves along them.
    void VineCurtain(FMapSurf& Wood, FMapSurf& Leaf, float Width, float Height)
    {
        FRandomStream R(777);
        for (int32 K = 0; K < 13; ++K)   // across, waving
        {
            const float Z = 12.f + K * (Height - 24.f) / 12.f + R.FRandRange(-6.f, 6.f), Ph = R.FRandRange(0.f, 6.f);
            TArray<FVector> Pts; for (int32 I = 0; I <= 6; ++I) { const float X = FMath::Lerp(-Width * .5f - 25.f, Width * .5f + 25.f, I / 6.f); Pts.Add(FVector(X, R.FRandRange(-9.f, 9.f), Z + 11.f * FMath::Sin(Ph + I * 1.3f))); }
            Wood.Pipe(Pts, R.FRandRange(3.f, 5.f), R.FRandRange(2.5f, 4.f), 7, 5, Col(.4f, 1.f, 0.f), K * 1.7f);
        }
        for (int32 K = 0; K < 7; ++K)   // up from the ground, twisting
        {
            const float X0 = FMath::Lerp(-Width * .5f, Width * .5f, (K + .5f) / 7.f), Ph = R.FRandRange(0.f, 6.f);
            TArray<FVector> Pts; for (int32 I = 0; I <= 6; ++I) { const float Z = FMath::Lerp(-20.f, Height + 10.f, I / 6.f); Pts.Add(FVector(X0 + 18.f * FMath::Sin(Ph + I * 1.1f), 10.f * FMath::Cos(Ph + I), Z)); }
            Wood.Pipe(Pts, 6.f, 2.5f, 8, 5, Col(.4f, 1.f, 0.f), 40.f + K);
        }
        for (int32 K = 0; K < 90; ++K)   // leaves
        {
            const FVector At(R.FRandRange(-Width * .55f, Width * .55f), R.FRandRange(-12.f, 12.f), R.FRandRange(10.f, Height));
            const float A = R.FRandRange(0.f, 2 * PI), Len = R.FRandRange(14.f, 24.f);
            const FVector Dir = FVector(FMath::Cos(A) * .6f, FMath::Sin(A), R.FRandRange(-.5f, .3f)).GetSafeNormal(), Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
            const int32 Base = Leaf.V.Num();
            for (int32 U = 0; U <= 3; ++U) for (int32 J = 0; J <= 2; ++J)
            {
                const float Uf = U / 3.f, S = J - 1.f, W = 6.f * FMath::Sin(PI * FMath::Max(Uf, .05f));
                Leaf.Vert(At + Dir * (Uf * Len) + Side * (S * W), FVector::CrossProduct(Side, Dir).GetSafeNormal(), FVector2D(Uf, J * .5f), Col(.36f, .52f, .2f));
            }
            for (int32 U = 0; U < 3; ++U) for (int32 J = 0; J < 2; ++J) { const int32 Q = Base + U * 3 + J; Leaf.Quad(Q, Q + 1, Q + 4, Q + 3); }
        }
    }
    // A spitting orchid's bulb (local: its mouth toward +X), and its stem and leaves.
    void OrchidBulb(FMapSurf& S)
    {
        const FVector2D Prof[] = {{0.f, 11.f}, {6.f, 9.f}, {9.f, 4.f}, {9.5f, -2.f}, {7.f, -8.f}, {3.f, -12.f}, {0.f, -13.f}};
        S.Lathe(FVector::ZeroVector, Prof, 14, Col(.78f, .26f, .58f, .05f));
        for (int32 I = 0; I < 5; ++I)   // the lips round its mouth
        {
            const float A = 2 * PI * I / 5; const FVector Rd(0.f, FMath::Cos(A), FMath::Sin(A));
            const FVector Pts[] = {FVector(0, 0, 10.f) + Rd * 3.f, FVector(3.f, 0, 13.f) + Rd * 6.f, FVector(5.f, 0, 15.f) + Rd * 10.f};
            S.Pipe(Pts, 2.2f, .8f, 6, 3, Col(.95f, .85f, .35f, .2f), I * 2.f);
        }
    }
    void OrchidBody(FMapSurf& Leaves, FMapSurf& Stem, const FVector& Dir)
    {
        const FVector Pts[] = {FVector(0, 0, -5.f), FVector(0, 0, 20.f), Dir * 6.f + FVector(0, 0, 40.f), Dir * 4.f + FVector(0, 0, 52.f)};
        Stem.Pipe(Pts, 3.2f, 2.4f, 8, 4, Col(.38f, .5f, .22f), 3.f);
        for (int32 I = 0; I < 4; ++I)
        {
            const float A = 2 * PI * I / 4 + .7f; const FVector Rd(FMath::Cos(A), FMath::Sin(A), 0.f), Sd(-FMath::Sin(A), FMath::Cos(A), 0.f);
            const int32 Base = Leaves.V.Num();
            for (int32 U = 0; U <= 6; ++U) for (int32 J = 0; J <= 2; ++J)
            {
                const float Uf = U / 6.f, S = J - 1.f, W = 9.f * FMath::Sin(PI * FMath::Max(Uf, .06f));
                Leaves.Vert(Rd * (Uf * 40.f) + FVector(0, 0, 3.f + 12.f * FMath::Sin(PI * Uf * .7f) - 8.f * Uf * Uf) + Sd * (S * W), FVector::UpVector, FVector2D(Uf, J * .5f), Col(.3f, .5f, .2f));
            }
            for (int32 U = 0; U < 6; ++U) for (int32 J = 0; J < 2; ++J) { const int32 Q = Base + U * 3 + J; Leaves.Quad(Q, Q + 1, Q + 4, Q + 3); }
        }
    }
    // A lantern moth's wing (local: +X along its body, +Y out from it), lit from within: amber at its root, teal at its edge, an
    // eye on the forewing.
    void MothWing(FMapSurf& S, float Side)
    {
        auto Panel = [&](float Span, float Root, float Chord, float Sweep, bool bEye)
        {
            constexpr int32 Nu = 8, Nv = 5; const int32 Base = S.V.Num();
            for (int32 U = 0; U <= Nu; ++U)
                for (int32 V = 0; V <= Nv; ++V)
                {
                    const float Uf = float(U) / Nu, Vf = float(V) / Nv, Ch = FMath::Lerp(Root, Chord, Uf) * FMath::Sqrt(FMath::Max(1.f - FMath::Pow(Uf, 6.f), .02f));
                    const FVector P(Sweep * Uf + (Vf - .5f) * Ch, Side * Span * Uf, 2.f * FMath::Sin(PI * Vf) * Uf);
                    const float Edge = FMath::Max(Smooth(.7f, 1.f, Uf), Smooth(.3f, .0f, FMath::Min(Vf, 1.f - Vf)) * Uf);
                    float R = FMath::Lerp(1.f, .45f, Edge), G = FMath::Lerp(.72f, .88f, Edge), B = FMath::Lerp(.38f, 1.f, Edge), A = FMath::Lerp(.32f, .55f, Edge);
                    if (bEye) { const float E = FVector2D::Distance(FVector2D(Uf, Vf), FVector2D(.62f, .5f)); const float Ring = Smooth(.2f, .14f, E) * Smooth(.06f, .1f, E); R = FMath::Lerp(R, .25f, Ring); G = FMath::Lerp(G, .18f, Ring); B = FMath::Lerp(B, .3f, Ring); A = FMath::Lerp(A, .05f, Ring); if (E < .06f) { A = .8f; R = 1.f; G = 1.f; B = .85f; } }
                    S.Vert(P, FVector::UpVector, FVector2D(Uf, Vf), Col(R, G, B, A));
                }
            for (int32 U = 0; U < Nu; ++U) for (int32 V = 0; V < Nv; ++V) { const int32 Q = Base + U * (Nv + 1) + V; S.Quad(Q, Q + 1, Q + Nv + 2, Q + Nv + 1); }
        };
        Panel(40.f, 12.f, 21.f, 8.f, true);     // forewing (spread, wider than he is tall)
        Panel(27.f, 13.f, 18.f, -12.f, false);  // hindwing
    }
    void MothBody(FMapSurf& S)
    {
        const FVector Pts[] = {FVector(-9.f, 0, -1.f), FVector(-3.f, 0, 0.f), FVector(3.f, 0, .5f), FVector(7.f, 0, .5f)};
        S.Pipe(Pts, 2.2f, 3.f, 8, 3, Col(.55f, .42f, .3f, .03f), 1.f);
        for (const float Sd : {-1.f, 1.f}) { const FVector A[] = {FVector(7.f, Sd * 1.f, 1.f), FVector(11.f, Sd * 4.f, 5.f), FVector(13.f, Sd * 7.f, 7.f)}; S.Pipe(A, .4f, .3f, 4, 3, Col(.9f, .8f, .5f, .3f), Sd); }
    }
    void Octa(FMapSurf& S, const FVector& C, float R, const FColor& Cl)
    {
        const FVector D[] = {{R, 0, 0}, {-R, 0, 0}, {0, R, 0}, {0, -R, 0}, {0, 0, R}, {0, 0, -R}};
        const int32 B = S.V.Num(); for (const FVector& Q : D) S.Vert(C + Q, Q.GetSafeNormal(), FVector2D::ZeroVector, Cl);
        const int32 F[8][3] = {{0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4}, {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}};
        for (const auto& T : F) S.TriFacing(B + T[0], B + T[1], B + T[2], (D[T[0]] + D[T[1]] + D[T[2]]).GetSafeNormal());
    }
    void Emit(UProceduralMeshComponent* M, FMapSurf& S, UMaterialInterface* Mat, bool bShadow = true)
    {
        M->CreateMeshSection(0, S.V, S.T, S.N, S.UV, S.C, S.Tan, false);
        M->SetMaterial(0, Mat); M->SetCastShadow(bShadow); M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

// ------------------------------------------------------------------ AFeyLife
bool AFeyLife::bCalm = false;
int32 AFeyLife::FlowerToggles = 0, AFeyLife::OrchidShots = 0, AFeyLife::OrchidHits = 0, AFeyLife::MothTouches = 0;
const TArray<TWeakObjectPtr<AFeyLife>>& AFeyLife::All() { return GLife; }
const TArray<FVector>& FeyLifeClearings()
{
    static const TArray<FVector> S = []() { TArray<FVector> Out; for (int32 D = 0; D < Districts; ++D) Out.Append(Placed(D).Clear); return Out; }();
    return S;
}

AFeyLife::AFeyLife()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AFeyLife::EndPlay(const EEndPlayReason::Type Reason)
{
    GLife.RemoveAll([this](const TWeakObjectPtr<AFeyLife>& P) { return !P.IsValid() || P.Get() == this; });
    for (AActor* A : Spawned) if (IsValid(A)) A->Destroy();
    Super::EndPlay(Reason);
}

UProceduralMeshComponent* AFeyLife::NewMesh(USceneComponent* Parent, const FName& Name)
{
    auto* M = NewObject<UProceduralMeshComponent>(this, Name); M->SetupAttachment(Parent ? Parent : RootComponent.Get());
    M->bUseAsyncCooking = false; M->RegisterComponent(); return M;
}

void AFeyLife::Build(int32 D)
{
    District = D; GLife.Add(this);
    const FPlace& P = Placed(D);
    for (const TCHAR* Path : MatPaths) Mats.Add(LoadObject<UMaterialInterface>(nullptr, Path));
    Flowers = P.Flowers; Vines = P.Vines; Pollen = P.Pollen; Orchids = P.Orchids; Moths = P.Moths;
    // Flowers: body (leaves, stem) once, the head rebuilt as it changes.
    for (int32 I = 0; I < Flowers.Num(); ++I)
    {
        auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->RegisterComponent();
        Pivot->SetRelativeLocationAndRotation(Flowers[I].Pos, FRotator(0, Flowers[I].Yaw, 0)); Pivot->SetRelativeScale3D(FVector(1.6f));   // (knee-high to a man: as tall as he is)
        FMapSurf Leaves, Stem; FlowerBody(Leaves, Stem, I * 1.7f);
        Emit(NewMesh(Pivot, *FString::Printf(TEXT("FlowerLeaves%d"), I)), Leaves, Mats[MBlade]);
        Emit(NewMesh(Pivot, *FString::Printf(TEXT("FlowerStem%d"), I)), Stem, Mats[MPlant]);
        auto* Head = NewMesh(Pivot, *FString::Printf(TEXT("FlowerHead%d"), I));
        auto* Stamen = NewMesh(Head, *FString::Printf(TEXT("FlowerStamens%d"), I));
        FlowerHeads.Add(Head); FlowerHeads.Add(Stamen);
        ShowFlower(I);
    }
    for (int32 I = 0; I < Vines.Num(); ++I)
    {
        auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->RegisterComponent();
        Pivot->SetRelativeLocationAndRotation(Vines[I].C, FRotator(0, Vines[I].Yaw, 0));
        FMapSurf Wood, Leaf; VineCurtain(Wood, Leaf, Vines[I].Width, Vines[I].Height);
        auto* W = NewMesh(Pivot, *FString::Printf(TEXT("Vines%d"), I)); Emit(W, Wood, Mats[MRoot]);
        auto* L = NewMesh(W, *FString::Printf(TEXT("VineLeaves%d"), I)); Emit(L, Leaf, Mats[MBlade]);
        VineMeshes.Add(W);
        auto* B = NewObject<UBoxComponent>(this); B->SetupAttachment(Pivot); B->SetBoxExtent(FVector(Vines[I].Width * .5f + 30.f, 22.f, Vines[I].Height * .5f + 40.f));
        B->SetRelativeLocation(FVector(0, 0, Vines[I].Height * .5f)); B->SetCollisionProfileName(TEXT("BlockAll")); B->SetHiddenInGame(true); B->RegisterComponent();
        VineBoxes.Add(B);
    }
    // The boulders round the flower's nook, and what lies inside.
    if (UStaticMesh* RockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/ChultNature/SM_ChultRock2.SM_ChultRock2")))
        for (int32 I = 0; I < P.Rocks.Num(); ++I)
        {
            auto* R = NewObject<UStaticMeshComponent>(this); R->SetupAttachment(RootComponent); R->SetStaticMesh(RockMesh);
            const FVector& Q = P.Rocks[I];
            // (sized from the rock itself: about 3 m across and 2.2 m high, so the ring shuts)
            const FVector Ext = RockMesh->GetBounds().BoxExtent; const float Sx = 150.f / FMath::Max(1.f, FMath::Max(Ext.X, Ext.Y)) * Q.Z / 2.2f, Sz = FMath::Max(Sx, 115.f / FMath::Max(1.f, Ext.Z));
            R->SetRelativeTransform(FTransform(FRotator(0, I * 47.f, 0), FVector(Q.X, Q.Y, LandZ(D, Q.X, Q.Y) - 25.f), FVector(Sx, Sx, Sz)));
            R->SetCollisionProfileName(TEXT("BlockAll")); R->RegisterComponent();
        }
    for (const FVector& C : P.Cigs) if (ACigarettePickup* Cg = ACigarettePickup::Spawn(GetWorld(), FeyWorld(C + FVector(0, 0, 30.f)), FVector::ZeroVector, FeyWorld(C).Z)) Spawned.Add(Cg);   // (gone with the district)
    // Pollen: low gold flowers thick over each bed, and spores over it.
    if (Pollen.Num())
    {
        FMapSurf Bloom; FRandomStream R(31 + D);
        for (const FFeyPollen& Pb : Pollen)
        {
            const FQuat Q(FRotator(0, Pb.Yaw, 0)); const int32 N = FMath::RoundToInt(PI * Pb.R.X * Pb.R.Y / 140.f);
            for (int32 K = 0; K < N; ++K)
            {
                const float A = R.FRandRange(0.f, 2 * PI), Rr = FMath::Sqrt(R.FRand());
                const FVector L = FVector(Pb.C, 0.f) + Q.RotateVector(FVector(Pb.R.X * Rr * FMath::Cos(A), Pb.R.Y * Rr * FMath::Sin(A), 0.f));
                const FVector Head = FVector(L.X, L.Y, LandZ(D, L.X, L.Y) + R.FRandRange(5.f, 17.f)); const float Sz = R.FRandRange(4.5f, 8.f), Tw = R.FRandRange(0.f, 2 * PI);
                const FColor Cl = Col(1.f, R.FRandRange(.72f, .86f), R.FRandRange(.18f, .32f), R.FRandRange(.26f, .4f) * (Rr < .92f ? 1.f : .5f));
                const int32 B = Bloom.V.Num(); Bloom.Vert(Head + FVector(0, 0, .6f), FVector::UpVector, FVector2D(0.f, .5f), Cl);
                for (int32 Pt = 0; Pt < 10; ++Pt) { const float Ang = Tw + PI * Pt / 5.f, Rad = Pt % 2 ? Sz * .45f : Sz; Bloom.Vert(Head + FVector(Rad * FMath::Cos(Ang), Rad * FMath::Sin(Ang), 0.f), FVector::UpVector, FVector2D(Pt % 2 ? .5f : 1.f, .5f), Cl); }
                for (int32 Pt = 0; Pt < 10; ++Pt) Bloom.TriFacing(B, B + 1 + Pt, B + 1 + (Pt + 1) % 10, FVector::UpVector);
                if (K % 5 == 0) { SporeBase.Add(FVector(L.X, L.Y, LandZ(D, L.X, L.Y) + R.FRandRange(10.f, 70.f))); }
            }
        }
        Emit(NewMesh(nullptr, TEXT("PollenBlooms")), Bloom, Mats[MPlant], false);
        Spores = SporeBase; SporeMesh = NewMesh(nullptr, TEXT("PollenSpores"));
    }
    // Orchids.
    for (int32 I = 0; I < Orchids.Num(); ++I)
    {
        auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->RegisterComponent();
        Pivot->SetRelativeLocation(Orchids[I].Pos); Pivot->SetRelativeScale3D(FVector(1.8f));   // (taller than he is, readable over the garden)
        FMapSurf Leaves, Stem, Bulb; OrchidBody(Leaves, Stem, Orchids[I].Dir); OrchidBulb(Bulb);
        Emit(NewMesh(Pivot, *FString::Printf(TEXT("OrchidLeaves%d"), I)), Leaves, Mats[MBlade]);
        Emit(NewMesh(Pivot, *FString::Printf(TEXT("OrchidStem%d"), I)), Stem, Mats[MPlant]);
        auto* B = NewMesh(Pivot, *FString::Printf(TEXT("OrchidBulb%d"), I)); Emit(B, Bulb, Mats[MPlant]);
        B->SetRelativeLocationAndRotation(Orchids[I].Dir * 4.f + FVector(0, 0, 60.f), FRotator(-90.f, 0, 0) + Orchids[I].Dir.Rotation() + FRotator(90.f, 0, 0));
        B->SetRelativeRotation(FRotationMatrix::MakeFromZX(Orchids[I].Dir + FVector(0, 0, .25f), FVector::UpVector).Rotator());
        OrchidBulbs.Add(B);
        auto* L = NewObject<UPointLightComponent>(this); L->SetupAttachment(B); L->SetRelativeLocation(FVector(0, 0, 14.f));
        L->SetLightColor(FLinearColor(1.f, .35f, .75f)); L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(0.f); L->SetAttenuationRadius(260.f); L->SetCastShadows(false); L->RegisterComponent();
        OrchidLights.Add(L);
    }
    if (Orchids.Num()) SeedMesh = NewMesh(nullptr, TEXT("OrchidSeeds"));
    // Moths.
    for (int32 I = 0; I < Moths.Num(); ++I)
    {
        auto* Pivot = NewObject<USceneComponent>(this); Pivot->SetupAttachment(RootComponent); Pivot->RegisterComponent();
        FMapSurf Body; MothBody(Body); Emit(NewMesh(Pivot, *FString::Printf(TEXT("MothBody%d"), I)), Body, Mats[MPlant], false);
        for (const float Sd : {1.f, -1.f})
        {
            auto* Hinge = NewObject<USceneComponent>(this); Hinge->SetupAttachment(Pivot); Hinge->RegisterComponent(); Hinge->SetRelativeLocation(FVector(0, Sd * 1.5f, 1.f));
            FMapSurf Wing; MothWing(Wing, Sd); Emit(NewMesh(Hinge, *FString::Printf(TEXT("MothWing%d_%d"), I, Sd > 0 ? 1 : 0)), Wing, Mats[MPetal], false);
            (Sd > 0 ? MothWingR : MothWingL).Add(Hinge);
        }
        auto* L = NewObject<UPointLightComponent>(this); L->SetupAttachment(Pivot); L->SetLightColor(FLinearColor(1.f, .78f, .5f)); L->SetIntensityUnits(ELightUnits::Candelas);
        L->SetIntensity(45.f); L->SetAttenuationRadius(480.f); L->SetCastShadows(false); L->RegisterComponent();
        MothPivots.Add(Pivot);
        Moths[I].Last = Moths[I].Home;
    }
    if (auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0))) Respawns = Chuck->GetRespawns();
    UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_LIFE_BUILT district=%s flowers=%d vines=%d pollen=%d orchids=%d moths=%d"), FeyMap::District(D).Name, Flowers.Num(), Vines.Num(), Pollen.Num(), Orchids.Num(), Moths.Num());
}

void AFeyLife::ShowFlower(int32 I)
{
    FFeyFlower& F = Flowers[I];
    const float Glow = F.Pulse >= 0.f ? FMath::Sin(FMath::Clamp(F.Pulse / .9f, 0.f, 1.f) * PI) * (.6f + .4f * FMath::Sin(F.Pulse * 30.f)) : 0.f;
    const float Key = F.Open * 100.f + Glow * 10.f;
    if (FMath::IsNearlyEqual(Key, F.Shown, .05f)) return;
    F.Shown = Key;
    FMapSurf Petals, Stamens; FlowerHead(Petals, Stamens, F.Open, Glow);
    Emit(FlowerHeads[I * 2], Petals, Mats[MPetal]); Emit(FlowerHeads[I * 2 + 1], Stamens, Mats[MPlant], false);
    // (a shake as it pulses)
    FlowerHeads[I * 2]->SetRelativeRotation(FRotator(FMath::Sin(F.Pulse * 40.f) * 4.f * Glow, 0.f, FMath::Cos(F.Pulse * 33.f) * 4.f * Glow));
}

void AFeyLife::ResetAll()
{
    for (int32 I = 0; I < Flowers.Num(); ++I) { Flowers[I].Open = Flowers[I].Want = 0.f; Flowers[I].Pulse = -1.f; ShowFlower(I); }
    for (FFeyVines& V : Vines) V.Up = V.Want = 1.f;
    Seeds.Reset();
}

bool AFeyLife::TryScratch(const FVector& Location, const FVector& Ahead, float Reach)
{
    const FVector L = FeyLocal(Location); bool bAny = false;
    for (int32 I = 0; I < Flowers.Num(); ++I)
    {
        FFeyFlower& F = Flowers[I]; const FVector To = F.Pos - L;
        if (To.Size2D() - 24.f > Reach || FMath::Abs(To.Z + Stand) > 90.f) continue;
        if (FVector::DotProduct(To.GetSafeNormal2D(), Ahead.GetSafeNormal2D()) < .2f && To.Size2D() > 30.f) continue;
        bAny = true;
        if (F.Pulse >= 0.f) continue;   // (already changing)
        F.Pulse = 0.f; F.Want = F.Want > .5f ? 0.f : 1.f; ++FlowerToggles;
        UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_FLOWER_SCRATCHED district=%s flower=%d open=%d"), District >= 0 ? FeyMap::District(District).Name : TEXT("-"), I, F.Want > .5f);
    }
    return bAny;
}

bool AFeyLife::InPollen(const FVector& L) const
{
    for (const FFeyPollen& Pb : Pollen)
    {
        const FVector Q = FQuat(FRotator(0, -Pb.Yaw, 0)).RotateVector(FVector(L.X - Pb.C.X, L.Y - Pb.C.Y, 0.f));
        if (FMath::Square(Q.X / Pb.R.X) + FMath::Square(Q.Y / Pb.R.Y) < 1.f) return true;
    }
    return false;
}

FVector AFeyLife::MothAt(int32 I) const
{
    const FFeyMoth& M = Moths[I]; const float A = Clock * M.Rate + M.Phase; const FVector Side(-M.Axis.Y, M.Axis.X, 0.f);
    return M.Home + M.Axis * (FMath::Sin(A) * M.Range) + Side * (FMath::Sin(A * 3.1f + M.Phase * 2.f) * M.Weave) + FVector(0, 0, FMath::Sin(A * 2.3f + 1.f + M.Phase) * M.Bob);
}

void AFeyLife::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Dt = FMath::Clamp(DeltaSeconds, 0.f, .05f); Clock += Dt;
    auto* Chuck = Cast<AChuckCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Chuck) return;
    if (Chuck->GetRespawns() != Respawns) { Respawns = Chuck->GetRespawns(); ResetAll(); }   // (back as they began after a death)
    const FVector C = FeyLocal(Chuck->GetActorLocation()), Feet = C - FVector(0, 0, Stand);
    // Flowers: the pulse, then the change.
    for (int32 I = 0; I < Flowers.Num(); ++I)
    {
        FFeyFlower& F = Flowers[I];
        if (F.Pulse >= 0.f)
        {
            F.Pulse += Dt;
            if (F.Pulse > .7f)
            {
                F.Open = FMath::FInterpConstantTo(F.Open, F.Want, Dt, 1.2f);
                if (F.Vines >= 0) Vines[F.Vines].Want = 1.f - F.Want;
            }
            if (F.Pulse > .7f && FMath::IsNearlyEqual(F.Open, F.Want)) F.Pulse = -1.f;
        }
        ShowFlower(I);
    }
    for (int32 I = 0; I < Vines.Num(); ++I)
    {
        FFeyVines& V = Vines[I];
        // Never grows back over him.
        const FVector Q = FQuat(FRotator(0, -V.Yaw, 0)).RotateVector(C - V.C);
        const bool bInside = FMath::Abs(Q.X) < V.Width * .5f + 70.f && FMath::Abs(Q.Y) < 90.f && Q.Z < V.Height + 80.f;
        const float Want = V.Want > V.Up && bInside ? V.Up : V.Want;
        V.Up = FMath::FInterpConstantTo(V.Up, Want, Dt, .8f);
        const float S = FMath::Lerp(.03f, 1.f, Smooth(0.f, 1.f, V.Up));
        VineMeshes[I]->SetRelativeScale3D(FVector(1.f + .1f * (1.f - S), 1.f, S));
        VineMeshes[I]->SetVisibility(V.Up > .02f);
        const bool bShut = V.Up > .5f;
        if ((VineBoxes[I]->GetCollisionEnabled() != ECollisionEnabled::NoCollision) != bShut) VineBoxes[I]->SetCollisionEnabled(bShut ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
    // Spores drift up and round, then begin again.
    if (SporeMesh && Spores.Num())
    {
        FMapSurf S;
        for (int32 I = 0; I < Spores.Num(); ++I)
        {
            const float T = FMath::Frac(Clock * .07f + I * .137f);
            const FVector P = SporeBase[I] + FVector(14.f * FMath::Sin(Clock * .6f + I), 14.f * FMath::Cos(Clock * .5f + I * 1.3f), 80.f * T);
            Octa(S, P, 1.3f, Col(1.f, .9f, .55f, .7f * FMath::Sin(PI * T)));
        }
        Emit(SporeMesh, S, Mats[MPlant], false);
    }
    // Orchids: swell and glow, then spit along their lanes.
    for (int32 I = 0; I < Orchids.Num(); ++I)
    {
        FFeyOrchid& O = Orchids[I];
        const float T = FMath::Fmod(Clock + O.Phase, OrchidBeat), Wind = Smooth(OrchidBeat - OrchidWind, OrchidBeat, T), After = Smooth(.25f, 0.f, T);
        OrchidBulbs[I]->SetRelativeScale3D(FVector(1.f + .35f * Wind - .15f * After));
        OrchidLights[I]->SetIntensity(70.f * Wind + 25.f * After);
        if (T < .1f && !O.bFired)
        {
            O.bFired = true; ++OrchidShots;
            FFeySeed Sd; Sd.P = OrchidBulbs[I]->GetComponentLocation() - GetActorLocation() + O.Dir * 14.f; Sd.P.Z = O.Pos.Z + 48.f; Sd.V = O.Dir * SeedSpeed; Sd.Life = O.Range / SeedSpeed;
            Seeds.Add(Sd);
        }
        if (T > .5f) O.bFired = false;
    }
    if (SeedMesh)
    {
        FCollisionQueryParams Qp(SCENE_QUERY_STAT(FeySeed), false, Chuck); Qp.AddIgnoredActor(this);
        for (int32 I = Seeds.Num() - 1; I >= 0; --I)
        {
            FFeySeed& Sd = Seeds[I]; const FVector From = Sd.P; Sd.P += Sd.V * Dt; Sd.Life -= Dt;
            FHitResult Hit;
            const bool bStop = Sd.Life <= 0.f || GetWorld()->LineTraceSingleByChannel(Hit, FeyWorld(From), FeyWorld(Sd.P), ECC_Visibility, Qp);
            const FVector Near = FMath::ClosestPointOnSegment(Sd.P, Feet + FVector(0, 0, 8.f), Feet + FVector(0, 0, 58.f));
            if (!bCalm && FVector::Dist(Sd.P, Near) < SeedRadius + 15.f && !Chuck->IsAstral())
            {
                if (Chuck->TakeBite(FeyWorld(C - Sd.V.GetSafeNormal() * 100.f), 1, true, false)) { ++OrchidHits; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_ORCHID_HIT sanity=%d"), Chuck->GetSanity()); }
                Seeds.RemoveAt(I); continue;
            }
            if (bStop) Seeds.RemoveAt(I);
        }
        FMapSurf S; for (const FFeySeed& Sd : Seeds) Octa(S, Sd.P, SeedRadius, Col(.5f, .2f, .12f, .02f));
        Emit(SeedMesh, S, Mats[MPlant], false);
    }
    // Moths.
    for (int32 I = 0; I < Moths.Num(); ++I)
    {
        FFeyMoth& M = Moths[I]; const FVector P = MothAt(I), V = (P - M.Last) / FMath::Max(Dt, 1e-3f); M.Last = P;
        if (V.SizeSquared2D() > 100.f) M.Facing = FMath::RInterpTo(M.Facing, FRotator(FMath::Clamp(-V.Z * .08f, -20.f, 20.f), V.Rotation().Yaw, 0.f), Dt, 7.f);
        MothPivots[I]->SetRelativeLocationAndRotation(P, M.Facing);
        const float Flap = 38.f * FMath::Sin(Clock * 14.f + M.Phase) + 8.f;
        MothWingR[I]->SetRelativeRotation(FRotator(0, 0, Flap)); MothWingL[I]->SetRelativeRotation(FRotator(0, 0, -Flap));
        M.TouchWait -= Dt;
        const FVector Near = FMath::ClosestPointOnSegment(P, Feet + FVector(0, 0, 8.f), Feet + FVector(0, 0, 58.f));
        if (!bCalm && M.TouchWait <= 0.f && FVector::Dist(P, Near) < MothRadius + 16.f && !Chuck->IsAstral())
        {
            M.TouchWait = 1.2f;
            if (Chuck->TakeBite(FeyWorld(P), 1, true, false)) { ++MothTouches; UE_LOG(LogTemp, Display, TEXT("CHUCK_FEY_MOTH_TOUCH sanity=%d"), Chuck->GetSanity()); }
        }
    }
}

float FeyGroundSpeedScale(const FVector& World)
{
    if (!InFeyMap(World)) return 1.f;
    const FVector L = FeyLocal(World) - FVector(0, 0, Stand);
    for (const TWeakObjectPtr<AFeyLife>& P : GLife) if (P.IsValid() && P->InPollen(L)) return .5f;
    return 1.f;
}

bool FeyScratch(AChuckCharacter* Chuck, const FVector& Location, const FVector& Ahead, float Reach)
{
    if (!Chuck || !InFeyMap(Location)) return false;
    bool bAny = false;
    for (const TWeakObjectPtr<AFeyLife>& P : GLife) if (P.IsValid()) bAny |= P->TryScratch(Location, Ahead, Reach);
    return bAny;
}
